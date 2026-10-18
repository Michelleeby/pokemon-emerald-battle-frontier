# Player documentation media

The Lab animation and Lab/Audio screenshots come from the default production
ROM, built with `make rom` and the existing `agbcc` toolchain. The capture
scenes use the normal game menus and the game's own guided tutorial.

The tutorial starts a fresh new game and records continuously through the
complete guided introduction, without cuts or added demonstration steps.
The separate Lab and Audio screenshots use copies of the public anonymized
fixture at `tests/fixtures/saves/real-save.sav`, which stays unchanged.
Private saves, working saves, ROMs, and raw capture output are not part of
these assets.

| Files | Content |
| --- | --- |
| `pokemon-lab-editor-tutorial.gif` | New-game opening and the complete guided tutorial: species search and selection, nature, ability, held-item selection, EV presets, typing and accepting a move name, and finishing the demonstration |
| `team-lab-build.png`, `team-lab-stats.png`, `team-lab-moves.png` | Current Lab pages, centered headings, stacked controls, and shared footer |
| `audio-options.png` | Default trainer music, Hoenn + Kanto pool, retained Mono sound, and Low HP Beep Off |
| `trainer-music-picker.png` | Named tracks and the Select preview control |
| `audio-options-custom.png` | Stereo, Random, All Battles, and Low HP Beep On |

The arrival, facility, Hard mode, and Tucker assets do not contain Lab
screens. Their capture scenes remain under `tools/capture/scenes/`.

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

The PNGs are native 240 × 160 captures. The tutorial GIF uses a six-frame
capture stride, 10 fps at the original game speed, and 2× nearest-neighbor
scaling. Add `--no-encode` to the tutorial command to retain its raw PNG
frames. Each scene writes a `capture-report.json` with the production ROM hash
and frame count. The screenshot scenes also record the public fixture hash
and snapshot filenames.

## Current capture provenance

- Gameplay revision: `f333c96df9747b95687945c76e0da55fb9ed62d5`.
- Production ROM SHA-1: `82c1c06766be52b47c8f2c50153561208ba86128`.
- Production ROM SHA-256:
  `b6bcfba3f53d897bfd3fbf5a09a0a9cb5ab823d452e9bf63ab818505d7d9189d`.
- Public fixture SHA-256:
  `da9e9e05d005078483b1b4127775a9b9e5022c80a2672f7cf218ca7326fe1890`.

The complete new-game/tutorial recording contains 1,012 captured frames and
lasts 101.2 seconds. The separate Lab-page recording contains 489 frames; the
Audio recording contains 269. Both screenshot scenes verified that the public
fixture remained unchanged.
