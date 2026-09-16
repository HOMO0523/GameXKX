"""Capture only the two game paper panels, plus the UE audio mixer, for a sample cue."""
import argparse
import json
import subprocess
import time
from pathlib import Path

import imageio_ffmpeg
from ue_mcp_client import UnrealMCPClient

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / "Saved/Codex/ToolSfxRequirementSample-20260914"
SLATE = "SlateInspectorToolset.SlateInspectorToolset"


def main():
    p = argparse.ArgumentParser()
    p.add_argument("name")
    p.add_argument("ref", nargs="?", default="external")
    args = p.parse_args()
    client = UnrealMCPClient()
    assert client.connect() and client.is_in_pie()
    ffmpeg = imageio_ffmpeg.get_ffmpeg_exe()
    # Only opaque interiors of the known game panels reach the output. The gap
    # between the native translucent windows is excluded from the composition.
    filters = ("[0:v]split=2[a][b];[a]crop=342:424:0:36[inv];"
               "[b]crop=262:666:458:2[tool];"
               "[1:v][inv]overlay=520:176[c];[c][tool]overlay=950:26,fps=60[v]")
    command = [ffmpeg, "-hide_banner", "-y", "-f", "gdigrab", "-framerate", "60",
               "-draw_mouse", "0", "-offset_x", "882", "-offset_y", "300",
               "-video_size", "724x670", "-i", "desktop", "-f", "lavfi", "-i",
               "color=c=0x171d22:s=1280x720:r=60", "-filter_complex", filters, "-map", "[v]",
               "-an", "-c:v", "libx264", "-preset", "veryfast", "-crf", "16", "-pix_fmt",
               "yuv420p", "-r", "60", "-fps_mode", "cfr", "-movflags", "+faststart",
               str(OUT / (args.name + "-raw.mp4"))]
    with (OUT / (args.name + "-ffmpeg.log")).open("w", encoding="utf-8") as log:
        started = time.time()
        process = subprocess.Popen(command, stdin=subprocess.PIPE, stdout=subprocess.DEVNULL, stderr=log)
        recording = False
        try:
            time.sleep(1.0)
            assert process.poll() is None, "Video recorder failed"
            start = client.run_project_python_file("Content/Python/gamexxk_tool_sfx_sample.py", ["record-start", "--name", args.name])
            recording = True
            if args.ref == "external":
                ready = OUT / (args.name + "-ready.json")
                ready.write_text(json.dumps({"ready": time.time()}), encoding="utf-8")
                print("READY " + args.name, flush=True)
                deadline = time.monotonic() + 45
                stop_flag = OUT / (args.name + "-stop.json")
                while not stop_flag.exists() and time.monotonic() < deadline:
                    time.sleep(.1)
                assert stop_flag.exists(), "No external click completion in 45 seconds"
                click_before = click_after = json.loads(stop_flag.read_text(encoding="utf-8"))["click_time"]
            else:
                time.sleep(1.5)
                click_before = time.time()
                clicked = client.call_tool("Click", {"ref": args.ref}, toolset_name=SLATE)
                click_after = time.time()
                assert clicked, "Slate click rejected"
            time.sleep(2.5)
            stop = client.run_project_python_file("Content/Python/gamexxk_tool_sfx_sample.py", ["record-stop", "--name", args.name])
            recording = False
            time.sleep(.5)
            data = {"name": args.name, "video_process_start": started, "click_before": click_before,
                    "click_after": click_after, "ref": args.ref, "start": start, "stop": stop}
            (OUT / (args.name + "-capture.json")).write_text(json.dumps(data, ensure_ascii=False, indent=2), encoding="utf-8")
        finally:
            if recording:
                client.run_project_python_file("Content/Python/gamexxk_tool_sfx_sample.py", ["record-stop", "--name", args.name])
            if process.poll() is None:
                process.communicate(b"q", timeout=15)
    print(json.dumps({"name": args.name, "video_exit": process.returncode, "capture": data}, ensure_ascii=False))


if __name__ == "__main__":
    main()
