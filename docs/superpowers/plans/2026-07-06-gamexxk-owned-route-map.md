# GameXXK-Owned Route Map Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Make `L_RouteMap` a GameXXK-owned route-map screen that uses 1Game visual assets while keeping route seed, route state, node clicks, battle handoff, and battle return under GameXXK C++/UMG control.

**Architecture:** Keep `UGameXXKMVPSubsystem` and `UGameXXKMVPRules` as the only gameplay state owners. Rebuild `UGameXXKOneGameRouteMapWidget` as a scrollable GameXXK widget with 1Game background/icon/line assets. Keep `L_Battle_1Game` as the battle map target, but ensure route-node selection comes from GameXXK state rather than original 1Game PlayerController state.

**Tech Stack:** Unreal Engine 5.8 C++, UMG, Automation tests, UE MCP project Python scripts, `scripts/ue_tdd_pipeline.py`.

---

## File Structure

- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
  - Add a public seed-driven route-map generation API for tests and subsystem flow.
- Modify `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`
  - Implement deterministic route-map generation from `RouteSeed`.
  - Preserve pending combat node and battle victory semantics.
- Create `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`
  - Cover seed determinism and combat/non-combat route state transitions.
- Modify `D:/UE5 demo/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
  - Add background, scroll, content-size, and scroll-offset diagnostics.
- Modify `D:/UE5 demo/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`
  - Add 1Game background texture layer.
  - Replace compressed node placement with dynamic scrollable content.
- Modify `D:/UE5 demo/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`
  - Cover background creation, larger content sizing, scroll target, reachable hit boxes, and battle return.
- Modify `D:/UE5 demo/GameXXK/Public/MVP/GameXXKOneGameIslandRouteMapBridge.h`
  - Add a small testable predicate for direct GameXXK battle-screen entry.
- Modify `D:/UE5 demo/GameXXK/Private/MVP/GameXXKOneGameIslandRouteMapBridge.cpp`
  - Prefer the live GameInstance `UGameXXKMVPSubsystem` when entering `L_Battle_1Game`.
  - Show battle layout directly when GameXXK runtime is already `Battle`.
- Modify `D:/UE5 demo/GameXXK/Private/Tests/GameXXKOneGameIslandRouteMapBridgeTest.cpp`
  - Cover direct GameXXK battle runtime entry without original 1Game route-node advancement.
- Create `D:/UE5 demo/GameXXK/Content/Python/gamexxk_validate_owned_route_map.py`
  - Focused UE-side validation that `L_RouteMap` is not configured to use original `/Game/1Game` controller ownership.
- Create `D:/UE5 demo/GameXXK/scripts/gamexxk_validate_owned_route_map_mcp.py`
  - Tiny MCP runner for the validation script.

## Task 1: Add RED Seed And Route-State Rules Tests

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`

- [ ] **Step 1: Write the failing seed/rules test**

Create `Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp` with this content:

