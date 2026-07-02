#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKMVPHUD.h"
#include "UI/GameXXKPlayableRootWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayableRootWidgetTest,
	"GameXXK.MVP.PIE.MainMenuContinueWorldMap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayableRootWidgetTest::RunTest(const FString& Parameters)
{
	const FString RootTestSlot = TEXT("GameXXK_MVP_Automation_PlayableRoot_Start");
	const int32 UserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(RootTestSlot, UserIndex);

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKPlayableRootWidget* RootWidget = NewObject<UGameXXKPlayableRootWidget>();
	RootWidget->SetMVPSubsystem(Subsystem);
	RootWidget->SetStartGameSlotForTest(RootTestSlot, UserIndex);

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

	UGameplayStatics::DeleteGameInSlot(RootTestSlot, UserIndex);
	return true;
}

#endif
