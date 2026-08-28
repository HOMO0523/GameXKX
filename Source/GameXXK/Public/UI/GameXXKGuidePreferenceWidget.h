#pragma once

#include "CoreMinimal.h"
#include "Guide/GameXXKGuideAsset.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKGuidePreferenceWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

DECLARE_DELEGATE_OneParam(FGameXXKGuidePreferenceChosen, EGameXXKGuidePreference);

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKGuidePreferenceWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void RefreshFromProgress(const FGameXXKGuideProgress& Progress);
	void SetPreferenceChosenDelegate(FGameXXKGuidePreferenceChosen InDelegate);
	void ChooseExperiencedPlayerForTest();
	void ChooseNewPlayerForTest();
	bool IsPromptVisibleForTest() const;
	FText GetExperiencedButtonTextForTest() const;
	FText GetNewPlayerButtonTextForTest() const;

private:
	void BuildProgrammaticLayout();

	UFUNCTION()
	void HandleExperiencedClicked();

	UFUNCTION()
	void HandleNewPlayerClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PromptPanel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ExperiencedButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> NewPlayerButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ExperiencedText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> NewPlayerText;

	FGameXXKGuidePreferenceChosen PreferenceChosenDelegate;
	bool bPromptVisible = false;
};
