"""Shared normal and hard Battle Arena Greta boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .arena import (
    MAP_NUM_ARENA_BATTLE_ROOM,
    STREAK_ARENA_50,
    ArenaScenarioFailure as ScenarioFailure,
    arena_mode_addresses,
    complete_arena_route,
    create_arena_battle_room_save,
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
class GretaMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]


NORMAL_GRETA = GretaMode(
    name="normal",
    hard=False,
    challenge_mode=FRONTIER_CHALLENGE_NORMAL,
    start_streak=26,
    completed_streak=28,
    ordinary_pool=(120, 159),
)
HARD_GRETA = GretaMode(
    name="hard",
    hard=True,
    challenge_mode=FRONTIER_CHALLENGE_HARD,
    start_streak=12,
    completed_streak=14,
    ordinary_pool=(200, 299),
)


def run_greta_route(artifact_dir: Path, mode: GretaMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"arena-{mode.name}-greta.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_arena_battle_room_save(artifact_dir, save, greta_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_ARENA_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Arena battle room")

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = arena_mode_addresses(save_block1, save_block2)
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Arena fixture did not start at streak {mode.start_streak}"
            )
        challenge_mode = game.read(addresses["challenge_mode"], width=8)
        if challenge_mode != mode.challenge_mode:
            raise ScenarioFailure(
                f"Arena challenge mode is {challenge_mode}, expected {mode.name}"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Arena challenge did not enter the active saving state")

        addresses = complete_arena_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["sLockFieldControls"],
            symbols["gTrainerBattleOpponent_A"],
            symbols[E2E_AUTO_WIN_COUNT_SYMBOL],
            route_name=f"{mode.name} Greta",
            ordinary_pool=mode.ordinary_pool,
        )
        if (
            game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            != TRAINER_FRONTIER_BRAIN
        ):
            raise ScenarioFailure(f"{mode.name} Arena boundary did not battle Greta")
        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure(
                f"{mode.name} Greta route has unexpected challenge battle number"
            )
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Arena streak did not reach {mode.completed_streak}"
            )
        active_address = addresses[
            "hard_active_flags" if mode.hard else "normal_active_flags"
        ]
        if not game.read(active_address, width=32) & STREAK_ARENA_50:
            raise ScenarioFailure(f"{mode.name} Arena streak is not active")
        isolated_address = addresses[
            "normal_win_streak" if mode.hard else "hard_win_streak"
        ]
        if game.read(isolated_address, width=16) != 0:
            raise ScenarioFailure(
                f"{mode.name} Arena route changed the other mode's streak"
            )
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Greta route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
