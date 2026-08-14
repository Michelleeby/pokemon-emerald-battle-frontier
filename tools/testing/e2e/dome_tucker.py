"""Shared normal and hard Battle Dome Tucker boundary scenario."""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path

from .dome import (
    DOME_ROUNDS_COUNT,
    MAP_NUM_DOME_LOBBY,
    MAP_NUM_DOME_PRE_BATTLE_ROOM,
    STREAK_DOME_HARD_SINGLES_50,
    STREAK_DOME_SINGLES_50,
    DomeScenarioFailure as ScenarioFailure,
    continue_to_dome_battle,
    create_dome_tournament_save,
    dome_mode_addresses,
)
from .session import Session
from .symbols import load_symbols, require_symbols
from .tower import (
    CHALLENGE_STATUS_SAVING,
    CHALLENGE_STATUS_WON,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    current_map,
    map_id,
)


TRAINER_FRONTIER_BRAIN = 1022
FRONTIER_CHALLENGE_NORMAL = 0
FRONTIER_CHALLENGE_HARD = 1


@dataclass(frozen=True)
class TuckerMode:
    name: str
    hard: bool
    challenge_mode: int
    start_streak: int
    completed_streak: int
    ordinary_pool: tuple[int, int]
    active_flag: int


NORMAL_TUCKER = TuckerMode(
    name="normal",
    hard=False,
    challenge_mode=FRONTIER_CHALLENGE_NORMAL,
    start_streak=4,
    completed_streak=5,
    ordinary_pool=(140, 199),
    active_flag=STREAK_DOME_SINGLES_50,
)
HARD_TUCKER = TuckerMode(
    name="hard",
    hard=True,
    challenge_mode=FRONTIER_CHALLENGE_HARD,
    start_streak=2,
    completed_streak=3,
    ordinary_pool=(200, 299),
    active_flag=STREAK_DOME_HARD_SINGLES_50,
)


def run_tucker_route(artifact_dir: Path, mode: TuckerMode) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / f"dome-{mode.name}-tucker.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTrainerBattleOpponent_A",
        "gTasks",
        "Task_HandleMultichoiceInput",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )
    create_dome_tournament_save(artifact_dir, save, tucker_mode=mode.name)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        for _ in range(60):
            if current_map(game, symbols["gSaveBlock1Ptr"]) == map_id(
                MAP_NUM_DOME_PRE_BATTLE_ROOM
            ):
                break
            game.press("START", held_frames=1, released_frames=29)
            game.press("A", held_frames=1, released_frames=29)
        else:
            raise ScenarioFailure("Continue did not reach the Dome pre-battle room")

        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = dome_mode_addresses(save_block2)
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.start_streak:
            raise ScenarioFailure(
                f"{mode.name} Dome fixture did not start at streak {mode.start_streak}"
            )
        challenge_mode = game.read(addresses["challenge_mode"], width=8)
        if challenge_mode != mode.challenge_mode:
            raise ScenarioFailure(
                f"Dome challenge mode is {challenge_mode}, expected {mode.name}"
            )
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Dome challenge did not enter the active saving state")

        opponents: list[int] = []
        for round_index in range(DOME_ROUNDS_COUNT):
            continue_to_dome_battle(
                game,
                symbols["gTasks"],
                symbols["Task_HandleMultichoiceInput"],
                round_index=round_index,
            )
            for _ in range(900):
                auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
                trainer = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
                if auto_wins > round_index:
                    opponents.append(trainer)
                    break
                game.press("A", held_frames=1, released_frames=29)
            else:
                game.screenshot(f"round-{round_index + 1}-timeout.png")
                raise ScenarioFailure(f"Dome round {round_index + 1} did not finish")

        saw_won_status = False
        for _ in range(1200):
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            if 0x02000000 <= save_block2 < 0x02040000:
                addresses = dome_mode_addresses(save_block2)
                status = game.read(addresses["challenge_status"], width=8)
                saw_won_status |= status == CHALLENGE_STATUS_WON
                if (
                    saw_won_status
                    and current_map(game, symbols["gSaveBlock1Ptr"])
                    == map_id(MAP_NUM_DOME_LOBBY)
                    and status == 0
                    and game.read(symbols["sLockFieldControls"], width=8) == 0
                ):
                    break
            game.press("A", held_frames=1, released_frames=29)
        else:
            game.screenshot("route-timeout.png")
            raise ScenarioFailure(f"{mode.name} Dome Tucker route did not finish")

        ordinary_opponents = opponents[:-1]
        if len(ordinary_opponents) != DOME_ROUNDS_COUNT - 1 or any(
            not mode.ordinary_pool[0] <= trainer <= mode.ordinary_pool[1]
            for trainer in ordinary_opponents
        ):
            raise ScenarioFailure(
                f"{mode.name} Dome ordinary opponents are outside the "
                f"{mode.ordinary_pool[0]}-{mode.ordinary_pool[1]} pool: "
                f"{ordinary_opponents}"
            )
        if opponents[-1] != TRAINER_FRONTIER_BRAIN:
            raise ScenarioFailure(f"{mode.name} Dome boundary did not battle Tucker")
        battle_num = game.read(addresses["battle_num"], width=16)
        if battle_num != DOME_ROUNDS_COUNT - 1:
            raise ScenarioFailure(
                f"{mode.name} Tucker route has unexpected battle number {battle_num}"
            )
        streak_address = addresses[
            "hard_win_streak" if mode.hard else "normal_win_streak"
        ]
        if game.read(streak_address, width=16) != mode.completed_streak:
            raise ScenarioFailure(
                f"{mode.name} Dome streak did not reach {mode.completed_streak}"
            )
        isolated_address = addresses[
            "normal_win_streak" if mode.hard else "hard_win_streak"
        ]
        if game.read(isolated_address, width=16) != 0:
            raise ScenarioFailure(f"{mode.name} Dome route changed the other mode's streak")
        if not game.read(addresses["active_flags"], width=32) & mode.active_flag:
            raise ScenarioFailure(f"{mode.name} Dome Singles Lv. 50 streak is not active")
        auto_wins = game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
        if auto_wins != DOME_ROUNDS_COUNT:
            raise ScenarioFailure(
                f"{mode.name} Tucker route used {auto_wins} assisted outcomes, "
                f"expected {DOME_ROUNDS_COUNT}"
            )
        game.screenshot("passed.png")
