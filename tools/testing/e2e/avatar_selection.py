"""Exercise all eight new-game avatar previews, input wrapping, and saving."""

from pathlib import Path
import struct

try:
    from save_fixture import SECTOR_CHECKSUM_OFFSET, SECTOR_DATA_SIZE, SECTOR_SIZE, SECTION_SIZES, calculate_checksum, parse_save
except ModuleNotFoundError:
    from tools.testing.save_fixture import SECTOR_CHECKSUM_OFFSET, SECTOR_DATA_SIZE, SECTOR_SIZE, SECTION_SIZES, calculate_checksum, parse_save

from .audio_options import wait_for_title_input
from .appearance_options import APPEARANCE_SYMBOLS, configure_field_options, configure_title_options, verify_bag_rendering
from .arena import create_arena_battle_room_save
from .avatar_map_icons import MAP_ICON_SYMBOLS, verify_avatar_map_icons
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


def _decompress_picture(data: bytes, *, max_size: int = 4096) -> bytes:
    """Decode a bounded GBA LZ77 trainer picture for a pixel comparison."""
    _check(data[0] == 0x10, "trainer picture is not a GBA LZ77 asset")
    size = int.from_bytes(data[1:4], "little")
    _check(0 < size <= max_size, f"invalid picture size {size}")
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


def _player_object(game: Session, symbols: dict[str, int]) -> int:
    object_id = game.read(symbols["gPlayerAvatar"] + 5, width=8)
    _check(object_id < 16, "field has no valid player object")
    return symbols["gObjectEvents"] + object_id * 36


def _on_foot(game: Session, symbols: dict[str, int]) -> None:
    _check(game.read(symbols["gPlayerAvatar"], width=8) & 0x1F == 1,
           "travel activated a bike, surf, or dive state")
    _check(game.read(_player_object(game, symbols) + 5, width=8) == 134,
           "travel replaced Steven's overworld appearance")


def _movement_samples(game: Session, symbols: dict[str, int], steps: int,
                      *, max_frames: int = 160) -> list[tuple[int, int]]:
    previous_x = game.read(_player_object(game, symbols) + 16, width=16)
    changes = []
    for frame in range(max_frames):
        game.run_frames(1)
        _on_foot(game, symbols)
        x = game.read(_player_object(game, symbols) + 16, width=16)
        if x != previous_x:
            _check(abs(x - previous_x) == 1, "running skipped a collision/step coordinate")
            changes.append((frame, x))
            previous_x = x
            if len(changes) == steps:
                return changes
    raise AvatarSelectionFailure(f"running crossed only {len(changes)} of {steps} clear pier tiles")


def _check_acceleration(changes: list[tuple[int, int]]) -> None:
    intervals = [b[0] - a[0] for a, b in zip(changes, changes[1:])]
    _check(intervals[:2] == [16, 8] and all(value == 4 for value in intervals[2:]),
           f"running did not build Mach Bike speed: {intervals}")


def _coast_to_stop(game: Session, symbols: dict[str, int], direction: int) -> None:
    previous_x = game.read(_player_object(game, symbols) + 16, width=16)
    changes = []
    stop_frame = None
    game.set_keys()
    for frame in range(90):
        game.run_frames(1)
        _on_foot(game, symbols)
        x = game.read(_player_object(game, symbols) + 16, width=16)
        if x != previous_x:
            _check(x == previous_x + direction, "coasting changed direction before stopping")
            changes.append(frame)
            previous_x = x
        if len(changes) == 3 and stop_frame is None and game.read(symbols["gPlayerAvatar"] + 2, width=8) == 0:
            stop_frame = frame
    intervals = [b - a for a, b in zip(changes, changes[1:])]
    _check(len(changes) == 3 and intervals == [4, 8],
           f"release did not coast through three slowing tiles: {changes}")
    _check(stop_frame is not None and 15 <= stop_frame - changes[-1] <= 17,
           f"coasting did not finish at walking speed: stop={stop_frame}, steps={changes}")


