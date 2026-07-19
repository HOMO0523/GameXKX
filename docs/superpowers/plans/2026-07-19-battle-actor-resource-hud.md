# 战斗 Actor 资源与状态组件 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将每个战斗单位的资源条和状态效果拆为 Actor 持有的独立 Screen-space 组件：角色/NPC 显示气血与气力，怪物隐藏气力，护甲/状态 icon 作为独立可复用子 Widget，Battle Board 不再绘制任何脚底 HUD。

**Architecture:** `AGameXXKBattleSceneUnitActor` 持有一个脚点锚、两个独立锚和两个 `UWidgetComponent`。`UGameXXKBattleUnitResourceWidget` 只接收 HP/Mana 快照；`UGameXXKBattleUnitStatusEffectsWidget` 只接收 Armor/Statuses，并为每个状态创建 `UGameXXKBattleStatusIconWidget`。Actor 从 `CardRun.ActiveBattle.Units` 按 UnitId 解析权威数据，Board 继续负责手牌、指向箭头和敌方意图，不再保存 footer 状态或坐标。

**Tech Stack:** Unreal Engine 5.8、C++、UMG、Paper2D、Automation Tests、UE MCP、冷 UBT 编译。

---

## 文件结构

- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h` — 仅资源条的输入契约和测试读取口。
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp` — PSD 风格气血/气力资源行与原生 Widget tree。
- Create: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h` — 一个护甲或状态 icon 的可复用 UI 单元。
- Create: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp` — 单个 icon、层数、tooltip 和命中边框。
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h` — 独立 status-effect 行的输入契约、排序和增量刷新。
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp` — Armor/Statuses 到 icon 子 Widget 的映射。
- Delete: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h` — 旧的资源/状态混合 Widget，在调用方迁移完成后删除。
- Delete: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp` — 旧的资源/状态混合实现，在调用方迁移完成后删除。
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h` — 两个 anchors、两个 WidgetComponents、权威资源缓存和测试接口。
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp` — 权威 snapshot 解析、脚点锚和分别刷新资源/状态组件。
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` — 删除 Board footer 的 API、缓存和测试口。
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` — 删除 footer 创建/刷新/定位路径，保留箭头坐标与敌方意图。
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h` — 删除无效的世界 Widget hover bridge 声明。
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — 停止 footer projection 和 `UWidgetInteractionComponent`，保留 HitArea 卡牌选目标逻辑。
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp` — 资源 Widget 的文本、百分比、阵营可见性测试。
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp` — 状态行、icon 子 Widget、tooltip、清空和不重复重建测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp` — 两个 Actor 组件、权威 Card runtime 刷新、两个角色 anchors 和原生输入路由测试。
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp` — 用“不创建 footer”替换旧 Board footer 断言，保留卡牌箭头/意图牌覆盖。
- Reference only: `Source/GameXXK/Public/GameXXKCardTypes.h` — `FGameXXKCardCombatUnit` 权威 `HP/MaxHP/Mana/MaxMana/Armor/Statuses`。
- Reference only: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp` — 已导入状态图标、低饱和水墨风格和 tooltip 文案的唯一权威；本计划不重画这些资源。

## 不变的数据规则

| 单位 | 资源组件 | 状态效果组件 |
| --- | --- | --- |
| 主角、永久伙伴、临时任务 NPC | `气血 当前 / 最大` + `气力 当前 / 最大` | 护甲和所有有效状态 |
| 普通怪物、黑熊、老虎 Boss | `气血 当前 / 最大`，气力行折叠 | 护甲和所有有效状态 |
| 共享手牌能量 | 不传入组件 | 不传入组件 |

`bShowQi = !bEnemy && MaxMana > 0`。卡牌战斗已初始化时，Actor 必须以 Card runtime 覆盖 legacy 数据；legacy `HP/MaxHP/MP/MaxMP/Shield` 只在 Card runtime 未初始化时作回退。

### Task 1: 创建只负责气血与气力的资源 Widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp`

- [ ] **Step 1: 写入会失败的纯资源 Widget 自动化测试**

  创建 `GameXXKBattleUnitResourceWidgetTest.cpp`，注册 `GameXXK.UI.Battle.UnitResourceWidget` Automation test。加入以下测试；它引用尚不存在的类和接口，因此当前编译必须失败。

  ```cpp
  UGameXXKBattleUnitResourceWidget* Widget = NewObject<UGameXXKBattleUnitResourceWidget>();
  TestTrue(TEXT("resource widget constructs a native widget tree"), Widget->PrepareForScreenSpaceEmbedding());

  Widget->SetUnitResources(TEXT("我 1P"), FText::FromString(TEXT("主角")),
      72, 100, 18, 30, true);
  TestEqual(TEXT("health text includes label and both values"),
      Widget->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
  TestEqual(TEXT("qi text includes label and both values"),
      Widget->GetQiDisplayTextForTest(), FString(TEXT("气力 18 / 30")));
  TestTrue(TEXT("health fill uses authoritative fraction"),
      FMath::IsNearlyEqual(Widget->GetHealthPercentForTest(), 0.72f));
  TestTrue(TEXT("qi fill uses this unit's authoritative fraction"),
      FMath::IsNearlyEqual(Widget->GetQiPercentForTest(), 0.60f));
  TestTrue(TEXT("party unit exposes a qi row"), Widget->IsQiRowVisibleForTest());

  Widget->SetUnitResources(TEXT("敌 1P"), FText::FromString(TEXT("黑熊")),
      240, 240, 99, 100, false);
  TestEqual(TEXT("enemy health remains readable"),
      Widget->GetHealthDisplayTextForTest(), FString(TEXT("气血 240 / 240")));
  TestFalse(TEXT("enemy hides qi despite nonzero Mana"), Widget->IsQiRowVisibleForTest());
  TestEqual(TEXT("resource root never intercepts target clicks"),
      UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
  ```

