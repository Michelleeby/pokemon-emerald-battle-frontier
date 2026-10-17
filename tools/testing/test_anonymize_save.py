"""Synthetic tests for the public real-save anonymization helpers."""

from __future__ import annotations

import struct
import tempfile
import unittest
from pathlib import Path

from tools.testing.anonymize_save import anonymize_save, import_anonymized_fixture
from tools.testing.save_fixture import (
    NUM_SECTORS,
    NUM_PHYSICAL_SECTORS,
    SECTION_SIZES,
    SECTOR_CHECKSUM_OFFSET,
    SECTOR_COUNTER_OFFSET,
    SECTOR_ID_OFFSET,
    SECTOR_SIGNATURE,
    SECTOR_SIGNATURE_OFFSET,
    SECTOR_SIZE,
    SAVE_SIZE,
    SaveFixtureError,
    SUBSTRUCT_ORDER,
    calculate_checksum,
    extract_protected_state,
    parse_save,
)
from tools.testing.test_save_fixture import _sections


EOS = 0xFF
GEN3_UPPER_A = 0xBB
OLD_TRAINER_ID = 0x10203040
OLD_OWNER_IDS = (0x11111111, 0x22222222, 0x33333333)
TRAILER = bytes.fromhex("52444354455354000102030405060708")


def _gen3(text: str, width: int) -> bytes:
    encoded_values = []
    for char in text:
        if "A" <= char <= "Z":
            encoded_values.append(GEN3_UPPER_A + ord(char) - ord("A"))
        elif "0" <= char <= "9":
            encoded_values.append(0xA1 + ord(char) - ord("0"))
        else:
            raise ValueError(text)
    encoded = bytes(encoded_values)
    if len(encoded) >= width:
        raise ValueError(text)
    return encoded + bytes([EOS]) * (width - len(encoded))


def _make_pokemon(
    *,
    personality: int,
    ot_id: int,
    species: int,
    nickname: str = "SECRET",
    ot_name: str = "OTNAME",
    party: bool = False,
) -> bytes:
    size = 0x64 if party else 0x50
    raw = bytearray(size)
    struct.pack_into("<II", raw, 0, personality, ot_id)
    raw[0x08:0x12] = _gen3(nickname, 10)
    raw[0x12] = 2  # English language
    raw[19] = 2  # hasSpecies, as used by anonymize_save's active-mon check
    raw[0x14:0x1C] = _gen3(ot_name, 8)
    raw[0x1B] = 0xA5  # markings must survive the OT-name scrub

    growth = bytearray(12)
    struct.pack_into("<HHI", growth, 0, species, 7, 123456)
    growth[8] = 0x0A
    growth[9] = 77
    attacks = bytearray(12)
    struct.pack_into("<4H", attacks, 0, 33, 45, 52, 80)
    attacks[8:12] = bytes((7, 11, 3, 19))
    effort = bytearray((1, 2, 3, 4, 5, 6, 0, 0, 0, 0, 0, 0))
    misc = bytearray(12)
    ivs = (31, 30, 29, 28, 27, 26)
    ivs_ability = sum(value << shift for value, shift in zip(ivs, (0, 5, 10, 15, 20, 25)))
    ivs_ability |= 1 << 31
    struct.pack_into("<I", misc, 4, ivs_ability)

    logical = (bytes(growth), bytes(attacks), bytes(effort), bytes(misc))
    physical = [bytearray(12) for _ in range(4)]
    for logical_index, physical_index in enumerate(SUBSTRUCT_ORDER[personality % 24]):
        physical[physical_index][:] = logical[logical_index]
    secure = b"".join(physical)
    checksum = _pokemon_checksum(secure)
    struct.pack_into("<H", raw, 0x1C, checksum)
    key = personality ^ ot_id
    for offset in range(0, len(secure), 4):
        value = struct.unpack_from("<I", secure, offset)[0] ^ key
        struct.pack_into("<I", raw, 0x20 + offset, value)
    if party:
        raw[0x50:] = bytes(range(0x14))
    return bytes(raw)


def _pokemon_checksum(secure: bytes) -> int:
    return sum(struct.unpack("<24H", secure)) & 0xFFFF


