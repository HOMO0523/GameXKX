# Battle Animation Seven-Asset Pilot Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Generate, download, verify, and archive seven production-eligible Seedance battle-animation assets without allowing failed/rejected work to consume more than 565 credits.

**Architecture:** Drive the installed Dreamina CLI sequentially from a manifest and immutable prompt files. Submit only one paid task at a time, save its task ID immediately, wait for a terminal result, run deterministic media checks, and update a credit ledger before any next submission. Accepted pilot clips become production assets; rejected clips consume the retry ledger.

**Tech Stack:** Dreamina CLI `frames2video`, Seedance 2.0 VIP, Seedance 1.5 Pro, PowerShell, Python/Pillow for the pure-magenta frame, FFmpeg/ffprobe for media verification, JSON and UTF-8 prompt files.

---

### Task 1: Create the immutable pilot inputs and manifest

**Files:**
- Read: `SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png`
- Read: `SourceAssets/RouteEnemies/final_selected_v1/01_rooster.png`
- Create: `SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/hero_attack.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/hero_hit.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/rooster_attack.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/rooster_hit.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/status_buff_generic.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/status_debuff_generic.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/prompts/impact_ink_generic.txt`
- Create: `SourceAssets/AnimationProduction/pilot_v1/pilot_manifest.json`
- Test: `scripts/test_seedance_seven_asset_pilot.py`

- [ ] **Step 1: Write the failing manifest contract test**

Create `scripts/test_seedance_seven_asset_pilot.py` with tests that require exactly seven unique asset IDs, verify the two attacks use `seedance2.0_vip`, verify the other five use `seedance1.5pro`, sum expected credits to 340, require 720p/5s, and require every referenced input and prompt file to exist.

```python
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFEST = ROOT / "SourceAssets/AnimationProduction/pilot_v1/pilot_manifest.json"


class SevenAssetPilotTests(unittest.TestCase):
    def test_manifest_contract(self) -> None:
        data = json.loads(MANIFEST.read_text(encoding="utf-8"))
        assets = data["assets"]
        self.assertEqual(7, len(assets))
        self.assertEqual(7, len({item["id"] for item in assets}))
        self.assertEqual(340, sum(item["expected_credits"] for item in assets))
        self.assertEqual(565, data["retry_credit_cap"])
        for item in assets:
            self.assertEqual("720p", item["resolution"])
            self.assertEqual(5, item["duration_seconds"])
            expected_model = "seedance2.0_vip" if item["id"] in {"hero_attack", "rooster_attack"} else "seedance1.5pro"
            self.assertEqual(expected_model, item["model"])
            self.assertTrue((ROOT / item["first_frame"]).is_file())
            self.assertTrue((ROOT / item["last_frame"]).is_file())
            self.assertTrue((ROOT / item["prompt_file"]).is_file())


if __name__ == "__main__":
    unittest.main()
```

- [ ] **Step 2: Run the contract test and verify it fails**

Run:

```powershell
python scripts/test_seedance_seven_asset_pilot.py
```

Expected: `ERROR` because `pilot_manifest.json` does not yet exist.

- [ ] **Step 3: Create the magenta frame and prompt files**

Create a 720×720 RGB PNG whose every pixel is `(255, 0, 255)`. Create the seven UTF-8 prompt files with these exact contents:

`hero_attack.txt`:

```text
5秒战斗攻击动画，首帧和尾帧必须精确回到输入图的同一Idle姿势。严格保持主角身份、脸部、发型、衣服、背篓、葫芦、竹叶、身体比例、水墨水彩笔触、颜色和构图不变。角色始终位于画面右侧并保持头与身体朝画面左侧的3/4站姿，不转身、不翻向。0.00-0.70秒保持Idle；0.70秒快速启动，略微屈膝压低重心，从背篓中抽出一根竹枝或竹杖；1.10秒竹杖快速向左横扫并清楚经过画面中央命中点；1.10-1.35秒完成冲击，随后快进慢出；1.35-4.30秒慢速收势，背篓、葫芦、竹叶和衣摆产生受控的延迟摆动并逐渐停止；4.30-5.00秒将竹杖归回并精确恢复首帧Idle。镜头完全固定，纯#FF00FF洋红背景完全不变，无地面、阴影、文字和光效。禁止走路、滑步、跳跃、转身、攻击方向错误、把竹杖变成长剑、丢失背篓或葫芦、武器变形、肢体异常、比例变化、换装和画风变化。
```

