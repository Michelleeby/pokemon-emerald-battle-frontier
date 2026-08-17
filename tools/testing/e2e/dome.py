"""Shared fixture and state helpers for Battle Dome scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import FIXTURE_ELF, FIXTURE_ROM, FIXTURE_SAVED, wait_for_value


MAP_NUM_DOME_LOBBY = 18
MAP_NUM_DOME_PRE_BATTLE_ROOM = 20
DOME_ROUNDS_COUNT = 4
STREAK_DOME_SINGLES_50 = 1 << 2
STREAK_DOME_HARD_SINGLES_50 = 1 << 26
TASK_SIZE = 40
TASK_IS_ACTIVE_OFFSET = 4


class DomeScenarioFailure(E2EError):
    """The game did not satisfy a Battle Dome scenario predicate."""


def create_dome_tournament_save(
    artifact_dir: Path, save: Path, *, tucker_mode: str
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        keys = ["L"]
        if tucker_mode == "normal":
            keys.append("START")
        elif tucker_mode == "hard":
            keys.append("SELECT")
        else:
            raise ValueError(f"unknown Dome Tucker fixture mode: {tucker_mode}")
        fixture.set_keys(*keys)
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def dome_mode_addresses(save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xD0C,
        "hard_win_streak": save_block2 + 0xD24,
    }


def task_is_active(game: Session, tasks: int, task_func: int) -> bool:
    expected_func = task_func | 1
    for task_id in range(16):
        task = tasks + task_id * TASK_SIZE
        if (
            game.read(task + TASK_IS_ACTIVE_OFFSET, width=8)
            and game.read(task) == expected_func
        ):
            return True
    return False


def continue_to_dome_battle(
    game: Session, tasks: int, multichoice_task: int, *, round_index: int
) -> None:
    for _ in range(120):
        if task_is_active(game, tasks, multichoice_task):
            break
        game.press("A", held_frames=1, released_frames=29)
    else:
        raise DomeScenarioFailure(
            f"Dome round {round_index + 1} pre-battle menu did not open"
        )

    game.press("DOWN", held_frames=1, released_frames=29)
    game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=29)