def _build_sections(bank: int) -> dict[int, bytes]:
    sections = {section_id: bytearray(data) for section_id, data in _sections().items()}
    sb1 = bytearray(b"".join(bytes(sections[index]) for index in range(1, 5)))
    sb2 = sections[0]
    storage = bytearray(b"".join(bytes(sections[index]) for index in range(5, 14)))
    sb2[0:8] = _gen3("PLAYER", 8)
    struct.pack_into("<I", sb2, 0x0A, OLD_TRAINER_ID)
    struct.pack_into("<HH", sb2, 0xCE0, 41 + bank, 73 + bank)
    struct.pack_into("<H", sb1, 0x359C, 109 + bank)
    sb1[0x234] = 2
    party = _make_pokemon(
        personality=0x12345678 + bank,
        ot_id=OLD_OWNER_IDS[0],
        species=25 + bank,
        party=True,
    )
    party_two = _make_pokemon(
        personality=0x23456789 + bank,
        ot_id=OLD_OWNER_IDS[1 + bank],
        species=133 + bank,
        nickname="HIDDEN",
        party=True,
    )
    sb1[0x238:0x238 + 0x64] = party
    sb1[0x238 + 0x64:0x238 + 2 * 0x64] = party_two
    storage[4:4 + 0x50] = _make_pokemon(
        personality=0x3456789A + bank,
        ot_id=OLD_OWNER_IDS[0],
        species=152 + bank,
    )
    storage[4 + 30 * 0x50:4 + 31 * 0x50] = _make_pokemon(
        personality=0x456789AB + bank,
        ot_id=OLD_OWNER_IDS[2],
        species=172 + bank,
        nickname="BOXMON",
    )
    for index in range(14):
        start = 0x8344 + index * 9
        storage[start:start + 9] = _gen3(f"OLD{index + 1}", 9)
    rebuilt = {0: bytes(sb2)}
    start = 0
    for section_id in range(1, 5):
        end = start + SECTION_SIZES[section_id]
        rebuilt[section_id] = bytes(sb1[start:end])
        start = end
    start = 0
    for section_id in range(5, 14):
        end = start + SECTION_SIZES[section_id]
        rebuilt[section_id] = bytes(storage[start:end])
        start = end
    return rebuilt


def _make_save() -> bytes:
    raw = bytearray(SAVE_SIZE)
    for bank, rotation in enumerate((3, 8)):
        sections = _build_sections(bank)
        for section_id, data in sections.items():
            physical = (section_id + rotation) % NUM_SECTORS + bank * NUM_SECTORS
            sector = bytearray([0xC7]) * SECTOR_SIZE
            sector[:len(data)] = data
            struct.pack_into("<H", sector, SECTOR_ID_OFFSET, section_id)
            struct.pack_into("<I", sector, SECTOR_SIGNATURE_OFFSET, SECTOR_SIGNATURE)
            struct.pack_into("<I", sector, SECTOR_COUNTER_OFFSET, 41 + bank)
            struct.pack_into("<H", sector, SECTOR_CHECKSUM_OFFSET, calculate_checksum(data))
            start = physical * SECTOR_SIZE
            raw[start:start + SECTOR_SIZE] = sector
    # These sectors are outside the two valid banks and must not leak into a
    # public fixture. The RTC trailer is accepted by parse_save but omitted by
    # anonymize_save as well.
    extra_start = 2 * NUM_SECTORS * SECTOR_SIZE
    raw[extra_start:] = bytes([0xD9]) * ((NUM_PHYSICAL_SECTORS - 2 * NUM_SECTORS) * SECTOR_SIZE)
    return bytes(raw) + TRAILER


def _parse_bank(raw: bytes, bank: int):
    normalized = bytearray(SAVE_SIZE)
    for physical in range(bank * NUM_SECTORS, (bank + 1) * NUM_SECTORS):
        destination = (physical - bank * NUM_SECTORS) * SECTOR_SIZE
        source = physical * SECTOR_SIZE
        normalized[destination:destination + SECTOR_SIZE] = raw[source:source + SECTOR_SIZE]
    return parse_save(bytes(normalized))


