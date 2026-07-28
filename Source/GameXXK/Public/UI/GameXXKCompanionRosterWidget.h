#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKCompanionTypes.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKCompanionRosterWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UScrollBox;
class UTextBlock;
class UUniformGridPanel;
class UVerticalBox;

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

	// Focused automation read seams: these describe public presentation contracts without exposing mutable widget state.
	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterSlotCountForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|CompanionRoster|Test")
	int32 GetRosterColumnCountForTest() const;

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

private:
	void BuildProgrammaticLayout();
	void RefreshProgrammaticLayout();
	void RefreshSelectedCompanionData();
	void RefreshHeroCardData();
	void RefreshRosterSlots();
	void RefreshProfilePanel();
	void RefreshRecruitmentPanel();
	void RefreshPersonalCards();
	void RefreshDeckSummaries();
	void RefreshDeckEditorControls();
	void RefreshCardTooltip();
	void ClearCardTooltipHoverState();
	bool IsCurrentSelectedCompanion(const FName InstanceId) const;

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

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FrameCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UUniformGridPanel> RosterGrid;

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
	TObjectPtr<UTextBlock> ProfileTitleText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ProfileDetailText;

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
	TArray<TObjectPtr<UBorder>> RosterSlotSelectionBorders;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKCompanionRosterCardButton>> PersonalCardButtons;

	TArray<FGameXXKPermanentCompanion> CachedRoster;
	TArray<FName> VisibleRosterSlotInstanceIds;
	int32 RosterCapacity = 12;
	FName SelectedCompanionId = NAME_None;
	FGameXXKCompanionRosterProfileView SelectedCompanionProfile;
	TArray<FName> VisiblePersonalCardIds;
	TArray<FName> UnlockedPersonalCardIds;
	TArray<FName> PendingPersonalCardIds;
	TArray<FName> VisibleHeroCardIds;
	TArray<FName> UnlockedHeroCardIds;
	TArray<FName> PendingHeroCardIds;
	TArray<FName> HeroCardSummary;
	FGameXXKQuestNpcCardSelection TaskNpcCardSummary;
	FName HoveredCardTooltipId = NAME_None;
	bool bHoveredCardTooltipIsHeroDeck = false;
	FGameXXKPermanentCompanion PendingRecruitmentCandidate;
	FString RecruitmentFeedback;
	int32 SigilCount = 0;
	bool bLoadoutReadOnly = true;
	bool bRecruitmentActionsReadOnly = true;
	bool bEditingHeroDeck = false;
};
