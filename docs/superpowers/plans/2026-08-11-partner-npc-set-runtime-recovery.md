# GameXXK 伙伴、NPC 与套装新体系纠错实施计划

> 状态：执行中。该计划用于纠正 2026-08-11 将“主角 36 张通过”误报为“全卡完成”的验收错误。

**目标：** 在不改变已确认 UI 布局的前提下，真正替换六职业伙伴 108 张、六名任务 NPC 24 张和六套装 18 档旧效果，并完成确定性生成、存档迁移、运行时机制、文本和真实 PIE 验收。

**实施原则：** 每个批次先新增或改写新规格测试并亲眼确认 RED，再做最小生产实现；旧测试若锁定旧规则，必须改为新规则，不能把旧行为全绿当作完成证据。

## 1. 已核实的真实基线

| 范围 | 当前生产行为 | 新规格 | 结论 |
|---|---|---|---|
| 主角 36 张 | 新目录、八牌任务、重箭和共享机制已实现 | 保持，作为回归基线 | 已实现 |
| 永久伙伴 108 张 | 仍是旧牌名、费用和效果 | 六职业各 18 张目标牌 | 未实现 |
| 伙伴出生 | 4 核心 + 8 随机形成 12 张池；随等级/星级解锁 | 普通职业 2 核心 + 3 主流派 + 1 自由槽；出生仅 6 张且终身固定 | 未实现 |
| 阵师出生 | 旧普通职业池逻辑 | 六换场抽 2 + 十二收益抽 3 主流派和 1 全池 | 未实现 |
| NPC 24 张 | 仍是旧高费、旧目标和旧单卡效果 | 六人各 4 张新强牌，路线确定性 4 选 3 | 未实现 |
| NPC 法术任务 | 无来源隔离的三牌任务 | 月白、宋金宝各自携带三张全部主动一次 | 未实现 |
| 六套装 | 破军/山河基线存在；玄甲、青囊、追风、蚀骨仍是旧效果 | 新 2/4/6 件监听、唯一性和计数 | 部分实现 |

直接证据：

- `GameXXKCompanionRules.cpp` 的 `BuildPersonalCardPool` 仍要求 4 张核心、14 张候选并产出 12 张；`GetUnlockedPersonalCardCount` 仍根据等级和星级增加解锁数量。
- `GameXXKCardCatalog.cpp` 的旧《连珠箭》仍是 2 气、两段 65% 和标记，不是已确认的 1 气/3 内、流血 8、中毒 6、蓄力与重箭。
- NPC 月白、周光祖等仍注册旧费用、旧目标和旧效果。
- `GameXXKEquipmentSetCatalog.cpp` 仍把青囊写成治疗净化，把追风写成速度/开场抽牌，把蚀骨写成旧 DoT 百分比和回合末额外结算。

## 2. 权威规格与待冻结项目

已完整确认、可直接编码：

- 刀客 18 张与统一出生算法：`docs/superpowers/specs/2026-08-10-blade-card-archetypes-design.md`
- NPC 24 张与 4 选 3：`docs/superpowers/specs/2026-08-10-task-npc-card-pools-design.md`
- 全局状态、DoT、反击/格挡、重箭与任务边界：`docs/design/2026-08-11-gamexxk-project-plan/02-combat-rules-and-status-matrix.md`
- 六套装已确认主体：`docs/design/2026-08-11-gamexxk-project-plan/05-equipment-sets-and-economy.md`

实施前需要补成逐卡真源：

- 守卫 18 张：所有主要守护牌带格挡；普通转伤不耗甲；只有群体伤害可耗尽全部护甲，每点护甲增加 20 个百分点倍率。
- 药师 18 张：两张固定药方和其余每卡独立短药方；单体基础治疗不超过 12，群体不超过 6；药效双方使用都耗尽。
- 弓手 18 张：所有攻击均有本卡独立重箭条款；固定核心《连珠箭》《锐意感知》及毒矢、回环箭规则。
- 法师 18 张：炎/冰/雷各 4 张，通用任务牌；永久伙伴五牌任务。
- 阵师 18 张：六换场与十二张固定目标、全地势收益牌。
- 玄甲另外两档与追风 2/6 件精确触发次数，需要在进入对应实现批次前固定为唯一规则。

## 3. 批次 A：出生牌与 NPC 选择数据契约

### A1. 新测试先 RED

修改：

- `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCompanionFacadeTest.cpp`
- 新增 `Source/GameXXK/Private/Tests/GameXXKPartnerBirthPoolTest.cpp`
- 新增 `Source/GameXXK/Private/Tests/GameXXKQuestNpcCardSelectionTest.cpp`

锁定：

1. 普通职业恰为 2 核心 + 4 随机，最终 `PersonalCardIds == UnlockedPersonalCardIds == 6`。
2. 升级、升星不改变出生卡池或解锁数量。
3. 主流派 3 张 + 自由槽 1 张，无重复，相同种子完全一致。
4. 阵师恰为 2 张不同换场牌 + 4 张收益牌。
5. 六名 NPC 各四候选、确定性 4 选 3；未携带第四张不进入牌区或检索。
6. 旧 12 张伙伴存档迁移后保留可映射的出生六张和合法五张配置。

执行冷 UBT；RED 必须仅来自旧 12 张/等级解锁/固定 NPC 前三张契约，不能夹带无关编译错误。

### A2. 最小生产实现

修改：

