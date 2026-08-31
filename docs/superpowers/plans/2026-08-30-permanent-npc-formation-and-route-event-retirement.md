# Permanent NPC Formation and Route-Event Retirement Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make the saved Workbench formation the sole party authority, preserve its selected NPC through all lifecycle boundaries, and retire every NPC route encounter without breaking old saves.

**Architecture:** `FGameXXKCardRunState::OrderedFormation` remains the save-authoritative hero/companion/NPC membership. `FGameXXKPartyFormationRules` resolves and projects that formation for every consumer; `ActiveTemporaryQuestNpcId` becomes a migration-only tombstone. Idle travel replaces only its party projection when formation changes, while routes lock the same resolved party. NPC route-event definitions and actions become unreachable, and save version 30 repairs legacy formation/event state atomically.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT/UENUM SaveGame migration, UMG, Unreal Automation Tests, project Python source-policy/probe tests, UE 5.8 MCP, cold UBT through `scripts/ue_tdd_pipeline.py` or `scripts/ai_production_loop.py`.

---

## Execution Guardrails

- Work directly in `D:\UE5 demo\GameXXK` on `main`. Do not create or use a worktree.
- Do not use UnrealBridge. Use the UE 5.8 MCP client and project scripts.
- The worktree is already dirty, including user-tuned assets and overlapping source files. Before each edit, inspect the scoped file's existing diff. Never replace a whole dirty file and never stage unrelated hunks.
- The three user-staged deletions for the old right backpack scrollbar remain staged and must not enter any task commit.
- For dirty overlapping files, stage only task-owned hunks with interactive hunk staging or a reviewed cached patch. Before every commit, run `git diff --cached --name-status` and reject any unrelated path/hunk.
- Preserve the existing story-button, drag, animation, inventory, narrative-removal, town, and asset changes visible in the working tree.
- Before any cold build, if UE is running, let `scripts/ue_tdd_pipeline.py` save dirty packages through MCP and close safely. If MCP cannot save, stop; never force-close the editor.
- Never use Live Coding or Hot Reload as verification.
- Default PIE remains `/Game/GameXXK/Maps/L_DesktopTrainingHUD`. Load the 3D Qingshan map only for the explicitly required town round-trip acceptance.
- Do not run any automatic mouse/click driver during the final player acceptance. The final MCP/Python probe is read-only.

## File Responsibility Map

- `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
  - Public resolver/setter contract for the authoritative quest-NPC member.
- `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`
  - Exact hero + permanent companion + NPC validation, projection, and non-migration normalization.
- `Source/GameXXK/Public/GameXXKCardRunTypes.h`
  - Documents `ActiveTemporaryQuestNpcId` as a serialized migration tombstone.
- `Source/GameXXK/Public/GameXXKCardBattleAdapter.h`
- `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
  - Card/loadout compatibility uses the ordered formation; route cleanup preserves party selection.
- `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp`
- `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp`
  - Route entry, battle, merchant, balance, and settlement consume the authoritative NPC and never replace it.
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
  - Builds idle party data, awards NPC experience, applies formation changes atomically, and preserves the active idle runner.
- `Source/GameXXK/Public/GameXXKRouteEncounterCatalog.h`
- `Source/GameXXK/Private/GameXXKRouteEncounterCatalog.cpp`
  - Retires seven NPC encounter definitions while retaining explicit serialized enum ordinals.
- `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h`
- `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp`
- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
  - Removes reachable support UX/actions while keeping a no-op compatibility facade.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
  - Introduces v30, recovers the persistent NPC by the approved priority, and remaps pending removed encounters.
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
  - Displays and edits the resolved permanent NPC without empty/unlock semantics.
- `Source/GameXXK/Private/Tests/GameXXKPermanentNpcFormationTest.cpp` (new)
  - Focused authority, lifecycle, idle-runner, and Workbench contracts.
- `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp`
  - Replaced with retirement/tombstone contracts.
- `Source/GameXXK/Private/Tests/GameXXKPermanentNpcSaveMigrationTest.cpp` (new)
  - v29→v30 recovery priority, pending-event remap, and idempotence.
- Existing test fixtures returned by the final `rg` sweep
  - Migrate direct temporary-field setup/assertions to authoritative formation helpers.
- `scripts/test_permanent_npc_formation_policy.py` (new)
  - Static guard that forbids runtime temporary-NPC authority and removed support/event UX.
- `Content/Python/gamexxk_probe_training_visual_mvp.py`
  - Extend the existing read-only PIE snapshot with ordered and idle party member IDs.
- `docs/production/current-goal-acceptance.md`
  - Append only verified evidence after the implementation passes.

## Shared Verification Commands

Use a unique report directory per run so an old `index.json` cannot masquerade as fresh evidence.

```powershell
# Cold save/close/build/relaunch. Stop if MCP cannot save dirty packages.
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 200 --filter "[TDD]"

# Focused Automation after the new DLL is loaded.
python scripts/ai_production_loop.py --run-automation `
  --automation-tests "GameXXK.PartyFormation.PermanentNpcAuthority" `
  --automation-report "PermanentNpcFormation-Task1-Red"

# Parse the authoritative report rather than trusting commandlet exit text.
python scripts/parse_automation_index.py `
  --index "Saved/Automation/PermanentNpcFormation-Task1-Red/index.json"
```

Expected green evidence is a successful cold UBT plus an `index.json` with zero failed tests for the named filter. Expected red evidence is a freshly compiled test whose behavior assertion fails for the reason stated in the task; a stale DLL or missing report is not red evidence.

---

### Task 1: Establish the permanent NPC formation invariant

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcFormationTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h:288-295`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:2105-2173,2205-2273`
- Modify: `Source/GameXXK/Public/GameXXKCardBattleAdapter.h:20-30`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:3421-3493`

- [ ] **Step 1: Write the focused authority test against existing public state**

Create the test with a helper that reads the quest-NPC member from `OrderedFormation` without relying on the old temporary field:

```cpp
#include "GameXXKCompanionCatalog.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPermanentNpcFormationAuthorityTest,
	"GameXXK.PartyFormation.PermanentNpcAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPermanentNpcFormationAuthorityTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}

	FName ActiveNpcId;
	FString Error;
	TestTrue(TEXT("ordered formation resolves one permanent NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(), ActiveNpcId, &Error));
	TestEqual(TEXT("new game defaults to Tusi Chief"), ActiveNpcId, FName(TEXT("Npc.TusiChief")));
	TestTrue(TEXT("temporary route provenance is retired"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());

	for (const FGameXXKQuestNpcDefinition& Definition :
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		FGameXXKRuntimeState Candidate = Subsystem->GetRuntimeState();
		TestTrue(*FString::Printf(TEXT("%s can be selected without unlock state"),
			*Definition.NpcId.ToString()),
			FGameXXKPartyFormationRules::SetQuestNpc(Candidate, Definition.NpcId, &Error));
		FName ResolvedNpcId;
		TestTrue(TEXT("selected NPC resolves from ordered formation"),
			FGameXXKPartyFormationRules::ResolveQuestNpcId(Candidate, ResolvedNpcId, &Error));
		TestEqual(TEXT("ordered identity matches the selected catalog NPC"),
			ResolvedNpcId, Definition.NpcId);
		TestEqual(TEXT("loadout projection matches ordered identity"),
			Candidate.CardRun.PartySelection.QuestNpc.NpcId, Definition.NpcId);
		TestTrue(TEXT("selection never revives temporary provenance"),
			Candidate.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	}
	const FGameXXKRuntimeState BeforeInvalid = Subsystem->GetRuntimeStateCopy();
	FGameXXKRuntimeState InvalidCandidate = BeforeInvalid;
	TestFalse(TEXT("unknown NPC is rejected"),
		FGameXXKPartyFormationRules::SetQuestNpc(
			InvalidCandidate, TEXT("Npc.Unknown"), &Error));
	TestTrue(TEXT("unknown NPC rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&InvalidCandidate, &BeforeInvalid, PPF_None));
	return true;
}

#endif
```

- [ ] **Step 2: Add compile-only resolver/setter declarations, cold-build, and record behavior red**

Add only these declarations first:

```cpp
static bool ResolveQuestNpcId(
	const FGameXXKRuntimeState& State,
	FName& OutNpcId,
	FString* OutError = nullptr);
static bool SetQuestNpc(
	FGameXXKRuntimeState& InOutState,
	FName QuestNpcId,
	FString* OutError = nullptr);
```

Temporarily implement both to reset outputs and return `false`, then run the shared cold build and the focused test with report `PermanentNpcFormation-Task1-Red`. Expected: the new test compiles and fails because the resolver/setter return false and the legacy temporary field remains active.

- [ ] **Step 3: Refactor formation validation and projection to the approved invariant**

Implement the public resolver as an exact-one-member lookup:

```cpp
bool FGameXXKPartyFormationRules::ResolveQuestNpcId(
	const FGameXXKRuntimeState& State,
	FName& OutNpcId,
	FString* OutError)
{
	ResetError(OutError);
	OutNpcId = NAME_None;
	for (const FGameXXKPartyMemberRef& Ref : State.CardRun.OrderedFormation.Members)
	{
		if (Ref.Kind != EGameXXKPartyMemberKind::QuestNpc)
		{
			continue;
		}
		if (!OutNpcId.IsNone()
			|| !FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId))
		{
			SetError(OutError, TEXT("Ordered formation must contain exactly one approved NPC."));
			OutNpcId = NAME_None;
			return false;
		}
		OutNpcId = Ref.MemberId;
	}
	if (OutNpcId.IsNone())
	{
		SetError(OutError, TEXT("Ordered formation has no approved NPC."));
		return false;
	}
	return true;
}
```

