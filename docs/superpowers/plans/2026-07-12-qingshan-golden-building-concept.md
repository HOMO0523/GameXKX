# 青山镇黄金建筑 JSON 原画实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** 打通 `BLD_QS_S_A_HOUSE` 的“共享建筑风格母版 → 独立资产 JSON → 可复现提示包 → 正式原画 → 版本与 SHA-256 登记”链路，产出可供用户审核的首个 Tripo 输入金样。

**Architecture:** 保留 `qingshan_environment_assets.py` 作为资产目录、生成预算、证据哈希和事务写入的唯一权威；新增一个小型建筑概念编译器，把共享母版与单栋 `asset.json` 合并为确定性的提示包。目录构建器继续拥有 `asset.json` 的声明字段，输出登记 CLI 继续拥有生成证据字段，避免手工编辑与重新构建互相覆盖。

**Tech Stack:** Python 3.11+ 标准库、`unittest`、JSON、现有青山镇资产目录工具、Codex `imagegen` 技能。

**Scope:** 本计划只做到黄金样板原画审核，不上传 Tripo、不生成模型、不改 UE 关卡。其余五栋建筑、Tripo 高精度/约 50k 四边面重拓扑、贴图和 UE5.8 BSDF 材质分别在金样批准后制定后续计划。

---

## Task 1: 建筑概念编译器的失败测试

**Files:**
- Create: `scripts/test_qingshan_building_concepts.py`
- Create: `scripts/qingshan_building_concepts.py`

- [ ] **Step 1: 写共享母版校验失败测试**

覆盖以下契约：`style_id == QS_InkToon_Building_v2`、`schema_version == 1`、固定高位三分之四相机、暖米纸纯背景、单主体、无文字标注、暖黄色宣纸窗、粗门窗/梁柱、夸张但不过度扭曲、禁止逐片瓦片和微小格栅。

```python
def test_building_style_rejects_missing_warm_paper_window_rule(self):
    style = valid_style()
    style["material_contract"].pop("window_fill")
    with self.assertRaisesRegex(ConceptError, "window_fill"):
        validate_building_style(style)
```

- [ ] **Step 2: 写资产与母版不一致测试**

验证错误 `style_profile`、多主体、黑窗、重复瓦片、细窗格、缺少可见底座、缺少入口朝向，以及 S-A 不是独立单图时均被拒绝。

```python
def test_golden_asset_rejects_black_windows(self):
    asset = valid_golden_asset()
    asset["generation"]["subject"] += "; black window panes"
    with self.assertRaisesRegex(ConceptError, "warm yellow paper windows"):
        validate_golden_asset(asset, valid_style())
```

- [ ] **Step 3: 写确定性提示包测试**

相同 JSON 必须生成完全相同的 UTF-8/LF 字节；提示包必须记录母版路径、母版 SHA-256、资产路径、资产 SHA-256、完整正向提示、负向提示、构图契约和预期输出文件名。

```python
first = compile_prompt_packet(root, asset_id="BLD_QS_S_A_HOUSE", version="v001")
second = compile_prompt_packet(root, asset_id="BLD_QS_S_A_HOUSE", version="v001")
self.assertEqual(canonical_json_bytes(first), canonical_json_bytes(second))
```

- [ ] **Step 4: 运行测试并确认因模块尚未实现而失败**

Run:

```powershell
python -m unittest scripts.test_qingshan_building_concepts -v
```

Expected: `ModuleNotFoundError` 或缺少目标 API 的失败；不能是测试语法错误。

- [ ] **Step 5: 创建最小模块接口让测试进入契约失败阶段**

模块先声明：

```python
class ConceptError(ValueError):
    pass

def validate_building_style(data: dict) -> None: ...
def validate_golden_asset(asset: dict, style: dict) -> None: ...
def compile_prompt_packet(root: Path, asset_id: str, version: str) -> dict: ...
def canonical_json_bytes(data: dict) -> bytes: ...
```

- [ ] **Step 6: 提交失败测试与接口骨架**

```powershell
git add -- scripts/test_qingshan_building_concepts.py scripts/qingshan_building_concepts.py
git commit -m "test: define golden building concept contract"
```

