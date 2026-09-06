# 背包纸底四边裁切修正

用户指出上一版仍有纸边裁切，继续在已批准的 `codex/ui-visual-optimization` 根目录分支修正。

## 原因与改动

上一版只把外层裁剪从1450×849加高到1450×871。真实纸底相对于嵌入参考的范围为 `(-36.25,-21.225)..(1486.25,870.225)`，因此左右毛边与上方两角仍被矩形裁掉。实际PIE中仅将此层从 `ClipToBounds` 改为 `Inherit`，四角立即恢复，确认原因与贴图素材无关。

删除 `EmbeddedBackpackContentClip` 及其中转Canvas，将既有背包直接挂回不裁剪的嵌入Canvas。1450×849缩放参考、(-311,-173)偏移、装备格/立绘/字体和按钮位置保持不变。内部 `InventoryBackpackScrollBox` 与 `InventoryHeroDeckScrollBox` 继续使用 `ClipToBounds`。

本次为布局修正，没有新增模拟实现的单元测试，也没有修改或重新生成美术资源。

## 验证

- 冷UBT成功，未使用Live Coding/Hot Reload：`Saved/Codex/BackpackPaperEdges-20260906/cold-build.log`。
- 受影响的选人、13个角色展示、延迟刷新和局部关闭检查 **4/4通过，0错误、0警告**：`Saved/Automation/BackpackPaperEdges_20260906/index.json`。
- 最新DLL重启PIE，在默认 `L_DesktopTrainingHUD` 实拍50/75/100%。`after-50.png`、`after-75.png`、`after-100.png`均已逐图复核，纸底四角与毛边完整。
- 最终运行时外层裁剪对象不存在，内部两个滚动区仍为 `ClipToBounds`，嵌入偏移不变：`after-paper-bounds.json`。
- 实际打开卡组，并通过现有ScrollBox接口将偏移设为480；`deck-scroll-verified.png`确认滚动内容在顶部和底部裁剪，不溢出标题或应用按钮。该检查验证裁剪，不冒充滚轮输入路由验收。
- 当前存档、HUD与GameUserSettings恢复至本次开始前哈希，默认地图未改：`cleanup-and-integrity.json`。工作区仍未提交。

以上证据统一位于 `Saved/Codex/BackpackPaperEdges-20260906/`。原实机对照页已将背包对比替换为此次裁边修正前后的原始截图。
