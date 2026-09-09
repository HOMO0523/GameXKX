"""One-shot host monitor: poll Travel bar percents and measure actual painted fills over many waves."""
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


def measure(arr, rect, red):
    if not rect or rect["w"] <= 0 or rect["h"] <= 0:
        return None
    x0 = max(0, int(round(rect["x"])))
    y0 = max(0, int(round(rect["y"])))
    x1 = min(arr.shape[1], x0 + max(1, int(round(rect["w"]))))
    y1 = min(arr.shape[0], y0 + max(1, int(round(rect["h"]))))
    crop = arr[y0:y1, x0:x1].astype(int)
    r, g, b = crop[..., 0], crop[..., 1], crop[..., 2]
    mask = (r > 120) & (g < 115) & (b < 115) if red else (g > 130) & (r < 135) & (b < 135)
    return round(float(mask.mean()), 4)


def main() -> None:
    client = UnrealMCPClient(timeout=120)
    client.require_connected()
    controller = PreviewWindowController()
    window = controller.find_preview_window()
    rows = []
    for index in range(60):
        response = client.run_project_python_file("Content/Python/_tmp_bar_state2.py")
        state = json.loads(response["stdout"])
        data, _size = controller.capture_window_png(window)
        arr = np.asarray(Image.open(io.BytesIO(data)).convert("RGB"))
        fills = {}
        for name, rect in state["rects"].items():
            fills[name] = measure(arr, rect, red=name.startswith("enemy"))
        row = {
            "t": index,
            "logical": state["logical_phase"].split(":")[0].rsplit(".", 1)[-1],
            "visual": state["visual_phase"],
            "enemy_hps": [(e["id"], e["hp"], e["max"]) for e in state["enemies"]],
            "party_hps": state["party"],
            "percents": state["bar_percents"],
            "fills": fills,
        }
        rows.append(row)
        print(json.dumps(row, ensure_ascii=False), flush=True)
        if index < 59:
            time.sleep(2.0)
    divergences = []
    for row in rows:
        for name in ("hero", "comp0", "comp1", "enemy0", "enemy1", "enemy2"):
            percent = row["percents"].get(name)
            fill = row["fills"].get(name)
            if percent is None or fill is None:
                continue
            if percent >= 0.08 and fill <= 0.04:
                divergences.append({"t": row["t"], "name": name, "percent": percent, "fill": fill, "visual": row["visual"]})
            elif abs(percent - fill) > 0.18:
                divergences.append({"t": row["t"], "name": name, "percent": percent, "fill": fill, "visual": row["visual"]})
    print(json.dumps({"divergences": divergences}, ensure_ascii=False))


if __name__ == "__main__":
    main()