## Task 2: 共享建筑风格母版

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_Building_v2.json`
- Modify: `scripts/qingshan_building_concepts.py`
- Test: `scripts/test_qingshan_building_concepts.py`

- [ ] **Step 1: 添加母版存在性和路径安全测试**

母版只能引用项目根内的规范 POSIX 相对路径；引用图片必须存在且 SHA-256 匹配。拒绝绝对路径、反斜杠、`..` 和符号链接逃逸。

- [ ] **Step 2: 编写 `QS_InkToon_Building_v2.json`**

至少包含：

```json
{
  "schema_version": 1,
  "style_id": "QS_InkToon_Building_v2",
  "inherits": "style/QS_InkToon_v1.json",
  "camera_contract": {
    "view": "fixed_high_three_quarter",
    "single_complete_object": true,
    "visible_base": true
  },
  "material_contract": {
    "wall": "warm_off_white_plaster",
    "window_fill": "warm_yellow_translucent_rice_paper",
    "outline": "dark_teal_variable_dry_brush"
  },
  "shape_contract": {
    "exaggeration": "playful_asymmetric_moderate",
    "doors_windows": "few_large_thick_frames",
    "posts_beams": "chunky_single_readable_masses",
    "roof": "one_bold_bent_silhouette_no_tile_repetition"
  }
}
```

还要把用户确认的禁止项写入 `negative_prompt`：黑色窗面、逐片瓦片、细密窗格、三层细柱头、过度碎裂、点状噪声、过度怪异、镜像对称、文字、标注、多个建筑、场景拼贴。

- [ ] **Step 3: 实现严格母版校验**

采用字段白名单和精确类型检查；错误消息包含 JSON 字段路径。继承的 `QS_InkToon_v1.json` 与参考图都要计算并验证摘要，不能只验证文件存在。

- [ ] **Step 4: 运行针对性测试**

```powershell
python -m unittest scripts.test_qingshan_building_concepts.BuildingStyleTests -v
```

Expected: 全部通过。

- [ ] **Step 5: 提交母版与校验器**

```powershell
git add -- SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_Building_v2.json scripts/qingshan_building_concepts.py scripts/test_qingshan_building_concepts.py
git commit -m "feat: add qingshan building style master"
```

## Task 3: 把 S-A 金样声明接入目录构建器

**Files:**
- Modify: `scripts/build_qingshan_environment_catalog.py`
- Modify: `scripts/test_qingshan_environment_assets.py`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/asset.json` (由构建器生成)
- Test: `scripts/test_qingshan_building_concepts.py`

- [ ] **Step 1: 写构建器输出契约测试**

断言 S-A 使用 `QS_InkToon_Building_v2`，结构为紧凑两层小民居，橙色弯曲双坡屋顶、粗双开门、少量宽框暖黄宣纸窗、粗梁柱、偏置入口；原画只要求一个 `hero_3q` 正式 Tripo 输入视图。`structure_sheet` 留到建模审核阶段，不与首张 Tripo 输入混在一图。

- [ ] **Step 2: 写 Tripo/拓扑声明测试**

S-A 的正式模型管线改为：`Tripo_high_precision`、目标约 `50000` 四边面、允许范围 `45000..55000`、`quad_dominant`、先重拓扑后生成最终材质。此处只是 JSON 声明，`workflow_gates.tripo_allowed` 与 `ue_import_allowed` 仍必须为 `false`。

- [ ] **Step 3: 修改构建器的 S-A 专属覆盖**

不要把全目录所有建筑强制切到 v2。为 S-A 添加显式覆盖，保证重新运行构建器不会把金样退回 `QS_InkToon_v1` 或 Blender 代理管线。保持 `_overlay_registered_output()` 对 `output_path`、`sha256`、`version`、生成追踪字段和调用预算的现有保护。

- [ ] **Step 4: 生成并校验目录**

```powershell
python scripts/build_qingshan_environment_catalog.py --root SourceAssets/TownPCG/QingshanEnvironment
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
```

Expected: 构建成功；校验 JSON 输出包含 `"asset_count": 35`，没有证据摘要漂移。

- [ ] **Step 5: 运行目录回归测试**

```powershell
python -m unittest scripts.test_qingshan_environment_assets -v
```

Expected: 全部通过。

- [ ] **Step 6: 提交金样声明**

```powershell
git add -- scripts/build_qingshan_environment_catalog.py scripts/test_qingshan_environment_assets.py SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/asset.json
git commit -m "feat: define qingshan small-house golden asset"
```

## Task 4: 确定性提示包 CLI

