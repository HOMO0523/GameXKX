# 战斗意图牌与固定 P 位实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在现有卡牌战斗中交付固定外至内 P 位、三张敌方意图技能牌、逐牌敌方结算、角色脚下血条/护甲/状态与可验证的场景反馈。

**Architecture:** `FGameXXKBattlePresentation` 集中把权威 `UnitId` 映射为展示 P 位；战斗适配器保存和逐张消费真实敌方意图。战斗面板只显示和推进状态机，场景 Presenter 只刷新存活演员的状态，绝不自行计算伤害或重建整场演员。

**Tech Stack:** Unreal Engine 5.8、C++ UMG、Paper2D、Automation Tests、UE MCP/UBT。

---

## 文件结构

- Create: `Source/GameXXK/Public/GameXXKBattlePresentation.h` — P 位映射与显示标签的无状态共享接口。
- Create: `Source/GameXXK/Private/GameXXKBattlePresentation.cpp` — 按角色职责和敌方稳定顺序确定槽位。
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h` — 角色脚下状态条的最小 UMG 接口。
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp` — 血条、护甲、状态徽记的动态 Widget 树。
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h` — 敌方意图的牌面与效果字段。
- Modify: `Source/GameXXK/Public/GameXXKCardText.h` and `Source/GameXXK/Private/GameXXKCardText.cpp` — 所有非敌方卡片的统一悬停文案与交互结果段落。
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h` — 逐张敌方意图 API。
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — 构建、逐张结算、完成敌方阶段与旧全量 API 兼容包装。
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` — 敌方意图牌列、Tooltip 和演出状态机字段。
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` — 顶部牌列、逐牌 `NativeTick` 推进与输入锁定。
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h` and `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp` — 招募和卡组编辑卡片的 Hover/Unhover 绑定与纸张 Tooltip。
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h` — P 位放置和原位刷新接口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp` — 倒八字固定放置和不重建刷新。
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h` — 脚下状态组件、P 位和反馈接口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp` — 从卡牌运行时投影生命/护甲/状态并执行短震。
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` and `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` — 将面板结算结果刷新到已存在的战斗场景，并在我 1P 受击时触发短镜头震动。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp` — 意图、逐张结算和投影测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp` — 静态卡牌、预览卡牌和交互结果 Tooltip 文案测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp` — 顶部牌列、Tooltip、结束回合不跳过敌人演出的测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp` — 卡组编辑与招募卡片的纯 Hover Tooltip 测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` — P 位、血条/护甲/状态与无重建刷新测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp` — 伙伴与临时 NPC 缺席时仍保持我 1P/2P/3P 角色槽规则。

## Task 1: 固定 P 位与逐张敌方意图