def _set_latest_block(raw: bytearray, first_section: int, offset: int,
                      value: bytes, changed: set[int]) -> None:
    """Write one logical-block field into the newest physical bank."""
    section_count = 1 if first_section == 0 else (4 if first_section == 1 else 9)
    remaining = offset
    for section_id in range(first_section, first_section + section_count):
        size = SECTION_SIZES[section_id]
        if remaining < size:
            if remaining + len(value) > size:
                raise AssertionError("synthetic field crosses a section boundary")
            physical = NUM_SECTORS + (section_id + 8) % NUM_SECTORS
            start = physical * SECTOR_SIZE + remaining
            raw[start:start + len(value)] = value
            changed.add(section_id)
            return
        remaining -= size
    raise AssertionError(f"logical offset out of range: {offset:#x}")


def _finish_latest_sections(raw: bytearray, changed: set[int]) -> None:
    for section_id in changed:
        physical = NUM_SECTORS + (section_id + 8) % NUM_SECTORS
        start = physical * SECTOR_SIZE
        data = raw[start:start + SECTION_SIZES[section_id]]
        struct.pack_into("<H", raw, start + SECTOR_CHECKSUM_OFFSET, calculate_checksum(data))


def _seed_ancillary_identity(raw: bytes) -> bytes:
    """Add private ancillary identities to the newest logical save bank."""
    result = bytearray(raw)
    changed: set[int] = set()
    put = lambda block, offset, value: _set_latest_block(result, block, offset, value, changed)
    old_id = struct.pack("<I", 0x6A000001)

    # Normal Battle Tower records, including their stored party identities.
    for start in range(0x64C, 0xBD4, 0xEC):
        put(0, start + 4, _gen3("PRIVATE", 8))
        put(0, start + 12, old_id)
        put(0, start + 0x34 + 0x14, old_id)
        put(0, start + 0x34 + 0x20, _gen3("PRIVATEMON", 11))
    # The e-Reader trainer uses the same record shape with a different count.
    put(0, 0xBEC + 4, _gen3("EREADER", 8))
    put(0, 0xBEC + 12, old_id)
    put(0, 0xBEC + 0x34 + 0x14, old_id)
    put(0, 0xBEC + 0x34 + 0x20, _gen3("EREADERMON", 11))
    put(0, 0xBD8, _gen3("INTRV", 8))
    put(0, 0xBE0, _gen3("INTERMON", 11))
    for index in range(2):
        put(0, 0xEF9 + index * 8, _gen3("OPPO", 8))
        put(0, 0xF09 + index * 4, struct.pack("<I", 0x6A000010 + index))

    # Frontier numerical progress and the adjacent custom Dome party tail.
    put(0, 0xCE0, struct.pack("<HH", 123, 234))
    put(0, 0xCF0, struct.pack("<HH", 345, 456))
    put(0, 0xF20, bytes(range(0x24)))

    # SaveBlock1 optional identity regions.
    put(1, 0x1A9E, _gen3("SECRET", 7))
    put(1, 0x1AA5, old_id)
    put(1, 0x27CC, bytes(index & 0xFF for index in range(0x384)))
    put(1, 0x2BE0, bytes(range(0x24)))
    put(1, 0x2BF2, _gen3("MAIL", 8))
    put(1, 0x2BFA, old_id)
    put(1, 0x2E28, bytes([0xC3]) * 0x40)
    put(1, 0x2E90 + 22, _gen3("CONTEST", 8))
    put(1, 0x2E90 + 4, old_id)
    put(1, 0x2E90 + 11, _gen3("CONTESTMON", 11))
    put(1, 0x3030, _make_pokemon(personality=0x55667788, ot_id=0x6A000002,
                                 species=201))
    put(1, 0x3030 + 0x50 + 0x12, _gen3("DAYCARE", 8))
    put(1, 0x3030 + 0x50 + 0x1A, old_id)
    put(1, 0x3030 + 0x74, _gen3("DAYOWN", 8))
    put(1, 0x3030 + 0x7C, _gen3("DAYMON", 11))
    put(1, 0x3150, _gen3("LINK", 8))
    put(1, 0x3150 + 8, struct.pack("<H", 0xCAFE))
    put(1, 0x31F8, bytes([0xD1]) * 0x3A0)
    put(1, 0x360C, bytes([0xD2]) * 0x10C)
    put(1, 0x3728, bytes([0xD3]) * 0x3EC)
    put(1, 0x3B58, bytes([0xD4]) * 0x40)
    put(1, 0x3B98 + 4, _gen3("RECORD", 8))
    put(1, 0x3B98, old_id)
    put(1, 0x3C88, bytes([0xD5]) * 0xD2)
    put(1, 0x3D5A, bytes([0xD6]) * 10)
    put(1, 0x3D74, bytes([0xD7]) * 16)
    _finish_latest_sections(result, changed)
    return bytes(result)


