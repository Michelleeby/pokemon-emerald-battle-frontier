"""Shared normal and hard Battle Palace Spenser boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .palace import (
    MAP_NUM_PALACE_BATTLE_ROOM,
    STREAK_PALACE_SINGLES_50,
    PalaceScenarioFailure as ScenarioFailure,
    complete_palace_route,
    create_palace_battle_room_save,
    palace_mode_addresses,
)
from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    current_map,
    map_id,
)


TRAINER_FRONTIER_BRAIN = 1022
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1


@dataclass(frozen=True)
class SpenserMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]


NORMAL_SPENSER = SpenserMode(
    name="normal",
    hard=False,
    challenge_mode=FRONTIER_CHALLENGE_NORMAL,
    start_streak=19,
    completed_streak=21,
    ordinary_pool=(100, 139),
)
HARD_SPENSER = SpenserMode(
    name="hard",
    hard=True,
    challenge_mode=FRONTIER_CHALLENGE_HARD,
    start_streak=12,
    completed_streak=14,
    ordinary_pool=(200, 299),
)


def run_spenser_route(artifact_dir: Path, mode: SpenserMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"palace-{mode.name}-spenser.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_palace_battle_room_save(artifact_dir, save, spenser_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_PALACE_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Palace battle room")

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = palace_mode_addresses(save_block1, save_block2)
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Palace fixture did not start at streak {mode.start_streak}"
            )
        if game.read(addresses["challenge_mode"], width=8) != mode.challenge_mode:
            raise ScenarioFailure(
                f"Palace challenge mode is not {mode.name} at route start"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Palace challenge is not in the active saving state")

        addresses = complete_palace_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["sLockFieldControls"],
            symbols["gTrainerBattleOpponent_A"],
            symbols[E2E_AUTO_WIN_COUNT_SYMBOL],
            route_name=f"{mode.name} Spenser",
            ordinary_pool=mode.ordinary_pool,
        )
        if game.read(symbols["gTrainerBattleOpponent_A"], width=16) != TRAINER_FRONTIER_BRAIN:
            raise ScenarioFailure(f"{mode.name} Palace boundary did not battle Spenser")
        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure(f"{mode.name} Spenser route has unexpected battle number")
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Palace streak did not reach {mode.completed_streak}"
            )
        active_address = addresses[
            "hard_active_flags" if mode.hard else "normal_active_flags"
        ]
        if not game.read(active_address, width=32) & STREAK_PALACE_SINGLES_50:
            raise ScenarioFailure(f"{mode.name} Palace streak is not active")
        isolated_address = addresses[
            "normal_win_streak" if mode.hard else "hard_win_streak"
        ]
        if game.read(isolated_address, width=16) != 0:
            raise ScenarioFailure(f"{mode.name} Palace route changed the other mode")
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Spenser route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
