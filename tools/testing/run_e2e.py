#!/usr/bin/env python3
"""Run selected project-owned end-to-end scenarios."""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time
from typing import Callable

from e2e.session import DEFAULT_RESPONSE_TIMEOUT, DriverTimeout, ProtocolError
from e2e.arena_hard_greta import run as run_arena_hard_greta
from e2e.arena_normal_greta import run as run_arena_normal_greta
from e2e.dome_hard_tucker import run as run_dome_hard_tucker
from e2e.dome_normal_tucker import run as run_dome_normal_tucker
from e2e.factory_hard_noland import run as run_factory_hard_noland
from e2e.factory_hard_setup import run as run_factory_hard_setup
from e2e.factory_normal_noland import run as run_factory_normal_noland
from e2e.palace_hard_spenser import run as run_palace_hard_spenser
from e2e.palace_normal_spenser import run as run_palace_normal_spenser
from e2e.pike_hard_lucy import run as run_pike_hard_lucy
from e2e.pike_normal_lucy import run as run_pike_normal_lucy
from e2e.tower_hard_anabel import run as run_tower_hard_anabel
from e2e.tower_normal_anabel import run as run_tower_normal_anabel


ROOT = Path(__file__).resolve().parents[2]
ARTIFACT_ROOT = ROOT / "build" / "e2e" / "artifacts"
UPLOAD_ROOT = ROOT / "build" / "e2e" / "upload"
RELEASE_ROM = ROOT / "pokeemerald.gba"
GAMEPLAY_ROM = ROOT / "build" / "e2e" / "gameplay" / "pokeemerald.gba"
MGBA_REVISION = "26b7884bc25a5933960f3cdcd98bac1ae14d42e2"
FIXTURE_VERSION = 1
SCENARIO_VERSION = 1
RNG_SEEDS = {"primary": "0x1234", "secondary": "0x5678"}
ALLOWED_DIAGNOSTIC_NAMES = {"report.json", "input-trace.json"}
ALLOWED_DIAGNOSTIC_SUFFIXES = {".json", ".log", ".png", ".sav"}
SCENARIOS: dict[str, Callable[[Path], None]] = {
    "arena-hard-greta": run_arena_hard_greta,
    "arena-normal-greta": run_arena_normal_greta,
    "dome-hard-tucker": run_dome_hard_tucker,
    "dome-normal-tucker": run_dome_normal_tucker,
    "factory-hard-noland": run_factory_hard_noland,
    "factory-hard-setup": run_factory_hard_setup,
    "factory-normal-noland": run_factory_normal_noland,
    "palace-hard-spenser": run_palace_hard_spenser,
    "palace-normal-spenser": run_palace_normal_spenser,
    "pike-hard-lucy": run_pike_hard_lucy,
    "pike-normal-lucy": run_pike_normal_lucy,
    "tower-hard-anabel": run_tower_hard_anabel,
    "tower-normal-anabel": run_tower_normal_anabel,
}


def selected_scenarios(names: list[str]) -> list[str]:
    selected = names or list(SCENARIOS)
    unknown = [name for name in selected if name not in SCENARIOS]
    if unknown:
        raise ValueError("unknown E2E scenario(s): " + ", ".join(unknown))
    if len(selected) != len(set(selected)):
        raise ValueError("duplicate E2E scenario selection")
    return selected


def write_report(path: Path, report: dict[str, object]) -> None:
    path.mkdir(parents=True, exist_ok=True)
    (path / "report.json").write_text(
        json.dumps(report, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def commit_sha() -> str:
    configured = os.environ.get("E2E_COMMIT_SHA") or os.environ.get("GITHUB_SHA")
    if configured:
        return configured
    completed = subprocess.run(
        ["git", "rev-parse", "HEAD"],
        cwd=ROOT,
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def base_report(name: str) -> dict[str, object]:
    return {
        "commit_sha": commit_sha(),
        "fixture_version": FIXTURE_VERSION,
        "mgba_revision": MGBA_REVISION,
        "rng_seed": RNG_SEEDS,
        "rom_sha256": file_sha256(RELEASE_ROM),
        "gameplay_rom_sha256": file_sha256(GAMEPLAY_ROM),
        "battle_policy": "Tower, Factory, Dome, Arena, Palace, and Pike outcomes are assisted in E2E gameplay builds",
        "rtc_value": None,
        "scenario": name,
        "scenario_version": SCENARIO_VERSION,
        "timeout": {
            "driver_response_seconds": DEFAULT_RESPONSE_TIMEOUT,
            "frame_waits": "scenario-bounded",
        },
    }


def failure_status(error: BaseException) -> str:
    if isinstance(error, DriverTimeout):
        return "timed-out"
    if isinstance(error, ProtocolError):
        return "runner-crashed"
    return "failed"


def stage_failure_artifacts(name: str, source: Path) -> Path:
    destination = UPLOAD_ROOT / name
    shutil.rmtree(destination, ignore_errors=True)
    for path in source.rglob("*"):
        if not path.is_file() or path.is_symlink():
            continue
        if (
            path.name not in ALLOWED_DIAGNOSTIC_NAMES
            and path.suffix.lower() not in ALLOWED_DIAGNOSTIC_SUFFIXES
        ):
            continue
        relative = path.relative_to(source)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
    return destination


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("scenarios", nargs="*")
    args = parser.parse_args(argv)
    try:
        selected = selected_scenarios(args.scenarios)
    except ValueError as error:
        parser.error(str(error))

    failed = False
    for name in selected:
        artifact_dir = ARTIFACT_ROOT / name
        shutil.rmtree(artifact_dir, ignore_errors=True)
        shutil.rmtree(UPLOAD_ROOT / name, ignore_errors=True)
        started = time.monotonic()
        report = base_report(name)
        try:
            SCENARIOS[name](artifact_dir)
        except BaseException as error:
            failed = True
            report.update(
                {
                    "duration_seconds": round(time.monotonic() - started, 3),
                    "error": f"{type(error).__name__}: {error}",
                    "failed_predicate": str(error),
                    "status": failure_status(error),
                }
            )
            write_report(artifact_dir, report)
            stage_failure_artifacts(name, artifact_dir)
            print(f"FAIL {name}: {error}", file=sys.stderr)
        else:
            report.update(
                {
                    "duration_seconds": round(time.monotonic() - started, 3),
                    "failed_predicate": None,
                    "status": "passed",
                }
            )
            write_report(artifact_dir, report)
            print(f"PASS {name}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
