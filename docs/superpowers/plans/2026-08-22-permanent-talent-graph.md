# Permanent 35-Layer Talent Graph Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the permanent shared 35-layer talent graph and apply its approved combat, idle, offline, chest, capacity, and tool effects through authoritative rules.

**Architecture:** Keep serialized node ranks as the only talent source of truth and derive one clamped projection for consumers. Generate the large deterministic catalog from four branch templates, render it with a reusable graph widget, and integrate each effect at its owning gameplay boundary rather than mutating values in UI code.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT/SaveGame, UMG/Slate graph canvas, 64-bit ordinary currency, UE Automation Tests, UE MCP, cold UBT.

---

## File map

- Create `Source/GameXXK/Public/GameXXKTalentTypes.h`: branch/effect/node/progress/projection types.
- Create `Source/GameXXK/Public/GameXXKTalentCatalog.h` and `Source/GameXXK/Private/GameXXKTalentCatalog.cpp`: deterministic root, entries, tracks, edges, positions.
- Create `Source/GameXXK/Public/GameXXKTalentRules.h` and `Source/GameXXK/Private/GameXXKTalentRules.cpp`: validation, price, purchase, projection, caps.
- Create `Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp`: topology/catalog contract.
- Create `Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp`: price, transaction, and cap contracts.
- Create `Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h` and `Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp`: pannable graph and fixed details rail.
- Create `Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp`: layout/state UI contract.
- Modify `Source/GameXXK/Public/GameXXKMVPRules.h`: talent progress in runtime state and 64-bit `PlayerGold`.
- Modify `Source/GameXXK/Public/MVP/GameXXKSaveGame.h` and `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`: next-version talent/currency/capacity migration.
- Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` and `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`: talent purchase/view facade and projections.
- Modify `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h` and `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`: logical capacities.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: embed talent tree and enforce logical slot/page visibility.
- Modify `Source/GameXXK/Public/GameXXKTrainingRules.h` and `Source/GameXXK/Private/GameXXKTrainingRules.cpp`: reward multipliers, offline caps, chest progress/rolls.
- Modify `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: movement-speed walk-step cadence.
- Modify `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp` and combat stat assembly files: clamped combat projection.
- Modify `Source/GameXXK/Private/GameXXKEquipmentEconomyRules.cpp`: tool-only gold and crafted item-level projection.
- Modify existing Training, Inventory, Equipment, CardBattle, Workbench, SaveGame, and player-flow tests.

### Task 1: RED — catalog topology, depth, tracks, and prices

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp`

- [ ] **Step 1: Add catalog contract test**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentCatalogTopologyTest,
	"GameXXK.Talents.Catalog.RootFourBranchesAndThirtyFiveLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentCatalogTopologyTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTalentNodeDefinition>& Nodes = FGameXXKTalentCatalog::GetDefinitions();
	TestEqual(TEXT("one root"), Nodes.FilterByPredicate([](const auto& N) { return N.bRoot; }).Num(), 1);
	TestEqual(TEXT("four one-time entries"), Nodes.FilterByPredicate([](const auto& N) { return N.bBranchEntry; }).Num(), 4);
	TestEqual(TEXT("max depth is 35"), FGameXXKTalentCatalog::GetMaxCostTier(), 35);
	TestTrue(TEXT("catalog validates"), FGameXXKTalentCatalog::Validate());

	const FGameXXKTalentNodeDefinition* Root = FGameXXKTalentCatalog::Find(TEXT("Talent.Root"));
	TestTrue(TEXT("root is one-time 2500"), Root && Root->MaxRank == 1 && FGameXXKTalentRules::GetRankPrice(*Root, 0) == 2500);
	for (const EGameXXKTalentBranch Branch : {
		EGameXXKTalentBranch::Combat,
		EGameXXKTalentBranch::CapacityChest,
		EGameXXKTalentBranch::IdleOffline,
		EGameXXKTalentBranch::Tools})
	{
		const FGameXXKTalentNodeDefinition* Entry = FGameXXKTalentCatalog::FindBranchEntry(Branch);
		TestTrue(TEXT("entry exists"), Entry && Entry->MaxRank == 1 && Entry->CostTier == 0);
	}
	return true;
}

#endif
```

