# GameXXK 198 张卡牌交叉复审状态

日期：2026-08-11
分支：`main`
提交门禁：复审完成且用户确认前不暂存、不提交。

## 入口边界

- 最终复审时工作区共有 64 个 tracked 修改、217 个 untracked 路径；全部视为需保留的共享工作，不清理、不回滚。
- 本轮只用冷 UBT：`-NoHotReload -NoHotReloadFromIDE`。
- 任何“通过”均要求报告实际发现测试，`failed=0`、`notRun=0`，并单列 warning。
- 目标终稿、当前代码快照与实现状态分开维护；198 只代表目录数量，不代表逐卡行为自动正确。

## 总审计表

| 分区 | 期望数 | 目标真源 | 目录/结构证据 | 运行时证据 | Warning | 未决问题 | 状态 |
|---|---:|---|---|---|---|---|---|
| 全目录 | 198 | 全项目专题三/四 + 专项规格 | `CardCatalog` 1/1；`CardQuality` 2/2；`CardDocumentation` 1/1；`CardText` 1/1 | 全198×七地势共1386次；条件缺失分支；六组8+5+3混合卡组 | 0 | 无已知卡牌目录/可打出性缺口；装备套装另列 | 已验证 |
| 主角 | 36 | 专题三 + 当前 Hero 精确表 | `HeroCards.Catalog` 2/2；1级32张、5/10/15/20级各增1张 | Generic/Blade/Guard/Healer/Hunter/Mage/Formation/Integration/迁移共86/86 | 63条既有`[EnemyAtk]`聚焦模拟审计 | 无已知主角规则缺口 | 已验证 |
| 刀客伙伴 | 18 | 专题三刀客终稿 | 目录1/1；旧档迁移1/1 | 冲锋/收招、重放/留式、四流派共43/43 | 10条既有`[EnemyAtk]`审计 | 破军套归 Task 7 独立复审 | 已验证 |
| 守卫伙伴 | 18 | 专题三守卫表 + 目录精确表 | 18张精确目录随前缀通过 | 反震、护主、单体转伤、全甲群攻、保甲、独立来源格挡共8/8 | 4条既有`[EnemyAtk]`审计 | 玄甲/山河套归 Task 7 | 已验证 |
| 药师伙伴 | 18 | 药效/药方规则 + 目录精确表 | 18张精确目录随前缀通过 | 核心、16药方、敌方回合结算共9/9 | 7条既有`[EnemyAtk]`审计 | 青囊套归 Task 7 | 已验证 |
| 弓手伙伴 | 18 | 重箭/蓄力规则 + 目录精确表 | 18张效果与重箭元数据随前缀通过 | 核心循环、重箭负载与顺序共5/5 | 1条既有`[EnemyAtk]`审计 | 追风套归 Task 7 | 已验证 |
| 法师伙伴 | 18 | `2026-08-11-sorcerer-partner-card-pool-design.md` | 新目录、文本、当前198快照一致 | 五牌任务、四任务分支、排队/续档共32/32 | 0 | 无已知法师缺口 | 已验证 |
| 阵师伙伴 | 18 | 六换场 + 十二全地势收益 | 18张精确目录与六换场通过 | 全地势收益矩阵、村落序列共4/4 | 0 | 无已知阵师缺口 | 已验证 |
| 永久伙伴出生/配置 | 108 | 普通2+3+1；阵师2换场+3+1 | 1024种子出生1/1；迁移3/3；伙伴规则1/1 | 六张终身池、五张携带、无升级解锁；两条背包1/1+1/1 | 36条重复`[WarehouseGrid]`几何审计 | 无已知出生/配置缺口 | 已验证 |
| NPC | 24 | 专题三六人双机制4选3 | 目录1/1；4选3选择/默认携带2/2 | 六运行时前缀16/16；24种缺牌组合、72次逐卡结算、8次三牌任务完成；指向3/3；共享队列10/10 | 刀客2条`[EnemyAtk]`；队列1条`[EnemyAtk]`；战场指向139条既有HUD调试日志 | 无已知NPC规则缺口 | 已验证 |
| 路线牌 | 30 | 当前路线目录/配方 | 30张逐ID完整契约；配方6/6；目录/文本2/2 | 30个CardId、7种地势共91次结算；路线集成/规范化/迁移/商店33/33 | 0 | 无已知路线牌缺口 | 已验证 |
| 共享状态/指向/队列 | 不适用 | 专题二全局规则 | 状态5/5、战斗规则1/1、标记2/2、战斗运行时4/4、Foundation10/10、CounterBlock15/15、队列10/10 | 47/47；全198逐卡合法目标与原子拒绝门禁2/2 | 仅既有`[EnemyAtk]`审计：1+2+1+12+1条 | 无已知共享规则缺口 | 已验证 |
| 六套装备 | 6套×2/4/6 | 专题五 + 破军专项规格 | 六套描述符1/1；破军/青囊/追风/蚀骨目录契约随前缀通过；装备全前缀55/55 | 破军12/12、青囊5/5、追风5/5、蚀骨5/5；Speed不再进入新生成/重铸池且旧存档仍可读取；现代装备UI通过 | 0 | 玄甲/山河六个描述符无战斗消费者；玄甲阶位、山河逐地势取整需裁决 | 部分验证 |
| 平衡观测 | 2400场 | 专题六目标区间 | `FullMatrixExecution` 1/1；2400场全部终局、0 MaxRounds | 2118胜/282负，88.25%；`AuthoredProfilePolicy` 1/1只锁定正式九组倍率 | FullMatrix为SuccessWithWarnings、0 error | 贪心策略低估铺垫牌；分成长档胜率校准未完成 | 部分验证 |

