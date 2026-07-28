# Battle Card Readability Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the battle hand, hover detail and target-selection instruction readable for every existing card without changing combat rules, persisted data or approved PSD artwork.

**Architecture:** Add a pure `GameXXKCardText` formatter over existing immutable definitions and runtime previews. `UGameXXKBattleBoardWidget` owns only UMG presentation state: a larger fixed-size hand and a hit-test-invisible parchment detail panel. Card play and target validation remain in `FGameXXKCardBattleAdapter`.

**Tech Stack:** Unreal Engine 5.8 C++, UMG, existing card USTRUCTs, Unreal Automation Tests, cold UBT build and PIE smoke test.

---

## File structure

| File | Responsibility |
| --- | --- |
| Create `Source/GameXXK/Public/GameXXKCardText.h` | Pure player-facing target, effect and state-text interface. |
| Create `Source/GameXXK/Private/GameXXKCardText.cpp` | Exhaustive structured-rule-to-Chinese formatter. |
| Create `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp` | 174-card formatter and representative semantic tests. |
| Modify `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h` | Detail-panel widget references and test-visible read-only seams. |
| Modify `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp` | Larger PSD-proportional hand, accessible disabled cards, hover detail panel and targeting copy. |
| Modify `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp` | Layout/detail/disabled-card regression tests. |

### Task 1: Add a red card-text contract

**Files:**
- Create: `Source/GameXXK/Public/GameXXKCardText.h`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`

- [ ] **Step 1: Declare the desired pure API and write the failing test.**

```cpp
namespace GameXXKCardText
{
    FString DescribeTarget(const FGameXXKCardTargetSpec& TargetSpec);
    FString DescribeEffects(const FGameXXKCardDefinition& Definition);
    FString DescribeDetail(const FGameXXKCardDefinition& Definition, const FGameXXKCardPlayPreview* Preview);
}

for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
{
    const FString Detail = GameXXKCardText::DescribeDetail(Definition, nullptr);
    TestFalse(FString::Printf(TEXT("%s has player-readable card text"), *Definition.Id.ToString()), Detail.IsEmpty());
    TestFalse(FString::Printf(TEXT("%s has no unknown formatter fallback"), *Definition.Id.ToString()), Detail.Contains(TEXT("未知")));
}
```

Add exact expectations for `Hero.QingFengYiShi` (single enemy and attack damage), `Hero.GuiyuanArt` (single ally), `Npc.QiongMeiEr.*` (terrain mode override), one consumption card and `Route.Rare.*` with a delayed modifier.

- [ ] **Step 2: Run the red test.**

Run:

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\roguelike_batch_tdd_runner.py --commands "Automation RunTests GameXXK.Integration.CardText" --clear-log --settle 2 --log-lines 180 --json --timeout 120
```

Expected: the automation filter finds no passing card-text test because the formatter does not exist yet. If the editor is unavailable, cold-compile the test and preserve the expected missing-header failure as the red evidence.

### Task 2: Implement exhaustive formatter coverage

**Files:**
- Create: `Source/GameXXK/Private/GameXXKCardText.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardText.h`

- [ ] **Step 1: Implement explicit enum formatters.**

Implement private `DescribeTargetMode`, `DescribeEffectTarget`, `DescribeStatus`, `DescribeTerrain`, `DescribeCondition`, and `DescribeModifier` switches. Each switch must have an explicit branch for every non-invalid enum value that occurs in the catalogue. `DescribeEffects` joins one semantic line per effect in catalogue order and appends a formatted clause for a non-empty condition, consumed stack reference, guard link or persistent modifier.

```cpp
FString DescribeTarget(const FGameXXKCardTargetSpec& Spec)
{
    switch (Spec.Mode)
    {
    case EGameXXKCardTargetMode::SingleEnemy: return TEXT("目标：单体敌方（点击后选择目标）");
    case EGameXXKCardTargetMode::SingleAlly: return TEXT("目标：单体友方（点击后选择目标）");
    case EGameXXKCardTargetMode::AllEnemies: return TEXT("目标：全体敌方（直接施放）");
    case EGameXXKCardTargetMode::None: return TEXT("目标：无需选择（直接施放）");
    case EGameXXKCardTargetMode::Self: return TEXT("目标：自身（直接施放）");
    case EGameXXKCardTargetMode::OtherAlly: return TEXT("目标：其他单体友方（点击后选择目标）");
    case EGameXXKCardTargetMode::AllAllies: return TEXT("目标：全体友方（直接施放）");
    case EGameXXKCardTargetMode::AllOtherAllies: return TEXT("目标：除自身外全体友方（直接施放）");
    case EGameXXKCardTargetMode::RandomEnemy: return TEXT("目标：随机敌方（自动结算）");
    case EGameXXKCardTargetMode::LowestHealthAlly: return TEXT("目标：生命最低友方（自动结算）");
    case EGameXXKCardTargetMode::LowestHealthOtherAlly: return TEXT("目标：生命最低的其他友方（自动结算）");
    case EGameXXKCardTargetMode::AnyLivingUnit: return TEXT("目标：任意存活单位（点击后选择目标）");
    default: return TEXT("目标：无效");
    }
}
```

Do not inspect card IDs in the formatter. It reads only declarative definition/preview fields.

- [ ] **Step 2: Rerun the formatter test and cold build.**

Run the automation command from Task 1 and then:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Expected: all formatter assertions pass; UBT exits `0`.

