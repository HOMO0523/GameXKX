#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKDesktopTrainingWorkbenchWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
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

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsWorkbenchVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetWarehouseColumnCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FVector2D GetBackpackAspectRatioForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	int32 GetTrainingStageButtonCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetSelectedStageIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	FName GetCurrentTravelStageIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsChallengeViewportActiveForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsAutoBattleVisibleForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|DesktopTraining|Test")
	bool IsRetryVisibleForTest() const;

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
	bool AdvanceTravelForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|DesktopTraining|Test")
	bool SetRetryOnFailureForTest(bool bEnabled);

	void HandleStageClicked(FName StageId);
	void HandleActionClicked(int32 ActionId);

protected:
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildProgrammaticLayout();
	void BuildWorkbenchShell();
	void BuildChallengeViewport();
	void BuildWarehousePanel();
	void BuildBackpackPanel();
	void BuildTrainingMapPanel();
	void BuildTopIdleStrip();
	void BuildBottomNavigation();
	void RefreshLayout();
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

	/** Reuses the production card battle board inside the enlarged ChallengeViewport. */
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKBattleBoardWidget> ChallengeBattleBoard;

	uint64 ChallengeBattleVisualSessionToken = 0;

	EGameXXKDesktopTrainingNav ActiveNav = EGameXXKDesktopTrainingNav::Training;
	EGameXXKDesktopTrainingViewMode ViewMode = EGameXXKDesktopTrainingViewMode::Workbench;
	FName SelectedStageId = NAME_None;
	float AutoBattleAccumulator = 0.0f;
	float TravelAccumulator = 0.0f;
	FVector2D BackpackAspectRatio = FVector2D(1.76f, 1.0f);
	FText LastNotice;
};
