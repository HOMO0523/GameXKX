#include "InputKeyEventArgs.h"
#include "Components/Button.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCompanionRosterWidget.h"
#include "UI/GameXXKTownHudWidget.h"
#include "GameXXKMVPRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionRosterPlayerFlowTest,
	"GameXXK.MVP.UI.CompanionRosterPlayerFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionRosterPlayerFlowTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	AGameXXKMVPPlayerController* PlayerController = NewObject<AGameXXKMVPPlayerController>();
	PlayerController->SetMVPSubsystemForTest(Subsystem);

	TestTrue(TEXT("controller builds the isolated permanent-companion roster widget"), PlayerController->EnsurePlayerFlowWidgetsForTest());
	UGameXXKCompanionRosterWidget* RosterWidget = PlayerController->GetCompanionRosterWidgetForTest();
	TestNotNull(TEXT("controller owns the permanent companion roster widget"), RosterWidget);
	TestTrue(TEXT("the roster is not left visible outside town by construction"),
		RosterWidget && RosterWidget->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("the roster cannot open from the initial main menu"), PlayerController->OpenCompanionRoster());

	TestTrue(TEXT("new game opens the world map"), Subsystem->StartGame());
	TestTrue(TEXT("the normal world-map selection enters town"),
		UGameXXKMVPRules::EnterWorldRegion(Subsystem->GetMutableRuntimeState(), UGameXXKMVPRules::RegionQingshan()));
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestEqual(TEXT("fixture is in town before opening the roster"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	UGameXXKTownHudWidget* TownHud = PlayerController->GetTownHudWidgetForTest();
	TestNotNull(TEXT("the independent codex HUD remains available"), TownHud);
	TestTrue(TEXT("the existing task-NPC codex can open before roster routing"), TownHud && TownHud->OpenCompanionCodexForTest());
	UButton* RosterEntryButton = TownHud ? Cast<UButton>(TownHud->GetWidgetFromName(TEXT("TownHudCompanionRoster"))) : nullptr;
	TestNotNull(TEXT("town HUD adds a distinct PSD-backed permanent-companion backpack entry"), RosterEntryButton);
	if (RosterEntryButton)
	{
		RosterEntryButton->OnClicked.Broadcast();
	}
	TestTrue(TEXT("opening the roster preserves modality by closing the separate codex"), TownHud && !TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("the roster becomes visible in town"), RosterWidget && RosterWidget->GetVisibility() != ESlateVisibility::Collapsed);

	TestTrue(TEXT("Escape closes the roster before any battle or world transition"),
		PlayerController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
	TestTrue(TEXT("Escape hides the roster while retaining town state"),
		RosterWidget && RosterWidget->GetVisibility() == ESlateVisibility::Collapsed);
	TestEqual(TEXT("Escape does not leave town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);

	TestTrue(TEXT("the roster can be opened again before a route transition"), PlayerController->OpenCompanionRoster());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	PlayerController->RefreshPlayerFlowWidgetsForTest();
	TestTrue(TEXT("leaving town force-closes the roster"), RosterWidget && RosterWidget->GetVisibility() == ESlateVisibility::Collapsed);
	TestFalse(TEXT("the roster cannot open while on the route map"), PlayerController->OpenCompanionRoster());
	return true;
}

#endif