- [ ] **Step 2: Add exact price test**

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentPriceCurveTest,
	"GameXXK.Talents.Rules.PriceCurveInt64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentPriceCurveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("tier zero"), FGameXXKTalentRules::GetPriceForCostTier(0), int64(2500));
	TestEqual(TEXT("tier one"), FGameXXKTalentRules::GetPriceForCostTier(1), int64(3400));
	TestEqual(TEXT("tier five"), FGameXXKTalentRules::GetPriceForCostTier(5), int64(11200));
	TestEqual(TEXT("tier thirty-five"), FGameXXKTalentRules::GetPriceForCostTier(35), int64(91121700));
	TestEqual(TEXT("capacity path total"), FGameXXKTalentRules::GetFullCapacityPathPrice(), int64(1757301500));
	return true;
}
```

- [ ] **Step 3: Add declarations with empty catalogs and run RED**

Cold-build, then run `GameXXK.Talents.Catalog` and `GameXXK.Talents.Rules.PriceCurveInt64`.

Expected: FAIL because root/branches/tracks and prices do not exist.

### Task 2: GREEN — talent types, deterministic catalog, and price math

**Files:**
- Create: `Source/GameXXK/Public/GameXXKTalentTypes.h`
- Create: `Source/GameXXK/Public/GameXXKTalentCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKTalentCatalog.cpp`
- Create: `Source/GameXXK/Public/GameXXKTalentRules.h`
- Create: `Source/GameXXK/Private/GameXXKTalentRules.cpp`

- [ ] **Step 1: Define types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKTalentBranch : uint8 { None, Combat, CapacityChest, IdleOffline, Tools };

UENUM(BlueprintType)
enum class EGameXXKTalentEffect : uint8
{
	None,
	UnlockWarehousePage,
	UnlockOfflineRewards,
	UnlockTools,
	FlatAttack, FlatMaxHP, FlatDefense,
	RouteAttackPercent, RouteFinalDamagePercent, RouteDefensePercent, RouteMaxHPPercent,
	CriticalChancePercent, CriticalDamagePercent, TravelMovementRank,
	BackpackSlots,
	OnlineGoldPercent, OnlineExperiencePercent,
	OfflineGoldPercent, OfflineExperiencePercent,
	OfflineGoldTimePercent, OfflineExperienceTimePercent,
	NormalChestDropPercent, AdvancedChestDropPercent,
	OfflineChestMinutes,
	ToolExperiencePercent, ToolGoldPercent
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentNodeDefinition
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FName Id = NAME_None;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FText DisplayName;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FText Description;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) EGameXXKTalentBranch Branch = EGameXXKTalentBranch::None;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) EGameXXKTalentEffect Effect = EGameXXKTalentEffect::None;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 EffectPerRank = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 MaxRank = 5;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) int32 CostTier = 0;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) FVector2D NormalizedPosition = FVector2D::ZeroVector;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) TArray<FName> PrerequisiteIds;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) bool bRoot = false;
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere) bool bBranchEntry = false;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKTalentProgress
{
	GENERATED_BODY()
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame) TMap<FName, int32> NodeRanks;
};
```

Define `FGameXXKTalentProjection` with `int32` fixed/capacity/time fields, `float` percentage fields, booleans for offline/tools, and helper getters for Backpack/Warehouse pages and walk duration.

- [ ] **Step 2: Generate root and entries**

Create exact IDs:

```text
Talent.Root
Talent.Entry.Combat
Talent.Entry.CapacityChest
Talent.Entry.IdleOffline
Talent.Entry.Tools
```

