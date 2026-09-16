#pragma once

#include "CoreMinimal.h"
#include "Components/Widget.h"
#include "UI/GameXXKTrainingTravelVisualRuntime.h"
#include "GameXXKTravelLootWidget.generated.h"

class UGameXXKDesktopTrainingWorkbenchWidget;

struct FGameXXKTravelLootSample
{
	FVector2D Position = FVector2D::ZeroVector;
	float Scale = 0;
	float Angle = 0;
	float Opacity = 0;
};

namespace GameXXKTravelLoot
{
	GAMEXXK_API FVector2D Source(int32 EnemySlot);
	GAMEXXK_API FVector2D Target(EGameXXKTrainingRewardTier Tier);
	GAMEXXK_API float Arrival(bool bChest, int32 ParticleIndex = 0);
	GAMEXXK_API FGameXXKTravelLootSample Sample(const FGameXXKTravelLootBurst& Burst, int32 ParticleIndex, bool bChest);
	GAMEXXK_API float Pulse(float Age);
}

/** All particles and their collection targets share strip-local coordinates. */
UCLASS()
class GAMEXXK_API UGameXXKTravelLootWidget : public UWidget
{
	GENERATED_BODY()
public:
	void Configure(UGameXXKDesktopTrainingWorkbenchWidget* InOwner, const TArray<FSlateBrush>& InBrushes);
	UGameXXKDesktopTrainingWorkbenchWidget* GetHost() const { return Owner.Get(); }
	const FSlateBrush& GetLootBrush(int32 Index) const { return Brushes[Index]; }
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
private:
	TWeakObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Owner;
	UPROPERTY(Transient)
	TArray<FSlateBrush> Brushes;
};
