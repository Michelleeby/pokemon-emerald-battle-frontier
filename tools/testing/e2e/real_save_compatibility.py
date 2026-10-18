"""Exercise the Audio migration against an imported, immutable real save."""

from __future__ import annotations

import hashlib
import json
import shutil
from pathlib import Path

try:  # ``run_e2e.py`` puts tools/testing directly on sys.path.
    from save_fixture import (  # type: ignore[import-not-found]
        ParsedSave,
        SaveFixtureError,
        audio_record_from_save,
        compare_protected_state,
        extract_protected_state,
        parse_save,
    )
except ModuleNotFoundError:  # Unit-test/import tooling starts at repository root.
    from tools.testing.save_fixture import (
        ParsedSave,
        SaveFixtureError,
        audio_record_from_save,
        compare_protected_state,
        extract_protected_state,
        parse_save,
    )

from .audio_options import audio_record, close_options
from .overworld_menu import save_normally, select_start_action
from .session import Session
from .team_lab import GAMEPLAY_ROM, TeamLabScenarioFailure, boot_to_field, gameplay_symbols


ROOT = Path(__file__).resolve().parents[3]
FIXTURE = ROOT / "tests" / "fixtures" / "saves" / "real-save.sav"
BASELINE = ROOT / "tests" / "fixtures" / "saves" / "real-save.baseline.json"
EXPECTED_AUDIO = (0x4155, 1, 0, 0, 1)


def _sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def _read_bytes(game: Session, address: int, size: int) -> bytes:
    return game.read_bytes(address, size)


def _live_state(game: Session, symbols: dict[str, int]) -> dict[str, object]:
    sb1 = _read_bytes(game, game.read(symbols["gSaveBlock1Ptr"]), 0x3D88)
    sb2 = _read_bytes(game, game.read(symbols["gSaveBlock2Ptr"]), 0xF44)
    storage = _read_bytes(game, game.read(symbols["gPokemonStoragePtr"]), 0x83D0)
    parsed = ParsedSave(b"", "", 0, 0,
                       {0: sb2, 1: sb1[:0xF80], 2: sb1[0xF80:0x1F00],
                        3: sb1[0x1F00:0x2E80], 4: sb1[0x2E80:],
                        5: storage[:0xF80], 6: storage[0xF80:0x1F00],
                        7: storage[0x1F00:0x2E80], 8: storage[0x2E80:0x3E00],
                        9: storage[0x3E00:0x4D80], 10: storage[0x4D80:0x5D00],
                        11: storage[0x5D00:0x6C80], 12: storage[0x6C80:0x7C00],
                        13: storage[0x7C00:]}, ())
    return extract_protected_state(parsed)


def _check_state(expected: dict[str, object], actual: dict[str, object], where: str) -> None:
    try:
        compare_protected_state(expected, actual)
    except SaveFixtureError as error:
        raise TeamLabScenarioFailure(f"{where}: {error}") from error


def _check_audio(record: tuple[int, ...], where: str) -> None:
    if record[:5] != EXPECTED_AUDIO:
        raise TeamLabScenarioFailure(f"{where}: expected Audio defaults {EXPECTED_AUDIO}, got {record}")


def run(artifact_dir: Path) -> None:
    artifact_dir = Path(artifact_dir).resolve()
    if not FIXTURE.is_file() or not BASELINE.is_file():
        raise TeamLabScenarioFailure(
            "real save fixture is not installed; import the supplied .sav with "
            "tools/testing/save_fixture.py before running this scenario")
    original_hash = _sha256(FIXTURE)
    manifest = json.loads(BASELINE.read_text(encoding="utf-8"))
    if manifest.get("source_sha256") != original_hash:
        raise TeamLabScenarioFailure(
            "real save fixture SHA-256 does not match its pinned baseline; "
            "explicitly re-import the intended source save")
    try:
        imported = parse_save(FIXTURE)
    except SaveFixtureError as error:
        raise TeamLabScenarioFailure(f"fixture rejected: {error}") from error
    baseline_state = manifest["state"]
    _check_state(baseline_state, extract_protected_state(imported), "fixture baseline")

    working_save = artifact_dir / "real-save.sav"
    working_save.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(FIXTURE, working_save)
    symbols = gameplay_symbols(
        "gSaveBlock2Ptr", "gPokemonStoragePtr", "gSaveCounter",
        "sNumStartMenuActions", "sStartMenuCursorPos", "sCurrentStartMenuActions",
        "sSaveDialogCallback", "SaveOverwriteInputCallback",
    )
    with Session(GAMEPLAY_ROM, artifact_dir / "scenario", save=working_save) as game:
        boot_to_field(game, symbols)
        _check_state(baseline_state, _live_state(game, symbols), "initial Continue")

        select_start_action(game, symbols, 6)  # MENU_ACTION_OPTION
        for _ in range(3):
            game.press("DOWN", held_frames=1, released_frames=15)
        game.press("A", held_frames=1, released_frames=30)  # Audio
        _check_audio(audio_record(game, symbols), "old save Audio migration")
        close_options(game, symbols)
        _check_state(baseline_state, _live_state(game, symbols), "Options round trip")
        save_normally(game, symbols)

        try:
            saved = parse_save(working_save)
        except SaveFixtureError as error:
            raise TeamLabScenarioFailure(f"normal save is not complete: {error}") from error
        _check_state(baseline_state, extract_protected_state(saved), "saved protected state")
        _check_audio(audio_record_from_save(saved), "saved Audio record")
        if _sha256(FIXTURE) != original_hash:
            raise TeamLabScenarioFailure("immutable real save fixture changed during scenario")

        game.restart()
        boot_to_field(game, symbols)
        _check_state(baseline_state, _live_state(game, symbols), "reloaded Continue")
        _check_audio(audio_record(game, symbols), "reloaded Audio settings")
        try:
            parse_save(working_save)
        except SaveFixtureError as error:
            raise TeamLabScenarioFailure(f"reloaded save is not complete: {error}") from error
        if _sha256(FIXTURE) != original_hash:
            raise TeamLabScenarioFailure("immutable real save fixture changed after restart")
        game.screenshot("passed.png")
