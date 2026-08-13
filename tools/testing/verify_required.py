#!/usr/bin/env python3
"""Verify stable Tests / required inputs from GitHub job results."""

from __future__ import annotations

import os
import sys


class RequiredGateError(ValueError):
    pass


def verify(results: dict[str, str]) -> None:
    if results["select"] != "success":
        raise RequiredGateError(
            f"suite selection failed with result: {results['select']}"
        )

    if results["has_suites"] == "true":
        expected = {"build": "success", "run": "success", "no_suites": "skipped"}
    elif results["has_suites"] == "false":
        expected = {"build": "skipped", "run": "skipped", "no_suites": "success"}
    else:
        raise RequiredGateError(
            f"selector emitted invalid has_suites value: {results['has_suites']}"
        )

    if results["has_e2e"] == "true":
        expected.update({"e2e": "success", "no_e2e": "skipped"})
    elif results["has_e2e"] == "false":
        expected.update({"e2e": "skipped", "no_e2e": "success"})
    else:
        raise RequiredGateError(
            f"selector emitted invalid has_e2e_scenarios value: {results['has_e2e']}"
        )

    expected["cleanup"] = "success"
    mismatches = [
        f"{name}={results[name]} (expected {value})"
        for name, value in expected.items()
        if results[name] != value
    ]
    if mismatches:
        raise RequiredGateError("required test result mismatch: " + ", ".join(mismatches))


def main() -> int:
    results = {
        "select": os.environ.get("SELECT_RESULT", ""),
        "build": os.environ.get("BUILD_RESULT", ""),
        "run": os.environ.get("RUN_RESULT", ""),
        "no_suites": os.environ.get("NO_SUITES_RESULT", ""),
        "cleanup": os.environ.get("CLEANUP_RESULT", ""),
        "e2e": os.environ.get("E2E_RESULT", ""),
        "no_e2e": os.environ.get("NO_E2E_RESULT", ""),
        "has_suites": os.environ.get("HAS_SUITES", ""),
        "has_e2e": os.environ.get("HAS_E2E_SCENARIOS", ""),
    }
    try:
        verify(results)
    except RequiredGateError as error:
        print(error, file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
