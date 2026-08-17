"""Exercise Pokédex selection, inherited moves, and ability persistence through Team Lab."""

from pathlib import Path

from .session import Session
from .team_lab import (
    GAMEPLAY_ROM, MOVE_FAKE_OUT, SPECIES_LUDICOLO, TeamLabScenarioFailure,
    create_team_lab_save, gameplay_symbols, open_team_lab_editor, saved_build,
    wait_callback, boot_to_field,
)


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "team-lab-create-mon.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols(
        "CB2_Pokedex", "gE2ETeamLabRolledGender", "gE2ETeamLabCreatedGender"
    )
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)

        # The editor supplies Ludicolo as the initial selection; accepting the
        # current entry exercises only the editor/Pokédex return contract.
        game.press("A", held_frames=1, released_frames=90)
        wait_callback(game, symbols, "CB2_Pokedex")
        game.run_frames(180)
        game.press("A", held_frames=1, released_frames=120)
        wait_callback(game, symbols, "CB2_TeamLabScreen")
        game.run_frames(90)

        # Select Ludicolo's second ability and persist the build. Fake Out was
        # learned by Lombre, so validation crosses the pre-evolution path.
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
        game.press("DOWN", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=60)
        game.press("START", held_frames=1, released_frames=120)
        wait_for = symbols["gE2ETeamLabSaveCount"]
        game.wait(wait_for, 1, width=32, max_frames=600)

        build = saved_build(game, symbols)
        if build["species"] != SPECIES_LUDICOLO:
            raise TeamLabScenarioFailure(f"selected species was not persisted: {build}")
        if build["moves"][0] != MOVE_FAKE_OUT:
            raise TeamLabScenarioFailure(f"pre-evolution move was rejected: {build}")
        if build["ability"] != 1:
            raise TeamLabScenarioFailure(f"second ability slot was not persisted: {build}")
        rolled_gender = game.read(symbols["gE2ETeamLabRolledGender"], width=8)
        created_gender = game.read(symbols["gE2ETeamLabCreatedGender"], width=8)
        if created_gender != rolled_gender:
            raise TeamLabScenarioFailure(
                f"created gender {created_gender} did not match rolled gender {rolled_gender}"
            )
        game.screenshot("passed.png")
