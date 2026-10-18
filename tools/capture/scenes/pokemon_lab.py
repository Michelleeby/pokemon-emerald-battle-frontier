#!/usr/bin/env python3
"""Capture the production Team Lab editor flow for the player guide."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import shutil
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from e2e.team_lab import (  # noqa: E402
    boot_to_field,
    open_team_lab_editor,
    wait_callback,
)
from tools.capture.encode_gif import encode_gif  # noqa: E402


DEFAULT_ROM = ROOT / "pokeemerald.gba"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "pokemon-lab-editor-tutorial"
SAVE_SOURCE = ROOT / "tests" / "fixtures" / "saves" / "real-save.sav"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _prepare_output(output: Path) -> tuple[Path, str]:
    output = Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"capture output must be a fresh directory: {output}")
    output.mkdir(parents=True, exist_ok=True)
    source_hash = _sha256(SAVE_SOURCE)
    working_save = output / "working.sav"
    shutil.copyfile(SAVE_SOURCE, working_save)
    if _sha256(SAVE_SOURCE) != source_hash:
        raise RuntimeError("the public save fixture changed while preparing capture")
    if _sha256(working_save) != source_hash:
        raise RuntimeError("working save is not an exact copy of the public fixture")
    return output, source_hash


def _snapshot(game: Session, output: Path, name: str) -> str:
    source = game.screenshot(name)
    destination = output / name
    shutil.copyfile(source, destination)
    return name


def _symbols(rom: Path) -> dict[str, int]:
    return require_symbols(
        load_symbols(Path(rom).with_suffix(".elf")),
        "CB2_Pokenav",
        "CB2_Overworld",
        "CB2_TeamLabScreen",
        "CB2_UpdatePartyMenu",
        "gMain",
        "gSaveBlock1Ptr",
        "sLockFieldControls",
    )


def _write_report(
    output: Path,
    rom: Path,
    source_hash: str,
    frame_count: int,
    stride: int,
    snapshots: list[str],
) -> None:
    if _sha256(SAVE_SOURCE) != source_hash:
        raise RuntimeError("the public save fixture changed during capture")
    report = {
        "rom_sha256": _sha256(rom),
        "source_save_sha256": source_hash,
        "source_save_unchanged": True,
        "working_save_sha256": _sha256(output / "working.sav"),
        "frame_count": frame_count,
        "stride": stride,
        "snapshots": snapshots,
    }
    (output / "capture-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )


def capture_pokemon_lab(rom: Path, output: Path, *, stride: int = 4) -> int:
    """Capture build, nature, stat, move, and save interactions."""

    output, source_hash = _prepare_output(output)
    frames = output / "frames"
    symbols = _symbols(rom)
    snapshots: list[str] = []
    frame_count = 0

    with Session(Path(rom), output / "session", save=output / "working.sav") as game:
        boot_to_field(game, symbols)
        game.capture_start(frames, stride=stride)
        try:
            open_team_lab_editor(game, symbols)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "team-lab-build.png"))

            # Open and back out of the nature selector without changing the build.
            game.press("DOWN", held_frames=1, released_frames=30)
            game.press("A", held_frames=1, released_frames=30)
            game.run_frames(90)
            game.press("B", held_frames=1, released_frames=30)
            game.run_frames(60)

            game.press("R", held_frames=1, released_frames=30)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "team-lab-stats.png"))

            # A held on the selected stat previews the other column in red.  On
            # release the column is selected; edit it once and immediately undo.
            game.set_keys("A")
            game.run_frames(30)
            game.set_keys()
            game.run_frames(30)
            game.press("RIGHT", held_frames=1, released_frames=30)
            game.press("LEFT", held_frames=1, released_frames=30)
            game.set_keys("A")
            game.run_frames(30)
            game.set_keys()
            game.run_frames(30)
            game.press("LEFT", held_frames=1, released_frames=30)
            game.press("RIGHT", held_frames=1, released_frames=30)
            game.run_frames(90)

            game.press("R", held_frames=1, released_frames=30)
            game.run_frames(90)
            game.press("RIGHT", held_frames=1, released_frames=30)
            game.run_frames(60)
            game.press("SELECT", held_frames=1, released_frames=30)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "team-lab-moves.png"))

            game.press("START", held_frames=1, released_frames=30)
            wait_callback(game, symbols, "CB2_UpdatePartyMenu")
            game.run_frames(90)
            frame_count = game.capture_stop()
        except BaseException:
            game.capture_stop()
            raise

    _write_report(output, rom, source_hash, frame_count, stride, snapshots)
    return frame_count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=4)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)
    if not args.no_encode and args.stride != 4:
        raise ValueError("guide encoding requires --stride 4; use --no-encode otherwise")

    count = capture_pokemon_lab(args.rom, args.output, stride=args.stride)
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "pokemon-lab-editor-tutorial.gif"
        encode_gif(args.output / "frames", gif, fps=60 / args.stride)
        print(f"Encoded {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
