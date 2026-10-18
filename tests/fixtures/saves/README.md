# Real save fixtures

The compatibility scenario uses an anonymized copy of a real Emerald
RetroArch `.srm`. The party, boxes, inventory, and progress come from that
save. The private source and its original baseline stay local.

To create a public fixture from a private save, run this from the repository
root:

```sh
python3 tools/testing/anonymize_save.py /path/to/private-original.srm \
  --destination tests/fixtures/saves/real-save.sav
```

The command checks the save format and Pokémon checksums, leaves the source
unchanged, and writes these fixed fixture files:

- `real-save.sav` — the anonymized save;
- `real-save.sha256` — the anonymized fixture's SHA-256 and filename;
- `real-save.baseline.json` — the selected complete slot, counter, fixture
  SHA-256, protected-region hashes, and semantically decoded protected state.

The real-save regression is included in the default E2E suite. Run it alone:

```sh
make e2e TESTS=real-save-compatibility
```

It checks the loaded state, opens Audio to exercise the new defaults, saves
through the game menu, restarts, and checks protected state again. It uses a
scenario-local copy and verifies that the original fixture hash is unchanged.

## Anonymization and provenance

- Anonymized from a real RetroArch save on 2026-10-17.
- Fixture size: 131,072 bytes. Its hash is pinned in `real-save.sha256`.
- Selected save counter: 75. The source has no new Audio settings signature.
- Only scenario-local copies are loaded and saved by the emulator.

The converter replaces trainer/OT names and IDs, Pokémon nicknames and box
names. Pokémon secure data is decrypted and re-encrypted with the replacement
OT ID; moves, PP, EVs, IVs, items, experience and runtime stats are retained.
Numerical Frontier records, story flags, inventory and Pokédex stay intact.

Optional social/event text is cleared, including TV shows, mixed NPC data,
downloaded events/scripts and Union Room messages. Both banks are rebuilt
from the latest sanitized logical slot with valid checksums. Old bank data,
sector padding, Hall of Fame/recorded battle/Trainer Hill auxiliary sectors,
and emulator RTC footers are removed. This is a public test fixture, not a
replacement for the player's own save.

Normal test runs only read these files. To update them, remove the existing
fixture and run the anonymization command again as an explicit fixture update.
Keep private sources outside tracked files. Do not edit the fixture to make a
scenario pass or regenerate its baseline during a normal test run.

The parser uses the shipped SaveBlock2 and Frontier boundary (`0xF44`),
measured with the project's ARM ABI compiler at commit
`b0d9a56ebe58f2b001b70a04f8e6346d070326d7`. The vanilla header comment is
outdated, so it cannot define the fixture layout.
