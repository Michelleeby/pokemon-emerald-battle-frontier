"""Shared fixture, navigation, state, and rental helpers for Factory routes."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import (
    E2E_AUTO_WIN_COUNT_SYMBOL,
    FIXTURE_ELF,
    FIXTURE_ROM,
    FIXTURE_SAVED,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    advance_until,
    current_map,
    map_id,
    wait_for_value,
)


MAP_NUM_FACTORY_LOBBY = 31
MAP_NUM_FACTORY_PRE_BATTLE_ROOM = 32
MAP_NUM_FACTORY_BATTLE_ROOM = 33
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1
STREAK_FACTORY_SINGLES_50 = 1 << 8
CHALLENGE_STATUS_SAVING = 1
CHALLENGE_STATUS_WON = 3


class FactoryScenarioFailure(E2EError):
    """The game did not satisfy a Battle Factory scenario predicate."""


def create_factory_lobby_save(
    artifact_dir: Path,
    save: Path,
    *,
    noland_mode: str | None = None,
) -> None:
    """Create a checksummed Factory lobby save through the fixture ROM."""

    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        if noland_mode == "normal":
            fixture.set_keys("B", "START")
        elif noland_mode == "hard":
            fixture.set_keys("B", "SELECT")
        elif noland_mode is None:
            fixture.set_keys("B")
        else:
            raise ValueError(f"unknown Factory Noland fixture mode: {noland_mode}")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def wait_for_factory_lobby(
    game: Session,
    save_block1_ptr: int,
    map_header: int,
    expected_layout: int,
    player_avatar: int,
    object_events: int,
) -> int:
    """Select Continue and wait for an active player in the Factory lobby."""

    for _ in range(60):
        save_block1 = game.read(save_block1_ptr)
        if 0x02000000 <= save_block1 < 0x02040000:
            player_object_id = game.read(player_avatar + 5, width=8)
            player_is_active = (
                player_object_id < 16
                and game.read(object_events + player_object_id * 0x24, width=8) & 1
            )
            if (
                current_map(game, save_block1_ptr) == map_id(MAP_NUM_FACTORY_LOBBY)
                and game.read(map_header) == expected_layout
                and player_is_active
            ):
                return save_block1
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise FactoryScenarioFailure(
        "Continue did not reach the Battle Factory lobby in 3600 frames"
    )


def factory_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    """Return common and mode-specific Factory save-state addresses."""

    return {
        "challenge_status": save_block2 + 0xCA8,
        "lvl_mode": save_block2 + 0xCA9,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        # The added hard-mode Dome arrays shift fields after dome state by 0x18
        # from the vanilla offset comments retained in struct BattleFrontier.
        "normal_win_streak": save_block2 + 0xDFA,
        "normal_rents_count": save_block2 + 0xE0E,
        "rental_mons": save_block2 + 0xE88,
        "hard_active_flags": save_block1 + 0x35CC,
        "hard_win_streak": save_block1 + 0x35D0,
        "hard_rents_count": save_block1 + 0x35E0,
    }


def start_factory_singles_level_50(
    game: Session,
    special_result: int,
    lock_field_controls: int,
    main: int,
    select_screen_callback: int,
    *,
    hard: bool,
) -> None:
    """Enter a Singles Lv. 50 Factory challenge through production menus."""

    game.press("A", held_frames=1, released_frames=2)
    wait_for_value(game, lock_field_controls, 1)
    advance_until(game, special_result, 0xFF, "A")

    game.press("A", held_frames=1, released_frames=29)
    wait_for_value(game, special_result, 0, width=16)
    advance_until(game, special_result, 0xFF, "A")
    if hard:
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=29)

    advance_until(game, special_result, 0xFF, "A")
    game.press("A", held_frames=1, released_frames=29)

    for _ in range(80):
        if game.read(main + 4) == select_screen_callback | 1:
            return
        game.press("A", held_frames=1, released_frames=29)
    raise FactoryScenarioFailure("Factory rental selection did not open")


def select_first_three_rentals(
    game: Session,
    main: int,
    select_screen_callback: int,
    player_party_count: int,
) -> None:
    """Rent the first three generated Pokémon and confirm the selection."""

    for _ in range(20):
        if game.read(main + 4) == select_screen_callback | 1:
            break
        game.run_frames(30)
    else:
        raise FactoryScenarioFailure("Factory rental screen was not ready")

    game.run_frames(120)
    for index in range(3):
        game.press("A", held_frames=1, released_frames=59)
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=59)
        if index != 2:
            game.press("RIGHT", held_frames=1, released_frames=29)

    game.press("A", held_frames=1, released_frames=60)
    for _ in range(30):
        callback = game.read(main + 4)
        party_count = game.read(player_party_count, width=8)
        if callback != select_screen_callback | 1 and party_count == 3:
            return
        game.run_frames(30)
    raise FactoryScenarioFailure("Factory rental selection did not finish")


def _decline_swap_and_continue(game: Session, special_result: int) -> None:
    """Choose Go On, advance the opponent briefing, and decline the swap."""

    advance_until(game, special_result, 0xFF, "A")
    game.press("A", held_frames=1, released_frames=29)
    advance_until(game, special_result, 0xFF, "A")
    game.press("B", held_frames=1, released_frames=29)


def _decline_swap_before_brain(game: Session, special_result: int) -> None:
    """Choose Go On and decline the otherwise optional pre-Brain swap."""

    for _ in range(60):
        if game.read(special_result, width=8) == 0xFF:
            break
        game.press("A", held_frames=1, released_frames=29)
    else:
        raise FactoryScenarioFailure("Factory Brain preparation menu did not open")
    game.press("A", held_frames=1, released_frames=29)
    advance_until(game, special_result, 0xFF, "A")
    game.press("B", held_frames=1, released_frames=29)


def complete_factory_route(
    game: Session,
    save_block1_ptr: int,
    save_block2_ptr: int,
    special_result: int,
    lock_field_controls: int,
    *,
    expected_battles: int,
    expected_pre_battle_visits: int | None = None,
    route_name: str,
) -> dict[str, int]:
    """Drive a Factory route until its completed challenge returns control."""

    observed_battles = 0
    battle_room_visits = 0
    pre_battle_room_visits = 0
    saw_won_status = False
    handled_returns: set[int] = set()
    previous_map: int | None = None

    for _ in range(4200):
        observed_map = current_map(game, save_block1_ptr)
        if observed_map != previous_map:
            if observed_map == map_id(MAP_NUM_FACTORY_BATTLE_ROOM):
                battle_room_visits += 1
            elif observed_map == map_id(MAP_NUM_FACTORY_PRE_BATTLE_ROOM):
                pre_battle_room_visits += 1
            previous_map = observed_map

        save_block1 = game.read(save_block1_ptr)
        save_block2 = game.read(save_block2_ptr)
        if (
            0x02000000 <= save_block1 < 0x02040000
            and 0x02000000 <= save_block2 < 0x02040000
        ):
            addresses = factory_addresses(save_block1, save_block2)
            battle_num = game.read(addresses["battle_num"], width=16)
            if battle_num > observed_battles:
                if (
                    battle_num != observed_battles + 1
                    or battle_num > expected_battles
                ):
                    raise FactoryScenarioFailure(
                        "Factory battle number jumped from "
                        f"{observed_battles} to {battle_num}"
                    )
                observed_battles = battle_num

            status = game.read(addresses["challenge_status"], width=8)
            saw_won_status |= status == CHALLENGE_STATUS_WON
            if (
                0 < battle_num < expected_battles
                and battle_num not in handled_returns
                and observed_map == map_id(MAP_NUM_FACTORY_PRE_BATTLE_ROOM)
            ):
                if expected_battles == 2:
                    _decline_swap_before_brain(game, special_result)
                else:
                    _decline_swap_and_continue(game, special_result)
                handled_returns.add(battle_num)
                continue

            if (
                saw_won_status
                and observed_map == map_id(MAP_NUM_FACTORY_LOBBY)
                and status == 0
                and game.read(lock_field_controls, width=8) == 0
            ):
                expected_pre_battle_visits = (
                    expected_battles
                    if expected_pre_battle_visits is None
                    else expected_pre_battle_visits
                )
                if (
                    battle_room_visits != expected_battles
                    or not expected_pre_battle_visits
                    <= pre_battle_room_visits
                    <= expected_battles
                ):
                    raise FactoryScenarioFailure(
                        "Factory room route was incomplete "
                        f"(pre-battle={pre_battle_room_visits}, "
                        f"battle={battle_room_visits})"
                    )
                return addresses

        game.press("A", held_frames=1, released_frames=29)

    game.screenshot("route-timeout.png")
    raise FactoryScenarioFailure(
        f"{route_name} Factory route did not finish within 126000 "
        f"input-driven frames (battles={observed_battles}, "
        f"pre-battle rooms={pre_battle_room_visits}, "
        f"battle rooms={battle_room_visits})"
    )
