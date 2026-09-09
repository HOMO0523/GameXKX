"""Import and verify the 14-cue audio trial through the project's UE MCP entrypoint."""
import argparse
import builtins
import hashlib
import json
import time
from pathlib import Path

import unreal

ROOT = Path(unreal.Paths.project_dir()).resolve()
REPORT = ROOT / "Saved/Codex/EssentialSfx-20260909"
REPORT.mkdir(parents=True, exist_ok=True)
MANIFEST = ROOT / "SourceArt/Audio/Essential/manifest.json"

def world():
    return unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()

def import_assets():
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    assert manifest["cue_count"] == 14 and manifest["wave_count"] == 22
    tasks = []
    for record in manifest["files"]:
        source = ROOT / record["file"]
        assert hashlib.sha256(source.read_bytes()).hexdigest() == record["sha256"]
        task = unreal.AssetImportTask()
        task.set_editor_property("filename", str(source))
        task.set_editor_property("destination_path", "/Game/GameXXK/Audio/SFX/Essential")
        task.set_editor_property("destination_name", record["name"])
        task.set_editor_property("automated", True)
        task.set_editor_property("replace_existing", True)
        task.set_editor_property("save", True)
        tasks.append(task)
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks(tasks)
    result = []
    for record in manifest["files"]:
        sound = unreal.load_asset(record["asset"])
        assert isinstance(sound, unreal.SoundWave), record["asset"]
        sound.set_editor_property("looping", False)
        if record["cue"] == "Button":
            limits = sound.get_editor_property("concurrency_overrides")
            limits.set_editor_property("max_count", 2)
            limits.set_editor_property("retrigger_time", .08)
            sound.set_editor_property("concurrency_overrides", limits)
            sound.set_editor_property("override_concurrency", True)
        duration = float(sound.get_editor_property("duration"))
        assert abs(duration - record["seconds"]) < .03, (record["asset"], duration)
        assert unreal.EditorAssetLibrary.save_loaded_asset(sound), record["asset"]
        result.append({"asset": record["asset"], "class": sound.get_class().get_name(), "seconds": duration})
    report = {"ok": len(result) == 22, "assets": result}
    (REPORT / "import.json").write_text(json.dumps(report, indent=2), encoding="utf-8")
    return report

def state():
    current = world()
    return {"world": current.get_path_name() if current else None,
            "audio": json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(current)) if current else {},
            "audition": getattr(builtins, "_gamexxk_essential_sfx_audition", {}).get("report", {})}

def audition():
    current = world()
    assert current and "L_DesktopTrainingHUD" in current.get_path_name(), "Use the canonical 2D PIE map"
    previous = getattr(builtins, "_gamexxk_essential_sfx_audition", None)
    if previous and previous.get("handle"):
        unreal.unregister_slate_post_tick_callback(previous["handle"])
    manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))
    cues = [r["cue"] for r in manifest["files"]]
    unreal.GameXXKSfxLibrary.reset_diagnostics(current)
    assert json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(current)).get("master_volume", 0) > 0, "Audio is muted"
    unreal.AudioMixerLibrary.start_recording_output(current, 40.0)
    start = time.monotonic()
    data = {"report": {"status": "running", "samples": []}, "index": 0, "handle": None}
    setattr(builtins, "_gamexxk_essential_sfx_audition", data)
    def tick(delta):
        elapsed = time.monotonic() - start
        index = data["index"]
        if index < len(cues) and elapsed >= .5 + index * 1.35:
            cue = cues[index]
            played = unreal.GameXXKSfxLibrary.play_named(current, cue)
            data["report"]["samples"].append({"cue": cue, "accepted": bool(played), "elapsed": round(elapsed, 3)})
            data["index"] += 1
        elif index == len(cues) and elapsed > 1.9 + len(cues) * 1.35:
            unreal.AudioMixerLibrary.stop_recording_output(current, unreal.AudioRecordingExportType.WAV_FILE,
                                                                     "essential-sfx-output", str(REPORT))
            unreal.unregister_slate_post_tick_callback(data["handle"])
            data["handle"] = None
            data["report"].update(status="complete", all_accepted=all(s["accepted"] for s in data["report"]["samples"]),
                                   diagnostics=json.loads(unreal.GameXXKSfxLibrary.get_diagnostics(current)))
            (REPORT / "audition.json").write_text(json.dumps(data["report"], indent=2), encoding="utf-8")
    data["handle"] = unreal.register_slate_post_tick_callback(tick)
    return {"status": "started", "cues": len(cues), "expected_seconds": 32}

def record_start():
    current = world()
    assert current and "L_DesktopTrainingHUD" in current.get_path_name()
    unreal.GameXXKSfxLibrary.reset_diagnostics(current)
    unreal.AudioMixerLibrary.start_recording_output(current, 40.0)
    return {"ok": True, "state": state()}

def record_stop(name):
    current = world()
    unreal.AudioMixerLibrary.stop_recording_output(current, unreal.AudioRecordingExportType.WAV_FILE, name, str(REPORT))
    result = state()
    (REPORT / (name + ".json")).write_text(json.dumps(result, indent=2), encoding="utf-8")
    return result

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("action", choices=["import", "state", "audition", "record-start", "record-stop", "play", "api"])
    parser.add_argument("--name", default="essential-sfx-live")
    args = parser.parse_args()
    if args.action == "api": result = [n for n in dir(unreal) if "AudioMixer" in n or "AudioRecording" in n]
    elif args.action == "import": result = import_assets()
    elif args.action == "audition": result = audition()
    elif args.action == "record-start": result = record_start()
    elif args.action == "record-stop": result = record_stop(args.name)
    elif args.action == "play": result = {"accepted": unreal.GameXXKSfxLibrary.play_named(world(), args.name), "state": state()}
    else: result = state()
    print(json.dumps(result, ensure_ascii=False))
