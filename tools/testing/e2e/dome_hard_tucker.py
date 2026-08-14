"""Verify the modified hard-mode Dome trainer pool and Tucker boundary."""

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


MAP_NUM_DOME_LOBBY = 18
MAP_NUM_DOME_PRE_BATTLE_ROOM = 20
FRONTIER_CHALLENGE_HARD = 1
TRAINER_FRONTIER_BRAIN = 1022
HARD_DOME_START_STREAK = 2
HARD_DOME_COMPLETED_STREAK = 3
STREAK_DOME_HARD_SINGLES_50 = 1 << 26
DOME_ROUNDS_COUNT = 4
PARTY_MENU_SLOT_ID_OFFSET = 9
TASK_SIZE = 40
TASK_IS_ACTIVE_OFFSET = 4


class ScenarioFailure(E2EError):
    """The game did not satisfy the targeted hard-mode Dome predicates."""


def _create_save(artifact_dir: Path, save: Path) -> None:
    fixture_symbols = require_symbols(load_symbols(FIXTURE_ELF), "gE2EFixtureStatus")
    with Session(FIXTURE_ROM, artifact_dir / "fixture", save=save) as fixture:
        fixture.set_keys("L", "SELECT")
        wait_for_value(
            fixture,
            fixture_symbols["gE2EFixtureStatus"],
            FIXTURE_SAVED,
            width=32,
        )
        fixture.set_keys()


def _addresses(save_block2: int) -> dict[str, int]:
    return {
        "challenge_status": save_block2 + 0xCA8,
        "battle_num": save_block2 + 0xCB2,
        "active_flags": save_block2 + 0xCDC,
        "challenge_mode": save_block2 + 0xD09,
        "normal_win_streak": save_block2 + 0xD0C,
        "hard_win_streak": save_block2 + 0xD24,
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
                current_map(game, save_block1_ptr) == map_id(MAP_NUM_DOME_LOBBY)
                and game.read(map_header) == expected_layout
                and player_is_active
            ):
                return
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise ScenarioFailure("Continue did not reach the Battle Dome lobby")


def _register_highlighted_party_member(
    game: Session,
    selected_order_slot: int,
    tasks: int,
    choose_mon_task: int,
    selection_menu_task: int,
) -> int:
    for _ in range(30):
        if _task_is_active(game, tasks, selection_menu_task):
            break
        if _task_is_active(game, tasks, choose_mon_task):
            game.press("A", held_frames=1, released_frames=29)
        else:
            game.run_frames(30)
    else:
        raise ScenarioFailure("Dome party action menu did not open")
    game.press("A", held_frames=1, released_frames=119)
    for _ in range(600):
        selected = game.read(selected_order_slot, width=8)
        if selected in (1, 2, 3):
            return selected
        game.run_frames(1)
    raise ScenarioFailure("Dome round did not register a party member")


def _task_is_active(game: Session, tasks: int, task_func: int) -> bool:
    expected_func = task_func | 1
    for task_id in range(16):
        task = tasks + task_id * TASK_SIZE
        if (
            game.read(task + TASK_IS_ACTIVE_OFFSET, width=8)
            and game.read(task) == expected_func
        ):
            return True
    return False


def _wait_for_party_input(game: Session, tasks: int, choose_mon_task: int) -> None:
    for _ in range(600):
        if _task_is_active(game, tasks, choose_mon_task):
            return
        game.run_frames(1)
    raise ScenarioFailure("Dome party input did not resume")


def _normalize_party_input(
    game: Session,
    tasks: int,
    choose_mon_task: int,
    selection_menu_task: int,
) -> None:
    if _task_is_active(game, tasks, selection_menu_task):
        game.press("B", held_frames=1, released_frames=119)
    _wait_for_party_input(game, tasks, choose_mon_task)


def _select_first_two_party_members(
    game: Session,
    selected_order: int,
    party_menu: int,
    tasks: int,
    choose_mon_task: int,
    selection_menu_task: int,
) -> None:
    _normalize_party_input(game, tasks, choose_mon_task, selection_menu_task)
    wait_for_value(game, selected_order, 0, width=8, max_frames=600)
    first = _register_highlighted_party_member(
        game,
        selected_order,
        tasks,
        choose_mon_task,
        selection_menu_task,
    )
    _wait_for_party_input(game, tasks, choose_mon_task)
    first_slot = game.read(party_menu + PARTY_MENU_SLOT_ID_OFFSET, width=8)
    game.press("DOWN", held_frames=1, released_frames=119)
    second_slot = game.read(party_menu + PARTY_MENU_SLOT_ID_OFFSET, width=8)
    if second_slot == first_slot:
        raise ScenarioFailure("Dome party cursor did not move to a second member")
    second = _register_highlighted_party_member(
        game,
        selected_order + 1,
        tasks,
        choose_mon_task,
        selection_menu_task,
    )
    if second == first:
        raise ScenarioFailure("Dome round did not select its second party member")
    game.press("A", held_frames=1, released_frames=119)


