#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKMVPRules.h"
#include "UObject/SoftObjectPtr.h"
#include "GameXXKBattleSceneUnitActor.generated.h"

class UBoxComponent;
class UGameXXKMVPSubsystem;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UTextRenderComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKBattleSceneUnitActor : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKBattleSceneUnitActor();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene")
	void ConfigureFromRuntimeUnit(bool bInEnemy, int32 InUnitIndex, const FGameXXKBattleRuntimeUnit& Unit);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene")
	bool ApplyPrimaryPartyAttack(APawn* InstigatorPawn);

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	bool IsEnemyUnit() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	bool CanReceivePrimaryPartyAttack() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	bool CanOpenPartyCommandMenu() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	bool CanReceiveTargetedBattleAction() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	int32 GetUnitIndex() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	FName GetUnitId() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	UBoxComponent* GetHitArea() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	UTextRenderComponent* GetLabelTextComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	UPaperFlipbookComponent* GetBattleVisualComponent() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	UPaperFlipbook* GetCurrentBattleFlipbook() const;

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UBoxComponent> HitArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UPaperFlipbookComponent> BattleVisual;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UTextRenderComponent> LabelText;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> HeroBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> FollowerBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> EnemyBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> MoneyMouseBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> NiuHuanBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> BlackBearBattleFlipbookAsset;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GameXXK|BattleScene|Visual")
	TSoftObjectPtr<UPaperFlipbook> TigerBossBattleFlipbookAsset;

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem(APawn* InstigatorPawn) const;
	void RefreshFromRuntimeState(UGameXXKMVPSubsystem* Subsystem);
	void RefreshLabel();
	void RefreshVisual();
	void RefreshPlayerFlowWidgets(APawn* InstigatorPawn) const;
	UPaperFlipbook* ResolveBattleFlipbook() const;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	bool bEnemy = false;

	UPROPERTY(Transient)
	int32 UnitIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FName UnitId;

	UPROPERTY(Transient)
	int32 CurrentHP = 0;

	UPROPERTY(Transient)
	int32 MaxHP = 0;

	UPROPERTY(Transient)
	bool bDefeated = false;
};
