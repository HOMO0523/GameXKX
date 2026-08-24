# Party Progression, Locked Deck, and Level Gates Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make locked difficulties viewable, give the deployed trio independent 1-100 progression, expose all eighteen companion cards with level locks, and reject newly equipped items above the target character level.

**Architecture:** Keep durable progression in the existing runtime/save graph. Add a dedicated per-NPC progression map, centralize Training experience awards at the runtime-state boundary, derive companion card ownership deterministically from catalog plus the preserved six-card birth prefix, and enforce equipment level requirements in the state-aware equipment economy transaction used by every UI entry path.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT SaveGame data, programmatic UMG, UE Automation Tests, UBT cold compilation, UE MCP PIE verification.

---

### Task 1: Locked difficulties are viewable without changing Travel

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingMapPanelRedesignTest.cpp`

- [ ] **Step 1: Write the failing view-only difficulty test**

Add a test that opens the dropdown in a new game, verifies Hard is enabled as a viewing option while still labelled `未解锁`, selects it with action `622`, and asserts:

```cpp
TestTrue(TEXT("locked Hard remains viewable"), HardOption && HardOption->GetIsEnabled());
Widget->HandleActionClicked(622);
TestEqual(TEXT("Hard view selects Hard 1-1"),
    Widget->GetSelectedStageIdForTest(),
    FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Hard, 1));
TestEqual(TEXT("viewing Hard keeps Normal Travel"),
    Widget->GetCurrentTravelStageIdForTest(), NormalOne);
TestEqual(TEXT("viewing Hard preserves the walk cursor"),
    Subsystem->GetTrainingTravelRuntimeCopy().WalkStep, WalkStepBefore);
```

- [ ] **Step 2: Run the focused test and verify RED**

Run `GameXXK.DesktopTraining.Workbench.ThreeNodeChapterMapPanel` through UE Automation. Expected: FAIL because Hard is disabled or the view is forced back to Normal.

- [ ] **Step 3: Implement view-only difficulty selection**

Remove the forced-unlocked fallback from `BuildTrainingMapPanel`. Keep every dropdown option enabled, retain `未解锁` text/tint for locked difficulties, and remove the rejection branch from the difficulty action:

```cpp
Option->SetIsEnabled(true);
const bool bUnlocked = FGameXXKTrainingRules::IsDifficultyUnlocked(Progress, Difficulty);
// bUnlocked controls label/tint and node actions, not whether the page can be viewed.
```

Difficulty/chapter selection may call `SelectTrainingStage` but must never call `StartTrainingTravel`.

- [ ] **Step 4: Run the focused test and verify GREEN**

Expected: locked Hard/Hell can be viewed; nodes and bottom actions remain disabled; Travel runtime fields are unchanged.

### Task 2: Unify all permanent character levels at 100

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCharacterStatRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCharacterStatRulesTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`

- [ ] **Step 1: Write failing cap and threshold tests**

Assert `MaxCharacterLevel == 100`, hero/companion thresholds both equal `L * 100`, level 99 can reach 100, and level 100 clears residual XP:

```cpp
TestEqual(TEXT("shared level cap"), FGameXXKCharacterStatRules::MaxCharacterLevel, 100);
TestEqual(TEXT("hero level ten threshold"), UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(10), 1000);
TestEqual(TEXT("companion level ten threshold"), FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(10), 1000);
```

- [ ] **Step 2: Run tests and verify RED**

Expected: cap reports 20 and companion threshold reports 220 at level 10.

- [ ] **Step 3: Implement cap and unified curve**

Set:

```cpp
static constexpr int32 MaxCharacterLevel = 100;
```

Change companion threshold to:

```cpp
return CurrentLevel >= 1 && CurrentLevel < MaxCompanionLevel
    ? CurrentLevel * 100
    : 0;
```

Keep the existing linear stat formulas, now clamped at 100. Update load normalization so valid level-20 characters remain level 20 rather than being treated as capped.

