# Reference-Faithful Task Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** Restore only the reward coin, EXP, and token icons with source-faithful super-resolution while leaving the user-approved paper frame, arrow, tab, card, buttons, labels, widget code, gameplay, maps, and user-tuned assets unchanged.

**Architecture:** The pre-redraw reward crops from revision 33b1d3ae2e042731ed4ac3c9b8989b2a76c32e60 are immutable visual anchors. Built-in image generation may only upscale each exact crop. A candidate can replace the current production icon only after visual comparison; every changed-art candidate is discarded and the exact source crop is the fallback.

**Tech Stack:** Git, Python 3/Pillow, built-in image-edit generation, Unreal Engine 5.8 UnrealEditor-Cmd, existing Content/Python/gamexxk_import_task_ui_assets.py.

---

## File map

| File | Responsibility |
| --- | --- |
| docs/ui/tasks/reference_crops/reward_coin.png | Exact pre-redraw coin visual authority. |
| docs/ui/tasks/reference_crops/reward_exp.png | Exact pre-redraw EXP visual authority. |
| docs/ui/tasks/reference_crops/reward_token.png | Exact pre-redraw token visual authority. |
| scripts/export_task_ui_icon_reference_crops.py | Binary-safe export of those three anchors. |
| scripts/test_reference_faithful_task_ui_icons.py | Contract audit with an anchor-only bootstrap mode and a strict final production mode. |
| docs/ui/tasks/source_art/reward_{coin,exp,token}.png | Final selected source artwork. |
| Content/GameXXK/UI/Tasks/Textures/T_Reward{Coin,Exp,Token}.uasset | Reimported UE textures. |

## Task 1: Add source anchors and a test-first asset contract

**Files:**
- Create: scripts/export_task_ui_icon_reference_crops.py
- Create: scripts/test_reference_faithful_task_ui_icons.py
- Create: docs/ui/tasks/reference_crops/reward_coin.png
- Create: docs/ui/tasks/reference_crops/reward_exp.png
- Create: docs/ui/tasks/reference_crops/reward_token.png

- [ ] **Step 1: Write the failing audit.**

~~~python
# scripts/test_reference_faithful_task_ui_icons.py
from pathlib import Path
from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
REFERENCE = ROOT / "docs" / "ui" / "tasks" / "reference_crops"
SOURCE = ROOT / "docs" / "ui" / "tasks" / "source_art"
NAMES = ("reward_coin.png", "reward_exp.png", "reward_token.png")

def info(path: Path):
    with Image.open(path) as image:
        return image.size, image.mode

def main():
    failures = []
    for name in NAMES:
        reference, production = REFERENCE / name, SOURCE / name
        if not reference.is_file():
            failures.append(f"missing reference crop: {reference}")
            continue
        if not production.is_file():
            failures.append(f"missing production icon: {production}")
            continue
        reference_size, reference_mode = info(reference)
        production_size, production_mode = info(production)
        if abs(reference_size[0] / reference_size[1] - production_size[0] / production_size[1]) > 0.02:
            failures.append(f"aspect drift: {name}")
        if production_size[0] < reference_size[0] or production_size[1] < reference_size[1]:
            failures.append(f"production icon shrank: {name}")
        if reference_mode not in {"RGB", "RGBA"} or production_mode not in {"RGB", "RGBA"}:
            failures.append(f"unsupported image mode: {name}")
    print("reference_faithful_task_icons: " + ("PASS" if not failures else "FAIL"))
    print("\n".join(failures))
    return 0 if not failures else 1

if __name__ == "__main__":
    raise SystemExit(main())
~~~

- [ ] **Step 2: Prove RED.**

~~~powershell
python scripts/test_reference_faithful_task_ui_icons.py --anchors-only
~~~

Expected: exit code 1 and three missing-reference failures.

The current `075f8fd` production icons intentionally have different proportions from the pre-redraw crops, so the default strict audit cannot become green until Task 3. Add a `--anchors-only` mode that validates only the three immutable reference files (presence plus RGB/RGBA mode); keep the no-argument mode strict, validating references and the production aspect/size/mode contract exactly as originally specified. This avoids hiding the known bad production state while allowing Task 1's exporter to have a real red/green loop.

