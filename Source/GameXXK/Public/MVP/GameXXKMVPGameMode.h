#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Town/GameXXKTownNpcActor.h"
#include "GameXXKMVPGameMode.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AGameXXKMVPGameMode();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	int32 GetSpawnedTownNpcCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	int32 GetConfiguredTownNpcCount() const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town")
	TSubclassOf<AGameXXKTownNpcActor> TownNpcClass;

private:
	void SpawnTownNpc(EGameXXKTownNpcRole NpcRole, const FVector& Location);

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGameXXKTownNpcActor>> SpawnedTownNpcs;
};