Root and entries have `MaxRank=1`, `CostTier=0`, and the approved one-time effects. Place entries at four 45-degree directions around root.

- [ ] **Step 3: Generate repeatable tracks**

Use a helper that creates 35 nodes with five ranks, cost tiers 1..35, stable IDs, predecessor edges, and fan-offset positions:

```cpp
void AddTrack(
	TArray<FGameXXKTalentNodeDefinition>& Nodes,
	const TCHAR* Prefix,
	EGameXXKTalentBranch Branch,
	EGameXXKTalentEffect Effect,
	int32 EffectPerRank,
	int32 LayerCount,
	FName EntryId,
	FVector2D Direction,
	float LateralOffset);
```

Generate:

- Capacity: 35 `BackpackSlots`, +1/rank.
- Warehouse milestone nodes at capacity tiers 5, 15, 25, 35, one-time.
- Online/offline gold/experience amount: eight? Use four distinct 35-layer tracks for online gold, online XP, offline gold, offline XP, +2/rank.
- Offline gold/XP time: two 35-layer tracks, +2/rank.
- Normal/advanced chest relative drop: two 35-layer tracks, +2/rank.
- Offline chest time: one 35-layer track, +3 minutes/rank.
- Combat fixed and percentage tracks sufficient to reach the approved caps within about ten graph layers.
- One five-rank `TravelMovementRank` node.
- Tool XP and gold: two ten-layer tracks, +5/rank.

Track node arrays may share visual depth while remaining separate parallel fan lanes. Validate stable unique IDs, all prerequisites, no cycles, cost tiers <=35, ranks 1 or 5, and effect-specific caps.

- [ ] **Step 4: Implement 64-bit price math**

```cpp
int64 FGameXXKTalentRules::GetPriceForCostTier(const int32 CostTier)
{
	const int32 SafeTier = FMath::Clamp(CostTier, 0, 35);
	const double Raw = 2500.0 * FMath::Pow(1.35, static_cast<double>(SafeTier));
	return static_cast<int64>(FMath::RoundToDouble(Raw / 100.0) * 100.0);
}
```

Use checked `int64` accumulation for full-path totals. All ranks of one node return the same cost-tier price.

- [ ] **Step 5: Run GREEN and commit catalog foundation**

Run catalog/price tests. Commit new files only as `feat: add permanent talent catalog foundation`.

