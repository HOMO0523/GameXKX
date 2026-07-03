#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "Town/GameXXKTownNpcActor.h"
#include "GameXXKMVPGameMode.generated.h"

class AGameXXKHeroCharacter;
class AGameXXKTownExitActor;
class AGameXXKTownNpcCharacter;
class APawn;
class UGameXXKMVPSubsystem;

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	int32 GetConfiguredTownExitCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	FVector GetConfiguredTownExitLocation() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	TSubclassOf<AGameXXKHeroCharacter> GetMerchantTownNpcVisualClass() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Visual")
	TSubclassOf<AGameXXKHeroCharacter> GetPersonTownNpcVisualClass() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	TSubclassOf<AGameXXKTownNpcCharacter> GetMerchantTownNpcCharacterClass() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town")
	TSubclassOf<AGameXXKTownNpcCharacter> GetPersonTownNpcCharacterClass() const;

	void ApplyQuestNpcRuntimeState(AGameXXKTownNpcCharacter* QuestNpc, const UGameXXKMVPSubsystem* Subsystem, APawn* FollowTarget) const;
	void ApplyPlayerRuntimeState(APawn* PlayerPawn, const UGameXXKMVPSubsystem* Subsystem) const;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town")
	TSubclassOf<AGameXXKTownNpcCharacter> MerchantTownNpcCharacterClass;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|Town")
	TSubclassOf<AGameXXKTownNpcCharacter> PersonTownNpcCharacterClass;

private:
	AGameXXKTownNpcCharacter* SpawnTownNpc(EGameXXKTownNpcRole NpcRole, const FVector& Location, TSubclassOf<AGameXXKTownNpcCharacter> NpcClass);
	AGameXXKTownNpcCharacter* FindSpawnedTownNpc(EGameXXKTownNpcRole NpcRole) const;
	AGameXXKTownExitActor* EnsureTownExit();
	AGameXXKTownExitActor* FindExistingTownExit() const;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGameXXKTownNpcCharacter>> SpawnedTownNpcs;

	UPROPERTY(Transient)
	TObjectPtr<AGameXXKTownExitActor> SpawnedTownExit;
};
