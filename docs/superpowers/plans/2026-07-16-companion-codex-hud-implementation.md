# Companion Codex HUD Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the town HUD's text-only companion panel with a persistent, filterable, scrollable companion codex that discovers real quest and battle entities.

**Architecture:** `UGameXXKMVPRules` owns the static codex catalogue and all discovery/read/save-migration state; `UGameXXKMVPSubsystem` exposes read-only views and a single mark-read operation. `UGameXXKTownHudWidget` renders the codex locally without adding `TownPanelMode::Codex`, so it stays independent from the legacy TownOverlay command model and closes through the existing player-controller Escape path.

**Tech Stack:** Unreal Engine 5.8 C++, UMG programmatic widgets, Unreal Automation Tests, UBT cold builds, UE MCP / PIE verification.

---

## Locked implementation decisions

- Work directly on `main` in `D:\UE5 demo\GameXXK`; do not create a worktree.
- Preserve the current uncommitted world-map, HUD and inventory work. Stage only files listed by each task.
- The codex uses `EGameXXKCodexCategory::{All, Hero, Spirit, Monster, Beast}` mapped in UI to `全部、侠客、仙灵、妖怪、珍兽`.
- The first-row companion-card base is the only visual geometry reference. Runtime cards use a fixed `216 x 238` unit rectangle (same 10:11 ratio as the measured `120 x 132` first-row base), never second-row source bounds.
- `Spirit` intentionally has no current real-game entry. It renders the approved empty state instead of fabricated PSD sample characters.
- The known initial catalogue is only real current runtime content: `Codex.Guide`, `Codex.Bandit`, `Codex.Wolf`, `Codex.EliteBandit`, and `Codex.Boss`.
- Do not bind the untracked `T_TownBackpack_WindowFrame`, `T_TownBackpack_BackArrow`, or `T_TownBackpack_ActionBlank` assets. Use programmatic `UBorder` / `UButton` chrome plus tracked Companion category textures until those user-owned assets are explicitly committed.

## File structure

| File | Responsibility |
| --- | --- |
| `Source/GameXXK/Public/GameXXKMVPRules.h` | Codex category, definition/view structs, runtime persisted sets, and rule APIs. |
| `Source/GameXXK/Private/GameXXKMVPRules.cpp` | Static catalogue, masking, discovery/read operations, quest/battle hooks, and v4→v5 migration. |
| `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` | Read-only codex view/count/unread APIs plus `MarkCodexEntryRead`. |
| `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp` | Thin wrappers around rules; no duplicate state. |
| `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h` | Typed filter/card buttons, codex overlay state, public close/test APIs. |
| `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp` | Programmatic fixed-left-rail / right-scroll-grid codex, unread badges, filtering and selection. |
| `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` | Esc closes an open Town HUD codex before battle targeting. |
| `Source/GameXXK/Private/Tests/GameXXKCompanionCodexRulesTest.cpp` | Isolated rule/discovery/masking/idempotence red-green test. |
| `Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp` | v5 slot round trip and v4 migration test. |
| `Source/GameXXK/Private/Tests/GameXXKCompanionCodexWidgetTest.cpp` | HUD construction, filter, three-column, scroll, red-dot and mutual-exclusion test. |
| `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp` | Real player-controller Esc close integration assertion. |
| `Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp` | Updates the existing save-version expectation from 4 to 5 only. |

## Shared commands

Use a cold build after every new or changed C++ test; never use Live Coding or Hot Reload.

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
```

Run one focused automation test after the build by replacing the test path below.

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Codex.RulesDiscovery;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

---

### Task 1: Add a persistent rules-owned codex catalogue and discovery contract

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`

- [ ] **Step 1: Write the failing rules/discovery test.**

Create `GameXXKCompanionCodexRulesTest.cpp` with this registration and assertions. It intentionally refers to APIs that do not exist yet.