### Task 3: Add red board presentation tests

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleBoardWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`

- [ ] **Step 1: Extend the existing board fixture with desired assertions.**

Add test-only read accessors for `GetHandCardRuntimeSizeForTest`, `GetCardDetailTextForTest`, `IsCardDetailVisibleForTest`, and `GetHandCardButtonEnabledForTest`. Assert:

```cpp
TestEqual(TEXT("hand cards use the readable approved-frame scale"),
    Board->GetCardFrameRuntimeSizeForTest(), FVector2D(150.0f, 171.0f));
CardButton->OnHovered.Broadcast();
Board->AdvanceHandCardHoverMotionForTest(1.0f);
TestTrue(TEXT("hover reaches a clearly legible scale"), CardButton->GetRenderTransform().Scale.X >= 1.15f);
TestTrue(TEXT("hover lifts the readable card above its row"), CardButton->GetRenderTransform().Translation.Y <= -25.0f);
TestTrue(TEXT("hover exposes readable effect text"), Board->IsCardDetailVisibleForTest());
TestTrue(TEXT("hover detail names the target instruction"), Board->GetCardDetailTextForTest().Contains(TEXT("目标：")));
TestTrue(TEXT("hover detail names an effect"), Board->GetCardDetailTextForTest().Contains(TEXT("伤害")));
```

Create an insufficient-energy/mana state in the same fixture. Assert the card button remains enabled for inspection, its detail includes the preview failure reason, and `ClickCardInHand` returns false without mutation.

- [ ] **Step 2: Run the red board test.**

Run:

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\roguelike_batch_tdd_runner.py --commands "Automation RunTests GameXXK.Integration.CardBattle.BoardHandCardReadability" --clear-log --settle 2 --log-lines 220 --json --timeout 120
```

Expected: test fails because the old 113 x 129 hand and absent panel do not satisfy the assertions.

### Task 4: Implement hand and detail-panel presentation

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKBattleBoardWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp`

- [ ] **Step 1: Replace local magic numbers with named UI constants.**

Use `HandCardSize(150,171)`, `HandCardSlotPadding(4,0,4,0)`, `HandCardRowSize(790,173)`, `HoverScale(1.16)`, `HoverLift(-26)`, `SelectedScale(1.20)`, and `SelectedLift(-32)`. Keep the centre-bottom anchor and move its offsets to `(-395,-193,790,173)`. Do not modify `CardFrameTexturePath`, portrait resolution, strip tint or battle rules.

- [ ] **Step 2: Construct a hit-test-invisible paper detail panel.**

Create `CardDetailPanel`, title, meta, body and state `UTextBlock`s under `RootCanvas`; use `BattleStatusWindowFrameTexture`, dark ink and auto-wrapping. Anchor at bottom centre above the hand. Add `RefreshHandCardDetail()` that selects the pending card slot during targeting, otherwise the hovered slot, resolves the immutable definition and current preview, fills the formatter text, calls `SetToolTipText`, and collapses when no card is inspected.

- [ ] **Step 3: Preserve disabled-card inspection.**

During `RefreshHandCards`, set all present card buttons enabled so their hover handlers remain available. Retain non-playable visual opacity/tint and bind the current `FailureReason` into the detail state. Existing `ClickCardInHand` remains the only play gate and must be called unchanged by `HandleHandCardSlotClicked`.

- [ ] **Step 4: Keep instruction visible during targeting.**

When `IsCardTargetingActive`, `RefreshHandCardDetail` uses the selected card and `PendingCardPreview`: it shows manual side/category instruction plus legal-candidate count. `AdvanceHandCardHoverMotion` preserves selected emphasis and no longer removes the panel. `CancelBattleTargeting` refreshes the panel and returns the card to baseline.

- [ ] **Step 5: Run the board test and related targeting tests.**

Run:

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\roguelike_batch_tdd_runner.py --commands "Automation RunTests GameXXK.Integration.CardBattle.BoardHandCardReadability+GameXXK.Integration.CardBattle.BoardHandCardHoverStyle+GameXXK.Integration.CardBattle.BoardTargeting" --clear-log --settle 3 --log-lines 300 --json --timeout 150
```

Expected: all three tests pass with no changed gameplay state outside their fixtures.

### Task 5: Integrate and verify the playable flow

**Files:**
- Modify only files named in Tasks 1–4 if a test exposes a direct defect.

- [ ] **Step 1: Run the complete card-board regression group.**

```powershell
& 'C:\Users\shxuw\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' scripts\roguelike_batch_tdd_runner.py --commands "Automation RunTests GameXXK.Integration.CardBattle+GameXXK.Integration.CardRoute.EventSupport+GameXXK.Integration.CardRoute.QuestNpc+GameXXK.MVP.Battle.PartyDeckVisualMapping" --clear-log --settle 3 --log-lines 400 --json --timeout 180
```

Expected: all selected tests pass, including pending insight/forced discard, target arrow, NPC support and route reward gates.

- [ ] **Step 2: Cold compile and actual PIE smoke check.**

Run cold UBT using the Task 2 command, then use the approved UE MCP/TDD pipeline to start PIE without clearing saves. Verify a hand card opens the parchment description, a manual card updates it to a target instruction, legal units highlight, right-click cancels, and the end-turn button remains responsive.

- [ ] **Step 3: Record the evidence.**

Update the active production acceptance record with exact automation and UBT outputs. Do not claim full project completion from this UI subset; retain the broader goal’s terrain, assets and playable-flow audits.
