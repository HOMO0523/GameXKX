---
status: active
owner: codex
updated_at: 2026-09-03T20:43:29+08:00
source_commit: 2e0db810df578590758f842ced2a36c72588a58e
working_tree: Fixed Mana committed; approved remaining card runtime work continues; pill and tooltip text require user table review before edits
---

> **2026-09-03 攻击显示口径纠正**：用户明确卡面攻击也必须显示数字，不能仅显示百分比；取实际攻击来源对象的属性，不默认主角或牌持有者。草案已修正为V2，区分来源计算的攻击伤害与对实际目标的护甲吸收/预计损血，倍率保留在详述解释。具体逐卡文本仍须先审阅后修改，UI代码未改。


> **2026-09-03 用户新增 tooltip 文案审阅要求**：已确认的卡牌实装继续；后续pill与短/长tooltip必须先列逐卡文本表，交用户看过后再改。用户随后纠正：攻击也要按实际取值对象显示数字；详述再列倍率、计算系数与实际补正。本轮先形成待审显示规则与效果片段样表，文件为 docs/design/2026-09-03-card-tooltip-text-review-draft.md；不是173张完整文案，尚未修改UI代码，详细显示格式也尚未冻结。Plan 2 Task 8已增加逐批文案审阅门槛。


> **2026-09-03 固定内力与全部装备排除完成**：提交2e0db81，主角基础上限30、法师伙伴34不随等级/升星增长；所有角色均不从装备获得内力，含基础、等级、强化、旧固定20与纳息百分比。纳息停止生成及洗炼，旧记录可读并显示已停用，换装预览同步为零内力贡献。玄冰+4/+8、山泉+6、月白玉璧节点+1等明确效果保留，战斗额外容量及当前值保存读取不丢失。另修正旧装备空宝石孔先补齐后校验，并补齐旧回归的天赋、容量、队伍前提及非消耗DOT期望。冷UBT与装备/主角牌/伙伴牌/存档/模拟316/316通过，0失败、0警告，见 docs/production/2026-09-03-fixed-mana-equipment-acceptance.md。只完成Task 6资源前置，伙伴冰牌、气力和阵师效果继续；雷走候选倍率未确认。以下为历史记录。

> **2026-09-03 药师／弓手阶段完成**：Task 5提交 `c8ce175`。药师稀有治疗系数保留标注品质，百级系数25零药效125、药效5时155；群体只消耗出牌者药效一次。清心清除四类DOT并按种类计入每回合3次预算，止血与温养用药师防御产甲；方剂记录开启品质，读取原动作快照，互不触发。弓手重箭及无视防御按批准品质/等级结算，锐意与回环取消基础返气，猎网/猎魂标用品质耗内表，隐踪史诗蓄力2，掠影每2蓄力给1灵动且最多2。冷UBT与伙伴总回归、主角药师/弓手、共享计算、卡表、存档147/147通过、0失败/0警告；详见 `docs/production/2026-09-03-healer-hunter-rebalance-acceptance.md`。下一步Task 6落实固定主角30/法师34内力与已确认的冰牌、法师气力、阵师效果；雷走候选倍率仍未确认，完整数值文案和后续计划尚未完成。以下为历史记录。

> **2026-09-03 两项数值口径确认、Task 5验证中**：用户确认法师内力采用一级基础值，只加明确固定补正（主角30、法师伙伴34）；默认补正为0，玄冰本场上限每次＋4、自身阵赏再＋8；山泉养气＋6及月白玉璧每路线节点＋1现只加主角，本路线结束清除；六合回8/16、寒序回6只回复当前内力。旧装备墨砚＋20、腰带/饰品曲线、强化与纳息百分比不是默认保留项，内力运行时尚未修改。用户另确认稀有治疗系数25已经包含品质，百级零药效治疗125；规格增加原始标注品质与一次最终取整规则。Task 5新增测试在冷UBT成功后复现6组旧行为差异，另复现方剂品质/阈值差异；目前药师/弓手实现已写入工作区，正在冷编译与新旧回归，未宣称阶段完成。方剂禁止相互触发、合法模拟队伍为主角＋一名常驻伙伴＋一名固定NPC，相关计划矛盾已同步修正；雷走候选倍率仍未确认。

> **2026-09-03 伙伴刀客／守卫阶段完成**：Task 4提交 `cda0190`，36张保留原身份和格挡/援护/起收招结构，血刃按已结算流血每点＋2攻击百分点；斩尽基础300%、横云100%、敛息稀有回4/史诗回6、破军额外攻击稀有60%/史诗70%；饮血收招预算改为系数20经品质和等级结算。守卫14项主护甲按打印费用与防御生成，5项副护甲使用独立40%/50%防御系数，镇岳/壁垒为180%/220%基础＋每点消耗护甲1个百分点，一夫全队各一份完整护甲。新增规则测试先RED后GREEN，旧测试按非消耗DOT水库和真实防御重算并保留断言；冷UBT与刀客/守卫/新规则/装备联动57/57通过、0失败/0警告，证据 `Saved/Automation/InRun02_Task04_BladeGuardContracts_GREEN/index.json`。主角前阶段123/123和模拟2/2记录保留；全卡数值文案仍由Task 8补齐。继续Task 5药师／弓手，尚未完成全计划或真实PIE验收。

> **2026-09-03 主角阶段提交完成，继续伙伴阶段**：Plan 2 Task 3提交 `7a9b869`，包含主角36张重平衡、四牌任务、主角法师1/1/1/0气力、寒序防御甲与按等级/品质换算实际溢出、连营下一次真实地势收益2/3/4、归序一次减费，以及旧八牌任务/旧连营/旧归序状态的无副作用迁移。冷UBT与主角＋缩放＋迁移123/123通过，模拟基础/装备矩阵2/2通过，均0失败/0警告；仅提交23个源码与测试文件。验收与证据为 `docs/production/2026-09-03-hero-rebalance-acceptance.md`。扩展的旧QualityResolution仍含青锋140%和固定治疗15旧期望及运行时数值文案缺口，已明确挂到Task 8，未伪称宽回归通过。现在进入Task 4伙伴刀客/守卫；雷走具体倍率仍等待已发出的用户确认，伙伴新气力将在后续相应阶段落地。以下为历史记录。

> **2026-09-03 用户“继续”恢复已确认部分实现**：按原顺序继续Plan 2 Task 3主角36张与遗留语义修复，保留现有未提交工作区；不把此前候选雷走倍率或引雷易伤提案自动冻结。已向用户异步确认雷走具体倍率，其余已批准工作继续。新测试修正费用和等级换算预期、纠正自动目标测试传参后，冷UBT成功，`InRun02_Task03_ResumeConfirmed_RED_v2` 为120项中112通过、8失败，准确覆盖寒序、连营、回春蚀伤清除、归序一次减费及对应卡表。寒序已改为1气、独立40%防御护甲、固定回6内，实际溢出按品质与队伍等级生成护甲；随后冷UBT及 `InRun02_Task03_HeroIce_GREEN` 聚焦验证通过，报告 `Saved/HarnessReports/20260903-165832-ai-production-loop.md`。其余主角缺口继续处理，未宣称Task 3或全项目完成。下方暂停描述记录此前设计讨论阶段，当前执行状态以本条为准。

