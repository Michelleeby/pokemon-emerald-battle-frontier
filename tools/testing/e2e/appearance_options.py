"""Exercise independent appearance preferences through the production Options UI."""

from .audio_options import wait_for_title_input
from .overworld_menu import select_start_action
from .session import E2EError, Session


APPEARANCE_SYMBOLS = (
    "Task_OptionMenuProcessInput", "Task_AppearanceMenuProcessInput",
    "Task_HandleMainMenuInput", "sAppearanceMenu", "CB2_BagMenuRun",
    "gBagMenu", "gBagPosition", "gBagMaleTiles", "gBagFemaleTiles",
    "gBagScreenMale_Pal", "gBagScreenFemale_Pal", "gPlttBufferUnfaded",
)
TASK_SIZE = 40
SPRITE_SIZE = 68
VAR_BASE = 0x139C


class AppearanceOptionsFailure(E2EError):
    """Options did not retain or render the chosen independent appearance."""


def _check(condition: bool, message: str) -> None:
    if not condition:
        raise AppearanceOptionsFailure(message)


def _wait_task(game: Session, symbols: dict[str, int], name: str) -> int:
    for _ in range(120):
        for index in range(16):
            task = symbols["gTasks"] + index * TASK_SIZE
            if game.read(task) == symbols[name] | 1:
                return task
        game.run_frames(5)
    raise AppearanceOptionsFailure(f"Options did not reach {name}")


def saved_choices(game: Session, symbols: dict[str, int]) -> tuple[int, int]:
    sb1 = game.read(symbols["gSaveBlock1Ptr"])
    values = tuple(game.read(sb1 + VAR_BASE + (var - 0x4000) * 2, width=16)
                   for var in (0x40F8, 0x40F9))
    _check(values[0] in (0xB100, 0xB101) and values[1] in (0xB200, 0xB201),
           f"appearance preferences have invalid saved tags: {values}")
    return values[0] - 0xB100, values[1] - 0xB200


def _pending_choices(game: Session, symbols: dict[str, int]) -> tuple[int, int]:
    return (game.read(symbols["sAppearanceMenu"] + 1, width=8),
            game.read(symbols["sAppearanceMenu"] + 2, width=8))


def _open_appearance(game: Session, symbols: dict[str, int]) -> None:
    task = _wait_task(game, symbols, "Task_OptionMenuProcessInput")
    for _ in range(8):
        if game.read(task + 8, width=16) == 5:  # Appearance replaces the old Frame row.
            break
        game.press("DOWN", held_frames=1, released_frames=15)
    else:
        raise AppearanceOptionsFailure("Options never selected Appearance")
    game.press("A", held_frames=1, released_frames=45)
    _wait_task(game, symbols, "Task_AppearanceMenuProcessInput")


def _verify_preview(game: Session, symbols: dict[str, int], bag: int, theme: int, decode) -> None:
    sprite_id = game.read(symbols["sAppearanceMenu"] + 3, width=8)
    _check(sprite_id < 64, "Appearance preview has no valid bag sprite")
    sprite = symbols["gSprites"] + sprite_id * SPRITE_SIZE
    tile_num = game.read(sprite + 4, width=16) & 0x3FF
    sheet = decode(game.read_bytes(
        symbols["gBagMaleTiles" if bag == 0 else "gBagFemaleTiles"], 0x3000), max_size=0x3000)
    _check(game.read_bytes(0x06010000 + tile_num * 32, 0x800) == sheet[0x800:0x1000],
           "Appearance preview does not show the selected bag's pixels")
    palette = decode(game.read_bytes(
        symbols["gBagScreenMale_Pal" if theme == 0 else "gBagScreenFemale_Pal"], 256))
    _check(game.read_bytes(symbols["gPlttBufferUnfaded"] + (13 * 16 + 7) * 2, 2) == palette[14:16],
           "Appearance preview does not show the selected theme palette")


