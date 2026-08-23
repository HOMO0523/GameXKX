# Decoupled Party Formation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the v24 generic ordered-member formation with a v25 model that separately stores one Hero, one fixed Companion, one persistent Quest NPC, and a `1P / 2P / 3P` category permutation.

**Architecture:** Keep the v24 `OrderedFormation.Members` field only as an append-only migration source. Add an authoritative decoupled formation state, one resolver that maps category order to member refs, and route-local projection helpers that never erase persistent composition. All consumers and UI read the same resolver.

**Tech Stack:** Unreal Engine 5.8 C++, SaveGame USTRUCTs, UMG/Slate, UE Automation Tests, cold UBT, UE MCP.

---

## Source specification

`docs/superpowers/specs/2026-08-23-decoupled-party-formation-design.md`

## File map

- Modify `Source/GameXXK/Public/GameXXKPartyFormationTypes.h`: v25 composition/order types while retaining v24 member refs.
- Modify `Source/GameXXK/Public/GameXXKPartyFormationRules.h`: validation, category-order resolution, migration, and projection APIs.
- Modify `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`: pure v25 rules.
- Modify `Source/GameXXK/Public/GameXXKCardRunTypes.h`: add authoritative v25 formation field; retain legacy v24 field.
- Modify `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`: composition/order rule tests.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`: allocate v25.
- Modify `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: v24/pre-v24 migration and current validation.
- Modify `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`: migration and roundtrip coverage.
- Modify `Source/GameXXK/Public/GameXXKCompanionRules.h`: fixed-roster/all-card normalization.
- Modify `Source/GameXXK/Private/GameXXKCompanionRules.cpp`: fixed six companions, complete unlocks, default decks.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`: v25 formation facade.
- Modify `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: atomic composition/order commit and route NPC projection.
- Modify `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`: ordered battle resolution.
- Modify `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`: ordered merchant owner pool.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: two-section Formation editor.
- Modify `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`: draft state and test seams.
- Modify `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`: remove recruitment/dismiss controls and expose all companion cards.
- Modify focused rule, migration, consumer, facade, Workbench, and player-flow tests listed below.

## Global constraints

- Work directly on root `main`; do not use a worktree.
- Preserve unrelated B1/B4/health dirty hunks in shared files.
- Do not use UnrealBridge, Live Coding, or Hot Reload.
- Every behavior change follows RED -> intended failure -> minimal GREEN -> related regression.
- Fixed companions are never recruited, randomly replaced, or dismissed by player-facing flows.
- Companion card locking/level requirements are deferred; this delivery unlocks all current companion cards.

### Task 1: RED — composition and position order are independent

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`
- Modify declarations only: `Source/GameXXK/Public/GameXXKPartyFormationTypes.h`
- Modify declarations only: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`

- [ ] **Step 1: Add v25 type declarations with stubbed rules**

Add the exact public types that later tasks implement:

```cpp
UENUM(BlueprintType)
enum class EGameXXKPartyCategory : uint8
{
	Invalid = 0 UMETA(Hidden),
	Hero = 1,
	Companion = 2,
	QuestNpc = 3
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPartyComposition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SelectedHeroId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SelectedCompanionId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName SelectedQuestNpcId = NAME_None;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPartyPositionOrder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<EGameXXKPartyCategory> Slots;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPartyFormationState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPartyComposition Composition;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPartyPositionOrder PositionOrder;
};
```

Declare `Validate`, `BuildDefault`, `ResolveOrderedMembers`, `SwapPositions`, and `ProjectCompatibility` with stubbed `false` returns.

- [ ] **Step 2: Add failing rule tests**

Add `GameXXK.PartyFormation.DecoupledRules` tests that assert:

