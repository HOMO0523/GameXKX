# Ordered Party Formation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the implicit fixed hero/companion/NPC party with a persisted, validated, ordered `1P / 2P / 3P` formation and an orderly draft/apply Workbench editor.

**Architecture:** Add one focused formation type/rules module and store only stable member references. Every runtime consumer resolves the same ordered array; existing hero/companion/NPC fields remain compatibility projections during migration rather than competing sources of order.

**Tech Stack:** Unreal Engine 5.8 C++, SaveGame USTRUCTs, UMG/Slate, UE Automation Tests, cold UBT, UE MCP.

---

## File map

- Create `Source/GameXXK/Public/GameXXKPartyFormationTypes.h`: member kind/reference and ordered formation.
- Create `Source/GameXXK/Public/GameXXKPartyFormationRules.h`: validation, legacy projection, normalization.
- Create `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`: pure formation rules.
- Create `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`: rule and migration-facing tests.
- Modify `Source/GameXXK/Public/GameXXKCardRunTypes.h`: persisted ordered formation field.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`: formation read/apply facade.
- Modify `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: transactional facade and Travel party projection.
- Modify `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: next-version migration and normalization.
- Modify `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`: old-save order and invalid formation migration.
- Modify `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`: card combat unit/card order.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`: battle order.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: three slots, candidate filters, swap/replace draft, Apply.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: draft state/test seams.
- Modify `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`: formation UI.
- Modify `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`: use ordered member/card pool from Unit B seam.

### Task 1: RED — legal ordered formation and compatibility migration

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`

- [ ] **Step 1: Add the failing rules tests**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyFormationRulesTest,
	"GameXXK.PartyFormation.Rules.OrderValidationAndLegacyProjection",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyFormationRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FGameXXKOrderedPartyFormation Legacy;
	TestTrue(TEXT("legacy selection projects"), FGameXXKPartyFormationRules::BuildLegacyProjection(State, Legacy));
	TestEqual(TEXT("projection has three slots"), Legacy.Members.Num(), 3);
	TestEqual(TEXT("hero begins in 1P"), Legacy.Members[0].Kind, EGameXXKPartyMemberKind::Hero);

	FGameXXKOrderedPartyFormation Swapped = Legacy;
	Swap(Swapped.Members[0], Swapped.Members[1]);
	FString Error;
	TestTrue(TEXT("hero may occupy 2P"), FGameXXKPartyFormationRules::Validate(State, Swapped, &Error));

	FGameXXKOrderedPartyFormation Duplicate = Legacy;
	Duplicate.Members[2] = Duplicate.Members[1];
	TestFalse(TEXT("duplicate entity is rejected"), FGameXXKPartyFormationRules::Validate(State, Duplicate, &Error));

	FGameXXKOrderedPartyFormation NoHero = Legacy;
	NoHero.Members.RemoveAt(0);
	NoHero.Members.Add(NoHero.Members[0]);
	TestFalse(TEXT("party without hero is rejected"), FGameXXKPartyFormationRules::Validate(State, NoHero, &Error));
	return true;
}

#endif
```

- [ ] **Step 2: Add declarations only and run RED**

Create headers with declarations from Task 2 but return `false` from rule methods. Cold-build and run `GameXXK.PartyFormation.Rules`.

Expected: FAIL on legacy projection/validation behavior, not compile errors.

### Task 2: GREEN — focused persisted types and pure rules

**Files:**
- Create: `Source/GameXXK/Public/GameXXKPartyFormationTypes.h`
- Create: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
- Create: `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`

