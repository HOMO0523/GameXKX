"""Prepare the scoped CC0 audio trial; original recordings and provenance are retained."""
import argparse
import hashlib
import json
import shutil
import subprocess
import wave
from pathlib import Path

import imageio_ffmpeg
import numpy as np

ROOT = Path(__file__).resolve().parents[1]
DEST = ROOT / "SourceArt/Audio/Essential"
FFMPEG = imageio_ffmpeg.get_ffmpeg_exe()
RATE = 48000

# cue, variant, pack, source basename, start seconds, max input seconds, tempo, target RMS
SPECS = [
    ("HitLight", 1, "kenney-impact", "impactPunch_medium_000.ogg", 0, .40, 1, .13),
    ("HitLight", 2, "kenney-impact", "impactPunch_medium_001.ogg", 0, .40, 1, .13),
    ("HitLight", 3, "kenney-impact", "impactPunch_medium_002.ogg", 0, .40, 1, .13),
    ("HitHeavy", 1, "kenney-impact", "impactPunch_heavy_000.ogg", 0, .48, 1, .18),
    ("HitHeavy", 2, "kenney-impact", "impactPunch_heavy_001.ogg", 0, .48, 1, .18),
    ("CardPlay", 1, "kenney-rpg", "bookPlace1.ogg", 0, .30, 1, .075),
    ("CardPlay", 2, "kenney-rpg", "bookPlace2.ogg", 0, .30, 1, .075),
    ("Block", 1, "kenney-impact", "impactPlate_medium_000.ogg", 0, .42, 1, .13),
    ("Block", 2, "kenney-impact", "impactPlate_medium_001.ogg", 0, .42, 1, .13),
    ("Lightning", 1, "brandon-spells", "electricspell.ogg", 11.30, 1.25, 1.7, .12),
    ("Lightning", 2, "brandon-spells", "electricspell.ogg", 16.27, 1.04, 1.5, .12),
    ("Fire", 1, "rubberduck-rpg", "spell_fire_01.ogg", 0, .68, 1.15, .12),
    ("Fire", 2, "rubberduck-rpg", "spell_fire_06.ogg", 0, .45, 1.15, .10),
    ("Frost", 1, "bart-ice", "ice.wav", .03, .65, 1.15, .12),
    ("Frost", 2, "bart-ice", "coldsnap.wav", 0, .55, 1, .12),
    ("Heal", 1, "brandon-spells", "healing.ogg", 11.47, 1.16, 1.6, .075),
    ("Down", 1, "kenney-rpg", "dropLeather.ogg", 0, .45, 1, .105),
    ("Victory", 1, "kenney-jingles", "jingles_PIZZI01.ogg", 0, 1.10, 1, .09),
    ("Defeat", 1, "kenney-jingles", "jingles_PIZZI11.ogg", 0, .85, 1, .075),
    ("Reward", 1, "rubberduck-rpg", "lock_01.ogg", 0, .80, 1, .10),
    ("Button", 1, "kenney-interface", "click_001.ogg", 0, .12, 1, .04),
    ("Tool", 1, "kenney-impact", "impactMining_000.ogg", 0, .52, 1, .09),
]

def decode(path, start, duration, tempo):
    args = [FFMPEG, "-v", "error", "-ss", str(start), "-i", str(path), "-t", str(duration / tempo)]
    if tempo != 1:
        args += ["-af", "atempo=" + str(tempo)]
    args += ["-f", "f32le", "-ac", "1", "-ar", str(RATE), "-"]
    return np.frombuffer(subprocess.run(args, capture_output=True, check=True).stdout, dtype="<f4").copy()

