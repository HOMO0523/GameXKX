# 战斗状态图标语言与脚下 HUD 设计

## 目标

把战斗角色脚下的生命、护甲和状态从默认 UMG 文字/色条改成与现有 PSD 卡框一致的纸签、墨线、朱印视觉语言。状态必须以低饱和、清晰区分的图标显示层数；鼠标悬停图标只显示真实规则 Tooltip，不触发任何战斗操作。

## 视觉规则

- 生命条使用已导入的 `T_TownHUD_HealthBarFrame` 与 `T_TownHUD_HealthBarFill`，作为可复用的 PSD 纸纹边框和填充来源；不使用默认纯色 `UProgressBar`。
- 每个护甲/状态使用一个 30px 方形纸签底：淡黄宣纸、深褐墨线、轻微阴影。图形本身占纸签约 76% 面积，使用低饱和、中低对比的单一矿物色；主体必须是小尺寸仍可读的扁平剪影，禁止第二种彩色块。轮廓和纸底允许轻微干笔、水墨晕染与手绘不规则感，但禁止写实体积高光、复杂材质和密集笔触。
- 图标右上角常驻层数圆印；层数至少为 1 时显示阿拉伯数字，超过 99 显示 `99+`。护甲同样采用图标加数值，不再单独显示“甲 N”纯文本。
- 减益颜色：中毒灰草绿、流血暗朱红、灼烧陶土橙、蚀伤灰靛紫、易伤灰洋红、标记砖红。增益颜色：敏捷灰青蓝、守护灰靛青、气势赭金、免疫青白、战术/下一击灰紫金、地形灰青绿、转移雾靛蓝。护甲为青灰钢蓝配赭金铆钉；任何单图都不使用纯高光或高纯度撞色。
- 角色名与 P 位仍在生命条上方；生命数值仍可读为 `当前/最大`。状态纸签排在生命条下方，不遮住角色或手牌区域。

## 图标覆盖与数据映射

生成 13 个独立、透明背景的图标资产，并以稳定资源 ID 映射所有当前 `EGameXXKCardStatus`：

| 图标资源 | 覆盖状态 |
| --- | --- |
| ArmorShield | 护甲（运行时 Armor，不是状态） |
| MomentumSeal | Momentum |
| AgilityWing | Agility |
| VulnerabilityMask | Vulnerability |
| BleedDrop | Bleed |
| PoisonVial | Poison |
| BurnFlame | Burn |
| MarkTarget | Mark |
| GuardShield | Guard |
| RotSpiral | DamageOverTime |
| ImmunityTalisman | CannotReceiveVulnerability |
| TacticSeal | NextAttackBonus、NextAttackAppliesVulnerability、NextHealingBonus |
| TerrainAndRedirect | TerrainBonusDouble、NextTerrainCardFree、NextTerrainCardEnergyReduction、RedirectSingleTargetEnemyAttack、TerrainBonusDoubleThisRound |

没有映射的未来有效状态不得消失：显示统一的墨色问号纸签、数字层数与完整枚举名 Tooltip，并在开发日志记录一次缺失映射。

## Tooltip 与交互

- 每个可见状态纸签是 Hover 命中区域；离开立即隐藏 Tooltip。图标本身不能被点击，Tooltip 为 `HitTestInvisible`。
- Tooltip 复用当前 PSD 纸张说明框，内容顺序固定为：状态名称、层数、效果、触发时机/消耗规则。状态层数与说明只来自当前运行时 `FGameXXKCardStatusStack`，不产生 UI 自行计算的数值。
- 护甲 Tooltip 写明当前 `N` 点、先于生命抵扣伤害，以及本回合/下回合的实际清除规则（由现有规则数据决定；若没有时限则不得编造）。

## 技术边界

- 新建 `FGameXXKBattleStatusIconStyle`（资源路径、显示名、Tooltip 文案、颜色/备用图形）和状态到样式的纯函数；UMG Widget 只按该投影创建/刷新纸签。
- 使用一张 `UImage` 表示图标、一个纸签背景 Border、一个不拦截输入的右上数字 TextBlock。最多显示当前非零状态；若超过安全可读上限，优先显示减益、再显示增益，最后增加 `+N` 纸签。
- 场景 Presenter 按 UnitId 差分刷新：保留仍存在的 Actor，移除离场 UnitId，只生成新进入的 UnitId。Actor 状态刷新必须清除过期纸签和 Tooltip，不能残留上个单位的状态。
- 不修改用户调整过的相机、关卡、PaperZD 角色或原始 PSD 资产；图标作为新 UI 素材导入。

## 验收

1. 战斗脚下生命条可证明使用现有 PSD Frame/Fill 资源，而非默认绿色填充。
2. Armor 与所有现有 18 个有效状态均能取得专属或分组图标；层数正确显示，0 层不显示。
3. 中毒、流血、灼烧、蚀伤、易伤、敏捷、守护和至少一个临时战术/地形效果的 Hover Tooltip 都显示正确机制且不改变战斗状态。
4. 刷新同一 UnitId 后其 Actor 对象仍然存在且更新生命/护甲/状态；离场 UnitId 才会销毁；状态减少为 0 后纸签和 Tooltip 均消失。
5. 所有生成图标为透明背景、无文字、无水印，并保持现有江湖纸墨 UI 的比例与色彩语义；图标主体只使用一种低饱和矿物色，带克制的干笔墨边。
