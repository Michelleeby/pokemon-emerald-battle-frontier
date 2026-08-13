#!/usr/bin/env python3
"""Validate tests/manifest.json without selecting suites."""

from __future__ import annotations

import argparse
from pathlib import Path
import sys

from select_suites import DEFAULT_MANIFEST, ManifestError, load_manifest


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("manifest", nargs="?", type=Path, default=DEFAULT_MANIFEST)
    args = parser.parse_args()
    try:
        manifest = load_manifest(args.manifest)
    except ManifestError as error:
        parser.error(str(error))
    print(f"{args.manifest}: valid ({len(manifest['suites'])} suites, {len(manifest['rules'])} rules)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