def _pokemon_records(parsed) -> list[tuple[bytes, dict[str, object]]]:
    records: list[tuple[bytes, dict[str, object]]] = []
    sb1 = parsed.save_block1
    storage = parsed.storage
    for index in range(2):
        raw = sb1[0x238 + index * 0x64:0x238 + (index + 1) * 0x64]
        record = parsed_record = _parse_record(raw, party=True)
        if parsed_record is not None:
            records.append((raw, record))
    for box in range(14):
        start = 4 + box * 30 * 0x50
        for slot in range(30):
            raw = storage[start + slot * 0x50:start + (slot + 1) * 0x50]
            record = _parse_record(raw)
            if record is not None:
                records.append((raw, record))
    return records


def _parse_record(raw: bytes, party: bool = False) -> dict[str, object] | None:
    personality, ot_id = struct.unpack_from("<II", raw)
    if personality == 0 and ot_id == 0:
        return None
    secure = bytearray(raw[0x20:0x50])
    key = personality ^ ot_id
    for offset in range(0, len(secure), 4):
        value = struct.unpack_from("<I", secure, offset)[0] ^ key
        struct.pack_into("<I", secure, offset, value)
    return {
        "personality": personality,
        "ot_id": ot_id,
        "secure": bytes(secure),
        "checksum": struct.unpack_from("<H", raw, 0x1C)[0],
        "markings": raw[0x1B],
        "runtime": raw[0x50:] if party else b"",
    }


def _numeric_state(parsed) -> dict[str, object]:
    state = extract_protected_state(parsed)
    keys = ("species", "held_item", "experience", "friendship", "pp_bonuses",
            "moves", "pp", "evs", "ivs", "ability_number", "runtime")
    mons = lambda values: [
        None if mon is None else {
            key: mon[key] for key in keys if key in mon
        }
        for mon in values
    ]
    return {
        "gender": state["trainer"]["gender"],
        "options": state["options"],
        "pokedex": state["pokedex"],
        "party_count": state["party_count"],
        "party": mons(state["party"]),
        "boxes": [mons(box) for box in state["boxes"]],
        "current_box": state["box_metadata"]["current_box"],
        "wallpapers": state["box_metadata"]["wallpapers"],
        "money": state["money"],
        "coins": state["coins"],
        "registered_item": state["registered_item"],
        "bag": state["bag"],
        "pc_items": state["pc_items"],
        "frontier": {
            "normal_streaks": parsed.save_block2[0xCE0:0xD0C].hex(),
            "hard_streak": parsed.save_block1[0x359C:0x359E].hex(),
        },
    }


