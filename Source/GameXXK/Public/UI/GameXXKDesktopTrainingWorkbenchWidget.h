#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKTrainingRules.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKDesktopWorkbenchSessionState.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKInventoryItemPresentation.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "UI/GameXXKTrainingTravelVisualRuntime.h"
#include "GameXXKDesktopTrainingWorkbenchWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UCanvasPanelSlot;
class UImage;
class UOverlay;
class UProgressBar;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UVerticalBox;
class UGameXXKInventoryWindowWidget;
class UGameXXKGuideCoordinator;
class UGameXXKGuideOverlayWidget;
class UGameXXKGuidePreferenceWidget;
enum class EGameXXKGuidePreference : uint8;
struct FGameXXKGuideProgress;

DECLARE_DELEGATE_RetVal(bool, FGameXXKStoryCarriageRequested);

UCLASS()
class GAMEXXK_API UGameXXKDesktopTrainingStageButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKDesktopTrainingWorkbenchWidget* InOwner, FName InStageId);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKDesktopTrainingWorkbenchWidget> Owner;
	FName StageId = NAME_None;
};

UCLASS()
class GAMEXXK_API UGameXXKDesktopTrainingActionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKDesktopTrainingWorkbenchWidget* InOwner, int32 InActionId);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

	bool HandleRightMouseButtonDown();
	bool HandleMouseWheel(float WheelDelta);
	void SetScaleOnPress(bool bEnabled) { bScaleOnPress = bEnabled; }
	int32 GetConfiguredActionIdForTest() const { return ActionId; }

	UFUNCTION()
	void HandlePressed();

	UFUNCTION()
	void HandleReleased();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKDesktopTrainingWorkbenchWidget> Owner;
	int32 ActionId = INDEX_NONE;
	bool bScaleOnPress = false;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKDesktopTrainingWorkbenchWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining")
	bool OpenWorkbench();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining")
	bool CloseWorkbench();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining")
	bool OpenBackpack();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsBackpackExpandedForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsWarehousePanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsRightPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTopToolbarButtonCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTopToolbarAlwaysOnTopResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTopToolbarAlwaysOnTopOffResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsAlwaysOnTopForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsMutedForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsExitConfirmationOpenForTest() const;
	int32 GetHudScalePercentForTest() const { return HudScalePercent; }

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool CancelExitForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetToolSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetToolModeCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetOccupiedToolSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetToolSlotItemIdForTest(int32 SlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SetToolModeForTest(EGameXXKDesktopToolMode Mode);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ConfirmToolForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsCarryingItemForTest() const;

	/** True while one non-committing desktop entry preview is attached to the cursor. */
	bool HasDesktopCarriedEntry() const;

	FText GetLastDesktopInventoryNoticeForTest() const;
	EGameXXKDesktopNoticeCategory GetLastNoticeCategoryForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool PickUpBackpackSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool PickUpToolSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool CancelCarriedItemForTest();

	bool DropCarriedOnBackpackSlotForTest(int32 SlotIndex);
	bool DropCarriedOnToolSlotForTest(int32 SlotIndex)
	{
		return DropCarriedOnToolSlot(SlotIndex);
	}
	void NotifyApplicationDeactivatedForTest();
	bool CancelCarriedFromWorkbenchRightMouseForTest();
	void ForceExternalSlateRebuildForTest();
	void DestructForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool RightClickBackpackSlotForTest(int32 SlotIndex);
	bool RightClickWarehouseSlotForTest(int32 PhysicalSlotIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 FindBackpackItemSlotForTest(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 FindFirstBackpackEquipmentSlotForTest() const;

	/** Embedded approved-backpack input seam. */
	bool HandleDesktopBackpackSlotLeftClicked(int32 SlotIndex);
	bool HandleDesktopBackpackSlotRightClicked(int32 SlotIndex);
	bool HandleDesktopEquipmentSlotLeftClicked(EGameXXKEquipmentSlot EquipmentSlot);
	bool HandleDesktopEquipmentSlotAltClicked(EGameXXKEquipmentSlot EquipmentSlot);
	bool HandleDesktopSlotAltClicked(
		EGameXXKDesktopItemContainer Container,
		int32 SlotIndex);
	bool HandleDesktopToolSlotAltClicked(int32 SlotIndex);
	bool HandleDesktopCarryRightClicked();
	void HandleDesktopCharacterSubpageClicked(EGameXXKCharacterBackpackTab Tab);
	bool ShouldHideDesktopInventoryEntry(
		EGameXXKDesktopItemContainer Container,
		const FGameXXKDesktopInventoryEntryKey& Entry) const;

	/** Selected owner for the shared role/companion backpack surface. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetActiveBackpackCharacterIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	EGameXXKDesktopTrainingNav GetActiveNavForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	EGameXXKDesktopTrainingCenterPage GetActiveCenterPageForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsToolsPanelActiveForTest() const;

	/** Hero first, followed by the save-authoritative permanent companion instance IDs. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetBackpackCharacterIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetCompanionCharacterIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetNpcCharacterIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetEmbeddedBackpackCharacterIdForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SelectBackpackCharacterForTest(FName CharacterId);

	/** Selects a formation candidate without changing either authoritative party slot. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SelectFormationCandidateForTest(FName CharacterId);

	/** Explicitly commits the selected permanent-partner or owned-NPC candidate. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ApplyFormationCandidateForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsWorkbenchVisibleForTest() const;

	/** Whether the independent settings surface is open above the backpack. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsSettingsPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool HasResetCombatGuideButtonForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsGuidePreferencePromptVisibleForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ResetCombatGuideForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehouseColumnCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehouseRowCountForTest() const;

	/** The approved MasterV2 resources the programmatic shell binds at runtime. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FString> GetMasterV2ResourcePathsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FString> GetBottomNavigationIconResourcePathsForTest() const;

	FString GetBottomNavigationIconResourcePathForTest(EGameXXKDesktopTrainingNav Nav) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector2D GetBackpackAspectRatioForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetRuntimeGoldForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetPendingTravelGoldForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetPendingTravelNormalChestCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetPendingTravelAdvancedChestCountForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool CollectTravelRewardsForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehouseOccupancyForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehousePageCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehousePageIndexForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetVisibleWarehouseInstanceIdsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool NextWarehousePageForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool PreviousWarehousePageForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool QuickEquipVisibleWarehouseSlotForTest(int32 VisibleSlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SortWarehouseForTest();

	/** Removes one of the six active character equipment slots back into the warehouse. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool QuickUnequipActiveBackpackSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetVisibleBackpackItemIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTrainingStageButtonCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetSelectedStageIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetCurrentTravelStageIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool HasTravelVisualStripForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelVisualScrollOffsetForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTravelVisualWalkFrameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTravelVisualCompletedLoopCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTravelVisualNativeTickCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualAtlasResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool AreTravelCombatAtlasesOneKForTest() const;

#if WITH_DEV_AUTOMATION_TESTS
	void SetTravelAtlasCacheForTest(TUniquePtr<FGameXXKBattleAtlasCache> InAtlasCache);
	FSoftObjectPath GetTravelAppliedCompanionAtlasPathForTest(int32 CompanionIndex) const;
	int32 GetTravelAppliedCompanionFrameForTest(int32 CompanionIndex) const;
#endif

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualBackgroundResourcePathForTest() const;

	int32 GetTravelBackgroundTileCountForTest() const;
	int32 GetTravelVerboseTextBlockCountForTest() const;
	EGameXXKTrainingTravelVisualPhase GetTravelVisualPhaseForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualPhaseNameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelLogicalPhaseNameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualHeroActionNameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualPartyActionNameForTest(int32 PartyIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FString GetTravelVisualEnemyActionNameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsTravelVisualEnemyVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetTravelVisualEnemyDefinitionIdForTest() const;

	EGameXXKBattleAnimationAction GetTravelVisualHeroActionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelVisualEnemyHealthFractionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelVisualHeroHealthFractionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelVisualPartyHealthFractionForTest(int32 PartyIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelHeroHealthBarPercentForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	void SetTravelHeroHealthBarPercentForTest(float InPercent);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	void SetTravelHeroHealthBarFillColorForTest(FLinearColor InColor);

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetProgrammaticLayoutBuildCountForTestBlueprint() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool HasPendingLayoutRefreshForTestBlueprint() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector2D GetTravelHeroHealthBarSlateSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsTravelHeroHealthBarSlateValidForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelCompanionHealthBarPercentForTest(int32 CompanionIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelEnemyHealthBarPercentForTest(int32 EnemySlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetTravelHeroHealthBarRectForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetTravelCompanionHealthBarRectForTest(int32 CompanionIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetTravelEnemyHealthBarRectForTest(int32 EnemySlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	float GetTravelVisualScrollVelocityForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTravelVisualHeroRenderedFrameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTravelVisualEnemyRenderedFrameForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FText GetStageTooltipForTest(FName StageId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SelectStageForTest(FName StageId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ClickChallengeForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ClickTravelForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool AdvanceTravelForTest(int32 ElapsedSeconds = 1);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SetRetryOnFailureForTest(bool bEnabled);

	/** Drives the same low-cost tick path used by PIE automation without a live Slate viewport. */
	void TickForTest(float InDeltaTime);

	/** True when a travel tick requested a tree rebuild that waits for explicit UI refresh. */
	bool HasPendingLayoutRefreshForTest() const;

	/** Invokes NativeConstruct from an automation fixture without exposing it to Blueprint. */
	void ConstructForTest();
	/** Reproduces the live viewport detach/build/Slate-reattach sequence. */
	void SimulateViewportReattachForTest();

	/** Number of programmatic WidgetTree builds performed by this instance. */
	int32 GetProgrammaticLayoutBuildCountForTest() const;
	bool IsCollapsedResourceUnloadPendingForTest() const;
	bool AreCollapsedResourcesReleasedForTest() const;
	float GetCollapsedResourceUnloadRemainingSecondsForTest() const;
	int32 GetCollapsedGcRequestCountForTest() const;
	int32 GetEmbeddedInventoryWidgetCountForTest() const;
	EGameXXKCharacterBackpackTab GetEmbeddedBackpackTabForTest() const;
	TArray<FName> GetEmbeddedPendingDeckIdsForTest() const;

	void HandleStageClicked(FName StageId);
	void HandleActionClicked(int32 ActionId);
	bool HandleActionAltClicked(int32 ActionId);
	bool HandleActionRightClicked(int32 ActionId);
	void HandleActionHoverChanged(int32 ActionId, bool bHovered);
	bool HandleActionMouseWheel(int32 ActionId, float WheelDelta);
	int32 GetNoticeScrollOffsetForTest() const { return NoticeScrollOffset; }
	void NotifyDesktopNativeMoveCompleted();
	void NotifyDesktopNativeDisplayMetricsChanged();
	bool AttachDesktopNativeWindowForPresentation(void* NativeWindowHandle);
	void DetachDesktopNativeWindowForPresentation();
	void RefreshDesktopNativeMousePassthrough();
	void InitializeDesktopPresentationHostSize(const FVector2D& PhysicalWorkAreaSize);
	void SetPresentationMode(EGameXXKDesktopHudPresentationMode InMode);
	FGameXXKDesktopWorkbenchSessionState CaptureSessionStateForMapTravel();
	void RestoreSessionStateAfterMapTravel(
		const FGameXXKDesktopWorkbenchSessionState& State);
	void SetTownMapTravelPending(bool bPending);
	void SetStoryCarriageRequestedForTest(FGameXXKStoryCarriageRequested InRequest)
	{
		StoryCarriageRequested = MoveTemp(InRequest);
	}
	void SetTutorialMapInspectionRequestedForTest(
		FGameXXKTutorialMapInspectionRequested InRequest)
	{
		TutorialMapInspectionRequested = MoveTemp(InRequest);
	}
	EGameXXKDesktopHudPresentationMode GetPresentationModeForTest() const
	{
		return PresentationMode;
	}
	bool CanDragDesktopHudForTest() const
	{
		return PresentationMode == EGameXXKDesktopHudPresentationMode::DesktopWindow;
	}
	FVector2D GetDesktopWindowTopLeftForHost() const
	{
		return DesktopOverlayPlacement.HudTopLeft;
	}
	FVector2D GetDesktopWindowSizeForHost() const
	{
		return DesktopOverlayPlacement.HudSize;
	}
	float GetDesktopPresentationScaleForTest() const
	{
		return DesktopOverlayPlacement.Scale;
	}
	bool IsTownMapTravelPendingForTest() const { return bTownMapTravelPending; }

	/** Set while a real Slate button callback is executing so layout rebuilds are deferred to the next tick. */
	bool bInActionCallback = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseButtonUp(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual FReply NativeOnMouseMove(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnMouseCaptureLost(const FCaptureLostEvent& CaptureLostEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void NativeDestruct() override;

private:
	void BuildProgrammaticLayout();
	FVector2D GetCurrentDesignCanvasSize() const;
	float GetNoticePanelLogicalHeight() const;
	void ApplyUpwardExpansionTransforms();
	void TickDesktopNativeWindow();
	void UpdateTownPresentationInputLock();
	bool RequestTownToggle();
	bool RequestStoryCarriage();
	void ReleaseDesktopNativeWindow();
	void ApplyDesktopNativeWindowLayout(bool bForce);
	void SetDesktopNativeMousePassthrough(bool bEnabled);
	void UpdateExpansionDirectionFromNativeWindow();
	void LoadDesktopNativeWindowPosition();
	void SaveDesktopNativeWindowPosition();
	void UpdateDesktopOverlayPlacement(const FVector2D& HostSize);
	bool TryGetDesktopHudPointerScreenPosition(FVector2D& OutPointerScreen) const;
	void UpdateDesktopOverlayAnchorFromPointer();
	void RebuildLayoutNow();
	void BuildWorkbenchShell();
	void BuildTownToggleButton();
	void BuildStoryQuestButton();
	void BuildBackpackTabToggle();
	void BuildTopToolbar();
	void BuildHudSettingsPanel();
	void EnsureGuideSurfaces();
	void RefreshGuideSurfaces();
	void HandleGuidePreferenceChosen(EGameXXKGuidePreference Preference);
	void HandleGuideEvent(FName EventId);
	bool PersistGuideProgressCandidate(const FGameXXKGuideProgress& Candidate);
	bool HandleResetCombatGuide();
	void BuildExitConfirmation();
	void BuildCarriedItemVisual();
	void BuildWarehousePanel();
	void BuildBackpackPanel();
	void BuildSharedGoldIndicator();
	void BuildCharacterRosterTabs();
	void BuildFormationPanel();
	void BuildTalentsPanel();
	void BuildTrainingMapPanel();
	void BuildToolsPanel();
	void BuildPanelCloseButton(FName WidgetName, int32 ActionId, FVector2D Position);
	void BuildTopIdleStrip();
	void BuildIdleSummaryControls(const FVector2D& RowOrigin);
	void BuildNoticeRail();
	void BuildBottomNavigation();
	FVector2D GetNoticeRailLogicalPosition() const;
	FName ResolvePreferredTrainingStageForPage(
		EGameXXKTrainingDifficulty Difficulty,
		int32 Chapter) const;
	int32 ResolvePreferredTrainingChapter(EGameXXKTrainingDifficulty Difficulty) const;
	void SynchronizeTrainingPageFromStage(FName StageId);
	void RefreshLayout();
	void ScheduleCollapsedResourceUnload();
	void CancelCollapsedResourceUnload();
	void TickCollapsedResourceUnload(float InDeltaTime);
	void ReleaseCollapsedResources();
	void CaptureExpandedSessionState();
	void PreserveEmbeddedSessionForLocalClose();
	void UpdateTravelVisuals();
	void RefreshLivePresentation(bool bForce);
	void EnsureTravelAtlasSession();
	void RequestTravelCombatAtlases(FName EnemyDefinitionId);
	TArray<FName> GetTravelCompanionUnitIds() const;
	void RequestTravelAtlas(const FGameXXKBattleAnimationClipDescriptor& Clip);
	void RequestTravelAtlas(const FGameXXKBattleAnimationClipPair& ClipPair);
	void ReleaseTravelAtlasSession();
	bool ApplyTravelAnimationFrame(
		UImage* Image,
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		bool bLooping,
		FSoftObjectPath& InOutAppliedPath,
		int32& InOutAppliedFrame);
	bool ApplyTravelAnimationFrame(
		UImage* Image,
		const FGameXXKBattleAnimationClipPair& ClipPair,
		bool bLooping,
		FSoftObjectPath& InOutAppliedPath,
		int32& InOutAppliedFrame);
	void ApplyAction(int32 ActionId);
	bool PickUpDesktopEntry(EGameXXKDesktopItemContainer Container, int32 SlotIndex);
	bool PickUpToolEntry(int32 SlotIndex);
	bool DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer Container, int32 SlotIndex);
	bool DropCarriedOnToolSlot(int32 SlotIndex);
	TSet<FGameXXKDesktopInventoryEntryKey> BuildBatchTransferExclusions() const;
	bool ToggleDesktopEntryLock(const FGameXXKDesktopInventoryEntryKey& Entry);
	bool RouteBackpackRightClick(int32 SlotIndex);
	bool RequestTutorialMapInspection(
		const FGameXXKDesktopInventoryEntryKey& Entry);
	bool CancelCarriedItem();
	void CancelCarryForStructuralChange();
	void ReturnAllToolEntries();
	void CloseWarehousePanelToParent();
	void CloseCentralPageToBackpack();
	void CloseRightPanelToParent();
	void ResetWorkbenchChildrenForGlobalClose();
	void AbortTransientInventoryInteraction(bool bReturnToolEntries, bool bRefreshLayout);
	void HandleApplicationActivationChanged(bool bIsActive);
	bool HandleWorkbenchRightMouseCancel();
	void HandlePersistenceBoundary();
	void HandleTalentPurchaseCommitted();
	void UpdateCarriedItemVisualPosition();
	bool ToggleAlwaysOnTop();
	bool ToggleMuted();
	void LoadHudScaleSetting();
	bool SetHudScalePercent(int32 InPercent);
	bool RequestExit();
	bool ConfirmExit(bool bExecutePlatformQuit);
	void SetNotice(
		const FText& Notice,
		EGameXXKDesktopNoticeCategory Category = EGameXXKDesktopNoticeCategory::System);
	void RefreshNoticePresentation();
	void RefreshNoticeControlVisibility();
	bool IsNoticeAction(int32 ActionId) const;
	void LoadNoticeCategorySettings();
	void SaveNoticeCategorySetting(EGameXXKDesktopNoticeCategory Category) const;
	FName ResolveRosterRepresentativeCharacterId(EGameXXKDesktopTrainingCharacterRoster Roster) const;
	FName ResolveRememberedBackpackCharacterId(EGameXXKDesktopTrainingCharacterRoster Roster) const;
	void PreserveEmbeddedSessionForCharacter(FName CharacterId);
	void EnsureFormationCandidate();

	struct FLivePresentationSnapshot
	{
		FName TravelStageId = NAME_None;
		int32 TravelEncounterIndex = INDEX_NONE;
		int32 PlayerGold = 0;
		int32 PlayerLevel = 0;
		int32 PlayerExperience = 0;
		int32 PlayerHealth = 0;
		int32 PlayerMana = 0;
		int32 PendingGold = 0;
		int32 PendingExperience = 0;
		int32 PendingNormalChests = 0;
		int32 PendingAdvancedChests = 0;
		int32 HeldNormalChests = 0;
		int32 HeldAdvancedChests = 0;
		int32 NormalChestCooldown = 0;
		int32 AdvancedChestCooldown = 0;
		int32 WarehouseOccupancy = 0;
		int32 WarehousePageCount = 1;
		int32 ToolLevel = 1;
		int64 ToolExperience = 0;
		int32 ToolCraftingLevel = 1;
		int32 OccupiedToolSlots = 0;

		bool Equals(const FLivePresentationSnapshot& Other) const
		{
			return TravelStageId == Other.TravelStageId
				&& TravelEncounterIndex == Other.TravelEncounterIndex
				&& PlayerGold == Other.PlayerGold
				&& PlayerLevel == Other.PlayerLevel
				&& PlayerExperience == Other.PlayerExperience
				&& PlayerHealth == Other.PlayerHealth
				&& PlayerMana == Other.PlayerMana
				&& PendingGold == Other.PendingGold
				&& PendingExperience == Other.PendingExperience
				&& PendingNormalChests == Other.PendingNormalChests
				&& PendingAdvancedChests == Other.PendingAdvancedChests
				&& HeldNormalChests == Other.HeldNormalChests
				&& HeldAdvancedChests == Other.HeldAdvancedChests
				&& NormalChestCooldown == Other.NormalChestCooldown
				&& AdvancedChestCooldown == Other.AdvancedChestCooldown
				&& WarehouseOccupancy == Other.WarehouseOccupancy
				&& WarehousePageCount == Other.WarehousePageCount
				&& ToolLevel == Other.ToolLevel
				&& ToolExperience == Other.ToolExperience
				&& ToolCraftingLevel == Other.ToolCraftingLevel
				&& OccupiedToolSlots == Other.OccupiedToolSlots;
		}
	};

	FLivePresentationSnapshot CaptureLivePresentationSnapshot() const;
	void UpdateWaveProgressPresentation(const FLivePresentationSnapshot& Snapshot);
	void UpdateTrainingChestPresentation(bool bAdvanced, int32 Count);
	void UpdateWarehouseNumericPresentation(const FLivePresentationSnapshot& Snapshot);
	void UpdateToolNumericPresentation(const FLivePresentationSnapshot& Snapshot);

	struct FDesktopToolEntry
	{
		FGameXXKDesktopInventoryEntryKey Entry;
		EGameXXKDesktopItemContainer AuthoritativeContainer = EGameXXKDesktopItemContainer::Backpack;
		int32 AuthoritativeSlotIndex = INDEX_NONE;
		int32 Quantity = 0;
		FString IconPath;

		bool IsValid() const { return Entry.IsValid(); }
	};

	struct FDesktopCarriedEntry
	{
		FDesktopToolEntry Payload;
		bool bOriginIsTool = false;
		int32 OriginToolSlotIndex = INDEX_NONE;

		bool IsValid() const { return Payload.IsValid(); }
		void Reset() { *this = FDesktopCarriedEntry(); }
	};

	enum class EDesktopNoticeDisplayMode : uint8
	{
		Single,
		Medium,
		Long
	};

	struct FDesktopNoticeEntry
	{
		EGameXXKDesktopNoticeCategory Category = EGameXXKDesktopNoticeCategory::System;
		FText Message;
		uint64 Ordinal = 0;
	};

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> DesktopOverlayRootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> HudDesignCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> RootCanvasDesignSlot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanelSlot> DesktopHudCanvasSlot;

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> RootScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ReferenceCanvasBox;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> TownToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> StoryQuestButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> ResetCombatGuideButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuideOverlayWidget> GuideOverlayWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuidePreferenceWidget> GuidePreferenceWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKGuideCoordinator> GuideCoordinator;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> IdleGroupCanvas;

	/** Approved page-03 backpack reused inside the center crop host. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKInventoryWindowWidget> EmbeddedInventoryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NoticePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> NoticeSurfaceButton;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> NoticeRecordsBar;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NoticeSettingsPanel;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> NoticeLineTexts;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BackpackGoldText;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> TrainingNormalChestButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> TrainingAdvancedChestButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingNormalChestCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingAdvancedChestCountText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TrainingWaveProgressFill;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingWaveStageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingWaveIndexText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TrainingWaveMarkerImages;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingFoldedNormalChestText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TrainingFoldedAdvancedChestText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WarehousePageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> WarehouseFooterText;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> WarehouseBatchToBackpackButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> BackpackBatchToWarehouseButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDesktopTrainingActionButton>> WarehousePageButtons;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToolProgressText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ToolCraftLevelText;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDesktopTrainingActionButton> ToolConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CarriedItemImage;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDesktopTrainingStageButton>> StageButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDesktopTrainingActionButton>> ActionButtons;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TravelStageText;

	/** Clipped top-strip surface for the seamless Travel lane. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TravelVisualViewport;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelBackgroundImageA;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelBackgroundImageB;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TravelBackgroundImages;

	/** Three fixed enemy formation slots; unused slots stay collapsed. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TravelEnemyImages;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelHeroImage;

	/** Selected permanent companion followed by the active route quest NPC. */
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TravelCompanionImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> TravelEnemyHealthTracks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> TravelEnemyHealthFills;

	UPROPERTY(Transient)
	TObjectPtr<UProgressBar> TravelHeroHealth;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UProgressBar>> TravelCompanionHealthBars;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TravelHeroAtlasTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TravelBackgroundTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TravelHeroFallbackTextures;

	TUniquePtr<FGameXXKBattleAtlasCache> TravelAtlasCache;
	uint64 TravelAtlasSessionToken = 0;
	TSet<FSoftObjectPath> TravelRequestedAtlasPaths;
	TSet<FSoftObjectPath> TravelPinnedAtlasPaths;
	TMap<FSoftObjectPath, TWeakObjectPtr<UTexture2D>> TravelLoadedAtlasTextures;
	FSoftObjectPath TravelAppliedHeroAtlasPath;
	TArray<FSoftObjectPath> TravelAppliedEnemyAtlasPaths;
	TArray<FSoftObjectPath> TravelAppliedCompanionAtlasPaths;
	int32 TravelAppliedHeroFrame = INDEX_NONE;
	TArray<int32> TravelAppliedEnemyFrames;
	TArray<int32> TravelAppliedCompanionFrames;
	float TravelAppliedHeroHealth = -1.0f;
	TArray<float> TravelAppliedEnemyHealth;
	TArray<float> TravelAppliedCompanionHealth;

	EGameXXKDesktopTrainingNav ActiveNav = EGameXXKDesktopTrainingNav::None;
	EGameXXKDesktopTrainingCenterPage ActiveCenterPage = EGameXXKDesktopTrainingCenterPage::Backpack;
	EGameXXKDesktopTrainingRightPanel RightPanel = EGameXXKDesktopTrainingRightPanel::None;
	EGameXXKDesktopToolMode ActiveToolMode = EGameXXKDesktopToolMode::Dismantle;
	EGameXXKDesktopHudPresentationMode PresentationMode =
		EGameXXKDesktopHudPresentationMode::DesktopWindow;
	EGameXXKToolCombineKind ActiveToolCombineKind = EGameXXKToolCombineKind::Equipment;
	int32 SelectedToolSocketIndex = 0;
	EGameXXKDesktopTrainingCharacterRoster ActiveCharacterRoster = EGameXXKDesktopTrainingCharacterRoster::Hero;
	EGameXXKDesktopTrainingCharacterRoster ActiveFormationRoster = EGameXXKDesktopTrainingCharacterRoster::Companions;
	FName SelectedStageId = NAME_None;
	int32 ActiveTrainingDifficultyIndex = 0;
	int32 ActiveTrainingChapter = 1;
	FName ActiveBackpackCharacterId = NAME_None;
	FName LastCompanionBackpackCharacterId = NAME_None;
	FName LastNpcBackpackCharacterId = NAME_None;
	FName FormationCandidateCharacterId = NAME_None;
	int32 WarehousePageIndex = 0;
	float TravelAccumulator = 0.0f;
	float LivePresentationAccumulator = 0.0f;
	FLivePresentationSnapshot LastLivePresentationSnapshot;
	bool bHasLivePresentationSnapshot = false;
	int32 TravelVisualNativeTickCount = 0;
	FGameXXKTrainingTravelVisualRuntime TravelVisualRuntime;
	FVector2D BackpackAspectRatio = FVector2D(1.76f, 1.0f);
	bool bSettingsPanelOpen = false;
	bool bBackpackExpanded = false;
	bool bIdleStripFolded = false;
	bool bExpandUpward = false;
	bool bWarehousePanelOpen = false;
	bool bTrainingDifficultyDropdownOpen = false;
	bool bRestoreTrainingPanelAfterChallenge = false;
	FGuid RouteSettlementReceiptAtChallengeStart;
	bool bCharacterRosterMembersExpanded = false;
	bool bAlwaysOnTop = true;
	bool bMuted = false;
	int32 HudScalePercent = 100;
	bool bHudScaleSettingLoaded = false;
	void* DesktopNativeWindowHandle = nullptr;
	void* DesktopPreviousWindowProc = nullptr;
	FVector2D DesktopWindowPositionNormalized = FVector2D(0.5f, 0.08f);
	FVector2D DesktopOverlayHostSize = FVector2D(1920.0f, 1020.0f);
	FVector2D DesktopHudDragStartPointerScreen = FVector2D::ZeroVector;
	FVector2D DesktopHudDragStartNormalizedAnchor = FVector2D::ZeroVector;
	FIntPoint DesktopWorkAreaOrigin = FIntPoint::ZeroValue;
	GameXXKDesktopTrainingLayout::FDesktopOverlayPlacement DesktopOverlayPlacement;
	GameXXKDesktopTrainingLayout::FDesktopHudResolvedMetrics DesktopResolvedMetrics;
	bool bDesktopWindowPositionLoaded = false;
	bool bDesktopResolvedMetricsValid = false;
	bool bDesktopNativeHookInstalled = false;
	bool bDesktopNativeMousePassthrough = false;
	bool bDesktopNativeLayoutDirty = true;
	bool bDesktopNativeMoveSavePending = false;
	bool bDesktopHudDragging = false;
	bool bTownMapTravelPending = false;
	FGameXXKStoryCarriageRequested StoryCarriageRequested;
	FGameXXKTutorialMapInspectionRequested TutorialMapInspectionRequested;
	bool bDesktopNativeLastExpanded = false;
	int32 DesktopNativeLastHudScalePercent = INDEX_NONE;
	float DesktopInputDpiScale = 1.0f;
	float UnmutedVolumeMultiplier = 1.0f;
	bool bExitConfirmationOpen = false;
	bool bNativeTickActive = false;
	bool bLayoutRefreshPending = false;
	bool bLayoutRebuildScheduled = false;
	bool bInternalLayoutRebuild = false;
	int32 ProgrammaticLayoutBuildCount = 0;
	static constexpr float CollapsedResourceUnloadDelaySeconds = 3.0f;
	float CollapsedResourceUnloadRemainingSeconds = 0.0f;
	uint64 CollapsedResourceGeneration = 0;
	int32 CollapsedGcRequestCount = 0;
	bool bCollapsedResourceUnloadPending = false;
	bool bCollapsedResourcesReleased = false;
	bool bHasSavedEmbeddedInventorySession = false;
	FGameXXKEmbeddedInventorySessionState SavedEmbeddedInventorySession;
	TArray<FDesktopToolEntry> ToolSlots;
	FDesktopCarriedEntry CarriedEntry;
	FText LastNotice;
	TArray<FDesktopNoticeEntry> NoticeHistory;
	TMap<EGameXXKDesktopNoticeCategory, bool> NoticeCategoryEnabled;
	EDesktopNoticeDisplayMode NoticeDisplayMode = EDesktopNoticeDisplayMode::Single;
	int32 NoticeScrollOffset = 0;
	int32 NoticeHoveredWidgetCount = 0;
	uint64 NextNoticeOrdinal = 1;
	float NoticeHoverHideRemainingSeconds = 0.0f;
	bool bNoticeSettingsOpen = false;
	bool bNoticeSettingsLoaded = false;
	FDelegateHandle ApplicationActivationHandle;
	FDelegateHandle PersistenceBoundaryHandle;
	FDelegateHandle GuideEventHandle;
};
