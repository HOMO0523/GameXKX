#pragma once

#include "CoreMinimal.h"
#include "Components/ScrollBar.h"
#include "GameXXKInkScrollBar.generated.h"

class UScrollBox;

/** A pinned ink thumb for a scroll box whose own bar lives outside a nested viewport. */
UCLASS()
class GAMEXXK_API UGameXXKInkScrollBar : public UScrollBar
{
	GENERATED_BODY()
public:
	void Configure(UScrollBox* InTarget, float InFixedThumbLength = 0.0f);
	void RefreshFromTarget();
	void ScrollToFraction(float Fraction);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	float GetContentExtent() const;
	float GetThumbFraction() const;
	float FixedThumbLength = 0.0f;
	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> Target;
};