def prepare(source_root):
    (DEST / "Waves").mkdir(parents=True, exist_ok=True)
    (DEST / "Licenses").mkdir(parents=True, exist_ok=True)
    records, packs = [], {}
    for cue, variant, pack, filename, start, duration, tempo, target_rms in SPECS:
        pack_dir = source_root / pack
        source_manifest = json.loads((pack_dir / "source-manifest.json").read_text(encoding="utf-8"))
        assert source_manifest["license"] == "CC0-1.0"
        matches = [p for p in pack_dir.rglob(filename) if p.is_file()]
        assert len(matches) == 1, (pack, filename)
        original = matches[0]
        keep = DEST / "Originals" / pack / filename
        keep.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, keep)
        if pack not in packs:
            license_folder = DEST / "Licenses" / pack
            license_folder.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(pack_dir / "source-manifest.json", license_folder / "source-manifest.json")
            shutil.copyfile(pack_dir / "source-page.html", license_folder / "source-page.html")
            for index, license_file in enumerate(source_manifest.get("license_files", []), 1):
                (license_folder / f"LICENSE-{index}.txt").write_text(license_file["text"], encoding="utf-8")
            packs[pack] = {k: source_manifest[k] for k in ("author", "source", "license", "license_url")}
        samples = decode(original, start, duration, tempo)
        assert len(samples) and np.isfinite(samples).all()
        threshold = max(float(np.abs(samples).max()) * .003, .00015)
        active = np.flatnonzero(np.abs(samples) >= threshold)
        assert len(active)
        first = max(0, int(active[0]) - 48)
        last = min(len(samples), int(active[-1]) + 1 + int(.025 * RATE))
        samples = samples[first:last]
        # Remove DC and normalize floating decoder output before quantization.
        samples -= samples.mean()
        rms = max(float(np.sqrt(np.mean(samples ** 2))), 1e-9)
        peak = max(float(np.abs(samples).max()), 1e-9)
        gain = min(target_rms / rms, .72 / peak)
        if cue == "Button":
            gain = min(gain, .25 / peak)
        samples *= gain
        fade_in = min(int(.001 * RATE), len(samples) // 8)
        fade_out = min(int(.025 * RATE), len(samples) // 5)
        samples[:fade_in] *= np.linspace(0, 1, fade_in)
        samples[-fade_out:] *= np.linspace(1, 0, fade_out)
        pcm = (np.clip(samples, -.99, .99) * 32767).astype("<i2")
        name = f"SFX_{cue}_v{variant:02}"
        target = DEST / "Waves" / (name + ".wav")
        with wave.open(str(target), "wb") as w:
            w.setnchannels(1); w.setsampwidth(2); w.setframerate(RATE); w.writeframes(pcm.tobytes())
        with wave.open(str(target), "rb") as w:
            assert (w.getnchannels(), w.getsampwidth(), w.getframerate()) == (1, 2, RATE)
            assert w.getnframes() == len(samples)
        records.append({
            "cue": cue, "variant": variant, "name": name,
            "asset": "/Game/GameXXK/Audio/SFX/Essential/" + name,
            "file": str(target.relative_to(ROOT)).replace("\\", "/"),
            "source_file": str(keep.relative_to(ROOT)).replace("\\", "/"),
            "pack": pack, **packs[pack],
            "source_sha256": hashlib.sha256(original.read_bytes()).hexdigest(),
            "sha256": hashlib.sha256(target.read_bytes()).hexdigest(),
            "seconds": round(len(samples) / RATE, 4), "sample_rate": RATE, "channels": 1, "pcm_bits": 16,
            "peak": round(float(np.abs(pcm.astype(float)).max()) / 32768, 5),
            "rms": round(float(np.sqrt(np.mean((pcm.astype(float) / 32768) ** 2))), 5),
            "processing": {"start": start, "input_seconds": duration, "tempo": tempo, "trim_lead_samples": first,
                           "gain": round(gain, 6), "dc_removed": True, "fade_in_ms": 1, "fade_out_ms_max": 25},
        })
    assert len(records) == 22 and len({r["cue"] for r in records}) == 14
    assert all(.005 < r["seconds"] < 1.6 and 0 < r["peak"] <= .73 for r in records)
    manifest = {"version": 1, "status": "trial", "license": "CC0-1.0", "cue_count": 14, "wave_count": 22,
                "files": records, "packs": packs}
    (DEST / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    text = ["# 必要音效试用素材", "", "14类、22个短WAV。来源采用CC0；保留原始文件和作者发布页。这里的16-bit PCM是UE试用导入版，不把有损源伪称24-bit录音母带。", "",
            "处理：裁短、去首尾空白/直流、轻微加速、按用途调增益和淡出；没有改变授权条件。", "",
            "## 来源", ""]
    for key, p in packs.items():
        text += [f"- {key}: [{p['author']}]({p['source']}) · [CC0]({p['license_url']})", ""]
    text += ["## 映射", "", "|音效|原文件|时长秒|", "|---|---|---|"]
    text += [f"|{r['name']}|{r['pack']}/{Path(r['source_file']).name}|{r['seconds']}|" for r in records]
    (DEST / "README.md").write_text("\n".join(text) + "\n", encoding="utf-8")
    print(json.dumps({"status": "PASS", "cues": 14, "waves": len(records),
                      "seconds": round(sum(r["seconds"] for r in records), 3),
                      "bytes": sum((ROOT / r["file"]).stat().st_size for r in records),
                      "manifest": str(DEST / "manifest.json")}, ensure_ascii=True))

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--source-root", type=Path, required=True)
    prepare(parser.parse_args().source_root)
