#!/usr/bin/env python3

from __future__ import annotations

import unittest

from verify_required import RequiredGateError, verify


def passing_results() -> dict[str, str]:
    return {
        "select": "success",
        "build": "success",
        "run": "success",
        "no_suites": "skipped",
        "cleanup": "success",
        "e2e": "success",
        "no_e2e": "skipped",
        "has_suites": "true",
        "has_e2e": "true",
    }


class RequiredGateTests(unittest.TestCase):
    def test_selected_suites_and_e2e_pass(self) -> None:
        verify(passing_results())

    def test_no_suites_and_no_e2e_pass(self) -> None:
        results = passing_results()
        results.update(
            {
                "build": "skipped",
                "run": "skipped",
                "no_suites": "success",
                "e2e": "skipped",
                "no_e2e": "success",
                "has_suites": "false",
                "has_e2e": "false",
            }
        )
        verify(results)

    def test_e2e_failure_fails(self) -> None:
        results = passing_results()
        results["e2e"] = "failure"
        with self.assertRaises(RequiredGateError):
            verify(results)

    def test_e2e_timeout_or_cancellation_fails(self) -> None:
        for result in ("cancelled", "timed_out"):
            with self.subTest(result=result):
                results = passing_results()
                results["e2e"] = result
                with self.assertRaises(RequiredGateError):
                    verify(results)

    def test_missing_e2e_result_fails(self) -> None:
        results = passing_results()
        results["e2e"] = "skipped"
        with self.assertRaises(RequiredGateError):
            verify(results)

    def test_cleanup_failure_fails(self) -> None:
        results = passing_results()
        results["cleanup"] = "failure"
        with self.assertRaises(RequiredGateError):
            verify(results)


if __name__ == "__main__":
    unittest.main()
