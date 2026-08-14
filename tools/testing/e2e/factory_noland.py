"""Shared normal and hard Battle Factory Noland boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .factory import (
    CHALLENGE_STATUS_SAVING,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    FRONTIER_CHALLENGE_HARD,
    FRONTIER_CHALLENGE_NORMAL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    MAP_NUM_FACTORY_PRE_BATTLE_ROOM,
    STREAK_FACTORY_SINGLES_50,
    FactoryScenarioFailure as ScenarioFailure,
    complete_factory_route,
    create_factory_lobby_save,
    factory_addresses,
)
from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import current_map, map_id


TRAINER_FRONTIER_BRAIN = 1022


@dataclass(frozen=True)
class NolandMode:
    name: str
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]
    rental_pool: tuple[int, int]
    rental_iv: int


NORMAL_NOLAND = NolandMode(
    name="normal",
    challenge_mode=FRONTIER_CHALLENGE_NORMAL,
    start_streak=19,
    completed_streak=21,
    ordinary_pool=(100, 139),
    rental_pool=(267, 371),
    rental_iv=9,
)
HARD_NOLAND = NolandMode(
    name="hard",
    challenge_mode=FRONTIER_CHALLENGE_HARD,
    start_streak=12,
    completed_streak=14,
    ordinary_pool=(200, 299),
    rental_pool=(372, 849),
    rental_iv=31,
)


def _mode_address(addresses: dict[str, int], mode: NolandMode) -> int:
    return addresses["hard_win_streak" if mode.name == "hard" else "normal_win_streak"]


def _other_mode_address(addresses: dict[str, int], mode: NolandMode) -> int:
    return addresses["normal_win_streak" if mode.name == "hard" else "hard_win_streak"]


def run_noland_route(artifact_dir: Path, mode: NolandMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"factory-{mode.name}-noland.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gPlayerPartyCount",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSpecialVar_Result",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_factory_lobby_save(artifact_dir, save, noland_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_FACTORY_PRE_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Factory pre-battle room")

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = factory_addresses(save_block1, save_block2)
        if game.read(_mode_address(addresses, mode), width=16) != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Factory fixture did not start at streak {mode.start_streak}"
            )
        if game.read(addresses["challenge_mode"], width=8) != mode.challenge_mode:
            raise ScenarioFailure(
                f"Factory challenge mode is not {mode.name} at route start"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Factory challenge is not in the active saving state")

        for _ in range(300):
            trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            if (
                game.read(symbols["gPlayerPartyCount"], width=8) == 3
                and trainer
                and game.read(addresses["rental_mons"] + 8, width=8)
            ):
                break
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Factory rental setup did not finish")

        ordinary_trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
        if not mode.ordinary_pool[0] <= ordinary_trainer <= mode.ordinary_pool[1]:
            raise ScenarioFailure(
                f"{mode.name} ordinary opponent {ordinary_trainer} is outside pool "
                f"{mode.ordinary_pool[0]}-{mode.ordinary_pool[1]}"
            )

        addresses = complete_factory_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            expected_battles=2,
            expected_pre_battle_visits=1,
            route_name=f"{mode.name} Noland",
        )
        if game.read(symbols["gTrainerBattleOpponent_A"], width=16) != TRAINER_FRONTIER_BRAIN:
            raise ScenarioFailure(f"{mode.name} Factory boundary did not battle Noland")
        if game.read(_mode_address(addresses, mode), width=16) != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Factory streak did not reach {mode.completed_streak}"
            )
        if game.read(_other_mode_address(addresses, mode), width=16) != 0:
            raise ScenarioFailure(f"{mode.name} Factory route changed the other mode")
        active_address = addresses[
            "hard_active_flags" if mode.name == "hard" else "normal_active_flags"
        ]
        if not game.read(active_address, width=32) & STREAK_FACTORY_SINGLES_50:
            raise ScenarioFailure(f"{mode.name} Factory streak is not active")
        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure("Noland route has an unexpected completed battle counter")
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode.name} Noland route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
