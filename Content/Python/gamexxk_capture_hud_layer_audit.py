"""One-shot Editor-only HUD layer capture. Creates no native overlay window."""

from __future__ import annotations

import json

import unreal


result_text = unreal.GameXXKEditorCaptureAutomationLibrary.capture_desktop_hud_layer_audit("")
result = json.loads(result_text)
unreal.log(f"[HUD_LAYER_AUDIT] {result_text}")
if not result.get("success", False):
    raise RuntimeError(result.get("error", "HUD layer audit failed"))

unreal.SystemLibrary.execute_console_command(None, "QUIT_EDITOR")
