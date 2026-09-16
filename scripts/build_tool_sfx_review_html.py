"""Embed the existing audio sample as one portable, editable browser review file."""
import base64
import hashlib
import json
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PACKAGE = ROOT / "Deliverables/音效需求/14_工具操作_v001"
HTML = PACKAGE / "00_音效需求.html"


def load(path):
    return json.loads(path.read_text(encoding="utf-8-sig"))


def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    manifest = load(PACKAGE / "05_文件核验.json")
    files = {r["file"]: r for r in manifest["files"]}
    mapping = {
        "video": ("01_演示_现有效果.mp4", "video/mp4"),
        "silentVideo": ("02_演示_无声版.mp4", "video/mp4"),
        "tool": ("参考音频/SFX_Tool_v01.wav", "audio/wav"),
        "button": ("参考音频/SFX_Button_v01.wav", "audio/wav"),
        "sheet": ("03_时间轴需求.xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"),
        "toolOriginal": ("来源与授权/kenney-impact/impactMining_000.ogg", "audio/ogg"),
        "buttonOriginal": ("来源与授权/kenney-interface/click_001.ogg", "audio/ogg"),
        "toolLicense": ("来源与授权/kenney-impact/LICENSE-1.txt", "text/plain;charset=utf-8"),
        "buttonLicense": ("来源与授权/kenney-interface/LICENSE-1.txt", "text/plain;charset=utf-8"),
    }
    assets = {}
    for key, (relative, mime) in mapping.items():
        path = PACKAGE / relative
        assert sha(path) == files[relative]["sha256"], relative
        assets[key] = {"name": path.name if "License" not in key else key+"_CC0.txt", "mime": mime,
                       "sha256": sha(path), "base64": base64.b64encode(path.read_bytes()).decode("ascii")}
    titles = ["强化成功", "九件合成", "洗炼 · 生成候选", "洗炼 · 采用新属性", "洗炼 · 保留原属性"]
    notes = ["等级提升、材料扣除后确认完成", "九格清空，新装备进入背包", "展示新旧对比，还没有替换属性", "选择新属性后，替换正式生效", "放弃本次候选，保留原有属性"]
    payload = {"schema": 1, "pageVersion": "1.0", "package": "14_工具操作_v001", "video": manifest["video"],
               "assets": assets, "soundEvents": manifest["sound_events"], "review": None,
               "scenes": [{"title": title, "note": notes[i], "start": i*5, "end": (i+1)*5,
                           "targetFrame": manifest["visual_result_frames"][i], "tool": i in (0,1,3)} for i,title in enumerate(titles)]}
    template = (ROOT / "scripts/templates/tool-sfx-review.html").read_text(encoding="utf-8")
    assert template.count("__REVIEW_PAYLOAD__") == 1
    packed = json.dumps(payload, ensure_ascii=False, separators=(",", ":")).replace("<", "\\u003c")
    HTML.write_text(template.replace("__REVIEW_PAYLOAD__", packed), encoding="utf-8")
    # The browser file is the entry point; retain the original media/table files
    # for production tools, and keep the complete ZIP and hashes current.
    readme = PACKAGE / "00_先看这里.md"
    text = readme.read_text(encoding="utf-8")
    marker = "<!-- web-review-entry -->"
    if marker not in text:
        intro = """<!-- web-review-entry -->
**网页入口：[00_音效需求.html](00_音效需求.html)**。双击可离线打开；视频、参考音频和需求均已内嵌，单独发送这个HTML即可查看。

点击时间轴定位或播放片段，导入新WAV后可切换“新音效试配”。版本、备注和成品音频通过“导出评审HTML”一并保存；刷新或关闭前请先导出。浏览器导出的HTML可以直接发回、重新打开和继续评审。网页试听不会修改项目WAV或游戏接入。
<!-- /web-review-entry -->

"""
        first, rest = text.split("\n", 1)
        readme.write_text(first+"\n\n"+intro+rest.lstrip("\n"), encoding="utf-8")
    manifest["web_review"] = {"file": HTML.name, "standalone": True, "embedded_assets": len(assets), "page_version": "1.0"}
    manifest["files"] = [{"file": p.relative_to(PACKAGE).as_posix(), "bytes": p.stat().st_size, "sha256": sha(p)}
                         for p in sorted(PACKAGE.rglob("*")) if p.is_file() and p.name != "05_文件核验.json"]
    (PACKAGE / "05_文件核验.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")
    archive_path = PACKAGE.with_suffix(".zip")
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(PACKAGE.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(PACKAGE.parent))
    with zipfile.ZipFile(archive_path) as archive:
        assert archive.testzip() is None
    print(json.dumps({"html": str(HTML), "bytes": HTML.stat().st_size, "embedded_assets": len(assets),
                      "zip": str(archive_path), "zip_bytes": archive_path.stat().st_size}, ensure_ascii=False))


if __name__ == "__main__":
    main()
