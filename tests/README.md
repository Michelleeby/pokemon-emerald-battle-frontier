# Gameplay testing

The project has two complementary gameplay test layers:

| Layer | Purpose | Emulator |
|---|---|---|
| C gameplay suites | Fast, deterministic checks of gameplay logic and state transitions | `mgba-rom-test` |
| End-to-end scenarios | Input-driven checks through production menus, scripts, maps, and saves | Project-owned driver linked to mGBA |

Test builds use `TESTING=1` and separate object trees under `build/test/` or
`build/e2e/`. These objects are not linked into the production ROM.

## Quick start

List the C gameplay suites:

```sh
make list-tests
```

Run the default `team-lab` suite, selected suites, or every suite:

```sh
make check
make check TESTS="frontier-common save-load"
make check-all
```

The C suites require `mgba-rom-test`. The runner searches
`MGBA_ROM_TEST`, the executable on `PATH`, and the default sibling mGBA
build at `../mgba/build/test/mgba-rom-test`.

Run selected E2E scenarios or the complete scenario set:

```sh
make e2e TESTS="tower-normal-anabel tower-hard-anabel"
make e2e
```

The E2E driver expects a built mGBA checkout at `../mgba`. Override that
location with `MGBA_DIR=/path/to/mgba`.

## C gameplay suites

The authoritative suite and dependency list is
[`tests/manifest.json`](manifest.json).

| Suite | Primary coverage |
|---|---|
| `frontier-common` | Shared Normal/Hard Frontier state, records, and boundaries |
| `team-lab` | Pokémon creation, validation, legal moves, stats, nature, and ability data |
| `new-game-tutorial` | New-game state and guided-introduction behavior |
| `save-load` | Party and project-specific save fields |
| `battle-shared` | Shared battle setup and controller commands |
| `frontier-tower` | Tower modes, parties, pools, progression, and Anabel boundaries |
| `frontier-factory` | Rentals, swaps, opponents, progression, and Noland boundaries |
| `frontier-dome` | Tournament generation, advancement, and Tucker boundaries |
| `frontier-arena` | Arena judgment, progression, and Greta boundaries |
| `frontier-palace` | Palace move selection, progression, and Spenser boundaries |
| `frontier-pike` | Pike rooms, healing, wild encounters, progression, and Lucy boundaries |
| `frontier-pyramid` | Floors, objects, wild encounters, light, progression, and Brandon boundaries |

Fixtures clear save blocks, storage, and both parties, then seed both game RNGs
explicitly. Every suite starts in fresh emulator state.

## End-to-end scenarios

The authoritative scenario list and path-selection rules are in
[`tests/e2e_manifest.json`](e2e_manifest.json). Current scenarios cover:

- Normal and Hard Frontier Brain boundaries for Tower, Factory, Dome, Arena,
  Palace, Pike, and Pyramid
- Hard-mode Factory rental setup
- Pokémon Lab creation and stat editing
- PokéNav access to the Pokémon Lab
- The guided Frontier introduction and its Pokémon Lab traversal

The facility scenarios focus on behavior changed by this project. They do not
replay unchanged vanilla facility plumbing. For long special-trainer battles,
an E2E-only assistance seam supplies outcomes after production code constructs
the opponent and enters the battle flow. Production scripts still handle
progression, warps, rewards, and saves. The release ROM contains neither the
assistance task nor its counters.

Each scenario starts with isolated save data, fixed RNG seeds, and a fresh mGBA
core. The driver supports frame advancement, keypad input, bounded memory
predicates, memory reads, screenshots, and complete emulator restart.

## Selection and CI

`tests/manifest.json` maps changed paths to C suites and
`tests/e2e_manifest.json` maps them to E2E scenarios. Selection is
conservative:

- Facility-local changes select that facility's coverage and dependencies.
- Shared gameplay or testing infrastructure can select all relevant tests.
- Unknown gameplay paths select broad coverage.
- Documentation-only and release-only changes select no gameplay tests.

Validate the manifests, selector behavior, or a specific diff with:

```sh
python3 tools/testing/validate_manifest.py
python3 -m unittest discover -s tools/testing -p 'test_*.py' -v
python3 tools/testing/select_suites.py --files src/battle_tower.c
python3 tools/testing/select_suites.py --base main --head HEAD
```

CI runs selected tests in independent matrix jobs with fail-fast disabled. The
stable `Tests / required` check requires successful selection, every selected
test job, and artifact cleanup. A documentation-only change uses explicit
`no-suites` and `no-scenarios` jobs.

## Diagnostics and artifacts

C-suite logs and JSON reports are written to:

```text
build/test/artifacts/<suite>/
```

E2E logs, reports, input traces, screenshots, and scenario-local saves are
written to:

```text
build/e2e/artifacts/<scenario>/
```

Run the opt-in runner diagnostics with:

```sh
make check-runner
make check-e2e-runner
```

`check-runner` verifies pass, intentional-failure, and timeout handling.
`check-e2e-runner` exercises the live driver protocol against mGBA.

CI may stage only JSON, logs, PNG screenshots, and scenario-local save files.
ROMs, ELF and map files, symbol files, mGBA binaries, and shared build output
must never be uploaded as test artifacts.

## Test-development constraints

Every production-linked C suite enforces at least:

- 8 KiB of unused EWRAM
- 1.5 KiB between the end of static IWRAM and the system stack base at
  `0x03007E00`

Keep large fixture tables constant so they remain in ROM. Avoid large local
arrays, deep test-only call chains, and new zero-initialized buffers. The
`TEST_MIN_EWRAM_FREE` and `TEST_MIN_IWRAM_STACK` overrides are diagnostic
tools; lowering committed thresholds is not an acceptable fix.

`make clean` removes gameplay and E2E build trees. After changing test
infrastructure, confirm that a normal `TESTING=0` build still has the
documented production SHA-1 and contains no test object paths or test-only
symbols.
