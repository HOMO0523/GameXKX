#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTownHudWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UProgressBar;
class UScrollBox;
class USizeBox;
class UTextBlock;
class UUniformGridPanel;

class UGameXXKTownHudWidget;

UCLASS()
class GAMEXXK_API UGameXXKCompanionCodexFilterButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKTownHudWidget* InOwner, EGameXXKCodexCategory InCategory);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTownHudWidget> Owner;

	EGameXXKCodexCategory Category = EGameXXKCodexCategory::All;
};

UCLASS()
class GAMEXXK_API UGameXXKCompanionCodexCardButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKTownHudWidget* InOwner, FName InEntryId);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTownHudWidget> Owner;

	FName EntryId = NAME_None;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTownHudWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TownHud")
	void RefreshFromState();

	bool CloseCompanionCodex();
	bool IsCompanionCodexOpenForTest() const;
	bool OpenCompanionCodexForTest();
	bool SelectCodexCategoryForTest(EGameXXKCodexCategory Category);
	bool SelectCodexEntryForTest(FName EntryId);
	bool SelectTaskNpcCodexEntryForTest(FName NpcId);
	EGameXXKCodexCategory GetActiveCodexCategoryForTest() const;
	TArray<FName> GetVisibleCodexEntryIdsForTest() const;
	TArray<FName> GetTaskNpcCodexEntryIdsForTest() const;
	TArray<FName> GetTaskNpcFixedRouteLoadoutForTest(FName NpcId) const;
	FString GetTaskNpcPortraitResourcePathForTest(FName NpcId) const;
	FString GetHeroDetailPortraitResourcePathForTest() const;
	FText GetTaskNpcCodexDetailForTest() const;
	int32 GetCodexColumnCountForTest() const;
	FVector2D GetCodexCardSizeForTest() const;
	bool IsCodexEmptyStateVisibleForTest() const;
	bool HasCompanionUnreadNoticeForTest() const;
	FText GetCodexCollectionSummaryForTest() const;
	float GetCodexScrollOffsetForTest() const;
	bool SetCodexScrollOffsetForTest(float Offset);
	void HandleConfiguredCodexFilterClicked(EGameXXKCodexCategory Category);
	void HandleConfiguredCodexCardClicked(FName EntryId);

private:
	void BuildProgrammaticLayout();
	void BuildCompanionCodexOverlay();
	void RefreshPanels();
	void RefreshCompanionCodex();
	void RefreshCompanionUnreadBadge();
	bool OpenCompanionCodex();
	bool IsValidCodexCategory(EGameXXKCodexCategory Category) const;
	void CloseAuxiliaryPanels();
	void SetNotice(const FText& Notice);

	UFUNCTION()
	void HandleTaskClicked();

	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleMapClicked();

	UFUNCTION()
	void HandleCompanionClicked();

	UFUNCTION()
	void HandleCodexDiscClicked();

	UFUNCTION()
	void HandleCodexCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroLevelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> IngotValueText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CodexOverlay;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CodexFrame;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> CodexScroll;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> CodexGrid;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CodexCollectionText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CodexEmptyText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CodexDetailPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CodexDetailText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> TaskNpcCodexDetailPortrait;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> TaskNpcCodexDetailPortraitSlot;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> TaskNpcCodexLoadoutPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UCanvasPanel>> TaskNpcCodexLoadoutCards;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> TaskNpcCodexLoadoutLabels;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CodexCloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CompanionUnreadBadge;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TaskButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InventoryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MapButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CompanionButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CodexDiscButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionCodexFilterButton>> CodexFilterButtons;

	bool bCompanionCodexOpen = false;
	EGameXXKCodexCategory ActiveCodexCategory = EGameXXKCodexCategory::All;
	FName SelectedCodexEntryId = NAME_None;
	FName SelectedTaskNpcCodexId = NAME_None;
	TArray<FName> VisibleCodexEntryIds;
	TArray<FName> VisibleTaskNpcCodexEntryIds;
	int32 CodexColumnCount = 6;
	FVector2D CodexCardSize = FVector2D(113.0f, 129.0f);
	float LastCodexScrollOffset = 0.0f;
};
