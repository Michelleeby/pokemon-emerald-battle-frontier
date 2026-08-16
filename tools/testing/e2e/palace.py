"""Shared fixture and state helpers for Battle Palace scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_WON,
    FIXTURE_ELF,
    FIXTURE_ROM,
    FIXTURE_SAVED,
    current_map,
    map_id,
    wait_for_value,
)


MAP_NUM_PALACE_LOBBY = 22
MAP_NUM_PALACE_BATTLE_ROOM = 24
STREAK_PALACE_SINGLES_50 = 1 << 4


class PalaceScenarioFailure(E2EError):
    """The game did not satisfy a Battle Palace scenario predicate."""


def create_palace_battle_room_save(
    artifact_dir: Path, save: Path, *, spenser_mode: str
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        if spenser_mode == "normal":
            fixture.set_keys("A", "START")
        elif spenser_mode == "hard":
            fixture.set_keys("A", "SELECT")
        else:
            raise ValueError(f"unknown Palace Spenser fixture mode: {spenser_mode}")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def palace_mode_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xDE0,
        "hard_active_flags": save_block1 + 0x35AC,
        "hard_win_streak": save_block1 + 0x35B0,
    }


def complete_palace_route(
    game: Session,
    save_block1_ptr: int,
    save_block2_ptr: int,
    lock_field_controls: int,
    trainer_opponent: int,
    auto_win_count: int,
    *,
    route_name: str,
    ordinary_pool: tuple[int, int],
) -> dict[str, int]:
    expected_maps = [
        map_id(MAP_NUM_PALACE_BATTLE_ROOM),
        map_id(MAP_NUM_PALACE_LOBBY),
    ]
    next_map = 0
    observed_battles = 0
    saw_won_status = False
    saw_expected_pool_trainer = False

    for _ in range(3600):
        observed_map = current_map(game, save_block1_ptr)
        trainer = game.read(trainer_opponent, width=16)
        if game.read(auto_win_count, width=32) == 0 and (
            ordinary_pool[0] <= trainer <= ordinary_pool[1]
        ):
            saw_expected_pool_trainer = True
        if next_map < len(expected_maps) and observed_map == expected_maps[next_map]:
            next_map += 1

        save_block1 = game.read(save_block1_ptr)
        save_block2 = game.read(save_block2_ptr)
        if (
            0x02000000 <= save_block1 < 0x02040000
            and 0x02000000 <= save_block2 < 0x02040000
        ):
            addresses = palace_mode_addresses(save_block1, save_block2)
            battle_num = game.read(addresses["battle_num"], width=16)
            if battle_num > observed_battles:
                if battle_num != observed_battles + 1 or battle_num > 1:
                    raise PalaceScenarioFailure(
                        f"Palace battle number jumped from {observed_battles} to {battle_num}"
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
                if not saw_expected_pool_trainer:
                    raise PalaceScenarioFailure(
                        "Palace route did not select an ordinary trainer from "
                        f"the {ordinary_pool[0]}-{ordinary_pool[1]} pool"
                    )
                return addresses

        game.press("A", held_frames=1, released_frames=29)

    game.screenshot("route-timeout.png")
    raise PalaceScenarioFailure(
        f"{route_name} Palace route did not finish within 108000 input-driven frames "
        f"(challenge battles={observed_battles}, room transitions={next_map})"
    )
