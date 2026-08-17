#!/usr/bin/env python3
"""Capture and compose labeled exterior stills of all Frontier facilities."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import struct
import subprocess
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.session import Session  # noqa: E402
from tools.capture.fixture import create_frontier_facility_save  # noqa: E402


DEFAULT_ROM = ROOT / "build" / "capture" / "gameplay" / "pokeemerald.gba"
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "frontier-facilities"
FACILITIES = (
    ("battle-tower", "Battle Tower"),
    ("battle-dome", "Battle Dome"),
    ("battle-factory", "Battle Factory"),
    ("battle-palace", "Battle Palace"),
    ("battle-arena", "Battle Arena"),
    ("battle-pike", "Battle Pike"),
    ("battle-pyramid", "Battle Pyramid"),
)


def _png_dimensions(path: Path) -> tuple[int, int]:
    with path.open("rb") as image:
        header = image.read(24)
    if header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        raise ValueError(f"not a PNG: {path}")
    return struct.unpack(">II", header[16:24])


def _capture_still(rom: Path, output: Path, slug: str) -> Path:
    save = output / "saves" / f"{slug}.sav"
    fixture_dir = output / "fixtures" / slug
    session_dir = output / "sessions" / slug
    frames = output / "frames" / slug
    source = output / "source" / f"{slug}.png"

    for path in (save, source):
        if path.exists():
            path.unlink()
    if frames.exists():
        shutil.rmtree(frames)

    save.parent.mkdir(parents=True, exist_ok=True)
    source.parent.mkdir(parents=True, exist_ok=True)
    create_frontier_facility_save(fixture_dir, save, slug)

    with Session(rom, session_dir, save=save) as game:
        # Select Continue and give the exterior map, weather, camera, and NPCs
        # ample time to settle before retaining one native-resolution frame.
        for _ in range(8):
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        game.run_frames(360)
        game.capture_start(frames, stride=1)
        game.run_frames(2)
        game.capture_stop()

    captured = sorted(frames.glob("frame-*.png"))
    if not captured:
        raise RuntimeError(f"no frame captured for {slug}")
    shutil.copy2(captured[-1], source)
    if _png_dimensions(source) != (240, 160):
        raise RuntimeError(f"unexpected source dimensions for {source}")
    return source


def _compose_montage(output: Path, sources: list[Path]) -> Path:
    montage = output / "frontier-facilities.png"
    font = "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf"
    panel_w, panel_h = 480, 360
    canvas_w, canvas_h = panel_w * 4, panel_h * 2
    filters = []
    overlays = []

    for index, ((_, label), _) in enumerate(zip(FACILITIES, sources)):
        filters.append(
            f"[{index}:v]scale=480:320:flags=neighbor,"
            f"pad={panel_w}:{panel_h}:0:0:black,"
            f"drawtext=fontfile={font}:text='{label}':fontcolor=white:"
            "fontsize=25:x=(w-text_w)/2:y=326[label%d]" % index
        )

    filters.append(f"color=c=black:s={canvas_w}x{canvas_h}[base]")
    previous = "base"
    positions = [
        (0, 0), (480, 0), (960, 0), (1440, 0),
        (240, 360), (720, 360), (1200, 360),
    ]
    for index, (x, y) in enumerate(positions):
        result = "out" if index == len(positions) - 1 else f"stage{index}"
        overlays.append(f"[{previous}][label{index}]overlay={x}:{y}[{result}]")
        previous = result

    command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y"]
    for source in sources:
        command.extend(("-i", str(source)))
    command.extend(
        (
            "-filter_complex",
            ";".join(filters + overlays),
            "-map",
            "[out]",
            "-frames:v",
            "1",
            str(montage),
        )
    )
    subprocess.run(command, check=True)
    return montage


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--rom", type=Path, default=DEFAULT_ROM)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--compose-only", action="store_true")
    args = parser.parse_args(argv)

    output = args.output.resolve()
    if args.compose_only:
        sources = [output / "source" / f"{slug}.png" for slug, _ in FACILITIES]
    else:
        sources = [_capture_still(args.rom.resolve(), output, slug) for slug, _ in FACILITIES]
    missing = [source for source in sources if not source.is_file()]
    if missing:
        raise FileNotFoundError(f"missing source screenshots: {missing}")
    montage = _compose_montage(output, sources)
    print(f"Created {montage}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
