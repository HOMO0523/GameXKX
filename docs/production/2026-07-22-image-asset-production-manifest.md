# GameXXK 图像资产生产清单（Phase 0）

更新日期：2026-07-22  
负责人：专职图像资产 Agent（`/root/image_asset_lead`）  
状态：Phase 0 只读审计完成，尚未开始生成、导入或替换  
上位目标：[`2026-07-22-project-expansion-overall-goal.md`](./2026-07-22-project-expansion-overall-goal.md)

## 1. 本清单的执行边界

本清单是后续所有生图工作的唯一排期与验收账本。它不授权在 Phase 0 修改 C++、蓝图、地图、现有贴图或现有角色资产。

- **初版阶段**：允许按批次生成、抠透明底、质检并导入版本化 `Draft` 命名空间；无需逐张请用户确认。功能开发只能绑定这些初版 Draft 或明确列出的既有资产。
- **迭代阶段**：一次只能处理一个 `AssetId`。该图候选完成后，图像 Agent 把预览、候选路径、完整提示词和修改说明交给主 Agent；主 Agent向用户确认。用户明确通过之前，不得生成下一张。
- **终版入库**：所有处于 `final-approval-needed` 的图片均取得用户明确通过后，才执行一次终版批量导入/替换。先通过的图片只进入批准暂存区，不提前替换游戏内容。
- **不可覆盖**：既有 PSD 原子、主角、现有 Boss、V4 状态图标、人工调整过的角色/PaperZD/地图/镜头/HD2D 资产均是只读输入。任何修订都新建版本，绝不覆盖旧文件或旧 UE 包。
- **卡牌关键约束**：174 张逻辑卡牌继续共享 **17 个图像族/ArtKey**，不是 174 张独立生图。卡名、品质、数值、效果文本属于运行时 UI，不烘焙进图片。

## 2. 状态定义与命名空间

### 2.1 状态字段

| 字段值 | 含义 |
|---|---|
| `existing` | 文件或 UE 资产已经存在；只能复用、审阅或从旧资产派生，不能覆盖 |
| `initial-draft-needed` | 尚无可供功能绑定的初版，需进入无确认批量生成阶段 |
| `final-approval-needed` | 终版必须进入“一图一确认”队列；`No` 表示已有明确锁定或不属于生图终审范围 |
| `locked-read-only` | 已有素材明确锁定，只能作为参考/绑定，禁止重新生成与覆盖 |
| `retired-draft-read-only` | 旧初版仍保留供追溯，但已被最新美术要求否决，不得作为终版 |

### 2.2 版本化路径

| 阶段 | 本地源文件根 | UE 根 | 规则 |
|---|---|---|---|
| 初版候选 | `SourceArt/Generated/Draft/V1/<Domain>/` | `/Game/GameXXK/Generated/Draft/V1/<Domain>/` | 可批量生成和导入；不覆盖既有资产 |
| 单图迭代 | `SourceArt/Generated/Candidates/<AssetId>/vNN/` | 不导入 | 每次只允许一个 AssetId；保留每个版本 |
| 已批准暂存 | `SourceArt/Generated/Approved/<AssetId>/vNN/` | 不导入 | 记录批准人、日期、候选 SHA-256 和提示词 |
| 终版批量入库 | 各领域下述 `Final source` | 各领域下述 `UE final target` | 全部终审通过后一次执行；未来修订用 V2，不覆盖 V1 |

文件名统一使用 ASCII：`<asset_slug>_draft_v1_chroma.png`、`<asset_slug>_draft_v1_alpha.png`；单图迭代使用 `<asset_slug>_candidate_vNN_*`。透明素材默认先生成纯 `#ff00ff` 背景，再用已安装的 `remove_chroma_key.py` 生成 RGBA；复杂边缘无法合格时才升级透明方案。

## 3. 全局视觉合同

所有新图共享：GameXXK 当前分层 PSD、主角、任务 NPC、金钱鼠、黑熊、虎王的视觉语言；简化 Q 版中国水墨与轻水彩；低饱和、低对比；外轮廓清楚；大头紧凑身体；减少衣褶、小零件和高频纹理；纸张颗粒克制；禁止照片感、3D 渲染、像素风、Logo、文字、数值和 UI 烘焙。

参考根：

- PSD：`outputs/UI_PSD/GameXXK_Town_4K.psd`
- PSD 切图与语义：`SourceArt/UI/PSD/town-v2/`
- 已锁定状态风格：`SourceArt/UI/Battle/StatusIcons/*_inkflat_v4.png`
- 既有角色/怪物资产：`Content/GameXXK/Characters/`、`Content/GameXXK/Sprites/`

透明方形图标的主体填充率目标为 82%–94%；角色/怪物为可用画布的 78%–92%。所有 Alpha 图四角必须为 0，禁止洋红溢色、白边、裁切和贴边阴影。