```cpp
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static FString BuildRouteSignature(const FGameXXKRuntimeState& State)
	{
		FString Signature;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			Signature += FString::Printf(
				TEXT("N%d:%d:%d:%d:%.2f:%.2f|"),
				Node.NodeId,
				Node.LayerIndex,
				Node.ColumnIndex,
				static_cast<int32>(Node.NodeKind),
				Node.NormalizedPosition.X,
				Node.NormalizedPosition.Y);
			for (int32 OutgoingNodeId : Node.OutgoingNodeIds)
			{
				Signature += FString::Printf(TEXT(">%d"), OutgoingNodeId);
			}
			Signature += TEXT(";");
		}
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			Signature += FString::Printf(TEXT("E%d>%d;"), Edge.FromNodeId, Edge.ToNodeId);
		}
		return Signature;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMapSeedRulesTest,
	"GameXXK.MVP.RouteMap.SeedRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMapSeedRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState Seed101A = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed101A, 101);
	TestTrue(TEXT("explicit seed generates a route map"), Seed101A.bHasGeneratedRouteMap);
	TestEqual(TEXT("explicit seed persists on runtime state"), Seed101A.RouteSeed, 101);
	TestTrue(TEXT("generated route has at least ten nodes"), Seed101A.RouteMapNodes.Num() >= 10);
	TestTrue(TEXT("generated route has branching edges"), Seed101A.RouteMapEdges.Num() > Seed101A.RouteMapNodes.Num());
	TestEqual(TEXT("generated route starts with one reachable node"), Seed101A.ReachableRouteNodeIds.Num(), 1);
	TestTrue(TEXT("generated route starts with reachable start node"), Seed101A.ReachableRouteNodeIds.Contains(0));

	FGameXXKRuntimeState Seed101B = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed101B, 101);
	TestEqual(TEXT("same seed produces same route signature"), BuildRouteSignature(Seed101B), BuildRouteSignature(Seed101A));

	FGameXXKRuntimeState Seed102 = UGameXXKMVPRules::CreateNewGame();
	UGameXXKMVPRules::GenerateRouteMapForSeed(Seed102, 102);
	TestNotEqual(TEXT("different seed changes visible route signature"), BuildRouteSignature(Seed102), BuildRouteSignature(Seed101A));

	UGameInstance* EntryGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EntrySubsystem = NewObject<UGameXXKMVPSubsystem>(EntryGameInstance);
	TestTrue(TEXT("entry starts game"), EntrySubsystem->StartGame());
	TestTrue(TEXT("entry selects Qingshan"), EntrySubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("entry accepts quest"), EntrySubsystem->AcceptQuest());
	TestTrue(TEXT("entry opens dungeon route map"), EntrySubsystem->OpenDungeonFromTownExit());
	TestNotEqual(TEXT("dungeon entry assigns a nonzero route seed"), EntrySubsystem->GetRuntimeState().RouteSeed, 0);

	FGameXXKRuntimeState& FlowState = Seed101A;
	TestTrue(TEXT("start node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, 0));
	TestEqual(TEXT("non-combat start keeps route map active"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("start node is visited"), FlowState.VisitedRouteNodeIds.Contains(0));
	TestTrue(TEXT("battle branch is reachable"), FlowState.ReachableRouteNodeIds.Contains(1));

	TestTrue(TEXT("combat node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(FlowState, 1));
	TestEqual(TEXT("combat node opens battle screen"), FlowState.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("combat node becomes pending, not visited"), FlowState.PendingRouteNodeId, 1);
	TestFalse(TEXT("pending combat node waits for victory before visited"), FlowState.VisitedRouteNodeIds.Contains(1));

	TestTrue(TEXT("battle victory resolves pending route node"), UGameXXKMVPRules::ResolveBattleVictory(FlowState, false));
	TestEqual(TEXT("battle victory returns to route map"), FlowState.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("pending node is cleared"), FlowState.PendingRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("battle node is visited after victory"), FlowState.VisitedRouteNodeIds.Contains(1));

	return true;
}

#endif
```

- [ ] **Step 2: Run the RED test**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
```

Expected: UBT compile fails because `UGameXXKMVPRules::GenerateRouteMapForSeed` is not defined.

- [ ] **Step 3: Commit the RED test**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp
git commit -m "test: cover route map seed rules"
```

## Task 2: Implement Seed-Driven Route Generation

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/GameXXKMVPRules.cpp`

- [ ] **Step 1: Add the public generation API**

In `GameXXKMVPRules.h`, add this declaration after `CanEnterDungeon`:

```cpp
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	static void GenerateRouteMapForSeed(UPARAM(ref) FGameXXKRuntimeState& State, int32 Seed);
```

- [ ] **Step 2: Add seed helpers**

In the anonymous `GameXXKMVP` namespace in `GameXXKMVPRules.cpp`, above `AddRouteNode`, add:

```cpp
	static int32 NormalizeRouteSeed(int32 Seed)
	{
		if (Seed == 0)
		{
			return 1;
		}
		return FMath::Abs(Seed);
	}

	static int32 MakeNewRouteSeed()
	{
		const int32 Seed = FMath::Rand();
		return NormalizeRouteSeed(Seed);
	}
```

- [ ] **Step 3: Replace the private fixed generator**

Replace the body of the current private `GenerateRouteMap(FGameXXKRuntimeState& State)` with this wrapper:

```cpp
	static void GenerateRouteMap(FGameXXKRuntimeState& State)
	{
		const int32 Seed = State.RouteSeed != 0 ? State.RouteSeed : MakeNewRouteSeed();
		UGameXXKMVPRules::GenerateRouteMapForSeed(State, Seed);
	}
