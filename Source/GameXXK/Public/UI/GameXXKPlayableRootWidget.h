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
	void HandleCommandButton0Clicked();

	UFUNCTION()
	void HandleCommandButton1Clicked();

	UFUNCTION()
	void HandleCommandButton2Clicked();

	UFUNCTION()
	void HandleCommandButton3Clicked();

	UFUNCTION()
	void HandleCommandButton4Clicked();

	UFUNCTION()
	void HandleCommandButton5Clicked();

	UFUNCTION()
	void HandleCommandButton6Clicked();

	UFUNCTION()
	void HandleCommandButton7Clicked();

	void BuildProgrammaticLayout();
	void ConfigureCommandButton(int32 ButtonIndex, const FGameXXKMVPCommandDescriptor* Command);
	void ExecuteCommandButtonAtIndex(int32 ButtonIndex);
	void BindCommandButton(UButton* Button, int32 ButtonIndex);

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> RootBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusText;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UButton>> CommandButtons;

	UPROPERTY(Transient)
	TArray<TObjectPtr<UTextBlock>> CommandButtonLabels;

	UPROPERTY(Transient)
	TArray<FName> CommandButtonNames;

	UPROPERTY(Transient)
	FString StartGameSlotNameOverride;

	UPROPERTY(Transient)
	int32 StartGameUserIndexOverride = 0;
};