### Task 3: RED/GREEN — persistent ranks, 64-bit gold, migration, and purchase transaction

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp`

- [ ] **Step 1: Add RED transaction tests**

Cover root 2500 purchase, insufficient funds, same-node equal price across ranks, successor reveal, cap rejection, and candidate-copy rollback:

```cpp
FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
State.PlayerGold = 2500;
FGameXXKTalentPurchaseResult Result;
TestTrue(TEXT("root purchases"), FGameXXKTalentRules::Purchase(State, TEXT("Talent.Root"), Result));
TestEqual(TEXT("root spends all gold"), State.PlayerGold, int64(0));
TestEqual(TEXT("root rank one"), State.Talents.NodeRanks.FindRef(TEXT("Talent.Root")), 1);
TestFalse(TEXT("root cannot repurchase"), FGameXXKTalentRules::Purchase(State, TEXT("Talent.Root"), Result));
```

- [ ] **Step 2: Widen ordinary gold and add talent progress**

In runtime state:

```cpp
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int64 PlayerGold = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKTalentProgress Talents;
```

Widen save mirrors, pending ordinary-gold rewards, UI formatting, and transaction intermediates that can add to `PlayerGold`. Keep route travel money as `int32`.

- [ ] **Step 3: Add next-version migration**

Old integer gold converts losslessly to `int64`; old saves receive empty talent ranks, logical capacity normalized to preserve occupied slots, and enough Warehouse pages to expose occupied cells. New games start with Backpack 20 and Warehouse page 1 through the zero-rank projection.

- [ ] **Step 4: Implement purchase**

```cpp
bool FGameXXKTalentRules::Purchase(
	FGameXXKRuntimeState& InOutState,
	const FName NodeId,
	FGameXXKTalentPurchaseResult& OutResult)
{
	OutResult = {};
	const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(NodeId);
	if (!Node) return Fail(OutResult, TEXT("Unknown talent node."));
	FGameXXKRuntimeState Candidate = InOutState;
	const int32 RankBefore = Candidate.Talents.NodeRanks.FindRef(NodeId);
	if (RankBefore >= Node->MaxRank) return Fail(OutResult, TEXT("Talent is at maximum rank."));
	if (!ArePrerequisitesMet(Candidate.Talents, *Node)) return Fail(OutResult, TEXT("Talent prerequisites are not met."));
	const int64 Price = GetPriceForCostTier(Node->CostTier);
	if (Candidate.PlayerGold < Price) return Fail(OutResult, TEXT("Not enough gold."));
	Candidate.Talents.NodeRanks.Add(NodeId, RankBefore + 1);
	FGameXXKTalentProjection Projection;
	if (!BuildProjection(Candidate.Talents, Projection)) return Fail(OutResult, TEXT("Talent caps would be exceeded."));
	Candidate.PlayerGold -= Price;
	InOutState = MoveTemp(Candidate);
	OutResult.bPurchased = true;
	OutResult.Price = Price;
	OutResult.RankAfter = RankBefore + 1;
	return true;
}
```

- [ ] **Step 5: Add subsystem facade and run GREEN**

Expose `GetTalentView`, `PurchaseTalentNode`, and `GetTalentProjection`. Use candidate-copy mutation and notify UI only after commit. Run TalentRules and SaveGame suites.

### Task 4: RED/GREEN — projection caps and combat application

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKTalentRules.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardBattleAdapter.cpp`
- Modify: relevant combat stat projection file/tests.

- [ ] **Step 1: Add cap tests**

Build synthetic ranks beyond authored totals and assert projection clamps/rejects:

```cpp
TestEqual(TEXT("flat attack cap"), Projection.FlatAttack, 200);
TestEqual(TEXT("flat health cap"), Projection.FlatMaxHP, 200);
TestEqual(TEXT("flat defense cap"), Projection.FlatDefense, 200);
TestTrue(TEXT("route attack cap"), FMath::IsNearlyEqual(Projection.RouteAttackPercent, 1.0f));
TestTrue(TEXT("final damage cap"), FMath::IsNearlyEqual(Projection.RouteFinalDamagePercent, 1.0f));
TestTrue(TEXT("crit chance cap"), FMath::IsNearlyEqual(Projection.CriticalChance, 0.20f));
TestTrue(TEXT("crit damage cap"), FMath::IsNearlyEqual(Projection.CriticalDamageBonus, 0.50f));
```

- [ ] **Step 2: Implement clamped projection**

Sum `rank * EffectPerRank` per effect, then clamp at the approved aggregate cap. Critical per-rank values come from catalog definitions but must reach, never exceed, 20%/50% at the end of the short branch.

- [ ] **Step 3: Apply once during combat unit assembly**

Apply fixed bonuses to every party member, and route-only percentages to route/challenge battle snapshots:

```cpp
Unit.Attack = FMath::Max(1, Unit.Attack + Projection.FlatAttack);
Unit.MaxHealth = FMath::Max(1, Unit.MaxHealth + Projection.FlatMaxHP);
Unit.Defense = FMath::Max(0, Unit.Defense + Projection.FlatDefense);
Unit.Attack = ScaleStat(Unit.Attack, Projection.RouteAttackPercent);
Unit.MaxHealth = ScaleStat(Unit.MaxHealth, Projection.RouteMaxHPPercent);
Unit.Defense = ScaleStat(Unit.Defense, Projection.RouteDefensePercent);
```