```

Then add this public implementation below `CanEnterDungeon`:

```cpp
void UGameXXKMVPRules::GenerateRouteMapForSeed(FGameXXKRuntimeState& State, int32 Seed)
{
	const int32 NormalizedSeed = GameXXKMVP::NormalizeRouteSeed(Seed);
	State.bHasGeneratedRouteMap = true;
	State.RouteSeed = NormalizedSeed;
	State.CurrentRouteNodeId = 0;
	State.PendingRouteNodeId = INDEX_NONE;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.VisitedRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Add(0);

	const bool bOddSeed = (NormalizedSeed % 2) != 0;
	const bool bThirdSeed = (NormalizedSeed % 3) == 0;
	const EGameXXKNodeKind MiddleLeftKind = bOddSeed ? EGameXXKNodeKind::Chest : EGameXXKNodeKind::Event;
	const EGameXXKNodeKind MiddleRightKind = bThirdSeed ? EGameXXKNodeKind::Merchant : EGameXXKNodeKind::Camp;
	const EGameXXKNodeKind LateRightKind = bOddSeed ? EGameXXKNodeKind::Event : EGameXXKNodeKind::Chest;

	GameXXKMVP::AddRouteNode(State, 0, 0, 0, EGameXXKNodeKind::Start, 0.50f, 0.00f, {1, 2});
	GameXXKMVP::AddRouteNode(State, 1, 1, 0, EGameXXKNodeKind::Battle, 0.34f, 0.18f, {3, 4});
	GameXXKMVP::AddRouteNode(State, 2, 1, 1, EGameXXKNodeKind::Merchant, 0.66f, 0.18f, {4, 5});
	GameXXKMVP::AddRouteNode(State, 3, 2, 0, MiddleLeftKind, 0.25f, 0.36f, {6});
	GameXXKMVP::AddRouteNode(State, 4, 2, 1, EGameXXKNodeKind::Elite, 0.50f, 0.36f, {7, 8});
	GameXXKMVP::AddRouteNode(State, 5, 2, 2, MiddleRightKind, 0.75f, 0.36f, {8});
	GameXXKMVP::AddRouteNode(State, 6, 3, 0, EGameXXKNodeKind::Camp, 0.30f, 0.56f, {11});
	GameXXKMVP::AddRouteNode(State, 7, 3, 1, EGameXXKNodeKind::Merchant, 0.52f, 0.56f, {9});
	GameXXKMVP::AddRouteNode(State, 8, 3, 2, LateRightKind, 0.74f, 0.56f, {10});
	GameXXKMVP::AddRouteNode(State, 9, 4, 0, EGameXXKNodeKind::Battle, 0.42f, 0.76f, {11});
	GameXXKMVP::AddRouteNode(State, 10, 4, 1, EGameXXKNodeKind::Camp, 0.62f, 0.76f, {11});
	GameXXKMVP::AddRouteNode(State, 11, 5, 0, EGameXXKNodeKind::Boss, 0.50f, 1.00f, {});
}
```

- [ ] **Step 4: Run GREEN for seed rules**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600
```

Expected: UBT compile succeeds, and Automation output includes `GameXXK.MVP.RouteMap.SeedRules` as a successful test.

- [ ] **Step 5: Run adjacent route/save tests**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600
```

Expected: Automation output includes successful `GameXXK.MVP.RouteMap.OneGameAdapter`, `GameXXK.MVP.SaveGame.SlotRoundTrip`, and `GameXXK.MVP.Battle.BoardWidget` tests. If `GameXXK.MVP.RouteMap.OneGameAdapter` fails because an old assertion assumes a now-seeded node kind that is no longer stable, update that assertion in this task to check reachability or room type behavior instead of a hard-coded generated node kind, then rerun until this command passes.

- [ ] **Step 6: Commit seed generation**

```powershell
git add Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp
git commit -m "mvp: generate route map from seed"
```

## Task 3: Add RED Widget Background And Scroll Layout Tests

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp`

- [ ] **Step 1: Add failing widget assertions**

In `GameXXKOneGameRouteMapAdapterTest.cpp`, after the existing `RouteWidget->RefreshFromState();` call that follows `RouteWidget->NativeConstruct();`, add:

