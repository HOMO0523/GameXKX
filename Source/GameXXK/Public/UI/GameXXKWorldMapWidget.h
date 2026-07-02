#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKWorldMapWidget.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKWorldMapWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|WorldMap")
	bool TrySelectRegion(FName RegionId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap")
	FName GetLastSelectedRegion() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|WorldMap")
	bool WasLastSelectionUnlocked() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|WorldMap")
	void OnLockedRegionSelected(FName RegionId);

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|WorldMap")
	void OnUnlockedRegionSelected(FName RegionId);

private:
	UPROPERTY(Transient)
	FName LastSelectedRegion;

	UPROPERTY(Transient)
	bool bLastSelectionUnlocked = false;
};
