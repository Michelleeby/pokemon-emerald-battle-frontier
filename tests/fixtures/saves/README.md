# Real save fixtures

The compatibility scenario uses an anonymized derivative of a real Emerald
RetroArch `.srm`. Its party, boxes, inventory and progression come from the
real save. The private source and its unsanitized baseline stay local.

After receiving the real save path, import it once from the repository root:

```sh
python3 tools/testing/anonymize_save.py /path/to/private-original.srm \
  --destination tests/fixtures/saves/real-save.sav
```

The command rejects malformed saves or invalid Pokémon checksums, leaves the
source untouched, and writes the pinned companion files:

* `real-save.sav` — the anonymized save;
* `real-save.sha256` — the anonymized fixture's SHA-256 and filename;
* `real-save.baseline.json` — the selected complete slot, counter, fixture
  SHA-256, protected-region hashes, and semantically decoded protected state.

The real-save regression is included in the default E2E suite. Run it alone:

```sh
make e2e TESTS=real-save-compatibility
```

It checks the loaded state, opens Audio to exercise the new defaults, saves
through the game menu, restarts, and checks protected state again. It uses a
scenario-local copy and verifies that the original fixture hash is unchanged.

## Anonymization and provenance

- Derived on 2026-10-17 from the user's real RetroArch save at their request.
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
replacement save for a player's game.

Normal test runs only read these files. To update them, remove the existing
fixture and run the anonymization command again as an explicit fixture update.
Keep private sources outside tracked files. Do not edit the fixture to make a
scenario pass or regenerate its baseline during a normal test run.

The parser's SaveBlock2 and Frontier boundary (`0xF44`) are pinned to the
compiled shipped layout measured with the project's ARM ABI compiler at
commit `b0d9a56ebe58f2b001b70a04f8e6346d070326d7`; the stale vanilla header
comment is not used as the fixture format authority.
