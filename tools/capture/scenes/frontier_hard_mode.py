#!/usr/bin/env python3
"""Capture Hard Mode selection at the Battle Tower."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from e2e.tower import (  # noqa: E402
    wait_for_tower_lobby,
    wait_for_value,
)
from tools.capture.encode_gif import encode_gif  # noqa: E402
from tools.capture.fixture import create_capture_tower_save  # noqa: E402


DEFAULT_ROM = ROOT / "pokeemerald.gba"
DEFAULT_ELF = ROOT / "pokeemerald.elf"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "frontier-hard-mode"
README_RANGES = [(150, 211), (320, 520)]


def _advance_to_choice(game: Session, result: int) -> None:
    for _ in range(20):
        if game.read(result, width=8) == 0xFF:
            return
        game.press("A", held_frames=1, released_frames=59)
    raise RuntimeError("Tower receptionist dialogue did not reach a choice menu")


def capture_frontier_hard_mode(
    rom: Path,
    elf: Path,
    output: Path,
    *,
    stride: int = 2,
) -> int:
    """Record the production Battle Tower flow through Hard Mode selection."""

    output = Path(output).resolve()
    frames = output / "frames"
    if frames.exists() and any(frames.glob("frame-*.png")):
        raise FileExistsError(f"capture frames already exist: {frames}")

    save = output / "frontier-hard-mode.sav"
    if save.exists():
        raise FileExistsError(f"capture save already exists: {save}")

    symbols = require_symbols(
        load_symbols(Path(elf)),
        "BattleFrontier_BattleTowerLobby_Layout",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "gSaveBlock1Ptr",
        "gSpecialVar_Result",
        "sLockFieldControls",
    )
    create_capture_tower_save(output / "fixture", save)

    with Session(Path(rom), output / "session", save=save) as game:
        wait_for_tower_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleTowerLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        game.run_frames(120)
        wait_for_value(game, symbols["sLockFieldControls"], 0)

        game.capture_start(frames, stride=stride)
        game.run_frames(60)
        game.press("A", held_frames=1, released_frames=59)
        wait_for_value(game, symbols["sLockFieldControls"], 1)
        _advance_to_choice(game, symbols["gSpecialVar_Result"])

        # Choose CHALLENGE, then wait for the four level/mode combinations.
        game.run_frames(90)
        game.press("A", held_frames=1, released_frames=59)
        wait_for_value(game, symbols["gSpecialVar_Result"], 0, width=16)
        _advance_to_choice(game, symbols["gSpecialVar_Result"])
        game.run_frames(90)

        # NORMAL LV. 50, NORMAL OPEN LEVEL, HARD LV. 50, HARD OPEN LEVEL.
        game.press("DOWN", held_frames=1, released_frames=59)
        game.press("DOWN", held_frames=1, released_frames=90)
        game.press("A", held_frames=1, released_frames=120)
        game.run_frames(90)

        return game.capture_stop()


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--elf", type=Path, default=DEFAULT_ELF)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=2)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)

    if not args.no_encode and args.stride != 2:
        raise ValueError("README encoding requires --stride 2; use --no-encode otherwise")
    count = capture_frontier_hard_mode(
        args.rom,
        args.elf,
        args.output,
        stride=args.stride,
    )
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "frontier-hard-mode.gif"
        encoded = encode_gif(
            args.output / "frames",
            gif,
            fps=60 / args.stride,
            ranges=README_RANGES,
        )
        print(f"Encoded {encoded} frames to {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