- [ ] **Step 2: 使用冷编译确认测试因缺少资源 Widget 而失败**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 120
  ```

  Expected: 保存运行中的编辑器资源后，冷编译失败，报错明确指出 `UGameXXKBattleUnitResourceWidget` 或其测试接口未定义；不得使用 Live Coding 或 Hot Reload。

- [ ] **Step 3: 实现资源 Widget 的单一输入契约与 Widget tree**

  在头文件声明下列唯一资源入口。不要 include `GameXXKCardTypes.h`，不要提供 Armor 或 Statuses 参数。

  ```cpp
  UCLASS()
  class GAMEXXK_API UGameXXKBattleUnitResourceWidget : public UUserWidget
  {
      GENERATED_BODY()
  public:
      void SetUnitResources(const FString& InSlotLabel, const FText& InDisplayName,
          int32 InCurrentHP, int32 InMaxHP,
          int32 InCurrentMana, int32 InMaxMana, bool bInShowQi);
      bool PrepareForScreenSpaceEmbedding();
      bool HasRuntimeWidgetTreeForTest() const;
      FString GetHealthDisplayTextForTest() const;
      FString GetQiDisplayTextForTest() const;
      float GetHealthPercentForTest() const;
       float GetQiPercentForTest() const;
       bool IsQiRowVisibleForTest() const;
       static ESlateVisibility GetRootHitTestVisibilityForTest();
   private:
       void EnsureWidgetTree();
       void RefreshDisplay();
       UPROPERTY(Transient) TObjectPtr<UVerticalBox> RootBox;
       UPROPERTY(Transient) TObjectPtr<UTextBlock> IdentityText;
       UPROPERTY(Transient) TObjectPtr<UTextBlock> HealthText;
       UPROPERTY(Transient) TObjectPtr<UProgressBar> HealthBar;
       UPROPERTY(Transient) TObjectPtr<UHorizontalBox> QiRow;
       UPROPERTY(Transient) TObjectPtr<UTextBlock> QiText;
       UPROPERTY(Transient) TObjectPtr<UProgressBar> QiBar;
       FString SlotLabel;
       FText DisplayName;
       int32 CurrentHP = 0;
       int32 MaxHP = 1;
       int32 CurrentMana = 0;
       int32 MaxMana = 0;
       bool bShowQi = false;
   };
  ```

  在 cpp 维护 `RootBox`、`IdentityText`、`HealthText`、`HealthBar`、`QiRow`、`QiText`、`QiBar` 和缓存的 slot/name/HP/Mana/bShowQi。`PrepareForScreenSpaceEmbedding()` 必须沿用已验证的 native UUserWidget 初始化模式：`Initialize(); EnsureWidgetTree(); RefreshDisplay();`。

  `RefreshDisplay()` 必须使用同一快照产生数字和 fill，且只由 `bShowQi` 决定气力行可见性：

  ```cpp
  const int32 SafeMaxHP = FMath::Max(1, MaxHP);
  const int32 SafeMaxMana = FMath::Max(1, MaxMana);
  HealthText->SetText(FText::FromString(FString::Printf(TEXT("气血 %d / %d"), CurrentHP, MaxHP)));
  HealthBar->SetPercent(FMath::Clamp(static_cast<float>(CurrentHP) / SafeMaxHP, 0.0f, 1.0f));
  QiRow->SetVisibility(bShowQi ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
  QiText->SetText(FText::FromString(FString::Printf(TEXT("气力 %d / %d"), CurrentMana, MaxMana)));
  QiBar->SetPercent(FMath::Clamp(static_cast<float>(CurrentMana) / SafeMaxMana, 0.0f, 1.0f));
  ```

  资源根固定 `SelfHitTestInvisible`。健康 bar 使用 `/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterHealthFrame.T_TownPsd_CharacterHealthFrame`，气力 bar 使用 `/Game/GameXXK/UI/Town/Textures/PSD/Character/T_TownPsd_CharacterManaFrame.T_TownPsd_CharacterManaFrame`；填充色分别为低饱和朱红 `FLinearColor(0.62f, 0.25f, 0.22f, 1.0f)` 和墨青蓝 `FLinearColor(0.24f, 0.43f, 0.56f, 1.0f)`。纹理加载失败时保留纸色 fallback，不让资源框为空。

- [ ] **Step 4: 运行资源 Widget 自动化测试并确认通过**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.UnitResourceWidget; Quit'
  ```

  Expected: `GameXXK.UI.Battle.UnitResourceWidget` 通过，文字、fill、阵营气力可见性和 input-transparent root 全部正确。

- [ ] **Step 5: 提交资源 Widget 与测试**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleUnitResourceWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitResourceWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp
  git commit -m "feat(battle): add unit resource widget"
  ```

### Task 2: 创建独立的状态效果组件与单个 icon 子 Widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`

- [ ] **Step 1: 写入会失败的独立 icon/status-effect 测试**

  创建 `GameXXKBattleStatusEffectsWidgetTest.cpp`，注册 `GameXXK.UI.Battle.StatusEffectsWidget`。测试先构建一个 Effects Widget，再构建 armor、Poison、Momentum 三个模型；当前项目没有这些类，所以必须先失败。

  ```cpp
  UGameXXKBattleUnitStatusEffectsWidget* Effects = NewObject<UGameXXKBattleUnitStatusEffectsWidget>();
  TestTrue(TEXT("effects widget constructs its tree"), Effects->PrepareForScreenSpaceEmbedding());
  const auto MakeStatus = [](const EGameXXKCardStatus Status, const int32 Stacks)
  {
      FGameXXKCardStatusStack Result;
      Result.Status = Status;
      Result.Stacks = Stacks;
      return Result;
  };
  Effects->SetStatusEffects(7,
      {MakeStatus(EGameXXKCardStatus::Poison, 2),
       MakeStatus(EGameXXKCardStatus::Momentum, 1)});
  TestEqual(TEXT("armor and two statuses create three independent icon children"), Effects->GetIconCountForTest(), 3);
  TestEqual(TEXT("armor keeps priority over debuffs"), Effects->GetIconIdForTest(0), FName(TEXT("ArmorShield")));
  TestEqual(TEXT("poison icon preserves its stack count"), Effects->GetIconDisplayedStackForTest(1), FString(TEXT("2")));
  TestEqual(TEXT("status row is input-transparent"),
      UGameXXKBattleUnitStatusEffectsWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
  TestEqual(TEXT("icon hit target alone receives hover"),
      UGameXXKBattleStatusIconWidget::GetHitTargetVisibilityForTest(), ESlateVisibility::Visible);
  TestEqual(TEXT("tooltip does not intercept the pointer"),
      UGameXXKBattleStatusIconWidget::GetTooltipVisibilityForTest(), ESlateVisibility::HitTestInvisible);

  const int32 StableGeneration = Effects->GetIconRebuildGenerationForTest();
  Effects->SetStatusEffects(7,
      {MakeStatus(EGameXXKCardStatus::Poison, 2),
       MakeStatus(EGameXXKCardStatus::Momentum, 1)});
  TestEqual(TEXT("unchanged status snapshot does not rebuild child icons"),
      Effects->GetIconRebuildGenerationForTest(), StableGeneration);
  Effects->SetStatusEffects(0, {});
  TestEqual(TEXT("clearing effects removes stale icon children"), Effects->GetIconCountForTest(), 0);
  TestTrue(TEXT("changed effects rebuild only the icon row"),
      Effects->GetIconRebuildGenerationForTest() > StableGeneration);
  ```

  在同一测试加入下面的 overflow fixture。它明确锁定常量为 `6`：armor 加 7 个有效 status 一共是 8 个 badge model，因此前 6 个可读、索引 6 的第 7 个 child 必须是 `MoreStatuses`，层数为 `+2`。复用上面同一个 `MakeStatus` lambda，不重复定义它。

  ```cpp
  constexpr int32 MaxReadableStatusBadges = 6;
  Effects->SetStatusEffects(4,
      {MakeStatus(EGameXXKCardStatus::Poison, 1), MakeStatus(EGameXXKCardStatus::Bleed, 1),
       MakeStatus(EGameXXKCardStatus::Burn, 1), MakeStatus(EGameXXKCardStatus::DamageOverTime, 1),
       MakeStatus(EGameXXKCardStatus::Vulnerability, 1), MakeStatus(EGameXXKCardStatus::Agility, 1),
       MakeStatus(EGameXXKCardStatus::Guard, 1)});
  TestEqual(TEXT("overflow still uses seven visible child widgets"), Effects->GetIconCountForTest(), MaxReadableStatusBadges + 1);
  TestEqual(TEXT("overflow child is the MoreStatuses icon"), Effects->GetIconIdForTest(MaxReadableStatusBadges), FName(TEXT("MoreStatuses")));
  TestEqual(TEXT("overflow count includes the two hidden models"), Effects->GetIconDisplayedStackForTest(MaxReadableStatusBadges), FString(TEXT("+2")));
  ```

- [ ] **Step 2: 使用冷编译确认 status-effect/icon API 尚不存在**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 120
  ```

  Expected: 冷编译失败，错误指出 `UGameXXKBattleUnitStatusEffectsWidget`、`UGameXXKBattleStatusIconWidget` 或它们的测试接口未定义。

- [ ] **Step 3: 实现单个 icon 子 Widget，不给它任何战斗状态权限**

  在 `GameXXKBattleStatusIconWidget.h` 定义只接收一个已经解析好的模型的接口。`EnsureWidgetTree()` 和 `RefreshDisplay()` 是 private helper；这个 class 不 include 运行时战斗子系统，也不读取 Actor。

  ```cpp
  UCLASS()
  class GAMEXXK_API UGameXXKBattleStatusIconWidget : public UUserWidget
  {
      GENERATED_BODY()
  public:
      void SetBadgeModel(const FGameXXKBattleStatusBadgeModel& InBadgeModel, bool bInOverflow = false);
      bool PrepareForScreenSpaceEmbedding();
      bool HasRuntimeWidgetTreeForTest() const;
      FName GetIconIdForTest() const;
      FString GetDisplayedStackForTest() const;
       static FString FormatStackForTest(int32 Stacks);
       static ESlateVisibility GetHitTargetVisibilityForTest();
       static ESlateVisibility GetTooltipVisibilityForTest();
   private:
       void EnsureWidgetTree();
       void RefreshDisplay();
       UPROPERTY(Transient) TObjectPtr<USizeBox> RootBox;
       UPROPERTY(Transient) TObjectPtr<UBorder> HitTarget;
       UPROPERTY(Transient) TObjectPtr<UTextBlock> StackText;
       FGameXXKBattleStatusBadgeModel CachedBadgeModel;
       bool bIsOverflowBadge = false;
   };
  ```

  Widget tree 为 `SelfHitTestInvisible` root、`38×38` SizeBox、唯一的 `Visible` `HitTarget` Border、纸底/Overlay、图标或 fallback glyph、右上层数 seal。`HitTarget->SetToolTip(...)` 的 tooltip paper 必须返回 `HitTestInvisible`。复用 `FGameXXKBattleStatusIconStyle` 的 `TexturePath`、`FallbackGlyph`、`Tint` 和现有 tooltip 文案；不改变 `GameXXKBattleStatusIconStyle.{h,cpp}` 或 `SourceArt/UI/Battle/StatusIcons/*_inkflat_v4.png`。

  `SetBadgeModel()` 复制 `InBadgeModel` 与 `bInOverflow` 后立即调用 `RefreshDisplay()`；普通 badge 以 `FormatStackForTest(CachedBadgeModel.Stacks)` 显示层数，overflow badge 以 `FString::Printf(TEXT("+%d"), CachedBadgeModel.Stacks)` 显示层数。没有匹配纹理时显示 `FallbackGlyph`，`MoreStatuses` 则显示 `TEXT("⋯")`，不能渲染空白 icon。

  `PrepareForScreenSpaceEmbedding()` 必须执行：

  ```cpp
  Initialize();
  EnsureWidgetTree();
  RefreshDisplay();
  return RootBox && WidgetTree && WidgetTree->RootWidget == RootBox;
  ```

  这保证 icon 被 `WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>()` 嵌入父 widget 后不会缺失 native tree。

- [ ] **Step 4: 实现 status-effect 行、模型排序和仅在变化时重建**

  在 `GameXXKBattleUnitStatusEffectsWidget.h` 固定以下 API 和 private state。这个 header include `GameXXKCardTypes.h` 与 `UI/GameXXKBattleStatusIconStyle.h`；资源 Widget 的 header 不 include 它们。

  ```cpp
   UCLASS()
   class GAMEXXK_API UGameXXKBattleUnitStatusEffectsWidget : public UUserWidget
   {
       GENERATED_BODY()
   public:
       void SetStatusEffects(int32 InArmor, const TArray<FGameXXKCardStatusStack>& InStatuses);
       bool PrepareForScreenSpaceEmbedding();
       bool HasRuntimeWidgetTreeForTest() const;
       static FString BuildStatusText(const TArray<FGameXXKCardStatusStack>& InStatuses);
       static TArray<FGameXXKBattleStatusBadgeModel> BuildBadgeModels(
           int32 InArmor, const TArray<FGameXXKCardStatusStack>& InStatuses);
       int32 GetIconCountForTest() const;
       FName GetIconIdForTest(int32 Index) const;
       FString GetIconDisplayedStackForTest(int32 Index) const;
       int32 GetIconRebuildGenerationForTest() const;
       static ESlateVisibility GetRootHitTestVisibilityForTest();
   private:
       void EnsureWidgetTree();
       void RefreshStatusIcons();
       UPROPERTY(Transient) TObjectPtr<UHorizontalBox> StatusIconRow;
       TArray<FGameXXKBattleStatusBadgeModel> CachedBadgeModels;
       int32 IconRebuildGeneration = 0;
   };
  ```

  `BuildBadgeModels()` 移植旧 Widget 的 armor-first、priority、状态 tooltip 逻辑；保留 `Poison`、`Bleed`、`Burn`、`DamageOverTime`、`Vulnerability`、`Agility`、`Guard` 的中文 abbreviation 和 Unknown fallback。`SetStatusEffects()` 构建 `NextBadgeModels` 后，只有当每项 `Style.IconId`、`Stacks`、`Tooltip` 或 `Style.TexturePath` 与缓存不同时才调用 `RefreshStatusIcons()`。

  ```cpp
  if (AreBadgeModelArraysEquivalent(CachedBadgeModels, NextBadgeModels))
  {
      return;
  }
  CachedBadgeModels = MoveTemp(NextBadgeModels);
  RefreshStatusIcons();
  ++IconRebuildGeneration;
  ```

  在同一 cpp 定义比较 helper 和 overflow 规则，避免将“变化检测”留为隐式约定：

  ```cpp
  namespace
  {
      constexpr int32 MaxReadableStatusBadges = 6;

      bool AreBadgeModelArraysEquivalent(
          const TArray<FGameXXKBattleStatusBadgeModel>& Left,
          const TArray<FGameXXKBattleStatusBadgeModel>& Right)
      {
          if (Left.Num() != Right.Num())
          {
              return false;
          }
          for (int32 Index = 0; Index < Left.Num(); ++Index)
          {
              const FGameXXKBattleStatusBadgeModel& A = Left[Index];
              const FGameXXKBattleStatusBadgeModel& B = Right[Index];
              if (A.Style.IconId != B.Style.IconId
                  || A.Style.TexturePath != B.Style.TexturePath
                  || A.Stacks != B.Stacks
                  || A.Tooltip != B.Tooltip)
              {
                  return false;
              }
          }
          return true;
      }
  }
  ```

  `EnsureWidgetTree()` 用 `StatusIconRow` 作为 root，设置为 `SelfHitTestInvisible` 并赋给 `WidgetTree->RootWidget`。`RefreshStatusIcons()` 只清空 `StatusIconRow`，再用 `WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>()` 创建每个 child、显式调用 `PrepareForScreenSpaceEmbedding()`、设置 model 后加入 `UHorizontalBox`：

  ```cpp
  StatusIconRow->ClearChildren();
  const int32 VisibleCount = FMath::Min(MaxReadableStatusBadges, CachedBadgeModels.Num());
  for (int32 Index = 0; Index < VisibleCount; ++Index)
  {
      UGameXXKBattleStatusIconWidget* Icon = WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>();
      Icon->PrepareForScreenSpaceEmbedding();
      Icon->SetBadgeModel(CachedBadgeModels[Index]);
      StatusIconRow->AddChildToHorizontalBox(Icon);
  }
  if (CachedBadgeModels.Num() > VisibleCount)
  {
      FGameXXKBattleStatusBadgeModel OverflowBadge;
      OverflowBadge.Style.IconId = TEXT("MoreStatuses");
      OverflowBadge.Style.DisplayName = TEXT("更多状态");
      OverflowBadge.Stacks = CachedBadgeModels.Num() - VisibleCount;
      for (int32 Index = VisibleCount; Index < CachedBadgeModels.Num(); ++Index)
      {
          OverflowBadge.Tooltip += FString::Printf(TEXT("%s × %d\n"), *CachedBadgeModels[Index].Style.DisplayName, CachedBadgeModels[Index].Stacks);
      }
      UGameXXKBattleStatusIconWidget* OverflowIcon = WidgetTree->ConstructWidget<UGameXXKBattleStatusIconWidget>();
      OverflowIcon->PrepareForScreenSpaceEmbedding();
      OverflowIcon->SetBadgeModel(OverflowBadge, true);
      StatusIconRow->AddChildToHorizontalBox(OverflowIcon);
  }
  StatusIconRow->SetVisibility(CachedBadgeModels.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
  ```

  资源 Widget 没有被引用或重建。

- [ ] **Step 5: 运行独立状态效果自动化测试并确认通过**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.StatusEffectsWidget; Quit'
  ```

  Expected: armor/status 排序、stack、overflow、tooltip hit-test、清空和“不变 snapshot 不重建”断言均通过；资源 Widget 测试仍独立通过。

- [ ] **Step 6: 提交独立状态效果与 icon 组件**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleStatusIconWidget.h Source/GameXXK/Private/UI/GameXXKBattleStatusIconWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleUnitStatusEffectsWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusEffectsWidget.cpp Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp
  git commit -m "feat(battle): split status effects into icon widgets"
  ```

### Task 3: 将两个独立 HUD 组件接入战斗 Actor 和权威运行时数据

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入会失败的 Actor 组件树与权威数据测试**

  在 `GameXXKBattleSceneActorTest.cpp` 将旧 `UGameXXKBattleUnitStatusWidget` static badge 测试移至 Task 2 的专用测试。替换旧 World `StatusWidgetComponent` 断言为：

  ```cpp
  AGameXXKBattleSceneUnitActor* HeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
  UWidgetComponent* ResourceComponent = HeroActor->GetResourceHudWidgetComponentForTest();
  UWidgetComponent* EffectsComponent = HeroActor->GetStatusEffectsWidgetComponentForTest();
  TestNotNull(TEXT("actor owns a resource component"), ResourceComponent);
  TestNotNull(TEXT("actor owns a separate status-effects component"), EffectsComponent);
  TestEqual(TEXT("resource HUD is Screen space"), ResourceComponent->GetWidgetSpace(), EWidgetSpace::Screen);
  TestEqual(TEXT("effects HUD is Screen space"), EffectsComponent->GetWidgetSpace(), EWidgetSpace::Screen);
  TestEqual(TEXT("resource component attaches to its own anchor"),
      ResourceComponent->GetAttachParent(), HeroActor->GetResourceHudAnchorComponentForTest());
  TestEqual(TEXT("effects component attaches to its own anchor"),
      EffectsComponent->GetAttachParent(), HeroActor->GetStatusEffectsAnchorComponentForTest());
  TestEqual(TEXT("resource HUD has no world trace collision"),
      ResourceComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
  TestEqual(TEXT("effects HUD has no world trace collision"),
      EffectsComponent->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
  ```

  在既有 `BuildFixedSlotSceneBattleState()` fixture 中把 Card runtime 值设为主角 `72/100,18/30`（初始 `Armor=7`、`Poison=2`）、伙伴 `55/80,9/16`、`Npc.YueBai` `31/60,6/12`、敌人 `240/240,99/100`，同时让 legacy unit 的 HP/MP 使用不同数值。以同一 fixture 分别 `NewObject<AGameXXKBattleSceneUnitActor>()` 创建并配置 `HeroActor`、`CompanionActor`、`NpcActor`、`EnemyActor`，再断言：

  ```cpp
  TestEqual(TEXT("hero uses Card runtime HP"), HeroActor->GetCurrentHealthForTest(), 72);
  TestEqual(TEXT("hero uses Card runtime Mana"), HeroActor->GetCurrentManaForTest(), 18);
  TestTrue(TEXT("hero shows qi"), HeroActor->ShouldShowQiForTest());
   TestTrue(TEXT("permanent companion shows qi"), CompanionActor->ShouldShowQiForTest());
   TestTrue(TEXT("temporary NPC shows qi"), NpcActor->ShouldShowQiForTest());
   TestFalse(TEXT("enemy hides qi even with Mana"), EnemyActor->ShouldShowQiForTest());
   ```

  接着只改主角 Card runtime 的 `Mana=7`，保留初始 HP、Armor、Poison，且不调用 legacy sync；以旧 legacy unit 重调同一 Actor，断言：

  ```cpp
  TestEqual(TEXT("Mana-only retained refresh takes the Card runtime Mana"), HeroActor->GetCurrentManaForTest(), 7);
  TestEqual(TEXT("Mana-only refresh keeps armor snapshot unchanged"), HeroActor->GetArmorForTest(), 7);
  TestTrue(TEXT("Mana-only refresh keeps status snapshot unchanged"), HeroActor->GetStatusTextForTest().Contains(TEXT("毒 2")));
  ```

  最后把同一主角 Card runtime 改为 `HP=55`、`Mana=7`、`Armor=3`、仅 `Bleed=2`，继续不调用 legacy sync，以旧 legacy unit 重调同一 Actor，断言：

  ```cpp
  TestEqual(TEXT("retained hero refresh takes changed Card runtime HP"), HeroActor->GetCurrentHealthForTest(), 55);
  TestEqual(TEXT("retained hero refresh takes changed Card runtime Mana"), HeroActor->GetCurrentManaForTest(), 7);
  TestEqual(TEXT("retained hero refresh takes changed Card runtime armor"), HeroActor->GetArmorForTest(), 3);
  TestTrue(TEXT("retained hero refresh takes changed Card runtime bleed"), HeroActor->GetStatusTextForTest().Contains(TEXT("流 2")));
  ```

  这必须在当前只从 Card runtime 读取 Armor/Statuses 的实现上失败。

- [ ] **Step 2: 使用冷编译确认 Actor 接口/权威快照测试失败**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\ue_tdd_pipeline.py --pie-duration 1 --log-lines 120
  ```

  Expected: 编译失败，错误指向新的 Actor component getter、Mana getter 或 `ShouldShowQiForTest` 不存在；旧 Actor 仍只有一个 `StatusWidgetComponent`。

- [ ] **Step 3: 实现两个 actor-owned anchors、两个 Screen WidgetComponents 和权威快照解析**

  在 Actor header 声明并创建下列组件和测试 getter。保留现有的 `CurrentHP`、`MaxHP`、`CurrentArmor`、`CurrentStatuses` 单一字段，不得另建同名缓存；将 `MaxHP` 的安全默认值改为 `1`，并只新增 Mana/MaxMana/bShowQi 字段。

  ```cpp
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
   TObjectPtr<USceneComponent> HudAnchorComponent;
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
   TObjectPtr<USceneComponent> ResourceHudAnchorComponent;
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
   TObjectPtr<USceneComponent> StatusEffectsAnchorComponent;
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
   TObjectPtr<UWidgetComponent> ResourceHudWidgetComponent;
   UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
   TObjectPtr<UWidgetComponent> StatusEffectsWidgetComponent;

  USceneComponent* GetHudAnchorComponentForTest() const;
  USceneComponent* GetResourceHudAnchorComponentForTest() const;
  USceneComponent* GetStatusEffectsAnchorComponentForTest() const;
  UWidgetComponent* GetResourceHudWidgetComponentForTest() const;
   UWidgetComponent* GetStatusEffectsWidgetComponentForTest() const;
   int32 GetCurrentHealthForTest() const;
   int32 GetMaxHealthForTest() const;
   int32 GetCurrentManaForTest() const;
   int32 GetMaxManaForTest() const;
   int32 GetArmorForTest() const;
   FString GetStatusTextForTest() const;
   bool ShouldShowQiForTest() const;
private:
   void RefreshHudAnchor();
   void RefreshResourceHudWidget();
   void RefreshStatusEffectsWidget();
   void ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit);

   UPROPERTY(Transient) int32 CurrentHP = 0;
   UPROPERTY(Transient) int32 MaxHP = 1;
   UPROPERTY(Transient) int32 CurrentMana = 0;
   UPROPERTY(Transient) int32 MaxMana = 0;
   UPROPERTY(Transient) int32 CurrentArmor = 0;
   UPROPERTY(Transient) TArray<FGameXXKCardStatusStack> CurrentStatuses;
   UPROPERTY(Transient) bool bShowQi = false;
   ```

  在 header 的 forward declarations 加入 `class USceneComponent;`、`class UGameXXKBattleUnitResourceWidget;` 和 `class UGameXXKBattleUnitStatusEffectsWidget;`，删除旧 `class UGameXXKBattleUnitStatusWidget;`。在构造函数中显式创建 anchor 和两个组件；`HudAnchorComponent` 附到 `BattleVisual`，两个 child anchor 都附到 `HudAnchorComponent` 并保持相同的脚点位置。垂直次序只由两个 Screen widget 的互补 pivot 保证，避免再通过 Board 做像素偏移：

  ```cpp
  HudAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HudAnchor"));
  HudAnchorComponent->SetupAttachment(BattleVisual);
  ResourceHudAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("ResourceHudAnchor"));
  StatusEffectsAnchorComponent = CreateDefaultSubobject<USceneComponent>(TEXT("StatusEffectsAnchor"));
  ResourceHudWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("ResourceHudWidget"));
  StatusEffectsWidgetComponent = CreateDefaultSubobject<UWidgetComponent>(TEXT("StatusEffectsWidget"));
  ResourceHudAnchorComponent->SetupAttachment(HudAnchorComponent);
  StatusEffectsAnchorComponent->SetupAttachment(HudAnchorComponent);
  ResourceHudWidgetComponent->SetupAttachment(ResourceHudAnchorComponent);
  StatusEffectsWidgetComponent->SetupAttachment(StatusEffectsAnchorComponent);
  ResourceHudWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
  ResourceHudWidgetComponent->SetDrawSize(FIntPoint(300, 96));
  ResourceHudWidgetComponent->SetPivot(FVector2D(0.5f, 1.0f));
  StatusEffectsWidgetComponent->SetWidgetSpace(EWidgetSpace::Screen);
  StatusEffectsWidgetComponent->SetDrawSize(FIntPoint(300, 46));
  StatusEffectsWidgetComponent->SetPivot(FVector2D(0.5f, 0.0f));
  ResourceHudWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  StatusEffectsWidgetComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  ResourceHudWidgetComponent->SetWidgetClass(UGameXXKBattleUnitResourceWidget::StaticClass());
  StatusEffectsWidgetComponent->SetWidgetClass(UGameXXKBattleUnitStatusEffectsWidget::StaticClass());
  ```

  删除旧 `StatusWidgetComponent`，其 World offset、`SetTwoSided(true)` 和 `ECC_Visibility` 配置。`RefreshHudAnchor()` 继续从 `BattleVisual->Bounds` 求脚点并设置 `HudAnchorComponent`：

  ```cpp
  const FVector Foot = BattleVisual->Bounds.Origin - FVector(0.0f, 0.0f, BattleVisual->Bounds.BoxExtent.Z);
  HudAnchorComponent->SetWorldLocation(Foot + FVector(0.0f, 0.0f, 8.0f));
  ```

  将 `ResolveCardRuntimeStatus()` 改为 `ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit)`，并让 `ConfigureFromRuntimeUnit()` 把它调用为 `ResolveCardRuntimePresentation(Unit)`。先用 legacy 值填充 `CurrentHP/MaxHP/CurrentMana/MaxMana/CurrentArmor/CurrentStatuses`，再在 active card battle 下按 UnitId 覆盖全部值：

  ```cpp
  MaxHP = FMath::Max(1, LegacyUnit.MaxHP);
  CurrentHP = FMath::Clamp(LegacyUnit.HP, 0, MaxHP);
  MaxMana = FMath::Max(0, LegacyUnit.MaxMP);
  CurrentMana = FMath::Clamp(LegacyUnit.MP, 0, MaxMana);
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
          CurrentArmor = FMath::Clamp(CardUnit->Armor, 0, 99);
          CurrentStatuses = CardUnit->Statuses;
          bDefeated = !CardUnit->bLiving;
          if (SlotNumber == INDEX_NONE)
          {
              SlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, UnitId);
          }
      }
  }
  bShowQi = !bEnemy && MaxMana > 0;
  ```

  拆旧 `RefreshStatusWidget()` 为下列两个方法；两者每次只将自己的字段传给自己的 Widget：

  ```cpp
  void RefreshResourceHudWidget(); // slot/name, HP/MaxHP, Mana/MaxMana, bShowQi
  void RefreshStatusEffectsWidget(); // Armor, Statuses
  ```

  两个刷新函数必须同时同步 `UWidgetComponent` 可见性和自己的 native Widget，不能让一个组件承担另一个组件的数据。保留 `!GetWorld()` 的安全早退：无 World 的 Actor unit test 只验证构造配置和解析缓存，两个专用 native Widget test 验证文本、fill、icon；真实战斗 Actor 在 World 内走下面的转发路径。

  ```cpp
  if (!ResourceHudWidgetComponent || !StatusEffectsWidgetComponent || !GetWorld())
  {
      return;
  }
  const bool bHudVisible = !bDefeated && CurrentHP > 0;
  ResourceHudWidgetComponent->SetVisibility(bHudVisible, true);
  ResourceHudWidgetComponent->InitWidget();
  if (UGameXXKBattleUnitResourceWidget* Widget = Cast<UGameXXKBattleUnitResourceWidget>(ResourceHudWidgetComponent->GetUserWidgetObject()))
  {
      Widget->SetUnitResources(FGameXXKBattlePresentation::FormatSlotLabel(
          bEnemy ? EGameXXKCardTargetSide::Enemy : EGameXXKCardTargetSide::Party, SlotNumber),
          DisplayName, CurrentHP, MaxHP, CurrentMana, MaxMana, bShowQi);
  }

  StatusEffectsWidgetComponent->SetVisibility(bHudVisible, true);
  StatusEffectsWidgetComponent->InitWidget();
  if (UGameXXKBattleUnitStatusEffectsWidget* Widget = Cast<UGameXXKBattleUnitStatusEffectsWidget>(StatusEffectsWidgetComponent->GetUserWidgetObject()))
  {
      Widget->SetStatusEffects(CurrentArmor, CurrentStatuses);
  }
  ```

  上述 Mana-only retained-refresh 断言与 Task 2 的 `IconRebuildGeneration` 断言共同覆盖“资源刷新不重建 status icon children”的契约。

  两个组件在 `bDefeated || CurrentHP <= 0` 时都折叠；存活时都显示。`RefreshHudAnchor()` 在 visual 刷新、短震 tick 和短震复原后执行，使组件组随可见 sprite 脚点移动。

- [ ] **Step 4: 运行 Actor、资源和状态效果测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.UI.Battle.UnitResourceWidget+GameXXK.UI.Battle.StatusEffectsWidget; Quit'
  ```

  Expected: Actor 两个组件的 ownership、Screen mode、anchor、NoCollision、hero/partner/NPC 气力、敌方隐藏气力和 Card runtime 即时刷新全部通过；两个纯 Widget 测试仍通过。

- [ ] **Step 5: 提交 Actor 双组件接线**

  ```powershell
  git add -- Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
  git commit -m "feat(battle): attach separate resource and effects HUDs"
  ```

### Task 4: 移除 Board footer、旧混合 Widget 与世界 hover bridge

**Files:**
- Delete: `Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h`
- Delete: `Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`

- [ ] **Step 1: 写入会失败的“无第三套 footer / 无世界 hover bridge”测试**

  在 `GameXXKBattleSceneActorTest.cpp` 保留或加入 `#include "Components/WidgetInteractionComponent.h"`，因为测试仍需要 `UWidgetInteractionComponent` 类型来证明组件已从 Controller 删除。随后从 `GameXXKCardBattleBoardWidgetTest.cpp` 删除原 `RegisterBattleUnitFooterScreenPosition`、`IsBattleUnitFooterVisibleForTest`、`GetBattleUnitFooterPositionForTest`、`GetBattleUnitFooterBadgeCountForTest` 断言块，在 `Board->RefreshFromState()` 后加入：

  ```cpp
  TestNull(TEXT("battle board never creates a duplicate unit footer"),
      Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleUnitFooter_00")) : nullptr);
  ```

  在 Actor 测试中用普通组件查询替换旧 `GetBattleStatusHoverInteractionComponentForTest()`：

  ```cpp
  AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
  TestNull(TEXT("screen HUD does not retain a world-trace hover bridge"),
      Controller->FindComponentByClass<UWidgetInteractionComponent>());
  ```

  旧 Board refresh 会构建 `BattleUnitFooter_00`，旧 Controller 会构建 `BattleStatusHoverInteraction`，所以这两个断言在当前实现上都失败。

- [ ] **Step 2: 运行 Automation，确认旧 Board/footer 和 hover bridge 被捕获**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: 新的 Board footer 断言和 world-hover-bridge 断言失败；现有手牌、指向箭头和敌方意图测试保持可运行。

- [ ] **Step 3: 删除重复 Board footer 通路和旧混合 Widget**

  从 `UGameXXKBattleBoardWidget` 删除 footer 的 public/private/test API、地图和刷新路径：

  ```cpp
  void RegisterBattleUnitFooterScreenPosition(FName UnitId, FVector2D ScreenPosition);
  void ClearBattleUnitFooterScreenPositions();
  void RefreshBattleUnitFooters(bool bRefreshContent);
  UGameXXKBattleUnitStatusWidget* FindOrCreateBattleUnitFooter(FName UnitId);
  FVector2D ResolveBattleUnitFooterPosition(FVector2D UnitScreenPosition) const;
  TMap<FName, FVector2D> RegisteredBattleUnitFooterScreenPositions;
  TMap<FName, TObjectPtr<UGameXXKBattleUnitStatusWidget>> BattleUnitFooterWidgets;
  ```

  还要删除 `BattleUnitFooterSize`/padding 常量、`UI/GameXXKBattleUnitStatusWidget.h` include、`RefreshProgrammaticLayout()` 中的 `RefreshBattleUnitFooters(true)` 和所有 footer 测试辅助实现。保留 `RegisteredBattleUnitScreenPositions`、`RegisterBattleUnitScreenPosition()`、目标箭头起点和 `SetCardTargetHighlight()`；它们与 footer 完全不同，仍驱动卡牌指向。

  从 `RefreshBattleCardTargetingBridge()` 删除 `ClearBattleUnitFooterScreenPositions()` 和 `GetBattleFooterWorldLocation()` 投影/注册，但保留 `BattleVisual->Bounds.Origin` 投影到 `RegisterBattleUnitScreenPosition()`。随后删除 Actor 的 `GetBattleFooterWorldLocation()` 声明和实现。

  从 PlayerController header/cpp 删除 `UWidgetInteractionComponent` include、`BattleStatusHoverInteraction` 属性、构造函数 `CreateDefaultSubobject`、`EnsureBattleScenePresenter()` 的 `SetActive` 代码和测试 getter。Screen-space components 没有 traceable collision body，状态 hover 由 native UMG screen layer 负责；Battle Actor 的 `HitArea` 仍处理卡牌目标点击。

  最后删除旧 `GameXXKBattleUnitStatusWidget.h/.cpp`。在删除前后执行：

  ```powershell
  rg -n "GameXXKBattleUnitStatusWidget|RegisterBattleUnitFooterScreenPosition|ClearBattleUnitFooterScreenPositions|GetBattleFooterWorldLocation|BattleStatusHoverInteraction" Source Config
  ```

  Expected: 删除后命令以 exit code 1 结束且没有源代码匹配结果；任何 Blueprint asset 引用必须在冷启动日志中明确报出，不能留下无效 no-op API。

- [ ] **Step 4: 运行 Board/Actor 回归测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardBattle.BoardTargeting+GameXXK.UI.Battle.UnitResourceWidget+GameXXK.UI.Battle.StatusEffectsWidget; Quit'
  ```

  Expected: Board 不再产生 `BattleUnitFooter_00`；手牌 tooltip、指向箭头、合法目标高亮、顶部 `敌 nP` 意图卡与其 tooltip 继续通过；Screen HUD 不再有世界 hover bridge。

- [ ] **Step 5: 提交清理后的唯一 Actor HUD 通路**

  ```powershell
  git add -- Source/GameXXK/Public/UI/GameXXKBattleUnitStatusWidget.h Source/GameXXK/Private/UI/GameXXKBattleUnitStatusWidget.cpp Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp
  git commit -m "refactor(battle): remove legacy combined foot HUD"
  ```

### Task 5: 冷编译和真实 PIE 验收

**Files:**
- Create at runtime only: `Saved/HarnessReports/battle-actor-resource-hud.json`
- Create at runtime only: `Saved/Codex/real_flow_after_battle.png`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleUnitResourceWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleStatusEffectsWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`

- [ ] **Step 1: 通过安全保存、关闭、冷编译和 PIE 管线验证二进制**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\ue_tdd_pipeline.py --pie-duration 8 --log-lines 400
  ```

  Expected: 管线通过 UE MCP 保存 dirty packages 后关闭编辑器，执行 `-NoHotReload` 冷编译，用项目路径重启并进入/退出 PIE；输出包含 `[BUILD] Compile succeeded`，没有 `Assertion failed`、Landscape assertion 或 `dotnet.exe` 异常。

- [ ] **Step 2: 在最终二进制上运行全部受影响的 Automation 测试**

  Run:

  ```powershell
  & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -unattended -nop4 -NullRHI '-ExecCmds=Automation RunTests GameXXK.UI.Battle.UnitResourceWidget+GameXXK.UI.Battle.StatusEffectsWidget+GameXXK.MVP.Battle.SceneActors+GameXXK.Integration.CardBattle.BoardTargeting; Quit'
  ```

  Expected: 四组 Automation 全部通过；没有 legacy snapshot、混合 Widget、footer API、Widget tree 或失效世界 hover bridge 的报错。

- [ ] **Step 3: 用既有 MCP 可玩流程进入真实战斗并保留 PIE**

  Run:

  ```powershell
  & 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\gamexxk_real_play_flow_mcp.py --keep-pie --report Saved\HarnessReports\battle-actor-resource-hud.json
  ```

  Expected: report 的 `ok` 为 true，流程从主菜单进入城镇、路线和 `L_BattleTown`；`Saved/Codex/real_flow_after_battle.png` 已生成，PIE 保持打开。命令使用项目现有 UE MCP/Slate 流程，不使用 Computer Use。

- [ ] **Step 4: 在保留的 PIE 中完成可见验收后，经 UE MCP 停止 PIE**

  记录以下结果：

  1. 在 `1280×720` 和 `1920×1080` PIE 视口，主角、伙伴、临时 NPC、普通怪物、黑熊和老虎 Boss 的资源组件与状态组件作为一组贴在可见脚点，未偏左、未落进手牌区、未裁切。
  2. 主角/伙伴/NPC 显示红色 `气血 当前 / 最大` 和低饱和蓝色 `气力 当前 / 最大`；所有敌人只显示气血和独立状态 icon 行。
  3. 打出消耗 Mana 的卡后，只资源组件中的气力数值和 fill 改变；受到伤害/治疗不会重建无变化的 status icon children；护甲、中毒、流血改变时，只有 status-effects 行新增、移除或更新对应 icon/层数。
  4. 将鼠标停在 Armor、Poison、Bleed 等任一 icon 上，tooltip 出现；将卡牌进入目标选择状态后，点击同一角色 HUD 的非 icon 区域仍可命中 Actor `HitArea` 并提交合法目标。
  5. 顶部所有存活敌人的意图牌、`敌 nP` 标签和 hover tooltip 始终存在；结束回合不会因 footer 删除而卡死。

  停止 PIE 前通过 UE MCP 保存 dirty packages；不强制关闭可能含用户未保存资源的编辑器。

## 计划自检

- 规格覆盖：Task 1 只实现气血/气力；Task 2 只实现 armor/status/icon/tooltip；Task 3 将两者作为独立 Actor components 接到权威 Card runtime；Task 4 删除旧 Board/世界 hover 重复通路；Task 5 覆盖冷编译、Automation、PIE 和交互回归。
- 类型一致性：资源入口始终为 `SetUnitResources(slot, name, HP, MaxHP, Mana, MaxMana, bShowQi)`；状态入口始终为 `SetStatusEffects(Armor, Statuses)`；单 icon 入口始终为 `SetBadgeModel(model, bOverflow)`。没有组件直接读取或修改卡牌运行时状态。
- 范围控制：不改共享出牌能量、卡牌费用、相机、战斗地形、角色精灵、PaperZD 或已有 status icon 原画；每个 icon 是 status-effects 组件的子 Widget，不创建每状态一个 Actor-level WidgetComponent。
