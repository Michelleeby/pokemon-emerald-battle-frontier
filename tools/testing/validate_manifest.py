#!/usr/bin/env python3
"""Validate the C-suite and E2E selection manifests."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from select_suites import (
    DEFAULT_E2E_MANIFEST,
    DEFAULT_MANIFEST,
    ManifestError,
    load_e2e_manifest,
    load_manifest,
)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?", type=Path, default=DEFAULT_MANIFEST)
    parser.add_argument(
        "--e2e-manifest", type=Path, default=DEFAULT_E2E_MANIFEST
    )
    args = parser.parse_args()
    try:
        manifest = load_manifest(args.manifest)
        e2e_manifest = load_e2e_manifest(args.e2e_manifest)
    except ManifestError as error:
        parser.error(str(error))
    print(f"{args.manifest}: valid ({len(manifest['suites'])} suites, {len(manifest['rules'])} rules)")
    print(
        f"{args.e2e_manifest}: valid "
        f"({len(e2e_manifest['scenarios'])} scenarios, "
        f"{len(e2e_manifest['rules'])} rules)"
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
