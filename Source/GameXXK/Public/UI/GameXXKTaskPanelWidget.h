#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTaskPanelWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTaskPanelWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel")
	bool OpenTaskPanel();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel")
	bool OpenTaskOfferPanel();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel")
	bool CloseTaskPanel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool IsTaskPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool HasAllTaskFiltersForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool HasMainAndSideTaskFiltersForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool IsShowingTaskOffersForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool IsShowingAcceptedTasksForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	bool HasPrimaryTaskActionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	EGameXXKTaskCategory GetActiveCategoryForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|TaskPanel|Test")
	int32 GetVisibleTaskCountForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel|Test")
	bool SelectTaskCategoryForTest(EGameXXKTaskCategory Category);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TaskPanel|Test")
	bool TriggerPrimaryTaskActionForTest();

	// The controller validates a click against the currently rendered offer rows.
	// This prevents a stale or unrelated task id from accepting the active NPC quest.
	bool HasVisibleTaskOffer(FName TaskId) const;

private:
	void BuildProgrammaticLayout();
	void RebuildTaskList();
	bool SelectTaskCategory(EGameXXKTaskCategory Category);
	bool TriggerTaskAction(int32 TaskIndex);
	void RefreshTabVisuals();

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleMainTabClicked();

	UFUNCTION()
	void HandleSideTabClicked();

	UFUNCTION()
	void HandleTaskAction0Clicked();

	UFUNCTION()
	void HandleTaskAction1Clicked();

	UFUNCTION()
	void HandleTaskAction2Clicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PanelBorder;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MainTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SideTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> TaskListBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> EmptyStateText;

	EGameXXKTaskCategory ActiveCategory = EGameXXKTaskCategory::Main;
	bool bShowingTaskOffers = false;
	bool bHasPrimaryTaskAction = false;
	TArray<FGameXXKTaskView> VisibleTasks;
};
