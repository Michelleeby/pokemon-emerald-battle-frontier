"""Flash-backed Battle Tower save and restart end-to-end scenario."""

from __future__ import annotations

from pathlib import Path

from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_PAUSED,
    CHALLENGE_STATUS_SAVING,
    MAP_NUM_TOWER_LOBBY,
    RELEASE_ELF,
    RELEASE_ROM,
    STREAK_TOWER_SINGLES_50,
    TowerScenarioFailure as ScenarioFailure,
    advance_until,
    create_tower_lobby_save,
    frontier_addresses,
    map_id,
    start_singles_level_50,
    wait_for_tower_lobby,
    wait_for_value,
)


def _assert_saved_tower_state(game: Session, addresses: dict[str, int]) -> None:
    expected = {
        "challenge_status": CHALLENGE_STATUS_PAUSED,
        "battle_num": 1,
        "challenge_mode": 0,
    }
    for name, value in expected.items():
        width = 16 if name == "battle_num" else 8
        actual = game.read(addresses[name], width=width)
        if actual != value:
            raise ScenarioFailure(
                f"saved Tower {name} is {actual:#x}, expected {value:#x}"
            )

    lvl_mode_and_flags = game.read(addresses["lvl_mode"], width=8)
    if lvl_mode_and_flags & 0x3 != 0:
        raise ScenarioFailure("saved Tower level mode is not Lv. 50")
    if not lvl_mode_and_flags & (1 << 2):
        raise ScenarioFailure("saved Tower challenge is not marked paused")

    selected = [
        game.read(addresses["selected_party"] + index * 2, width=16)
        for index in range(3)
    ]
    if selected != [1, 2, 3]:
        raise ScenarioFailure(f"saved Tower party order is {selected}, expected [1, 2, 3]")

    active_flags = game.read(addresses["active_flags"])
    if not active_flags & STREAK_TOWER_SINGLES_50:
        raise ScenarioFailure("saved Tower Singles Lv. 50 streak is not active")


def _wait_until_tower_save_completes(
    game: Session,
    save_block2_ptr: int,
    special_result: int,
    main: int,
    party_menu_callback: int,
) -> dict[str, int]:
    for _ in range(700):
        save_block2 = game.read(save_block2_ptr)
        if 0x02000000 <= save_block2 < 0x02040000:
            addresses = frontier_addresses(save_block2)
            if game.read(addresses["challenge_status"], width=8) == CHALLENGE_STATUS_PAUSED:
                return addresses
            if (
                game.read(addresses["battle_num"], width=16) == 1
                and game.read(special_result, width=16) == 0xFF
            ):
                game.press("DOWN", held_frames=1, released_frames=29)
                game.press("DOWN", held_frames=1, released_frames=29)
                game.press("A", held_frames=1, released_frames=29)
                wait_for_value(game, special_result, 2, width=16)
                advance_until(game, special_result, 0xFF, "A")
                game.press("A", held_frames=1, released_frames=29)
        if game.read(main + 4) == party_menu_callback | 1:
            game.press("DOWN", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    game.screenshot("save-timeout.png")
    raise ScenarioFailure("paused Tower challenge save did not complete")


def _continue_and_observe_saved_status(
    game: Session, save_block2_ptr: int
) -> tuple[int, dict[str, int]]:
    for _ in range(180):
        save_block2 = game.read(save_block2_ptr)
        if 0x02000000 <= save_block2 < 0x02040000:
            addresses = frontier_addresses(save_block2)
            if game.read(addresses["challenge_status"], width=8) == CHALLENGE_STATUS_PAUSED:
                return save_block2, addresses
        game.press("START", held_frames=1, released_frames=9)
        game.press("A", held_frames=1, released_frames=9)
    raise ScenarioFailure("Continue did not load the saved Tower challenge status")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "tower-save-restart.sav"
    save.unlink(missing_ok=True)
    release_symbols = require_symbols(
        load_symbols(RELEASE_ELF),
        "gMapHeader",
        "gMain",
        "gObjectEvents",
        "gPlayerAvatar",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSelectedOrderFromParty",
        "gSpecialVar_Result",
        "BattleFrontier_BattleTowerLobby_Layout",
        "CB2_UpdatePartyMenu",
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

        save_block2 = game.read(release_symbols["gSaveBlock2Ptr"])
        addresses = frontier_addresses(save_block2)
        start_singles_level_50(
            game,
            release_symbols["gSpecialVar_Result"],
            release_symbols["sLockFieldControls"],
            release_symbols["gSelectedOrderFromParty"],
            release_symbols["gMain"],
            release_symbols["CB2_UpdatePartyMenu"],
        )
        addresses = _wait_until_tower_save_completes(
            game,
            release_symbols["gSaveBlock2Ptr"],
            release_symbols["gSpecialVar_Result"],
            release_symbols["gMain"],
            release_symbols["CB2_UpdatePartyMenu"],
        )
        _assert_saved_tower_state(game, addresses)
        game.run_frames(120)

        game.restart()
        _, loaded_addresses = _continue_and_observe_saved_status(
            game, release_symbols["gSaveBlock2Ptr"]
        )
        _assert_saved_tower_state(game, loaded_addresses)

        loaded_save_block1 = wait_for_tower_lobby(
            game,
            release_symbols["gSaveBlock1Ptr"],
            release_symbols["gMapHeader"],
            release_symbols["BattleFrontier_BattleTowerLobby_Layout"],
            release_symbols["gPlayerAvatar"],
            release_symbols["gObjectEvents"],
        )
        lobby_map = map_id(MAP_NUM_TOWER_LOBBY)
        for _ in range(60):
            current_save_block1 = game.read(release_symbols["gSaveBlock1Ptr"])
            current_save_block2 = game.read(release_symbols["gSaveBlock2Ptr"])
            current_addresses = frontier_addresses(current_save_block2)
            if (
                game.read(current_save_block1 + 4, width=16) != lobby_map
                and game.read(current_addresses["challenge_status"], width=8)
                == CHALLENGE_STATUS_SAVING
            ):
                break
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("saved Tower challenge did not resume after Continue")

        game.screenshot("passed.png")
