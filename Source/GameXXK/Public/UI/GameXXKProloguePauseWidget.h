#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKProloguePauseWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_DELEGATE(FGameXXKPrologueResumeRequested);
DECLARE_DELEGATE(FGameXXKPrologueReturnDesktopRequested);

UCLASS()
class GAMEXXK_API UGameXXKProloguePauseWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void SetResumeRequested(FGameXXKPrologueResumeRequested InDelegate);
	void SetReturnDesktopRequested(FGameXXKPrologueReturnDesktopRequested InDelegate);

	FText GetTitleTextForTest() const;
	int32 GetButtonCountForTest() const;
	void RequestResumeForTest();
	void RequestReturnDesktopForTest();

private:
	void BuildProgrammaticLayout();

	UFUNCTION()
	void HandleResumeClicked();

	UFUNCTION()
	void HandleReturnDesktopClicked();

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TitleText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ResumeButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> ReturnDesktopButton;

	FGameXXKPrologueResumeRequested ResumeRequested;
	FGameXXKPrologueReturnDesktopRequested ReturnDesktopRequested;
};