```cpp
TestTrue(TEXT("default formation builds"), FGameXXKPartyFormationRules::BuildDefault(State, Formation, &Error));
TestEqual(TEXT("default 1P is Hero"), Formation.PositionOrder.Slots[0], EGameXXKPartyCategory::Hero);
TestEqual(TEXT("default 2P is Companion"), Formation.PositionOrder.Slots[1], EGameXXKPartyCategory::Companion);
TestEqual(TEXT("default 3P is QuestNpc"), Formation.PositionOrder.Slots[2], EGameXXKPartyCategory::QuestNpc);

const FGameXXKPartyComposition BeforeComposition = Formation.Composition;
TestTrue(TEXT("position swap succeeds"), FGameXXKPartyFormationRules::SwapPositions(Formation, 0, 2, &Error));
TestTrue(TEXT("position swap never changes selected ids"),
	FGameXXKPartyComposition::StaticStruct()->CompareScriptStruct(
		&Formation.Composition, &BeforeComposition, PPF_None));

const FGameXXKPartyPositionOrder BeforeOrder = Formation.PositionOrder;
Formation.Composition.SelectedCompanionId = GuardCompanionId;
TestTrue(TEXT("changing companion preserves order"),
	FGameXXKPartyPositionOrder::StaticStruct()->CompareScriptStruct(
		&Formation.PositionOrder, &BeforeOrder, PPF_None));
```

Also reject missing categories, duplicate categories, unknown IDs, non-fixed companions, unavailable Quest NPCs, and any attempt to clear one selected category.

- [ ] **Step 3: Run RED**

Run cold UBT, then:

```powershell
D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe "D:\UE5 demo\GameXXK\GameXXK.uproject" -Unattended -NoSound -NullRHI -NoSplash -NoPause -ReportOutputPath="D:\UE5 demo\GameXXK\Saved\Automation\DecoupledFormationRulesRed" -ExecCmds="Automation RunTests GameXXK.PartyFormation.DecoupledRules; Quit"
```

Expected: behavioral failures from stubbed validation/resolution, not fixture errors.

### Task 2: GREEN — v25 types and pure decoupled rules

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKPartyFormationTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKPartyFormationRules.h`
- Modify: `Source/GameXXK/Private/GameXXKPartyFormationRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRunTypes.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPartyFormationRulesTest.cpp`

- [ ] **Step 1: Add the authoritative v25 field**

Retain the v24 array as migration-only data and add:

```cpp
/** v24 migration source only; never read for current ordering. */
UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame, meta=(DeprecatedProperty))
FGameXXKOrderedPartyFormation OrderedFormation;

/** v25 authoritative composition and category order. */
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKPartyFormationState PartyFormation;
```

- [ ] **Step 2: Implement exact validation**

Validation checks all selected IDs and the exact category permutation:

```cpp
const TArray<EGameXXKPartyCategory> Required = {
	EGameXXKPartyCategory::Hero,
	EGameXXKPartyCategory::Companion,
	EGameXXKPartyCategory::QuestNpc};

if (Formation.PositionOrder.Slots.Num() != 3)
{
	return SetFailure(OutError, TEXT("Position order must contain exactly 1P, 2P, and 3P categories."));
}
for (const EGameXXKPartyCategory Category : Required)
{
	if (Formation.PositionOrder.Slots.Count(Category) != 1)
	{
		return SetFailure(OutError, TEXT("Hero, Companion, and QuestNpc must each appear exactly once."));
	}
}
```

Resolve the Hero against the current hero roster, the Companion against the six fixed role records, and the Quest NPC against the owned/approved quest-character set. Do not require route-local NPC mirrors.

- [ ] **Step 3: Implement default and ordered resolution**

`BuildDefault` selects the current Hero, Blade companion, and deterministic default Quest NPC, then assigns default category order.

`ResolveOrderedMembers` maps each category without changing the composition:

```cpp
for (const EGameXXKPartyCategory Category : Formation.PositionOrder.Slots)
{
	FGameXXKPartyMemberRef Ref;
	Ref.Kind = KindForCategory(Category);
	Ref.MemberId = IdForCategory(Formation.Composition, Category);
	OutMembers.Members.Add(Ref);
}
```

- [ ] **Step 4: Implement position swaps and compatibility projection**

`SwapPositions` swaps only two category entries. `ProjectCompatibility` updates the legacy active permanent companion mirror. Route-local NPC mirrors are updated only by the route-projection API in Task 5.

- [ ] **Step 5: Run GREEN and commit**

Run `GameXXK.PartyFormation.DecoupledRules` plus current formation rules. Expected: all pass, zero errors.

Commit only the scoped type/rule/test files:

```powershell
git commit -m "feat: decouple party composition and position order"
```

### Task 3: RED/GREEN — v25 save migration

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`

