"""Hard Battle Tower Anabel boundary scenario."""

from pathlib import Path

from .tower_anabel import HARD_ANABEL, run_anabel_route


def run(artifact_dir: Path) -> None:
    run_anabel_route(artifact_dir, HARD_ANABEL)