> **2026-09-03 法师气力费用确认**：用户确认伙伴仅周天、照见0气，其余16张1气；主角炎／寒／雷／归为1／1／1／0气。内力费用和独立效果不变，寒序仍40%防御护甲。新增 `docs/superpowers/specs/2026-09-03-sorcerer-energy-cost-design.md`，同步总规格、速查、雷走设计、冰法账本前提、Plan 2/7和总入口，旧稿加历史费用提示。五张至少先付3气：3气周转、4气专精、5气爆发，阵赏返气在完成后到账。原生四雷＋周天、照见六合冰需4气，斗转四雷／六合冰需5气，主角四牌需3气。原单回合伤害表必须满足新预算，跨回合另算状态。费用已冻结，不代表雷走120/180/220/240或引雷易伤提案获批；运行时代码继续按整体讨论要求暂停。下方旧气力值均为历史。

> **2026-09-03 雷走单次重击方向确认**：用户选择“单次重击、保留标记”。保留普通、0气/4内；每个敌人一次直接攻击，按普通命中消耗最多1层标记，余下标记可接连霆。新增 `docs/superpowers/specs/2026-09-03-lightning-single-hit-design.md`，同步总规格、Plan 2/7、计划总入口与法师速查表。120%无标记／180%有标记／第3～4位220%及先加3标记再一段240%的阵赏仍是数值候选；引雷“易伤2替换回气抽牌”未获本轮确认，不纳入计算。`Saved/HarnessReports/2026-09-03-lightning-single-hit-projection.py/.json/.md` 保存44组新旧同序、45组标记/序位/品质边界与斗转额外重放对照。百级495攻、老虎146防御固定条件下，候选定标完整任务3520、雷走3610、斗转按末位牌与顺序3814～4417；全史诗转阶段另算。已区分排序收益与形态增强，这些不是UE验证或胜率结论。运行时代码继续暂停，寒冰10%确认不变；下方旧雷走逐层落雷与固定3次阵赏数据保留为历史对照。

> **2026-09-03 法师卡牌速查已整理**：用户要求回顾卡牌效果，新增 `docs/design/2026-09-03-sorcerer-card-quick-reference.md`，覆盖伙伴18张、主角4张及通用16种阵赏分支。按原生品质展示攻击/护甲倍率，灼烧统一标注百级示例，区分出牌、编序、首牌阵赏及免费重放；保留最新四冰10%、六合回内8/16与消耗护甲25%规则。这是当前讨论设计的查阅页，不新增平衡改动，运行时仍暂停；火雷推演和实现差异继续以下一条报告为准。

> **2026-09-03 炎法、雷法与通用阵赏计算完成（待设计审阅）**：已按现行设计计算伙伴30种阵赏入口，火/雷各8种固定起手路线，并对最低合法品质/全史诗、有/无阶段清空做64组对照；另补主角火/雷各三种品质。报告为 `Saved/HarnessReports/2026-09-03-element-reward-projection.md`，逐牌伤害、灼烧、标记和内力账本为 `Saved/HarnessReports/2026-09-03-element-reward-steps.md`，同名Python/JSON保存可复算数据。百级495攻、无额外状态的老虎靶子示例：斗转四火主动1178＋重放1602＋阵赏665＝3445，留84灼烧；斗转四雷1021＋1248＋702＝2971，调整组合与连霆序位的对照为3307。火的状态/资源奖励与雷的标记消耗须单独看，不能把余下灼烧直接计入即时伤害。发现旧实现仍有奖励品质强制Common、连霆/雷走按全标记而非原确认固定5/3次、周天读取上一张而非此前总支付、斗转火重放前后差量在转阶段时丢来源等差异；本轮只记录，未改角色/怪物数值或运行代码，也未运行UE验证。数值与强弱结论待用户审阅，寒冰10%确认记录保持。以下为历史状态。

> **2026-09-03 寒冰10%方案确认**：用户认可10%版本的数值方向，四张冰牌统一回复当前内力10%并向上取整，正式取代此前25%/20%草案。内力溢出仍按品质和队伍等级倍率转甲；标准冰爆为100%×品质＋每点消耗护甲1个百分点；霜镜与六合寒冰阵赏保留按消耗护甲25%给存活友方、向下取整；六合固定回8/16并共享本次溢出产甲。规格、Plan 2/7和总入口已统一验收数字。百级495攻、34/34内力、零甲、最低合法品质控制：六合起手潜在冰爆2782、法师最终193甲/另两人各265；照见＋六合3161、最终228/168/168；斗转＋六合5915、最终0/168/168；照见原四冰1694、自身114甲。逐步账本为 `docs/design/2026-09-03-ice-mana-armor-confirmed-step-tables.md`。这是数值设计确认，不冒充实机或胜率验收；运行时代码仍保留未提交工作区并按此前要求暂停，剩余全局语义审查继续。以下旧版本仅作历史记录。

> **2026-09-03 六合护法修订（五）**：用户将六合基础改为只给法师当前内力+8，再把本次实际溢出转换成同一个护甲数值，完整发给每名存活友方；前一张任务记录牌不含直接伤害时，以+16替代+8。维持0气/4内，主动先支付费用，重放不支付；内力上限与队友内力不变，也不得再叠加一次法师独享溢出护甲。六合寒冰阵赏改为标准冰爆后每名友方获得本次消耗护甲的25%，向下取整，不再加40%法师防御。已同步规格、Plan 2/7和总入口。当前正式四冰25%口径下，六合起手为1314护甲/潜在冰爆4519/阵赏每人新增328，法师结束328、另两友方各400；20%仅作待确认对照，对应1098/3824/274、另两友方各346。斗转→寒息→六合→冰鉴→霜镜会触发16档，潜在冰爆25%时9236、20%时8001。记录见 `Saved/HarnessReports/2026-09-03-liuhe-mana-armor-revision-5.md`。代码继续暂停，本轮只有讨论文档与推演更新；以下旧版本数字保留为历史。

> **2026-09-03 标准冰爆与冰鉴修订（四）**：用户明确标准冰爆改为 `100%×品质＋每点消耗护甲1个百分点`，冰鉴改为回复当前内力25%并向上取整。冰鉴其余检索与补偿语义、霜镜冰爆后按消耗护甲25%给每名存活友方的阵赏保留。规格与Plan 2/7、总入口已同步。相同百级495攻击、34/34内力、零甲、最低合法品质控制下：照见四冰消耗1240护甲、潜在冰爆4217、自身返甲310；斗转四冰消耗2336护甲、潜在冰爆7872；霜镜起手消耗803护甲、潜在冰爆2875、每名友方200护甲。完整账本见 `Saved/HarnessReports/2026-09-03-ice-mana-armor-revision-4.md`。本轮只改讨论文档与分析报告，未改运行时代码、未做新UBT/PIE，继续等待整体设计讨论确认。下方修订（三）及更早报告均为历史数值。

