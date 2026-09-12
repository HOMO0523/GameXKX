"""Font-rollout probe: audit every visible TextBlock's font on the live desktop surface.

Phases (run through UE MCP run_project_python_file):
    --phase prepare-map                 load /Game/GameXXK/Maps/L_DesktopTrainingHUD
    --phase audit [--action N] [--capture NAME] [--open-backpack]
    --phase stop                        stop nothing; kept for symmetry with other probes

The audit calls UGameXXKLocalizationLibrary::AuditTextLayout on the workbench
widget, aggregates the font asset paths, and writes the raw report to
Saved/Diagnostics/FontRollout/<label>.json.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

import unreal


HUD_MAP = "/Game/GameXXK/Maps/L_DesktopTrainingHUD"
OUT_DIR = Path(unreal.Paths.project_saved_dir()) / "Diagnostics" / "FontRollout"
TITLE_FONT_PATH = (
    "/Game/GameXXK/UI/Fonts/Trial/FF_Trial_ZhHans_JiangHuGuFeng_Font."
    "FF_Trial_ZhHans_JiangHuGuFeng_Font"
)
BODY_FONT_PATH = (
    "/Game/GameXXK/UI/Fonts/Body/FF_Body_KeinannMaruPOP_Font."
    "FF_Body_KeinannMaruPOP_Font"
)


def emit(payload: dict) -> dict:
    print(json.dumps(payload, ensure_ascii=False, sort_keys=True))
    return payload


def call(obj, name, *args):
    fn = getattr(obj, name, None) if obj is not None else None
    if not callable(fn):
        return None
    try:
        return fn(*args)
    except Exception as exc:  # pragma: no cover - runs inside UE Python
        return f"ERR:{exc}"


def world():
    editor = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
    return editor.get_game_world() if editor else None


def controller_and_widget():
    live = world()
    if not live:
        return None, None, None
    controller = unreal.GameplayStatics.get_player_controller(live, 0)
    widget = call(controller, "get_desktop_training_workbench_widget_for_test")
    if widget is None or isinstance(widget, str):
        call(controller, "set_desktop_training_workbench_enabled_for_test", True)
        widget = call(controller, "get_desktop_training_workbench_widget_for_test")
    return live, controller, widget


def prepare_map():
    if world():
        return emit({"ok": False, "phase": "prepare-map", "reason": "pie_is_running"})
    loaded = unreal.EditorLoadingAndSavingUtils.load_map(HUD_MAP)
    return emit({"ok": bool(loaded), "phase": "prepare-map", "map": HUD_MAP})


def audit(label: str, action: int | None, capture: str, open_backpack: bool):
    live, controller, widget = controller_and_widget()
    if widget is None or isinstance(widget, str):
        return emit({"ok": False, "phase": "audit", "reason": "workbench_missing", "detail": widget})
    call(controller, "set_desktop_training_workbench_enabled_for_test", True)
    if open_backpack:
        call(widget, "open_backpack")
    if action is not None:
        call(widget, "handle_desktop_action_for_test", int(action))
    raw = call(unreal.GameXXKLocalizationLibrary, "audit_text_layout", widget)
    if not isinstance(raw, str) or raw.startswith("{" ) is False:
        return emit({"ok": False, "phase": "audit", "reason": "audit_failed", "detail": str(raw)[:400]})
    report = json.loads(raw)
    texts = report.get("texts", [])

    by_font: dict[str, int] = {}
    foreign: list[dict] = []
    overflow: list[dict] = []
    for row in texts:
        font = row.get("font") or "None"
        by_font[font] = by_font.get(font, 0) + 1
        if font not in (TITLE_FONT_PATH, BODY_FONT_PATH):
            foreign.append({"widget": row.get("widget"), "text": row.get("text"), "font": font})
        if row.get("potentialOverflow"):
            overflow.append(
                {
                    "widget": row.get("widget"),
                    "text": row.get("text"),
                    "font": font,
                    "naturalWidth": row.get("naturalWidth"),
                    "availableWidth": row.get("availableWidth"),
                }
            )

    OUT_DIR.mkdir(parents=True, exist_ok=True)
    raw_path = OUT_DIR / f"{label}.json"
    raw_path.write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding="utf-8")

    payload = {
        "ok": True,
        "phase": "audit",
        "label": label,
        "language": report.get("language"),
        "applicationScale": report.get("applicationScale"),
        "textBlockCount": len(texts),
        "titleFontCount": by_font.get(TITLE_FONT_PATH, 0),
        "bodyFontCount": by_font.get(BODY_FONT_PATH, 0),
        "fontHistogram": by_font,
        "foreignFonts": foreign[:40],
        "foreignFontCount": len(foreign),
        "overflowCount": len(overflow),
        "overflow": overflow[:20],
        "rawReport": str(raw_path),
    }
    if capture:
        # unreal.AutomationLibrary.take_high_res_screenshot writes a blank frame
        # from this automation path; the console HighResShot is what produced the
        # project's earlier real screenshots. It is asynchronous, so the caller
        # picks up the newest PNG under the screenshots directory afterwards.
        shots_dir = Path(unreal.Paths.project_saved_dir()) / "Screenshots" / "WindowsEditor"
        before = {p.name for p in shots_dir.glob("*.png")} if shots_dir.is_dir() else set()
        unreal.SystemLibrary.execute_console_command(live, "HighResShot 1600x900")
        payload["captureRequested"] = True
        payload["captureDir"] = str(shots_dir)
        payload["captureExisting"] = sorted(before)
    return emit(payload)


def measure(text: str, size: int):
    """Compare the two role fonts' natural width for one string (local Slate units)."""
    service = unreal.SlateApplication.get_platform_application()
    measure_service = None
    try:
        measure_service = unreal.FSlateApplication.get().get_renderer().get_font_measure_service()
    except Exception:  # pragma: no cover - older API shape
        measure_service = None
    if measure_service is None:
        return emit({"ok": False, "phase": "measure", "reason": "no_font_measure_service"})
    rows = {}
    for label, path in (("title", TITLE_FONT_PATH), ("body", BODY_FONT_PATH)):
        font_asset = unreal.EditorAssetLibrary.load_asset(path)
        if font_asset is None:
            rows[label] = None
            continue
        info = unreal.SlateFontInfo(font_asset, int(size), unreal.Name("Default"))
        width = measure_service.measure(unreal.Text.from_string(text), info).x
        rows[label] = {"width": float(width), "perChar": float(width) / max(1, len(text))}
    return emit(
        {
            "ok": True,
            "phase": "measure",
            "text": text,
            "size": int(size),
            "characters": len(text),
            "rows": rows,
        }
    )


