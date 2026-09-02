# Training Boss Settlement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make a single-map Training Boss victory atomically grant one reward receipt, reopen that receipt after load, show a functional settlement page, and return to the pure-2D workbench only after confirmation.

**Architecture:** New pure Training settlement rules build and apply an immutable receipt against a copy of runtime state. The subsystem stores the applied receipt until acknowledgement; a small programmatic UMG widget only projects it. Existing route settlement remains separate, and single-map Boss victory never enters Boss-card reward choice.

**Tech Stack:** Unreal Engine 5.8 C++, Training/MVP state, SaveGame v38, UMG/Slate, UE Automation, pure-2D PIE.

---

## Preconditions and files

Complete Plans 1-5 first; current schema is v37. Work on `codex/overall-in-run-optimization`; preserve unrelated dirty assets. Settlement visual polish is deferred, so use existing paper styling and a functional information hierarchy only.

Create:

- `Source/GameXXK/Public/GameXXKTrainingSettlementRules.h`
- `Source/GameXXK/Private/GameXXKTrainingSettlementRules.cpp`
- `Source/GameXXK/Public/UI/GameXXKTrainingSettlementWidget.h`
- `Source/GameXXK/Private/UI/GameXXKTrainingSettlementWidget.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementRulesTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementSaveMigrationTest.cpp`

Modify:

- `Source/GameXXK/Public/GameXXKTrainingRules.h`
- `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- `Source/GameXXK/Public/GameXXKCardTypes.h`
- `Source/GameXXK/Private/GameXXKCardRules.cpp`
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- existing Training flow and workbench tests.

---

### Task 1: Add battle-session statistics and immutable receipt types

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Create: `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementRulesTest.cpp`

- [ ] **Step 1: Write red statistics and receipt-validation tests**

Resolve one player attack, enemy attack, heal, and Armor grant. Assert total cards, damage dealt/taken, healing, Armor, rounds, and surviving HP. Construct/default-serialize the receipt type and assert its IDs begin invalid, numeric rewards begin zero, and a fully populated round trip preserves every field; behavioral receipt validation starts in Task 2 after its rules class exists.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.SettlementRules --automation-report InRun06_Task01_RED --json
```

- [ ] **Step 3: Add saved statistics and receipt structs**

```cpp
USTRUCT(BlueprintType)
struct FGameXXKBattleSessionStats
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Rounds = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 ActiveCardsPlayed = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyDamageDealt = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyDamageTaken = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 HealingDone = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 ArmorGenerated = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 SurvivingPartyUnits = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyEndingHealth = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int64 PartyEndingMaxHealth = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKTrainingSettlementReceipt
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGuid ReceiptId;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName StageId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Gold = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Experience = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) EGameXXKTrainingRewardTier ChestTier = EGameXXKTrainingRewardTier::None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName GrantedChestItemId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FName GrantedEquipmentInstanceId = NAME_None;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 EquipmentItemLevel = 0;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bFirstClear = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bUnlockedNextStage = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) bool bUnlockedNextDifficulty = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) FGameXXKBattleSessionStats Stats;
};
```

Add `Stats` to `FGameXXKCardBattleRuntime` and increment transactional counters only after successful atomic commits. After each commit/round boundary, refresh surviving-unit count and party current/maximum HP from authoritative units; Task 2's `Build` freezes those terminal values into the receipt exactly once at victory.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.SettlementRules --automation-report InRun06_Task01_GREEN --json
git add Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Public/GameXXKTrainingRules.h Source/GameXXK/Private/Tests/GameXXKTrainingSettlementRulesTest.cpp
git diff --cached --check
git commit -m "feat: capture training settlement statistics"
```

### Task 2: Build and atomically apply one Training Boss receipt

**Files:**
- Create: `Source/GameXXK/Public/GameXXKTrainingSettlementRules.h`
- Create: `Source/GameXXK/Private/GameXXKTrainingSettlementRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementRulesTest.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`

- [ ] **Step 1: Write red idempotency tests**

Build receipts with an invalid ID and a mismatched StageId and assert rejection. Complete a Boss node and assert: no three-choice reward; one receipt exists; stage clear/unlocks and rewards are already applied; replaying `Apply` with the same ID changes nothing; receipt remains pending until acknowledgement; non-Boss victory still returns to route progression.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.SettlementRules.Idempotency --automation-report InRun06_Task02_RED --json
```