def _running_speed(game: Session, symbols: dict[str, int]) -> None:
    # The arrival pier has a clear horizontal path. Observe real input, tile
    # coordinates and frame timing; never write emulator memory to move players.
    game.set_keys("B", "LEFT")
    _check_acceleration(_movement_samples(game, symbols, 5))
    _coast_to_stop(game, symbols, -1)

    # Restart from rest must rebuild momentum, rather than retain top speed.
    game.set_keys("B", "RIGHT")
    _check_acceleration(_movement_samples(game, symbols, 3))
    _coast_to_stop(game, symbols, 1)

    # A direction change first brakes in the old direction, then turns and
    # accelerates from rest. The visual avatar remains Steven for every frame.
    game.set_keys("B", "LEFT")
    _check_acceleration(_movement_samples(game, symbols, 3))
    start_x = game.read(_player_object(game, symbols) + 16, width=16)
    game.set_keys("B", "RIGHT")
    changes = _movement_samples(game, symbols, 6)
    _check([x for _, x in changes[:3]] == [start_x - 1, start_x - 2, start_x - 3],
           f"turning did not coast in the old direction: {changes}")
    _check([x for _, x in changes[3:]] == [start_x - 2, start_x - 1, start_x],
           f"turning did not reverse after braking: {changes}")
    _check_acceleration(changes[3:])
    _coast_to_stop(game, symbols, 1)


