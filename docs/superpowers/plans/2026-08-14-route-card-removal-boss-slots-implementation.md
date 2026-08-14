# 路线临时卡退出与首领卡槽实施计划(2026-08-14)

> 设计真源:`docs/superpowers/specs/2026-08-14-route-card-removal-boss-slots-design.md`
> 纪律:每个阶段先写失败测试(RED)→ 最小实现 → 冷 UBT → 聚焦自动化 GREEN → 独立提交。

## 0. 改动面清单

- 状态:`GameXXKCardRunTypes.h`(`RouteCardIds`/`RouteCardEntries`/`NextRouteCardEntryOrdinal`/`EGameXXKRouteCardSourceKind`/`FGameXXKRouteCardEntry` 删除;新增 `BossCardSlots` ≤3 与常量 `MaxBossCardSlots=3`;`FGameXXKPendingRouteCardReward` 的 BossCard 选项去向改为槽+手牌)。
- 规则:`GameXXKRunDeckRules.*`、`GameXXKRouteCardRecipe.*` 删除;`GameXXKMVPRules`/`GameXXKMVPSubsystem` 的发卡/合并/替换/商人卡货架/事件宝箱发卡路径删除;`ResolveBattleVictory` Boss 选项 → `AddBossCardToSlot`+入当前手牌。
- 迁移:`GameXXKSaveMigration` v16→v17(丢弃路线卡字段,初始化 BossCardSlots 空)。
- UI:`GameXXKBattleBoardWidget` 奖励框 Boss 选项结算路径;`GameXXKRouteMerchantWidget` 只遗物;`GameXXKRouteEncounterPanelWidget` 不再显示卡牌奖励。
- 测试:CardRoute 组(~9)、RunDeckRules、RouteCardRecipe、RouteMerchant、相关迁移测试改造;新增 Boss 槽规则/迁移/UI 测试。

## 执行进度(2026-08-14 更新)

- ✅ Phase 1 核心语义:首领槽字段+奖励去路+牌库装配(8 英雄+5 伙伴+3 任务NPC+0~3 首领槽,无路线卡/填充卡)+迁移 v17;
- ✅ Phase 2 测试改造:RunDeck/Recipe/路线条目测试删除或改写,迁移链 v16→v17,UI 奖励框直通首领槽+入手牌;
- ✅ Phase 3 UI:商人只卖遗物、替换流程状态机退役(返回空)、Boss 选项槽满禁用;
- ✅ 冷 UBT 编译通过;全量自动化 594 项 589 通过;
- ✅ 剩余 5 项平衡失败已修复(用户选定方案):
  1. 两场首领战模拟卡死 MaxDecisions(玄甲月白 ch3 Boss、追风金龟 ch2 Boss)——根因是模拟 AI 无进展时不肯结束回合(月白 0 费"周天归元/照见五蕴"循环)。修法:连续 64 次决策没有压低任何敌人血量则强制结束玩家回合;连续 5 个回合边界双方血量无变化则判定僵持战败(`Simulation.Defeat` + `bStalemateResolved`)。
  2. 正交 Blade 用例:土司首领"延迟保留"卡在弃绝堆丢失——根因是保留牌主人(土司首领 NPC)在敌人阶段阵亡,`RemoveDefeatedPartyOwnerCards` 清除了弃绝堆里的孤立实例,次回合边界严格查账失败。修法:主人阵亡时取消该延迟蓄力(牌随主人退场),保留槽预留与严格实例校验同步修正。
- ✅ 新增回归测试:`GameXXK.RouteBalance.Diagnostics.ZhuiFengJinGuiBoss942090`、`GameXXK.RouteBalance.Diagnostics.BladeRetainedOwnerDefeat1100213`;
- ✅ 全量 2400 锁定案例 0 卡死/0 错误(胜利 2298 / 战败 102);卡牌文档已重新生成;
- ✅ 修复两个进程 RNG 导致的偶发测试(`BattleEntry` 固定路线种子、`PlayerFlowOwnsFlowWidgets` 固定初始同伴招募种子 + 目标卡必须造成伤害);
- ✅ 全量自动化 596 项 0 失败(两轮通过);
- ✅ 真机 PIE:新档→青山→接任务→路线图→战斗入场→16 张共享牌库正确装配(无路线卡)→通过真实 UMG 打出卡牌/结束回合/敌方意图演出正常;奖励框的首领牌选择路径由无头 UI 测试 + 2400 案例矩阵钉死;
- ⏳ 待办:提交推送。

## 1. Phase 1 — 状态与规则层(核心)

1.1 `FGameXXKCardRunState`:删除路线卡字段,新增 `TArray<FName> BossCardSlots`;`MaxBossCardSlots = 3` 常量。
1.2 删除 `FGameXXKRunDeckRules`、`FGameXXKRouteCardRecipe` 全部引用;清理 `DefaultRouteCardIds`。
1.3 `ResolveBattleVictory`:Boss 选项池=现有 Boss 卡池(虎/熊);选中 → 空闲槽写入 + 加入当前手牌;槽满 → 候选剔除。
1.4 事件/宝箱:卡牌奖励替换为遗物/气力点/抽牌数(权重化、确定性种子不变)。
1.5 商人:货架只遗物(4 格),删除卡牌货架/替换预览逻辑。
1.6 `GameXXKSaveMigration`:CurrentSaveVersion 16→17;v16 旧档丢弃 RouteCardIds/RouteCardEntries、BossCardSlots 置空。

## 2. Phase 2 — 测试改造(每步 RED→GREEN)

2.1 迁移测试:旧 v16 档(含路线卡)→ v17 加载丢弃、槽空。
2.2 Boss 槽测试:入槽/满 3 剔除/加入手牌/跨战斗持久。
2.3 奖励构成矩阵更新:普通/精英/Boss 三选一各构成。
2.4 删除断言:RunDeck/Recipe/商人卡货架/事件发卡不复存在(对应测试删除或改为"无卡牌路径")。

## 3. Phase 3 — UI

3.1 奖励框 Boss 选项:文案"加入首领卡槽并入手牌";满槽置灰。
3.2 商人货架:遗物×4 布局。
3.3 路线遭遇面板:事件/宝箱奖励展示无卡牌。

## 4. Phase 4 — 全量验证与提交

冷 UBT + 全量 Automation(0 failed)+ 真机 PIE(普通/精英/Boss 战斗与奖励、首领入槽入手牌、事件宝箱商人)+ 独立提交 + push。

## 5. 风险与回滚

- 存档迁移不可逆:迁移前备份 `SaveGames`;迁移测试钉死行为;提交前全量回归。
- 删除公共 API 会影响蓝图:先 grep 蓝图引用(1Game 桥不涉及路线卡)。
- 每个 Phase 独立提交,可 `git revert`。
