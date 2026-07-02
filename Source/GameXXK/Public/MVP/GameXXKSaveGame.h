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
};