```cpp
	TestTrue(TEXT("adapter creates a route background visual"), RouteWidget->HasRouteBackgroundVisualForTest());
	TestTrue(TEXT("adapter uses the 1Game map background texture"), RouteWidget->GetRouteBackgroundTexturePathForTest().Contains(TEXT("图层_1")));
	const FVector2D RouteContentSize = RouteWidget->GetRouteContentSizeForTest();
	TestTrue(TEXT("route content is wider than the old compressed canvas"), RouteContentSize.X >= 560.0f);
	TestTrue(TEXT("route content height supports vertical scrolling"), RouteContentSize.Y >= 900.0f);
	TestTrue(TEXT("route map applies an initial scroll offset toward the first reachable node"), RouteWidget->GetLastAppliedScrollOffsetForTest() > 0.0f);
```

Then after `const auto SparseVisualStates = SparseRouteWidget->GetRouteNodeVisualStatesForTest();`, add:

```cpp
	TestTrue(TEXT("sparse route content still has scrollable height"), SparseRouteWidget->GetRouteContentSizeForTest().Y >= 520.0f);
	TestTrue(TEXT("enabled sparse node has viewport-adjusted hit position"), SparseVisualStates[0].ViewportHitBoxPosition.Y <= SparseVisualStates[0].HitBoxPosition.Y + 0.1f);
```

- [ ] **Step 2: Add battle victory return assertions**

After the existing assertion that a battle node targets `/Game/GameXXK/Maps/L_Battle_1Game`, add:

```cpp
	TestTrue(TEXT("battle victory resolves through GameXXK rules"), Subsystem->ResolveBattleVictory(false));
	TestEqual(TEXT("battle victory returns to route-map screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(
		TEXT("battle victory targets GameXXK-owned route map"),
		GameXXKLevelFlow::MapForRuntimeState(Subsystem->GetRuntimeState()),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
```

- [ ] **Step 3: Run the RED widget test**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
```

Expected: UBT compile fails because the new widget diagnostic methods do not exist.

- [ ] **Step 4: Commit the RED widget test**

```powershell
git add Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp
git commit -m "test: require owned route map visuals"
```

## Task 4: Implement Scrollable 1Game-Style Route Widget

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp`

- [ ] **Step 1: Add widget class declarations and test accessors**

In `GameXXKOneGameRouteMapWidget.h`, add forward declarations:

```cpp
class UOverlay;
class UScrollBox;
class USizeBox;
```

Add public test accessors after `GetCreatedNodeVisualIconPath`:

```cpp
	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	bool HasRouteBackgroundVisualForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FString GetRouteBackgroundTexturePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	FVector2D GetRouteContentSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteMap")
	float GetLastAppliedScrollOffsetForTest() const;
```

Add private helpers after `GetNodeCanvasPosition`:

```cpp
	FVector2D CalculateRouteContentSize(const TArray<FGameXXKOneGameRouteNode>& Nodes) const;
	void ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes);
```

Add private properties before `RootCanvas`:

```cpp
	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|RouteMap|Texture")
	TSoftObjectPtr<UTexture2D> OneGameRouteBackgroundTexture;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> RootOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> RouteBackgroundImage;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> RouteScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RouteContentSizeBox;
```

Add private state after `RouteMapViewportSize`:

```cpp
	UPROPERTY(Transient)
	FVector2D RouteContentSize = FVector2D(640.0f, 1040.0f);

	UPROPERTY(Transient)
	float LastAppliedScrollOffset = 0.0f;
```

- [ ] **Step 2: Add includes and the background texture path**

In `GameXXKOneGameRouteMapWidget.cpp`, add includes:

```cpp
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
```

In the existing texture-path constants, add:

```cpp
	static const TCHAR* OneGameRouteBackgroundTexturePath = TEXT("/Game/1Game/Texture/图层_1.图层_1");
	const FVector2D MinimumRouteContentSize(640.0f, 1040.0f);
	const float RouteHorizontalPadding = 96.0f;
	const float RouteTopPadding = 96.0f;
	const float RouteBottomPadding = 128.0f;
	const float RouteLayerGap = 180.0f;
	const float RouteInitialScrollTopPadding = 120.0f;
```

In the constructor, add:

```cpp
	OneGameRouteBackgroundTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameRouteBackgroundTexturePath));
```

- [ ] **Step 3: Implement the test accessors**

Add these methods near the existing test accessors:

