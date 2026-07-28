# 装备基础执行状态（2026-07-22）

## 已完成

- Task 1：冻结装备数据类型、五维裸装属性、主角/永久伙伴 20 级上限与兼容字段。
- Task 2：完成 36 个现代装备定义、7 个 Legacy 定义、35 个词缀定义和 18 个套装描述；目录、词缀、套装测试各 1/1 通过。
- Task 3：完成确定性装备生成、200 格有序仓库、主角与最多 12 名永久伙伴的六槽所有权、穿脱/替换/归还事务和 Legacy overflow 恢复规则。
- Task 4：完成固定顺序的属性投影、强化后基础值、通用与套装词缀一次加算、2/4/6 套装效果、团队效果唯一来源和完整换装 Tooltip 差值。
- Task 5：完成强化、付费洗炼预览/接受/取消、运行时穿脱包装、批量分解与旧制装备兼容购买。

## Task 3 验收证据

- `GameXXK.Equipment.Rules`：3/3 通过，0 失败。
- 10,000 次固定种子生成：最终记录 0.037 秒。
- 固定 ASCII CRC、稳定实例 ID、保存重载后继续操作一致。
- 词缀档位不得高于装备品质；现有实例与 pending reforge 共用校验。
- `NextInstanceOrdinal == MAX_int32` 时创建原子失败，输出 ID 清空，完整集合字节不变。
- 201 个 Legacy 仓库实例允许载入；装备降至 200 后自动清除 overflow 标记；overflow 期间新增、卸下和全部归还均受阻。
- 主角与伙伴 loadout 经序列化/反序列化后继续 swap/return-all，结果、所有权冗余和最终字节一致。
- 冷 UBT、编辑器重启、PIE、Task 2 三项回归和 `git diff --check` 均通过。
- 独立规格复审：PASS；独立质量复审：APPROVED。

## Task 4 验收证据

- 冷 UBT 成功，PIE 自动冒烟正常启动并停止。
- 最终 `StartsWith:GameXXK.Equipment`：12/12 通过，0 失败。
- `GameXXK.Equipment.Stats`：4/4 通过；覆盖跨层同类 BP 一次加算、被动属性与事件描述器拆分、混合品质套装计数、团队来源选择和完整换装 Tooltip。
- Task 3 `Rules`：3/3 回归通过；Task 2 `Catalog`、`AffixCatalog`、`SetCatalog` 各 1/1 回归通过。
- Tooltip 始终复用装备事务核心；已装备同槽保持零差值，同时返回真实阻断错误，且不修改输入集合。
- UE 脏包为空，Task 4 文件空白检查通过。
- 独立质量复审：PASS；无 Critical、Important 或 Minor 问题。

## Task 5 验收证据

- 冷 `-NoHotReload` UBT 成功，编辑器重启后 PIE 冒烟成功。
- `GameXXK.Equipment.Economy`：4/4 通过；`Stats`：4/4；`Rules`：3/3；`Catalog`、`AffixCatalog`、`SetCatalog` 各 1/1。
- 强化严格覆盖 +0 至 +10；洗炼候选、扣费、序号和接受/取消保持确定性及原子性。
- 批量分解覆盖受保护确认、逐实例 80% 强化石返还、砂产出、装备清槽和 overflow 恢复。
- 资源接近 `MAX_int32` 或实例/洗炼序号耗尽时，在扣费、删除或改槽前原子拒绝；结果不报告无法到账的虚假增量。
- UE 脏包为空，五个 Task 5 文件空白检查通过；独立复审无重要问题。

## 正在执行

- Task 6：存档版本 0–7 迁移、v6→v7 装备实例转换、旧接口兼容镜像与磁盘备份/失败恢复。
- B02 装备视觉初版：43/43 次内置生图与透明底文件均已落盘；正在完成最终 QC、联系表与报告，尚未导入 UE。

## 当前资源状态

- 根因确认为本长任务派生子代理的完整历史 rollout 日志；不是 UE 项目、存档或生图源文件。
- 已只删除 81 份本主任务下已结束的子代理日志，回收约 21.43 GB；当前主对话文件保留。
- C 盘恢复到约 21.01 GB 可用；Task 4 冷流水线和 B02 已恢复。
- 后续子代理必须使用最小上下文派发，禁止再次为每个子代理复制完整长对话历史。

