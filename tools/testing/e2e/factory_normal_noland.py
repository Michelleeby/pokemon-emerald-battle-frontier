"""Prove that normal Factory retains its vanilla Noland streak boundary."""

from __future__ import annotations

from pathlib import Path

from .factory import (
    CHALLENGE_STATUS_SAVING,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    FRONTIER_CHALLENGE_NORMAL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    STREAK_FACTORY_SINGLES_50,
    FactoryScenarioFailure as ScenarioFailure,
    complete_factory_route,
    create_factory_lobby_save,
    factory_addresses,
    select_first_three_rentals,
    start_factory_singles_level_50,
    wait_for_factory_lobby,
    wait_for_value,
)
from .session import Session
from .symbols import load_symbols, require_symbols


TRAINER_FRONTIER_BRAIN = 1022
NORMAL_NOLAND_START_STREAK = 20
NORMAL_NOLAND_COMPLETED_STREAK = 21


def _assert_completed_state(game: Session, addresses: dict[str, int]) -> None:
    battle_num = game.read(addresses["battle_num"], width=16)
    if battle_num != 0:
        raise ScenarioFailure(
            "normal Noland route changed the ordinary Factory battle counter "
            f"to {battle_num}"
        )

    challenge_mode = game.read(addresses["challenge_mode"], width=8)
    if challenge_mode != FRONTIER_CHALLENGE_NORMAL:
        raise ScenarioFailure(
            f"Factory challenge mode is {challenge_mode}, expected normal"
        )

    win_streak = game.read(addresses["normal_win_streak"], width=16)
    if win_streak != NORMAL_NOLAND_COMPLETED_STREAK:
        raise ScenarioFailure(
            "normal Factory Singles Lv. 50 streak is "
            f"{win_streak}, expected {NORMAL_NOLAND_COMPLETED_STREAK}"
        )

    active_flags = game.read(addresses["normal_active_flags"], width=32)
    if not active_flags & STREAK_FACTORY_SINGLES_50:
        raise ScenarioFailure("normal Factory Singles Lv. 50 streak is not active")

    if game.read(addresses["hard_win_streak"], width=16) != 0:
        raise ScenarioFailure("normal Factory route changed the hard-mode streak")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "factory-normal-noland.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gMain",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "gPlayerPartyCount",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSpecialVar_Result",
        "gTrainerBattleOpponent_A",
        "BattleFrontier_BattleFactoryLobby_Layout",
        "CB2_SelectScreen",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )

    create_factory_lobby_save(artifact_dir, save, noland_mode="normal")

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        save_block1 = wait_for_factory_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleFactoryLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = factory_addresses(save_block1, save_block2)
        if (
            game.read(addresses["normal_win_streak"], width=16)
            != NORMAL_NOLAND_START_STREAK
        ):
            raise ScenarioFailure("normal Factory fixture did not start at streak 20")

        game.run_frames(120)
        wait_for_value(game, symbols["sLockFieldControls"], 0)
        start_factory_singles_level_50(
            game,
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            symbols["gMain"],
            symbols["CB2_SelectScreen"],
            hard=False,
        )
        select_first_three_rentals(
            game,
            symbols["gMain"],
            symbols["CB2_SelectScreen"],
            symbols["gPlayerPartyCount"],
        )

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = factory_addresses(save_block1, save_block2)
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Factory challenge did not enter the saving state")

        addresses = complete_factory_route(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gSaveBlock2Ptr"],
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            expected_battles=1,
            route_name="normal Noland",
        )
        if (
            game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            != TRAINER_FRONTIER_BRAIN
        ):
            raise ScenarioFailure("normal Factory streak 20 did not battle Noland")
        _assert_completed_state(game, addresses)

        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != 1:
            raise ScenarioFailure(
                f"normal Noland route used {auto_wins} assisted outcomes, expected 1"
            )
        game.screenshot("passed.png")
