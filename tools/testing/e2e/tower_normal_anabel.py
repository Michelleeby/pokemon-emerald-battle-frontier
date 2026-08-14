"""Normal Battle Tower Anabel boundary scenario."""

from pathlib import Path

from .tower_anabel import run_anabel_route


def run(artifact_dir: Path) -> None:
    run_anabel_route(artifact_dir, hard=False)
