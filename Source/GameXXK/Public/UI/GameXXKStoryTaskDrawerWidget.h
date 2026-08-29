#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Narrative/GameXXKStoryTaskDrawerRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKStoryTaskDrawerWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UVerticalBox;
class UWidget;

DECLARE_DELEGATE(FGameXXKStoryTaskDrawerCloseDelegate);
DECLARE_DELEGATE_ThreeParams(
	FGameXXKStoryTaskDrawerPrimaryActionDelegate,
	FName /* TaskId */,
	EGameXXKTaskState /* State */,
	EGameXXKStoryTaskContinuation /* Continuation */);

UCLASS()
class GAMEXXK_API UGameXXKStoryTaskDrawerRowButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(class UGameXXKStoryTaskDrawerWidget* InOwner, int32 InRowIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<class UGameXXKStoryTaskDrawerWidget> Owner;

	int32 RowIndex = INDEX_NONE;
};

UCLASS()
class GAMEXXK_API UGameXXKStoryTaskDrawerScrollBox : public UScrollBox
{
	GENERATED_BODY()

public:
	float GetDesiredScrollOffsetForTest() const { return DesiredScrollOffset; }
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKStoryTaskDrawerWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	/** Presents a caller-produced drawer snapshot without reading or writing RuntimeState. */
	void ApplySnapshot(const FGameXXKStoryTaskDrawerSnapshot& InSnapshot, const FGameXXKStoryTaskDrawerUiState& InUiState);
	bool OpenDrawer();
	bool CloseDrawer();

	void SetCloseRequested(FGameXXKStoryTaskDrawerCloseDelegate InDelegate);
	void SetPrimaryActionRequested(FGameXXKStoryTaskDrawerPrimaryActionDelegate InDelegate);

	FVector2D GetPanelFootprintForTest() const;
	FString GetPanelResourcePathForTest() const;
	FVector4 GetCloseRectForTest() const;
	EGameXXKStoryTaskDrawerTab GetActiveTabForTest() const;
	FName GetSelectedTaskIdForTest() const;
	FText GetPrimaryActionLabelForTest() const;
	bool IsPrimaryActionEnabledForTest() const;
	bool IsClaimableRedDotVisibleForTest() const;
	int32 GetRowCountForTest() const;
	int32 CountRowActionButtonsForTest() const;
	FText GetRowSummaryForTest(int32 RowIndex) const;
	FGameXXKStoryTaskDrawerUiState GetUiStateForTest() const;
	UWidget* FindNamedControlForTest(FName Name) const;
	bool SimulateSelectTabForTest(EGameXXKStoryTaskDrawerTab Tab);
	bool SimulateSelectRowForTest(int32 RowIndex);
	bool SimulateCloseForTest();
	bool SimulatePrimaryActionForTest();

	void SelectVisibleRow(int32 RowIndex);

private:
	void BuildProgrammaticLayout();
	void RebuildRowsAndDetail();
	void RefreshRowSelectionVisuals();
	void ResolveCachedResources();
	void RefreshTabVisuals();
	void RefreshDetail();
	void NormalizeUiState();
	const TArray<FGameXXKStoryTaskDrawerEntryView>& GetVisibleEntries() const;
	FName& GetSelectedTaskIdRef();
	float& GetScrollOffsetRef();
	const FGameXXKStoryTaskDrawerEntryView* GetSelectedEntry() const;
	bool SelectTab(EGameXXKStoryTaskDrawerTab Tab);
	void EmitPrimaryAction();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleActionableTabClicked();

	UFUNCTION()
	void HandleClaimableTabClicked();

	UFUNCTION()
	void HandlePrimaryActionClicked();

	UFUNCTION()
	void HandleListScrolled(float CurrentOffset);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ActionableTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClaimableTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ClaimableRedDot;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKStoryTaskDrawerScrollBox> StoryTaskList;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> StoryTaskRows;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailDescriptionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailStateText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailRewardText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PrimaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryActionLabelText;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> PanelTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> CloseTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> NormalTabTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> SelectedTabTexture;

	TArray<FName> VisibleRowTaskIds;
	TArray<TObjectPtr<UGameXXKStoryTaskDrawerRowButton>> RowButtons;
	TArray<TObjectPtr<UTextBlock>> RowMarkers;
	TArray<TObjectPtr<UTextBlock>> RowTitles;
	TArray<TObjectPtr<UTextBlock>> RowSummaries;
	FSlateBrush PanelBrush;
	FButtonStyle CloseButtonStyle;
	FButtonStyle TabNormalButtonStyle;
	FButtonStyle TabSelectedButtonStyle;
	FButtonStyle RowNormalButtonStyle;
	FButtonStyle RowSelectedButtonStyle;
	FButtonStyle PrimaryButtonStyle;
	FGameXXKStoryTaskDrawerCloseDelegate CloseRequestedDelegate;
	FGameXXKStoryTaskDrawerPrimaryActionDelegate PrimaryActionRequestedDelegate;

	FGameXXKStoryTaskDrawerSnapshot Snapshot;
	FGameXXKStoryTaskDrawerUiState UiState;
	bool bRestoringListScroll = false;
	bool bDrawerOpen = false;
};
