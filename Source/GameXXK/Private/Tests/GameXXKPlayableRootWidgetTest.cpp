#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "Components/Button.h"
#include "GameFramework/SaveGame.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKPlayableRootWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static const FString PlayableRootTestSlot(TEXT("GameXXK_MVP_Automation_PlayableRoot_Start"));
	static const int32 PlayableRootUserIndex = 0;

	struct FScopedSaveSlotBackup
	{
		FScopedSaveSlotBackup(const FString& InSlotName, int32 InUserIndex)
			: SlotName(InSlotName)
			, UserIndex(InUserIndex)
		{
			bHadExistingSave = UGameplayStatics::DoesSaveGameExist(SlotName, UserIndex);
			if (bHadExistingSave)
			{
				ExistingSave = UGameplayStatics::LoadGameFromSlot(SlotName, UserIndex);
				if (!ExistingSave)
				{
					return;
				}
				ExistingSave->AddToRoot();
			}
			bReady = true;
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
		}

		~FScopedSaveSlotBackup()
		{
			if (!bReady)
			{
				return;
			}
			UGameplayStatics::DeleteGameInSlot(SlotName, UserIndex);
			if (bHadExistingSave && ExistingSave)
			{
				UGameplayStatics::SaveGameToSlot(ExistingSave, SlotName, UserIndex);
				ExistingSave->RemoveFromRoot();
			}
		}

		FString SlotName;
		int32 UserIndex = 0;
		bool bHadExistingSave = false;
		bool bReady = false;
		TObjectPtr<USaveGame> ExistingSave;
	};

	static bool ClickProgrammaticRootCommand(UGameXXKPlayableRootWidget* RootWidget, int32 CommandIndex)
	{
		if (!RootWidget)
		{
			return false;
		}

		UButton* CommandButton = Cast<UButton>(RootWidget->GetWidgetFromName(*FString::Printf(TEXT("CommandButton%d"), CommandIndex)));
		if (!CommandButton || !CommandButton->GetIsEnabled())
		{
			return false;
		}

		CommandButton->OnClicked.Broadcast();
		return true;
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

	static bool ResolveRootBattleVictoryAndSkipRouteReward(UGameXXKPlayableRootWidget* RootWidget, UGameXXKMVPSubsystem* Subsystem)
	{
		if (!RootWidget || !Subsystem)
		{
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		if (State.Screen != EGameXXKScreen::Battle || !State.CardRun.bHasActiveCardBattle)
		{
			return false;
		}
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		if (!RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory")))
			|| State.Screen != EGameXXKScreen::Battle
			|| State.CardRun.PendingReward.Options.Num() != 3)
		{
			return false;
		}

		FString RewardError;
		return FGameXXKCardBattleAdapter::SkipPendingRouteReward(State, &RewardError)
			&& RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory")));
	}

	static bool ExecuteRootRouteTowardKind(UGameXXKPlayableRootWidget* RootWidget, UGameXXKMVPSubsystem* Subsystem, EGameXXKNodeKind TargetKind)
	{
		if (!RootWidget || !Subsystem)
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
			if (!RootWidget->ExecuteVisibleCommand(MakeRouteNodeCommandName(Node->NodeId)))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::Battle && !ResolveRootBattleVictoryAndSkipRouteReward(RootWidget, Subsystem))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteEvent)
			{
				bool bResolvedChoice = false;
				for (int32 ChoiceIndex = 0; ChoiceIndex < 3 && !bResolvedChoice; ++ChoiceIndex)
				{
					const FName ChoiceCommand(*FString::Printf(TEXT("ResolveRouteChoice%d"), ChoiceIndex));
					bResolvedChoice = RootWidget->HasVisibleCommand(ChoiceCommand, true)
						&& RootWidget->ExecuteVisibleCommand(ChoiceCommand);
				}
				if (!bResolvedChoice)
				{
					return false;
				}
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteCamp && !RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveCampHeal"))))
			{
				return false;
			}
			if (Subsystem->GetRuntimeState().Screen == EGameXXKScreen::RouteMerchant && !RootWidget->ExecuteVisibleCommand(FName(TEXT("CompleteMerchantNode"))))
			{
				return false;
			}
			if (NodeKind == TargetKind)
			{
				// The canonical route spans three chapters. A chapter boss advances to
				// the next generated map; only the terminal boss completes the route.
				if (TargetKind == EGameXXKNodeKind::Boss
					&& Subsystem->GetRuntimeState().Screen == EGameXXKScreen::DungeonMap)
				{
					continue;
				}
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootWidgetTest,
	"GameXXK.MVP.PIE.MainMenuContinueWorldMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootWidgetTest::RunTest(const FString& Parameters)
{
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	const FString ManualSlot1 = UGameXXKMVPSubsystem::GetManualSaveSlotName(0);
	FScopedSaveSlotBackup ManualSlot1Backup(ManualSlot1, PlayableRootUserIndex);
	if (!TestTrue(TEXT("playable root safely isolates the player's manual slot 1"), ManualSlot1Backup.bReady))
	{
		return false;
	}

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* RootWidget = NewObject<UGameXXKPlayableRootWidget>();
	RootWidget->SetMVPSubsystem(Subsystem);
	RootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);

	TestEqual(TEXT("playable root starts on main menu"), RootWidget->GetCurrentScreen(), EGameXXKScreen::MainMenu);
	TestEqual(TEXT("main menu has New Game, Continue, five slots, and five deletes"), RootWidget->BuildVisibleCommands().Num(), 12);
	TestTrue(TEXT("UMG New Game command is available"), RootWidget->HasVisibleCommand(FName(TEXT("StartGame")), true));
	TestTrue(TEXT("UMG Continue command is disabled without a save"), RootWidget->HasVisibleCommand(FName(TEXT("ContinueGame")), false));
	TestTrue(TEXT("UMG continue slot 1 is disabled before the isolated manual save"),
		RootWidget->HasVisibleCommand(FName(TEXT("ContinueSlot1")), false));
	TestTrue(TEXT("UMG delete slot 1 is disabled before the isolated manual save"),
		RootWidget->HasVisibleCommand(FName(TEXT("DeleteSlot1")), false));
	TestTrue(TEXT("UMG New Game opens Qingshan town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("UMG root updates to town after start"), RootWidget->GetCurrentScreen(), EGameXXKScreen::Town);
	TestEqual(TEXT("UMG root selects Qingshan after start"), Subsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());
	TestFalse(TEXT("UMG New Game does not create a manual save slot"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));
	TestTrue(TEXT("town exposes manual save button"), RootWidget->HasVisibleCommand(FName(TEXT("SaveGame")), true));
	TestTrue(TEXT("town exposes explicit world-map navigation"), RootWidget->ExecuteVisibleCommand(FName(TEXT("OpenWorldMap"))));
	TestEqual(TEXT("world-map command leaves town"), RootWidget->GetCurrentScreen(), EGameXXKScreen::WorldMap);
	TestTrue(TEXT("world map exposes Qingshan button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectQingshan")), true));
	TestTrue(TEXT("world map exposes locked Tanjiang button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));
	TestTrue(TEXT("world map exposes manual save button"), RootWidget->HasVisibleCommand(FName(TEXT("SaveGame")), true));
	TestTrue(TEXT("world map exposes save slot 1 button"), RootWidget->HasVisibleCommand(FName(TEXT("SaveSlot1")), true));
	Subsystem->GetMutableRuntimeState().PlayerGold = 321;
	TestTrue(TEXT("world map save slot 1 command writes the isolated manual slot"),
		RootWidget->ExecuteVisibleCommand(FName(TEXT("SaveSlot1"))));
	TestTrue(TEXT("manual slot 1 exists after the save command"),
		UGameplayStatics::DoesSaveGameExist(ManualSlot1, PlayableRootUserIndex));

	UGameXXKMVPSubsystem* ManualContinueSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* ManualContinueRoot = NewObject<UGameXXKPlayableRootWidget>();
	ManualContinueRoot->SetMVPSubsystem(ManualContinueSubsystem);
	ManualContinueRoot->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	TestTrue(TEXT("main menu enables continue slot 1 after the save command"),
		ManualContinueRoot->HasVisibleCommand(FName(TEXT("ContinueSlot1")), true));
	TestTrue(TEXT("main menu enables delete slot 1 after the save command"),
		ManualContinueRoot->HasVisibleCommand(FName(TEXT("DeleteSlot1")), true));
	TestTrue(TEXT("continue slot 1 command restores the isolated manual save"),
		ManualContinueRoot->ExecuteVisibleCommand(FName(TEXT("ContinueSlot1"))));
	TestEqual(TEXT("continue slot 1 restores the saved world-map screen"),
		ManualContinueSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("continue slot 1 restores the saved state payload"),
		ManualContinueSubsystem->GetRuntimeState().PlayerGold, 321);

	UGameXXKMVPSubsystem* ManualDeleteSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* ManualDeleteRoot = NewObject<UGameXXKPlayableRootWidget>();
	ManualDeleteRoot->SetMVPSubsystem(ManualDeleteSubsystem);
	ManualDeleteRoot->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	TestTrue(TEXT("delete slot 1 command removes the isolated manual save"),
		ManualDeleteRoot->ExecuteVisibleCommand(FName(TEXT("DeleteSlot1"))));
	TestFalse(TEXT("manual slot 1 is absent after the delete command"),
		UGameplayStatics::DoesSaveGameExist(ManualSlot1, PlayableRootUserIndex));
	TestTrue(TEXT("continue slot 1 disables after deletion"),
		ManualDeleteRoot->HasVisibleCommand(FName(TEXT("ContinueSlot1")), false));
	TestTrue(TEXT("delete slot 1 disables after deletion"),
		ManualDeleteRoot->HasVisibleCommand(FName(TEXT("DeleteSlot1")), false));
	TestFalse(TEXT("locked Tanjiang UMG button cannot execute"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestTrue(TEXT("Qingshan UMG button enters town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectQingshan"))));
	TestEqual(TEXT("Qingshan UMG click opens town"), RootWidget->GetCurrentScreen(), EGameXXKScreen::Town);
	TestFalse(TEXT("Qingshan UMG click does not autosave"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	UGameXXKPlayableRootWidget* HUDRootWidget = HUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates real UMG playable root widget"), HUDRootWidget);
	TestTrue(TEXT("HUD retains playable root widget"), HUD->HasPlayableRootWidget());
	TestEqual(TEXT("HUD root widget receives same MVP subsystem"), HUDRootWidget ? HUDRootWidget->GetMVPSubsystem() : nullptr, Subsystem);

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootHUDIntegrationTest,
	"GameXXK.MVP.PIE.PlayableRootHUDIntegration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootHUDIntegrationTest::RunTest(const FString& Parameters)
{
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SeedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("seed slot starts game"), SeedSubsystem->StartGame());
	SeedSubsystem->GetMutableRuntimeState().QuestState = EGameXXKQuestState::Completed;
	SeedSubsystem->GetMutableRuntimeState().UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	TestTrue(TEXT("seed slot explicitly opens the world map"), SeedSubsystem->OpenWorldMap());
	TestTrue(TEXT("seed slot saves Tanjiang unlock"), SeedSubsystem->SaveCurrentGame(PlayableRootTestSlot, PlayableRootUserIndex));

	UGameXXKMVPSubsystem* RoutedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(RoutedSubsystem);
	HUD->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* HUDRootWidget = HUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for dynamic button test"), HUDRootWidget);
	if (!HUDRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("HUD-created UMG root initializes widget tree"), HUDRootWidget->Initialize());
	HUDRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD-created UMG root Continue button uses HUD slot override"), ClickProgrammaticRootCommand(HUDRootWidget, 1));
	TestEqual(TEXT("HUD-created UMG root restores through seeded slot"), RoutedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("HUD-created UMG root restores Tanjiang unlock from seeded slot"), RoutedSubsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestTrue(TEXT("HUD-created UMG root keeps unimplemented Tanjiang disabled"), HUDRootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));

	UGameXXKMVPSubsystem* RefreshSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* RefreshHUD = NewObject<AGameXXKMVPHUD>();
	RefreshHUD->SetMVPSubsystemForTest(RefreshSubsystem);
	RefreshHUD->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* RefreshRootWidget = RefreshHUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for refresh test"), RefreshRootWidget);
	if (!RefreshRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("refresh UMG root initializes widget tree"), RefreshRootWidget->Initialize());
	RefreshRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD command starts game"), RefreshHUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("HUD start command lands directly in Qingshan town"), RefreshSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("HUD command refreshes UMG root to the town map button"), ClickProgrammaticRootCommand(RefreshRootWidget, 0));
	TestEqual(TEXT("town map button opens the world map"), RefreshSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("refreshed world map exposes the Qingshan button"), ClickProgrammaticRootCommand(RefreshRootWidget, 0));
	TestEqual(TEXT("refreshed UMG root click enters Qingshan town"), RefreshSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("refreshed UMG root click selects Qingshan"), RefreshSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());

	const FString InvalidManualSaveSlot = FString::ChrN(320, TEXT('X'));
	UGameXXKMVPSubsystem* SaveProbeSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestFalse(TEXT("invalid manual save slot fails SaveGameToSlot"), SaveProbeSubsystem->SaveCurrentGame(InvalidManualSaveSlot, PlayableRootUserIndex));

	UGameXXKMVPSubsystem* ManualSaveFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* ManualSaveFailureHUD = NewObject<AGameXXKMVPHUD>();
	ManualSaveFailureHUD->SetMVPSubsystemForTest(ManualSaveFailureSubsystem);
	ManualSaveFailureHUD->SetStartGameSlotForTest(InvalidManualSaveSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* ManualSaveFailureRootWidget = ManualSaveFailureHUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for manual save failure refresh test"), ManualSaveFailureRootWidget);
	if (!ManualSaveFailureRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("manual save failure root initializes widget tree"), ManualSaveFailureRootWidget->Initialize());
	ManualSaveFailureRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD command starts even when manual save slot override is invalid"), ManualSaveFailureHUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("manual save failure command lands directly in town"), ManualSaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("HUD manual save command reports invalid slot failure"), ManualSaveFailureHUD->HandleDemoCommand(FName(TEXT("SaveGame"))));
	TestTrue(TEXT("HUD refreshes the town map command after manual save failure"), ClickProgrammaticRootCommand(ManualSaveFailureRootWidget, 0));
	TestEqual(TEXT("manual-save-failure town map command opens world map"), ManualSaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("HUD refreshes the Qingshan command on the world map"), ClickProgrammaticRootCommand(ManualSaveFailureRootWidget, 0));
	TestEqual(TEXT("manual-save-failure refreshed root click enters Qingshan town"), ManualSaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("manual-save-failure refreshed root click selects Qingshan"), ManualSaveFailureSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootFullLoopTest,
	"GameXXK.MVP.PIE.PlayableRootCommandsDriveFullLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootFullLoopTest::RunTest(const FString& Parameters)
{
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* RootWidget = NewObject<UGameXXKPlayableRootWidget>();
	RootWidget->SetMVPSubsystem(Subsystem);
	RootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);

	TestTrue(TEXT("UMG root starts directly in Qingshan town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("UMG root start selects the town screen"), RootWidget->GetCurrentScreen(), EGameXXKScreen::Town);
	TestFalse(TEXT("UMG root does not autosave start or town entry"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));
	TestTrue(TEXT("UMG root exposes manual save in town"), RootWidget->HasVisibleCommand(FName(TEXT("SaveGame")), true));

	TestTrue(TEXT("UMG root exposes disabled dungeon gate before quest"), RootWidget->HasVisibleCommand(FName(TEXT("EnterDungeon")), false));
	TestFalse(TEXT("UMG root rejects dungeon before quest"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestFalse(TEXT("UMG root does not expose route quest acceptance because F on the quest NPC owns that flow"), RootWidget->HasVisibleCommand(FName(TEXT("AcceptQuest")), true));
	TestFalse(TEXT("UMG root rejects route quest acceptance because F on the quest NPC owns that flow"), RootWidget->ExecuteVisibleCommand(FName(TEXT("AcceptQuest"))));
	TestTrue(TEXT("test flow marks the route quest accepted after the NPC interaction path"), Subsystem->AcceptQuest());
	RootWidget->RefreshFromState();
	TestTrue(TEXT("UMG root joins follower after NPC quest acceptance"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldBeforeTrade = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("town trade flow buys healing powder"), Subsystem->BuyItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("UMG root buy spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("town trade flow sells healing powder"), Subsystem->SellItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	TestEqual(TEXT("UMG root sell refunds gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 5);
	RootWidget->RefreshFromState();

	TestTrue(TEXT("UMG root enters dungeon map after quest"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("UMG root advances start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("UMG root opens battle node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("UMG root reaches battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("UMG root fails battle back to town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("FailBattle"))));
	TestEqual(TEXT("UMG root failure returns to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	const int32 GoldAfterFailure = Subsystem->GetRuntimeState().PlayerGold;
	const int32 HealingPowderAfterFailure = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("town trade flow resupplies after failure"), Subsystem->BuyItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	RootWidget->RefreshFromState();
	TestEqual(TEXT("post-failure resupply spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFailure - 10);
	TestEqual(TEXT("post-failure resupply adds healing powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingPowderAfterFailure + 1);
	Subsystem->GetMutableRuntimeState().PlayerHP = 40;
	TestTrue(TEXT("UMG root exposes post-failure healing item"), RootWidget->HasVisibleCommand(FName(TEXT("UseHealingPowder")), true));
	TestTrue(TEXT("UMG root uses post-failure resupply before retry"), RootWidget->ExecuteVisibleCommand(FName(TEXT("UseHealingPowder"))));
	TestTrue(TEXT("post-failure healing restores HP before retry"), Subsystem->GetRuntimeState().PlayerHP > 40);
	TestEqual(TEXT("post-failure healing consumes one powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingPowderAfterFailure);

	TestTrue(TEXT("UMG root retries dungeon"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("UMG root retries start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("UMG root follows generated route to camp"), ExecuteRootRouteTowardKind(RootWidget, Subsystem, EGameXXKNodeKind::Camp));
	TestEqual(TEXT("UMG root camp restores HP"), Subsystem->GetRuntimeState().PlayerHP, Subsystem->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("UMG root follows generated route to boss and clears it"), ExecuteRootRouteTowardKind(RootWidget, Subsystem, EGameXXKNodeKind::Boss));
	TestEqual(TEXT("UMG root returns to world map after boss"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("UMG root keeps unimplemented Tanjiang disabled after boss"), RootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));
	TestFalse(TEXT("UMG root route clear waits for manual save"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));
	TestTrue(TEXT("UMG root manual save after route clear succeeds"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SaveGame"))));
	TestFalse(TEXT("UMG root rejects unimplemented Tanjiang after unlock"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("UMG root remains on world map after unavailable Tanjiang"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);

	UGameXXKMVPSubsystem* ReloadedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* ReloadedRootWidget = NewObject<UGameXXKPlayableRootWidget>();
	ReloadedRootWidget->SetMVPSubsystem(ReloadedSubsystem);
	ReloadedRootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	TestTrue(TEXT("reloaded UMG root exposes New Game command"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("StartGame")), true));
	TestTrue(TEXT("reloaded UMG root exposes Continue command"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("ContinueGame")), true));
	TestTrue(TEXT("reloaded UMG root restores manual save through Continue command"), ReloadedRootWidget->ExecuteVisibleCommand(FName(TEXT("ContinueGame"))));
	TestEqual(TEXT("reloaded UMG root restores progress to world map"), ReloadedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("reloaded UMG root keeps quest complete"), ReloadedSubsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Completed);
	TestTrue(TEXT("reloaded UMG root keeps Tanjiang unlock"), ReloadedSubsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestTrue(TEXT("reloaded UMG root keeps restored Tanjiang command disabled"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));
	TestFalse(TEXT("reloaded UMG root rejects restored Tanjiang until its map exists"), ReloadedRootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("reloaded UMG root remains on world map after unavailable Tanjiang"), ReloadedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("reloaded UMG root keeps world-map selection clear after unavailable Tanjiang"), ReloadedSubsystem->GetRuntimeState().CurrentRegion, NAME_None);

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootPostFailureResupplyRetryTest,
	"GameXXK.MVP.PIE.PlayableRootPostFailureResupplyRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootPostFailureResupplyRetryTest::RunTest(const FString& Parameters)
{
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* RootWidget = NewObject<UGameXXKPlayableRootWidget>();
	RootWidget->SetMVPSubsystem(Subsystem);
	RootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);

	TestTrue(TEXT("post-failure test starts directly in Qingshan town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("post-failure test begins on the town screen"), RootWidget->GetCurrentScreen(), EGameXXKScreen::Town);
	TestFalse(TEXT("post-failure UMG root rejects route quest acceptance because F on the quest NPC owns that flow"), RootWidget->ExecuteVisibleCommand(FName(TEXT("AcceptQuest"))));
	TestTrue(TEXT("post-failure test marks the route quest accepted after the NPC interaction path"), Subsystem->AcceptQuest());
	RootWidget->RefreshFromState();
	TestTrue(TEXT("post-failure test follower joins before challenge"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestTrue(TEXT("post-failure test enters dungeon"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("post-failure test selects start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("post-failure test selects battle node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("post-failure test reaches battle"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("post-failure test fails battle"), RootWidget->ExecuteVisibleCommand(FName(TEXT("FailBattle"))));
	TestEqual(TEXT("post-failure test returns to town after failure"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestTrue(TEXT("post-failure test keeps follower after failure"), Subsystem->GetRuntimeState().bFollowerJoined);

	const int32 GoldAfterFailure = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderAfterFailure = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("post-failure town trade buys healing powder after return"), Subsystem->BuyItem(UGameXXKMVPRules::ItemHealingPowder(), 1));
	RootWidget->RefreshFromState();
	TestEqual(TEXT("post-failure test buy spends town gold"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFailure - 10);
	TestEqual(TEXT("post-failure test buy adds one powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderAfterFailure + 1);

	Subsystem->GetMutableRuntimeState().PlayerHP = 40;
	TestTrue(TEXT("post-failure test exposes resupplied healing command"), RootWidget->HasVisibleCommand(FName(TEXT("UseHealingPowder")), true));
	TestTrue(TEXT("post-failure test uses resupplied healing before retry"), RootWidget->ExecuteVisibleCommand(FName(TEXT("UseHealingPowder"))));
	TestTrue(TEXT("post-failure test restores HP before retry"), Subsystem->GetRuntimeState().PlayerHP > 40);
	TestEqual(TEXT("post-failure test consumes resupplied powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderAfterFailure);

	TestTrue(TEXT("post-failure test retries dungeon"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("post-failure test retries start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("post-failure test follows generated route to camp"), ExecuteRootRouteTowardKind(RootWidget, Subsystem, EGameXXKNodeKind::Camp));
	TestEqual(TEXT("post-failure test camp restores HP"), Subsystem->GetRuntimeState().PlayerHP, Subsystem->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("post-failure test follows generated route to boss and clears it"), ExecuteRootRouteTowardKind(RootWidget, Subsystem, EGameXXKNodeKind::Boss));
	TestEqual(TEXT("post-failure test boss clear returns to world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("post-failure test completes quest after retry"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Completed);
	TestTrue(TEXT("post-failure test unlocks Tanjiang after retry"), Subsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

#endif