- [ ] **Step 3: Implement the binary-safe source exporter.**

~~~python
# scripts/export_task_ui_icon_reference_crops.py
import json
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVISION = "33b1d3ae2e042731ed4ac3c9b8989b2a76c32e60"
DESTINATION = ROOT / "docs" / "ui" / "tasks" / "reference_crops"
NAMES = ("reward_coin.png", "reward_exp.png", "reward_token.png")

def main():
    DESTINATION.mkdir(parents=True, exist_ok=True)
    exports = []
    for name in NAMES:
        result = subprocess.run(
            ["git", "show", f"{REVISION}:docs/ui/tasks/source_art/{name}"],
            cwd=ROOT, check=True, capture_output=True,
        )
        output = DESTINATION / name
        output.write_bytes(result.stdout)
        exports.append(output.relative_to(ROOT).as_posix())
    print(json.dumps({"reference_revision": REVISION, "exported_count": len(exports), "exports": exports}))
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
~~~

- [ ] **Step 4: Export anchors, run GREEN, and commit.**

~~~powershell
python scripts/export_task_ui_icon_reference_crops.py
python scripts/test_reference_faithful_task_ui_icons.py --anchors-only
git add -- scripts/export_task_ui_icon_reference_crops.py scripts/test_reference_faithful_task_ui_icons.py docs/ui/tasks/reference_crops/reward_coin.png docs/ui/tasks/reference_crops/reward_exp.png docs/ui/tasks/reference_crops/reward_token.png
git diff --cached --check
git commit -m "test: anchor original task reward icons"
~~~

Expected: exporter JSON contains "exported_count": 3; the `--anchors-only` audit prints `reference_faithful_task_icons: PASS`. Also run the default strict audit and record its expected failure against the still-bad current production icons; Task 3 must make that default audit pass.

## Task 2: Generate candidates with a source-lock visual gate

**Files:**
- Create (untracked): Saved/Codex/task_ui_icon_candidates/reward_coin.png
- Create (untracked): Saved/Codex/task_ui_icon_candidates/reward_exp.png
- Create (untracked): Saved/Codex/task_ui_icon_candidates/reward_token.png
- Create (untracked): Saved/Codex/task_ui_icon_comparison.png
- Create (untracked): Saved/Codex/task_ui_icon_acceptance.json

- [ ] **Step 1: Inspect the three immutable source crops.**

Use view_image on every icon under docs/ui/tasks/reference_crops before editing. Lock the literal source silhouette, inner mark, glyphs, paper texture, uneven ink edge, palette, and opaque/transparent behavior. The coin must remain its ochre hand-drawn coin; EXP must remain exactly EXP with the original green-yellow treatment; the token must remain its teal hand-drawn token.

- [ ] **Step 2: Make one built-in image-edit request per exact crop.**

Use the built-in image-edit path, not a text-to-image call, CLI fallback, SVG, vector, or deterministic redraw. Save matching candidates under Saved/Codex/task_ui_icon_candidates. Use this exact prompt:

~~~text
Upscale and restore this exact supplied UI crop only. Preserve the original water-ink hand-drawn style, old-paper texture, irregular brush edge, silhouette, composition, symbol, color, ink stroke, and background/alpha behavior exactly. Improve resolution and clarity only. Do not redraw, redesign, reinterpret, simplify, vectorize, add, remove, replace, recolor, restyle, move, or change any element. Keep all Chinese and Latin glyphs exactly unchanged. No new text or watermark.
~~~

If a candidate alters any content, discard it. One retry with the same crop and exact same prompt is the only allowed retry. Do not create a new icon description.

- [ ] **Step 3: Build a source/candidate sheet.**

~~~powershell
@'
from pathlib import Path
from PIL import Image, ImageDraw