**Files:**
- Create: `Source/GameXXK/Public/GameXXKBattlePresentation.h`
- Create: `Source/GameXXK/Private/GameXXKBattlePresentation.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h:10-31`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h:64-76`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:491-536,925-999`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`

- [ ] **Step 1: 写入失败的 P 位与逐张结算测试**

  在 `GameXXKCardBattleAdapterTest.cpp` 新增一个三敌、主角、永久伙伴、任务 NPC 的固定 fixture，并断言尚不存在的 API：

  ```cpp
  TestEqual(TEXT("hero is always player 1P"), FGameXXKBattlePresentation::GetSlotNumber(Runtime, TEXT("Player")), 1);
  TestEqual(TEXT("permanent companion is always player 2P"), FGameXXKBattlePresentation::GetSlotNumber(Runtime, CompanionId), 2);
  TestEqual(TEXT("temporary NPC is always player 3P"), FGameXXKBattlePresentation::GetSlotNumber(Runtime, TEXT("Npc.YueBai")), 3);
  TestEqual(TEXT("enemy stable order maps outer to inner"), FGameXXKBattlePresentation::GetSlotNumber(Runtime, TEXT("Enemy.Three")), 3);

  FGameXXKCardEnemyIntent ResolvedIntent;
  TArray<FGameXXKCardDamageResult> Results;
  bool bIntentsFinished = false;
  TestTrue(TEXT("one adapter call resolves exactly one enemy intent"),
      FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
  TestEqual(TEXT("only first intent was consumed"), State.CardRun.NextEnemyIntentIndex, 1);
  TestEqual(TEXT("enemy phase remains visible until completion"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
  ```

- [ ] **Step 2: 编译并确认测试因为缺少展示模型/API 而失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误明确指出 `GameXXKBattlePresentation` 或 `ResolveNextEnemyIntent` 未定义。

- [ ] **Step 3: 实现唯一的 P 位映射源**

  在 `GameXXKBattlePresentation.h/.cpp` 定义下列公开接口。玩家按职责而非数组下标映射，因此没有伙伴时任务 NPC 仍是我 3P；敌方按 `StableSortOrder + 1` 映射，保证死亡敌人不改变其他敌人的编号。

  ```cpp
  struct FGameXXKBattlePresentationSlot
  {
      FName UnitId = NAME_None;
      EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
      int32 SlotNumber = INDEX_NONE;
  };

  class GAMEXXK_API FGameXXKBattlePresentation
  {
  public:
      static int32 GetSlotNumber(const FGameXXKCardBattleRuntime& Runtime, FName UnitId);
      static FString FormatSlotLabel(EGameXXKCardTargetSide Side, int32 SlotNumber);
      static TArray<FGameXXKBattlePresentationSlot> BuildSlots(const FGameXXKCardBattleRuntime& Runtime);
  };
  ```

  `GetSlotNumber` 对 `Hero` 返回 1、六种永久伙伴职责（`Blade`、`Guard`、`Healer`、`Hunter`、`Sorcerer`、`FormationMaster`）返回 2、`QuestNpc` 返回 3；敌方返回夹紧到 1–3 的 `StableSortOrder + 1`。找不到单位或无效职责返回 `INDEX_NONE`。

- [ ] **Step 4: 让意图成为真实的可展示数据并支持单张消费**

  扩充 `FGameXXKCardEnemyIntent`：

  ```cpp
  UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName CardId = NAME_None;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FString CardDisplayName;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SourceSlotNumber = INDEX_NONE;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 TargetSlotNumber = INDEX_NONE;
  UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TArray<FGameXXKCardStatusStack> OnHitStatuses;
  ```

  在 `BuildEnemyIntents` 中使用 `FGameXXKBattlePresentation` 写入两侧槽位，使用 `Monster.Intent.<EnemyUnitId>` 作为 `CardId`、`攻击` 作为当前规则未提供专属名称时的 `CardDisplayName`，并保留已有 `Damage` 与 `Kind`。在适配器头文件增加：

  ```cpp
  static bool ResolveNextEnemyIntent(FGameXXKRuntimeState& InOutState,
      FGameXXKCardEnemyIntent& OutResolvedIntent,
      TArray<FGameXXKCardDamageResult>& OutDamageResults,
      bool& bOutIntentsFinished, FString* OutError = nullptr);
  static bool CompleteEnemyCardPhase(FGameXXKRuntimeState& InOutState,
      TArray<FGameXXKCardDamageResult>& OutDamageResults, FString* OutError = nullptr);
  ```

  `ResolveNextEnemyIntent` 只递增一次 `NextEnemyIntentIndex`，将 `Intent.OnHitStatuses` 复制到 `FGameXXKCardDamageContext::OnHitStatuses`，同步遗留投影并保持 Enemy phase。`CompleteEnemyCardPhase` 仅在全部意图被消费后调用 `BeginNextPlayerCardRound`，随后清空意图。将现有 `ResolveEnemyPhase` 改为循环这两个 API，保留现有调用者的兼容行为。

- [ ] **Step 5: 编译并运行适配器自动化测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.Integration.CardBattle.Adapter; Quit'
  ```

  Expected: `GameXXK.Integration.CardBattle.Adapter` 通过；测试记录第一张意图后仍是 Enemy phase，三张后才回到 Player phase。

- [ ] **Step 6: 提交仅本任务的源文件与测试**

  ```powershell
  git add -- Source/GameXXK/Public/GameXXKBattlePresentation.h Source/GameXXK/Private/GameXXKBattlePresentation.cpp Source/GameXXK/Public/GameXXKCardRunTypes.h Source/GameXXK/Public/GameXXKCardBattleAdapter.h Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp
  git commit -m "feat(battle): add stable slots and enemy intent steps"
  ```

## Task 2: 顶部敌方意图牌列、Tooltip 与逐牌面板状态机

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h:142-258,380-505`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:250-253,612-645,1280-1360,1580-1790,2777-2779`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: 写入失败的面板测试**

  在现有 `BoardTargeting` fixture 结束出牌前添加三个敌人的 fixture，并新增断言：

  ```cpp
  TestTrue(TEXT("end turn enters presentation instead of resolving all enemies"), Board->EndCardPlayerPhase());
  TestEqual(TEXT("top rail contains every living enemy intent"), Board->GetVisibleEnemyIntentCountForTest(), 3);
  TestEqual(TEXT("first rail card keeps its enemy P label"), Board->GetEnemyIntentSlotLabelForTest(0), FString(TEXT("敌 1P")));
  TestTrue(TEXT("first rail tooltip names target and real damage"),
      Board->GetEnemyIntentTooltipForTest(0).Contains(TEXT("我 1P")) && Board->GetEnemyIntentTooltipForTest(0).Contains(TEXT("伤害")));
  TestEqual(TEXT("no enemy intent is resolved before its presentation tick"), State.CardRun.NextEnemyIntentIndex, 0);
  ```

- [ ] **Step 2: 编译并确认测试因为意图牌列接口缺失而失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误指出上述 `GetVisibleEnemyIntentCountForTest` 等测试接口不存在。

- [ ] **Step 3: 创建三张顶部敌方牌与非拦截 Tooltip**

  在 `NativeConstruct` 动态创建 `EnemyIntentCardButtons[3]`、`EnemyIntentSideLabels[3]`、`EnemyIntentTooltipPanel` 和中央 `EnemyIntentShowcase`。每张按钮调用 `StyleCardButton(Button, true)`，顶部锚点为 `(0.5, 0)`，三张牌的 X 偏移为 `-246`、`-75`、`96`；每张尺寸使用现有 `ReadableHandCardSize`。侧签 `敌 nP` 置于牌左边，不覆盖卡面。

  在头文件增加：

  ```cpp
  void RefreshEnemyIntentCards();
  void RefreshEnemyIntentTooltip();
  FString BuildEnemyIntentTooltip(const FGameXXKCardEnemyIntent& Intent) const;
  void SetEnemyIntentHoverState(int32 SlotIndex, bool bHovered);
  ```

  Tooltip 必须按“攻击者、技能、目标规则/实际目标、基础伤害、护甲吸收、生命伤害、附加状态、触发时机”生成，并设置为 `HitTestInvisible`；无状态时显示 `无附加状态`，不省略效果段落。

- [ ] **Step 4: 替换同步结束回合为逐牌状态机**

  在 `UGameXXKBattleBoardWidget` 增加：

  ```cpp
  enum class EGameXXKEnemyIntentPresentationPhase : uint8 { None, Reveal, Resolve, Settle };
  EGameXXKEnemyIntentPresentationPhase EnemyIntentPresentationPhase = EGameXXKEnemyIntentPresentationPhase::None;
  int32 PresentedEnemyIntentIndex = INDEX_NONE;
  float EnemyIntentPhaseElapsedSeconds = 0.0f;
  void BeginEnemyIntentPresentation();
  void AdvanceEnemyIntentPresentation(float DeltaSeconds);
  bool ResolvePresentedEnemyIntent();
  void CompleteEnemyIntentPresentation();
  ```

  `EndCardPlayerPhase` 只能调用 `EndPlayerCardPhase` 与 `BeginEnemyIntentPresentation`，不得调用 `ResolveEnemyPhase`。`NativeTick` 使用 Reveal `0.55s`、Resolve `0.18s`、Settle `0.32s`；Resolve 阶段调用一次 `ResolveNextEnemyIntent`，最后一张 Settled 后调用 `CompleteEnemyCardPhase`。敌方阶段中 `RefreshActionButtons` 禁用手牌与结束回合。每张展示牌高亮，中央展示区只显示当前敌牌的放大副本。

- [ ] **Step 5: 编译并运行战斗面板自动化测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: 测试通过；结束回合后敌方三牌均可见，第一张结算前索引仍为 0，完整 tick 序列结束后才回到 Player phase。

- [ ] **Step 6: 提交仅面板与其测试**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp
  git commit -m "feat(battle): present enemy intent cards sequentially"
  ```

## Task 3: 全部卡片入口的纯悬停 Tooltip

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardText.h:10-15`
- Modify: `Source/GameXXK/Private/GameXXKCardText.cpp:359-376`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h:250-505`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp:1280-1360,1580-1790,1815-1970`
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h:67-85,213-380`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp:310-337,950-1007,1192-1220`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`

- [ ] **Step 1: 写入失败的统一 Tooltip 文案与 Hover 测试**

  在 `GameXXKCardTextTest.cpp` 增加包含交互结果的 formatter 断言；在两个 Widget 测试中加入只通过 Hover/Unhover 改变可见性的断言：

  ```cpp
  FGameXXKCardTooltipContext RewardContext;
  RewardContext.InteractionResult = TEXT("点击后加入临时路线卡组；已满时需替换一张路线牌。");
  const FString RewardTooltip = GameXXKCardText::DescribeTooltip(*QingFeng, nullptr, RewardContext);
  TestTrue(TEXT("reward tooltip contains card effects"), RewardTooltip.Contains(TEXT("效果：")));
  TestTrue(TEXT("reward tooltip contains its real interaction result"), RewardTooltip.Contains(TEXT("替换一张路线牌")));

  CardButton->OnHovered.Broadcast();
  TestTrue(TEXT("hover alone displays the paper tooltip"), Roster->IsCardTooltipVisibleForTest());
  TestTrue(TEXT("hover does not change the pending card selection"), Roster->GetPendingHeroCardIdsForTest() == PendingBefore);
  CardButton->OnUnhovered.Broadcast();
  TestFalse(TEXT("unhover hides the tooltip"), Roster->IsCardTooltipVisibleForTest());
  ```

  在 `BoardTargeting` 测试中对奖励卡、洞察候选卡和路线替换卡重复相同断言，并断言 tooltip panel 的可见性为 `HitTestInvisible`。

- [ ] **Step 2: 编译并确认测试因为统一 Tooltip API 和 Widget 测试接口缺失而失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误指出 `FGameXXKCardTooltipContext`、`DescribeTooltip` 或 Widget Tooltip 测试接口尚未定义。

- [ ] **Step 3: 用现有 CardText 统一生成卡牌与交互结果文案**

  在 `GameXXKCardText.h` 增加不保存状态的上下文和入口：

  ```cpp
  struct GAMEXXK_API FGameXXKCardTooltipContext
  {
      FString InteractionResult;
      FString UnavailableReason;
  };

  GAMEXXK_API FString DescribeTooltip(const FGameXXKCardDefinition& Definition,
      const FGameXXKCardPlayPreview* Preview,
      const FGameXXKCardTooltipContext& Context);
  ```

  `DescribeTooltip` 必须先复用 `DescribeDetail`，再按顺序追加非空的 `交互结果：` 和 `当前不可操作：` 行。卡组编辑、招募、奖励、替换都只传入实际将发生的交互结果；不能从按钮标题猜测结果。敌方意图继续使用 Task 2 的 `BuildEnemyIntentTooltip`，因为它由待执行敌方意图而非卡牌定义驱动。

- [ ] **Step 4: 将战斗面板的所有卡片入口接入同一纸张 Tooltip**

  保留现有 `HandCardDetailPanel` 的 PSD 纸张与 `HitTestInvisible` 行为，并将其重命名为通用 `CardTooltipPanel` / `CardTooltipBody`。增加：

  ```cpp
  void ShowCardTooltip(FName CardId, const FGameXXKCardPlayPreview* Preview,
      const FGameXXKCardTooltipContext& Context);
  void HideCardTooltip();
  void SetRewardCardHoverState(int32 SlotIndex, bool bHovered);
  void SetPendingChoiceCardHoverState(int32 SlotIndex, bool bHovered);
  void SetRouteReplacementCardHoverState(FName CardId, bool bHovered);
  ```

  为 `UGameXXKPendingChoiceCardButton` 和 `UGameXXKRouteRewardReplacementButton` 增加 `HandleHovered`/`HandleUnhovered`，在各自 `Configure` 内只绑定一次。三张奖励卡也用对应的 slot handler 绑定 `OnHovered`/`OnUnhovered`。Reward Context 写入“点击后加入临时路线卡组；满位时选择要替换的路线牌。”；Pending Choice Context 分别写入“点击后加入手牌。”或“点击后弃置此牌。”；Replacement Context 写入“点击后作为被替换的临时路线牌。”。所有 handler 只调用 `ShowCardTooltip` 或 `HideCardTooltip`，不调用任何选牌、奖励或卡组 mutation。

  删除仅用于默认系统提示的 `SetToolTipText` 调用，确保面板、奖励、洞察和替换卡统一使用可见的 PSD 纸张 Tooltip。

- [ ] **Step 5: 将主角/伙伴卡组编辑和招募卡接入纯 Hover**

  在 `UGameXXKCompanionRosterCardButton::Configure` 中绑定 `OnHovered` 和 `OnUnhovered`，新增：

  ```cpp
  void HandleHovered();
  void HandleUnhovered();
  void ShowCardTooltip(FName CardId, bool bHeroDeckCard);
  void HideCardTooltip();
  bool IsCardTooltipVisibleForTest() const;
  FString GetCardTooltipTextForTest() const;
  ```

  `UGameXXKCompanionRosterWidget` 创建一个现有 WindowFrameTexturePath 纸张风格的 `UBorder` 和 `UTextBlock`，并置为 `HitTestInvisible`。主角卡显示“点击后编入/移出主角牌组；需保持 8 张”；伙伴卡显示“点击后编入/移出该伙伴个人牌组；需保持 5 张”。对于锁定卡，额外传入 `当前不可操作：本次路线已锁定。` 或 `当前不可操作：该牌尚未解锁。`。招募候选若以卡片按钮呈现，使用同一 `ShowCardTooltip`，交互结果明确为“点击招募；满员时替换当前选中的伙伴。”。

- [ ] **Step 6: 编译并运行 Tooltip 相关自动化测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.Integration.CardText+GameXXK.Integration.CardBattle.BoardTargeting+GameXXK.UI.CompanionRoster; Quit'
  ```

  Expected: 三类测试通过；每个测试的 Tooltip 仅在 Hover 可见，Unhover 后隐藏，且 Hover 前后卡组、奖励、招募状态没有变化。

- [ ] **Step 7: 提交 Tooltip 代码与测试**

  ```powershell
  git add -- Source/GameXXK/Public/GameXXKCardText.h Source/GameXXK/Private/GameXXKCardText.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp
  git commit -m "feat(ui): show card details on hover everywhere"
  ```

## Task 4: 倒八字场景、脚下状态条与不重建刷新

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h:12-49`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp:22-135`
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h:11-112`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp:119-354`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h:225-229`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:795-850`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp`

- [ ] **Step 1: 写入失败的场景投影测试**

  在 `GameXXKBattleSceneActorTest.cpp` 构造主角、永久伙伴与 `Npc.YueBai`，给任务 NPC `Armor = 7`、`Poison = 2`，并断言：

  ```cpp
  TestEqual(TEXT("hero is placed in outer 1P"), HeroPlacement->SlotNumber, 1);
  TestEqual(TEXT("companion is placed in middle 2P"), CompanionPlacement->SlotNumber, 2);
  TestEqual(TEXT("temporary NPC is placed in inner 3P"), NpcPlacement->SlotNumber, 3);
  TestTrue(TEXT("party row opens toward the central card space"), HeroPlacement->Location.X < CompanionPlacement->Location.X && CompanionPlacement->Location.X < NpcPlacement->Location.X);
  TestEqual(TEXT("card armor projects to legacy shield"), State.ActiveBattleParty[2].Shield, 7);
  TestTrue(TEXT("status widget exposes poison stack"), NpcActor->GetStatusTextForTest().Contains(TEXT("毒 2")));
  ```

  在 `GameXXKCardRouteEventSupportTest.cpp` 增加“无永久伙伴但有任务 NPC”的 case，断言任务 NPC 的 `SlotNumber == 3`，主角仍为 1，且没有创建 2P 的假伙伴。

- [ ] **Step 2: 编译并确认测试因为槽位/状态条接口不存在而失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误指出 `SlotNumber`、`GetStatusTextForTest` 或状态 Widget 不存在。

- [ ] **Step 3: 实现脚下血条、护甲与状态 Widget**

  用一个独立的 `UGameXXKBattleUnitStatusWidget` 建立动态 `UProgressBar`、名称文本、护甲文本和状态文本，公开：

  ```cpp
  void SetUnitStatus(FText SlotAndName, int32 CurrentHP, int32 MaxHP,
      int32 Armor, const TArray<FGameXXKCardStatusStack>& Statuses);
  FString GetStatusTextForTest() const;
  ```

  状态文本按固定映射输出 `毒`、`流`、`灼`、`蚀`、`易伤`、`闪`、`护`；未识别的有效状态输出其枚举名和层数。`AGameXXKBattleSceneUnitActor` 新增 `UWidgetComponent`，以 `World` 空间附着到根组件脚下，尺寸 `260 x 72`，并保存 `DisplaySlotNumber`、`CurrentArmor` 与 `CurrentStatuses`。`ConfigureFromRuntimeUnit` 后从 `CardRun.ActiveBattle.Units` 以 `UnitId` 拉取权威 Armor 和 Statuses。

  在 `SyncCardBattleToLegacyProjection` 设置：

  ```cpp
  LegacyUnit.Shield = FMath::Clamp(CardUnit->Armor, 0, 99);
  ```

- [ ] **Step 4: 固定倒八字位置并原位刷新演员**

  向 `FGameXXKBattleSceneUnitPlacement` 添加 `int32 SlotNumber`；`BuildUnitPlacementsForState` 使用 `FGameXXKBattlePresentation` 而非数组数量计算。采用以下镜像坐标，外侧 P1 最大程度远离中央、P3 最接近中央：

  ```cpp
  // enemy P1/P2/P3
  { FVector(-190.0f, -330.0f, 90.0f), FVector(-70.0f, -260.0f, 90.0f), FVector(45.0f, -190.0f, 90.0f) }
  // party P1/P2/P3
  { FVector(-190.0f, 330.0f, 90.0f), FVector(-70.0f, 260.0f, 90.0f), FVector(45.0f, 190.0f, 90.0f) }
  ```

  新增 `bool RefreshBattleScene()`：若现有 `SpawnedUnitObjects` 的 `UnitId` 集合与 placements 相同，更新位置与 `ConfigureFromRuntimeUnit`，不调用 `ClearSpawnedUnits`；集合变化时才调用 `EnsureBattleScene`。面板每次单牌 Resolve/Settle 后通过 PlayerController 调用该刷新方法。

- [ ] **Step 5: 实现攻击者/受击反馈和主角轻震屏**

  在单位 Actor 增加 `PlayIntentAttackFeedback()` 和 `PlayHitFeedback()`。启用 actor tick 仅在 0.18 秒动画窗口内，以初始位置为基准沿局部 X 轴作 `+6/-6/+3/0` 的短震，结束后还原位置。PlayerController 添加 `PlayHeroCardHitFeedback()`，使用 0.12 秒、极小角度的 `UMatineeCameraShake`；只在结果目标的 P 位为我 1P 时调用。每个 Resolve 后先触发攻击者，再触发伤害结果中的目标。

- [ ] **Step 6: 编译并运行场景/事件自动化测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardRoute.EventSupport; Quit'
  ```

  Expected: 两个测试通过；测试输出显示任务 NPC 即使没有伙伴也固定为我 3P，护甲和毒层同步到脚下 Widget，刷新不会改变演员数量。

- [ ] **Step 7: 提交仅场景投影与其测试**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp Source/GameXXK/Public/MVP/GameXXKBattleScenePresenter.h Source/GameXXK/Private/MVP/GameXXKBattleScenePresenter.cpp Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp
  git commit -m "feat(battle): refresh fixed formation status feedback"
  ```

## Task 5: 全量验证与真实 PIE 验收

**Files:**
- Modify: `docs/production/` only if an existing acceptance record for this battle flow is found and requires the new acceptance criteria.
- Test: `scripts/ue_tdd_pipeline.py`
- Test: `scripts/gamexxk_real_play_flow_mcp.py`

- [ ] **Step 1: 运行全部受影响的 Automation 测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI -ExecCmds='Automation RunTests GameXXK.Integration.CardBattle+GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardRoute.EventSupport; Quit'
  ```

  Expected: 所有匹配测试通过，没有 assertion、无效 `UnitId` 或阶段未完成错误。

- [ ] **Step 2: 在编辑器关闭后执行正常 UBT 验证**

  Run:

  ```powershell
  python scripts/ue_tdd_pipeline.py --no-launch
  ```

  Expected: 流程先安全确认编辑器状态，再完成非 Live Coding 编译；输出包含 `Compile succeeded`。

- [ ] **Step 3: 通过 UE MCP 启动并验证可玩流程**

  Run:

  ```powershell
  python scripts/ue_tdd_pipeline.py --pie-duration 10
  python scripts/gamexxk_real_play_flow_mcp.py
  ```

  Expected: 从主菜单进入城镇、路线并进入战斗；手牌可悬停和指向目标；结束回合后顶部敌方意图牌逐张可见、P 位正确、角色脚下生命/护甲/状态同步，最后回到下一玩家回合。

- [ ] **Step 4: 记录人工 PIE 观察并提交验收记录（如存在对应生产记录）**

  依次确认：我 1P 是最外侧主角、我 2P 是中段永久伙伴、我 3P 是内侧临时 NPC；敌方 P 位按外至内；顶部仅有敌方意图牌；悬停任一意图牌能读到攻击者、目标、伤害、护甲、状态和触发时机；敌方逐牌震动/受击/主角轻震屏均可见。仅在已存在并明确覆盖战斗流程的 `docs/production` 验收记录中追加这些结果。

- [ ] **Step 5: 提交验收记录（若 Task 5 Step 4 改动了记录）**

  ```powershell
  git add -- docs/production
  git commit -m "test(battle): record intent presentation acceptance"
  ```

## 计划自检

- 规格覆盖：Task 1 覆盖 P 位、意图真实数据与逐张结算；Task 2 覆盖顶部敌方三牌、中央展示与输入锁定；Task 3 覆盖所有卡片入口的纯 Hover Tooltip；Task 4 覆盖倒八字、伙伴/NPC、血条、护甲、状态、反馈与无重建刷新；Task 5 覆盖编译、自动化和 PIE。
- 类型一致性：所有 UI 以 `FGameXXKCardEnemyIntent`、`FGameXXKCardDamageResult` 和 `FGameXXKBattlePresentation` 为数据边界；权威伤害仍仅在卡牌规则/适配器计算。
- 范围：不新增主线、招募、存档格式或替换用户已调过的地图、PaperZD、角色资源；仅在缺失牌面资源时使用现有 PSD 卡框。
