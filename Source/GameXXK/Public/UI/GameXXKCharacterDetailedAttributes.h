#pragma once

#include "CoreMinimal.h"

class UGameXXKMVPSubsystem;

struct GAMEXXK_API FGameXXKCharacterDetailRow
{
	FName Id;
	FString Label;
	FString Value;
	FString Note;
	bool bSection = false;
	bool bInactive = false;
};

namespace GameXXKCharacterDetailedAttributes
{
	/** Permanent loadout values; conditional sets are separate from unconditional bonuses. */
	GAMEXXK_API TArray<FGameXXKCharacterDetailRow> Build(const UGameXXKMVPSubsystem* Subsystem, FName CharacterId);
}
