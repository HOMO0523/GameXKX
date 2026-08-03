#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameFramework/PlayerController.h"
#include "UI/GameXXKBattleOverlayCoordinator.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "GameXXKMVPPlayerController.generated.h"

class UGameXXKBattleBoardWidget;
class UGameXXKBattleOverlayCoordinator;
class UGameXXKCompanionRosterWidget;
class UGameXXKInventoryWindowWidget;
class UGameXXKMainMenuWidget;
class UGameXXKMVPSubsystem;
class UGameXXKOneGameRouteMapWidget;
class UGameXXKQuestDialogWidget;
class UGameXXKRouteEncounterPanelWidget;
class UGameXXKRouteMerchantWidget;
class UGameXXKRelicBarWidget;
class UGameXXKTaskPanelWidget;
class UGameXXKTownHudWidget;
class UGameXXKTownOverlayWidget;
class UGameXXKWorldMapWidget;
class UWidget;
class ACameraActor;
class AGameXXKBattleScenePresenter;
class AGameXXKBattleSceneUnitActor;
class AGameXXKRouteEncounterSceneActor;
struct FGameXXKRuntimeState;
struct FWorldContext;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPPlayerController : public APlayerController, public IGameXXKBattleOverlayHost
{
	GENERATED_BODY()

public:
	AGameXXKMVPPlayerController();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
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

	/** Direct development seam for the same battle-camera configuration used by live PIE. */
	void ConfigureBattleSceneCameraForTest(ACameraActor* CameraActor);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	void RefreshPlayerFlowWidgetsFromState();

	void EnterBattleOverlay();
	void ExitBattleOverlay();
	bool IsBattleOverlayActive() const;

	virtual FGameXXKBattleOverlaySnapshot CaptureBattleOverlaySnapshot(
		const UGameXXKOneGameRouteMapWidget& RouteWidget) const override;
	virtual bool ApplyBattleOverlayEntry(
		UGameXXKOneGameRouteMapWidget& RouteWidget,
		UGameXXKBattleBoardWidget& BattleWidget,
		uint64 SessionToken) override;
	virtual void CancelBattleVisualLoads(uint64 ClosingSessionToken) override;
	virtual void RestoreBattleOverlaySnapshot(
		const FGameXXKBattleOverlaySnapshot& Snapshot,
		UGameXXKOneGameRouteMapWidget* RouteWidget,
		UGameXXKBattleBoardWidget* BattleWidget) override;

	/** Queues fullscreen HUD combat presentation after a card-state mutation. */
	void RefreshBattleSceneAfterCardMutation(FName AttackerUnitId, const TArray<FGameXXKCardDamageResult>& DamageResults);

