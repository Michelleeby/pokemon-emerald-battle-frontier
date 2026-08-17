#!/usr/bin/env python3
"""Capture the new-game route from the title screen to the Battle Frontier."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from tools.capture.encode_gif import encode_gif  # noqa: E402


DEFAULT_ROM = ROOT / "pokeemerald.gba"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "frontier-arrival"
README_RANGES = [(222, 295), (340, 560), (680, 820), (850, 1031)]


def capture_frontier_arrival(rom: Path, output: Path, *, stride: int = 2) -> int:
    """Record a deterministic new game and its arrival at the Frontier."""

    output = Path(output).resolve()
    frames = output / "frames"
    if frames.exists() and any(frames.glob("frame-*.png")):
        raise FileExistsError(f"capture frames already exist: {frames}")

    save = output / "frontier-arrival.sav"
    if save.exists():
        raise FileExistsError(f"capture save already exists: {save}")

    with Session(Path(rom), output / "session", save=save) as game:
        # Let the unattended boot animation reach its stable title sequence.
        game.run_frames(900)
        game.capture_start(frames, stride=stride)

        # Title screen, main menu, and Professor Birch's opening prompt.
        game.press("START", released_frames=180)
        game.press("A", released_frames=180)
        game.press("A", released_frames=180)
        game.press("A", released_frames=180)

        # Choose the first avatar, enter a one-letter name, and accept the
        # default Blaziken starter offered by this project.
        game.press("A", released_frames=120)
        game.press("A", released_frames=120)
        game.press("A", released_frames=120)
        game.press("A", released_frames=20)
        game.press("START", released_frames=60)
        game.press("A", released_frames=120)
        game.press("A", released_frames=300)
        game.press("A", released_frames=120)

        # The production new-game script boards the ferry and arrives at the
        # Frontier. Stop on the first fully visible landing frame, before the
        # guided Team Lab tutorial opens the PokéNav.
        game.press("A", released_frames=347)

        return game.capture_stop()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=2)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)

    if not args.no_encode and args.stride != 2:
        raise ValueError("README encoding requires --stride 2; use --no-encode otherwise")
    count = capture_frontier_arrival(args.rom, args.output, stride=args.stride)
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "frontier-arrival.gif"
        encode_gif(
            args.output / "frames",
            gif,
            fps=60 / args.stride,
            ranges=README_RANGES,
        )
        print(f"Encoded {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
