# Gameplay tests

Stage 4 builds production gameplay code with the modern compiler and
`TESTING=1`, isolated under `build/test/`. Production objects are compiled
once and each suite is linked into its own ROM, so every emulator invocation
starts with clean GBA state while avoiding twelve full recompilations.

Run one or more suites with:

```sh
make check TESTS="frontier-common save-load"
make check-all
make list-tests
```

The green gameplay suite inventory is `frontier-common`, `team-lab`,
`new-game-tutorial`, `save-load`, `battle-shared`, and the Stage 5
`frontier-tower`, `frontier-factory`, `frontier-dome`, `frontier-arena`,
`frontier-palace`, `frontier-pike`, and `frontier-pyramid` suites. Every fixture clears save blocks, storage, both
parties, and seeds both game RNGs explicitly.

## Coverage ownership

| Behavior | Automated coverage | Follow-up owner and reason |
|---|---|---|
| Normal/hard Frontier record slots and Tower mode boundaries | `frontier-common` | Follow-up: cancel paths, record-computer UI, and full facility scripts require input-driven integration coverage. |
| Team Lab validation, legal and pre-evolution moves, creation, IV/EV mutation, nature, and deterministic personality generation | `team-lab` | Follow-up: editor UI, item/move search, party capacity, and summary rendering require input-driven tests. |
| Tutorial start, first action, double-tap skip, and completion | `new-game-tutorial` | Follow-up: complete onboarding, name/avatar UI, ferry transition, and all checkpoints require frame-driven tests. |
| Party plus new Frontier fields preserved across an in-memory save-block copy | `save-load` | Follow-up: flash write/checksum/load, corruption recovery, and facility restart require flash-backed fixtures. |
| Shared controller command encoding and first-battle setup | `battle-shared` | Follow-up: recorded, Safari, link, and full facility callbacks require battle-state fixtures. |
| Battle Tower normal/hard initialization, Level 50/open levels, singles/doubles flags and parties, trainer-pool round boundaries, Anabel boundaries, win progression, mode isolation, result cleanup, disqualification, and pause/resume state; hard 19→21 Anabel boundary and hardest ordinary trainer pool through production scripts | `frontier-tower`; `tower-hard-anabel` E2E | The targeted E2E route owns the modified hard-mode behavior; unchanged vanilla plumbing remains outside E2E scope. |
| Battle Factory normal/hard initialization, Level 50/open rental ranges, first/middle/seventh trainer pools, rental rank and swap gating, opponent exclusion, opponent rental metadata, party reconstruction, Return replacement, Noland boundaries, hard-mode IV/AI behavior, mode-isolated progression, lost-state cleanup, battle flags, and pause preparation; hard 13→14 Noland boundary, 31-IV rentals, and hardest ordinary trainer pool through production scripts | `frontier-factory`; `factory-hard-noland`, `factory-hard-setup` E2E | The targeted E2E routes own the modified hard-mode behavior; unchanged vanilla plumbing remains outside E2E scope. |
| Battle Dome normal/hard initialization, mode-specific streak/record/championship data, first-through-final bracket generation and advancement, normal/hard trainer pools, player seeding, opponent preview and party levels, Tucker boundaries, singles/doubles flags, win/loss/retirement resolution, lost-state cleanup, and pause preparation; hard championship 2→3 Tucker boundary and hardest ordinary trainer pool through production scripts | `frontier-dome`; `dome-hard-tucker` E2E | The targeted E2E route owns the modified hard-mode behavior; unchanged tournament plumbing remains outside E2E scope. |
| Battle Arena normal/hard initialization, mode-isolated streak progression, first/middle/seventh and hardest trainer pools, Level 50/open-level parties, Arena battle flags, normal/hard Greta boundaries, lost/retirement cleanup, pause preparation, Mind and Skill point accounting, Body HP snapshots, judgment ties and forced results, and the production three-turn judgment trigger; hard 12→14 Greta boundary and hardest ordinary trainer pool through production scripts | `frontier-arena`; `arena-hard-greta` E2E | The targeted E2E route owns the modified hard-mode behavior; unchanged Arena battle plumbing is out of scope. |
| Battle Palace normal/hard initialization, mode-isolated streak and record progression, shared first/middle/seventh and hardest trainer selection, Level 50 singles and open-level doubles parties and flags, normal/hard Spenser boundaries, lost/retirement cleanup, pause preparation, and real nature/HP/PP-driven move-group selection and fallback | `frontier-palace` | Targeted E2E gap: prove the hard-mode hardest trainer pool and shortened Spenser boundary through production scripts. Unchanged Palace battle plumbing is out of scope. |
| Battle Pike normal/hard initialization and mode isolation; hinted, constrained, healing-disabled, status, wild, single, hard, double, Brain, and final-room behavior; real random status infliction and reporting; partial/full healing and held-item restoration; wild table tiers, moves, Level 50/open scaling, and Keen Eye suppression; trainer pools, parties, and battle flags; normal/hard Lucy boundaries; streak/record/total progression; lost/retirement cleanup; and pause preparation | `frontier-pike` | Targeted E2E gap: prove the hard-mode hardest trainer pool and shortened Lucy boundary through production scripts. Unchanged room traversal is out of scope. |
| Battle Pyramid normal/hard initialization and mode isolation; deterministic floor layout and object generation; trainer and item events; shared trainer-pool round boundaries; Level 50 and open-level wild tiers, moves, and level scaling; high-streak wild IV scaling; Pyramid battle flags and parties; party restoration after move mutation; light-radius progression and clamp; normal/hard Brandon boundaries; streak/record progression; escape-preserving and defeat cleanup state transitions; pause preparation; summit boundary; and floor/top location detection | `frontier-pyramid` | Targeted E2E gap: prove the hard-mode hardest trainer pool and shortened Brandon boundary through production scripts. Unchanged floor traversal is out of scope. |