```cpp
bool UGameXXKOneGameRouteMapWidget::HasRouteBackgroundVisualForTest() const
{
	return RouteBackgroundImage != nullptr && !OneGameRouteBackgroundTexture.IsNull();
}

FString UGameXXKOneGameRouteMapWidget::GetRouteBackgroundTexturePathForTest() const
{
	return OneGameRouteBackgroundTexture.ToSoftObjectPath().ToString();
}

FVector2D UGameXXKOneGameRouteMapWidget::GetRouteContentSizeForTest() const
{
	return RouteContentSize;
}

float UGameXXKOneGameRouteMapWidget::GetLastAppliedScrollOffsetForTest() const
{
	return LastAppliedScrollOffset;
}
```

- [ ] **Step 4: Replace the compressed root layout**

Rewrite the start of `BuildProgrammaticLayout()` so it creates a root overlay, background image, scroll box, size box, and route content canvas:

```cpp
void UGameXXKOneGameRouteMapWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("OneGameRouteMapWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKOneGameRouteNode> AdapterNodes = BuildAdapterNodes();
	RouteContentSize = CalculateRouteContentSize(AdapterNodes);
	const int32 RouteNodeCount = FMath::Clamp(AdapterNodes.Num(), 0, MaxRouteNodeButtons);
	const int32 RouteLineCount = Subsystem && Subsystem->GetRuntimeState().bHasGeneratedRouteMap
		? Subsystem->GetRuntimeState().RouteMapEdges.Num()
		: FMath::Max(0, RouteNodeCount - 1);

	if (!RootOverlay || WidgetTree->RootWidget != RootOverlay)
	{
		RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameXXKOneGameRouteMapRoot"));
		WidgetTree->RootWidget = RootOverlay;

		RouteBackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GameXXKOneGameRouteMapBackground"));
		if (UTexture2D* BackgroundTexture = OneGameRouteBackgroundTexture.LoadSynchronous())
		{
			RouteBackgroundImage->SetBrushFromTexture(BackgroundTexture, true);
		}
		RouteBackgroundImage->SetColorAndOpacity(FLinearColor::White);
		if (UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(RouteBackgroundImage))
		{
			BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		}

		RouteScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GameXXKOneGameRouteMapScroll"));
		if (UOverlaySlot* ScrollSlot = RootOverlay->AddChildToOverlay(RouteScrollBox))
		{
			ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrollSlot->SetVerticalAlignment(VAlign_Fill);
		}

		RouteContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GameXXKOneGameRouteMapContentSize"));
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKOneGameRouteMapContent"));
		RouteContentSizeBox->AddChild(RootCanvas);
		RouteScrollBox->AddChild(RouteContentSizeBox);
	}
	else if (RootCanvas)
	{
		RootCanvas->ClearChildren();
	}

	if (!RootCanvas || !RouteContentSizeBox)
	{
		return;
	}

	RouteContentSizeBox->SetWidthOverride(RouteContentSize.X);
	RouteContentSizeBox->SetHeightOverride(RouteContentSize.Y);

	LineVisualWidgets.Reset();
	LineVisualBorders.Reset();
	for (int32 LineIndex = 0; LineIndex < RouteLineCount; ++LineIndex)
	{
		if (UWidget* LineVisual = ConstructLineVisualWidget(LineIndex))
		{
			if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(LineVisual))
			{
				LineSlot->SetSize(FVector2D(128.0f, 18.0f));
			}
			LineVisualWidgets.Add(LineVisual);
		}
	}

	NodeVisualWidgets.Reset();
	NodeVisualBorders.Reset();
	NodeVisualLabels.Reset();
	NodeVisualImages.Reset();
	NodeVisualIconPaths.Reset();
	for (int32 NodeIndex = 0; NodeIndex < RouteNodeCount; ++NodeIndex)
	{
		if (UWidget* NodeVisual = ConstructNodeVisualWidget(NodeIndex))
		{
			if (UCanvasPanelSlot* NodeSlot = RootCanvas->AddChildToCanvas(NodeVisual))
			{
				NodeSlot->SetSize(FVector2D(96.0f, 96.0f));
			}
			NodeVisualWidgets.Add(NodeVisual);
		}
	}

	NodeButtons.Reset();
	NodeButtonLabels.Reset();
	NodeButtonIndices.Reset();
	for (int32 ButtonIndex = 0; ButtonIndex < RouteNodeCount; ++ButtonIndex)
	{
		UButton* NodeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("RouteNodeButton%d"), ButtonIndex));
		UTextBlock* NodeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("RouteNodeLabel%d"), ButtonIndex));
		NodeButton->AddChild(NodeLabel);
		BindNodeButton(NodeButton, ButtonIndex);
		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(NodeButton))
		{
			CanvasSlot->SetSize(FVector2D(110.0f, 72.0f));
		}

		NodeButtons.Add(NodeButton);
		NodeButtonLabels.Add(NodeLabel);
		NodeButtonIndices.Add(INDEX_NONE);
	}
}
```

