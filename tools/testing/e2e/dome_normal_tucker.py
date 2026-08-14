"""Normal Battle Dome Tucker boundary scenario."""

from pathlib import Path

from .dome_tucker import NORMAL_TUCKER, run_tucker_route


def run(artifact_dir: Path) -> None:
    run_tucker_route(artifact_dir, NORMAL_TUCKER)
