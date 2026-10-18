# Player documentation media

The Lab tutorial and the Lab and Audio screenshots come from the default
production ROM, built with `make rom` and the existing `agbcc` toolchain.
Capture scripts use the game menus and the game's own guided tutorial.

The tutorial starts on the PokéNav main menu with Condition highlighted,
matching the original Lab video. It continues through the complete guided
Lab sequence without cuts or added steps. The script reaches this point
through a fresh game; character setup and arrival stay in the arrival video.
The screenshots use copies of the public anonymized fixture at
`tests/fixtures/saves/real-save.sav`. The original fixture stays unchanged.
Private saves, working saves, ROMs, and raw captures stay out of these assets.

| Files | Content |
| --- | --- |
| `pokemon-lab-editor-tutorial.gif` | PokéNav Condition selection and the complete guided Lab tutorial: species search and selection, nature, ability, held-item selection, EV presets, typing and accepting a move name, and finishing the demonstration |
| `team-lab-build.png`, `team-lab-stats.png`, `team-lab-moves.png` | Current Lab pages, centered headings, stacked controls, and shared footer |
| `audio-options.png` | Default trainer music, Hoenn + Kanto pool, retained Mono sound, and Low HP Beep Off |
| `trainer-music-picker.png` | Named tracks and the Select preview control |
| `audio-options-custom.png` | Stereo, Random, All Battles, and Low HP Beep On |

The arrival, facility, Hard mode, and Tucker videos have no Lab screens to
update. Their capture scripts remain under `tools/capture/scenes/`.

## Recapture

Build the production ROM and the existing mGBA driver first. The driver
requires the sibling mGBA build described in [the testing guide](../tests/README.md).
GIF encoding also requires `ffmpeg` on `PATH`.

Run these commands from the repository root, using fresh output directories:

```sh
make -j2 rom
make e2e-runner
python3 -m tools.capture.scenes.tutorial_capture --output build/capture/docs-tutorial
python3 -m tools.capture.scenes.pokemon_lab --output build/capture/docs-lab --no-encode
python3 -m tools.capture.scenes.audio_options --output build/capture/docs-audio --no-encode
cp build/capture/docs-tutorial/pokemon-lab-editor-tutorial.gif assets/
cp build/capture/docs-lab/team-lab-{build,stats,moves}.png assets/
cp build/capture/docs-audio/{audio-options,trainer-music-picker,audio-options-custom}.png assets/
```

The PNGs are native 240 × 160 captures. The tutorial records every sixth
frame and plays at 10 fps to preserve the original game speed. The GIF uses
2× nearest-neighbor scaling. Add `--no-encode` to capture PNG frames without
encoding a GIF. Each script writes a `capture-report.json` with the production
ROM hash and frame count. Screenshot reports also include the public fixture
hash and screenshot filenames.

## Current capture provenance

- Gameplay revision: `22be2f73cd31f41c1f658e0aec000a9b089202ea`.
- Production ROM SHA-1: `82c1c06766be52b47c8f2c50153561208ba86128`.
- Production ROM SHA-256:
  `b6bcfba3f53d897bfd3fbf5a09a0a9cb5ab823d452e9bf63ab818505d7d9189d`.
- Public fixture SHA-256:
  `da9e9e05d005078483b1b4127775a9b9e5022c80a2672f7cf218ca7326fe1890`.

The complete Lab tutorial recording contains 657 captured frames and
lasts 65.7 seconds. The separate Lab-page recording contains 489 frames; the
Audio recording contains 269. Both screenshot scenes verified that the public
fixture remained unchanged.