root = Path(r"D:\UE5 demo\GameXXK")
names = ("reward_coin.png", "reward_exp.png", "reward_token.png")
sheet = Image.new("RGB", (1600, 960), "#252722")
draw = ImageDraw.Draw(sheet)
for row, name in enumerate(names):
    source = Image.open(root / "docs/ui/tasks/reference_crops" / name).convert("RGBA")
    candidate = Image.open(root / "Saved/Codex/task_ui_icon_candidates" / name).convert("RGBA")
    source.thumbnail((600, 220), Image.Resampling.LANCZOS)
    candidate.thumbnail((600, 220), Image.Resampling.LANCZOS)
    y = 30 + row * 305
    draw.text((30, y), "reference  " + name, fill="white")
    draw.text((830, y), "candidate  " + name, fill="white")
    sheet.paste(source, (30, y + 40), source)
    sheet.paste(candidate, (830, y + 40), candidate)
sheet.save(root / "Saved/Codex/task_ui_icon_comparison.png")
'@ | python -
~~~

- [ ] **Step 4: Apply the acceptance rule and record it.**

Inspect Saved/Codex/task_ui_icon_comparison.png with view_image. An icon is accepted only if it remains the same original art at a glance: contour, inner mark or literal EXP glyphs, brush irregularity, paper texture, palette, and background behavior must match. A clearer but newly designed icon is rejected. For a rejected candidate, use the exact reference crop as the production fallback.

Write Saved/Codex/task_ui_icon_acceptance.json with the actual choices using only candidate or reference_fallback values:

~~~json
{
  "reward_coin.png": {"decision": "candidate"},
  "reward_exp.png": {"decision": "reference_fallback"},
  "reward_token.png": {"decision": "candidate"}
}
~~~

This evidence stays untracked.

## Task 3: Promote the accepted art, reimport, and verify scope

**Files:**
- Modify: docs/ui/tasks/source_art/reward_coin.png
- Modify: docs/ui/tasks/source_art/reward_exp.png
- Modify: docs/ui/tasks/source_art/reward_token.png
- Modify: Content/GameXXK/UI/Tasks/Textures/T_RewardCoin.uasset
- Modify: Content/GameXXK/UI/Tasks/Textures/T_RewardExp.uasset
- Modify: Content/GameXXK/UI/Tasks/Textures/T_RewardToken.uasset

- [ ] **Step 1: Promote only a candidate or exact source fallback.**

For each candidate decision, copy the matching candidate into docs/ui/tasks/source_art. For each reference_fallback decision, copy the matching immutable reference crop into docs/ui/tasks/source_art. Do not touch task_panel_frame, task_panel_back_arrow, task_tab_selected, task cards, task buttons, labels, the importer script, UMG code, gameplay, levels, or user-tuned assets.

- [ ] **Step 2: Audit, reimport, and audit again.**

~~~powershell
python scripts/test_reference_faithful_task_ui_icons.py
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -run=pythonscript -script='D:/UE5 demo/GameXXK/Content/Python/gamexxk_import_task_ui_assets.py' -Unattended -NoSplash -NoSound -NullRHI -log -stdout -FullStdOutLogOutput
python scripts/test_reference_faithful_task_ui_icons.py
~~~

Expected: both audits print reference_faithful_task_icons: PASS; importer JSON contains "imported_count": 13 and final summary is Success - 0 error(s).

- [ ] **Step 3: Verify strict scope, inspect the final three images, and commit.**

~~~powershell
git diff --check
git status --short -- docs/ui/tasks/source_art/reward_coin.png docs/ui/tasks/source_art/reward_exp.png docs/ui/tasks/source_art/reward_token.png Content/GameXXK/UI/Tasks/Textures/T_RewardCoin.uasset Content/GameXXK/UI/Tasks/Textures/T_RewardExp.uasset Content/GameXXK/UI/Tasks/Textures/T_RewardToken.uasset
git add -- docs/ui/tasks/source_art/reward_coin.png docs/ui/tasks/source_art/reward_exp.png docs/ui/tasks/source_art/reward_token.png Content/GameXXK/UI/Tasks/Textures/T_RewardCoin.uasset Content/GameXXK/UI/Tasks/Textures/T_RewardExp.uasset Content/GameXXK/UI/Tasks/Textures/T_RewardToken.uasset
git diff --cached --check
git commit -m "fix: restore source-faithful task reward icons"
~~~

Before staging, use view_image to compare the final production files with their anchors. Verify the other ten task atoms have no new changes in this correction.