- [ ] **Step 5: Add dynamic content sizing and node placement**

Add these methods:

```cpp
FVector2D UGameXXKOneGameRouteMapWidget::CalculateRouteContentSize(const TArray<FGameXXKOneGameRouteNode>& Nodes) const
{
	int32 MaxLayerIndex = 0;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (State && State->bHasGeneratedRouteMap)
	{
		for (const FGameXXKRouteMapNode& RouteNode : State->RouteMapNodes)
		{
			MaxLayerIndex = FMath::Max(MaxLayerIndex, RouteNode.LayerIndex);
		}
	}
	else
	{
		MaxLayerIndex = FMath::Max(0, Nodes.Num() - 1);
	}

	const float DynamicHeight = RouteTopPadding + RouteBottomPadding + static_cast<float>(MaxLayerIndex + 1) * RouteLayerGap;
	return FVector2D(
		FMath::Max(MinimumRouteContentSize.X, RouteMapViewportSize.X),
		FMath::Max(MinimumRouteContentSize.Y, DynamicHeight));
}

FVector2D UGameXXKOneGameRouteMapWidget::GetNodeCanvasPosition(const FGameXXKOneGameRouteNode& Node) const
{
	const float UsableWidth = FMath::Max(1.0f, RouteContentSize.X - RouteHorizontalPadding * 2.0f);
	const float UsableHeight = FMath::Max(1.0f, RouteContentSize.Y - RouteTopPadding - RouteBottomPadding);
	return FVector2D(
		RouteHorizontalPadding + Node.NormalizedPosition.X * UsableWidth,
		RouteTopPadding + (1.0f - Node.NormalizedPosition.Y) * UsableHeight);
}
```

- [ ] **Step 6: Apply initial scroll in refresh**

At the end of `RefreshFromState()`, after node and line configuration, call:

```cpp
	ApplyInitialScrollOffset(Nodes);
```

Add:

```cpp
void UGameXXKOneGameRouteMapWidget::ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes)
{
	const FGameXXKOneGameRouteNode* TargetNode = Nodes.FindByPredicate([](const FGameXXKOneGameRouteNode& Node)
	{
		return Node.bEnabled;
	});
	if (!TargetNode)
	{
		TargetNode = Nodes.FindByPredicate([](const FGameXXKOneGameRouteNode& Node)
		{
			return Node.bVisited;
		});
	}
	if (!TargetNode && !Nodes.IsEmpty())
	{
		TargetNode = &Nodes[0];
	}

	if (!TargetNode)
	{
		LastAppliedScrollOffset = 0.0f;
		return;
	}

	const FVector2D NodePosition = GetNodeCanvasPosition(*TargetNode);
	LastAppliedScrollOffset = FMath::Max(0.0f, NodePosition.Y - RouteInitialScrollTopPadding);
	if (RouteScrollBox)
	{
		RouteScrollBox->SetScrollOffset(LastAppliedScrollOffset);
	}
}
```

- [ ] **Step 7: Account for scroll in viewport hit-box diagnostics**

In `GetRouteNodeVisualStatesForTest()`, replace the viewport position assignment with:

```cpp
		VisualState.ViewportHitBoxPosition = RouteMapViewportPosition + VisualState.HitBoxPosition - FVector2D(0.0f, LastAppliedScrollOffset);
		VisualState.ViewportHitBoxCenter = VisualState.ViewportHitBoxPosition + VisualState.HitBoxSize * 0.5f;
```

- [ ] **Step 8: Run GREEN for route widget**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600
```

Expected: UBT compile succeeds, and Automation output includes `GameXXK.MVP.RouteMap.OneGameAdapter` as a successful test.

- [ ] **Step 9: Commit route widget ownership visuals**

```powershell
git add Source/GameXXK/Public/UI/GameXXKOneGameRouteMapWidget.h Source/GameXXK/Private/UI/GameXXKOneGameRouteMapWidget.cpp Source/GameXXK/Private/Tests/GameXXKOneGameRouteMapAdapterTest.cpp
git commit -m "mvp: render owned route map with 1game visuals"
```

## Task 5: Make Battle Map Entry Direct From GameXXK Runtime

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Public/MVP/GameXXKOneGameIslandRouteMapBridge.h`
- Modify: `D:/UE5 demo/GameXXK/Private/MVP/GameXXKOneGameIslandRouteMapBridge.cpp`
- Modify: `D:/UE5 demo/GameXXK/Private/Tests/GameXXKOneGameIslandRouteMapBridgeTest.cpp`