`hero_hit.txt`:

```text
5秒战斗受击动画，首帧和尾帧必须精确回到输入图的同一Idle姿势。严格保持主角身份、脸部、发型、衣服、背篓、葫芦、竹叶、身体比例、水墨水彩笔触、颜色和构图不变。角色始终位于画面右侧并保持头与身体朝画面左侧的3/4站姿，不转身、不翻向。0.00-0.70秒保持Idle；0.70秒感受到来自画面左侧的攻击并迅速绷紧；1.10秒出现清楚受击峰值，肩胸和上身快速向画面右侧后仰错开但双脚锚点稳定；1.10-1.35秒完成短促冲击；1.35-4.30秒慢速恢复，背篓、葫芦、竹叶和衣摆在身体之后延迟摆动并逐渐停止；4.30-5.00秒精确恢复首帧Idle。动作快进慢出、克制但冲击明确。镜头完全固定，纯#FF00FF洋红背景完全不变，无攻击者、地面、阴影、文字和光效。禁止倒地、转身、翻向、滑步、身份漂移、肢体异常、比例变化、换装和画风变化。
```

`rooster_attack.txt`:

```text
5秒战斗攻击动画，首帧和尾帧必须精确回到输入图的同一Idle姿势。严格保持公鸡的脸、鸡冠、肉垂、喙、翅膀、双腿、尾羽、身体比例、夸张图形化轮廓、水墨水彩笔触、颜色和构图不变。公鸡始终位于画面左侧并保持头与身体朝画面右侧的3/4姿势，不转身、不翻向。0.00-0.70秒保持骄傲警觉的Idle；0.70秒快速压低重心，收紧翅膀和颈部蓄力；1.10秒向画面右侧完成一次短促有力的啄击，喙尖清楚经过画面中央命中点；1.10-1.35秒完成冲击；1.35-4.30秒慢速回收，鸡冠、肉垂、翅膀和尾羽产生受控延迟摆动；4.30-5.00秒精确恢复首帧骄傲Idle。镜头完全固定，纯#FF00FF洋红背景完全不变，无地面、阴影、文字和光效。禁止拟人叉腰、走路、双足滑动、跳跃、翻向、变成其他鸟类、额外肢体、结构变化和画风变化。
```

`rooster_hit.txt`:

```text
5秒战斗受击动画，首帧和尾帧必须精确回到输入图的同一Idle姿势。严格保持公鸡的脸、鸡冠、肉垂、喙、翅膀、双腿、尾羽、身体比例、夸张图形化轮廓、水墨水彩笔触、颜色和构图不变。公鸡始终位于画面左侧并保持头与身体朝画面右侧的3/4姿势，不转身、不翻向。0.00-0.70秒保持Idle；0.70秒感受到来自画面右侧的攻击并迅速绷紧；1.10秒出现清楚受击峰值，身体快速向画面左侧压缩后退但双脚锚点稳定，一侧翅膀短暂张开；1.10-1.35秒完成短促冲击；1.35-4.30秒慢速恢复，鸡冠、肉垂和尾羽在身体之后延迟抖动并逐渐停止；4.30-5.00秒强撑着精确恢复首帧骄傲Idle。镜头完全固定，纯#FF00FF洋红背景完全不变，无攻击者、地面、阴影、文字和光效。禁止倒地、走路、滑步、翻向、变成其他鸟类、额外肢体、结构变化和画风变化。
```

`status_buff_generic.txt`:

```text
5秒通用正向Buff特效动画，纯#FF00FF洋红正方形画布，首帧和尾帧必须是完全相同的纯洋红空白图。画面中禁止出现人物、动物、场景和文字。0.00-0.70秒保持完全空白；0.70-1.10秒在画布正中央快速形成一个尺寸较大、扁平简化、断续不闭合的金黄色水墨光环，少量简化的小叶片和黄色水墨小十字星沿环流转并向上升起；1.10秒达到最清楚的峰值，中央出现一个大型、清楚、扁平、通用的正向状态徽记；1.10-3.80秒光环与小元素缓慢流动并逐渐消散；3.80-5.00秒恢复完全纯洋红空白。特效始终在画布中央，图形简洁、墨迹笔触清楚、没有复杂装饰、没有镜头运动、没有地面和阴影，背景颜色绝不漂移。
```

