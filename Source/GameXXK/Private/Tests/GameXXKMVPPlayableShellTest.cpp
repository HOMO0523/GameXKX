#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPGameMode.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Interaction/GameXXKInteractable.h"
#include "GameFramework/Character.h"
#include "GameMapsSettings.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKMVPCommandRouter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static bool HasCommand(const TArray<FGameXXKMVPCommandDescriptor>& Commands, FName CommandName, bool* bOutEnabled = nullptr)
	{
		for (const FGameXXKMVPCommandDescriptor& Command : Commands)
		{
			if (Command.CommandName == CommandName)
			{
				if (bOutEnabled)
				{
					*bOutEnabled = Command.bEnabled;
				}
				return true;
			}
		}
		return false;
	}

	static FName MakeRouteNodeCommandName(int32 NodeId)
	{
		return FName(*FString::Printf(TEXT("RouteNode%d"), NodeId));
	}

	static const FGameXXKRouteMapNode* FindRouteNodeById(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	static bool RouteNodeCanReachKind(const FGameXXKRuntimeState& State, int32 StartNodeId, EGameXXKNodeKind TargetKind)
	{
		TSet<int32> Visited;
		TArray<int32> Stack;
		Stack.Add(StartNodeId);
		while (!Stack.IsEmpty())
		{
			const int32 NodeId = Stack.Pop(EAllowShrinking::No);
			if (Visited.Contains(NodeId))
			{
				continue;
			}
			Visited.Add(NodeId);
			const FGameXXKRouteMapNode* Node = FindRouteNodeById(State, NodeId);
			if (!Node)
			{
				continue;
			}
			if (Node->NodeKind == TargetKind)
			{
				return true;
			}
			for (int32 OutgoingNodeId : Node->OutgoingNodeIds)
			{
				Stack.Add(OutgoingNodeId);
			}
		}
		return false;
	}

	static const FGameXXKRouteMapNode* FindReachableRouteStepTowardKind(const FGameXXKRuntimeState& State, EGameXXKNodeKind TargetKind)
	{
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = FindRouteNodeById(State, NodeId);
			if (Node && Node->NodeKind == TargetKind)
			{
				return Node;
			}
		}
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			if (RouteNodeCanReachKind(State, NodeId, TargetKind))
			{
				return FindRouteNodeById(State, NodeId);
			}
		}
		return nullptr;
	}

	static bool HandleRouteTowardKind(AGameXXKMVPHUD* HUD, const UGameXXKMVPSubsystem* Subsystem, EGameXXKNodeKind TargetKind)
	{
		if (!HUD || !Subsystem)
		{
			return false;
		}
		for (int32 StepGuard = 0; StepGuard < 32 && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::DungeonMap; ++StepGuard)
		{
			const FGameXXKRouteMapNode* Node = FindReachableRouteStepTowardKind(Subsystem->GetRuntimeState(), TargetKind);
			if (!Node)
			{
				return false;
			}
			const EGameXXKNodeKind NodeKind = Node->NodeKind;
			if (!HUD->HandleDemoCommand(MakeRouteNodeCommandName(Node->NodeId)))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle && !HUD->HandleDemoCommand(FName(TEXT("ResolveBattleVictory"))))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteEvent && !HUD->HandleDemoCommand(FName(TEXT("ResolveEventGold"))))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteCamp && !HUD->HandleDemoCommand(FName(TEXT("ResolveCampHeal"))))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteMerchant && !HUD->HandleDemoCommand(FName(TEXT("CompleteMerchantNode"))))
			{
				return false;
			}
			if (NodeKind == TargetKind)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPPlayableHUDTest,
	"GameXXK.MVP.PlayableShell.HUDCommandsDriveFullLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPPlayableHUDTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	const FString TestSlotName = TEXT("GameXXK_MVP_Automation_PlayableHUD");
	HUD->SetStartGameSlotForTest(TestSlotName, 0);
	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);

	TestTrue(TEXT("main menu exposes start command"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("StartGame"))));
	TestTrue(TEXT("start command opens world map"), HUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("screen after start"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestFalse(TEXT("start command does not create a manual save slot"), UGameplayStatics::DoesSaveGameExist(TestSlotName, 0));
	TestTrue(TEXT("world map exposes manual save command"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SaveGame"))));

	bool bTanjiangEnabled = true;
	TestTrue(TEXT("world map shows locked Tanjiang button"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SelectTanjiang")), &bTanjiangEnabled));
	TestFalse(TEXT("Tanjiang is disabled before Boss"), bTanjiangEnabled);
	TestFalse(TEXT("locked Tanjiang command is rejected"), HUD->HandleDemoCommand(FName(TEXT("SelectTanjiang"))));
	TestTrue(TEXT("Qingshan command enters town"), HUD->HandleDemoCommand(FName(TEXT("SelectQingshan"))));
	TestEqual(TEXT("screen after Qingshan"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("entering town does not autosave playable slot"), UGameplayStatics::DoesSaveGameExist(TestSlotName, 0));
	TestTrue(TEXT("town exposes manual save command"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SaveGame"))));

	bool bEnterDungeonEnabled = true;
	TestTrue(TEXT("town shows dungeon gate before quest"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("EnterDungeon")), &bEnterDungeonEnabled));
	TestFalse(TEXT("dungeon gate disabled before quest"), bEnterDungeonEnabled);
	TestFalse(TEXT("dungeon command rejected before quest"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestFalse(TEXT("town HUD does not expose route quest acceptance because F on the quest NPC owns that flow"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("AcceptQuest"))));
	TestFalse(TEXT("town HUD rejects route quest acceptance because F on the quest NPC owns that flow"), HUD->HandleDemoCommand(FName(TEXT("AcceptQuest"))));
	TestTrue(TEXT("test flow marks the route quest accepted after the NPC interaction path"), Subsystem->AcceptQuest());
	TestTrue(TEXT("follower joins after NPC quest acceptance"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldBeforeTrade = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("town HUD buys healing powder"), HUD->HandleDemoCommand(FName(TEXT("BuyHealingPowder"))));
	TestEqual(TEXT("buy spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("town HUD sells healing powder"), HUD->HandleDemoCommand(FName(TEXT("SellHealingPowder"))));
	TestEqual(TEXT("sell refunds gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 5);
	TestFalse(TEXT("town trade commands do not autosave playable slot"), UGameplayStatics::DoesSaveGameExist(TestSlotName, 0));
	bool bUseHealingEnabled = true;
	const int32 HealingBeforeDisabledUse = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("town HUD shows healing command at full HP"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("UseHealingPowder")), &bUseHealingEnabled));
	TestFalse(TEXT("healing command disabled at full HP"), bUseHealingEnabled);
	TestFalse(TEXT("disabled healing command cannot execute"), HUD->HandleDemoCommand(FName(TEXT("UseHealingPowder"))));
	TestEqual(TEXT("disabled healing command keeps inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingBeforeDisabledUse);

	TestTrue(TEXT("accepted quest enters dungeon map"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestEqual(TEXT("screen after dungeon gate"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	const TArray<FGameXXKMVPRouteNodeDescriptor> RouteNodes = GameXXKMVPCommandRouter::BuildRouteMapNodes(Subsystem);
	TestTrue(TEXT("dungeon route map shows generated Slay-the-Spire nodes"), RouteNodes.Num() >= 15);
	if (RouteNodes.Num() >= 3)
	{
		TestEqual(TEXT("route start node command"), RouteNodes[0].CommandName, FName(TEXT("RouteNode0")));
		TestTrue(TEXT("route start node is clickable"), RouteNodes[0].bEnabled);
		TestEqual(TEXT("route battle node command"), RouteNodes[1].CommandName, FName(TEXT("RouteNode1")));
		TestFalse(TEXT("route battle node waits for start node"), RouteNodes[1].bEnabled);
		TestEqual(TEXT("route boss node kind"), RouteNodes.Last().NodeKind, EGameXXKNodeKind::Boss);
		TestFalse(TEXT("route boss node waits for earlier nodes"), RouteNodes.Last().bEnabled);
	}
	TestTrue(TEXT("dungeon HUD advances start node"), HUD->HandleDemoCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("dungeon HUD opens battle node"), HUD->HandleDemoCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("battle screen visible"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle HUD failure returns to town"), HUD->HandleDemoCommand(FName(TEXT("FailBattle"))));
	TestEqual(TEXT("failure command returns town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("follower remains after HUD failure"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestTrue(TEXT("retry enters dungeon after HUD failure"), HUD->HandleDemoCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("retry advances start node"), HUD->HandleDemoCommand(FName(TEXT("SelectStart"))));
	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("HUD follows generated route to a camp"), HandleRouteTowardKind(HUD, Subsystem, EGameXXKNodeKind::Camp));
	TestEqual(TEXT("camp restored HP"), Subsystem->GetRuntimeState().PlayerHP, Subsystem->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("HUD follows generated route to boss and clears it"), HandleRouteTowardKind(HUD, Subsystem, EGameXXKNodeKind::Boss));
	TestEqual(TEXT("boss returns to world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);

	const FGameXXKRuntimeState CompletedRouteState = Subsystem->GetRuntimeState();
	UGameXXKMVPSubsystem* ReloadedBeforeManualSave = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestFalse(TEXT("HUD route clear does not autosave playable slot"), ReloadedBeforeManualSave->LoadGameFromSlot(TestSlotName, 0));
	TestTrue(TEXT("world map after route clear still exposes manual save"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SaveGame"))));
	TestTrue(TEXT("HUD manual save writes playable slot"), HUD->HandleDemoCommand(FName(TEXT("SaveGame"))));
	UGameXXKMVPSubsystem* ReloadedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("HUD manual save slot loads"), ReloadedSubsystem->LoadGameFromSlot(TestSlotName, 0));
	TestEqual(TEXT("manual save persists completed quest"), ReloadedSubsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Completed);
	TestTrue(TEXT("manual save persists Tanjiang unlock"), ReloadedSubsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("manual save persists route level"), ReloadedSubsystem->GetRuntimeState().PlayerLevel, CompletedRouteState.PlayerLevel);
	TestEqual(TEXT("manual save persists route XP"), ReloadedSubsystem->GetRuntimeState().PlayerXP, CompletedRouteState.PlayerXP);
	TestEqual(TEXT("manual save persists route gold"), ReloadedSubsystem->GetRuntimeState().PlayerGold, CompletedRouteState.PlayerGold);

	bTanjiangEnabled = false;
	TestTrue(TEXT("world map still shows Tanjiang button"), HasCommand(HUD->BuildVisibleCommands(), FName(TEXT("SelectTanjiang")), &bTanjiangEnabled));
	TestTrue(TEXT("Tanjiang enabled after Boss"), bTanjiangEnabled);
	TestTrue(TEXT("Tanjiang command enters next town"), HUD->HandleDemoCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("Tanjiang opens town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	UGameplayStatics::DeleteGameInSlot(TestSlotName, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMVPPlayableGameModeTest,
	"GameXXK.MVP.PlayableShell.GameModeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMVPPlayableGameModeTest::RunTest(const FString& Parameters)
{
	AGameXXKMVPGameMode* GameMode = NewObject<AGameXXKMVPGameMode>();
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();

	TestTrue(TEXT("default player controller is MVP controller"), GameMode->PlayerControllerClass.Get() == AGameXXKMVPPlayerController::StaticClass());
	TestTrue(TEXT("MVP controller shows mouse cursor for clickable Start"), PlayerController->bShowMouseCursor);
	TestTrue(TEXT("MVP controller enables HUD hitbox click events for Start"), PlayerController->bEnableClickEvents);
	TestTrue(TEXT("MVP controller enables hover hit testing for visible commands"), PlayerController->bEnableMouseOverEvents);
	TestTrue(TEXT("default HUD is MVP HUD"), GameMode->HUDClass.Get() == AGameXXKMVPHUD::StaticClass());
	const FString HeroCharacterGeneratedClassPath = TEXT("/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C");
	TestNotNull(TEXT("default pawn class is configured"), GameMode->DefaultPawnClass.Get());
	TestEqual(TEXT("default pawn uses editable BP_HeroCharacter"), GameMode->DefaultPawnClass ? GameMode->DefaultPawnClass->GetPathName() : FString(), HeroCharacterGeneratedClassPath);
	TestTrue(TEXT("default pawn is a Character blueprint class"), GameMode->DefaultPawnClass && GameMode->DefaultPawnClass->IsChildOf(ACharacter::StaticClass()));
	TestFalse(TEXT("default pawn is not the pure C++ town pawn"), GameMode->DefaultPawnClass.Get() == AGameXXKTownPlayerPawn::StaticClass());
	TestEqual(TEXT("project global default GameMode resolves MVP class"), UGameMapsSettings::GetGlobalDefaultGameMode(), FString(TEXT("/Script/GameXXK.GameXXKMVPGameMode")));
	TestEqual(TEXT("playable shell defines town NPC spawn roles"), GameMode->GetConfiguredTownNpcCount(), 3);
	TestEqual(TEXT("playable shell defines one in-world town route entrance"), GameMode->GetConfiguredTownExitCount(), 1);
	TestTrue(TEXT("playable shell places the town route entrance at the north dungeon gate"),
		GameMode->GetConfiguredTownExitLocation().Equals(FVector(0.0f, 1380.0f, 120.0f), 0.1f));
	TestTrue(TEXT("quest/follower NPC class is a hero-style Character blueprint"),
		GameMode->GetPersonTownNpcVisualClass() && GameMode->GetPersonTownNpcVisualClass()->IsChildOf(AGameXXKHeroCharacter::StaticClass()));
	TestTrue(TEXT("merchant NPC class is a hero-style Character blueprint"),
		GameMode->GetMerchantTownNpcVisualClass() && GameMode->GetMerchantTownNpcVisualClass()->IsChildOf(AGameXXKHeroCharacter::StaticClass()));
	TestTrue(TEXT("quest/follower NPC class can be interacted with directly as an NPC body"),
		GameMode->GetPersonTownNpcVisualClass() && GameMode->GetPersonTownNpcVisualClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()));
	TestTrue(TEXT("merchant NPC class can be interacted with directly as an NPC body"),
		GameMode->GetMerchantTownNpcVisualClass() && GameMode->GetMerchantTownNpcVisualClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()));
	UGameInstance* QuestNpcRestoreGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* QuestNpcRestoreSubsystem = NewObject<UGameXXKMVPSubsystem>(QuestNpcRestoreGameInstance);
	FGameXXKRuntimeState& QuestNpcRestoreState = QuestNpcRestoreSubsystem->GetMutableRuntimeState();
	const FVector SavedQuestNpcLocation(410.0f, -135.0f, 72.0f);
	QuestNpcRestoreState.QuestState = EGameXXKQuestState::Accepted;
	QuestNpcRestoreState.bFollowerJoined = true;
	QuestNpcRestoreState.bHasQuestNpcLocation = true;
	QuestNpcRestoreState.QuestNpcLocation = SavedQuestNpcLocation;
	AGameXXKTownNpcCharacter* RestoredQuestNpc = NewObject<AGameXXKTownNpcCharacter>();
	AGameXXKHeroCharacter* RestoredQuestTarget = NewObject<AGameXXKHeroCharacter>();
	RestoredQuestNpc->SetActorLocation(FVector(260.0f, -120.0f, 72.0f));
	GameMode->ApplyQuestNpcRuntimeState(RestoredQuestNpc, QuestNpcRestoreSubsystem, RestoredQuestTarget);
	TestTrue(TEXT("GameMode applies saved quest NPC location"), RestoredQuestNpc->GetActorLocation().Equals(SavedQuestNpcLocation, 0.1f));
	TestTrue(TEXT("GameMode reactivates saved quest NPC follower"), RestoredQuestNpc->IsFollowerActive());
	TestTrue(TEXT("GameMode restores quest NPC follow target"), RestoredQuestNpc->GetFollowTarget() == RestoredQuestTarget);
	const FVector SavedPlayerLocation(512.0f, -256.0f, 120.0f);
	QuestNpcRestoreState.Screen = EGameXXKScreen::Town;
	QuestNpcRestoreState.bHasPlayerLocation = true;
	QuestNpcRestoreState.PlayerLocation = SavedPlayerLocation;
	AGameXXKHeroCharacter* RestoredPlayerPawn = NewObject<AGameXXKHeroCharacter>();
	RestoredPlayerPawn->SetActorLocation(FVector::ZeroVector);
	GameMode->ApplyPlayerRuntimeState(RestoredPlayerPawn, QuestNpcRestoreSubsystem);
	TestTrue(TEXT("GameMode applies saved player location"), RestoredPlayerPawn->GetActorLocation().Equals(SavedPlayerLocation, 0.1f));

	UGameXXKMVPSubsystem* NotAcceptedQuestNpcSubsystem = NewObject<UGameXXKMVPSubsystem>(QuestNpcRestoreGameInstance);
	FGameXXKRuntimeState& NotAcceptedQuestNpcState = NotAcceptedQuestNpcSubsystem->GetMutableRuntimeState();
	NotAcceptedQuestNpcState.QuestState = EGameXXKQuestState::NotAccepted;
	NotAcceptedQuestNpcState.bFollowerJoined = true;
	NotAcceptedQuestNpcState.bHasQuestNpcLocation = true;
	NotAcceptedQuestNpcState.QuestNpcLocation = SavedQuestNpcLocation;
	AGameXXKTownNpcCharacter* NotAcceptedQuestNpc = NewObject<AGameXXKTownNpcCharacter>();
	const FVector DefaultQuestNpcLocation(260.0f, -120.0f, 72.0f);
	NotAcceptedQuestNpc->SetActorLocation(DefaultQuestNpcLocation);
	GameMode->ApplyQuestNpcRuntimeState(NotAcceptedQuestNpc, NotAcceptedQuestNpcSubsystem, RestoredQuestTarget);
	TestTrue(TEXT("GameMode keeps not-accepted quest NPC at default location"), NotAcceptedQuestNpc->GetActorLocation().Equals(DefaultQuestNpcLocation, 0.1f));
	TestFalse(TEXT("GameMode does not activate not-accepted quest NPC follower even if a stale flag exists"), NotAcceptedQuestNpc->IsFollowerActive());

	return true;
}

#endif
