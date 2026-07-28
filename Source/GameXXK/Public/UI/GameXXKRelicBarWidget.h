#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKRelicBarWidget.generated.h"

class UCanvasPanel;
class UUniformGridPanel;

/** Shared upper-right route/battle relic inventory. Newest relics render first, six per row. */
UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKRelicBarWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Relics")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Relics|Test")
	bool PrepareForEmbedding();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Relics|Test")
	int32 GetColumnCountForTest() const { return 6; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Relics|Test")
	int32 CalculateRowCountForTest(int32 ItemCount) const { return FMath::DivideAndRoundUp(FMath::Max(0, ItemCount), 6); }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Relics|Test")
	bool UsesTooltipsForTest() const { return bUsesTooltips; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|Relics|Test")
	int32 GetRenderedRelicCountForTest() const { return RenderedRelicCount; }

private:
	void EnsureWidgetTree();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> RelicGrid;

	bool bUsesTooltips = true;
	int32 RenderedRelicCount = 0;
};
