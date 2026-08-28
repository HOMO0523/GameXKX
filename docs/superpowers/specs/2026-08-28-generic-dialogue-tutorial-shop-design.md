# GameXXK 通用对话、新手序章与常驻商店重做设计

**状态：** 设计已确认，运行时实施尚未开始
**日期：** 2026-08-28  
**默认入口：** `/Game/GameXXK/Maps/L_DesktopTrainingHUD`  
**序章场景：** 现有青山镇地图的街道与河边区域  
**首段剧情：** `Story.Main.XuXiakeTreasure` / `Task.Main.XuXiake.Prologue` / `Step.Main.XuXiake.RiverScroll`

## 1. 目标

本工作包建立一套数据驱动、可保存、可分支、可复用的通用对话系统，并用它完成河边卷轴与月白登场的新手序章。系统同时替换 NPC 当前硬编码的 `F` 交互入口，并为后续任务、商店、事件和主线剧情提供统一入口。人物、剧情、任务、演出和实际场景位置必须分层，使未来替换地图或 UI 时不修改剧情内容。

本设计还冻结与 NPC 对话相关的常驻商店边界：恢复背包顶部商店入口，保留六种套装装备包，删除伙伴包并新增宝石包，同时统一宝箱与九合一的十阶品质规则。

## 2. 非目标

- 不制作捏脸、换装或模块化外观系统。
- 不建立新的序章地图或流式序章子关卡。
- 不在 CharacterCatalog、DialogueAsset、StoryCatalog 或 TaskCatalog 中保存地图坐标、Actor 实例或具体地图路径。
- 不在 NPC 对话中提供“入队”“招募”按钮；阵容只在编队界面配置。
- 战斗教程的场景节点只负责触发遭遇；实际战斗继续进入现有全屏 BattleBoard，不改为城镇内战斗。
- 不改变局内路线商店的卡牌强化与遗物规则。
- 不允许剧情 JSON 调用任意 C++ 或 Blueprint 函数。
- 不把任务、商店、移动、镜头或奖励逻辑写进对话显示 Widget。

## 3. 当前项目基础与迁移方向

项目已经具备以下基础：

- 桌面 `剧情任务` 入口可以进入城镇并激活 `TutorialQuest`。
- `FGameXXKTutorialQuestProgress` 已保存状态与稳定步骤 ID，首步为 `Tutorial.EnterTown`。
- `UGameXXKQuestDialogWidget` 已有纸张和按钮资源，但当前内容、按钮与任务逻辑均为硬编码。
- 主角城镇状态已包含 Idle、起步、行走、急停、深呼吸、整理背包、采集、战斗待机、打拳和踢腿。
- 月白出场、待机、离场，以及马和马车的待机、奔跑、急停资源已经进入 Atlas 流程。
- 现有常驻商店正好拥有七个商品位：六种套装包加旧伙伴包。

迁移后，旧 `QuestDialog` 只保留可复用的纸张资源与视觉套件；任务确认、NPC 行为和招募分支从 Widget 中移除。新的显示层只消费通用对话运行时输出。

## 4. 总体架构

```text
CharacterCatalog                人物是谁、能做什么
       ↓
StoryCatalog → TaskCatalog      为什么发生、进度到哪里
       ↓
DialogueAsset                   说什么、如何分支
NarrativeSequenceAsset          人物做什么、演出顺序
       ↓
StageContract                   需要哪些语义位置
       ↓
SceneRegistry → SceneProfile    当前地图在哪里提供这些位置

EncounterDefinition             敌人、规则、奖励
       ↓
BattleProfile                   全屏 BattleBoard 站位、镜头、VFX 槽

GuideAsset                      指引目标、输入策略、完成事件
       ↓
GuideTargetRegistry             UI 语义 ID → 当前实际控件
```

### 4.1 CharacterCatalog

人物库只保存稳定 `CharacterId`、显示名、头像、Actor 类型、动画库、动作能力和默认交互 SequenceId。它不保存 NPC 位置、出生位置、地图或战斗站位。同一人物可以被多个任务、场景和遭遇复用。

### 4.2 StoryCatalog 与 TaskCatalog

