"""Complete seven-win Battle Tower route end-to-end scenario."""

from __future__ import annotations

from pathlib import Path

from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    CHALLENGE_STATUS_WON,
    MAP_NUM_TOWER_BATTLE_ROOM,
    MAP_NUM_TOWER_CORRIDOR,
    MAP_NUM_TOWER_ELEVATOR,
    MAP_NUM_TOWER_LOBBY,
    RELEASE_ELF,
    RELEASE_ROM,
    STREAK_TOWER_SINGLES_50,
    TowerScenarioFailure as ScenarioFailure,
    create_tower_lobby_save,
    current_map,
    frontier_addresses,
    map_id,
    start_singles_level_50,
    wait_for_tower_lobby,
    wait_for_value,
)


def _complete_tower_route(
    game: Session,
    save_block1_ptr: int,
    save_block2_ptr: int,
    lock_field_controls: int,
) -> dict[str, int]:
    expected_maps = [
        map_id(MAP_NUM_TOWER_ELEVATOR),
        map_id(MAP_NUM_TOWER_CORRIDOR),
        map_id(MAP_NUM_TOWER_BATTLE_ROOM),
        map_id(MAP_NUM_TOWER_LOBBY),
    ]
    next_map = 0
    observed_battles = 0
    saw_won_status = False

    for _ in range(3600):
        observed_map = current_map(game, save_block1_ptr)
        if next_map < len(expected_maps) and observed_map == expected_maps[next_map]:
            next_map += 1

        save_block2 = game.read(save_block2_ptr)
        if 0x02000000 <= save_block2 < 0x02040000:
            addresses = frontier_addresses(save_block2)
            battle_num = game.read(addresses["battle_num"], width=16)
            if battle_num > observed_battles:
                if battle_num != observed_battles + 1 or battle_num > 7:
                    raise ScenarioFailure(
                        f"Tower battle number jumped from {observed_battles} to {battle_num}"
                    )
                observed_battles = battle_num

            status = game.read(addresses["challenge_status"], width=8)
            saw_won_status |= status == CHALLENGE_STATUS_WON
            if (
                saw_won_status
                and next_map == len(expected_maps)
                and status == 0
                and game.read(lock_field_controls, width=8) == 0
            ):
                return addresses

        game.press("A", held_frames=1, released_frames=29)

    game.screenshot("route-timeout.png")
    raise ScenarioFailure(
        "seven-win Tower route did not finish within 108000 input-driven frames "
        f"(battles={observed_battles}, room transitions={next_map})"
    )


def _assert_completed_state(game: Session, addresses: dict[str, int]) -> None:
    battle_num = game.read(addresses["battle_num"], width=16)
    if battle_num != 7:
        raise ScenarioFailure(f"completed Tower battle number is {battle_num}, expected 7")

    win_streak = game.read(addresses["win_streak"], width=16)
    if win_streak != 7:
        raise ScenarioFailure(f"Tower Singles Lv. 50 streak is {win_streak}, expected 7")

    active_flags = game.read(addresses["active_flags"], width=32)
    if not active_flags & STREAK_TOWER_SINGLES_50:
        raise ScenarioFailure("completed Tower Singles Lv. 50 streak is not active")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "tower-seven-win.sav"
    save.unlink(missing_ok=True)
    release_symbols = require_symbols(
        load_symbols(RELEASE_ELF),
        "gMain",
        "gMapHeader",
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
        wait_for_tower_lobby(
            game,
            release_symbols["gSaveBlock1Ptr"],
            release_symbols["gMapHeader"],
            release_symbols["BattleFrontier_BattleTowerLobby_Layout"],
            release_symbols["gPlayerAvatar"],
            release_symbols["gObjectEvents"],
        )
        game.run_frames(120)
        wait_for_value(game, release_symbols["sLockFieldControls"], 0)

        start_singles_level_50(
            game,
            release_symbols["gSpecialVar_Result"],
            release_symbols["sLockFieldControls"],
            release_symbols["gSelectedOrderFromParty"],
            release_symbols["gMain"],
            release_symbols["CB2_UpdatePartyMenu"],
        )

        save_block2 = game.read(release_symbols["gSaveBlock2Ptr"])
        addresses = frontier_addresses(save_block2)
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Tower challenge did not enter the active saving state")

        addresses = _complete_tower_route(
            game,
            release_symbols["gSaveBlock1Ptr"],
            release_symbols["gSaveBlock2Ptr"],
            release_symbols["sLockFieldControls"],
        )
        _assert_completed_state(game, addresses)
        game.screenshot("passed.png")
