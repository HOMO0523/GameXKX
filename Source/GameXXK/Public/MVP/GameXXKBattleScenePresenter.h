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

	/** Fixed display slot (1P outer through 3P inner); never inferred from array index. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	int32 SlotNumber = INDEX_NONE;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|BattleScene")
	FVector Location = FVector::ZeroVector;
};

/** A pure UnitId membership decision used by scene refresh and automation coverage. */
enum class EGameXXKBattleSceneRefreshAction : uint8
{
	Retain,
	Remove,
	Spawn
};

struct GAMEXXK_API FGameXXKBattleSceneUnitRefreshDecision
{
	FName UnitId;
	EGameXXKBattleSceneRefreshAction Action = EGameXXKBattleSceneRefreshAction::Retain;
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

	/**
	 * Updates the current battle formation by UnitId difference.  Retained units
	 * keep their actor identity (and transient feedback); only departed UnitIds
	 * are destroyed and only new UnitIds are spawned.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene")
	bool RefreshBattleScene();

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	TArray<AGameXXKBattleSceneUnitActor*> GetSpawnedUnitsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene|Test")
	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	static TArray<FGameXXKBattleSceneUnitPlacement> BuildUnitPlacementsForState(const FGameXXKRuntimeState& State);
	/** Applies a scene anchor to the immutable local P-slot formation. */
	static TArray<FGameXXKBattleSceneUnitPlacement> BuildUnitPlacementsForStateAtAnchor(
		const FGameXXKRuntimeState& State,
		const FVector& SceneAnchor);
	static TArray<FGameXXKBattleSceneUnitRefreshDecision> BuildUnitRefreshDecisions(
		const TArray<FName>& CurrentUnitIds,
		const TArray<FGameXXKBattleSceneUnitPlacement>& NextPlacements);

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
