#!/usr/bin/env python3
"""Capture the production Audio options screen and trainer music picker."""

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

from e2e.overworld_menu import select_start_action  # noqa: E402
from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from e2e.team_lab import boot_to_field  # noqa: E402
from tools.capture.encode_gif import encode_gif  # noqa: E402


DEFAULT_ROM = ROOT / "pokeemerald.gba"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "audio-options"
SAVE_SOURCE = ROOT / "tests" / "fixtures" / "saves" / "real-save.sav"
MENU_ACTION_OPTION = 6


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
    shutil.copyfile(source, output / name)
    return name


def _symbols(rom: Path) -> dict[str, int]:
    return require_symbols(
        load_symbols(Path(rom).with_suffix(".elf")),
        "CB2_Overworld",
        "gMain",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "sLockFieldControls",
        "sNumStartMenuActions",
        "sStartMenuCursorPos",
        "sCurrentStartMenuActions",
        "gMPlayInfo_BGM",
        "gSongTable",
        "sCurrentMapMusic",
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


def capture_audio_options(rom: Path, output: Path, *, stride: int = 4) -> int:
    """Capture legacy defaults, a Hoenn Trainer preview, and custom choices."""

    output, source_hash = _prepare_output(output)
    frames = output / "frames"
    symbols = _symbols(rom)
    snapshots: list[str] = []
    frame_count = 0

    with Session(Path(rom), output / "session", save=output / "working.sav") as game:
        boot_to_field(game, symbols)
        game.capture_start(frames, stride=stride)
        try:
            select_start_action(game, symbols, MENU_ACTION_OPTION)
            game.run_frames(90)
            for _ in range(3):
                game.press("DOWN", held_frames=1, released_frames=20)
            game.press("A", held_frames=1, released_frames=30)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "audio-options.png"))

            # Enter Trainer Music, select the first Hoenn Trainer catalog item,
            # and preview it explicitly with SELECT.
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("A", held_frames=1, released_frames=30)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("SELECT", held_frames=1, released_frames=30)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "trainer-music-picker.png"))
            game.press("B", held_frames=1, released_frames=30)

            # Keep the normal menu cursor flow visible: Stereo, Random music,
            # All Battles pool, and Low HP Beep On.
            game.press("UP", held_frames=1, released_frames=20)
            game.press("RIGHT", held_frames=1, released_frames=20)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("A", held_frames=1, released_frames=30)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("A", held_frames=1, released_frames=30)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("RIGHT", held_frames=1, released_frames=20)
            game.press("DOWN", held_frames=1, released_frames=20)
            game.press("RIGHT", held_frames=1, released_frames=20)
            game.run_frames(90)
            snapshots.append(_snapshot(game, output, "audio-options-custom.png"))
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

    count = capture_audio_options(args.rom, args.output, stride=args.stride)
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "audio-options.gif"
        encode_gif(args.output / "frames", gif, fps=60 / args.stride)
        print(f"Encoded {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