## 4. 既有且不可覆盖的基线资产

| AssetId / 组 | 现状 | 用途与数量 | 参考 / 当前绑定 | 规格与 Alpha | Draft path | UE target / 当前绑定 | 终审 |
|---|---|---|---|---|---|---|---|
| `BASE.PSD.TOWN.ATOMS` | `existing`, `locked-read-only` | 47 个语义原子 + 6 个背景；角色/伙伴统一界面的纸底、页签、按钮、栏位基线 | `SourceArt/UI/PSD/town-v2/manifest.json`、`semantic-map.json` | 原始尺寸；按现有 Alpha/九宫规则 | 不生成 | `/Game/GameXXK/UI/Town/Textures/PSD`，53 个既有 UAsset | `No` |
| `BASE.UI.CARD_FRAME` | `existing`, `locked-read-only` | 全卡牌框 | PSD atom 057 | 既有透明框 | 不生成 | `/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057` | `No` |
| `BASE.UI.SCROLLBAR` | `existing`, `locked-read-only` | 图鉴/伙伴背包等滚动条 | `Content/GameXXK/UI/PartyDeck/Scrollbars` | 既有 Track/Thumb | 不生成 | 当前 PartyDeck style binding | `No` |
| `BASE.UI.RESOURCE_BARS` | `existing`, `locked-read-only` | 血量/内力空槽与满层，共 6 张 | `SourceArt/UI/Battle/ResourceBars/` | 既有 RGBA | 不生成 | `/Game/GameXXK/UI/Battle/ResourceBars` | `No` |
| `BASE.UI.STATUS_V4` | `existing`, `locked-read-only` | 13 个已锁定的扁平单色水墨状态图标 | `SourceArt/UI/Battle/StatusIcons/*_inkflat_v4.png` | 方形 RGBA；运行时层数/Tooltip 不烘焙 | 不生成 | `/Game/GameXXK/UI/Battle/StatusIcons` | `No`（用户已确认 V4） |
| `BASE.UI.PARTY_QI` | `existing`, `locked-read-only` | 右下手牌旁全队气力魂魄图 | `SourceArt/UI/Battle/PartyQi/battle_party_qi_soul_orb_v1.png` | 方形 RGBA；数字运行时叠加 | 不生成 | `/Game/GameXXK/UI/Battle/PartyQi/T_BattlePartyQi_SoulOrb`，`GameXXKBattlePartyQiWidget.cpp` | `No`；仅运行时尺寸仍可调，不改图 |
| `BASE.UI.META_CURRENCY_ITEMS` | `existing`, `locked-read-only` | 局外金币与强化石等局外资源图标 | PSD Backpack/HUD 中既有方孔钱、玉石/晶石原子；由语义映射选定，不新增相似图 | 既有方形 RGBA；数值运行时叠加 | 不生成 | `/Game/GameXXK/UI/Town/Textures/PSD`；最终代码只引用语义原子 | `No` |
| `BASE.CHAR.HERO` | `existing`, `locked-read-only` | 主角立绘/战斗图/动画 | 现有主角资产与已调 PaperZD | 保持原规格 | 不生成 | 现有 Hero 根 | `No` |
| `BASE.ENEMY.BOSSES` | `existing`, `locked-read-only` | 金钱鼠、黑熊、虎王 3 个章节 Boss | `FB_Enemy_MoneyMouse`、`FB_Enemy_BlackBear`、`FB_Boss_Tiger` | 保持原规格 | 不生成 | `/Game/GameXXK/Characters/Enemies` | `No` |
| `BASE.EVENT.NIUHUAN` | `existing`, `locked-read-only` | 牛欢事件 NPC 视觉 | 现有 `FB_Enemy_NiuHuan` 等资产；只改变语义用途，不重画 | 保持原规格 | 不生成 | 现有角色根，后续由事件映射复用 | `No` |
| `BASE.CHAR.PARTYDECK_OLD` | `existing`, `retired-draft-read-only` | 旧六职业/六 NPC 八向、Idle/Walk、PaperZD 共 12 套 | `SourceAssets/PartyDeck/character-references/generated/`、旧 PartyDeck UE 根 | 旧 171×205 流程；画风过细已被否决 | 不再生成到旧根 | `/Game/GameXXK/Characters/PartyDeckNPC`、`PartyDeckPartners`，仅回溯 | `Yes`，由新 V1 套件替代 |

## 5. 卡牌：174 张逻辑卡共享 17 个 ArtKey

当前战斗、伙伴和城镇代码直接绑定以下 17 个 `T_CardPortrait_*` 路径。终版只迭代 17 个共享图像族；174 张逻辑卡的品质色、费用、数值、名称、效果、目标和 Tooltip 由运行时绘制。

