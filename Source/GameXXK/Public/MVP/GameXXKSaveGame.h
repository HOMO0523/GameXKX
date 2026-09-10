#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "GameFramework/SaveGame.h"
#include "GameXXKSaveGame.generated.h"

UCLASS(BlueprintType)
class GAMEXXK_API UGameXXKSaveGame : public USaveGame
{
	GENERATED_BODY()

public:
	UGameXXKSaveGame();

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame, Category = "GameXXK|MVP")
	FGameXXKSaveState SaveState;

	/** Optional integrity envelope. Legacy saves have schema zero and remain readable. */
	UPROPERTY(SaveGame)
	int32 IntegritySchema = 0;

	UPROPERTY(SaveGame)
	uint32 PayloadChecksum = 0;
};
