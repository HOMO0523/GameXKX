#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "UI/GameXXKTrainingTravelVisualRuntime.h"
#include "GameXXKDesktopTrainingWorkbenchWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UTexture2D;
class UGameXXKBattleBoardWidget;

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingNav : uint8
{
	Warehouse,
	Formation,
	Talents,
	Tools,
	Training
};

UENUM(BlueprintType)
enum class EGameXXKDesktopTrainingViewMode : uint8
{
	Workbench,
	ChallengeViewport
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

	/** Selected owner for the shared role/companion backpack surface. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetActiveBackpackCharacterIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	EGameXXKDesktopTrainingNav GetActiveNavForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsToolsPanelActiveForTest() const;

	/** Hero first, followed by the save-authoritative permanent companion instance IDs. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FName> GetBackpackCharacterIdsForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SelectBackpackCharacterForTest(FName CharacterId);

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsWorkbenchVisibleForTest() const;

	/** Whether the independent settings surface is open above the backpack. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsSettingsPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehouseColumnCountForTest() const;

	/** The approved MasterV2 resources the programmatic shell binds at runtime. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	TArray<FString> GetMasterV2ResourcePathsForTest() const;

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
	bool IsChallengeViewportActiveForTest() const;

	/** The warehouse and training-map shells remain present but input-locked during challenge. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool AreChallengeSidePanelsReadOnlyForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsAutoBattleVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsRetryVisibleForTest() const;

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
	FString GetTravelVisualBackgroundResourcePathForTest() const;

	/** Geometry contract for the merged challenge route/battle canvas. */
	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetChallengeViewportRectForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetChallengeCombatStripRectForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector4 GetChallengeBattleBoardRectForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetChallengeCombatSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FText GetStageTooltipForTest(FName StageId) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SelectStageForTest(FName StageId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ClickChallengeForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ClickTravelForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool ToggleAutoBattleForTest(bool bEnabled);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool AdvanceChallengeForTest();

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

	void HandleStageClicked(FName StageId);
	void HandleActionClicked(int32 ActionId);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildProgrammaticLayout();
	void BuildWorkbenchShell();
	void BuildChallengeViewport();
	void BuildChallengeCombatStrip();
	void BuildWarehousePanel(bool bReadOnly = false);
	void BuildBackpackPanel();
	void BuildTalentsPanel();
	void BuildTrainingMapPanel(bool bReadOnly = false);
	void BuildToolsPanel();
	void BuildTopIdleStrip();
	void BuildBottomNavigation();
	void RefreshLayout();
	void UpdateTravelCooldownText();
	void UpdateTravelVisuals();
	void ApplyAction(int32 ActionId);
	void SetNotice(const FText& Notice);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> NoticePanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDesktopTrainingStageButton>> StageButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDesktopTrainingActionButton>> ActionButtons;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TravelStageText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ChallengeStatusText;

	/** Live remaining cooldown readout for Travel normal/advanced chest rolls. */
	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TravelCooldownText;

	/** Clipped top-strip surface for the seamless Travel lane. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> TravelVisualViewport;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelBackgroundImageA;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelBackgroundImageB;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TravelHeroImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TravelVisualStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TravelHeroAtlasTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TravelBackgroundTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TravelHeroFallbackTextures;

	/** Reuses the production card battle board inside the enlarged ChallengeViewport. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> ChallengeBattleBoard;

	uint64 ChallengeBattleVisualSessionToken = 0;

	EGameXXKDesktopTrainingNav ActiveNav = EGameXXKDesktopTrainingNav::Training;
	EGameXXKDesktopTrainingViewMode ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	FName SelectedStageId = NAME_None;
	FName ActiveBackpackCharacterId = NAME_None;
	int32 WarehousePageIndex = 0;
	float AutoBattleAccumulator = 0.0f;
	float TravelAccumulator = 0.0f;
	int32 TravelVisualNativeTickCount = 0;
	FGameXXKTrainingTravelVisualRuntime TravelVisualRuntime;
	FVector2D BackpackAspectRatio = FVector2D(1.76f, 1.0f);
	bool bSettingsPanelOpen = false;
	bool bChallengeSidePanelsReadOnly = false;
	bool bNativeTickActive = false;
	bool bLayoutRefreshPending = false;
	int32 ProgrammaticLayoutBuildCount = 0;
	FText LastNotice;
};