def _write_legacy_fixture(source: Path, destination: Path, *, item: int | None = None,
                          travel: str | None = None, avatar_id: int | None = None) -> None:
    # Only mutate this scenario's freshly generated save, never the repository's
    # compatibility fixture or a user's SRAM. Reuse the canonical sector parser.
    parsed = parse_save(source)
    sb1 = bytearray(parsed.save_block1)
    if avatar_id is not None:
        struct.pack_into("<H", sb1, AVATAR_VAR_OFFSET, avatar_id)
    if item is not None:
        key = int.from_bytes(parsed.save_block2[0xAC:0xB0], "little") & 0xFFFF
        struct.pack_into("<H", sb1, 0x496, item)
        struct.pack_into("<HH", sb1, 0x5D8, item, key ^ 1)
    if travel in ("water", "underwater"):
        x, y = (19, 70) if travel == "water" else (10, 10)
        group, number, layout = (26, 4, 265) if travel == "water" else (0, 50, 274)
        struct.pack_into("<hh", sb1, 0, x, y)
        struct.pack_into("<BBbBhh", sb1, 4, group, number, -1, 0, x, y)
        struct.pack_into("<H", sb1, 0x32, layout)
        # Saved map views must describe the new location rather than the pier.
        # General metatile 0x170 is ocean water, matching West (19, 70).
        sb1[0x34:0x234] = (bytes((0x70, 1)) if travel == "water" else bytes(2)) * 256
        cruise_flag = 0x88D
        sb1[0x1270 + cruise_flag // 8] &= ~(1 << (cruise_flag % 8))
    if travel == "bike":
        for index in range(16):
            obj = 0xA30 + index * 36
            if sb1[obj + 2] & 1:
                sb1[obj + 5] = 1  # Legacy Brendan Mach Bike graphics.
                break
        else:
            raise AvatarSelectionFailure("saved fixture has no player object")
    raw = bytearray(parsed.raw)
    for sector in parsed.sectors:
        if not (sector.valid and sector.bank == parsed.bank and sector.counter == parsed.counter
                and 1 <= sector.sector_id <= 4):
            continue
        offset = (sector.sector_id - 1) * SECTOR_DATA_SIZE
        payload = sb1[offset:offset + SECTION_SIZES[sector.sector_id]]
        start = sector.physical_index * SECTOR_SIZE
        raw[start:start + len(payload)] = payload
        struct.pack_into("<H", raw, start + SECTOR_CHECKSUM_OFFSET, calculate_checksum(payload))
    parse_save(raw)  # Fail here if fixture construction broke a section.
    destination.write_bytes(raw)


def _legacy_travel(artifact_dir: Path, source: Path, symbols: dict[str, int]) -> None:
    original = parse_save(source).save_block1
    # Both bikes, all three rods, and the Wailmer Pail must stay unusable even
    # when an older save already owns and registers one of them.
    for item in (259, 272, 262, 263, 264, 268):
        legacy = artifact_dir / f"legacy-item-{item}.sav"
        _write_legacy_fixture(source, legacy, item=item, travel="bike" if item in (259, 272) else None)
        with Session(GAMEPLAY_ROM, artifact_dir / f"legacy-item-{item}", save=legacy) as game:
            boot_to_field(game, symbols)
            _on_foot(game, symbols)
            sb1 = game.read(symbols["gSaveBlock1Ptr"])
            _check(game.read(sb1 + 4, width=16) == int.from_bytes(original[4:6], "little"),
                   "dry legacy save changed maps during travel normalization")
            key = game.read(game.read(symbols["gSaveBlock2Ptr"]) + 0xAC) & 0xFFFF
            _check(game.read(sb1 + 0x5D8, width=16) == item
                   and game.read(sb1 + 0x5DA, width=16) ^ key == 1,
                   "legacy registered-item fixture did not retain the owned item")
            game.press("SELECT", held_frames=1, released_frames=10)
            _on_foot(game, symbols)
            _check(game.read(sb1 + 0x496, width=16) == 0, "unavailable legacy item stayed registered")
            for _ in range(30):
                _on_foot(game, symbols)
                if game.read(symbols["sLockFieldControls"], width=8) == 0:
                    break
                game.press("A", held_frames=1, released_frames=14)
            else:
                raise AvatarSelectionFailure("unavailable registered item left field controls locked")
    for travel in ("water", "underwater"):
        legacy = artifact_dir / f"legacy-{travel}.sav"
        _write_legacy_fixture(source, legacy, travel=travel)
        with Session(GAMEPLAY_ROM, artifact_dir / f"legacy-{travel}", save=legacy) as game:
            boot_to_field(game, symbols)
            _on_foot(game, symbols)
            sb1 = game.read(symbols["gSaveBlock1Ptr"])
            _check(game.read(sb1 + 4, width=16) == 0x0E1A,
                   f"legacy {travel} save did not recover to Frontier east")
            _check((game.read(sb1, width=16), game.read(sb1 + 2, width=16)) == (3, 52),
                   f"legacy {travel} save did not reach the safe landing")
            game.screenshot("safe-landing.png")


def _frontier_standins(artifact_dir: Path, symbols: dict[str, int]) -> None:
    source = artifact_dir / "arena-source.sav"
    create_arena_battle_room_save(artifact_dir / "arena-fixture", source, greta_mode="normal")
    graphics = (0, 89, 230, 231, 235, 236, 135, 134)
    for avatar_id, expected_gfx in enumerate(graphics):
        save = artifact_dir / f"arena-avatar-{avatar_id}.sav"
        _write_legacy_fixture(source, save, avatar_id=avatar_id)
        with Session(GAMEPLAY_ROM, artifact_dir / f"arena-avatar-{avatar_id}", save=save) as game:
            for _ in range(90):
                if (game.read(symbols["gMain"] + 4) == symbols["CB2_Overworld"] | 1
                        and game.read(game.read(symbols["gSaveBlock1Ptr"]) + 4, width=16) == 0x1E1A):
                    break
                game.press("START", held_frames=1, released_frames=14)
                game.press("A", held_frames=1, released_frames=14)
            else:
                raise AvatarSelectionFailure("Continue never reached the Arena entrance")
            for _ in range(60):
                for index in range(16):
                    obj = symbols["gObjectEvents"] + index * 36
                    if game.read(obj, width=8) & 1 and game.read(obj + 8, width=8) == 8:
                        _check(game.read(obj + 5, width=8) == expected_gfx,
                               f"Arena replaced avatar {avatar_id} with graphics {game.read(obj + 5, width=8)}")
                        sprite = symbols["gSprites"] + game.read(obj + 4, width=8) * SPRITE_SIZE
                        _check(game.read(sprite + 5, width=8) >> 4 == 0,
                               "Arena stand-in does not use the player palette")
                        game.screenshot("entrance.png")
                        break
                else:
                    game.run_frames(1)
                    continue
                break
            else:
                raise AvatarSelectionFailure("Arena entrance has no scripted player stand-in")

def _confirm_avatar(game: Session, symbols: dict[str, int]) -> None:
    # The user-requested opening proceeds straight from avatar to naming.
    game.press("A", held_frames=1, released_frames=60)
    game.wait(symbols["gMain"] + 4, symbols["CB2_NamingScreen"] | 1, max_frames=600)


def _saved_customization(game: Session, symbols: dict[str, int], bag: int, theme: int) -> None:
    sb1 = game.read(symbols["gSaveBlock1Ptr"])
    bag_offset = 0x139C + (0x40F8 - 0x4000) * 2
    theme_offset = 0x139C + (0x40F9 - 0x4000) * 2
    _check(game.read(sb1 + bag_offset, width=16) == (0xB100 | bag),
           "saved bag choice does not match the selected bag")
    _check(game.read(sb1 + theme_offset, width=16) == (0xB200 | theme),
           "saved theme choice does not match the selected colors")


def _open_avatar_selection(game: Session, symbols: dict[str, int], *, main_menu_ready: bool = False) -> int:
    if not main_menu_ready:
        wait_for_title_input(game, symbols)
        game.press("START", held_frames=1, released_frames=120)
    game.press("A", held_frames=1, released_frames=60)
    for _ in range(160):
        task = _find_task(game, symbols, "Task_NewGameBirchSpeech_ChooseGender")
        if task is not None:
            _preview(game, symbols, task, 1)
            return task
        game.press("A", held_frames=1, released_frames=14)
    raise AvatarSelectionFailure("new game never reached the avatar chooser")


def _run_independent_combination(artifact_dir: Path, symbols: dict[str, int],
                                *, avatar: int, bag: int, theme: int) -> None:
    save = artifact_dir / f"combination-{avatar}-{bag}-{theme}.sav"
    save.unlink(missing_ok=True)
    with Session(GAMEPLAY_ROM, artifact_dir / f"combination-{avatar}-{bag}-{theme}", save=save) as game:
        configure_title_options(game, symbols, bag, theme, _decompress_picture)
        task = _open_avatar_selection(game, symbols, main_menu_ready=True)
        for selected in range(2, avatar + 1):
            _cycle(game, symbols, task, "DOWN", selected)
        _confirm_avatar(game, symbols)
        game.run_frames(120)
        game.press("A", held_frames=1, released_frames=15)
        game.press("START", held_frames=1, released_frames=30)
        game.press("A", held_frames=1, released_frames=120)
        _reach_field(game, symbols)
        _check(_saved_avatar(game, symbols) == avatar, "new game lost the avatar choice")
        _saved_customization(game, symbols, bag, theme)
        verify_bag_rendering(game, symbols, bag, theme, _decompress_picture)
        if avatar == 3 and bag == 0 and theme == 0:
            verify_avatar_map_icons(game, symbols, avatar)
        save_normally(game, symbols)
        game.restart()
        boot_to_field(game, symbols)
        _check(_saved_avatar(game, symbols) == avatar, "restart lost the avatar choice")
        _saved_customization(game, symbols, bag, theme)
        verify_bag_rendering(game, symbols, bag, theme, _decompress_picture)


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "avatar-selection.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gTasks", "gSprites", "gMain", "Task_TitleScreenPhase3",
        "Task_NewGameBirchSpeech_ChooseGender", "CB2_NamingScreen",
        "gNewGameAvatarStyle", "gSaveBlock1Ptr", "gSaveBlock2Ptr",
        "gTrainerFrontPicTable", *APPEARANCE_SYMBOLS, *MAP_ICON_SYMBOLS,
        "gE2EFrontierIntroCheckpoint", "gE2EFrontierIntroFinishCount",
        "CB2_Overworld", "sLockFieldControls", "sNumStartMenuActions",
        "sStartMenuCursorPos", "sCurrentStartMenuActions", "sSaveDialogCallback",
        "SaveOverwriteInputCallback", "gSaveCounter", "gPlayerAvatar", "gObjectEvents",
    )
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        task = _open_avatar_selection(game, symbols)

        # Complete a cycle in each direction, including both wrap boundaries.
        for step in range(1, 9):
            avatar = (1 + step) % 8
            _cycle(game, symbols, task, "DOWN", avatar)
            game.screenshot(f"avatar-{AVATAR_NAMES[avatar]}.png")
        for step in range(1, 9):
            _cycle(game, symbols, task, "UP", (1 - step) % 8)
        _cycle(game, symbols, task, "UP", 0)
        _cycle(game, symbols, task, "UP", 7)

        _confirm_avatar(game, symbols)
        _check(game.read(symbols["gNewGameAvatarStyle"], width=8) == 7,
               "confirming Steven lost the selected avatar")
        game.run_frames(120)
        game.screenshot("steven-naming.png")
        game.press("A", held_frames=1, released_frames=15)  # Enter a name.
        game.press("START", held_frames=1, released_frames=30)
        game.press("A", held_frames=1, released_frames=120)
        _reach_field(game, symbols)
        _check(_saved_avatar(game, symbols) == 7, "new-game initialization reset the avatar")
        _saved_customization(game, symbols, 0, 0)
        configure_field_options(game, symbols, 1, 1, _decompress_picture)
        _saved_customization(game, symbols, 1, 1)
        verify_bag_rendering(game, symbols, 1, 1, _decompress_picture)
        verify_avatar_map_icons(game, symbols, 7)
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
        _check(game.read(game.read(symbols["gSaveBlock1Ptr"]) + 0x496, width=16) == 0,
               "new game still registers a bike")
        _running_speed(game, symbols)
        save_normally(game, symbols)
        game.restart()
        boot_to_field(game, symbols)
        _check(_saved_avatar(game, symbols) == 7, "avatar did not survive normal save/restart/load")
        _saved_customization(game, symbols, 1, 1)
        _on_foot(game, symbols)
        game.screenshot("passed.png")
    _legacy_travel(artifact_dir, save, symbols)
    _frontier_standins(artifact_dir, symbols)
    for avatar, bag, theme in ((3, 0, 0), (7, 0, 1), (3, 1, 0)):
        _run_independent_combination(artifact_dir, symbols, avatar=avatar, bag=bag, theme=theme)