| AssetId | 覆盖逻辑卡 | 现状 | 图像来源 / 提示词差异 | 规格 / Alpha | Draft path | UE final target / 当前绑定 | 终审 |
|---|---:|---|---|---|---|---|---|
| `CARD.ART.HERO` | 12 | `existing`, `locked-read-only` | 现有主角原画裁切；不重新生图 | 1024×1024 RGBA | 不生成 | `/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero` | `No` |
| `CARD.ART.ROLE.BLADE` | 18 | `existing`, `final-approval-needed` | 新职业 `Blade` 站立母版的确定性半身裁切；敏捷刀客、青灰短褂、束带、单刀 | 1024×1024 RGBA | `.../Draft/V1/CardArt/role_blade.png` | `T_CardPortrait_Role_Blade`；当前是旧共享图 | `Yes` |
| `CARD.ART.ROLE.GUARD` | 18 | `existing`, `final-approval-needed` | 新 `Guard` 母版裁切；结实乡勇、赭色棉衣、圆盾、短枪 | 同上 | `.../role_guard.png` | `T_CardPortrait_Role_Guard` | `Yes` |
| `CARD.ART.ROLE.HEALER` | 18 | `existing`, `final-approval-needed` | 新 `Healer` 母版裁切；浅鼠尾草色医者、药囊、药葫芦 | 同上 | `.../role_healer.png` | `T_CardPortrait_Role_Healer` | `Yes` |
| `CARD.ART.ROLE.HUNTER` | 18 | `existing`, `final-approval-needed` | 新 `Hunter` 母版裁切；棕绿色披风、弓与箭袋 | 同上 | `.../role_hunter.png` | `T_CardPortrait_Role_Hunter` | `Yes` |
| `CARD.ART.ROLE.SORCERER` | 18 | `existing`, `final-approval-needed` | 新 `Sorcerer` 母版裁切；蓝灰符师、法杖、两张符纸 | 同上 | `.../role_sorcerer.png` | `T_CardPortrait_Role_Sorcerer` | `Yes` |
| `CARD.ART.ROLE.FORMATION_MASTER` | 18 | `existing`, `final-approval-needed` | 新 `FormationMaster` 母版裁切；暖灰谋士、羽扇、阵旗 | 同上 | `.../role_formation_master.png` | `T_CardPortrait_Role_FormationMaster` | `Yes` |
| `CARD.ART.NPC.TUSI_CHIEF` | 4 | `existing`, `final-approval-needed` | 新土司首领母版裁切；保留 PPT/现有身份特征 | 同上 | `.../npc_tusi_chief.png` | `T_CardPortrait_Npc_TusiChief` | `Yes` |
| `CARD.ART.NPC.SONG_JIN_BAO` | 4 | `existing`, `final-approval-needed` | 新宋金宝母版裁切；保留原身份 | 同上 | `.../npc_song_jin_bao.png` | `T_CardPortrait_Npc_SongJinBao` | `Yes` |
| `CARD.ART.NPC.YUE_BAI` | 4 | `existing`, `final-approval-needed` | 新月白母版裁切；保留原身份 | 同上 | `.../npc_yue_bai.png` | `T_CardPortrait_Npc_YueBai` | `Yes` |
| `CARD.ART.NPC.ZHOU_GUANG_ZU` | 4 | `existing`, `final-approval-needed` | 新周光祖母版裁切；保留原身份 | 同上 | `.../npc_zhou_guang_zu.png` | `T_CardPortrait_Npc_ZhouGuangZu` | `Yes` |
| `CARD.ART.NPC.JIN_GUI` | 4 | `existing`, `final-approval-needed` | 新金贵母版裁切；保留原身份 | 同上 | `.../npc_jin_gui.png` | `T_CardPortrait_Npc_JinGui` | `Yes` |
| `CARD.ART.NPC.QIONG_MEI_ER` | 4 | `existing`, `final-approval-needed` | 新琼么儿母版裁切；保留原身份 | 同上 | `.../npc_qiong_mei_er.png` | `T_CardPortrait_Npc_QiongMeiEr` | `Yes` |
| `CARD.ART.ROUTE.GENERAL` | 共享 30 中的普通路线牌 | `existing`, `final-approval-needed` | 现有通用路线水墨动作图 | 1024×1024 RGBA | 初版复用当前图 | `T_CardPortrait_Route_General` | `Yes` |
| `CARD.ART.ROUTE.TERRAIN` | 共享 30 中的地形牌 | `existing`, `final-approval-needed` | 现有山水/地形图 | 同上 | 初版复用当前图 | `T_CardPortrait_Route_Terrain` | `Yes` |
| `CARD.ART.ROUTE.RARE` | 共享 30 中的稀有牌 | `existing`, `final-approval-needed` | 现有稀有路线图 | 同上 | 初版复用当前图 | `T_CardPortrait_Route_Rare` | `Yes` |
| `CARD.ART.ROUTE.BOSS` | 共享 30 中的 Boss 牌 | `existing`, `final-approval-needed` | 现有 Boss 路线图 | 同上 | 初版复用当前图 | `T_CardPortrait_Route_Boss` | `Yes` |