- [ ] **Step 1: Write the failing bridge predicate test**

In `GameXXKOneGameIslandRouteMapBridgeTest.cpp`, after the existing `ShouldOpenBattleLayoutForOriginalLevelAdvance` assertions, add:

```cpp
	TestTrue(
		TEXT("GameXXK battle runtime opens battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::Battle));
	TestFalse(
		TEXT("GameXXK route-map runtime does not open battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::DungeonMap));
	TestFalse(
		TEXT("GameXXK town runtime does not open battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::Town));
```

- [ ] **Step 2: Run the RED bridge test**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
```

Expected: UBT compile fails because `ShouldOpenBattleLayoutForGameXXKRuntimeScreen` does not exist.

- [ ] **Step 3: Add the bridge predicate**

In `GameXXKOneGameIslandRouteMapBridge.h`, add the include:

```cpp
#include "GameXXKMVPRules.h"
```

Then add public static declaration:

```cpp
	static bool ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen Screen);
```

In `GameXXKOneGameIslandRouteMapBridge.cpp`, add:

```cpp
bool AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen Screen)
{
	return Screen == EGameXXKScreen::Battle;
}
```

- [ ] **Step 4: Prefer live GameXXK subsystem on battle-map load**

In `SynchronizeOriginalBattleLayout`, before reading original `current_level`, add:

```cpp
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	UGameXXKMVPSubsystem* ExistingSubsystem = GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
	if (ExistingSubsystem && ShouldOpenBattleLayoutForGameXXKRuntimeScreen(ExistingSubsystem->GetRuntimeState().Screen))
	{
		BattleSubsystem = ExistingSubsystem;
		OpenBattleLayoutFromOriginalRoute(RouteMapWidget);
		return;
	}
```

In `ResolveBattleSubsystem`, before creating a new object, add:

```cpp
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	if (UGameXXKMVPSubsystem* ExistingSubsystem = GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr)
	{
		BattleSubsystem = ExistingSubsystem;
		return BattleSubsystem;
	}
```

Keep the existing `PrimeBattleSubsystemForIslandRoute` fallback for isolated tests and old manual map opening.

- [ ] **Step 5: Run GREEN for bridge**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0 --log-lines 40
python scripts\gamexxk_mvp_playtest.py --skip-build --test-timeout 600
```

Expected: UBT compile succeeds, and Automation output includes `GameXXK.MVP.OneGameIslandRouteMapBridge` as a successful test.

- [ ] **Step 6: Commit direct battle entry**

```powershell
git add Source/GameXXK/Public/MVP/GameXXKOneGameIslandRouteMapBridge.h Source/GameXXK/Private/MVP/GameXXKOneGameIslandRouteMapBridge.cpp Source/GameXXK/Private/Tests/GameXXKOneGameIslandRouteMapBridgeTest.cpp
git commit -m "mvp: enter battle layout from gamexxk route state"
```

## Task 6: Add UE MCP Asset Ownership Validation

**Files:**
- Create: `D:/UE5 demo/GameXXK/Content/Python/gamexxk_validate_owned_route_map.py`
- Create: `D:/UE5 demo/GameXXK/scripts/gamexxk_validate_owned_route_map_mcp.py`

- [ ] **Step 1: Add UE-side validation script**

Create `Content/Python/gamexxk_validate_owned_route_map.py`:

