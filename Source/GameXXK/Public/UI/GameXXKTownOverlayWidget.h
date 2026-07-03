#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKTownOverlayWidget.generated.h"

class UButton;
class UCanvasPanel;
class UTextBlock;
class UVerticalBox;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKTownOverlayWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	void RefreshFromState();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool EnterRouteMap();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town")
	bool SaveToSlotOne();

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Town|Test")
	bool ExecuteTownCommandForTest(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	TArray<FGameXXKMVPCommandDescriptor> BuildTownActionsForTest() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	bool HasTownActionForTest(FName CommandName, bool bRequireEnabled) const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	bool IsTownOverlayVisible() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Town|Test")
	FText GetStatusTextForTest() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Town")
	void OnRouteMapEntered();

private:
	void BuildProgrammaticLayout();
	void ConfigureProgrammaticLayout();
	bool ExecuteTownCommand(FName CommandName);
	UTextBlock* AddTextBlock(UVerticalBox* Parent, const FText& Text) const;
	UButton* AddCommandButton(UVerticalBox* Parent, const FText& Label) const;

	UFUNCTION()
	void HandleSaveClicked();

	UFUNCTION()
	void HandleEnterRouteClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UVerticalBox> PanelBox;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> StatusTextBlock;

	UPROPERTY(Transient)
	TObjectPtr<UButton> SaveButton;

	UPROPERTY(Transient)
	TObjectPtr<UButton> EnterRouteButton;

	UPROPERTY(Transient)
	FText CachedStatusText;
};