Final damage and critical rules apply in the central damage resolver exactly once. Add deterministic RNG tests for 0% and capped 20% critical chance.

- [ ] **Step 4: Run combat GREEN**

Run talent cap, CardBattleAdapter, and combat rules suites.

### Task 5: RED/GREEN — logical Backpack capacity and Warehouse pages

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKTalentRules.cpp`
- Modify: `Source/GameXXK/Public/GameXXKDesktopInventoryRules.h`
- Modify: `Source/GameXXK/Private/GameXXKDesktopInventoryRules.cpp`
- Modify: Workbench/inventory UI and tests.

- [ ] **Step 1: Add capacity/milestone tests**

Assert zero-rank 20/one page, root gives page 2, capacity entry gives 25, each repeat rank adds one slot, max 200, and capacities 50/100/150/200 reveal/purchase pages 3/4/5/6.

- [ ] **Step 2: Add rules getters**

```cpp
static int32 GetUnlockedBackpackCapacity(const FGameXXKRuntimeState& State);
static int32 GetUnlockedWarehousePageCount(const FGameXXKRuntimeState& State);
static int32 GetUnlockedWarehouseCapacity(const FGameXXKRuntimeState& State)
{
	return FMath::Min(200, GetUnlockedWarehousePageCount(State) * 36);
}
```

- [ ] **Step 3: Enforce logical limits**

`FindFirstEmptySlot`, moves, drops, sorting, right-click transfers, and UI slot creation iterate only unlocked logical capacity/page count. Physical arrays remain 200. Locked slots never accept writes.

- [ ] **Step 4: Preserve old saves**

Migration unlocks at least `HighestOccupiedIndex + 1` Backpack capacity and `ceil((HighestWarehouseIndex+1)/36)` pages, clamped to physical limits. Never move or delete entries.

- [ ] **Step 5: Run Inventory/Workbench GREEN**

Run DesktopInventory and Workbench capacity/page tests.

### Task 6: RED/GREEN — online/offline reward and time tracks

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKTrainingRules.h`
- Modify: `Source/GameXXK/Private/GameXXKTrainingRules.cpp`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTrainingRulesTest.cpp`

- [ ] **Step 1: Add multiplier and cap tests**

Assert every full 35-layer amount/time track produces +350% and independent tracks do not leak:

```cpp
TestTrue(TEXT("online gold +350"), FMath::IsNearlyEqual(Projection.OnlineGoldMultiplier, 4.5f));
TestTrue(TEXT("offline XP +350"), FMath::IsNearlyEqual(Projection.OfflineExperienceMultiplier, 4.5f));
TestEqual(TEXT("offline gold cap 108h"), Projection.OfflineGoldCapSeconds, 108 * 60 * 60);
TestEqual(TEXT("offline XP cap 108h"), Projection.OfflineExperienceCapSeconds, 108 * 60 * 60);
TestEqual(TEXT("offline chest cap"), Projection.OfflineChestCapSeconds, 16 * 60 * 60 + 45 * 60);
```

- [ ] **Step 2: Apply online amount multipliers at encounter settlement**

Scale base online gold/XP once with checked 64-bit rounding; do not scale route travel money or quest rewards.

- [ ] **Step 3: Split offline simulation caps**

Clamp gold and XP accrual independently to their 24h..108h unlocked caps. Clamp chest-window simulation independently to 8h..16h45m. One reward type reaching its cap must not stop the other types' allowed simulation.

- [ ] **Step 4: Run Training offline GREEN**

Cover partial caps, full caps, zero elapsed, one huge elapsed interval, deterministic equivalence to chunked simulation, and save/resume.

### Task 7: RED/GREEN — chest windows, relative chance, and encounter mapping

**Files:**
- Modify: Training types/rules/tests and save migration.

- [ ] **Step 1: Add exact window tests**

Use persisted accumulated seconds rather than ambiguous remaining cooldowns. Assert:

```cpp
TestEqual(TEXT("online normal window"), GetChestWindowSeconds(false, EGameXXKTrainingRewardTier::NormalChest), 120);
TestEqual(TEXT("online advanced window"), GetChestWindowSeconds(false, EGameXXKTrainingRewardTier::AdvancedChest), 180);
TestEqual(TEXT("offline normal window"), GetChestWindowSeconds(true, EGameXXKTrainingRewardTier::NormalChest), 240);
TestEqual(TEXT("offline advanced window"), GetChestWindowSeconds(true, EGameXXKTrainingRewardTier::AdvancedChest), 360);
```

Run an online simulation longer than eight hours and assert the windows remain 120/180; there is no duration downgrade.

- [ ] **Step 2: Migrate timer fields to accumulated progress**

Persist `TravelNormalChestWindowProgressSeconds` and `TravelAdvancedChestWindowProgressSeconds`. Migrate current remaining values into conservative progress without granting an immediate duplicate roll.

- [ ] **Step 3: Implement eligibility and roll consumption**

Accumulate matching timers by online/offline elapsed time, cap at the active threshold, and on encounter completion:

```cpp
const bool bNormalPool = EncounterKind == EGameXXKTrainingEncounterKind::Normal;
const bool bAdvancedPool = EncounterKind == EGameXXKTrainingEncounterKind::Elite
	|| EncounterKind == EGameXXKTrainingEncounterKind::Boss;
