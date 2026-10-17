"""Unit tests for the real-save section parser and baseline importer."""

from __future__ import annotations

import hashlib
import json
import struct
import tempfile
import unittest
from pathlib import Path

from tools.testing.save_fixture import (
    NUM_SECTORS,
    SECTION_SIZES,
    SECTOR_CHECKSUM_OFFSET,
    SECTOR_COUNTER_OFFSET,
    SECTOR_ID_OFFSET,
    SECTOR_SIGNATURE,
    SECTOR_SIGNATURE_OFFSET,
    SECTOR_SIZE,
    SAVE_SIZE,
    SaveFixtureError,
    calculate_checksum,
    extract_protected_state,
    import_fixture,
    parse_save,
)


def _sections() -> dict[int, bytes]:
    sb1 = bytearray(0x3D88)
    sb2 = bytearray(0xF44)
    storage = bytearray(0x83D0)
    key = 0x12345678
    struct.pack_into("<I", sb2, 0xAC, key)
    struct.pack_into("<I", sb1, 0x490, 0xAABBCCDD ^ key)
    struct.pack_into("<H", sb1, 0x494, 321 ^ (key & 0xFFFF))
    struct.pack_into("<H", sb1, 0x496, 77)
    struct.pack_into("<HH", sb1, 0x560, 13, 12 ^ (key & 0xFFFF))
    # PC quantities are cleartext, unlike the bag quantities.
    struct.pack_into("<HH", sb1, 0x498, 99, 5)
    sections = {0: bytes(sb2)}
    for index in range(4):
        start = index * 0xF80
        end = start + SECTION_SIZES[index + 1]
        sections[index + 1] = bytes(sb1[start:end])
    for index in range(9):
        start = index * 0xF80
        end = start + SECTION_SIZES[index + 5]
        sections[index + 5] = bytes(storage[start:end])
    return sections


def _save(counters: list[tuple[int, int]], rotation: int = 3) -> bytes:
    raw = bytearray(SAVE_SIZE)
    sections = _sections()
    for bank, counter in counters:
        for section_id, data in sections.items():
            physical = (section_id + rotation) % NUM_SECTORS + bank * NUM_SECTORS
            sector = bytearray(SECTOR_SIZE)
            sector[:len(data)] = data
            struct.pack_into("<H", sector, SECTOR_ID_OFFSET, section_id)
            struct.pack_into("<I", sector, SECTOR_SIGNATURE_OFFSET, SECTOR_SIGNATURE)
            struct.pack_into("<I", sector, SECTOR_COUNTER_OFFSET, counter)
            struct.pack_into("<H", sector, SECTOR_CHECKSUM_OFFSET,
                             calculate_checksum(data))
            start = physical * SECTOR_SIZE
            raw[start:start + SECTOR_SIZE] = sector
    return bytes(raw)


class SaveFixtureTests(unittest.TestCase):
    def test_rotating_sections_and_newest_counter(self) -> None:
        parsed = parse_save(_save([(0, 41), (1, 42)], rotation=9))
        self.assertEqual(parsed.bank, 1)
        self.assertEqual(parsed.counter, 42)
        state = extract_protected_state(parsed)
        self.assertEqual(len(parsed.save_block2), 0xF44)
        self.assertEqual(len(parsed.save_block1), 0x3D88)
        self.assertEqual(state["money"], 0xAABBCCDD)
        self.assertEqual(state["coins"], 321)
        self.assertEqual(state["bag"]["items"][0], [13, 12])
        self.assertEqual(state["pc_items"][0], [99, 5])

    def test_counter_wrap_zero_is_newer_than_maximum(self) -> None:
        parsed = parse_save(_save([(0, 0xFFFFFFFF), (1, 0)], rotation=1))
        self.assertEqual((parsed.bank, parsed.counter), (1, 0))

    def test_accepts_and_hashes_mgba_rtc_trailer(self) -> None:
        raw = _save([(0, 12)])
        trailer = bytes.fromhex("52444354455354000102030405060708")
        parsed = parse_save(raw + trailer)
        self.assertEqual(parsed.raw, raw + trailer)
        self.assertEqual(parsed.source_sha256, hashlib.sha256(raw + trailer).hexdigest())
        self.assertEqual(len(parsed.save_block2), 0xF44)

    def test_incomplete_or_bad_checksum_is_rejected(self) -> None:
        raw = bytearray(_save([(0, 7)]))
        raw[SECTOR_SIZE * 2 + 17] ^= 0xFF
        with self.assertRaisesRegex(SaveFixtureError, "no complete valid save slot"):
            parse_save(bytes(raw))

    def test_import_pins_source_hash_and_manifest(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "original.sav"
            destination = root / "fixtures" / "real-save.sav"
            raw = _save([(0, 12)])
            source.write_bytes(raw)
            source_hash = hashlib.sha256(raw).hexdigest()
            result = import_fixture(source, destination)
            self.assertEqual(result["sha256"], source_hash)
            self.assertEqual(source.read_bytes(), raw)
            self.assertEqual(destination.read_bytes(), raw)
            manifest = json.loads(destination.with_suffix(".baseline.json").read_text())
            self.assertEqual(manifest["source_sha256"], source_hash)
            with self.assertRaises(SaveFixtureError):
                import_fixture(source, destination)


if __name__ == "__main__":
    unittest.main()
