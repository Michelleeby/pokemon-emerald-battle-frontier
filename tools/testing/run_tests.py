#!/usr/bin/env python3
"""Run gameplay suites or the opt-in runner diagnostic ROMs with mGBA."""

from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import shlex
import shutil
import subprocess
import sys
import time


ROOT = Path(__file__).resolve().parents[2]
CORE_TESTS = (
    "frontier-common",
    "team-lab",
    "new-game-tutorial",
    "save-load",
    "battle-shared",
    "frontier-tower",
    "frontier-factory",
    "frontier-dome",
    "frontier-arena",
    "frontier-palace",
    "frontier-pike",
    "frontier-pyramid",
)
DIAGNOSTIC_TESTS = ("spike-pass", "spike-fail", "spike-hang")


def find_emulator(explicit: str | None) -> str:
    candidates = (
        explicit,
        os.environ.get("MGBA_ROM_TEST"),
        shutil.which("mgba-rom-test"),
        str(ROOT.parent / "mgba" / "build" / "test" / "mgba-rom-test"),
    )
    for candidate in candidates:
        if candidate and Path(candidate).is_file() and os.access(candidate, os.X_OK):
            return candidate
    raise RuntimeError(
        "mgba-rom-test was not found; set MGBA_ROM_TEST or build the sibling "
        "mGBA checkout with -DBUILD_ROM_TEST=ON"
    )


def parse_tests(value: str, diagnostics: bool = False) -> list[str]:
    tests = shlex.split(value.replace(",", " "))
    known = DIAGNOSTIC_TESTS if diagnostics else CORE_TESTS
    unknown = sorted(set(tests) - set(known))
    if unknown:
        raise ValueError("unknown test(s): " + ", ".join(unknown))
    if not tests:
        raise ValueError("zero tests selected")
    return tests


def write_report(path: Path, report: dict[str, object]) -> None:
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")


def run_test(emulator: str, name: str, timeout_seconds: float) -> bool:
    rom = ROOT / "build" / "test" / "roms" / f"{name}.gba"
    artifact_dir = ROOT / "build" / "test" / "artifacts" / name
    artifact_dir.mkdir(parents=True, exist_ok=True)
    log_path = artifact_dir / "emulator.log"
    report_path = artifact_dir / "report.json"
    command = [emulator, "-S", "0x0F", "-R", "r0", str(rom)]
    started = time.monotonic()

    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            timeout=timeout_seconds,
            check=False,
        )
        output = completed.stdout
        status = "passed" if completed.returncode == 0 and "TEST_PASS" in output else "failed"
        if completed.returncode != 0 and "TEST_FAIL" not in output:
            status = "crashed-or-runner-error"
        report = {
            "test": name,
            "status": status,
            "exit_code": completed.returncode,
            "duration_seconds": round(time.monotonic() - started, 3),
            "command": command,
        }
    except subprocess.TimeoutExpired as error:
        output = error.stdout or ""
        if isinstance(output, bytes):
            output = output.decode(errors="replace")
        status = "timed-out"
        report = {
            "test": name,
            "status": status,
            "exit_code": None,
            "duration_seconds": round(time.monotonic() - started, 3),
            "timeout_seconds": timeout_seconds,
            "command": command,
        }

    log_path.write_text(output, encoding="utf-8")
    write_report(report_path, report)
    print(f"{name}: {status} ({report_path.relative_to(ROOT)})")
    if status != "passed" and output:
        print(output.rstrip())
    return status == "passed"


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--tests", default="team-lab")
    parser.add_argument("--diagnostics", action="store_true")
    parser.add_argument("--emulator")
    parser.add_argument(
        "--timeout", type=float, default=float(os.environ.get("TEST_TIMEOUT", "5"))
    )
    args = parser.parse_args()

    try:
        tests = parse_tests(args.tests, args.diagnostics)
        emulator = find_emulator(args.emulator)
    except (RuntimeError, ValueError) as error:
        parser.error(str(error))

    results = [run_test(emulator, name, args.timeout) for name in tests]
    return 0 if all(results) else 1


if __name__ == "__main__":
    sys.exit(main())
