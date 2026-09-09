"""One-shot host test: does a forced SetPercent repaint the visible bar now vs later."""
from __future__ import annotations

import io
import json
import sys
import time
from pathlib import Path

import numpy as np
from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from scripts.gamexxk_real_play_flow_mcp import PreviewWindowController  # noqa: E402
from scripts.ue_mcp_client import UnrealMCPClient  # noqa: E402


def measure_hero(client, controller, window):
    state_response = client.run_project_python_file("Content/Python/_tmp_bar_state2.py")
    state = json.loads(state_response["stdout"])
    rect = state["rects"]["hero"]
    data, _size = controller.capture_window_png(window)
    arr = np.asarray(Image.open(io.BytesIO(data)).convert("RGB")).astype(int)
    x0, y0 = int(round(rect["x"])), int(round(rect["y"]))
    x1, y1 = x0 + int(round(rect["w"])), y0 + int(round(rect["h"]))
    crop = arr[y0:y1, x0:x1]
    r, g, b = crop[..., 0], crop[..., 1], crop[..., 2]
    mask = (g > 130) & (r < 135) & (b < 135)
    return {
        "percent": state["bar_percents"]["hero"],
        "fill": round(float(mask.mean()), 4),
        "visual": state["visual_phase"],
        "logical": state["logical_phase"].split(":")[0].rsplit(".", 1)[-1],
    }


def main() -> None:
    client = UnrealMCPClient(timeout=120)
    client.require_connected()
    controller = PreviewWindowController()
    window = controller.find_preview_window()
    rows = []
    for target, wait in ((0.25, 1.0), (0.9, 1.0), (0.1, 75.0), (0.65, 1.0)):
        response = client.run_project_python_file("Content/Python/_tmp_set_hero_bar.py", [str(target)])
        rows.append({"set": target, "setter": json.loads(response["stdout"])})
        time.sleep(wait)
        rows[-1]["measured"] = measure_hero(client, controller, window)
        print(json.dumps(rows[-1], ensure_ascii=False), flush=True)
    print(json.dumps({"rows": rows}, ensure_ascii=False))


if __name__ == "__main__":
    main()