> **2026-09-03 霜镜阵赏修订（三）**：用户将霜镜起手的冰爆后全队护甲，由法师防御40%改为本次冰爆消耗护甲的25%。沿用既有返甲向下取整：先快照并消耗护甲、结算标准冰爆，再向每名存活友方（包括法师）独立发放完整的 `floor(消耗护甲/4)`，不再按品质、等级、防御或伤害量缩放。只有霜镜为任务启动牌才发放此阵赏。四张基础效果沿用上一版；照见/斗转起手仍为潜在冰爆6701/12647。新增同牌组霜镜起手对照为1139消耗护甲、潜在冰爆4342、每名友方284护甲，记录见 `Saved/HarnessReports/2026-09-03-ice-mana-armor-revision-3.md`。规格、Plan 2/7及总入口已同步，未修改或编译运行时代码；仍等待整体设计讨论确认。下方修订（二）及更早记录保留为历史。

> **2026-09-03 寒冰四牌语义修订与设计讨论暂停点**：用户明确将寒息、玄冰、霜镜零甲分支、冰鉴的基础产甲改为回复当前内力 25%/25%/25%/50%，回复先向上取整，实际溢出再按 `ceil(溢出内力 × 品质 × (TeamMaxLevel/25+1))` 生成护甲。玄冰先加本场上限4再回复；霜镜有甲时仅翻倍；冰鉴无合法检索时复制本次生成的护甲（包括0），不再次回内。标准冰爆与四张阵赏身份保留。当前解释和验收例子已写入总规格4.3.1/6.3、Plan 2 Task 6/8及Plan 7；同一百级基准的照见四冰为1912护甲、潜在冰爆6701，斗转四冰为3680护甲、潜在冰爆12647，逐步账本见 `Saved/HarnessReports/2026-09-03-ice-mana-armor-revision-2.md`。这些是设计推演，尚非运行时验收。代码继续暂停，待用户整体讨论确认；本轮只更新文档，没有运行新UBT/PIE，也没有提交或改写已有运行时代码。此前旧任务迁移修复已在未提交工作区通过119项联合回归（`Saved/HarnessReports/20260903-100459-ai-production-loop.md`），随后新增语义检查仍有失败（`Saved/HarnessReports/20260903-102616-ai-production-loop.md`），不能视为Plan 2完成。以下旧恢复记录与验收结论保留为历史。

> **2026-09-03 中断恢复审查（Plan 2 Task 3 待修兼容缺口）**：原任务记录的终止原因是用量限制；工作仍保存在用户已授权的根目录分支 `codex/overall-in-run-optimization`。Plan 2 Task 1 数值策略 `a2d4c87`、Task 2 路线卡退役及 v34 迁移 `8a78a66` 已提交；36 张主角牌的 Task 3 尚未提交。本次对最后编辑重新冷 UBT 并复验 `GameXXK.Data.HeroCards`，116/116、0 failed/0 warning/0 error；新增旧存档回归后确认 v33/v34 八牌术士任务均无法通过新的四牌任务校验，迁移桶 2 passed / 1 failed。该缺口必须先修复、转绿，再提交 Task 3 并继续后续卡牌。证据、七阶段现状、Plan 7 双伙伴模拟与固定 NPC 规则的冲突及恢复顺序见 `docs/production/2026-09-03-in-run-optimization-recovery-audit.md`。本条不宣称全量回归、真实 PIE 或正式打包已通过，保护原有资源删除和未跟踪素材。

> **2026-09-03 整体局内优化（Plan 1 战斗缩放基础已完成，Plan 2 待开始）**：当前工作分支为 `codex/overall-in-run-optimization`，批准规格为 `docs/superpowers/specs/2026-09-03-card-monster-progression-rebalance-design.md`，依赖顺序与七份可执行计划入口为 `docs/superpowers/plans/2026-09-03-overall-in-run-optimization-plan-suite.md`（计划提交 `15fe408`）。Plan 1 从 `4754be7` 到 `7eb8002` 完成普通/稀有/史诗 `1.0/1.2/1.4`、连续等级换算、队伍最高等级和难度快照、无 99 上限护甲、四类 DOT 水库、药效治疗与累计余数、下一回合气力惩罚以及 v32→v33 迁移；用户资产删除与未跟踪内容均未进入提交。
>
> Plan 1 最终门禁：冷 UBT GREEN；`GameXXK.Data.CombatScaling` 1/1、`GameXXK.Data.CardBattleRuntime` 5/5、`GameXXK.SaveMigration` 1/1、`GameXXK.MVP.SaveGame` 18/18、装备迁移 9/9、对话 1/1、商店 1/1、叙事 2/2、序章 10/10、天赋 1/1，均 0 failed/0 error。序章套件保留一条既有警告：测试编队不足三人时教程路线图仍发到实时背包，但完整状态持久化被延后；本轮未把它伪装为新错误或静默删除。Plan 2 的 173 张有效玩家牌目录重平衡尚未开始，不能把本条当作全套七计划完成证据。
>
> 规范继续冻结纯 2D `/Game/GameXXK/Maps/L_DesktopTrainingHUD` 为主流程；3D 城镇可保留在 UE 工程中，但不得重新进入默认启动、常规 PIE、结算返回或教程主链。局内商店/事件/遗物/奖励视觉、路线图原创美术、结算 UX 精修、跨机器 DPI 白边、3D 城镇 cook 隔离与教程重做仍为独立待设计项，不随本轮计划暗中扩张。现有用户资产删除与未跟踪美术/探针保持保护，不得被后续任务顺带提交。