`StoryId` 表示一条完整剧情线；一条 Story 可以拥有任务有向图并同时激活多个任务。`TaskId` 保存目标、前置条件、奖励和稳定 Step 图；玩家可以同时进行多条 Story 与多个 Task，但界面只保存一个 `TrackedTaskId`。

稳定 ID 层级为：

```text
StoryId
  └─ TaskId
      └─ StepId
          ├─ DialogueId
          ├─ SequenceId
          ├─ EncounterId
          ├─ StageContractId
          └─ GuideId
```

首条主线使用：

```text
Story.Main.XuXiakeTreasure
Task.Main.XuXiake.Prologue
Step.Main.XuXiake.RiverScroll
Step.Main.XuXiake.CombatTutorial
Sequence.Main.XuXiake.CarriageArrival
Dialogue.Tutorial.001
Route.Tutorial.CombatBasics
```

### 4.3 DialogueAsset

`UGameXXKDialogueAsset` 是编译后的台词/分支权威数据。它保存稳定对话 ID、版本、人物角色、入口节点和节点图。它不保存地图路径、坐标或 Actor 实例。运行时不直接读取 JSON，不接受动态脚本字符串。

### 4.4 NarrativeSequenceAsset

Sequence 保存人物生成、动作、相对移动、语义 Slot 移动、镜头、特效、等待和玩法命令。它只引用 CharacterId/角色别名、StageContractId 和 SlotId，不引用具体 SceneProfile。Task Step 负责组合可复用 Dialogue 与 Sequence。

### 4.5 DialogueRunner

`FGameXXKDialogueRunner` 是无 UI、无 Actor 指针的确定性规则层，负责：

- 开始、继续、暂停和结束会话。
- 线性推进与真实分支。
- 条件判断与选项可见性。
- 记录已读节点与已选选项。
- 生成显示事件，并把稳定 `OutcomeId` 返回给调用它的 Sequence。
- 从节点边界恢复。

### 4.6 DialoguePresenter

首版提供两个显示实现：

- `UGameXXKDialoguePanelWidget`：底部横向纸质正式对话框。
- `UGameXXKSpeechBubbleWidget`：锚定世界角色的头顶气泡。

Presenter 不推进任务、不发放奖励，也不直接移动角色。

### 4.7 NarrativeSequenceRunner 与 CommandDispatcher

`FGameXXKNarrativeSequenceRunner` 是第二个纯状态机。它负责 Sequence 步骤、等待、Dialogue 调用、Outcome 分支、命令边界恢复与命令幂等；DialogueRunner 不执行世界或玩法命令。Dispatcher 只接收 Sequence 发出的已注册命令类型，并路由给独立适配器：

- 城镇角色与 PaperZD 动作适配器。
- 镜头与画面效果适配器。
- 特效与音效适配器。
- 命名 UI 适配器。
- 物品、金币、伙伴与任务适配器。
- 常驻商店适配器。

每条命令返回 `Completed`、`Pending` 或 `Failed`。SequenceRunner 仅在命令完成并提交幂等键后继续。

### 4.8 StageContract、SceneProfile 与 BattleProfile

剧情和演出只引用 `StageContractId` 与语义 SlotId，例如 `Stage.Tutorial.River` / `CarriageStop`。`SceneRegistry` 把 StageContract 映射到当前启用的 SceneProfile；SceneProfile 才保存地图软引用、场景根和 Slot 的实际位置。替换场景只切换注册表映射。

SceneProfile 还拥有 NPC 人物 ID 到固定/巡逻/交互槽位的映射。NPC 位置不进入人物库。

`EncounterId` 保存敌人、战斗规则和奖励；SceneProfile 只提供遭遇触发 Slot。进入遭遇后使用独立 BattleProfile 配置全屏 BattleBoard 的我方、敌方、镜头和特效槽位。

### 4.9 GuideAsset

GuideAsset 保存触发事件、语义控件目标、箭头/遮罩/文字、`Forced` 或 `Soft` 输入策略、完成事件和下一 Step。GuideTargetRegistry 由当前 UI 注册语义 ID，不保存坐标。目标缺失时必须解除输入限制并记录错误。

## 5. JSON 内容格式

