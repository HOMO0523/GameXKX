#include "UI/GameXXKMainMenuWidget.h"

#include "MVP/GameXXKMVPSubsystem.h"

bool UGameXXKMainMenuWidget::StartGame()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bStarted = Subsystem && Subsystem->StartGame();
	if (bStarted)
	{
		OnStartGameSucceeded();
	}
	return bStarted;
}

bool UGameXXKMainMenuWidget::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const bool bStarted = Subsystem && Subsystem->StartGameFromSlot(SlotName, UserIndex);
	if (bStarted)
	{
		OnStartGameSucceeded();
	}
	return bStarted;
}