```python
import json
import unreal


ROUTE_MAP = "/Game/GameXXK/Maps/L_RouteMap"


def _path(obj):
    return obj.get_path_name() if obj else ""


def main():
    unreal.EditorLoadingAndSavingUtils.load_map(ROUTE_MAP)
    world = unreal.EditorLevelLibrary.get_editor_world()
    if not world:
        raise RuntimeError("No editor world after loading L_RouteMap")

    settings = world.get_world_settings()
    game_mode = settings.get_editor_property("default_game_mode")
    game_mode_path = _path(game_mode)
    if "/Game/1Game/" in game_mode_path:
        raise RuntimeError(f"L_RouteMap uses original 1Game GameMode: {game_mode_path}")
    if "GameXXK" not in game_mode_path:
        raise RuntimeError(f"L_RouteMap does not use a GameXXK GameMode: {game_mode_path}")

    result = {
        "ok": True,
        "route_map": ROUTE_MAP,
        "game_mode": game_mode_path,
    }
    unreal.log("[GameXXK][OwnedRouteMap] " + json.dumps(result, ensure_ascii=False))
    return result


if __name__ == "__main__":
    print(json.dumps(main(), ensure_ascii=False))
```

- [ ] **Step 2: Add MCP runner**

Create `scripts/gamexxk_validate_owned_route_map_mcp.py`:

```python
#!/usr/bin/env python3
from __future__ import annotations

import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from ue_mcp_client import UnrealMCPClient


def main() -> int:
    client = UnrealMCPClient()
    if not client.connect():
        print(json.dumps({"ok": False, "error": f"Cannot connect to {client.endpoint}"}, ensure_ascii=False))
        return 2
    result = client.run_project_python_file("Content/Python/gamexxk_validate_owned_route_map.py")
    ok = bool(result.get("ok", False))
    print(json.dumps(result, ensure_ascii=False, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
```

- [ ] **Step 3: Run MCP validation**

With the editor open and MCP available, run:

```powershell
python scripts\gamexxk_validate_owned_route_map_mcp.py
```

Expected: JSON output includes `"ok": true` and a GameXXK GameMode path. If MCP is unavailable, run the full C++/Automation verification in Task 7 and record that MCP validation was blocked by editor connectivity.

- [ ] **Step 4: Commit validation scripts**

```powershell
git add Content/Python/gamexxk_validate_owned_route_map.py scripts/gamexxk_validate_owned_route_map_mcp.py
git commit -m "test: validate owned route map level"
```

## Task 7: Full Verification

**Files:**
- No source changes unless verification exposes a defect.

- [ ] **Step 1: Run focused Automation**

Run:

```powershell
python scripts\gamexxk_mvp_playtest.py --test-timeout 600
```

Expected: build succeeds and Automation output includes successful `GameXXK.MVP.RouteMap.SeedRules`, `GameXXK.MVP.RouteMap.OneGameAdapter`, `GameXXK.MVP.OneGameIslandRouteMapBridge`, `GameXXK.MVP.Battle.BoardWidget`, and `GameXXK.MVP.SaveGame.SlotRoundTrip` tests.

- [ ] **Step 2: Run short PIE smoke**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 0.1 --log-lines 80
```

Expected: UBT succeeds, PIE starts briefly without fatal errors, and logs do not show route-map widget compile errors.

- [ ] **Step 3: Run real-flow MCP harness if editor is stable**

Run:

```powershell
python scripts\gamexxk_real_play_flow_mcp.py --low-load
```

Expected: flow reaches `L_RouteMap`, route widget is visible, route start advances in place, battle node opens `L_Battle_1Game`, and battle victory returns to `L_RouteMap`. If the script does not support `--low-load`, run it without that flag and capture only the final summary.

- [ ] **Step 4: Check diff hygiene**

Run:

```powershell
git diff --check
git status --short --branch
```

Expected: `git diff --check` has no output. `git status` shows only intentional files if any verification artifacts were generated.

- [ ] **Step 5: Commit any verification-only fixes**

Only if Task 7 exposed and fixed defects:

```powershell
git add Source/GameXXK Content/Python scripts
git commit -m "fix: stabilize owned route map flow"
```

If no fixes were needed, do not create an empty commit.

## Self-Review Checklist

- Spec coverage:
  - GameXXK owns `L_RouteMap`: covered by Tasks 4 and 6.
  - 1Game visuals only: covered by Task 4.
  - Seed and route persistence: covered by Tasks 1 and 2, with existing save tests rerun.
  - Node clicks and battle return: covered by Tasks 3, 5, and 7.
  - No original 1Game PlayerController route progression in acceptance path: covered by Tasks 5 and 6.
- No worktree usage: all commands assume `D:/UE5 demo/GameXXK` root on `main`.
- No UnrealBridge usage: verification uses UBT, Automation, project scripts, and UE MCP.
- TDD order: each behavior-changing task starts with a failing test or validation before implementation.
