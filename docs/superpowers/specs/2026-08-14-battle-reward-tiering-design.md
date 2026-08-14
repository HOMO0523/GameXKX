# 战斗奖励分层重设计（2026-08-14）

## 需求来源
用户指示：战后三选一不再从 30 张路线牌池（通用/地形/稀有/Boss）出卡；只出玩家配置的卡组牌、首领牌、遗物；选卡组牌提升该牌稀有度；精英可掉气力上限+1 或每回合抽牌+1；普通/精英/Boss 做奖励分层。

## 分层表（用户确认版）

| 战斗类型 | 三选一构成 |
|---|---|
| 普通怪 Battle | 遗物×2 + 卡组牌×1（遗物为主+少量卡） |
| 精英怪 Elite | 属性奖×1 + 卡组牌×1 + 遗物×1 |
| Boss | 首领牌×1 + 卡组牌×1 + 遗物×1 |

## 机制定案（用户确认）

1. **卡组牌范围** = 英雄配置卡（HeroSelectedCardIds）+ 出战伙伴配置卡（ActiveCompanion.SelectedCardIds）。
   - 选中 → 该卡品质逐级+1（普通→稀有→珍稀）。
   - 已是珍稀的卡不进入候选；候选为空时用遗物替补。
   - 品质提升跨战斗持久：新增 `CardRun.UpgradedCardQualities: TMap<FName, EGameXXKCardQuality>`，战斗组牌时覆盖实例品质。
2. **首领牌** = Boss 专属池（虎/熊），仅 Boss 战三选一出现；作为新卡入路线牌库，沿用 12 容量 + 替换流程（bRequiresRouteCardReplacement）。
3. **属性奖**（仅精英）= 气力上限+1 或 每回合抽牌+1，二选一由种子掷定，混在三选一里。
   - 持久化：`CardRun.BonusSharedEnergyCap` / `CardRun.BonusRoundDrawCount`。
   - 应用点：共享气力 = 基础 3 + BonusSharedEnergyCap；回合抽牌数 = 手牌上限 5 + BonusRoundDrawCount。
4. **遗物** = 既有遗物授予机制（节点奖励同款）。

## 状态与迁移

- 新枚举 `EGameXXKBattleRewardKind { DeckCardUpgrade, BossCard, Relic, EnergyCapBonus, DrawBonus }` + 选项结构 `FGameXXKBattleRewardOption { Kind, CardId, RelicId }`。
- `CardRun.PendingReward` 由 `CardIds[3]` 改为 `Options[3]` 变体数组。
- 存档版本 15 → **16**：新增 UpgradedCardQualities、BonusSharedEnergyCap、BonusRoundDrawCount；旧格式 PendingReward（3 张路线卡 id）加载时清空（胜利后重掷），迁移测试钉死该行为。

## 规则改动

- `ResolveBattleVictory`：按节点类型（Battle/Elite/Boss）构建分层三选一；选项种子确定性（沿用 ChoiceSeed 链）。
- 选择结算新增分支：DeckCardUpgrade（品质+1 写 UpgradedCardQualities）、BossCard（路线牌库入卡+替换流程）、Relic（遗物授予）、EnergyCapBonus/DrawBonus（+1）。
- 能量与抽牌规则处读取新加成字段。

## UI 改动

- RewardCardBox 三槽按选项类型渲染：卡面（卡组牌/首领牌）、遗物图标、属性奖文案（"气力上限 +1"/"每回合抽牌 +1"）。
- 点击结算接缝：ChoosePendingRouteReward 按选项索引 → 规则分支；测试接缝改为返回选项视图（probes/PIE 脚本同步更新）。

## 测试范围

- 更新：BoardRewards/BoardRewardReplacement/BoardRewardAtomicFacade、RouteRewardEntryAcquisition(.Skip)、CardRouteRewardChoice/RewardGate/ChoiceSeed、MVPFlow、MVPUIWidget、存档版本断言（v15→16）。
- 新增：分层构成矩阵（Battle/Elite/Boss × 选项类型）、品质升级持久化与满级剔除、属性加成对气力/抽牌的生效、迁移 v15 清空旧奖励。

## 阶段

1. 状态类型 + 存档迁移 v16（含迁移测试）
2. 奖励生成与结算规则 + 规则测试
3. 战斗棋盘奖励 UI + UI 测试
4. 全量构建测试 + PIE 实机验证（luna 复核分层界面）
