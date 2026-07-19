# 战斗 Actor 资源 HUD 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让每个战斗角色 Actor 以唯一、可读、屏幕稳定的脚底 HUD 显示气血、个人气力、护甲与状态；怪物隐藏气力，Battle Board 不再绘制重复脚底 HUD。

**Architecture:** `AGameXXKBattleSceneUnitActor` 新增语义化脚底锚点，并持有唯一的 Screen-space `UWidgetComponent`。Actor 按 `UnitId` 从 `CardRun.ActiveBattle.Units` 读取 HP、Mana、Armor 和 Statuses，只有卡牌战斗尚未初始化时才回退到 legacy runtime unit。`UGameXXKBattleBoardWidget` 保留手牌、指向箭头、顶部敌方意图与奖励展示，删除它的脚底 HUD 投影和 Widget 生命周期。

**Tech Stack:** Unreal Engine 5.8、C++、UMG、Paper2D、Automation Tests、UE MCP、冷 UBT 编译。

---

## 文件结构

- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h` — 资源 HUD 输入契约、可测试的文本/百分比/可见性接口。
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp` — PSD 风格双资源行、可读标签、放大的单色水墨状态徽记布局与原生 UMG 命中路由。
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h` — Actor-owned `HudAnchorComponent`、资源缓存与测试读取口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp` — 从 Card runtime 读取权威快照、屏幕组件配置、脚底锚点刷新。
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` — 删除 Board 侧脚底 HUD 公共/私有 API 与缓存。
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` — 删除重复 footer 的创建、投影、刷新与测试辅助实现；保留卡牌目标坐标图。
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` — 删除 Screen HUD 不再使用的 `UWidgetInteractionComponent` 声明和测试入口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — 停止 footer 投影与世界射线 hover bridge；保留目标箭头与 3D `HitArea` 选取。
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` — Widget 文案、Actor 组件归属、权威资源刷新、角色/怪物可见性与原生 UMG 路由测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp` — 删除旧 footer 断言，锁定 Board 不再创建它且箭头/意图卡仍正常。
- Reference only: `Source/GameXXK/Public/GameXXKCardTypes.h` — `FGameXXKCardCombatUnit` 的 `HP/MaxHP/Mana/MaxMana/Armor/Statuses` 权威字段。
- Reference only: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` — 现有 legacy 兼容投影；本计划不修改卡牌费用或共享手牌能量规则。

## 固定契约

| 情形 | HUD 行 | 数据源 |
| --- | --- | --- |
| 主角、永久伙伴、临时任务 NPC | `气血 当前 / 最大`、`气力 当前 / 最大`、护甲/状态 | 同一 `FGameXXKCardCombatUnit` |
| 普通怪物、黑熊、老虎 Boss | `气血 当前 / 最大`、护甲/状态 | 同一 `FGameXXKCardCombatUnit`；强制隐藏气力 |
| 卡牌战斗尚未初始化 | 与上面相同的资源格式 | 当前 legacy `FGameXXKBattleRuntimeUnit` 回退值 |
| 共享出牌能量 | 不属于单位 HUD | 仍由手牌区管理，绝不写入蓝色气力行 |

