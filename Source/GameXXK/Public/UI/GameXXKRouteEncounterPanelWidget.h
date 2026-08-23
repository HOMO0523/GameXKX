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
	None,
	AcceptTaskNpcSupport,
	TakeGold,
	TakeHealingPowder,
	CampRest,
	CampTakeHealingPowder,
	MerchantLeave,
	SelectChoice0,
	SelectChoice1,
	SelectChoice2,
	ClosePanel,
	CampTakeLifeSavingTalisman,
	CampTakeRouteMoney
};

class UGameXXKRouteEncounterPanelWidget;

/** A configured button keeps the visual widget independent from route-rule execution. */
UCLASS()
class GAMEXXK_API UGameXXKRouteEncounterActionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKRouteEncounterPanelWidget* InOwner, EGameXXKRouteEncounterAction InAction);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKRouteEncounterPanelWidget> Owner;

	EGameXXKRouteEncounterAction Action = EGameXXKRouteEncounterAction::None;
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

private:
	friend class UGameXXKRouteEncounterActionButton;

	void BuildProgrammaticLayout();
	bool BuildPresentation();
	bool ExecuteAction(EGameXXKRouteEncounterAction InAction);
	void ApplyActionButton(UGameXXKRouteEncounterActionButton* Button, UTextBlock* Label, EGameXXKRouteEncounterAction InAction, const FText& Text, bool bEnabled);

	UFUNCTION()
	void HandleCloseClicked();

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
};
