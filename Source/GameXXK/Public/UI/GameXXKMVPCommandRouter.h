#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPCommandDescriptor.h"

class UGameXXKMVPSubsystem;

namespace GameXXKMVPCommandRouter
{
	GAMEXXK_API TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands(const UGameXXKMVPSubsystem* Subsystem);
	GAMEXXK_API bool ExecuteVisibleCommand(UGameXXKMVPSubsystem* Subsystem, FName CommandName, const FString& SlotName = TEXT(""), int32 UserIndex = 0, bool bAutosave = true);
	GAMEXXK_API FText BuildStatusText(const UGameXXKMVPSubsystem* Subsystem);
}