校验口径：`12 + 6×18 + 6×4 + 30 = 174`；共享图像族数量固定为 `1 + 6 + 6 + 4 = 17`。新职业/NPC 图不另行生一张卡图，而从已批准站立母版确定性裁切，保持身份一致。

## 6. 统一角色/伙伴视觉套件（12 个身份）

每个身份有三个独立生成记录：`MASTER`（站立母版）、`DIR8`（4×2 八向表）、`IDLE4`（四帧战斗 Idle）；另从批准母版确定性派生 `PORTRAIT`，不额外生图。伙伴名称池 72 个名字只区分实例，六职业每个职业的 12 名伙伴共享该职业同一视觉套件。

**原画高保真硬规则：**凡是已有原画、卡面、PPT 设定或游戏内现有素材的角色/怪物，以该原画作为唯一身份与几何基准；必须保留物种与身体结构、脸型、轮廓、比例、发型/火焰走势、服装结构、标志物和主配色。只允许做透明底整理、低饱和水墨 Q 版语言统一、像素/方向/动作适配；不得借风格统一重新设计身份。只有在确认不存在任何原画参考时才能按锁定风格补创。每张门槛页必须显式列出“原画保持点”并逐项校验。

| AssetId 组 | 身份 | 现状 | 提示词身份差异 | 规格 / 方向 / Alpha | Draft path | UE final target | 终审 |
|---|---|---|---|---|---|---|---|
| `CHAR.ROLE.BLADE.{MASTER,DIR8,IDLE4}` | Blade | `initial-draft-needed` | 敏捷刀客、青灰短褂、束带、单刀，Q 版简化 | 母版 2048²；DIR8 为 4×2、每格 512²，顺序 S/SW/W/NW/N/NE/E/SE；Idle 为 4×512² 横排朝左；RGBA | `SourceAssets/CharacterVisuals/candidates/v1/Draft/role_blade_*` | `/Game/GameXXK/Characters/Generated/Partners/V1` 等 V1 根 | `Yes` |
| `CHAR.ROLE.GUARD.{MASTER,DIR8,IDLE4}` | Guard | `initial-draft-needed` | 结实乡勇、赭色棉衣、圆盾、短枪 | 同上 | `.../role_guard_*` | 同上 | `Yes` |
| `CHAR.ROLE.HEALER.{MASTER,DIR8,IDLE4}` | Healer | `initial-draft-needed` | 浅鼠尾草色医者、药囊、药葫芦 | 同上 | `.../role_healer_*` | 同上 | `Yes` |
| `CHAR.ROLE.HUNTER.{MASTER,DIR8,IDLE4}` | Hunter | `initial-draft-needed` | 棕绿色披风、弓、箭袋 | 同上 | `.../role_hunter_*` | 同上 | `Yes` |
| `CHAR.ROLE.SORCERER.{MASTER,DIR8,IDLE4}` | Sorcerer | `initial-draft-needed` | 蓝灰符师、法杖、两张符纸 | 同上 | `.../role_sorcerer_*` | 同上 | `Yes` |
| `CHAR.ROLE.FORMATION_MASTER.{MASTER,DIR8,IDLE4}` | FormationMaster | `initial-draft-needed` | 暖灰谋士、羽扇、阵旗 | 同上 | `.../role_formation_master_*` | 同上 | `Yes` |
| `CHAR.NPC.TUSI_CHIEF.{MASTER,DIR8,IDLE4}` | 土司首领 | `initial-draft-needed` | 以批准 PPT/原画为身份参考，统一 Q 版简化 | 同上 | `.../npc_tusi_chief_*` | `/Game/GameXXK/Characters/Generated/TaskNpc/V1` 等 V1 根 | `Yes` |
| `CHAR.NPC.SONG_JIN_BAO.{MASTER,DIR8,IDLE4}` | 宋金宝 | `initial-draft-needed` | 保留原画面部、服装主色和标志物 | 同上 | `.../npc_song_jin_bao_*` | 同上 | `Yes` |
| `CHAR.NPC.YUE_BAI.{MASTER,DIR8,IDLE4}` | 月白 | `initial-draft-needed` | 严格保留原画的单团淡紫蓝火焰、锐利面部与火焰走势；无身体、服装、四肢、武器或附属火球 | 母版 2048²；DIR8/Idle 围绕同一无身体火焰结构适配；RGBA | `.../npc_yue_bai_*` | 同上 | `Yes` |
| `CHAR.NPC.ZHOU_GUANG_ZU.{MASTER,DIR8,IDLE4}` | 周光祖 | `initial-draft-needed` | 同上 | 同上 | `.../npc_zhou_guang_zu_*` | 同上 | `Yes` |
| `CHAR.NPC.JIN_GUI.{MASTER,DIR8,IDLE4}` | 金贵 | `initial-draft-needed` | 同上 | 同上 | `.../npc_jin_gui_*` | 同上 | `Yes` |
| `CHAR.NPC.QIONG_MEI_ER.{MASTER,DIR8,IDLE4}` | 琼么儿 | `initial-draft-needed` | 同上 | 同上 | `.../npc_qiong_mei_er_*` | 同上 | `Yes` |

