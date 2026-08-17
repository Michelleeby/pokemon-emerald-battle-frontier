"""Shared fixture and state helpers for Battle Pike scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import FIXTURE_ELF, FIXTURE_ROM, FIXTURE_SAVED, wait_for_value


MAP_NUM_PIKE_LOBBY = 34
MAP_NUM_PIKE_THREE_PATH_ROOM = 36
STREAK_PIKE_50 = 1 << 10


class PikeScenarioFailure(E2EError):
    """The game did not satisfy a Battle Pike scenario predicate."""


def create_pike_three_path_save(
    artifact_dir: Path, save: Path, *, lucy_mode: str
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        if lucy_mode == "normal":
            fixture.set_keys("A", "B")
        elif lucy_mode == "hard":
            fixture.set_keys("A", "B", "SELECT")
        else:
            raise ValueError(f"unknown Pike Lucy fixture mode: {lucy_mode}")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def pike_mode_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xE1C,
        "hard_active_flags": save_block1 + 0x35F0,
        "hard_win_streak": save_block1 + 0x35F4,
    }
