"""Hard Battle Pike Lucy boundary scenario."""

from pathlib import Path

from .pike_lucy import HARD_LUCY, run_lucy_route


def run(artifact_dir: Path) -> None:
    run_lucy_route(artifact_dir, HARD_LUCY)
