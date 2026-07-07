#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKMVPRules.h"
#include "GameXXKBattleScenePresenter.generated.h"

class AGameXXKBattleSceneUnitActor;
class UGameXXKMVPSubsystem;

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleSceneUnitPlacement
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	bool bEnemy = false;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	int32 UnitIndex = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	FName UnitId;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	FVector Location = FVector::ZeroVector;
};

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKBattleScenePresenter : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKBattleScenePresenter();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene")
	bool EnsureBattleScene();

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	TArray<AGameXXKBattleSceneUnitActor*> GetSpawnedUnitsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene|Test")
	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	static TArray<FGameXXKBattleSceneUnitPlacement> BuildUnitPlacementsForState(const FGameXXKRuntimeState& State);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TSubclassOf<AGameXXKBattleSceneUnitActor> UnitActorClass;

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;
	void ClearSpawnedUnits();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	TArray<TObjectPtr<AGameXXKBattleSceneUnitActor>> SpawnedUnitObjects;
};
