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
| Battle Tower normal/hard initialization, Level 50/open levels, singles/doubles flags and parties, trainer-pool round boundaries, Anabel boundaries, win progression, mode isolation, result cleanup, disqualification, and pause/resume state | `frontier-tower` | Follow-up: full lobby cancel menus, input-driven retirement, seven actual battles, room warps, flash-backed restart, and multis partner interaction require frame-driven script/battle and initialized flash fixtures. |
| Battle Factory normal/hard initialization, Level 50/open rental ranges, first/middle/seventh trainer pools, rental rank and swap gating, opponent exclusion, opponent rental metadata, party reconstruction, Return replacement, Noland boundaries, hard-mode IV/AI behavior, mode-isolated progression, lost-state cleanup, battle flags, and pause preparation | `frontier-factory` | Follow-up: lobby cancel, rental-selection and swap-screen input, retirement/disqualification scripts, seven actual battles, room warps, Noland battle presentation, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |
| Battle Dome normal/hard initialization, mode-specific streak/record/championship data, first-through-final bracket generation and advancement, normal/hard trainer pools, player seeding, opponent preview and party levels, Tucker boundaries, singles/doubles flags, win/loss/retirement resolution, lost-state cleanup, and pause preparation | `frontier-dome` | Follow-up: lobby and tournament-tree cancel input, complete rendered previews, four actual battles, transition callbacks, room warps, Tucker presentation, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |
| Battle Arena normal/hard initialization, mode-isolated streak progression, first/middle/seventh and hardest trainer pools, Level 50/open-level parties, Arena battle flags, normal/hard Greta boundaries, lost/retirement cleanup, pause preparation, Mind and Skill point accounting, Body HP snapshots, judgment ties and forced results, and the production three-turn judgment trigger | `frontier-arena` | Follow-up: lobby cancel input, three actual turns and seven actual battles, rendered Mind/Skill/Body judgment presentation, transition callbacks, room warps, Greta presentation, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |
| Battle Palace normal/hard initialization, mode-isolated streak and record progression, shared first/middle/seventh and hardest trainer selection, Level 50 singles and open-level doubles parties and flags, normal/hard Spenser boundaries, lost/retirement cleanup, pause preparation, and real nature/HP/PP-driven move-group selection and fallback | `frontier-palace` | Follow-up: lobby cancel input, seven actual battles, rendered low-HP flavor text, transition callbacks, room warps, Spenser presentation, doubles target preferences, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |
| Battle Pike normal/hard initialization and mode isolation; hinted, constrained, healing-disabled, status, wild, single, hard, double, Brain, and final-room behavior; real random status infliction and reporting; partial/full healing and held-item restoration; wild table tiers, moves, Level 50/open scaling, and Keen Eye suppression; trainer pools, parties, and battle flags; normal/hard Lucy boundaries; streak/record/total progression; lost/retirement cleanup; and pause preparation | `frontier-pike` | Follow-up: input-driven path and lobby cancellation, complete fourteen-room traversal, actual trainer/wild battles, status-flash and NPC presentation, transition callbacks, room warps, Lucy presentation, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |
| Battle Pyramid normal/hard initialization and mode isolation; deterministic floor layout and object generation; trainer and item events; shared trainer-pool round boundaries; Level 50 and open-level wild tiers, moves, and level scaling; high-streak wild IV scaling; Pyramid battle flags and parties; party restoration after move mutation; light-radius progression and clamp; normal/hard Brandon boundaries; streak/record progression; escape-preserving and defeat cleanup state transitions; pause preparation; summit boundary; and floor/top location detection | `frontier-pyramid` | Follow-up: input-driven lobby cancellation, complete seven-floor traversal, rendered hints and light effects, actual item pickup and trainer/wild battles, frame-driven escape/defeat and summit scripts, map warps, Brandon presentation, and flash-backed restart require a host-driven frame/script harness and initialized flash fixture. |

The remaining follow-ups require frame-driven, presentation, or initialized
flash fixtures beyond the current C-level state suites. Replace each follow-up
with named coverage when the corresponding harness exists.

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
