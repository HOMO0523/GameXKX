#!/usr/bin/env python3
"""Regression coverage for UE fixture result normalization."""

from __future__ import annotations

import importlib.util
import sys
import types
import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
PROBE_PATH = PROJECT_ROOT / "Content" / "Python" / "gamexxk_probe_real_play_flow.py"


def _load_probe_module():
    module_name = "_gamexxk_fixture_apply_result_test"
    original_unreal = sys.modules.get("unreal")
    sys.modules["unreal"] = types.ModuleType("unreal")
    try:
        spec = importlib.util.spec_from_file_location(module_name, PROBE_PATH)
        if spec is None or spec.loader is None:
            raise RuntimeError("Cannot load real play-flow probe for its fixture helper")
        module = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(module)
        return module
    finally:
        if original_unreal is None:
            sys.modules.pop("unreal", None)
        else:
            sys.modules["unreal"] = original_unreal


class FixtureApplyResultTest(unittest.TestCase):
    def test_treats_single_empty_out_string_as_success(self) -> None:
        module = _load_probe_module()

        self.assertEqual((True, ""), module._fixture_apply_result(""))
        self.assertEqual((True, "detail"), module._fixture_apply_result("detail"))
        self.assertEqual((False, ""), module._fixture_apply_result(None))
        self.assertEqual((True, ""), module._fixture_apply_result((True, "")))
        self.assertEqual((False, "rejected"), module._fixture_apply_result((False, "rejected")))


if __name__ == "__main__":
    unittest.main()