`status_debuff_generic.txt`:

```text
5秒通用负向Debuff特效动画，纯#FF00FF洋红正方形画布，首帧和尾帧必须是完全相同的纯洋红空白图。画面中禁止出现人物、动物、场景和文字。0.00-0.70秒保持完全空白；0.70-1.10秒在画布正中央快速形成一个尺寸较大、扁平简化、断续不闭合的紫灰色与暗红色水墨光环，少量简化的碎片、墨点和短笔触沿环流转并向下沉降；1.10秒达到最清楚的峰值，中央出现一个大型、清楚、扁平、通用的负向状态徽记；1.10-3.80秒光环与小元素缓慢下沉并逐渐消散；3.80-5.00秒恢复完全纯洋红空白。特效始终在画布中央，图形简洁、墨迹笔触清楚、没有复杂装饰、没有镜头运动、没有地面和阴影，背景颜色绝不漂移。
```

`impact_ink_generic.txt`:

```text
5秒通用攻击墨迹爆点动画，纯#FF00FF洋红正方形画布，首帧和尾帧必须是完全相同的纯洋红空白图。画面中禁止出现人物、动物、武器、状态符号、场景和文字。0.00-1.70秒保持完全空白；1.70-2.20秒少量墨迹从四周快速压缩聚集到画布正中央；2.20秒达到唯一且最强的爆点峰值：无方向性的径向水墨冲击，中央为米白色核心，外围只有4到6根粗短的炭黑色干笔尖刺和少量赭黄色墨点；2.20-4.60秒墨迹碎片慢速飞散、变淡并消失；4.60-5.00秒恢复完全纯洋红空白。构图扁平、极简、图形化，峰值清楚有力，镜头完全固定，背景颜色绝不漂移，无连续光束、无火焰、无烟雾场景。
```

Run:

```powershell
@'
from pathlib import Path
from PIL import Image
path = Path(r"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png")
path.parent.mkdir(parents=True, exist_ok=True)
Image.new("RGB", (720, 720), (255, 0, 255)).save(path)
'@ | python -
```

Expected: `magenta_blank_720.png` exists and is exactly 720×720 RGB.

- [ ] **Step 4: Create the seven-entry manifest**

Write `pilot_manifest.json` with this order and cost assignment:

```json
{
  "budget_baseline": 7145,
  "retry_credit_cap": 565,
  "assets": [
    {"id":"hero_attack","model":"seedance2.0_vip","expected_credits":70,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png","last_frame":"SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/hero_attack.txt"},
    {"id":"hero_hit","model":"seedance1.5pro","expected_credits":40,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png","last_frame":"SourceAssets/CharacterVisuals/final_selected_v1/00_hero.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/hero_hit.txt"},
    {"id":"rooster_attack","model":"seedance2.0_vip","expected_credits":70,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/RouteEnemies/final_selected_v1/01_rooster.png","last_frame":"SourceAssets/RouteEnemies/final_selected_v1/01_rooster.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/rooster_attack.txt"},
    {"id":"rooster_hit","model":"seedance1.5pro","expected_credits":40,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/RouteEnemies/final_selected_v1/01_rooster.png","last_frame":"SourceAssets/RouteEnemies/final_selected_v1/01_rooster.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/rooster_hit.txt"},
    {"id":"status_buff_generic","model":"seedance1.5pro","expected_credits":40,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","last_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/status_buff_generic.txt"},
    {"id":"status_debuff_generic","model":"seedance1.5pro","expected_credits":40,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","last_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/status_debuff_generic.txt"},
    {"id":"impact_ink_generic","model":"seedance1.5pro","expected_credits":40,"duration_seconds":5,"resolution":"720p","first_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","last_frame":"SourceAssets/AnimationProduction/pilot_v1/inputs/magenta_blank_720.png","prompt_file":"SourceAssets/AnimationProduction/pilot_v1/prompts/impact_ink_generic.txt"}
  ]
}
```

- [ ] **Step 5: Run the contract test and verify it passes**

Run:

```powershell
python scripts/test_seedance_seven_asset_pilot.py
```

Expected: `OK` with one passing test.

