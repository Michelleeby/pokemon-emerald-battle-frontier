#!/usr/bin/env python3
"""Select test suites conservatively from a Git diff or explicit file list."""

from __future__ import annotations

import argparse
import json
from pathlib import Path, PurePosixPath
import subprocess
import sys
from typing import Any


ROOT = Path(__file__).resolve().parents[2]
DEFAULT_MANIFEST = ROOT / "tests" / "manifest.json"


class ManifestError(ValueError):
    pass


def matches(path: str, patterns: list[str]) -> bool:
    candidate = PurePosixPath(path)
    return any(candidate.match(pattern) for pattern in patterns)


def load_manifest(path: Path = DEFAULT_MANIFEST) -> dict[str, Any]:
    try:
        manifest = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError) as error:
        raise ManifestError(f"cannot read {path}: {error}") from error
    validate_manifest(manifest)
    return manifest


def validate_manifest(manifest: dict[str, Any]) -> None:
    if manifest.get("version") != 1:
        raise ManifestError("manifest version must be 1")
    suites = manifest.get("suites")
    if not isinstance(suites, dict) or not suites:
        raise ManifestError("suites must be a non-empty object")
    suite_names = set(suites)
    for name, definition in suites.items():
        if not isinstance(name, str) or not name:
            raise ManifestError("suite names must be non-empty strings")
        dependencies = definition.get("dependencies") if isinstance(definition, dict) else None
        if not isinstance(dependencies, list) or not all(isinstance(item, str) for item in dependencies):
            raise ManifestError(f"suite {name!r} dependencies must be a string array")
        unknown = set(dependencies) - suite_names
        if unknown:
            raise ManifestError(f"suite {name!r} has unknown dependencies: {sorted(unknown)}")

    rules = manifest.get("rules")
    if not isinstance(rules, list) or not rules:
        raise ManifestError("rules must be a non-empty array")
    rule_names: set[str] = set()
    for rule in rules:
        if not isinstance(rule, dict) or not isinstance(rule.get("name"), str):
            raise ManifestError("every rule must have a string name")
        if rule["name"] in rule_names:
            raise ManifestError(f"duplicate rule name: {rule['name']}")
        rule_names.add(rule["name"])
        paths = rule.get("paths")
        if not isinstance(paths, list) or not paths or not all(isinstance(item, str) and item for item in paths):
            raise ManifestError(f"rule {rule['name']!r} paths must be a non-empty string array")
        selected = rule.get("suites")
        if selected != "all" and (not isinstance(selected, list) or set(selected) - suite_names):
            raise ManifestError(f"rule {rule['name']!r} refers to unknown suites")

    for key in ("relevant_paths", "ignored_paths"):
        value = manifest.get(key)
        if not isinstance(value, list) or not all(isinstance(item, str) and item for item in value):
            raise ManifestError(f"{key} must be a string array")

    def visit(name: str, visiting: set[str], visited: set[str]) -> None:
        if name in visiting:
            raise ManifestError(f"dependency cycle includes {name!r}")
        if name in visited:
            return
        visiting.add(name)
        for dependency in suites[name]["dependencies"]:
            visit(dependency, visiting, visited)
        visiting.remove(name)
        visited.add(name)

    visited: set[str] = set()
    for name in suites:
        visit(name, set(), visited)


def dependency_closure(selected: set[str], suites: dict[str, Any]) -> set[str]:
    pending = list(selected)
    while pending:
        name = pending.pop()
        for dependency in suites[name]["dependencies"]:
            if dependency not in selected:
                selected.add(dependency)
                pending.append(dependency)
    return selected


def select_suites(files: list[str], manifest: dict[str, Any]) -> dict[str, Any]:
    all_suites = set(manifest["suites"])
    selected: set[str] = set()
    full = False

    normalized = sorted({path.strip().replace("\\", "/").removeprefix("./") for path in files if path.strip()})
    for path in normalized:
        matched = False
        for rule in manifest["rules"]:
            if matches(path, rule["paths"]):
                matched = True
                suites = all_suites if rule["suites"] == "all" else set(rule["suites"])
                selected.update(suites)
                full = full or rule["suites"] == "all"
        if not matched and matches(path, manifest["relevant_paths"]):
            selected.update(all_suites)
            full = True
        elif not matched and not matches(path, manifest["ignored_paths"]):
            selected.update(all_suites)
            full = True

    selected = dependency_closure(selected, manifest["suites"])
    suites = sorted(selected)
    # GitHub rejects an empty dynamic matrix before a job-level condition can
    # reliably turn the job into an intentional skip.
    matrix_suites = suites if suites else ["no-suites"]
    return {"changed_files": normalized, "suites": suites, "matrix": {"suite": matrix_suites}, "full": full}


def changed_files(base: str, head: str) -> list[str]:
    # GitHub uses an all-zero `before` SHA when a push creates a branch. There
    # is no pre-push tree to diff in that case, so conservatively select every
    # suite through the manifest's test-infrastructure rule.
    if base and set(base) == {"0"}:
        return ["tests/manifest.json"]
    # Deleting owned code is just as test-relevant as adding or modifying it.
    command = ["git", "diff", "--name-only", "--diff-filter=ACMRD", f"{base}...{head}", "--"]
    completed = subprocess.run(command, cwd=ROOT, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE, check=False)
    if completed.returncode:
        raise RuntimeError(completed.stderr.strip() or "git diff failed")
    return completed.stdout.splitlines()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--manifest", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument("--base")
    parser.add_argument("--head", default="HEAD")
    parser.add_argument("--files", nargs="*")
    parser.add_argument("--github-output", type=Path)
    args = parser.parse_args()
    if args.files is None and not args.base:
        parser.error("provide --base or --files")
    try:
        manifest = load_manifest(args.manifest)
        files = args.files if args.files is not None else changed_files(args.base, args.head)
        result = select_suites(files, manifest)
    except (ManifestError, RuntimeError) as error:
        parser.error(str(error))

    print("Changed files:")
    for path in result["changed_files"]:
        print(f"  {path}")
    reasons_by_file: dict[str, list[str]] = {}
    # Re-run the small matching loop solely to keep the JSON output compact and the audit log explicit.
    for path in result["changed_files"]:
        names = [rule["name"] for rule in manifest["rules"] if matches(path, rule["paths"])]
        if not names and matches(path, manifest["relevant_paths"]):
            names = ["unclassified relevant path"]
        elif not names and not matches(path, manifest["ignored_paths"]):
            names = ["unclassified repository path"]
        reasons_by_file[path] = names
    print("Selection rules:")
    for path, names in reasons_by_file.items():
        print(f"  {path}: {', '.join(names) if names else 'ignored'}")
    print("Selected suites: " + (", ".join(result["suites"]) if result["suites"] else "none"))
    print(json.dumps(result, sort_keys=True))
    if args.github_output:
        with args.github_output.open("a", encoding="utf-8") as output:
            output.write(f"matrix={json.dumps(result['matrix'], separators=(',', ':'))}\n")
            output.write(f"suites={json.dumps(result['suites'], separators=(',', ':'))}\n")
            output.write(f"full={str(result['full']).lower()}\n")
            output.write(f"has_suites={str(bool(result['suites'])).lower()}\n")
    return 0


if __name__ == "__main__":
    sys.exit(main())
