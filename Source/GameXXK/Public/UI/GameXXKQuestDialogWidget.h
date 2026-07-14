#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKQuestDialogWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKQuestDialogWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
	void OpenDialog();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
	bool CloseDialog();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Quest|Test")
	bool IsDialogOpen() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Quest|Test")
	FString GetDialogFrameResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Quest|Test")
	FString GetAcceptButtonResourcePathForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Quest|Test")
	FString GetLeaveButtonResourcePathForTest() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Quest")
	bool AcceptQuest();

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Quest")
	void OnQuestAccepted();

private:
	void BuildProgrammaticLayout();

	UFUNCTION()
	void HandleAcceptClicked();

	UFUNCTION()
	void HandleLeaveClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ModalBackdrop;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DialogFrame;

	UPROPERTY(Transient)
	TObjectPtr<UButton> AcceptButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> LeaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> SpeakerTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DialogTextBlock;
};
