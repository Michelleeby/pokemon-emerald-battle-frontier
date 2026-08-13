#!/usr/bin/env python3

from __future__ import annotations

import unittest

from e2e.symbols import SymbolError, require_symbols


class SymbolTests(unittest.TestCase):
    def test_require_symbols_returns_requested_subset(self) -> None:
        self.assertEqual(require_symbols({"a": 1, "b": 2}, "b"), {"b": 2})

    def test_require_symbols_reports_all_missing_names(self) -> None:
        with self.assertRaisesRegex(SymbolError, "missing ELF symbols: a, c"):
            require_symbols({"b": 2}, "a", "c")


if __name__ == "__main__":
    unittest.main()