def _edit_pending(game: Session, symbols: dict[str, int], bag: int, theme: int, decode) -> None:
    old = saved_choices(game, symbols)
    _open_appearance(game, symbols)
    bag_before, theme_before = _pending_choices(game, symbols)
    if bag_before != bag:
        game.press("RIGHT", held_frames=1, released_frames=20)
    game.press("DOWN", held_frames=1, released_frames=15)
    if theme_before != theme:
        game.press("RIGHT", held_frames=1, released_frames=20)
    _check(_pending_choices(game, symbols) == (bag, theme),
           "Appearance controls did not independently select the requested bag and colors")
    _check(saved_choices(game, symbols) == old, "Appearance wrote preferences before Options exit")
    _verify_preview(game, symbols, bag, theme, decode)
    game.screenshot(f"appearance-options-{bag}-{theme}.png")

    # Back leaves the submenu without discarding the pending choices. Reopening
    # must retain them, while leaving outer Options applies the draft.
    game.press("B", held_frames=1, released_frames=30)
    _wait_task(game, symbols, "Task_OptionMenuProcessInput")
    _check(saved_choices(game, symbols) == old, "Back from Appearance committed outer Options")
    _open_appearance(game, symbols)
    _check(_pending_choices(game, symbols) == (bag, theme), "reopening Appearance lost pending choices")
    _verify_preview(game, symbols, bag, theme, decode)
    game.press("B", held_frames=1, released_frames=30)
    _wait_task(game, symbols, "Task_OptionMenuProcessInput")


def configure_field_options(game: Session, symbols: dict[str, int], bag: int, theme: int, decode) -> None:
    select_start_action(game, symbols, 6)  # MENU_ACTION_OPTION
    _edit_pending(game, symbols, bag, theme, decode)
    game.press("B", held_frames=1, released_frames=90)  # Existing Options applies on exit.
    game.press("B", held_frames=1, released_frames=60)  # Close the restored start menu.
    game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=600)
    _check(saved_choices(game, symbols) == (bag, theme), "Options exit did not apply appearance choices")


def configure_title_options(game: Session, symbols: dict[str, int], bag: int, theme: int, decode) -> None:
    wait_for_title_input(game, symbols)
    game.press("START", held_frames=1, released_frames=120)
    game.press("DOWN", held_frames=1, released_frames=30)  # Fresh title menu: New Game, Options.
    game.press("A", held_frames=1, released_frames=120)
    _edit_pending(game, symbols, bag, theme, decode)
    game.press("B", held_frames=1, released_frames=120)
    task = _wait_task(game, symbols, "Task_HandleMainMenuInput")
    _check(saved_choices(game, symbols) == (bag, theme), "title Options did not retain appearance choices")
    for _ in range(6):
        if game.read(task + 10, width=16) == 0:  # Main-menu current item data[1].
            return
        game.press("UP", held_frames=1, released_frames=20)
    raise AppearanceOptionsFailure("title menu did not return to New Game")


def verify_bag_rendering(game: Session, symbols: dict[str, int], bag: int, theme: int, decode) -> None:
    select_start_action(game, symbols, 2)  # MENU_ACTION_BAG
    game.wait(symbols["gMain"] + 4, symbols["CB2_BagMenuRun"] | 1, max_frames=600)
    game.run_frames(60)
    menu = game.read(symbols["gBagMenu"])
    _check(0x02000000 <= menu < 0x02040000, "Bag menu was not allocated")
    sprite_id = game.read(menu + 0x804, width=8)
    _check(sprite_id < 64, "Bag menu has no valid bag sprite")
    sprite = symbols["gSprites"] + sprite_id * SPRITE_SIZE
    tile_num = game.read(sprite + 4, width=16) & 0x3FF
    pocket = game.read(symbols["gBagPosition"] + 5, width=8)
    _check(pocket < 5, f"Bag selected invalid pocket {pocket}")
    sheet = decode(game.read_bytes(symbols["gBagMaleTiles" if bag == 0 else "gBagFemaleTiles"], 0x3000),
                   max_size=0x3000)
    start = (pocket + 1) * 0x800
    _check(game.read_bytes(0x06010000 + tile_num * 32, 0x800) == sheet[start:start + 0x800],
           "Bag menu displayed another bag shape")
    expected_palette = decode(game.read_bytes(
        symbols["gBagScreenMale_Pal" if theme == 0 else "gBagScreenFemale_Pal"], 256))
    _check(game.read_bytes(symbols["gPlttBufferUnfaded"], 64) == expected_palette,
           "Bag menu displayed colors from avatar gender instead of the chosen theme")
    game.screenshot(f"bag-options-{bag}-{theme}.png")
    game.press("B", held_frames=1, released_frames=90)
    game.press("B", held_frames=1, released_frames=60)
    game.wait(symbols["sLockFieldControls"], 0, width=8, max_frames=600)