def capture_hud(label: str):
    """Offscreen desktop-HUD layer capture (project capture library, no PIE needed)."""
    out_dir = OUT_DIR / label
    out_dir.mkdir(parents=True, exist_ok=True)
    raw = unreal.GameXXKEditorCaptureAutomationLibrary.capture_desktop_hud_layer_audit(
        str(out_dir)
    )
    payload = json.loads(raw) if isinstance(raw, str) and raw.startswith("{") else {"raw": str(raw)[:400]}
    payload["phase"] = "capture-hud"
    payload["outputDirectory"] = str(out_dir)
    return emit(payload)


def main(argv=None):
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", choices=("prepare-map", "audit", "measure", "capture-hud"), required=True)
    parser.add_argument("--label", default="workbench")
    parser.add_argument("--action", type=int, default=None)
    parser.add_argument("--capture", default="")
    parser.add_argument("--open-backpack", action="store_true")
    parser.add_argument("--text", default="")
    parser.add_argument("--size", type=int, default=20)
    args = parser.parse_args(argv)

    if args.phase == "prepare-map":
        return prepare_map()
    if args.phase == "measure":
        return measure(args.text, args.size)
    if args.phase == "capture-hud":
        return capture_hud(args.label)
    return audit(args.label, args.action, args.capture, args.open_backpack)


if __name__ == "__main__":
    main()
