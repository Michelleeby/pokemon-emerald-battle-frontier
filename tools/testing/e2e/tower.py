"""Shared fixture, navigation, and state helpers for Battle Tower scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols


ROOT = Path(__file__).resolve().parents[3]
FIXTURE_ROM = ROOT / "build" / "e2e" / "fixtures" / "tower-lobby.gba"
FIXTURE_ELF = FIXTURE_ROM.with_suffix(".elf")
GAMEPLAY_ROM = ROOT / "build" / "e2e" / "gameplay" / "pokeemerald.gba"
GAMEPLAY_ELF = GAMEPLAY_ROM.with_suffix(".elf")

FIXTURE_SAVED = 0x45324532
MAP_GROUP_TOWER = 0x1A
MAP_NUM_TOWER_LOBBY = 5
MAP_NUM_TOWER_ELEVATOR = 6
MAP_NUM_TOWER_CORRIDOR = 7
MAP_NUM_TOWER_BATTLE_ROOM = 8
CHALLENGE_STATUS_SAVING = 1
CHALLENGE_STATUS_PAUSED = 2
CHALLENGE_STATUS_WON = 3
STREAK_TOWER_SINGLES_50 = 1 << 0
E2E_AUTO_WIN_COUNT_SYMBOL = "gE2EAutoWinCount"


class TowerScenarioFailure(E2EError):
    """The game did not satisfy a Battle Tower scenario predicate."""


def map_id(map_num: int) -> int:
    return (map_num << 8) | MAP_GROUP_TOWER


def current_map(game: Session, save_block1_ptr: int) -> int | None:
    save_block1 = game.read(save_block1_ptr)
    if not 0x02000000 <= save_block1 < 0x02040000:
        return None
    return game.read(save_block1 + 4, width=16)


def wait_for_value(
    game: Session,
    address: int,
    expected: int,
    *,
    width: int = 8,
    max_frames: int = 600,
) -> None:
    game.wait(address, expected, width=width, max_frames=max_frames)


def advance_until(game: Session, address: int, expected: int, key: str) -> None:
    for _ in range(20):
        if game.read(address, width=8) == expected:
            return
        game.press(key, held_frames=1, released_frames=29)
    raise TowerScenarioFailure(
        f"predicate at {address:#x} did not become {expected:#x} while pressing {key}"
    )


def create_tower_lobby_save(
    artifact_dir: Path,
    save: Path,
    *,
    anabel_mode: str | None = None,
) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        keys = []
        if anabel_mode == "normal":
            keys.append("START")
        elif anabel_mode == "hard":
            keys.append("SELECT")
        elif anabel_mode is not None:
            raise ValueError(f"unknown Tower Anabel fixture mode: {anabel_mode}")
        fixture.set_keys(*keys)
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def wait_for_tower_lobby(
    game: Session,
    save_block1_ptr: int,
    map_header: int,
    expected_layout: int,
    player_avatar: int,
    object_events: int,
) -> int:
    for _ in range(60):
        save_block1 = game.read(save_block1_ptr)
        if 0x02000000 <= save_block1 < 0x02040000:
            player_object_id = game.read(player_avatar + 5, width=8)
            player_is_active = (
                player_object_id < 16
                and game.read(object_events + player_object_id * 0x24, width=8) & 1
            )
            if (
                current_map(game, save_block1_ptr) == map_id(MAP_NUM_TOWER_LOBBY)
                and game.read(map_header) == expected_layout
                and player_is_active
            ):
                return save_block1
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise TowerScenarioFailure(
        "Continue did not reach the Battle Tower lobby in 3600 frames"
    )


def frontier_addresses(save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "lvl_mode": save_block2 + 0xCA9,
        "selected_party": save_block2 + 0xCAA,
        "battle_num": save_block2 + 0xCB2,
        "active_flags": save_block2 + 0xCDC,
        "win_streak": save_block2 + 0xCE0,
        "challenge_mode": save_block2 + 0xD09,
    }


def tower_mode_addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    addresses = frontier_addresses(save_block2)
    addresses.update(
        {
            "hard_active_flags": save_block1 + 0x3598,
            "hard_win_streak": save_block1 + 0x359C,
        }
    )
    return addresses


def select_first_three_party_members(game: Session, selected_order: int) -> None:
    wait_for_value(game, selected_order, 0, width=8, max_frames=600)
    for index in range(3):
        game.press("A", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
        wait_for_value(game, selected_order + index, index + 1, width=8)
        if index != 2:
            game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=60)


def start_singles_level_50(
    game: Session,
    special_result: int,
    lock_field_controls: int,
    selected_order: int,
    main: int,
    party_menu_callback: int,
    *,
    hard: bool = False,
) -> None:
    game.press("A", held_frames=1, released_frames=2)
    wait_for_value(game, lock_field_controls, 1)
    advance_until(game, special_result, 0xFF, "A")

    game.press("A", held_frames=1, released_frames=29)
    wait_for_value(game, special_result, 0, width=16)
    advance_until(game, special_result, 0xFF, "A")
    if hard:
        game.run_frames(30)
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=29)

    for _ in range(30):
        if game.read(main + 4) == party_menu_callback | 1:
            break
        game.press("A", held_frames=1, released_frames=29)
    else:
        raise TowerScenarioFailure("party selection did not open")

    game.run_frames(120)
    select_first_three_party_members(game, selected_order)

    advance_until(game, special_result, 0xFF, "A")
    game.press("A", held_frames=1, released_frames=29)


def complete_tower_route(
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
        map_id(MAP_NUM_TOWER_BATTLE_ROOM),
        map_id(MAP_NUM_TOWER_LOBBY),
    ]
    next_map = 0
    observed_battles = 0
    saw_won_status = False
    saw_expected_pool_trainer = False

    for _ in range(2400):
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
            addresses = tower_mode_addresses(save_block1, save_block2)
            battle_num = game.read(addresses["battle_num"], width=16)
            if battle_num > observed_battles:
                if battle_num != observed_battles + 1 or battle_num > 1:
                    raise TowerScenarioFailure(
                        "Tower battle number jumped from "
                        f"{observed_battles} to {battle_num}"
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
                    raise TowerScenarioFailure(
                        "Tower route did not select an ordinary trainer from "
                        f"the {ordinary_pool[0]}-{ordinary_pool[1]} pool"
                    )
                return addresses

        game.press("A", held_frames=1, released_frames=29)

    game.screenshot("route-timeout.png")
    raise TowerScenarioFailure(
        f"{route_name} Tower route did not finish within 72000 input-driven frames "
        f"(challenge battles={observed_battles}, room transitions={next_map})"
    )
