#!/usr/bin/env python3
"""Capture the Battle Tower's Frontier Brain Anabel sequence."""

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
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    MAP_NUM_TOWER_BATTLE_ROOM,
    MAP_NUM_TOWER_LOBBY,
    current_map,
    map_id,
    tower_mode_addresses,
)
from tools.capture.encode_gif import encode_gif  # noqa: E402
from tools.capture.fixture import create_capture_tower_save  # noqa: E402


DEFAULT_OUTPUT = ROOT / "build" / "capture" / "frontier-brain-anabel"
TRAINER_FRONTIER_BRAIN = 1022
CHALLENGE_STATUS_WON = 3


def capture_frontier_brain_anabel(output: Path, *, stride: int = 2) -> int:
    """Record Anabel's reveal, assisted battle, and Tower result sequence."""

    output = Path(output).resolve()
    frames = output / "frames"
    if frames.exists() and any(frames.glob("frame-*.png")):
        raise FileExistsError(f"capture frames already exist: {frames}")

    save = output / "frontier-brain-anabel.sav"
    if save.exists():
        raise FileExistsError(f"capture save already exists: {save}")

    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
    )
    create_capture_tower_save(output / "fixture", save, anabel=True)

    with Session(GAMEPLAY_ROM, output / "session", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_TOWER_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise RuntimeError("Continue did not reach the Tower battle room")

        # Complete the ordinary boundary battle without recording. Start on
        # the first production frame where Anabel owns the opponent slot.
        for _ in range(1200):
            if (
                game.read(symbols["gTrainerBattleOpponent_A"], width=16)
                == TRAINER_FRONTIER_BRAIN
            ):
                break
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise RuntimeError("Tower route did not reach Anabel")

        game.capture_start(frames, stride=stride)
        game.run_frames(60)
        saw_won = False
        for _ in range(1200):
            save_block1 = game.read(symbols["gSaveBlock1Ptr"])
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            if (
                0x02000000 <= save_block1 < 0x02040000
                and 0x02000000 <= save_block2 < 0x02040000
            ):
                status = game.read(
                    tower_mode_addresses(save_block1, save_block2)["challenge_status"],
                    width=8,
                )
                saw_won |= status == CHALLENGE_STATUS_WON
                if (
                    saw_won
                    and status == 0
                    and current_map(game, symbols["gSaveBlock1Ptr"])
                    == map_id(MAP_NUM_TOWER_LOBBY)
                    and game.read(symbols["sLockFieldControls"], width=8) == 0
                ):
                    game.run_frames(90)
                    return game.capture_stop()
            game.press("A", held_frames=1, released_frames=29)

        raise RuntimeError("Anabel capture did not return to the Tower lobby")


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=2)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)

    if not args.no_encode and args.stride != 2:
        raise ValueError("README encoding requires --stride 2; use --no-encode otherwise")
    count = capture_frontier_brain_anabel(args.output, stride=args.stride)
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "frontier-brain-anabel.gif"
        encoded = encode_gif(args.output / "frames", gif, fps=60 / args.stride)
        print(f"Encoded {encoded} frames to {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
