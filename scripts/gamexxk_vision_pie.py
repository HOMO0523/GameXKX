#!/usr/bin/env python3
"""PIE live screenshot -> DeepSeek vision flow through the UE 5.8 MCP.

Captures the current GameXXK Preview Slate window via the built-in
``SlateInspectorToolset.SlateInspectorToolset`` Screenshot tool, saves the PNG
under ``Saved/Codex``, then sends it to ``deepseek-v4-flash-vision-exp`` and
persists an evidence report under ``Saved/HarnessReports``.

The canonical surface follows the project default: with PIE already running on
``/Game/GameXXK/Maps/L_DesktopTrainingHUD`` this script only observes, it never
touches gameplay state. ``--start-pie`` starts PIE when it is not running.

Requires: ``DEEPSEEK_API_KEY`` (or ``--api-key``), plus a running UE editor
with MCP (port 18765) or ``--start-pie`` against an already-open editor.

Examples:
    python scripts/gamexxk_vision_pie.py --capture-only
    python scripts/gamexxk_vision_pie.py --ocr --detail low
    python scripts/gamexxk_vision_pie.py --prompt "检查历练条是否被遮挡"
"""

from __future__ import annotations

import argparse
import json
import sys
import time
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

PROJECT_ROOT = Path(__file__).resolve().parents[1]
SCRIPT_DIR = PROJECT_ROOT / "scripts"
if str(SCRIPT_DIR) not in sys.path:
    sys.path.insert(0, str(SCRIPT_DIR))

from gamexxk_real_play_flow_mcp import (  # noqa: E402
    SLATE_TOOLSET,
    _decode_slate_screenshot_png,
    _png_size,
    _slate_preview_window_ref,
)
from gamexxk_vision import (  # noqa: E402
    DEFAULT_MODEL,
    DeepSeekVisionClient,
    DeepSeekVisionError,
    VisionResult,
)
from ue_mcp_client import (  # noqa: E402
    DEFAULT_HOST,
    DEFAULT_PATH,
    DEFAULT_PORT,
    UnrealMCPClient,
)

SCREENSHOT_DIR = PROJECT_ROOT / "Saved" / "Codex"
REPORT_DIR = PROJECT_ROOT / "Saved" / "HarnessReports"

DEFAULT_PIE_PROMPT = (
    "请检查这张游戏运行截图：先简述画面内容与 HUD 布局，"
    "再指出明显异常（元素缺失、错位、裁切、遮挡、文字乱码、过亮过暗等）。"
    "如果画面看起来正常，请明确说明没有发现异常。"
)
DEFAULT_OCR_PROMPT = "识别这张游戏截图里的全部文字，按画面中的顺序输出。"


def _atomic_write_bytes(path: Path, data: bytes) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_name(f"{path.name}.{time.time_ns()}.tmp")
    try:
        temporary.write_bytes(data)
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def _atomic_write_text(path: Path, text: str) -> None:
    _atomic_write_bytes(path, text.encode("utf-8"))


def find_preview_ref(client: UnrealMCPClient, timeout: float = 15.0, interval: float = 0.10) -> str:
    """Return the Slate ref of the GameXXK PIE Preview window."""
    deadline = time.monotonic() + max(0.0, timeout)
    while True:
        snapshot = str(
            client.call_tool(
                "Snapshot",
                {"ref": "", "maxDepth": 3, "bIncludeSourceLocations": False},
                toolset_name=SLATE_TOOLSET,
                timeout=client.timeout,
            )
        )
        preview_ref = _slate_preview_window_ref(snapshot)
        if preview_ref:
            return preview_ref
        remaining = deadline - time.monotonic()
        if remaining <= 0.0:
            raise RuntimeError(
                "GameXXK Preview Slate window was not found. Is PIE running on this editor? "
                f"Last snapshot prefix: {snapshot[:500]}"
            )
        time.sleep(min(interval, remaining))


def capture_pie_screenshot(client: UnrealMCPClient, name: str) -> tuple[Path, tuple[int, int]]:
    """Capture the current PIE Slate window and persist it under Saved/Codex."""
    preview_ref = find_preview_ref(client)
    payload = client.call_tool(
        "Screenshot",
        {"ref": preview_ref},
        toolset_name=SLATE_TOOLSET,
        timeout=client.timeout,
    )
    data = _decode_slate_screenshot_png(payload)
    size = _png_size(data)
    if size[0] <= 0 or size[1] <= 0:
        raise RuntimeError(f"Slate screenshot returned invalid size: {size}")
    path = SCREENSHOT_DIR / name
    _atomic_write_bytes(path, data)
    return path, size


