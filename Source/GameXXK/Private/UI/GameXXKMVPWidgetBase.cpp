#include "UI/GameXXKMVPWidgetBase.h"

#include "Engine/GameInstance.h"
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
