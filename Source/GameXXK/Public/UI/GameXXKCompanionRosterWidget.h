#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKCompanionTypes.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKCompanionRosterWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;
class UGameXXKCardTooltipWidget;

/** Read-only profile data rendered for the permanent companion selected in the roster backpack. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionRosterProfileView
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Level = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Star = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ExperienceRequiredForNextLevel = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCompanionAttributes Attributes;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bIsActive = false;
};

class UGameXXKCompanionRosterWidget;

/** Page 03-style text filter above the partner warehouse window. */
UCLASS()
class GAMEXXK_API UGameXXKCompanionFilterButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKCompanionRosterWidget* InOwner, int32 InFilterIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCompanionRosterWidget> Owner;

	int32 FilterIndex = INDEX_NONE;
};

UENUM(BlueprintType)
enum class EGameXXKCompanionBackpackTab : uint8
{
	Attributes,
	Equipment,
	Cards,
	Talents,
	Titles
};

enum class EGameXXKCompanionEquipmentSlotSource : uint8
{
	Warehouse,
	Equipped
};

UCLASS()
class GAMEXXK_API UGameXXKCompanionRosterSlotButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKCompanionRosterWidget* InOwner, int32 InSlotIndex);

	UFUNCTION()
	void HandleClicked();

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCompanionRosterWidget> Owner;

	int32 SlotIndex = INDEX_NONE;
};

UCLASS()
class GAMEXXK_API UGameXXKCompanionRosterCardButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKCompanionRosterWidget* InOwner, FName InCardId, bool bInHeroDeck = false);

	UFUNCTION()
	void HandleClicked();

	UFUNCTION()
	void HandleHovered();

	UFUNCTION()
	void HandleUnhovered();

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCompanionRosterWidget> Owner;

	FName CardId = NAME_None;
	bool bHeroDeckCard = false;
};

UCLASS()
class GAMEXXK_API UGameXXKCompanionEquipmentSlotButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(
		UGameXXKCompanionRosterWidget* InOwner,
		EGameXXKCompanionEquipmentSlotSource InSource,
		int32 InWarehouseIndex,
		EGameXXKEquipmentSlot InEquipmentSlot = EGameXXKEquipmentSlot::Invalid);

	UFUNCTION()
	void HandleClicked();

	bool HandleRightMouseButtonDown();

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKCompanionRosterWidget> Owner;

	EGameXXKCompanionEquipmentSlotSource Source = EGameXXKCompanionEquipmentSlotSource::Warehouse;
	int32 WarehouseIndex = INDEX_NONE;
	EGameXXKEquipmentSlot EquipmentSlot = EGameXXKEquipmentSlot::Invalid;
};

/**
 * Isolated permanent-companion backpack. It owns only roster/deck presentation and delegates every
 * persistent mutation to UGameXXKMVPSubsystem's companion facade.
 */
UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKCompanionRosterWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	void RefreshFromState();

	/** Changes only the local profile/deck selection; it never changes the active route party. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool SelectCompanion(FName InstanceId);

	/** Stages one unlocked personal card. Exactly five cards are required before applying. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ToggleSelectedCompanionCard(FName CardId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ApplySelectedCompanionCardLoadout();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool SetSelectedCompanionAsActive();

	/** Town-only action that clears the optional permanent partner without dismissing the companion. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ClearActivePermanentCompanion();

	/** Opens the fixed hero's twelve-card pool. It remains viewable, but not mutable, after the route lock. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool OpenHeroDeckEditor();

	/** Stages one unlocked hero card. Exactly eight cards are required before applying. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ToggleHeroCard(FName CardId);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ApplyHeroCardLoadout();

	/** Starts or reopens the save-authoritative town recruitment ticket. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool BeginRandomRecruitment();

	/** Replaces the currently selected permanent companion with the fixed pending candidate. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool ResolvePendingRecruitmentWithSelectedCompanion();

	/** Explicitly abandons the current fixed full-roster candidate without rerolling it. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool DiscardPendingRecruitment();

	/** Consumes one existing sigil through the canonical facade; this widget never grants experience. */
	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster")
	bool PromoteSelectedCompanionStar();

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	FGameXXKCompanionRosterProfileView GetSelectedCompanionProfile() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	TArray<FName> GetVisiblePersonalCardIds() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	TArray<FName> GetPendingPersonalCardIds() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	TArray<FName> GetHeroCardSummary() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	TArray<FName> GetVisibleHeroCardIds() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	TArray<FName> GetPendingHeroCardIds() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster")
	FGameXXKQuestNpcCardSelection GetTaskNpcCardSummary() const;

	void HandleConfiguredRosterSlotClicked(int32 SlotIndex);
	void HandleConfiguredPersonalCardClicked(FName CardId);
	void HandleConfiguredHeroCardClicked(FName CardId);
	/** Card-button hover forwarding; it changes only tooltip presentation, never deck staging. */
	void HandleConfiguredCardHoverChanged(FName CardId, bool bInHeroDeck, bool bHovered);
	void HandleConfiguredEquipmentSlotClicked(EGameXXKCompanionEquipmentSlotSource Source, int32 WarehouseIndex, EGameXXKEquipmentSlot EquipmentSlot);
	bool HandleConfiguredEquipmentSlotRightClicked(EGameXXKCompanionEquipmentSlotSource Source, int32 WarehouseIndex, EGameXXKEquipmentSlot EquipmentSlot);
	/** Page 03 filter-row selection; called by UGameXXKCompanionFilterButton. */
	void HandleBackpackFilterClicked(int32 FilterIndex);

	// Focused automation read seams: these describe public presentation contracts without exposing mutable widget state.
	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterColumnCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterPageSizeForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterPageCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetCurrentRosterPageForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetVisibleRosterButtonCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	TArray<FName> GetVisibleRosterSlotInstanceIdsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRosterSlotTooltipTextForTest(int32 VisibleSlotIndex) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster|Test")
	bool GoToNextRosterPageForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|CompanionRoster|Test")
	bool GoToPreviousRosterPageForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRosterPageLeftResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRosterPageRightResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRosterPortraitResourcePathForTest(int32 VisibleSlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	bool HasSeparateSetActiveButtonForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetWindowFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRosterSlotResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetPersonalCardFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FName GetSelectedCompanionIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	bool IsLoadoutReadOnlyForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	bool HasPersonalCardScrollBoxForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetPersonalCardScrollTrackResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetPersonalCardScrollThumbResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	bool HasPendingRecruitmentForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FName GetPendingRecruitmentCandidateIdForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	FString GetRecruitmentStatusForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	bool IsHeroDeckEditorOpenForTest() const;
	FString GetCardTooltipTextForTest() const;
	bool IsCardTooltipVisibleForTest() const;
	bool IsCardTooltipHitTestInvisibleForTest() const;
	FText GetTitleTextForTest() const;
	int32 GetEquipmentSlotCountForTest() const;
	int32 GetEquipmentBackpackViewportSlotCountForTest() const;
	int32 GetEquipmentBackpackStorageCapacityForTest() const;
	bool HasEquipmentBackpackScrollBoxForTest() const;
	bool IsEquipmentBackpackTabOpenForTest() const;
	bool IsCardBackpackTabOpenForTest() const;
	bool OpenEquipmentBackpackTabForTest();
	bool OpenCardBackpackTabForTest();
	TArray<FName> GetVisibleEquipmentInstanceIdsForTest() const;
	bool QuickEquipSelectedCompanionInstanceForTest(FName InstanceId);
	bool QuickUnequipSelectedCompanionSlotForTest(EGameXXKEquipmentSlot EquipmentSlot);
	FName GetSelectedCompanionEquippedInstanceForTest(EGameXXKEquipmentSlot EquipmentSlot) const;
	FString GetLockedCardIconResourcePathForTest() const;
	FString GetCloseButtonResourcePathForTest() const;

private:
	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	void RefreshSelectedCompanionData();
	void RefreshHeroCardData();
	void RefreshRosterSlots();
	void RefreshVisibleRosterPage();
	void RefreshProfilePanel();
	void RefreshRecruitmentPanel();
	void RefreshPersonalCards();
	void RefreshDeckSummaries();
	void RefreshDeckEditorControls();
	void RefreshCardTooltip();
	void RefreshEquipmentBackpack();
	void RefreshBackpackTabVisibility();
	void RefreshCenterCompanionPresentation();
	void UpdateEquipmentScrollbarThumb();
	void UpdatePersonalCardScrollbarThumb();
	void ClearCardTooltipHoverState();
	bool IsCurrentSelectedCompanion(const FName InstanceId) const;
	bool ChangeRosterPage(int32 Direction);
	void SetActiveBackpackTab(EGameXXKCompanionBackpackTab Tab);

	UFUNCTION()
	void HandleRosterPageLeftClicked();

	UFUNCTION()
	void HandleRosterPageRightClicked();

	UFUNCTION()
	void HandleAttributesTabClicked();

	UFUNCTION()
	void HandleTalentsTabClicked();

	UFUNCTION()
	void HandleTitlesTabClicked();

	UFUNCTION()
	void HandleEquipmentBackpackScrolled(float CurrentOffset);

	UFUNCTION()
	void HandlePersonalCardScrolled(float CurrentOffset);

	UFUNCTION()
	void HandleApplyLoadoutClicked();

	UFUNCTION()
	void HandleSetActiveClicked();

	UFUNCTION()
	void HandleClearActiveClicked();

	UFUNCTION()
	void HandleHeroDeckToggleClicked();

	UFUNCTION()
	void HandleApplyHeroLoadoutClicked();

	UFUNCTION()
	void HandleRecruitClicked();

	UFUNCTION()
	void HandleReplacePendingClicked();

	UFUNCTION()
	void HandleDiscardPendingClicked();

	UFUNCTION()
	void HandlePromoteStarClicked();

	UFUNCTION()
	void HandleEquipmentBackpackTabClicked();

	UFUNCTION()
	void HandleCardBackpackTabClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FrameCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AttributesTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EquipmentBackpackTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CardBackpackTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TalentsTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TitlesTabButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> EquipmentBackpackPanel;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> EquipmentBackpackScrollBox;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> EquipmentBackpackGrid;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionEquipmentSlotButton>> EquipmentWarehouseSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> EquipmentWarehouseSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionEquipmentSlotButton>> CompanionEquipmentSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> CompanionEquipmentSlotIcons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> CompanionEquipmentTooltipFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CompanionEquipmentTooltipNameBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CompanionEquipmentTooltipDetailBlocks;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PersonalDeckPanel;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> RosterGrid;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RosterPageLeftButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RosterPageRightButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RosterCountText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> RecruitmentStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RecruitButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReplacePendingButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> DiscardPendingButton;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> AttributesBodyPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProfileTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProfileDetailText;

	/** Page 18 center: the selected companion's idle full body plus its random display name. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> CenterCompanionPortraitImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CenterCompanionNameText;

	/** Page 03-style selection inks: one bracket above the warehouse column, one above the equipped slot. */
	UPROPERTY(Transient)
	TObjectPtr<UImage> ScrollbarTrackImage;

	UPROPERTY(Transient)
	TObjectPtr<UImage> InventoryScrollbarThumb;

	UPROPERTY(Transient)
	TObjectPtr<UImage> BackpackSelectionInk;

	UPROPERTY(Transient)
	TObjectPtr<UImage> EquipmentSelectionInk;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionFilterButton>> BackpackFilterButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackFilterTextBlocks;

	/** Page 03 filter selection (0=全部, 1=装备, 2=道具, 3=材料, 4=任务). */
	int32 ActiveBackpackFilter = 0;

	int32 SelectedWarehouseSlotIndex = INDEX_NONE;
	int32 SelectedEquippedSlotIndex = INDEX_NONE;

	// Page 03-style warehouse presentation (mirrors the hero backpack content source).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> BackpackTooltipFrames;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackTooltipNameTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackTooltipDetailTextBlocks;

	/** Replaced-slot comparison rows (red gain / green loss) inside each warehouse tooltip. */
	TArray<TArray<TObjectPtr<UTextBlock>>> BackpackCompareTextBlocks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> BackpackSlotLabels;

	TArray<FName> CurrentBackpackSlotItemIds;
	TArray<FName> CurrentBackpackSlotEquipmentInstanceIds;
	TArray<int32> CurrentBackpackSlotQuantities;
	TArray<FString> CurrentBackpackSlotIconPaths;

	// Hero-style personal deck cards (name band, costs, selection ink, lock).
	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PersonalCardSelectedInks;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PersonalCardNameLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PersonalCardCostLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> PersonalCardManaCostLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PersonalCardLockedIcons;

	// Shared fixed-width compact/Shift card tooltips on every card.
	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCardTooltipWidget>> PersonalCardTooltipWidgets;

	TArray<FString> PersonalCardTooltipTexts;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroDeckSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DeckCaptionText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> HeroDeckToggleButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroDeckToggleButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyHeroLoadoutButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ApplyHeroLoadoutButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> HeroDeckStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TaskNpcDeckSummaryText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LoadoutStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ApplyLoadoutButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ApplyLoadoutButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SetActiveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SetActiveButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ClearActiveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ClearActiveButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PromoteStarButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PromoteStarButtonText;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> PersonalCardScroll;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> PersonalCardGrid;

	/** Shared PSD paper tooltip for personal and hero deck-editor cards; it never intercepts input. */
	UPROPERTY(Transient)
	TObjectPtr<UBorder> CardTooltipPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CardTooltipText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionRosterSlotButton>> RosterSlotButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> RosterSlotLabels;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> RosterSlotPortraits;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UBorder>> RosterSlotSelectionBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionRosterCardButton>> PersonalCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> PersonalCardPortraits;

	TArray<FGameXXKPermanentCompanion> CachedRoster;
	TArray<FName> VisibleRosterSlotInstanceIds;
	TArray<FString> CurrentRosterPortraitResourcePaths;
	int32 RosterCapacity = 12;
	int32 CurrentRosterPage = 0;
	bool bRosterPageInitialized = false;
	FName SelectedCompanionId = NAME_None;
	FGameXXKCompanionRosterProfileView SelectedCompanionProfile;
	TArray<FName> VisiblePersonalCardIds;
	TArray<FName> UnlockedPersonalCardIds;
	TArray<FName> PendingPersonalCardIds;
	TArray<FName> VisibleHeroCardIds;
	TArray<FName> UnlockedHeroCardIds;
	TArray<FName> PendingHeroCardIds;
	TArray<FName> HeroCardSummary;
	TArray<FName> VisibleEquipmentWarehouseInstanceIds;
	FGameXXKQuestNpcCardSelection TaskNpcCardSummary;
	FName HoveredCardTooltipId = NAME_None;
	bool bHoveredCardTooltipIsHeroDeck = false;
	bool bCardTooltipShiftExpanded = false;
	FGameXXKPermanentCompanion PendingRecruitmentCandidate;
	FString RecruitmentFeedback;
	int32 SigilCount = 0;
	bool bLoadoutReadOnly = true;
	bool bRecruitmentActionsReadOnly = true;
	bool bEditingHeroDeck = false;
	EGameXXKCompanionBackpackTab ActiveBackpackTab = EGameXXKCompanionBackpackTab::Equipment;
	FGameXXKCharacterBackpackModel CharacterBackpackModel;
};
