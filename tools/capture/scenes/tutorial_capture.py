#!/usr/bin/env python3
"""Record the unsteered production Team Lab tutorial from a new game."""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from tools.capture.encode_gif import encode_gif  # noqa: E402


DEFAULT_ROM = ROOT / "pokeemerald.gba"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "pokemon-lab-editor-tutorial"


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _prepare_output(output: Path) -> Path:
    output = Path(output).resolve()
    if output.exists() and any(output.iterdir()):
        raise FileExistsError(f"capture output must be a fresh directory: {output}")
    output.mkdir(parents=True, exist_ok=True)
    save = output / "new-game.sav"
    if save.exists():
        raise FileExistsError(f"new-game save already exists: {save}")
    return output


def _symbols(rom: Path) -> dict[str, int]:
    # sTutorial is a production EWRAM symbol. Reading its active byte does not
    # alter the game and avoids test-only tutorial instrumentation.
    return require_symbols(load_symbols(Path(rom).with_suffix(".elf")), "sTutorial")


def _write_report(
    output: Path,
    rom: Path,
    save: Path,
    frame_count: int,
    stride: int,
    natural_completed: bool,
) -> None:
    report = {
        "rom_sha256": _sha256(rom),
        "raw_frame_count": frame_count,
        "stride": stride,
        "fresh_new_game": True,
        "natural_completed": natural_completed,
        "save": save.name,
    }
    (output / "capture-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )


def capture_tutorial(rom: Path, output: Path, *, stride: int = 6) -> int:
    """Capture the complete current production tutorial without steering it."""

    output = _prepare_output(output)
    frames = output / "frames"
    save = output / "new-game.sav"
    symbols = _symbols(rom)
    frame_count = 0
    natural_completed = False

    with Session(Path(rom), output / "session", save=save) as game:
        # Match the established new-game scene, recording continuously before
        # advancing the title screen and normal new-game prompts.
        game.run_frames(900)
        game.capture_start(frames, stride=stride)
        try:
            game.press("START", released_frames=180)
            game.press("A", released_frames=180)
            game.press("A", released_frames=180)
            game.press("A", released_frames=180)

            game.press("A", released_frames=120)
            game.press("A", released_frames=120)
            game.press("A", released_frames=120)
            game.press("A", released_frames=20)
            game.press("START", released_frames=60)
            game.press("A", released_frames=120)
            game.press("A", released_frames=300)
            game.press("A", released_frames=120)
            game.press("A", released_frames=347)

            # No input is supplied once the new-game route reaches Frontier;
            # the production tutorial owns every subsequent button press.
            game.set_keys()
            game.wait(symbols["sTutorial"], 1, width=8, max_frames=1800)
            game.wait(symbols["sTutorial"], 0, width=8, max_frames=18000)
            natural_completed = game.read(symbols["sTutorial"], width=8) == 0
            if not natural_completed:
                raise RuntimeError("production tutorial did not finish naturally")
            game.run_frames(180)
            frame_count = game.capture_stop()
        except BaseException:
            game.capture_stop()
            raise

    _write_report(output, rom, save, frame_count, stride, natural_completed)
    return frame_count


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=6)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)
    if not args.no_encode and args.stride != 6:
        raise ValueError("tutorial encoding requires --stride 6; use --no-encode otherwise")

    count = capture_tutorial(args.rom, args.output, stride=args.stride)
    print(f"Captured {count} PNG frames in {args.output.resolve() / 'frames'}")
    if not args.no_encode:
        gif = args.output.resolve() / "pokemon-lab-editor-tutorial.gif"
        encode_gif(args.output / "frames", gif, fps=60 / args.stride)
        print(f"Encoded {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
