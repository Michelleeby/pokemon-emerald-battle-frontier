#!/usr/bin/env python3

from __future__ import annotations

import copy
import unittest
from unittest import mock

from select_suites import (
    ManifestError,
    changed_files,
    load_e2e_manifest,
    load_manifest,
    select_e2e_scenarios,
    select_suites,
    validate_e2e_manifest,
    validate_manifest,
)


class SelectorTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls) -> None:
        cls.manifest = load_manifest()
        cls.e2e_manifest = load_e2e_manifest()
        cls.all_suites = sorted(cls.manifest["suites"])

    def select(self, *files: str) -> dict[str, object]:
        return select_suites(list(files), self.manifest)

    def test_documentation_only_selects_no_suites(self) -> None:
        result = self.select("README.md", "docs/testing.md", "plans/notes.md")
        self.assertEqual(result["suites"], [])
        self.assertFalse(result["full"])

    def test_tower_change_selects_tower_and_dependencies(self) -> None:
        result = self.select("src/battle_tower.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-tower", "save-load"],
        )
        self.assertFalse(result["full"])

    def test_factory_change_includes_team_lab_dependency(self) -> None:
        result = self.select("data/maps/BattleFrontier_BattleFactoryLobby/scripts.inc")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-factory", "save-load", "team-lab"],
        )

    def test_dome_change_selects_dome_and_dependencies(self) -> None:
        result = self.select("src/battle_dome.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-dome", "save-load"],
        )

    def test_arena_change_selects_arena_and_dependencies(self) -> None:
        result = self.select("src/battle_arena.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-arena", "frontier-common", "save-load"],
        )

    def test_palace_change_selects_palace_and_dependencies(self) -> None:
        result = self.select("src/battle_palace.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-palace", "save-load"],
        )

    def test_pike_change_selects_pike_and_dependencies(self) -> None:
        result = self.select("src/battle_pike.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-pike", "save-load"],
        )

    def test_pyramid_change_selects_pyramid_and_dependencies(self) -> None:
        result = self.select("src/battle_pyramid.c")
        self.assertEqual(
            result["suites"],
            ["battle-shared", "frontier-common", "frontier-pyramid", "save-load"],
        )

    def test_shared_frontier_change_selects_all_frontier_suites(self) -> None:
        result = self.select("src/frontier_util.c")
        expected = {
            "battle-shared",
            "frontier-common",
            "frontier-arena",
            "frontier-factory",
            "frontier-palace",
            "frontier-pike",
            "frontier-pyramid",
            "frontier-dome",
            "frontier-tower",
            "save-load",
            "team-lab",
        }
        self.assertEqual(set(result["suites"]), expected)

    def test_unknown_source_change_selects_every_suite(self) -> None:
        result = self.select("src/unclassified_gameplay.c")
        self.assertEqual(result["suites"], self.all_suites)
        self.assertTrue(result["full"])

    def test_unknown_root_file_selects_every_suite(self) -> None:
        result = self.select("configure.ac")
        self.assertEqual(result["suites"], self.all_suites)

    def test_test_infrastructure_selects_every_suite(self) -> None:
        result = self.select("tools/testing/select_suites.py")
        self.assertEqual(result["suites"], self.all_suites)

    @mock.patch("select_suites.subprocess.run")
    def test_first_push_sentinel_selects_every_suite_without_diff(self, run: mock.Mock) -> None:
        files = changed_files("0" * 40, "1" * 40)
        run.assert_not_called()
        result = select_suites(files, self.manifest)
        self.assertEqual(result["suites"], self.all_suites)
        self.assertTrue(result["full"])

    def test_invalid_dependency_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        manifest["suites"]["frontier-common"]["dependencies"].append("missing")
        with self.assertRaises(ManifestError):
            validate_manifest(manifest)

    def test_dependency_cycle_is_rejected(self) -> None:
        manifest = copy.deepcopy(self.manifest)
        manifest["suites"]["save-load"]["dependencies"].append("frontier-common")
        with self.assertRaises(ManifestError):
            validate_manifest(manifest)

    def test_tower_change_selects_all_e2e_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/battle_tower.c"], self.e2e_manifest)
        self.assertEqual(
            result["scenarios"],
            [
                "arena-hard-greta",
                "arena-normal-greta",
                "dome-hard-tucker",
                "dome-normal-tucker",
                "factory-hard-noland",
                "factory-hard-setup",
                "factory-normal-noland",
                "frontier-intro-team-lab",
                "palace-hard-spenser",
                "palace-normal-spenser",
                "pike-hard-lucy",
                "pike-normal-lucy",
                "pokenav-team-lab-access",
                "pyramid-hard-brandon",
                "pyramid-normal-brandon",
                "team-lab-create-mon",
                "team-lab-edit-stats",
                "tower-hard-anabel",
                "tower-normal-anabel",
            ],
        )

    def test_arena_change_selects_paired_greta_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/battle_arena.c"], self.e2e_manifest)
        self.assertEqual(
            result["scenarios"], ["arena-hard-greta", "arena-normal-greta"]
        )
        self.assertEqual(
            result["matrix"],
            {"scenario": ["arena-hard-greta", "arena-normal-greta"]},
        )

    def test_dome_change_selects_paired_tucker_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/battle_dome.c"], self.e2e_manifest)
        self.assertEqual(
            result["scenarios"], ["dome-hard-tucker", "dome-normal-tucker"]
        )
        self.assertEqual(
            result["matrix"],
            {"scenario": ["dome-hard-tucker", "dome-normal-tucker"]},
        )

    def test_save_change_conservatively_selects_all_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/save.c"], self.e2e_manifest)
        self.assertEqual(result["scenarios"], self.e2e_manifest["scenarios"])

    def test_palace_change_selects_paired_spenser_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/battle_palace.c"], self.e2e_manifest)
        self.assertEqual(
            result["scenarios"], ["palace-hard-spenser", "palace-normal-spenser"]
        )
        self.assertEqual(
            result["matrix"],
            {"scenario": ["palace-hard-spenser", "palace-normal-spenser"]},
        )

    def test_factory_change_selects_factory_noland_e2e_scenarios(self) -> None:
        result = select_e2e_scenarios(
            ["src/battle_factory.c"], self.e2e_manifest
        )
        self.assertEqual(
            result["scenarios"],
            ["factory-hard-noland", "factory-hard-setup", "factory-normal-noland"],
        )
        self.assertEqual(
            result["matrix"],
            {"scenario": ["factory-hard-noland", "factory-hard-setup", "factory-normal-noland"]},
        )

    def test_pike_change_selects_paired_lucy_scenarios(self) -> None:
        result = select_e2e_scenarios(["src/battle_pike.c"], self.e2e_manifest)
        self.assertEqual(
            result["scenarios"], ["pike-hard-lucy", "pike-normal-lucy"]
        )
        self.assertEqual(
            result["matrix"],
            {"scenario": ["pike-hard-lucy", "pike-normal-lucy"]},
        )

    def test_documentation_change_selects_no_e2e_scenario(self) -> None:
        result = select_e2e_scenarios(["README.md"], self.e2e_manifest)
        self.assertEqual(result["scenarios"], [])

    def test_unknown_source_change_selects_all_e2e_scenarios(self) -> None:
        result = select_e2e_scenarios(
            ["src/unclassified_gameplay.c"], self.e2e_manifest
        )
        self.assertEqual(result["scenarios"], self.e2e_manifest["scenarios"])
        self.assertTrue(result["full"])

    def test_e2e_infrastructure_selects_all_scenarios(self) -> None:
        result = select_e2e_scenarios(
            ["tools/testing/e2e/session.py"], self.e2e_manifest
        )
        self.assertEqual(result["scenarios"], self.e2e_manifest["scenarios"])

    def test_e2e_manifest_rejects_unknown_scenario(self) -> None:
        manifest = copy.deepcopy(self.e2e_manifest)
        manifest["rules"][0]["scenarios"] = ["missing"]
        with self.assertRaisesRegex(ManifestError, "unknown scenarios"):
            validate_e2e_manifest(manifest)


if __name__ == "__main__":
    unittest.main()