- [ ] **Step 1: Define stable types**

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameXXKPartyFormationTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKPartyMemberKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	Hero,
	PermanentCompanion,
	QuestNpc
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPartyMemberRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKPartyMemberKind Kind = EGameXXKPartyMemberKind::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName MemberId = NAME_None;

	bool IsValid() const { return Kind != EGameXXKPartyMemberKind::Invalid && !MemberId.IsNone(); }
	bool operator==(const FGameXXKPartyMemberRef& Other) const { return Kind == Other.Kind && MemberId == Other.MemberId; }
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKOrderedPartyFormation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKPartyMemberRef> Members;
};
```

Add to `FGameXXKCardRunState`:

```cpp
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKOrderedPartyFormation OrderedFormation;
```

- [ ] **Step 2: Define rule API**

```cpp
class GAMEXXK_API FGameXXKPartyFormationRules final
{
public:
	static constexpr int32 PartySize = 3;
	static bool BuildLegacyProjection(const FGameXXKRuntimeState& State, FGameXXKOrderedPartyFormation& OutFormation);
	static bool ResolveEffective(const FGameXXKRuntimeState& State, FGameXXKOrderedPartyFormation& OutFormation, FString* OutError = nullptr);
	static bool Validate(const FGameXXKRuntimeState& State, const FGameXXKOrderedPartyFormation& Formation, FString* OutError = nullptr);
	static bool Normalize(FGameXXKRuntimeState& InOutState, FString* OutError = nullptr);
	static void ProjectCompatibility(FGameXXKRuntimeState& InOutState);
};
```

- [ ] **Step 3: Implement validation**

Validation requires exactly three valid unique refs and at least one `Hero`. Resolve each ref against the hero ID, permanent companion instances, or approved/current task NPC. Reject an unavailable NPC. `BuildLegacyProjection` emits Hero, active permanent companion, active task NPC; if either optional slot is absent, choose the first deterministic owned legal replacement without duplicating.

Use one helper:

```cpp
bool ResolveMember(const FGameXXKRuntimeState& State, const FGameXXKPartyMemberRef& Ref)
{
	switch (Ref.Kind)
	{
	case EGameXXKPartyMemberKind::Hero:
		return Ref.MemberId == FGameXXKEquipmentRules::HeroCharacterId();
	case EGameXXKPartyMemberKind::PermanentCompanion:
		return State.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
			[&Ref](const FGameXXKPermanentCompanion& Item) { return Item.InstanceId == Ref.MemberId; });
	case EGameXXKPartyMemberKind::QuestNpc:
		return Ref.MemberId == State.CardRun.ActiveTemporaryQuestNpcId
			|| Ref.MemberId == State.CardRun.PartySelection.QuestNpc.NpcId;
	default:
		return false;
	}
}
```

`Normalize` keeps a valid saved formation; otherwise builds the legacy projection. `ProjectCompatibility` finds the first permanent companion/NPC refs in order and updates old active fields, but never reorders `OrderedFormation`.

- [ ] **Step 4: Run GREEN**

Cold-build and run `GameXXK.PartyFormation.Rules`. Expected: all pass.

- [ ] **Step 5: Commit types/rules**

```powershell
git add -- Source/GameXXK/Public/GameXXKPartyFormationTypes.h Source/GameXXK/Public/GameXXKPartyFormationRules.h Source/GameXXK/Private/GameXXKPartyFormationRules.cpp Source/GameXXK/Public/GameXXKCardRunTypes.h Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp
git commit -m "feat: add ordered party formation rules"
```

### Task 3: RED/GREEN — next-version save migration

**Files:**
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`

- [ ] **Step 1: Add old-save migration test**

Create a save fixture at the immediately previous save version with empty `OrderedFormation`, an active companion, and task NPC. Migrate and assert three refs in `Hero / Companion / NPC` order, then save/load and assert equality.

```cpp
TestEqual(TEXT("migrated order has three"), Migrated.RuntimeState.CardRun.OrderedFormation.Members.Num(), 3);
TestEqual(TEXT("hero is 1P"), Migrated.RuntimeState.CardRun.OrderedFormation.Members[0].Kind, EGameXXKPartyMemberKind::Hero);
TestEqual(TEXT("companion is 2P"), Migrated.RuntimeState.CardRun.OrderedFormation.Members[1].MemberId, CompanionId);
TestEqual(TEXT("NPC is 3P"), Migrated.RuntimeState.CardRun.OrderedFormation.Members[2].MemberId, NpcId);
```

