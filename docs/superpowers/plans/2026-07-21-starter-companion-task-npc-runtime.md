# Starter Companion and Task NPC Runtime Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Every player-created game begins with one persisted, randomly seeded and active permanent companion, while each accepted task route supplies its mapped temporary NPC and an intentional immutable three-card support loadout.

**Architecture:** Keep permanent-partner creation in the game-start facade so `CreateNewGame()` remains a pure baseline used by save migration and focused rules tests. Store one fresh recruit seed and the generated companion in the normal `CardRun.CompanionRoster`; synchronize it into the party selection. Resolve the current task's NPC inside the route-entry rules, set it using the existing route-local NPC API, and clear it with the existing route lifecycle. Add an explicit `DefaultRouteCardIds` catalog field so NPC default cards never depend on lexical ID order.

**Tech Stack:** Unreal Engine 5.8 C++, Unreal Automation Tests, UBT cold build (`-NoHotReload`).

---

### Task 1: Give a started game one persisted active permanent companion

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKStarterCompanionTest.cpp`

- [ ] **Step 1: Write the failing test**

Create `GameXXK.MVP.Companion.NewGameStarterRecruit`. It must instantiate a `UGameXXKMVPSubsystem`, call `StartNewGame()`, and assert all of the following against real runtime state:

```cpp
TestEqual(TEXT("new game owns exactly one starter companion"),
    State.CardRun.CompanionRoster.PermanentCompanions.Num(), 1);
TestTrue(TEXT("starter companion has a persisted random seed"),
    State.CardRun.CompanionRoster.RecruitSequenceSeed != 0);
TestTrue(TEXT("starter companion is the active permanent partner"),
    State.CardRun.CompanionRoster.PermanentCompanions[0].bIsActive);
TestEqual(TEXT("party selection points at that active companion"),
    State.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
    State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId);
```

Also save through `UGameXXKMVPRules::MakeSaveState` and restore with `RestoreFromSaveState`; assert instance ID, selected cards, active flag and recruit seed match exactly. Do not assert a particular profession or card ID.

- [ ] **Step 2: Run the test to verify it fails**

Run the focused Unreal automation test with `UnrealEditor-Cmd.exe`, `-NullRHI`, and `Automation RunTests GameXXK.MVP.Companion.NewGameStarterRecruit`. Expected: the permanent roster is empty before the feature is implemented.

- [ ] **Step 3: Implement the smallest game-start helper**

In the private namespace of `GameXXKMVPSubsystem.cpp`, add a helper used only by `StartNewGame()` that:

```cpp
FString Error;
FGameXXKCompanionRecruitResult Result;
FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error);
State.CardRun.CompanionRoster.RecruitSequenceSeed = MakeFreshNonZeroStarterRecruitSeed();
FGameXXKCompanionRules::CreateAndResolveNextRecruitment(
    State.CardRun.CompanionRoster, Result, &Error);
FGameXXKCompanionRules::SetActivePermanentCompanion(
    State.CardRun.CompanionRoster, Result.Companion.InstanceId, &Error);
FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error);
```

`MakeFreshNonZeroStarterRecruitSeed()` must produce a nonzero, non-`MIN_int32` value exactly once for a brand-new started game and the value must be stored before recruitment. It must not be called from `EnsureCardRunInitialized()`, `ContinueGameFromSlot()`, `RestoreFromSaveState()`, or companion UI actions. If any low-level call fails, `StartNewGame()` returns `false` without opening the world map.

- [ ] **Step 4: Run the focused test to verify it passes**

Repeat the Task 1 command. Expected: PASS; load/restore sees exactly the same starter companion rather than a re-roll.

### Task 2: Make NPC default route decks explicit catalog data

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp`

- [ ] **Step 1: Write the failing catalog and adapter test**

Create `GameXXK.Data.Companion.QuestNpcDefaultLoadouts`. For all six named NPC definitions, assert exactly four `FixedCardIds`, exactly three `DefaultRouteCardIds`, uniqueness, and membership of every default in the fixed set. For each NPC, call `SetQuestNpcForCurrentRun(State, NpcId, {})` and assert the saved selected IDs equal the definition's explicit defaults.

The expected route defaults are:

```cpp
// Npc.TusiChief
{ "Npc.TusiChief.ZhaiZhuHaoLing", "Npc.TusiChief.ShiMenShouShi", "Npc.TusiChief.TuSiJunLing" }
// Npc.SongJinBao
{ "Npc.SongJinBao.ErMuMiBao", "Npc.SongJinBao.ShangQianGuWu", "Npc.SongJinBao.YiNuoQianJin" }
// Npc.YueBai
{ "Npc.YueBai.QingYanDianDeng", "Npc.YueBai.CanJuanPiZhu", "Npc.YueBai.YueBaiZhaoYe" }
// Npc.ZhouGuangZu
{ "Npc.ZhouGuangZu.YiCaoBianShi", "Npc.ZhouGuangZu.HuangShanFuZhi", "Npc.ZhouGuangZu.YanFenFengMai" }
// Npc.JinGui
{ "Npc.JinGui.ShiJingErMu", "Npc.JinGui.QiaoYanZhouXuan", "Npc.JinGui.ZaYiChouBei" }
// Npc.QiongMeiEr
{ "Npc.QiongMeiEr.TengQiaoFeiDu", "Npc.QiongMeiEr.GuWuMiZong", "Npc.QiongMeiEr.YinLingZhenXin" }
```