### Task 2: Establish the credit ledger and paid-submission gate

**Files:**
- Create: `SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json`
- Modify: `scripts/test_seedance_seven_asset_pilot.py`

- [ ] **Step 1: Add a failing ledger test**

Require `budget_baseline`, `production_budget`, `retry_credit_cap`, `rejected_credit_spend`, `accepted_credit_spend`, and an empty `submissions` list. Assert that the retry cap is 565, production budget is 6580, and `budget_baseline - production_budget >= retry_credit_cap` when measured against the conservative 7145 budget baseline.

- [ ] **Step 2: Run the test and verify it fails**

Run `python scripts/test_seedance_seven_asset_pilot.py`.

Expected: failure because `credit_ledger.json` does not exist.

- [ ] **Step 3: Query live credits without submitting work**

Run:

```powershell
& 'C:\Users\shxuw\AppData\Local\Temp\gamexxk-dreamina-cli-review\dreamina.exe' user_credit
```

Expected: JSON with `total_credit` of at least 7145. If it is below 7145, stop without submitting.

- [ ] **Step 4: Create the ledger**

Initialize conservative budgeting with:

```json
{
  "budget_baseline": 7145,
  "live_credit_at_start": 7150,
  "production_budget": 6580,
  "retry_credit_cap": 565,
  "accepted_credit_spend": 0,
  "rejected_credit_spend": 0,
  "submissions": []
}
```

If the live query differs, only `live_credit_at_start` may change; `budget_baseline`, `production_budget`, and `retry_credit_cap` remain locked.

- [ ] **Step 5: Run the ledger test and verify it passes**

Run `python scripts/test_seedance_seven_asset_pilot.py`.

Expected: all tests pass.

### Task 3: Submit and verify the two player-facing clips

**Files:**
- Create: `SourceAssets/AnimationProduction/pilot_v1/hero_attack/`
- Create: `SourceAssets/AnimationProduction/pilot_v1/hero_hit/`
- Modify: `SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json`

- [ ] **Step 1: Submit `hero_attack`**

Run the exact manifest inputs with `seedance2.0_vip`, 720p, 5 seconds, and `--poll=20`. Save stdout immediately so the submit ID survives a terminal interruption.

```powershell
$dreamina = 'C:\Users\shxuw\AppData\Local\Temp\gamexxk-dreamina-cli-review\dreamina.exe'
$prompt = Get-Content -LiteralPath 'SourceAssets\AnimationProduction\pilot_v1\prompts\hero_attack.txt' -Raw
& $dreamina frames2video --first='SourceAssets\CharacterVisuals\final_selected_v1\00_hero.png' --last='SourceAssets\CharacterVisuals\final_selected_v1\00_hero.png' --prompt=$prompt --video_resolution=720p --duration=5 --model_version=seedance2.0_vip --poll=20 | Tee-Object -FilePath 'SourceAssets\AnimationProduction\pilot_v1\hero_attack\submission.txt'
```

Expected: one accepted task and a submit ID; expected debit 70.

- [ ] **Step 2: Poll and download `hero_attack`**

Run `query_result --submit_id=<recorded-id> --download_dir='SourceAssets/AnimationProduction/pilot_v1/hero_attack'` in bounded calls until the task reaches success or failure. Do not submit another task while status is pending.

- [ ] **Step 3: Verify `hero_attack` technically and visually**

Use `ffprobe` to assert 720×720 dimensions and approximately 5 seconds. Inspect frames near 0.00, 0.70, 1.10, 1.35, 4.30, and 5.00 seconds for left-facing identity, bamboo-weapon stability, fixed camera/background, exact impact timing, and return to Idle. Record `accepted` or `rejected` in the ledger.

- [ ] **Step 4: Submit, download, and verify `hero_hit`**

Repeat the bounded sequence with `seedance1.5pro` and `hero_hit.txt`. Verify a left-origin hit, fast rightward recoil at 1.10 seconds, delayed basket/gourd/leaves, no turn, and slow return. Expected debit 40.

### Task 4: Submit and verify the two rooster clips

**Files:**
- Create: `SourceAssets/AnimationProduction/pilot_v1/rooster_attack/`
- Create: `SourceAssets/AnimationProduction/pilot_v1/rooster_hit/`
- Modify: `SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json`

