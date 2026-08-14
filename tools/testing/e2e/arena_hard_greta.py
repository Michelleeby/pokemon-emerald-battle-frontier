"""Verify the modified hard-mode Arena trainer pool and Greta boundary."""

from __future__ import annotations

from pathlib import Path

from .session import E2EError, Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    CHALLENGE_STATUS_WON,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    FIXTURE_ELF,
    FIXTURE_ROM,
    FIXTURE_SAVED,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    current_map,
    map_id,
    start_singles_level_50,
    wait_for_value,
)


MAP_NUM_ARENA_LOBBY = 28
FRONTIER_CHALLENGE_HARD = 1
TRAINER_FRONTIER_BRAIN = 1022
HARD_ARENA_START_STREAK = 12
HARD_ARENA_COMPLETED_STREAK = 14
STREAK_ARENA_50 = 1 << 6


class ScenarioFailure(E2EError):
    """The game did not satisfy the targeted hard-mode Arena predicates."""


def _create_save(artifact_dir: Path, save: Path) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        fixture.set_keys("R", "SELECT")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def _addresses(save_block1: int, save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "normal_active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xDF2,
        "hard_active_flags": save_block1 + 0x35C0,
        "hard_win_streak": save_block1 + 0x35C4,
    }


def _wait_for_lobby(
    game: Session,
    save_block1_ptr: int,
    map_header: int,
    expected_layout: int,
    player_avatar: int,
    object_events: int,
) -> None:
    for _ in range(60):
        save_block1 = game.read(save_block1_ptr)
        if 0x02000000 <= save_block1 < 0x02040000:
            player_object_id = game.read(player_avatar + 5, width=8)
            player_is_active = (
                player_object_id < 16
                and game.read(object_events + player_object_id * 0x24, width=8) & 1
            )
            if (
                current_map(game, save_block1_ptr) == map_id(MAP_NUM_ARENA_LOBBY)
                and game.read(map_header) == expected_layout
                and player_is_active
            ):
                return
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise ScenarioFailure("Continue did not reach the Battle Arena lobby")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "arena-hard-greta.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gMain",
        "gMapHeader",
        "gObjectEvents",
        "gPlayerAvatar",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSelectedOrderFromParty",
        "gTrainerBattleOpponent_A",
        "gSpecialVar_Result",
        "BattleFrontier_BattleArenaLobby_Layout",
        "CB2_UpdatePartyMenu",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )

    _create_save(artifact_dir, save)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        _wait_for_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleArenaLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = _addresses(save_block1, save_block2)
        if game.read(addresses["hard_win_streak"], width=16) != HARD_ARENA_START_STREAK:
            raise ScenarioFailure("hard Arena fixture did not start at streak 12")

        game.run_frames(120)
        wait_for_value(game, symbols["sLockFieldControls"], 0)
        start_singles_level_50(
            game,
            symbols["gSpecialVar_Result"],
            symbols["sLockFieldControls"],
            symbols["gSelectedOrderFromParty"],
            symbols["gMain"],
            symbols["CB2_UpdatePartyMenu"],
            hard=True,
        )

        save_block1 = game.read(symbols["gSaveBlock1Ptr"])
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = _addresses(save_block1, save_block2)
        if game.read(addresses["challenge_mode"], width=8) != FRONTIER_CHALLENGE_HARD:
            raise ScenarioFailure("Arena challenge did not enter hard mode")
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Arena challenge did not enter the saving state")

        saw_hard_pool_trainer = False
        saw_won_status = False
        for _ in range(3600):
            trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
            if auto_wins == 0 and 200 <= trainer <= 299:
                saw_hard_pool_trainer = True

            save_block1 = game.read(symbols["gSaveBlock1Ptr"])
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            if (
                0x02000000 <= save_block1 < 0x02040000
                and 0x02000000 <= save_block2 < 0x02040000
            ):
                addresses = _addresses(save_block1, save_block2)
                status = game.read(addresses["challenge_status"], width=8)
                saw_won_status |= status == CHALLENGE_STATUS_WON
                if (
                    saw_won_status
                    and current_map(game, symbols["gSaveBlock1Ptr"])
                    == map_id(MAP_NUM_ARENA_LOBBY)
                    and status == 0
                    and game.read(symbols["sLockFieldControls"], width=8) == 0
                ):
                    break
            game.press("A", held_frames=1, released_frames=29)
        else:
            game.screenshot("route-timeout.png")
            raise ScenarioFailure("hard Arena Greta route did not finish")

        if not saw_hard_pool_trainer:
            raise ScenarioFailure(
                "hard Arena route did not select an ordinary trainer from the 200-299 pool"
            )
        if game.read(symbols["gTrainerBattleOpponent_A"], width=16) != TRAINER_FRONTIER_BRAIN:
            raise ScenarioFailure("hard Arena streak 13 did not battle Greta")
        if game.read(addresses["battle_num"], width=16) != 1:
            raise ScenarioFailure("hard Greta route has an unexpected battle number")
        if game.read(addresses["hard_win_streak"], width=16) != HARD_ARENA_COMPLETED_STREAK:
            raise ScenarioFailure("hard Arena streak did not reach 14")
        if game.read(addresses["normal_win_streak"], width=16) != 0:
            raise ScenarioFailure("hard Arena route changed the normal streak")
        if not game.read(addresses["hard_active_flags"]) & STREAK_ARENA_50:
            raise ScenarioFailure("hard Arena Lv. 50 streak is not active")
        if game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32) != 2:
            raise ScenarioFailure("hard Greta route did not use exactly two assisted outcomes")
        game.screenshot("passed.png")