> **2026-08-31 青山序章马车预览（实现与静态视觉门禁完成，玩家手工链待验收）**：桌面工作台现有 `剧情` 按钮只发出一次独立语义请求，携带瞬态 `GameXXKIntro=CarriagePreview` 进入已有六 NPC 的 `/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo`；普通城镇入口不带该参数。剧情入口会先收起已展开的背包，但不改挂机条折叠、阵容、任务、剧情、引导、奖励或存档状态。地图内唯一受管 Rig 复用现有包含马、车夫和车厢的图集，固定复制主角入场相机，马车从左侧 400 UU 驶入、停车后主角出现在车前、停 2 秒，再沿同方向驶离到 +800 UU；Escape 暂停层提供继续/返回桌面，异常与取消共用 fail-open 清理。实现提交依次为 `7e1b0c1`、`e002364`、`1e88bca`、`c1fc9ae`、`9ed4fb5`、`a018c02`、`5596f4b`，最终朝向、相机、背包收起与玩家批准坐标校准为 `b796505`。
>
> 玩家批准的 PlayerStart/Rig 锚点为 `(16678.592, 5270.000, 1075.711)`、Yaw `0`，显示统一应用 `Z=-72` 落地偏移；只移动了唯一 PlayerStart 与受管 Rig。MCP 地图校验 `ok=true`：Rig/PlayerStart 各 1、驶入距离 `400`、驶离同向点积 `320000`、Exit `(0,800,0)`、HeroReveal `(-80,0,0)`、四张 1K/2K 图集源尺寸正确、`dirty_after=[]`。新锚点四阶段证据为 `Saved/Screenshots/WindowsEditor/prologue_carriage_{arriving,parked,departing,handoff}_anchor_v1.png`；Luna Max 报告 `Saved/HarnessReports/prologue-carriage-anchor-v1-luna.md` 对可见项判定 PASS：没有台阶/地面/草地/建筑穿模，没有悬空、图集边框、透明框或帧裁切，马车明确朝右，停车时主角在前景、驶离时马车在后层，交接背景锚点无明显跳镜。静态截图不能证明连续运动、完整出画或真实输入恢复，这些不被冒充为已验收。
>
> 2026-08-31 最终冷 UBT：`python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 160 --filter "[TDD]"`，`GameXXKEditor Win64 Development -NoHotReload` GREEN。编辑器内精确 Automation 7/7、0 failed、0 skipped、0 warning：`GameXXK.Prologue.Carriage.{Rig,Rules,Widget}`、`GameXXK.DesktopTraining.Workbench.{StoryQuestCarriageRequest,TownTogglePresentation}`、`GameXXK.MVP.LevelFlow`、`GameXXK.MVP.PlayableShell.GameModeDefaults`。`scripts.test_prologue_carriage_policy` 3/3、三个 Python 脚本 `py_compile`、`git diff --check` 均 exit 0；harness validator 为 `OK`，仅保留既有旧生产文档 metadata warnings。已知无关 `GameXXK.MVP.Town.ShellInputInteractionFollower` 仍受用户动画/相机语义漂移影响，本轮没有削弱其旧断言或伪称全量全绿。
>
> 尚未由玩家亲手完成并确认的链保持显式未验收：真实点击 `剧情` 后背包收起并播完全段、再次点击完整重播、Escape 暂停后继续同一阶段、暂停后返回桌面无输入锁、关闭/重启只进桌面且不自动播剧情。新手引导与后续剧情内容尚未开始；必须等本段玩家验收后再进入独立设计/实现周期。

> **2026-08-30 固定 NPC 编队与 NPC 路线事件退役（实现完成，手工月白链待玩家验收）**：`OrderedFormation` 现固定为“主角 + 一名永久伙伴 + 六名固定 NPC 之一”，`ActiveTemporaryQuestNpcId` 在当前运行时必须为空；挂机、路线/战斗、装备/卡组投影和经验归属读取同一 NPC。七个 NPC 路线事件与可达临时支援操作已删除，仅保留山泉和四类宝箱；旧枚举/action 序号作为隐藏墓碑保留。存档升至 v30，按“有效有序 NPC → 旧选择 → v29 临时字段 → 土司首领”恢复，并把待处理旧 NPC 事件原地改为山泉，不发奖励、不结算节点。实现提交依次为 `71eba5d`、`0b2dfe0`、`cca9bbb`、`1ba56d4`、`b1422cf`、`2231252`、`8ae6d64`。
>
> 最终冷 UBT：`GameXXKEditor Win64 Development -NoHotReload` GREEN（2026-08-30 21:01）。编辑器 Automation 精确门禁 14/14、0 failed：永久 NPC authority、挂机换人不重置、三人经验、NPC 事件目录/兼容 facade 退役、v30 迁移、CardRoute lifecycle、三条 settlement formation、Workbench 固定 NPC 肖像/六候选/地图会话、Town NPC 图鉴。可复查的分组报告包括 `Saved/Automation/PermanentNpcFormation-Task7-PartyFormation-Green-1/index.json`（6/6）、`PermanentNpcFormation-Task7-Companion-Affected-2`（11/11）、`PermanentNpcFormation-Task7-CardRoute-Broad-4`（19/19）、`PermanentNpcFormation-Task7-EnemyIntent-Affected-2`（39/39）、`PermanentNpcFormation-Task7-RouteMap-Broad`（6/6）、`PermanentNpcFormation-Task7-CardAdapter-Affected-2`（5/5）、`PermanentNpcFormation-Task7-RouteMerchant-Affected-1` 与 `PermanentNpcFormation-Task7-MVPRouteMerchant-Affected-5`（各 12/12）、`PermanentNpcFormation-Task5-SaveGame-Green-Final`（18/18）、`PermanentNpcFormation-Task5-EquipmentSave-Green-Final`（9/9）。静态 policy 2/2 通过；生产引用只剩字段声明、必须为空校验与 v30 迁移读取/清空，`TemporaryNpcSupport` 只剩隐藏枚举墓碑。
>
> 广泛回归没有伪装为全绿：一次编辑器内 `StartsWith:GameXXK` 完整跑完 916 项，794 通过、122 失败；其后修复了所有已识别的空编队夹具并逐组转绿。后续整套重跑在既有 `CompanionRoster.PersonalDeck` → `SplitStatusSegments` tooltip 路径触发无关 `EXCEPTION_ACCESS_VIOLATION`，所以没有最终完整计数。仍明确存在的非本任务基线包括战斗/挂机动画 atlas 与时序断言、Workbench `InnerGeometry`/图集库存、营地“护符 vs 30% 治疗”旧语义、Simulation 旧指标、CompanionRoster 旧布局/卡组交互与 tooltip 崩溃；不在本任务中削弱断言或改保护资产追求假绿。
>
> 只读 PIE（`/Game/GameXXK/Maps/L_DesktopTrainingHUD`）证据：探针 `observe` 返回有序成员 `Player / CompanionInstance.Companion_Blade_01.00002621 / Npc.TusiChief`，挂机 `travel_party_ids` 完全相同，`selected_quest_npc_id=Npc.TusiChief`，`active_temporary_quest_npc_id=None`，Travel 正在 Combat；随后正常停止 PIE。该验收未运行鼠标脚本、自动点击、选人、路线、保存或输入驱动，也未请求截图。月白的真实“编入队伍 → 城镇往返 → 路线结束 → 正常关闭/重启加载 → 再换 NPC”手工链尚未由玩家执行，因此本文不声称这些可见/重启检查已完成；其状态生命周期目前只有上述 C++ 门禁证据。未执行 package build，也未宣称视觉 PASS。

> **2026-08-27 中途返回恢复与 8 格局内商店（当前工作区）**：修复挑战/路线结算返回后仅保留 `TrainingTravelRuntime`、却未恢复 `bTravelActive` 与遭遇游标造成的挂机停止；窗口复用时重新挂载原生布局，1-1 中途退出后游历、遇敌与双宝箱时钟继续。挑战路线起点现为自动占据的营地标记，不可点击且无奖励；Boss 胜利跳过普通三选一，直接应用 `Cleared` 路线结算并恢复挂机。局内商店改为单页两排 8 格：上排 4 张携带卡强化、下排 4 个可购买遗物，统一消耗普通金币，刷新仅替换未购商品并保留已售格；卡片缩小、详情移入 Tooltip、按钮单行高对比。冷 UBT GREEN；Route Merchant 12/12、Route Merchant Widget 7/7、Workbench 67/67、PlayerFlow 4/4、RouteSettlement 2/2；Training 仍为 35/38，剩余 `DeployedTrioExperience`、`RewardResolver`、`TravelOfflineSubsystemBridge` 是本轮前已有问题。

