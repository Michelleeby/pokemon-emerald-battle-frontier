"""Shared fixture and state helpers for Battle Arena scenarios."""

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


MAP_NUM_ARENA_LOBBY = 28
MAP_NUM_ARENA_BATTLE_ROOM = 30
STREAK_ARENA_50 = 1 << 6


class ArenaScenarioFailure(E2EError):
    """The game did not satisfy a Battle Arena scenario predicate."""


def create_arena_battle_room_save(
    artifact_dir: Path, save: Path, *, greta_mode: str
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        if greta_mode == "normal":
            fixture.set_keys("R", "START")
        elif greta_mode == "hard":
            fixture.set_keys("R", "SELECT")
        else:
            raise ValueError(f"unknown Arena Greta fixture mode: {greta_mode}")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def arena_mode_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xDF2,
        "hard_active_flags": save_block1 + 0x35C0,
        "hard_win_streak": save_block1 + 0x35C4,
    }


def complete_arena_route(
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
    expected_maps = [map_id(MAP_NUM_ARENA_BATTLE_ROOM), map_id(MAP_NUM_ARENA_LOBBY)]
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
            addresses = arena_mode_addresses(save_block1, save_block2)
            battle_num = game.read(addresses["battle_num"], width=16)
            if battle_num > observed_battles:
                if battle_num != observed_battles + 1 or battle_num > 1:
                    raise ArenaScenarioFailure(
                        f"Arena battle number jumped from {observed_battles} to {battle_num}"
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
                    raise ArenaScenarioFailure(
                        "Arena route did not select an ordinary trainer from "
                        f"the {ordinary_pool[0]}-{ordinary_pool[1]} pool"
                    )
                return addresses

        game.press("A", held_frames=1, released_frames=29)

    game.screenshot("route-timeout.png")
    raise ArenaScenarioFailure(
        f"{route_name} Arena route did not finish within 108000 input-driven frames "
        f"(challenge battles={observed_battles}, room transitions={next_map})"
    )