## 本轮续作记录（2026-07-23）

### 战斗装备投影与事务门面

- 卡牌战斗已从装备权威快照投影主角和永久伙伴的属性、速度与效果描述器；任务 NPC 不读取永久装备。
- `GameXXK.Equipment.BattleIntegration`、`GameXXK.Equipment.Facade` 的现有自动化覆盖装备投影、仓库 200 格上限、路线锁定、NPC 拒绝装备、完整套装 Tooltip、强化、洗炼、卸装与分解事务。

### Task 10：真实规则确定性模拟基础

- 新增 `FGameXXKCombatSimulationRules::RunScenario`：它只经由 `FGameXXKCardBattleAdapter` 与 `GameXXKCardRules` 执行开始战斗、合法出牌/目标、强制弃牌/洞察选择、结束回合、敌方意图和敌方阶段完成；没有第二套伤害、状态、抽牌或敌方公式。
- 核心自动化 `GameXXK.Simulation.Foundation.Core` 验证：同一个输入与种子得到字节一致的 metrics/trace、场景输入不被修改、每场有限终止，并且攻击 trace 的 `HealthDelta` 正确记录敌方受伤的负总生命差分。
- 装备矩阵 `GameXXK.Simulation.Foundation.EquipmentMatrix` 覆盖 5 个等级的裸装，以及 6 套装 × 3 品质 × +0/+10 × 5 等级；共 185 个场景、370 次重复真实规则执行。最新实测耗时 `0.321739s`。
- 裸装基准 `GameXXK.Simulation.Benchmark.NakedHundred`：单场 `0.000974s`，100 场 `0.042027s`，按同一核心路径线性外推 2400 场约 `1.009s`。这是模拟核心时间，不包含冷编译、编辑器启动或报告写入；后续三章节认证仍按 30 分钟目标、60 分钟硬上限计时。
- 最新冷编译日志显示 UBT `Result: Succeeded`。一次外层自动化调用在 64 秒工具时限到达前已完成 UBT 并启动编辑器；随后通过 UE MCP 证实编辑器就绪并完成 PIE 启停，因此该超时记录为工具时限观察，不作为编译失败。

### 下一依赖任务

- 三章节路线、21 个敌人目录、显式 1P/2P/3P 编队、NPC 闭环和正式 2400 场认证将遵循 `docs/superpowers/plans/2026-07-22-three-chapter-route-enemies-balance.md`。该计划的前置是当前装备模拟基础；数值认证只能调用本模拟器，并且不得自动回写怪物、卡牌或装备源数值。

### 三章节路线 Task 6：敌方意图结算（进行中）

- 存活目录敌人在玩家行动期间同时保留可保存的意图预报；来源、槽位、目标、效果、倍率结果和速度降序／槽位升序均在预报时锁定。
- 已结算直接伤害（含多段）、护甲、治疗、状态、气力扣减、临时攻击、临时速度与正面状态移除。速度只影响下一次敌方意图排序，并在受影响敌方阶段完成后撤销。
- 蓄力牌现在先展示警告并锁定目标，下一次敌方阶段才结算；锁定目标阵亡时不会改选。
- 白猿“扰乱”已闭环：敌方阶段只保存一个待生效的 `+1` 耗气请求；下一玩家抽牌完成后，从当时可正常打出的手牌中确定一张费用最高的牌（同费用按获得序号、实例 ID 稳定裁决），为它附加仅一次、仅该实例有效的耗气修正。预览与实际出牌共用同一修正路径，不会修改全局卡牌定义。
- 该修正会在成功出牌后消耗；目标被强制弃置、玩家回合结束弃置或无可选手牌时立即清理，不会在同一回合补牌后复活。胜利／失败终态会清除尚未物化的请求；已物化的单牌修正仅可在玩家／胜利阶段保存。
- 存档校验限制其来源必须为敌方、目标必须仍在当前手牌、数量最多一个，且完整校验触发、目标、范围、数值、过期方式及所有未使用条件字段的规范形态；普通未绑定修正保持原有兼容性。
- 最新证据：冷 `-NoHotReload` UBT 成功并重启编辑器；`GameXXK.Battle.EnemyIntentRules.WhiteApeDisturbBindsOneNextHandCardEnergySurcharge` 为 1/1 Success，完整 `GameXXK.Battle.EnemyIntentRules` 为 13/13 Success，`git diff --check` 与新增测试文件空白检查均通过。
- 敌方意图现在以完整 `FGameXXKRuntimeState` 事务结算：效果包、遗物受击触发、意图游标／蓄力锁定和最终旧战斗投影全部先在副本中执行，任何后续效果或投影失败都会还原完整状态。公开输出仍保持旧接口语义：失败时为默认意图、空伤害记录和 `false` 完成标志。
- 新增红绿回归以“先给来源加 5 护甲、再施加非法 `+2` 单牌耗气”构造失败，断言完整运行时、索引、游标、冷却、蓄力锁定和输出均不泄漏。最新独立 MCP 复跑 `GameXXK.Battle.EnemyIntentRules` 为 13/13 Success；规格复核 PASS，代码质量复核 APPROVED。
- Task 6 尚未完成：意图文字／Tooltip／HUD 属于后续 UI 任务；精英被动和 Boss 二阶段属于后续 Task 7。

