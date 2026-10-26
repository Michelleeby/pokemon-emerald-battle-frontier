"""Exercise all eight new-game avatar previews, input wrapping, and saving."""

from pathlib import Path

from .audio_options import wait_for_title_input
from .overworld_menu import save_normally
from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .team_lab import boot_to_field
from .tower import GAMEPLAY_ELF, GAMEPLAY_ROM


AVATAR_FRONT_PICS = (71, 72, 89, 90, 91, 92, 70, 81)
AVATAR_NAMES = ("brendan", "may", "red", "leaf", "rs-brendan", "rs-may", "wally", "steven")
TASK_SIZE = 40
SPRITE_SIZE = 68
AVATAR_VAR_OFFSET = 0x139C + (0x40F7 - 0x4000) * 2


class AvatarSelectionFailure(E2EError):
    """The real new-game path did not retain the selected avatar."""


def _check(condition: bool, message: str) -> None:
    if not condition:
        raise AvatarSelectionFailure(message)


def _find_task(game: Session, symbols: dict[str, int], name: str) -> int | None:
    for index in range(16):
        address = symbols["gTasks"] + index * TASK_SIZE
        if game.read(address) == symbols[name] | 1:
            return address
    return None


def _preview(game: Session, symbols: dict[str, int], task: int, expected: int) -> None:
    actual = game.read(task + 8 + 6 * 2, width=16)
    _check(actual == expected, f"avatar input selected {actual}, expected {expected}")
    sprite_id = game.read(task + 8 + 2 * 2, width=16)
    _check(sprite_id < 64, f"avatar preview has invalid sprite {sprite_id}")
    sprite = symbols["gSprites"] + sprite_id * SPRITE_SIZE
    _check(game.read(sprite + 0x3E, width=8) & 5 == 1, "avatar preview is hidden or unused")
    # The managed picture API owns decompressed frames. Compare the actual
    # frame data with the trainer table's ROM data, rather than only task state.
    compressed = game.read(symbols["gTrainerFrontPicTable"] + AVATAR_FRONT_PICS[expected] * 8)
    expected_pixels = _decompress_picture(game.read_bytes(compressed, 4096))
    frames = game.read(sprite + 0x0C)
    actual_pixels = game.read_bytes(game.read(frames), len(expected_pixels))
    _check(actual_pixels == expected_pixels, f"avatar {expected} preview shows another trainer")


def _decompress_picture(data: bytes) -> bytes:
    """Decode a bounded GBA LZ77 trainer picture for a pixel comparison."""
    _check(data[0] == 0x10, "trainer picture is not a GBA LZ77 asset")
    size = int.from_bytes(data[1:4], "little")
    _check(0 < size <= 4096, f"invalid trainer picture size {size}")
    result = bytearray()
    cursor = 4
    while len(result) < size:
        flags = data[cursor]
        cursor += 1
        for bit in range(7, -1, -1):
            if len(result) == size:
                break
            if flags & (1 << bit):
                block = int.from_bytes(data[cursor:cursor + 2], "big")
                cursor += 2
                count = (block >> 12) + 3
                distance = (block & 0xFFF) + 1
                for _ in range(count):
                    result.append(result[-distance])
            else:
                result.append(data[cursor])
                cursor += 1
    return bytes(result[:size])


def _cycle(game: Session, symbols: dict[str, int], task: int, key: str, expected: int) -> None:
    game.press(key, held_frames=1, released_frames=4)
    game.wait(task, symbols["Task_NewGameBirchSpeech_ChooseGender"] | 1, max_frames=240)
    _preview(game, symbols, task, expected)


def _reach_field(game: Session, symbols: dict[str, int]) -> None:
    # Select the starter and advance the ferry opening through production input.
    # The guided Lab introduction is optional, so use its documented double-B skip.
    for _ in range(900):
        if (game.read(symbols["gE2EFrontierIntroCheckpoint"], width=8) != 0
                and game.read(symbols["gE2EFrontierIntroFinishCount"]) == 0):
            game.press("B", held_frames=1, released_frames=8)
            game.press("B", held_frames=1, released_frames=8)
        if (game.read(symbols["gMain"] + 4) == symbols["CB2_Overworld"] | 1
                and game.read(symbols["sLockFieldControls"], width=8) == 0
                and game.read(symbols["gE2EFrontierIntroFinishCount"]) != 0):
            return
        game.press("A", held_frames=1, released_frames=29)
    raise AvatarSelectionFailure("new game did not finish the Frontier arrival")


