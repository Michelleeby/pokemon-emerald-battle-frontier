"""Import and inspect immutable Pokemon Emerald save fixtures.

The parser intentionally knows the legacy section sizes rather than importing
the current C struct sizes.  This keeps the compatibility baseline stable as
the ROM grows new fields.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import shutil
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Mapping


SECTOR_SIZE = 0x1000
SECTOR_DATA_SIZE = 0xF80
SECTOR_ID_OFFSET = 0xFF4
SECTOR_CHECKSUM_OFFSET = 0xFF6
SECTOR_SIGNATURE_OFFSET = 0xFF8
SECTOR_COUNTER_OFFSET = 0xFFC
SECTOR_SIGNATURE = 0x08012025
NUM_SECTORS = 14
NUM_PHYSICAL_SECTORS = 32

# These are the shipped legacy layout sizes.  Do not derive them from the
# current C structs: the new audio fields intentionally do not move any
# protected field, and the manifest must remain comparable across revisions.
SECTION_SIZES = {
    0: 0xF44,  # SaveBlock2 (measured shipped ABI layout)
    1: SECTOR_DATA_SIZE,
    2: SECTOR_DATA_SIZE,
    3: SECTOR_DATA_SIZE,
    4: 0xF08,   # tail of SaveBlock1
    5: SECTOR_DATA_SIZE,
    6: SECTOR_DATA_SIZE,
    7: SECTOR_DATA_SIZE,
    8: SECTOR_DATA_SIZE,
    9: SECTOR_DATA_SIZE,
    10: SECTOR_DATA_SIZE,
    11: SECTOR_DATA_SIZE,
    12: SECTOR_DATA_SIZE,
    13: 0x7D0,  # tail of PokemonStorage
}
SAVE_SIZE = NUM_PHYSICAL_SECTORS * SECTOR_SIZE
MGBA_RTC_TRAILER_SIZE = 16
SAVE_BLOCK1_SIZE = 0x3D88
SAVE_BLOCK2_SIZE = SECTION_SIZES[0]
STORAGE_SIZE = 0x83D0
PARTY_SIZE = 6
PARTY_MON_SIZE = 0x64
BOX_MON_SIZE = 0x50
BOX_COUNT = 14
BOX_SLOTS = 30


class SaveFixtureError(ValueError):
    """The supplied file is not a supported complete Emerald save."""


@dataclass(frozen=True)
class Sector:
    physical_index: int
    bank: int
    sector_id: int
    counter: int
    checksum: int
    calculated_checksum: int
    valid: bool
    reason: str = ""


@dataclass(frozen=True)
class ParsedSave:
    raw: bytes
    source_sha256: str
    bank: int
    counter: int
    sections: Mapping[int, bytes]
    sectors: tuple[Sector, ...]

    @property
    def save_block2(self) -> bytes:
        return self.sections[0]

    @property
    def save_block1(self) -> bytes:
        return b"".join(self.sections[index] for index in range(1, 5))

    @property
    def storage(self) -> bytes:
        return b"".join(self.sections[index] for index in range(5, 14))


def calculate_checksum(data: bytes) -> int:
    if len(data) % 4:
        raise SaveFixtureError(f"section data is not word aligned: {len(data):#x}")
    total = sum(struct.unpack_from("<I", data, offset)[0]
                for offset in range(0, len(data), 4))
    return ((total >> 16) + total) & 0xFFFF


def _serial_newer(candidate: int, current: int) -> bool:
    distance = (candidate - current) & 0xFFFFFFFF
    return distance != 0 and distance < 0x80000000


def _read_u16(data: bytes, offset: int) -> int:
    return struct.unpack_from("<H", data, offset)[0]


def _read_u32(data: bytes, offset: int) -> int:
    return struct.unpack_from("<I", data, offset)[0]


def _valid_sector(raw: bytes, physical_index: int) -> Sector:
    bank = physical_index // NUM_SECTORS
    sector_id = _read_u16(raw, SECTOR_ID_OFFSET)
    checksum = _read_u16(raw, SECTOR_CHECKSUM_OFFSET)
    signature = _read_u32(raw, SECTOR_SIGNATURE_OFFSET)
    counter = _read_u32(raw, SECTOR_COUNTER_OFFSET)
    if sector_id not in SECTION_SIZES:
        return Sector(physical_index, bank, sector_id, counter, checksum, 0,
                      False, "unsupported section ID")
    calculated = calculate_checksum(raw[:SECTION_SIZES[sector_id]])
    if signature != SECTOR_SIGNATURE:
        return Sector(physical_index, bank, sector_id, counter, checksum,
                      calculated, False, "signature mismatch")
    if checksum != calculated:
        return Sector(physical_index, bank, sector_id, counter, checksum,
                      calculated, False, "section checksum mismatch")
    return Sector(physical_index, bank, sector_id, counter, checksum,
                  calculated, True)


def parse_save(source: Path | bytes | bytearray) -> ParsedSave:
    """Return the newest complete save slot from *source*.

    A complete slot is fourteen valid logical sections in one physical bank
    with one common counter.  Section IDs are used to rebuild logical data;
    physical order and the rotating write offset are deliberately ignored.
    """
    if isinstance(source, (bytes, bytearray)):
        raw = bytes(source)
    else:
        raw = Path(source).read_bytes()
    if len(raw) not in (SAVE_SIZE, SAVE_SIZE + MGBA_RTC_TRAILER_SIZE):
        raise SaveFixtureError(
            f"unsupported save size {len(raw):#x}; expected a {SAVE_SIZE:#x}-byte "
            f"Emerald save or mGBA save with a {MGBA_RTC_TRAILER_SIZE}-byte RTC trailer")

    save_data = raw[:SAVE_SIZE]
    sectors: list[Sector] = []
    candidates: dict[tuple[int, int], dict[int, int]] = {}
    for physical_index in range(NUM_PHYSICAL_SECTORS):
        start = physical_index * SECTOR_SIZE
        sector = _valid_sector(save_data[start:start + SECTOR_SIZE], physical_index)
        sectors.append(sector)
        if sector.valid:
            candidates.setdefault((sector.bank, sector.counter), {})[sector.sector_id] = physical_index

    complete = [key for key, ids in candidates.items() if len(ids) == NUM_SECTORS
                and set(ids) == set(SECTION_SIZES)]
    if not complete:
        valid_count = sum(sector.valid for sector in sectors)
        raise SaveFixtureError(
            "no complete valid save slot: "
            f"{valid_count}/{NUM_PHYSICAL_SECTORS} valid sectors, "
            "expected all 14 section IDs with one counter in one bank")

    newest = complete[0]
    for candidate in complete[1:]:
        if _serial_newer(candidate[1], newest[1]) or (
                candidate[1] == newest[1] and candidate[0] > newest[0]):
            newest = candidate
    bank, counter = newest
    sections = {
        section_id: save_data[physical * SECTOR_SIZE:physical * SECTOR_SIZE
                         + SECTION_SIZES[section_id]]
        for section_id, physical in candidates[newest].items()
    }
    return ParsedSave(raw, hashlib.sha256(raw).hexdigest(), bank, counter,
                      sections, tuple(sectors))


def _hex(data: bytes) -> str:
    return data.hex()


def _item_slots(data: bytes, offset: int, count: int, key: int) -> list[list[int]]:
    result = []
    for index in range(count):
        item_id, encrypted_quantity = struct.unpack_from("<HH", data, offset + index * 4)
        result.append([item_id, encrypted_quantity ^ (key & 0xFFFF)])
    return result


SUBSTRUCT_ORDER = (
    (0, 1, 2, 3), (0, 1, 3, 2), (0, 2, 1, 3), (0, 3, 1, 2),
    (0, 2, 3, 1), (0, 3, 2, 1), (1, 0, 2, 3), (1, 0, 3, 2),
    (2, 0, 1, 3), (3, 0, 1, 2), (2, 0, 3, 1), (3, 0, 2, 1),
    (1, 2, 0, 3), (1, 3, 0, 2), (2, 1, 0, 3), (3, 1, 0, 2),
    (2, 3, 0, 1), (3, 2, 0, 1), (1, 2, 3, 0), (1, 3, 2, 0),
    (2, 1, 3, 0), (3, 1, 2, 0), (2, 3, 1, 0), (3, 2, 1, 0),
)


def _pokemon(raw: bytes, party: bool = False) -> dict[str, object] | None:
    if len(raw) not in (BOX_MON_SIZE, PARTY_MON_SIZE):
        raise SaveFixtureError(f"unsupported Pokemon record size: {len(raw):#x}")
    personality, ot_id = struct.unpack_from("<II", raw)
    secure = bytearray(raw[0x20:0x50])
    key = personality ^ ot_id
    for offset in range(0, len(secure), 4):
        value = struct.unpack_from("<I", secure, offset)[0] ^ key
        struct.pack_into("<I", secure, offset, value)
    physical = [bytes(secure[index:index + 12]) for index in range(4)]
    logical = [physical[index] for index in SUBSTRUCT_ORDER[personality % 24]]
    growth, attacks, effort, misc = logical
    species, held_item, experience = struct.unpack_from("<HHI", growth)
    pp_bonuses, friendship = growth[8], growth[9]
    moves = list(struct.unpack_from("<4H", attacks))
    pp = list(attacks[8:12])
    evs = list(effort[:6])
    ivs_ability = struct.unpack_from("<I", misc, 4)[0]
    ivs = [(ivs_ability >> shift) & 0x1F for shift in (0, 5, 10, 15, 20, 25)]
    result: dict[str, object] = {
        "personality": personality,
        "ot_id": ot_id,
        "identity": raw[8:0x1C].hex(),
        "species": species,
        "held_item": held_item,
        "experience": experience,
        "friendship": friendship,
        "pp_bonuses": pp_bonuses,
        "moves": moves,
        "pp": pp,
        "evs": evs,
        "ivs": ivs,
        "ability_number": (ivs_ability >> 31) & 1,
        "growth": growth.hex(),
        "attacks": attacks.hex(),
        "effort": effort.hex(),
        "misc": misc.hex(),
    }
    if party:
        result["runtime"] = raw[0x50:].hex()
    if species == 0 and personality == 0 and ot_id == 0:
        return None
    return result


def _pokemon_list(data: bytes, offset: int, count: int, size: int,
                  party: bool = False) -> list[dict[str, object] | None]:
    return [_pokemon(data[offset + index * size:offset + (index + 1) * size], party)
            for index in range(count)]


def _legacy_hashes(sb1: bytes, sb2: bytes, storage: bytes) -> dict[str, str]:
    regions = {
        "saveblock2.trainer_identity": sb2[0:9] + sb2[0x0A:0x0E],
        "saveblock2.options": sb2[0x13:0x16],
        "saveblock2.pokedex": sb2[0x18:0x90],
        "saveblock2.normal_frontier": sb2[0x64C:0xF44],
        "saveblock1.pokedex_seen1": sb1[0x988:0x9BC],
        "saveblock1.party": sb1[0x234:0x490],
        "saveblock1.registered_item": sb1[0x496:0x498],
        "saveblock1.hard_frontier": sb1[0x3598:0x360C],
        "saveblock1.pokedex_seen2": sb1[0x3B24:0x3B58],
        "storage.all": storage[:STORAGE_SIZE],
    }
    return {name: hashlib.sha256(value).hexdigest() for name, value in regions.items()}


def extract_protected_state(parsed: ParsedSave) -> dict[str, object]:
    """Build the semantic, protected compatibility baseline for a save."""
    sb1, sb2, storage = parsed.save_block1, parsed.save_block2, parsed.storage
    key = _read_u32(sb2, 0xAC)
    boxes = []
    for box in range(BOX_COUNT):
        start = 4 + box * BOX_SLOTS * BOX_MON_SIZE
        boxes.append(_pokemon_list(storage, start, BOX_SLOTS, BOX_MON_SIZE))
    return {
        "trainer": {
            "name": _hex(sb2[0:8]),
            "gender": sb2[8],
            "trainer_id": _hex(sb2[0x0A:0x0E]),
        },
        "options": _hex(sb2[0x13:0x16]),
        "pokedex": _hex(sb2[0x18:0x90]),
        "pokedex_seen1": _hex(sb1[0x988:0x9BC]),
        "pokedex_seen2": _hex(sb1[0x3B24:0x3B58]),
        "party_count": sb1[0x234],
        "party": _pokemon_list(sb1, 0x238, PARTY_SIZE, PARTY_MON_SIZE, party=True),
        "boxes": boxes,
        "box_metadata": {
            "current_box": storage[0],
            "names": _hex(storage[0x8344:0x83C2]),
            "wallpapers": _hex(storage[0x83C2:0x83D0]),
        },
        "money": _read_u32(sb1, 0x490) ^ key,
        "coins": _read_u16(sb1, 0x494) ^ (key & 0xFFFF),
        "registered_item": _read_u16(sb1, 0x496),
        "bag": {
            "items": _item_slots(sb1, 0x560, 30, key),
            "key_items": _item_slots(sb1, 0x5D8, 30, key),
            "pokeballs": _item_slots(sb1, 0x650, 16, key),
            "tm_hm": _item_slots(sb1, 0x690, 64, key),
            "berries": _item_slots(sb1, 0x790, 46, key),
        },
        # PC quantities are stored plainly; only money, coins, and bag
        # quantities use the SaveBlock2 encryption key.
        "pc_items": _item_slots(sb1, 0x498, 50, 0),
        "frontier": {
            "normal_saveblock2": hashlib.sha256(sb2[0x64C:0xF44]).hexdigest(),
            "hard_saveblock1": hashlib.sha256(sb1[0x3598:0x360C]).hexdigest(),
        },
        "legacy_region_hashes": _legacy_hashes(sb1, sb2, storage),
    }


def audio_record_from_save(parsed: ParsedSave) -> tuple[int, ...]:
    """Read the optional audio record without making it part of protected state."""
    return struct.unpack_from("<H6B", parsed.save_block2, 0x90)


def baseline_manifest(parsed: ParsedSave) -> dict[str, object]:
    return {
        "format": 1,
        "source_sha256": parsed.source_sha256,
        "slot_bank": parsed.bank,
        "save_counter": parsed.counter,
        "state": extract_protected_state(parsed),
    }


def _first_difference(expected: object, actual: object, path: str = "state") -> str | None:
    if type(expected) is not type(actual):
        return f"{path}: expected {expected!r}, got {actual!r}"
    if isinstance(expected, dict):
        for key in expected:
            if key not in actual:  # type: ignore[operator]
                return f"{path}.{key}: missing in actual state"
            difference = _first_difference(expected[key], actual[key], f"{path}.{key}")  # type: ignore[index]
            if difference:
                return difference
        for key in actual:  # type: ignore[union-attr]
            if key not in expected:
                return f"{path}.{key}: unexpected actual value"
        return None
    if isinstance(expected, list):
        if len(expected) != len(actual):  # type: ignore[arg-type]
            return f"{path}: expected {len(expected)} entries, got {len(actual)}"
        for index, (left, right) in enumerate(zip(expected, actual)):
            difference = _first_difference(left, right, f"{path}[{index}]")
            if difference:
                return difference
        return None
    if expected != actual:
        return f"{path}: expected {expected!r}, got {actual!r}"
    return None


def compare_protected_state(expected: Mapping[str, object], actual: Mapping[str, object]) -> None:
    difference = _first_difference(expected, actual)
    if difference:
        raise SaveFixtureError(f"protected save state changed: {difference}")


def import_fixture(source: Path, destination: Path) -> dict[str, object]:
    """Copy and baseline a raw save, leaving *source* untouched."""
    source = Path(source).resolve()
    destination = Path(destination).resolve()
    if not source.is_file():
        raise SaveFixtureError(f"save source does not exist: {source}")
    parsed = parse_save(source)
    if destination.exists():
        raise SaveFixtureError(
            f"fixture already exists: {destination}; remove it only for an explicit fixture update")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)
    manifest_path = destination.with_suffix(".baseline.json")
    sha_path = destination.with_suffix(".sha256")
    manifest_path.write_text(json.dumps(baseline_manifest(parsed), indent=2, sort_keys=True) + "\n")
    sha_path.write_text(f"{parsed.source_sha256}  {destination.name}\n")
    return {"fixture": str(destination), "baseline": str(manifest_path),
            "sha256": parsed.source_sha256}


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    subparsers = parser.add_subparsers(dest="command", required=True)
    importer = subparsers.add_parser("import", help="copy a real .sav and pin its baseline")
    importer.add_argument("source", type=Path)
    importer.add_argument("--destination", type=Path,
                          default=Path("tests/fixtures/saves/real-save.sav"))
    args = parser.parse_args(argv)
    if args.command == "import":
        try:
            result = import_fixture(args.source, args.destination)
        except (OSError, SaveFixtureError) as exc:
            parser.error(str(exc))
        print(json.dumps(result, indent=2, sort_keys=True))
        return 0
    return 2


if __name__ == "__main__":
    raise SystemExit(main())
