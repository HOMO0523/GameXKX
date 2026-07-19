#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "GameXXKCardTypes.h"
#include "UI/GameXXKBattleStatusIconStyle.h"
#include "GameXXKBattleUnitStatusEffectsWidget.generated.h"

class UHorizontalBox;

/** Native, actor-independent projection of armor and battle-status effects. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitStatusEffectsWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetStatusEffects(int32 InArmor, const TArray<FGameXXKCardStatusStack>& InStatuses);
	bool PrepareForScreenSpaceEmbedding();
	bool HasRuntimeWidgetTreeForTest() const;
	static FString BuildStatusText(const TArray<FGameXXKCardStatusStack>& InStatuses);
	static TArray<FGameXXKBattleStatusBadgeModel> BuildBadgeModels(int32 InArmor, const TArray<FGameXXKCardStatusStack>& InStatuses);
	int32 GetIconCountForTest() const;
	FName GetIconIdForTest(int32 Index) const;
	FString GetIconDisplayedStackForTest(int32 Index) const;
	int32 GetIconRebuildGenerationForTest() const;
	static ESlateVisibility GetRootHitTestVisibilityForTest();

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureWidgetTree();
	void RefreshStatusIcons();

	UPROPERTY(Transient)
	TObjectPtr<UHorizontalBox> StatusIconRow;

	TArray<FGameXXKBattleStatusBadgeModel> CachedBadgeModels;
	int32 IconRebuildGeneration = 0;
};
