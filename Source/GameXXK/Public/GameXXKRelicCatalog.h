#pragma once

#include "CoreMinimal.h"
#include "GameXXKRelicTypes.h"

class GAMEXXK_API FGameXXKRelicCatalog final
{
public:
	static const TArray<FGameXXKRelicDefinition>& GetAllDefinitions();
	static const FGameXXKRelicDefinition* FindDefinition(FName RelicId);
};