### 三章节路线 Task 7：精英被动与 Boss 阶段（进行中）

- 已完成铁羽斗鸡的首个被动边界：仅由玩家打出的直接伤害会尝试触发；首次实际扣除生命的伤害在护甲、闪避与最终伤害计算之后减半。护甲完全吸收、1 点伤害减半为 0、闪避、DOT、反伤、被动伤害与自损均不会消耗该一次性被动。
- 玩家卡牌的普通攻击、直接效果与联合攻击统一经由同一条“玩家直接伤害”入口，而通用伤害路径保持不触发该被动。实现按敌人目录中的 `PassiveId` 分发，不比较本地化名称或硬编码敌人 ID。
- 运行时缺失铁羽状态时会在副本中初始化；已有状态若与目录定义不一致则整张牌原子失败。首次命中标记随现有 SaveGame 归档保存，失败不会泄漏部分伤害或状态变更。
- 已完成黑熊“厚皮”：仅玩家卡的直接伤害按“防御 → 易伤 → 护甲 → `floor(剩余伤害×85/100)` → 当前 HP 上限”结算。它不改变 DOT、反射／通用非卡牌伤害、被动伤害或自损；全敌方牌仍经同一入口。状态缺失与目录 ID 错配沿用同一原子初始化／失败规则。
- 黑熊回归覆盖普通单体、护甲顺序、HP 上限顺序、全敌方牌、DOT 排除及定义错配原子失败；测试在每次访问伤害数组前先验证结算成功与唯一结果，避免未来失败被越界访问掩盖。
- 已完成青角羊王“留甲”：敌方阶段开始时，目录 `PassiveId=BluehornArmorRetention` 的存活敌人保留 `floor(当前护甲/2)`；其他敌人沿用清空护甲。缺失状态在副本中初始化，目录 ID 错配会让整段阶段切换原子失败；9→4→2、8→4、1/0→0、混编、SaveGame 和失败输出不泄漏均有回归。
- 敌方阶段收尾已按冻结顺序事务化：敌方 DOT → 终局判定 → 仅仍存活敌人的蓄力／临时属性维护 → 遗物回合开始效果 → 下一轮意图预测。DOT 击杀青角羊王不会再消耗其未结算蓄力或临时攻击，也不会生成未来意图。
- `ResolveEnemyPhase` 现包裹完整运行时状态与伤害输出：即使第一张敌方意图已真实结算、随后在收尾或重建预报失败，也会保留调用方原始状态和原始输出。相应回归用真实双敌阵列验证“前半轮不得泄漏”。
- 已完成苍鬃狼王“标记猎杀”：仅其目录直接伤害对实际锁定的、存活且带 `Mark` 的我方目标按 `floor(基础数值×120/100)` 放大；已保存的意图效果同时驱动预测与实结算。`MarkedParty` 不再退化为攻击未标记的最低血目标，任意含 `RequiredTargetStatus` 的目录意图都会先用实际目标验证资格后才可入选；`PreyTarget` 的原有兜底未改。
- 苍鬃回归覆盖标记增伤的预测／三连击实结算一致性、普通敌与通用伤害隔离、无标记跳过 `ContinuousHunt`、13→15 向下取整，以及多角色时“非目标的标记不得误放大实际目标”。
- 已完成赤獠猪王“怒气”：仅由存活我方打出的直接卡牌伤害、且在闪避／防御／护甲／重定向后的实际受击者确实损失生命时，赤獠才获得 1 层 Rage；上限为 5。Rage 是统一受限的可保存状态，`AddCombatStatus`、运行时／存档校验和 RageStrike 预报共用该 5 层上限，故“怒獠”最多为基础值 `+100`，不会因手工或损坏的 6–8 层存档越界。DOT、通用／反击伤害、护甲全吸收、闪避和“重定向离开赤獠”均不叠怒；“重定向进入赤獠”按实际受击者正确叠 1 层。
- 赤獠回归先真实红测外部 6 层 Rage／非法保存 6 层／RageStrike `+120` 的失败，再以最小状态上限修复转绿；覆盖保存往返、超限校验拒绝、预测与实际结算同为基础 `+100`、重定向进入及通用伤害排除。独立规格审查 PASS、质量审查 APPROVED。
- 已完成白猿“状态守卫”：每名存活的目录 `PassiveId=WhiteApeStatusGuard` 敌人，在每个玩家回合中第一次**实际成功获得**状态层数后获得 `+8` 护甲并消耗自己按 `UnitId` 保存的守卫。`AddCombatStatus` 保持纯状态原语；玩家直击附带状态、普通 `ApplyStatus`、首个直伤反应状态和敌方意图状态均在返回正加层后才进入同一运行时守卫。免疫、封顶、0 层、死亡和致死后未施加状态均不消耗守卫。
- 白猿状态缺失会在事务副本中初始化并写入目录 ID；定义错配会拒绝整笔结算。守卫随 SaveGame 保存；只在 `CompleteEnemyCardPhase` 已成功进入玩家阶段后重置，玩家结束回合、敌方阶段失败或终局均不提前刷新。两只白猿同时存在时彼此不共享触发与护甲。
- 白猿红测先证明 7 个行为断言缺失，修复后定向 `WhiteApeStatusGuard` 8/8 Success；其中第二次直击的回归断言显式扣除本次护甲吸收，只检测状态守卫是否错误重复给予 `+8`。完整 `GameXXK.Battle.EnemyMechanics` 为 11/11 Success，`GameXXK.Battle.EnemyIntentRules` 为 25/25 Success。
- 已完成盘角鹿“回春”冷却：预报时仍按现有“最低生命百分比、稳定序号／单位 ID 打破平局”的规则锁定一名存活敌方目标，实结算绝不重选；即使该目标满血而治疗为 0，只要回春卡成功结算，目录 `CooldownRounds=2` 仍会进入该鹿的可保存状态。回春所在敌方阶段完成时只清除“本阶段启动”标记，不扣冷却；其后两次成功完成的敌方阶段依次 `2→1→0`，每次正冷却时强制跳过回春并保留固定轮转。
- 盘角鹿冷却启动、归零和推进都确认来源为存活的目录 `PassiveId=DeerHealCooldown`，缺失状态可初始化，定义错配拒绝整笔事务；开始、游标、锁定目标和失败输出均不会在失败路径泄漏。新标记与冷却均为 SaveGame 字段。
- 盘角鹿红测覆盖目标锁定、满血成功结算、SaveGame 往返、同阶段不减、两次未来阶段递减、回春禁用／恢复以及失败原子性；修复后 3/3 定向 Success。完整 `GameXXK.Battle.EnemyMechanics` 为 11/11 Success，`GameXXK.Battle.EnemyIntentRules` 为 28/28 Success。
- 最新验证：冷 `-NoHotReload` 流水线成功并重新启动编辑器；真实、日志标记隔离的 MCP 自动化已通过上述定向与完整套件，`git diff --check` 无空白错误。青角羊王、苍鬃与赤獠均通过既有独立规格和质量复核。
- 下一段继续以同一生命周期钩子覆盖其余精英被动、Boss 阶段切换与特殊顺序；不会把未实现的 UI 提示或 Boss 二阶段标为完成。

