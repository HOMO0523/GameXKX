#include "GameXXKMVPRules.h"
#include "Components/Button.h"
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootWidgetTest,
	"GameXXK.MVP.PIE.MainMenuContinueWorldMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootWidgetTest::RunTest(const FString& Parameters)
{
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* RootWidget = NewObject<UGameXXKPlayableRootWidget>();
	RootWidget->SetMVPSubsystem(Subsystem);
	RootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);

	TestEqual(TEXT("playable root starts on main menu"), RootWidget->GetCurrentScreen(), EGameXXKScreen::MainMenu);
	TestEqual(TEXT("main menu has New Game, Continue, five slots, and five deletes"), RootWidget->BuildVisibleCommands().Num(), 12);
	TestTrue(TEXT("UMG New Game command is available"), RootWidget->HasVisibleCommand(FName(TEXT("StartGame")), true));
	TestTrue(TEXT("UMG Continue command is disabled without a save"), RootWidget->HasVisibleCommand(FName(TEXT("ContinueGame")), false));
	TestTrue(TEXT("UMG continue slot 1 is disabled without a save"), RootWidget->HasVisibleCommand(FName(TEXT("ContinueSlot1")), false));
	TestTrue(TEXT("UMG delete slot 1 is disabled without a save"), RootWidget->HasVisibleCommand(FName(TEXT("DeleteSlot1")), false));
	TestTrue(TEXT("UMG New Game opens world map"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("UMG root updates to world map after start"), RootWidget->GetCurrentScreen(), EGameXXKScreen::WorldMap);
	TestFalse(TEXT("UMG New Game does not create a manual save slot"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));
	TestTrue(TEXT("world map exposes Qingshan button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectQingshan")), true));
	TestTrue(TEXT("world map exposes locked Tanjiang button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));
	TestTrue(TEXT("world map exposes manual save button"), RootWidget->HasVisibleCommand(FName(TEXT("SaveGame")), true));
	TestTrue(TEXT("world map exposes save slot 1 button"), RootWidget->HasVisibleCommand(FName(TEXT("SaveSlot1")), true));
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
	const FString DefaultSlot = UGameXXKMVPSubsystem::GetDefaultSaveSlotName();
	UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SeedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("seed slot starts game"), SeedSubsystem->StartGame());
	SeedSubsystem->GetMutableRuntimeState().QuestState = EGameXXKQuestState::Completed;
	SeedSubsystem->GetMutableRuntimeState().UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	TestTrue(TEXT("seed slot saves Tanjiang unlock"), SeedSubsystem->SaveCurrentGame(PlayableRootTestSlot, PlayableRootUserIndex));

	UGameXXKMVPSubsystem* RoutedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(RoutedSubsystem);
	HUD->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* HUDRootWidget = HUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for dynamic button test"), HUDRootWidget);
	if (!HUDRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("HUD-created UMG root initializes widget tree"), HUDRootWidget->Initialize());
	HUDRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD-created UMG root Continue button uses HUD slot override"), ClickProgrammaticRootCommand(HUDRootWidget, 1));
	TestEqual(TEXT("HUD-created UMG root restores through seeded slot"), RoutedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("HUD-created UMG root restores Tanjiang unlock from seeded slot"), HUDRootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), true));

	UGameXXKMVPSubsystem* RefreshSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* RefreshHUD = NewObject<AGameXXKMVPHUD>();
	RefreshHUD->SetMVPSubsystemForTest(RefreshSubsystem);
	RefreshHUD->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* RefreshRootWidget = RefreshHUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for refresh test"), RefreshRootWidget);
	if (!RefreshRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("refresh UMG root initializes widget tree"), RefreshRootWidget->Initialize());
	RefreshRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD command starts game"), RefreshHUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestTrue(TEXT("HUD command refreshes UMG root to Qingshan button"), ClickProgrammaticRootCommand(RefreshRootWidget, 0));
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
		UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("manual save failure root initializes widget tree"), ManualSaveFailureRootWidget->Initialize());
	ManualSaveFailureRootWidget->NativeConstruct();
	TestTrue(TEXT("HUD command starts even when manual save slot override is invalid"), ManualSaveFailureHUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("manual save failure command changes subsystem screen"), ManualSaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestFalse(TEXT("HUD manual save command reports invalid slot failure"), ManualSaveFailureHUD->HandleDemoCommand(FName(TEXT("SaveGame"))));
	TestTrue(TEXT("HUD refreshes UMG root after manual save failure"), ClickProgrammaticRootCommand(ManualSaveFailureRootWidget, 0));
	TestEqual(TEXT("manual-save-failure refreshed root click enters Qingshan town"), ManualSaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("manual-save-failure refreshed root click selects Qingshan"), ManualSaveFailureSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());

	UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
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

	TestTrue(TEXT("UMG root starts into world map"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestTrue(TEXT("UMG root enters Qingshan town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectQingshan"))));
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
	TestTrue(TEXT("UMG root buys healing powder"), RootWidget->ExecuteVisibleCommand(FName(TEXT("BuyHealingPowder"))));
	TestEqual(TEXT("UMG root buy spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 10);
	TestTrue(TEXT("UMG root sells healing powder"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SellHealingPowder"))));
	TestEqual(TEXT("UMG root sell refunds gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeTrade - 5);

	TestTrue(TEXT("UMG root enters dungeon map after quest"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("UMG root advances start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("UMG root opens battle node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("UMG root reaches battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("UMG root fails battle back to town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("FailBattle"))));
	TestEqual(TEXT("UMG root failure returns to town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	const int32 GoldAfterFailure = Subsystem->GetRuntimeState().PlayerGold;
	const int32 HealingPowderAfterFailure = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("UMG root resupplies after failure"), RootWidget->ExecuteVisibleCommand(FName(TEXT("BuyHealingPowder"))));
	TestEqual(TEXT("post-failure resupply spends gold"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFailure - 10);
	TestEqual(TEXT("post-failure resupply adds healing powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingPowderAfterFailure + 1);
	Subsystem->GetMutableRuntimeState().PlayerHP = 40;
	TestTrue(TEXT("UMG root exposes post-failure healing item"), RootWidget->HasVisibleCommand(FName(TEXT("UseHealingPowder")), true));
	TestTrue(TEXT("UMG root uses post-failure resupply before retry"), RootWidget->ExecuteVisibleCommand(FName(TEXT("UseHealingPowder"))));
	TestTrue(TEXT("post-failure healing restores HP before retry"), Subsystem->GetRuntimeState().PlayerHP > 40);
	TestEqual(TEXT("post-failure healing consumes one powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), HealingPowderAfterFailure);

	TestTrue(TEXT("UMG root retries dungeon"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("UMG root retries start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("UMG root retries battle node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBattle"))));
	TestTrue(TEXT("UMG root wins normal battle"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory"))));
	TestTrue(TEXT("UMG root resolves event reward"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveEventGold"))));
	Subsystem->GetMutableRuntimeState().PlayerHP = 1;
	TestTrue(TEXT("UMG root resolves camp heal"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveCampHeal"))));
	TestEqual(TEXT("UMG root camp restores HP"), Subsystem->GetRuntimeState().PlayerHP, Subsystem->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("UMG root opens boss node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBoss"))));
	TestEqual(TEXT("UMG root reaches boss battle"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("UMG root resolves boss victory"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory"))));
	TestEqual(TEXT("UMG root returns to world map after boss"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("UMG root enables Tanjiang after boss"), RootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), true));
	TestFalse(TEXT("UMG root route clear waits for manual save"), UGameplayStatics::DoesSaveGameExist(PlayableRootTestSlot, PlayableRootUserIndex));
	TestTrue(TEXT("UMG root manual save after route clear succeeds"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SaveGame"))));
	TestTrue(TEXT("UMG root enters Tanjiang after unlock"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("UMG root opens next town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

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
	TestTrue(TEXT("reloaded UMG root exposes restored Tanjiang command"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), true));
	TestTrue(TEXT("reloaded UMG root enters restored Tanjiang town"), ReloadedRootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("reloaded UMG root opens restored next town"), ReloadedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("reloaded UMG root selects restored Tanjiang region"), ReloadedSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionTanjiang());

	UGameXXKMVPSubsystem* ManualSlotSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* ManualSlotRootWidget = NewObject<UGameXXKPlayableRootWidget>();
	ManualSlotRootWidget->SetMVPSubsystem(ManualSlotSubsystem);
	TestTrue(TEXT("manual slot root starts new game"), ManualSlotRootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestTrue(TEXT("manual slot root saves slot 1"), ManualSlotRootWidget->ExecuteVisibleCommand(FName(TEXT("SaveSlot1"))));
	ManualSlotSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::MainMenu;
	ManualSlotRootWidget->RefreshFromState();
	TestTrue(TEXT("manual slot root can delete slot 1 after save"), ManualSlotRootWidget->ExecuteVisibleCommand(FName(TEXT("DeleteSlot1"))));
	ManualSlotRootWidget->RefreshFromState();
	TestTrue(TEXT("manual slot root disables deleted continue slot 1"), ManualSlotRootWidget->HasVisibleCommand(FName(TEXT("ContinueSlot1")), false));

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	UGameplayStatics::DeleteGameInSlot(UGameXXKMVPSubsystem::GetManualSaveSlotName(0), PlayableRootUserIndex);
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

	TestTrue(TEXT("post-failure test starts into world map"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestTrue(TEXT("post-failure test enters Qingshan town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectQingshan"))));
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
	TestTrue(TEXT("post-failure test buys healing powder after return"), RootWidget->ExecuteVisibleCommand(FName(TEXT("BuyHealingPowder"))));
	TestEqual(TEXT("post-failure test buy spends town gold"), Subsystem->GetRuntimeState().PlayerGold, GoldAfterFailure - 10);
	TestEqual(TEXT("post-failure test buy adds one powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderAfterFailure + 1);

	Subsystem->GetMutableRuntimeState().PlayerHP = 40;
	TestTrue(TEXT("post-failure test exposes resupplied healing command"), RootWidget->HasVisibleCommand(FName(TEXT("UseHealingPowder")), true));
	TestTrue(TEXT("post-failure test uses resupplied healing before retry"), RootWidget->ExecuteVisibleCommand(FName(TEXT("UseHealingPowder"))));
	TestTrue(TEXT("post-failure test restores HP before retry"), Subsystem->GetRuntimeState().PlayerHP > 40);
	TestEqual(TEXT("post-failure test consumes resupplied powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderAfterFailure);

	TestTrue(TEXT("post-failure test retries dungeon"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("post-failure test retries start node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectStart"))));
	TestTrue(TEXT("post-failure test retries battle node"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBattle"))));
	TestEqual(TEXT("post-failure test reaches retry battle"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("post-failure test wins retry battle"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory"))));
	TestTrue(TEXT("post-failure test resolves retry event"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveEventGold"))));
	TestTrue(TEXT("post-failure test resolves retry camp"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveCampHeal"))));
	TestTrue(TEXT("post-failure test selects retry boss"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectBoss"))));
	TestTrue(TEXT("post-failure test clears retry boss"), RootWidget->ExecuteVisibleCommand(FName(TEXT("ResolveBattleVictory"))));
	TestEqual(TEXT("post-failure test boss clear returns to world map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("post-failure test completes quest after retry"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Completed);
	TestTrue(TEXT("post-failure test unlocks Tanjiang after retry"), Subsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

#endif
