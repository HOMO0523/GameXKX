#pragma once

#include "CoreMinimal.h"
#include "GameXXKTalentTypes.h"

class GAMEXXK_API FGameXXKTalentCatalog final
{
public:
	static const TArray<FGameXXKTalentNodeDefinition>& GetDefinitions();
	static const FGameXXKTalentNodeDefinition* Find(FName NodeId);
	static const FGameXXKTalentNodeDefinition* FindBranchEntry(EGameXXKTalentBranch Branch);
	static int32 GetMaxCostTier();
	static bool Validate(FString* OutError = nullptr);
	static int32 GetIconAtlasIndex(EGameXXKTalentIcon Icon);
};