- [ ] **Step 1: Add migration RED tests**

Allocate v25 and cover:

- v24 `[Hero, Companion, QuestNpc]` preserves both IDs and category order;
- v24 `[Companion, Hero, QuestNpc]` becomes the same composition with `1P=Companion, 2P=Hero, 3P=QuestNpc`;
- v24 `[Hero, Companion, Companion]` selects the first valid companion, restores the deterministic owned Quest NPC, and uses an unambiguous/default order;
- pre-v24 derives Hero/current companion/current or default owned Quest NPC with default order;
- v25 malformed composition or category permutation is rejected;
- save/load roundtrip preserves composition and order exactly.

Run into `Saved/Automation/DecoupledFormationMigrationRed`. Expected: v25 API/version failures.

- [ ] **Step 2: Allocate v25**

```cpp
/** v25: party composition is independent from 1P/2P/3P category order. */
static constexpr int32 DecoupledPartyFormationIntroducedSaveVersion = 25;
static constexpr int32 CurrentSaveVersion = 25;
```

- [ ] **Step 3: Implement v24 and pre-v24 migration**

For v24, read `OrderedFormation.Members`. If it contains exactly one valid member of each category, derive both composition and position order. Otherwise preserve an unambiguous Hero/Companion, choose the deterministic default owned Quest NPC, and use the default order.

For pre-v24, use legacy mirrors for composition and the default order. Do not remove or overwrite companion progression, decks, equipment, or Quest NPC loadouts.

Current v25 saves call strict `Validate` and never silently normalize.

- [ ] **Step 4: Run GREEN**

Run focused migration, `GameXXK.MVP.SaveGame`, MetaShop migration, equipment migration, and current validator suites. Expected: zero failures/errors.

- [ ] **Step 5: Commit**

```powershell
git commit -m "feat: migrate saves to decoupled formation"
```

### Task 4: GREEN — fixed six companions with all cards unlocked

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKStarterCompanionTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionFacadeTest.cpp`

- [ ] **Step 1: Add fixed-roster tests**

Assert exactly one companion for each approved role, stable identities across repeated initialization, preserved level/experience/star, and no player-facing recruit/dismiss capability.

For each companion:

```cpp
const TArray<FGameXXKCardDefinition> AllCards =
	FGameXXKCardCatalog::GetCardDefinitionsForOwner(Companion.InstanceId);
TestEqual(TEXT("all companion cards are unlocked"),
	Companion.UnlockedPersonalCardIds.Num(), Companion.PersonalCardIds.Num());
TestEqual(TEXT("default deck contains five legal unique cards"), Companion.SelectedCardIds.Num(), 5);
```

Resolve catalog ownership through the existing companion card identity convention rather than assuming instance IDs when the catalog uses template/role IDs.

- [ ] **Step 2: Implement normalization**

Add `NormalizeFixedCompanionRosterAndCards` that:

- verifies one record for each six approved role;
- keeps existing progression and equipment owner IDs;
- sets `UnlockedPersonalCardIds = PersonalCardIds` in catalog order;
- preserves a valid five-card selected deck;
- otherwise selects the first five valid unique cards deterministically.

- [ ] **Step 3: Remove player-facing lifecycle mutations**

Subsystem recruitment, replacement, and dismissal facades return `false` with a clear capability error in current gameplay. Keep legacy rules only where migration still reads them.

- [ ] **Step 4: Run GREEN and commit**

Run companion rules/facade/starter/card-loadout/equipment ownership suites. Expected: zero failures.

```powershell
git commit -m "feat: fix companion roster and unlock all cards"
```

### Task 5: RED/GREEN — atomic composition/order facade and route NPC projection

**Files:**
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/GameXXKRouteSettlementRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionFacadeTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRouteEventSupportTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteSettlementTest.cpp`

- [ ] **Step 1: Add facade RED tests**

Expose:

```cpp
UFUNCTION(BlueprintPure, Category="GameXXK|Party")
FGameXXKPartyFormationState GetPartyFormation() const;

UFUNCTION(BlueprintCallable, Category="GameXXK|Party")
bool SetPartyFormation(const FGameXXKPartyFormationState& Formation, FString& OutError);
```

