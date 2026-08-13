"""Battle Tower lobby cancellation end-to-end scenario."""

from __future__ import annotations

from pathlib import Path

from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    MAP_NUM_TOWER_LOBBY,
    RELEASE_ELF,
    RELEASE_ROM,
    TowerScenarioFailure as ScenarioFailure,
    advance_until,
    create_tower_lobby_save,
    map_id,
    wait_for_tower_lobby,
    wait_for_value,
)


MULTI_B_PRESSED = 127


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "tower-lobby.sav"
    release_symbols = require_symbols(
        load_symbols(RELEASE_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSpecialVar_Result",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "BattleFrontier_BattleTowerLobby_Layout",
        "sLockFieldControls",
    )

    create_tower_lobby_save(artifact_dir, save)

    with Session(RELEASE_ROM, artifact_dir / "scenario", save=save) as game:
        save_block1 = wait_for_tower_lobby(
            game,
            release_symbols["gSaveBlock1Ptr"],
            release_symbols["gMapHeader"],
            release_symbols["BattleFrontier_BattleTowerLobby_Layout"],
            release_symbols["gPlayerAvatar"],
            release_symbols["gObjectEvents"],
        )
        game.run_frames(120)
        wait_for_value(game, release_symbols["sLockFieldControls"], 0)

        if game.read(save_block1, width=16) != 6 or game.read(save_block1 + 2, width=16) != 6:
            raise ScenarioFailure("fixture did not load at Tower lobby coordinate (6, 6)")

        save_block2 = game.read(release_symbols["gSaveBlock2Ptr"])
        challenge_status = save_block2 + 0xCA8
        initial_status = game.read(challenge_status, width=8)

        player_object_id = game.read(release_symbols["gPlayerAvatar"] + 5, width=8)
        player_facing = game.read(
            release_symbols["gObjectEvents"] + player_object_id * 0x24 + 0x18,
            width=16,
        ) & 0xF
        if player_facing != 2:
            raise ScenarioFailure(f"fixture player faces {player_facing}, not north")

        game.press("A", held_frames=1, released_frames=2)
        wait_for_value(game, release_symbols["sLockFieldControls"], 1)
        advance_until(game, release_symbols["gSpecialVar_Result"], 0xFF, "A")

        game.press("B", held_frames=1, released_frames=2)
        wait_for_value(
            game,
            release_symbols["gSpecialVar_Result"],
            MULTI_B_PRESSED,
            width=16,
        )
        advance_until(game, release_symbols["sLockFieldControls"], 0, "A")

        final_status = game.read(challenge_status, width=8)
        if final_status != initial_status:
            raise ScenarioFailure(
                f"Tower challenge status changed from {initial_status} to {final_status}"
            )
        if game.read(save_block1 + 4, width=16) != map_id(MAP_NUM_TOWER_LOBBY):
            raise ScenarioFailure("cancellation left the Battle Tower lobby")

        game.screenshot("passed.png")
