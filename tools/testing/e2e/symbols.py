"""Resolve runtime addresses from local ELF symbol tables."""

from __future__ import annotations

from pathlib import Path
import subprocess


class SymbolError(RuntimeError):
    """An ELF symbol could not be resolved unambiguously."""


def load_symbols(elf: Path, *, objdump: str = "arm-none-eabi-objdump") -> dict[str, int]:
    result = subprocess.run(
        [objdump, "-t", str(Path(elf).resolve())],
        check=True,
        capture_output=True,
        text=True,
    )
    symbols: dict[str, int] = {}
    duplicates: set[str] = set()
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) < 5:
            continue
        try:
            address = int(fields[0], 16)
        except ValueError:
            continue
        name = fields[-1]
        if name in symbols and symbols[name] != address:
            duplicates.add(name)
        else:
            symbols[name] = address
    for name in duplicates:
        symbols.pop(name, None)
    return symbols


def require_symbols(symbols: dict[str, int], *names: str) -> dict[str, int]:
    missing = [name for name in names if name not in symbols]
    if missing:
        raise SymbolError("missing ELF symbols: " + ", ".join(missing))
    return {name: symbols[name] for name in names}
