#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKTutorial01ResultWidget.generated.h"

class UBorder;
class UButton;
class UTextBlock;

DECLARE_DELEGATE(FGameXXKTutorial01RetryRequested);
DECLARE_DELEGATE(FGameXXKTutorial01ReturnTownRequested);

UCLASS()
class GAMEXXK_API UGameXXKTutorial01ResultWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void PresentFailure(const FText& Reason);
	void Dismiss();
	void SetRetryRequested(FGameXXKTutorial01RetryRequested InDelegate);
	void SetReturnTownRequested(FGameXXKTutorial01ReturnTownRequested InDelegate);
	bool IsVisibleForTest() const;
	FString GetPaperTexturePathForTest() const;
	void ChooseRetryForTest();
	void ChooseReturnTownForTest();

private:
	void BuildProgrammaticLayout();

	UFUNCTION()
	void HandleRetryClicked();

	UFUNCTION()
	void HandleReturnTownClicked();

	UPROPERTY(Transient)
	TObjectPtr<UBorder> PaperPanel;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ReasonText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> RetryButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnTownButton;

	FGameXXKTutorial01RetryRequested RetryRequestedDelegate;
	FGameXXKTutorial01ReturnTownRequested ReturnTownRequestedDelegate;
};
