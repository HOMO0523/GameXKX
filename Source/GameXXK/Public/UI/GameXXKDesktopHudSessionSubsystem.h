#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "UI/GameXXKDesktopWorkbenchSessionState.h"
#include "GameXXKDesktopHudSessionSubsystem.generated.h"

UCLASS()
class GAMEXXK_API UGameXXKDesktopHudSessionSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	void StoreForMapTravel(const FGameXXKDesktopWorkbenchSessionState& State);
	bool ConsumeAfterMapTravel(FGameXXKDesktopWorkbenchSessionState& OutState);
	void DiscardPending();
	bool HasPendingState() const { return PendingState.IsSet(); }

private:
	TOptional<FGameXXKDesktopWorkbenchSessionState> PendingState;
};
