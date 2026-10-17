"""Create a public regression fixture from a private Emerald save.

The latest logical save supplies the gameplay state. Both physical banks are
rebuilt from that state, so previous banks, unused bytes, auxiliary flash
sectors and an emulator RTC footer cannot retain private names or messages.
"""

from __future__ import annotations

import argparse
import json
import struct
from pathlib import Path

try:
    from .save_fixture import (
        BOX_MON_SIZE, NUM_SECTORS, PARTY_MON_SIZE, SAVE_SIZE, SECTION_SIZES,
        SECTOR_CHECKSUM_OFFSET, SECTOR_COUNTER_OFFSET, SECTOR_ID_OFFSET,
        SECTOR_SIGNATURE, SECTOR_SIGNATURE_OFFSET, SECTOR_SIZE,
        SaveFixtureError, baseline_manifest, calculate_checksum, parse_save,
    )
except ImportError:  # Direct command-line execution.
    from save_fixture import (
        BOX_MON_SIZE, NUM_SECTORS, PARTY_MON_SIZE, SAVE_SIZE, SECTION_SIZES,
        SECTOR_CHECKSUM_OFFSET, SECTOR_COUNTER_OFFSET, SECTOR_ID_OFFSET,
        SECTOR_SIGNATURE, SECTOR_SIGNATURE_OFFSET, SECTOR_SIZE,
        SaveFixtureError, baseline_manifest, calculate_checksum, parse_save,
    )


def _text(value: str, length: int) -> bytes:
    """Encode the deliberately restricted, public ASCII labels in Gen III."""
    encoded = bytes(0xBB + ord(c) - ord('A') if 'A' <= c <= 'Z'
                    else 0xA1 + ord(c) - ord('0') for c in value)
    if len(encoded) >= length:
        raise SaveFixtureError('anonymous label needs room for its terminator')
    return encoded + b'\xff' * (length - len(encoded))


class _Anonymizer:
    def __init__(self, player_id: int):
        self.ids = {player_id: 0x5A000001}

    def trainer_id(self, value: int) -> int:
        if value not in self.ids:
            self.ids[value] = 0x5A000001 + len(self.ids)
        return self.ids[value]

    def identity(self, data: bytearray, name: int, length: int, tid: int) -> None:
        original = struct.unpack_from('<I', data, tid)[0]
        data[name:name + length] = _text('TESTER', length)
        struct.pack_into('<I', data, tid, self.trainer_id(original))

    def pokemon(self, data: bytearray, offset: int, size: int) -> None:
        mon = bytearray(data[offset:offset + size])
        if not mon[19] & 2:  # hasSpecies; also removes stale unused records.
            data[offset:offset + size] = bytes(size)
            return
        personality, old_id = struct.unpack_from('<II', mon)
        secure = bytearray(mon[0x20:0x50])
        for i in range(0, 48, 4):
            value = struct.unpack_from('<I', secure, i)[0] ^ personality ^ old_id
            struct.pack_into('<I', secure, i, value)
        checksum = sum(struct.unpack('<24H', secure)) & 0xFFFF
        if checksum != struct.unpack_from('<H', mon, 0x1C)[0]:
            raise SaveFixtureError('cannot anonymize a Pokemon with an invalid checksum')
        new_id = self.trainer_id(old_id)
        struct.pack_into('<I', mon, 4, new_id)
        mon[8:18] = _text('MON', 10)
        mon[18] = 2  # English labels.
        mon[20:27] = _text('TESTER', 7)
        mon[0x1E:0x20] = bytes(2)
        for i in range(0, 48, 4):
            value = struct.unpack_from('<I', secure, i)[0] ^ personality ^ new_id
            struct.pack_into('<I', mon, 0x20 + i, value)
        data[offset:offset + size] = mon

    def tower_record(self, data: bytearray, start: int, count: int, checksum_offset: int) -> None:
        self.identity(data, start + 4, 8, start + 12)
        for i in range(count):
            mon = start + 0x34 + i * 0x2C
            tid = struct.unpack_from('<I', data, mon + 0x14)[0]
            struct.pack_into('<I', data, mon + 0x14, self.trainer_id(tid))
            data[mon + 0x20:mon + 0x2B] = _text('MON', 11)
        if checksum_offset == 0xE8:
            data[start + 0xE5:start + 0xE8] = bytes(3)
        checksum = sum(struct.unpack_from('<I', data, start + i)[0]
                       for i in range(0, checksum_offset, 4)) & 0xFFFFFFFF
        struct.pack_into('<I', data, start + checksum_offset, checksum)

    def mail(self, data: bytearray, offset: int) -> None:
        data[offset:offset + 18] = b'\xff' * 18
        self.identity(data, offset + 0x12, 8, offset + 0x1A)
        data[offset + 0x22:offset + 0x24] = bytes(2)