## 已生成的新鲜报告

- `Saved/Automation/Task8_FinalFresh_Sorcerer_20260811/Report/index.json`：32/32。
- `Saved/Automation/Task8_FinalFresh_HeroSpellTask_20260811/Report/index.json`：19/19。
- `Saved/Automation/Task8_FinalFresh_TaskNpcSpellTask_20260811/Report/index.json`：2/2。
- `Saved/Automation/Task8_FinalFresh_CardResolutionQueue_20260811/Report/index.json`：10/10；1条既有敌方攻击审计warning。
- `Saved/Automation/Task8_FinalFresh_CardCatalog_20260811/Report/index.json`：1/1。
- `Saved/Automation/Full198_Task1_CardQuality_20260811/Report/index.json`：2/2。
- `Saved/Automation/Task8_FinalFresh_CardText_20260811/Report/index.json`：1/1。
- `Saved/Automation/Task8_FinalFresh_CardDocumentation_20260811/Report/index.json`：1/1，无更新开关。
- `Saved/Automation/Task8_FinalFresh_CompanionBirth_20260811/Report/index.json`：1/1。
- `Saved/Automation/Task8_FinalFresh_CompanionBirthMigration_20260811/Report/index.json`：3/3。
- `Saved/Automation/Full198_Task2_CombatStatus_20260811/Report/index.json`：5/5，0 warning。
- `Saved/Automation/Full198_Task2_CardCombatRules_GREEN_v2_20260811/Report/index.json`：1/1；1条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task2_MarkRules_20260811/Report/index.json`：2/2，0 warning。
- `Saved/Automation/Full198_Task2_CardBattleRuntime_GREEN_v3_20260811/Report/index.json`：4/4；2条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task2_HeroFoundation_20260811/Report/index.json`：10/10；1条既有`[EnemyAtk]`公鸡攻击审计warning。
- `Saved/Automation/Full198_Task2_HeroCounterBlock_20260811/Report/index.json`：15/15；12条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task2_Guard_20260811/Report/index.json`：8/8；4条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task3_HeroCatalog_20260811/Report/index.json`：2/2，0 warning。
- `Saved/Automation/Full198_Task3_HeroGeneric_20260811/Report/index.json`：12/12，0 warning。
- `Saved/Automation/Full198_Task3_HeroBlade_20260811/Report/index.json`：13/13，0 warning。
- `Saved/Automation/Full198_Task3_HeroGuard_20260811/Report/index.json`：5/5，0 warning。
- `Saved/Automation/Full198_Task3_HeroHealer_20260811/Report/index.json`：11/11，0 warning。
- `Saved/Automation/Full198_Task3_HeroHunter_20260811/Report/index.json`：8/8，0 warning。
- `Saved/Automation/Full198_Task3_HeroMage_20260811/Report/index.json`：19/19，0 warning。
- `Saved/Automation/Full198_Task3_HeroFormation_20260811/Report/index.json`：7/7，0 warning。
- `Saved/Automation/Full198_Task3_HeroIntegration_20260811/Report/index.json`：10/10；63条既有`[EnemyAtk]`聚焦模拟审计warning。
- `Saved/Automation/Full198_Task3_HeroCardPoolV12_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task4_BladeCatalog_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task4_BladeRuntime_20260811/Report/index.json`：43/43；10条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task4_BladeMigration_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task4_Guard_20260811/Report/index.json`：8/8；4条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task4_Healer_GREEN_v2_20260811/Report/index.json`：9/9；7条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task4_Hunter_20260811/Report/index.json`：5/5；1条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task4_Sorcerer_20260811/Report/index.json`：32/32，0 warning。
- `Saved/Automation/Full198_Task4_Formation_20260811/Report/index.json`：4/4，0 warning。
- `Saved/Automation/Full198_Task4_CompanionBirthArchetypes_20260811/Report/index.json`：1/1，逐职业1024种子，0 warning。
- `Saved/Automation/Full198_Task4_CompanionBirthPoolV13_20260811/Report/index.json`：3/3，0 warning。
- `Saved/Automation/Full198_Task4_CompanionRules_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task4_PartnerBackpackConfig_20260811/Report/index.json`：1/1；13条重复`[WarehouseGrid]`几何审计warning。
- `Saved/Automation/Full198_Task4_CompanionPersonalDeck_20260811/Report/index.json`：1/1；23条重复`[WarehouseGrid]`几何审计warning。
- `Saved/Automation/Full198_Task5_NpcCatalog_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task5_NpcAll24_GREEN_v2_20260811/Report/index.json`：2/2，覆盖24种缺牌组合、72次逐卡结算与8次法术任务完成，0 warning。
- `Saved/Automation/Full198_Task5_NpcBladeTiming_20260811/Report/index.json`：4/4；2条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task5_NpcSupport_20260811/Report/index.json`：3/3，0 warning。
- `Saved/Automation/Full198_Task5_NpcHealerFormation_20260811/Report/index.json`：4/4，0 warning。
- `Saved/Automation/Full198_Task5_NpcSpellTask_20260811/Report/index.json`：2/2，0 warning。
- `Saved/Automation/Full198_Task5_QuestNpcSelection_20260811/Report/index.json`：1/1，逐NPC 256种子，0 warning。
- `Saved/Automation/Full198_Task5_QuestNpcLoadouts_20260811/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task5_SharedQueue_20260811/Report/index.json`：10/10；1条既有`[EnemyAtk]`审计warning。
- `Saved/Automation/Full198_Task5_QuestNpcTargeting_GREEN_v2_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task5_FormationTargeting_GREEN_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task5_FormationBoardTargeting_GREEN_20260812/Report/index.json`：1/1；139条既有`[Board]/[HPSnap]/[HudSet]/[HudView]/[HpText]`调试warning，0 error。
- `Saved/Automation/Full198_Task6_RouteRecipe_GREEN_v2_20260812/Report/index.json`：6/6；含30张逐ID完整契约与91次实际结算，0 warning。
- `Saved/Automation/Full198_Task6_CardRouteIntegration_GREEN_v2_20260812/Report/index.json`：16/16，0 warning。
- `Saved/Automation/Full198_Task6_CanonicalMaterialization_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task6_ThreeChapterCanonical_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task6_RouteCardEntriesV9_20260812/Report/index.json`：4/4，0 warning。
- `Saved/Automation/Full198_Task6_RouteMerchant_BASELINE_20260812/Report/index.json`：12/12，0 warning。
- `Saved/Automation/Full198_Task6_GlobalCardCatalog_GREEN_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task6_GlobalCardText_GREEN_v2_20260812/Report/index.json`：1/1，0 warning；精确注册路径为`GameXXK.Integration.CardText`。
- `Saved/Automation/Full198_Task7_SetCatalog_BASELINE_20260812/Report/index.json`：1/1，0 warning。
- `Saved/Automation/Full198_Task7_PoJun_BASELINE_20260812/Report/index.json`：12/12，0 warning。
- `Saved/Automation/Full198_Task7_QingNang_GREEN_20260812/Report/index.json`：5/5，0 warning；旧八门轮转手牌计数夹具已按抽3弃1校准，并锁定延迟抽牌`1→0`。
- `Saved/Automation/Full198_Task7_ZhuiFeng_BASELINE_20260812/Report/index.json`：5/5，0 warning。
- `Saved/Automation/Full198_Task7_ShiGu_BASELINE_20260812/Report/index.json`：5/5，0 warning。
- `Saved/Automation/Full198_Task8_PlayabilityGroup_FINAL_20260812/Report/index.json`：2/2，覆盖198张卡×七地势1386次结算、条件缺失基础效果、手动指向原子拒绝及六组8+5+3，0 warning。
- `Saved/Automation/Full198_Task8_SorcererHandQueue_FINAL_20260812/Report/index.json`：4/4，覆盖20张满手牌排队、无候选回退与检索归属隔离，0 warning。
- `Saved/Automation/Full198_Task8_SorcererSaveResume_FINAL_20260812/Report/index.json`：5/5，覆盖任务/选择/20+2排队的存档恢复，0 warning。
- `Saved/Automation/Full198_Task8_HeroFoundation_FINAL_20260812/Report/index.json`：10/10；1条测试预期的`[EnemyAtk]`致死攻击审计warning，0 error。
- `Saved/Automation/Full198_Task8_SorcererTaskLifecycle_FINAL_20260812/Report/index.json`：3/3，覆盖五牌重放、通用奖励额外重放与第二张分支锁定，0 warning。
- `Saved/Automation/Full198_Task9_RouteBalance_AFTER_INDEPENDENT_DISCOUNTS_20260812/index.json`：2400/2400真实规则场景完成，0 failed、0 runtime error。
- `Saved/Automation/Full198_FinalFresh_A_20260812/*/Report/index.json` 与 `Full198_FinalFresh_B_20260812/*/Report/index.json`：55份独立报告，共333项；302 Success + 31 SuccessWithWarnings，0 failed、0 notRun、0 error。250条warning分类为既有`[EnemyAtk]`98、`[WarehouseGrid]`13、战场HUD诊断139。
- `Saved/Automation/Full198_FinalDocs_20260812/Update/Report/index.json` 与 `Verify/Report/index.json`：卡牌目录更新/无开关复验各1/1，0 warning、0 error。
- `Saved/Automation/Full198_TextLeak_RED_20260812/CardDocumentation/index.json`：文档可见效果列精确复现4张卡的8条失败（中文缺失+内部枚举泄漏）；同轮`CardText` 1/1通过，证明局内格式器不是根因。
- `Saved/Automation/Full198_TextLeak_GREEN_20260812/CardDocumentationUpdate/index.json` 与 `CardDocumentationVerify/index.json`：更新/无开关复验各1/1，0 warning、0 error；`CardCatalog` 1/1，`AllCardsPlayability` 2/2，0 warning、0 error。
- `Saved/Automation/Full198_YaoWangSideAnchor_RED_20260812/index.json`：药王归元致死首段精确复现1/1失败；失败仅为死亡选中目标不再提供敌方阵营锚点。
- `Saved/Automation/Full198_YaoWangSideAnchor_GREEN_20260812/index.json`：1/1通过，0 warning、0 error；锁定同一效果链后续`SelectedTargetSide`仍按原选中身份所属阵营选择存活目标。
- `Saved/Automation/Full198_SpeedAffix_GREEN_20260812/AffixCatalog/index.json`、`DeterministicRolls/index.json`与`PaidReforgeTransactions_CurrentContract/index.json`：3/3通过，0 warning、0 error；新生成及重铸不再产出Speed，旧Speed定义/旧装备仍兼容。
- `Saved/Automation/Full198_TaskNpcProjection_GREEN_20260812/index.json`：1/1通过，0 warning、0 error；任务NPC战斗投影使用NPC自身等级属性，不继承主角装备属性。
- `Saved/Automation/Full198_EQUIPMENT_BROAD_FINAL1_20260812/index.json`：装备全前缀55/55 Success，0 warning、0 error、0 failed、0 notRun；同时锁定强化、固定拆解收益、1砂重铸、套装、旧Speed兼容和现代装备UI契约。
- `Saved/Automation/Full198_PostFixRegression_20260812/*/index.json`：药师伙伴、任务NPC、主角牌与全卡可打出性共143项；124 Success + 19 SuccessWithWarnings，0 failed、0 notRun、0 error。80条warning均为既有`[EnemyAtk]`审计日志。
- `Saved/Automation/Full198_BalanceMatrix_GREEN_20260812/index.json`：`FullMatrixExecution` 1/1 SuccessWithWarnings、0 failed、0 notRun、0 error；九组汇总2400场全部终局，2118胜/282负、88.25%、0 MaxRounds。
- `Saved/Automation/Full198_AuthoredBalancePolicy_GREEN_20260812/index.json`：`AuthoredProfilePolicy` 1/1 Success、0 warning、0 error；只认证正式九组生命/攻击/防御倍率，不代表最终平衡完成。
- `Saved/Automation/Full198_DATA_BROAD_FINAL1_20260812/index.json`、`Full198_MVP_BROAD_FINAL1_20260812/index.json`、`Full198_ROUTE_BROAD_FINAL1_20260812/index.json`、`Full198_INTEGRATION_BROAD_FINAL1_20260812/index.json`：分别261/261、76/76、50/50、40/40；全部0 failed、0 notRun、0 error。
- `Saved/Automation/Full198_ModernInventoryUI_GREEN2_20260812/index.json`：现代装备UI 1/1 Success、0 warning、0 error。
- `Saved/Automation/Full198_BoardPresentation_GREEN2_20260812/index.json`：Board GREEN2 1/1 SuccessWithWarnings、0 failed、0 notRun、0 error；269条为既有显示诊断日志。
- `Saved/Automation/Full198_SaveV15_GREEN_20260812/index.json`、`Full198_FollowerLocationBackfill_GREEN_20260812/index.json`、`Full198_PlayableRootSaveFlow_GREEN_20260812/index.json`：v15迁移、旧档位置回填、手动槽隔离各1/1 Success、0 warning、0 error。
- `Saved/Automation/Full198_GAMEXXK_BROAD_FINAL2_20260812/index.json`：`GameXXK` 全前缀573/573，565 Success + 8 SuccessWithWarnings，0 failed、0 notRun、0 error。28条 warning 全部归类为Atlas预算/空资产回退21、无game viewport 5、测试用未知状态255回退1、缺失BlockShield图标资产回退1。
- `Saved/Automation/Full198_BATTLE_BROAD_FINAL2_20260812/index.json`：Battle全前缀50/50 Success，0 warning、0 failed、0 notRun、0 error。
- `Saved/Automation/Full198_BlackBearFixture_GREEN2_20260812/index.json` 与 `Full198_IronfeatherFixture_GREEN_20260812/index.json`：`BlackBearThickHideDirectPlayerCard`、`IronfeatherFirstHitDirectPlayerCard` 两条 `EnemyMechanics` 各1/1 Success，0 warning、0 error。
- `Saved/Automation/Full198_StatusEffectsWidget_GREEN_20260812/index.json`：1 SuccessWithWarnings、0 failed、0 notRun、0 error；把已变为毒爆伤害牌的旧 NPC 夹具换为纯状态 NPC 卡。21条 warning 为20条 `[HudSet]/[HPSnap]` 显示诊断和1条缺失BlockShield图标资产回退。
- `Saved/HarnessReports/full198-final-real-pie-safe-warm-final-20260812.json`：最终真实PIE唯一真源，`ok=true`且`cleanup.ok=true,errors=[]`；主菜单真实点击→青山镇→F任务→active follower→手动保存→商店→城门→路线→战斗/HUD连续通过。分离事件key=S、玩家移动310.64cm、距初始NPC 387.15cm、NPC实际跟随268.81cm；runtime/live、saved/live和玩家存档三种位置误差均为0。
- `scripts/gamexxk_real_play_flow_mcp.py` 的旧“NPC固定不动”夹具已校准为active follower、实际移动和位置持久化，saved/live位置容差收紧为5cm；当前新鲜Python回归66/66通过。脚本依据玩家相对NPC的主轴选择反方向并走出跟随半径；旧固定D方向偶发未走出96cm的RED仅作诊断。删除主槽前原子写`GameXXK_MVP_SaveSlot_1.sav.codex-real-flow-backup`，正常、keep-pie和cleanup异常均在`finally`恢复，前次崩溃在下次运行开始时先恢复；任意fixture、PIE或存档cleanup失败会让顶层报告转红。
- 最终实跑capture为`existed=false,size=0,recovered_previous_run=false`，restore为`existed=false,size=0,backup_removed=true`；结束后主槽和sidecar均不存在。
- `Saved/Codex/real_flow_after_qingshan.png` 为第二轮暖缓存正常视觉证据；首轮冷启动的Lumen曝光是资产初始化瞬态，同轮18秒后的任务截图也已正常，因此不列为生产缺陷。

## Task 2 复审结论

- 生产规则未因本次复审改动；两处失败均定位为新终稿落地后的旧总测试契约。
- `GameXXKCardCombatRulesTest.cpp` 已改为在完整敌方牌边界结算格挡，并锁定“100%当前攻击力 + 受击后当前护甲”。
- `GameXXKCardBattleRuntimeTest.cpp` 已迁移反震甲、铁锁横江和六合护法场景：格挡按敌牌边界触发；守护次数读取稳定 GuardLink；六合护法使用合法5张唯一法师携带牌并验证4内力换全队3护甲。
- 对应 RED 报告保留在 `Full198_Task2_CardCombatRules_FIX_GREEN_20260811`、`Full198_Task2_CardBattleRuntime_20260811` 与 `Full198_Task2_CardBattleRuntime_GREEN_v2_20260811`；随后均由上列独立 GREEN 报告关闭。

## Task 3 复审结论

- 36张主角牌目录和九个行为/迁移分区共88/88通过；本 Task 未观察到需改生产规则的新缺陷。
- 解锁边界为1级32张（8张初始泛用 + 24张职业牌直接可选），5/10/15/20级各解锁1张后续泛用牌；携带数始终精确为8。
- 剑意终稿锁定为`攻击力 × (260% + 20% × 气势层数) + 气势层数`，随后消耗全部气势；达到3层时最多返还1气力，不存在独立“剑意状态”。
- 法师任务只统计当前携带的8个唯一主角牌ID，8张都打出后按原顺序重放基础效果且只执行首张任务奖励；阵师4张牌在全部地势下均保持单敌方锚点，收益由实时地势展开。

## Task 4 复审结论

- 六职业伙伴目录/运行时共102/102通过；出生、迁移、伙伴规则和两条背包门禁再8/8通过，未观察到生产行为缺口。
- 刀客18张已覆盖冲锋/收招、重放、留式、流血与气势倍率及多次反击；守卫18张锁定每张敌牌、每个独立来源只消耗1层格挡。
- 药师首次全组运行暴露旧测试仍把已重做的法师卡`Profession.Sorcerer.RanLingHuanYuan`当作自损触发器；已改为现行`Profession.Healer.XingQiZhen`的35→34与30→29跨线夹具。生产规则未改，冷编译、单叶1/1和药师全组9/9均通过。
- 普通伙伴出生锁定2核心+3主流派+1等概率自由位；阵师锁定2张换场+3张主流派收益+1张全收益自由位。出生池始终6张、携带始终5张，等级/星级不会增牌或重抽。

## Task 5 复审结论

- 六名NPC的24张目录牌均已在自身NPC上下文中预览并结算；新增矩阵进一步穷举每人四种“缺一张”携带组合，共24组三牌、72次逐卡结算。
- 重箭、药效、DOT/毒爆及命名状态的消费牌均要求在自身效果链前段先建立前置；宋金宝、岳白的8个三牌组合均实际完成任务，并且每次只产生3次基础重放与1次首牌奖励。
- 被排除的第四张牌不会进入手牌、抽牌堆、弃牌堆、消耗区、待处理队列、检索候选、任务锁、完成历史或任务快照；4选3在逐NPC 256种子下保持确定性与均匀覆盖。
- 藤桥飞渡旧诊断仍期待悬崖/森林自动改为我方群体，和现行终稿冲突；测试已校准为七种地势固定单敌方锚点，并锁定敌方可选、我方/自身不可选。此处只改测试契约，未改生产行为。

## Task 6 复审结论

- 路线目录精确为30张：10张通用、10张地势、5张稀有、5张首领；新增逐ID表锁定名称、稀有度、品质、费用、目标、完整效果/条件、画框与获取键，并反向保证没有遗漏或多余路线牌。
- 新增运行时门禁真实执行全部30个CardId：10张地势牌分别覆盖平原、山崖、森林、水岸、渡口、村落、洞穴，其他20张在平原结算，并补绝境反击低血量分支，共91次成功预览/结算。
- 首次路线集成全组暴露4处旧测试夹具：月白旧立绘路径、任务NPC旧自动指派假设、伙伴进度夹具未显式选择NPC，以及把可自动修复的坏Hero选择当成原子失败。均按当前明确流程校准测试；生产代码未改。
- 路线配方、获取、商店、三章规范化、V9迁移、全局目录和文本门禁最终共42/42，`failed=0`、`notRun=0`、0 warning；不存在已知路线牌行为缺口。

## Task 8 复审结论

- 全198张稳定CardId均在七种地势下使用合法拥有者、双友方、双敌方、充足资源与代表性状态完成预览和实际结算，共1386次；同时锁定“条件不满足仍结算无条件基础效果”和“缺失必选目标时不付费、不改运行时”。
- 六组真实混合卡组分别覆盖刀客、守卫、药师、弓手、法师、阵师与六名NPC；每组精确为主角8张、伙伴5张、NPC3张。法师组同时保持主角8牌任务、伙伴5牌任务与NPC3牌任务，检索候选不会跨拥有者。
- 首次矩阵失败仅因测试夹具没有显式写入主角8张装备快照；生产初始化按设计不从混合牌堆猜测装备。补齐夹具后原断言全部通过，未放宽任务、资源或归属契约。
- 满手牌排队、死亡目标回退、同归于尽按玩家胜利、自动重放不扣费/不计主动出牌及存档中断恢复均由独立专项回归通过；当前无已知198张卡牌可打出性、指向或任务串线缺口。

## Task 9 最终复审结论

- 死亡回退目标上的致死首段不再使后续依赖原目标的效果回滚整张自动牌：仅跳过已失效目标的后续目标段，不中途改指向；对应叶测试、原始岳白场景和邻接前缀均通过。
- 法师核心检索允许两张不同手牌实例各自持有一次`-3`内力，仍拒绝同一实例重复叠加；新增独立叶测试、法师33项全前缀和2400场矩阵均通过。
- Python观测目录解析已覆盖`AddHero/AddBlade/AddHealer/AddSorcerer/AddQuestNpcCard`本地构建器；10/10脚本单测通过，当前费用分布为0/1/2/3气力=`43/97/48/10`。
- 现行 FullMatrix 的2400场全部终局，2118胜/282负、88.25%且无MaxRounds；旧离线三批观测已作废。功能门禁已通过，但贪心策略仍低估铺垫牌，分成长档平衡未完成，不在本轮静默改数值。
- 最终冷UBT使用`-NoHotReload -NoHotReloadFromIDE`并返回`Result: Succeeded`；未修改确认好的UI坐标、尺寸或页签布局，仅把伙伴牌组标题由旧12张校准为出生6张、编入5张。
- 局内Tooltip与全卡文档现共同锁定《连营布势》《铁壁如山》《万象归阵》《行军布阵》的简洁中文表述；198张“完整效果”列静态扫描无内部枚举泄漏，唯一英文词为正常的`NPC`。
- 六套装备中破军、青囊、追风、蚀骨已有运行时证据；玄甲仅确认“指定阶位保留50%护甲”，其余阶位及山河逐地势载荷/取整尚无批准规格，因此两套仍不得宣称完成。

## Task 10 收尾缺陷复审结论

- `SelectedTargetSide`只把原选中身份当作阵营锚点，实际接收者仍必须存活；药王归元先击杀所选敌人后，后续敌方群体段不再误判“无目标”并回滚整张牌。
- Speed从所有新生成与重铸候选池退出，但仍保留只读定义和旧装备属性投影，避免旧存档丢词缀；追风套运行时不再依赖Speed。
- 任务NPC进入战斗时改用自身NPC属性投影，不再复制主角装备后的攻防生命/内力；专项与装备全前缀均已覆盖。
- 当前装备经济统一为：有效品质拆解每件固定1砂、1石、10金币；任意品质重铸固定1砂。旧80%概率以及10/30/90砂文本均不再作为当前契约。
- 最新装备55项及卡牌邻接143项均为0 failed、0 notRun、0 error；当前仍不把玄甲/山河描述符当作已实现的运行时套装。

## 2026-08-12 最终主链与门禁收口

- F接任务后 `bFollowerJoined=true`；任务NPC交互调用 `ActivateFollower` 并记录NPC当前世界位置。玩家明确手动保存后，任务、跟随状态、玩家与NPC位置均可恢复。
- 当前存档版本v15会把v14及更早的Accepted/false follower规范化为true；旧档缺失NPC位置时，在关卡恢复跟随并调用 `ActivateFollower` 后回填当前位置。迁移与回填各1/1通过。
- 分前缀证据为Equipment 55/55、Data 261/261、MVP 76/76、Route 50/50、Integration 40/40，全部0 failed、0 notRun、0 error；最终决定性 `GameXXK` 全前缀为573/573（565 Success + 8 SuccessWithWarnings），同样0 failed、0 notRun、0 error。
- 最终全项门禁28条 warning 已全部归类：Atlas回退21、无viewport 5、未知状态255回退1、缺失BlockShield图标1；Battle全前缀50/50完全无warning，黑熊厚皮与铁羽首次受击两条精确回归也各1/1无warning。
- `StatusEffectsWidget` 使用纯状态NPC卡夹具后1/1通过；其SuccessWithWarnings仅来自20条HUD显示诊断和1条缺失BlockShield图标回退，不是状态规则或卡牌结算失败。
- 现代装备UI、Board GREEN2和现代根界面手动槽隔离均有1/1证据；Board保留既有诊断warning，但0 error。
- 早期 `FinalCandidateTargets` 已退役；`AuthoredProfilePolicy` 只锁定正式九组倍率。FullMatrix全部终局也不构成最终平衡认证，铺垫牌估值与分层胜率仍需后续校准。
- 最终safe-warm真实PIE报告为`ok=true`且cleanup全绿：主菜单、青山镇、F任务、active follower实际移动268.81cm、手存三种零误差位置、商店、城门、路线、战斗和HUD均闭环；删除主槽`result=true`，原子sidecar保护完成且主槽/备份均无残留。冷启动Lumen曝光已由后续及暖缓存截图证明为瞬态，不列缺陷。
