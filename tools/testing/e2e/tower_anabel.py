"""Shared normal and hard Battle Tower Anabel boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    MAP_NUM_TOWER_BATTLE_ROOM,
    STREAK_TOWER_SINGLES_50,
    TowerScenarioFailure as ScenarioFailure,
    complete_tower_route,
    create_tower_lobby_save,
    current_map,
    map_id,
    tower_mode_addresses,
)


TRAINER_FRONTIER_BRAIN = 1022
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1
NORMAL_START_STREAK = 33
NORMAL_COMPLETED_STREAK = 35
HARD_START_STREAK = 19
HARD_COMPLETED_STREAK = 21


@dataclass(frozen=True)
class AnabelMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]


NORMAL_ANABEL = AnabelMode(
    name="normal",
    hard=False,
    challenge_mode=FRONTIER_CHALLENGE_NORMAL,
    start_streak=NORMAL_START_STREAK,
    completed_streak=NORMAL_COMPLETED_STREAK,
    ordinary_pool=(140, 179),
)
HARD_ANABEL = AnabelMode(
    name="hard",
    hard=True,
    challenge_mode=FRONTIER_CHALLENGE_HARD,
    start_streak=HARD_START_STREAK,
    completed_streak=HARD_COMPLETED_STREAK,
    ordinary_pool=(200, 299),
)


def run_anabel_route(artifact_dir: Path, mode: AnabelMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"tower-{mode.name}-anabel.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )

    create_tower_lobby_save(artifact_dir, save, anabel_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_TOWER_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Tower battle room")
        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = tower_mode_addresses(save_block1, save_block2)
        streak_address = (
            addresses["hard_win_streak"] if mode.hard else addresses["win_streak"]
        )
        if game.read(streak_address, width=16) != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Tower fixture did not start at streak {mode.start_streak}"
            )

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = tower_mode_addresses(save_block1, save_block2)
        challenge_mode = game.read(addresses["challenge_mode"], width=8)
        if challenge_mode != mode.challenge_mode:
            raise ScenarioFailure(
                f"Tower challenge mode is {challenge_mode}, expected {mode.name}"
            )
        streak_address = (
            addresses["hard_win_streak"] if mode.hard else addresses["win_streak"]
        )
        preserved_streak = game.read(streak_address, width=16)
        if preserved_streak != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Tower streak became {preserved_streak} during entry, "
                f"expected {mode.start_streak}"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Tower challenge did not enter the active saving state")

        addresses = complete_tower_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["sLockFieldControls"],
            symbols["gTrainerBattleOpponent_A"],
            symbols[E2E_AUTO_WIN_COUNT_SYMBOL],
            route_name=f"{mode.name} Anabel",
            ordinary_pool=mode.ordinary_pool,
        )
        if (
            game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            != TRAINER_FRONTIER_BRAIN
        ):
            raise ScenarioFailure(f"{mode.name} Tower boundary did not battle Anabel")

        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure(
                f"{mode.name} Anabel route has unexpected challenge battle number"
            )
        streak_address = (
            addresses["hard_win_streak"] if mode.hard else addresses["win_streak"]
        )
        if game.read(streak_address, width=16) != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Tower streak did not reach {mode.completed_streak}"
            )
        active_address = (
            addresses["hard_active_flags"] if mode.hard else addresses["active_flags"]
        )
        if not game.read(active_address, width=32) & STREAK_TOWER_SINGLES_50:
            raise ScenarioFailure(f"{mode.name} Tower streak is not active")
        isolated_address = (
            addresses["win_streak"] if mode.hard else addresses["hard_win_streak"]
        )
        if game.read(isolated_address, width=16) != 0:
            raise ScenarioFailure(
                f"{mode.name} Tower route changed the other mode's streak"
            )

        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Anabel route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