Change the private `ResolveMember` quest-NPC branch to catalog ownership only:

```cpp
case EGameXXKPartyMemberKind::QuestNpc:
	return FGameXXKCompanionCatalog::FindQuestNpcDefinition(Ref.MemberId) != nullptr;
```

Strengthen `Validate` with exact kind counts:

```cpp
int32 HeroCount = 0;
int32 CompanionCount = 0;
int32 QuestNpcCount = 0;
switch (Ref.Kind)
{
case EGameXXKPartyMemberKind::Hero:
	++HeroCount;
	break;
case EGameXXKPartyMemberKind::PermanentCompanion:
	++CompanionCount;
	break;
case EGameXXKPartyMemberKind::QuestNpc:
	++QuestNpcCount;
	break;
default:
	SetError(OutError, TEXT("Party formation contains an invalid member kind."));
	return false;
}
if (HeroCount != 1 || CompanionCount != 1 || QuestNpcCount != 1)
{
	SetError(OutError,
		TEXT("Party formation requires exactly one hero, one permanent companion, and one NPC."));
	return false;
}
```

Rewrite `ProjectCompatibility` so ordered formation projects the active companion and NPC loadout, then clears the tombstone:

```cpp
InOutState.CardRun.PartySelection.ActivePermanentCompanionInstanceId = FirstCompanionId;
bool bAssignedActiveCompanion = false;
for (FGameXXKPermanentCompanion& Companion :
	InOutState.CardRun.CompanionRoster.PermanentCompanions)
{
	const bool bShouldBeActive = !bAssignedActiveCompanion
		&& Companion.InstanceId == FirstCompanionId;
	Companion.bIsActive = bShouldBeActive;
	bAssignedActiveCompanion |= bShouldBeActive;
}
const FGameXXKQuestNpcOwnedCardLoadout* SavedLoadout =
	InOutState.CardRun.PartySelection.QuestNpcCardLoadouts.Find(FirstQuestNpcId);
InOutState.CardRun.PartySelection.QuestNpc.NpcId = FirstQuestNpcId;
InOutState.CardRun.PartySelection.QuestNpc.SelectedCardIds =
	SavedLoadout ? SavedLoadout->SelectedCardIds : TArray<FName>();
```

`ValidateCompatibilityProjection` must require `PartySelection.QuestNpc.NpcId == FirstQuestNpcId` and `ActiveTemporaryQuestNpcId.IsNone()`.

- [ ] **Step 4: Normalize missing formations without reading the legacy tombstone**

Rewrite `BuildLegacyProjection`/`Normalize` to select, in order, a valid existing ordered member, a valid `PartySelection.QuestNpc.NpcId`, then `Npc.TusiChief`. Select one existing ordered/active/first stable owned companion, and construct exactly:

```cpp
FGameXXKOrderedPartyFormation Candidate;
AddUniqueMember(Candidate,
	MakeMember(EGameXXKPartyMemberKind::Hero, FGameXXKEquipmentRules::HeroCharacterId()));
AddUniqueMember(Candidate,
	MakeMember(EGameXXKPartyMemberKind::PermanentCompanion, CompanionId));
AddUniqueMember(Candidate,
	MakeMember(EGameXXKPartyMemberKind::QuestNpc, QuestNpcId));
```

Do not read `ActiveTemporaryQuestNpcId` here; only save migration may recover it. After a successful normalize, call `ProjectCompatibility` and validate again before committing the candidate.

- [ ] **Step 5: Implement atomic NPC selection and adapt the card compatibility setter**

Implement `SetQuestNpc` on a candidate copy:

```cpp
bool FGameXXKPartyFormationRules::SetQuestNpc(
	FGameXXKRuntimeState& InOutState,
	const FName QuestNpcId,
	FString* OutError)
{
	if (InOutState.CardRun.bLoadoutLockedForRoute
		|| InOutState.CardRun.bHasActiveCardBattle
		|| InOutState.bHasActiveBattle
		|| InOutState.bDungeonActive
		|| InOutState.Training.bChallengeActive
		|| InOutState.Screen == EGameXXKScreen::Battle)
	{
		SetError(OutError, TEXT("NPC formation cannot change during a route or battle."));
		return false;
	}
	if (!FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId))
	{
		SetError(OutError, TEXT("Selected NPC is not one of the six owned definitions."));
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Normalize(Candidate, OutError))
	{
		return false;
	}
	FGameXXKPartyMemberRef* Slot = Candidate.CardRun.OrderedFormation.Members.FindByPredicate(
		[](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
		});
	if (!Slot)
	{
		SetError(OutError, TEXT("Normalized formation has no NPC slot."));
		return false;
	}
	Slot->MemberId = QuestNpcId;
	ProjectCompatibility(Candidate);
	if (!Validate(Candidate, Candidate.CardRun.OrderedFormation, OutError)
		|| !ValidateCompatibilityProjection(Candidate, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(Candidate);
	return true;
}
```

Keep `FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun` as a compatibility API, but reject `NAME_None`, persist an explicitly supplied valid three-card selection into `QuestNpcCardLoadouts`, then call `FGameXXKPartyFormationRules::SetQuestNpc`. It must never write a non-empty `ActiveTemporaryQuestNpcId`.

In `EnsureCardRunInitialized`, remove the old “active temporary vs selection” clearing/mismatch block. If the roster already contains a permanent companion, normalize/project the permanent formation; the early card-only initialization used before starter companions exist must remain legal.

Update the `ActiveTemporaryQuestNpcId` header comment to:

```cpp
/** Serialized v29-and-earlier route-provenance tombstone. Current runtime keeps this NAME_None. */
```

- [ ] **Step 6: Make new-game startup produce the invariant directly**

After starter companions and six owned NPC loadouts exist, normalize the ordered formation and select Tusi Chief through `SetQuestNpc`. Remove any startup expectation that activating Tusi requires temporary route provenance.

- [ ] **Step 7: Cold-build and prove the focused test green**

Run the shared cold build, then `GameXXK.PartyFormation.PermanentNpcAuthority` with report `PermanentNpcFormation-Task1-Green`. Expected: zero failures; all six NPCs select successfully; Tusi is the new-game default; the tombstone remains `NAME_None`.

- [ ] **Step 8: Commit only Task 1 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "fix: make NPC formation permanent"
```

Do not commit the pre-existing scrollbar deletions or unrelated dirty hunks.

---

### Task 2: Route every gameplay consumer through the permanent formation

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcFormationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteLifecycleTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteSettlementFormationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKQingshanTaskNpcRouteTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp:350-420,540-590,3458-3468`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp:1094-1134,1945-1985,2602-2837,3025-3073,3430-3455`
- Modify: `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp:56-101`
- Modify: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp:220-250,840-870`
- Modify: `Source/GameXXK/Private/GameXXKRouteBalanceRules.cpp:400-435`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:965-976,1781-1812,1970-2018`
- Modify: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`

- [ ] **Step 1: Reverse the route-lifecycle tests before changing production**

Change the lifecycle fixture to select Yue Bai before route entry and assert preservation after defeat and abandonment:

```cpp
UGameXXKMVPSubsystem* Subsystem =
	NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
TestTrue(TEXT("route lifecycle fixture starts"), Subsystem && Subsystem->StartGame());
FGameXXKRuntimeState State = Subsystem->GetRuntimeStateCopy();
TestTrue(TEXT("Yue Bai becomes the persistent NPC"),
	FGameXXKPartyFormationRules::SetQuestNpc(State, TEXT("Npc.YueBai")));
TestTrue(TEXT("route quest is accepted"), UGameXXKMVPRules::AcceptTownQuest(State));
TestTrue(TEXT("route opens with the persistent party"), UGameXXKMVPRules::EnterDungeon(State));
const FGameXXKRuntimeState LockedBefore = State;
TestFalse(TEXT("route lock rejects NPC replacement"),
	FGameXXKPartyFormationRules::SetQuestNpc(State, TEXT("Npc.JinGui")));
TestTrue(TEXT("rejected replacement is atomic"),
	FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
		&State, &LockedBefore, PPF_None));
TestTrue(TEXT("defeat returns to town"), UGameXXKMVPRules::FailDungeonToTown(State));
FName NpcAfterDefeat;
TestTrue(TEXT("NPC still resolves after defeat"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcAfterDefeat));
TestEqual(TEXT("defeat preserves Yue Bai"), NpcAfterDefeat, FName(TEXT("Npc.YueBai")));
TestEqual(TEXT("NPC loadout projection survives"),
	State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.YueBai")));
TestTrue(TEXT("temporary provenance stays retired"),
	State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
```

Rewrite settlement-formation assertions that formerly expected NPC replacement. Snapshot `OrderedFormation`, `QuestNpc`, all six loadouts, and all six progressions before settlement, then require exact equality afterward. Rewrite the Qingshan support test as “route uses selected permanent NPC and cleanup preserves it”; remove acceptance of event support.

Add `GameXXK.Training.PermanentNpcChallengeLifecycle` to `GameXXKPermanentNpcFormationTest.cpp`:

