#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "GameXXKBattleUnitResourceWidget.generated.h"

class UHorizontalBox;
class UImage;
class UMaterialInstanceDynamic;
class UProgressBar;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

/** Native, actor-independent screen-space projection of one unit's resources. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetUnitVitals(
		const FString& InSlotLabel,
		const FText& InDisplayName,
		int32 InCurrentHP,
		int32 InMaxHP,
		int32 InCurrentMana,
		int32 InMaxMana,
		bool bInShowMana);

	bool PrepareForScreenSpaceEmbedding();
	bool HasRuntimeWidgetTreeForTest() const;

	/** Read-only rendered-value seams used by the real-PIE HUD probe. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetHealthDisplayTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetManaDisplayTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	float GetHealthPercentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	float GetManaPercentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool IsHealthFillLeftToRightForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool IsManaFillLeftToRightForTest() const;
	static bool UsesWholeFullBarMaskForTest();

	/** The four texture sources must remain distinct PSD-derived Track/Full assets. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetHealthTrackResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetHealthFullResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetManaTrackResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetManaFullResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FString GetResourceMaskMaterialPathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool IsManaRowVisibleForTest() const;
	ESlateVisibility GetManaRowVisibilityForTest() const;
	bool AreContentWidgetsHitTestTransparentForTest() const;
	static ESlateVisibility GetRootHitTestVisibilityForTest();

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureWidgetTree();
	void RefreshDisplay();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> IdentityText;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> HealthRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> HealthBarSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> HealthMaskMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HealthTrackTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> HealthFullTexture;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> ManaRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ManaText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ManaBar;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> ManaProgressBar;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ManaBarSizeBox;

	UPROPERTY(Transient)
	TObjectPtr<UMaterialInstanceDynamic> ManaMaskMaterial;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ManaTrackTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> ManaFullTexture;

	FString SlotLabel;
	FText DisplayName;
	int32 CurrentHP = 0;
	int32 MaxHP = 1;
	int32 CurrentMana = 0;
	int32 MaxMana = 0;
	bool bShowMana = false;
	float HealthPercent = 0.0f;
	float ManaPercent = 0.0f;
};