每段对话使用一个 UTF-8 JSON 文件。节点、选项和 Outcome 全部使用稳定字符串 ID；演出与副作用命令写在独立的 Sequence JSON 中。

```json
{
  "schemaVersion": 1,
  "dialogueId": "Dialogue.Tutorial.001",
  "dialogueVersion": 1,
  "entryNode": "river.notice",
  "nodes": {
    "river.notice": {
      "type": "line",
      "presentation": "bubble",
      "speaker": "Hero",
      "textId": "tutorial.river.notice",
      "text": "再往前就是天台山了……这是什么？",
      "next": "river.choice"
    },
    "river.choice": {
      "type": "choice",
      "presentation": "dialogue",
      "options": [
        {
          "optionId": "salvage_scroll",
          "textId": "tutorial.river.salvage",
          "text": "打捞河里的卷轴",
          "outcomeId": "Outcome.Tutorial.SalvageRiverMap",
          "next": "yuebai.appear"
        }
      ]
    }
  }
}
```

### 5.1 节点类型

- `line`：显示一段气泡或正式对话。
- `choice`：显示一至四个真实分支选项并输出稳定 OutcomeId。
- `end`：结束会话并向调用它的 Sequence 返回最终 OutcomeId。

演出命令与等待属于 NarrativeSequence，不嵌入 DialogueAsset。Sequence 可以启动 Dialogue 并等待它返回 OutcomeId，再决定后续演出或奖励命令。

### 5.2 条件

首版支持以下注册条件：

- 剧情或世界标记。
- 教程与任务状态。
- 道具数量。
- 金币数量。
- 伙伴是否解锁。
- 之前是否选择指定选项。
- 指定节点是否已读。

选项条件不满足时默认隐藏；配置 `disabledReason` 的选项改为置灰并显示原因。

### 5.3 编译校验

内容编译器必须拒绝：

- 重复或空 ID。
- 不存在的入口、下一节点或分支目标。
- 不可达节点。
- 没有明确出口的循环。
- 不存在的说话者或人物角色。
- 未注册条件或 Outcome。
- 参数越界。
- 超过四个选项。
- 气泡正文超过两行预算。

`textId` 是未来本地化键，`text` 保存当前中文原文并用于内容审查。

## 6. 运行时与存档

通用对话加入下一版存档迁移，版本号在当前 v27 之后递增为 v28。

`FGameXXKDialogueSessionState` 至少保存：

- 是否存在活动阻塞会话。
- `StoryId`、`StoryVersion`、`TaskId` 与 `StepId`。
- 当前 `SequenceId` 与 `StageContractId`。
- `DialogueId` 与 `DialogueVersion`。
- 当前节点 ID；保存值始终表示恢复时要执行的节点边界。
- 已提交选项 ID。
- 已读节点 ID。
- 最近一百条说话者、正文和选择历史。
- 会话暂停原因。

`FGameXXKNarrativeSequenceSessionState` 另外保存：

- 是否存在活动 Sequence。
- `StoryId`、`StoryVersion`、`TaskId`、`StepId`、`SequenceId`、`SequenceVersion` 与 `StageContractId`。
- 当前 Sequence 步骤 ID；保存值始终是可重放的步骤边界。
- 正在等待的 DialogueId、UI 结果或外部完成事件，以及最近返回的 `OutcomeId`。
- 已执行副作用命令的完整幂等键。
- 可重新绑定的上下文角色 ID，不保存 Actor 指针。
- 会话暂停/失败原因；角色命名结果保存在玩家身份状态，不塞入任一状态机。

剧情与任务运行时另外保存：

- `StoryProgressById`：每条剧情的版本、活动/完成状态和活动任务 ID。
- `TaskProgressById`：每个任务的状态、当前 Step、目标计数和奖励提交状态。
- `TrackedTaskId`：当前 UI 追踪任务；不限制其他任务继续推进。
- `GuidePreference`：`Unset`、`NewPlayer` 或 `ExperiencedPlayer`。
- `CompletedGuideStepIds`、`ActiveGuideId` 与 `ActiveGuideStepId`。

保存规则：

