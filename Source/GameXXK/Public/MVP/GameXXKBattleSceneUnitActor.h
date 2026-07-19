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
class UGameXXKBattleUnitResourceWidget;
class UGameXXKBattleUnitStatusEffectsWidget;
class UPaperFlipbook;
class UPaperFlipbookComponent;
class UTextRenderComponent;
class UWidgetComponent;

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	USceneComponent* GetHudAnchorComponentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	USceneComponent* GetResourceHudAnchorComponentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	USceneComponent* GetStatusEffectsAnchorComponentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	UWidgetComponent* GetResourceHudWidgetComponentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	UWidgetComponent* GetStatusEffectsWidgetComponentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetSlotNumberForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetArmorForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetCurrentHealthForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetMaxHealthForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetCurrentManaForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetMaxManaForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	bool ShouldShowQiForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetResourcePresentationGenerationForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	int32 GetStatusEffectsPresentationGenerationForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|BattleScene|Test")
	FString GetStatusTextForTest() const;

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

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> HudAnchorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> ResourceHudAnchorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<USceneComponent> StatusEffectsAnchorComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UWidgetComponent> ResourceHudWidgetComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|BattleScene")
	TObjectPtr<UWidgetComponent> StatusEffectsWidgetComponent;

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
	void RefreshHudAnchor();
	void RefreshResourceHudWidget();
	void RefreshStatusEffectsWidget();
	void ResolveCardRuntimePresentation(const FGameXXKBattleRuntimeUnit& LegacyUnit);
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
	int32 MaxHP = 1;

	UPROPERTY(Transient)
	int32 CurrentMana = 0;

	UPROPERTY(Transient)
	int32 MaxMana = 0;

	UPROPERTY(Transient)
	int32 SlotNumber = INDEX_NONE;

	UPROPERTY(Transient)
	int32 CurrentArmor = 0;

	UPROPERTY(Transient)
	TArray<FGameXXKCardStatusStack> CurrentStatuses;

	UPROPERTY(Transient)
	FText DisplayName;

	UPROPERTY(Transient)
	bool bDefeated = false;

	UPROPERTY(Transient)
	bool bShowQi = false;

	bool bHasResourcePresentation = false;
	int32 LastResourceCurrentHP = 0;
	int32 LastResourceMaxHP = 1;
	int32 LastResourceCurrentMana = 0;
	int32 LastResourceMaxMana = 0;
	int32 LastResourceSlotNumber = INDEX_NONE;
	FText LastResourceDisplayName;
	bool bLastResourceShowQi = false;
	int32 ResourcePresentationGeneration = 0;

	bool bHasStatusEffectsPresentation = false;
	int32 LastStatusEffectsArmor = 0;
	TArray<FGameXXKCardStatusStack> LastStatusEffectsStatuses;
	int32 StatusEffectsPresentationGeneration = 0;
};