角色/伙伴统一 UI 本身不新增一套烘焙界面图：继续使用 `BASE.PSD.TOWN.ATOMS` 的角色页、装备页、页签、背包格与按钮；主角用既有主角立绘，伙伴选择栏切换上述六职业视觉，任务 NPC 图鉴切换六 NPC 视觉。

## 7. 三章节新怪物（18 只）与新增状态字形（7 个）

每只新怪物生成两个独立记录：`MASTER` 与 `BATTLE`；Codex 头像从批准 `MASTER` 确定性裁切。现有三个 Boss 不重画。通用提示词增量：完整主体、Q 版动物、78%–90% 填充、纯 `#ff00ff`、无文字/卡框/UI；战斗图保留母版身份与配色并适配当前左右战斗站位。

| AssetId 组 | 怪物 | 章节/级别 | 现状 | Draft path | Final source / UE final target | 终审 |
|---|---|---|---|---|---|---|
| `ENEMY.CH1.ROOSTER.{MASTER,BATTLE}` | 公鸡 | 1/普通 | `initial-draft-needed` | `SourceAssets/RouteEnemies/candidates/v1/Draft/ch1_rooster_*` | `approved/v1`；`/Game/GameXXK/Sprites/Generated/RouteEnemies/V1`、Characters/Codex V1 | `Yes` |
| `ENEMY.CH1.GOAT.{MASTER,BATTLE}` | 山羊 | 1/普通 | `initial-draft-needed` | `.../ch1_goat_*` | 同上 | `Yes` |
| `ENEMY.CH1.WEASEL.{MASTER,BATTLE}` | 黄鼬 | 1/普通 | `initial-draft-needed` | `.../ch1_weasel_*` | 同上 | `Yes` |
| `ENEMY.CH1.CIVET.{MASTER,BATTLE}` | 果子狸 | 1/普通 | `initial-draft-needed` | `.../ch1_civet_*` | 同上 | `Yes` |
| `ENEMY.CH1.IRONFEATHER_ROOSTER.{MASTER,BATTLE}` | 铁羽鸡王 | 1/精英 | `initial-draft-needed` | `.../ch1_ironfeather_rooster_*` | 同上 | `Yes` |
| `ENEMY.CH1.BLUEHORN_GOAT_KING.{MASTER,BATTLE}` | 青角羊王 | 1/精英 | `initial-draft-needed` | `.../ch1_bluehorn_goat_king_*` | 同上 | `Yes` |
| `ENEMY.CH2.GRAY_WOLF.{MASTER,BATTLE}` | 灰狼 | 2/普通 | `initial-draft-needed` | `.../ch2_gray_wolf_*` | 同上 | `Yes` |
| `ENEMY.CH2.BOAR.{MASTER,BATTLE}` | 野猪 | 2/普通 | `initial-draft-needed` | `.../ch2_boar_*` | 同上 | `Yes` |
| `ENEMY.CH2.MACAQUE.{MASTER,BATTLE}` | 猕猴 | 2/普通 | `initial-draft-needed` | `.../ch2_macaque_*` | 同上 | `Yes` |
| `ENEMY.CH2.PORCUPINE.{MASTER,BATTLE}` | 豪猪 | 2/普通 | `initial-draft-needed` | `.../ch2_porcupine_*` | 同上 | `Yes` |
| `ENEMY.CH2.GRAYMANE_WOLF_KING.{MASTER,BATTLE}` | 灰鬃狼王 | 2/精英 | `initial-draft-needed` | `.../ch2_graymane_wolf_king_*` | 同上 | `Yes` |
| `ENEMY.CH2.REDTUSK_BOAR_KING.{MASTER,BATTLE}` | 赤牙猪王 | 2/精英 | `initial-draft-needed` | `.../ch2_redtusk_boar_king_*` | 同上 | `Yes` |
| `ENEMY.CH3.VENOM_SNAKE.{MASTER,BATTLE}` | 毒蛇 | 3/普通 | `initial-draft-needed` | `.../ch3_venom_snake_*` | 同上 | `Yes` |
| `ENEMY.CH3.WILDCAT.{MASTER,BATTLE}` | 野猫 | 3/普通 | `initial-draft-needed` | `.../ch3_wildcat_*` | 同上 | `Yes` |
| `ENEMY.CH3.VULTURE.{MASTER,BATTLE}` | 秃鹫 | 3/普通 | `initial-draft-needed` | `.../ch3_vulture_*` | 同上 | `Yes` |
| `ENEMY.CH3.GIANT_TOAD.{MASTER,BATTLE}` | 巨蟾 | 3/普通 | `initial-draft-needed` | `.../ch3_giant_toad_*` | 同上 | `Yes` |
| `ENEMY.CH3.WHITE_APE.{MASTER,BATTLE}` | 白猿 | 3/精英 | `initial-draft-needed` | `.../ch3_white_ape_*` | 同上 | `Yes` |
| `ENEMY.CH3.SPIRAL_HORN_DEER.{MASTER,BATTLE}` | 旋角鹿 | 3/精英 | `initial-draft-needed` | `.../ch3_spiral_horn_deer_*` | 同上 | `Yes` |

