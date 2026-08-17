"""Verify the two PokéNav entries added beside the Team Lab editor."""

from pathlib import Path

from .session import Session
from .team_lab import GAMEPLAY_ROM, TeamLabScenarioFailure, boot_to_field, create_team_lab_save, gameplay_symbols, open_condition_menu, wait_callback


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "pokenav-team-lab-access.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols(
        "gE2EPokenavTeamLabOpenCount",
        "gE2EPokenavPcOpenCount",
        "gE2EPokenavPcReturnCount",
    )
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_condition_menu(game, symbols)
        game.press("A", held_frames=1, released_frames=120)
        game.wait(symbols["gE2EPokenavTeamLabOpenCount"], 1, width=32, max_frames=600)
        wait_callback(game, symbols, "CB2_UpdatePartyMenu")
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)

        open_condition_menu(game, symbols)
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=120)
        game.wait(symbols["gE2EPokenavPcOpenCount"], 1, width=32, max_frames=900)
        # Close the PC menu immediately; storage behavior itself is vanilla.
        game.press("B", held_frames=1, released_frames=120)
        game.wait(symbols["gE2EPokenavPcReturnCount"], 1, width=32, max_frames=900)
        game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=900)
        game.screenshot("passed.png")