```cpp
UGameXXKMVPSubsystem* Challenge =
	NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
TestTrue(TEXT("challenge fixture starts"), Challenge && Challenge->StartGame());
TestTrue(TEXT("challenge fixture selects Yue Bai"),
	Challenge->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
const FName StageId =
	FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
TestTrue(TEXT("challenge route starts"), Challenge->StartTrainingChallenge(StageId));
TestFalse(TEXT("challenge route rejects NPC replacement"),
	Challenge->SelectTownQuestNpcForParty(TEXT("Npc.JinGui")));
FName NpcDuringChallenge;
TestTrue(TEXT("challenge resolves its frozen NPC"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(
		Challenge->GetRuntimeState(), NpcDuringChallenge));
TestEqual(TEXT("challenge keeps Yue Bai"),
	NpcDuringChallenge, FName(TEXT("Npc.YueBai")));
TestTrue(TEXT("challenge can return to Workbench"),
	Challenge->CancelTrainingChallengeToWorkbench());
FName NpcAfterCancel;
TestTrue(TEXT("cancelled challenge still resolves NPC"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(
		Challenge->GetRuntimeState(), NpcAfterCancel));
TestEqual(TEXT("challenge cancellation preserves Yue Bai"),
	NpcAfterCancel, FName(TEXT("Npc.YueBai")));
```

- [ ] **Step 2: Cold-build and capture lifecycle red**

Run the shared cold build and these filters with report `PermanentNpcFormation-Task2-Red`:

```text
GameXXK.Integration.CardRoute.Lifecycle
GameXXK.Route.Settlement.Formation
GameXXK.Integration.CardRoute.QingshanTaskNpc
GameXXK.Training.PermanentNpcChallengeLifecycle
```

Expected: at least one fresh behavior failure because route cleanup still clears/replaces the NPC.

- [ ] **Step 3: Make route-local cleanup party-neutral**

Reduce `ClearRouteLocalCardState` to route-owned state:

```cpp
void FGameXXKCardBattleAdapter::ClearRouteLocalCardState(FGameXXKRuntimeState& InOutState)
{
	FGameXXKCardRunState& Run = InOutState.CardRun;
	Run.bLoadoutLockedForRoute = false;
	ClearActiveCardBattle(InOutState);
	Run.PendingEvent = FGameXXKPendingRouteEvent();
	Run.RouteMerchant = FGameXXKRouteMerchantState();
	FGameXXKRelicRules::ClearRouteRelics(InOutState);
}
```

Do not reset `PartySelection.QuestNpc` or `OrderedFormation`.

Update the public adapter comment to: `Clears battle, reward, merchant, relic, and pending-event route state while preserving the authoritative party and every owned loadout/progression.` Update `CreateRouteEventOffer` documentation so it describes deterministic environmental/chest identity only and contains no temporary-support promise.

In `EnterDungeon`, remove the save/clear/restore sequence around `SelectedTownNpcId`. Initialize route state, set `bLoadoutLockedForRoute = true`, and validate the existing ordered formation without reselecting an NPC.

When `StartTrainingChallenge` generates its route map, set `Candidate.CardRun.bLoadoutLockedForRoute = true` before commit. Ensure `CancelTrainingChallengeToWorkbench`/terminal challenge settlement clears that lock through the existing battle-projection cleanup while preserving formation. Extend `IsCompanionConfigurationLocked` to reject edits when `Training.bChallengeActive` or `bDungeonActive` is true; ordinary idle travel remains editable.

In settlement, delete the unavailable-NPC repair block. After route-local cleanup, validate the unchanged formation and compatibility projection directly:

```cpp
FGameXXKCardBattleAdapter::ClearRouteLocalCardState(Candidate);
Candidate.CardRun.RouteProgress = FGameXXKRouteProgress();
FGameXXKRouteEconomyRules::ClearRouteEconomy(Candidate.CardRun);
Candidate.CardRun.PendingSettlement = FGameXXKRouteSettlementReceipt();
if (!FGameXXKPartyFormationRules::Validate(
		Candidate, Candidate.CardRun.OrderedFormation, OutError)
	|| !FGameXXKPartyFormationRules::ValidateCompatibilityProjection(Candidate, OutError))
{
	return false;
}
```

- [ ] **Step 4: Replace production reads of the temporary field**

At each card-deck, route-merchant, route-balance, and battle projection read, use the same checked pattern:

```cpp
FName ActiveNpcId;
if (!FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ActiveNpcId, OutError))
{
	return false;
}
```

For functions without an error parameter, resolve into a local `FString Error` and return their existing failure value. Card instance construction must use `PartySelection.QuestNpc.SelectedCardIds` only after verifying its `NpcId` equals the resolved ordered NPC.

Update the following concrete consumers:

- card-run combat-unit/card-instance construction in `GameXXKCardBattleAdapter.cpp`;
- NPC merchant owner/loadout projection in `GameXXKRouteMerchantRules.cpp`;
- route-balance fixture setup in `GameXXKRouteBalanceRules.cpp`;
- runtime turn-order/party views in `GameXXKMVPRules.cpp`.

Delete `RepairUnavailableQuestNpcSlotsPreservingOrder` after its settlement caller is gone. Keep `InsertOrReplaceCurrentQuestNpcPreservingOrder` only until Task 4 removes its last event-support caller.

- [ ] **Step 5: Preserve the formation through every terminal route outcome**

For clear, defeat, and abandonment, compare candidate formation before and after settlement and allow only route-owned state to change. Do not call `ProjectCompatibility` as a way to repair a missing NPC at route exit; the formation must already validate.

Add assertions for:

```cpp
TestEqual(TEXT("route clear preserves NPC"), ResolvedNpcAfter, ResolvedNpcBefore);
TestTrue(TEXT("ordered formation is unchanged"),
	FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
		&After.CardRun.OrderedFormation,
		&Before.CardRun.OrderedFormation,
		PPF_None));
```

- [ ] **Step 6: Cold-build and run the route-focused suites green**

Run the shared cold build, then:

```text
GameXXK.Integration.CardRoute.Lifecycle
GameXXK.Route.Settlement.Formation
GameXXK.Integration.CardRoute.QingshanTaskNpc
GameXXK.Training.PermanentNpcChallengeLifecycle
GameXXK.Route.ThreeChapter
GameXXK.Route.Merchant
GameXXK.Integration.CardBattle
```

Use report `PermanentNpcFormation-Task2-Green`. Expected: zero failures in these filters, and no assertion expects route cleanup to remove an NPC.

- [ ] **Step 7: Commit only Task 2 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "fix: preserve NPC across route lifecycle"
```

---

### Task 3: Make idle travel and experience use the same NPC without resetting progress

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcFormationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyProgressionAndLevelGateTest.cpp:155-237`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:58-135,360-510,5310-5482`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h:623-633,644-658`

- [ ] **Step 1: Add a failing active-idle swap test**

