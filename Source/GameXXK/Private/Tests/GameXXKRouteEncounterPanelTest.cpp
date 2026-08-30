#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteMerchantRules.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "UI/GameXXKRouteMerchantWidget.h"
#include "UObject/Package.h"
#include "UObject/GarbageCollection.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState BuildPendingRouteEncounterState(EGameXXKNodeKind NodeKind, EGameXXKScreen Screen)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, NodeKind, FVector2D(0.5f, 0.5f), TArray<int32>{2}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		State.CurrentRouteNodeId = 1;
		State.PendingRouteNodeId = 1;
		State.CardRun.RouteProgress.CurrentChapter = 1;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun);
		State.Screen = Screen;
		return State;
	}

	FGameXXKRuntimeState BuildClickableRouteEncounterState(EGameXXKNodeKind NodeKind)
	{
		FGameXXKRuntimeState State = BuildPendingRouteEncounterState(NodeKind, EGameXXKScreen::DungeonMap);
		State.RouteSeed = 137;
		State.CurrentRouteNodeId = 0;
		State.PendingRouteNodeId = INDEX_NONE;
		State.VisitedRouteNodeIds = TArray<int32>{0};
		State.ReachableRouteNodeIds = TArray<int32>{1};
		State.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
		State.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
		return State;
	}

	FGameXXKRuntimeState BuildClickableRouteMerchantState()
	{
		FGameXXKRuntimeState State = BuildClickableRouteEncounterState(EGameXXKNodeKind::Merchant);
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {State.RouteSeed};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = 1;
		State.CardRun.bLoadoutLockedForRoute = true;
		State.CardRun.bRouteEconomyInitialized = true;
		State.CardRun.RouteTravelMoney = 500;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				State.CardRun.HeroUnlockedCardIds.Add(Definition.Id);
				if (State.CardRun.HeroUnlockedCardIds.Num() == 8)
				{
					break;
				}
			}
		}
		State.CardRun.HeroSelectedCardIds = State.CardRun.HeroUnlockedCardIds;
		return State;
	}

	int32 TotalRouteAttributeBonus(const FGameXXKRouteAttributeBonuses& Bonuses)
	{
		return Bonuses.MaxHealth + Bonuses.MaxMana + Bonuses.Attack + Bonuses.Defense + Bonuses.Speed;
	}

	void ConfigureMountainSpringEncounter(FGameXXKRuntimeState& State)
	{
		State.CardRun.PendingEvent.SourceNodeId = 1;
		State.CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
		State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.MountainSpring");
	}

	void ConfigureChestRelicEncounter(FGameXXKRuntimeState& State)
	{
		State.CardRun.PendingEvent.SourceNodeId = 1;
		State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Chest.Bamboo");
		State.CardRun.PendingRelicOffer.SourceNodeId = 1;
		State.CardRun.PendingRelicOffer.ChoiceSeed = 137;
		State.CardRun.PendingRelicOffer.RelicIds = {
			TEXT("Relic.BambooTally"),
			TEXT("Relic.TigerSeal"),
			TEXT("Relic.RedCord")};
	}

	FGameXXKRuntimeState BuildSecondPendingChestState()
	{
		FGameXXKRuntimeState State = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
		State.CurrentRouteNodeId = 2;
		State.PendingRouteNodeId = 2;
		State.ReachableRouteNodeIds = TArray<int32>{2};
		if (FGameXXKRouteMapNode* Node = State.RouteMapNodes.FindByPredicate([](const FGameXXKRouteMapNode& Candidate)
		{
			return Candidate.NodeId == 2;
		}))
		{
			Node->NodeKind = EGameXXKNodeKind::Chest;
		}
		State.CardRun.PendingEvent.SourceNodeId = 2;
		State.CardRun.PendingEvent.ChoiceSeed = 902;
		State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Chest.Bronze");
		State.CardRun.PendingRelicOffer.SourceNodeId = 2;
		State.CardRun.PendingRelicOffer.ChoiceSeed = 902;
		State.CardRun.PendingRelicOffer.RelicIds = {
			TEXT("Relic.TigerSeal"),
			TEXT("Relic.RedCord"),
			TEXT("Relic.BambooTally")};
		return State;
	}

	FString MakeDesktopContextFixturePackageName()
	{
		static int32 FixtureSerial = 976;
		for (;;)
		{
			const FString Candidate = FString::Printf(
				TEXT("/Game/GameXXK/Maps/UEDPIE_%d_L_DesktopTrainingHUD"),
				++FixtureSerial);
			if (!FindPackage(nullptr, *Candidate))
			{
				return Candidate;
			}
		}
	}

	FString MakeForeignContextFixturePackageName()
	{
		static int32 FixtureSerial = 1200;
		for (;;)
		{
			const FString Candidate = FString::Printf(TEXT("/Game/GameXXK/Maps/UEDPIE_%d_L_Main"), ++FixtureSerial);
			if (!FindPackage(nullptr, *Candidate))
			{
				return Candidate;
			}
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterStaleSourceLessContextTest,
	"GameXXK.MVP.RouteEncounter.Panel.StaleSourceLessContextCannotMutate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterStaleSourceLessContextTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState Node1State = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Node1State);
	Subsystem->GetMutableRuntimeState() = Node1State;
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("stale-context fixture creates the real panel"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("stale-context fixture owns the panel"), Panel);
	if (!Panel)
	{
		return false;
	}
	TestTrue(TEXT("node1 source-less panel opens"), Controller->OpenRouteEncounterPanel());
	TestTrue(TEXT("node1 third choice selects"), Panel->SelectChoiceForTest(2));
	TestTrue(TEXT("node1 panel owns the move lock"), Controller->IsMoveInputIgnored());

	const FGameXXKRuntimeState Node2State = BuildSecondPendingChestState();
	Subsystem->GetMutableRuntimeState() = Node2State;
	TestFalse(TEXT("stale node1 confirm cannot resolve node2"), Panel->ConfirmSelectedChoiceForTest());
	TestEqual(TEXT("stale confirm preserves node2 screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("stale confirm preserves node2 pending identity"), Subsystem->GetRuntimeState().PendingRouteNodeId, 2);
	TestTrue(TEXT("stale confirm preserves node2 relic offer"),
		Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds == Node2State.CardRun.PendingRelicOffer.RelicIds);
	TestEqual(TEXT("stale confirm grants no relic"), Subsystem->GetRuntimeState().CardRun.Relics.Num(), 0);
	TestTrue(TEXT("stale confirm preserves visited graph"), Subsystem->GetRuntimeState().VisitedRouteNodeIds == Node2State.VisitedRouteNodeIds);
	TestFalse(TEXT("invalid stale panel is force-closed locally"), Controller->IsRouteEncounterPanelOpenForTest());
	TestFalse(TEXT("invalid stale panel releases move lock"), Controller->IsMoveInputIgnored());

	TestTrue(TEXT("new node2 panel opens after stale confirm"), Controller->OpenRouteEncounterPanel());
	TestEqual(TEXT("new node2 panel starts without stale selection"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	TestTrue(TEXT("new node2 first choice selects"), Panel->SelectChoiceForTest(0));
	TestTrue(TEXT("new node2 confirm resolves normally"), Panel->ConfirmSelectedChoiceForTest());
	TestTrue(TEXT("new node2 resolution visits node2"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(2));

	Subsystem->GetMutableRuntimeState() = Node1State;
	Controller->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("node1 panel reopens for stale X scenario"), Controller->IsRouteEncounterPanelOpenForTest());
	UButton* CloseButton = nullptr;
	if (Panel->WidgetTree)
	{
		CloseButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterCloseAction")));
	}
	TestNotNull(TEXT("stale X scenario finds the real CloseInk button"), CloseButton);
	Subsystem->GetMutableRuntimeState() = Node2State;
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("stale X preserves node2 event screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("stale X preserves node2 pending identity"), Subsystem->GetRuntimeState().PendingRouteNodeId, 2);
	TestTrue(TEXT("stale X preserves node2 relic offer"),
		Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds == Node2State.CardRun.PendingRelicOffer.RelicIds);
	TestFalse(TEXT("stale X force-closes only the old panel"), Controller->IsRouteEncounterPanelOpenForTest());
	TestFalse(TEXT("stale X releases move lock"), Controller->IsMoveInputIgnored());
	TestTrue(TEXT("node2 can safely open after stale X"), Controller->OpenRouteEncounterPanel());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterEscapeReturnTest,
	"GameXXK.MVP.RouteEncounter.Panel.EscapeReturnsSourceLessChoiceToMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterEscapeReturnTest::RunTest(const FString& Parameters)
{
	const struct
	{
		EGameXXKNodeKind NodeKind;
		EGameXXKScreen Screen;
		const TCHAR* Label;
	} Cases[] = {
		{EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent, TEXT("event")},
		{EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp, TEXT("camp")},
	};
	for (const auto& Case : Cases)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(Case.NodeKind, Case.Screen);
		if (Case.Screen == EGameXXKScreen::RouteEvent)
		{
			ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
		}
		AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
		Controller->SetMVPSubsystemForTest(Subsystem);
		TestTrue(FString::Printf(TEXT("%s Escape fixture creates widgets"), Case.Label), Controller->EnsurePlayerFlowWidgetsForTest());
		UGameXXKOneGameRouteMapWidget* RouteMap = Controller->GetRouteMapWidgetForTest();
		TestNotNull(FString::Printf(TEXT("%s Escape fixture owns route map"), Case.Label), RouteMap);
		TestTrue(FString::Printf(TEXT("%s panel opens before Escape"), Case.Label), Controller->OpenRouteEncounterPanel());
		TestTrue(FString::Printf(TEXT("%s panel owns move lock"), Case.Label), Controller->IsMoveInputIgnored());
		TestTrue(FString::Printf(TEXT("%s Escape is consumed"), Case.Label),
			Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
		TestEqual(FString::Printf(TEXT("%s Escape returns to route map"), Case.Label), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
		TestEqual(FString::Printf(TEXT("%s Escape preserves pending node"), Case.Label), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
		TestFalse(FString::Printf(TEXT("%s Escape closes panel"), Case.Label), Controller->IsRouteEncounterPanelOpenForTest());
		TestFalse(FString::Printf(TEXT("%s Escape releases move lock"), Case.Label), Controller->IsMoveInputIgnored());
		if (!RouteMap)
		{
			continue;
		}
		RouteMap->RefreshFromState();
		const TArray<FGameXXKOneGameRouteNodeVisualState> Visuals = RouteMap->GetRouteNodeVisualStatesForTest();
		const FGameXXKOneGameRouteNodeVisualState* PendingVisual = Visuals.FindByPredicate([](const FGameXXKOneGameRouteNodeVisualState& Visual)
		{
			return Visual.NodeId == 1;
		});
		TestTrue(FString::Printf(TEXT("%s pending route node is clickable after Escape"), Case.Label), PendingVisual && PendingVisual->bEnabled);
		TestTrue(FString::Printf(TEXT("%s pending node resumes after Escape"), Case.Label), RouteMap->ExecuteRouteNodeById(1));
		TestEqual(FString::Printf(TEXT("%s resume restores encounter screen"), Case.Label), Subsystem->GetRuntimeState().Screen, Case.Screen);
		TestTrue(FString::Printf(TEXT("%s resume reopens panel"), Case.Label), Controller->IsRouteEncounterPanelOpenForTest());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterForeignMapAutoOpenGateTest,
	"GameXXK.MVP.RouteEncounter.Panel.ForeignMapNeverAutoOpensSourceLess",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterForeignMapAutoOpenGateTest::RunTest(const FString& Parameters)
{
	const FString PackageName = MakeForeignContextFixturePackageName();
	UPackage* WorldPackage = CreatePackage(*PackageName);
	UWorld* RuntimeWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("L_Main_ForeignRouteContext"), WorldPackage);
	TestNotNull(TEXT("foreign gate creates game world"), RuntimeWorld);
	if (!RuntimeWorld)
	{
		return false;
	}
	RuntimeWorld->AddToRoot();
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(RuntimeWorld);
	RuntimeWorld->InitializeActorsForPlay(FURL());
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKMVPPlayerController* Controller = RuntimeWorld->SpawnActor<AGameXXKMVPPlayerController>(
		AGameXXKMVPPlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
	ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	TestNotNull(TEXT("foreign gate spawns controller"), Controller);
	if (!Controller)
	{
		RuntimeWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(RuntimeWorld);
		RuntimeWorld->RemoveFromRoot();
		return false;
	}
	Controller->SetPlayer(LocalPlayer);
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("foreign gate creates route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	if (UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest())
	{
		Panel->TakeWidget();
		Panel->NativeConstruct();
	}

	const struct
	{
		EGameXXKNodeKind NodeKind;
		EGameXXKScreen Screen;
		const TCHAR* Label;
	} Cases[] = {
		{EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent, TEXT("event")},
		{EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp, TEXT("camp")},
	};
	for (const auto& Case : Cases)
	{
		Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(Case.NodeKind, Case.Screen);
		if (Case.Screen == EGameXXKScreen::RouteEvent)
		{
			ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
		}
		for (int32 RefreshIndex = 0; RefreshIndex < 2; ++RefreshIndex)
		{
			Controller->RefreshPlayerFlowWidgetsForTest();
			TestFalse(FString::Printf(TEXT("foreign %s refresh %d keeps panel closed"), Case.Label, RefreshIndex),
				Controller->IsRouteEncounterPanelOpenForTest());
			TestFalse(FString::Printf(TEXT("foreign %s refresh %d keeps move unlocked"), Case.Label, RefreshIndex),
				Controller->IsMoveInputIgnored());
			TestFalse(FString::Printf(TEXT("foreign %s refresh %d creates no active identity"), Case.Label, RefreshIndex),
				Controller->HasActiveRouteEncounterContextForTest());
		}
	}

	Controller->EndPlay(EEndPlayReason::Destroyed);
	RuntimeWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(RuntimeWorld);
	RuntimeWorld->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPresentationIdentityTest,
	"GameXXK.MVP.RouteEncounter.Panel.SelectionFollowsPresentationIdentity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterPresentationIdentityTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("presentation identity fixture creates route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("presentation identity fixture owns panel"), Panel);
	if (!Panel)
	{
		return false;
	}
	TestTrue(TEXT("presentation identity fixture opens panel"), Controller->OpenRouteEncounterPanel());

	UButton* ConfirmButton = nullptr;
	UWidget* ThirdSelectionInk = nullptr;
	if (Panel->WidgetTree)
	{
		ConfirmButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterConfirmAction")));
		ThirdSelectionInk = Panel->WidgetTree->FindWidget(TEXT("RouteEncounterChoiceSelectionInk2"));
	}
	TestNotNull(TEXT("identity fixture finds confirm"), ConfirmButton);
	TestNotNull(TEXT("identity fixture finds third ink"), ThirdSelectionInk);
	TestTrue(TEXT("third choice selects for the first offer"), Panel->SelectChoiceForTest(2));
	Panel->RefreshFromState();
	TestEqual(TEXT("ordinary refresh preserves selection for the same offer"), Panel->GetSelectedChoiceIndexForTest(), 2);
	TestTrue(TEXT("same-offer refresh keeps confirm enabled"), ConfirmButton && ConfirmButton->GetIsEnabled());
	TestEqual(TEXT("same-offer refresh keeps selected ink visible"),
		ThirdSelectionInk ? ThirdSelectionInk->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);

	FGameXXKRuntimeState& ReplacedOfferState = Subsystem->GetMutableRuntimeState();
	ReplacedOfferState.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Chest.Bronze");
	ReplacedOfferState.CardRun.PendingEvent.ChoiceSeed = 991;
	ReplacedOfferState.CardRun.PendingRelicOffer.ChoiceSeed = 991;
	ReplacedOfferState.CardRun.PendingRelicOffer.RelicIds = {
		TEXT("Relic.RedCord"),
		TEXT("Relic.BambooTally"),
		TEXT("Relic.TigerSeal")};
	Panel->RefreshFromState();
	TestEqual(TEXT("replacement offer clears stale selection"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	TestFalse(TEXT("replacement offer disables confirm"), ConfirmButton && ConfirmButton->GetIsEnabled());
	TestEqual(TEXT("replacement offer hides stale selection ink"),
		ThirdSelectionInk ? ThirdSelectionInk->GetVisibility() : ESlateVisibility::HitTestInvisible,
		ESlateVisibility::Collapsed);

	TestTrue(TEXT("replacement offer accepts a fresh selection"), Panel->SelectChoiceForTest(1));
	Subsystem->GetMutableRuntimeState().CardRun.PendingRelicOffer.ChoiceSeed = 992;
	Panel->RefreshFromState();
	TestEqual(TEXT("choice seed change alone clears selection"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterSelectionInkOcclusionTest,
	"GameXXK.MVP.RouteEncounter.Panel.SelectionInkDoesNotOccludeCard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterSelectionInkOcclusionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("selection ink fixture creates route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("selection ink fixture owns panel"), Panel);
	if (!Panel || !Panel->WidgetTree)
	{
		return false;
	}

	TArray<UWidget*> SelectionInks;
	SelectionInks.SetNumZeroed(3);
	UWidget* ThirdArt = nullptr;
	UWidget* ThirdName = nullptr;
	UWidget* ThirdDescription = nullptr;
	UButton* ConfirmButton = nullptr;
	Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		for (int32 ChoiceIndex = 0; ChoiceIndex < 3; ++ChoiceIndex)
		{
			if (Widget->GetFName() == *FString::Printf(TEXT("RouteEncounterChoiceSelectionInk%d"), ChoiceIndex))
			{
				SelectionInks[ChoiceIndex] = Widget;
			}
		}
		if (Widget->GetFName() == TEXT("RouteEncounterChoiceArt2")) ThirdArt = Widget;
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceName2")) ThirdName = Widget;
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceDescription2")) ThirdDescription = Widget;
		else if (Widget->GetFName() == TEXT("RouteEncounterConfirmAction")) ConfirmButton = Cast<UButton>(Widget);
	});
	for (int32 ChoiceIndex = 0; ChoiceIndex < SelectionInks.Num(); ++ChoiceIndex)
	{
		TestNotNull(FString::Printf(TEXT("selection ink %d exists"), ChoiceIndex), SelectionInks[ChoiceIndex]);
		TestEqual(FString::Printf(TEXT("selection ink %d starts collapsed"), ChoiceIndex),
			SelectionInks[ChoiceIndex] ? SelectionInks[ChoiceIndex]->GetVisibility() : ESlateVisibility::Visible,
			ESlateVisibility::Collapsed);
	}
	TestTrue(TEXT("third card selects"), Panel->SelectChoiceForTest(2));
	const UImage* ThirdSelectionImage = Cast<UImage>(SelectionInks[2]);
	const UObject* ThirdSelectionResource = ThirdSelectionImage
		? ThirdSelectionImage->GetBrush().GetResourceObject()
		: nullptr;
	TestTrue(TEXT("route choice uses the approved square selected-state base"),
		ThirdSelectionResource
		&& ThirdSelectionResource->GetPathName().Contains(TEXT("T_MasterV2_SquareSelected")));
	TestEqual(TEXT("third selection ink becomes hit-test invisible"),
		SelectionInks[2] ? SelectionInks[2]->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	const UCanvasPanelSlot* InkSlot = SelectionInks[2] ? Cast<UCanvasPanelSlot>(SelectionInks[2]->Slot) : nullptr;
	TestNotNull(TEXT("third selection ink owns a real canvas slot"), InkSlot);
	if (InkSlot)
	{
		TestTrue(TEXT("selection ink width is a small corner stamp"), InkSlot->GetSize().X <= 64.0f);
		TestTrue(TEXT("selection ink height is a small corner stamp"), InkSlot->GetSize().Y <= 64.0f);
		TestTrue(TEXT("selection ink sits in the card right-side safe area"), InkSlot->GetPosition().X >= 150.0f);
		TestTrue(TEXT("selection ink stays near the card top edge"), InkSlot->GetPosition().Y <= 24.0f);
	}
	TestTrue(TEXT("third art remains visible"), ThirdArt && ThirdArt->GetVisibility() != ESlateVisibility::Collapsed && ThirdArt->GetVisibility() != ESlateVisibility::Hidden);
	TestTrue(TEXT("third name remains visible"), ThirdName && ThirdName->GetVisibility() != ESlateVisibility::Collapsed && ThirdName->GetVisibility() != ESlateVisibility::Hidden);
	TestTrue(TEXT("third description remains visible"), ThirdDescription && ThirdDescription->GetVisibility() != ESlateVisibility::Collapsed && ThirdDescription->GetVisibility() != ESlateVisibility::Hidden);
	TestTrue(TEXT("confirm enables for selected card"), ConfirmButton && ConfirmButton->GetIsEnabled());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterOverlayNodeGateTest,
	"GameXXK.MVP.RouteEncounter.Panel.RouteOverlayNodesRemainVisibleDisabled",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterOverlayNodeGateTest::RunTest(const FString& Parameters)
{
	const struct
	{
		EGameXXKNodeKind NodeKind;
		EGameXXKScreen Screen;
		const TCHAR* Label;
	} Cases[] = {
		{EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent, TEXT("event")},
		{EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp, TEXT("camp")},
		{EGameXXKNodeKind::Merchant, EGameXXKScreen::RouteMerchant, TEXT("merchant")},
	};

	for (const auto& Case : Cases)
	{
		UGameInstance* TestGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
		Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(Case.NodeKind, Case.Screen);
		UGameXXKOneGameRouteMapWidget* RouteMap = NewObject<UGameXXKOneGameRouteMapWidget>();
		RouteMap->SetMVPSubsystem(Subsystem);
		TestTrue(FString::Printf(TEXT("%s route map initializes"), Case.Label), RouteMap->Initialize());
		RouteMap->NativeConstruct();
		RouteMap->RefreshFromState();

		const TArray<FGameXXKOneGameRouteNodeVisualState> Visuals = RouteMap->GetRouteNodeVisualStatesForTest();
		TestEqual(FString::Printf(TEXT("%s overlay keeps route owner visible"), Case.Label), RouteMap->GetVisibility(), ESlateVisibility::Visible);
		TestEqual(FString::Printf(TEXT("%s overlay preserves every route node"), Case.Label), Visuals.Num(), Subsystem->GetRuntimeState().RouteMapNodes.Num());
		TestEqual(FString::Printf(TEXT("%s overlay preserves every route line"), Case.Label), RouteMap->GetCreatedLineVisualWidgetCount(), Subsystem->GetRuntimeState().RouteMapEdges.Num());
		for (const FGameXXKOneGameRouteNodeVisualState& Visual : Visuals)
		{
			TestFalse(FString::Printf(TEXT("%s overlay disables node %d"), Case.Label, Visual.NodeId), Visual.bEnabled);
		}

		const EGameXXKScreen ScreenBeforeClick = Subsystem->GetRuntimeState().Screen;
		const int32 PendingBeforeClick = Subsystem->GetRuntimeState().PendingRouteNodeId;
		const TArray<int32> VisitedBeforeClick = Subsystem->GetRuntimeState().VisitedRouteNodeIds;
		TestFalse(FString::Printf(TEXT("%s overlay rejects route-node click"), Case.Label), RouteMap->ExecuteRouteNodeById(1));
		TestEqual(FString::Printf(TEXT("%s rejected click preserves screen"), Case.Label), Subsystem->GetRuntimeState().Screen, ScreenBeforeClick);
		TestEqual(FString::Printf(TEXT("%s rejected click preserves pending"), Case.Label), Subsystem->GetRuntimeState().PendingRouteNodeId, PendingBeforeClick);
		TestTrue(FString::Printf(TEXT("%s rejected click preserves visited graph"), Case.Label),
			Subsystem->GetRuntimeState().VisitedRouteNodeIds == VisitedBeforeClick);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterFocusTargetsTest,
	"GameXXK.MVP.RouteEncounter.Panel.EventAndReturnFocusTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterFocusTargetsTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("focus fixture creates controller-owned route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	UGameXXKOneGameRouteMapWidget* RouteMap = Controller->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("focus fixture owns encounter panel"), Panel);
	TestNotNull(TEXT("focus fixture owns route map"), RouteMap);
	if (!Panel || !RouteMap)
	{
		return false;
	}
	TestTrue(TEXT("event panel opens"), Controller->OpenRouteEncounterPanel());
	TestTrue(TEXT("controller-owned event panel is focusable"), Panel->IsFocusable());
	TestTrue(TEXT("controller-owned route map is focusable under event"), RouteMap->IsFocusable());

	UButton* CloseButton = Panel->WidgetTree
		? Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterCloseAction")))
		: nullptr;
	TestNotNull(TEXT("focus fixture finds real event X"), CloseButton);
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("event X returns to route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("event X closes encounter focus owner"), Controller->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("route map remains a valid focus target after X"), RouteMap->IsFocusable());
	TestEqual(TEXT("route map is visible after X"), RouteMap->GetVisibility(), ESlateVisibility::Visible);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterDesktopTrainingContextTest,
	"GameXXK.MVP.RouteEncounter.Panel.DesktopTrainingContextDelegates",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterDesktopTrainingContextTest::RunTest(const FString& Parameters)
{
	const FString FixturePackageName = MakeDesktopContextFixturePackageName();
	UPackage* const WorldPackage = CreatePackage(*FixturePackageName);
	UWorld* const RuntimeWorld = UWorld::CreateWorld(EWorldType::Game, false, TEXT("L_DesktopTrainingHUD"), WorldPackage);
	TestNotNull(TEXT("desktop context fixture creates a real game world"), RuntimeWorld);
	if (!RuntimeWorld)
	{
		return false;
	}
	RuntimeWorld->AddToRoot();
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	WorldContext.SetCurrentWorld(RuntimeWorld);
	RuntimeWorld->InitializeActorsForPlay(FURL());

	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.MountainSpring");

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKMVPPlayerController* const Controller = RuntimeWorld->SpawnActor<AGameXXKMVPPlayerController>(
		AGameXXKMVPPlayerController::StaticClass(),
		FVector::ZeroVector,
		FRotator::ZeroRotator,
		SpawnParameters);
	TestNotNull(TEXT("desktop context fixture spawns the production controller"), Controller);
	if (!Controller)
	{
		RuntimeWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(RuntimeWorld);
		RuntimeWorld->RemoveFromRoot();
		return false;
	}
	ULocalPlayer* const LocalPlayer = NewObject<ULocalPlayer>(GEngine);
	Controller->SetPlayer(LocalPlayer);
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("real controller can host viewport widgets"), Controller->CanAddPlayerWidgetsToViewportForTest());
	TestTrue(TEXT("real controller creates route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* const Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("real controller owns the encounter panel"), Panel);
	if (Panel)
	{
		Panel->TakeWidget();
		Panel->NativeConstruct();
	}
	TestTrue(TEXT("real constructed fixture explicitly opens source-less event"), Controller->OpenSourceLessRouteEncounterPanelForTest());
	TestTrue(TEXT("real constructed fixture applies final event visibility after native construct"), Panel && Panel->OpenEncounterPanel());
	TestTrue(TEXT("real constructed fixture reports event panel open"), Controller->IsRouteEncounterPanelOpenForTest());

	const FString CurrentPackageName = RuntimeWorld->GetOutermost()->GetName();
	TestTrue(TEXT("fixture package is PIE-compatible DesktopTrainingHUD"),
		GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(CurrentPackageName));
	TestTrue(TEXT("production predicate accepts the canonical 2D package"),
		Controller->IsSourceLessRouteEncounterPackageValidForTest(CurrentPackageName));
	TestTrue(TEXT("production predicate preserves the legacy route-event map"),
		Controller->IsSourceLessRouteEncounterPackageValidForTest(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestFalse(TEXT("production predicate still rejects a foreign map"),
		Controller->IsSourceLessRouteEncounterPackageValidForTest(TEXT("/Game/GameXXK/Maps/L_Main")));

	UButton* ThirdEventCard = nullptr;
	UButton* ConfirmButton = nullptr;
	UButton* CampPrimaryButton = nullptr;
	if (Panel && Panel->WidgetTree)
	{
		Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			if (Widget->GetFName() == TEXT("RouteEncounterChoiceCard2"))
			{
				ThirdEventCard = Cast<UButton>(Widget);
			}
			else if (Widget->GetFName() == TEXT("RouteEncounterConfirmAction"))
			{
				ConfirmButton = Cast<UButton>(Widget);
			}
			else if (Widget->GetFName() == TEXT("RouteEncounterPrimaryAction"))
			{
				CampPrimaryButton = Cast<UButton>(Widget);
			}
		});
	}
	TestNotNull(TEXT("real event third card delegate exists"), ThirdEventCard);
	TestNotNull(TEXT("real event confirm delegate exists"), ConfirmButton);
	TestNotNull(TEXT("real camp primary delegate exists"), CampPrimaryButton);
	TestTrue(TEXT("real event confirm delegate is bound"), ConfirmButton && ConfirmButton->OnClicked.IsBound());
	const FGameXXKRouteAttributeBonuses BonusesBefore = Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses;
	if (ThirdEventCard)
	{
		ThirdEventCard->OnClicked.Broadcast();
	}
	TestEqual(TEXT("real event third card owns selection before confirm"), Panel ? Panel->GetSelectedChoiceIndexForTest() : INDEX_NONE, 2);
	TestTrue(TEXT("real event confirm enables after third-card click"), ConfirmButton && ConfirmButton->GetIsEnabled());
	TestTrue(TEXT("real event panel remains open after third-card click"), Controller->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("real event context remains valid after third-card click"), Controller->HasValidRouteEncounterContextForTest());
	TestTrue(TEXT("real event resolves through controller production dispatch"),
		Controller->ResolveRouteEncounterAction(EGameXXKRouteEncounterAction::SelectChoice2));
	TestEqual(TEXT("canonical event confirm resolves back to route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("real event third card grants exactly five max health"),
		Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxHealth, BonusesBefore.MaxHealth + 5);
	TestEqual(TEXT("real event third card does not trigger max mana choice"),
		Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxMana, BonusesBefore.MaxMana);
	TestEqual(TEXT("real event third card does not trigger defense choice"),
		Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.Defense, BonusesBefore.Defense);

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	Subsystem->GetMutableRuntimeState().PlayerHP = 33;
	Controller->RefreshPlayerFlowWidgetsForTest();
	if (!Controller->IsRouteEncounterPanelOpenForTest())
	{
		TestTrue(TEXT("real constructed fixture explicitly opens source-less camp"), Controller->OpenSourceLessRouteEncounterPanelForTest());
		TestTrue(TEXT("real constructed fixture applies final camp visibility after native construct"), Panel && Panel->OpenEncounterPanel());
	}
	TestTrue(TEXT("canonical camp opens the real encounter panel"), Controller->IsRouteEncounterPanelOpenForTest());
	if (CampPrimaryButton)
	{
		CampPrimaryButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("canonical camp action resolves back to route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("canonical camp charm action never heals directly"), Subsystem->GetRuntimeState().PlayerHP, 33);
	TestTrue(TEXT("canonical camp action acquires the life-saving talisman"),
		Subsystem->GetRuntimeState().CardRun.Relics.ContainsByPredicate([](const FGameXXKRelicInstance& Relic)
		{
			return Relic.RelicId == TEXT("Relic.LifeSavingTalisman");
		}));

	Controller->EndPlay(EEndPlayReason::Destroyed);
	RuntimeWorld->DestroyWorld(false);
	GEngine->DestroyWorldContext(RuntimeWorld);
	RuntimeWorld->RemoveFromRoot();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterSyntheticWorldRepeatTest,
	"GameXXK.MVP.RouteEncounter.Panel.SyntheticWorldFixtureRepeatsWithoutResidue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterSyntheticWorldRepeatTest::RunTest(const FString& Parameters)
{
	const FString PackageNames[] = {
		MakeDesktopContextFixturePackageName(),
		MakeDesktopContextFixturePackageName(),
	};
	TestNotEqual(TEXT("synthetic fixture allocates unique PIE package names"), PackageNames[0], PackageNames[1]);
	if (PackageNames[0] == PackageNames[1])
	{
		return false;
	}

	TArray<TWeakObjectPtr<UWorld>> WeakWorlds;
	TArray<TWeakObjectPtr<UPackage>> WeakPackages;
	TArray<TWeakObjectPtr<AGameXXKMVPPlayerController>> WeakControllers;
	TArray<TWeakObjectPtr<ULocalPlayer>> WeakLocalPlayers;
	for (int32 Iteration = 0; Iteration < 2; ++Iteration)
	{
		UPackage* WorldPackage = CreatePackage(*PackageNames[Iteration]);
		UWorld* RuntimeWorld = UWorld::CreateWorld(
			EWorldType::Game,
			false,
			*FString::Printf(TEXT("L_DesktopTrainingHUD_Context_%d"), Iteration),
			WorldPackage);
		TestNotNull(FString::Printf(TEXT("iteration %d creates world"), Iteration), RuntimeWorld);
		if (!RuntimeWorld)
		{
			continue;
		}
		WeakWorlds.Add(RuntimeWorld);
		WeakPackages.Add(WorldPackage);
		RuntimeWorld->AddToRoot();
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		WorldContext.SetCurrentWorld(RuntimeWorld);
		RuntimeWorld->InitializeActorsForPlay(FURL());

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGameXXKMVPPlayerController* Controller = RuntimeWorld->SpawnActor<AGameXXKMVPPlayerController>(
			AGameXXKMVPPlayerController::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		ULocalPlayer* LocalPlayer = NewObject<ULocalPlayer>(GEngine);
		TestNotNull(FString::Printf(TEXT("iteration %d spawns controller"), Iteration), Controller);
		TestNotNull(FString::Printf(TEXT("iteration %d creates local player"), Iteration), LocalPlayer);
		WeakControllers.Add(Controller);
		WeakLocalPlayers.Add(LocalPlayer);
		if (Controller)
		{
			Controller->SetPlayer(LocalPlayer);
			Controller->EndPlay(EEndPlayReason::Destroyed);
		}

		RuntimeWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(RuntimeWorld);
		TestNull(FString::Printf(TEXT("iteration %d removes world context"), Iteration), GEngine->GetWorldContextFromWorld(RuntimeWorld));
		RuntimeWorld->RemoveFromRoot();
		Controller = nullptr;
		LocalPlayer = nullptr;
		RuntimeWorld = nullptr;
		WorldPackage = nullptr;
		CollectGarbage(RF_NoFlags);
	}

	for (int32 Iteration = 0; Iteration < WeakWorlds.Num(); ++Iteration)
	{
		TestFalse(FString::Printf(TEXT("iteration %d leaves no world object"), Iteration), WeakWorlds[Iteration].IsValid());
		TestFalse(FString::Printf(TEXT("iteration %d leaves no package object"), Iteration), WeakPackages[Iteration].IsValid());
		TestFalse(FString::Printf(TEXT("iteration %d leaves no controller object"), Iteration), WeakControllers[Iteration].IsValid());
		TestFalse(FString::Printf(TEXT("iteration %d leaves no local-player object"), Iteration), WeakLocalPlayers[Iteration].IsValid());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterCampCloseInkPresentationTest,
	"GameXXK.MVP.RouteEncounter.Panel.CampCloseInkPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterCampCloseInkPresentationTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("camp fixture creates the real route widgets"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	UGameXXKOneGameRouteMapWidget* RouteMap = Controller->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("camp owns the real encounter panel"), Panel);
	TestNotNull(TEXT("camp keeps the route map underneath"), RouteMap);
	if (!Panel || !RouteMap)
	{
		return false;
	}
	RouteMap->TakeWidget();
	Panel->TakeWidget();
	Controller->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("camp panel is open after real construction"), Controller->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("camp keeps the route-map owner visible underneath"), RouteMap->GetVisibility(), ESlateVisibility::Visible);

	UButton* CampClose = nullptr;
	UWidget* CampFrameCanvas = nullptr;
	if (Panel->WidgetTree)
	{
		Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
		{
			if (!Widget)
			{
				return;
			}
			if (Widget->GetFName() == TEXT("RouteEncounterCloseAction"))
			{
				CampClose = Cast<UButton>(Widget);
			}
			else if (Widget->GetFName() == TEXT("RouteEncounterPaperContent"))
			{
				CampFrameCanvas = Widget;
			}
		});
	}
	TestNotNull(TEXT("camp tree contains a real close widget"), CampClose);
	TestNotNull(TEXT("camp tree contains the visible frame canvas"), CampFrameCanvas);
	TestTrue(TEXT("camp close remains attached to the frame layout"), CampClose && CampClose->GetParent() == CampFrameCanvas);
	TestTrue(TEXT("camp close owns a concrete panel slot"), CampClose && CampClose->Slot != nullptr);
	TestEqual(TEXT("camp close is visible"), CampClose ? CampClose->GetVisibility() : ESlateVisibility::Collapsed, ESlateVisibility::Visible);
	TestTrue(TEXT("camp close is enabled"), CampClose && CampClose->GetIsEnabled());
	TestTrue(TEXT("camp close has nonzero render opacity"), CampClose && CampClose->GetRenderOpacity() > 0.0f);
	TestTrue(TEXT("camp close uses approved CloseInk"), CampClose
		&& CampClose->GetStyle().Normal.GetResourceObject()
		&& CampClose->GetStyle().Normal.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_CloseInk")));

	const TArray<int32> ReachableBefore = Subsystem->GetRuntimeState().ReachableRouteNodeIds;
	const TArray<int32> VisitedBefore = Subsystem->GetRuntimeState().VisitedRouteNodeIds;
	if (CampClose)
	{
		CampClose->OnClicked.Broadcast();
	}
	TestEqual(TEXT("camp close returns to route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("camp close preserves the pending node"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	TestTrue(TEXT("camp close preserves reachable graph"), Subsystem->GetRuntimeState().ReachableRouteNodeIds == ReachableBefore);
	TestTrue(TEXT("camp close preserves visited graph"), Subsystem->GetRuntimeState().VisitedRouteNodeIds == VisitedBefore);
	TestFalse(TEXT("camp close hides the modal"), Controller->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("same unresolved camp resumes"), RouteMap->ExecuteRouteNodeById(1));
	TestEqual(TEXT("resumed camp returns to camp screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("resumed camp panel reopens"), Controller->IsRouteEncounterPanelOpenForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterThreeCardConfirmTest,
	"GameXXK.MVP.RouteEncounter.Panel.ThreeCardSelectThenConfirm",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterThreeCardConfirmTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("widgets exist"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	UGameXXKOneGameRouteMapWidget* RouteMap = Controller->GetRouteMapWidgetForTest();
	TestNotNull(TEXT("encounter panel exists"), Panel);
	TestNotNull(TEXT("route map exists"), RouteMap);
	TestTrue(TEXT("panel opens"), Controller->OpenRouteEncounterPanel());
	if (!Panel || !RouteMap)
	{
		return false;
	}

	const TArray<FName> ChoicesBefore = Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds;
	const TArray<int32> ReachableBefore = Subsystem->GetRuntimeState().ReachableRouteNodeIds;
	const TArray<int32> VisitedBefore = Subsystem->GetRuntimeState().VisitedRouteNodeIds;
	UButton* ThirdChoiceCard = nullptr;
	UButton* ConfirmButton = nullptr;
	UButton* CloseButton = nullptr;
	UWidget* FirstChoiceSelectionInk = nullptr;
	UWidget* ThirdChoiceSelectionInk = nullptr;
	UWidget* ThirdChoiceArt = nullptr;
	UWidget* ThirdChoiceName = nullptr;
	UWidget* ThirdChoiceDescription = nullptr;
	UWidget* ThirdChoiceDisabledReason = nullptr;
	Panel->WidgetTree->ForEachWidget([&](UWidget* Widget)
	{
		if (!Widget)
		{
			return;
		}
		if (Widget->GetFName() == TEXT("RouteEncounterChoiceCard2"))
		{
			ThirdChoiceCard = Cast<UButton>(Widget);
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterConfirmAction"))
		{
			ConfirmButton = Cast<UButton>(Widget);
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterCloseAction"))
		{
			CloseButton = Cast<UButton>(Widget);
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceSelectionInk0"))
		{
			FirstChoiceSelectionInk = Widget;
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceSelectionInk2"))
		{
			ThirdChoiceSelectionInk = Widget;
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceArt2"))
		{
			ThirdChoiceArt = Widget;
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceName2"))
		{
			ThirdChoiceName = Widget;
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceDescription2"))
		{
			ThirdChoiceDescription = Widget;
		}
		else if (Widget->GetFName() == TEXT("RouteEncounterChoiceDisabledReason2"))
		{
			ThirdChoiceDisabledReason = Widget;
		}
	});
	TestNotNull(TEXT("third full card is a real button"), ThirdChoiceCard);
	TestNotNull(TEXT("confirm is a real button"), ConfirmButton);
	TestNotNull(TEXT("event X is a real button"), CloseButton);
	TestNotNull(TEXT("full card exposes reward art"), ThirdChoiceArt);
	TestNotNull(TEXT("full card exposes reward name"), ThirdChoiceName);
	TestNotNull(TEXT("full card exposes description"), ThirdChoiceDescription);
	TestNotNull(TEXT("full card exposes disabled reason"), ThirdChoiceDisabledReason);
	TestTrue(TEXT("choice uses the approved card frame"), ThirdChoiceCard
		&& ThirdChoiceCard->GetStyle().Normal.GetResourceObject()
		&& ThirdChoiceCard->GetStyle().Normal.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_CardFrame")));
	TestTrue(TEXT("event X uses the approved CloseInk"), CloseButton
		&& CloseButton->GetStyle().Normal.GetResourceObject()
		&& CloseButton->GetStyle().Normal.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_CloseInk")));
	TestEqual(TEXT("three full card faces render"), Panel->GetRenderedChoiceCardCountForTest(), 3);
	TestEqual(TEXT("nothing selected initially"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	TestEqual(TEXT("third selection ink starts hidden"), ThirdChoiceSelectionInk ? ThirdChoiceSelectionInk->GetVisibility() : ESlateVisibility::Visible, ESlateVisibility::Collapsed);
	TestFalse(TEXT("real confirm button starts disabled"), ConfirmButton && ConfirmButton->GetIsEnabled());
	TestFalse(TEXT("confirm without a selected card cannot commit"), Panel->ConfirmSelectedChoiceForTest());
	if (ThirdChoiceCard)
	{
		ThirdChoiceCard->OnClicked.Broadcast();
	}
	TestEqual(TEXT("third card owns the single selection"), Panel->GetSelectedChoiceIndexForTest(), 2);
	TestEqual(TEXT("third selection ink shows after click"), ThirdChoiceSelectionInk ? ThirdChoiceSelectionInk->GetVisibility() : ESlateVisibility::Collapsed, ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("first selection ink remains hidden"), FirstChoiceSelectionInk ? FirstChoiceSelectionInk->GetVisibility() : ESlateVisibility::Visible, ESlateVisibility::Collapsed);
	TestEqual(TEXT("selection does not resolve screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("selection grants no relic"), Subsystem->GetRuntimeState().CardRun.Relics.Num(), 0);
	TestEqual(TEXT("selection does not mark the node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Num(), VisitedBefore.Num());

	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("event X shows route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("event X preserves pending node"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	TestTrue(TEXT("event X preserves pending event"), Subsystem->GetRuntimeState().CardRun.PendingEvent.SourceNodeId == 1);
	TestTrue(TEXT("event X preserves pending relic choices"), Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds == ChoicesBefore);
	TestTrue(TEXT("event X preserves reachable nodes"), Subsystem->GetRuntimeState().ReachableRouteNodeIds == ReachableBefore);
	TestTrue(TEXT("event X preserves visited nodes"), Subsystem->GetRuntimeState().VisitedRouteNodeIds == VisitedBefore);
	TestFalse(TEXT("event X hides the modal"), Controller->IsRouteEncounterPanelOpenForTest());

	TestFalse(TEXT("another node remains blocked while the choice is pending"), RouteMap->ExecuteRouteNodeById(2));
	TestTrue(TEXT("same pending node reopens without reroll"), RouteMap->ExecuteRouteNodeById(1));
	TestTrue(TEXT("panel reopens"), Controller->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("reopening preserves deterministic choices"), Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds == ChoicesBefore);
	TestEqual(TEXT("reopening starts with no transient selection"), Panel->GetSelectedChoiceIndexForTest(), INDEX_NONE);
	if (ThirdChoiceCard)
	{
		ThirdChoiceCard->OnClicked.Broadcast();
	}
	TestEqual(TEXT("real third card reselects"), Panel->GetSelectedChoiceIndexForTest(), 2);
	TestTrue(TEXT("real confirm button enables after selection"), ConfirmButton && ConfirmButton->GetIsEnabled());
	if (ConfirmButton)
	{
		ConfirmButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("confirm returns to map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("confirm grants selected relic"), Subsystem->GetRuntimeState().CardRun.Relics.ContainsByPredicate(
		[Expected = ChoicesBefore[2]](const FGameXXKRelicInstance& Relic) { return Relic.RelicId == Expected; }));
	TestTrue(TEXT("confirm marks the node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	Controller->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("camp keeps its explicit two-reward panel"), Controller->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("camp primary is the life-saving talisman"),
		Panel->GetPrimaryActionForTest(), EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman);
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("camp X returns to route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("camp X preserves pending node"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	TestFalse(TEXT("camp X does not mark the node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("same unresolved camp resumes"), RouteMap->ExecuteRouteNodeById(1));
	TestEqual(TEXT("resumed camp returns to camp screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("resumed camp panel reopens"), Controller->IsRouteEncounterPanelOpenForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterNodeClickFlowTest,
	"GameXXK.MVP.RouteEncounter.NodeClick.EventChestAndMerchantReachTheirDedicatedHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterNodeClickFlowTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates the route map and encounter HUD"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKOneGameRouteMapWidget* RouteMap = PlayerController->GetRouteMapWidgetForTest();
	UGameXXKRouteEncounterPanelWidget* Panel = PlayerController->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("route map widget is available for real node-click execution"), RouteMap);
	TestNotNull(TEXT("encounter panel is available for real node-click execution"), Panel);
	if (!RouteMap || !Panel)
	{
		return false;
	}

	Subsystem->GetMutableRuntimeState() = BuildClickableRouteEncounterState(EGameXXKNodeKind::Event);
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	const int32 EventBonusBefore = TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses);
	TestTrue(TEXT("clicking the reachable question-mark node executes through the route-map widget"), RouteMap->ExecuteRouteNodeById(1));
	TestEqual(TEXT("question-mark click enters the event screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("question-mark click records the clicked node as pending"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	TestEqual(TEXT("question-mark click builds an event offer for the same node"), Subsystem->GetRuntimeState().CardRun.PendingEvent.SourceNodeId, 1);
	TestFalse(TEXT("question-mark click produces a concrete encounter identity"), Subsystem->GetRuntimeState().CardRun.PendingEvent.EncounterId.IsNone());
	TestTrue(TEXT("question-mark click opens the visible choice HUD without a second interaction"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("question-mark HUD keeps the current route map visible underneath"), RouteMap->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("question-mark primary choice is a concrete catalog choice"), Panel->GetPrimaryActionForTest(), EGameXXKRouteEncounterAction::SelectChoice0);
	TestFalse(TEXT("question-mark primary choice has visible text"), Panel->GetPrimaryActionTextForTest().IsEmpty());
	TestTrue(TEXT("clicking the visible event card selects it without resolving"), Panel->TriggerPrimaryActionForTest());
	TestEqual(TEXT("selected event card remains on the event screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestTrue(TEXT("confirming the selected event card resolves the node"), Panel->ConfirmSelectedChoiceForTest());
	TestTrue(TEXT("event choice grants a route-local character attribute"),
		TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses) > EventBonusBefore);
	TestEqual(TEXT("resolved event returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("resolved event marks the clicked node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("resolved event unlocks its outgoing node"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(2));
	TestFalse(TEXT("resolved event closes its modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildClickableRouteEncounterState(EGameXXKNodeKind::Chest);
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("clicking the reachable treasure node executes through the route-map widget"), RouteMap->ExecuteRouteNodeById(1));
	TestEqual(TEXT("treasure click enters the reward event screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("treasure click records the clicked node as pending"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	TestTrue(TEXT("treasure click opens the visible three-choice HUD without a second interaction"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("treasure HUD keeps the current route map visible underneath"), RouteMap->GetVisibility(), ESlateVisibility::Visible);
	TestEqual(TEXT("treasure click creates exactly three relic choices"), Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds.Num(), 3);
	TSet<FName> UniqueRelicIds;
	for (const FName RelicId : Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds)
	{
		UniqueRelicIds.Add(RelicId);
	}
	TestEqual(TEXT("treasure relic choices are distinct"), UniqueRelicIds.Num(), 3);
	TestEqual(TEXT("treasure primary choice selects its first relic"), Panel->GetPrimaryActionForTest(), EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("treasure secondary choice selects its second relic"), Panel->GetSecondaryActionForTest(), EGameXXKRouteEncounterAction::SelectChoice1);
	TestFalse(TEXT("treasure third relic choice has visible text"), Panel->GetTertiaryActionTextForTest().IsEmpty());
	const FName ExpectedRelicId = Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds.IsValidIndex(2)
		? Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds[2]
		: NAME_None;
	TestTrue(TEXT("clicking the visible third relic card selects it"), Panel->TriggerTertiaryActionForTest());
	TestEqual(TEXT("selected relic remains pending until confirm"), Subsystem->GetRuntimeState().CardRun.Relics.Num(), 0);
	TestTrue(TEXT("confirming the third relic resolves the treasure node"), Panel->ConfirmSelectedChoiceForTest());
	TestEqual(TEXT("treasure choice adds exactly one run relic"), Subsystem->GetRuntimeState().CardRun.Relics.Num(), 1);
	TestEqual(TEXT("treasure choice grants the relic that was visibly selected"),
		Subsystem->GetRuntimeState().CardRun.Relics.IsValidIndex(0) ? Subsystem->GetRuntimeState().CardRun.Relics[0].RelicId : NAME_None,
		ExpectedRelicId);
	TestEqual(TEXT("resolved treasure returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("resolved treasure marks the clicked node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("resolved treasure unlocks its outgoing node"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(2));
	TestFalse(TEXT("resolved treasure closes its modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildClickableRouteMerchantState();
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("dedicated merchant HUD starts hidden on the route map"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestTrue(TEXT("clicking the reachable merchant node executes through the route-map widget"), RouteMap->ExecuteRouteNodeById(1));
	TestEqual(TEXT("merchant click enters the merchant screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteMerchant);
	TestEqual(TEXT("merchant click records the clicked node as pending"), Subsystem->GetRuntimeState().PendingRouteNodeId, 1);
	UGameXXKRouteMerchantWidget* MerchantWidget = PlayerController->GetRouteMerchantWidgetForTest();
	TestNotNull(TEXT("controller owns the dedicated merchant HUD"), MerchantWidget);
	TestTrue(TEXT("dedicated merchant HUD accepts the controller focus target"), MerchantWidget && MerchantWidget->IsFocusable());
	TestTrue(TEXT("merchant click opens the dedicated merchant HUD"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestEqual(TEXT("merchant HUD keeps the current route map visible underneath"), RouteMap->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("merchant HUD locks pawn movement while it owns focus"), PlayerController->IsRouteMerchantInputLockedForTest());
	TestFalse(TEXT("merchant click never opens the generic encounter panel"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("merchant HUD consumes legacy F without opening the generic panel"),
		PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::F, IE_Pressed, 1.0f)));
	TestFalse(TEXT("legacy F leaves the generic encounter panel closed"), PlayerController->IsRouteEncounterPanelOpenForTest());
	PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f));
	TestEqual(TEXT("Escape does not resolve or close the merchant screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteMerchant);
	TestTrue(TEXT("Escape leaves the dedicated merchant HUD open"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestEqual(TEXT("runtime merchant HUD renders four card offers"), MerchantWidget ? MerchantWidget->GetRenderedCardOfferCountForTest() : 0, 4);
	TestEqual(TEXT("runtime merchant HUD renders four relic offers"),
		MerchantWidget ? MerchantWidget->GetRenderedRelicOfferCountForTest() : 0,
		FGameXXKRouteMerchantRules::RelicSlotCount);
	TestTrue(TEXT("dedicated merchant leave action resolves the node"), MerchantWidget && MerchantWidget->LeaveMerchant());
	TestEqual(TEXT("merchant leave returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("merchant leave hides the dedicated merchant HUD"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestFalse(TEXT("merchant leave releases the pawn movement lock"), PlayerController->IsRouteMerchantInputLockedForTest());
	TestEqual(TEXT("merchant leave restores the route-map widget"), RouteMap->GetVisibility(), ESlateVisibility::Visible);
	TestTrue(TEXT("merchant leave marks the clicked node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("merchant leave unlocks its outgoing node"), Subsystem->GetRuntimeState().ReachableRouteNodeIds.Contains(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPanelAutoOpenTest,
	"GameXXK.MVP.RouteEncounter.Panel.AutoOpensPureHudScenes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterPanelAutoOpenTest::RunTest(const FString& Parameters)
{
	const struct
	{
		EGameXXKNodeKind NodeKind;
		EGameXXKScreen Screen;
		const TCHAR* Label;
	} Cases[] = {
		{EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent, TEXT("event")},
		{EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent, TEXT("chest")},
		{EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp, TEXT("camp")},
	};

	for (const auto& Case : Cases)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(Case.NodeKind, Case.Screen);
		AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
		PlayerController->SetMVPSubsystemForTest(Subsystem);
		TestTrue(FString::Printf(TEXT("%s controller creates the shared route encounter HUD"), Case.Label),
			PlayerController->EnsurePlayerFlowWidgetsForTest());
		PlayerController->RefreshPlayerFlowWidgetsForTest();
		TestTrue(
			FString::Printf(TEXT("%s scene opens the pure HUD immediately after route entry"), Case.Label),
			PlayerController->IsRouteEncounterPanelOpenForTest());
		PlayerController->RefreshPlayerFlowWidgetsForTest();
		TestTrue(
			FString::Printf(TEXT("%s pure HUD remains open across a state refresh"), Case.Label),
			PlayerController->IsRouteEncounterPanelOpenForTest());
		PlayerController->CloseRouteEncounterPanel();
	}

	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Subsystem->GetMutableRuntimeState() = BuildClickableRouteMerchantState();
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("merchant controller creates route widgets"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	TestTrue(TEXT("merchant auto-open fixture enters through the route rule"), Subsystem->SelectRouteNodeById(1));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("merchant state does not auto-open the generic encounter panel"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("merchant state auto-opens its dedicated HUD"), PlayerController->IsRouteMerchantWidgetOpenForTest());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPanelEventTest,
	"GameXXK.MVP.RouteEncounter.Panel.EventIdentityAndExplicitChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterPanelEventTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.MountainSpring");

	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates a dedicated PSD-backed route encounter panel"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = PlayerController->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("route encounter panel is owned by the player controller"), Panel);

	const int32 GoldBeforeOpen = Subsystem->GetRuntimeState().PlayerGold;
	const FName PendingNpcBeforeOpen = Subsystem->GetRuntimeState().CardRun.PendingEvent.EventNpcId;
	TestTrue(TEXT("route entry automatically opens the PSD encounter panel"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("opening the pure HUD keeps the event screen active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::RouteEvent);
	TestEqual(TEXT("opening the pure HUD grants no automatic event reward"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeOpen);
	TestEqual(TEXT("opening the pure HUD preserves the event NPC identity"), Subsystem->GetRuntimeState().CardRun.PendingEvent.EventNpcId, PendingNpcBeforeOpen);
	TestTrue(TEXT("route encounter panel uses the approved backpack paper window frame"),
		Panel && Panel->GetWindowFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame")));
	TestTrue(TEXT("route encounter panel uses the approved backpack header strip"),
		Panel && Panel->GetHeaderResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_Header")));
	TestTrue(TEXT("route encounter panel uses the approved backpack action blank"),
		Panel && Panel->GetActionResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_ActionBlank")));
	TestEqual(TEXT("environment event shows the Mountain Spring identity"), Panel ? Panel->GetSpeakerTextForTest().ToString() : FString(), FString(TEXT("无名山泉")));
	TestEqual(TEXT("event primary choice selects its first route attribute"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("event alternative selects its second route attribute"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice1);
	TestEqual(TEXT("event renders three complete selectable cards"), Panel ? Panel->GetRenderedChoiceCardCountForTest() : 0, 3);
	TestTrue(TEXT("event primary button names its concrete attribute gain"), Panel && Panel->GetPrimaryActionTextForTest().ToString().Contains(TEXT("最大气血")));

	const int32 RouteMaxHealthBefore = Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxHealth;
	TestTrue(TEXT("pressing the explicit primary card selects the event attribute"), Panel && Panel->TriggerPrimaryActionForTest());
	TestEqual(TEXT("event card selection alone grants no attribute"), Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxHealth, RouteMaxHealthBefore);
	TestTrue(TEXT("confirm grants the selected event attribute"), Panel && Panel->ConfirmSelectedChoiceForTest());
	TestTrue(TEXT("explicit event choice increases route-local maximum health"), Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxHealth > RouteMaxHealthBefore);
	TestTrue(TEXT("attribute events never install a temporary support"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("explicit environment choice completes the route node back to map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("panel closes after its explicit route choice resolves"), PlayerController->IsRouteEncounterPanelOpenForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPanelVariantTest,
	"GameXXK.MVP.RouteEncounter.Panel.EnvironmentChestCampChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterPanelVariantTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller owns the route encounter panel for event, chest and camp states"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = PlayerController->GetRouteEncounterPanelWidgetForTest();

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	ConfigureMountainSpringEncounter(Subsystem->GetMutableRuntimeState());
	TestTrue(TEXT("Mountain Spring event can be opened as a player choice"), PlayerController->OpenRouteEncounterPanel());
	TestEqual(TEXT("environment event displays its concrete identity"), Panel ? Panel->GetSpeakerTextForTest().ToString() : FString(), FString(TEXT("无名山泉")));
	TestEqual(TEXT("Mountain Spring primary is its first route-attribute choice"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("Mountain Spring alternative is its second route-attribute choice"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice1);
	PlayerController->CloseRouteEncounterPanel();

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	TestTrue(TEXT("chest opens a dedicated explicit reward choice"), PlayerController->OpenRouteEncounterPanel());
	TestEqual(TEXT("chest presentation identifies the concrete treasure source"), Panel ? Panel->GetSpeakerTextForTest().ToString() : FString(), FString(TEXT("竹编秘匣")));
	TestEqual(TEXT("chest primary selects its first relic"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("chest alternative selects its second relic"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice1);
	UButton* ChestRouteCardChoice = nullptr;
	if (Panel && Panel->WidgetTree)
	{
		Panel->WidgetTree->ForEachWidget([&ChestRouteCardChoice](UWidget* Widget)
		{
			if (Widget && Widget->GetFName() == TEXT("RouteEncounterTertiaryAction"))
			{
				ChestRouteCardChoice = Cast<UButton>(Widget);
			}
		});
	}
	TestNotNull(TEXT("chest presents the third relic reward choice"), ChestRouteCardChoice);
	PlayerController->CloseRouteEncounterPanel();

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("camp opens a choice panel instead of healing on F"), PlayerController->OpenRouteEncounterPanel());
	TestEqual(TEXT("camp primary explicitly selects the life-saving talisman"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::CampTakeLifeSavingTalisman);
	TestEqual(TEXT("camp alternative explicitly selects route-local money"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::CampTakeRouteMoney);
	PlayerController->CloseRouteEncounterPanel();

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPanelResolutionTest,
	"GameXXK.MVP.RouteEncounter.Panel.VisibleChoicesResolveOnlyOnClick",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterPanelResolutionTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates the reusable route choice panel"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = PlayerController->GetRouteEncounterPanelWidgetForTest();

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	ConfigureMountainSpringEncounter(Subsystem->GetMutableRuntimeState());
	const int32 EnvironmentAttributesBefore = TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses);
	TestTrue(TEXT("environment primary route choice opens before it resolves"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("environment primary click selects its concrete route attribute"), Panel && Panel->TriggerPrimaryActionForTest());
	TestTrue(TEXT("environment selected card resolves only on confirm"), Panel && Panel->ConfirmSelectedChoiceForTest());
	TestTrue(TEXT("environment route attribute is awarded only after its explicit click"),
		TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses) > EnvironmentAttributesBefore);
	TestEqual(TEXT("environment click returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("environment click closes the modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	const FName ExpectedChestRelic = Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds[1];
	TestTrue(TEXT("chest stays unresolved until a visible reward is selected"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("chest alternate relic selects through the panel"), Panel && Panel->TriggerSecondaryActionForTest());
	TestTrue(TEXT("chest alternate relic resolves through confirm"), Panel && Panel->ConfirmSelectedChoiceForTest());
	TestEqual(TEXT("chest alternate selection grants the visible relic"),
		Subsystem->GetRuntimeState().CardRun.Relics.IsEmpty() ? NAME_None : Subsystem->GetRuntimeState().CardRun.Relics[0].RelicId,
		ExpectedChestRelic);
	TestEqual(TEXT("chest choice returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("chest choice closes the modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	Subsystem->GetMutableRuntimeState().PlayerHP = 33;
	TestTrue(TEXT("camp charm is shown before player state changes"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("camp charm resolves from the explicit panel click"), Panel && Panel->TriggerPrimaryActionForTest());
	TestEqual(TEXT("camp charm never heals directly"), Subsystem->GetRuntimeState().PlayerHP, 33);
	TestTrue(TEXT("camp charm click grants the life-saving talisman"),
		Subsystem->GetRuntimeState().CardRun.Relics.ContainsByPredicate([](const FGameXXKRelicInstance& Relic)
		{
			return Relic.RelicId == TEXT("Relic.LifeSavingTalisman");
		}));
	TestEqual(TEXT("camp choice returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("camp choice closes the modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterFocusedSourceTest,
	"GameXXK.MVP.RouteEncounter.Panel.FocusedSourceAndResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterFocusedSourceTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Event, EGameXXKScreen::RouteEvent);
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.SourceNodeId = 1;
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Event.Attribute.MountainSpring");
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.MountainSpring");

	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates and refreshes the source-less route HUD"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	TestTrue(TEXT("route event automatically opens without a world interaction source"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestNull(TEXT("pure HUD route event stores no legacy world actor"), PlayerController->GetRouteEncounterSourceActorForTest());
	const int32 GoldBeforeChoice = Subsystem->GetRuntimeState().PlayerGold;
	TestEqual(TEXT("opening the source-less HUD grants no reward before choice"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeChoice);

	UGameXXKRouteEncounterPanelWidget* Panel = PlayerController->GetRouteEncounterPanelWidgetForTest();
	TestTrue(TEXT("source-less event selects only after the visible primary card"), Panel && Panel->TriggerPrimaryActionForTest());
	TestTrue(TEXT("source-less event resolves only after confirm"), Panel && Panel->ConfirmSelectedChoiceForTest());
	TestNull(TEXT("successful source-less choice keeps the legacy source empty"), PlayerController->GetRouteEncounterSourceActorForTest());
	TestFalse(TEXT("successful choice closes the route panel"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestEqual(TEXT("successful choice clears the pending route node"), Subsystem->GetRuntimeState().PendingRouteNodeId, INDEX_NONE);
	TestTrue(TEXT("successful choice marks the original route node visited"), Subsystem->GetRuntimeState().VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("successful choice clears pending event identity"), Subsystem->GetRuntimeState().CardRun.PendingEvent.EventNpcId.IsNone());
	TestTrue(TEXT("successful choice clears pending event source"), Subsystem->GetRuntimeState().CardRun.PendingEvent.SourceNodeId < 0);

	Subsystem->GetMutableRuntimeState() = BuildClickableRouteMerchantState();
	TestTrue(TEXT("merchant source-isolation fixture enters through the route rule"), Subsystem->SelectRouteNodeById(1));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestFalse(TEXT("merchant scene keeps the generic route panel collapsed"), PlayerController->IsRouteEncounterPanelOpenForTest());
	TestTrue(TEXT("merchant scene opens the dedicated merchant HUD"), PlayerController->IsRouteMerchantWidgetOpenForTest());
	TestNull(TEXT("merchant dedicated HUD stores no legacy encounter actor"), PlayerController->GetRouteEncounterSourceActorForTest());

	return true;
}

#endif
