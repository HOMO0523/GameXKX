#!/usr/bin/env python3
"""Regression contract: desktop HUD must not install a global mouse hook."""

from __future__ import annotations

import unittest
from pathlib import Path


PROJECT_ROOT = Path(__file__).resolve().parents[1]
WORKBENCH_CPP = (
    PROJECT_ROOT
    / "Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp"
)


class DesktopMouseHookPolicyTest(unittest.TestCase):
    def test_global_low_level_mouse_hook_is_absent(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")

        for forbidden in (
            "WH_MOUSE_LL",
            "SetWindowsHookExW",
            "UnhookWindowsHookEx",
            "GameXXKDesktopLowLevelMouseProc",
            "GetDesktopLowLevelMouseHook",
            "InstallDesktopLowLevelMouseHook",
            "ReleaseDesktopLowLevelMouseHookIfUnused",
        ):
            self.assertNotIn(forbidden, source)

    def test_drag_update_uses_immutable_screen_delta(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        drag_body = source.split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "UpdateDesktopOverlayAnchorFromPointer",
            1,
        )[1].split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::"
            "UpdateExpansionDirectionFromNativeWindow",
            1,
        )[0]

        for forbidden in (
            "HostGeometry.AbsoluteToLocal",
            "GetWindowRect",
            "DesktopHudDragPointerOffset",
        ):
            self.assertNotIn(forbidden, drag_body)

        self.assertIn("ResolveDesktopHudDragAnchor(", drag_body)
        self.assertIn("DesktopHudDragStartPointerScreen", drag_body)
        self.assertIn("DesktopHudDragStartNormalizedAnchor", drag_body)

    def test_native_window_tick_owns_passthrough_refresh(self) -> None:
        source = WORKBENCH_CPP.read_text(encoding="utf-8")
        tick_body = source.split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::TickDesktopNativeWindow()",
            1,
        )[1].split(
            "void UGameXXKDesktopTrainingWorkbenchWidget::ReleaseDesktopNativeWindow()",
            1,
        )[0]

        self.assertIn("RefreshDesktopNativeMousePassthrough();", tick_body)


if __name__ == "__main__":
    unittest.main()