- 完成节点后保存下一节点。
- 提交选择后立即保存。
- Sequence 的游戏奖励或任务命令成功后立即记录完整幂等键 `StoryId/TaskId/StepId/CommandId`。
- 中途退出时，Dialogue 从当前节点开头重播，Sequence 从当前步骤边界重放；已记录的副作用命令不重复执行。
- 对话版本变化时，迁移表把旧节点 ID 映射到新节点 ID；没有映射的活动会话安全回到该段入口，不重复已记录奖励。
- 现有 v27 `TutorialQuest` 迁移到 `Story.Main.XuXiakeTreasure` / `Task.Main.XuXiake.Prologue`，不再作为新系统的唯一剧情进度源。

## 7. 演出命令与等待

### 7.1 稳定角色和语义槽位

JSON 不保存坐标、地图或 Marker Actor。`Stage.Tutorial.River` 声明以下必需 SlotId，当前 `SceneProfile.Qingshan.River` 负责把它们绑定到现有街道与河边的实际位置：

- `Tutorial.River.CarriageEntry`
- `Tutorial.River.CarriageStop`
- `Tutorial.River.HeroSpawn`
- `Tutorial.River.CarriageExit`
- `Tutorial.River.ScrollSpawn`
- `Tutorial.River.YueBaiSpawn`
- `Tutorial.River.YueBaiAdvance`
- `Tutorial.River.CameraOverview`
- `Tutorial.River.EncounterTrigger`
- `Tutorial.River.TownRelease`

上下文角色别名包括 `Hero`、`YueBai`、`Horse` 和 `Carriage`，均通过 CharacterCatalog 解析。未来场景提供另一份 SceneProfile 即可，不修改对话或演出序列。

SceneProfile 必须同时提供：

- 玩家进入、复活、返回与安全恢复 Slot。
- NPC 人物 ID、固定 Slot、朝向、巡逻区域与交互锚点。
- 剧情人物、道具、卷轴、VFX、音效和镜头 Slot。
- 遭遇触发区与地图出口。
- SceneProfile 失效时使用的安全出生 Slot。

### 7.2 命令类型

- 角色：生成、显示、隐藏、设置朝向、移动到语义 Slot、按演出单位相对移动、播放动作、恢复 Idle。
- 镜头：锁定、聚焦、平移、跟随、震动、恢复玩家镜头。
- 画面：闪白、角色弹幕、特效、音效、小字提示。
- UI：打开命名、打开商店、关闭指定 UI、等待 UI 返回。
- 玩法：获得物品、修改金币、解锁伙伴、设置剧情标记、推进任务。
- 场景：锁定输入、隐藏普通城镇 UI/NPC、保存与恢复演出前现场。

### 7.3 等待

- 固定秒数。
- 移动完成。
- PaperZD 动作完成。
- 特效或音效完成。
- 玩家推进。
- 命名结果。
- 玩家选择。

跳过已读文字只压缩文字等待；角色移动、命令、奖励和任务仍按顺序执行。

Sequence 命令 `openShop` 会隐藏仍可见的正式对话框并把模态输入所有权交给常驻商店；命令保持 `Pending`，直到商店关闭后从同一 Sequence 步骤继续。需要返回 NPC 菜单时，由后续 Sequence 步骤重新启动对应 Dialogue。商店不能再启动第二个阻塞 NarrativeSequence。

### 7.4 失败策略

- 必需命令失败：暂停 Sequence，记录 Story/Task/Sequence/步骤/命令、活动 Dialogue（若有）与原因，并恢复镜头、输入和城镇 UI。
- 可选表现失败：记录错误并继续。
- 奖励、任务、命名和商店命令不可设为可选。
- 奖励与购买使用同一个候选 RuntimeState；玩法变更、幂等键和下一个 Sequence 步骤全部验证通过后一次原子提交。

## 8. 对话 UI 与操作

### 8.1 头顶气泡

- 锚定说话角色并随角色移动。
- 最多两行。
- 支持阻塞与环境模式。
- 序章使用阻塞模式；普通路人短句以后可用环境模式。
- 不显示头像和选项。

### 8.2 正式对话框

- 位于屏幕底部，使用横向纸质框。
- 左侧或右侧显示说话者头像。
- 显示姓名、正文与继续提示。
- 一至四个选项纵向排列。
- 复用项目现有纸张和按钮套件，不沿用旧 QuestDialog 的硬编码双按钮结构。

