"""Shared fixture and UI helpers for Team Lab integration scenarios."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import FIXTURE_ELF, FIXTURE_ROM, FIXTURE_SAVED, GAMEPLAY_ELF, GAMEPLAY_ROM, wait_for_value


SPECIES_LUDICOLO = 297
MOVE_FAKE_OUT = 252
FLAG_SYS_PC_FROM_POKENAV = 0x20
FLAG_FRONTIER_INTRO_COMPLETE = 0x91B
FLAG_FRONTIER_INTRO_PENDING = 0x91C


class TeamLabScenarioFailure(E2EError):
    """The game did not satisfy a Team Lab integration predicate."""


def create_team_lab_save(artifact_dir: Path, save: Path, *, tutorial: bool = False) -> None:
    symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        keys = ["START", "SELECT"]
        if tutorial:
            keys.append("UP")
        fixture.set_keys(*keys)
        wait_for_value(fixture, symbols["gE2EFixtureStatus"], FIXTURE_SAVED, width=32)
        fixture.set_keys()


def gameplay_symbols(*extra: str) -> dict[str, int]:
    return require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "CB2_Pokenav",
        "CB2_Overworld",
        "CB2_TeamLabScreen",
        "CB2_UpdatePartyMenu",
        "gE2ETeamLabSaveCount",
        "gE2ETeamLabSavedBuild",
        "gMain",
        "gSaveBlock1Ptr",
        "sLockFieldControls",
        *extra,
    )


def wait_callback(game: Session, symbols: dict[str, int], name: str, max_frames: int = 900) -> None:
    game.wait(symbols["gMain"] + 4, symbols[name] | 1, max_frames=max_frames)


def boot_to_field(game: Session, symbols: dict[str, int]) -> None:
    for _ in range(60):
        if (
            game.read(symbols["gMain"] + 4) == symbols["CB2_Overworld"] | 1
            and game.read(symbols["sLockFieldControls"], width=8) == 0
        ):
            return
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise TeamLabScenarioFailure("Continue did not reach an unlocked field")


def open_condition_menu(game: Session, symbols: dict[str, int]) -> None:
    game.press("L", held_frames=1, released_frames=60)
    wait_callback(game, symbols, "CB2_Pokenav")
    game.run_frames(120)
    game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=90)


def open_team_lab_editor(game: Session, symbols: dict[str, int]) -> None:
    open_condition_menu(game, symbols)
    game.press("A", held_frames=1, released_frames=90)
    wait_callback(game, symbols, "CB2_UpdatePartyMenu")
    game.run_frames(120)
    game.press("A", held_frames=1, released_frames=90)
    wait_callback(game, symbols, "CB2_TeamLabScreen")
    game.run_frames(90)


def saved_build(game: Session, symbols: dict[str, int]) -> dict[str, object]:
    base = symbols["gE2ETeamLabSavedBuild"]
    return {
        "species": game.read(base, width=16),
        "held_item": game.read(base + 2, width=16),
        "moves": [game.read(base + 4 + i * 2, width=16) for i in range(4)],
        "level": game.read(base + 12, width=8),
        "nature": game.read(base + 13, width=8),
        "ability": game.read(base + 14, width=8),
        "ivs": [game.read(base + 15 + i, width=8) for i in range(6)],
        "evs": [game.read(base + 21 + i, width=8) for i in range(6)],
    }


def flag_address(save_block1: int, flag: int) -> tuple[int, int]:
    return save_block1 + 0x1270 + flag // 8, 1 << (flag % 8)