### 三章节路线 Task 1：存档架构与迁移

- 全局存档版本提升为动态当前版本 `8`；`UGameXXKMVPRules::GetCurrentSaveVersion()` 是测试、迁移和备份名称的统一入口。
- 新增保存的路线进度、敌人状态、战斗槽位／等级和意图效果契约；旧状态枚举严格追加，未更改既有序列化值。
- 旧的进行中路线迁移为第 1 章，根种子取原 `RouteSeed`，章节种子为 `{RootSeed, Derive(RootSeed,2), Derive(RootSeed,3)}`（按地图种子规范化），战斗等级锁定为当时玩家等级并限制在 `1–20`；既有路线图序列化签名不变。
- 旧的纯伤害敌方意图会迁移为一条 `DirectDamage` 解析效果，同时保留原始 `Damage`、攻击者及目标字段。
- 实际槽加载现在采用 `<Slot>.PreV<动态目标版本>Backup[.NNN]`：先对原对象序列化并校验和，再只复用完全匹配的备份；不匹配备份永不覆盖，会创建 `.001`、`.002` 等尝试槽。升级主档写入或重载失败时，只从本次校验和匹配的备份回滚并复验。
- 新增结构验证，拒绝不完整的三章节快照与未命名／非法的敌人状态；当前版本存档仍为只读加载。
- 新鲜证据：冷 `-NoHotReload` UBT 成功、编辑器重启、PIE 启停成功；`GameXXK.Route.ThreeChapter`、`GameXXK.MVP.SaveGame.ThreeChapterVersionMigration`、`GameXXK.MVP.SaveGame.MigrationTransaction`、`GameXXK.Equipment.SaveMigration`、`GameXXK.Simulation.Foundation` 全部成功。