### Task 1: 双资源状态 Widget 与 PSD 资源行

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 先写会失败的资源文案与可见性测试**

  在 `GameXXKBattleSceneActorTest.cpp` 加入 `#include "UI/GameXXKBattleUnitStatusWidget.h"`，并在现有 `GameXXK.MVP.Battle.SceneActors` 测试的独立区块加入以下代码。它要求尚不存在的气力参数和读取接口，因此当前编译必须失败。

  ```cpp
  UGameXXKBattleUnitStatusWidget* ResourceWidget = NewObject<UGameXXKBattleUnitStatusWidget>();
  TestTrue(TEXT("resource widget builds its native runtime tree"),
      ResourceWidget->PrepareForScreenSpaceEmbedding());

  ResourceWidget->SetUnitStatus(
      TEXT("我 1P"), FText::FromString(TEXT("主角")),
      72, 100, 18, 30, true,
      3, {FGameXXKCardStatusStack{EGameXXKCardStatus::Bleed, 2}});
  TestEqual(TEXT("health always includes its Chinese label and both values"),
      ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
  TestEqual(TEXT("party qi always includes its Chinese label and both values"),
      ResourceWidget->GetQiDisplayTextForTest(), FString(TEXT("气力 18 / 30")));
  TestTrue(TEXT("health fill uses current divided by maximum"),
      FMath::IsNearlyEqual(ResourceWidget->GetHealthPercentForTest(), 0.72f));
  TestTrue(TEXT("qi fill uses the unit's own current divided by maximum"),
      FMath::IsNearlyEqual(ResourceWidget->GetQiPercentForTest(), 0.60f));
  TestTrue(TEXT("party resource widget exposes the qi row"), ResourceWidget->IsQiRowVisibleForTest());

  ResourceWidget->SetUnitStatus(TEXT("敌 1P"), FText::FromString(TEXT("黑熊")),
      240, 240, 99, 100, false, 0, {});
  TestEqual(TEXT("enemy health remains readable when qi is hidden"),
      ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 240 / 240")));
  TestFalse(TEXT("enemy qi row is hidden even with nonzero runtime mana"),
      ResourceWidget->IsQiRowVisibleForTest());
  ```

- [ ] **Step 2: 冷编译并确认测试接口尚不存在**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误指向扩展后的 `SetUnitStatus` 参数或 `GetQiDisplayTextForTest`、`GetQiPercentForTest`、`IsQiRowVisibleForTest` 未定义；不得使用 Live Coding 或 Hot Reload 绕过该失败。

- [ ] **Step 3: 实现单一资源显示契约和 PSD 风格双行布局**

  在 `GameXXKBattleUnitStatusWidget.h` 将输入契约固定为以下顺序，并添加只读测试口；不要暴露 `UTextBlock*` 或 `UProgressBar*` 给测试。

  ```cpp
  void SetUnitStatus(const FString& InSlotLabel, const FText& InDisplayName,
      int32 InCurrentHP, int32 InMaxHP,
      int32 InCurrentMana, int32 InMaxMana, bool bInShowQi,
      int32 InArmor, const TArray<FGameXXKCardStatusStack>& InStatuses);

  FString GetHealthDisplayTextForTest() const;
  FString GetQiDisplayTextForTest() const;
  float GetHealthPercentForTest() const;
  float GetQiPercentForTest() const;
  bool IsQiRowVisibleForTest() const;
  static ESlateVisibility GetRootHitTestVisibilityForTest();
  static ESlateVisibility GetStatusBadgeHitTestVisibilityForTest();
  ```

  新增 `QiRow`、`QiBar`、`QiText` 缓存字段和 `CurrentMana`、`MaxMana`、`bShowQi` 值。`RefreshDisplay()` 必须使用下列完整逻辑，保证值、百分比与 visibility 永远来自同一快照：

  ```cpp
  const int32 SafeMaxHP = FMath::Max(1, MaxHP);
  const int32 SafeMaxMana = FMath::Max(1, MaxMana);
  HealthBar->SetPercent(FMath::Clamp(static_cast<float>(CurrentHP) / SafeMaxHP, 0.0f, 1.0f));
  HealthText->SetText(FText::FromString(FString::Printf(TEXT("气血 %d / %d"), CurrentHP, MaxHP)));
  QiRow->SetVisibility(bShowQi ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
  QiBar->SetPercent(FMath::Clamp(static_cast<float>(CurrentMana) / SafeMaxMana, 0.0f, 1.0f));
  QiText->SetText(FText::FromString(FString::Printf(TEXT("气力 %d / %d"), CurrentMana, MaxMana)));
  ```

  `EnsureWidgetTree()` 中将每个标签置于其对应 bar 的上方：身份行、`HealthText`、`HealthBar`、`QiRow(QiText + QiBar)`、状态图标行。健康 bar 背景使用 `/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterHealthFrame.T_TownPsd_CharacterHealthFrame`，气力 bar 背景使用 `/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterManaFrame.T_TownPsd_CharacterManaFrame`；填充色固定为低饱和朱红 `FLinearColor(0.62f, 0.25f, 0.22f, 1.0f)` 和墨青蓝 `FLinearColor(0.24f, 0.43f, 0.56f, 1.0f)`，不新增高对比贴图。资源框纹理缺失时沿用现有纸色 fallback，不能让 bar 空白。

  保持 `RootBox` 和非交互行 `SelfHitTestInvisible`，将状态 icon 的 `InkOutline` visibility 统一从 `GetStatusBadgeHitTestVisibilityForTest()` 获取并返回 `Visible`，Tooltip 内容继续返回 `HitTestInvisible`。将每个 status `USizeBox` 从 `30×30` 调整为 `38×38`、glyph 从 `19` 调整为 `24`、层数文字从 `10` 调整为 `12`，但继续使用已有低饱和单色水墨图标资源与 Tooltip 文案。

