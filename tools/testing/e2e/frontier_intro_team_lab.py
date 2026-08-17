"""Verify the guided Frontier introduction's editor route and nested skip."""

from pathlib import Path

from .session import Session
from .team_lab import GAMEPLAY_ROM, TeamLabScenarioFailure, create_team_lab_save, gameplay_symbols


def _run_route(artifact_dir: Path, *, skip: bool) -> None:
    save = artifact_dir / ("skip.sav" if skip else "complete.sav")
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols("gE2EFrontierIntroCheckpoint", "gE2EFrontierIntroFinishCount")
    create_team_lab_save(artifact_dir / ("skip-fixture" if skip else "complete-fixture"), save, tutorial=True)
    with Session(GAMEPLAY_ROM, artifact_dir / ("skip" if skip else "complete"), save=save) as game:
        # Advance Continue without steering the tutorial's own generated input.
        for _ in range(60):
            if game.read(symbols["gE2EFrontierIntroCheckpoint"], width=8) != 0:
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise TeamLabScenarioFailure("Frontier introduction did not start")

        if skip:
            # Wait until the route is nested in the editor, then request its
            # documented double-tap unwind.
            game.wait(symbols["gE2EFrontierIntroCheckpoint"], 4, width=8, max_frames=7200)
            game.press("B", held_frames=1, released_frames=8)
            game.press("B", held_frames=1, released_frames=8)
        game.wait(symbols["gE2EFrontierIntroFinishCount"], 1, width=32, max_frames=18000)
        game.screenshot("passed.png")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    _run_route(artifact_dir, skip=False)
    _run_route(artifact_dir, skip=True)