- [ ] **Step 3: Implement the pure authority**

```cpp
class GAMEXXK_API FGameXXKTrainingSettlementRules final
{
public:
    static bool Build(
        const FGameXXKRuntimeState& State,
        FName StageId,
        const FGameXXKTrainingReward& Reward,
        FGameXXKTrainingSettlementReceipt& OutReceipt,
        FString* OutError = nullptr);
    static bool Apply(
        FGameXXKRuntimeState& InOutState,
        const FGameXXKTrainingSettlementReceipt& Receipt,
        FString* OutError = nullptr);
    static bool Acknowledge(FGameXXKRuntimeState& InOutState, FGuid ReceiptId, FString* OutError = nullptr);
};
```

Store in Training progress:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKTrainingSettlementReceipt PendingSettlement;

UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGuid LastAppliedSettlementId;
```

`Apply` validates the active Boss/stage snapshot, applies reward/clear/unlocks to a copy, sets both IDs, closes battle/challenge route state, targets `DesktopTrainingHUD`, and preserves `PendingSettlement`. Reapplying the same ID is a no-op. `Acknowledge` clears only the matching pending receipt.

- [ ] **Step 4: Replace the direct-return Boss branch**

Refactor `SettleTrainingChallengeBossNode`/`AdvanceTrainingChallengeEncounter` to call Build then Apply. Do not call `CreateTieredBattleRewardOffer`, `CommitBossCardReward`, or ordinary pending-choice resolution for a single-map Training Boss.

- [ ] **Step 5: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training.SettlementRules --automation-report InRun06_Task02_GREEN --json
git add Source/GameXXK/Public/GameXXKTrainingSettlementRules.h Source/GameXXK/Private/GameXXKTrainingSettlementRules.cpp Source/GameXXK/Private/Tests/GameXXKTrainingSettlementRulesTest.cpp Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp
git diff --cached --check
git commit -m "feat: persist training boss settlements"
```

### Task 3: Expose settlement APIs through the subsystem

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp`

- [ ] **Step 1: Write red facade tests**

```cpp
TestTrue(TEXT("pending receipt exposed"), Subsystem->HasPendingTrainingSettlement());
const FGameXXKTrainingSettlementReceipt Receipt = Subsystem->GetPendingTrainingSettlementCopy();
TestTrue(TEXT("receipt id valid"), Receipt.ReceiptId.IsValid());
TestTrue(TEXT("confirm receipt"), Subsystem->ConfirmTrainingSettlement(Receipt.ReceiptId));
TestFalse(TEXT("receipt cleared"), Subsystem->HasPendingTrainingSettlement());
TestFalse(TEXT("wrong id rejected"), Subsystem->ConfirmTrainingSettlement(FGuid::NewGuid()));
```

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.MVP.TrainingSettlement --automation-report InRun06_Task03_RED --json
```

- [ ] **Step 3: Add copy-safe Blueprint APIs**

```cpp
UFUNCTION(BlueprintPure, Category="GameXXK|Training")
bool HasPendingTrainingSettlement() const;

UFUNCTION(BlueprintPure, Category="GameXXK|Training")
FGameXXKTrainingSettlementReceipt GetPendingTrainingSettlementCopy() const;

UFUNCTION(BlueprintCallable, Category="GameXXK|Training")
bool ConfirmTrainingSettlement(FGuid ReceiptId);
```

The facade delegates to rules and uses the subsystem's normal atomic mutation/save path.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.MVP.TrainingSettlement --automation-report InRun06_Task03_GREEN --json
git add Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKMVPUIWidgetTest.cpp
git diff --cached --check
git commit -m "feat: expose training settlement facade"
```

### Task 4: Add the functional settlement widget

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKTrainingSettlementWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTrainingSettlementWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`

- [ ] **Step 1: Write red widget-tree tests**

