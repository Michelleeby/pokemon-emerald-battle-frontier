"""Battle Tower lobby cancellation end-to-end scenario."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols


ROOT = Path(__file__).resolve().parents[3]
FIXTURE_ROM = ROOT / "build" / "e2e" / "fixtures" / "tower-lobby.gba"
FIXTURE_ELF = FIXTURE_ROM.with_suffix(".elf")
RELEASE_ROM = ROOT / "pokeemerald.gba"
RELEASE_ELF = ROOT / "pokeemerald.elf"

FIXTURE_SAVED = 0x45324532
MAP_GROUP_TOWER_LOBBY = 0x1A
MAP_NUM_TOWER_LOBBY = 0x05
MULTI_B_PRESSED = 127


class ScenarioFailure(E2EError):
    """The game did not satisfy a scenario predicate."""


def _wait_for_value(
    game: Session,
    address: int,
    expected: int,
    *,
    width: int = 8,
    max_frames: int = 600,
) -> None:
    game.wait(address, expected, width=width, max_frames=max_frames)


def _wait_for_tower_lobby(
    game: Session,
    save_block1_ptr: int,
    map_header: int,
    expected_layout: int,
    player_avatar: int,
    object_events: int,
) -> int:
    for _ in range(60):
        save_block1 = game.read(save_block1_ptr)
        if 0x02000000 <= save_block1 < 0x02040000:
            map_id = game.read(save_block1 + 4, width=16)
            player_object_id = game.read(player_avatar + 5, width=8)
            player_is_active = (
                player_object_id < 16
                and game.read(object_events + player_object_id * 0x24, width=8) & 1
            )
            if (
                map_id == (MAP_NUM_TOWER_LOBBY << 8) | MAP_GROUP_TOWER_LOBBY
                and game.read(map_header) == expected_layout
                and player_is_active
            ):
                return save_block1
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise ScenarioFailure("Continue did not reach the Battle Tower lobby in 3600 frames")


def _advance_until(game: Session, address: int, expected: int, key: str) -> None:
    for _ in range(20):
        if game.read(address, width=8) == expected:
            return
        game.press(key, held_frames=1, released_frames=29)
    raise ScenarioFailure(
        f"predicate at {address:#x} did not become {expected:#x} while pressing {key}"
    )


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "tower-lobby.sav"
    fixture_symbols = require_symbols(
        load_symbols(FIXTURE_ELF), "gE2EFixtureStatus"
    )
    release_symbols = require_symbols(
        load_symbols(RELEASE_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSpecialVar_Result",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "BattleFrontier_BattleTowerLobby_Layout",
        "sLockFieldControls",
    )

    with Session(
        FIXTURE_ROM, artifact_dir / "fixture", save=save
    ) as fixture:
        _wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )

    with Session(RELEASE_ROM, artifact_dir / "scenario", save=save) as game:
        save_block1 = _wait_for_tower_lobby(
            game,
            release_symbols["gSaveBlock1Ptr"],
            release_symbols["gMapHeader"],
            release_symbols["BattleFrontier_BattleTowerLobby_Layout"],
            release_symbols["gPlayerAvatar"],
            release_symbols["gObjectEvents"],
        )
        game.run_frames(120)
        _wait_for_value(game, release_symbols["sLockFieldControls"], 0)

        if game.read(save_block1, width=16) != 6 or game.read(save_block1 + 2, width=16) != 6:
            raise ScenarioFailure("fixture did not load at Tower lobby coordinate (6, 6)")

        save_block2 = game.read(release_symbols["gSaveBlock2Ptr"])
        challenge_status = save_block2 + 0xCA8
        initial_status = game.read(challenge_status, width=8)

        player_object_id = game.read(release_symbols["gPlayerAvatar"] + 5, width=8)
        player_facing = game.read(
            release_symbols["gObjectEvents"] + player_object_id * 0x24 + 0x18,
            width=16,
        ) & 0xF
        if player_facing != 2:
            raise ScenarioFailure(f"fixture player faces {player_facing}, not north")

        game.press("A", held_frames=1, released_frames=2)
        _wait_for_value(game, release_symbols["sLockFieldControls"], 1)
        _advance_until(game, release_symbols["gSpecialVar_Result"], 0xFF, "A")

        game.press("B", held_frames=1, released_frames=2)
        _wait_for_value(
            game,
            release_symbols["gSpecialVar_Result"],
            MULTI_B_PRESSED,
            width=16,
        )
        _advance_until(game, release_symbols["sLockFieldControls"], 0, "A")

        final_status = game.read(challenge_status, width=8)
        if final_status != initial_status:
            raise ScenarioFailure(
                f"Tower challenge status changed from {initial_status} to {final_status}"
            )
        if game.read(save_block1 + 4, width=16) != (
            MAP_NUM_TOWER_LOBBY << 8
        ) | MAP_GROUP_TOWER_LOBBY:
            raise ScenarioFailure("cancellation left the Battle Tower lobby")

        game.screenshot("passed.png")
