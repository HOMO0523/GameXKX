---
status: accepted
owner: codex
updated_at: 2026-08-15T10:55:00+08:00
reported_at: 2026-08-15
resolved_at: 2026-08-15
accepted_at: 2026-08-15
source_commit: a490235a32ad324c2f74e21a8b79a344c65b0125
severity: visual-regression
scope: battle-targeting-presentation
---
# 战斗卡牌目标箭头错位问题报告

## 结论

本问题已修复，并于 2026-08-15 通过自动化、真实 PIE 截图对比和用户现场验收。

修复后的表现契约为：点击需要手动指定目标的卡牌后，水墨箭头继续跟随真实鼠标；橙色箭头头部的贴图中心严格落在实时鼠标端点；方向只影响贝塞尔曲线法线和箭头旋转，不再产生方向相关平移；箭头头部保持与末端墨迹相接。

实现提交：`a490235a32ad324c2f74e21a8b79a344c65b0125`（`fix: restore battle target arrow alignment`）。

## 用户可见问题

- 使用需要指定目标的卡牌后，橙色箭头头部出现在正确端点的右下方。
- 箭头头部与末端墨迹之间出现额外间隙，排列关系与之前已经接受的截图不一致。
- 偏移会随指向方向旋转，因此不是固定的窗口/DPI 平移，而是箭头头部自身被加入了方向相关位移。
- 本问题只影响表现；卡牌目标判定、预览和提交结果没有被证明发生玩法错误。

## 正确基线与验收边界

- 正确视觉基线：`Saved/Codex/arrow_alignment_accepted_baseline.png`。
- 目标语义继续采用提交 `6668146` 确立的规则：箭头跟随真实鼠标，绝不吸附到单位的固定阵型锚点。
- 本轮只修复箭头头部与鼠标端点/末端墨迹的相对位置，不修改箭头和墨迹纹理、不调整角色/战场资产、不改变卡牌结算。
- 真实 PIE 使用不同战场背景时，只比较箭头端点关系、方向变化和墨迹排列，不把背景差异计入本问题。

## 根因

回归工作区在 `NativePaint` 的箭头头部布局上加入了 `ArrowHeadTipOffsetRatio`，把未旋转贴图的左上角从原本的：

```cpp
TargetingEnd - ArrowSize * 0.5f
```

改成了等价于：

```cpp
TargetingEnd
    - Direction * (ArrowSize.X * ArrowHeadTipOffsetRatio)
    - ArrowSize * 0.5f
```

该常量会让 `74 px` 宽的箭头头部沿当前方向额外移动约 `26.85 px`。随后贴图又围绕自身旋转，于是最终表现为随方向变化的右下错位，并破坏了箭头头部与末端墨迹的连续关系。

坐标转换链路本身不是本次错位根因。Slate 绝对鼠标坐标仍通过设计舞台几何转换成 stage space；`UpdateTargetingPointer` 保留该 stage-space 位置供绘制使用。

## 修复内容

实现提交只包含以下 3 个 C++ 文件：