怪物生成源统一 2048×2048；批准后导入脚本只做去空边、落脚点归一和确定性头像裁切，不重画。精英通过角、羽、鬃、獠牙等单一强轮廓差异表达，不堆高频装饰。

| AssetId | 图形语义 | 现状 | 规格 / Alpha | Draft path | UE final target | 终审 |
|---|---|---|---|---|---|---|
| `STATUS.GLYPH.MEDICINE` | 草药束 | `initial-draft-needed` | 512²，单色扁平水墨，主体 82%–94%，RGBA | `.../Draft/V1/Status/medicine.png` | `/Game/GameXXK/UI/Battle/Status/V1` | `Yes` |
| `STATUS.GLYPH.WEAK` | 下垂断刃 | `initial-draft-needed` | 同上 | `.../weak.png` | 同上 | `Yes` |
| `STATUS.GLYPH.WEALTH` | 方孔钱 | `initial-draft-needed` | 同上 | `.../wealth.png` | 同上 | `Yes` |
| `STATUS.GLYPH.RAGE` | 角形火焰 | `initial-draft-needed` | 同上 | `.../rage.png` | 同上 | `Yes` |
| `STATUS.GLYPH.PREY` | 墨圈靶眼 | `initial-draft-needed` | 同上 | `.../prey.png` | 同上 | `Yes` |
| `STATUS.GLYPH.CHARGE` | 螺旋角 | `initial-draft-needed` | 同上 | `.../charge.png` | 同上 | `Yes` |
| `STATUS.GLYPH.COUNTER` | 回钩返刃 | `initial-draft-needed` | 同上 | `.../counter.png` | 同上 | `Yes` |

## 8. 遗物（30 个现有导入 Draft）

30 个 UAsset 已存在并被 `GameXXKRelicCatalog.cpp` 绑定，但源目录 `docs/ui/relics/source_art` 缺失，且没有逐图终审元数据。因此它们是 **可供功能使用的既有 Draft**，不是已批准终版。初版不重复生成；终版队列按现有导出预览逐图确认，需修改的单图才生成新候选。

| AssetId 组 | 子 AssetId（30） | 现状 | 规格 / Alpha | Draft / 参考 | UE target / 当前绑定 | 终审 |
|---|---|---|---|---|---|---|
| `RELIC.ICON.*` | `ANCIENT_COIN`, `JADE_BELL`, `BAMBOO_TALLY`, `TIGER_SEAL`, `MEDICINE_GOURD`, `INK_TALISMAN`, `CLOUD_MIRROR`, `STONE_BEAD`, `CRANE_FEATHER`, `IRON_KNOT`, `TEA_BRICK`, `COMPASS`, `RED_CORD`, `BRONZE_NEEDLE`, `RAIN_CAPE`, `CHESS_STONE`, `DRUM_CHARM`, `LOTUS_SEED`, `SWORD_GUARD`, `OLD_MAP`, `PINE_CONE`, `RIVER_PEARL`, `CANDLE_STUB`, `FOX_MASK`, `STONE_LION`, `WINE_CUP`, `HERB_BASKET`, `PAPER_CRANE`, `BROKEN_ARROW`, `MOON_DISC` | `existing`, `final-approval-needed` | 方形 RGBA，72px 可读，主体约 85%，低饱和简化水墨 | 当前 UAsset 导出预览作为 Draft；修订进 `SourceArt/Generated/Candidates/RELIC.ICON.<ID>/vNN/` | `/Game/GameXXK/UI/Relics/Icons`，30 个均已绑定 | `Yes`（逐图） |

## 9. 装备与局外商店图像

六套装：`PoJun` 破军、`XuanJia` 玄甲、`QingNang` 青囊、`ZhuiFeng` 追风、`ShiGu` 蚀骨、`ShanHe` 山河。六部件：`Weapon`、`Head`、`Armor`、`Belt`、`Shoes`、`Accessory`。每个 `(Set, Slot)` 是独立基础装备图，共 36 张；品质与词缀由 UI 表现，不在图中烘焙。另有 6 张套装/装备包徽记，局外商店的对应装备包复用该徽记。

