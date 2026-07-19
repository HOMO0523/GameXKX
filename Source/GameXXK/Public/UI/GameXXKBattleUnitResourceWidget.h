#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "GameXXKBattleUnitResourceWidget.generated.h"

class UHorizontalBox;
class UProgressBar;
class UTextBlock;
class UVerticalBox;

/** Native, actor-independent screen-space projection of one unit's resources. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitResourceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetUnitResources(
		const FString& InSlotLabel,
		const FText& InDisplayName,
		int32 InCurrentHP,
		int32 InMaxHP,
		int32 InCurrentMana,
		int32 InMaxMana,
		bool bInShowQi);

	bool PrepareForScreenSpaceEmbedding();
	bool HasRuntimeWidgetTreeForTest() const;
	FString GetHealthDisplayTextForTest() const;
	FString GetQiDisplayTextForTest() const;
	float GetHealthPercentForTest() const;
	float GetQiPercentForTest() const;
	bool IsQiRowVisibleForTest() const;
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
	TObjectPtr<UTextBlock> HealthText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> HealthBar;

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> QiRow;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QiText;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> QiBar;

	FString SlotLabel;
	FText DisplayName;
	int32 CurrentHP = 0;
	int32 MaxHP = 1;
	int32 CurrentMana = 0;
	int32 MaxMana = 0;
	bool bShowQi = false;
};
