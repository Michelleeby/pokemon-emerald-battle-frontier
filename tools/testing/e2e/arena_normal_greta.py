"""Normal Battle Arena Greta boundary scenario."""

from pathlib import Path

from .arena_greta import NORMAL_GRETA, run_greta_route


def run(artifact_dir: Path) -> None:
    run_greta_route(artifact_dir, NORMAL_GRETA)
