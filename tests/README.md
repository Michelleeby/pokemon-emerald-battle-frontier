# Gameplay testing

Use the C gameplay suites to check logic and state transitions. Use
end-to-end (E2E) scenarios to check how that logic works through the game
menus, scripts, maps, and saves. The two layers cover different parts of the
same behavior:

| Layer | Purpose | Emulator |
|---|---|---|
| C gameplay suites | Fast, deterministic checks of gameplay logic and state transitions | `mgba-rom-test` |
| End-to-end scenarios | Checks through production menus, scripts, maps, and saves using game input | Project-owned driver linked to mGBA |

C gameplay-suite builds use `TESTING=1` and keep their object files under
`build/test/`. E2E gameplay builds use `E2E_TESTING=1` and keep their object
files under `build/e2e/`. This keeps test objects out of the production ROM.

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

The C suites require `mgba-rom-test`. To find it, the runner checks
`MGBA_ROM_TEST`, looks for the executable on `PATH`, then checks the default
sibling mGBA build at `../mgba/build/test/mgba-rom-test`.

Run selected E2E scenarios or the complete scenario set:

```sh
make e2e TESTS="tower-normal-anabel tower-hard-anabel"
make e2e
```

The E2E driver expects a built mGBA checkout at `../mgba`. Override that
location with `MGBA_DIR=/path/to/mgba`.

## C gameplay suites

Use [`tests/manifest.json`](manifest.json) as the source for suite names and
dependencies. The suites cover:

| Suite | Primary coverage |
|---|---|
| `frontier-common` | Shared Normal/Hard Frontier state, records, and boundaries |
| `team-lab` | Pokémon creation, validation, legal moves, PP editing, stats, nature, and ability data |
| `new-game-tutorial` | New-game state and guided-introduction behavior |
| `save-load` | Party, save boundaries, all eight avatar mappings, invalid-avatar fallback, and project-specific save fields |
| `battle-shared` | Shared battle setup and controller commands |
| `audio-settings` | Audio save compatibility, music catalog, battle scope, and cosmetic random selection |
| `low-hp-audio` | Low health alert preferences, single/double battle state, and cleanup |
| `frontier-tower` | Tower modes, parties, pools, progression, and Anabel boundaries |
| `frontier-factory` | Rentals, swaps, opponents, progression, and Noland boundaries |
| `frontier-dome` | Tournament generation, advancement, and Tucker boundaries |
| `frontier-arena` | Arena judgment, progression, and Greta boundaries |
| `frontier-palace` | Palace move selection, progression, and Spenser boundaries |
| `frontier-pike` | Pike rooms, healing, wild encounters, progression, and Lucy boundaries |
| `frontier-pyramid` | Floors, objects, wild encounters, light, progression, and Brandon boundaries |

Fixtures clear save blocks, storage, and both parties, then set both game
random number generator (RNG) seeds explicitly. Every suite starts with a
fresh emulator state so state from one suite cannot carry into the next.

## End-to-end scenarios

Use [`tests/e2e_manifest.json`](e2e_manifest.json) as the source for scenario
names and the rules that select scenarios from changed file paths. Current
scenarios cover:

- Eight-avatar new-game selection, both D-pad wrap directions, front previews,
  independent bag/color previews, back navigation, all four bag/theme combinations,
  avatar-specific Frontier map icons, naming, normal save/reload, running acceleration
  and coasting, disabled legacy key items, and safe recovery of water/underwater saves
- Normal and Hard Frontier Brain boundaries for Tower, Factory, Dome, Arena,
  Palace, Pike, and Pyramid
- Hard-mode Factory rental setup
- Pokémon Lab creation and stat editing
- Pokémon Lab PP editing, direct Select Max PP, `NONE` clearing, and
  save/discard behavior
- Pokémon Lab IV/EV highlights and the values they select for editing
- Nested Audio options, music preview, pool selection, and preference saving
- PokéNav access to the Pokémon Lab
- The guided Frontier introduction and its Pokémon Lab traversal

The `real-save-compatibility` scenario loads an anonymized copy of a real
RetroArch save, checks protected data, saves through the game menu, and
restarts. It then compares that data again and checks that the original
fixture stayed unchanged. See
[`tests/fixtures/saves/README.md`](fixtures/saves/README.md) for import and run
commands. The scenario runs in the default E2E suite. Changes to the fixture,
parser, or anonymizer also select it automatically.

The facility scenarios focus on behavior changed by this project. They do
not replay unchanged vanilla facility flows. For long special-trainer
battles, test assistance code supplies the outcome after
production code constructs the opponent and enters the battle flow. This
lets the scenario continue through progression, warps, rewards, and saves
using production scripts. The assistance task and its counters are included
only in E2E builds; neither is included in the release ROM.

Each scenario starts with its own save data, fixed RNG seeds, and a fresh
mGBA core. The driver can advance frames, send keypad input, wait for memory
conditions within a set limit, read memory, capture screenshots, and restart
the emulator completely.

## Selection and CI

`tests/manifest.json` maps changed paths to C suites, while
`tests/e2e_manifest.json` maps them to E2E scenarios. The selectors use these
rules to include the coverage a change may affect:

- Changes within a facility select that facility's coverage and dependencies.
- Shared gameplay or testing infrastructure can select all relevant tests.
- Unknown gameplay paths select broad coverage.
- Documentation-only and release-only changes select no gameplay tests.

Validate the manifests and selector behavior, then check which suites a
file or diff selects:

```sh
python3 tools/testing/validate_manifest.py
python3 -m unittest discover -s tools/testing -p 'test_*.py' -v
python3 tools/testing/select_suites.py --files src/battle_tower.c
python3 tools/testing/select_suites.py --base main --head HEAD
```

CI runs selected tests in independent matrix jobs with fail-fast disabled,
so a failed job does not stop the others. The `Tests / required` check passes
only when selection, every selected test job, and artifact cleanup succeed.
Documentation-only changes use explicit `no-suites` and `no-scenarios` jobs.

## Diagnostics and artifacts

Find C-suite logs and JSON reports in:

```text
build/test/artifacts/<suite>/
```

Find E2E logs, reports, input traces, screenshots, and saves for each
scenario in:

```text
build/e2e/artifacts/<scenario>/
```

Run the opt-in runner diagnostics with:

```sh
make check-runner
make check-e2e-runner
```

`check-runner` verifies that the runner handles passes, intentional
failures, and timeouts. `check-e2e-runner` checks the live driver protocol
against mGBA.

CI may stage only JSON, logs, PNG screenshots, and save files belonging to
an individual scenario. Do not upload ROMs, ELF and map files, symbol files,
mGBA binaries, or shared build output as test artifacts.

## Test-development constraints

Every C suite linked against production code must leave at least:

- 8 KiB of unused EWRAM
- 1.5 KiB between the end of static IWRAM and the system stack base at
  `0x03007E00`

Keep large fixture tables constant so they remain in ROM. Large local
arrays, deep test-only call chains, and new zero-initialized buffers can use
up the memory needed for these margins; avoid them. Use the
`TEST_MIN_EWRAM_FREE` and `TEST_MIN_IWRAM_STACK` overrides to diagnose a
problem. Fix the memory use rather than lowering the committed thresholds.

`make clean` removes the gameplay and E2E build trees. After changing test
infrastructure, check a normal `TESTING=0` build: it must still have the
documented production SHA-1, with no test object paths or test-only symbols.