- [ ] **Step 2: Run the test to verify it fails**

Run `Automation RunTests GameXXK.Data.Companion.QuestNpcDefaultLoadouts`. Expected: compile/test failure because `DefaultRouteCardIds` does not exist and empty NPC selection currently uses sorted `FixedCardIds`.

- [ ] **Step 3: Add immutable explicit defaults and consume them**

Add `TArray<FName> DefaultRouteCardIds` to `FGameXXKQuestNpcDefinition`. Extend the local `AddQuestNpcDefinition` factory to receive three exact IDs after collecting/sorting the four fixed cards, validate them in the test, and assign them in the six catalog definitions above. In `SetQuestNpcForCurrentRun`, replace:

```cpp
EffectiveSelection.Append(Definition->FixedCardIds.GetData(), QuestNpcSelectedCardCount);
```

with:

```cpp
EffectiveSelection = Definition->DefaultRouteCardIds;
```

Keep `QuestNpcSelectedCardCount == 3`; a task NPC remains a fixed, read-only temporary three-card contribution, not an editable permanent 12-card deck.

- [ ] **Step 4: Run the focused test to verify it passes**

Repeat the Task 2 automation command. Expected: all six definitions and their empty-selection adapter installs pass.

### Task 3: Attach the named task NPC when the accepted task enters its route

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp`

- [ ] **Step 1: Write the failing route-entry test**

Create `GameXXK.Integration.CardRoute.QingshanTaskSupport`. From a new runtime, open the world map, enter Qingshan, accept the town quest, and enter the dungeon. Assert:

```cpp
TestEqual(TEXT("Qingshan task route assigns its named temporary NPC"),
    State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));
TestEqual(TEXT("task NPC selection uses the designed deck"),
    State.CardRun.PartySelection.QuestNpc.SelectedCardIds,
    TArray<FName>({ TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("Npc.TusiChief.ShiMenShouShi"), TEXT("Npc.TusiChief.TuSiJunLing") }));
TestEqual(TEXT("task NPC never enters permanent roster"),
    State.CardRun.CompanionRoster.PermanentCompanions.Num(), 0);
```

Then enter a battle and assert the projected party contains Hero, one active permanent partner if supplied by the caller, and `Npc.TusiChief`; complete/fail the route and assert the temporary NPC is cleared. Add an event-support occupied-slot assertion showing an event NPC cannot replace `Npc.TusiChief`.

- [ ] **Step 2: Run the test to verify it fails**

Run `Automation RunTests GameXXK.Integration.CardRoute.QingshanTaskSupport`. Expected: `ActiveTemporaryQuestNpcId` is none immediately after `EnterDungeon()`.

- [ ] **Step 3: Add the explicit Qingshan task mapping**

In the `GameXXKMVP` private namespace define:

```cpp
static const FName QuestNpcTusiChiefName(TEXT("Npc.TusiChief"));

static FName ResolveRouteTaskNpc(const FGameXXKRuntimeState& State)
{
    return State.CurrentRegion == RegionQingshanName
        && State.QuestState == EGameXXKQuestState::Accepted
        && State.bFollowerJoined
        ? QuestNpcTusiChiefName
        : NAME_None;
}
```

In `EnterDungeon()`, call it after `ClearRouteLocalCardState(State)` and before `bLoadoutLockedForRoute = true`; when nonempty, call `SetQuestNpcForCurrentRun(State, MappedNpcId, {})` and fail entry if it fails. Keep existing `AcceptRouteEventNpcSupport` no-replacement rule untouched. Do not add any task NPC to `PermanentCompanions` and do not modify route cleanup.

- [ ] **Step 4: Run the focused tests to verify they pass**

Run both `GameXXK.Integration.CardRoute.QingshanTaskSupport` and existing `GameXXK.MVP.CardRoute.EventSupport`. Expected: Qingshan gets `Npc.TusiChief`; task support clears on lifecycle; existing event support still works when the task slot is empty and cannot overwrite a filled slot.

### Task 4: Cold verification and player-flow evidence

**Files:**
- Verify: modified files from Tasks 1–3

- [ ] **Step 1: Run relevant automation tests in one cold editor command**

Run the three new test prefixes plus existing companion recruitment, route lifecycle, route event support, and battle adapter tests using `UnrealEditor-Cmd.exe`, `-Unattended -NoSplash -NoSound -NullRHI`, and `-TestExit=Automation Test Queue Empty`.

- [ ] **Step 2: Cold-build the editor target**

With the editor closed and after saving any packages through UE MCP if it is running, execute:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Check whitespace and the real game flow**

Run `git diff --check`. Launch only `D:\UE5 demo\GameXXK\GameXXK.uproject`, enter Start Game → Qingshan → accept task → route. Verify party projection has an active permanent companion and `Npc.TusiChief`, and that NPC cards show the three intended tooltips. Preserve user-authored assets and do not use Live Coding, Hot Reload, `.sln`, or `dotnet` to launch the project.