- [ ] **Step 4: Run character and companion rule tests**

Expected: all cap/curve tests pass; level-100 XP is zero.

### Task 3: Add persistent independent NPC progression

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPartyProgressionAndLevelGateTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [ ] **Step 1: Write failing NPC persistence/stat tests**

Create `FGameXXKQuestNpcProgression` expectations for all six approved NPC IDs, award enough XP to level one NPC while leaving another unchanged, serialize/restore, and verify the NPC's own level drives attributes and `CombatLevel`.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: no NPC progression type/map/API exists and NPC combat level still follows the hero.

- [ ] **Step 3: Add save-compatible types and rules**

Add:

```cpp
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKQuestNpcProgression
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Level = 1;
    UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) int32 Experience = 0;
};
```

and to `FGameXXKCompanionPartySelection`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
TMap<FName, FGameXXKQuestNpcProgression> QuestNpcProgressions;
```

Add rule helpers:

```cpp
static bool NormalizeQuestNpcProgressions(FGameXXKCompanionPartySelection&, FString* OutError = nullptr);
static bool AwardQuestNpcExperience(FGameXXKCompanionPartySelection&, FName NpcId, int32 Amount, FString* OutError = nullptr);
static bool GetQuestNpcProgression(const FGameXXKCompanionPartySelection&, FName NpcId, FGameXXKQuestNpcProgression& Out);
```

Normalization creates exactly six entries, clamps level 1-100, normalizes XP against `Level * 100`, and removes unknown IDs.

- [ ] **Step 4: Route every NPC stat projection through its own level**

Add subsystem helpers `GetQuestNpcProgression` and `AwardQuestNpcExperience`. Replace `RuntimeState.PlayerLevel` at NPC stat/combat call sites in `GameXXKMVPSubsystem.cpp` and `GameXXKCardBattleAdapter.cpp` with the resolved NPC level.

- [ ] **Step 5: Run NPC and save tests**

Expected: independent NPC level survives save/load and drives stats/battle projection.

### Task 4: Award full Training experience to the deployed trio

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPartyProgressionAndLevelGateTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`

- [ ] **Step 1: Write failing online/offline/Challenge award tests**

For each reward path, record hero, active companion, active NPC, inactive companion, and inactive NPC XP. Assert the first three each receive the complete reward amount and the inactive entries remain unchanged.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: only hero gains Training XP; companion/NPC assertions fail.

- [ ] **Step 3: Add one transactional party-award helper**

Replace direct player-only calls with:

```cpp
static bool ApplyTrainingExperienceToDeployedParty(
    FGameXXKRuntimeState& State,
    int32 ExperienceAmount,
    FString* OutError = nullptr);
```

The helper applies `ExperienceAmount` independently to the hero, the exact active permanent companion, and the same active/default NPC identity used by `BuildTrainingTravelParty`. Apply it from both `ApplyTrainingRewardToRuntime` and `ApplyTrainingOfflineRewardToRuntime`.

- [ ] **Step 4: Synchronize the full Travel party after level-up**

Replace hero-only synchronization with a helper that rebuilds bare stats for all three durable party identities and updates the next-wave Travel runtime projection without changing its stage/walk cursor.

- [ ] **Step 5: Run award-path tests**

Expected: deployed trio advances equally; inactive roster entries do not change; UI-visible runtime values update in place.

### Task 5: Expand companions to eighteen cards with level unlocks

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKInventoryWindowWidget.h`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKCompanionBirthPoolMigrationTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Write failing pool/migration/unlock tests**

Assert all companion `PersonalCardIds` contain 18 cards; the original seeded six are unchanged at the prefix; selected five are unchanged; unlock counts at levels 1/5/10/15 are 6/10/14/18.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: personal pool contains only six and unlock count remains six.

- [ ] **Step 3: Build deterministic eighteen-card ownership**

Keep `BuildPersonalCardPool` as the birth-six resolver. Add a full-pool resolver:

```cpp
static bool BuildFullProfessionCardPool(
    EGameXXKCharacterRole Role,
    int32 CardSeed,
    TArray<FName>& OutCardIds,
    FString* OutError = nullptr);