Test composition-only changes, position-only swaps, invalid drafts, battle/route locks, MainMenu/WorldMap rejection, compatibility projection, and complete rollback.

- [ ] **Step 2: Implement candidate-copy commit**

```cpp
FGameXXKRuntimeState Candidate = RuntimeState;
if (!IsTownCompanionConfigurationAvailable(Candidate)
	|| !FGameXXKPartyFormationRules::Validate(Candidate, Formation, &OutError))
{
	return false;
}
Candidate.CardRun.PartyFormation = Formation;
FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, OutError))
{
	return false;
}
BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
RuntimeState = MoveTemp(Candidate);
return true;
```

- [ ] **Step 3: Project persistent Quest NPC on route entry**

At route entry, fetch `SelectedQuestNpcId` and its persistent loadout, then populate `ActiveTemporaryQuestNpcId` and `PartySelection.QuestNpc` on the candidate. Event support may unlock availability but never silently replace the selected formation NPC.

- [ ] **Step 4: Preserve persistent composition on route exit**

Route cleanup clears only route-local NPC mirrors, rewards, battles, and route state. It must not modify `PartyFormation.Composition.SelectedQuestNpcId` or `PositionOrder`.

Add consecutive-route tests proving:

```cpp
const FGameXXKPartyFormationState Before = State.CardRun.PartyFormation;
TestTrue(TEXT("route settles"), SettleRoute(State));
TestTrue(TEXT("persistent formation survives"),
	FGameXXKPartyFormationState::StaticStruct()->CompareScriptStruct(
		&State.CardRun.PartyFormation, &Before, PPF_None));
```

- [ ] **Step 5: Run GREEN and commit**

Run formation facade, route entry/event/settlement, SaveGame, and PlayerFlow suites.

```powershell
git commit -m "feat: project decoupled formation through route lifecycle"
```

### Task 6: RED/GREEN — battle, Travel, idle strip, and merchant consumers

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/GameXXKRouteMerchantRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleAdapterTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMerchantRulesTest.cpp`

- [ ] **Step 1: Add order-consumer RED tests**

Use one composition and two position orders. Assert changing only order changes all consumers while IDs remain selected:

```cpp
Formation.PositionOrder.Slots = {
	EGameXXKPartyCategory::Companion,
	EGameXXKPartyCategory::Hero,
	EGameXXKPartyCategory::QuestNpc};

TestEqual(TEXT("battle 1P is selected companion"), Battle.Units[0].UnitId, CompanionId);
TestEqual(TEXT("battle 2P is selected hero"), Battle.Units[1].UnitId, HeroId);
TestEqual(TEXT("battle 3P is selected NPC"), Battle.Units[2].UnitId, QuestNpcId);
```

Repeat for Training Travel order, idle strip, and merchant owner iteration.

- [ ] **Step 2: Switch every consumer to `ResolveOrderedMembers`**

Compatibility mirrors may supply legacy fields but never order. Preserve `PartySlot` as `Index + 1` in every resolved runtime unit.

- [ ] **Step 3: Run GREEN and commit**

Run CardBattleAdapter, Training, Workbench idle, RouteMerchant rules/widget, route battle, and retreat suites.

```powershell
git commit -m "feat: apply decoupled formation order to consumers"
```

### Task 7: RED/GREEN — two-section Workbench Formation editor

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKDesktopTrainingWorkbenchWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKDesktopTrainingWorkbenchWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterWidgetTest.cpp`

- [ ] **Step 1: Add UI RED tests**

Assert three fixed composition cards named for Hero/Companion/QuestNpc, category-filtered candidate grids, and three separate position slots labelled `1P/2P/3P`.

Test interaction:

