"""Exercise IV/EV feedback, accelerated input, and visible-row mapping."""

from pathlib import Path

from .session import Session
from .team_lab import GAMEPLAY_ROM, TeamLabScenarioFailure, boot_to_field, create_team_lab_save, gameplay_symbols, open_team_lab_editor, saved_build


def _assert_column_preview(game: Session, symbols: dict[str, int], expected: int) -> None:
    # Inspect the rendered HP digits, rather than the editor's column flag.
    # Content is window 1; each public Window has an 8-byte template and pointer.
    window = symbols["gWindows"] + 12
    width = game.read(window + 3, width=8)
    tiles = game.read(window + 8)
    words: dict[int, int] = {}
    red_columns = []
    for left, right in ((46, 66), (66, 86)):
        red = False
        for y in range(31, 44):
            for x in range(left, right):
                offset = ((y // 8) * width + x // 8) * 32 + (y % 8) * 4 + (x % 8) // 2
                aligned = offset & ~3
                if aligned not in words:
                    words[aligned] = game.read(tiles + aligned)
                color = (words[aligned] >> ((offset % 4) * 8 + (x % 2) * 4)) & 15
                red |= color == 4  # TEXT_COLOR_RED in the window's palette.
        red_columns.append(red)
    if red_columns != [expected == 0, expected == 1]:
        raise TeamLabScenarioFailure(
            f"A preview highlighted IV/EV={red_columns}, expected column {expected}"
        )


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "team-lab-edit-stats.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols("gWindows")
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        open_team_lab_editor(game, symbols)
        game.press("R", held_frames=1, released_frames=60)

        # A previews its destination while held, then commits on release.
        game.set_keys("A")
        game.run_frames(8)
        _assert_column_preview(game, symbols, 1)
        game.screenshot("ev-column-preview.png")
        game.set_keys()
        game.run_frames(15)
        game.press("RIGHT", held_frames=1, released_frames=15)
        game.set_keys("A")
        game.run_frames(8)
        _assert_column_preview(game, symbols, 0)
        game.screenshot("iv-column-preview.png")
        game.set_keys()
        game.run_frames(15)

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
        if build["evs"] != [1, 0, 0, 1, 1, 1]:
            raise TeamLabScenarioFailure(f"stat rows mapped to the wrong EV fields: {build}")
        game.screenshot("passed.png")