### 8.3 操作

- 鼠标左键、`Space`、`Enter`：推进。
- 数字键 `1` 至 `4` 或左键：选择。
- 自动播放：每个中文显示字符 0.06 秒，最短 1.2 秒、最长 6 秒；动画或语音更长时等待较长者。
- 自动播放遇到选择、命名、商店或错误时暂停。
- 按住 `Ctrl` 或点击跳过：只快速通过已读台词。
- 回看：显示本次会话最近一百条台词与已选选项，不重放命令。

### 8.4 输入锁定与退出

- 阻塞对话期间禁用移动、攻击、`F`、背包和其他 HUD 操作。
- `Esc` 打开剧情暂停层，提供“继续”和“退出剧情”。
- 退出剧情会恢复镜头、输入、城镇 UI 和隐藏 NPC，并保留活动 Sequence 步骤与 Dialogue 节点边界。
- 再次点击“剧情任务”或与对应 NPC 交互时继续。
- 同一时间只允许一个阻塞 NarrativeSequence；它内部最多挂起一个阻塞对话会话。

## 9. NPC `F` 交互重做

每个可交互 NPC 使用独立交互组件配置：

- 稳定 `InteractionId`。
- 显示名称。
- 默认 `NarrativeSequenceId`。
- 可交互条件。
- 交互优先级。
- 提示锚点。

每个可交互 NPC 还拥有一个以自身为中心、半径 300 Unreal Units 的圆形交互触发区。触发区只进行查询并仅响应玩家 Pawn 的 Overlap，不阻挡角色、NPC、镜头、攻击或其他碰撞。玩家进入时把该 NPC 登记到角色交互组件的候选集合，离开时立即移除；按 `F` 时只从当前 Overlap 候选中选择，不扫描全场，也不检查主角朝向。

候选按以下顺序确定唯一目标：

1. 交互优先级降序。
2. 距离升序。
3. `InteractionId` 字典序。

当前目标显示 `F 交谈 · NPC名称`。目标改变只更新提示，不自动打开 UI。对话、商店、背包或演出打开时禁用提示和 `F` 路由；关闭后根据仍与玩家重叠的触发区恢复候选，不保留已经离开范围的失效目标。

NPC 的 Dialogue 可以按条件提供：继续交谈、接取任务、提交任务、打开商店、暂且离开；选项只返回 Outcome，随后由交互 Sequence 执行任务或商店命令。NPC 对话永远不提供入队或招募；伙伴和任务 NPC 是否出战只在编队界面决定。

## 10. 新手序章

序章当前通过 `Stage.Tutorial.River → SceneProfile.Qingshan.River` 使用现有青山镇街道与河边，不建立新地图或子关卡。Story、Task、Dialogue 与 Sequence 均不引用青山镇地图路径；替换场景时只切换 SceneRegistry 映射。

### 10.1 开场顺序

1. 从桌面 `剧情任务` 进入城镇并激活教程。
2. 保存主角、镜头、普通 NPC 和城镇 UI 状态。
3. 锁定输入并隐藏无关城镇 UI/NPC。
4. 马与马车从 `Tutorial.River.CarriageEntry` 移动到 `Tutorial.River.CarriageStop`。
5. 播放马和马车的奔跑、急停和急停后待机。
6. 主角在 `Tutorial.River.HeroSpawn` 生成；当前 SceneProfile 令它与马车停车位置重合。
7. 马车重新起步，移动到 `Tutorial.River.CarriageExit` 后隐藏。
8. 马车完全离开后打开主角命名界面。
9. 名字确认并保存后，主角获得控制并开始河边卷轴主线。

命名界面不包含外观选项。默认名字为“小侠客”；输入去除首尾空白，允许一至十二个可显示 Unicode 字符，拒绝空字符串、控制字符和换行。确认后立即保存，剧情内 `Hero` 的显示名读取玩家名字。

### 10.2 河边主线

序章按 `docs/design/2026-08-27-tutorial-prologue-story.md` 的已记录原稿执行：