Extend the new test file with a second Automation test:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPermanentNpcIdleSwapTest,
	"GameXXK.Training.PermanentNpcIdleSwapPreservesProgress",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPermanentNpcIdleSwapTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FName StageId =
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	if (!TestTrue(TEXT("idle travel starts"), Subsystem->StartTrainingTravel(StageId)))
	{
		return false;
	}
	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	TestTrue(TEXT("idle runner advances before swap"),
		Subsystem->AdvanceTrainingTravelStep(
			bEncounterCompleted, bStageCompleted, bDefeated, Reward, 1));

	const FGameXXKRuntimeState StateBefore = Subsystem->GetRuntimeState();
	const FGameXXKTrainingTravelRuntime RuntimeBefore =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("Yue Bai selection succeeds during idle travel"),
		Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	const FGameXXKRuntimeState& StateAfter = Subsystem->GetRuntimeState();
	const FGameXXKTrainingTravelRuntime RuntimeAfter =
		Subsystem->GetTrainingTravelRuntimeCopy();

	TestEqual(TEXT("stage is preserved"), RuntimeAfter.StageId, RuntimeBefore.StageId);
	TestEqual(TEXT("encounter index is preserved"), RuntimeAfter.EncounterIndex, RuntimeBefore.EncounterIndex);
	TestEqual(TEXT("phase is preserved"), RuntimeAfter.Phase, RuntimeBefore.Phase);
	TestEqual(TEXT("walk progress is preserved"), RuntimeAfter.WalkStep, RuntimeBefore.WalkStep);
	TestEqual(TEXT("enemy count is preserved"),
		RuntimeAfter.Enemies.Num(), RuntimeBefore.Enemies.Num());
	for (int32 Index = 0;
		Index < RuntimeBefore.Enemies.Num() && Index < RuntimeAfter.Enemies.Num();
		++Index)
	{
		TestEqual(TEXT("enemy identity is preserved"),
			RuntimeAfter.Enemies[Index].EnemyDefinitionId,
			RuntimeBefore.Enemies[Index].EnemyDefinitionId);
		TestEqual(TEXT("enemy HP is preserved"),
			RuntimeAfter.Enemies[Index].HP, RuntimeBefore.Enemies[Index].HP);
		TestEqual(TEXT("enemy max HP is preserved"),
			RuntimeAfter.Enemies[Index].MaxHP, RuntimeBefore.Enemies[Index].MaxHP);
		TestEqual(TEXT("enemy attack is preserved"),
			RuntimeAfter.Enemies[Index].Attack, RuntimeBefore.Enemies[Index].Attack);
	}
	TestEqual(TEXT("normal chest timer is preserved"),
		StateAfter.Training.TravelNormalChestCooldownRemainingSeconds,
		StateBefore.Training.TravelNormalChestCooldownRemainingSeconds);
	TestEqual(TEXT("advanced chest timer is preserved"),
		StateAfter.Training.TravelAdvancedChestCooldownRemainingSeconds,
		StateBefore.Training.TravelAdvancedChestCooldownRemainingSeconds);
	TestEqual(TEXT("pending reward gold is preserved"),
		StateAfter.Training.PendingTravelGold, StateBefore.Training.PendingTravelGold);
	TestTrue(TEXT("complete Training progress and reward ledger are preserved"),
		FGameXXKTrainingProgress::StaticStruct()->CompareScriptStruct(
			&StateAfter.Training, &StateBefore.Training, PPF_None));
	TestTrue(TEXT("idle party still has three members"), RuntimeAfter.PartyUnits.Num() == 3);
	TestEqual(TEXT("idle third member becomes Yue Bai"),
		RuntimeAfter.PartyUnits[2].UnitId, FName(TEXT("Npc.YueBai")));
	const FGameXXKRuntimeState BeforeRejected = Subsystem->GetRuntimeStateCopy();
	const FGameXXKTrainingTravelRuntime RuntimeBeforeRejected =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestFalse(TEXT("invalid idle NPC replacement is rejected"),
		Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.Unknown")));
	TestTrue(TEXT("rejected idle replacement preserves runtime state"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(), &BeforeRejected, PPF_None));
	const FGameXXKTrainingTravelRuntime RuntimeAfterRejected =
		Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("rejected idle replacement preserves runner"),
		FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
			&RuntimeAfterRejected, &RuntimeBeforeRejected, PPF_None));
	return true;
}
```

- [ ] **Step 2: Cold-build and capture the reset red**

Run the shared cold build and `GameXXK.Training.PermanentNpcIdleSwapPreservesProgress` with report `PermanentNpcFormation-Task3-Red`. Expected: current `InitializeTravelRunner` usage resets phase/encounter/wave state when the NPC changes.

- [ ] **Step 3: Remove private NPC fallbacks from idle party and experience**

Replace `ResolveDeployedQuestNpcId` with a checked wrapper over the shared resolver:

```cpp
static bool ResolveDeployedQuestNpcId(
	const FGameXXKRuntimeState& State,
	FName& OutNpcId,
	FString* OutError = nullptr)
{
	return FGameXXKPartyFormationRules::ResolveQuestNpcId(State, OutNpcId, OutError);
}
```

`ApplyTrainingExperienceToDeployedParty` and `BuildTrainingTravelParty` must fail atomically when resolution fails. Delete both hard-coded `Npc.TusiChief` runtime fallbacks. Continue deriving NPC level from `QuestNpcProgressions`, attributes from the catalog, equipment from `BuildLoadoutSnapshot`, and combat values from the existing talent projection.

- [ ] **Step 4: Add a party-only idle runtime replacement helper**

Implement this next to `SynchronizeTrainingTravelPartyProgression`:

```cpp
static bool ReplaceTrainingTravelPartyPreservingProgress(
	const FGameXXKRuntimeState& State,
	FGameXXKTrainingTravelRuntime& InOutRuntime)
{
	TArray<FGameXXKTrainingTravelPartyUnitRuntime> FreshParty;
	if (!BuildTrainingTravelParty(State, FreshParty) || FreshParty.Num() != 3)
	{
		return false;
	}
	if (!State.Training.bTravelActive)
	{
		return true;
	}
	if (InOutRuntime.PartyUnits.Num() != 3)
	{
		return false;
	}
	for (int32 SlotIndex = 0; SlotIndex < FreshParty.Num(); ++SlotIndex)
	{
		FGameXXKTrainingTravelPartyUnitRuntime& Fresh = FreshParty[SlotIndex];
		const FGameXXKTrainingTravelPartyUnitRuntime& Previous =
			InOutRuntime.PartyUnits[SlotIndex];
		if (Fresh.UnitId == Previous.UnitId)
		{
			const int32 MissingHealth = FMath::Max(0, Previous.MaxHP - Previous.HP);
			Fresh.HP = FMath::Clamp(Fresh.MaxHP - MissingHealth, 0, Fresh.MaxHP);
		}
	}
	InOutRuntime.PartyUnits = MoveTemp(FreshParty);
	InOutRuntime.PlayerHP = InOutRuntime.PartyUnits[0].HP;
	InOutRuntime.PlayerMaxHP = InOutRuntime.PartyUnits[0].MaxHP;
	InOutRuntime.PlayerAttack = InOutRuntime.PartyUnits[0].Attack;
	InOutRuntime.ActivePartyIndex = FMath::Clamp(
		InOutRuntime.ActivePartyIndex, INDEX_NONE, InOutRuntime.PartyUnits.Num() - 1);
	InOutRuntime.NextEnemyTargetPartyIndex = FMath::Clamp(
		InOutRuntime.NextEnemyTargetPartyIndex, 0, InOutRuntime.PartyUnits.Num() - 1);
	return true;
}
```

This function changes no stage, encounter, enemy, phase, walk, cooldown, or reward field.

- [ ] **Step 5: Use the helper for companion and NPC formation changes**

In both `SetActivePermanentCompanion` and `SelectTownQuestNpcForParty`:

1. mutate a copied `FGameXXKRuntimeState`;
2. validate and project ordered formation;
3. copy `TrainingTravelRuntime`;
4. call `ReplaceTrainingTravelPartyPreservingProgress` only when travel is active;
5. commit state and runtime together only after every step succeeds.

Replace the NPC setter's call to `SetQuestNpcForCurrentRun` with `FGameXXKPartyFormationRules::SetQuestNpc`. Update the public comment from “temporary named NPC” to “one of the six permanently owned NPCs”.

- [ ] **Step 6: Prove experience goes to the displayed NPC**

Extend `FGameXXKTrainingDeployedPartyExperienceTest` so both online and offline fixtures select Yue Bai before applying their existing deterministic reward paths. For the online half, use the existing `AdvanceTrainingTravelEncounter` call:

```cpp
TestTrue(TEXT("online fixture selects Yue Bai"),
	Online->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
const FGameXXKCompanionPartySelection& PartyBefore =
	Online->GetRuntimeState().CardRun.PartySelection;
const int32 YueBefore =
	PartyBefore.QuestNpcProgressions.FindChecked(TEXT("Npc.YueBai")).Experience;
const int32 TusiBefore =
	PartyBefore.QuestNpcProgressions.FindChecked(TEXT("Npc.TusiChief")).Experience;
bool bStageCompleted = false;
FGameXXKTrainingReward OnlineReward;
TestTrue(TEXT("one online Training encounter settles"),
	Online->AdvanceTrainingTravelEncounter(bStageCompleted, OnlineReward));
const FGameXXKCompanionPartySelection& PartyAfter =
	Online->GetRuntimeState().CardRun.PartySelection;
TestTrue(TEXT("selected NPC gains XP"),
	PartyAfter.QuestNpcProgressions.FindChecked(TEXT("Npc.YueBai")).Experience > YueBefore);
TestEqual(TEXT("Tusi does not receive fallback XP"),
	PartyAfter.QuestNpcProgressions.FindChecked(TEXT("Npc.TusiChief")).Experience,
	TusiBefore);
```

For the offline half, select Yue Bai before `SimulateTrainingTravelOffline(512, OfflineReward)`, collect through `CollectTrainingTravelRewards`, then apply the same Yue-Bai-increased/Tusi-unchanged assertions after collection.

- [ ] **Step 7: Cold-build and run idle/progression tests green**

Run:

```text
GameXXK.Training.PermanentNpcIdleSwapPreservesProgress
GameXXK.Training.PartyProgression.DeployedTrioExperience
GameXXK.MVP.OwnedQuestNpcLoadouts
GameXXK.MVP.Companion
```

Use report `PermanentNpcFormation-Task3-Green`. Expected: the party ID changes, travel progress remains byte/field-equivalent outside party slots, and XP follows the same NPC.

- [ ] **Step 8: Commit only Task 3 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "fix: preserve idle progress during party changes"
```

---

### Task 4: Retire NPC route encounters and every reachable support action

**Files:**
- Replace contract: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKRouteEncounterCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKRouteEncounterCatalog.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKRouteEncounterPanelWidget.h:19-35`
- Modify: `Source/GameXXK/Private/UI/GameXXKRouteEncounterPanelWidget.cpp:152-160,590-687`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h:901-907`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp:2602-2709,2771-2837`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h:423-432`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp:4643-4683`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp:1478-1516`
- Modify: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`

- [ ] **Step 1: Replace old support tests with retirement contracts**

Keep the test file path so build integration remains stable, but replace support-acceptance assertions with these tests:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcRouteEventCatalogRetirementTest,
	"GameXXK.Route.Event.NpcCatalogRetired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcRouteEventCatalogRetirementTest::RunTest(const FString& Parameters)
{
	const TSet<FName> RemovedIds = {
		TEXT("Encounter.Event.TusiChief"),
		TEXT("Encounter.Event.SongJinBao"),
		TEXT("Encounter.Event.YueBai"),
		TEXT("Encounter.Event.ZhouGuangZu"),
		TEXT("Encounter.Event.JinGui"),
		TEXT("Encounter.Event.QiongMeiEr"),
		TEXT("Encounter.Event.NiuHuan")};
	const TArray<const FGameXXKRouteEncounterDefinition*> Events =
		FGameXXKRouteEncounterCatalog::GetDefinitionsOfKind(
			EGameXXKRouteEncounterKind::Event);
	TestEqual(TEXT("only one environmental event remains"), Events.Num(), 1);
	for (const FGameXXKRouteEncounterDefinition* Event : Events)
	{
		TestNotNull(TEXT("event definition exists"), Event);
		if (!Event) continue;
		TestFalse(TEXT("removed NPC event cannot be generated"), RemovedIds.Contains(Event->Id));
		TestEqual(TEXT("remaining event is Mountain Spring"),
			Event->Id, FName(TEXT("Encounter.Event.MountainSpring")));
		for (const FGameXXKRouteEncounterChoiceDefinition& Choice : Event->Choices)
		{
			TestTrue(TEXT("event choice is route-attribute only"),
				Choice.RewardKind == EGameXXKRouteEncounterRewardKind::RouteAttribute);
			TestTrue(TEXT("event choice has no NPC owner"), Choice.QuestNpcId.IsNone());
		}
	}
	TestEqual(TEXT("route attribute ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::RouteAttribute), uint8(0));
	TestEqual(TEXT("temporary support tombstone ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport), uint8(1));
	TestEqual(TEXT("relic ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::Relic), uint8(2));
	return true;
}
```

Add a no-op facade test:

```cpp
FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
const FGameXXKRuntimeState Before = State;
TestFalse(TEXT("retired support facade always rejects"),
	UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
TestTrue(TEXT("retired facade is observational"),
	FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &Before, PPF_None));
```

- [ ] **Step 2: Cold-build and capture catalog/support red**

Run `GameXXK.Route.Event.NpcCatalogRetired` and the facade test with report `PermanentNpcFormation-Task4-Red`. Expected: seven NPC definitions still exist and the support facade can still mutate a route.

- [ ] **Step 3: Freeze enum ordinals and remove NPC definitions from generation**

Make serialized values explicit:

```cpp
enum class EGameXXKRouteEncounterRewardKind : uint8
{
	RouteAttribute = 0,
	TemporaryNpcSupport = 1 UMETA(Hidden), // Serialized tombstone; never emitted.
	Relic = 2
};
```

Add a compatibility lookup:

```cpp
static bool IsRetiredNpcEncounterId(FName EncounterId);
```

Implement it with the exact seven IDs, but remove their full definitions from `BuildDefinitions`. Delete `NpcSupportChoice`. `BuildDefinitions` must return Mountain Spring plus the four chest definitions only.

- [ ] **Step 4: Remove support resolution while retaining a no-op facade**

Delete the `TemporaryNpcSupport` branch from `ResolveRouteEncounterChoice`; event choices now accept only `RouteAttribute`, while chest choices retain `Relic` handling.

Replace both facade implementations with no mutation:

```cpp
bool UGameXXKMVPRules::AcceptRouteEventNpcSupport(FGameXXKRuntimeState& State)
{
	return false;
}

bool UGameXXKMVPSubsystem::AcceptRouteEventNpcSupport()
{
	return false;
}
```

Mark their header comments as deprecated serialized-Blueprint compatibility. Remove `InsertOrReplaceCurrentQuestNpcPreservingOrder` after the support branch is gone.

- [ ] **Step 5: Preserve UI-action ordinal but remove every reachable binding**

Assign explicit action values and hide the old slot:

```cpp
enum class EGameXXKRouteEncounterAction : uint8
{
	None = 0,
	AcceptTaskNpcSupport = 1 UMETA(Hidden),
	TakeGold = 2,
	TakeHealingPowder = 3,
	CampRest = 4,
	CampTakeHealingPowder = 5,
	MerchantLeave = 6,
	SelectChoice0 = 7,
	SelectChoice1 = 8,
	SelectChoice2 = 9,
	ClosePanel = 10,
	CampTakeLifeSavingTalisman = 11,
	CampTakeRouteMoney = 12
};
```

In the route panel:

- delete `TaskNpcDisplayName`;
- delete support-slot occupancy logic and support tooltips;
- delete the fallback presentation that offers `邀请…支援`;
- present only catalog choices, legacy non-NPC event rewards, camp, merchant, and chest content.

In the player controller:

- remove `bTaskNpcEvent`;
- remove the `AcceptTaskNpcSupport` dispatch case, or leave only a `bResolved = false` tombstone case with no subsystem call;
- allow ordinary `TakeGold` without the old `!bTaskNpcEvent` gate;
- remove now-unused companion-catalog includes.

Run a focused binary/text reference scan before deciding anything about the retained facade:

```powershell
rg -a -n -S "AcceptRouteEventNpcSupport|AcceptTaskNpcSupport" Content Config Source
```

Regardless of scan result, keep the no-op facade and enum/action tombstones for v30 as specified. Record any Blueprint asset hit for the later cleanup boundary; do not edit that asset in this task.

- [ ] **Step 6: Cold-build and run encounter/UI tests green**

Run:

```text
GameXXK.Route.Event.NpcCatalogRetired
GameXXK.MVP.RouteEncounter.Panel
GameXXK.Route.EncounterSceneActor
GameXXK.MVP.Flow
```

Use report `PermanentNpcFormation-Task4-Green`. Expected: no generated NPC event, no reachable support action, Mountain Spring and chests still resolve, and the no-op facade preserves state.

- [ ] **Step 7: Commit only Task 4 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "feat: retire NPC route encounters"
```

---

### Task 5: Add save version 30 formation recovery and pending-event remap

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h:44-61`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp:1515-1808,1920-2000`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [ ] **Step 1: Write v29→v30 recovery-priority tests**

Create a helper that returns the ordered NPC and a table-driven test with four v29 inputs:

```cpp
struct FRecoveryCase
{
	FName OrderedNpcId;
	FName SelectionNpcId;
	FName TemporaryNpcId;
	FName ExpectedNpcId;
};

const FRecoveryCase Cases[] = {
	{TEXT("Npc.YueBai"), TEXT("Npc.JinGui"), TEXT("Npc.QiongMeiEr"), TEXT("Npc.YueBai")},
	{NAME_None, TEXT("Npc.JinGui"), TEXT("Npc.QiongMeiEr"), TEXT("Npc.JinGui")},
	{NAME_None, NAME_None, TEXT("Npc.QiongMeiEr"), TEXT("Npc.QiongMeiEr")},
	{NAME_None, NAME_None, NAME_None, TEXT("Npc.TusiChief")}};

for (const FRecoveryCase& Case : Cases)
{
	UGameXXKMVPSubsystem* Fixture =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("migration fixture starts"), Fixture && Fixture->StartGame());
	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(Fixture->GetRuntimeState());
	Source.SaveVersion = 29;
	SetOrderedNpcOrRemove(Source.RuntimeState, Case.OrderedNpcId);
	SetLegacySelectionOrClear(Source.RuntimeState, Case.SelectionNpcId);
	Source.RuntimeState.CardRun.ActiveTemporaryQuestNpcId = Case.TemporaryNpcId;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v29 source migrates"),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	FName ResolvedNpcId;
	TestTrue(TEXT("migrated ordered NPC resolves"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Migrated.RuntimeState, ResolvedNpcId));
	TestEqual(TEXT("recovery order selects the approved identity"),
		ResolvedNpcId, Case.ExpectedNpcId);
	TestTrue(TEXT("legacy temporary field is cleared"),
		Migrated.RuntimeState.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("migration targets v30"), Migrated.SaveVersion, 30);
}
```

Implement the two commented test setup operations with explicit helpers in the same test file:

```cpp
void SetOrderedNpcOrRemove(FGameXXKRuntimeState& State, const FName NpcId)
{
	FGameXXKPartyMemberRef* Ref = State.CardRun.OrderedFormation.Members.FindByPredicate(
		[](const FGameXXKPartyMemberRef& Member)
		{
			return Member.Kind == EGameXXKPartyMemberKind::QuestNpc;
		});
	if (Ref && NpcId.IsNone())
	{
		State.CardRun.OrderedFormation.Members.RemoveSingle(*Ref);
	}
	else if (Ref)
	{
		Ref->MemberId = NpcId;
	}
}