Set a receipt and assert the widget exposes Stage, Gold, XP, chest/equipment, first-clear/unlock, rounds, damage, healing, Armor, surviving units/HP, and one Confirm button. Confirm uses the exact receipt ID and cannot fire twice.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.TrainingSettlement --automation-report InRun06_Task04_RED --json
```

- [ ] **Step 3: Implement a projection-only widget**

```cpp
UCLASS()
class GAMEXXK_API UGameXXKTrainingSettlementWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    void SetReceipt(const FGameXXKTrainingSettlementReceipt& InReceipt);
    DECLARE_DELEGATE_OneParam(FOnConfirm, FGuid);
    FOnConfirm OnConfirm;
};
```

Build the programmatic tree once using existing paper panel/button styles. The widget does not calculate or grant rewards. `DesktopTrainingWorkbenchWidget` creates it whenever the subsystem has a pending receipt, blocks other workbench actions, and on confirmation delegates to `ConfirmTrainingSettlement` then restores the normal Training panel.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.UI.TrainingSettlement --automation-report InRun06_Task04_GREEN --json
git add Source/GameXXK/Public/UI/GameXXKTrainingSettlementWidget.h Source/GameXXK/Private/UI/GameXXKTrainingSettlementWidget.cpp Source/GameXXK/Private/Tests/GameXXKTrainingSettlementWidgetTest.cpp Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp
git diff --cached --check
git commit -m "feat: show functional training settlement"
```

### Task 5: Migrate v37 to v38 and verify reload recovery

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTrainingSettlementSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write red save/reload tests**

Serialize immediately after receipt Apply and before acknowledgement. Reload, assert rewards exist once and the same page/ID reappears; acknowledge, reload again, and assert no page/no duplicate. A v37 save gets empty receipt IDs.

- [ ] **Step 2: Run red**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.SaveMigration.TrainingSettlement --automation-report InRun06_Task05_RED --json
```

- [ ] **Step 3: Add v38**

```cpp
static constexpr int32 TrainingSettlementReceiptIntroducedSaveVersion = 38;
static constexpr int32 CurrentSaveVersion = 38;
```

Migration initializes empty settlement state only. It never reconstructs or grants a receipt from historical clear flags.

- [ ] **Step 4: Run green and commit**

```powershell
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.SaveMigration.TrainingSettlement --automation-report InRun06_Task05_GREEN --json
git add Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Private/Tests/GameXXKTrainingSettlementSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp
git diff --cached --check
git commit -m "feat: migrate training settlement receipts"
```

### Task 6: Run the pure-2D Boss-to-settlement acceptance

**Files:**
- Modify: `scripts/gamexxk_real_play_flow_mcp.py`
- Create: `scripts/test_training_settlement_flow.py`
- Modify: `scripts/README.md`

- [ ] **Step 1: Add a semantic flow probe**

Add `--flow` with `desktop-training-settlement` as its default and keep any legacy town harness behind an explicit `legacy-town` value. The new probe opens `L_DesktopTrainingHUD`, starts an unlocked stage, uses the existing deterministic battle fixture to reach Boss victory, asserts no reward-choice overlay, observes the settlement receipt/widget, confirms it once, and verifies `DesktopTrainingHUD` plus active Travel/workbench state. It never loads a 3D town or moves the user's pointer. The script test asserts an omitted `--flow` selects the 2D path and that only explicit `legacy-town` may reference `L_Main`/Qingshan.

- [ ] **Step 2: Run script tests and cold acceptance**

```powershell
python scripts/test_training_settlement_flow.py
python scripts/ai_production_loop.py --run-ubt --run-automation --automation-tests GameXXK.Training --automation-report InRun06_Training_GREEN --json
```

- [ ] **Step 3: Run the real PIE flow through UE MCP**

```powershell
python scripts/gamexxk_real_play_flow_mcp.py --flow desktop-training-settlement
```

Expected: Boss victory -> one settlement -> one confirmation -> 2D workbench, with no Boss-card option and no duplicate reward.

- [ ] **Step 4: Commit flow coverage**

```powershell
git add scripts/gamexxk_real_play_flow_mcp.py scripts/test_training_settlement_flow.py scripts/README.md
git diff --cached --check
git commit -m "test: cover training boss settlement flow"
```

Plan 6 is complete when a crash/reload at the pending page is idempotent and the pure-2D packaged flow never bypasses or duplicates settlement.
