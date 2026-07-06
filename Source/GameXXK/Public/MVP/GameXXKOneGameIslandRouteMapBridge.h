#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameXXKMVPRules.h"
#include "GameXXKOneGameIslandRouteMapBridge.generated.h"

class FBoolProperty;
class FIntProperty;
class UCanvasPanelSlot;
class UScrollBox;
class UGameXXKBattleBoardWidget;
class UGameXXKMVPSubsystem;
class UUserWidget;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKOneGameIslandRouteMapBridge : public AActor
{
	GENERATED_BODY()

public:
	AGameXXKOneGameIslandRouteMapBridge();

	static float CalculateTargetScrollOffset(float NodeCanvasY, float TopPadding);
	static bool ShouldOpenBattleLayoutForOriginalLevelAdvance(int32 PreviousLevel, int32 CurrentLevel, int32 BattleStartLevel);
	static bool ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen Screen);
	static bool ShouldUseLiveGameXXKBattleSubsystem(EGameXXKScreen Screen);
	static bool PrimeBattleSubsystemForIslandRoute(UGameXXKMVPSubsystem& Subsystem);

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
	void SynchronizeOriginalBattleLayout(UUserWidget* RouteMapWidget);
	bool TryReadOriginalCurrentLevel(int32& OutCurrentLevel) const;
	FIntProperty* FindOriginalCurrentLevelProperty(UClass* PlayerControllerClass) const;
	bool OpenBattleLayoutFromOriginalRoute(UUserWidget* RouteMapWidget);
	UGameXXKMVPSubsystem* ResolveBattleSubsystem();

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

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game")
	TSubclassOf<UGameXXKBattleBoardWidget> BattleBoardWidgetClass;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game", meta = (ClampMin = "1"))
	int32 OriginalBattleStartLevel = 1;

	UPROPERTY(EditAnywhere, Category = "GameXXK|1Game", meta = (ClampMin = "0"))
	int32 OriginalCurrentLevelIntPropertyIndex = 1;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> BattleSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> BattleBoardWidget;

	FTimerHandle SyncTimerHandle;
	int32 SyncAttempts = 0;
	float LastAppliedScrollOffset = -1.0f;
	int32 LastObservedOriginalLevel = INDEX_NONE;
	bool bBattleLayoutOpen = false;
};