### 三章节路线 Task 2：21 种敌人的不可变目录

- 新增 `FGameXXKEnemyCatalog` 与 Blueprint 可读的敌人、意图、意图效果及运行时计算属性数据契约；`ComputeStats` 按冻结规则对 `CombatLevel - 1` 进行半入远离零取整，且将 HP／攻击／防御／速度分别限制在 `1/1/0/1` 的安全下限。
- 目录精确包含 3 章各 `4 普通 + 2 精英 + 1 Boss` 共 21 种敌人；第 1 章 Boss 为金钱鼠，第 2 章 Boss 为黑熊，第 3 章 Boss 为老虎。普通、精英、Boss 的意图数分别冻结为 `3/4/6`，三位 Boss 均在 50% 生命阈值进入各自二阶段。
- 21 个条目已绑定稳定的 Codex ID、预留的肖像／战斗视觉软路径，以及数据化的直接伤害、护甲、治疗、状态、气力扣减、攻防速修正、反击／蓄力等效果字段。真正结算将由后续单一敌方意图解析器完成，不在目录内另建伤害公式。
- `GameXXK.Data.EnemyCatalog` 现逐一校验身份、中文名、章节、层级、基础与成长数值、速度、被动／阶段、意图数量、全局唯一性、跨章节池隔离、软路径合法性，且额外锁定所有 21 个敌人的意图 ID 与排列顺序。
- 新鲜证据：冷 `-NoHotReload` UBT 成功、编辑器重启、PIE 启停成功；`GameXXK.Data.EnemyCatalog`、`GameXXK.Route.ThreeChapter.Schema`、`GameXXK.Simulation.Foundation.Core`、`GameXXK.Simulation.Foundation.EquipmentMatrix` 全部成功。

### 三章节路线 Task 3：确定性敌人编队与显式槽位

