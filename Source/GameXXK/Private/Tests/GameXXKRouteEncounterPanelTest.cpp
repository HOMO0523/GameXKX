#include "Components/Button.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "InputKeyEventArgs.h"
#include "Misc/AutomationTest.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKRouteEncounterSceneActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKOneGameRouteMapWidget.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "UI/GameXXKRouteMerchantWidget.h"

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

	void ConfigureNiuHuanEncounter(FGameXXKRuntimeState& State)
	{
		State.CardRun.PendingEvent.SourceNodeId = 1;
		State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.Event.NiuHuan");
		State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.NiuHuan");
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
	TestTrue(TEXT("clicking the visible event choice resolves the node"), Panel->TriggerPrimaryActionForTest());
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
	TestTrue(TEXT("clicking the visible third relic resolves the treasure node"), Panel->TriggerTertiaryActionForTest());
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
	TestEqual(TEXT("runtime merchant HUD renders three card offers"), MerchantWidget ? MerchantWidget->GetRenderedCardOfferCountForTest() : 0, 3);
	TestEqual(TEXT("runtime merchant HUD renders three relic offers"), MerchantWidget ? MerchantWidget->GetRenderedRelicOfferCountForTest() : 0, 3);
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
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("controller creates the shared route encounter HUD"), PlayerController->EnsurePlayerFlowWidgetsForTest());

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
		Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(Case.NodeKind, Case.Screen);
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

	Subsystem->GetMutableRuntimeState() = BuildClickableRouteMerchantState();
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
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	Subsystem->GetMutableRuntimeState().CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.YueBai");

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
	TestEqual(TEXT("task-NPC event shows the pending YueBai identity"), Panel ? Panel->GetSpeakerTextForTest().ToString() : FString(), FString(TEXT("月白")));
	TestEqual(TEXT("event primary choice selects its first route attribute"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("event alternative selects its second route attribute"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice1);
	TestTrue(TEXT("event primary button names its concrete attribute gain"), Panel && Panel->GetPrimaryActionTextForTest().ToString().Contains(TEXT("最大内力")));

	const int32 RouteMaxManaBefore = Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxMana;
	TestTrue(TEXT("pressing the explicit primary button grants the selected event attribute"), Panel && Panel->TriggerPrimaryActionForTest());
	TestTrue(TEXT("explicit event choice increases route-local maximum mana"), Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses.MaxMana > RouteMaxManaBefore);
	TestTrue(TEXT("attribute events never install a temporary support"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());
	TestEqual(TEXT("explicit task-NPC choice completes the route node back to map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("panel closes after its explicit route choice resolves"), PlayerController->IsRouteEncounterPanelOpenForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterPanelVariantTest,
	"GameXXK.MVP.RouteEncounter.Panel.NiuHuanChestCampChoices",
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
	ConfigureNiuHuanEncounter(Subsystem->GetMutableRuntimeState());
	TestTrue(TEXT("NiuHuan event can be opened as a player choice"), PlayerController->OpenRouteEncounterPanel());
	TestEqual(TEXT("event-only NPC displays its concrete identity"), Panel ? Panel->GetSpeakerTextForTest().ToString() : FString(), FString(TEXT("牛欢")));
	TestEqual(TEXT("NiuHuan primary is its first route-attribute choice"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice0);
	TestEqual(TEXT("NiuHuan alternative is its second route-attribute choice"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::SelectChoice1);
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
	TestEqual(TEXT("camp primary explicitly selects rest"), Panel ? Panel->GetPrimaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::CampRest);
	TestEqual(TEXT("camp alternative explicitly selects supplies"), Panel ? Panel->GetSecondaryActionForTest() : EGameXXKRouteEncounterAction::None, EGameXXKRouteEncounterAction::CampTakeHealingPowder);
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
	ConfigureNiuHuanEncounter(Subsystem->GetMutableRuntimeState());
	const int32 NiuHuanAttributesBefore = TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses);
	TestTrue(TEXT("NiuHuan primary route choice opens before it resolves"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("NiuHuan primary click resolves its concrete route attribute"), Panel && Panel->TriggerPrimaryActionForTest());
	TestTrue(TEXT("NiuHuan route attribute is awarded only after its explicit click"),
		TotalRouteAttributeBonus(Subsystem->GetRuntimeState().CardRun.RouteAttributeBonuses) > NiuHuanAttributesBefore);
	TestEqual(TEXT("NiuHuan click returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("NiuHuan click closes the modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Chest, EGameXXKScreen::RouteEvent);
	ConfigureChestRelicEncounter(Subsystem->GetMutableRuntimeState());
	const FName ExpectedChestRelic = Subsystem->GetRuntimeState().CardRun.PendingRelicOffer.RelicIds[1];
	TestTrue(TEXT("chest stays unresolved until a visible reward is selected"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("chest alternate relic resolves through the panel"), Panel && Panel->TriggerSecondaryActionForTest());
	TestEqual(TEXT("chest alternate selection grants the visible relic"),
		Subsystem->GetRuntimeState().CardRun.Relics.IsEmpty() ? NAME_None : Subsystem->GetRuntimeState().CardRun.Relics[0].RelicId,
		ExpectedChestRelic);
	TestEqual(TEXT("chest choice returns to the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("chest choice closes the modal"), PlayerController->IsRouteEncounterPanelOpenForTest());

	Subsystem->GetMutableRuntimeState() = BuildPendingRouteEncounterState(EGameXXKNodeKind::Camp, EGameXXKScreen::RouteCamp);
	Subsystem->GetMutableRuntimeState().PlayerHP = 33;
	TestTrue(TEXT("camp rest is shown before player health changes"), PlayerController->OpenRouteEncounterPanel());
	TestTrue(TEXT("camp rest resolves from the explicit panel click"), Panel && Panel->TriggerPrimaryActionForTest());
	TestTrue(TEXT("camp rest improves health only after its click"), Subsystem->GetRuntimeState().PlayerHP > 33);
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
	TestTrue(TEXT("source-less event resolves only after the visible primary action"), Panel && Panel->TriggerPrimaryActionForTest());
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