- [ ] **Step 4: 运行 Widget 所在自动化测试并确认通过**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors; Quit'
  ```

  Expected: `GameXXK.MVP.Battle.SceneActors` 通过；日志包含气血/气力文案和百分比断言，无 UMG widget-tree 或 texture-load assertion。

- [ ] **Step 5: 提交资源 Widget 与其测试**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
  git commit -m "feat(battle): add actor resource HUD rows"
  ```

### Task 2: Actor 组件归属、权威资源快照与脚底锚点

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入会失败的 Actor 归属与权威数据测试**

  在 `BuildFixedSlotSceneBattleState()` 已有的主角、永久伙伴、`Npc.YueBai`、敌方 fixture 内，将 `CardRun.ActiveBattle.Units` 的资源值设为主角 `72/100, 18/30`、伙伴 `55/80, 9/16`、NPC `31/60, 6/12`、敌人 `240/240, 99/100`；同时故意将对应 legacy `ActiveBattleParty/ActiveBattleEnemies` 的 HP/MP 写成不同数值。对每个 Actor 调用 `SetMVPSubsystemForTest()` 和 `ConfigureFromRuntimeUnit()` 后加入：

  ```cpp
  TestEqual(TEXT("status widget is a screen component owned by the actor"),
      HeroActor->GetStatusWidgetComponentForTest()->GetWidgetSpace(), EWidgetSpace::Screen);
  TestEqual(TEXT("status widget attaches to the semantic HUD anchor"),
      HeroActor->GetStatusWidgetComponentForTest()->GetAttachParent(), HeroActor->GetHudAnchorComponentForTest());
  TestEqual(TEXT("hero reads HP directly from the card runtime"), HeroActor->GetCurrentHealthForTest(), 72);
  TestEqual(TEXT("hero reads Mana directly from the card runtime"), HeroActor->GetCurrentManaForTest(), 18);
  TestTrue(TEXT("hero exposes individual qi"), HeroActor->ShouldShowQiForTest());
  TestTrue(TEXT("permanent companion exposes individual qi"), CompanionActor->ShouldShowQiForTest());
  TestTrue(TEXT("temporary support NPC exposes individual qi"), NpcActor->ShouldShowQiForTest());
  TestFalse(TEXT("enemy hides qi regardless of its card-runtime mana"), EnemyActor->ShouldShowQiForTest());
  ```

  再只修改主角 `FGameXXKCardCombatUnit` 为 `HP=55`、`Mana=7`、`Armor=3`、`Statuses={Bleed,2}`，故意不调用 `SyncCardBattleToLegacyProjection`，以旧的 legacy unit 重调同一 Actor：

  ```cpp
  HeroActor->ConfigureFromRuntimeUnit(false, 0, StaleLegacyHero, 1);
  TestEqual(TEXT("retained actor refreshes immediate card-runtime health"), HeroActor->GetCurrentHealthForTest(), 55);
  TestEqual(TEXT("retained actor refreshes immediate card-runtime qi"), HeroActor->GetCurrentManaForTest(), 7);
  TestEqual(TEXT("retained actor refreshes immediate card-runtime armor"), HeroActor->GetArmorForTest(), 3);
  TestTrue(TEXT("retained actor refreshes immediate card-runtime debuff"),
      HeroActor->GetStatusTextForTest().Contains(TEXT("流 2")));
  ```

  删除当前把 `StatusWidgetComponent` 锁死为 `World`、`QueryOnly`、`ECC_Visibility`、`TwoSided` 的断言；这些断言必须先让当前实现失败，而不是保留过时架构。

