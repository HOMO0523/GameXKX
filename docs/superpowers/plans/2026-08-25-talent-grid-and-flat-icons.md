# Talent Grid and Flat Icons Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the current free-angle permanent-talent graph and illustrated atlas with a collision-free four-quadrant grid that uses project Item Slots, flat transparent icons, and rank-one progression.

**Architecture:** Keep `FGameXXKTalentProgress::NodeRanks` and every existing node ID authoritative. Rewire catalog positions and prerequisites into fixed 180-pixel grid cells, render nodes through the existing `UGameXXKTalentTreeWidget`, and import only the missing flat icons while reusing current navigation art. UI refresh remains local to the talent graph and publishes the existing purchase-completed event to the workbench.

**Tech Stack:** Unreal Engine 5.8 C++, UMG/Slate, UE Automation Tests, UE MCP Python asset import, image generation for missing transparent raster icons.

**Project constraint:** Work on root `main`; do not create a worktree and do not use UnrealBridge.

---

## File map

- `Source/GameXXK/Private/GameXXKTalentCatalog.cpp`: node IDs, effect tracks, fixed grid positions, prerequisites, cost tiers.
- `Source/GameXXK/Private/GameXXKTalentRules.cpp`: rank-one prerequisite evaluation and purchase validation.
- `Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h`: test-visible presentation snapshot accessors.
- `Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp`: Item Slot node component, icon routing, labels, selected overlay, snapped lines.
- `Source/GameXXK/Private/UI/GameXXKDesktopTrainingWorkbenchWidget.cpp`: talent page title/detail button styling and purchase propagation.
- `Source/GameXXK/Private/Tests/GameXXKTalentGridLayoutTest.cpp`: topology, angle, quadrant, overlap, and `1 → 2 → 1` tests.
- `Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp`: rank-one reveal and AND-prerequisite transaction tests.
- `Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp`: node component resources, text placement, icon aspect, upgrade button, live refresh.
- `SourceArt/UI/Talents/flat/`: transparent source PNGs and deterministic manifest.
- `Content/Python/gamexxk_import_flat_talent_icons.py`: imports/configures selected-slot and icon textures.
- `Content/GameXXK/UI/Talents/Flat/`: imported UI textures.

