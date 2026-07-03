#include "UI/GameXXKMVPWidgetBase.h"

#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

void UGameXXKMVPWidgetBase::SetMVPSubsystem(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

UGameXXKMVPSubsystem* UGameXXKMVPWidgetBase::GetMVPSubsystem() const
{
	return ResolveMVPSubsystem();
}

UGameXXKMVPSubsystem* UGameXXKMVPWidgetBase::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = GetGameInstance();
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

AGameXXKMVPPlayerController* UGameXXKMVPWidgetBase::ResolveMVPPlayerController() const
{
	if (AGameXXKMVPPlayerController* PlayerController = Cast<AGameXXKMVPPlayerController>(GetOwningPlayer()))
	{
		return PlayerController;
	}

	return GetTypedOuter<AGameXXKMVPPlayerController>();
}

bool UGameXXKMVPWidgetBase::NotifyPlayerFlowStateChanged()
{
	AGameXXKMVPPlayerController* PlayerController = ResolveMVPPlayerController();
	if (!PlayerController)
	{
		return false;
	}

	PlayerController->RefreshPlayerFlowWidgetsFromState();
	return true;
}
