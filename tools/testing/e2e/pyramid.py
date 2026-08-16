"""Shared fixture and state helpers for Battle Pyramid scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import FIXTURE_ELF, FIXTURE_ROM, FIXTURE_SAVED, wait_for_value


MAP_NUM_PYRAMID_LOBBY = 25
MAP_NUM_PYRAMID_FLOOR = 26
MAP_NUM_PYRAMID_TOP = 27
STREAK_PYRAMID_50 = 1 << 12


class PyramidScenarioFailure(E2EError):
    """The game did not satisfy a Battle Pyramid scenario predicate."""


def create_pyramid_floor_save(
    artifact_dir: Path, save: Path, *, brandon_mode: str
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        if brandon_mode == "normal":
            fixture.set_keys("L", "B")
        elif brandon_mode == "hard":
            fixture.set_keys("L", "B", "SELECT")
        else:
            raise ValueError(f"unknown Pyramid Brandon fixture mode: {brandon_mode}")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def pyramid_mode_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xE32,
        "hard_active_flags": save_block1 + 0x3600,
        "hard_win_streak": save_block1 + 0x3604,
    }
