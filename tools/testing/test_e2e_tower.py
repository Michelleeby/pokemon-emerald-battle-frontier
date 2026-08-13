#!/usr/bin/env python3

from __future__ import annotations

import unittest

from e2e.tower import (
    MAP_NUM_TOWER_BATTLE_ROOM,
    MAP_NUM_TOWER_CORRIDOR,
    MAP_NUM_TOWER_ELEVATOR,
    MAP_NUM_TOWER_LOBBY,
    TowerScenarioFailure,
    advance_until,
    current_map,
    frontier_addresses,
    map_id,
    select_first_three_party_members,
)


class FakeSession:
    def __init__(self, reads: dict[tuple[int, int], list[int] | int]) -> None:
        self.reads = reads
        self.presses: list[tuple[str, int, int]] = []
        self.waits: list[tuple[int, int, int, int]] = []

    def read(self, address: int, *, width: int = 32) -> int:
        value = self.reads[(address, width)]
        if isinstance(value, list):
            if len(value) > 1:
                return value.pop(0)
            return value[0]
        return value

    def press(self, key: str, *, held_frames: int, released_frames: int) -> None:
        self.presses.append((key, held_frames, released_frames))

    def wait(
        self, address: int, expected: int, *, width: int, max_frames: int
    ) -> None:
        self.waits.append((address, expected, width, max_frames))


class TowerHelperTests(unittest.TestCase):
    def test_map_ids_cover_the_ordered_singles_route(self) -> None:
        self.assertEqual(
            [
                map_id(MAP_NUM_TOWER_LOBBY),
                map_id(MAP_NUM_TOWER_ELEVATOR),
                map_id(MAP_NUM_TOWER_CORRIDOR),
                map_id(MAP_NUM_TOWER_BATTLE_ROOM),
            ],
            [0x051A, 0x061A, 0x071A, 0x081A],
        )

    def test_current_map_rejects_an_uninitialized_save_pointer(self) -> None:
        game = FakeSession({(0x3000000, 32): 0})
        self.assertIsNone(current_map(game, 0x3000000))

    def test_current_map_reads_the_save_location(self) -> None:
        game = FakeSession(
            {
                (0x3000000, 32): 0x2020000,
                (0x2020004, 16): map_id(MAP_NUM_TOWER_CORRIDOR),
            }
        )
        self.assertEqual(
            current_map(game, 0x3000000), map_id(MAP_NUM_TOWER_CORRIDOR)
        )

    def test_frontier_addresses_match_the_save_structure(self) -> None:
        self.assertEqual(
            frontier_addresses(0x2020000),
            {
                "challenge_status": 0x2020CA8,
                "lvl_mode": 0x2020CA9,
                "selected_party": 0x2020CAA,
                "battle_num": 0x2020CB2,
                "active_flags": 0x2020CDC,
                "win_streak": 0x2020CE0,
                "challenge_mode": 0x2020D09,
            },
        )

    def test_advance_until_uses_bounded_input(self) -> None:
        game = FakeSession({(0x2020000, 8): [0, 0, 0xFF]})
        advance_until(game, 0x2020000, 0xFF, "A")
        self.assertEqual(game.presses, [("A", 1, 29), ("A", 1, 29)])

    def test_advance_until_reports_an_unmet_predicate(self) -> None:
        game = FakeSession({(0x2020000, 8): 0})
        with self.assertRaisesRegex(TowerScenarioFailure, "did not become 0xff"):
            advance_until(game, 0x2020000, 0xFF, "A")
        self.assertEqual(len(game.presses), 20)

    def test_party_selection_records_first_three_in_order(self) -> None:
        game = FakeSession({})
        select_first_three_party_members(game, 0x2021000)
        self.assertEqual(
            game.waits,
            [
                (0x2021000, 0, 8, 600),
                (0x2021000, 1, 8, 600),
                (0x2021001, 2, 8, 600),
                (0x2021002, 3, 8, 600),
            ],
        )
        self.assertEqual(
            [key for key, _, _ in game.presses],
            ["A", "A", "DOWN", "A", "A", "DOWN", "A", "A", "A"],
        )


if __name__ == "__main__":
    unittest.main()
