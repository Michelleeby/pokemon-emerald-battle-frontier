"""Verify the modified hard-mode Factory rental IVs and trainer pool."""

from __future__ import annotations

from pathlib import Path

from .factory import (
    CHALLENGE_STATUS_SAVING,
    FRONTIER_CHALLENGE_HARD,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    FactoryScenarioFailure as ScenarioFailure,
    create_factory_lobby_save,
    factory_addresses,
    select_first_three_rentals,
    start_factory_singles_level_50,
    wait_for_factory_lobby,
    wait_for_value,
)
from .session import Session
from .symbols import load_symbols, require_symbols


HARD_POOL_FIRST_TRAINER = 200
HARD_POOL_LAST_TRAINER = 299
HARD_RENTAL_IV = 31


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "factory-hard-setup.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gMain",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "gPlayerParty",
        "gPlayerPartyCount",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSpecialVar_Result",
        "gTrainerBattleOpponent_A",
        "BattleFrontier_BattleFactoryLobby_Layout",
        "CB2_SelectScreen",
        "sLockFieldControls",
    )

    create_factory_lobby_save(artifact_dir, save)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        wait_for_factory_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleFactoryLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        game.run_frames(120)
        wait_for_value(game, symbols["sLockFieldControls"], 0)
        start_factory_singles_level_50(
            game,
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            symbols["gMain"],
            symbols["CB2_SelectScreen"],
            hard=True,
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
        if game.read(addresses["challenge_mode"], width=8) != FRONTIER_CHALLENGE_HARD:
            raise ScenarioFailure("Factory setup did not enter hard mode")
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Factory hard setup did not enter the saving state")

        for index in range(3):
            rental_ivs = game.read(addresses["rental_mons"] + index * 12 + 8, width=8)
            if rental_ivs != HARD_RENTAL_IV:
                raise ScenarioFailure(
                    f"hard Factory rental {index} has IV {rental_ivs}, expected 31"
                )

        trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
        if not HARD_POOL_FIRST_TRAINER <= trainer <= HARD_POOL_LAST_TRAINER:
            raise ScenarioFailure(
                f"hard Factory opponent {trainer} is outside the 200-299 pool"
            )
        game.screenshot("passed.png")