### Task 1: Lock the fixed-grid topology with failing tests

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKTalentGridLayoutTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp`

- [ ] **Step 1: Add the grid topology automation test**

Create the new test with the following topology checks:

```cpp
#include "Misc/AutomationTest.h"
#include "GameXXKTalentCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	constexpr float GridPitch = 180.0f;
	constexpr float NodeFootprintWidth = 130.0f;
	constexpr float NodeFootprintHeight = 132.0f;

	bool IsAllowedConnection(const FVector2D Delta)
	{
		const float X = FMath::Abs(Delta.X);
		const float Y = FMath::Abs(Delta.Y);
		return (FMath::IsNearlyZero(X) && Y > 0.0f)
			|| (FMath::IsNearlyZero(Y) && X > 0.0f)
			|| (X > 0.0f && FMath::IsNearlyEqual(X, Y, 0.1f));
	}

	FBox2D BoundsFor(const FGameXXKTalentNodeDefinition& Node)
	{
		const FVector2D Half(NodeFootprintWidth * 0.5f, NodeFootprintHeight * 0.5f);
		return FBox2D(Node.GraphPosition - Half, Node.GraphPosition + Half);
	}

	bool IsInsideBranchQuadrant(const FGameXXKTalentNodeDefinition& Node)
	{
		switch (Node.Branch)
		{
		case EGameXXKTalentBranch::Combat: return Node.GraphPosition.X < 0.0f && Node.GraphPosition.Y < 0.0f;
		case EGameXXKTalentBranch::CapacityChest: return Node.GraphPosition.X < 0.0f && Node.GraphPosition.Y > 0.0f;
		case EGameXXKTalentBranch::IdleOffline: return Node.GraphPosition.X > 0.0f && Node.GraphPosition.Y < 0.0f;
		case EGameXXKTalentBranch::Tools: return Node.GraphPosition.X > 0.0f && Node.GraphPosition.Y > 0.0f;
		default: return Node.bRoot && Node.GraphPosition.IsNearlyZero();
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentGridLayoutTest,
	"GameXXK.Talents.Grid.FixedAnglesQuadrantsAndNoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentGridLayoutTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTalentNodeDefinition>& Nodes = FGameXXKTalentCatalog::GetDefinitions();
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FGameXXKTalentNodeDefinition& Node = Nodes[Index];
		TestTrue(*FString::Printf(TEXT("%s stays in its quadrant"), *Node.Id.ToString()), IsInsideBranchQuadrant(Node));
		for (const FName PrerequisiteId : Node.PrerequisiteIds)
		{
			const FGameXXKTalentNodeDefinition* Prerequisite = FGameXXKTalentCatalog::Find(PrerequisiteId);
			TestTrue(TEXT("prerequisite exists"), Prerequisite != nullptr);
			if (Prerequisite)
			{
				TestTrue(TEXT("connection is horizontal, vertical, or diagonal 45"),
					IsAllowedConnection(Node.GraphPosition - Prerequisite->GraphPosition));
			}
		}
		for (int32 OtherIndex = Index + 1; OtherIndex < Nodes.Num(); ++OtherIndex)
		{
			TestFalse(TEXT("node footprints never overlap"),
				BoundsFor(Node).Intersect(BoundsFor(Nodes[OtherIndex])));
		}
	}
	return true;
}
#endif
```

- [ ] **Step 2: Add exact `1 → 2 → 1` prerequisite assertions**

Append assertions for representative cycles:

```cpp
const auto RequiresExactly = [this](const TCHAR* NodeId, std::initializer_list<const TCHAR*> Expected)
{
	const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(FName(NodeId));
	if (!TestNotNull(NodeId, Node)) return;
	TestEqual(TEXT("prerequisite count"), Node->PrerequisiteIds.Num(), static_cast<int32>(Expected.size()));
	for (const TCHAR* RequiredId : Expected)
	{
		TestTrue(TEXT("expected prerequisite is present"), Node->PrerequisiteIds.Contains(FName(RequiredId)));
	}
};
RequiresExactly(TEXT("Talent.Combat.FlatAttack.01"), {TEXT("Talent.Entry.Combat")});
RequiresExactly(TEXT("Talent.Combat.FlatHealth.01"), {TEXT("Talent.Entry.Combat")});
RequiresExactly(TEXT("Talent.Combat.FlatAttack.08"),
	{TEXT("Talent.Combat.FlatAttack.01"), TEXT("Talent.Combat.FlatHealth.01")});
RequiresExactly(TEXT("Talent.Tools.Experience.01"), {TEXT("Talent.Entry.Tools")});
RequiresExactly(TEXT("Talent.Tools.Gold.01"), {TEXT("Talent.Entry.Tools")});
```

- [ ] **Step 3: Compile and run the new test to prove RED**

Run:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --filter "[TDD]" --log-lines 100
python -c "import sys; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; print(UnrealMCPClient(timeout=60).execute_console_command('Automation RunTests GameXXK.Talents.Grid'))"
```

Expected: compile succeeds; `GameXXK.Talents.Grid.FixedAnglesQuadrantsAndNoOverlap` fails because the current fan layout has arbitrary offsets and overlapping full-node footprints.

- [ ] **Step 4: Commit the failing topology test**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKTalentGridLayoutTest.cpp Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp
git commit -m "test: define fixed permanent talent grid"
```

### Task 2: Rewire the catalog into mirrored 180-pixel cells

**Files:**
- Modify: `Source/GameXXK/Private/GameXXKTalentCatalog.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp`

- [ ] **Step 1: Add deterministic grid helpers**

Add these helpers inside the catalog implementation namespace:

```cpp
constexpr float TalentGridPitch = 180.0f;

FVector2D GridPoint(const EGameXXKTalentBranch Branch, const int32 X, const int32 Y)
{
	const float SignX = Branch == EGameXXKTalentBranch::Combat
		|| Branch == EGameXXKTalentBranch::CapacityChest ? -1.0f : 1.0f;
	const float SignY = Branch == EGameXXKTalentBranch::Combat
		|| Branch == EGameXXKTalentBranch::IdleOffline ? -1.0f : 1.0f;
	return FVector2D(SignX * X * TalentGridPitch, SignY * Y * TalentGridPitch);
}

