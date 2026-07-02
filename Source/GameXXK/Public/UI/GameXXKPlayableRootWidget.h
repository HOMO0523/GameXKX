#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKPlayableRootWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKPlayableRootWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	EGameXXKScreen GetCurrentScreen() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasVisibleCommand(FName CommandName, bool bExpectedEnabled) const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Playable")
	bool ExecuteVisibleCommand(FName CommandName);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Playable")
	void RefreshFromState();

	void SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex);

private:
	UFUNCTION()
	void HandleStartClicked();

	UFUNCTION()
	void HandleQingshanClicked();

	UFUNCTION()
	void HandleTanjiangClicked();

	void BuildProgrammaticLayout();
	void ConfigureCommandButton(UButton* Button, UTextBlock* Label, FName CommandName, const FText& CommandLabel);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> StartButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StartButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> QingshanButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> QingshanButtonLabel;

	UPROPERTY(Transient)
	TObjectPtr<UButton> TanjiangButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> TanjiangButtonLabel;

	UPROPERTY(Transient)
	FString StartGameSlotNameOverride;

	UPROPERTY(Transient)
	int32 StartGameUserIndexOverride = 0;
};
