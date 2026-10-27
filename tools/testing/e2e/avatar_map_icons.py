"""Verify the selected avatar's actual Frontier-map tile and palette data."""

from .overworld_menu import select_start_action
from .session import E2EError, Session


# Match the authored south-facing overworld frame and its own palette, rather
# than accepting a binary gender marker or only checking the saved avatar ID.
AVATAR_GRAPHICS = (
    "gObjectEventGraphicsInfo_BrendanNormal",
    "gObjectEventGraphicsInfo_MayNormal",
    "gObjectEventGraphicsInfo_Red",
    "gObjectEventGraphicsInfo_Leaf",
    "gObjectEventGraphicsInfo_RubySapphireBrendan",
    "gObjectEventGraphicsInfo_RubySapphireMay",
    "gObjectEventGraphicsInfo_Wally",
    "gObjectEventGraphicsInfo_Steven",
)
AVATAR_PALETTES = (
    "gObjectEventPal_Brendan",
    "gObjectEventPal_May",
    "gObjectEventPal_RedLeaf",
    "gObjectEventPal_RedLeaf",
    "gObjectEventPal_RubySapphireBrendan",
    "gObjectEventPal_RubySapphireMay",
    "gObjectEventPal_Npc1",
    "gObjectEventPal_Npc4",
)
MAP_ICON_SYMBOLS = tuple(dict.fromkeys((
    "sMapData", "sRegionMap", "CB2_Pokenav", "Task_HandleFrontierPassInput", "Task_HandleFrontierMap",
    "gPlttBufferUnfaded", *AVATAR_GRAPHICS, *AVATAR_PALETTES,
)))
TASK_SIZE = 40
SPRITE_SIZE = 68


class AvatarMapIconFailure(E2EError):
    """The displayed map marker does not represent the selected avatar."""


def _check(condition: bool, message: str) -> None:
    if not condition:
        raise AvatarMapIconFailure(message)


def _wait_task(game: Session, symbols: dict[str, int], name: str) -> int:
    for _ in range(120):
        for index in range(16):
            task = symbols["gTasks"] + index * TASK_SIZE
            if game.read(task) == symbols[name] | 1:
                return task
        game.run_frames(5)
    raise AvatarMapIconFailure(f"map did not reach {name}")


def _verify_marker_data(game: Session, symbols: dict[str, int], avatar_id: int,
                        sprite: int, label: str) -> None:
    delta = sprite - symbols["gSprites"]
    _check(0 <= delta < 64 * SPRITE_SIZE and delta % SPRITE_SIZE == 0,
           f"{label} has no valid player head sprite")
    _check(game.read(sprite + 0x3E, width=8) & 1 == 1,
           f"{label} player head is inactive")

    frame_table = game.read(symbols[AVATAR_GRAPHICS[avatar_id]] + 0x1C)
    expected_tiles = game.read_bytes(game.read(frame_table), 0x80)
    attr2 = game.read(sprite + 4, width=16)
    tile_num = attr2 & 0x3FF
    actual_tiles = game.read_bytes(0x06010000 + tile_num * 32, 0x80)
    _check(actual_tiles == expected_tiles,
           f"{label} uses another avatar's head tiles for avatar {avatar_id}")

    palette_slot = (attr2 >> 12) & 0xF
    expected_palette = game.read_bytes(symbols[AVATAR_PALETTES[avatar_id]], 32)
    actual_palette = game.read_bytes(symbols["gPlttBufferUnfaded"] + 0x200 + palette_slot * 32, 32)
    _check(actual_palette == expected_palette,
           f"{label} uses another avatar's palette for avatar {avatar_id}")


def verify_frontier_map_icon(game: Session, symbols: dict[str, int], avatar_id: int) -> None:
    """Open the actual map, inspect its rendered head, and return to the field."""
    _check(0 <= avatar_id < len(AVATAR_GRAPHICS), f"invalid test avatar {avatar_id}")
    select_start_action(game, symbols, 4)  # MENU_ACTION_PLAYER opens Frontier Pass.
    _wait_task(game, symbols, "Task_HandleFrontierPassInput")
    game.press("A", held_frames=1, released_frames=90)  # Frontier map is highlighted.
    task = _wait_task(game, symbols, "Task_HandleFrontierMap")
    game.wait(task + 8, 1, width=16, max_frames=600)

    map_data = game.read(symbols["sMapData"])
    _check(0x02000000 <= map_data < 0x02040000, "Frontier map data was not allocated")
    sprite = game.read(map_data + 8)
    _verify_marker_data(game, symbols, avatar_id, sprite, "Frontier map")
    game.screenshot(f"map-avatar-{avatar_id}.png")

    game.press("B", held_frames=1, released_frames=120)
    _wait_task(game, symbols, "Task_HandleFrontierPassInput")
    game.press("B", held_frames=1, released_frames=120)
    _return_to_field(game, symbols)


def _return_to_field(game: Session, symbols: dict[str, int]) -> None:
    # Both maps return through the normal start-menu callback. Close that menu
    # through input so the caller resumes from the same unlocked field state.
    for _ in range(30):
        if (game.read(symbols["gMain"] + 4) == symbols["CB2_Overworld"] | 1
                and game.read(symbols["sLockFieldControls"], width=8) == 0):
            return
        game.press("B", held_frames=1, released_frames=15)
    raise AvatarMapIconFailure("map verification did not return to the unlocked field")


def verify_region_map_icon(game: Session, symbols: dict[str, int], avatar_id: int) -> None:
    """Open the actual PokéNav region map and check the selected head art."""
    game.press("L", held_frames=1, released_frames=120)
    game.wait(symbols["gMain"] + 4, symbols["CB2_Pokenav"] | 1, max_frames=600)
    game.run_frames(120)
    game.press("A", held_frames=1, released_frames=180)  # Map is the first entry.
    region_map = game.read(symbols["sRegionMap"])
    _check(0x02000000 <= region_map < 0x02040000, "region map data was not allocated")
    sprite = game.read(region_map + 0x20)
    _verify_marker_data(game, symbols, avatar_id, sprite, "region map")
    # The ordinary region-map marker can blink. Capture its visible phase.
    for _ in range(64):
        if game.read(sprite + 0x3E, width=8) & 4 == 0:
            break
        game.run_frames(1)
    game.screenshot(f"region-map-avatar-{avatar_id}.png")
    game.press("B", held_frames=1, released_frames=120)
    game.press("B", held_frames=1, released_frames=120)
    _return_to_field(game, symbols)


def verify_avatar_map_icons(game: Session, symbols: dict[str, int], avatar_id: int) -> None:
    """Check both map surfaces with the same selected avatar."""
    verify_frontier_map_icon(game, symbols, avatar_id)
    verify_region_map_icon(game, symbols, avatar_id)
