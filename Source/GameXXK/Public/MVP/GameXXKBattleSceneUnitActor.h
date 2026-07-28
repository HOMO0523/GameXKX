#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKCardTypes.h"
#include "GameXXKMVPRules.h"
#include "UObject/SoftObjectPtr.h"
#include "GameXXKBattleSceneUnitActor.generated.h"

class UBoxComponent;
class USceneComponent;
class UGameXXKMVPSubsystem;
class UPaperFlipbook;
class UPaperFlipbookComponent;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKBattleSceneUnitActor : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKBattleSceneUnitActor();
	virtual void Tick(float DeltaSeconds) override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene")
	// UHT requires a literal default here; -1 is UE's INDEX_NONE value.
	void ConfigureFromRuntimeUnit(bool bInEnemy, int32 InUnitIndex, const FGameXXKBattleRuntimeUnit& Unit, int32 InSlotNumber = -1);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene|Feedback")
	void PlayIntentAttackFeedback();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene|Feedback")
	void PlayHitFeedback();

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
	UPaperFlipbookComponent* GetBattleVisualComponent() const;

	/** Returns the visual foot used by the board-owned projected battle HUD. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	FVector GetBattleHudProjectionWorldLocation() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene")
	UPaperFlipbook* GetCurrentBattleFlipbook() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetSlotNumberForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetCurrentHealthForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetMaxHealthForTest() const;

	/** Scene-facing half of card targeting: the controller applies the Board's legal stable UnitId set here. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|BattleScene|Cards")
	void SetCardTargetHighlight(bool bHighlighted);

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Cards")
	bool IsCardTargetHighlighted() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Cards")
	bool IsCardTargetOutlineEnabled() const;

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> SceneRoot;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UBoxComponent> HitArea;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UPaperFlipbookComponent> BattleVisual;

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
	/** Sprite-content correction inside a fixed P slot; never moves SceneRoot or HitArea. */
	FVector ResolveBattleVisualSlotOffset() const;
	/** Adds the fixed-slot correction to the authored Paper2D local position without accumulating it. */
	void ApplyBattleVisualSlotOffset();
	void RefreshVisual();
	void ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit);
	void BeginFeedback(bool bInAttackFeedback);
	void RestoreFeedbackVisual();
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
	int32 MaxHP = 1;

	UPROPERTY(Transient)
	int32 SlotNumber = INDEX_NONE;

	UPROPERTY(Transient)
	bool bDefeated = false;

	UPROPERTY(Transient)
	bool bCardTargetHighlighted = false;

	bool bFeedbackActive = false;
	bool bFeedbackIsAttack = false;
	float FeedbackElapsed = 0.0f;
	FVector FeedbackBaseLocation = FVector::ZeroVector;
	FVector FeedbackBaseScale = FVector(0.55f, 0.55f, 0.55f);
	FVector BattleVisualAuthoredBaseLocation = FVector::ZeroVector;
	bool bHasBattleVisualAuthoredBaseLocation = false;

};