def _choose_round_party(
    game: Session,
    save_block1_ptr: int,
    main: int,
    party_menu_callback: int,
    selected_order: int,
    party_menu: int,
    tasks: int,
    choose_mon_task: int,
    selection_menu_task: int,
    multichoice_task: int,
) -> None:
    for _ in range(120):
        if _task_is_active(game, tasks, multichoice_task):
            break
        game.press("A", held_frames=1, released_frames=29)
    else:
        raise ScenarioFailure("Dome pre-battle menu did not open")

    game.press("DOWN", held_frames=1, released_frames=29)
    game.press("DOWN", held_frames=1, released_frames=29)
    game.press("A", held_frames=1, released_frames=29)

    for _ in range(120):
        if game.read(main + 4) == party_menu_callback | 1:
            _select_first_two_party_members(
                game,
                selected_order,
                party_menu,
                tasks,
                choose_mon_task,
                selection_menu_task,
            )
            return
        game.press("A", held_frames=1, released_frames=29)
    raise ScenarioFailure("Dome round party selection did not open")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    save = artifact_dir / "dome-hard-tucker.sav"
    save.unlink(missing_ok=True)
    symbols = require_symbols(
        load_symbols(GAMEPLAY_ELF),
        "gMain",
        "gMapHeader",
        "gObjectEvents",
        "gPartyMenu",
        "gPlayerAvatar",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gSelectedOrderFromParty",
        "gTrainerBattleOpponent_A",
        "gSpecialVar_Result",
        "gTasks",
        "BattleFrontier_BattleDomeLobby_Layout",
        "CB2_UpdatePartyMenu",
        "Task_HandleChooseMonInput",
        "Task_HandleMultichoiceInput",
        "Task_HandleSelectionMenuInput",
        "sLockFieldControls",
        E2E_AUTO_WIN_COUNT_SYMBOL,
    )

    _create_save(artifact_dir, save)

    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=save) as game:
        _wait_for_lobby(
            game,
            symbols["gSaveBlock1Ptr"],
            symbols["gMapHeader"],
            symbols["BattleFrontier_BattleDomeLobby_Layout"],
            symbols["gPlayerAvatar"],
            symbols["gObjectEvents"],
        )
        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = _addresses(save_block2)
        if game.read(addresses["hard_win_streak"], width=16) != HARD_DOME_START_STREAK:
            raise ScenarioFailure("hard Dome fixture did not start at streak 2")

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

        save_block2 = game.read(symbols["gSaveBlock2Ptr"])
        addresses = _addresses(save_block2)
        if game.read(addresses["challenge_mode"], width=8) != FRONTIER_CHALLENGE_HARD:
            raise ScenarioFailure("Dome challenge did not enter hard mode")
        if game.read(addresses["challenge_status"], width=8) != CHALLENGE_STATUS_SAVING:
            raise ScenarioFailure("Dome challenge did not enter the saving state")

        opponents: list[int] = []
        for round_index in range(DOME_ROUNDS_COUNT):
            _choose_round_party(
                game,
                symbols["gSaveBlock1Ptr"],
                symbols["gMain"],
                symbols["CB2_UpdatePartyMenu"],
                symbols["gSelectedOrderFromParty"],
                symbols["gPartyMenu"],
                symbols["gTasks"],
                symbols["Task_HandleChooseMonInput"],
                symbols["Task_HandleSelectionMenuInput"],
                symbols["Task_HandleMultichoiceInput"],
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
                addresses = _addresses(save_block2)
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
            raise ScenarioFailure("hard Dome Tucker route did not finish")

        ordinary_opponents = opponents[:-1]
        if len(ordinary_opponents) != DOME_ROUNDS_COUNT - 1 or any(
            not 200 <= trainer <= 299 for trainer in ordinary_opponents
        ):
            raise ScenarioFailure(
                f"hard Dome ordinary opponents are outside the 200-299 pool: {ordinary_opponents}"
            )
        if opponents[-1] != TRAINER_FRONTIER_BRAIN:
            raise ScenarioFailure("hard Dome streak 2 did not battle Tucker in the final")
        battle_num = game.read(addresses["battle_num"], width=16)
        if battle_num != DOME_ROUNDS_COUNT - 1:
            raise ScenarioFailure(
                f"hard Tucker route has unexpected battle number {battle_num}"
            )
        if game.read(addresses["hard_win_streak"], width=16) != HARD_DOME_COMPLETED_STREAK:
            raise ScenarioFailure("hard Dome streak did not reach 3")
        if game.read(addresses["normal_win_streak"], width=16) != 0:
            raise ScenarioFailure("hard Dome route changed the normal streak")
        if not game.read(addresses["active_flags"]) & STREAK_DOME_HARD_SINGLES_50:
            raise ScenarioFailure("hard Dome Singles Lv. 50 streak is not active")
        if game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32) != DOME_ROUNDS_COUNT:
            raise ScenarioFailure("hard Tucker route did not use exactly four assisted outcomes")
        game.screenshot("passed.png")
