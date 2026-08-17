"""Hard Battle Dome Tucker boundary scenario."""

from pathlib import Path

from .dome_tucker import HARD_TUCKER, run_tucker_route


def run(artifact_dir: Path) -> None:
    run_tucker_route(artifact_dir, HARD_TUCKER)
