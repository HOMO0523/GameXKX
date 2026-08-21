#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKDesktopInventoryRules.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "UI/GameXXKTrainingTravelVisualRuntime.h"
#include "GameXXKDesktopTrainingWorkbenchWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UProgressBar;
class UScaleBox;
class USizeBox;
class UTextBlock;
class UTexture2D;
class UGameXXKInventoryWindowWidget;

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingNav : uint8
{
	None,
	Warehouse,
	Formation,
	Talents,
	Tools,
	Training
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingCenterPage : uint8
{
	Backpack,
	Formation,
	Talents
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingRightPanel : uint8
{
	None,
	TrainingMap,
	Tools
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingCharacterRoster : uint8
{
	Hero,
	Companions,
	Npcs
};

UENUM(BlueprintType)
enum class EGameXXKDesktopToolMode : uint8
{
	Dismantle,
	Combine,
	Enhance,
	Reforge,
	Socket
};

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

	bool HandleRightMouseButtonDown();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKDesktopTrainingWorkbenchWidget> Owner;
	int32 ActionId = INDEX_NONE;
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

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool PickUpBackpackSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool PickUpToolSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool CancelCarriedItemForTest();

	bool DropCarriedOnBackpackSlotForTest(int32 SlotIndex);
	void NotifyApplicationDeactivatedForTest();
	void ForceExternalSlateRebuildForTest();
	void DestructForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool RightClickBackpackSlotForTest(int32 SlotIndex);

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 FindBackpackItemSlotForTest(FName ItemId) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 FindFirstBackpackEquipmentSlotForTest() const;

	/** Embedded approved-backpack input seam. */
	bool HandleDesktopBackpackSlotLeftClicked(int32 SlotIndex);
	bool HandleDesktopBackpackSlotRightClicked(int32 SlotIndex);
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

	/** Explicitly commits the selected permanent-partner or task-NPC candidate. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ApplyFormationCandidateForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsWorkbenchVisibleForTest() const;

	/** Whether the independent settings surface is open above the backpack. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsSettingsPanelOpenForTest() const;

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
	bool HandleActionRightClicked(int32 ActionId);

	/** Set while a real Slate button callback is executing so layout rebuilds are deferred to the next tick. */
	bool bInActionCallback = false;

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual void NativeOnFocusLost(const FFocusEvent& InFocusEvent) override;
	virtual void NativeDestruct() override;

private:
	void BuildProgrammaticLayout();
	void RebuildLayoutNow();
	void BuildWorkbenchShell();
	void BuildBackpackTabToggle();
	void BuildTopToolbar();
	void BuildExitConfirmation();
	void BuildCarriedItemVisual();
	void BuildWarehousePanel();
	void BuildBackpackPanel();
	void BuildCharacterRosterTabs();
	void BuildFormationPanel();
	void BuildTalentsPanel();
	void BuildTrainingMapPanel();
	void BuildToolsPanel();
	void BuildTopIdleStrip();
	void BuildBottomNavigation();
	void RefreshLayout();
	void ScheduleCollapsedResourceUnload();
	void CancelCollapsedResourceUnload();
	void TickCollapsedResourceUnload(float InDeltaTime);
	void ReleaseCollapsedResources();
	void CaptureExpandedSessionState();
	void UpdateTravelVisuals();
	void EnsureTravelAtlasSession();
	void RequestTravelCombatAtlases(FName EnemyDefinitionId);
	TArray<FName> GetTravelCompanionUnitIds() const;
	void RequestTravelAtlas(const FGameXXKBattleAnimationClipDescriptor& Clip);
	void ReleaseTravelAtlasSession();
	bool ApplyTravelAnimationFrame(
		UImage* Image,
		const FGameXXKBattleAnimationClipDescriptor& Clip,
		bool bLooping,
		FSoftObjectPath& InOutAppliedPath,
		int32& InOutAppliedFrame);
	void ApplyAction(int32 ActionId);
	bool PickUpDesktopEntry(EGameXXKDesktopItemContainer Container, int32 SlotIndex);
	bool PickUpToolEntry(int32 SlotIndex);
	bool DropCarriedOnDesktopSlot(EGameXXKDesktopItemContainer Container, int32 SlotIndex);
	bool DropCarriedOnToolSlot(int32 SlotIndex);
	bool RouteBackpackRightClick(int32 SlotIndex);
	bool CancelCarriedItem();
	void CancelCarryForStructuralChange();
	void ReturnAllToolEntries();
	void AbortTransientInventoryInteraction(bool bReturnToolEntries, bool bRefreshLayout);
	void HandleApplicationActivationChanged(bool bIsActive);
	void HandlePersistenceBoundary();
	void UpdateCarriedItemVisualPosition();
	bool ToggleAlwaysOnTop();
	bool ToggleMuted();
	bool RequestExit();
	bool ConfirmExit(bool bExecutePlatformQuit);
	void SetNotice(const FText& Notice);
	FName ResolveRosterRepresentativeCharacterId(EGameXXKDesktopTrainingCharacterRoster Roster) const;
	void EnsureFormationCandidate();

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

	UPROPERTY(Transient)
	TObjectPtr<UScaleBox> RootScaleBox;

	UPROPERTY(Transient)
	TObjectPtr<USizeBox> ReferenceCanvasBox;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	/** Approved page-03 backpack reused inside the center crop host. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKInventoryWindowWidget> EmbeddedInventoryWidget;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NoticePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoticeText;

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
	TArray<TObjectPtr<UProgressBar>> TravelEnemyHealthBars;

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
	EGameXXKDesktopTrainingCharacterRoster ActiveCharacterRoster = EGameXXKDesktopTrainingCharacterRoster::Hero;
	EGameXXKDesktopTrainingCharacterRoster ActiveFormationRoster = EGameXXKDesktopTrainingCharacterRoster::Companions;
	FName SelectedStageId = NAME_None;
	FName ActiveBackpackCharacterId = NAME_None;
	FName FormationCandidateCharacterId = NAME_None;
	int32 WarehousePageIndex = 0;
	float TravelAccumulator = 0.0f;
	int32 TravelVisualNativeTickCount = 0;
	FGameXXKTrainingTravelVisualRuntime TravelVisualRuntime;
	FVector2D BackpackAspectRatio = FVector2D(1.76f, 1.0f);
	bool bSettingsPanelOpen = false;
	bool bBackpackExpanded = false;
	bool bWarehousePanelOpen = false;
	bool bAlwaysOnTop = false;
	bool bMuted = false;
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
	bool bHasCollapsedWorkbenchSession = false;
	bool bHasSavedEmbeddedInventorySession = false;
	FGameXXKEmbeddedInventorySessionState SavedEmbeddedInventorySession;
	TArray<FDesktopToolEntry> ToolSlots;
	FDesktopCarriedEntry CarriedEntry;
	FText LastNotice;
	FDelegateHandle ApplicationActivationHandle;
	FDelegateHandle PersistenceBoundaryHandle;
};