- 主角气泡发现河中卷轴。
- 玩家选择打捞。
- 获得 `Item.Tutorial.RiverMap`，首版中文名为“河中旧图”。
- 画面闪白，月白出场，主角按 Sequence 的相对演出单位后退。
- 进入 `Dialogue.Tutorial.001` 的正式对话。
- 对话结束后解锁同伴“月白”。
- 开启主线目标“沿徐霞客游历之路，寻找传说中的宝藏”。
- 本段不自动接取旧青山镇主线 `Task.QingshanMain`。

完成、退出或错误恢复时，演出协调器恢复城镇现场并把主角放到 `Tutorial.River.TownRelease`。

## 11. 战斗教程路线与指引

### 11.1 首次选择

每个存档首次进入 `Route.Tutorial.CombatBasics` 时显示：

**标题：** `是否跳过战斗引导？`

- `我是老玩家，跳过`
- `我是新手，继续`

选择写入 `GuidePreference`，同一存档不重复询问。设置页提供“重置战斗引导”，把状态恢复为 `Unset`。老玩家只跳过 GuideAsset 的箭头、遮罩、说明和输入限制，仍完整游玩路线、战斗和奖励。

### 11.2 固定单线地图

教程路线不读取随机种子，没有分支和回头边：

```text
Tutorial.Start（自动占据）
→ Tutorial.Battle.0-1
→ Tutorial.Merchant.0-1
→ Tutorial.Event.0-1
→ Tutorial.Camp.0-1
→ Tutorial.Chest.0-1
→ Tutorial.Boss.0-1
→ Tutorial.Settlement
```

逻辑路线 ID 为 `Route.Tutorial.CombatBasics`。任务 Step 引用 `Encounter.Main.XuXiake.0-1`、`Stage.Tutorial.River` 与语义 Slot `Tutorial.River.EncounterTrigger`；SceneRegistry 才把它解析到当前 SceneProfile。战斗仍进入现有全屏 BattleBoard，并由 `BattleProfile.Tutorial.0-1` 提供站位与镜头。

### 11.3 GuideAsset 步骤

路线图：软提示单线推进，随后强制点击唯一可达的普通战节点。

普通战：

1. 软提示气力、手牌和敌方生命。
2. 强制点击动态目标 `Battle.Hand.FirstPlayableTargetedCard`。
3. 卡牌需要目标时强制点击 `Battle.Enemy.FirstLegalTarget`。
4. 等待结算。
5. 强制点击 `Battle.EndTurn`。
6. 软提示敌方行动、受伤和下一回合。
7. 解除限制，由玩家完成战斗。

商店：软提示上排卡牌强化、下排遗物和普通金币；玩家可购买任意商品或直接离开，不强制消费。首次打开详情或点击离开后完成本段。

事件：软提示选项会产生不同结果；输入限制在当前有效选项内，但不强制具体选项。

休息点：玩家任选“全队各恢复最大生命值 30%（不超过上限）”或“获得 100 局内行旅钱”；不再提供金创药或保命护符。

宝箱：强制点击开启，随后软提示产物进入背包，以及背包满时宝箱不会被消耗。

首领与结算：首领战仅软提示综合运用，不限制出牌；胜利后强制点击结算确认并完成 `Task.Main.XuXiake.Prologue`。

任何强制语义目标缺失时，GuideCoordinator 必须立即解除输入限制、保存错误上下文并允许玩家继续，禁止软锁。

## 12. 常驻商店重做

### 12.1 入口

背包顶部扩为六个按钮，顺序固定为：

`置顶 → 商店 → 声音 → 消息 → 设置 → 退出`

顶部商店按钮与 NPC 交互 Sequence 命令 `openShop` 打开同一个常驻商店。

### 12.2 商品

商店保持七个商品位：

1. 破军装备包。
2. 玄甲装备包。
3. 青囊装备包。
4. 追风装备包。
5. 蚀骨装备包。
6. 山河装备包。
7. 宝石包。

六种套装装备包：

- 每包 10000 普通金币。
- 只从所选套装随机武器、头部、护甲、腰带、鞋子或饰品之一。
- 普通 70%、稀有 25%、珍稀 5%。
- 装备等级等于玩家当前等级。
- 沿用原装备仓库交付；装备仓库满时拒绝购买且不扣金币。

