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
