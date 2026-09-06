# 背包选人与纸底统一验收（2026-09-06）

> 后续复核：用户指出纸底仍被裁边。下面“只释放下沿”的初版保留了左右与上方裁剪，不能视为四周完整。最新修正移除整个外层纸面裁剪，保留两个内部滚动区；最终实拍与构建记录补充于 `2026-09-06-backpack-paper-edges-acceptance.md`。

已在用户批准的根目录分支 `codex/ui-visual-optimization` 完成；未使用worktree，改动保留在工作区。沿用国风水墨素材与江湖体，视觉依据均为当前工程的真实UE 5.8运行截图。

## 完成内容

- 背包只释放纸底下沿：嵌入参考仍为1450×849，原控件偏移仍为(-311,-173)，裁剪高度延至871，露出原纸底最后一段。物品和卡组滚动区继续裁剪；装备格、角色立绘和物品坐标未移动。
- “主角 / 伙伴 / NPC”改为纯文字入口，按嵌入立绘的真实中心放在其下方。按钮106×42，江湖体20。
- 伙伴/NPC各使用完整背包区域的3列×2行大卡。卡名居上、等级在左、现有卡面在右下；“正在查看 / 出战中”与下边框留出距离。当前每类6人，超过6人按页显示，末页不复制角色。
- 打开候选页不提前切换角色；点击候选才进入该角色背包。返回与右键取消保留原查看对象；查看角色不改变出战编队。
- 同次背包会话按角色保留未应用卡组及页签状态；关闭整套背包清理会话，不新增永久存档字段。
- 仓库、历练地图、选人页及同类纸面保留九宫格切片，四角与嵌入背包采用相同呈现尺度，中间和边延展。修正之前直接使用原纹理角尺寸导致的粗角。设置页保留纸面内容层，“缩放”保持单行。

## 构建与自动化

| 检查 | 结果 | 证据 |
|---|---|---|
| 最终冷UBT | Succeeded；未用Live Coding/Hot Reload | `Saved/Codex/FullPlayAudit-20260906/backpack-release-cold-build.log` |
| 最终DLL的选人、草稿、分页及关闭回归 | 10/10通过，0错误，2条既有警告 | `Saved/Automation/BackpackPicker_Release_20260906/index.json` |
| 工作台扩大回归 | 77项中73项通过，4项既有失配 | `Saved/Automation/BackpackPicker_FinalRegression_20260906/index.json` |

扩大回归在最后的文字内边距微调之前运行；本轮末尾用最终DLL重复了上述10项，并重新实拍。4项未通过分别为 `InnerGeometry`、`TravelCombatPresentation`、`TravelPartyAtlasAsyncFallback`、`TravelPartyAtlasFallbackInventory`；仍对应此前已记录的用户调优坐标、旧编制及旧图集路径期望，没有为迁就旧测试还原美术资产。先前局内核心96/96和最终UI25/25证据继续有效，不能据此宣称全项目全绿。

## 原生窗口实测

所有图片保存在 `Saved/Codex/FullPlayAudit-20260906/`，截图原始尺寸1920×1020。交互使用所见画面坐标的真实鼠标输入；深度Slate观察用于定位和辅助读取，最终比对实际像素。

- `backpack-release-{50,75,100}.png`：底边完整、文本选择入口居中；`paper-panels-release-{50,75,100}.png`：仓库和历练地图角尺寸一致。
- `picker-release-{50,75,100}.png`、`npc-picker-release-{50,75,100}.png`：两类六张卡均完整显示；没有名称方形底、重叠候选缩略图。
- 点击伙伴后当前查看仍为主角；点击守卫大卡后查看ID为 `CompanionInstance.Companion_Guard_01.000058B1`，出战仍为刀客 `...Blade_01.000058B0`。证据 `picker-select-guard-native.json`。
- 点击“返回背包”、在纸面右键均回到守卫背包。证据 `picker-return-native.*`、`picker-right-click-native.*`。
- 实际取消青锋一式：8/8→7/8；再打开伙伴、查看守卫、返回主角，画面仍为7/8。两次正式主角牌组均8张，出战编队未变化。证据 `deck-draft-seven-release.*` 与 `deck-draft-restored-release.*`，并已看图确认。
- 原生Esc在本次编辑器环境被“停止PIE”快捷键消费，未计作桌面选人页的取消验收；返回与右键路径已通过。源码的Esc处理仅在按键传到该Widget时生效。

## 实测边界与清理

- 当前保存位置下切到100%时，整组导航/侧栏底部可超出屏幕；50/75%完整。这是整组垂直适配问题，区别于背包纸底裁剪，已经加入整体试玩清单。
- 本轮不改挂机数据、怪物动画帧和素材清晰度；其确认问题与建议见 [整体试玩报告](2026-09-06-full-play-audit.md)。
- 试玩检查点已归档为 `audit-runtime-checkpoint.sav`。原主存档、HUD配置和GameUserSettings恢复至试玩前哈希，HUD为50%；默认地图包哈希与任务开始前一致。证据 `cleanup-and-integrity.json`。
- 用户原有字体调整、地图修改、素材删除及未跟踪资产保留；没有提交、推送或合并本轮工作。
