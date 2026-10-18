#!/usr/bin/env python3

from __future__ import annotations

import json
import os
from pathlib import Path
import tempfile
import unittest
from unittest.mock import patch

from e2e.session import (
    CommandError,
    DriverTimeout,
    ProtocolError,
    Session,
    key_mask,
    parse_fields,
)


ROOT = Path(__file__).resolve().parents[2]


class SessionUnitTests(unittest.TestCase):
    def test_key_mask_combines_names_case_insensitively(self) -> None:
        self.assertEqual(key_mask("a", "LEFT", "r"), 0x121)

    def test_key_mask_rejects_unknown_name(self) -> None:
        with self.assertRaisesRegex(ValueError, "unknown GBA key"):
            key_mask("MENU")

    def test_parse_fields_ignores_non_fields(self) -> None:
        self.assertEqual(
            parse_fields("OK frames=12 value=0x34 detail"),
            {"frames": "12", "value": "0x34"},
        )

    def test_read_bytes_uses_little_endian_hex_readback(self) -> None:
        session = Session(rom=Path(__file__), artifact_dir=Path(tempfile.gettempdir()))
        with patch.object(session, "_command", return_value="OK data=01020304") as command:
            self.assertEqual(session.read_bytes(0x02000000, 4), b"\x01\x02\x03\x04")
        command.assert_called_once_with("READBLOCK 0x2000000 4")

    def test_read_bytes_rejects_invalid_length_and_hex(self) -> None:
        session = Session(rom=Path(__file__), artifact_dir=Path(tempfile.gettempdir()))
        with patch.object(session, "_command", return_value="OK data=0102"):
            with self.assertRaisesRegex(ProtocolError, "invalid data length"):
                session.read_bytes(0x02000000, 4)
        with patch.object(session, "_command", return_value="OK data=zzzz"):
            with self.assertRaisesRegex(ProtocolError, "invalid hexadecimal"):
                session.read_bytes(0x02000000, 2)

    def test_read_bytes_rejects_oversized_blocks(self) -> None:
        session = Session(rom=Path(__file__), artifact_dir=Path(tempfile.gettempdir()))
        with self.assertRaisesRegex(ValueError, "between 0 and 65536"):
            session.read_bytes(0x02000000, 65537)

    def test_wall_clock_timeout_kills_driver_and_records_trace(self) -> None:
        fake_driver = ROOT / "tests" / "fixtures" / "e2e-fake-driver.py"
        with tempfile.TemporaryDirectory() as directory:
            session = Session(
                rom=Path(__file__),
                runner=fake_driver,
                artifact_dir=Path(directory),
                response_timeout=0.05,
            )
            with session:
                with self.assertRaises(DriverTimeout):
                    session._command("HANG")

            trace = json.loads((Path(directory) / "input-trace.json").read_text())
            self.assertEqual(trace["events"][-1]["command"], "HANG")
            self.assertIn("DriverTimeout", trace["events"][-1]["response"])

    def test_runner_crash_is_reported_and_recorded(self) -> None:
        fake_driver = ROOT / "tests" / "fixtures" / "e2e-fake-driver.py"
        with tempfile.TemporaryDirectory() as directory:
            session = Session(
                rom=Path(__file__), runner=fake_driver, artifact_dir=Path(directory)
            )
            with session:
                with self.assertRaisesRegex(ProtocolError, "status 17"):
                    session._command("CRASH")

            trace = json.loads((Path(directory) / "input-trace.json").read_text())
            self.assertIn("ProtocolError", trace["events"][-1]["response"])


@unittest.skipUnless(
    os.environ.get("E2E_INTEGRATION") == "1",
    "set E2E_INTEGRATION=1 through make check-e2e-runner",
)
class SessionDriverIntegrationTests(unittest.TestCase):
    def test_driver_lifecycle_and_protocol(self) -> None:
        runner = ROOT / "build" / "e2e" / "mgba-e2e"
        rom = ROOT / "pokeemerald.gba"

        with tempfile.TemporaryDirectory() as directory:
            artifacts = Path(directory) / "artifacts"
            save = Path(directory) / "game.sav"
            with Session(rom, artifacts, save=save, runner=runner) as game:
                game.ping()
                self.assertEqual(game.read_bytes(0x080000A0, 12), b"POKEMON EMER")
                self.assertEqual(game.read_bytes(0x080000A0, 0), b"")
                with self.assertRaises(CommandError):
                    game._command("READBLOCK 0x08000000 65537")
                self.assertEqual(game.read(0x04000130, width=16), 0x3FF)
                self.assertEqual(game.run_frames(2), 2)
                game.set_keys("A")
                game.run_frames(1)
                self.assertEqual(game.read(0x04000130, width=16), 0x3FE)
                game.set_keys()
                frames, value = game.wait(
                    0x04000130, 0x3FF, mask=0x3FF, width=16, max_frames=1
                )
                self.assertEqual((frames, value), (0, 0x3FF))
                screenshot = game.screenshot("frame.png")
                self.assertEqual(screenshot.read_bytes()[:8], b"\x89PNG\r\n\x1a\n")
                self.assertEqual(game.restart(), 0)
                with self.assertRaises(CommandError):
                    game.wait(0x02000000, 0xAA, width=8, max_frames=0)

            self.assertEqual(save.stat().st_size, 128 * 1024)
            trace = json.loads((artifacts / "input-trace.json").read_text())
            self.assertGreaterEqual(len(trace["events"]), 10)


if __name__ == "__main__":
    unittest.main()