The remaining E2E follow-ups are limited to behavior changed by hard mode:
hardest trainer-pool selection and shortened Frontier Brain boundaries. The
Factory also owns the only modified rental-IV path. Unchanged vanilla facility
plumbing, presentation, cancellation, traversal, and save/restart behavior are
not E2E goals for this project.

## End-to-end harness development

Stage 7 has a project-owned headless driver linked against the pinned sibling
mGBA build. The first driver slice supports normal ROM boot, scenario-local
save files, exact frame advancement, keypad input, memory reads, bounded memory
predicates, PNG screenshots, and complete emulator-core restart. A Python
session wrapper enforces wall-clock response timeouts, separates emulator logs
from the command protocol, and records a replayable `input-trace.json`.

Build the driver with:

```sh
make e2e-runner
```

Run named gameplay scenarios through the same headless session path with:

```sh
make e2e TESTS="tower-hard-anabel factory-hard-noland factory-hard-setup arena-hard-greta dome-hard-tucker"
```

Omitting `TESTS` runs every registered E2E scenario. Unknown or duplicate
scenario names fail before execution. The scenarios intentionally cover only
behavior changed by hard mode, not unchanged facility plumbing.
`tower-hard-anabel` seeds the shortened hard boundary at 19, enters through the
production challenge UI, verifies that the ordinary opponent comes from the
hardest trainer pool, wins that battle and the following Anabel battle, and
checks the final hard streak of 21 without changing the normal streak.
`factory-hard-noland` seeds a hard Factory streak of 13, enters through the
production challenge and rental-selection UI, verifies 31-IV rentals, defeats
Noland once, and checks the final hard streak of 14 without changing the normal
streak. Because the Brain branch does not increment the ordinary Factory
challenge counter, the completed route retains a battle number of zero.
`factory-hard-setup` enters a fresh hard Factory challenge only far enough to
verify 31-IV rentals and an ordinary opponent from the hardest trainer pool;
it does not replay the vanilla seven-battle route.
`arena-hard-greta` seeds a hard Arena streak of 12, verifies that the first
ordinary opponent comes from the hardest trainer pool, defeats that opponent
and Greta, and checks the final hard streak of 14 without changing the normal
streak.
`dome-hard-tucker` seeds a hard Dome championship streak of 2, verifies that
all three ordinary tournament opponents come from the hardest trainer pool,
defeats Tucker in the final, and checks the final hard streak of 3 without
changing the normal streak.