- [ ] **Step 2: Run RED**

Expected: empty ordered formation after migration.

- [ ] **Step 3: Allocate the next free save version and normalize**

Add one append-only introduced-version constant in `GameXXKSaveGame.h`. In migration after older companion/NPC migrations:

```cpp
	if (Source.SaveVersion < OrderedPartyFormationIntroducedSaveVersion)
	{
		Candidate.RuntimeState.CardRun.OrderedFormation = FGameXXKOrderedPartyFormation();
	}
	if (!FGameXXKPartyFormationRules::Normalize(Candidate.RuntimeState, &MigrationError))
	{
		return false;
	}
```

Set final save version to the current version only after normalization succeeds.

- [ ] **Step 4: Run GREEN and SaveGame suite**

Expected: focused migration and complete `GameXXK.MVP.SaveGame` pass.

### Task 4: RED/GREEN — subsystem transaction facade

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionFacadeTest.cpp`

- [ ] **Step 1: Add facade API and tests**

```cpp
	UFUNCTION(BlueprintPure, Category = "GameXXK|Party")
	FGameXXKOrderedPartyFormation GetOrderedPartyFormation() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Party")
	bool SetOrderedPartyFormation(const FGameXXKOrderedPartyFormation& Formation, FString& OutError);
```

Test a legal swap commits, invalid duplicate/no-hero drafts fail, and failures leave the previous formation unchanged.

- [ ] **Step 2: Implement candidate-copy commit**

```cpp
bool UGameXXKMVPSubsystem::SetOrderedPartyFormation(
	const FGameXXKOrderedPartyFormation& Formation,
	FString& OutError)
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKPartyFormationRules::Validate(Candidate, Formation, &OutError)) return false;
	Candidate.CardRun.OrderedFormation = Formation;
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}
```

Disallow commits while route loadout is locked or a battle is active; return a visible error.

- [ ] **Step 3: Run facade GREEN**

Expected: all companion/formation facade tests pass.

### Task 5: RED/GREEN — battle, Travel, and merchant consumers

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`

- [ ] **Step 1: Add order-consumer tests**

Use a `Companion / Hero / NPC` fixture and assert:

```cpp
TestEqual(TEXT("battle slot 1 follows 1P"), Battle.Units[0].UnitId, CompanionUnitId);
TestEqual(TEXT("battle slot 2 follows 2P"), Battle.Units[1].UnitId, HeroUnitId);
TestEqual(TEXT("battle slot 3 follows 3P"), Battle.Units[2].UnitId, NpcUnitId);
TestEqual(TEXT("Travel first attacker follows 1P"), Runner.PartyUnits[0].UnitId, CompanionUnitId);
TestEqual(TEXT("merchant owner order follows party"), OfferOwners, TArray<FName>{CompanionUnitId, HeroUnitId, NpcUnitId});
```

- [ ] **Step 2: Run RED**

Expected: current fixed hero-first projections fail.

- [ ] **Step 3: Resolve every consumer through rules**

At each source, call:

```cpp
FGameXXKOrderedPartyFormation Formation;
if (!FGameXXKPartyFormationRules::ResolveEffective(State, Formation, OutError)) return false;
for (const FGameXXKPartyMemberRef& Member : Formation.Members)
{
	// Resolve stats/cards for this exact stable ref and append in array order.
}
```

`BuildTrainingTravelParty` and card combat creation preserve the exact array order. Merchant `BuildEffectiveDeployedCardPool` iterates the same array. Compatibility mirrors may identify a hero or companion but never decide order.

- [ ] **Step 4: Run GREEN and related suites**

Run CardBattleAdapter, Training, and RouteMerchant rules prefixes. Expected: zero failures.

