"""Exercise accelerated stat input and visible-row-to-data mapping."""

from pathlib import Path

from .session import Session
from .team_lab import GAMEPLAY_ROM, TeamLabScenarioFailure, boot_to_field, create_team_lab_save, gameplay_symbols, open_team_lab_editor, saved_build


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "team-lab-edit-stats.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols()
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)
        game.press("R", held_frames=1, released_frames=60)

        # Holding LEFT must produce more than the initial one-point change.
        game.press("LEFT", held_frames=30, released_frames=30)
        # Rows 3, 4, and 5 are Sp. Atk, Sp. Def, and Speed, while their
        # internal data indices are 4, 5, and 3 respectively.
        for _ in range(3):
            game.press("DOWN", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=15)
        game.press("RIGHT", held_frames=1, released_frames=15)
        game.press("DOWN", held_frames=1, released_frames=15)
        game.press("RIGHT", held_frames=1, released_frames=15)
        game.press("DOWN", held_frames=1, released_frames=15)
        game.press("RIGHT", held_frames=1, released_frames=30)
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 1, width=32, max_frames=600)

        build = saved_build(game, symbols)
        if not build["ivs"][0] < 30:
            raise TeamLabScenarioFailure(f"held stat input did not accelerate: {build}")
        if build["evs"] != [0, 0, 0, 1, 1, 1]:
            raise TeamLabScenarioFailure(f"stat rows mapped to the wrong EV fields: {build}")
        game.screenshot("passed.png")