class AnonymizeSaveTests(unittest.TestCase):
    def test_ancillary_identity_is_scrubbed_and_frontier_progress_survives(self) -> None:
        source = _seed_ancillary_identity(_make_save())
        expected = _parse_bank(source, 1)
        anonymized = anonymize_save(source)
        after = _parse_bank(anonymized, 1)

        self.assertEqual(_numeric_state(expected)["frontier"], _numeric_state(after)["frontier"])
        self.assertEqual(after.save_block2[0xF20:0xF44], expected.save_block2[0xF20:0xF44])

        # Tower and e-Reader identities are replaced and their record checksums
        # are rebuilt after the replacement.
        for start in range(0x64C, 0xBD4, 0xEC):
            self.assertEqual(after.save_block2[start + 4:start + 12], _gen3("TESTER", 8))
            self.assertNotEqual(after.save_block2[start + 12:start + 16], struct.pack("<I", 0x6A000001))
            self.assertEqual(after.save_block2[start + 0x34 + 0x20:start + 0x34 + 0x2B], _gen3("MON", 11))
            self.assertNotEqual(
                after.save_block2[start + 0x34 + 0x14:start + 0x34 + 0x18],
                struct.pack("<I", 0x6A000001),
            )
            checksum = sum(
                struct.unpack_from("<I", after.save_block2, start + offset)[0]
                for offset in range(0, 0xE8, 4)
            ) & 0xFFFFFFFF
            self.assertEqual(checksum, struct.unpack_from("<I", after.save_block2, start + 0xE8)[0])
        self.assertEqual(after.save_block2[0xBD8:0xBE0], _gen3("TESTER", 8))
        self.assertEqual(after.save_block2[0xBE0:0xBEB], _gen3("MON", 11))
        self.assertEqual(after.save_block2[0xBEC + 4:0xBEC + 12], _gen3("TESTER", 8))
        self.assertNotEqual(after.save_block2[0xBEC + 12:0xBEC + 16], struct.pack("<I", 0x6A000001))
        checksum = sum(
            struct.unpack_from("<I", after.save_block2, 0xBEC + offset)[0]
            for offset in range(0, 0xB8, 4)
        ) & 0xFFFFFFFF
        self.assertEqual(checksum, struct.unpack_from("<I", after.save_block2, 0xBEC + 0xB8)[0])
        for index in range(2):
            self.assertEqual(after.save_block2[0xEF9 + index * 8:0xF01 + index * 8], _gen3("TESTER", 8))
            self.assertNotEqual(
                after.save_block2[0xF09 + index * 4:0xF0D + index * 4],
                struct.pack("<I", 0x6A000010 + index),
            )

        sb1 = after.save_block1
        self.assertEqual(sb1[0x27CC:0x2B50], bytes(0x384))
        self.assertEqual(sb1[0x2E28:0x2E68], bytes(0x40))
        self.assertEqual(sb1[0x2BE0:0x2BF2], bytes([0xFF]) * 18)
        self.assertEqual(sb1[0x2BF2:0x2BFA], _gen3("TESTER", 8))
        self.assertNotEqual(sb1[0x2BFA:0x2BFE], struct.pack("<I", 0x6A000001))
        self.assertEqual(sb1[0x2E90 + 22:0x2E90 + 30], _gen3("TESTER", 8))
        self.assertEqual(sb1[0x2E90 + 11:0x2E90 + 22], _gen3("MON", 11))

        daycare = sb1[0x3030:0x3080]
        daycare_record = _parse_record(daycare)
        self.assertIsNotNone(daycare_record)
        self.assertEqual(daycare[0x08:0x12], _gen3("MON", 10))
        self.assertEqual(daycare[0x14:0x1B], _gen3("TESTER", 7))
        self.assertEqual(sb1[0x3030 + 0x50:0x3030 + 0x50 + 0x12], bytes([0xFF]) * 18)
        self.assertEqual(sb1[0x3030 + 0x50 + 0x12:0x3030 + 0x50 + 0x1A], _gen3("TESTER", 8))
        self.assertEqual(sb1[0x3030 + 0x74:0x3030 + 0x7C], _gen3("TESTER", 8))
        self.assertEqual(sb1[0x3030 + 0x7C:0x3030 + 0x87], _gen3("MON", 11))
        for index in range(5):
            start = 0x3150 + index * 16
            self.assertEqual(sb1[start:start + 8], _gen3("TESTER", 8))
            self.assertEqual(sb1[start + 8:start + 10], struct.pack("<H", index + 1))
        self.assertEqual(sb1[0x31F8:0x3598], bytes(0x3A0))
        self.assertEqual(sb1[0x360C:0x3718], bytes(0x10C))
        self.assertEqual(sb1[0x3728:0x3B14], bytes(0x3EC))
        self.assertEqual(sb1[0x3B58:0x3B98], bytes(0x40))
        self.assertEqual(sb1[0x3B98 + 4:0x3B98 + 12], _gen3("TESTER", 8))
        self.assertNotEqual(sb1[0x3B98:0x3B9C], struct.pack("<I", 0x6A000001))
        self.assertEqual(sb1[0x3C88:0x3D5A], bytes([0xFF]) * 0xD2)
        self.assertEqual(sb1[0x3D5A:0x3D64], bytes(10))
        self.assertEqual(sb1[0x3D74:0x3D84], bytes([0xFF]) * 16)

    def test_deterministic_names_ids_secure_data_and_numeric_state(self) -> None:
        source = _make_save()
        anonymized = anonymize_save(source)
        self.assertEqual(anonymized, anonymize_save(source))
        self.assertEqual(len(anonymized), SAVE_SIZE)
        self.assertEqual(source[-len(TRAILER):], TRAILER)
        self.assertEqual(_parse_bank(source, 0).source_sha256 != _parse_bank(anonymized, 0).source_sha256, True)

        expected = _parse_bank(source, 1)
        expected_records = _pokemon_records(expected)
        source_ids: dict[int, int] = {}
        for bank in range(2):
            after = _parse_bank(anonymized, bank)
            self.assertEqual(_numeric_state(expected), _numeric_state(after))
            self.assertEqual(after.save_block2[0:8], _gen3("TESTER", 8))
            self.assertNotEqual(after.save_block2[0x0A:0x0E], struct.pack("<I", OLD_TRAINER_ID))
            for index in range(14):
                start = 0x8344 + index * 9
                self.assertEqual(after.storage[start:start + 9], _gen3(f"BOX{index + 1}", 9))

            after_records = _pokemon_records(after)
            self.assertEqual(len(expected_records), len(after_records))
            for (before_raw, before_record), (after_raw, after_record) in zip(expected_records, after_records):
                self.assertEqual(after_raw[0x08:0x12], _gen3("MON", 10))
                self.assertEqual(after_raw[0x14:0x1B], _gen3("TESTER", 7))
                self.assertEqual(after_record["markings"], before_record["markings"])
                self.assertNotEqual(after_record["ot_id"], before_record["ot_id"])
                self.assertEqual(after_record["secure"], before_record["secure"])
                self.assertEqual(_pokemon_checksum(after_record["secure"]), after_record["checksum"])
                self.assertEqual(after_record["runtime"], before_record["runtime"])
                source_ids.setdefault(before_record["ot_id"], after_record["ot_id"])
                self.assertEqual(source_ids[before_record["ot_id"]], after_record["ot_id"])

    def test_invalid_active_pokemon_checksum_is_rejected(self) -> None:
        raw = bytearray(_make_save())
        physical = NUM_SECTORS + (1 + 8) % NUM_SECTORS
        sector_start = physical * SECTOR_SIZE
        raw[sector_start + 0x238 + 0x20] ^= 1
        data = raw[sector_start:sector_start + SECTION_SIZES[1]]
        struct.pack_into(
            "<H", raw, sector_start + SECTOR_CHECKSUM_OFFSET, calculate_checksum(data)
        )
        with self.assertRaisesRegex(SaveFixtureError, "invalid checksum"):
            anonymize_save(bytes(raw))

    def test_both_banks_are_valid_and_padding_extra_sectors_are_cleared(self) -> None:
        anonymized = anonymize_save(_make_save())
        for bank in range(2):
            parsed = _parse_bank(anonymized, bank)
            self.assertEqual(len(parsed.sections), 14)
            for sector in parsed.sectors[:NUM_SECTORS]:
                self.assertTrue(sector.valid, sector)
            for physical in range(bank * NUM_SECTORS, (bank + 1) * NUM_SECTORS):
                sector_id = struct.unpack_from("<H", anonymized, physical * SECTOR_SIZE + SECTOR_ID_OFFSET)[0]
                start = physical * SECTOR_SIZE + SECTION_SIZES[sector_id]
                end = physical * SECTOR_SIZE + 0xF80
                self.assertEqual(anonymized[start:end], bytes(end - start))
        extra_start = 2 * NUM_SECTORS * SECTOR_SIZE
        self.assertEqual(
            anonymized[extra_start:],
            bytes([0xFF]) * ((NUM_PHYSICAL_SECTORS - 2 * NUM_SECTORS) * SECTOR_SIZE),
        )

    def test_import_preserves_source_and_rejects_existing_destination(self) -> None:
        source_bytes = _make_save()
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "original.sav"
            destination = root / "fixtures" / "anonymous.sav"
            source.write_bytes(source_bytes)
            result = import_anonymized_fixture(source, destination)
            self.assertEqual(source.read_bytes(), source_bytes)
            self.assertEqual(destination.read_bytes(), anonymize_save(source_bytes))
            self.assertEqual(result["fixture"], str(destination.resolve()))
            with self.assertRaises(SaveFixtureError):
                import_anonymized_fixture(source, destination)


if __name__ == "__main__":
    unittest.main()
