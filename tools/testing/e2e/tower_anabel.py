"""Shared normal and hard Battle Tower Anabel boundary scenario."""

from __future__ import annotations

from pathlib import Path

from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    STREAK_TOWER_SINGLES_50,
    TowerScenarioFailure as ScenarioFailure,
    complete_tower_route,
    create_tower_lobby_save,
    start_singles_level_50,
    tower_mode_addresses,
    wait_for_tower_lobby,
    wait_for_value,
)


TRAINER_FRONTIER_BRAIN = 1022
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1
NORMAL_START_STREAK = 33
NORMAL_COMPLETED_STREAK = 35
HARD_START_STREAK = 19
HARD_COMPLETED_STREAK = 21


def run_anabel_route(artifact_dir: Path, *, hard: bool) -> None:
    mode = "hard" if hard else "normal"
    start_streak = HARD_START_STREAK if hard else NORMAL_START_STREAK
    completed_streak = HARD_COMPLETED_STREAK if hard else NORMAL_COMPLETED_STREAK
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"tower-{mode}-anabel.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gMain",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSelectedOrderFromParty",
        "gSpecialVar_Result",
        "gTrainerBattleOpponent_A",
        "BattleFrontier_BattleTowerLobby_Layout",
        "CB2_UpdatePartyMenu",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )

    create_tower_lobby_save(artifact_dir, save, anabel_mode=mode)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        save_block1 = wait_for_tower_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleTowerLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = tower_mode_addresses(save_block1, save_block2)
        streak_address = (
            addresses["hard_win_streak"] if hard else addresses["win_streak"]
        )
        if game.read(streak_address, width=16) != start_streak:
            raise ScenarioFailure(
                f"{mode} Tower fixture did not start at streak {start_streak}"
            )

        game.run_frames(120)
        wait_for_value(game, symbols["sLockFieldControls"], 0)
        start_singles_level_50(
            game,
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            symbols["gSelectedOrderFromParty"],
            symbols["gMain"],
            symbols["CB2_UpdatePartyMenu"],
            hard=hard,
        )

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = tower_mode_addresses(save_block1, save_block2)
        expected_mode = FRONTIER_CHALLENGE_HARD if hard else FRONTIER_CHALLENGE_NORMAL
        challenge_mode = game.read(addresses["challenge_mode"], width=8)
        if challenge_mode != expected_mode:
            raise ScenarioFailure(
                f"Tower challenge mode is {challenge_mode}, expected {mode}"
            )
        streak_address = (
            addresses["hard_win_streak"] if hard else addresses["win_streak"]
        )
        preserved_streak = game.read(streak_address, width=16)
        if preserved_streak != start_streak:
            raise ScenarioFailure(
                f"{mode} Tower streak became {preserved_streak} during entry, "
                f"expected {start_streak}"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Tower challenge did not enter the active saving state")

        addresses = complete_tower_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["sLockFieldControls"],
            route_name=f"{mode} Anabel",
        )
        if (
            game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            != TRAINER_FRONTIER_BRAIN
        ):
            raise ScenarioFailure(f"{mode} Tower boundary did not battle Anabel")

        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure(
                f"{mode} Anabel route has unexpected challenge battle number"
            )
        streak_address = (
            addresses["hard_win_streak"] if hard else addresses["win_streak"]
        )
        if game.read(streak_address, width=16) != completed_streak:
            raise ScenarioFailure(
                f"{mode} Tower streak did not reach {completed_streak}"
            )
        active_address = (
            addresses["hard_active_flags"] if hard else addresses["active_flags"]
        )
        if not game.read(active_address, width=32) & STREAK_TOWER_SINGLES_50:
            raise ScenarioFailure(f"{mode} Tower streak is not active")
        isolated_address = (
            addresses["win_streak"] if hard else addresses["hard_win_streak"]
        )
        if game.read(isolated_address, width=16) != 0:
            raise ScenarioFailure(f"{mode} Tower route changed the other mode's streak")

        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 2:
            raise ScenarioFailure(
                f"{mode} Anabel route used {auto_wins} assisted outcomes, expected 2"
            )
        game.screenshot("passed.png")