void SetLegacySelectionOrClear(FGameXXKRuntimeState& State, const FName NpcId)
{
	State.CardRun.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
	if (NpcId.IsNone()) return;
	const FGameXXKQuestNpcOwnedCardLoadout* Loadout =
		State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(NpcId);
	if (Loadout)
	{
		State.CardRun.PartySelection.QuestNpc.NpcId = NpcId;
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds = Loadout->SelectedCardIds;
	}
}
```

- [ ] **Step 2: Write removed-pending-event migration tests**

For every retired encounter ID, create a v29 save in `RouteEvent` with stable node/seed and Yue Bai selected. After migration require:

```cpp
TestEqual(TEXT("pending node is preserved"),
	Migrated.RuntimeState.CardRun.PendingEvent.SourceNodeId, 71);
TestEqual(TEXT("choice seed is preserved"),
	Migrated.RuntimeState.CardRun.PendingEvent.ChoiceSeed, 0x7135);
TestEqual(TEXT("removed event remaps to Mountain Spring"),
	Migrated.RuntimeState.CardRun.PendingEvent.EncounterId,
	FName(TEXT("Encounter.Event.MountainSpring")));
TestEqual(TEXT("environment presentation identity is installed"),
	Migrated.RuntimeState.CardRun.PendingEvent.EventNpcId,
	FName(TEXT("Event.Attribute.MountainSpring")));