FGameXXKTalentNodeDefinition& RequireNode(
	TArray<FGameXXKTalentNodeDefinition>& Nodes,
	const FName NodeId)
{
	FGameXXKTalentNodeDefinition* Node = Nodes.FindByPredicate(
		[NodeId](const FGameXXKTalentNodeDefinition& Candidate) { return Candidate.Id == NodeId; });
	checkf(Node, TEXT("Missing permanent talent node %s"), *NodeId.ToString());
	return *Node;
}

void PlaceNode(
	TArray<FGameXXKTalentNodeDefinition>& Nodes,
	const FName NodeId,
	const FVector2D Position,
	const TArray<FName>& Prerequisites,
	const int32 CostTier)
{
	FGameXXKTalentNodeDefinition& Node = RequireNode(Nodes, NodeId);
	Node.GraphPosition = Position;
	Node.PrerequisiteIds = Prerequisites;
	Node.CostTier = FMath::Clamp(CostTier, 0, 35);
	Node.GraphLayer = Node.CostTier;
}

TArray<FName> NumberedIds(const TCHAR* Prefix, const int32 First, const int32 Last)
{
	TArray<FName> Result;
	for (int32 Index = First; Index <= Last; ++Index)
	{
		Result.Add(FName(*FString::Printf(TEXT("%s.%02d"), Prefix, Index)));
	}
	return Result;
}

void PlaceStraightTrack(
	TArray<FGameXXKTalentNodeDefinition>& Nodes,
	const TArray<FName>& NodeIds,
	const FVector2D Start,
	const FVector2D Step,
	const FName RootPrerequisite,
	const int32 FirstCostTier,
	const bool bPreserveAuthoredCostTier = false)
{
	FName Previous = RootPrerequisite;
	for (int32 Index = 0; Index < NodeIds.Num(); ++Index)
	{
		FGameXXKTalentNodeDefinition& Node = RequireNode(Nodes, NodeIds[Index]);
		Node.GraphPosition = Start + Step * Index;
		Node.PrerequisiteIds = {Previous};
		if (!bPreserveAuthoredCostTier)
		{
			Node.CostTier = FMath::Clamp(FirstCostTier + Index, 1, 35);
			Node.GraphLayer = Node.CostTier;
		}
		Previous = Node.Id;
	}
}
```

- [ ] **Step 2: Apply the exact approved branch mapping**

After constructing all existing definitions, call `ApplyApprovedGridLayout(Nodes)`. Its data tables must use these exact IDs:

```cpp
const TArray<FName> CombatMains = {
	TEXT("Talent.Entry.Combat"), TEXT("Talent.Combat.FlatAttack.08"),
	TEXT("Talent.Combat.FlatHealth.08"), TEXT("Talent.Combat.FlatDefense.08"),
	TEXT("Talent.Combat.CriticalDamage.05")};
const TArray<TPair<TArray<FName>, TArray<FName>>> CombatSides = {
	{NumberedIds(TEXT("Talent.Combat.FlatAttack"), 1, 7), NumberedIds(TEXT("Talent.Combat.FlatHealth"), 1, 7)},
	{NumberedIds(TEXT("Talent.Combat.FlatDefense"), 1, 7), {TEXT("Talent.Combat.Movement.01")}},
	{NumberedIds(TEXT("Talent.Combat.AttackPercent"), 1, 10), NumberedIds(TEXT("Talent.Combat.FinalDamage"), 1, 10)},
	{NumberedIds(TEXT("Talent.Combat.DefensePercent"), 1, 10), NumberedIds(TEXT("Talent.Combat.HealthPercent"), 1, 10)},
	{NumberedIds(TEXT("Talent.Combat.CriticalChance"), 1, 4), NumberedIds(TEXT("Talent.Combat.CriticalDamage"), 1, 4)}};

const TArray<FName> CapacityMains = {
	TEXT("Talent.Entry.CapacityChest"), TEXT("Talent.Capacity.Backpack.34"), TEXT("Talent.Capacity.Backpack.35")};
const TArray<TPair<TArray<FName>, TArray<FName>>> CapacitySides = {
	{NumberedIds(TEXT("Talent.Capacity.Backpack"), 1, 33),
		{TEXT("Talent.Capacity.WarehousePage.03"), TEXT("Talent.Capacity.WarehousePage.04"),
		 TEXT("Talent.Capacity.WarehousePage.05"), TEXT("Talent.Capacity.WarehousePage.06")}},
	{NumberedIds(TEXT("Talent.Chest.NormalDrop"), 1, 35), NumberedIds(TEXT("Talent.Chest.AdvancedDrop"), 1, 35)},
	{NumberedIds(TEXT("Talent.Chest.OfflineTime"), 1, 35), {}}};