- `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- `Source/GameXXK/Public/GameXXKCompanionRules.h`
- `Source/GameXXK/Public/GameXXKCompanionTypes.h`（仅确有新持久字段时）
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- 相关 facade/adapter，只替换数据语义，不移动 UI 控件。

完成后跑 Companion、SaveGame、CardBattleAdapter 和现有 UI 数据测试。

## 4. 批次 B：刀客 18 张

### B1. 目录与迁移 RED

新增独立刀客目录测试，逐卡锁定 CardId、名称、费用、目标、基础、冲锋和收招；锁定 4 个旧 CardId 到新藏锋 CardId 的迁移。

### B2. 运行时 RED/GREEN

按以下小组逐个完成：

1. 核心：《裂风斩》《回锋架势》。
2. 血刃：实时流血 10% 倍率、三段快照、保层和吸血窗口。
3. 断势：气势固定值与每层 10% 倍率并存、破绽追加段、返费与双计。
4. 游刃：多来源反击、仇恨标记、闪避仍反击、群体反击波次。
5. 藏锋：藏式、开锋、余式、同门/异流检索，阻止临时牌无限复制。

主要生产文件：

- `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- `Source/GameXXK/Public/GameXXKCardTypes.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`

## 5. 批次 C：守卫、药师、弓手、法师、阵师伙伴

每个职业先提交一份 18 张逐卡机器可测规格，再按“目录测试 → 单卡运行 → 四流派缺一组合 → 混编回归”顺序实施。

### C1. 守卫

- 格挡必须为独立来源：`100% 攻击 + 当前护甲`，不耗甲。
- 不耗甲的护甲转伤与全耗甲群攻严格分开。
- 禁止部分耗甲和把守卫格挡写作反击。

### C2. 药师

- 药方首打基础费用 +1，激活后恢复基础费用。
- 每张药方独立监听，终端血量变化不递归。
- 药效对友/敌统一快照并耗尽；每次获得至少 6 层给气势 1。
- 群体施加中毒每目标最多 1 层；毒爆不引爆蚀伤。

### C3. 弓手

- 所有攻击基础效果可独立结算，随后耗尽蓄力。
- 每张攻击按自身重箭条款追加攻击、抽牌、毒爆或资源。
- 《连珠箭》《锐意感知》必须使用已确认数值。

### C4. 法师

- 气力 0 为主、最多 1；1 气牌以检索本来源牌为主。
- 永久伙伴携带五张各主动一次后按顺序重放基础，只执行启动牌奖励。
- 炎、冰、雷奖励分别走灼烧顺序、溢出内力转甲/耗甲群攻、锁定标记落雷。

### C5. 阵师

- 六张换场牌各自带对应基础收益。
- 十二张收益牌在六地势下目标模式固定；错误地势也有基础效果。
- 六地势矩阵必须覆盖所有 18 张，不能出现预览合法而结算回滚。

## 6. 批次 D：NPC 24 张

逐人实施土司首领、金贵、琼么儿、周光祖、月白、宋金宝。测试覆盖：

- 协战稳定选人并走共享直接伤害管线。
- NPC 重箭先给协战者蓄力，再锁定并耗尽。
- 守卫型格挡与刀客型反击分开。
- 月白、宋金宝三牌任务按各自携带三张来源隔离。
- 周光祖/月白地势收益使用同一共享表。
- 任意 4 选 3 组合均可完成自己的循环。

## 7. 批次 E：六套装

### E1. 描述符 RED

改写 `GameXXKEquipmentSetCatalogTest.cpp`，禁止旧青囊、旧追风、旧蚀骨枚举和文本继续通过。

### E2. 战斗监听 RED/GREEN

- 破军：5% 直接伤害、首次多段破甲 1、首次攻击破甲目标 120% 追击。
- 玄甲：常规护甲衰减点只保留 50%；另外两档按最终冻结表。
- 青囊（全队唯一）：每回合首次支付至少 2 气时依阈值抽 1、全队非致死失 1 后回 2、回复 1 气。
- 追风（全队唯一）：按主动出牌数触发抽牌、回气、全队蓄力；自动行为不计数。
- 蚀骨（穿戴者独立）：每卡每目标首次三 DoT 施加蚀伤 1；首次满足两种三 DoT 自动毒爆；首次毒爆保层。
- 山河：地势增强、首张联动减费与相邻队友、六件团队阵眼。

Speed 不再提供玩家追风收益；旧存档字段只保留兼容。

## 8. 批次 F：总验收

1. 冷 UBT，禁止 Live Coding/Hot Reload。
2. 精确 Automation：目录、出生、迁移、六职业运行、NPC、套装、文本、卡牌目标和 UI 数据。
3. 组合矩阵：普通职业所有主流派缺一组合；阵师所有换场对和六地势；NPC 四种缺牌组合。
4. 三次相同 2,400 场确定性观测，要求 CSV 哈希一致、`stranded=0`，并单列各职业/套装极端值。
5. 真实 PIE：主菜单 → 城镇 → 伙伴配置 → 装备 → 路线 → 战斗；现场核对出生六张、NPC 三张和套装触发。
6. 对比 UI 结构和截图，只允许文案/状态数据改变，不改变已确认 slot、anchor、size、页签和页面布局。
7. 重导当前 198 张卡文档；只有真正通过的项目才能从“待落地”改为“已实现”。

## 9. 提交边界

按以下独立提交，不混入美术、生成图、打包目录或用户未跟踪脚本：

1. `test: lock fixed companion and npc card selection`
2. `feat: migrate permanent companion birth decks`
3. `feat: replace blade partner card runtime`
4. 每个其余伙伴职业一个实现提交
5. `feat: replace task npc card runtime`
6. `feat: replace equipment set battle effects`
7. `test: certify partner npc and set runtime`
8. `docs: refresh implemented full card catalog`