> **2026-08-26 挂机摘要栏与波次刻度（当前工作区）**：桌面挂机条新增始终可见的单行战报、`72×24` 折叠/展开按钮、从右向左推进的波次进度与 `72×24` Tab。Tab 关闭时进度宽 420，Tab 打开时缩为 340，整行分别对齐 953/945 的现有挂机区域；折叠后隐藏角色场景并在 Tab 后显示普通/高级宝箱实时数量。两个宝箱按钮移出内部 ScaleBox，逻辑尺寸固定为 `72×72`；五个顶部按钮、金币和背包 X 已收进纸框。三枚 GPT 位图刻度已拆成 256² 真 Alpha PNG 并导入；实际遭遇顺序保持原有 `普通→普通→精英→普通→精英→普通→首领`，视觉从左到右镜像为 `首领→普通→精英→普通→精英→普通→普通`。冷 UBT GREEN，Workbench 66/66；Training 35/38，剩余 3 条为本轮之前已有的离线经验/奖励 blocker（`DeployedTrioExperience`、`RewardResolver`、`TravelOfflineSubsystemBridge`），没有隐藏或冒充全绿。

> **2026-08-26 透明桌面 HUD 与城镇往返（当前工作区）**：Win64 项目插件使用 D3D12 DirectComposition 逐像素呈现，完整原生窗口不做区域裁切；全客户区玻璃透明消除了黑色矩形，透明空处与纸面控件使用同步鼠标命中策略，携带道具改用独立窗口客户区坐标。展开工作台时显示最终批准的圆形“进入城镇”图，仓库打开时按钮移动到仓库外侧；3D 青山镇显示配套“退出城镇”图。两向真实点击已完成 `L_DesktopTrainingHUD → L_Qingshan_AsianVillage_Demo → L_DesktopTrainingHUD`，3D 内 HUD 固定、桌面 HUD 可拖动，背包展开状态经一次性 GameInstance 快照恢复。冷 UBT GREEN；城镇按钮资源测试 1/1、Workbench 66/66、LevelFlow 1/1 通过。正式打包与长时内存采样仍需另行执行，本文不把它们冒充已完成。

> **2026-08-21 挑战路线图与逐条结算修正（本轮实施）**：点击“挑战”不再直接开战，而是生成挑战专属路线图（1 个入口普通战 → 普通/精英分支 ×2 → 普通 → 首领，共 7 节点，复用现有爬塔路线图 UI）；玩家点击可达节点进入该节点的真实卡牌战斗，胜利后结算节点并返回路线图继续选路，首领胜利才通关回工作台；自动战斗开关只影响单场战斗内自动出牌，不再驱动下一场连打。战斗板新增“逐条结算日志”，每个伤害包在命中瞬间按队列顺序追加一行（“XX用【卡牌】对YY造成了N伤害”），随后同包伤害/血量覆盖立即同步，保留最近 6 条。桌面 2D 懒加载补上 `DungeonMap` → 共享路线图 Widget 的提升路径；Town 返回工作台改用“离开 Town 时确实关闭过”标志，消除 Collapsed 重建递归死循环。回归：Training 23/23、DesktopTraining 31/31、CardBattle 38/38；冷 UBT `-NoHotReload` GREEN。真实 PIE 证据：挑战点击 → `DUNGEON_MAP`、`route_map_active=true`、7 节点、可达 [0]；点节点 0 → `BATTLE`、`battle_active=true`；取消回工作台后游历 `Walking` 全自动推进，挑战关闭；截图与状态探针确认路线图可见、路线进度为 0/7，且无背包或战斗板遮挡。局外历练挂机全自动推进保持不变。
# GameXXK 当前目标(滚动指针)

> 本文件是"当前做到哪了"的**唯一滚动指针**。`AGENTS.md` 不再硬编码验收状态,改指向这里。每次目标收尾后更新本文件。

> **2026-08-21 纯 2D 默认流程（当前最高优先）**：编辑器/游戏默认地图和 `Town` 权威解析均为 `L_DesktopTrainingHUD`；未明确要求时禁止把日常开发、PIE 或截图切到 3D。挑战不接青山镇任务，直接在同地图切换现有全屏 BattleBoard，退出后恢复工作台。目标箭头已同时修复“浮动窗口桌面原点混入 Board-local”的整段偏移和“贴图中心代替可见箭尖”的局部热点错误；60/60 Automation 与三尺寸/三窗口原点真实 PIE 通过。权威记录：`docs/production/2026-08-21-desktop-2d-default-pointer-acceptance.md`。本文所有“默认入口继续 3D/禁止切 2D”的旧描述自此仅作历史记录，不再是执行约束。

> **2026-08-19 用户纠偏（最高优先）**：本文下方所有“工作台内合并 ChallengeViewport、内嵌路线图/BattleBoard、3 敌 3 我顶栏、挑战只读侧壳、`StartTrainingChallenge` 作为玩家挑战入口”的描述均已废止。当前玩家挑战流程必须复用现有传送门→路线图→全屏 BattleBoard；工作台挑战只负责委托现有路线入口并关闭工作台。权威规格为 `docs/superpowers/specs/2026-08-19-route-owned-auto-battle-correction-design.md`。旧段落仅作历史记录，不作为当前状态。

> **2026-08-20 桌面 HUD 纠错（当前最新）**：主角、伙伴、NPC 真实头像入口已固定在中栏左下，查看角色不再换队；队伍写入只允许在独立编队页点击“编入队伍”。用户否决的星点长条/通用错误 Tab 底永久停用，状态底只使用 `003_tab_1` / `004_tab_2`。挑战不依赖青山镇任务，未接任务可直接进入真实 Battle 且任务状态不变。该范围冷 UBT、Training 21/21、DesktopTraining 30/30、三分辨率与真实点击链均通过；详见 `docs/production/2026-08-20-desktop-training-hud-roster-flow-acceptance.md`。

## 当前基线(更新于 2026-08-19)

