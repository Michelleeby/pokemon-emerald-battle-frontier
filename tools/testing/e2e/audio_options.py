"""Exercise nested Audio options, explicit preview, and normal save/reload."""

from pathlib import Path

from .session import Session
from .overworld_menu import save_normally, select_start_action
from .team_lab import (
    GAMEPLAY_ROM,
    TeamLabScenarioFailure,
    boot_to_field,
    create_team_lab_save,
    gameplay_symbols,
)


def _check(condition: bool, message: str) -> None:
    if not condition:
        raise TeamLabScenarioFailure(message)


def close_options(game: Session, symbols: dict[str, int]) -> None:
    game.press("B", held_frames=1, released_frames=30)  # Audio -> Options
    game.press("B", held_frames=1, released_frames=90)  # Commit Options
    game.press("B", held_frames=1, released_frames=60)  # Close start menu
    game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=600)


def audio_record(game: Session, symbols: dict[str, int]) -> tuple[int, ...]:
    base = game.read(symbols["gSaveBlock2Ptr"]) + 0x90
    return (
        game.read(base, width=16),
        *(game.read(base + i, width=8) for i in range(2, 8)),
    )


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "audio-options.sav"
    save.unlink(missing_ok=True)
    symbols = gameplay_symbols(
        "gSaveBlock2Ptr", "gMPlayInfo_BGM", "gSongTable", "sCurrentMapMusic",
        "sNumStartMenuActions", "sStartMenuCursorPos", "sCurrentStartMenuActions",
        "sSaveDialogCallback", "SaveOverwriteInputCallback", "gSaveCounter",
    )
    create_team_lab_save(artifact_dir, save)
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        boot_to_field(game, symbols)
        select_start_action(game, symbols, 6)  # MENU_ACTION_OPTION
        original_song = game.read(symbols["gMPlayInfo_BGM"])
        original_map_music = game.read(symbols["sCurrentMapMusic"], width=16)
        game.press("RIGHT", held_frames=1, released_frames=15)  # Fast -> Slow
        for _ in range(3):
            game.press("DOWN", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=30)  # Audio
        game.screenshot("audio-defaults.png")
        record = audio_record(game, symbols)
        _check(record[:5] == (0x4155, 1, 0, 0, 1), f"wrong Audio defaults: {record}")

        game.press("RIGHT", held_frames=1, released_frames=15)  # Stereo
        game.press("DOWN", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=30)  # Picker
        for _ in range(2):
            game.press("DOWN", held_frames=1, released_frames=15)
        _check(game.read(symbols["gMPlayInfo_BGM"]) == original_song,
               "highlighting a track played it without preview input")
        game.press("SELECT", held_frames=1, released_frames=30)
        first_track = game.read(symbols["gSongTable"] + 476 * 8)
        _check(game.read(symbols["gMPlayInfo_BGM"]) == first_track,
               "Select did not preview Hoenn Trainer")
        _check(game.read(symbols["sCurrentMapMusic"], width=16) == original_map_music,
               "preview changed map music bookkeeping")
        game.screenshot("music-preview.png")
        game.press("B", held_frames=1, released_frames=30)
        _check(game.read(symbols["gMPlayInfo_BGM"]) == original_song,
               "canceling the picker did not restore prior music")

        # Confirm the last Kanto track, exercising scrolling, then exclude it.
        game.press("A", held_frames=1, released_frames=30)
        for _ in range(12):
            game.press("DOWN", held_frames=1, released_frames=10)
        game.press("SELECT", held_frames=1, released_frames=30)
        kanto_champion = game.read(symbols["gSongTable"] + 511 * 8)
        _check(game.read(symbols["gMPlayInfo_BGM"]) == kanto_champion,
               "scrolling did not reach Kanto Champion")
        game.screenshot("music-scrolling.png")
        game.press("A", held_frames=1, released_frames=30)
        _check(game.read(symbols["gMPlayInfo_BGM"]) == original_song,
               "confirming a previewed track did not restore prior music")
        game.press("DOWN", held_frames=1, released_frames=15)  # Pool
        game.press("LEFT", held_frames=1, released_frames=15)  # Hoenn
        game.screenshot("excluded-track-reset.png")

        # The picker must now start on Default, not the excluded fixed track.
        game.press("UP", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=30)
        game.press("DOWN", held_frames=1, released_frames=15)  # Random
        game.press("A", held_frames=1, released_frames=30)
        game.press("DOWN", held_frames=1, released_frames=15)  # Pool
        game.press("RIGHT", held_frames=1, released_frames=15)
        game.press("RIGHT", held_frames=1, released_frames=15)  # All Battles
        game.screenshot("audio-edited.png")
        close_options(game, symbols)
        expected = audio_record(game, symbols)
        _check(expected[:5] == (0x4155, 1, 0, 2, 2),
               f"Audio choices/pool reset were not committed: {expected}")
        option_bits = game.read(game.read(symbols["gSaveBlock2Ptr"]) + 0x14, width=16)
        _check(option_bits & 7 == 0, "Audio submenu discarded the Text Speed draft")
        _check(option_bits & (1 << 8) != 0, "Audio Sound did not commit Stereo")
        save_normally(game, symbols)
        game.restart()
        boot_to_field(game, symbols)
        _check(audio_record(game, symbols) == expected,
               "Audio preferences did not survive normal save/restart/load")
        reloaded = game.read(game.read(symbols["gSaveBlock2Ptr"]) + 0x14, width=16)
        _check(reloaded == option_bits, "other Options changed during save/reload")
        game.screenshot("passed.png")