宝石包：

- 每包 5000 普通金币。
- 攻击、防御、生命三类等概率。
- 普通 70%、稀有 25%、珍稀 5%。
- 进入共享背包；同类同品质可堆叠。
- 新堆叠需要格子且背包已满时拒绝购买，不扣金币。

购买采用确定性商店种子与购买序号。扣款和产物入库必须在同一个候选状态中成功后再提交。

### 12.3 伙伴包退役

- `CompanionPack` 的已序列化枚举数值保留但标记为废弃，避免旧存档枚举错位。
- 商品目录不返回伙伴包。
- 购买预览和购买接口明确拒绝废弃商品 ID。
- UI 删除伙伴包说明、头像、结果与替换流程。
- 新 `GemPack` 使用新的枚举值。
- 旧存档保留已拥有伙伴；清除已经无法完成的待招募或待替换订单。存在已扣款但尚未处理的旧伙伴订单时返还原价 500 金币，不生成新伙伴。

## 13. 宝箱品质分布

以下数值均为界面百分比，而不是 0 至 1 的代码小数。代码实现时除以 100。

| 品质 | 普通宝箱 | 高级宝箱 |
|---|---:|---:|
| 普通 | 67.9778% | 46.9112% |
| 稀有 | 25% | 25% |
| 珍稀 | 5% | 20% |
| 传奇 | 2% | 8% |
| 不朽 | 0.02% | 0.08% |
| 至宝 | 0.002% | 0.008% |
| 超凡 | 0.0002% | 0.0008% |

高级宝箱只把珍稀及以上概率乘以四；稀有保持 25%，剩余概率归入普通。每列总和必须精确为 100%。宝箱最高只能直接产出超凡；天界、登神和宇宙只能通过合成获得。

## 14. 九合一品质规则

每次合成都消耗九件同类型、同品质输入并生成一件结果。即使结果保持原品质，也消耗全部九件。

| 输入品质 | 结果概率 |
|---|---|
| 普通 | 100% 稀有 |
| 稀有 | 100% 珍稀 |
| 珍稀 | 100% 传奇 |
| 传奇 | 49.9% 不朽；49.9% 至宝；0.2% 超凡 |
| 不朽 | 49.9% 至宝；49.9% 超凡；0.2% 天界 |
| 至宝 | 49.9% 超凡；49.9% 天界；0.2% 登神 |
| 超凡 | 65.9% 保持超凡；33.9% 天界；0.2% 登神 |
| 天界 | 65.9% 保持天界；33.9% 登神；0.2% 宇宙 |
| 登神 | 75% 保持登神；25% 宇宙 |
| 宇宙 | 品质上限，不允许继续合成 |

装备和宝石使用同一套十阶中文品质语义，但保留各自的枚举类型和产物规则。

## 15. 性能与资源生命周期

- Dialogue、NarrativeSequence 与 Guide 系统空闲时均不 Tick。
- 一次只加载活动 `DialogueAsset`、`NarrativeSequenceAsset` 及当前节点/步骤需要的头像、特效和音效软引用。
- 节点或步骤离开后释放不再需要的临时资源；结束阻塞 NarrativeSequence 后释放整段对话与演出资源。
- 气泡只更新当前可见角色的锚点。
- Sequence 命令调度有单会话上限和无进展保护，阻止错误 JSON 形成无限执行循环。

## 16. 测试与验收

### 16.1 内容编译

- Dialogue、Sequence 与 Guide JSON Schema 的所有校验错误各有失败测试。
- 序章全部节点可达，所有分支最终有明确出口。
- 所有人物、物品、动画、命令、StageContract、SceneProfile、Encounter、BattleProfile、Guide 与语义目标引用存在。

### 16.2 Runner

- 线性推进、真实分支和条件可见性。
- 自动播放、已读跳过和选择暂停。
- 节点中断恢复。
- 对话版本迁移与安全回退。
- Sequence 的 Dialogue Outcome 分支、Pending 等待、步骤中断恢复和无进展保护。
- 重复进入 Sequence 不重复发奖。

### 16.3 UI 与输入

