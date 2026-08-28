#pragma once

#include "CoreMinimal.h"
#include "Components/Button.h"
#include "Dialogue/GameXXKDialogueTypes.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKDialoguePanelWidget.generated.h"

class UBorder;
class UCanvasPanel;
class UImage;
class UTextBlock;
class UGameXXKDialoguePanelWidget;

DECLARE_DELEGATE(FGameXXKDialogueAdvanceRequested);
DECLARE_DELEGATE_OneParam(FGameXXKDialogueOptionRequested, FName);

UCLASS()
class GAMEXXK_API UGameXXKDialogueOptionButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKDialoguePanelWidget* InOwner, int32 InOptionIndex);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDialoguePanelWidget> Owner;
	int32 OptionIndex = INDEX_NONE;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKDialoguePanelWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void Present(const FGameXXKDialoguePresentationView& View);
	void ClearPresentation();
	void SetAdvanceRequested(FGameXXKDialogueAdvanceRequested Delegate);
	void SetOptionRequested(FGameXXKDialogueOptionRequested Delegate);

	int32 GetPaperFrameCountForTest() const;
	int32 GetPortraitCountForTest() const;
	bool HasContinueIndicatorForTest() const;
	int32 GetVisibleOptionCountForTest() const;
	FText GetSpeakerTextForTest() const;
	FText GetBodyTextForTest() const;
	bool IsOptionVisibleForTest(int32 OptionIndex) const;
	bool IsOptionEnabledForTest(int32 OptionIndex) const;
	FText GetOptionTooltipForTest(int32 OptionIndex) const;
	bool IsContinueIndicatorVisibleForTest() const;
	bool RequestOptionForTest(int32 OptionIndex);
	void RequestAdvanceForTest();

private:
	friend class UGameXXKDialogueOptionButton;
	void BuildProgrammaticLayout();
	bool RequestOption(int32 OptionIndex);

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PaperFrame;

	UPROPERTY(Transient)
	TObjectPtr<UImage> PortraitImage;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakerText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> BodyText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UGameXXKDialogueOptionButton>> OptionButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> OptionTexts;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinueIndicator;

	FGameXXKDialoguePresentationView CurrentView;
	FGameXXKDialogueAdvanceRequested AdvanceRequested;
	FGameXXKDialogueOptionRequested OptionRequested;
};