- `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- `Source/GameXXK/Private/Tests/GameXXKBattleBoardWidgetTest.cpp`

具体处理：

1. 将生产绘制路径使用的箭头左上角计算提取为确定性测试缝 `ResolveTargetingArrowHeadTopLeftForTest`。
2. 生产代码和测试统一使用 `TargetingEnd - ArrowSize * 0.5f`，保证贴图中心严格等于实时鼠标端点。
3. 方向只继续参与曲线法线和 `Atan2` 旋转，不参与头部平移。
4. 保留独立且正确的 stage-space 目标命中修正 `TryResolveCardTargetUnitAtStagePosition(ScreenPosition, ...)`。
5. 删除临时 Win32 光标诊断、循环 `[ArrowPaint]` 日志和 `ArrowHeadTipOffsetRatio`，未修改任何纹理或 `.uasset`。

## 验证记录

### TDD 与自动化

- RED：`GameXXK.MVP.Battle.TargetArrowAlignment` 在旧方向偏移公式下被发现并运行；水平、垂直、对角三个方向的左上角/中心共 6 个断言均按预期失败。
- GREEN：同一测试在中心对齐公式下通过。
- 主代理最终复验：
  - `Saved/Automation/battle-target-arrow-alignment-final-primary/index.json`：发现 1、成功 1、失败 0、错误 0。
  - `Saved/Automation/battle-board-widget-adjacent-final-primary/index.json`：发现 1、成功 1、失败 0、错误 0；包含 11 条既有 `BattleVisual` 诊断 warning。
  - 对应生产循环记录：`Saved/HarnessReports/20260815-093434-ai-production-loop.md`、`Saved/HarnessReports/20260815-093455-ai-production-loop.md`。
- `python scripts/gamexxk_battle_target_art_check.py`：`ok=True`，12 个墨迹块全部有效。
- `git diff --check 5ccb88e..a490235`：通过。

### 构建与 PIE

- `python scripts/ue_tdd_pipeline.py --pie-duration 1 --log-lines 120`：使用 `-NoHotReload`；UBT `Result: Succeeded`；编辑器重启后 PIE 正常启动并停止。
- 完整目标预览流程：`Saved/HarnessReports/battle-target-arrow-alignment-final-20260815.json`。
- 修复后单体反向截图：
  - `Saved/Codex/arrow_alignment_repaired_single.png`
  - `Saved/Codex/target_outcome_Healing.png`
- 修复后的 Single 中，箭头头部与末端墨迹相接，没有额外右下间隙。
- Healing 将方向翻转到另一侧后，箭头头部只旋转、不发生方向相关平移。

### 图像完整性

| 文件 | SHA-256 |
|---|---|
| `arrow_alignment_accepted_baseline.png` | `2EA86BE31973379D2B4830139AF7B504E0E9701CFA47E484E7E4619E6B79B918` |
| 恢复后的 `target_outcome_Single.png` | `2EA86BE31973379D2B4830139AF7B504E0E9701CFA47E484E7E4619E6B79B918` |
| `arrow_alignment_repaired_single.png` | `E89597F2A8FE63B451E0E9E52DAE45FECB90FED98DFEA6FE42739A38BD242331` |
| `target_outcome_Healing.png` | `46F2CE8881A4E5CA4B5944C60224CF9700A8C17F19B1506E2C8E3F9C2F54397A` |

旧正确基线与恢复后的 canonical Single 截图字节完全一致。新修复截图单独保存，避免覆盖或冒充旧基线。

### 用户现场验收

- 使用 `python scripts/gamexxk_real_play_flow_mcp.py --keep-pie` 通过真实玩家路径推进到战斗。
- 在 PIE 中应用非保存型 `Outcome.Single` 验收夹具，真实点击 `青锋一式`，将鼠标停在 `敌人 1P`，确认目标预览可见且箭头仍处于鼠标跟随状态。
- 验收现场记录：
  - `Saved/HarnessReports/battle-arrow-manual-acceptance-stage-20260815.json`
  - `Saved/Codex/battle_arrow_manual_acceptance_ready.png`
- 用户在同一 PIE 会话中完成手动检查并回复 `ok`，本报告据此记录人工视觉验收通过。
- 验收结束后已清除临时目标结果夹具；UE MCP `save_dirty_packages` 返回 `dirty_before=[]`、`dirty_after=[]`；PIE 已正常停止。

## 已知测试工具欠账

`Saved/HarnessReports/battle-target-arrow-alignment-final-20260815.json` 的顶层 `ok=false` 不代表产品修复失败。失败被限定为旧的 `pointer_matches_target` 断言：

- `scripts/gamexxk_real_play_flow_mcp.py` 仍把实时指针与单位固定阵型锚点按 `1 stage px` 比较。
- 该断言来自提交 `062776a` 时的旧吸附语义。
- 提交 `6668146` 已明确删除吸附并冻结“箭头跟随真实鼠标”的语义；harness 自身也把 OS 鼠标移动到真实 Slate 目标按钮中心，而不是固定阵型锚点。
- 10 个单体场景仅此条件失败；截图、目标锚点、提示框、缓存/布局稳定性、预测与提交结果、取消悬停和夹具恢复均通过；两个群体场景通过。
- 单体指针差值约为左侧 `(+44.636, +78.659)`、右侧 `(+45.115, +78.659)`，稳定落在历史 `180 x 320` 目标代理区域内。

处理决定：不为让报告变绿而恢复错误的目标吸附。后续应单独维护 harness，使其比较“stage-space 实时指针与 OS 鼠标转换结果”，或校验指针位于真实目标代理区域；该欠账不阻塞本次产品视觉验收。

## 审查与范围结论

- 规格审查：通过。
- 代码质量审查：Critical 0、Important 0。
- PIE 证据审查：Critical 0、Important 0，Ready to deliver: Yes。
- 最终整体审查：Critical 0、Important 0，Ready to deliver: Yes。
- 相关生产范围 `Source/GameXXK` 与 `Content/GameXXK` 无本次提交后的未提交覆盖；仓库其他既有诊断探针和素材工作不属于本问题，未被回滚或覆盖。

## 关联记录

- 设计：`docs/superpowers/specs/2026-08-15-battle-target-arrow-alignment-repair-design.md`（`bb9f9e0`）
- 计划：`docs/superpowers/plans/2026-08-15-battle-target-arrow-alignment-repair.md`（`5ccb88e`）
- 实现：`a490235a32ad324c2f74e21a8b79a344c65b0125`
