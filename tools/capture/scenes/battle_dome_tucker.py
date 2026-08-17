#!/usr/bin/env python3
"""Capture a Battle Dome match and the Normal Mode Tucker final."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / "tools" / "testing"))

from e2e.dome import (  # noqa: E402
    DOME_ROUNDS_COUNT,
    MAP_NUM_DOME_LOBBY,
    MAP_NUM_DOME_PRE_BATTLE_ROOM,
    continue_to_dome_battle,
    dome_mode_addresses,
)
from e2e.session import Session  # noqa: E402
from e2e.symbols import load_symbols, require_symbols  # noqa: E402
from e2e.tower import (  # noqa: E402
    CHALLENGE_STATUS_WON,
    E2E_AUTO_WIN_COUNT_SYMBOL,
    GAMEPLAY_ELF,
    GAMEPLAY_ROM,
    current_map,
    map_id,
)
from tools.capture.encode_gif import encode_gif_sources  # noqa: E402
from tools.capture.fixture import create_battle_dome_tucker_save  # noqa: E402


CAPTURE_GAMEPLAY_ROM = ROOT / "build" / "capture" / "gameplay" / "pokeemerald.gba"
CAPTURE_GAMEPLAY_ELF = CAPTURE_GAMEPLAY_ROM.with_suffix(".elf")
DEFAULT_OUTPUT = ROOT / "build" / "capture" / "battle-dome-tucker"
# End indices are exclusive. The requested GIF uses frames 1065-1250, then
# jumps to frames 2750-3456.
RESULT_RANGES = [(1065, 1251), (2750, 3457)]
CONTROLLER_CHOOSE_ACTION = 18
CONTROLLER_CHOOSE_MOVE = 20


def _boot_pre_battle(game: Session, save_block1: int) -> None:
    for _ in range(60):
        if current_map(game, save_block1) == map_id(MAP_NUM_DOME_PRE_BATTLE_ROOM):
            return
        game.press("START", held_frames=1, released_frames=29)
        game.press("A", held_frames=1, released_frames=29)
    raise RuntimeError("Continue did not reach the Dome pre-battle room")


def _symbols(elf: Path, *, assisted: bool) -> dict[str, int]:
    names = [
        "BattleMainCB2",
        "CB2_UpdatePartyMenu",
        "Task_HandleMultichoiceInput",
        "gMain",
        "gSaveBlock1Ptr",
        "gSaveBlock2Ptr",
        "gTasks",
        "gTrainerBattleOpponent_A",
        "sLockFieldControls",
    ]
    if assisted:
        names.append(E2E_AUTO_WIN_COUNT_SYMBOL)
    return require_symbols(load_symbols(elf), *names)


def _wait_for_controller(
    game: Session, symbols: dict[str, int], command: int, *, max_frames: int = 1800
) -> None:
    for _ in range(max_frames):
        if game.read(symbols["gBattleBufferA"], width=8) == command:
            game.run_frames(30)
            if game.read(symbols["gBattleBufferA"], width=8) == command:
                return
        else:
            game.press("A", held_frames=1, released_frames=7)
    raise RuntimeError(f"battle did not reach controller command {command}")


def _select_move(game: Session, symbols: dict[str, int], slot: int) -> None:
    _wait_for_controller(game, symbols, CONTROLLER_CHOOSE_ACTION)
    # FIGHT is the upper-left action. These inputs are idempotent from any cursor.
    game.press("UP", held_frames=1, released_frames=3)
    game.press("LEFT", held_frames=1, released_frames=3)
    game.press("A", held_frames=1, released_frames=3)
    _wait_for_controller(game, symbols, CONTROLLER_CHOOSE_MOVE, max_frames=120)

    cursor = game.read(symbols["gMoveSelectionCursor"], width=8)
    if cursor & 2 and not slot & 2:
        game.press("UP", held_frames=1, released_frames=3)
    elif not cursor & 2 and slot & 2:
        game.press("DOWN", held_frames=1, released_frames=3)
    cursor = game.read(symbols["gMoveSelectionCursor"], width=8)
    if cursor & 1 and not slot & 1:
        game.press("LEFT", held_frames=1, released_frames=3)
    elif not cursor & 1 and slot & 1:
        game.press("RIGHT", held_frames=1, released_frames=3)
    game.press("A", held_frames=1, released_frames=3)


def _capture_tucker_result(output: Path, save: Path, *, stride: int) -> int:
    frames = output / "result-frames"
    symbols = _symbols(CAPTURE_GAMEPLAY_ELF, assisted=True)
    symbols.update(
        require_symbols(
            load_symbols(CAPTURE_GAMEPLAY_ELF),
            "gBattleBufferA",
            "gMoveSelectionCursor",
            "gBattleOutcome",
        )
    )
    with Session(CAPTURE_GAMEPLAY_ROM, output / "result-session", save=save) as game:
        _boot_pre_battle(game, symbols["gSaveBlock1Ptr"])

        for round_index in range(DOME_ROUNDS_COUNT - 1):
            continue_to_dome_battle(
                game,
                symbols["gTasks"],
                symbols["Task_HandleMultichoiceInput"],
                round_index=round_index,
            )
            for _ in range(900):
                if (
                    game.read(symbols[E2E_AUTO_WIN_COUNT_SYMBOL], width=32)
                    > round_index
                ):
                    break
                game.press("A", held_frames=1, released_frames=29)
            else:
                raise RuntimeError(f"Dome round {round_index + 1} did not finish")

        continue_to_dome_battle(
            game,
            symbols["gTasks"],
            symbols["Task_HandleMultichoiceInput"],
            round_index=DOME_ROUNDS_COUNT - 1,
        )
        for _ in range(600):
            if game.read(symbols["gTrainerBattleOpponent_A"], width=16) == 1022:
                break
            game.press("A", held_frames=1, released_frames=15)
        else:
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            battle_num = (
                game.read(dome_mode_addresses(save_block2)["battle_num"], width=8)
                if 0x02000000 <= save_block2 < 0x02040000
                else -1
            )
            opponent = game.read(symbols["gTrainerBattleOpponent_A"], width=16)
            raise RuntimeError(
                f"Dome final did not reach Tucker (battle={battle_num}, opponent={opponent})"
            )
        game.capture_start(frames, stride=stride)

        # Three Calm Minds happen in the raw capture but are omitted from the
        # encoded scene. Psychic removes Swampert, then Dragon Claw removes
        # Salamence; both attacks are deterministic from the fixture stats.
        for _ in range(3):
            _select_move(game, symbols, 3)
        _select_move(game, symbols, 0)
        _select_move(game, symbols, 1)

        saw_won = False
        for _ in range(1500):
            save_block2 = game.read(symbols["gSaveBlock2Ptr"])
            if 0x02000000 <= save_block2 < 0x02040000:
                status = game.read(
                    dome_mode_addresses(save_block2)["challenge_status"], width=8
                )
                saw_won |= status == CHALLENGE_STATUS_WON
                if (
                    saw_won
                    and status == 0
                    and current_map(game, symbols["gSaveBlock1Ptr"])
                    == map_id(MAP_NUM_DOME_LOBBY)
                    and game.read(symbols["sLockFieldControls"], width=8) == 0
                ):
                    game.run_frames(90)
                    return game.capture_stop()
            game.press("A", held_frames=1, released_frames=15)
        raise RuntimeError("Tucker result route did not return to the Dome lobby")


def capture_battle_dome_tucker(output: Path, *, stride: int = 2) -> int:
    output = Path(output).resolve()
    frames = output / "result-frames"
    if frames.exists() and any(frames.glob("frame-*.png")):
        raise FileExistsError(f"capture frames already exist: {frames}")

    result_save = output / "result.sav"
    if result_save.exists():
        raise FileExistsError(f"capture saves already exist in {output}")
    create_battle_dome_tucker_save(output / "result-fixture", result_save)
    return _capture_tucker_result(output, result_save, stride=stride)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    parser.add_argument("--stride", type=int, default=2)
    parser.add_argument("--no-encode", action="store_true")
    args = parser.parse_args(argv)
    if not args.no_encode and args.stride != 2:
        raise ValueError("README encoding requires --stride 2; use --no-encode otherwise")

    result_count = capture_battle_dome_tucker(args.output, stride=args.stride)
    print(f"Captured {result_count} result frames")
    if not args.no_encode:
        gif = args.output.resolve() / "battle-dome-tucker.gif"
        encoded = encode_gif_sources(
            [(args.output / "result-frames", RESULT_RANGES)],
            gif,
            fps=60 / args.stride,
        )
        print(f"Encoded {encoded} frames to {gif}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
