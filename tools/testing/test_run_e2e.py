#!/usr/bin/env python3

from __future__ import annotations

import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import run_e2e
from e2e.session import DriverTimeout, ProtocolError
from run_e2e import failure_status, selected_scenarios, stage_failure_artifacts


class E2ESelectionTests(unittest.TestCase):
    def test_empty_selection_runs_all_scenarios(self) -> None:
        self.assertEqual(
            selected_scenarios([]),
            [
                "arena-hard-greta",
                "arena-normal-greta",
                "dome-hard-tucker",
                "dome-normal-tucker",
                "factory-hard-noland",
                "factory-hard-setup",
                "tower-hard-anabel",
                "tower-normal-anabel",
            ],
        )

    def test_named_selection_is_preserved(self) -> None:
        self.assertEqual(
            selected_scenarios(["tower-hard-anabel"]), ["tower-hard-anabel"]
        )

    def test_factory_setup_scenario_can_be_selected(self) -> None:
        self.assertEqual(
            selected_scenarios(["factory-hard-setup"]), ["factory-hard-setup"]
        )

    def test_unknown_scenario_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown E2E scenario"):
            selected_scenarios(["missing"])

    def test_duplicate_scenario_is_rejected(self) -> None:
        with self.assertRaisesRegex(ValueError, "duplicate E2E scenario"):
            selected_scenarios(["tower-hard-anabel", "tower-hard-anabel"])

    def test_failure_status_distinguishes_timeout_and_runner_crash(self) -> None:
        self.assertEqual(failure_status(DriverTimeout("late")), "timed-out")
        self.assertEqual(failure_status(ProtocolError("gone")), "runner-crashed")
        self.assertEqual(failure_status(AssertionError("wrong")), "failed")

    def test_failure_staging_uses_strict_allow_list(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            source = root / "source"
            source.mkdir()
            (source / "report.json").write_text("{}")
            (source / "emulator.log").write_text("failure")
            (source / "failure.png").write_bytes(b"png")
            (source / "scenario.sav").write_bytes(b"save")
            (source / "game.gba").write_bytes(b"rom")
            (source / "game.elf").write_bytes(b"elf")
            with mock.patch.object(run_e2e, "UPLOAD_ROOT", root / "upload"):
                destination = stage_failure_artifacts("scenario", source)
            copied = sorted(
                path.name for path in destination.iterdir() if path.is_file()
            )
            self.assertEqual(
                copied,
                ["emulator.log", "failure.png", "report.json", "scenario.sav"],
            )

    def test_failure_report_contains_reproduction_metadata(self) -> None:
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            rom = root / "game.gba"
            rom.write_bytes(b"rom")

            def fail(artifact_dir: Path) -> None:
                artifact_dir.mkdir(parents=True)
                raise AssertionError("expected field control")

            with (
                mock.patch.object(run_e2e, "ARTIFACT_ROOT", root / "artifacts"),
                mock.patch.object(run_e2e, "UPLOAD_ROOT", root / "upload"),
                mock.patch.object(run_e2e, "RELEASE_ROM", rom),
                mock.patch.object(run_e2e, "GAMEPLAY_ROM", rom),
                mock.patch.dict(run_e2e.SCENARIOS, {"tower-hard-anabel": fail}, clear=True),
                mock.patch.object(run_e2e, "commit_sha", return_value="abc123"),
            ):
                self.assertEqual(run_e2e.main(["tower-hard-anabel"]), 1)

            report = json.loads(
                (root / "artifacts" / "tower-hard-anabel" / "report.json").read_text()
            )
            self.assertEqual(report["commit_sha"], "abc123")
            self.assertEqual(report["failed_predicate"], "expected field control")
            self.assertEqual(report["status"], "failed")
            for field in (
                "fixture_version",
                "mgba_revision",
                "rng_seed",
                "rom_sha256",
                "gameplay_rom_sha256",
                "rtc_value",
                "scenario_version",
                "timeout",
            ):
                self.assertIn(field, report)

    def test_timeout_and_crash_write_distinct_report_statuses(self) -> None:
        for error, expected in (
            (DriverTimeout("driver stopped responding"), "timed-out"),
            (ProtocolError("driver exited with status 17"), "runner-crashed"),
        ):
            with self.subTest(status=expected), tempfile.TemporaryDirectory() as directory:
                root = Path(directory)
                rom = root / "game.gba"
                rom.write_bytes(b"rom")

                def fail(artifact_dir: Path, raised: BaseException = error) -> None:
                    artifact_dir.mkdir(parents=True)
                    raise raised

                with (
                    mock.patch.object(run_e2e, "ARTIFACT_ROOT", root / "artifacts"),
                    mock.patch.object(run_e2e, "UPLOAD_ROOT", root / "upload"),
                    mock.patch.object(run_e2e, "RELEASE_ROM", rom),
                    mock.patch.object(run_e2e, "GAMEPLAY_ROM", rom),
                    mock.patch.dict(
                        run_e2e.SCENARIOS,
                        {"tower-hard-anabel": fail},
                        clear=True,
                    ),
                    mock.patch.object(run_e2e, "commit_sha", return_value="abc123"),
                ):
                    self.assertEqual(run_e2e.main(["tower-hard-anabel"]), 1)

                report = json.loads(
                    (
                        root
                        / "artifacts"
                        / "tower-hard-anabel"
                        / "report.json"
                    ).read_text()
                )
                self.assertEqual(report["status"], expected)


if __name__ == "__main__":
    unittest.main()
