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
	static float GetIconOverscanPaddingForTest();
	static ESlateVisibility GetHitTargetVisibilityForTest();
	static ESlateVisibility GetTooltipVisibilityForTest();
	FReply GetMouseButtonDownReplyForTest() const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	bool DoesMouseDownPassThroughForTest() const;

protected:
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
	static FReply MakeMouseButtonDownReply();
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
