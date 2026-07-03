#include "MVP/GameXXKFlowMapGameMode.h"

#include "GameFramework/SpectatorPawn.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKMVPHUD.h"

AGameXXKFlowMapGameMode::AGameXXKFlowMapGameMode()
{
	PlayerControllerClass = AGameXXKMVPPlayerController::StaticClass();
	HUDClass = AGameXXKMVPHUD::StaticClass();
	DefaultPawnClass = ASpectatorPawn::StaticClass();
}