TestEqual(TEXT("screen remains an unresolved event"),
	Migrated.RuntimeState.Screen, EGameXXKScreen::RouteEvent);
TestEqual(TEXT("no reward is granted"),
	Migrated.RuntimeState.PlayerGold, Source.RuntimeState.PlayerGold);
TestTrue(TEXT("route attributes are unchanged"),
	FGameXXKRouteAttributeBonuses::StaticStruct()->CompareScriptStruct(
		&Migrated.RuntimeState.CardRun.RouteAttributeBonuses,
		&Source.RuntimeState.CardRun.RouteAttributeBonuses,
		PPF_None));
TestEqual(TEXT("inventory is unchanged"),
	Migrated.RuntimeState.Inventory, Source.RuntimeState.Inventory);
TestTrue(TEXT("pending settlement is unchanged"),
	FGameXXKRouteSettlementReceipt::StaticStruct()->CompareScriptStruct(
		&Migrated.RuntimeState.CardRun.PendingSettlement,
		&Source.RuntimeState.CardRun.PendingSettlement,
		PPF_None));
TestEqual(TEXT("route node remains unresolved"),
	Migrated.RuntimeState.PendingRouteNodeId, Source.RuntimeState.PendingRouteNodeId);
```

Also migrate the already-migrated v30 result again and assert `CompareScriptStruct` equality.

Add one pre-catalog legacy case with `EncounterId = NAME_None` and `EventNpcId = Npc.YueBai`, plus one with `EventNpcId = Npc.Event.NiuHuan`; both must follow the same Mountain Spring remap without settling or rewarding.

- [ ] **Step 3: Cold-build and capture migration red**

Run `GameXXK.MVP.SaveGame.PermanentNpcV30Migration` with report `PermanentNpcFormation-Task5-Red`. Expected: target version remains 29, temporary recovery is absent, or removed pending event IDs fail validation/remain unresolved without a catalog definition.

- [ ] **Step 4: Declare v30 and add one atomic migration helper**

In the header:

```cpp
/** v30: ordered formation always owns one permanent NPC; NPC route encounters are retired. */
static constexpr int32 PermanentNpcFormationIntroducedSaveVersion = 30;
static constexpr int32 CurrentSaveVersion = 30;
```

In the migration source, add a helper that runs on a candidate state:

```cpp
bool MigratePermanentNpcFormation(
	FGameXXKRuntimeState& State,
	FGameXXKSaveMigrationReport& Report,
	FString& OutError)
{
	FName OrderedNpcId;
	const bool bHasOrderedNpc =
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, OrderedNpcId);
	const FName SelectionNpcId = State.CardRun.PartySelection.QuestNpc.NpcId;
	const FName TemporaryNpcId = State.CardRun.ActiveTemporaryQuestNpcId;
	FName RecoveredNpcId = bHasOrderedNpc ? OrderedNpcId : NAME_None;
	if (RecoveredNpcId.IsNone()
		&& FGameXXKCompanionCatalog::FindQuestNpcDefinition(SelectionNpcId))
	{
		RecoveredNpcId = SelectionNpcId;
	}
	if (RecoveredNpcId.IsNone()
		&& FGameXXKCompanionCatalog::FindQuestNpcDefinition(TemporaryNpcId))
	{
		RecoveredNpcId = TemporaryNpcId;
	}
	if (RecoveredNpcId.IsNone())
	{
		RecoveredNpcId = TEXT("Npc.TusiChief");
		Report.Warnings.Add(TEXT("Missing legacy NPC formation repaired to Tusi Chief."));
	}
	State.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
	const FGameXXKQuestNpcOwnedCardLoadout* RecoveredLoadout =
		State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(RecoveredNpcId);
	if (!RecoveredLoadout)
	{
		OutError = TEXT("Recovered NPC has no persisted owned loadout.");
		return false;
	}
	State.CardRun.PartySelection.QuestNpc.NpcId = RecoveredNpcId;
	State.CardRun.PartySelection.QuestNpc.SelectedCardIds =
		RecoveredLoadout->SelectedCardIds;
	if (!FGameXXKPartyFormationRules::Normalize(State, &OutError))
	{
		return false;
	}
	FGameXXKPartyMemberRef* NpcSlot =
		State.CardRun.OrderedFormation.Members.FindByPredicate(
			[](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
			});
	if (!NpcSlot)
	{
		OutError = TEXT("Normalized migrated formation has no NPC slot.");
		return false;
	}
	NpcSlot->MemberId = RecoveredNpcId;
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	if (!FGameXXKPartyFormationRules::Validate(
			State, State.CardRun.OrderedFormation, &OutError)
		|| !FGameXXKPartyFormationRules::ValidateCompatibilityProjection(State, &OutError))
	{
		return false;
	}
	return true;
}
```

Run six-loadout/progression normalization before this helper so the recovered identity always has a valid persisted loadout to project.

- [ ] **Step 5: Remap retired pending events without settling or rewarding**

Add:

```cpp
bool MigrateRetiredNpcEncounter(FGameXXKRuntimeState& State, FString& OutError)
{
	FGameXXKPendingRouteEvent& Pending = State.CardRun.PendingEvent;
	const bool bRetiredCatalogId =
		FGameXXKRouteEncounterCatalog::IsRetiredNpcEncounterId(Pending.EncounterId);
	const bool bLegacyRosterNpc =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(Pending.EventNpcId) != nullptr;
	const bool bLegacyNiuHuan = Pending.EventNpcId == TEXT("Npc.Event.NiuHuan");
	if (!bRetiredCatalogId && !bLegacyRosterNpc && !bLegacyNiuHuan)
	{
		return true;
	}
	const FGameXXKRouteEncounterDefinition* Replacement =
		FGameXXKRouteEncounterCatalog::ChooseDeterministic(
			EGameXXKRouteEncounterKind::Event, Pending.ChoiceSeed);
	if (!Replacement)
	{
		Pending = FGameXXKPendingRouteEvent();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		return true;
	}
	Pending.EncounterId = Replacement->Id;
	Pending.EventNpcId = Replacement->EventNpcId;
	Pending.bCanRecruitPermanentCompanion = false;
	return true;
}
```

Do not touch `SourceNodeId`, `ChoiceSeed`, `PendingRouteNodeId`, route receipts, player currency, inventory, formation, or rewards.

- [ ] **Step 6: Invoke v30 normalization on every dispatcher path**

Call both helpers:

- for v29-and-earlier general migration before final validation;
- inside the special v24 upgrade path before validation/return;
- inside the v30 current-save normalization path so a malformed current save is repaired only at the load boundary.

Change `ValidateRuntimeState` to require `ActiveTemporaryQuestNpcId.IsNone()` and the new compatibility projection. Remove the old equality check between temporary provenance and `PartySelection.QuestNpc`.

Update existing save tests that hard-code version 29 to use `CurrentSaveVersion` where they mean “current”, and keep explicit 29 only where they test the new boundary.

- [ ] **Step 7: Cold-build and run migration suites green**

Run:

```text
GameXXK.MVP.SaveGame.PermanentNpcV30Migration
GameXXK.MVP.SaveGame
GameXXK.Equipment.SaveMigration
GameXXK.Training.SaveValidation
```

Use report `PermanentNpcFormation-Task5-Green`. Expected: zero failures, v29 targets v30, all four recovery cases pass, each removed event remaps without a reward, and current-state normalization is idempotent.

- [ ] **Step 8: Commit only Task 5 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "feat: migrate permanent NPC formations"
```

---