```

It appends all remaining same-role catalog cards in catalog order after the six birth cards. `GetUnlockedPersonalCardCount` returns 6 below level 5, 10 at level 5, 14 at level 10, and 18 at level 15. Profile normalization expands legacy six-card saves without changing `SelectedCardIds`.

- [ ] **Step 4: Render all cards and add lock-level labels**

Keep `HeroCardBackpackIds = Companion.PersonalCardIds`. Add a transient unlock-label array parallel to the existing lock icon array. For locked cards set dim tint, show the existing lock icon, show `5级解锁`/`10级解锁`/`15级解锁`, and disable deck selection while preserving the tooltip. Collapse the text/icon immediately when unlocked.

- [ ] **Step 5: Run companion migration and FinalInventory tests**

Expected: all eighteen render; locked cards are below the unlocked prefix; unlocking never rewrites the selected five.

### Task 6: Enforce equipment item-level requirements

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKEquipmentRules.h`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKPartyProgressionAndLevelGateTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`

- [ ] **Step 1: Write failing atomic rejection tests**

Create level-1 hero/companion/NPC targets and a level-2 item. Exercise subsystem equip and desktop-cell equip, then assert `LevelRequirementNotMet`, message `需要角色达到 2 级`, and byte-identical inventory/loadout state.

- [ ] **Step 2: Run focused tests and verify RED**

Expected: all three targets currently accept the level-2 item.

- [ ] **Step 3: Add the state-aware economy gate**

Add `LevelRequirementNotMet` to `EGameXXKEquipmentTransactionError`. In `FGameXXKEquipmentEconomyRules::Equip`, resolve target level from the full runtime state before calling core `EquipInstance`:

```cpp
if (Instance->ItemLevel > TargetLevel)
{
    OutResult.Error = EGameXXKEquipmentTransactionError::LevelRequirementNotMet;
    OutResult.Message = FText::FromString(
        FString::Printf(TEXT("需要角色达到 %d 级"), Instance->ItemLevel));
    return false;
}
```

Do not add this check to collection normalization or unequip, so legacy over-level equipment remains worn.

- [ ] **Step 4: Verify every UI entry path shares the gate**

Exercise right-click quick equip, left-click desktop-cell placement, and occupied-slot replacement through their existing subsystem APIs. Expected: identical atomic rejection and message.

### Task 7: Show live NPC progression and complete regression

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKFinalInventoryWidgetTest.cpp`
- Test: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Write failing NPC UI refresh test**

Open an embedded NPC Attributes tab, award experience, call live refresh, and assert level text, `经验 current / required`, and progress percent update without replacing the inventory widget.

- [ ] **Step 2: Run focused test and verify RED**

Expected: NPC experience widgets remain collapsed and level follows hero.

- [ ] **Step 3: Bind NPC progression to Attributes UI**

Remove the NPC exclusion from `bShowExperience`. Resolve hero, companion, or NPC `Level/Experience` explicitly; use the unified `Level * 100` threshold and existing live refresh path.

- [ ] **Step 4: Cold compile and run full regression groups**

Save dirty UE packages through MCP, stop PIE/editor, then run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Run focused tests plus:

- `GameXXK.Training`
- `GameXXK.DesktopTraining.Workbench`
- `GameXXK.MVP.UI.FinalInventory`
- `GameXXK.Companion`
- `GameXXK.Equipment`

Expected: zero failures.

- [ ] **Step 5: Visible PIE acceptance**

Launch the visible `.uproject` on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start PIE, and verify:

- locked Hard/Hell can be browsed without changing Travel;
- active companion/NPC XP and levels refresh live;
- companion deck shows 18 cards with dim lock overlays/labels;
- over-level equipment is rejected with the required-level message;
- existing equipped over-level items are not auto-removed.