const TArray<FName> IdleMains = {
	TEXT("Talent.Entry.IdleOffline"), TEXT("Talent.Idle.OnlineGold.35"), TEXT("Talent.Idle.OnlineExperience.35")};
const TArray<TPair<TArray<FName>, TArray<FName>>> IdleSides = {
	{NumberedIds(TEXT("Talent.Idle.OnlineGold"), 1, 34), NumberedIds(TEXT("Talent.Idle.OnlineExperience"), 1, 34)},
	{NumberedIds(TEXT("Talent.Idle.OfflineGold"), 1, 35), NumberedIds(TEXT("Talent.Idle.OfflineExperience"), 1, 35)},
	{NumberedIds(TEXT("Talent.Idle.OfflineGoldTime"), 1, 35), NumberedIds(TEXT("Talent.Idle.OfflineExperienceTime"), 1, 35)}};

const TArray<FName> ToolMains = {TEXT("Talent.Entry.Tools")};
const TArray<TPair<TArray<FName>, TArray<FName>>> ToolSides = {
	{NumberedIds(TEXT("Talent.Tools.Experience"), 1, 10), NumberedIds(TEXT("Talent.Tools.Gold"), 1, 10)}};
```

For each branch/cycle `N`:

```cpp
const FVector2D MainPosition = GridPoint(Branch, N + 1, N + 1);
PlaceNode(Nodes, MainId, MainPosition, MainPrerequisites, N == 0 ? 0 : N);
PlaceStraightTrack(Nodes, HorizontalIds, MainPosition + GridPoint(Branch, 1, 0),
	GridPoint(Branch, 1, 0), MainId, N + 1);
PlaceStraightTrack(Nodes, VerticalIds, MainPosition + GridPoint(Branch, 0, 1),
	GridPoint(Branch, 0, 1), MainId, N + 1, Branch == EGameXXKTalentBranch::CapacityChest && N == 0);
```

The next main prerequisite array is exactly the first horizontal and first vertical ID from the preceding cycle. A one-sided final cycle has no following main.

- [ ] **Step 3: Use unified main-line labels and icons without changing effects**

Set display metadata only:

```cpp
void ConfigureMainPresentation(
	FGameXXKTalentNodeDefinition& Node,
	const FString& Prefix,
	const int32 Ordinal,
	const EGameXXKTalentIcon Icon)
{
	Node.DisplayName = FText::FromString(FString::Printf(TEXT("%s %02d"), *Prefix, Ordinal));
	Node.Icon = Icon;
}
```

Use `战斗根基`, `行囊根基`, `行旅根基`, and `百工根基`. Do not modify `Effect`, `EffectPerRank`, `MaxRank`, or the node ID.

- [ ] **Step 4: Run topology and catalog tests**

Run the cold pipeline, then:

```powershell
python -c "import sys; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; print(UnrealMCPClient(timeout=60).execute_console_command('Automation RunTests GameXXK.Talents.Catalog')); print(UnrealMCPClient(timeout=60).execute_console_command('Automation RunTests GameXXK.Talents.Grid'))"
```

Expected: catalog and grid tests report `Result={Success}`; maximum cost tier remains 35.

- [ ] **Step 5: Commit catalog layout**

```powershell
git add Source/GameXXK/Private/GameXXKTalentCatalog.cpp Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp
git commit -m "feat: arrange talents on mirrored fixed grid"
```

### Task 3: Prove and preserve rank-one progression

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp`
- Modify only if test exposes a regression: `Source/GameXXK/Private/GameXXKTalentRules.cpp`

- [ ] **Step 1: Replace the old max-rank reveal expectation**

In `FGameXXKTalentPurchaseTransactionTest`, purchase one rank and assert the next same-line node is revealed:

```cpp
State.PlayerGold = 3400;
TestTrue(TEXT("one attack rank purchases"),
	FGameXXKTalentRules::Purchase(State, FirstAttack, Result));
TestEqual(TEXT("first attack stays at rank one"), State.Talents.NodeRanks.FindRef(FirstAttack), 1);
TestTrue(TEXT("rank one reveals the next same-line node"),
	SecondAttackNode && FGameXXKTalentRules::IsRevealed(State.Talents, *SecondAttackNode));
```

