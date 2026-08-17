#!/usr/bin/env python3
"""Encode numbered mGBA capture frames as an optimized README GIF."""

from __future__ import annotations

import argparse
from pathlib import Path
import shutil
import subprocess
import tempfile


FrameRange = tuple[int, int]


def _frame_count(frames: Path) -> int:
    paths = sorted(frames.glob("frame-*.png"))
    if not paths:
        raise FileNotFoundError(f"no capture frames found in {frames}")

    for index, path in enumerate(paths):
        expected = f"frame-{index:08d}.png"
        if path.name != expected:
            raise ValueError(
                f"capture frames must be contiguous; expected {expected}, found {path.name}"
            )
    return len(paths)


def _parse_range(value: str) -> FrameRange:
    try:
        start_text, end_text = value.split(":", 1)
        return int(start_text), int(end_text)
    except ValueError as error:
        raise argparse.ArgumentTypeError(
            "frame ranges must use the half-open START:END form"
        ) from error


def encode_gif(
    frames: Path,
    output: Path,
    *,
    fps: float = 30,
    scale: int = 2,
    ranges: list[FrameRange] | None = None,
) -> int:
    """Encode a contiguous frame sequence and atomically replace output."""

    frames = Path(frames).resolve()
    output = Path(output).resolve()
    if fps <= 0:
        raise ValueError("GIF frame rate must be positive")
    if scale <= 0:
        raise ValueError("GIF scale must be positive")
    if shutil.which("ffmpeg") is None:
        raise FileNotFoundError("ffmpeg was not found on PATH")

    count = _frame_count(frames)
    selected = ranges or [(0, count)]
    for start, end in selected:
        if start < 0 or end <= start or end > count:
            raise ValueError(
                f"invalid frame range {start}:{end} for a {count}-frame capture"
            )
    output.parent.mkdir(parents=True, exist_ok=True)
    temporary = output.with_name(f".{output.stem}.tmp.gif")
    temporary.unlink(missing_ok=True)

    trims = [
        f"[0:v]trim=start_frame={start}:end_frame={end},setpts=PTS-STARTPTS[clip{index}]"
        for index, (start, end) in enumerate(selected)
    ]
    clips = "".join(f"[clip{index}]" for index in range(len(selected)))
    filter_graph = ";".join(trims) + ";" + (
        f"{clips}concat=n={len(selected)}:v=1:a=0[sequence];"
        f"[sequence]scale=iw*{scale}:ih*{scale}:flags=neighbor,split[original][palette_input];"
        "[palette_input]palettegen=stats_mode=diff[palette];"
        "[original][palette]paletteuse=dither=bayer:bayer_scale=3:diff_mode=rectangle"
    )
    command = [
        "ffmpeg",
        "-hide_banner",
        "-loglevel",
        "error",
        "-y",
        "-framerate",
        f"{fps:g}",
        "-i",
        str(frames / "frame-%08d.png"),
        "-filter_complex",
        filter_graph,
        "-loop",
        "0",
        str(temporary),
    ]

    try:
        subprocess.run(command, check=True)
        temporary.replace(output)
    except BaseException:
        temporary.unlink(missing_ok=True)
        raise
    return sum(end - start for start, end in selected)


def encode_gif_sources(
    sources: list[tuple[Path, list[FrameRange]]],
    output: Path,
    *,
    fps: float = 30,
    scale: int = 2,
) -> int:
    """Join selected ranges from multiple capture directories into one GIF."""

    output = Path(output).resolve()
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="gif-frames-", dir=output.parent) as temporary:
        assembled = Path(temporary)
        output_index = 0
        for frames, ranges in sources:
            frames = Path(frames).resolve()
            count = _frame_count(frames)
            for start, end in ranges:
                if start < 0 or end <= start or end > count:
                    raise ValueError(
                        f"invalid frame range {start}:{end} for a {count}-frame capture"
                    )
                for source_index in range(start, end):
                    source = frames / f"frame-{source_index:08d}.png"
                    destination = assembled / f"frame-{output_index:08d}.png"
                    try:
                        destination.hardlink_to(source)
                    except OSError:
                        shutil.copy2(source, destination)
                    output_index += 1
        return encode_gif(assembled, output, fps=fps, scale=scale)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("frames", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--fps", type=float, default=30)
    parser.add_argument("--scale", type=int, default=2)
    parser.add_argument(
        "--range",
        dest="ranges",
        action="append",
        type=_parse_range,
        help="include a half-open START:END frame range; may be repeated",
    )
    args = parser.parse_args(argv)

    count = encode_gif(
        args.frames,
        args.output,
        fps=args.fps,
        scale=args.scale,
        ranges=args.ranges,
    )
    print(f"Encoded {count} frames to {args.output.resolve()}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