```cpp
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCompanionCodexRulesTest,
    "GameXXK.MVP.Codex.RulesDiscovery",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexRulesTest::RunTest(const FString& Parameters)
{
    FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
    const FName Guide = FName(TEXT("Codex.Guide"));
    const FName Bandit = FName(TEXT("Codex.Bandit"));
    const FName Wolf = FName(TEXT("Codex.Wolf"));

    TestEqual(TEXT("catalogue has five real current entries"), UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::All), 5);
    TestEqual(TEXT("spirit category is intentionally empty"), UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::Spirit), 0);
    TestEqual(TEXT("new game has no discovered entries"), UGameXXKMVPRules::GetDiscoveredCodexEntryCount(State, EGameXXKCodexCategory::All), 0);
    TestFalse(TEXT("new game has no codex red dot"), UGameXXKMVPRules::HasUnreadCodexEntries(State));

    const TArray<FGameXXKCodexEntryView> InitialViews = UGameXXKMVPRules::BuildCodexEntryViews(State, EGameXXKCodexCategory::All);
    const FGameXXKCodexEntryView* InitialGuide = InitialViews.FindByPredicate([Guide](const FGameXXKCodexEntryView& View)
    {
        return View.Id == Guide;
    });
    TestNotNull(TEXT("guide definition is present"), InitialGuide);
    TestTrue(TEXT("undiscovered guide remains masked"), InitialGuide && !InitialGuide->bIsDiscovered);
    TestEqual(TEXT("undiscovered guide name is masked"), InitialGuide ? InitialGuide->DisplayName.ToString() : FString(), FString(TEXT("????")));

    TestTrue(TEXT("world map opens from new game"), UGameXXKMVPRules::OpenWorldMap(State));
    TestTrue(TEXT("Qingshan town opens from world map"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
    TestTrue(TEXT("accepting the town quest succeeds"), UGameXXKMVPRules::AcceptTownQuest(State));
    TestTrue(TEXT("quest discovery records the guide"), State.DiscoveredCodexEntryIds.Contains(Guide));
    TestTrue(TEXT("newly discovered guide is unread"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
    TestTrue(TEXT("marking guide read succeeds once"), UGameXXKMVPRules::MarkCodexEntryRead(State, Guide));
    TestFalse(TEXT("reading guide clears all current unread state"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
    TestFalse(TEXT("re-discovery is idempotent"), UGameXXKMVPRules::DiscoverCodexEntry(State, Guide));
    TestTrue(TEXT("re-discovery does not remove read state"), State.ReadCodexEntryIds.Contains(Guide));
    TestFalse(TEXT("unknown codex id is rejected"), UGameXXKMVPRules::DiscoverCodexEntry(State, FName(TEXT("Codex.Unknown"))));
    TestFalse(TEXT("unknown codex id cannot be marked read"), UGameXXKMVPRules::MarkCodexEntryRead(State, FName(TEXT("Codex.Unknown"))));

    TestTrue(TEXT("accepted quest enters route map"), UGameXXKMVPRules::EnterDungeon(State));
    TestTrue(TEXT("route start advances"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
    TestTrue(TEXT("battle node creates a battle"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
    TestTrue(TEXT("battle discovery records bandit"), State.DiscoveredCodexEntryIds.Contains(Bandit));
    TestTrue(TEXT("battle discovery records wolf"), State.DiscoveredCodexEntryIds.Contains(Wolf));
    TestTrue(TEXT("battle encounters produce an unread red dot"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
    return true;
}

#endif
```

- [ ] **Step 2: Run the red build and verify the failure is missing codex symbols.**

Run the shared cold-build command. Expected result: UHT/C++ fails only because `EGameXXKCodexCategory`, `FGameXXKCodexEntryView`, and the listed `UGameXXKMVPRules` APIs are undefined.

- [ ] **Step 3: Add the exact public data contract in `GameXXKMVPRules.h`.**

Insert these types after `EGameXXKItemKind`; append the two `TSet<FName>` fields at the end of `FGameXXKRuntimeState`; append the APIs near `BuildTurnOrder`.

```cpp
UENUM(BlueprintType)
enum class EGameXXKCodexCategory : uint8
{
    All,
    Hero,
    Spirit,
    Monster,
    Beast
};

USTRUCT(BlueprintType)
struct FGameXXKCodexEntryDef
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere) FName Id;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) EGameXXKCodexCategory Category = EGameXXKCodexCategory::Hero;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FText DisplayName;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FText Description;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FSoftObjectPath IconPath;
};

USTRUCT(BlueprintType)
struct FGameXXKCodexEntryView
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadWrite, EditAnywhere) FName Id;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) EGameXXKCodexCategory Category = EGameXXKCodexCategory::Hero;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) bool bIsDiscovered = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) bool bIsRead = false;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FText DisplayName;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FText Description;
    UPROPERTY(BlueprintReadWrite, EditAnywhere) FSoftObjectPath IconPath;
};
```

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere)
TSet<FName> DiscoveredCodexEntryIds;

UPROPERTY(BlueprintReadWrite, EditAnywhere)
TSet<FName> ReadCodexEntryIds;
```

```cpp
static TArray<FGameXXKCodexEntryDef> GetCodexEntryDefs();
static FGameXXKCodexEntryDef GetCodexEntryDef(FName EntryId, bool& bFound);
static TArray<FGameXXKCodexEntryView> BuildCodexEntryViews(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category);
static int32 GetCodexEntryCount(EGameXXKCodexCategory Category);
static int32 GetDiscoveredCodexEntryCount(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category);
static bool HasUnreadCodexEntries(const FGameXXKRuntimeState& State);
static bool DiscoverCodexEntry(FGameXXKRuntimeState& State, FName EntryId);
static bool MarkCodexEntryRead(FGameXXKRuntimeState& State, FName EntryId);
```

- [ ] **Step 4: Implement the minimal static catalogue, masking and idempotent discovery in `GameXXKMVPRules.cpp`.**

In namespace `GameXXKMVP`, add stable constants and helpers. Keep icon paths empty: the HUD must use a programmatic placeholder until real per-entry art exists.

```cpp
static const FName CodexGuideName(TEXT("Codex.Guide"));
static const FName CodexBanditName(TEXT("Codex.Bandit"));
static const FName CodexWolfName(TEXT("Codex.Wolf"));
static const FName CodexEliteBanditName(TEXT("Codex.EliteBandit"));
static const FName CodexBossName(TEXT("Codex.Boss"));

