#!/usr/bin/env python3
"""Reject production-linked test ROMs with unsafe GBA RAM headroom."""

from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path


EWRAM_END = 0x02040000
IWRAM_STACK_BASE = 0x03007E00
SECTION_PATTERN = re.compile(
    r"^\s*\d+\s+(?P<name>ewram|iwram)\s+"
    r"(?P<size>[0-9a-fA-F]+)\s+(?P<vma>[0-9a-fA-F]+)\s+"
)


def read_sections(elf: Path, objdump: str) -> dict[str, tuple[int, int]]:
    completed = subprocess.run(
        [objdump, "-h", str(elf)],
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    if completed.returncode:
        raise RuntimeError(completed.stderr.strip() or f"{objdump} failed")

    sections: dict[str, tuple[int, int]] = {}
    for line in completed.stdout.splitlines():
        match = SECTION_PATTERN.match(line)
        if match:
            sections[match.group("name")] = (
                int(match.group("vma"), 16),
                int(match.group("size"), 16),
            )
    missing = {"ewram", "iwram"} - sections.keys()
    if missing:
        raise RuntimeError("missing ELF section(s): " + ", ".join(sorted(missing)))
    return sections


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--elf", required=True, type=Path)
    parser.add_argument("--objdump", default="arm-none-eabi-objdump")
    parser.add_argument("--min-ewram-free", required=True, type=int)
    parser.add_argument("--min-iwram-stack", required=True, type=int)
    args = parser.parse_args()

    try:
        sections = read_sections(args.elf, args.objdump)
    except RuntimeError as error:
        parser.error(str(error))

    ewram_start, ewram_size = sections["ewram"]
    iwram_start, iwram_size = sections["iwram"]
    ewram_free = EWRAM_END - (ewram_start + ewram_size)
    iwram_stack = IWRAM_STACK_BASE - (iwram_start + iwram_size)

    print(
        f"Test memory headroom: EWRAM {ewram_free} bytes free "
        f"(minimum {args.min_ewram_free}); IWRAM stack {iwram_stack} bytes "
        f"(minimum {args.min_iwram_stack})"
    )

    failures = []
    if ewram_free < args.min_ewram_free:
        failures.append(
            f"EWRAM headroom is {ewram_free} bytes; require at least "
            f"{args.min_ewram_free}"
        )
    if iwram_stack < args.min_iwram_stack:
        failures.append(
            f"IWRAM stack headroom is {iwram_stack} bytes; require at least "
            f"{args.min_iwram_stack} below 0x{IWRAM_STACK_BASE:08X}"
        )
    if failures:
        for failure in failures:
            print(f"error: {failure}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