The assistance seam exists only in the E2E gameplay build and is restricted to
Tower, Factory, Dome, and Arena special trainer battles. Production still constructs the
facility opponent and enters the ordinary facility end-of-battle handling, so
the scenarios cover the surrounding scripts, state progression, warps,
rewards, and saves without repeatedly exercising vanilla battle strategy. The
release ROM contains neither the assisted task nor its outcome counter.

CI runs each E2E scenario in a separate matrix job with a 30-minute job
timeout. Run long scenarios separately when a local command host imposes a
shorter aggregate wall-clock limit.

Run its host unit tests and live mGBA integration diagnostic with:

```sh
make check-e2e-runner
```

The live diagnostic uses the ordinary release ROM entry path. It verifies the
physical keypad register, frame execution, memory predicates, screenshot
output, isolated flash creation, protocol-error reporting, and destruction and
recreation of the mGBA core. This diagnostic validates harness mechanics and
is separate from the named gameplay coverage above.

The driver is written to `build/e2e/mgba-e2e`; scenario reports include both
the release and gameplay ROM SHA-256 values and disclose the assisted-outcome
policy. Logs, traces,
screenshots, and scenario-local saves are written below
`build/e2e/artifacts/<scenario>/`. `make clean` removes the entire `build/e2e`
directory and the isolated `build/e2e-fixture-obj` and
`build/e2e-gameplay-obj` object trees. E2E failure
artifacts must not include ROM, ELF, map, symbol, mGBA binary, or shared
build-bundle output.

`tests/e2e_manifest.json` owns E2E selection independently from the C-suite
manifest. Tower changes select every scenario because the assisted gameplay
seam is implemented in the shared special-battle dispatcher, Factory changes
select both targeted Factory scenarios, Dome changes select `dome-hard-tucker`,
Arena changes select `arena-hard-greta`,
E2E infrastructure changes select every scenario, and
documentation-only or explicitly uncovered facility changes select none.
Unknown relevant gameplay paths conservatively select every E2E scenario.

CI runs selected scenarios in a `fail-fast: false` matrix. Each matrix job
checks out and builds the pinned mGBA revision and builds its own release ROM,
fixture ROM, and native driver in its transient workspace; these build outputs
are never transferred between jobs or uploaded. A no-scenarios job preserves
the documentation-only path. The stable `Tests / required` job requires either
the complete selected E2E matrix or the no-scenarios job to pass, as well as
successful artifact cleanup.

Every scenario report records the commit SHA, release and gameplay ROM
SHA-256 values, assisted-outcome policy, pinned mGBA revision, scenario and
fixture versions, fixed RNG seeds, RTC policy, driver response timeout,
bounded-frame-wait policy, duration, status, and failed predicate. Failures are
classified as assertion failures, driver timeouts, or runner crashes. The
runner attempts a failure screenshot while the emulator is still responsive.
Only JSON, logs, PNGs, and scenario-local saves are copied to the CI upload
staging directory; ROM, ELF, map, symbol, and executable output is excluded.
Workflow cleanup deletes both transient gameplay bundles and E2E diagnostics
after matrix completion.

## Determinism and diagnostics

Stage 2 pinned mGBA 0.10.5 at commit
`26b7884bc25a5933960f3cdcd98bac1ae14d42e2`. mGBA is MPL-2.0 licensed and is
built as a separate executable; no mGBA source is copied into this tree.