static FGameXXKCodexEntryDef MakeCodexEntry(FName Id, EGameXXKCodexCategory Category, const TCHAR* Name, const TCHAR* Description)
{
    FGameXXKCodexEntryDef Def;
    Def.Id = Id;
    Def.Category = Category;
    Def.DisplayName = FText::FromString(Name);
    Def.Description = FText::FromString(Description);
    return Def;
}

static TArray<FGameXXKCodexEntryDef> GetCodexEntryDefsInternal()
{
    return {
        MakeCodexEntry(CodexGuideName, EGameXXKCodexCategory::Hero, TEXT("引路人"), TEXT("在青山镇相遇的同行者。")),
        MakeCodexEntry(CodexBanditName, EGameXXKCodexCategory::Monster, TEXT("山匪"), TEXT("盘踞在青山道上的敌人。")),
        MakeCodexEntry(CodexWolfName, EGameXXKCodexCategory::Beast, TEXT("野狼"), TEXT("出没于山道的凶兽。")),
        MakeCodexEntry(CodexEliteBanditName, EGameXXKCodexCategory::Monster, TEXT("精英山匪"), TEXT("比普通山匪更难应对的精锐。")),
        MakeCodexEntry(CodexBossName, EGameXXKCodexCategory::Monster, TEXT("虎王"), TEXT("守在青山尽头的首领。")),
    };
}

static bool MatchesCodexCategory(const FGameXXKCodexEntryDef& Def, EGameXXKCodexCategory Category)
{
    return Category == EGameXXKCodexCategory::All || Def.Category == Category;
}

static FName GetCodexEntryIdForBattleUnit(FName RuntimeUnitId)
{
    if (RuntimeUnitId == TEXT("Bandit")) return CodexBanditName;
    if (RuntimeUnitId == TEXT("Wolf")) return CodexWolfName;
    if (RuntimeUnitId == TEXT("EliteBandit")) return CodexEliteBanditName;
    if (RuntimeUnitId == TEXT("Boss")) return CodexBossName;
    return NAME_None;
}
```

Implement the public view methods by iterating `GetCodexEntryDefsInternal()` only. For an undiscovered definition, set `DisplayName` to `????`, `Description` to `未遇见`, `bIsDiscovered=false`, `bIsRead=false`, and clear `IconPath`. For discovered entries preserve the definition text and set `bIsRead` from `ReadCodexEntryIds`.

Implement discovery and read exactly as follows:

```cpp
bool UGameXXKMVPRules::DiscoverCodexEntry(FGameXXKRuntimeState& State, FName EntryId)
{
    bool bFound = false;
    GetCodexEntryDef(EntryId, bFound);
    if (!bFound || State.DiscoveredCodexEntryIds.Contains(EntryId))
    {
        return false;
    }
    State.DiscoveredCodexEntryIds.Add(EntryId);
    return true;
}

