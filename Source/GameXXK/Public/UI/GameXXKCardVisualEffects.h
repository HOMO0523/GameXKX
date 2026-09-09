#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "UI/GameXXKCardSynergyPresentation.h"
#include "GameXXKCardVisualEffects.generated.h"

struct FGameXXKCardMotionSample
{
	FVector2D Offset = FVector2D::ZeroVector;
	FVector2D Scale = FVector2D(1,1);
	float Angle = 0;
	float Opacity = 1;
	bool bFrontFace = true;
};

namespace GameXXKCardVisualEffects
{
	GAMEXXK_API FGameXXKCardMotionSample Deal(float Age,int32 Index,int32 Order=INDEX_NONE);
	GAMEXXK_API FGameXXKCardMotionSample Flip(float Age,int32 Index);
	GAMEXXK_API float Breath(float Seconds);
	GAMEXXK_API FLinearColor StatusColor(EGameXXKCardStatus Status);
	GAMEXXK_API FLinearColor DamageColor(EGameXXKCardDamageCause Cause);
}

/** A cheap local-space Slate ink aura. It never intercepts a card's input. */
UCLASS()
class GAMEXXK_API UGameXXKCardAuraWidget : public UWidget
{
	GENERATED_BODY()
public:
	void SetCues(const TArray<FGameXXKCardSynergyCue>& InCues);
	const TArray<FGameXXKCardSynergyCue>& GetCues() const { return Cues; }
	UFUNCTION(BlueprintPure,Category="GameXXK|UI|Test")
	FString GetCueDebugText() const;
	UFUNCTION(BlueprintPure,Category="GameXXK|UI|Test")
	int32 GetCueCountForTest() const { return Cues.Num(); }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TArray<FGameXXKCardSynergyCue> Cues;
};
