"""Shared input helpers for production overworld menus and saves."""

from .session import Session
from .team_lab import TeamLabScenarioFailure


def _check(condition: bool, message: str) -> None:
    if not condition:
        raise TeamLabScenarioFailure(message)


def select_start_action(game: Session, symbols: dict[str, int], action: int) -> None:
    """Navigate the actual start menu without relying on optional entries."""
    game.press("START", held_frames=1, released_frames=60)
    count = game.read(symbols["sNumStartMenuActions"], width=8)
    _check(0 < count <= 9, f"invalid start menu action count: {count}")
    for _ in range(count + 1):
        cursor = game.read(symbols["sStartMenuCursorPos"], width=8)
        if game.read(symbols["sCurrentStartMenuActions"] + cursor, width=8) == action:
            game.press("A", held_frames=1, released_frames=90)
            return
        game.press("DOWN", held_frames=1, released_frames=15)
    raise TeamLabScenarioFailure(f"start menu action {action} was not found")


def save_normally(game: Session, symbols: dict[str, int]) -> None:
    """Save through the production menu, then wait for completion."""
    before = game.read(symbols["gSaveCounter"])
    select_start_action(game, symbols, 5)  # MENU_ACTION_SAVE
    for _ in range(100):
        if (
            game.read(symbols["gSaveCounter"]) != before
            and game.read(symbols["sLockFieldControls"], width=8) == 0
        ):
            return
        callback = game.read(symbols["sSaveDialogCallback"])
        if callback == symbols["SaveOverwriteInputCallback"] | 1:
            game.press("UP", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=29)
    raise TeamLabScenarioFailure("normal game save did not complete")