def build_vision_report(
    image_path: Path,
    size: tuple[int, int],
    result: VisionResult,
    *,
    prompt: str,
    detail: str | None,
) -> dict[str, Any]:
    return {
        "ok": True,
        "captured_at_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "image": str(image_path),
        "image_size": list(size),
        "model": result.model,
        "finish_reason": result.finish_reason,
        "usage": dict(result.usage),
        "detail": detail,
        "prompt": prompt,
        "analysis": result.content,
    }


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--host", default=DEFAULT_HOST)
    parser.add_argument("--port", type=int, default=DEFAULT_PORT)
    parser.add_argument("--path", default=DEFAULT_PATH)
    parser.add_argument("--timeout", type=float, default=60.0, help="MCP tool timeout in seconds")
    parser.add_argument("--connect-timeout", type=float, default=10.0, help="MCP connect retry window in seconds")
    parser.add_argument("--start-pie", action="store_true", help="start PIE before capturing (does not stop it)")
    parser.add_argument("--warmup", type=float, default=2.0, help="PIE warmup seconds when --start-pie")
    parser.add_argument("--settle", type=float, default=0.0, help="extra settle seconds between capture and analysis")
    parser.add_argument("--name", default=None, help="screenshot filename (default: timestamped vision_pie_*.png)")
    parser.add_argument("--prompt", default=None, help=f"override prompt (default: built-in check prompt)")
    parser.add_argument("--ocr", action="store_true", help="use the OCR prompt instead of the visual-check prompt")
    parser.add_argument("--capture-only", action="store_true", help="capture the PNG without calling the vision API")
    parser.add_argument("--detail", choices=("low", "high", "original", "auto"), default=None)
    parser.add_argument("--model", default=DEFAULT_MODEL)
    parser.add_argument("--base-url", default="https://api.deepseek.com")
    parser.add_argument("--api-key", default=None, help="defaults to $DEEPSEEK_API_KEY")
    parser.add_argument("--max-tokens", type=int, default=None)
    parser.add_argument("--output", default=None, help="report path (default: Saved/HarnessReports/gamexxk_vision_pie_*.json)")
    parser.add_argument("--json", action="store_true", help="print the full report as JSON")
    return parser


def main(argv: list[str] | None = None) -> int:
    parser = build_parser()
    args = parser.parse_args(argv)
    if hasattr(sys.stdout, "reconfigure"):
        sys.stdout.reconfigure(encoding="utf-8")
    if hasattr(sys.stderr, "reconfigure"):
        sys.stderr.reconfigure(encoding="utf-8")

    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    image_name = args.name or f"vision_pie_{stamp}.png"
    report_path = Path(args.output) if args.output else REPORT_DIR / f"gamexxk_vision_pie_{stamp}.json"

    try:
        client = UnrealMCPClient(host=args.host, port=args.port, path=args.path, timeout=args.timeout)
        deadline = time.monotonic() + max(0.0, args.connect_timeout)
        while not client.connect():
            if time.monotonic() >= deadline:
                raise RuntimeError(f"cannot connect to UE MCP at {client.endpoint}")
            time.sleep(1.0)

        if args.start_pie:
            client.start_pie(warmup_seconds=args.warmup)
            if not client.wait_for_pie_state(True, timeout=60.0, interval=0.5):
                raise RuntimeError("PIE did not reach the running state after start_pie")
        elif not client.is_in_pie():
            raise RuntimeError(
                "PIE is not running. Start PIE in the editor or pass --start-pie "
                "(the editor must be open with MCP enabled)."
            )

        if args.settle > 0:
            time.sleep(args.settle)

        image_path, size = capture_pie_screenshot(client, image_name)
        if args.capture_only:
            report = {
                "ok": True,
                "captured_at_utc": datetime.now(timezone.utc).isoformat(timespec="seconds"),
                "image": str(image_path),
                "image_size": list(size),
                "analysis": None,
            }
            _atomic_write_text(report_path, json.dumps(report, ensure_ascii=False, indent=2) + "\n")
            print(json.dumps(report, ensure_ascii=False, indent=2))
            return 0

        prompt = args.prompt or (DEFAULT_OCR_PROMPT if args.ocr else DEFAULT_PIE_PROMPT)
        vision_client = DeepSeekVisionClient(
            api_key=args.api_key,
            base_url=args.base_url,
            model=args.model,
        )
        result = vision_client.analyze(
            prompt,
            images=[image_path],
            detail=args.detail,
            max_tokens=args.max_tokens,
        )
        report = build_vision_report(image_path, size, result, prompt=prompt, detail=args.detail)
        _atomic_write_text(report_path, json.dumps(report, ensure_ascii=False, indent=2) + "\n")
        if args.json:
            print(json.dumps(report, ensure_ascii=False, indent=2))
        else:
            print(result.content)
        return 0
    except (DeepSeekVisionError, RuntimeError, OSError, ValueError) as exc:
        error_report = {"ok": False, "error": str(exc)}
        if args.json:
            print(json.dumps(error_report, ensure_ascii=False))
        else:
            print(f"error: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