### Task 6: Align Workbench presentation and town travel with the authoritative NPC

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPermanentNpcFormationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp:5969-6209`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp:2047-2087,5620-5650,6208-6333`
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h:227-237`
- Create: `scripts/test_permanent_npc_formation_policy.py`

- [ ] **Step 1: Add failing Workbench identity and no-empty-state assertions**

Rename the existing NPC portrait Automation path to `GameXXK.DesktopTraining.Workbench.FormationNpcPortraitPermanent`, then update it so clearing only the legacy tombstone cannot empty the UI:

```cpp
FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
State.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
FName ExpectedNpcId;
TestTrue(TEXT("fixture resolves an authoritative NPC"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(State, ExpectedNpcId));

UGameXXKDesktopTrainingWorkbenchWidget* Widget =
	NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
Widget->SetMVPSubsystem(Subsystem);
Widget->ConstructForTest();
TestTrue(TEXT("workbench opens"), Widget->OpenWorkbench());
TestTrue(TEXT("backpack opens"), Widget->OpenBackpack());
Widget->HandleActionClicked(1); // existing formation-page action

UImage* NpcPortrait = Widget->WidgetTree
	? Cast<UImage>(Widget->WidgetTree->FindWidget(TEXT("FormationCurrentPortrait_2")))
	: nullptr;
TestNotNull(TEXT("permanent NPC portrait exists"), NpcPortrait);
TestEqual(TEXT("permanent NPC portrait remains visible"),
	NpcPortrait ? NpcPortrait->GetVisibility() : ESlateVisibility::Collapsed,
	ESlateVisibility::HitTestInvisible);
TestFalse(TEXT("permanent NPC portrait has a real texture"),
	GetImageResourcePath(NpcPortrait).IsEmpty());
```

In the roster-flow test, resolve the initial/selected NPC through `ResolveQuestNpcId` and assert all six candidate buttons exist and are enabled. After apply, assert ordered identity and `TrainingTravelRuntime.PartyUnits[2].UnitId` match.

- [ ] **Step 2: Add a town-session non-mutation test**

Using one `UGameInstance`/subsystem, select Yue Bai, capture the Workbench map-travel session, reconstruct a second Workbench against the same subsystem, restore the session, and assert:

```cpp
FName NpcBefore;
FName NpcAfter;
TestTrue(TEXT("Yue Bai resolves before map session transfer"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(Subsystem->GetRuntimeState(), NpcBefore));
const FGameXXKDesktopWorkbenchSessionState Session =
	FirstWidget->CaptureSessionStateForMapTravel();
SecondWidget->RestoreSessionStateAfterMapTravel(Session);
TestTrue(TEXT("NPC resolves after map session transfer"),
	FGameXXKPartyFormationRules::ResolveQuestNpcId(Subsystem->GetRuntimeState(), NpcAfter));
TestEqual(TEXT("map session is presentation-only"), NpcAfter, NpcBefore);
```

This unit test does not claim real `OpenLevel` coverage; Task 8 provides the actual PIE round trip.

- [ ] **Step 3: Cold-build and capture Workbench red**

Run the NPC portrait/roster tests and the new map-session test with report `PermanentNpcFormation-Task6-Red`. Expected: UI still reads `ActiveTemporaryQuestNpcId`, shows/collapses an empty slot, or uses old task-NPC copy.

- [ ] **Step 4: Replace all Workbench temporary-field reads**

Use one local helper pattern:

```cpp
FName ResolveWorkbenchNpcId(const UGameXXKMVPSubsystem* Subsystem)
{
	FName NpcId;
	FString Error;
	return Subsystem
		&& FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(), NpcId, &Error)
		? NpcId
		: NAME_None;
}
```

Replace the reads in roster representative resolution, party equipment-owner collection, and `BuildFormationPanel`. For a valid state, the NPC slot label is always `QuestNpcDisplayName(NpcId)`; delete the `NPC · 未编入` branch. Change the hint from “当前伙伴或任务 NPC” to “当前伙伴或 NPC”.

Candidate enumeration remains the six catalog definitions and receives no unlock predicate. The explicit apply action remains the only formation mutation.

Update public comments from “task-NPC candidate” to “owned NPC candidate”.

- [ ] **Step 5: Add a static production policy test**

Create:

```python
from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]
PRIVATE = ROOT / "Source/GameXXK/Private"
PUBLIC = ROOT / "Source/GameXXK/Public"


class PermanentNpcFormationPolicyTests(unittest.TestCase):
    def test_temporary_field_is_tombstone_only_in_production(self) -> None:
        allowed = {
            ROOT / "Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp",
            ROOT / "Source/GameXXK/Private/GameXXKPartyFormationRules.cpp",
            ROOT / "Source/GameXXK/Public/GameXXKCardRunTypes.h",
        }
        offenders = []
        for base in (PRIVATE, PUBLIC):
            for path in base.rglob("*.[ch]pp"):
                if path not in allowed and "ActiveTemporaryQuestNpcId" in path.read_text(
                    encoding="utf-8", errors="ignore"
                ):
                    offenders.append(path.relative_to(ROOT).as_posix())
            for path in base.rglob("*.h"):
                if path not in allowed and "ActiveTemporaryQuestNpcId" in path.read_text(
                    encoding="utf-8", errors="ignore"
                ):
                    offenders.append(path.relative_to(ROOT).as_posix())
        self.assertEqual(offenders, [])

    def test_removed_support_copy_and_actions_are_unreachable(self) -> None:
        production = "\n".join(
            path.read_text(encoding="utf-8", errors="ignore")
            for base in (PRIVATE, PUBLIC)
            for path in list(base.rglob("*.cpp")) + list(base.rglob("*.h"))
        )
        for forbidden in (
            "邀请月白同行",
            "邀请{0}支援",
            "已有任务支援",
            "临时 NPC",
            "NPC · 未编入",
        ):
            self.assertNotIn(forbidden, production)


if __name__ == "__main__":
    unittest.main()
```

Allow `TemporaryNpcSupport` enum/facade tombstones by testing reachability/copy rather than banning the symbol globally.

- [ ] **Step 6: Run Python policy and Workbench suites green**

```powershell
python -m unittest scripts.test_permanent_npc_formation_policy -v
```

Then run:

```text
GameXXK.DesktopTraining.Workbench.FormationNpcPortraitPermanent
GameXXK.DesktopTraining.Workbench.CharacterRosterPlacementAndViewIsolation
GameXXK.DesktopTraining.Workbench.MapTravelSessionPreservesNpc
GameXXK.DesktopTraining.Workbench
```

Use report `PermanentNpcFormation-Task6-Green`. Expected: no empty NPC, all six candidates available, map-session reconstruction preserves identity, and no new Workbench failure beyond any explicitly recorded unrelated baseline failure.

- [ ] **Step 7: Commit only Task 6 hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "fix: align Workbench with permanent NPC formation"
```

---

### Task 7: Migrate obsolete test fixtures and prove no production consumer remains

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKPermanentPartyTestFixtures.h`
- Modify only where `rg` reports obsolete setup/assertions:
  - `Source/GameXXK/Private/Tests/GameXXKBattleHudFixtureTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKBattleRetreatTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCardRouteQuestNpcTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCompanionBattleProgressionRewardTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCompanionFacadeTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKEquipmentBattleIntegrationTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKEquipmentCompanionReplacementTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKHeroCardIntegrationTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKPartyProgressionAndLevelGateTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKQuestNpcCardSelectionTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKQuestNpcDefaultLoadoutTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKRouteEncounterPanelTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKRouteEncounterSceneActorTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKRouteMerchantWidgetTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKStarterCompanionTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKThreeChapterRouteTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKTownNpcInteractionRulesTest.cpp`
  - `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`

- [ ] **Step 1: Add one test-only authoritative helper**

Create:

```cpp
#pragma once

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKMVPRules.h"

namespace GameXXKPermanentPartyTestFixtures
{
	inline bool SelectNpc(
		FGameXXKRuntimeState& State,
		const FName NpcId,
		FString* OutError = nullptr)
	{
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, OutError))
		{
			return false;
		}
		return FGameXXKPartyFormationRules::SetQuestNpc(State, NpcId, OutError);
	}

	inline FName ResolveNpc(const FGameXXKRuntimeState& State)
	{
		FName NpcId;
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcId);
		return NpcId;
	}
}
```

This helper is for tests only. Production code must call the formation rules directly.

- [ ] **Step 2: Classify every legacy-field test occurrence before editing**

Run:

```powershell
rg -n -S "ActiveTemporaryQuestNpcId|SetQuestNpcForCurrentRun|temporary task NPC|temporary NPC|任务支援|邀请.*同行" Source/GameXXK/Private/Tests
```

For each hit, assign exactly one category:

1. **Normal current fixture:** replace direct writes with `SelectNpc` and direct reads with `ResolveNpc`.
2. **Route cleanup expectation:** reverse it to require preservation.
3. **Retired route-event expectation:** replace it with catalog/facade retirement assertions from Task 4.
4. **Legacy migration fixture:** retain direct `ActiveTemporaryQuestNpcId` setup, include `GameXXKPermanentNpcSaveMigrationTest.cpp` or an explicit `legacy v29` comment, and assert the migrated result clears it.
5. **Corruption/atomicity fixture:** retain only when the test intentionally creates invalid legacy state and never describes it as a valid current formation.

No hit may be silently deleted merely to make a suite green.

- [ ] **Step 3: Migrate normal fixtures and assertions**

Use this exact replacement pattern:

```cpp
FString FormationError;
TestTrue(TEXT("fixture selects Yue Bai through ordered formation"),
	GameXXKPermanentPartyTestFixtures::SelectNpc(
		State, TEXT("Npc.YueBai"), &FormationError));
TestEqual(TEXT("fixture resolves Yue Bai"),
	GameXXKPermanentPartyTestFixtures::ResolveNpc(State),
	FName(TEXT("Npc.YueBai")));
TestTrue(TEXT("current fixture keeps the tombstone empty"),
	State.CardRun.ActiveTemporaryQuestNpcId.IsNone());
```

Update helper functions named `ResolveDeployedNpcId` in tests to call `FGameXXKPartyFormationRules::ResolveQuestNpcId`, not to fall back to Tusi.

Update old test descriptions from “temporary task NPC”/“route support” to “selected NPC”/“permanent NPC formation” unless they are explicit v29 migration cases.

- [ ] **Step 4: Remove obsolete empty-NPC and support assumptions**

Rewrite or delete only the assertions that encode behavior the approved design removed:

- current formation may be empty;
- route settlement replaces the NPC with a second companion;
- route event can install a temporary NPC;
- `ActiveTemporaryQuestNpcId` is the active identity;
- task-support occupancy disables an invitation button.

Replacement assertions must check one of the approved contracts: exact three-role formation, preservation, no-op facade, removed catalog IDs, or v29 migration.

- [ ] **Step 5: Run source-policy and production-reference sweeps**

```powershell
python -m unittest scripts.test_permanent_npc_formation_policy -v

