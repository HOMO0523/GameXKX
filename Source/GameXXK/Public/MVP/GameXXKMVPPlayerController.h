#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "UI/GameXXKBattleOverlayCoordinator.h"
#include "UI/GameXXKRouteEncounterPanelWidget.h"
#include "GameXXKMVPPlayerController.generated.h"

class UGameXXKBattleBoardWidget;
class UGameXXKBattleOverlayCoordinator;
class UGameXXKCompanionRosterWidget;
class UGameXXKInventoryWindowWidget;
class UGameXXKMainMenuWidget;
class UGameXXKMetaShopWidget;
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
class UGameXXKDesktopTrainingWorkbenchWidget;
class UWidget;
class AGameXXKRouteEncounterSceneActor;
struct FWorldContext;

enum class EGameXXKPlayerFlowBootProfile : uint8
{
	FullPlayerFlow,
	DesktopTrainingOnly
};

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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	void RefreshPlayerFlowWidgetsFromState();

	void EnterBattleOverlay();
	void ExitBattleOverlay();
	bool IsBattleOverlayActive() const;

	/** Single canonical battle-board instance shared by the flow and route bridge. */
	UGameXXKBattleBoardWidget* GetOrCreateBattleBoardWidget();

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
	UGameXXKMetaShopWidget* GetMetaShopWidgetForTest() const;

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
	bool IsMetaShopOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsMetaShopInputLockedForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsQuestDialogOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|PlayerFlow|Test")
	bool IsQuestDialogModalInputLockedForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow|Test")
	bool OpenQuestDialogPreviewForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenQuestDialogForNpc(AActor* QuestNpc, APawn* InstigatorPawn);

	/** Opens the contextual town NPC menu. Every named NPC offers Join; Tusi/Song also offer Story/Shop. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenTownNpcInteractionForNpc(AActor* TownNpc, APawn* InstigatorPawn);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool ExecutePendingTownNpcPrimaryAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool RecruitPendingTownNpc();

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

	// Kept only for serialized Blueprint compatibility. Town merchants now use
	// the seven-card meta shop and this legacy inventory-trade entry never opens.
	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow", meta = (DeprecatedFunction, DeprecationMessage = "Use OpenMetaShopWindow."))
	bool OpenMerchantTradeWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool OpenMetaShopWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseMetaShopWindow();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|PlayerFlow")
	bool CloseInventoryWindow();

	/** Explicit opt-in shell entry; 3D town remains the default rollback path. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining")
	bool OpenDesktopTrainingWorkbench();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining")
	bool CloseDesktopTrainingWorkbench();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	UGameXXKDesktopTrainingWorkbenchWidget* GetDesktopTrainingWorkbenchWidgetForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	void SetDesktopTrainingWorkbenchEnabledForTest(bool bEnabled);

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
	void SetDesktopTrainingBootProfileForTest(bool bEnabled);
	FString GetDesktopTrainingPerfProfileForTest() const;
	bool EnsureDesktopTrainingWidgetsForTest();
	bool ApplyDesktopTrainingPerfProfileForTest(const FString& Profile);
	bool PrepareForRuntimeStateMapTravelForTest(const FString& CurrentPackageName);
	/** Supplies already-resolved BattleBoard-local pointer coordinates to the real PlayerTick path in headless automation. */
	void SetBattleMousePositionOverrideForTest(FVector2D InMousePosition);
	void ClearBattleMousePositionOverrideForTest();
	void SetShouldPerformFullTickWhenPausedForTest(bool bEnabled);
#endif

private:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;
	EGameXXKPlayerFlowBootProfile ResolvePlayerFlowBootProfile() const;
	bool EnsureDesktopTrainingWidgets();
	bool ApplyDesktopTrainingPerfProfile(const FString& Profile);
	bool EnsurePlayerFlowWidgets();
	UGameXXKOneGameRouteMapWidget* EnsureRouteMapWidget();
	UGameXXKBattleBoardWidget* EnsureBattleBoardWidget();
	UGameXXKInventoryWindowWidget* EnsureInventoryWindowWidget();
	UGameXXKMetaShopWidget* EnsureMetaShopWidget();
	UGameXXKCompanionRosterWidget* EnsureCompanionRosterWidget();
	UGameXXKQuestDialogWidget* EnsureQuestDialogWidget();
	UGameXXKRouteEncounterPanelWidget* EnsureRouteEncounterPanelWidget();
	UGameXXKRouteMerchantWidget* EnsureRouteMerchantWidget();
	UGameXXKRelicBarWidget* EnsureRelicBarWidget();
	UGameXXKTaskPanelWidget* EnsureTaskPanelWidget();
	UGameXXKTownHudWidget* EnsureTownHudWidget();
	UGameXXKDesktopTrainingWorkbenchWidget* EnsureDesktopTrainingWorkbenchWidget();
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
	bool UpdateBattleTargetingPointer(FVector2D CursorScreenPosition);
	bool UpdateBattleTargetingPointerFromMouse();
	bool CanAddPlayerWidgetsToViewport() const;
	void HandleMetaShopClosed();
	void HandleMetaShopCompanionReplacementRequested();

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
	TSubclassOf<UGameXXKMetaShopWidget> MetaShopWidgetClass;

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

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|DesktopTraining")
	TSubclassOf<UGameXXKDesktopTrainingWorkbenchWidget> DesktopTrainingWorkbenchWidgetClass;

	/** Deliberately false: do not change the default 3D town entry before acceptance. */
	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|DesktopTraining")
	bool bEnableDesktopTrainingWorkbench = false;

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
	TObjectPtr<UGameXXKMetaShopWidget> MetaShopWidget;

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
	TObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> DesktopTrainingWorkbenchWidget;

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
	bool bMetaShopInputLocked = false;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	EGameXXKTrackedInputMode TrackedInputMode = EGameXXKTrackedInputMode::GameAndUI;
	bool bBattleOverlayAcquiredFullTickWhenPaused = false;
	bool bBattleOverlayAcquiredMoveInputIgnore = false;
	bool bBattleOverlayAcquiredLookInputIgnore = false;
	FDelegateHandle PreLoadMapWithContextDelegateHandle;

#if WITH_DEV_AUTOMATION_TESTS
	TOptional<EGameXXKPlayerFlowBootProfile> OverrideBootProfileForTest;
	bool bUseBattleMousePositionOverrideForTest = false;
	FVector2D BattleMousePositionOverrideForTest = FVector2D::ZeroVector;
#endif
};
