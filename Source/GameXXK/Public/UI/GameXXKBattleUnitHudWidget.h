#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/SlateWrapperTypes.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKBattleUnitHudWidget.generated.h"

class UGameXXKBattleUnitResourceWidget;
class UGameXXKBattleUnitStatusEffectsWidget;
class UVerticalBox;

/** Ordinary screen-space composite of one authoritative unit's resource and status HUD children. */
UCLASS()
class GAMEXXK_API UGameXXKBattleUnitHudWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetUnitView(const FGameXXKBattleUnitHudView& InView);
	bool PrepareForBoardEmbedding();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	FName GetUnitIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	EGameXXKCardTargetSide GetSideForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	int32 GetSlotNumberForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UGameXXKBattleUnitResourceWidget* GetResourceWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	UGameXXKBattleUnitStatusEffectsWidget* GetStatusEffectsWidgetForTest() const;

	static ESlateVisibility GetRootHitTestVisibilityForTest();

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureWidgetTree();
	void RefreshFromView();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleUnitResourceWidget> ResourceWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleUnitStatusEffectsWidget> StatusEffectsWidget;

	FGameXXKBattleUnitHudView CachedView;
	bool bHasUnitView = false;
};
