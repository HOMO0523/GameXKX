#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "UI/GameXXKBattleStatusIconStyle.h"
#include "GameXXKBattleStatusIconWidget.generated.h"

class UBorder;
class USizeBox;
class UTextBlock;

/** One reusable, resolved battle-status badge with its own native hover target. */
UCLASS()
class GAMEXXK_API UGameXXKBattleStatusIconWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetBadgeModel(const FGameXXKBattleStatusBadgeModel& InBadgeModel, bool bInOverflow = false);
	bool PrepareForScreenSpaceEmbedding();
	bool HasRuntimeWidgetTreeForTest() const;
	FName GetIconIdForTest() const;
	FString GetDisplayedStackForTest() const;
	static FString FormatStackForTest(int32 Stacks);
	static ESlateVisibility GetHitTargetVisibilityForTest();
	static ESlateVisibility GetTooltipVisibilityForTest();

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureWidgetTree();
	void RefreshDisplay();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> HitTarget;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StackText;

	FGameXXKBattleStatusBadgeModel CachedBadgeModel;
	bool bIsOverflowBadge = false;
};
