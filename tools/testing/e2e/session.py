#!/usr/bin/env python3
"""Synchronous Python client for the project-owned mGBA E2E driver."""

from __future__ import annotations

import json
import os
from pathlib import Path
import select
import subprocess
import time
from typing import TextIO


ROOT = Path(__file__).resolve().parents[3]
DEFAULT_RUNNER = ROOT / "build" / "e2e" / "mgba-e2e"
DEFAULT_MGBA_BUILD = ROOT.parent / "mgba" / "build"

KEYS = {
    "A": 1 << 0,
    "B": 1 << 1,
    "SELECT": 1 << 2,
    "START": 1 << 3,
    "RIGHT": 1 << 4,
    "LEFT": 1 << 5,
    "UP": 1 << 6,
    "DOWN": 1 << 7,
    "R": 1 << 8,
    "L": 1 << 9,
}
DEFAULT_RESPONSE_TIMEOUT = 5.0


class E2EError(RuntimeError):
    """Base error for E2E driver failures."""


class DriverTimeout(E2EError):
    """The driver did not respond before the wall-clock deadline."""


class ProtocolError(E2EError):
    """The driver emitted an invalid or unexpected response."""


class CommandError(E2EError):
    """The driver rejected a valid protocol command."""

    def __init__(self, command: str, response: str):
        super().__init__(f"driver rejected {command!r}: {response}")
        self.command = command
        self.response = response


def key_mask(*names: str) -> int:
    """Return the mGBA key mask for one or more canonical key names."""

    mask = 0
    for name in names:
        try:
            mask |= KEYS[name.upper()]
        except KeyError as error:
            raise ValueError(f"unknown GBA key: {name}") from error
    return mask


def parse_fields(response: str) -> dict[str, str]:
    """Parse key=value fields following an OK or READY response."""

    fields: dict[str, str] = {}
    for token in response.split()[1:]:
        if "=" in token:
            key, value = token.split("=", 1)
            fields[key] = value
    return fields


