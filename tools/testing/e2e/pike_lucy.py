"""Shared normal and hard Battle Pike Lucy boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .pike import (
    MAP_NUM_PIKE_LOBBY,
    MAP_NUM_PIKE_THREE_PATH_ROOM,
    STREAK_PIKE_50,
    PikeScenarioFailure as ScenarioFailure,
    create_pike_three_path_save,
    pike_mode_addresses,
)
from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_WON,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    current_map,
    map_id,
)


TRAINER_FRONTIER_BRAIN = 1022
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1
CHALLENGE_STATUS_ACTIVE = 99


@dataclass(frozen=True)
class LucyMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]


NORMAL_LUCY = LucyMode("normal", False, FRONTIER_CHALLENGE_NORMAL, 24, 28, (80, 119))
HARD_LUCY = LucyMode("hard", True, FRONTIER_CHALLENGE_HARD, 10, 14, (200, 299))


def run_lucy_route(artifact_dir: Path, mode: LucyMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"pike-{mode.name}-lucy.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_pike_three_path_save(artifact_dir, save, lucy_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_PIKE_THREE_PATH_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Pike three-path room")

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = pike_mode_addresses(save_block1, save_block2)
        streak_address = addresses["hard_win_streak" if mode.hard else "normal_win_streak"]
        starting_streak = game.read(streak_address, width=16)
        if starting_streak != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Pike fixture started at streak {starting_streak}, "
                f"expected {mode.start_streak}"
            )
        if game.read(addresses["challenge_mode"], width=8) != mode.challenge_mode:
            raise ScenarioFailure(f"Pike challenge mode is not {mode.name} at route start")
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_ACTIVE:
            raise ScenarioFailure("Pike challenge is not in the active running state")
        if game.read(addresses["battle_num"], width=16) != 11:
            raise ScenarioFailure("Pike fixture did not start before room 12")

        saw_ordinary = False
        saw_lucy = False
        saw_won_status = False
        for _ in range(4200):
            trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
            if auto_wins == 0 and mode.ordinary_pool[0] <= trainer <= mode.ordinary_pool[1]:
                saw_ordinary = True
            if trainer == TRAINER_FRONTIER_BRAIN:
                saw_lucy = True

            save_block1 = game.read(symbols["gSaveBlock1Ptr"])
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            addresses = pike_mode_addresses(save_block1, save_block2)
            status = game.read(addresses["challenge_status"], width=8)
            saw_won_status |= status == CHALLENGE_STATUS_WON
            if (
                saw_won_status
                and current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(MAP_NUM_PIKE_LOBBY)
                and status == 0
                and game.read(symbols["sLockFieldControls"], width=8) == 0
            ):
                break

            game.press("UP", held_frames=1, released_frames=14)
            game.press("A", held_frames=1, released_frames=14)
        else:
            game.screenshot("route-timeout.png")
            raise ScenarioFailure(f"{mode.name} Lucy route did not finish")

        if not saw_ordinary:
            raise ScenarioFailure(
                f"{mode.name} Pike route did not use pool {mode.ordinary_pool[0]}-{mode.ordinary_pool[1]}"
            )
        if not saw_lucy:
            raise ScenarioFailure(f"{mode.name} Pike boundary did not battle Lucy")
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        completed_streak = game.read(streak_address, width=16)
        if completed_streak != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Pike streak reached {completed_streak}, "
                f"expected {mode.completed_streak}"
            )
        isolated = addresses["normal_win_streak" if mode.hard else "hard_win_streak"]
        if game.read(isolated, width=16) != 0:
            raise ScenarioFailure(f"{mode.name} Pike route changed the other mode")
        active = addresses["hard_active_flags" if mode.hard else "normal_active_flags"]
        if not game.read(active, width=32) & STREAK_PIKE_50:
            raise ScenarioFailure(f"{mode.name} Pike streak is not active")
        if game.read(addresses["battle_num"], width=16) != 14:
            raise ScenarioFailure(f"{mode.name} Lucy route has unexpected room counter")
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Lucy route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
