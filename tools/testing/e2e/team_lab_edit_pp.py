"""Exercise PP Up controls, Max All, save, and discard in Team Lab."""

from pathlib import Path

from .overworld_menu import save_normally
from .session import Session
from .team_lab import (
    GAMEPLAY_ROM,
    TeamLabScenarioFailure,
    boot_to_field,
    create_team_lab_save,
    gameplay_symbols,
    open_team_lab_editor,
    saved_build,
)


def _open_moves_page(game: Session) -> None:
    game.press("R", held_frames=1, released_frames=30)
    game.press("R", held_frames=1, released_frames=30)


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "team-lab-edit-pp.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols(
        "gSaveBlock2Ptr", "sNumStartMenuActions", "sStartMenuCursorPos",
        "sCurrentStartMenuActions", "sSaveDialogCallback",
        "SaveOverwriteInputCallback", "gSaveCounter",
    )
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)

        # Fake Out has base PP 10. A single RIGHT raises it to 12 while
        # Max All raises it to 16 and saves the packed slot count as three.
        game.press("RIGHT", held_frames=1, released_frames=30)
        game.screenshot("pp-per-move.png")
        game.press("SELECT", held_frames=1, released_frames=30)
        game.press("A", held_frames=1, released_frames=30)
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 1, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["pp_bonuses"] & 0x3 != 0x3 or build["pp"][0] != 16:
            raise TeamLabScenarioFailure(f"Max All did not preserve/save PP state: {build}")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        # Verify that the existing PP byte and packed bonuses survive the
        # ordinary production save path and a full restart/load.
        save_normally(game, symbols)
        game.restart()
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.screenshot("pp-reloaded.png")
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 1, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["pp_bonuses"] & 0x3 != 0x3 or build["pp"][0] != 16:
            raise TeamLabScenarioFailure(f"normal save/load lost PP state: {build}")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        # A second edit is discarded. Reopening and saving should recover the
        # prior three upgrades rather than the unsaved fourth state.
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("LEFT", held_frames=1, released_frames=30)
        game.press("B", held_frames=1, released_frames=120)
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 2, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["pp_bonuses"] & 0x3 != 0x3 or build["pp"][0] != 16:
            raise TeamLabScenarioFailure(f"discard changed the saved PP state: {build}")
        game.screenshot("passed.png")