### Task 6: RED/GREEN — Workbench formation editor

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`

- [ ] **Step 1: Add draft/apply UI test**

```cpp
TestTrue(TEXT("formation page opens"), Widget->OpenFormationForTest());
TestEqual(TEXT("three visible party slots"), Widget->GetFormationPartySlotCountForTest(), 3);
TestTrue(TEXT("select 1P"), Widget->SelectFormationPartySlotForTest(0));
TestTrue(TEXT("select companion candidate"), Widget->SelectFormationCandidateForTest(CompanionId));
TestTrue(TEXT("draft changes without commit"), Widget->HasFormationDraftChangesForTest());
TestTrue(TEXT("swap 1P and 2P"), Widget->SwapFormationSlotsForTest(0, 1));
TestTrue(TEXT("apply commits"), Widget->ApplyFormationDraftForTest());
TestEqual(TEXT("committed order matches draft"), Subsystem->GetOrderedPartyFormation().Members, Widget->GetFormationDraftForTest().Members);
```

Add separate assertions that local `X` discards draft and returns central content to Backpack, while committed order persists.

- [ ] **Step 2: Run RED**

Expected: current page has fixed hero/companion/NPC cards and no slot-target draft/swap.

- [ ] **Step 3: Add draft state**

```cpp
	FGameXXKOrderedPartyFormation FormationDraft;
	int32 SelectedFormationSlotIndex = INDEX_NONE;
	bool bFormationDraftDirty = false;
```

Opening Formation copies the subsystem formation. Candidate click replaces `FormationDraft.Members[SelectedFormationSlotIndex]` only after local validation; clicking another deployed slot swaps. Apply calls subsystem transaction, then clears dirty state. Close/global reset restores the committed snapshot and clears selection.

- [ ] **Step 4: Rebuild the layout**

Render three equal top cards labelled `1P`, `2P`, `3P`; below, render Hero/Companion/NPC filters and a uniform candidate grid. Companion text uses one mapping:

```cpp
switch (Companion.Role)
{
case EGameXXKCharacterRole::Blade: return TEXT("刀客");
case EGameXXKCharacterRole::Guard: return TEXT("守卫");
case EGameXXKCharacterRole::Healer: return TEXT("药师");
case EGameXXKCharacterRole::Hunter: return TEXT("射手");
case EGameXXKCharacterRole::Sorcerer: return TEXT("法师");
case EGameXXKCharacterRole::FormationMaster: return TEXT("阵师");
default: return TEXT("伙伴");
}
```

Use approved portrait and selected/normal tab assets. Do not show instance IDs.

- [ ] **Step 5: Run GREEN**

Run focused Formation page test and complete Workbench suite.

### Task 7: Full verification, PIE, Luna, and commit

- [ ] **Step 1: Cold UBT**

Expected: `Result: Succeeded` with no Hot Reload.

- [ ] **Step 2: Focused/full automation**

Run:

```text
GameXXK.PartyFormation
GameXXK.MVP.SaveGame
GameXXK.CardBattle.Adapter
GameXXK.Training
GameXXK.MVP.RouteMerchant
GameXXK.DesktopTraining.Workbench
```

Use fresh report paths and parse every `index.json`; expected zero failed/errors.

- [ ] **Step 3: Real PIE**

On the pure-2D map, open Formation, put the companion in 1P and hero in 2P, Apply, close to Backpack, and verify idle strip left-to-right order. Enter route battle and verify the same three members/order; enter merchant and verify owner labels follow the same formation.

- [ ] **Step 4: Luna Max review**

Review default, selected-slot, draft replacement, and swapped/apply screenshots. Acceptance: three equal slots, readable `1P/2P/3P`, tidy candidate grid, only six companion profession labels, approved paper style, local `X` present.

- [ ] **Step 5: Commit Unit C**

```powershell
git commit -m "feat: add ordered three-member formation"
```
