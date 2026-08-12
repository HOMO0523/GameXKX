#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKCardOutcomePreview.h"
#include "GameXXKCardOutcomePreviewWidget.generated.h"

class UVerticalBox;

UCLASS()
class GAMEXXK_API UGameXXKCardOutcomePreviewWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetLines(const TArray<FGameXXKCardOutcomeTextLine>& InLines);
	void Clear();
	int32 GetVisibleLineCountForTest() const;
	FString GetPlainLineForTest(int32 LineIndex) const;
	FLinearColor GetSegmentColorForTest(int32 LineIndex, int32 SegmentIndex) const;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	void RefreshLines();

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> LineBox;

	TArray<FGameXXKCardOutcomeTextLine> Lines;
};