- 规格冻结基线:`ba90810a56e06a3b70ed0e3125c4ef67a59a0685`（2026-08-17 `docs: freeze desktop training workbench design`）；它是设计/规则基线，不冒充当前代码 HEAD。根目录 `main` 已从祖先 `628c46a` 安全快进并完成到 `0fd4c88` 的运行时/资产/harness checkpoint；原 `codex/desktop-training-2d-hud-migration` 分支保留在 `57a06e4`，未使用或创建 worktree。
- 当前运行时/证据源码基线:`0fd4c88`（活动分支 `main`；滚动指针由后续 docs-only commit 承载）。HUD-only 懒启动/折叠卸载、工作台两态置顶图钉、存档快照归一化、v21/v22 库存/NPC、32 张当前可达 1K atlas、8 张 ImageTruth 和 Phase 0 harness 已分三批 checkpoint；用户 `L_Main.umap` 及未跟踪源美术/探针保持保护。桌面历练规则、程序化工作台、确定性 TravelRunner、离线账本、奖励/冷却 Resolver、背包/伙伴/NPC 切换、仓库 4 列分页、拖拽/右键路由、工具 3×3 已保留；旧“挑战只读侧壳/960×968 ChallengeViewport”已按用户纠偏废止，当前挑战走现有路线图与全屏 BattleBoard。
- SaveVersion 边界必须分层记录：`ba90810` 规格冻结时的历史前置基线为 v17；当前工作区代码实际 `CurrentSaveVersion=22`，`DesktopTrainingWorkbenchIntroducedSaveVersion=18`、`TrainingRewardCooldownsIntroducedSaveVersion=19`、`TrainingOfflineCollectionIntroducedSaveVersion=20`、`DesktopInventoryStorageIntroducedSaveVersion=21`、`QuestNpcEquipmentOwnerIntroducedSaveVersion=22`。任何旧 v16/v17/v18 历练索引均为历史 `shelved`；后续迁移从 v23 继续。
- 最近一次目标验收:`docs/production/2026-08-21-desktop-2d-default-pointer-acceptance.md`（2D 默认入口、同地图直接挑战/返回、浮动窗口箭头吸附）
- 最近一次全量代码/文档审查与优化方案:`docs/production/2026-08-16-full-project-optimization-proposal.md`;上一轮定向建议见 `docs/production/2026-08-16-optimization-followup.md`
- 最新历史全量自动化:**598/598 通过、0 error**，证据为 `Saved/Automation/ChargeFinishSubject/index.json`（2026-08-16 12:01:35）；最近历史冷 UBT GREEN，证据为 `Saved/HarnessReports/20260816-114544-ai-production-loop.md`。这两份报告只作历史回归参考，不冒充当前工作区的本轮全量运行。
- 历史 2026-08-18 production-loop 证据保留作迁移对照（其中旧报告的“1-1 无箱”与旧 MasterV2 NavDisc 接入均已被当前规则/真源 supersede）；最新可采信证据见 2026-08-19 条目。mcp-live 与 asset-contract 的历史失败不被隐藏，仍按标签单独记录。
- 2026-08-19 17:10 最新聚焦证据：当前源码冷 UBT `GameXXKEditor Win64 Development -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2` GREEN；`GameXXK.MVP.SaveGame` 12/12、`GameXXK.DesktopTraining.Workbench` 19/19、`GameXXK.Training` 21/21，均 0 failed/0 error。Workbench 的 6 条 warning 已归类为 3 条 BattleVisual 生命周期诊断与 3 个尚未导入的 Guard/宋金宝/月白 1K idle atlas。Harness validator `findings=[]`；headless/all 报告 `Saved/HarnessReports/20260819-170948-ai-production-loop.md` 通过；mcp-live 报告 `Saved/HarnessReports/20260819-170230-ai-production-loop.md` 六个脚本全绿，HP HUD 真实流与 PartyDeck 198 目录合同均已关闭旧失败。asset-contract 当前为 58/69 通过、11 个显式 blocker，报告 `Saved/HarnessReports/20260819-170933-ai-production-loop.md`；分类见 `docs/production/2026-08-19-goal-progress-evidence.md`，未改保护资产追求假绿。
- 2026-08-18 11:18 的当前 HEAD 门禁重跑：默认 `--run-script-tests` 为 PASS（`Saved/HarnessReports/20260818-111819-ai-production-loop.md`），`headless` 为 PASS（`20260818-111803-ai-production-loop.md`），`--script-tests all` 为 PASS 且未启动 UnrealEditor（`20260818-111810-ai-production-loop.md`）。本轮修正把依赖 NumPy/源美术的 `test_walkloop_atlas_pipeline.py` 从隐式 headless 归入 `asset-contract`；asset-contract 与 mcp-live 仍需单独处理，不能因此宣称 Phase 0 完成。
- 2026-08-18 11:26 的 system-Python asset-contract（Pillow 11.3、NumPy 2.5）为旧报告；当前复测 `Saved/HarnessReports/20260819-170933-ai-production-loop.md` 为 58/69 通过、11 个失败。失败集中在外部 PSD/地形源缺失、PartyDeck/PSD 合同漂移、`L_QingshanInn.umap` 保护 hash 与用户当前文件不一致、视觉合同漂移及未实现的 golden-asset 合同；它们保持显式 blocker，不改保护资产来追求假绿。
- 当前工作区保护：`Content/GameXXK/Maps/L_Main.umap` 保留用户已有修改；`SourceAssets/`、`SourceArt/` 及未跟踪探针不在本轮 Phase 0 写入范围。
- 2026-08-19 工作区增量：桌面库存现已拥有 v21 持久化的背包/仓库物理格与装备容器分区；程序化工作台接入左键吸附、合法空格原子提交、携带中右键回滚，以及未携带右键的 `仓库 > 工具 > 快速装备` 优先级。工具右栏为五模式、固定 3×3 非破坏性输入与确定按钮；未接配方不消耗。携带事务会在 Tab/折叠、切页、排序、主体切换、关闭/替换侧栏、挑战/游历、退出确认、应用失活、存读档/新游戏、外部 Slate 重建和销毁时统一回滚。冷 UBT `-NoHotReload -NoUBA -MaxParallelActions=2` 为 GREEN；Automation 为 `GameXXK.DesktopInventory` 1/1、`GameXXK.DesktopTraining.Workbench.ItemCarry*` 2/2、完整 Workbench 19/19、`GameXXK.MVP.SaveGame` 12/12、Equipment SaveMigration 6/6、MetaShop SaveMigration 1/1，均 0 failed。真实 PIE/MCP 与本轮视觉证据仍待补。
- 2026-08-19 10:47 增量：Normal 1-1 已取消“一滴血/禁箱”旧例外；游历运行时改为真实 `主角 + 一名伙伴 + 一名 NPC` 三人轮转攻击、敌方每轮反击、全队阵亡才失败，默认队伍为主角/刀客/土司首领。新档获得六职业伙伴各一位与六名可配置 NPC；角色页增加主角/伙伴/NPC 三入口，头像点击直接替换对应队伍槽。NPC 现有独立属性快照、六装备槽、v22 `QuestNpc` 装备 owner 与 4 选 3 可编辑卡组。聚焦回归：Training 21/21、Workbench 19/19、FinalInventory 3/3、Equipment SaveMigration 6/6、StarterCompanion 1/1、Companion Facade 4/4。第一章当前可达的主角/刀客/土司与五类敌人共 32 张战斗 atlas 已真实重导为 1024×1024 BC7（每张 UE resource 1 MiB），验证 32/32。
- 2026-08-19 纯 2D 挂机编辑器内存复测（`UnrealEditor -game /Game/GameXXK/Maps/L_DesktopTrainingHUD`，1672×941，HUD-only，无 3D Pawn）：20 秒 `Working Set 3248.6 MiB / Private 4652.8 MiB`，50 秒 `Working Set 3387.1 MiB / Private 4759.6 MiB`，报告 `Saved/HarnessReports/desktop-training-hud-memory-20260819-113244.json`。相比懒启动前的 3447.3/5008.8 MiB 有所下降，但仍远超 TaskBarHero 与目标包络；编辑器固定开销/插件资产仍是主因，不能据此启用默认入口，仍需空壳/静置/局内/3D 四组同机数据。

## 已落地(最近六轮)