class Session:
    """Own one live mGBA driver process and its scenario-local artifacts."""

    def __init__(
        self,
        rom: Path,
        artifact_dir: Path,
        *,
        save: Path | None = None,
        runner: Path | None = None,
        response_timeout: float = DEFAULT_RESPONSE_TIMEOUT,
        environment: dict[str, str] | None = None,
    ) -> None:
        self.rom = Path(rom).resolve()
        self.save = Path(save).resolve() if save is not None else None
        self.artifact_dir = Path(artifact_dir).resolve()
        self.runner = Path(
            runner or os.environ.get("MGBA_E2E_RUNNER", DEFAULT_RUNNER)
        ).resolve()
        self.response_timeout = response_timeout
        self.environment = environment
        self.process: subprocess.Popen[str] | None = None
        self._emulator_log: TextIO | None = None
        self._trace: list[dict[str, object]] = []
        self._trace_path = self.artifact_dir / "input-trace.json"

    def __enter__(self) -> Session:
        self.start()
        return self

    def __exit__(self, exc_type: object, exc: object, traceback: object) -> None:
        if exc_type is not None and self.process is not None:
            try:
                self.screenshot("failure.png")
            except E2EError:
                pass
        self.close()

    def start(self) -> None:
        if self.process is not None:
            raise E2EError("session is already running")
        if not self.runner.is_file():
            raise E2EError(f"mGBA E2E runner was not found: {self.runner}")
        if not self.rom.is_file():
            raise E2EError(f"ROM was not found: {self.rom}")

        self.artifact_dir.mkdir(parents=True, exist_ok=True)
        self._emulator_log = (self.artifact_dir / "emulator.log").open(
            "w", encoding="utf-8"
        )
        command = [str(self.runner), "--rom", str(self.rom)]
        if self.save is not None:
            self.save.parent.mkdir(parents=True, exist_ok=True)
            command.extend(("--save", str(self.save)))

        env = os.environ.copy()
        library_path = str(DEFAULT_MGBA_BUILD)
        if env.get("LD_LIBRARY_PATH"):
            library_path += os.pathsep + env["LD_LIBRARY_PATH"]
        env["LD_LIBRARY_PATH"] = library_path
        if self.environment:
            env.update(self.environment)

        self.process = subprocess.Popen(
            command,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=self._emulator_log,
            text=True,
            bufsize=1,
            env=env,
        )
        try:
            response = self._read_response("startup")
            if not response.startswith("READY "):
                raise ProtocolError(f"expected READY from driver, got: {response}")
            version = parse_fields(response).get("protocol")
            if version != "1":
                raise ProtocolError(f"unsupported driver protocol: {version!r}")
            self._record("<startup>", response, 0.0)
        except BaseException:
            self._terminate()
            self._close_resources()
            raise

    def close(self) -> None:
        if self.process is None:
            return
        if self.process.poll() is None:
            try:
                self._command("QUIT")
                self.process.wait(timeout=self.response_timeout)
            except (E2EError, subprocess.TimeoutExpired):
                self._terminate()
        self._close_resources()

    def ping(self) -> None:
        self._command("PING")

    def run_frames(self, count: int) -> int:
        return self._frame_from(self._command(f"FRAME {count}"))

    def set_keys(self, *names: str) -> None:
        self._command(f"KEYS {key_mask(*names)}")

    def press(self, *names: str, held_frames: int = 1, released_frames: int = 1) -> int:
        mask = key_mask(*names)
        response = self._command(f"PRESS {mask} {held_frames} {released_frames}")
        return self._frame_from(response)

    def read(self, address: int, *, width: int = 32) -> int:
        self._validate_width(width)
        response = self._command(f"READ{width} {address:#x}")
        try:
            return int(parse_fields(response)["value"], 0)
        except (KeyError, ValueError) as error:
            raise ProtocolError(f"READ response has no valid value: {response}") from error

    def wait(
        self,
        address: int,
        expected: int,
        *,
        mask: int | None = None,
        width: int = 32,
        max_frames: int = 600,
    ) -> tuple[int, int]:
        self._validate_width(width)
        if mask is None:
            mask = (1 << width) - 1
        response = self._command(
            f"WAIT{width} {address:#x} {mask:#x} {expected:#x} {max_frames}"
        )
        fields = parse_fields(response)
        try:
            return int(fields["frames"], 0), int(fields["value"], 0)
        except (KeyError, ValueError) as error:
            raise ProtocolError(f"WAIT response has invalid fields: {response}") from error

    def screenshot(self, name: str) -> Path:
        if not name or Path(name).name != name or any(char.isspace() for char in name):
            raise ValueError("screenshot name must be a whitespace-free basename")
        path = self.artifact_dir / name
        self._command(f"SCREENSHOT {path}")
        return path

    def restart(self) -> int:
        return self._frame_from(self._command("RESTART"))

    def _command(self, command: str) -> str:
        process = self._require_process()
        if process.poll() is not None:
            raise ProtocolError(f"driver exited before command with status {process.returncode}")
        assert process.stdin is not None

        started = time.monotonic()
        try:
            process.stdin.write(command + "\n")
            process.stdin.flush()
        except BrokenPipeError as error:
            raise ProtocolError("driver closed its command pipe") from error
        try:
            response = self._read_response(command)
        except E2EError as error:
            self._record(
                command,
                f"EXCEPTION {type(error).__name__}: {error}",
                time.monotonic() - started,
            )
            raise
        self._record(command, response, time.monotonic() - started)
        if response == "ERR" or response.startswith("ERR "):
            raise CommandError(command, response)
        if response != "OK" and not response.startswith("OK "):
            raise ProtocolError(f"invalid response to {command!r}: {response}")
        return response

    def _read_response(self, operation: str) -> str:
        process = self._require_process()
        assert process.stdout is not None
        ready, _, _ = select.select([process.stdout], [], [], self.response_timeout)
        if not ready:
            self._terminate()
            raise DriverTimeout(
                f"driver timed out after {self.response_timeout:g}s during {operation}"
            )
        response = process.stdout.readline()
        if response == "":
            returncode = process.poll()
            raise ProtocolError(
                f"driver exited during {operation} with status {returncode}"
            )
        return response.rstrip("\r\n")

    def _record(self, command: str, response: str, duration: float) -> None:
        self._trace.append(
            {
                "command": command,
                "response": response,
                "duration_seconds": round(duration, 6),
            }
        )
        self._trace_path.write_text(
            json.dumps({"version": 1, "events": self._trace}, indent=2) + "\n",
            encoding="utf-8",
        )

    def _frame_from(self, response: str) -> int:
        try:
            return int(parse_fields(response)["frame"], 0)
        except (KeyError, ValueError) as error:
            raise ProtocolError(f"response has no valid frame: {response}") from error

    @staticmethod
    def _validate_width(width: int) -> None:
        if width not in (8, 16, 32):
            raise ValueError("memory width must be 8, 16, or 32 bits")

    def _require_process(self) -> subprocess.Popen[str]:
        if self.process is None:
            raise E2EError("session is not running")
        return self.process

    def _terminate(self) -> None:
        if self.process is not None and self.process.poll() is None:
            self.process.kill()
            self.process.wait()

    def _close_resources(self) -> None:
        if self.process is not None:
            if self.process.stdin is not None:
                self.process.stdin.close()
            if self.process.stdout is not None:
                self.process.stdout.close()
        if self._emulator_log is not None:
            self._emulator_log.close()
        self.process = None
        self._emulator_log = None
