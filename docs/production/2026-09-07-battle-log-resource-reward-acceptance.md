# 战报、墨迹资源条与奖励卡补充验收

本轮沿用已获授权的 `codex/ui-visual-optimization` 根工作区。无 worktree，无 Live Coding；地图一直为 `L_DesktopTrainingHUD`。

## 已完成

- 战报放到意图卡左侧；地势标题位于同列上方居中。紧凑模式只显示最新一条，江湖体 14 号；点击 `+` 放大至 18 号并显示已有记录，点击 `-` 收起。
- 根据用户复核，撤掉纸底，恢复半透明深色底和浅色字；四角半径 9。紧凑区域向右拓宽至首张意图卡前，保留间距。
- 首版负坐标仍位于 `BattleDesignStage` 的 `ClipToBounds` 内，导致左侧裁字。现把战报和地势移到 viewport 层，按同一安全区缩放；未修改战斗画布的裁剪或目标箭头坐标链。
- 挂机和战斗资源条共用已确认的第三稿墨条材质：挂机 124×18，战斗 252×28，气血朱红、内力青绿。源 PNG 未改像素，材质取共同轮廓、保留端头和控制填充。战斗数字改江湖体；数值仍来自原运行时。
- 遗物三选一改为名称、等比插图、介绍；选中名称变白并显示背包同款墨迹底。
- 战斗奖励三张牌整体放大 20%；遗物使用正方形图槽，底部介绍区域增加至 89 个设计像素。正文 14 号，长说明在固定范围内按比例缩小，完整说明仍在原悬停提示中。普通卡牌的立绘绘制方式保留。
- 敌方 1P 死亡后显示两个 3P 的问题已修复：原先只隐藏卡按钮，旧编号的父布局仍显示。现在整组卡、侧标与间距同步收起，并清除空位编号。存活者仍保留原始槽号。

## 验证

- 最终冷 UBT Editor：`Saved/Codex/CardEffects-20260907/reward-fit-enemy-labels-editor-build-final.log`，通过。
- 自动化：`Saved/Automation/BattleUiFinal_20260907/index.json`，85/85 通过（72 正常，13 带既有生命周期诊断警告），0 失败。
- 新增死亡编号覆盖：1P 死亡、连续死亡、全部死亡、重新复用、2P 或 3P 单独死亡。旧的固定时间动画断言改为观测实际伤害包阶段，保留结算顺序和中间气血断言。
- 真实出牌复现前：`enemy-intent-slots-before-fix.json` 为 `2P / 3P / 3P`，末位卡隐藏而布局仍显示。修复后 `enemy-intent-slots-after-fix.json` 为 `2P / 3P / 空`，空位整体隐藏。
- 实拍：`final-battle-log-compact.png`、`final-battle-log-expanded.png`、`final-battle-log-collapsed-again.png`、`final-battle-log-small.png`；已实际点击两次缩放按钮，并在 1920×1016、1280×760 两种窗口检查裁边。
- 新奖励与死亡实拍：`final-reward-fit.png`、`final-enemy-intent-after-first-death.png`。奖励介绍完整留在卡框内；实拍时升级牌悬停提示可见。
- 上述实拍、检查 JSON 均位于 `Saved/Codex/CardEffects-20260907/`。本轮未重新执行完整 Cook / Stage 或 2,040 场平衡矩阵。
- `final-player-check.json` 确认原存档、桌面 75% 缩放设置与默认地图哈希未变；保留用户当前桌面位置，不用旧备份覆盖。

## 仍属讨论范围

法术任务专用图标与任务特写流程尚未接入，不能将这次资源条及卡面验收视为该功能完成。用户否决了开源特效候选的复杂画风，并决定自行提供参考，停止继续搜索素材。收到参考后，先以简约轮廓、大色块、少碎光重绘少量关键帧确认，再补过渡帧；统一尺寸、锚点和色彩，不将旧候选导入 UE。