- [ ] **Step 2: 冷编译并确认新 Actor 测试失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
  ```

  Expected: 编译失败，错误指向 `GetHudAnchorComponentForTest`、资源 getter 或 `ShouldShowQiForTest` 不存在；旧 Actor 仍会读取 legacy HP 且使用 World widget。

- [ ] **Step 3: 将可见 HUD 完全收回 Actor，并从 Card runtime 解析快照**

  在 Actor 头文件加入一个可见的 `USceneComponent* HudAnchorComponent`、`CurrentMana`、`MaxMana`、`bShowQi`，以及以下无状态读取口：

  ```cpp
  USceneComponent* GetHudAnchorComponentForTest() const;
  int32 GetCurrentHealthForTest() const;
  int32 GetMaxHealthForTest() const;
  int32 GetCurrentManaForTest() const;
  int32 GetMaxManaForTest() const;
  bool ShouldShowQiForTest() const;

  void ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit);
  void RefreshHudAnchor();
  ```

  在构造函数用以下组件关系替换固定 `(-76, 0, -82)` 的世界空间 footer。Screen widget 不建立 `ECC_Visibility` collision，也不保留 `SetTwoSided(true)`：

  ```cpp
  HudAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HudAnchor"));
  HudAnchorComponent->SetupAttachment(SceneRoot);
  StatusWidgetComponent->SetupAttachment(HudAnchorComponent);
  StatusWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
  StatusWidgetComponent->SetDrawSize(FIntPoint(300, 132));
  StatusWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
  StatusWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  StatusWidgetComponent->SetGenerateOverlapEvents(false);
  ```

  `ConfigureFromRuntimeUnit()` 先保存 UnitId/阵营/显示名，再调用 `ResolveCardRuntimePresentation(Unit)`。该函数必须先写 legacy fallback，再用同 UnitId 的 `CardRun.ActiveBattle.Units` 完整覆盖资源，而不是只覆盖 armor/status：

  ```cpp
  CurrentHP = FMath::Clamp(LegacyUnit.HP, 0, FMath::Max(1, LegacyUnit.MaxHP));
  MaxHP = FMath::Max(1, LegacyUnit.MaxHP);
  CurrentMana = FMath::Max(0, LegacyUnit.MP);
  MaxMana = FMath::Max(0, LegacyUnit.MaxMP);
  CurrentArmor = FMath::Max(0, LegacyUnit.Shield);
  CurrentStatuses.Reset();

  UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(nullptr);
  if (Subsystem && Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
  {
      const FGameXXKCardBattleRuntime& Runtime = Subsystem->GetRuntimeState().CardRun.ActiveBattle;
      const FGameXXKCardCombatUnit* CardUnit = Runtime.Units.FindByPredicate([this](const FGameXXKCardCombatUnit& Candidate)
      {
          return Candidate.UnitId == UnitId;
      });
      if (CardUnit)
      {
          MaxHP = FMath::Max(1, CardUnit->MaxHP);
          CurrentHP = FMath::Clamp(CardUnit->HP, 0, MaxHP);
          MaxMana = FMath::Max(0, CardUnit->MaxMana);
          CurrentMana = FMath::Clamp(CardUnit->Mana, 0, MaxMana);
          CurrentArmor = FMath::Max(0, CardUnit->Armor);
          CurrentStatuses = CardUnit->Statuses;
      }
  }
  bShowQi = !bEnemy && MaxMana > 0;
  ```

  `RefreshHudAnchor()` 在 `RefreshVisual()` 之后、反馈 tick 和恢复之后调用。它以现有 `BattleVisual->Bounds` 的下沿作为语义脚点，而不是使用固定世界坐标：

  ```cpp
  const FVector Foot = BattleVisual->Bounds.Origin - FVector(0.0f, 0.0f, BattleVisual->Bounds.BoxExtent.Z);
  HudAnchorComponent->SetWorldLocation(Foot + FVector(0.0f, 0.0f, 8.0f));
  ```

  `RefreshStatusWidget()` 把 `CurrentHP/MaxHP/CurrentMana/MaxMana/bShowQi/CurrentArmor/CurrentStatuses` 按 Task 1 的固定顺序传入。若 `bDefeated || CurrentHP <= 0`，折叠 `StatusWidgetComponent`；存活单位显示它。暂时保留 `GetBattleFooterWorldLocation()`，直到 Task 3 先删除它在 Board/Controller 中的最后一个调用方后再一并移除。

- [ ] **Step 4: 运行 Actor 自动化测试并确认权威刷新通过**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors; Quit'
  ```

  Expected: 主角、永久伙伴、任务 NPC 的 `ShouldShowQiForTest()` 为 true，敌人为 false；同一 retained actor 在 legacy 未同步时仍读取 Card runtime 的新 HP/Mana/Armor/流血层数。

- [ ] **Step 5: 提交 Actor HUD 所有权与权威快照**

  ```powershell
  git add -- Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
  git commit -m "feat(battle): attach authoritative HUD to unit actors"
  ```

### Task 3: 删除 Battle Board 的重复脚底 HUD，而不破坏箭头或意图牌

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: 写入会失败的“Board 不拥有脚底 HUD”测试**

  在 `GameXXKCardBattleBoardWidgetTest.cpp` 删除原来的 `RegisterBattleUnitFooterScreenPosition`、`IsBattleUnitFooterVisibleForTest`、`GetBattleUnitFooterPositionForTest`、`GetBattleUnitFooterBadgeCountForTest` 断言块。保留 `Board->RefreshFromState()`，紧接着加入：

  ```cpp
  TestNull(TEXT("battle board never creates a duplicate unit foot HUD"),
      Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleUnitFooter_00")) : nullptr);
  ```

  旧实现会在 `RefreshBattleUnitFooters(true)` 中创建名为 `BattleUnitFooter_00` 的 native widget，即使尚未注册其屏幕坐标，所以该断言会在当前行为下失败。保留紧随其后的 `RegisterBattleUnitScreenPosition`、手牌预览、箭头起点、目标高亮、敌方意图牌与 hover Tooltip 断言。

- [ ] **Step 2: 运行 Board 测试并确认它因重复 footer 失败**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: `battle board never creates a duplicate unit foot HUD` 失败；箭头和顶部敌方意图牌的现有断言仍可运行。

- [ ] **Step 3: 删除 Footer API、缓存和刷新路径，仅保留卡牌目标投影**

  从 `GameXXKBattleBoardWidget.h/.cpp` 删除以下完整 footer 通路：

  ```cpp
  void RegisterBattleUnitFooterScreenPosition(FName UnitId, FVector2D ScreenPosition);
  void ClearBattleUnitFooterScreenPositions();
  void RefreshBattleUnitFooters(bool bRefreshContent);
  UGameXXKBattleUnitStatusWidget* FindOrCreateBattleUnitFooter(FName UnitId);
  FVector2D ResolveBattleUnitFooterPosition(FVector2D UnitScreenPosition) const;
  TMap<FName, FVector2D> RegisteredBattleUnitFooterScreenPositions;
  TMap<FName, TObjectPtr<UGameXXKBattleUnitStatusWidget>> BattleUnitFooterWidgets;
  ```

  同时删除 footer 尺寸常量、`UI/GameXXKBattleUnitStatusWidget.h` include、footer 测试辅助方法和 `RefreshProgrammaticLayout()` 末尾的 `RefreshBattleUnitFooters(true)` 调用。不要删除 `RegisteredBattleUnitScreenPositions`、`RegisterBattleUnitScreenPosition()` 或 `ClearBattleUnitScreenPositions()`；它们仍是目标箭头从打牌角色出发的唯一坐标源。

  从 `AGameXXKMVPPlayerController::RefreshBattleCardTargetingBridge()` 删除下列 footer 投影代码，但保留 `BattleVisual->Bounds.Origin` 到 `RegisterBattleUnitScreenPosition()` 的投影和 `SetCardTargetHighlight()`：

  ```cpp
  BattleBoardWidget->ClearBattleUnitFooterScreenPositions();
  FVector2D UnitFooterScreenPosition;
  if (UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
          this, UnitActor->GetBattleFooterWorldLocation(), UnitFooterScreenPosition, true))
  {
      BattleBoardWidget->RegisterBattleUnitFooterScreenPosition(UnitActor->GetUnitId(), UnitFooterScreenPosition);
  }
  ```

  先执行 `rg -n "RegisterBattleUnitFooterScreenPosition|ClearBattleUnitFooterScreenPositions|GetBattleFooterWorldLocation" Source Config`，确认不存在残留 C++ 调用；随后删除 Actor 的 `GetBattleFooterWorldLocation()` 声明和实现。冷启动 Automation 时若有 Blueprint 引用，日志必须明确报出该引用；在没有引用的构建上不得留下无效 no-op API。

- [ ] **Step 4: 运行 Board 回归测试，确认箭头和意图牌保持正常**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: Board 不再含 `BattleUnitFooter_00`；手牌悬停 Tooltip、指向箭头起点/跟随鼠标、合法目标高亮、顶部 `敌 1P` 意图卡和意图 Tooltip 全部通过。

- [ ] **Step 5: 提交 Board 去重与回归测试**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp
  git commit -m "refactor(battle): remove duplicate board foot HUD"
  ```

### Task 4: 用原生 Screen UMG 保留状态 Tooltip 与角色目标点击

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入会失败的 Screen UMG 路由测试**

  在 Actor 测试中保留 `#include "Components/WidgetInteractionComponent.h"`，并用组件查询替换旧 `GetBattleStatusHoverInteractionComponentForTest()` 断言；`HeroActor` 复用 Task 2 的权威 fixture Actor：

  ```cpp
  AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
  TestNull(TEXT("screen status HUD has no obsolete world-trace hover bridge"),
      Controller->FindComponentByClass<UWidgetInteractionComponent>());
  TestEqual(TEXT("screen status component has no trace collision"),
      HeroActor->GetStatusWidgetComponentForTest()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
  TestEqual(TEXT("noninteractive HUD root allows a target click to pass through"),
      UGameXXKBattleUnitStatusWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
  TestEqual(TEXT("a status icon itself remains hoverable"),
      UGameXXKBattleUnitStatusWidget::GetStatusBadgeHitTestVisibilityForTest(), ESlateVisibility::Visible);
  TestEqual(TEXT("tooltip contents do not intercept the pointer"),
      UGameXXKBattleUnitStatusWidget::GetStatusTooltipVisibilityForTest(), ESlateVisibility::HitTestInvisible);
  ```

  现有 Controller 构造函数仍创建 `BattleStatusHoverInteraction`，因此第一个断言必须失败；第二个断言锁定 Task 2 已完成的 Screen 组件无 collision 配置，防止后续改动回退。

- [ ] **Step 2: 编译并运行测试，确认旧世界 hover bridge 被捕获**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors; Quit'
  ```

  Expected: 自动化测试编译成功后运行时断言失败，原因是旧 `UWidgetInteractionComponent` 仍存在或组件仍带世界 collision；不得用虚假的 `SetActive(false)` 代替删除。

- [ ] **Step 3: 删除世界射线 bridge，并让 Screen 层处理状态徽记 hover**

  从 Controller 头文件、构造函数、`EnsureBattleScenePresenter()` 和测试 getter 中删除所有 `BattleStatusHoverInteraction` / `UWidgetInteractionComponent` 代码；不得保留未激活的组件。保留 `RefreshBattleCardTargetingBridge()` 中的 actor `HitArea` 查询、`FindAnyBattleSceneUnitUnderCursor()`、卡牌箭头和 `ConfirmTargetingUnit()`，因为它们处理的是卡牌目标而非 UMG Tooltip。

  Actor 的 status widget root 保持 `SelfHitTestInvisible`，只有 `InkOutline` 徽记调用 `SetVisibility(ESlateVisibility::Visible)` 并携带 Tooltip。这样 Screen `UWidgetComponent` 由 UE 的 game-layer 投影处理 native UMG hover；未点在图标上的部分不会截获鼠标，卡牌指向仍能落到角色 `HitArea`。

- [ ] **Step 4: 运行 Actor 与 Board 两条自动化回归**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: Screen HUD 路由断言通过；卡牌目标箭头、取消、合法目标提交和敌方意图牌断言仍全部通过。

- [ ] **Step 5: 提交原生 hover 路由迁移**

  ```powershell
  git add -- Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
  git commit -m "refactor(battle): use native screen HUD hover"
  ```

### Task 5: 冷编译、Automation 与真实 PIE 验收

**Files:**
- Create at runtime only: `Saved/HarnessReports/battle-actor-resource-hud.json`
- Create at runtime only: `Saved/Codex/real_flow_after_battle.png`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: 用安全保存、关闭、冷编译和 PIE 管线验证最终二进制**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\ue_tdd_pipeline.py --pie-duration 8 --log-lines 400
  ```

  Expected: 管线先通过 UE MCP 保存 dirty packages，再关闭编辑器、执行 `-NoHotReload` 冷编译、重新用项目路径启动编辑器、进入并退出 PIE；输出包含 `[BUILD] Compile succeeded`，没有 `Assertion failed`、Landscape assertion 或 `dotnet.exe` 异常。

- [ ] **Step 2: 在最终二进制上运行两条受影响 Automation 测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: 两个 Automation 名称均通过；没有 legacy snapshot 读取、Widget tree、丢失 footer API 或无效 hover bridge 的日志错误。

- [ ] **Step 3: 经既有 MCP 可玩流程进入真实战斗，并保留 PIE 作视觉检查**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_real_play_flow_mcp.py --keep-pie --report Saved\HarnessReports\battle-actor-resource-hud.json
  ```

  Expected: report 的 `ok` 为 true，流程从主菜单进入城镇、路线和 `L_BattleTown`；`Saved/Codex/real_flow_after_battle.png` 已生成，PIE 保持打开以便检查。该命令使用项目既有 UE MCP/Slate 流程，不使用 Computer Use。

- [ ] **Step 4: 在保留的 PIE 中记录以下可见验收结果后停止 PIE**

  逐项确认并在本次交付说明中记录结果：

  1. 分别在 `1280×720` 和 `1920×1080` PIE 视口检查每个存活角色的 HUD：它位于其 Paper2D 可见脚点正上方，未偏到左侧、未落入手牌区、未被裁切，并且状态 icon 仍清晰可读。
  2. 主角、永久伙伴和临时 NPC 显示红色 `气血 当前 / 最大` 与低饱和蓝色 `气力 当前 / 最大`；普通怪物、黑熊和老虎 Boss 只显示气血、护甲和状态，不显示蓝色气力行。
  3. 打出消耗 Mana 的卡后，出牌者的蓝色数值和 fill 在刷新后同步下降；受伤、护甲变化和中毒/流血层数同步到同一个 Actor HUD。
  4. 将鼠标停在任意状态 icon 上，Tooltip 出现；把卡牌进入目标选择状态后，点击同一角色 HUD 的非 icon 区域仍能选中角色并提交合法目标。
  5. 顶部所有存活敌人的意图牌、`敌 nP` 标签和 hover Tooltip 仍持续显示；结束回合不会因为移除 footer 而卡死。

  停止 PIE 时先通过 UE MCP 结束会话并保存 dirty packages；不要强制关闭可能含有用户未保存资源的编辑器。

## 计划自检

- 规格覆盖：Task 1 交付红色气血、蓝色气力、两行数字和放大状态图标；Task 2 交付 Actor 组件归属、角色/NPC/怪物规则、权威 Card runtime 刷新和脚点锚定；Task 3 移除 Board 重复显示而保留箭头/意图；Task 4 交付原生 UMG Tooltip 与点击穿透；Task 5 覆盖冷编译、Automation 和实际 PIE。
- 类型一致性：`SetUnitStatus` 的参数顺序始终是 HP、MaxHP、Mana、MaxMana、bShowQi、Armor、Statuses；Actor 始终从 `FGameXXKCardCombatUnit` 解析这些字段，Board 从不保存单位 footer 数据。
- 范围控制：不改变卡牌费用、共享手牌能量、战斗地形、相机、角色精灵、PaperZD 或用户调过的关卡资源；不生成新 icon，而是沿用已有低饱和单色水墨 status 图标与已导入的 PSD 资源框。
