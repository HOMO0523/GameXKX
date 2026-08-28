#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"

#include "GameXXKSceneRegistry.generated.h"

class UGameXXKCharacterCatalog;
class UGameXXKSceneProfile;
class UGameXXKStageContract;

UCLASS()
class GAMEXXK_API UGameXXKSceneRegistry : public UObject
{
	GENERATED_BODY()

public:
	bool RegisterActiveProfile(
		const UGameXXKStageContract& Contract,
		UGameXXKSceneProfile& Profile,
		const UGameXXKCharacterCatalog* CharacterCatalog,
		const FSoftObjectPath& CurrentMapPath,
		FString* OutError = nullptr);

	UGameXXKSceneProfile* ResolveProfile(FName StageContractId) const;

private:
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKSceneProfile>> ActiveProfiles;
};