- [ ] **Step 1: Submit, download, and verify `rooster_attack`**

Use `01_rooster.png` for identical first/last frames, `rooster_attack.txt`, `seedance2.0_vip`, 720p, and 5 seconds. Verify right-facing 3/4 orientation, fixed feet, compressed anticipation, rightward beak impact at 1.10 seconds, delayed comb/wattle/tail motion, and slow proud recovery. Expected debit 70.

- [ ] **Step 2: Submit, download, and verify `rooster_hit`**

Use `rooster_hit.txt` and `seedance1.5pro`. Verify the hit originates from the right, recoil goes left at 1.10 seconds, one wing opens briefly, secondary feathers lag, and identity/orientation remain stable. Expected debit 40.

### Task 5: Submit and verify the three reusable VFX clips

**Files:**
- Create: `SourceAssets/AnimationProduction/pilot_v1/status_buff_generic/`
- Create: `SourceAssets/AnimationProduction/pilot_v1/status_debuff_generic/`
- Create: `SourceAssets/AnimationProduction/pilot_v1/impact_ink_generic/`
- Modify: `SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json`

- [ ] **Step 1: Submit, download, and verify `status_buff_generic`**

Use `magenta_blank_720.png` as both frames and `seedance1.5pro`. Verify blank beginning/end, centered flat gold broken ring, upward leaves/crosses, a large generic positive emblem at 1.10 seconds, and no person/text/background drift. Expected debit 40.

- [ ] **Step 2: Submit, download, and verify `status_debuff_generic`**

Use the same blank frames and `seedance1.5pro`. Verify blank beginning/end, centered purple-gray/dark-red broken ring, downward fragments/ink drops, a large generic negative emblem at 1.10 seconds, and no person/text/background drift. Expected debit 40.

- [ ] **Step 3: Submit, download, and verify `impact_ink_generic`**

Use the same blank frames and `seedance1.5pro`. Verify pure-magenta hold through 1.70 seconds, rapid compression, directionless radial peak at exactly 2.20 seconds, 4–6 charcoal spikes, rice-white core, sparse ochre droplets, and return to blank. Expected debit 40.

### Task 6: Finalize the pilot evidence without spending more credits

**Files:**
- Create: `SourceAssets/AnimationProduction/pilot_v1/results.json`
- Create: `SourceAssets/AnimationProduction/pilot_v1/contact_sheets/`
- Modify: `docs/production/2026-07-24-battle-animation-pilot.md`

- [ ] **Step 1: Validate all successful media**

For every downloaded MP4, run `ffprobe -v error -show_entries stream=width,height,r_frame_rate -show_entries format=duration -of json <file>`. Require 720×720 and approximately 5.0 seconds; record actual frame rate and duration.

- [ ] **Step 2: Produce timestamp contact sheets**

Extract representative PNGs at the required timing anchors. Unit clips use 0.00, 0.70, 1.10, 1.35, 4.30, and 4.90 seconds; status clips use 0.00, 0.70, 1.10, 3.80, and 4.90 seconds; impact uses 0.00, 1.70, 2.20, 4.60, and 4.90 seconds.

- [ ] **Step 3: Write `results.json`**

For each asset record model, submit ID, terminal status, expected and actual debit, MP4 path, dimensions, duration, frame rate, visual verdict, and rejection reason. Assert accepted plus rejected spending matches the observed credit delta.

- [ ] **Step 4: Update production status**

Append a dated “Seven-asset pilot” section to `docs/production/2026-07-24-battle-animation-pilot.md` with the seven verdicts, total accepted cost, rejected/retry spend, remaining retry allowance, and whether the project may proceed to the remaining 132 production clips.

- [ ] **Step 5: Run final non-paid verification**

Run:

```powershell
python scripts/test_seedance_seven_asset_pilot.py
git diff --check -- docs/superpowers/specs/2026-07-25-battle-animation-seven-asset-pilot-design.md docs/superpowers/plans/2026-07-25-battle-animation-seven-asset-pilot.md scripts/test_seedance_seven_asset_pilot.py SourceAssets/AnimationProduction/pilot_v1/pilot_manifest.json SourceAssets/AnimationProduction/pilot_v1/credit_ledger.json docs/production/2026-07-24-battle-animation-pilot.md
```

Expected: tests pass and `git diff --check` prints no errors.
