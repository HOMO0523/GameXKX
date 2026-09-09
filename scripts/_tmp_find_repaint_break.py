"""One-shot host experiment: find when Slate repaint breaks relative to rebuild count."""
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


def hero_fill(controller, window, rect):
    data, _size = controller.capture_window_png(window)
    arr = np.asarray(Image.open(io.BytesIO(data)).convert("RGB")).astype(int)
    x0, y0 = int(round(rect["x"])), int(round(rect["y"]))
    x1, y1 = x0 + int(round(rect["w"])), y0 + int(round(rect["h"]))
    crop = arr[y0:y1, x0:x1]
    r, g, b = crop[..., 0], crop[..., 1], crop[..., 2]
    mask = (g > 130) & (r < 135) & (b < 135)
    return round(float(mask.mean()), 4)


def main() -> None:
    client = UnrealMCPClient(timeout=120)
    client.require_connected()
    controller = PreviewWindowController()
    window = controller.find_preview_window()
    rows = []
    for index in range(18):
        response = client.run_project_python_file("Content/Python/_tmp_diag_state.py")
        state = json.loads(response["stdout"])
        fill = hero_fill(controller, window, state["hero_rect"])
        state["fill"] = fill
        rows.append(state)
        print(json.dumps(state, ensure_ascii=False), flush=True)
        time.sleep(5.0)
    response = client.run_project_python_file("Content/Python/_tmp_set_hero_bar.py", ["0.3"])
    setter = json.loads(response["stdout"])
    time.sleep(0.6)
    state_response = client.run_project_python_file("Content/Python/_tmp_diag_state.py")
    state = json.loads(state_response["stdout"])
    state["fill"] = hero_fill(controller, window, state["hero_rect"])
    print(json.dumps({"forced": setter, "after_forced": state}, ensure_ascii=False), flush=True)
    print(json.dumps({"rows": rows}, ensure_ascii=False))


if __name__ == "__main__":
    main()
