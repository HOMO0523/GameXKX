#include "MVP/GameXXKTutorial01GameMode.h"

#include "GameFramework/HUD.h"
#include "MVP/GameXXKMVPPlayerController.h"

AGameXXKTutorial01GameMode::AGameXXKTutorial01GameMode()
{
	PlayerControllerClass = AGameXXKMVPPlayerController::StaticClass();
	DefaultPawnClass = nullptr;
	HUDClass = AHUD::StaticClass();
}