```

Only the matching eligible pool rolls. Base chance is 0.25. Apply relative bonus:

```cpp
const float EffectiveChance = FMath::Clamp(0.25f * (1.0f + RelativeTalentBonus), 0.0f, 1.0f);
```

Consume/reset only the matching eligible timer after the deterministic roll. Boss uses advanced chance and is not guaranteed.

- [ ] **Step 4: Run chest GREEN**

Cover normal/elite/Boss mapping, no eligibility, failed roll, successful roll, 100% clamp, online/offline intervals, and chunk determinism.

### Task 8: RED/GREEN — real movement-speed duration

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`
- Modify: Workbench/Training tests.

- [ ] **Step 1: Add duration table test**

Assert ranks 0..5 return `{5.0, 4.5, 4.0, 3.5, 3.0, 2.5}` and that prior death presentation time consumes none of the interval.

- [ ] **Step 2: Keep five logical walk steps and vary their real cadence**

```cpp
const float RequiredWalkSeconds = Projection.GetTravelWalkSeconds();
const float WalkStepCadence = RequiredWalkSeconds
	/ static_cast<float>(FGameXXKTrainingRules::TravelEncounterSpawnDelaySeconds);
const float AdvanceCadence = AuthoritativeTravelRuntime.Phase == EGameXXKTrainingTravelPhase::Walking
	? WalkStepCadence
	: 1.0f;
```

Advance one logical step whenever `TravelAccumulator >= AdvanceCadence`; retain the existing queued-presentation gate. Combat cadence remains one second.

- [ ] **Step 3: Run GREEN**

Measure every rank boundary with 0.01-second ticks; enemies/bars remain hidden for the full effective interval.

### Task 9: RED/GREEN — tool unlock, experience, crafted item level, and tool gold

**Files:**
- Modify: talent/runtime/equipment economy/subsystem/Workbench files and tests.

- [ ] **Step 1: Add tool projection tests**

Assert the one-time entry unlocks all five modes, ten-layer tracks reach 4.5x total gain (+250%), and no modifier changes idle/route/quest rewards.

- [ ] **Step 2: Persist tool progression**

Add `int64 ToolExperience` and derive ToolLevel through a deterministic threshold table. Tool XP talent multiplies only XP awarded by successful tool transactions.

- [ ] **Step 3: Apply crafted item level**