- [ ] **Step 2: Add the dual-prerequisite main test**

```cpp
const FName HorizontalRoot(TEXT("Talent.Combat.FlatAttack.01"));
const FName VerticalRoot(TEXT("Talent.Combat.FlatHealth.01"));
const FGameXXKTalentNodeDefinition* NextMain =
	FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatAttack.08"));
State.Talents.NodeRanks.Add(HorizontalRoot, 1);
TestFalse(TEXT("one side alone does not reveal next main"),
	NextMain && FGameXXKTalentRules::IsRevealed(State.Talents, *NextMain));
State.Talents.NodeRanks.Add(VerticalRoot, 1);
TestTrue(TEXT("both side roots at rank one reveal next main"),
	NextMain && FGameXXKTalentRules::IsRevealed(State.Talents, *NextMain));
```

- [ ] **Step 3: Run the rules suite**

```powershell
python -c "import sys; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; print(UnrealMCPClient(timeout=60).execute_console_command('Automation RunTests GameXXK.Talents.Rules'))"
```

Expected: all talent rule tests succeed. If the test fails, keep `LockReasonFor` and `ValidateProgress` prerequisite checks at `RankOf(...) <= 0`; do not reintroduce max-rank checks.

- [ ] **Step 4: Commit progression tests**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp Source/GameXXK/Private/GameXXKTalentRules.cpp
git commit -m "test: enforce rank-one talent progression"
```

### Task 4: Produce and import the flat icon kit

**Files:**
- Create: `SourceArt/UI/Talents/flat/T_TalentFlat_*.png`
- Create: `SourceArt/UI/Talents/flat/manifest.json`
- Create: `Content/Python/gamexxk_import_flat_talent_icons.py`
- Create: `Content/GameXXK/UI/Talents/Flat/*.uasset`
- Reuse: `Content/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.uasset`
- Reuse: `Content/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.uasset`
- Reuse: `Content/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.uasset`

- [ ] **Step 1: Generate the missing single-symbol art**

Use the image-generation workflow with the current `T_TrainingNavTalents` screenshot as style reference. Generate one transparent 4×3 source sheet containing exactly these eleven subjects in order: sword, heart, shield, four-point star, cloth shoe, cloth backpack, single coin, single scroll, crescent moon, hourglass, single chest.

Prompt constraints must state: transparent background; one subject per square; two main colors maximum; thick dark-ink outline; no paper, frame, text, rays, speed lines, piles, scenery, gradients, or shadows outside the subject.

- [ ] **Step 2: Split into independent square PNGs and record deterministic metadata**

Split cells without repainting and write `manifest.json` with this shape:

```json
{
  "version": 1,
  "canvas": [512, 512],
  "icons": [
    {"name": "Attack", "file": "T_TalentFlat_Attack.png"},
    {"name": "Health", "file": "T_TalentFlat_Health.png"},
    {"name": "Defense", "file": "T_TalentFlat_Defense.png"},
    {"name": "Critical", "file": "T_TalentFlat_Critical.png"},
    {"name": "Movement", "file": "T_TalentFlat_Movement.png"},
    {"name": "Backpack", "file": "T_TalentFlat_Backpack.png"},
    {"name": "Gold", "file": "T_TalentFlat_Gold.png"},
    {"name": "Experience", "file": "T_TalentFlat_Experience.png"},
    {"name": "Offline", "file": "T_TalentFlat_Offline.png"},
    {"name": "Time", "file": "T_TalentFlat_Time.png"},
    {"name": "Chest", "file": "T_TalentFlat_Chest.png"}
  ]
}
```

Verify every file is 512×512 RGBA, has transparent corners, and has a non-transparent bounding box occupying 60–80% of each dimension.

- [ ] **Step 3: Import and configure UI textures through UE MCP**

The import script must set:

```python
texture.set_editor_property("mip_gen_settings", unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
texture.set_editor_property("compression_settings", unreal.TextureCompressionSettings.TC_EDITOR_ICON)
texture.set_editor_property("lod_group", unreal.TextureGroup.TEXTUREGROUP_UI)
texture.set_editor_property("filter", unreal.TextureFilter.TF_BILINEAR)
texture.set_editor_property("address_x", unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property("address_y", unreal.TextureAddress.TA_CLAMP)
texture.set_editor_property("srgb", True)
texture.set_editor_property("never_stream", True)
```

Also import the existing source `SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/UIV4_item_slot_selected.png` as `/Game/GameXXK/UI/Talents/Flat/T_TalentItemSlotSelected`.

Run:

```powershell
python -c "import sys,json; sys.path.insert(0, r'scripts'); from ue_mcp_client import UnrealMCPClient; print(json.dumps(UnrealMCPClient(timeout=180).run_project_python_file('Content/Python/gamexxk_import_flat_talent_icons.py'), ensure_ascii=False, indent=2))"
```

Expected: JSON reports `ok: true`, eleven new icon textures, one selected-slot texture, and no dirty package remains after save.

- [ ] **Step 4: Visually review the imported icons at native size and 90-pixel node size**

Reject any icon that has an internal paper square, illegible detail at 90 pixels, inconsistent line weight, or content outside the 60–80% fill range. Regenerate only rejected subjects.

- [ ] **Step 5: Commit the approved art and importer**

```powershell
git add SourceArt/UI/Talents/flat Content/Python/gamexxk_import_flat_talent_icons.py Content/GameXXK/UI/Talents/Flat
git commit -m "art: add flat permanent talent icon kit"
```

### Task 5: Rebuild the node component with Item Slot and external labels

**Files:**
- Modify: `Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp`

- [ ] **Step 1: Add failing presentation assertions and test accessors**

Expose these read-only test methods:

```cpp
FString GetNodeFrameResourcePathForTest(FName NodeId) const;
FString GetNodeIconResourcePathForTest(FName NodeId) const;
FVector2D GetNodeIconDesiredSizeForTest(FName NodeId) const;
FText GetNodeNameForTest(FName NodeId) const;
FText GetNodeRankForTest(FName NodeId) const;
bool IsNodeTextInsideButtonForTest(FName NodeId) const;
FString GetPurchaseButtonResourcePathForTest() const;
FText GetPurchaseButtonLabelForTest() const;
```

Add assertions:

```cpp
TestTrue(TEXT("node uses MasterV2 Item Slot"),
	Widget->GetNodeFrameResourcePathForTest(TEXT("Talent.Root")).Contains(TEXT("T_MasterV2_ItemSlot")));
TestTrue(TEXT("root reuses the current talent navigation icon"),
	Widget->GetNodeIconResourcePathForTest(TEXT("Talent.Root")).Contains(TEXT("T_TrainingNavTalents")));
TestEqual(TEXT("icon is square 74x74 inside the 90 slot"),
	Widget->GetNodeIconDesiredSizeForTest(TEXT("Talent.Root")), FVector2D(74.0f, 74.0f));
TestEqual(TEXT("root name sits below the image"),
	Widget->GetNodeNameForTest(TEXT("Talent.Root")), FText::FromString(TEXT("行旅根基")));
TestEqual(TEXT("root rank sits below the image"),
	Widget->GetNodeRankForTest(TEXT("Talent.Root")), FText::FromString(TEXT("0/1")));
TestFalse(TEXT("no text is embedded in the slot button"),
	Widget->IsNodeTextInsideButtonForTest(TEXT("Talent.Root")));
TestEqual(TEXT("detail action is always Upgrade"),
	Widget->GetPurchaseButtonLabelForTest(), FText::FromString(TEXT("升级")));
```

Run the widget test and expect it to fail against the current solid button/atlas implementation.

- [ ] **Step 2: Replace the node overlay with a 90×90 slot plus separate labels**

Use exact presentation constants:

```cpp
constexpr FVector2D TalentSlotSize(90.0f, 90.0f);
constexpr FVector2D TalentIconSize(74.0f, 74.0f);
constexpr FVector2D TalentNodeFootprint(130.0f, 132.0f);
const TCHAR* TalentItemSlotPath =
	TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
const TCHAR* TalentSelectedSlotPath =
	TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentItemSlotSelected.T_TalentItemSlotSelected");
```

For each node, add a `UButton` at the top-center of the footprint. Its content is only a `UScaleBox` containing one `UImage`. Add a selected-frame `UImage` over the same 90×90 rectangle and show it only when `SelectedNodeId == View.Definition.Id`. Add the name and rank below as siblings at Y=94 and Y=114. Never add either text block to the button content.

Wrap the name block in a `130×18` `UScaleBox` using `ScaleToFitX` and `DownOnly`; set the text to 16px, single-line, centered, and no auto-wrap. This shrinks long names without entering adjacent cells. The rank uses a centered 14px text block.

Use `ScaleToFit` and `EStretchDirection::Both` for the icon; this preserves aspect ratio while fitting both smaller and larger source assets. Set icon tint to `(0.30,0.30,0.28,0.92)` only for locked nodes and white for every lit state.

- [ ] **Step 3: Route icons to reused or flat assets**

Implement one exact path resolver:

```cpp
const TCHAR* TalentIconPath(const EGameXXKTalentIcon Icon)
{
	switch (Icon)
	{
	case EGameXXKTalentIcon::Root: return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
	case EGameXXKTalentIcon::Attack: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Attack.T_TalentFlat_Attack");
	case EGameXXKTalentIcon::Health: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Health.T_TalentFlat_Health");
	case EGameXXKTalentIcon::Defense: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Defense.T_TalentFlat_Defense");
	case EGameXXKTalentIcon::Critical: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Critical.T_TalentFlat_Critical");
	case EGameXXKTalentIcon::Movement: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Movement.T_TalentFlat_Movement");
	case EGameXXKTalentIcon::Backpack: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Backpack.T_TalentFlat_Backpack");
	case EGameXXKTalentIcon::Warehouse: return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse");
	case EGameXXKTalentIcon::Gold: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Gold.T_TalentFlat_Gold");
	case EGameXXKTalentIcon::Experience: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Experience.T_TalentFlat_Experience");
	case EGameXXKTalentIcon::Offline: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Offline.T_TalentFlat_Offline");
	case EGameXXKTalentIcon::Time: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Time.T_TalentFlat_Time");
	case EGameXXKTalentIcon::Chest:
	case EGameXXKTalentIcon::ChestTime: return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Chest.T_TalentFlat_Chest");
	case EGameXXKTalentIcon::Tools:
	case EGameXXKTalentIcon::ToolReward: return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools");
	default: return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
	}
}
```

Remove the talent atlas member, `GetAtlasResourcePathForTest`, its old atlas assertion, and atlas UV code after all test accessors use direct texture resources.

- [ ] **Step 4: Restyle and rename the details action**

Bind the button style to the existing project tabs and always set its label to `升级`. Add this local helper before building the details panel:

```cpp
const TCHAR* UpgradeButtonPath = TEXT("/Game/GameXXK/UI/Inventory/Textures/003_tab_1.003_tab_1");
FButtonStyle MakeTalentUpgradeButtonStyle()
{
	FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	if (UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, UpgradeButtonPath))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.DrawAs = ESlateBrushDrawType::Box;
		Brush.ImageSize = FVector2D(190.0f, 54.0f);
		Brush.Margin = FMargin(0.08f);
		Style.Normal = Brush;
		Style.Hovered = Brush;
		Style.Hovered.TintColor = FSlateColor(FLinearColor(1.08f, 1.08f, 1.08f, 1.0f));
		Style.Pressed = Brush;
		Style.Pressed.TintColor = FSlateColor(FLinearColor(0.82f, 0.82f, 0.82f, 1.0f));
		Style.Disabled = Brush;
		Style.Disabled.TintColor = FSlateColor(FLinearColor(0.48f, 0.48f, 0.48f, 0.78f));
	}
	return Style;
}
PurchaseButton->SetStyle(MakeTalentUpgradeButtonStyle());
CastChecked<UTextBlock>(PurchaseButton->GetContent())->SetText(FText::FromString(TEXT("升级")));
PurchaseButton->SetIsEnabled(!bMaxed &&
	(View->State == EGameXXKTalentNodeState::Available || View->State == EGameXXKTalentNodeState::Purchased));
```

Keep `已满级` in the detail body text rather than changing the button label.

- [ ] **Step 5: Run widget tests and commit**

Run `GameXXK.Talents.Widget`; expected all tests succeed.

```powershell
git add Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp
git commit -m "feat: render talents with project item slots"
```

### Task 6: Snap line rendering and preserve live graph state

**Files:**
- Modify: `Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h`
- Modify: `Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp`

- [ ] **Step 1: Add rendered-angle and scroll-preservation tests**

Expose `TArray<float> GetRenderedConnectionAnglesForTest() const` and current horizontal/vertical scroll offsets. Assert every normalized angle is one of `0`, `45`, `90`, `135`, and that purchasing a node does not change either offset.

- [ ] **Step 2: Replace `Atan2` freedom with explicit snapped classification**

Use:

```cpp
bool ResolveConnectionAngle(const FVector2D Delta, float& OutAngle)
{
	const float X = FMath::Abs(Delta.X);
	const float Y = FMath::Abs(Delta.Y);
	if (FMath::IsNearlyZero(Y) && X > 0.0f) { OutAngle = Delta.X >= 0.0f ? 0.0f : 180.0f; return true; }
	if (FMath::IsNearlyZero(X) && Y > 0.0f) { OutAngle = Delta.Y >= 0.0f ? 90.0f : -90.0f; return true; }
	if (X > 0.0f && FMath::IsNearlyEqual(X, Y, 0.1f))
	{
		OutAngle = Delta.X * Delta.Y >= 0.0f ? 45.0f : -45.0f;
		if (Delta.X < 0.0f) OutAngle += 180.0f;
		return true;
	}
	return false;
}
```

If resolution fails, do not draw the line and emit one `ensureMsgf` naming both node IDs. Store the approved normalized angle for tests.

- [ ] **Step 3: Preserve graph offsets during in-place refresh**

Capture `HorizontalScroll->GetScrollOffset()` and `VerticalScroll->GetScrollOffset()` before `GraphCanvas->ClearChildren()`, rebuild children in place, then restore both offsets. Do not release the parent workbench Slate tree.

- [ ] **Step 4: Run UI and cross-panel tests**

Run:

```powershell
Automation RunTests GameXXK.Talents.Widget
Automation RunTests GameXXK.DesktopTraining.Workbench.LiveNumericRefreshWithoutRebuild
```

Expected: success and no graph replacement, scroll reset, or unsupported-angle assertion.

- [ ] **Step 5: Commit snapped rendering**

```powershell
git add Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp
git commit -m "fix: snap talent graph lines and preserve scroll"
```

### Task 7: Full verification and PIE visual acceptance

**Files:**
- Modify only if a regression is found: files already listed in Tasks 1–6
- Save evidence under: `Saved/HarnessReports/TalentGridFlatIcons-*`

- [ ] **Step 1: Run a cold UBT build**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --filter "[TDD]" --log-lines 200
```

Expected: `Result: Succeeded`; no Live Coding or Hot Reload.

- [ ] **Step 2: Run focused suites**

Run and require empty failure lists:

```text
GameXXK.Talents
GameXXK.DesktopTraining.Workbench
GameXXK.MVP.UI.FinalInventory
GameXXK.Equipment.Tools
```

- [ ] **Step 3: Perform canonical PIE acceptance**

Open `/Game/GameXXK/Maps/L_DesktopTrainingHUD`, start PIE, expand Backpack, open Talents, and verify:

1. Root uses the current Celtic talent-nav icon inside a 1:1 Item Slot.
2. Name and rank are below the image.
3. Root upgrade reveals four mirrored entries.
4. A branch main at `1/5` reveals exactly two side roots.
5. Both side roots at `1/5` reveal exactly one next main.
6. Lines are visually horizontal, vertical, or 45° and no full node footprints overlap.
7. Upgrade refreshes in place without resetting graph scroll.

- [ ] **Step 4: Save packages and commit final corrections**

Use UE MCP `save_dirty_packages`, confirm `dirty_after=[]`, then commit only scoped talent/art files:

```powershell
git add Source/GameXXK/Private/GameXXKTalentCatalog.cpp Source/GameXXK/Private/GameXXKTalentRules.cpp Source/GameXXK/Public/UI/GameXXKTalentTreeWidget.h Source/GameXXK/Private/UI/GameXXKTalentTreeWidget.cpp Source/GameXXK/Private/Tests/GameXXKTalentCatalogTest.cpp Source/GameXXK/Private/Tests/GameXXKTalentRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKTalentGridLayoutTest.cpp Source/GameXXK/Private/Tests/GameXXKTalentTreeWidgetTest.cpp SourceArt/UI/Talents/flat Content/Python/gamexxk_import_flat_talent_icons.py Content/GameXXK/UI/Talents/Flat
git commit -m "feat: finish permanent talent grid redesign"
```
