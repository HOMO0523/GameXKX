#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "Input/Reply.h"
#include "GameXXKBattleBoardWidget.generated.h"

class UCanvasPanel;
class UButton;
class UTextBlock;
class UTexture2D;
class UVerticalBox;

UENUM(BlueprintType)
enum class EGameXXKBattleInteractionMode : uint8
{
	Hidden,
	Idle,
	CommandMenuOpen,
	TargetingBasicAttack,
	TargetingCraneWingSlash
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKBattleBoardWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;
	virtual int32 NativePaint(
		const FPaintArgs& Args,
		const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements,
		int32 LayerId,
		const FWidgetStyle& InWidgetStyle,
		bool bParentEnabled) const override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecutePrimaryEnemyAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteBasicAttackAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteCraneWingSlashAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteGuiyuanArtAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteDefendAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ExecuteHealingPowderAction();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool OpenCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ToggleCommandMenuForPartyUnit(int32 PartyIndex, FVector2D MenuScreenPosition, FVector2D UnitScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	void UpdateTargetingPointer(FVector2D ScreenPosition);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool ConfirmTargetingEnemy(int32 EnemyIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Battle")
	bool CancelBattleTargeting();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	bool IsBattleBoardVisible() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	int32 GetEnemySlotCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	int32 GetPartySlotCount() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	FString GetEnemySlotSide() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Battle")
	FString GetPartySlotSide() const;

	FVector2D GetEnemySlotPositionForTest(int32 SlotIndex) const;
	FVector2D GetPartySlotPositionForTest(int32 SlotIndex) const;
	FString GetBattleStatusTextForTest() const;
	bool HasBattleActionForTest(FName ActionName, bool bRequireEnabled) const;
	bool IsCommandMenuVisibleForTest() const;
	bool IsTargetingBattleActionForTest() const;
	bool KeepTargetingAfterEmptyClickForTest() const;
	int32 GetSelectedPartyIndexForTest() const;
	FName GetTargetingActionNameForTest() const;
	FVector2D GetTargetingPointerPositionForTest() const;
	FVector2D GetCommandMenuAnchorForTest() const;
	FString GetBattleActionButtonResourcePathForTest(FName ActionName);
	FLinearColor GetBattleActionButtonTintForTest(FName ActionName) const;
	FString GetTargetingArrowHeadResourcePathForTest();
	int32 GetTargetingInkDabTextureCountForTest();

private:
	void BuildProgrammaticLayout();
	UButton* AddBattleActionButton(const FText& Label, FName ButtonName, FName ActionName);
	void RefreshProgrammaticLayout();
	void RefreshActionButtons();
	void EnsureBattleVisualResourcesLoaded();
	void StyleBattleActionButton(UButton* Button, FName ActionName);
	FLinearColor ResolveBattleActionButtonTint(FName ActionName) const;
	FVector2D ResolveCommandMenuAnchor(FVector2D UnitScreenPosition) const;
	bool BeginTargetingBattleAction(FName ActionName);
	bool ExecuteBattleAction(FName ActionName);
	int32 FindFirstLivingEnemyIndex() const;
	FString BuildBattleStatusText() const;

	UFUNCTION()
	void HandleBasicAttackClicked();

	UFUNCTION()
	void HandleCraneWingSlashClicked();

	UFUNCTION()
	void HandleGuiyuanArtClicked();

	UFUNCTION()
	void HandleDefendClicked();

	UFUNCTION()
	void HandleHealingPowderClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> ActionBox;

	UPROPERTY(Transient)
	TObjectPtr<UButton> BasicAttackButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CraneWingSlashButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> GuiyuanArtButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DefendButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HealingPowderButton;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> BattleActionInkButtonTexture;

	UPROPERTY(Transient)
	TObjectPtr<UTexture2D> TargetingArrowHeadTexture;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTexture2D>> TargetingInkDabTextures;

	UPROPERTY(Transient)
	EGameXXKBattleInteractionMode InteractionMode = EGameXXKBattleInteractionMode::Hidden;

	UPROPERTY(Transient)
	int32 SelectedPartyIndex = INDEX_NONE;

	UPROPERTY(Transient)
	FName TargetingActionName;

	UPROPERTY(Transient)
	FVector2D CommandMenuAnchor = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D SelectedPartyScreenPosition = FVector2D::ZeroVector;

	UPROPERTY(Transient)
	FVector2D TargetingPointerPosition = FVector2D::ZeroVector;
};