def anonymize_save(raw: bytes) -> bytes:
    """Retain current gameplay state with public names and replacement IDs.

    Optional social/event records and auxiliary sectors are cleared. Numerical
    Frontier progress, flags, inventory and the Pokemon's decrypted battle
    data are retained; this produces a test fixture, not a player's new save.
    """
    source = parse_save(raw)
    sb1, sb2, storage = (bytearray(source.save_block1),
                         bytearray(source.save_block2), bytearray(source.storage))
    scrub = _Anonymizer(struct.unpack_from('<I', sb2, 0x0A)[0])
    scrub.identity(sb2, 0, 8, 0x0A)
    sb2[0x16:0x18] = bytes(2)
    sb2[0xDC:0x1EC] = bytes(0x110)  # Apprentices received through record mixing.
    for start in range(0x21C, 0x57C, 16):
        scrub.identity(sb2, start + 6, 8, start)
        sb2[start + 15] = 0
    for start in range(0x57C, 0x624, 28):
        scrub.identity(sb2, start + 10, 8, start)
        scrub.identity(sb2, start + 18, 8, start + 4)
        sb2[start + 27] = 0
    for start in range(0x64C, 0xBD4, 0xEC):
        scrub.tower_record(sb2, start, 4, 0xE8)
    sb2[0xBD8:0xBE0] = _text('TESTER', 8)
    sb2[0xBE0:0xBEB] = _text('MON', 11)
    scrub.tower_record(sb2, 0xBEC, 3, 0xB8)
    # Offsets after the patch's additional Hard Dome fields use the shipped ABI.
    for i in range(2):
        scrub.identity(sb2, 0xEF9 + i * 8, 8, 0xF09 + i * 4)

    for i in range(6):
        scrub.pokemon(sb1, 0x238 + i * PARTY_MON_SIZE, PARTY_MON_SIZE)
    for start, end in ((0x235, 0x238), (0x9C2, 0x9C8), (0xA2E, 0xA30),
                       (0x27CA, 0x27CC), (0x2E25, 0x2E28)):
        sb1[start:end] = bytes(end - start)
    storage[1:4] = bytes(3)
    for i in range(14 * 30):
        scrub.pokemon(storage, 4 + i * BOX_MON_SIZE, BOX_MON_SIZE)
    for i in range(14):
        storage[0x8344 + i * 9:0x8344 + (i + 1) * 9] = _text(f'BOX{i + 1}', 9)

    for i in range(20):
        scrub.identity(sb1, 0x1A9E + i * 0xA0, 7, 0x1AA5 + i * 0xA0)
        sb1[0x1ACE + i * 0xA0:0x1AD0 + i * 0xA0] = bytes(2)
    sb1[0x27CC:0x2B50] = bytes(0x384)  # TV shows include free names and source IDs.
    for i in range(16):
        scrub.mail(sb1, 0x2BE0 + i * 0x24)
    sb1[0x2E28:0x2E68] = bytes(0x40)  # Mixed Mauville NPC data (measured ABI).
    for i in range(13):
        start = 0x2E90 + i * 0x20
        scrub.identity(sb1, start + 22, 8, start + 4)
        sb1[start + 11:start + 22] = _text('MON', 11)
        sb1[start + 31] = 0
    for i in range(2):
        start = 0x3030 + i * 0x8C
        scrub.pokemon(sb1, start, BOX_MON_SIZE)
        scrub.mail(sb1, start + 0x50)
        sb1[start + 0x74:start + 0x7C] = _text('TESTER', 8)
        sb1[start + 0x7C:start + 0x87] = _text('MON', 11)
    for i in range(5):
        start = 0x3150 + i * 16
        sb1[start:start + 8] = _text('TESTER', 8)
        struct.pack_into('<H', sb1, start + 8, i + 1)
    sb1[0x31F8:0x3598] = bytes(0x3A0)  # Downloaded berry and Mystery Gift data.
    sb1[0x360C:0x3718] = bytes(0x10C)
    sb1[0x3728:0x3B14] = bytes(0x3EC)  # Downloaded scripts can contain arbitrary text.
    sb1[0x3B58:0x3B98] = bytes(0x40)  # Mixed Lilycove Lady data.
    for i in range(20):
        start = 0x3B98 + i * 12
        scrub.identity(sb1, start + 4, 8, start)
    sb1[0x3C88:0x3D5A] = b'\xff' * 0xD2  # Union Room registered free text.
    sb1[0x3D5A:0x3D64] = bytes(10)
    sb1[0x3D74:0x3D84] = b'\xff' * 16  # Custom wallpaper phrase.

    sections = {0: bytes(sb2)}
    for first, data in ((1, sb1), (5, storage)):
        count = 4 if first == 1 else 9
        for i in range(count):
            start = i * 0xF80
            sections[first + i] = bytes(data[start:start + SECTION_SIZES[first + i]])
    output = bytearray(b'\xff' * SAVE_SIZE)
    positions = {s.sector_id: s.physical_index % NUM_SECTORS for s in source.sectors
                 if s.valid and s.bank == source.bank and s.counter == source.counter}
    for bank in (0, 1):
        counter = source.counter if bank == source.bank else (source.counter - 1) & 0xFFFFFFFF
        for section_id, payload in sections.items():
            sector = bytearray(SECTOR_SIZE)
            sector[:len(payload)] = payload
            struct.pack_into('<H', sector, SECTOR_ID_OFFSET, section_id)
            struct.pack_into('<H', sector, SECTOR_CHECKSUM_OFFSET, calculate_checksum(payload))
            struct.pack_into('<I', sector, SECTOR_SIGNATURE_OFFSET, SECTOR_SIGNATURE)
            struct.pack_into('<I', sector, SECTOR_COUNTER_OFFSET, counter)
            physical = bank * NUM_SECTORS + positions[section_id]
            output[physical * SECTOR_SIZE:(physical + 1) * SECTOR_SIZE] = sector
    return bytes(output)


def import_anonymized_fixture(source: Path, destination: Path) -> dict[str, object]:
    source, destination = Path(source).resolve(), Path(destination).resolve()
    if destination.exists():
        raise SaveFixtureError(f'fixture already exists: {destination}')
    parsed = parse_save(anonymize_save(source.read_bytes()))
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(parsed.raw)
    destination.with_suffix('.baseline.json').write_text(
        json.dumps(baseline_manifest(parsed), indent=2, sort_keys=True) + '\n')
    destination.with_suffix('.sha256').write_text(f'{parsed.source_sha256}  {destination.name}\n')
    return {'fixture': str(destination), 'sha256': parsed.source_sha256}


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('--destination', type=Path, required=True)
    args = parser.parse_args()
    try:
        print(json.dumps(import_anonymized_fixture(args.source, args.destination), indent=2))
    except (OSError, SaveFixtureError) as exc:
        parser.error(str(exc))
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
