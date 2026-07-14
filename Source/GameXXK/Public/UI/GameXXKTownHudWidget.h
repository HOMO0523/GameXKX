#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTownHudWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTownHudWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|TownHud")
	void RefreshFromState();

private:
	void BuildProgrammaticLayout();
	void RefreshPanels();
	void SetNotice(const FText& Notice);

	UFUNCTION()
	void HandleTaskClicked();

	UFUNCTION()
	void HandleInventoryClicked();

	UFUNCTION()
	void HandleCharacterClicked();

	UFUNCTION()
	void HandleMapClicked();

	UFUNCTION()
	void HandleCompanionClicked();

	UFUNCTION()
	void HandleResourcePlusClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> LevelText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExperienceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> PowerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> GoldText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExperienceResourceText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> MaterialText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NoticeText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CharacterPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CharacterStatsText;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> CompanionPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> CompanionStatusText;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CharacterLabel;

	UPROPERTY(Transient)
	TObjectPtr<UImage> CompanionLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TaskButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InventoryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CharacterButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> MapButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CompanionButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResourcePlusButton;
};