rg -n -S "ActiveTemporaryQuestNpcId" Source/GameXXK/Private Source/GameXXK/Public `
  --glob '!**/Tests/**'

rg -n -S "邀请.*同行|已有任务支援|NPC · 未编入|TemporaryNpcSupport" `
  Source/GameXXK/Private Source/GameXXK/Public --glob '!**/Tests/**'
```

Expected:

- `ActiveTemporaryQuestNpcId` appears only in its `GameXXKCardRunTypes.h` tombstone declaration, `GameXXKSaveMigration.cpp` recovery/clear path, and the `GameXXKPartyFormationRules.cpp` invariant check outside tests.
- `TemporaryNpcSupport` appears only as explicit tombstone declarations/compatibility tests, never in a catalog choice or resolver branch.
- Removed player-facing strings have zero production hits.

- [ ] **Step 6: Cold-build and run all affected semantic suites**

Use report `PermanentNpcFormation-Task7-Affected` for:

```text
GameXXK.PartyFormation
GameXXK.Training.PartyProgression
GameXXK.MVP.Companion
GameXXK.Integration.CardRoute
GameXXK.Route.Settlement
GameXXK.Route.Merchant
GameXXK.MVP.RouteEncounter
GameXXK.DesktopTraining.Workbench
GameXXK.MVP.SaveGame
GameXXK.Equipment.SaveMigration
```

Expected: no new failure. If the Workbench suite still contains the previously recorded unrelated inner-geometry/animation contract failures, record their exact names and confirm the permanent-NPC focused tests are green; do not weaken those unrelated assertions in this task.

- [ ] **Step 7: Run the broad GameXXK regression and compare, not conceal, baseline failures**

```powershell
python scripts/ai_production_loop.py --run-automation `
  --automation-tests "GameXXK" `
  --automation-report "PermanentNpcFormation-Task7-All"
python scripts/parse_automation_index.py `
  "Saved/Automation/PermanentNpcFormation-Task7-All/index.json"
```

Expected: zero permanent-NPC/event/migration failures and no regression beyond explicitly named pre-existing failures. A failure is not reclassified as “pre-existing” without a prior report or a source-level reason recorded in the handoff.

- [ ] **Step 8: Commit only Task 7 fixture/policy hunks**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "test: migrate permanent NPC fixtures"
```

---

### Task 8: Collect read-only PIE evidence and update the rolling acceptance pointer

**Files:**
- Modify: `Content/Python/gamexxk_probe_training_visual_mvp.py`
- Modify after verification only: `docs/production/current-goal-acceptance.md`

- [ ] **Step 1: Extend the existing probe with ordered and idle party IDs**

Add these fields to `_party_snapshot`:

```python
ordered_formation = getattr(card_run, "ordered_formation", None)
ordered_members = []
for member in list(getattr(ordered_formation, "members", []) or []):
    ordered_members.append(
        {
            "kind": str(getattr(member, "kind", "")),
            "member_id": str(getattr(member, "member_id", "")),
        }
    )
travel_runtime = _call(subsystem, "get_training_travel_runtime_copy")
travel_party_ids = [
    str(getattr(unit, "unit_id", ""))
    for unit in list(getattr(travel_runtime, "party_units", []) or [])
]
```

Return:

```python
"ordered_members": ordered_members,
"travel_party_ids": travel_party_ids,
```

Keep the probe read-only: do not call selection, travel, route, input, click, or save mutation APIs.

Replace the existing anonymous `HighResShot` capture command with a deterministic optional filename:

```python
capture_path = (
    Path(unreal.Paths.project_saved_dir())
    / "Screenshots/WindowsEditor/permanent-npc-yuebai-workbench.png"
).resolve()
capture_path.parent.mkdir(parents=True, exist_ok=True)
unreal.SystemLibrary.execute_console_command(
    world,
    f'HighResShot filename="{capture_path.as_posix()}" 1600x900',
)
payload["capture_requested"] = True
payload["capture_path"] = str(capture_path)
```

Add `from pathlib import Path` at the top. This writes only screenshot evidence and does not mutate gameplay state.

- [ ] **Step 2: Unit-check the probe syntax without launching UE**

```powershell
python -m py_compile Content/Python/gamexxk_probe_training_visual_mvp.py
```

Expected: exit code 0.

- [ ] **Step 3: Run one final cold UBT and focused Automation gate**

Run the shared cold build, then one combined report `PermanentNpcFormation-Final` covering:

```text
GameXXK.PartyFormation.PermanentNpcAuthority
GameXXK.Training.PermanentNpcIdleSwapPreservesProgress
GameXXK.Training.PartyProgression.DeployedTrioExperience
GameXXK.Route.Event.NpcCatalogRetired
GameXXK.MVP.SaveGame.PermanentNpcV30Migration
GameXXK.Integration.CardRoute.Lifecycle
GameXXK.Route.Settlement.Formation
GameXXK.DesktopTraining.Workbench
```

Expected: cold UBT succeeds and all task-focused tests pass. Record any unrelated suite failure separately; do not call the task complete while any named focused test fails.

- [ ] **Step 4: Start visible PIE without automatic input**

Use the normal visible editor launcher and the canonical map:

```powershell
.\Launch_GameXXK_Editor.cmd
```

Start PIE on `/Game/GameXXK/Maps/L_DesktopTrainingHUD`. Do not run `gamexxk_real_play_flow_mcp.py`, mouse hooks, synthetic clicks, or window-button test automation for this acceptance.

- [ ] **Step 5: Perform the manual Yue Bai lifecycle while taking read-only snapshots**

At each checkpoint, run the existing probe through UE MCP:

```powershell
python -c "import sys; sys.path.insert(0, 'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); print(c.run_project_python_file('gamexxk_probe_training_visual_mvp.py', ['--phase', 'observe']))"
```

Manual checkpoints:

1. Select Yue Bai in the formation page and click `编入队伍`.
2. Confirm ordered member 3 and idle `travel_party_ids[2]` are `Npc.YueBai`.
3. Click `进入城镇`, then `退出城镇`; confirm both IDs remain Yue Bai.
4. Start a route/challenge; confirm the battle party third identity is Yue Bai through the C++ focused test evidence and visible party portrait/name.
5. Finish or abandon the route; confirm idle third identity remains Yue Bai.
6. Save, close the game normally, restart, load, and confirm both IDs remain Yue Bai.
7. Switch to another NPC and confirm the same immediate ordered/idle identity change.

The probe observes only; all interactions are performed by the user/manual tester.

- [ ] **Step 6: Review the scoped visual evidence if screenshots are needed**

If the portrait/name presentation needs screenshot judgment, capture only the relevant Workbench/idle strip and review it with a suitable method:

```powershell
python -c "import sys; sys.path.insert(0, 'scripts'); from ue_mcp_client import UnrealMCPClient; c=UnrealMCPClient(); print(c.run_project_python_file('gamexxk_probe_training_visual_mvp.py', ['--phase', 'observe', '--capture']))"
```

Review `Saved/Screenshots/WindowsEditor/permanent-npc-yuebai-workbench.png` and verify that the formation NPC portrait/name and idle-strip third member show the same NPC; report any mismatch only.

Expected: formation and idle-strip identity agree; no `未编入`, invitation, or temporary-support UI appears. This visual check does not replace the state-ID probe.

- [ ] **Step 7: Update the rolling acceptance pointer with exact evidence**

Append a dated entry to `docs/production/current-goal-acceptance.md` containing:

- implementation commit IDs;
- cold UBT result;
- focused Automation report paths and pass/fail counts;
- broad regression report and any explicitly unrelated failures;
- PIE checkpoint results for Yue Bai, town round-trip, route exit, and restart;
- confirmation that no automatic input driver was used;
- confirmation that the seven NPC events are absent and Mountain Spring remains.

Do not claim a package build, full all-green suite, or visual pass unless that evidence actually exists.

- [ ] **Step 8: Commit the probe and verified acceptance record**

```powershell
git diff --check
git diff --cached --name-status
git commit -m "docs: record permanent NPC formation acceptance"
```

- [ ] **Step 9: Run final repository hygiene checks**

```powershell
git status --short --branch
git diff --check
python scripts/harness_state_validator.py
```

Expected: task files have no unstaged accidental edits, the pre-existing user work remains preserved, and the harness validator reports no new finding caused by this task.

---

## Final Completion Checklist

- [ ] `OrderedFormation` resolves exactly hero + one permanent companion + one of six NPCs.
- [ ] `ActiveTemporaryQuestNpcId` is `NAME_None` in current runtime and appears only in its tombstone declaration, migration, and invariant validation outside tests.
- [ ] Workbench, idle party, route/battle, equipment/card projection, and XP agree on one NPC ID.
- [ ] Active idle progress/cooldowns/reward ledgers survive party replacement.
- [ ] Town entry/exit, route clear/defeat/abandon, save/load, and restart preserve the selected NPC.
- [ ] All seven NPC route events are absent from generation.
- [ ] Temporary-support enum/action ordinals remain serialized tombstones; no reachable support UX/action remains.
- [ ] v29 saves recover NPCs in the approved priority and remap pending removed events without rewards or settlement.
- [ ] Focused Automation, cold UBT, read-only PIE evidence, and rolling acceptance documentation are complete.
- [ ] Unrelated dirty files/assets and the user's staged deletions remain untouched.
