# GameXXK UI 壳体非破坏式拆分实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不覆盖用户已调整内容的前提下，把商店页整屏合成壳体拆成一个独立城镇背景、八个透明窗口组件和可恢复的隐藏原图层，并写回用户最后保存的 UI Master PSD。

**Architecture:** 先从锁定的 1672×941 合成壳体提取纸张组件蒙版，同时将无 UI 城镇底图做线性色调匹配；再输出 1920×1080 对应的紧边透明 PNG、放置清单和复合对比图。最后通过独立 Photoshop JSX 只修改现有 PSD 的 `07_商店交易/10_Background`，保留用户文字、图标、位置和状态组，并在写回前创建可校验的 PSD 备份。

**Tech Stack:** Python 3.12、Pillow、NumPy、JSON、Node.js、Photoshop ExtendScript/COM、PowerShell。

**Testing policy:** 用户明确要求美术问题不做 TDD。本计划不增加红灯优先测试；每个美术输出步骤执行结构检查、像素检查和原分辨率视觉复核。不得刷新 `RuntimeAssets`，不得修改 UE。

---

## 文件结构

- Create `scripts/split_gamexxk_ui_shell.py`: 生成色调匹配背景、八个透明窗口组件、复合预览、差异预览和放置清单。
- Create `scripts/ui_psd_pipeline/build-shell-split-jsx.js`: 根据放置清单和 UI Master 页面原点生成只修改现有 PSD 的 JSX。
- Create `scripts/ui_psd_pipeline/run-shell-split.ps1`: 校验用户 PSD 已保存、确认备份、运行 JSX 并检查回执。
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/*.png`: 八个透明窗口组件和一个 1920×1080 色调匹配背景。
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json`: 图层名、源框、目标放置框、文件路径和透明度统计。
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分复合.png`.
- Create `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分差异.png`.
- Modify `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`: 只追加拆分图层、隐藏原合成壳体，不重新生成其他图层。
- Create `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.before-shell-split.<timestamp>.psd`: 用户 PSD 的字节级备份。
- Create `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split.validation.json`: Photoshop 写回回执与备份哈希。

## 固定组件定义

```python
COMPONENTS = (
    {"name": "01_主角身份条", "file": "identity_panel.png", "box": (0, 0, 500, 180), "seed": (100, 90)},
    {"name": "02_顶部铜钱条", "file": "currency_panel.png", "box": (975, 0, 1672, 130), "seed": (1320, 60)},
    {"name": "03_导航圆底_背包", "file": "nav_disc_backpack.png", "box": (10, 170, 180, 330), "seed": (85, 245)},
    {"name": "04_导航圆底_伙伴", "file": "nav_disc_companion.png", "box": (10, 300, 180, 455), "seed": (85, 375)},
    {"name": "05_导航圆底_图鉴", "file": "nav_disc_codex.png", "box": (10, 425, 180, 585), "seed": (85, 505)},
    {"name": "06_导航圆底_任务", "file": "nav_disc_task.png", "box": (10, 550, 180, 715), "seed": (85, 635)},
    {"name": "07_导航圆底_路线", "file": "nav_disc_route.png", "box": (10, 680, 180, 845), "seed": (85, 765)},
    {"name": "08_中央商店窗口", "file": "main_shop_panel.png", "box": (260, 140, 1545, 900), "seed": (800, 500)},
)
```

### Task 1: 锁定并备份用户最后保存的 PSD

**Files:**
- Read: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.before-shell-split.<timestamp>.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split-source.json`

- [ ] **Step 1: 记录当前 PSD 的大小、时间和 SHA-256**

```powershell
$psd = Resolve-Path -LiteralPath 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd'
$item = Get-Item -LiteralPath $psd
$hash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psd).Hash.ToLowerInvariant()
[pscustomobject]@{ Path=$item.FullName; Length=$item.Length; LastWriteTime=$item.LastWriteTime; Sha256=$hash } | Format-List
```

Expected: PSD 存在、长度大于 50 MB，修改时间对应用户最后一次保存。

- [ ] **Step 2: 确认 Photoshop 目标文档没有未保存修改**

```powershell
powershell -NoProfile -Command "$app = New-Object -ComObject Photoshop.Application; foreach ($doc in $app.Documents) { if ($doc.FullName -eq (Resolve-Path 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd').Path -and -not $doc.Saved) { throw 'Target PSD has unsaved changes' } }; 'PSD save state ok'"
```

Expected: `PSD save state ok`。若未保存，停止并让用户保存，不从旧磁盘状态备份。

- [ ] **Step 3: 创建时间戳备份并校验哈希一致**

```powershell
$source = (Resolve-Path -LiteralPath 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd').Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = Join-Path (Split-Path $source) "GameXXK_UI_Master_V1.before-shell-split.$stamp.psd"
Copy-Item -LiteralPath $source -Destination $backup
if ((Get-FileHash $source).Hash -ne (Get-FileHash $backup).Hash) { throw 'PSD backup hash mismatch' }
$backup
```

Expected: 打印绝对备份路径，源文件与备份哈希一致。

- [ ] **Step 4: 写入源版本回执**

```powershell
$source = (Resolve-Path -LiteralPath 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd').Path
$backup = Get-ChildItem -LiteralPath (Split-Path $source) -Filter 'GameXXK_UI_Master_V1.before-shell-split.*.psd' | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$receipt = [ordered]@{
    version = 1
    sourcePsd = $source
    sourceSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $source).Hash.ToLowerInvariant()
    sourceLength = (Get-Item -LiteralPath $source).Length
    backupPsd = $backup.FullName
    backupSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $backup.FullName).Hash.ToLowerInvariant()
}
$receipt | ConvertTo-Json | Set-Content -Encoding UTF8 -LiteralPath 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split-source.json'
```

Expected: source and backup hashes in the JSON are identical.

### Task 2: 生成背景和八个透明窗口组件

**Files:**
- Create: `scripts/split_gamexxk_ui_shell.py`
- Read: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_large_panel_clean.png`
- Read: `SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png`
- Create: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/*`

- [ ] **Step 1: 实现 CLI 和源文件校验**

```python
parser = argparse.ArgumentParser()
parser.add_argument("--shell", type=Path, required=True)
parser.add_argument("--background", type=Path, required=True)
parser.add_argument("--output-root", type=Path, required=True)
parser.add_argument("--review-root", type=Path, required=True)
args = parser.parse_args()
report = build_shell_components(args.shell, args.background, args.output_root, args.review_root)
print(json.dumps(report, ensure_ascii=False))
```

Require both inputs to be 1672×941. Use the exact `COMPONENTS` tuple above.

- [ ] **Step 2: 实现无 UI 背景线性色调匹配**

Exclude all component boxes. Sample every twentieth remaining pixel with mean brightness between 30 and 220, then fit each RGB channel independently:

```python
for channel in range(3):
    slope, intercept = np.polyfit(source[:, channel], target[:, channel], 1)
    matched[:, :, channel] = clean[:, :, channel] * slope + intercept
```

Require safe-region mean absolute error ≤ 9 and p95 ≤ 20. Stop before export if either threshold fails.

- [ ] **Step 3: 实现纸张主体与阴影蒙版**

Inside each component box, create a core with `(R > 135) & (G > 110) & (B > 85) & (R - B > 18)`. Keep the four-connected component containing the local seed, close three-pixel gaps with `MaxFilter(5)` then `MinFilter(5)`, and expand eligible edge support with `MaxFilter(19)`. Outside the opaque paper core, retain only genuine luminance darkening and reconstruct it as a neutral ink-coloured shadow; absolute RGB residuals from the fitted background must not become alpha because uncompositing them produces cyan/magenta fringes.

```python
luma_shell = shell_crop.astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722])
luma_background = background_crop.astype(np.float32) @ np.array([0.2126, 0.7152, 0.0722])
shadow_alpha = np.clip(
    (luma_background - luma_shell - 2.0) / np.maximum(luma_background - 18.0, 1.0) * 255.0,
    0,
    255,
).astype(np.uint8)
shadow_alpha[~support | core] = 0
alpha = np.where(core, 255, shadow_alpha)
foreground[core] = shell_crop[core]
foreground[~core] = np.array([22, 18, 14], dtype=np.uint8)
```

Reject a mask when alpha touches its source crop boundary; adjust only the offending component box.

- [ ] **Step 4: 输出 1920×1080 背景、紧边组件和放置清单**

Resize the full canvas to 1920×1080. Trim every component to alpha `> 2` bounds plus two transparent pixels. The manifest must contain `version`, `sourceCanvas`, `targetCanvas`, one `background` record, exactly eight `components`, each component's measured target `box`, source box, seed and alpha bounds.

- [ ] **Step 5: 运行生成器**

```powershell
python scripts/split_gamexxk_ui_shell.py --shell SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/hero_backpack_large_panel_clean.png --background SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png --output-root SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents --review-root SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop
```

Expected: JSON reports `componentCount: 8`, background `[1920,1080]`, safe MAE ≤ 9 and both review PNG paths.

### Task 3: 写回 PSD 前的素材门

**Files:**
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/*.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分复合.png`
- Review: `SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分差异.png`

- [ ] **Step 1: 结构检查**

```powershell
@'
import json
from pathlib import Path
from PIL import Image
root = Path("SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents")
data = json.loads((root / "shell-components-manifest.json").read_text(encoding="utf-8"))
assert data["targetCanvas"] == [1920, 1080]
assert len(data["components"]) == 8
assert Image.open(root / data["background"]["file"]).size == (1920, 1080)
for record in data["components"]:
    image = Image.open(root / record["file"]).convert("RGBA")
    assert image.getchannel("A").getbbox() is not None
    assert 0 in image.getchannel("A").getdata()
print("shell component structure ok")
'@ | python -
```

- [ ] **Step 2: 原分辨率视觉检查**

Require recognizable paper fiber, torn edge, ink line and shadow; transparent navigation corners; no roof, stone, foliage or bridge pixels outside paper; no baked title, product, detail or button in the main panel; no bright seam in the difference review. If any item fails, regenerate only that component and do not invoke Photoshop.

- [ ] **Step 3: 提交生成器和已核准素材**

```powershell
git add -- scripts/split_gamexxk_ui_shell.py SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分复合.png SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop/07_商店交易_壳体拆分差异.png
git commit -m "art: split the meta shop shell into reusable layers"
```

### Task 4: 生成只修改现有 PSD 的 Photoshop 注入脚本

**Files:**
- Create: `scripts/ui_psd_pipeline/build-shell-split-jsx.js`
- Create: `scripts/ui_psd_pipeline/run-shell-split.ps1`
- Read: `tmp/meta-shop-psd-review/master-manifest.json`
- Read: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json`

- [ ] **Step 1: 实现 JSX 生成器参数与校验**

The Node CLI must accept `--psd`, `--components`, `--master-manifest`, `--output`, and `--receipt`. Resolve `pages[].group === "07_商店交易"`; require origin `[4080,1200]`. Require one background and exactly eight components. Reject missing files before writing JSX.

- [ ] **Step 2: 生成非破坏式 Photoshop 操作**

The generated JSX must perform these exact operations:

1. attach to or open the target PSD;
2. find top-level group `07_商店交易` and child `10_Background`;
3. stop if `00_ShellComponents` already exists;
4. recursively find the layer name containing `approved_v2_shop_shell`;
5. rename it `99_原始合成壳体_备份` and hide it;
6. create `00_ShellComponents` under `10_Background`;
7. import the full background at `[4080,1200]`;
8. import eight trimmed components at `[4080 + box.x, 1200 + box.y]`;
9. preserve the exact Chinese manifest names;
10. save the existing PSD in place without flattening;
11. write the receipt with a local `jsonQuote` serializer, never `JSON.stringify`, because the installed Photoshop ExtendScript runtime has no global JSON object.

The JSX must never delete, rasterize or recreate `20_GlobalShell`, `30_ShopPaper`, `40_ProductGrid`, `50_ProductDetail`, or state groups `71`–`74`.

- [ ] **Step 3: 实现 PowerShell 保护层**

`run-shell-split.ps1` must require `-Psd`, `-ComponentsManifest`, `-MasterManifest`, and `-Receipt`. It must fail when the PSD is missing, when no matching `before-shell-split` backup exists, when Photoshop reports unsaved target changes, or when the receipt does not report nine imported layers. Print `PSD shell split finished.` only after all checks pass.

- [ ] **Step 4: 静态验证生成 JSX**

```powershell
node scripts/ui_psd_pipeline/build-shell-split-jsx.js --psd outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd --components SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json --master-manifest tmp/meta-shop-psd-review/master-manifest.json --output tmp/meta-shop-shell-split/apply-shell-split.jsx --receipt outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split.validation.json
rg -n "00_ShellComponents|99_原始合成壳体_备份|07_商店交易|10_Background|jsonQuote" tmp/meta-shop-shell-split/apply-shell-split.jsx
if (rg -n "JSON.stringify" tmp/meta-shop-shell-split/apply-shell-split.jsx) { throw 'Unsupported JSON.stringify remains in JSX' }
```

Expected: all required names are found; `JSON.stringify` is absent.

- [ ] **Step 5: 提交注入器**

```powershell
git add -- scripts/ui_psd_pipeline/build-shell-split-jsx.js scripts/ui_psd_pipeline/run-shell-split.ps1
git commit -m "feat: inject separated shell layers into the saved PSD"
```

### Task 5: 写回用户 PSD 并验证可恢复性

**Files:**
- Modify: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd`
- Create: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split.validation.json`
- Preserve: `outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.before-shell-split.<timestamp>.psd`

- [ ] **Step 1: 再次核对源 PSD 哈希**

Compare the current PSD hash to Task 1's source receipt. If the user saved again after Task 1, stop, create a new backup from the newer PSD, and replace the source receipt with the new hash. Never write against an unrecorded version.

- [ ] **Step 2: 运行一次 Photoshop 注入**

```powershell
powershell -ExecutionPolicy Bypass -File scripts/ui_psd_pipeline/run-shell-split.ps1 -Psd outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd -ComponentsManifest SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/shell-components-manifest.json -MasterManifest tmp/meta-shop-psd-review/master-manifest.json -Receipt outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split.validation.json
```

Expected: `PSD shell split finished.` and receipt fields `page: 07_商店交易`, `group: 00_ShellComponents`, `importedLayerCount: 9`, `originalLayerHidden: true`.

- [ ] **Step 3: 验证 PSD、回执与备份**

```powershell
@'
import json
from pathlib import Path
p = Path("outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd")
r = Path("outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.shell-split.validation.json")
d = json.loads(r.read_text(encoding="utf-8-sig"))
assert p.is_file() and p.stat().st_size > 50_000_000
assert d["page"] == "07_商店交易"
assert d["group"] == "00_ShellComponents"
assert d["importedLayerCount"] == 9
assert d["originalLayerHidden"] is True
assert len(d["componentLayers"]) == 8
assert list(p.parent.glob("GameXXK_UI_Master_V1.before-shell-split.*.psd"))
print("PSD shell split validation ok")
'@ | python -
```

- [ ] **Step 4: Photoshop 人工开关检查**

Open `07_商店交易/10_Background/00_ShellComponents` and toggle each layer. Moving one component must reveal only the matched town background. Enabling `99_原始合成壳体_备份` must restore the pre-split appearance. Text, icons and state layers must remain unchanged.

### Task 6: 最终交付与停工门

**Files:**
- Deliver: modified PSD and timestamped backup PSD
- Deliver: `SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/`
- Deliver: recomposed and difference review PNGs

- [ ] **Step 1: 最终范围检查**

```powershell
git status --short -- scripts/split_gamexxk_ui_shell.py scripts/ui_psd_pipeline SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MetaShop outputs/UI_PSD/Candidates
```

Expected: no `RuntimeAssets`, UE `Content`, `Source/GameXXK`, or Task 9 probe file was modified by this plan.

- [ ] **Step 2: 交付路径并停止**

Provide clickable absolute paths to the modified PSD, backup PSD, component directory, manifest, recomposed review and difference review. State that RuntimeAssets and UE were not modified. Wait for the user to finish further PSD adjustments; do not migrate to UE in the same turn.