共享规格：1024×1024 RGBA，方形主体填充 82%–92%，一件清楚的 Q 版水墨道具，低饱和单一主色，不带边框、文字、星级或品质光效。提示词由“套装流派材质/纹样 + 部件轮廓”组合，禁止简单换色伪装成不同装备。

| AssetId 组 | 36 个子 AssetId | 现状 | Draft path | UE final target | 终审 |
|---|---|---|---|---|---|
| `EQUIP.POJUN.*` | `WEAPON`, `HEAD`, `ARMOR`, `BELT`, `SHOES`, `ACCESSORY` | `initial-draft-needed` | `SourceArt/Generated/Draft/V1/Equipment/pojun_<slot>.png` | `/Game/GameXXK/UI/Equipment/Icons/V1` | `Yes` |
| `EQUIP.XUANJIA.*` | 同六部件 | `initial-draft-needed` | `.../xuanjia_<slot>.png` | 同上 | `Yes` |
| `EQUIP.QINGNANG.*` | 同六部件 | `initial-draft-needed` | `.../qingnang_<slot>.png` | 同上 | `Yes` |
| `EQUIP.ZHUIFENG.*` | 同六部件 | `initial-draft-needed` | `.../zhuifeng_<slot>.png` | 同上 | `Yes` |
| `EQUIP.SHIGU.*` | 同六部件 | `initial-draft-needed` | `.../shigu_<slot>.png` | 同上 | `Yes` |
| `EQUIP.SHANHE.*` | 同六部件 | `initial-draft-needed` | `.../shanhe_<slot>.png` | 同上 | `Yes` |

| AssetId 组 | 子 AssetId | 现状 | 提示词差异 | 规格 / Alpha | Draft path | UE final target | 终审 |
|---|---|---|---|---|---|---|---|
| `EQUIP.SET_EMBLEM.*` | `POJUN`, `XUANJIA`, `QINGNANG`, `ZHUIFENG`, `SHIGU`, `SHANHE` | `initial-draft-needed` | 分别表达猛攻、坚守、治疗、速度、状态、均衡；只用一个清晰象征物 | 1024² RGBA，85% 高填充 | `.../Draft/V1/Equipment/SetEmblems/<set>.png` | `/Game/GameXXK/UI/Equipment/SetEmblems/V1`；装备包复用 | `Yes` |
| `SHOP.PACK.COMPANION` | 伙伴包 | `initial-draft-needed` | 六职业共用的简化行囊/人物剪影组合，不绑定名字与具体职业 | 1024² RGBA | `.../Draft/V1/MetaShop/companion_pack.png` | `/Game/GameXXK/UI/MetaShop/V1` | `Yes` |

## 10. 路线货币、路线商店、事件与奖励插图

| AssetId | 用途 | 现状 | 提示词 / 参考 | 规格 / Alpha | Draft path | UE final target / 当前绑定 | 终审 |
|---|---|---|---|---|---|---|---|
| `ROUTE.MERCHANT.TRAVEL_MONEY` | 左上角行旅货币图标与价格 | `initial-draft-needed` | 青灰麻绳穿起的方孔旅钱，小布签；GameXXK 水墨 Q 版，高填充，无字无数值 | 512² RGBA，主体 65%–88% | `SourceArt/Generated/Draft/V1/RouteMerchant/T_RouteMerchant_TravelMoney.png` | `/Game/GameXXK/UI/RouteMerchant/Textures` | `Yes` |
| `ROUTE.MERCHANT.PORTRAIT` | 路线商人立绘 | `initial-draft-needed` | 亲切行脚商人，简化大头紧凑身形，低饱和纸墨风；无摊位/UI/字 | 1024² RGBA，主体 55%–88% | `.../T_RouteMerchant_Portrait.png` | 同上 | `Yes` |
| `ROUTE.MERCHANT.PAPER` | 商店整体纸底/九宫 | `initial-draft-needed` | 与 PSD 一致的淡黄白宣纸，墨线轻边框，中心干净，禁文字/格子/按钮 | 1536×864 RGBA，四边 72px 九宫安全区 | `.../T_RouteMerchant_Paper.png` | 同上 | `Yes` |
| `ROUTE.EVENT.MOUNTAIN_SPRING` | 正向事件“山泉”左侧插图 | `initial-draft-needed` | 山石间清泉、竹叶、简化水墨小景，主体清楚，禁止整屏背景 | 1024² RGBA | `.../Draft/V1/RouteEncounter/mountain_spring.png` | `/Game/GameXXK/UI/RouteEncounters/V1` | `Yes` |
| `ROUTE.REWARD.CHEST_BAMBOO` | 奖励三选一竹箱插图 | `initial-draft-needed` | 竹编旅箱，方形高填充简化水墨 | 1024² RGBA | `.../chest_bamboo.png` | 同上 | `Yes` |
| `ROUTE.REWARD.CHEST_BRONZE` | 青铜箱插图 | `initial-draft-needed` | 小型青铜匣，旧铜低饱和 | 同上 | `.../chest_bronze.png` | 同上 | `Yes` |
| `ROUTE.REWARD.CHEST_SHRINE` | 神龛奖励插图 | `initial-draft-needed` | 便携小神龛/供匣，轮廓简化 | 同上 | `.../chest_shrine.png` | 同上 | `Yes` |
| `ROUTE.REWARD.CHEST_TRAVELLER` | 旅人包裹插图 | `initial-draft-needed` | 布包、绳结、卷轴的单一包裹组合 | 同上 | `.../chest_traveller.png` | 同上 | `Yes` |