```cpp
const FGameXXKPartyPositionOrder OrderBefore = Widget->GetFormationDraftForTest().PositionOrder;
TestTrue(TEXT("companion candidate replacement succeeds"), Widget->SelectFormationCompanionForTest(GuardId));
TestEqual(TEXT("replacement keeps order"), Widget->GetFormationDraftForTest().PositionOrder, OrderBefore);

const FGameXXKPartyComposition CompositionBefore = Widget->GetFormationDraftForTest().Composition;
TestTrue(TEXT("2P category frame selects"), Widget->SelectFormationPositionForTest(1));
TestTrue(TEXT("selected middle frame exposes both arrows"),
	Widget->IsFormationMoveLeftVisibleForTest() && Widget->IsFormationMoveRightVisibleForTest());
TestTrue(TEXT("right arrow moves the selected category one slot"),
	Widget->MoveSelectedFormationCategoryForTest(+1));
TestEqual(TEXT("arrow move keeps selected ids"), Widget->GetFormationDraftForTest().Composition, CompositionBefore);
TestFalse(TEXT("moved category at 3P has no right arrow"), Widget->IsFormationMoveRightVisibleForTest());
```

- [ ] **Step 2: Build the composition section**

Create three fixed cards: `主角`, `伙伴`, `任务角色`. Selecting one changes only the candidate filter. Companion candidates show only `刀客 / 守卫 / 药师 / 射手 / 法师 / 阵师`.

- [ ] **Step 3: Build the position-order section**

Create `1P`, `2P`, `3P` slots that display category and selected character. Left-clicking a slot selects its category frame and shows movement arrows beside it. The left arrow swaps the selected category with the immediately preceding slot; the right arrow swaps it with the immediately following slot. Hide the invalid direction at `1P` and `3P`. Keep the selected category active after it moves. Do not use character IDs as the stored position value.

- [ ] **Step 4: Implement draft/apply/discard**

Formation open copies authoritative state. Apply calls `SetPartyFormation`. Local `X`, Backpack parent `X`, and `Tab` discard the draft. Reopening starts from authoritative state.

- [ ] **Step 5: Remove obsolete companion lifecycle controls**

Do not create recruitment, replacement, or dismissal controls. Keep progression and card-deck editing for the six fixed companions. Ensure all companion cards render selectable in this delivery.

- [ ] **Step 6: Run GREEN and commit**

Run Workbench Formation, close-stack, CompanionRoster, card-loadout, and player-flow suites.

```powershell
git commit -m "feat: build decoupled formation editor"
```

### Task 8: full verification, PIE, Luna, and production record

**Files:**
- Verify all Unit C files.

- [ ] **Step 1: Run full automation**

Run fresh reports for:

- `GameXXK.PartyFormation`
- `GameXXK.MVP.SaveGame`
- `GameXXK.MVP.Companion`
- `GameXXK.DesktopTraining.Workbench`
- `GameXXK.Integration.CardBattle`
- `GameXXK.Route.Merchant`
- route settlement/retreat/player-flow suites.

Expected: zero failed/errors; classify existing fixture warnings explicitly.

- [ ] **Step 2: Cold UBT**

```powershell
D:\UE_5.8\Engine\Build\BatchFiles\Build.bat GameXXKEditor Win64 Development -Project="D:\UE5 demo\GameXXK\GameXXK.uproject" -NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2
```

Expected: `Result: Succeeded`.

- [ ] **Step 3: Real pure-2D PIE**

On `/Game/GameXXK/Maps/L_DesktopTrainingHUD`:

1. Open Formation and capture default composition/order.
2. Change Companion while verifying the Companion category keeps its P-slot.
3. Swap Hero and QuestNpc position categories while verifying selected IDs stay fixed.
4. Apply, close, reopen, and verify persistence.
5. Enter Travel/route battle and verify attack/portrait order.
6. Open Merchant and verify offer owner order.
7. Settle route and verify persistent selected Quest NPC/order survive.

- [ ] **Step 4: Luna Max review**

Capture composition selection, candidate filtering, position swap, applied/reopened state, idle strip, battle, and Merchant. Invoke `codex_vision.ps1 -Effort max`. Acceptance: the two sections are visually distinct, category labels are readable, companion profession labels are exact, and no panel loses its approved close affordance.

- [ ] **Step 5: Final integrity checks**

Run:

```powershell
python scripts/harness_state_validator.py
git diff --check
```

Save dirty packages through UE MCP before a normal editor exit. Stage only scoped hunks; leave unrelated dirty work untouched.
