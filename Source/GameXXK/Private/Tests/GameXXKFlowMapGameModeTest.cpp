#include "MVP/GameXXKFlowMapGameMode.h"

#include "GameFramework/SpectatorPawn.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFlowMapGameModeTest,
	"GameXXK.MVP.FlowMap.GameModeDefaults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFlowMapGameModeTest::RunTest(const FString& Parameters)
{
	const AGameXXKFlowMapGameMode* GameMode = NewObject<AGameXXKFlowMapGameMode>();
	TestNotNull(TEXT("flow map game mode exists"), GameMode);
	TestEqual(TEXT("flow maps use MVP player controller"), GameMode->PlayerControllerClass.Get(), AGameXXKMVPPlayerController::StaticClass());
	TestEqual(TEXT("flow maps use MVP HUD"), GameMode->HUDClass.Get(), AGameXXKMVPHUD::StaticClass());
	TestEqual(TEXT("flow maps use spectator pawn instead of town hero"), GameMode->DefaultPawnClass.Get(), ASpectatorPawn::StaticClass());
	return true;
}

#endif