- **敌方意图随机化与守卫嘲讽闭环**(`9598072`、`e78be7c`):敌方单体攻击意图默认 `MarkedPartyElseRandom`(只有带标记的队友吸引集火,否则稳定种子随机目标);标记施加类意图随机落点;种子加 Murmur finalizer 防止两名队友机械交替。守卫 16 张加甲牌结算末尾给守卫本人上 1 层标记,与标记优先规则形成坦克闭环;标记既有代价规则(受击增伤/命中消耗)随之生效。聚焦敌方 61/61、守卫 23/23、全量 598/598 通过。
- **战斗 tooltip / 卡牌文本 / HP HUD 一致性热修簇**(`a490235..b6763a0`,13 个提交):补全刀锋特质/药丸/分类型伤害行的卡牌悬浮文本;彩色双字特质药丸与内容自适应纸张;tooltip 行确定性换行、药丸排序与跟随定位;猎手重箭/掠影箭文本;修复未封顶 sentinel 泄漏、自愈后 HUD 生命值冻结、蓄力/终结句缺失卡牌主语。全量自动化 598/598、冷 UBT GREEN。
- **战斗目标箭头视觉回归修复**(`a490235`):移除箭头头部的方向相关平移，恢复贴图中心严格落在实时鼠标端点；保留自由鼠标跟随与 stage-space 目标命中；新增水平/垂直/对角回归测试。真实 PIE 的 Single/Healing 双方向对比通过，用户在保持运行的目标卡验收现场确认 `ok`。
- **奖励体系重构**(`00002f1`):彻底删除路线临时卡体系(路线卡条目/12 容量/18 张配方/事件宝箱商人发卡);新增玩家卡组**首领卡槽 3 个**,击杀首领后选择首领牌(虎/熊)写入空槽并加入当前手牌;战后奖励只剩首领牌(例外)+携带牌品质升级+遗物+气力点+抽牌数;存档 v16→v17 迁移。
- **两项平衡修复**:①模拟 AI 无进展死循环(连续 64 次决策无敌方掉血强制结束回合;5 个回合边界血量无变化判僵持战败);②刀锋延迟保留牌主人阵亡时实例丢失(牌随主人退场)。
- 新增回归测试 `ZhuiFengJinGuiBoss942090`、`BladeRetainedOwnerDefeat1100213`;修复两处进程 RNG 偶发测试;卡牌文档重新生成。
- 全量自动化 **596/596 通过**(奖励重构轮,历史记录);2400 锁定案例 0 卡死/0 错误(胜 2298 / 负 102)。
- 198 张卡(36 主角 + 108 永久伙伴 + 24 任务 NPC + 30 路线牌)完整执行,全 198 × 七地势 + 条件缺失分支通过。

## 关键语义冻结(注意差异)

- **任务接受与跟随者激活语义已切换**。`AcceptTownQuest` 接取青山镇主线**不再**自动设置 `bFollowerJoined`;引导 NPC 留在原地(不跟随、不自动入队)。玩家在 NPC 对话框点"入队"(`RecruitPendingTownNpc`)后,引导 NPC 才成为叙事跟随者(`bFollowerJoined=true`,并清空 `bHasQuestNpcLocation`/`QuestNpcLocation` 随主角离镇)。旧档迁移(`GameXXKSaveMigration`)仍会把 v14 及更早"已接受但未跟随"的存档规范化为跟随已加入,保持兼容。
- 地形增益仍为**旧表**;新模型(每回合全员 1 次 + 对应职业再 1 次 + 山河套再 1 次,敌方统一 1 易伤+1 燃烧)已裁决,待 §5 山河三档与 `TriggerTerrainBenefit` 口径复核后实施,见 `docs/design/2026-08-13-terrain-benefit-redesign.md`。

## 桌面历练工作台当前进度(2026-08-18，部分内容已被 2026-08-19 纠偏废止)

> 本节“挑战态 3+3 顶栏/ChallengeViewport/只读侧壳/`StartTrainingChallenge` 玩家路径”均为旧设计，已被用户否决；仅保留工作台作为 2D 挂机/游历壳。挑战正确路径见上方纠偏。

- **规则/存档已推进但未完成玩法闭环**：`GameXXKTrainingRules.*` 覆盖三难度、27 个稳定关卡 ID、挑战/游历分离、普通 1-1 默认通关、失败策略、章节编制、seeded challenge/travel reward resolver 和普通/高级 Travel 240/360 秒冷却；当前代码与测试已让普通 1-1 使用与挑战相同的敌方生命、三人编制和概率箱规则，不再保留旧的一滴血/禁箱特例；v20 提供 UTC 基线，v21/v22 分别承载物理格和 NPC 配置。
- **程序化工作台、真实挑战桥接和 TravelRunner 已提交**：`GameXXKDesktopTrainingWorkbenchWidget.*` 提供 1920×1080 几何合同、仓库 4 列/20 格分页、背包比例约 1.76:1、右侧 27 节点/三难度页签、挑战/游历按钮、顶部 3 敌+3 我挂机条；挂机条读取 TravelRunner 的遭遇、阶段和 HP，并显示普通箱/精英箱剩余 CD（规则分别为 240/360 秒）。挑战态重建 Slate 树时会保留原可见性，避免 Battle 状态只剩战斗层；真实 PIE 已确认左仓库、中央 BattleBoard、右只读历练地图同窗保留。背包内部现在有主角/两名永久伙伴切换、当前角色对可见仓库格 quick-equip、确定性排序、装备槽卸下回仓、独立设置面板和独立关闭动作，工具替换右栏，天赋替换中栏，挑战期间导航只读。本轮工作区增量已将批准的 MasterV2 `PanelLarge`、`ItemSlot`、`EquipmentSlot`、页签、路线节点，以及五张圆形 `NavDisc` 以缓存纹理和九宫格/等比槽位接入；资源合同与 DesktopTraining 回归已通过，但仍不是最终 PSD/manifest 交付。`StartTrainingChallenge` 已创建真实 `FGameXXKCardBattleAdapter` 会话并支持单步推进；自动模式会对强制弃牌、洞察、任务检索和自动解析队列做有界确定性选择；`StartTrainingTravel`/`AdvanceTrainingTravelStep` 已支持走动、单敌人自动攻击、掉血、击杀、Boss 结算、重试/回退和 1-1 一血规则。本分支进一步把 `Town` 状态接到 HUD-only 地图并自动打开工作台；`main` 仍保留 3D 城镇入口。
- **章节敌人语义已按最终口径冻结到规则与测试**：普通候选为公鸡/狸猫，次级精英为山羊/黄鼬，每场 4 个普通槽、2 个精英槽和 1 个首领；1-1 山羊、1-2 黄鼬、1-3 青角羊王。最新裁决要求挑战与游历读取同一生命/编制数据，取消游历 1 HP 例外；代码与真实表现仍待下一工作包完成并复核。
- **当前仍未完成**：完整共享三敌/三我局内表现与最终路线卡 UX、真实天赋 read model/最终概率、FIFO 箱批/容量/箱内物品、工具真实规则、最终 PSD 页面交付、其余顶部/节点/挑战/游历/重试图标真源、1920/2560 整体工作台截图、空壳/静置/局内/3D 四组性能数据和默认入口验收。仓库/背包容器、物理格、拖拽/右键路由与工具 3×3 交互基础已经落地，不再列为“未实现”。

