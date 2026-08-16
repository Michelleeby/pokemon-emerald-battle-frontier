"""Normal Battle Pike Lucy boundary scenario."""

from pathlib import Path

from .pike_lucy import NORMAL_LUCY, run_lucy_route


def run(artifact_dir: Path) -> None:
    run_lucy_route(artifact_dir, NORMAL_LUCY)
