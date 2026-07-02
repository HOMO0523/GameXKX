#include "MVP/GameXXKMVPPlayerController.h"

#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"

void AGameXXKMVPPlayerController::BeginPlay()
{
	Super::BeginPlay();

	bShowMouseCursor = true;
	FInputModeGameAndUI InputMode;
	InputMode.SetHideCursorDuringCapture(false);
	SetInputMode(InputMode);
}

void AGameXXKMVPPlayerController::FlushPressedKeys()
{
	Super::FlushPressedKeys();

	if (AGameXXKTownPlayerPawn* TownPawn = Cast<AGameXXKTownPlayerPawn>(GetPawn()))
	{
		TownPawn->ResetTownMovementInput();
	}
	else if (AGameXXKHeroCharacter* HeroCharacter = Cast<AGameXXKHeroCharacter>(GetPawn()))
	{
		HeroCharacter->ResetTownMovementInput();
	}
}
