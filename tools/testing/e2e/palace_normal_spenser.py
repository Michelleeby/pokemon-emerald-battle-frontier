"""Normal Battle Palace Spenser boundary scenario."""

from pathlib import Path

from .palace_spenser import NORMAL_SPENSER, run_spenser_route


def run(artifact_dir: Path) -> None:
    run_spenser_route(artifact_dir, NORMAL_SPENSER)
