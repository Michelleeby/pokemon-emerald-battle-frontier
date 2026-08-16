"""Shared normal and hard Battle Pyramid Brandon boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .pyramid import (
    MAP_NUM_PYRAMID_FLOOR,
    MAP_NUM_PYRAMID_LOBBY,
    MAP_NUM_PYRAMID_TOP,
    STREAK_PYRAMID_50,
    PyramidScenarioFailure as ScenarioFailure,
    create_pyramid_floor_save,
    pyramid_mode_addresses,
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
class BrandonMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]


NORMAL_BRANDON = BrandonMode("normal", False, FRONTIER_CHALLENGE_NORMAL, 20, 21, (140, 159))
HARD_BRANDON = BrandonMode("hard", True, FRONTIER_CHALLENGE_HARD, 13, 14, (200, 299))


def run_brandon_route(artifact_dir: Path, mode: BrandonMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"pyramid-{mode.name}-brandon.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_pyramid_floor_save(artifact_dir, save, brandon_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(MAP_NUM_PYRAMID_FLOOR):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Pyramid final floor")

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = pyramid_mode_addresses(save_block1, save_block2)
        streak_address = addresses["hard_win_streak" if mode.hard else "normal_win_streak"]
        if game.read(streak_address, width=16) != mode.start_streak:
            raise ScenarioFailure(f"{mode.name} Pyramid fixture did not start at streak {mode.start_streak}")
        if game.read(addresses["challenge_mode"], width=8) != mode.challenge_mode:
            raise ScenarioFailure(f"Pyramid challenge mode is not {mode.name} at route start")
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_ACTIVE:
            raise ScenarioFailure("Pyramid challenge is not in the active running state")
        if game.read(addresses["battle_num"], width=16) != 6:
            raise ScenarioFailure("Pyramid fixture did not start on floor 7")

        expected_maps = [
            map_id(MAP_NUM_PYRAMID_FLOOR),
            map_id(MAP_NUM_PYRAMID_TOP),
            map_id(MAP_NUM_PYRAMID_LOBBY),
        ]
        next_map = 0
        saw_ordinary = False
        saw_brandon = False
        saw_won_status = False
        for _ in range(600):
            observed_map = current_map(game, symbols["gSaveBlock1Ptr"])
            if next_map < len(expected_maps) and observed_map == expected_maps[next_map]:
                next_map += 1

            trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
            if auto_wins == 0 and mode.ordinary_pool[0] <= trainer <= mode.ordinary_pool[1]:
                saw_ordinary = True
            if trainer == TRAINER_FRONTIER_BRAIN:
                saw_brandon = True

            save_block1 = game.read(symbols["gSaveBlock1Ptr"])
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            addresses = pyramid_mode_addresses(save_block1, save_block2)
            status = game.read(addresses["challenge_status"], width=8)
            saw_won_status |= status == CHALLENGE_STATUS_WON
            if (
                saw_won_status
                and next_map == len(expected_maps)
                and status == 0
                and game.read(symbols["sLockFieldControls"], width=8) == 0
            ):
                game.run_frames(60)
                break

            if observed_map == map_id(MAP_NUM_PYRAMID_FLOOR):
                key = "A" if auto_wins == 0 else "RIGHT"
                game.press(key, held_frames=1, released_frames=29)
            elif observed_map == map_id(MAP_NUM_PYRAMID_TOP):
                game.press("A", held_frames=1, released_frames=14)
                game.press("UP", held_frames=1, released_frames=14)
            else:
                game.press("A", held_frames=1, released_frames=29)
        else:
            game.screenshot("route-timeout.png")
            raise ScenarioFailure(f"{mode.name} Brandon route did not finish")

        if not saw_ordinary:
            raise ScenarioFailure(
                f"{mode.name} Pyramid route did not use pool {mode.ordinary_pool[0]}-{mode.ordinary_pool[1]}"
            )
        if not saw_brandon:
            raise ScenarioFailure(f"{mode.name} Pyramid boundary did not battle Brandon")
        if game.read(addresses["battle_num"], width=16) != 7:
            raise ScenarioFailure(f"{mode.name} Brandon route has unexpected floor counter")
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        completed_streak = game.read(streak_address, width=16)
        if completed_streak != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Pyramid streak reached {completed_streak}, "
                f"expected {mode.completed_streak}"
            )
        active = addresses["hard_active_flags" if mode.hard else "normal_active_flags"]
        if not game.read(active, width=32) & STREAK_PYRAMID_50:
            raise ScenarioFailure(f"{mode.name} Pyramid streak is not active")
        isolated = addresses["normal_win_streak" if mode.hard else "hard_win_streak"]
        if game.read(isolated, width=16) != 0:
            raise ScenarioFailure(f"{mode.name} Pyramid route changed the other mode")
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Brandon route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
