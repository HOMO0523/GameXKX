#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKBattlePartyQiWidget.generated.h"

class UImage;
class UOverlay;
class USizeBox;
class UTextBlock;

/**
 * Passive battle-board projection of the player team's one shared current-turn Qi.
 * It intentionally exposes only the current value: this is not a unit MP resource bar.
 */
UCLASS()
class GAMEXXK_API UGameXXKBattlePartyQiWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetSharedQi(int32 InSharedQi);
	bool PrepareForBoardEmbedding();

	bool HasRuntimeWidgetTreeForTest() const;
	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle|Test", meta = (DevelopmentOnly))
	int32 GetSharedQiForTest() const;
	FString GetDisplayTextForTest() const;
	FString GetSubtitleTextForTest() const;
	bool AreContentWidgetsHitTestTransparentForTest() const;
	/** Legacy test seam name retained for source compatibility; returns the active Qi icon asset path. */
	FString GetPaperFrameResourcePathForTest() const;
	FLinearColor GetQiInkColorForTest() const;

protected:
	virtual void NativeConstruct() override;

private:
	void EnsureWidgetTree();
	void RefreshDisplay();

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UOverlay> IconOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UImage> SoulIcon;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QiText;

	int32 SharedQi = 0;
};
