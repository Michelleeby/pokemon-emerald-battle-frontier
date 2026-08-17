"""Capture-only save fixtures that stay independent of the e2e test fixture."""

from __future__ import annotations

from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from e2e.tower import wait_for_value  # noqa: E402


CAPTURE_FIXTURE_ROM = ROOT / "build" / "capture" / "fixtures" / "battle-dome-tucker.gba"
CAPTURE_FIXTURE_ELF = ROOT / "build" / "capture" / "fixtures" / "battle-dome-tucker.elf"
CAPTURE_FIXTURE_SAVED = 0x45324532


def _create_capture_save(
    artifact_dir: Path, save: Path, *, keys: tuple[str, ...]
) -> None:
    symbols = require_symbols(
        load_symbols(CAPTURE_FIXTURE_ELF), "gCaptureFixtureStatus"
    )
    with Session(CAPTURE_FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        fixture.set_keys(*keys)
        wait_for_value(
            fixture,
            symbols["gCaptureFixtureStatus"],
            CAPTURE_FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def create_battle_dome_tucker_save(artifact_dir: Path, save: Path) -> None:
    _create_capture_save(artifact_dir, save, keys=("L",))


def create_capture_tower_save(
    artifact_dir: Path, save: Path, *, anabel: bool = False
) -> None:
    _create_capture_save(artifact_dir, save, keys=("START",) if anabel else ())


_FACILITY_KEYS = {
    "battle-tower": ("LEFT",),
    "battle-dome": ("LEFT", "A"),
    "battle-factory": ("LEFT", "B"),
    "battle-palace": ("LEFT", "L"),
    "battle-arena": ("LEFT", "R"),
    "battle-pike": ("LEFT", "START"),
    "battle-pyramid": ("LEFT", "SELECT"),
}


def create_frontier_facility_save(
    artifact_dir: Path, save: Path, facility: str
) -> None:
    """Create a May save positioned outside one Frontier facility."""

    try:
        keys = _FACILITY_KEYS[facility]
    except KeyError as error:
        raise ValueError(f"unknown Frontier facility: {facility}") from error
    _create_capture_save(artifact_dir, save, keys=keys)
