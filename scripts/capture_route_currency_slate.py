"""Capture the actual desktop overlay and preview surfaces without guessing which is visible."""
import argparse
import re
from pathlib import Path
from ue_mcp_client import UnrealMCPClient
from gamexxk_real_play_flow_mcp import _decode_slate_screenshot_png, SLATE_TOOLSET

parser = argparse.ArgumentParser()
parser.add_argument("stem")
args = parser.parse_args()
assert re.fullmatch(r"[a-z0-9-]+", args.stem)
root = Path(__file__).resolve().parents[1] / "Saved/RouteNodeArt"
client = UnrealMCPClient(timeout=30)
assert client.connect()
snapshot = str(client.call_tool("Snapshot", {"ref": "", "maxDepth": 2, "bIncludeSourceLocations": False}, toolset_name=SLATE_TOOLSET))
written = []
for line in snapshot.splitlines():
    if not line.startswith("window ") or not any(name in line for name in ("GameXXK Preview", "GameXXKDesktopOverlay")):
        continue
    match = re.search(r"\[ref=([^\]]+)\]", line)
    if not match: continue
    ref = match.group(1)
    result = client.call_tool("Screenshot", {"ref": ref}, toolset_name=SLATE_TOOLSET)
    try:
        png = _decode_slate_screenshot_png(result)
    except RuntimeError:
        print({"ref": ref, "response": result})
        continue
    path = root / f"{args.stem}-{ref}.png"
    path.write_bytes(png)
    written.append(str(path))
print({"written": written})
assert written, "No game surface returned a screenshot"