## 仅规划未实施 / 已搁置

- **旧历练桌面迁移 7 包索引**(历练放置、离线收益、双宝箱、旧 2D 主界面、桌面迷你窗、自动战斗、任务 NPC 显式入队 + 默认入口迁移):**已标记 shelved，不得按旧计划直接执行**。新桌面工作台设计见 `docs/superpowers/specs/2026-08-17-gamexxk-desktop-training-workbench-design.md`；当前已进入 opt-in 规则/壳实现，但 PSD、真实战斗、挂机表现和性能/PIE 验收仍未完成。旧计划中的 v16/v17/v18 边界失效，现行工作区已推进到 v22；后续迁移从 v23 继续。
- 项目自身优化:`docs/production/2026-08-16-full-project-optimization-proposal.md` Phase 0 正在执行；Phase 1/2/3/4 仍未实施，旧 `optimization-plan.md` 仅作历史索引。

## 下一步待办

### 后续目标与计划（2026-08-21）

- 2026-08-21 纯 2D 默认入口与 BlockShield 验收之后的完整后续路线图已合并为唯一权威计划 `docs/production/2026-08-21-next-goals-and-plan.md`(目标 G0–G8 / 分层 P0–P5、工作包 WP0–WP8、WP-A Task 6 收尾、WP-B 启动器、各包验收口径、执行顺序、验证命令、待用户拍板清单与保护边界);原并发草案 `docs/production/2026-08-21-next-goals-roadmap.md` 已标 `superseded` 保留为历史记录。此后新增目标优先按合并版执行;本文继续作为"当前做到哪了"的滚动指针。

### 本轮视觉工作流纠偏（2026-08-18）

- 用户否决了“把所有 UI 重新生成为一张概念图”的路径：它不符合实机像素比例、窄条挂机布局、现有 MasterV2/Town PSD，也会把文字和图标重绘成噪点。
- 新冻结规则：实机截图与 `Content/GameXXK/UI` 批准资源是唯一视觉基准；GPT 生图只允许生成无文字、无 UI、无角色/怪物的背景板。框体、按钮、图标、文字、角色和敌人必须复用/拆分现有资源，并保持等比与 nearest-neighbor。
- 已登记并导入一个仅供 opt-in 运行时 MVP 使用的窄条背景候选：`SourceArt/UI/PSD/desktop-training-v1/generated/TrainingIdleStrip_Background_GPT_v003_Seamless_RGBA.png`；源文件仍登记为 draft，UE 资产与源 SHA256 见 `SourceAssets/AnimationProcessing/walkloop_pilot_v1/character_00_hero_walk_left/runtime-import-manifest.json`。它不得被当作 PSD 完成或入口切换证据。
- 目标运行包络仍是约 `1200×108` 的顶部挂机条，六个既有角色和三只既有敌人以后在其上层合成；禁止非等比拉伸。最终 PSD 必须按 `BG_CharcoalInk / BG_MountainSilhouette / BG_Path / BG_Decor / FX_GroundShadow / Actors_ExistingSprites / HUD_RuntimeOnly` 分层交付。
- 主角向左走路试作已隔离登记：`SourceAssets/AnimationProduction/walkloop_pilot_v1/hero_walk_left/` 保存 1600×1600 接触姿势、即梦提交记录与原片；`scripts/prepare_walkloop_atlases.py` 生成 60 帧 RGBA 512 cell、4K 工作母图、2048² 2K atlas 和 1024² 1K atlas。首尾帧字节一致，`scripts/test_walkloop_atlas_pipeline.py` 3/3 通过；源状态仍为 `review-only`，仅 2K/1K 作为 opt-in 历练视觉 MVP 导入，未替换现有角色动画。
- **游历顶部条表现竖切已通过，仍非总目标完成**：`FGameXXKTrainingTravelVisualRuntime` 已把 1 秒权威步进与实时表现分离，使用三段无缝底图和 `Walking / EncounterIdle / HeroAttack / EnemyHit / EnemyAttack / HeroHit / EnemyDeath / HeroDeath / Paused` 状态；滚动速度连续减速/恢复，循环不再把 offset/步行动画重置。顶部条复用局内 battle atlas，显示当前敌人、双方 HP，并移除调试段落。双方取消错误镜像后保持“敌左向右、我右向左”；Hero Walk/Idle/Attack/Hit/Death 以脚底中心锚点和动作独立透明留白归一化保持同一视觉身高。冷 UBT、Workbench 14/14、Travel runtime 3/3、真实 PIE 60 帧动态探针均通过；Luna max 对 6.3 秒/36 帧桌面序列的总判定为 PASS。证据总表见 `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`。该竖切不改变 1-1 默认通关或挑战/游历规则，也不冒充尚未实现的共享三敌/三我最终编制。
- **2D HUD 迁移分支已建立独立验收面，但未切默认入口**：隔离副本 `Content/GameXXK/Maps/L_DesktopTrainingHUD.umap` 复制 `L_QingshanInn` 后仅保留 `PlayerStart`，删除放置场景/灯光/出口/角色 Actor；直接加载该地图会自动显示工作台。`Town` 状态和主菜单继续指向已验收的 3D 青山镇，原城镇与用户修改的 `L_Main.umap` 保持回退基线；只有完整目标最终验收通过后才允许切换。迁移证据与限制见 `docs/production/2026-08-18-desktop-training-2d-hud-migration.md`。

- Phase 0 基线证据：`docs/production/2026-08-17-phase0-baseline.md`；执行计划：`docs/superpowers/plans/2026-08-17-gamexxk-phase0-source-of-truth-and-gates.md`；本轮完整复核：`docs/production/2026-08-18-desktop-training-goal-review.md`。
- 项目自身优化:见 `docs/production/2026-08-16-full-project-optimization-proposal.md`(Phase 0 → Phase 4 全量方案)。Phase 0 门禁为 harness 无 finding、默认生产循环全绿、headless 脚本全绿、all 不启动编辑器、`git diff --check` 通过。
- 玩法顺序:Phase 0 收尾已基本完成 → Phase 1 地形增益重设计(先复核 §5 山河三档与 `TriggerTerrainBenefit` 两个口径)→ Phase 2 数值迭代(以 `2026-08-12-balance-tuning-ledger.md` §4.8 为最新基准)。
- 非阻塞测试工具维护:更新 `scripts/gamexxk_real_play_flow_mcp.py` 的旧 `pointer_matches_target` 吸附断言，使其符合 `6668146` 冻结的自由鼠标跟随语义；Phase 0 已把 `--script-tests all` 分成 `headless`、`asset-contract`、`mcp-live` 三类，历史 64/86 仅保留作迁移前对照，当前 headless/all 门禁通过，asset-contract 当前 57/69（剩余 12 项为外部源/旧合同/环境问题），mcp-live 仍需单独取证，不能冒充全绿。