其余事件人物不新增重复插图：土司首领、宋金宝、月白、周光祖、金贵、琼么儿复用第 6 节批准母版派生头像；牛欢复用 `BASE.EVENT.NIUHUAN`。事件与奖励画面继续使用 PSD 纸底 + 运行时立绘/选择按钮，不生成烘焙整屏截图。

## 11. 初版批次顺序

| 批次 | 内容 | 生成记录数 | 初版导入目的 | 用户确认 |
|---|---|---:|---|---|
| `DRAFT-B01-ROUTE-UI` | 路线货币/商店 3 + 山泉/四奖励插图 5 + 新状态字形 7 | 15 | 先解除路线商店、事件、奖励和状态显示的视觉占位 | 初版免确认 |
| `DRAFT-B02-EQUIPMENT` | 36 基础装备图 + 6 套装徽记 + 1 伙伴包 | 43 | 装备背包、Tooltip、装备包和伙伴包功能绑定 | 初版免确认 |
| `DRAFT-B03-CHARACTERS` | 12 身份 × 3 生成记录 | 36 | 新伙伴/NPC 八向、Idle、统一 UI 立绘与共享卡面派生 | 初版免确认 |
| `DRAFT-B04-ENEMIES` | 18 新怪 × 2 生成记录 | 36 | 三章节普通/精英战斗、图鉴派生 | 初版免确认 |
| `DRAFT-B05-EXISTING-EXPORT` | 30 遗物 UAsset 导出预览 + 17 卡面族联系表 | 0（只导出/登记） | 为逐图终审建立可见源与 SHA-256 | 初版免确认 |

首个实际生图批次固定为 `DRAFT-B01-ROUTE-UI`。每一张仍使用独立 image generation 调用和独立 AssetId，但可连续生产；只进入 Draft 路径。该批完成后应输出 15 张联系表、Alpha/QC 报告和 Draft UE 导入清单，向主 Agent报告进度，但**不得据此提前开始最终视觉绑定**。必须完成 `DRAFT-B01` 至 `DRAFT-B05` 的全部初版生成/导出、QC 与 Draft UE 导入后，功能界面才统一绑定初版素材；初版绑定不代表终版通过。

## 12. 终版逐图确认顺序与门禁

1. Route UI：按 B01 的 15 个 AssetId 顺序，一次一图。
2. 装备：先 6 套装徽记，再按 `Set × Weapon/Head/Armor/Belt/Shoes/Accessory` 逐图，共 42；最后伙伴包。
3. 角色：每个身份按 `MASTER → DIR8 → IDLE4 → 派生 PORTRAIT`；前一项未通过不进入下一项。
4. 怪物：每只按 `MASTER → BATTLE → 派生 CODEX`；前一项未通过不进入下一项。
5. 既有 Draft：30 遗物逐图；17 卡面 ArtKey 中主角只核对不重画，12 个职业/NPC 随角色批准，4 个路线 ArtKey 逐图。
6. 当且仅当所有终审项的批准状态均为 `Approved` 且 SHA-256、提示词、用户确认时间完整，才执行终版批量导入/映射；任何未决项都会阻断整批终版替换。

每次向主 Agent 提交单图时必须包含：

```text
AssetId:
候选预览:
候选本地绝对路径:
完整提示词:
参考图绝对路径及 SHA-256:
尺寸 / Alpha / 主体占比 QC:
相对上一版修改:
请求用户：通过 / 继续修改
```

## 13. Phase 0 审计结论

- 174 张卡已确认是 17 个共享图像族，现有代码在战斗、伙伴和城镇直接复用这些路径；不得扩成 174 张独立图。
- 18 新怪、7 新状态字形、12 套新角色视觉、36 基础装备图、6 套装徽记、1 伙伴包、3 路线商店图和 5 事件/奖励插图均尚缺可绑定初版。
- 30 遗物已存在并绑定，但缺源图和逐图批准证据；可作 Draft，不可宣称终版。
- PSD 原子、Hero、三个 Boss、牛欢既有视觉、V4 状态图标、血/内力条、气力魂魄图、Card Frame 与 Scrollbar 均进入不可覆盖清单。
- 本阶段没有生成图片、没有导入 UE、没有改代码/蓝图/地图、没有覆盖任何现有资产。