def _saved_avatar(game: Session, symbols: dict[str, int]) -> int:
    return game.read(game.read(symbols["gSaveBlock1Ptr"]) + AVATAR_VAR_OFFSET, width=16)


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "avatar-selection.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gTasks", "gSprites", "gMain", "Task_TitleScreenPhase3",
        "Task_NewGameBirchSpeech_ChooseGender", "CB2_NamingScreen",
        "gNewGameAvatarStyle", "gSaveBlock1Ptr", "gSaveBlock2Ptr",
        "gTrainerFrontPicTable",
        "gE2EFrontierIntroCheckpoint", "gE2EFrontierIntroFinishCount",
        "CB2_Overworld", "sLockFieldControls", "sNumStartMenuActions",
        "sStartMenuCursorPos", "sCurrentStartMenuActions", "sSaveDialogCallback",
        "SaveOverwriteInputCallback", "gSaveCounter", "gPlayerAvatar", "gObjectEvents",
    )
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        wait_for_title_input(game, symbols)
        game.press("START", held_frames=1, released_frames=120)
        # A fresh cartridge's main menu defaults to NEW GAME.
        game.press("A", held_frames=1, released_frames=60)
        task = None
        for _ in range(160):
            task = _find_task(game, symbols, "Task_NewGameBirchSpeech_ChooseGender")
            if task is not None:
                break
            game.press("A", held_frames=1, released_frames=14)
        _check(task is not None, "new game never reached the avatar chooser")
        _preview(game, symbols, task, 1)

        # Complete a cycle in each direction, including both wrap boundaries.
        for step in range(1, 9):
            avatar = (1 + step) % 8
            _cycle(game, symbols, task, "DOWN", avatar)
            game.screenshot(f"avatar-{AVATAR_NAMES[avatar]}.png")
        for step in range(1, 9):
            _cycle(game, symbols, task, "UP", (1 - step) % 8)
        _cycle(game, symbols, task, "UP", 0)
        _cycle(game, symbols, task, "UP", 7)

        game.press("A", held_frames=1, released_frames=60)
        game.wait(symbols["gMain"] + 4, symbols["CB2_NamingScreen"] | 1, max_frames=600)
        _check(game.read(symbols["gNewGameAvatarStyle"], width=8) == 7,
               "confirming Steven lost the selected avatar")
        game.run_frames(120)
        game.screenshot("steven-naming.png")
        game.press("A", held_frames=1, released_frames=15)  # Enter a name.
        game.press("START", held_frames=1, released_frames=30)
        game.press("A", held_frames=1, released_frames=120)
        _reach_field(game, symbols)
        _check(_saved_avatar(game, symbols) == 7, "new-game initialization reset the avatar")
        # Story gender is deliberately fixed to FEMALE by this project's
        # existing new-game flow; visual avatar gender remains independent.
        _check(game.read(game.read(symbols["gSaveBlock2Ptr"]) + 8, width=8) == 1,
               "new game changed the existing story gender")
        _check(game.read(symbols["gPlayerAvatar"] + 7, width=8) == 0,
               "Steven did not retain a binary male visual gender")
        object_id = game.read(symbols["gPlayerAvatar"] + 5, width=8)
        _check(object_id < 16, "new game has no valid player object")
        _check(game.read(symbols["gObjectEvents"] + object_id * 36 + 5, width=8) == 134,
               "new game's overworld appearance is not Steven")
        game.screenshot("steven-field.png")
        save_normally(game, symbols)
        game.restart()
        boot_to_field(game, symbols)
        _check(_saved_avatar(game, symbols) == 7, "avatar did not survive normal save/restart/load")
        game.screenshot("passed.png")
