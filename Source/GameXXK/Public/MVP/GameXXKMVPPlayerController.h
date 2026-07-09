#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameXXKMVPPlayerController.generated.h"

class UGameXXKBattleBoardWidget;
class UGameXXKInventoryWindowWidget;
class UGameXXKMainMenuWidget;
class UGameXXKMVPSubsystem;
class UGameXXKOneGameRouteMapWidget;
class UGameXXKTownOverlayWidget;
class AGameXXKBattleScenePresenter;
class AGameXXKBattleSceneUnitActor;
class AGameXXKRouteEncounterSceneActor;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	AGameXXKMVPPlayerController();

	virtual void BeginPlay() override;
	virtual void PlayerTick(float DeltaTime) override;
	virtual void SetupInputComponent() override;
	virtual bool InputKey(const FInputKeyEventArgs& Params) override;
	virtual void FlushPressedKeys() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool EnsurePlayerFlowWidgetsForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	void RefreshPlayerFlowWidgetsForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	void RefreshPlayerFlowWidgetsFromState();

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKMainMenuWidget* GetMainMenuWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKTownOverlayWidget* GetTownOverlayWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKOneGameRouteMapWidget* GetRouteMapWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKBattleBoardWidget* GetBattleBoardWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKInventoryWindowWidget* GetInventoryWindowWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasMainMenuWidgetInViewportForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasTownOverlayWidgetInViewportForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasRouteMapWidgetInViewportForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasBattleBoardWidgetInViewportForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsInventoryWindowModalInputLockedForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow")
	bool IsInventoryWindowModalInputLocked() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenFreeInventoryWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenMerchantTradeWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseInventoryWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool OpenBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool ToggleBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool ConfirmBattleTargetForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool CancelBattleTargetingForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool UpdateBattleTargetingPointerForTest(FVector2D CursorScreenPosition);

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;
	bool EnsurePlayerFlowWidgets();
	UGameXXKInventoryWindowWidget* EnsureInventoryWindowWidget();
	void RefreshPlayerFlowWidgets();
	void ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const;
	void ApplyPlayerFlowInputMode();
	void HandleRouteMapPrimaryClick();
	bool TryHandleRouteEncounterInteract();
	AGameXXKRouteEncounterSceneActor* FindRouteEncounterActorForActiveScreen() const;
	void EnsureBattleScenePresenter();
	void ApplyBattleSceneCamera();
	bool TryHandleBattleSceneRightClick();
	bool TryHandleBattleSceneLeftClick();
	bool ToggleBattleCommandMenuForUnit(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);
	bool UpdateBattleTargetingPointer(FVector2D CursorScreenPosition);
	bool UpdateBattleTargetingPointerFromMouse();
	AGameXXKBattleSceneUnitActor* FindBattleSceneUnitUnderCursor(bool bRequireEnemyTarget) const;
	AActor* FindBattleSceneCameraActor() const;
	bool CanAddPlayerWidgetsToViewport() const;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKTownOverlayWidget> TownOverlayWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKOneGameRouteMapWidget> RouteMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKBattleBoardWidget> BattleBoardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKInventoryWindowWidget> InventoryWindowWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMainMenuWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTownOverlayWidget> TownOverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKOneGameRouteMapWidget> RouteMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> BattleBoardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKInventoryWindowWidget> InventoryWindowWidget;

	UPROPERTY(Transient)
	TObjectPtr<AGameXXKBattleScenePresenter> BattleScenePresenter;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;
};