	/** Pure policy: only a real HP hit on the runtime Party Hero requests the small camera shake. */
	static bool ShouldTriggerHeroHitCameraShake(const FGameXXKRuntimeState& State, const FGameXXKCardDamageResult& DamageResult);

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKMainMenuWidget* GetMainMenuWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKWorldMapWidget* GetWorldMapWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKTownOverlayWidget* GetTownOverlayWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKOneGameRouteMapWidget* GetRouteMapWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKBattleBoardWidget* GetBattleBoardWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKInventoryWindowWidget* GetInventoryWindowWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKCompanionRosterWidget* GetCompanionRosterWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKQuestDialogWidget* GetQuestDialogWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKTaskPanelWidget* GetTaskPanelWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKRouteEncounterPanelWidget* GetRouteEncounterPanelWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKRouteMerchantWidget* GetRouteMerchantWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsRouteMerchantWidgetOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsRouteMerchantInputLockedForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKRelicBarWidget* GetRelicBarWidgetForTest() const { return RelicBarWidget; }

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	UGameXXKTownHudWidget* GetTownHudWidgetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasMainMenuWidgetInViewportForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool HasWorldMapWidgetInViewportForTest() const;

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

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsQuestDialogOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsQuestDialogModalInputLockedForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool OpenQuestDialogPreviewForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenQuestDialogForNpc(AActor* QuestNpc, APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenTaskOfferPanelForNpc(AActor* QuestNpc, APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool AcceptQuestDialog();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool AcceptTaskOfferById(FName TaskId);

	// Kept only for serialized Blueprint compatibility. Task offers must carry
	// their selected id, so this legacy entry point intentionally cannot accept.
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow", meta = (DeprecatedFunction, DeprecationMessage = "Use AcceptTaskOfferById with the clicked task id."))
	bool AcceptTaskOffer();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseQuestDialog();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenFreeInventoryWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenMerchantTradeWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseInventoryWindow();

	/** Opens the permanent-partner backpack in town without replacing the task-NPC codex. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenCompanionRoster();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseCompanionRoster();

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsCompanionRosterOpenForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenTaskPanel();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseTaskPanel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsTaskPanelOpenForTest() const;

	/**
	 * Headless automation seam for route-panel presentation.  A live game-world
	 * caller must use OpenRouteEncounterPanelFromActor so focus and source
	 * identity are preserved.
	 */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenRouteEncounterPanel();

	/** Opens a route panel only when SourceActor is the player's exact focused encounter. */
	bool OpenRouteEncounterPanelFromActor(AGameXXKRouteEncounterSceneActor* SourceActor);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseRouteEncounterPanel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsRouteEncounterPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	AGameXXKRouteEncounterSceneActor* GetRouteEncounterSourceActorForTest() const;

	/** Dispatches one visible route-panel choice into the existing route rules. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool ResolveRouteEncounterAction(EGameXXKRouteEncounterAction Action);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool OpenBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool ToggleBattleCommandMenuForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool ConfirmBattleTargetForUnitForTest(AGameXXKBattleSceneUnitActor* UnitActor);

	/** Stable HUD bridge. The fullscreen board can submit a canonical unit id without a scene actor. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool ConfirmBattleTargetForUnitId(FName UnitId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool CancelBattleTargetingForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool UpdateBattleTargetingPointerForTest(FVector2D CursorScreenPosition);

#if WITH_DEV_AUTOMATION_TESTS
	EGameXXKTrackedInputMode GetTrackedInputModeForTest() const { return TrackedInputMode; }
	void SetTrackedInputModeForTest(EGameXXKTrackedInputMode InputMode);
	bool PrepareForRuntimeStateMapTravelForTest(const FString& CurrentPackageName);
	// Test-only seam for exercising InputKey -> TryHandleBattleSceneLeftClick
	// without a Slate viewport-backed hardware cursor.
	void SetBattleSceneCursorHitOverrideForTest(AGameXXKBattleSceneUnitActor* InUnitActor);
	void ClearBattleSceneCursorHitOverrideForTest();
	/** Uses a deterministic projection only for headless controller-bridge automation. */
	void SetBattleWorldProjectionOverrideForTest(bool bEnabled);
	/** Supplies already-resolved BattleBoard-local pointer coordinates to the real PlayerTick path in headless automation. */
	void SetBattleMousePositionOverrideForTest(FVector2D InMousePosition);
	void ClearBattleMousePositionOverrideForTest();
	void SetShouldPerformFullTickWhenPausedForTest(bool bEnabled);
#endif

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;
	bool EnsurePlayerFlowWidgets();
	UGameXXKInventoryWindowWidget* EnsureInventoryWindowWidget();
	UGameXXKCompanionRosterWidget* EnsureCompanionRosterWidget();
	UGameXXKQuestDialogWidget* EnsureQuestDialogWidget();
	UGameXXKRouteEncounterPanelWidget* EnsureRouteEncounterPanelWidget();
	UGameXXKRouteMerchantWidget* EnsureRouteMerchantWidget();
	UGameXXKRelicBarWidget* EnsureRelicBarWidget();
	UGameXXKTaskPanelWidget* EnsureTaskPanelWidget();
	UGameXXKTownHudWidget* EnsureTownHudWidget();
	UGameXXKWorldMapWidget* EnsureWorldMapWidget();
	bool ConfirmPendingQuestNpc(FName TaskId);
	void RefreshPlayerFlowWidgets();
	void ConfigureRouteMapWidgetViewport(UGameXXKOneGameRouteMapWidget* RouteWidget) const;
	void ApplyPlayerFlowInputMode();
	void SetTrackedInputMode(EGameXXKTrackedInputMode InputMode, UWidget* WidgetToFocus = nullptr);
	bool PrepareForRuntimeStateMapTravel(const FString& CurrentPackageName);
	void HandlePreLoadMapWithContext(const FWorldContext& WorldContext, const FString& MapName);
	void HandleRouteMapPrimaryClick();
	bool TryHandleRouteEncounterInteract();
	AGameXXKRouteEncounterSceneActor* GetFocusedRouteEncounterActor() const;
	bool OpenRouteEncounterPanelInternal(AGameXXKRouteEncounterSceneActor* SourceActor);
	bool HasValidRouteEncounterContext() const;
	void ClearRouteEncounterContext();
	void EnsureBattleScenePresenter();
	void RefreshBattleCardTargetingBridge();
	bool ProjectBattleWorldLocationToWidgetPosition(FVector WorldLocation, FVector2D& OutScreenPosition) const;
	void ApplyBattleSceneCamera();
	bool TryHandleBattleSceneRightClick();
	bool TryHandleBattleSceneLeftClick();
	bool ConfirmBattleTargetForSceneUnit(AGameXXKBattleSceneUnitActor* UnitActor);
	bool ToggleBattleCommandMenuForUnit(AGameXXKBattleSceneUnitActor* UnitActor, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);
	bool UpdateBattleTargetingPointer(FVector2D CursorScreenPosition);
	bool UpdateBattleTargetingPointerFromMouse();
	AGameXXKBattleSceneUnitActor* FindBattleSceneUnitUnderCursor(bool bRequireEnemyTarget) const;
	AGameXXKBattleSceneUnitActor* FindAnyBattleSceneUnitUnderCursor() const;
	AActor* FindBattleSceneCameraActor() const;
	bool CanAddPlayerWidgetsToViewport() const;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKMainMenuWidget> MainMenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKWorldMapWidget> WorldMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKTownOverlayWidget> TownOverlayWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKOneGameRouteMapWidget> RouteMapWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKBattleBoardWidget> BattleBoardWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKInventoryWindowWidget> InventoryWindowWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKCompanionRosterWidget> CompanionRosterWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKQuestDialogWidget> QuestDialogWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKRouteEncounterPanelWidget> RouteEncounterPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKRouteMerchantWidget> RouteMerchantWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKRelicBarWidget> RelicBarWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKTaskPanelWidget> TaskPanelWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|PlayerFlow")
	TSubclassOf<UGameXXKTownHudWidget> TownHudWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMainMenuWidget> MainMenuWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKWorldMapWidget> WorldMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTownOverlayWidget> TownOverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKOneGameRouteMapWidget> RouteMapWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> BattleBoardWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleOverlayCoordinator> BattleOverlayCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKInventoryWindowWidget> InventoryWindowWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCompanionRosterWidget> CompanionRosterWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKQuestDialogWidget> QuestDialogWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterPanelWidget> RouteEncounterPanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteMerchantWidget> RouteMerchantWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRelicBarWidget> RelicBarWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTaskPanelWidget> TaskPanelWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTownHudWidget> TownHudWidget;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> PendingQuestNpc;

	UPROPERTY(Transient)
	TWeakObjectPtr<APawn> PendingQuestInstigator;

	UPROPERTY(Transient)
	TWeakObjectPtr<AGameXXKRouteEncounterSceneActor> ActiveRouteEncounterSourceActor;

	UPROPERTY(Transient)
	EGameXXKScreen ActiveRouteEncounterScreen = EGameXXKScreen::MainMenu;

	UPROPERTY(Transient)
	int32 ActiveRouteEncounterNodeId = INDEX_NONE;

	UPROPERTY(Transient)
	bool bRouteMerchantInputLocked = false;

	UPROPERTY(Transient)
	TObjectPtr<AGameXXKBattleScenePresenter> BattleScenePresenter;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	EGameXXKTrackedInputMode TrackedInputMode = EGameXXKTrackedInputMode::GameAndUI;
	bool bBattleOverlayAcquiredFullTickWhenPaused = false;
	bool bBattleOverlayAcquiredMoveInputIgnore = false;
	bool bBattleOverlayAcquiredLookInputIgnore = false;
	FDelegateHandle PreLoadMapWithContextDelegateHandle;

#if WITH_DEV_AUTOMATION_TESTS
	bool bUseBattleSceneCursorHitOverrideForTest = false;
	TWeakObjectPtr<AGameXXKBattleSceneUnitActor> BattleSceneCursorHitOverrideForTest;
	bool bUseBattleWorldProjectionOverrideForTest = false;
	bool bUseBattleMousePositionOverrideForTest = false;
	FVector2D BattleMousePositionOverrideForTest = FVector2D::ZeroVector;
#endif
};