When Combine or another crafting rule creates a rolled equipment instance, use clamped ToolLevel as the request's item level. Enhancement/reforge/socket do not rewrite an existing item's base item level.

- [ ] **Step 4: Apply tool gold only**

Multiply gold returned by successful dismantle/tool transactions with checked 64-bit math. Do not alter `PendingTravelGold`, route money, quest reward, or settlement APIs.

- [ ] **Step 5: Gate the Workbench tools panel**

Before the entry purchase, Tools shows a locked explanation and no usable modes. After purchase all five mode tabs enable together; their underlying rules still decide whether a specific recipe/input is valid.

- [ ] **Step 6: Run Equipment/Workbench GREEN**

Cover lock/unlock, +250% XP/gold caps, crafted item level, invalid recipe no reward, and no cross-system leakage.

### Task 10: RED/GREEN — talent graph widget and Workbench integration

**Files:**
- Create: `Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp`
- Modify: Workbench files/tests.

- [ ] **Step 1: Add widget structure test**

Assert approved paper background, one center root, four 45-degree entries, lines below nodes, hidden successors, fixed details rail, ordinary gold text, rank/price text, upgrade button, local CloseInk, and pannable content larger than viewport.

- [ ] **Step 2: Build programmatic graph**

Use `UCanvasPanel` inside a clipped/pannable viewport. Build line images first (lower Z), then node buttons. Convert normalized catalog positions into content-local positions only. The detail rail is a fixed sibling outside the pannable canvas.

- [ ] **Step 3: Bind authoritative views**

Node click selects only. Upgrade calls subsystem purchase and refreshes projection/view. Locked nodes expose reason; hidden nodes are not constructed. Local `X` calls the Workbench central-close action and returns to Backpack.

- [ ] **Step 4: Embed in Workbench**

`BuildTalentsPanel` constructs `UGameXXKTalentTreeWidget`, assigns the subsystem, and gives it the existing content rect. Do not draw the old twelve placeholders.

- [ ] **Step 5: Run widget/Workbench GREEN**

Expected: all talent widget and complete Workbench tests pass.

### Task 11: Full verification, real PIE, Luna, production record, and commit

- [ ] **Step 1: Cold UBT**

Run canonical cold build. Expected: `Result: Succeeded`.

- [ ] **Step 2: Focused suites**

Run fresh reports for:

```text
GameXXK.Talents
GameXXK.DesktopInventory
GameXXK.Training
GameXXK.MVP.SaveGame
GameXXK.Equipment
GameXXK.CardBattle
GameXXK.DesktopTraining.Workbench
```

Parse every report; expected zero failed/errors.

- [ ] **Step 3: Real pure-2D talent flow**

On `L_DesktopTrainingHUD`, buy root and four entries, inspect all four directions, rank one node five times, verify equal per-rank price and next-layer 1.35 price, pan to capacity/reward depth, close to Backpack, reopen, and verify persisted ranks.

- [ ] **Step 4: Real integration samples**

Verify movement ranks 0 and 5 (5.0 and 2.5 seconds), normal/advanced online windows 2/3 minutes with no eight-hour degradation, offline windows 4/6 minutes, capacity/page milestones, 108h offline amount caps, 16h45 chest cap, tool unlock/XP/gold, and combat caps.

- [ ] **Step 5: Luna Max review**

Capture root-only, four-entry, mid-depth, selected details, locked reason, max-rank, and panned deep branch. Acceptance: paper/map style, 45-degree fan, readable lines/nodes, details rail never covers graph, local X aligned, no unrelated Workbench layout changes.

- [ ] **Step 6: Update production acceptance**

Add a dated entry to `docs/production/current-goal-acceptance.md` with exact automation reports, cold UBT result, PIE evidence, save version, and Luna report. Preserve historical entries.

- [ ] **Step 7: Commit Unit D**

Stage only Unit D source/tests/docs and commit:

```powershell
git commit -m "feat: add permanent workbench talent progression"
```
