#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKOneGameIslandRouteMapBridge.generated.h"

class FBoolProperty;
class UCanvasPanelSlot;
class UScrollBox;
class UUserWidget;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKOneGameIslandRouteMapBridge : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKOneGameIslandRouteMapBridge();

	static float CalculateTargetScrollOffset(float NodeCanvasY, float TopPadding);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void SynchronizeRouteMapScroll();
	UClass* ResolveRouteMapWidgetClass() const;
	UClass* ResolveRouteNodeWidgetClass() const;
	UUserWidget* FindRouteMapWidget(UClass* RouteMapClass) const;
	FBoolProperty* FindRouteNodeClickableProperty(UClass* RouteNodeClass) const;
	bool IsRouteNodeClickable(UUserWidget* RouteNodeWidget, FBoolProperty* ClickableProperty) const;
	UScrollBox* FindParentScrollBox(UUserWidget* RouteNodeWidget) const;
	UCanvasPanelSlot* FindCanvasSlot(UUserWidget* RouteNodeWidget) const;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game")
	TSoftClassPtr<UUserWidget> RouteMapWidgetClass;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game")
	TSoftClassPtr<UUserWidget> RouteNodeWidgetClass;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game", meta = (ClampMin = "0.0"))
	float ScrollTopPadding = 280.0f;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game", meta = (ClampMin = "0.05"))
	float SyncIntervalSeconds = 0.25f;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game", meta = (ClampMin = "1"))
	int32 MaxSyncAttempts = 240;

	FTimerHandle SyncTimerHandle;
	int32 SyncAttempts = 0;
	float LastAppliedScrollOffset = -1.0f;
};
