#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "GameXXKMVPHUD.generated.h"

class UGameXXKMVPSubsystem;
class UGameXXKBattleBoardWidget;
class UGameXXKMainMenuWidget;
class UGameXXKOneGameRouteMapWidget;
class UGameXXKPlayableRootWidget;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGameXXKMVPHUD();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Playable")
	bool HandleDemoCommand(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	FText BuildStatusText() const;

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);
	void SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex);
	UGameXXKMainMenuWidget* CreateMainMenuWidgetForTest();
	UGameXXKPlayableRootWidget* CreatePlayableRootWidgetForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasMainMenuWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKMainMenuWidget* GetMainMenuWidget() const;

	void SetDebugPlayableShellEnabledForTest(bool bEnabled);
	bool IsDebugPlayableShellEnabledForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasPlayableRootWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKPlayableRootWidget* GetPlayableRootWidget() const;

	UGameXXKOneGameRouteMapWidget* CreateRouteMapWidgetForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasRouteMapWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKOneGameRouteMapWidget* GetRouteMapWidget() const;

	bool ShouldDrawLegacyRouteMapForTest() const;
	void RefreshAuxiliaryWidgetsForTest();

	UGameXXKBattleBoardWidget* CreateBattleBoardWidgetForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasBattleBoardWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKBattleBoardWidget* GetBattleBoardWidget() const;

	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

protected:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;

private:
	UGameXXKMainMenuWidget* CreateMainMenuWidget();
	UGameXXKPlayableRootWidget* CreatePlayableRootWidget();
	UGameXXKOneGameRouteMapWidget* CreateRouteMapWidget();
	UGameXXKBattleBoardWidget* CreateBattleBoardWidget();
	void ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const;
	void RefreshMainMenuWidget();
	void RefreshRouteMapWidget();
	void RefreshBattleBoardWidget();
	void RefreshAuxiliaryWidgets();
	bool ShouldDrawDebugPlayableShell() const;
	bool ShouldDrawLegacyRouteMap() const;
	void DrawRouteMap(const UGameXXKMVPSubsystem* Subsystem);
	FVector2D GetRouteNodeCanvasPosition(const FVector2D& NormalizedPosition, const FVector2D& Origin, const FVector2D& Size) const;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKPlayableRootWidget> PlayableRootWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKOneGameRouteMapWidget> RouteMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKBattleBoardWidget> BattleBoardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	bool bShowDebugPlayableShell = false;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMainMenuWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKPlayableRootWidget> PlayableRootWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKOneGameRouteMapWidget> RouteMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> BattleBoardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	FString StartGameSlotNameOverride;

	UPROPERTY(Transient)
	int32 StartGameUserIndexOverride = 0;
};
