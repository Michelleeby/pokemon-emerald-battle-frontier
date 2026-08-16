"""Normal-mode Battle Pyramid Brandon boundary scenario."""

from pathlib import Path

from .pyramid_brandon import NORMAL_BRANDON, run_brandon_route


def run(artifact_dir: Path) -> None:
    run_brandon_route(artifact_dir, NORMAL_BRANDON)
