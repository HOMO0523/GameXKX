#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKRouteEncounterPanelWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;

/**
 * A route interaction never resolves merely because the player presses F.
 * This is the small, serializable choice vocabulary presented by the paper
 * encounter panel and dispatched by the player controller into existing rules.
 */
UENUM(BlueprintType)
enum class EGameXXKRouteEncounterAction : uint8
{
	None = 0,
	AcceptTaskNpcSupport = 1 UMETA(Hidden),
	TakeGold = 2,
	TakeHealingPowder = 3,
	CampRest = 4,
	CampTakeHealingPowder = 5,
	MerchantLeave = 6,
	SelectChoice0 = 7,
	SelectChoice1 = 8,
	SelectChoice2 = 9,
	ClosePanel = 10,
	CampTakeLifeSavingTalisman = 11,
	CampTakeRouteMoney = 12
};

class UGameXXKRouteEncounterPanelWidget;

struct FGameXXKRouteChoicePresentationIdentity
{
	EGameXXKScreen Screen = EGameXXKScreen::MainMenu;
	int32 PendingNodeId = INDEX_NONE;
	int32 EventSourceNodeId = INDEX_NONE;
	int32 EventChoiceSeed = 0;
	FName EncounterId = NAME_None;
	int32 RelicSourceNodeId = INDEX_NONE;
	int32 RelicChoiceSeed = 0;
	TArray<FName> RelicIds;

	bool operator==(const FGameXXKRouteChoicePresentationIdentity& Other) const
	{
		return Screen == Other.Screen
			&& PendingNodeId == Other.PendingNodeId
			&& EventSourceNodeId == Other.EventSourceNodeId
			&& EventChoiceSeed == Other.EventChoiceSeed
			&& EncounterId == Other.EncounterId
			&& RelicSourceNodeId == Other.RelicSourceNodeId
			&& RelicChoiceSeed == Other.RelicChoiceSeed
			&& RelicIds == Other.RelicIds;
	}
};

/** A configured button keeps the visual widget independent from route-rule execution. */
UCLASS()
class GAMEXXK_API UGameXXKRouteEncounterActionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKRouteEncounterPanelWidget* InOwner, EGameXXKRouteEncounterAction InAction);
	void ConfigureChoice(UGameXXKRouteEncounterPanelWidget* InOwner, int32 InChoiceIndex);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterPanelWidget> Owner;

	EGameXXKRouteEncounterAction Action = EGameXXKRouteEncounterAction::None;
	int32 ChoiceIndex = INDEX_NONE;
};

/**
 * Programmatic paper-and-ink route panel. It only renders the current pending
 * encounter and delegates a clicked action to AGameXXKMVPPlayerController.
 */
UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKRouteEncounterPanelWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter")
	bool OpenEncounterPanel();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter")
	bool CloseEncounterPanel();

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	bool IsEncounterPanelOpenForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FString GetWindowFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FString GetHeaderResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FString GetActionResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FText GetSpeakerTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FText GetPrimaryActionTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FText GetSecondaryActionTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	FText GetTertiaryActionTextForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	EGameXXKRouteEncounterAction GetPrimaryActionForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|RouteEncounter|Test")
	EGameXXKRouteEncounterAction GetSecondaryActionForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	bool TriggerPrimaryActionForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	bool TriggerSecondaryActionForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	bool TriggerTertiaryActionForTest();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	bool SelectChoiceForTest(int32 ChoiceIndex);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|RouteEncounter|Test")
	bool ConfirmSelectedChoiceForTest();
	int32 GetSelectedChoiceIndexForTest() const { return SelectedChoiceIndex; }

	int32 GetRenderedChoiceCardCountForTest() const;

private:
	friend class UGameXXKRouteEncounterActionButton;

	void BuildProgrammaticLayout();
	bool BuildPresentation();
	bool ExecuteAction(EGameXXKRouteEncounterAction InAction);
	bool SelectChoice(int32 ChoiceIndex);
	bool ConfirmSelectedChoice();
	void RefreshChoiceCardStates();
	void RegisterGuideTargets();
	FName ResolveGuideActionId(EGameXXKRouteEncounterAction InAction) const;
	FName ResolveGuideCompletionEventId(EGameXXKRouteEncounterAction InAction) const;
	void ApplyActionButton(UGameXXKRouteEncounterActionButton* Button, UTextBlock* Label, EGameXXKRouteEncounterAction InAction, const FText& Text, bool bEnabled);

	UFUNCTION()
	void HandleCloseClicked();

	UFUNCTION()
	void HandleConfirmClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ModalBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> WindowFrame;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> FrameCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UImage> HeaderImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakerTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterActionButton> PrimaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterActionButton> SecondaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterActionButton> TertiaryActionButton;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterActionButton> CloseButton;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKRouteEncounterActionButton>> ChoiceCardButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ChoiceArtImages;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceNameTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceDescriptionTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> ChoiceDisabledReasonTexts;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UImage>> ChoiceSelectionInks;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ConfirmTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PrimaryActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SecondaryActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TertiaryActionTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CloseTextBlock;

	EGameXXKRouteEncounterAction PrimaryAction = EGameXXKRouteEncounterAction::None;
	EGameXXKRouteEncounterAction SecondaryAction = EGameXXKRouteEncounterAction::None;
	EGameXXKRouteEncounterAction TertiaryAction = EGameXXKRouteEncounterAction::None;
	int32 SelectedChoiceIndex = INDEX_NONE;
	TArray<EGameXXKRouteEncounterAction> ChoiceActions;
	TOptional<FGameXXKRouteChoicePresentationIdentity> ChoicePresentationIdentity;
	bool bGuideEncounterOpenedEmitted = false;
};
