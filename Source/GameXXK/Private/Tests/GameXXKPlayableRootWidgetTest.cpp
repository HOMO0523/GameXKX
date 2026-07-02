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
	TestEqual(TEXT("main menu has one visible UMG command"), RootWidget->BuildVisibleCommands().Num(), 1);
	TestTrue(TEXT("UMG start/continue command is available"), RootWidget->HasVisibleCommand(FName(TEXT("StartGame")), true));
	TestTrue(TEXT("UMG Start/Continue opens world map"), RootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("UMG root updates to world map after start"), RootWidget->GetCurrentScreen(), EGameXXKScreen::WorldMap);
	TestTrue(TEXT("world map exposes Qingshan button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectQingshan")), true));
	TestTrue(TEXT("world map exposes locked Tanjiang button"), RootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), false));
	TestFalse(TEXT("locked Tanjiang UMG button cannot execute"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestTrue(TEXT("Qingshan UMG button enters town"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectQingshan"))));
	TestEqual(TEXT("Qingshan UMG click opens town"), RootWidget->GetCurrentScreen(), EGameXXKScreen::Town);

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
	TestTrue(TEXT("HUD-created UMG root Start button uses HUD slot override"), ClickProgrammaticRootCommand(HUDRootWidget, 0));
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

	const FString InvalidAutosaveSlot = FString::ChrN(320, TEXT('X'));
	UGameXXKMVPSubsystem* SaveProbeSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestFalse(TEXT("invalid autosave slot fails SaveGameToSlot"), SaveProbeSubsystem->SaveCurrentGame(InvalidAutosaveSlot, PlayableRootUserIndex));

	UGameXXKMVPSubsystem* AutosaveFailureSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPHUD* AutosaveFailureHUD = NewObject<AGameXXKMVPHUD>();
	AutosaveFailureHUD->SetMVPSubsystemForTest(AutosaveFailureSubsystem);
	AutosaveFailureHUD->SetStartGameSlotForTest(InvalidAutosaveSlot, PlayableRootUserIndex);
	UGameXXKPlayableRootWidget* AutosaveFailureRootWidget = AutosaveFailureHUD->CreatePlayableRootWidgetForTest();
	TestNotNull(TEXT("HUD creates playable root for autosave failure refresh test"), AutosaveFailureRootWidget);
	if (!AutosaveFailureRootWidget)
	{
		UGameplayStatics::DeleteGameInSlot(DefaultSlot, PlayableRootUserIndex);
		UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
		return false;
	}

	TestTrue(TEXT("autosave failure root initializes widget tree"), AutosaveFailureRootWidget->Initialize());
	AutosaveFailureRootWidget->NativeConstruct();
	TestFalse(TEXT("HUD command reports autosave failure after mutating state"), AutosaveFailureHUD->HandleDemoCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("autosave failure command still changes subsystem screen"), AutosaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("HUD refreshes UMG root even when autosave fails"), ClickProgrammaticRootCommand(AutosaveFailureRootWidget, 0));
	TestEqual(TEXT("autosave-failure refreshed root click enters Qingshan town"), AutosaveFailureSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("autosave-failure refreshed root click selects Qingshan"), AutosaveFailureSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionQingshan());

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

	TestTrue(TEXT("UMG root exposes disabled dungeon gate before quest"), RootWidget->HasVisibleCommand(FName(TEXT("EnterDungeon")), false));
	TestFalse(TEXT("UMG root rejects dungeon before quest"), RootWidget->ExecuteVisibleCommand(FName(TEXT("EnterDungeon"))));
	TestTrue(TEXT("UMG root accepts quest"), RootWidget->ExecuteVisibleCommand(FName(TEXT("AcceptQuest"))));
	TestTrue(TEXT("UMG root joins follower after quest"), Subsystem->GetRuntimeState().bFollowerJoined);

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
	TestTrue(TEXT("UMG root enters Tanjiang after unlock"), RootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("UMG root opens next town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	UGameXXKMVPSubsystem* ReloadedSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* ReloadedRootWidget = NewObject<UGameXXKPlayableRootWidget>();
	ReloadedRootWidget->SetMVPSubsystem(ReloadedSubsystem);
	ReloadedRootWidget->SetStartGameSlotForTest(PlayableRootTestSlot, PlayableRootUserIndex);
	TestTrue(TEXT("reloaded UMG root exposes start/continue command"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("StartGame")), true));
	TestTrue(TEXT("reloaded UMG root restores autosave through StartGame command"), ReloadedRootWidget->ExecuteVisibleCommand(FName(TEXT("StartGame"))));
	TestEqual(TEXT("reloaded UMG root restores progress to world map"), ReloadedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestEqual(TEXT("reloaded UMG root keeps quest complete"), ReloadedSubsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Completed);
	TestTrue(TEXT("reloaded UMG root keeps Tanjiang unlock"), ReloadedSubsystem->GetRuntimeState().UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestTrue(TEXT("reloaded UMG root exposes restored Tanjiang command"), ReloadedRootWidget->HasVisibleCommand(FName(TEXT("SelectTanjiang")), true));
	TestTrue(TEXT("reloaded UMG root enters restored Tanjiang town"), ReloadedRootWidget->ExecuteVisibleCommand(FName(TEXT("SelectTanjiang"))));
	TestEqual(TEXT("reloaded UMG root opens restored next town"), ReloadedSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestEqual(TEXT("reloaded UMG root selects restored Tanjiang region"), ReloadedSubsystem->GetRuntimeState().CurrentRegion, UGameXXKMVPRules::RegionTanjiang());

	UGameplayStatics::DeleteGameInSlot(PlayableRootTestSlot, PlayableRootUserIndex);
	return true;
}

#endif