- 新增 `FGameXXKEncounterRules`：所有采样仅使用由章节种子、节点 ID、节点种类混合出的本地 `FRandomStream`；相同输入产生相同阵容，失败绝不修改调用方的输出数组。
- 槽位规则已冻结并写入运行时入口：普通节点为两只不同普通怪、占 `1P/3P`，中央 `2P` 留空；精英节点为两只不同普通怪加一只精英、精英固定在 `2P`；Boss 节点为两只不同精英加 Boss、Boss 固定在 `2P`。所有单位都保留明确的定义 ID、等级与槽位，而不是依赖数组索引。
- `BeginBattle` 已用目录数值替换旧的单只硬编码金钱鼠／黑熊／虎王入口；稳定运行时 ID 采用 `Enemy.<DefinitionLeaf>.P<Slot>`。路线战斗等级遵从已保存的路线快照；精英为快照+1、Boss 为快照+2，均上限 20。
- 卡牌运行时复制敌人身份、`BattleSlotNumber`、`CombatLevel`；显示层优先使用显式槽位，只有迁移旧档无槽位时才回退 `StableSortOrder + 1`。重复、越界或缺身份／等级的显式敌方槽位被拒绝，因此不会形成重叠 HUD。
- 临时任务 NPC 的战斗属性改为优先读取保存的 `RouteCombatLevel`，不再因路线内后续 `PlayerLevel` 变化而漂移。
- 新鲜证据：冷 `-NoHotReload` UBT 成功、编辑器重启、PIE 启停成功；`GameXXK.Route.EncounterFormation`、`GameXXK.MVP.Battle.EncounterRules`、`GameXXK.Integration.CardBattleAdapter`、`GameXXK.Data.EnemyCatalog`、`GameXXK.Simulation.Foundation` 全部成功。

### 三章节路线 Task 4：保留原图结构的三章节运行时过渡

- 新开路线在既有七层随机地图生成完成后，保存权威的 `RootSeed`、三组章节地图种子、`CurrentChapter=1` 与 `RouteCombatLevel`。等级快照仅在入口写入一次，后续角色升级不会改变本局敌人数值。
- 第 1、2 章 Boss 胜利不再直接结束路线：只清除战斗局部状态，主角 HP／内力回满，保留路线卡、遗物、行旅货币、事件收益和临时任务 NPC，然后用已保存的下一章节种子再次调用原地图生成器。
- 第 3 章 Boss 才触发既有结算：完成任务、移除路线令、清除路线局部状态并返回世界地图；失败/终止路径也会清空活跃章节进度，避免产生“非路线中却带 active progress”的无效存档。
- 旧进行中存档迁移会一次性补全第 2、3 章规范化种子，但不调用地图生成器，因此当前地图节点、边、可达/已访问状态保持字节稳定。
- 新鲜证据：冷 `-NoHotReload` UBT 成功、编辑器重启、PIE 启停成功；`GameXXK.Route.ThreeChapter`（生命周期与存档架构）2/2 通过，`GameXXK.Equipment.SaveMigration.VersionSixDeterministicConversion` 与 `GameXXK.MVP.RouteMap.SeedRules` 回归均通过。

### 三章节路线 Task 5：局内经济与幂等路线结算

- 路线运行态保存独立的 `RouteTravelMoney`、实际获得路线卡数量，以及可保存的结算回执（含 GUID、来源快照、结果类型、永久金币与强化石数额）。永久金币不会在路线战斗、事件或商店时提前增加。
- 第 3 章通关按行旅货币 `10:1`、路线卡 `5:1` 结算；失败和玩家主动放弃分别保留 `Defeated`／`Abandoned` 结果并按 `20:1`、`10:1` 结算。预览、写入、重放和失败回滚均通过同一回执规则，已应用 ID 永不重复发放。
- 新增公开的 `AbandonDungeonToTown` 路径；第 3 章 Boss、失败和主动放弃都在清理路线局部状态前创建并原子应用同一结算回执。
- 修复宝箱的旧命令绕过：宝箱只允许经 `ResolveRouteEncounterChoice(0..2)` 领取三选一遗物。旧“领取金币”规则层调用被拒绝且不改变路线状态；HUD 命令层只公开真实的事件／遗物选项，面板遇到失效目录时仍从保存的候选遗物生成三项可点击选择与 Tooltip，不会退化为金币/补给假奖励。
- 新鲜证据：冷 `-NoHotReload` UBT 成功、编辑器正常重启、PIE 启停成功；`GameXXK.Route.Settlement` 3/3、`GameXXK.Route.Relics.EventAttributeAndChestChoice`、`GameXXK.MVP.RouteEncounter.NodeClick.EventAndChestCompleteFromVisibleChoices`、`GameXXK.MVP.PlayableShell.HUDCommandsDriveFullLoop` 与 `GameXXK.MVP.FullFlow` 均为 Success。

## 版本控制约束

- 所有 Task 1–5 文件仍未暂存、未提交。
- 工作树包含大量既有用户修改；不得使用 `git add -A`，不得回退或覆盖非本任务内容。