**Files:**
- Modify: `scripts/qingshan_building_concepts.py`
- Modify: `scripts/test_qingshan_building_concepts.py`
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.prompt.json` (由 CLI 生成)

- [ ] **Step 1: 写 CLI 失败测试**

拒绝未知资产、非 `v001..v003` 版本、目录外输出、已经存在的提示包、未通过目录校验的资产，以及母版/资产摘要在编译中途变化的情况。

- [ ] **Step 2: 实现 `--compile`**

```powershell
python scripts/qingshan_building_concepts.py --root SourceAssets/TownPCG/QingshanEnvironment --compile BLD_QS_S_A_HOUSE hero_3q v001
```

CLI 先调用现有 `validate_catalog()`，然后加载母版与资产，执行专属校验，最后以原子写入方式生成提示包。已有文件不覆盖；修订必须使用更高版本。

- [ ] **Step 3: 固定提示的优先级**

完整提示按以下顺序编译，避免聊天临时措辞改变结果：身份与用途 → 单栋结构 → 共享水墨卡通风格 → 中等夸张/不规整 → 暖黄宣纸窗 → 镜头与背景 → 3D 可解性 → 禁止项。提示中必须明确“single isolated building, not a town scene, no labels or text”。

- [ ] **Step 4: 运行测试和 CLI**

```powershell
python -m unittest scripts.test_qingshan_building_concepts -v
python scripts/qingshan_building_concepts.py --root SourceAssets/TownPCG/QingshanEnvironment --compile BLD_QS_S_A_HOUSE hero_3q v001
```

Expected: 测试通过并只生成一个 v001 提示包；再次运行因禁止覆盖而失败。

- [ ] **Step 5: 提交编译器和提示包**

```powershell
git add -- scripts/qingshan_building_concepts.py scripts/test_qingshan_building_concepts.py SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.prompt.json
git commit -m "feat: compile deterministic building concept prompts"
```

## Task 5: 由提示包生成并登记正式金样原画

**Files:**
- Create: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.png`
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/asset.json` (由登记 CLI 事务更新)
- Modify: `SourceAssets/TownPCG/QingshanEnvironment/manifest.json` (由登记 CLI 事务更新)

- [ ] **Step 1: 使用 `imagegen` 技能读取提示包生成一张图**

生成时不得追加会改变结构的聊天提示。允许的执行包装只有：使用提示包的完整正向/负向内容、输出 PNG、单张候选。不要把旧 Tripo 输入图直接改名；旧图只作为构图与体量参考，新图必须是本次 JSON 链路的产物。

- [ ] **Step 2: 做机器可检查的图像预检**

确认 PNG 可解码、尺寸至少 1024×1024、alpha/背景没有意外裁切、文件非空，并计算 SHA-256。失败时不登记，按 v002 修订且保留 v001 文件与提示包。

- [ ] **Step 3: 做人工视觉门禁**

必须同时满足：单栋完整三分之四视角、底部完整、橙色弯曲屋顶、粗门窗与梁柱、窗面暖黄、结构不对称、夸张程度比旧规整房屋更强但没有怪过头、无逐片瓦片、无细窗格、无文字标注、无桥/树/道路/其他建筑、可被 Tripo 解成稳定体块。

- [ ] **Step 4: 将通过预检的图复制到规范路径并登记**

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --register-output BLD_QS_S_A_HOUSE hero_3q v001 SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.png --generation-input-path assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.prompt.json --generation-reference-lineage style/QS_InkToon_Building_v2.json --generation-prompt-file assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.prompt.json
```

实现前若现有 CLI 没有 `--generation-prompt-file`，先为它补一条失败测试和该参数；读取提示包中的完整 `prompt` 字段后复用 `register_output()`，不要复制登记逻辑。保留现有 `--generation-prompt` 兼容性，并禁止两者同时出现。

- [ ] **Step 5: 验证登记证据与不可覆盖性**

```powershell
python scripts/qingshan_environment_assets.py --root SourceAssets/TownPCG/QingshanEnvironment --check
python scripts/build_qingshan_environment_catalog.py --root SourceAssets/TownPCG/QingshanEnvironment --check-no-rewrite
python -m unittest scripts.test_qingshan_environment_assets scripts.test_qingshan_building_concepts -v
```

Expected: 目录与构建器检查通过；`asset.json` 中记录 `generated_pending_review`、`v001`、规范相对输出路径、SHA-256 和完整生成链路；重新登记同版本被拒绝。

- [ ] **Step 6: 向用户展示金样，不上传 Tripo**

只汇报图片、资产 ID、版本、摘要和视觉门禁结果。用户明确批准前，`tripo_allowed` 保持 `false`，不消费 Tripo 高精度生成。

- [ ] **Step 7: 用户批准后提交生成证据**

```powershell
git add -- SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/concept/BLD_QS_S_A_HOUSE__hero_3q__v001.png SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE/asset.json SourceAssets/TownPCG/QingshanEnvironment/manifest.json scripts/qingshan_environment_assets.py scripts/test_qingshan_environment_assets.py
git commit -m "art: register qingshan small-house golden concept"
```

## Task 6: 阶段验收与下一计划输入

**Files:**
- Modify: `docs/superpowers/specs/2026-07-12-qingshan-building-json-tripo-concept-design.md` only if the approved image changes a previously agreed contract
- Create later: `docs/superpowers/plans/2026-07-12-qingshan-building-family-concepts.md`

- [ ] **Step 1: 检查需求覆盖与占位符**

```powershell
rg -n "TODO|FIXME|placeholder|\.\.\." scripts/qingshan_building_concepts.py scripts/test_qingshan_building_concepts.py SourceAssets/TownPCG/QingshanEnvironment/style/QS_InkToon_Building_v2.json SourceAssets/TownPCG/QingshanEnvironment/assets/BLD_QS_S_A_HOUSE
```

Expected: 没有实现占位符；测试中用于错误样例的文本可保留但要人工确认。

- [ ] **Step 2: 做最终工作树边界检查**

```powershell
git status --short
git diff --check
git diff --name-only HEAD
```

Expected: 本阶段只包含计划列出的文件；不得包含用户调过的地图、角色、PaperZD、相机、HD2D 平面、当前道路/悬浮道具修复或无关截图。

- [ ] **Step 3: 记录下一阶段的确定输入**

批准的 S-A 金样成为五栋新建筑的共享视觉锚点。下一计划只派生 M-A 客栈、M-B 沿街商铺、S-B 工坊、S-C 河岸小屋，并登记现有 L-A 最大商铺；每栋保持独立 JSON、独立单图和独立附件 actor，不直接进入 Tripo，直到整组原画审核通过。