bool UGameXXKMVPRules::MarkCodexEntryRead(FGameXXKRuntimeState& State, FName EntryId)
{
    bool bFound = false;
    GetCodexEntryDef(EntryId, bFound);
    if (!bFound || !State.DiscoveredCodexEntryIds.Contains(EntryId) || State.ReadCodexEntryIds.Contains(EntryId))
    {
        return false;
    }
    State.ReadCodexEntryIds.Add(EntryId);
    return true;
}
```

At the successful end of `AcceptTownQuest`, call `DiscoverCodexEntry(State, CodexGuideName)`. In `GameXXKMVP::BeginBattle`, immediately after all enemy rows are appended and before setting the battle screen, use the runtime enemy type that the project actually stores:

```cpp
for (const FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
{
    const FName CodexEntryId = GetCodexEntryIdForBattleUnit(Enemy.Id);
    if (!CodexEntryId.IsNone())
    {
        UGameXXKMVPRules::DiscoverCodexEntry(State, CodexEntryId);
    }
}
```

- [ ] **Step 5: Run the focused test and verify it is green.**

Run the shared cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Codex.RulesDiscovery;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: `GameXXK.MVP.Codex.RulesDiscovery` completes with `Success` and no unrelated compile failure.

- [ ] **Step 6: Commit only the rules test and rules files.**

```powershell
git add -- Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Private/Tests/GameXXKCompanionCodexRulesTest.cpp
git commit -m "feat: add companion codex discovery rules"
```

### Task 2: Persist codex state and expose HUD-safe subsystem wrappers

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp`

- [ ] **Step 1: Write the failing v5 persistence/migration test.**

Create `GameXXKCompanionCodexPersistenceTest.cpp` with a direct save-state migration test plus a real `SaveCurrentGame` slot round trip.

```cpp
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCompanionCodexPersistenceTest,
    "GameXXK.MVP.Codex.SaveMigration",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexPersistenceTest::RunTest(const FString& Parameters)
{
    const FName Guide(TEXT("Codex.Guide"));
    const FName Bandit(TEXT("Codex.Bandit"));
    FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
    TestTrue(TEXT("guide discovery succeeds"), UGameXXKMVPRules::DiscoverCodexEntry(State, Guide));
    TestTrue(TEXT("bandit discovery succeeds"), UGameXXKMVPRules::DiscoverCodexEntry(State, Bandit));
    TestTrue(TEXT("guide read succeeds"), UGameXXKMVPRules::MarkCodexEntryRead(State, Guide));

    const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(State);
    TestEqual(TEXT("codex save uses version five"), SaveState.SaveVersion, 5);
    const FGameXXKRuntimeState Restored = UGameXXKMVPRules::RestoreFromSaveState(SaveState);
    TestTrue(TEXT("round trip keeps discovered guide"), Restored.DiscoveredCodexEntryIds.Contains(Guide));
    TestTrue(TEXT("round trip keeps discovered bandit"), Restored.DiscoveredCodexEntryIds.Contains(Bandit));
    TestTrue(TEXT("round trip keeps guide read"), Restored.ReadCodexEntryIds.Contains(Guide));
    TestFalse(TEXT("round trip keeps unread bandit unread"), Restored.ReadCodexEntryIds.Contains(Bandit));

    const FString SlotName(TEXT("GameXXKCompanionCodexPersistence"));
    constexpr int32 UserIndex = 0;
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
    UGameInstance* SaveGameInstance = NewObject<UGameInstance>();
    UGameXXKMVPSubsystem* SaveSubsystem = NewObject<UGameXXKMVPSubsystem>(SaveGameInstance);
    SaveSubsystem->GetMutableRuntimeState() = State;
    TestTrue(TEXT("manual save writes codex state"), SaveSubsystem->SaveCurrentGame(SlotName, UserIndex));
    UGameXXKMVPSubsystem* LoadSubsystem = NewObject<UGameXXKMVPSubsystem>(SaveGameInstance);
    TestTrue(TEXT("manual save reloads codex state"), LoadSubsystem->LoadGameFromSlot(SlotName, UserIndex));
    TestTrue(TEXT("manual save keeps discovered guide"), LoadSubsystem->GetRuntimeState().DiscoveredCodexEntryIds.Contains(Guide));
    TestTrue(TEXT("manual save keeps read guide"), LoadSubsystem->GetRuntimeState().ReadCodexEntryIds.Contains(Guide));
    UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);

    FGameXXKSaveState LegacyAccepted;
    LegacyAccepted.SaveVersion = 4;
    LegacyAccepted.RuntimeState = UGameXXKMVPRules::CreateNewGame();
    LegacyAccepted.RuntimeState.QuestState = EGameXXKQuestState::Accepted;
    const FGameXXKRuntimeState MigratedAccepted = UGameXXKMVPRules::RestoreFromSaveState(LegacyAccepted);
    TestTrue(TEXT("v4 accepted save infers only guide"), MigratedAccepted.DiscoveredCodexEntryIds.Contains(Guide));
    TestFalse(TEXT("v4 accepted save does not invent bandit history"), MigratedAccepted.DiscoveredCodexEntryIds.Contains(Bandit));
    TestTrue(TEXT("migrated guide is presented as unread"), UGameXXKMVPRules::HasUnreadCodexEntries(MigratedAccepted));

    FGameXXKSaveState VersionFiveEmpty;
    VersionFiveEmpty.SaveVersion = 5;
    VersionFiveEmpty.RuntimeState = UGameXXKMVPRules::CreateNewGame();
    VersionFiveEmpty.RuntimeState.QuestState = EGameXXKQuestState::Accepted;
    const FGameXXKRuntimeState RestoredVersionFiveEmpty = UGameXXKMVPRules::RestoreFromSaveState(VersionFiveEmpty);
    TestFalse(TEXT("v5 empty record is not retroactively migrated"), RestoredVersionFiveEmpty.DiscoveredCodexEntryIds.Contains(Guide));
    return true;
}

#endif
```

In `GameXXKInventoryEnhancementTest.cpp`, change its existing expected save version assertion from `4` to `5`; do not alter the inventory behavior assertions.

- [ ] **Step 2: Run the red build and verify the failure is the current `SaveVersion == 4` implementation.**

Run the shared build. Expected result: the new migration test compiles after Task 1 but fails because `MakeSaveState` still outputs version 4 and no codex migration exists.

- [ ] **Step 3: Add the subsystem API declarations.**

Add these public methods beside the other state-reading APIs in `GameXXKMVPSubsystem.h`:

```cpp
UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
TArray<FGameXXKCodexEntryView> GetCodexEntryViews(EGameXXKCodexCategory Category) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
int32 GetCodexEntryCount(EGameXXKCodexCategory Category) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
int32 GetDiscoveredCodexEntryCount(EGameXXKCodexCategory Category) const;

UFUNCTION(BlueprintPure, Category = "GameXXK|Codex")
bool HasUnreadCodexEntries() const;

UFUNCTION(BlueprintCallable, Category = "GameXXK|Codex")
bool MarkCodexEntryRead(FName EntryId);
```

- [ ] **Step 4: Implement v4→v5 migration and wrappers.**

Add a single current-version constant in the rules namespace and use it in both write and restore paths:

```cpp
static constexpr int32 CurrentSaveVersion = 5;

static void MigrateCodexState(FGameXXKRuntimeState& State, int32 SaveVersion)
{
    if (SaveVersion < CurrentSaveVersion
        && (State.QuestState == EGameXXKQuestState::Accepted || State.QuestState == EGameXXKQuestState::Completed))
    {
        UGameXXKMVPRules::DiscoverCodexEntry(State, CodexGuideName);
    }
}
```

Set `SaveState.SaveVersion = CurrentSaveVersion`. In every existing `RestoreFromSaveState` branch, call `MigrateCodexState(State, SaveState.SaveVersion)` directly after the existing `MigrateInventoryCategoryItems(State)` call and before returning. This keeps inventory normalization first and applies only the approved legacy-guide inference afterward. Do not add duplicate codex fields to `FGameXXKSaveState`: `RuntimeState` is already the serialized full state.

Implement wrappers as one-line delegations:

```cpp
TArray<FGameXXKCodexEntryView> UGameXXKMVPSubsystem::GetCodexEntryViews(EGameXXKCodexCategory Category) const
{
    return UGameXXKMVPRules::BuildCodexEntryViews(RuntimeState, Category);
}

int32 UGameXXKMVPSubsystem::GetCodexEntryCount(EGameXXKCodexCategory Category) const
{
    return UGameXXKMVPRules::GetCodexEntryCount(Category);
}

int32 UGameXXKMVPSubsystem::GetDiscoveredCodexEntryCount(EGameXXKCodexCategory Category) const
{
    return UGameXXKMVPRules::GetDiscoveredCodexEntryCount(RuntimeState, Category);
}

bool UGameXXKMVPSubsystem::HasUnreadCodexEntries() const
{
    return UGameXXKMVPRules::HasUnreadCodexEntries(RuntimeState);
}

bool UGameXXKMVPSubsystem::MarkCodexEntryRead(FName EntryId)
{
    return UGameXXKMVPRules::MarkCodexEntryRead(RuntimeState, EntryId);
}
```

- [ ] **Step 5: Run the focused persistence test and the existing inventory save-version test.**

Run the shared cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.Codex.SaveMigration+GameXXK.MVP.Inventory.EnhancementAndStorage;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: both named tests complete with `Success`.

- [ ] **Step 6: Commit only persistence and subsystem files.**

```powershell
git add -- Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp
git commit -m "feat: persist companion codex progress"
```

### Task 3: Render the approved filterable, three-column companion codex inside Town HUD

**Files:**

- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTownHudWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`

- [ ] **Step 1: Write the failing focused Town HUD codex test.**

Create `GameXXKCompanionCodexWidgetTest.cpp`. It validates actual dynamic UMG names and behavior, not implementation-only private state.

```cpp
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKTownHudWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCompanionCodexWidgetTest,
    "GameXXK.MVP.UI.CompanionCodex",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexWidgetTest::RunTest(const FString& Parameters)
{
    UGameInstance* GameInstance = NewObject<UGameInstance>();
    UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
    Subsystem->StartGame();
    TestTrue(TEXT("codex test enters Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
    TestTrue(TEXT("codex test accepts quest"), Subsystem->AcceptQuest());

    UGameXXKTownHudWidget* TownHud = NewObject<UGameXXKTownHudWidget>();
    TownHud->SetMVPSubsystem(Subsystem);
    TownHud->TakeWidget();
    TownHud->RefreshFromState();
    UButton* CompanionButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCompanion"))) : nullptr;
    TestNotNull(TEXT("town HUD retains the companion nav button"), CompanionButton);
    if (CompanionButton)
    {
        CompanionButton->OnClicked.Broadcast();
    }
    TestTrue(TEXT("companion button opens codex"), TownHud->IsCompanionCodexOpenForTest());
    TestNotNull(TEXT("codex builds visible scroll box"), TownHud->WidgetTree ? Cast<UScrollBox>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexScroll"))) : nullptr);
    TestEqual(TEXT("codex begins on all category"), TownHud->GetActiveCodexCategoryForTest(), EGameXXKCodexCategory::All);
    TestEqual(TEXT("codex card grid has three columns"), TownHud->GetCodexColumnCountForTest(), 3);
    TestEqual(TEXT("codex uses first-row 10 by 11 card ratio"), TownHud->GetCodexCardSizeForTest(), FVector2D(216.0f, 238.0f));
    TestEqual(TEXT("codex displays discovered over total count"), TownHud->GetCodexCollectionSummaryForTest().ToString(), FString(TEXT("已收录 1 / 5")));
    TestTrue(TEXT("guide is visible in all category"), TownHud->GetVisibleCodexEntryIdsForTest().Contains(FName(TEXT("Codex.Guide"))));
    TestTrue(TEXT("unread guide produces HUD badge"), TownHud->HasCompanionUnreadNoticeForTest());

    TestTrue(TEXT("selecting guide marks it read"), TownHud->SelectCodexEntryForTest(FName(TEXT("Codex.Guide"))));
    TestFalse(TEXT("guide read clears last HUD badge"), TownHud->HasCompanionUnreadNoticeForTest());
    TestTrue(TEXT("spirit tab can be selected"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::Spirit));
    TestEqual(TEXT("spirit category has no visible real cards"), TownHud->GetVisibleCodexEntryIdsForTest().Num(), 0);
    TestTrue(TEXT("spirit category exposes empty state"), TownHud->IsCodexEmptyStateVisibleForTest());

    TestTrue(TEXT("all tab restores card rows"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::All));
    TestTrue(TEXT("test can move codex scroll"), TownHud->SetCodexScrollOffsetForTest(64.0f));
    TestTrue(TEXT("hero filter resets scroll position"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::Hero));
    TestEqual(TEXT("filter reset returns scroll to top"), TownHud->GetCodexScrollOffsetForTest(), 0.0f);

    UButton* CodexCloseButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexClose"))) : nullptr;
    TestNotNull(TEXT("codex supplies a dedicated close button"), CodexCloseButton);
    if (CodexCloseButton)
    {
        CodexCloseButton->OnClicked.Broadcast();
    }
    TestFalse(TEXT("codex close button hides the overlay"), TownHud->IsCompanionCodexOpenForTest());
    if (CompanionButton)
    {
        CompanionButton->OnClicked.Broadcast();
    }
    TestTrue(TEXT("same companion button reopens codex"), TownHud->IsCompanionCodexOpenForTest());
    if (CompanionButton)
    {
        CompanionButton->OnClicked.Broadcast();
    }
    TestFalse(TEXT("same companion button toggles codex closed"), TownHud->IsCompanionCodexOpenForTest());
    TestTrue(TEXT("test reopens codex before character exclusivity check"), TownHud->OpenCompanionCodexForTest());

    UButton* CharacterButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCharacter"))) : nullptr;
    TestNotNull(TEXT("town HUD retains character button"), CharacterButton);
    if (CharacterButton)
    {
        CharacterButton->OnClicked.Broadcast();
    }
    TestFalse(TEXT("opening character panel closes codex"), TownHud->IsCompanionCodexOpenForTest());
    return true;
}

#endif
```

- [ ] **Step 2: Run the red build and confirm missing Town HUD codex methods/widgets are the cause.**

Run the shared cold build. Expected result: compile fails only on the new public Town HUD codex APIs and `EGameXXKCodexCategory` UI integration, not on existing world-map or inventory code.

- [ ] **Step 3: Add typed filter/card buttons and public HUD APIs in `GameXXKTownHudWidget.h`.**

Include `GameXXKMVPRules.h` and `Components/Button.h`, then add these two button types before `UGameXXKTownHudWidget`:

```cpp
UCLASS()
class GAMEXXK_API UGameXXKCompanionCodexFilterButton : public UButton
{
    GENERATED_BODY()
public:
    void Configure(UGameXXKTownHudWidget* InOwner, EGameXXKCodexCategory InCategory);
    UFUNCTION() void HandleClicked();
private:
    UPROPERTY(Transient) TObjectPtr<UGameXXKTownHudWidget> Owner;
    EGameXXKCodexCategory Category = EGameXXKCodexCategory::All;
};

UCLASS()
class GAMEXXK_API UGameXXKCompanionCodexCardButton : public UButton
{
    GENERATED_BODY()
public:
    void Configure(UGameXXKTownHudWidget* InOwner, FName InEntryId);
    UFUNCTION() void HandleClicked();
private:
    UPROPERTY(Transient) TObjectPtr<UGameXXKTownHudWidget> Owner;
    FName EntryId;
};
```

Add the following public methods to `UGameXXKTownHudWidget`:

```cpp
bool CloseCompanionCodex();
bool IsCompanionCodexOpenForTest() const;
bool OpenCompanionCodexForTest();
bool SelectCodexCategoryForTest(EGameXXKCodexCategory Category);
bool SelectCodexEntryForTest(FName EntryId);
EGameXXKCodexCategory GetActiveCodexCategoryForTest() const;
TArray<FName> GetVisibleCodexEntryIdsForTest() const;
int32 GetCodexColumnCountForTest() const;
FVector2D GetCodexCardSizeForTest() const;
bool IsCodexEmptyStateVisibleForTest() const;
bool HasCompanionUnreadNoticeForTest() const;
FText GetCodexCollectionSummaryForTest() const;
float GetCodexScrollOffsetForTest() const;
bool SetCodexScrollOffsetForTest(float Offset);
void HandleConfiguredCodexFilterClicked(EGameXXKCodexCategory Category);
void HandleConfiguredCodexCardClicked(FName EntryId);
```

Add a private `UFUNCTION() void HandleCodexCloseClicked();` which delegates only to `CloseCompanionCodex()`. Use private state `bCompanionCodexOpen`, `ActiveCodexCategory`, `SelectedCodexEntryId`, `VisibleCodexEntryIds`, `CodexColumnCount = 3`, `CodexCardSize = FVector2D(216.0f, 238.0f)`, and `LastCodexScrollOffset`. Replace the old `CompanionPanel`, `CompanionStatusText`, and `CompanionLabel` member set with codex backdrop/frame/scroll/grid/filter/card/badge members, including `CodexCollectionText` and a `CodexCloseButton` named `TownHudCodexClose`.

- [ ] **Step 4: Implement the programmatic codex layout and interaction in `GameXXKTownHudWidget.cpp`.**

Keep `TownHudCompanion` as the existing nav button name. Build the new overlay once from `BuildProgrammaticLayout()` using a `1060 x 680` centered `UBorder`, a fixed `150`-wide left rail, a `UScrollBox` named `TownHudCodexScroll`, a `UUniformGridPanel` named `TownHudCodexGrid`, and a top-right `UButton` named `TownHudCodexClose` whose click is bound to `HandleCodexCloseClicked`.

Use this layout data and visibility contract:

```cpp
const FVector2D CodexPanelSize(1060.0f, 680.0f);
const FVector2D CodexCardSize(216.0f, 238.0f);
constexpr int32 CodexColumnCount = 3;
constexpr float CodexCardGap = 14.0f;

CodexScrollBox->SetOrientation(Orient_Vertical);
CodexScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
CodexScrollBox->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
```

For each category button configure one of `All`, `Hero`, `Spirit`, `Monster`, and `Beast`. Use the tracked category-label texture paths only as labels:

```cpp
const FString CompanionTextureRoot(TEXT("/Game/GameXXK/UI/Town/Textures/Companion/"));
const FString AllTexture = CompanionTextureRoot + TEXT("T_TownCompanion_AllSelected.T_TownCompanion_AllSelected");
const FString HeroTexture = CompanionTextureRoot + TEXT("T_TownCompanion_Wanderer.T_TownCompanion_Wanderer");
const FString SpiritTexture = CompanionTextureRoot + TEXT("T_TownCompanion_Fairy.T_TownCompanion_Fairy");
const FString MonsterTexture = CompanionTextureRoot + TEXT("T_TownCompanion_Demon.T_TownCompanion_Demon");
const FString BeastTexture = CompanionTextureRoot + TEXT("T_TownCompanion_RareCategory.T_TownCompanion_RareCategory");
```

`RefreshCompanionCodex()` must:

1. Collapse the overlay unless the runtime screen is `Town` and it is already open.
2. Read views/counts only through `UGameXXKMVPSubsystem`.
3. Set the HUD entry `CompanionUnreadBadge` (`●`, red text) visible only when `HasUnreadCodexEntries()` is true.
4. Set `CodexCollectionText` to `已收录 %d / %d`, using `GetDiscoveredCodexEntryCount(ActiveCodexCategory)` and `GetCodexEntryCount(ActiveCodexCategory)`; the `All` tab therefore reports the aggregate count.
5. Rebuild the grid from `GetCodexEntryViews(ActiveCodexCategory)`, storing all currently rendered IDs in `VisibleCodexEntryIds`.
6. Use a fixed `USizeBox` width/height override of `216 / 238` for every `TownHudCodexCardSize_<index>`; add each to `UUniformGridPanel` at `(index % 3, index / 3)`.
7. Render undiscovered cards with `????` / `未遇见`, a neutral placeholder, no description and no action that writes state.
8. Render discovered cards as `UGameXXKCompanionCodexCardButton`; selecting it calls `Subsystem->MarkCodexEntryRead`, writes the detail strip, and refreshes badge/card state. Add a hit-test-invisible red `UTextBlock` (`●`) on each discovered view whose `bIsRead` is false; do not render that dot for masked cards or read cards.
9. On category change clear `SelectedCodexEntryId`, call `CodexScrollBox->SetScrollOffset(0.0f)`, set `LastCodexScrollOffset = 0.0f`, then rebuild.
10. Set `CodexEmptyStateText` visible only when the requested category has zero definition views. Its exact text is `尚未收录`.

Implement `CloseCompanionCodex()` as local UI state only: collapse overlay, clear the selection, reset scroll to zero, return `true` if it was open. Make `HandleCompanionClicked()` toggle this same local state so the nav icon closes an already-open codex. Update `CloseAuxiliaryPanels()`, `HandleCharacterClicked()`, `HandleMapClicked()`, and `RefreshFromState()` to call it, ensuring the character panel and codex cannot remain open together and leaving town cannot preserve the overlay.

- [ ] **Step 5: Run the focused widget test and inspect the layout contract.**

Run the shared cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.CompanionCodex;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: `GameXXK.MVP.UI.CompanionCodex` completes with `Success`; the named scroll box and same-sized three-column card boxes are present.

- [ ] **Step 6: Commit only the Town HUD implementation and focused widget test.**

```powershell
git add -- Source/GameXXK/Public/UI/GameXXKTownHudWidget.h Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/Tests/GameXXKCompanionCodexWidgetTest.cpp
git commit -m "feat: add scrollable town companion codex"
```

### Task 4: Close the codex through the real player-controller Escape path

**Files:**

- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`

- [ ] **Step 1: Add a failing Escape integration assertion to the existing player-flow test.**

After the test has a town `PlayerController` and `TownHudWidget`, add:

```cpp
UGameXXKTownHudWidget* TownHud = PlayerController->GetTownHudWidgetForTest();
TestNotNull(TEXT("player controller owns town HUD for companion codex"), TownHud);
TestTrue(TEXT("town HUD opens companion codex for Escape test"), TownHud && TownHud->OpenCompanionCodexForTest());
TestTrue(TEXT("companion codex is visible before Escape"), TownHud && TownHud->IsCompanionCodexOpenForTest());
TestTrue(TEXT("Escape closes the open companion codex"), PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
TestFalse(TEXT("Escape hides companion codex without leaving town"), TownHud && TownHud->IsCompanionCodexOpenForTest());
TestEqual(TEXT("Escape leaves player in town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
```

- [ ] **Step 2: Run the red build and verify Escape currently does not close codex.**

Run the shared cold build and then the focused `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets` test. Expected result: the new assertion fails because `InputKey` has no Town HUD codex branch.

- [ ] **Step 3: Add the smallest controller branch.**

In `AGameXXKMVPPlayerController::InputKey`, inside the existing `Escape + IE_Pressed` block after the inventory-close condition and before battle-target cancellation, add:

```cpp
if (TownHudWidget && TownHudWidget->IsCompanionCodexOpenForTest())
{
    return TownHudWidget->CloseCompanionCodex();
}
```

Do not change player movement locks, screen state, or `TownPanelMode`; the codex is a local Town HUD overlay.

- [ ] **Step 4: Run the focused player-flow test green.**

Run the shared cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: `GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets` completes with `Success` and town remains active after Escape.

- [ ] **Step 5: Commit only controller Escape handling and its regression test.**

```powershell
git add -- Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp
git commit -m "feat: close companion codex with escape"
```

### Task 5: Perform complete non-Live-Coding regression and real playable verification

**Files:**

- Modify only if evidence reveals a defect: files from Tasks 1–4.
- Do not modify scene assets, PaperZD assets, camera transforms, HD2D plane values, or the user-owned untracked Town Backpack art.

- [ ] **Step 1: Run formatting and change-scope checks.**

```powershell
git diff --check
git status --short
```

Expected result: no whitespace errors; inspect staged candidates manually and leave unrelated user files unstaged.

- [ ] **Step 2: Run the complete MVP automation suite after a cold build.**

```powershell
python scripts/gamexxk_mvp_playtest.py --test-timeout 600 --report Saved/HarnessReports/companion-codex-full-green.json
```

Expected result: the build succeeds, every `GameXXK.MVP` automation test succeeds, and the report has `"ok": true` and an empty `failed_tests` list.

- [ ] **Step 3: Run the existing UE MCP smoke check and launch a normal editor only after it is safe.**

```powershell
python scripts/ue_mcp_smoke.py --allow-gamefeature-error
```

If a normal editor is running at this point, save dirty packages through UE MCP before any close/restart. Do not force-close an editor with unknown unsaved state.

- [ ] **Step 4: Verify the player-facing path in PIE.**

Use UE MCP to start PIE on `L_Main` and verify this exact sequence visually and through state probes:

1. Click Start/New Game; `WorldMap` is the active screen.
2. Click the Qingshan marker; `Town` is the active screen and Town HUD is visible.
3. Click the Town HUD companion icon; confirm fixed left categories, a visible right scrollbar, a three-column first-row-proportioned card grid, `0 / 5` or current state-accurate count, and masked undiscovered cards.
4. Accept the quest; reopen codex and confirm `引路人` is discovered with a red dot. Click it and confirm its red dot plus the HUD entry badge clear.
5. Enter a battle node; confirm the runtime discovers the scene's enemies before victory. Return safely to town through the existing failure/return path, reopen codex, and confirm discovered enemies have red dots.
6. Press Escape while codex is open; it closes without changing `Town`, world-map availability, or character panel state.

- [ ] **Step 5: Record the exact commands/results in the current task report and commit only post-verification source fixes, if any.**

If no source fix was necessary, do not make a no-op commit. If a source fix was necessary, add a regression test first, repeat its red/green cycle, rerun the full suite, then commit only that fix and its test.
