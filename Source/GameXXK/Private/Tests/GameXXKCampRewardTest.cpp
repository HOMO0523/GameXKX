#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState BuildGeneratedCampState(const EGameXXKScreen Screen)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = Screen;
		State.CurrentMapId = Screen == EGameXXKScreen::RouteCamp ? TEXT("RouteCamp") : TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}},
			FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Camp, FVector2D(0.5f, 0.5f), TArray<int32>{2}},
			FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}}};
		State.RouteMapEdges = {
			FGameXXKRouteMapEdge{0, 1},
			FGameXXKRouteMapEdge{1, 2}};
		State.VisitedRouteNodeIds = {0};
		State.ReachableRouteNodeIds = {1};
		State.CurrentRouteNodeId = 1;
		State.PendingRouteNodeId = 1;
		State.CardRun.RouteProgress.CurrentChapter = 1;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60);
		return State;
	}

	FGameXXKRuntimeState BuildFixedCampState()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::RouteCamp;
		State.CurrentMapId = TEXT("RouteCamp");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = false;
		State.DungeonNodeIndex = 3;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60);
		return State;
	}

	bool OwnsLifeSavingTalisman(const FGameXXKRuntimeState& State)
	{
		return FGameXXKRelicRules::OwnsLifeSavingTalisman(State);
	}

	const FGameXXKRouteTravelMoneyReceipt* FindReceipt(
		const FGameXXKRuntimeState& State,
		const int32 Chapter,
		const int32 NodeId)
	{
		return State.CardRun.RewardedTravelMoneyNodes.FindByPredicate(
			[Chapter, NodeId](const FGameXXKRouteTravelMoneyReceipt& Receipt)
			{
				return Receipt.Chapter == Chapter && Receipt.NodeId == NodeId;
			});
	}

	FString CollectCampPresentationCopy(UGameXXKRouteEncounterPanelWidget* Panel)
	{
		FString Copy;
		if (!Panel || !Panel->WidgetTree)
		{
			return Copy;
		}
		Panel->WidgetTree->ForEachWidget([&Copy](UWidget* Widget)
		{
			if (const UTextBlock* TextBlock = Cast<UTextBlock>(Widget))
			{
				Copy += TextBlock->GetText().ToString();
				Copy += TEXT("\n");
			}
			if (const UButton* Button = Cast<UButton>(Widget))
			{
				Copy += Button->GetToolTipText().ToString();
				Copy += TEXT("\n");
			}
		});
		return Copy;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardBlueprintPinCompatibilityTest,
	"GameXXK.MVP.RouteEncounter.Camp.BlueprintPinNameRemainsHealNow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardBlueprintPinCompatibilityTest::RunTest(const FString& Parameters)
{
	const UFunction* RulesFunction = UGameXXKMVPRules::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UGameXXKMVPRules, ResolveCampReward));
	const UFunction* SubsystemFunction = UGameXXKMVPSubsystem::StaticClass()->FindFunctionByName(
		GET_FUNCTION_NAME_CHECKED(UGameXXKMVPSubsystem, ResolveCampReward));
	TestNotNull(TEXT("rules compatibility function remains reflected"), RulesFunction);
	TestNotNull(TEXT("subsystem compatibility function remains reflected"), SubsystemFunction);
	TestNotNull(TEXT("rules compatibility function preserves the serialized bHealNow pin"),
		RulesFunction ? FindFProperty<FBoolProperty>(RulesFunction, TEXT("bHealNow")) : nullptr);
	TestNotNull(TEXT("subsystem compatibility function preserves the serialized bHealNow pin"),
		SubsystemFunction ? FindFProperty<FBoolProperty>(SubsystemFunction, TEXT("bHealNow")) : nullptr);
	TestNull(TEXT("rules compatibility function does not serialize the semantic implementation name"),
		RulesFunction ? FindFProperty<FBoolProperty>(RulesFunction, TEXT("bTakeLifeSavingTalisman")) : nullptr);
	TestNull(TEXT("subsystem compatibility function does not serialize the semantic implementation name"),
		SubsystemFunction ? FindFProperty<FBoolProperty>(SubsystemFunction, TEXT("bTakeLifeSavingTalisman")) : nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChallengeStartsAtCampTest,
	"GameXXK.MVP.RouteEncounter.Camp.TrainingChallengeStartsAtCamp",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChallengeStartsAtCampTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("camp-start fixture starts in Town"), Subsystem->StartGame()))
	{
		return false;
	}
	const FName StageId =
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	const int32 PowderBefore = UGameXXKMVPRules::GetItemCount(
		Subsystem->GetRuntimeState(),
		UGameXXKMVPRules::ItemHealingPowder());
	if (!TestTrue(TEXT("camp-start fixture starts the 1-1 challenge"),
		Subsystem->StartTrainingChallenge(StageId)))
	{
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const FGameXXKRouteMapNode* StartNode = State.RouteMapNodes.FindByPredicate(
		[](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeKind == EGameXXKNodeKind::Start;
		});
	if (!TestNotNull(TEXT("the challenge route keeps a visible current-camp marker"), StartNode))
	{
		return false;
	}
	TestTrue(TEXT("the current-camp marker is already visited when the route opens"),
		State.VisitedRouteNodeIds.Contains(StartNode->NodeId));
	TestFalse(TEXT("the current-camp marker is never a clickable reachable node"),
		State.ReachableRouteNodeIds.Contains(StartNode->NodeId));
	for (const int32 OutgoingNodeId : StartNode->OutgoingNodeIds)
	{
		TestTrue(TEXT("opening the route immediately unlocks every first branch"),
			State.ReachableRouteNodeIds.Contains(OutgoingNodeId));
	}
	TestFalse(TEXT("clicking the already occupied camp marker is rejected"),
		Subsystem->SelectRouteNodeById(StartNode->NodeId));
	TestEqual(TEXT("automatic camp placement grants no retired medicine"),
		UGameXXKMVPRules::GetItemCount(
			Subsystem->GetRuntimeState(),
			UGameXXKMVPRules::ItemHealingPowder()),
		PowderBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardCommandSurfaceTest,
	"GameXXK.MVP.RouteEncounter.Camp.CommandSurfaceHasHealAndRouteMoneyOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardCommandSurfaceTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	const struct
	{
		FGameXXKRuntimeState State;
		const TCHAR* Label;
	} Cases[] = {
		{BuildGeneratedCampState(EGameXXKScreen::RouteCamp), TEXT("generated Camp screen")},
		{BuildGeneratedCampState(EGameXXKScreen::DungeonMap), TEXT("DungeonMap compatibility entry")},
		{BuildFixedCampState(), TEXT("fixed-route compatibility entry")},
	};

	for (const auto& Case : Cases)
	{
		Subsystem->GetMutableRuntimeState() = Case.State;
		const TArray<FGameXXKMVPCommandDescriptor> Commands = GameXXKMVPCommandRouter::BuildVisibleCommands(Subsystem);
		const FGameXXKMVPCommandDescriptor* Heal = Commands.FindByPredicate([](const FGameXXKMVPCommandDescriptor& Command)
		{
			return Command.CommandName == TEXT("ResolveCampHeal");
		});
		const FGameXXKMVPCommandDescriptor* Money = Commands.FindByPredicate([](const FGameXXKMVPCommandDescriptor& Command)
		{
			return Command.CommandName == TEXT("ResolveCampRouteMoney");
		});
		TestNotNull(FString::Printf(TEXT("%s exposes the heal command"), Case.Label), Heal);
		TestNotNull(FString::Printf(TEXT("%s exposes the route-money command"), Case.Label), Money);
		TestEqual(FString::Printf(TEXT("%s heal command uses the approved label"), Case.Label),
			Heal ? Heal->Label.ToString() : FString(), FString(TEXT("全队恢复30%气血")));
		TestEqual(FString::Printf(TEXT("%s money command uses the approved label"), Case.Label),
			Money ? Money->Label.ToString() : FString(), FString(TEXT("获得100局内金币")));
		TestTrue(FString::Printf(TEXT("%s enables healing"), Case.Label), Heal && Heal->bEnabled);
		TestTrue(FString::Printf(TEXT("%s enables route money"), Case.Label), Money && Money->bEnabled);
		TestFalse(FString::Printf(TEXT("%s exposes no retired charm command"), Case.Label),
			Commands.ContainsByPredicate([](const FGameXXKMVPCommandDescriptor& Command)
			{
				return Command.CommandName == TEXT("ResolveCampCharm") || Command.Label.ToString().Contains(TEXT("护符"));
			}));
	}

	FGameXXKRuntimeState OwnedState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("owned command-surface fixture may already own a charm"),
		FGameXXKRelicRules::AcquireRelic(OwnedState, FGameXXKRelicRules::LifeSavingTalismanId()));
	Subsystem->GetMutableRuntimeState() = OwnedState;
	const TArray<FGameXXKMVPCommandDescriptor> OwnedCommands = GameXXKMVPCommandRouter::BuildVisibleCommands(Subsystem);
	const FGameXXKMVPCommandDescriptor* OwnedHeal = OwnedCommands.FindByPredicate([](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == TEXT("ResolveCampHeal");
	});
	const FGameXXKMVPCommandDescriptor* OwnedMoney = OwnedCommands.FindByPredicate([](const FGameXXKMVPCommandDescriptor& Command)
	{
		return Command.CommandName == TEXT("ResolveCampRouteMoney");
	});
	TestTrue(TEXT("healing remains enabled regardless of relic ownership"), OwnedHeal && OwnedHeal->bEnabled);
	TestTrue(TEXT("route money remains enabled regardless of relic ownership"), OwnedMoney && OwnedMoney->bEnabled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardPresentationTest,
	"GameXXK.MVP.RouteEncounter.Camp.CharmOrRouteMoneyPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardPresentationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Subsystem->GetMutableRuntimeState() = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("camp presentation fixture creates the route encounter panel"), Controller->EnsurePlayerFlowWidgetsForTest());
	UGameXXKRouteEncounterPanelWidget* Panel = Controller->GetRouteEncounterPanelWidgetForTest();
	TestNotNull(TEXT("camp presentation fixture owns the route encounter panel"), Panel);
	if (!Panel)
	{
		return false;
	}
	TestTrue(TEXT("camp presentation opens before either choice is made"), Controller->OpenRouteEncounterPanel());
	TestEqual(TEXT("camp primary label is exactly the party-healing reward"),
		Panel->GetPrimaryActionTextForTest().ToString(), FString(TEXT("全队恢复30%气血")));
	TestEqual(TEXT("camp secondary label is exactly one hundred run-local gold"),
		Panel->GetSecondaryActionTextForTest().ToString(), FString(TEXT("获得100局内金币")));
	TestTrue(TEXT("the explicit heal action is appended after the compatibility actions"),
		static_cast<uint8>(Panel->GetPrimaryActionForTest()) > static_cast<uint8>(EGameXXKRouteEncounterAction::ClosePanel));
	TestTrue(TEXT("the explicit money action is appended after the compatibility actions"),
		static_cast<uint8>(Panel->GetSecondaryActionForTest()) > static_cast<uint8>(EGameXXKRouteEncounterAction::ClosePanel));
	TestNotEqual(TEXT("the camp no longer presents the legacy rest action"),
		Panel->GetPrimaryActionForTest(), EGameXXKRouteEncounterAction::CampRest);
	TestNotEqual(TEXT("the camp no longer presents the legacy healing-powder action"),
		Panel->GetSecondaryActionForTest(), EGameXXKRouteEncounterAction::CampTakeHealingPowder);
	TestTrue(TEXT("the two camp rewards use distinct actions"),
		Panel->GetPrimaryActionForTest() != Panel->GetSecondaryActionForTest());

	UButton* PrimaryButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterPrimaryAction")));
	UButton* SecondaryButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterSecondaryAction")));
	UButton* TertiaryButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterTertiaryAction")));
	TestTrue(TEXT("camp heal choice is visible and enabled"),
		PrimaryButton && PrimaryButton->GetVisibility() == ESlateVisibility::Visible && PrimaryButton->GetIsEnabled());
	TestTrue(TEXT("camp money choice is visible and enabled"),
		SecondaryButton && SecondaryButton->GetVisibility() == ESlateVisibility::Visible && SecondaryButton->GetIsEnabled());
	TestTrue(TEXT("camp has no third reward choice"),
		TertiaryButton && TertiaryButton->GetVisibility() == ESlateVisibility::Collapsed);

	const FString CampCopy = CollectCampPresentationCopy(Panel);
	TestFalse(TEXT("camp copy contains no healing-powder item name"), CampCopy.Contains(TEXT("疗伤散")));
	TestFalse(TEXT("camp copy contains no legacy medicine name"), CampCopy.Contains(TEXT("金疮药")));
	TestFalse(TEXT("camp copy contains no full-health restoration promise"), CampCopy.Contains(TEXT("恢复至满血")));
	TestTrue(TEXT("camp copy states the exact restoration percentage"), CampCopy.Contains(TEXT("恢复30%")));

	FGameXXKRuntimeState& LiveState = Subsystem->GetMutableRuntimeState();
	TestTrue(TEXT("external charm ownership is irrelevant to camp healing"),
		FGameXXKRelicRules::AcquireRelic(LiveState, FGameXXKRelicRules::LifeSavingTalismanId()));
	Panel->RefreshFromState();
	PrimaryButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterPrimaryAction")));
	TestTrue(TEXT("refresh keeps the healing choice visible"),
		PrimaryButton && PrimaryButton->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("refresh keeps healing enabled after external relic acquisition"),
		PrimaryButton && PrimaryButton->GetIsEnabled());
	TestEqual(TEXT("refresh keeps the healing explanation"),
		PrimaryButton ? PrimaryButton->GetToolTipText().ToString() : FString(),
		FString(TEXT("每名当前队员恢复其最大气血的30%，不超过上限。")));
	TestTrue(TEXT("healing action resolves after refresh"), Panel->TriggerPrimaryActionForTest());
	TestEqual(TEXT("healing writes one node receipt"), LiveState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	TestEqual(TEXT("healing settles the Camp"), LiveState.Screen, EGameXXKScreen::DungeonMap);

	Controller->CloseRouteEncounterPanel();
	FGameXXKRuntimeState OwnedState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("owned-relic fixture owns the life-saving talisman"),
		FGameXXKRelicRules::AcquireRelic(OwnedState, FGameXXKRelicRules::LifeSavingTalismanId()));
	Subsystem->GetMutableRuntimeState() = OwnedState;
	TestTrue(TEXT("an owned relic does not hide the camp panel"), Controller->OpenRouteEncounterPanel());
	PrimaryButton = Cast<UButton>(Panel->WidgetTree->FindWidget(TEXT("RouteEncounterPrimaryAction")));
	TestTrue(TEXT("healing stays visible with an owned relic"),
		PrimaryButton && PrimaryButton->GetVisibility() == ESlateVisibility::Visible);
	TestTrue(TEXT("healing stays enabled with an owned relic"), PrimaryButton && PrimaryButton->GetIsEnabled());
	TestEqual(TEXT("healing tooltip remains independent from relic ownership"),
		PrimaryButton ? PrimaryButton->GetToolTipText().ToString() : FString(),
		FString(TEXT("每名当前队员恢复其最大气血的30%，不超过上限。")));
	TestTrue(TEXT("owned relic does not block healing"), Panel->TriggerPrimaryActionForTest());
	TestEqual(TEXT("healing settles the owned-relic camp"),
		Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	Controller->CloseRouteEncounterPanel();
	FGameXXKRuntimeState MoneyState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	const int32 RouteMoneyBefore = MoneyState.CardRun.RouteTravelMoney;
	const int32 PermanentGoldBefore = MoneyState.PlayerGold;
	Subsystem->GetMutableRuntimeState() = MoneyState;
	TestTrue(TEXT("money wiring fixture opens the Camp panel"), Controller->OpenRouteEncounterPanel());
	TestTrue(TEXT("the visible secondary choice dispatches through the controller"), Panel->TriggerSecondaryActionForTest());
	TestEqual(TEXT("the visible secondary choice adds exactly one hundred route-local gold"),
		Subsystem->GetRuntimeState().CardRun.RouteTravelMoney, RouteMoneyBefore + 100);
	TestEqual(TEXT("the visible secondary choice never changes permanent gold"),
		Subsystem->GetRuntimeState().PlayerGold, PermanentGoldBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardTransactionTest,
	"GameXXK.MVP.RouteEncounter.Camp.TransactionalRewardsAndDuplicateRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardTransactionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState CharmState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	CharmState.PlayerHP = 17;
	const int32 CharmGoldBefore = CharmState.PlayerGold;
	const int32 CharmMoneyBefore = CharmState.CardRun.RouteTravelMoney;
	const int32 CharmPowderBefore = UGameXXKMVPRules::GetItemCount(CharmState, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("generated camp heal choice resolves"), UGameXXKMVPRules::ResolveCampReward(CharmState, true));
	TestFalse(TEXT("generated camp heal choice acquires no relic"), OwnsLifeSavingTalisman(CharmState));
	TestEqual(TEXT("generated camp heal choice grants no relic"), CharmState.CardRun.Relics.Num(), 0);
	TestEqual(TEXT("generated camp heal choice restores exactly thirty percent max health"), CharmState.PlayerHP, 47);
	TestEqual(TEXT("generated camp heal choice never changes permanent gold"), CharmState.PlayerGold, CharmGoldBefore);
	TestEqual(TEXT("generated camp heal choice adds no route money"), CharmState.CardRun.RouteTravelMoney, CharmMoneyBefore);
	TestEqual(TEXT("generated camp heal choice adds no healing powder"),
		UGameXXKMVPRules::GetItemCount(CharmState, UGameXXKMVPRules::ItemHealingPowder()), CharmPowderBefore);
	TestEqual(TEXT("generated camp heal choice writes one settlement receipt"),
		CharmState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	const FGameXXKRouteTravelMoneyReceipt* CharmReceipt = FindReceipt(CharmState, 1, 1);
	TestNotNull(TEXT("generated camp charm receipt uses the pending node key"), CharmReceipt);
	if (CharmReceipt)
	{
		TestEqual(TEXT("generated camp charm receipt records zero currency"), CharmReceipt->Amount, 0);
	}
	TestTrue(TEXT("generated camp charm settlement visits the node"), CharmState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("generated camp charm settlement returns to the route map"), CharmState.Screen, EGameXXKScreen::DungeonMap);

	FGameXXKRuntimeState MoneyState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	MoneyState.PlayerHP = 19;
	const int32 MoneyGoldBefore = MoneyState.PlayerGold;
	const int32 MoneyBefore = MoneyState.CardRun.RouteTravelMoney;
	const int32 MoneyPowderBefore = UGameXXKMVPRules::GetItemCount(MoneyState, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("generated camp money choice resolves"), UGameXXKMVPRules::ResolveCampReward(MoneyState, false));
	TestEqual(TEXT("generated camp money choice adds exactly one hundred route-local gold"),
		MoneyState.CardRun.RouteTravelMoney, MoneyBefore + 100);
	TestEqual(TEXT("generated camp money choice never changes permanent gold"), MoneyState.PlayerGold, MoneyGoldBefore);
	TestEqual(TEXT("generated camp money choice never heals directly"), MoneyState.PlayerHP, 19);
	TestEqual(TEXT("generated camp money choice adds no healing powder"),
		UGameXXKMVPRules::GetItemCount(MoneyState, UGameXXKMVPRules::ItemHealingPowder()), MoneyPowderBefore);
	TestFalse(TEXT("generated camp money choice acquires no relic"), OwnsLifeSavingTalisman(MoneyState));
	TestEqual(TEXT("generated camp money choice writes one settlement receipt"),
		MoneyState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	const FGameXXKRouteTravelMoneyReceipt* MoneyReceipt = FindReceipt(MoneyState, 1, 1);
	TestNotNull(TEXT("generated camp money receipt uses the pending node key"), MoneyReceipt);
	if (MoneyReceipt)
	{
		TestEqual(TEXT("generated camp money receipt records exactly one hundred"), MoneyReceipt->Amount, 100);
	}

	FGameXXKRuntimeState DuplicateState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("duplicate direct-rule fixture acquires its first charm"),
		FGameXXKRelicRules::AcquireRelic(DuplicateState, FGameXXKRelicRules::LifeSavingTalismanId()));
	const int32 DuplicateMoneyBefore = DuplicateState.CardRun.RouteTravelMoney;
	const int32 DuplicateGoldBefore = DuplicateState.PlayerGold;
	const int32 DuplicateOrdinalBefore = DuplicateState.CardRun.NextRelicAcquisitionOrdinal;
	const TArray<int32> DuplicateVisitedBefore = DuplicateState.VisitedRouteNodeIds;
	const TArray<int32> DuplicateReachableBefore = DuplicateState.ReachableRouteNodeIds;
	DuplicateState.PlayerHP = 10;
	TestTrue(TEXT("Camp healing ignores already-owned relics"),
		UGameXXKMVPRules::ResolveCampReward(DuplicateState, true));
	TestEqual(TEXT("healing keeps the existing relic"), DuplicateState.CardRun.Relics.Num(), 1);
	TestEqual(TEXT("healing keeps one relic stack"), DuplicateState.CardRun.Relics[0].Stacks, 1);
	TestEqual(TEXT("healing keeps the relic acquisition ordinal"),
		DuplicateState.CardRun.NextRelicAcquisitionOrdinal, DuplicateOrdinalBefore);
	TestEqual(TEXT("healing writes one settlement receipt"),
		DuplicateState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	TestEqual(TEXT("healing keeps route money"),
		DuplicateState.CardRun.RouteTravelMoney, DuplicateMoneyBefore);
	TestEqual(TEXT("healing keeps permanent gold"), DuplicateState.PlayerGold, DuplicateGoldBefore);
	TestEqual(TEXT("healing restores thirty health"), DuplicateState.PlayerHP, 40);
	TestEqual(TEXT("healing settles the Camp"), DuplicateState.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("healing clears the pending node"), DuplicateState.PendingRouteNodeId, INDEX_NONE);
	TestFalse(TEXT("healing advances visited route structure"), DuplicateState.VisitedRouteNodeIds == DuplicateVisitedBefore);
	TestFalse(TEXT("healing advances reachable route structure"), DuplicateState.ReachableRouteNodeIds == DuplicateReachableBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardAtomicBoundariesTest,
	"GameXXK.MVP.RouteEncounter.Camp.OverflowBonusesReceiptKeysAndCrossChoiceReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardAtomicBoundariesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState OverflowState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	OverflowState.CardRun.RouteTravelMoney = MAX_int32 - 99;
	const FGameXXKRuntimeState OverflowBefore = OverflowState;
	TestFalse(TEXT("one-hundred route money rejects overflow"), UGameXXKMVPRules::ResolveCampReward(OverflowState, false));
	TestTrue(TEXT("route-money overflow rolls back the complete runtime"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&OverflowState, &OverflowBefore, PPF_None));

	FGameXXKRuntimeState WineCupState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("WineCup fixture acquires the real route-money relic"), FGameXXKRelicRules::AcquireRelic(WineCupState, TEXT("Relic.WineCup")));
	const int32 WineCupMoneyBefore = WineCupState.CardRun.RouteTravelMoney;
	const int32 WineCupGoldBefore = WineCupState.PlayerGold;
	TestTrue(TEXT("Camp money resolves with WineCup"), UGameXXKMVPRules::ResolveCampReward(WineCupState, false));
	TestEqual(TEXT("Camp money adds base one hundred plus the exact three-money WineCup bonus"),
		WineCupState.CardRun.RouteTravelMoney, WineCupMoneyBefore + 103);
	TestEqual(TEXT("WineCup Camp money never changes permanent gold"), WineCupState.PlayerGold, WineCupGoldBefore);
	const FGameXXKRouteTravelMoneyReceipt* WineCupReceipt = FindReceipt(WineCupState, 1, 1);
	TestNotNull(TEXT("WineCup Camp money writes the keyed receipt"), WineCupReceipt);
	if (WineCupReceipt)
	{
		TestEqual(TEXT("WineCup receipt records base one hundred plus separate three-money bonus"), WineCupReceipt->Amount, 103);
		TestEqual(TEXT("WineCup receipt retains an exact one-hundred Camp component"), WineCupReceipt->Amount - 3, 100);
	}

	FGameXXKRuntimeState ChapterTwoState = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	bool bChapterOneAwarded = false;
	TestTrue(TEXT("same-node chapter fixture writes the chapter-one receipt"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(ChapterTwoState.CardRun, 1, 1, 0, bChapterOneAwarded));
	TestTrue(TEXT("same-node chapter-one receipt is newly awarded"), bChapterOneAwarded);
	ChapterTwoState.CardRun.RouteProgress.CurrentChapter = 2;
	const int32 ChapterTwoMoneyBefore = ChapterTwoState.CardRun.RouteTravelMoney;
	TestTrue(TEXT("the same Camp node ID resolves independently in chapter two"),
		UGameXXKMVPRules::ResolveCampReward(ChapterTwoState, false));
	TestEqual(TEXT("chapter-two same-node Camp adds exactly one hundred"),
		ChapterTwoState.CardRun.RouteTravelMoney, ChapterTwoMoneyBefore + 100);
	TestEqual(TEXT("same node ID retains distinct chapter receipts"),
		ChapterTwoState.CardRun.RewardedTravelMoneyNodes.Num(), 2);
	const FGameXXKRouteTravelMoneyReceipt* ChapterTwoReceipt = FindReceipt(ChapterTwoState, 2, 1);
	TestNotNull(TEXT("chapter-two receipt uses the chapter-two key"), ChapterTwoReceipt);
	if (ChapterTwoReceipt)
	{
		TestEqual(TEXT("chapter-two receipt records exactly one hundred"), ChapterTwoReceipt->Amount, 100);
	}

	FGameXXKRuntimeState MoneyThenCharm = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	bool bMoneyApplied = false;
	TestTrue(TEXT("cross-choice money receipt fixture applies once"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(MoneyThenCharm.CardRun, 1, 1, 100, bMoneyApplied));
	TestTrue(TEXT("cross-choice money receipt is newly awarded"), bMoneyApplied);
	const int32 MoneyThenCharmBalance = MoneyThenCharm.CardRun.RouteTravelMoney;
	TestTrue(TEXT("a money receipt replayed through the charm choice only finishes structure"),
		UGameXXKMVPRules::ResolveCampReward(MoneyThenCharm, true));
	TestFalse(TEXT("money-to-charm replay does not acquire a charm"), FGameXXKRelicRules::OwnsLifeSavingTalisman(MoneyThenCharm));
	TestEqual(TEXT("money-to-charm replay does not duplicate money"), MoneyThenCharm.CardRun.RouteTravelMoney, MoneyThenCharmBalance);
	TestEqual(TEXT("money-to-charm replay keeps one receipt"), MoneyThenCharm.CardRun.RewardedTravelMoneyNodes.Num(), 1);

	FGameXXKRuntimeState CharmThenMoney = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("cross-choice charm fixture acquires the charm once"),
		FGameXXKRelicRules::AcquireRelic(CharmThenMoney, FGameXXKRelicRules::LifeSavingTalismanId()));
	bool bCharmApplied = false;
	TestTrue(TEXT("cross-choice charm receipt fixture applies once"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(CharmThenMoney.CardRun, 1, 1, 0, bCharmApplied));
	TestTrue(TEXT("cross-choice charm receipt is newly awarded"), bCharmApplied);
	const int32 CharmThenMoneyBalance = CharmThenMoney.CardRun.RouteTravelMoney;
	const int32 CharmThenMoneyOrdinal = CharmThenMoney.CardRun.NextRelicAcquisitionOrdinal;
	TestTrue(TEXT("a charm receipt replayed through money only finishes structure"),
		UGameXXKMVPRules::ResolveCampReward(CharmThenMoney, false));
	TestTrue(TEXT("charm-to-money replay keeps the one charm"), FGameXXKRelicRules::OwnsLifeSavingTalisman(CharmThenMoney));
	TestEqual(TEXT("charm-to-money replay does not add money"), CharmThenMoney.CardRun.RouteTravelMoney, CharmThenMoneyBalance);
	TestEqual(TEXT("charm-to-money replay does not reacquire the charm"),
		CharmThenMoney.CardRun.NextRelicAcquisitionOrdinal, CharmThenMoneyOrdinal);
	TestEqual(TEXT("charm-to-money replay keeps one receipt"), CharmThenMoney.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCampRewardCompatibilityAndReplayTest,
	"GameXXK.MVP.RouteEncounter.Camp.GeneratedMapCompatibilityFixedRouteAndReceiptReplay",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampRewardCompatibilityAndReplayTest::RunTest(const FString& Parameters)
{
	for (const bool bHeal : {true, false})
	{
		FGameXXKRuntimeState CompatibilityState = BuildGeneratedCampState(EGameXXKScreen::DungeonMap);
		const int32 MoneyBefore = CompatibilityState.CardRun.RouteTravelMoney;
		const int32 GoldBefore = CompatibilityState.PlayerGold;
		TestTrue(TEXT("DungeonMap compatibility Camp choice resolves through the shared transaction"),
			UGameXXKMVPRules::ResolveCampReward(CompatibilityState, bHeal));
		TestFalse(TEXT("DungeonMap compatibility choice grants no relic"),
			OwnsLifeSavingTalisman(CompatibilityState));
		TestEqual(TEXT("DungeonMap compatibility money delta matches the chosen reward"),
			CompatibilityState.CardRun.RouteTravelMoney - MoneyBefore, bHeal ? 0 : 100);
		TestEqual(TEXT("DungeonMap compatibility choice never changes permanent gold"), CompatibilityState.PlayerGold, GoldBefore);
		TestEqual(TEXT("DungeonMap compatibility choice records exactly one receipt"),
			CompatibilityState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
		TestTrue(TEXT("DungeonMap compatibility choice visits the Camp node"), CompatibilityState.VisitedRouteNodeIds.Contains(1));
	}

	for (const bool bHeal : {true, false})
	{
		FGameXXKRuntimeState FixedState = BuildFixedCampState();
		FixedState.PlayerHP = 23;
		const int32 MoneyBefore = FixedState.CardRun.RouteTravelMoney;
		const int32 GoldBefore = FixedState.PlayerGold;
		const int32 PowderBefore = UGameXXKMVPRules::GetItemCount(FixedState, UGameXXKMVPRules::ItemHealingPowder());
		TestTrue(TEXT("fixed-route Camp choice resolves through the shared transaction"),
			UGameXXKMVPRules::ResolveCampReward(FixedState, bHeal));
		TestFalse(TEXT("fixed-route choice grants no relic"),
			OwnsLifeSavingTalisman(FixedState));
		TestEqual(TEXT("fixed-route money delta matches the chosen reward"),
			FixedState.CardRun.RouteTravelMoney - MoneyBefore, bHeal ? 0 : 100);
		TestEqual(TEXT("fixed-route choice never changes permanent gold"), FixedState.PlayerGold, GoldBefore);
		TestEqual(TEXT("fixed-route healing matches the chosen reward"), FixedState.PlayerHP, bHeal ? 53 : 23);
		TestEqual(TEXT("fixed-route choice never adds healing powder"),
			UGameXXKMVPRules::GetItemCount(FixedState, UGameXXKMVPRules::ItemHealingPowder()), PowderBefore);
		TestEqual(TEXT("fixed-route choice records exactly one receipt"), FixedState.CardRun.RewardedTravelMoneyNodes.Num(), 1);
		TestEqual(TEXT("fixed-route choice advances exactly one node"), FixedState.DungeonNodeIndex, 4);
		TestEqual(TEXT("fixed-route choice returns to the route map"), FixedState.Screen, EGameXXKScreen::DungeonMap);
	}

	FGameXXKRuntimeState CharmReplay = BuildGeneratedCampState(EGameXXKScreen::RouteCamp);
	TestTrue(TEXT("charm replay fixture has the already-applied charm"),
		FGameXXKRelicRules::AcquireRelic(CharmReplay, FGameXXKRelicRules::LifeSavingTalismanId()));
	bool bCharmReceiptAwarded = false;
	TestTrue(TEXT("charm replay fixture writes the authoritative zero-value receipt"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(CharmReplay.CardRun, 1, 1, 0, bCharmReceiptAwarded));
	TestTrue(TEXT("charm replay fixture applies its receipt once"), bCharmReceiptAwarded);
	const int32 CharmOrdinalBeforeReplay = CharmReplay.CardRun.NextRelicAcquisitionOrdinal;
	TestTrue(TEXT("replaying a charm receipt finishes structural settlement"),
		UGameXXKMVPRules::ResolveCampReward(CharmReplay, true));
	TestEqual(TEXT("charm receipt replay keeps one unique charm"), CharmReplay.CardRun.Relics.Num(), 1);
	TestEqual(TEXT("charm receipt replay does not reacquire the charm"),
		CharmReplay.CardRun.NextRelicAcquisitionOrdinal, CharmOrdinalBeforeReplay);
	TestEqual(TEXT("charm receipt replay does not append a second receipt"),
		CharmReplay.CardRun.RewardedTravelMoneyNodes.Num(), 1);

	FGameXXKRuntimeState MoneyReplay = BuildFixedCampState();
	bool bMoneyReceiptAwarded = false;
	TestTrue(TEXT("money replay fixture applies its one-hundred receipt"),
		FGameXXKRouteEconomyRules::AwardNodeOnce(MoneyReplay.CardRun, 1, 3, 100, bMoneyReceiptAwarded));
	TestTrue(TEXT("money replay fixture records its first award"), bMoneyReceiptAwarded);
	const int32 MoneyBeforeReplay = MoneyReplay.CardRun.RouteTravelMoney;
	TestTrue(TEXT("replaying a money receipt finishes fixed-route structural settlement"),
		UGameXXKMVPRules::ResolveCampReward(MoneyReplay, false));
	TestEqual(TEXT("money receipt replay does not add another one hundred"),
		MoneyReplay.CardRun.RouteTravelMoney, MoneyBeforeReplay);
	TestEqual(TEXT("money receipt replay does not append a second receipt"),
		MoneyReplay.CardRun.RewardedTravelMoneyNodes.Num(), 1);
	TestEqual(TEXT("money receipt replay still advances the fixed Camp node"), MoneyReplay.DungeonNodeIndex, 4);
	return true;
}

#endif
