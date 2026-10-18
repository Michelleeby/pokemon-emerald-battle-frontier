"""Exercise PP Up controls, Max All, save, and discard in Team Lab."""

from pathlib import Path

from .overworld_menu import save_normally
from .session import Session
from .team_lab import (
    GAMEPLAY_ROM,
    MOVE_SWALLOW,
    TeamLabScenarioFailure,
    boot_to_field,
    create_team_lab_save,
    gameplay_symbols,
    open_team_lab_editor,
    saved_build,
    wait_callback,
)


def _open_moves_page(game: Session) -> None:
    game.press("R", held_frames=1, released_frames=30)
    game.press("R", held_frames=1, released_frames=30)


def _search_none(game: Session, symbols: dict[str, int]) -> None:
    game.press("A", held_frames=1, released_frames=90)
    wait_callback(game, symbols, "CB2_NamingScreen")
    game.run_frames(60)
    # The uppercase keyboard starts at A. Enter N, O, N, E, then confirm OK.
    for key in (
        "DOWN", "DOWN", "RIGHT", "A", "RIGHT", "A", "LEFT", "A",
        "UP", "UP", "RIGHT", "RIGHT", "RIGHT", "A", "START",
    ):
        game.press(key, held_frames=1, released_frames=15)
    game.press("A", held_frames=1, released_frames=120)
    wait_callback(game, symbols, "CB2_TeamLabScreen")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "team-lab-edit-pp.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols(
        "gSaveBlock2Ptr", "sNumStartMenuActions", "sStartMenuCursorPos",
        "sCurrentStartMenuActions", "sSaveDialogCallback",
        "SaveOverwriteInputCallback", "gSaveCounter", "CB2_NamingScreen",
    )
    create_team_lab_save(artifact_dir, save, high_move=True)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)

        # Swallow is move ID 256, so this path exercises the UI's full-width
        # move value. It has base PP 10. A single RIGHT raises it to 12 and
        # saves one PP Up before the Max All path can mask a narrowing bug.
        game.press("RIGHT", held_frames=1, released_frames=30)
        game.screenshot("pp-per-move.png")
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 1, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["moves"][0] != MOVE_SWALLOW or build["pp_bonuses"] & 0x3 != 0x1 or build["pp"][0] != 12:
            raise TeamLabScenarioFailure(f"single PP Up did not preserve high move ID/state: {build}")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        # Select applies Max PP immediately and stays on the Moves page.
        # Repeating it must not require confirmation or refill spent PP.
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("SELECT", held_frames=1, released_frames=30)
        game.press("SELECT", held_frames=1, released_frames=30)
        game.screenshot("pp-select-max.png")
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 2, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["moves"][0] != MOVE_SWALLOW or build["pp_bonuses"] & 0x3 != 0x3 or build["pp"][0] != 16:
            raise TeamLabScenarioFailure(f"Max All did not preserve/save PP state: {build}")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        # Decreasing once from max must preserve the two spent PP points.
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("LEFT", held_frames=1, released_frames=30)
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 3, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["moves"][0] != MOVE_SWALLOW or build["pp_bonuses"] & 0x3 != 0x2 or build["pp"][0] != 14:
            raise TeamLabScenarioFailure(f"PP Up decrement did not preserve spent PP: {build}")
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
        if build["moves"][0] != MOVE_SWALLOW or build["pp_bonuses"] & 0x3 != 0x2 or build["pp"][0] != 14:
            raise TeamLabScenarioFailure(f"normal save/load lost PP state: {build}")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        # A second edit is discarded. Reopening and saving should recover the
        # prior two upgrades rather than the unsaved third state.
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("RIGHT", held_frames=1, released_frames=30)
        game.press("B", held_frames=1, released_frames=120)
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 2, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if build["moves"][0] != MOVE_SWALLOW or build["pp_bonuses"] & 0x3 != 0x2 or build["pp"][0] != 14:
            raise TeamLabScenarioFailure(f"discard changed the saved PP state: {build}")

        # NONE is an ordinary search choice. It clears just the focused move,
        # its remaining PP and packed upgrades, preserving all other slots.
        before_clear = build
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)
        open_team_lab_editor(game, symbols)
        _open_moves_page(game)
        _search_none(game, symbols)
        game.screenshot("move-none.png")
        game.press("START", held_frames=1, released_frames=120)
        game.wait(symbols["gE2ETeamLabSaveCount"], 3, width=32, max_frames=600)
        build = saved_build(game, symbols)
        if (
            build["moves"] != [0, *before_clear["moves"][1:]]
            or build["pp"] != [0, *before_clear["pp"][1:]]
            or build["pp_bonuses"] != before_clear["pp_bonuses"] & ~0x3
        ):
            raise TeamLabScenarioFailure(f"NONE search did not clear only the focused slot: {build}")
        game.screenshot("passed.png")
