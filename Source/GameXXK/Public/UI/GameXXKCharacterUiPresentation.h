#pragma once

#include "CoreMinimal.h"

class UGameXXKMVPSubsystem;

/** Player-facing names for backpack and character-selection surfaces. */
namespace GameXXKCharacterUiPresentation
{
	GAMEXXK_API FString GetDisplayName(const UGameXXKMVPSubsystem* Subsystem, FName CharacterId);
}
