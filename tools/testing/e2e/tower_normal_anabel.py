"""Normal Battle Tower Anabel boundary scenario."""

from pathlib import Path

from .tower_anabel import NORMAL_ANABEL, run_anabel_route


def run(artifact_dir: Path) -> None:
    run_anabel_route(artifact_dir, NORMAL_ANABEL)