The ROM transport uses mGBA debug registers and `swi 0x0F`. Each run writes an
emulator log and JSON report under `build/test/artifacts/<suite>/`; ELF and map
files remain under `build/test/roms/`.

The Stage 2 pass/fail/hang fixtures remain available as opt-in harness
diagnostics:

```sh
make check-runner
```

They are not gameplay coverage and `make check-all` never runs them.

## Memory headroom

Every production-linked gameplay suite is checked after linking. The build
requires at least 8 KiB of unused EWRAM and 1.5 KiB between the end of static
IWRAM and the system stack base at `0x03007E00`. The latter is the practical
stack allowance; the linker's nominal free-IWRAM figure also includes the
upper 512-byte region reserved above the system stack base.

At the start of Stage 5, `frontier-common` uses 249,766 of 262,144 EWRAM bytes
and ends static IWRAM at `0x030076D0`. This leaves 12,378 EWRAM bytes and 1,840
bytes of practical system-stack headroom. The thresholds leave limited room
for small test seams while retaining a clear failure before static data can
silently collide with normal runtime stack use.

Keep facility fixture tables constant so they remain in ROM. Avoid large local
arrays, deep test-only call chains, and new zero-initialized test buffers. The
minimums can be overridden for diagnosis with `TEST_MIN_EWRAM_FREE` and
`TEST_MIN_IWRAM_STACK`, but lowering them is not an acceptable committed fix
for a failing suite.

## Release-build isolation

The modified game intentionally does not match the vanilla ROM hash. On
August 13, 2026, a clean legacy `TESTING=0` build established this SHA-256:

```text
fa441bb7e5146f5acdcc9be46b026a46f8fd1dad424b1d20eb91ffbe7f312336  pokeemerald.gba
```

After `make -j2 test-roms` built all twelve production-linked suites under
`build/test/`, a normal `make -j2` retained the same SHA-256. The release map
also contained no test object paths or test-only entry-point symbols. This
demonstrates that the `TESTING=1` seams and objects do not contaminate the
legacy `TESTING=0` output. The clean run also verifies that `test-roms` creates
its ROM output directory instead of relying on a prior test build.

## Suite selection

`manifest.json` maps changed paths to executable gameplay suites. Selection
is conservative: shared test/build infrastructure and unclassified gameplay
paths select every executable suite, while documentation-only changes select
none. Facility-local changes select their facility suite and core dependencies.

Validate selection with:

```sh
python3 tools/testing/validate_manifest.py
python3 -m unittest discover -s tools/testing -p 'test_*.py' -v
python3 tools/testing/select_suites.py --files src/battle_tower.c
python3 tools/testing/select_suites.py --base main --head HEAD
```

## Stage 5 CI verification

Stage 5 was completed on August 13, 2026 after verification with real GitHub
pull-request runs:

- [PR #3 full-suite run](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/actions/runs/31697145243): all twelve gameplay suites and `Tests / required` passed; cleanup left zero artifacts.
- [Documentation-only run](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/actions/runs/31698917663): the gameplay build and matrix were skipped, the no-suites path passed, and `Tests / required` passed.
- [Deliberate suite-failure run](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/actions/runs/31699192842): `frontier-tower` failed, the other eleven suites passed, cleanup ran, and `Tests / required` failed.
- [Concurrency-cancellation run](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/actions/runs/31699690805): the superseded build and matrix were canceled, cleanup ran, and `Tests / required` failed.
- [Deliberate job-timeout run](https://github.com/Michelleeby/pokemon-emerald-battle-frontier/actions/runs/31699843463): only `frontier-tower` timed out, the other eleven suites passed, cleanup left zero artifacts, and `Tests / required` failed.

Per-facility mutation checks were intentionally skipped in the completion
audit because representative regressions were introduced and detected while
the suites were developed. The deliberate Tower assertion above separately
verified end-to-end failure propagation. No temporary verification mutation,
workflow timeout, PR, or branch remains in the Stage 5 implementation.