- 气泡跟随、两行预算和阻塞/环境模式。
- 正式对话框、四选项、数字键与鼠标。
- 回看最多一百条。
- 阻塞会话期间移动、`F`、背包与 HUD 均不可用。
- 退出或错误恢复后输入、镜头和 UI 全部恢复。

### 16.4 NPC 交互

- NPC 圆形触发区半径为 300 Unreal Units，边界包含在内，且不会阻挡移动或其他碰撞。
- 只有进入圆形触发区的 NPC 才能被 `F` 选择；玩家朝向不影响结果，按键时不扫描全场。
- 候选按优先级、距离和稳定 ID 排序。
- 多 NPC 重叠时始终只有一个目标。
- 打开 UI 后提示与 `F` 路由暂停。
- NPC 对话中不存在入队或招募入口。

### 16.5 Story、Scene 与 Guide

- 多条 Story、多个 Task 可同时活动，只有一个 TrackedTask。
- DialogueSession 保存完整 Story/Task/Step/Sequence/Dialogue/Node 上下文。
- 新 SceneProfile 缺少 StageContract Slot 时不能启用。
- 切换 SceneRegistry 映射后无需修改剧情 JSON 或重置存档。
- 0-1 触发区进入全屏 BattleBoard，场景位置不泄漏到 BattleProfile。
- 新手/老玩家弹窗每存档只出现一次，设置重置后再次出现。
- Forced/Soft 指引、动态目标、缺失目标解锁和每节点恢复。
- 教程路线节点与边严格匹配固定单线定义。

### 16.6 商店、宝箱与合成

- 商品目录恰好为六套装备包和一个宝石包。
- 伙伴包预览与购买均被拒绝。
- 价格、套装限定、部位和品质权重。
- 宝石类型等概率与前三阶品质权重。
- 背包/仓库满和金币不足时不扣款。
- 普通、高级宝箱每列概率精确为 100%。
- 所有九合一输入的固定随机种子统计与边界值。
- 宇宙品质不能继续合成。

### 16.7 真实流程

从 `/Game/GameXXK/Maps/L_DesktopTrainingHUD` 执行：

1. 点击 `剧情任务`。
2. 进入正确城镇地图。
3. 马车移动、停车、主角生成、马车离开。
4. 马车离开后出现命名。
5. 完成河边卷轴与月白对话的每个节点。
6. 在不同节点退出并重新进入，确认恢复与幂等。
7. 确认月白解锁与主线目标只提交一次。
8. 首次进入教程路线分别选择“老玩家”和“新手”，验证一次性保存与设置重置。
9. 按固定单线经过普通战、商店、事件、休息、宝箱、首领和结算。
10. 验证 Forced/Soft 指引、动态 BattleBoard 目标和缺失目标自动解锁。
11. 验证普通 NPC `F`、任务选项和商店入口。
12. 切换到一份测试 SceneProfile，确认剧情/任务/对话 ID 不变。
13. 返回 2D 工作台后挂机继续运行。

最终必须通过 Editor Target 与 Game Target 冷编译，不使用 Live Coding，并把正确 `L_DesktopTrainingHUD` PIE 留给用户直接验收。

## 17. 实施阶段

1. 对话 JSON Schema、编译器、资产与静态校验。
2. 纯 DialogueRunner、对话会话存档和 v28 迁移。
3. CharacterCatalog、NarrativeSequence Runner/Dispatcher、Story/Task Catalog、StageContract、SceneRegistry/Profile、Encounter/BattleProfile、Guide 状态和 v29 迁移。
4. 气泡、正式纸框、选择、自动、已读跳过、回看与暂停层。
5. NPC 圆形 Overlap `F` 交互与旧 QuestDialog 解耦。
6. 角色、镜头、表现、玩法和 UI 命令适配器。
7. 可替换河边 SceneProfile、马车开场、命名、`Dialogue.Tutorial.001` 和 v30 玩家身份迁移。
8. 固定单线教程路线、GuideAsset、首次新手/老玩家弹窗与设置重置。
9. 背包顶部第六按钮、常驻商店七商品、宝箱概率、九合一规则和 v31 迁移。
10. 自动化、旧存档迁移、SceneProfile 替换验证、真实 PIE、冷编译和用户验收现场。
