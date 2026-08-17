"""Normal Battle Factory Noland boundary scenario."""

from pathlib import Path

from .factory_noland import NORMAL_NOLAND, run_noland_route


def run(artifact_dir: Path) -> None:
    run_noland_route(artifact_dir, NORMAL_NOLAND)
