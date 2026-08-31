#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "GameXXKPrologueMapWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UImage;
class UTextBlock;

UENUM()
enum class EGameXXKPrologueMapMode : uint8
{
	StoryCard,
	InspectOnly,
};

DECLARE_DELEGATE(FGameXXKPrologueMapInspectRequested);
DECLARE_DELEGATE(FGameXXKPrologueMapCloseRequested);
DECLARE_DELEGATE(FGameXXKPrologueMapContinueRequested);

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKPrologueMapWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void Configure(EGameXXKPrologueMapMode InMode);
	bool RequestInspection();
	bool RequestCloseInspection();
	bool RequestContinue();

	static FVector2D FitInspectionImageForTest(FVector2D ViewportSize);
	bool IsThumbnailVisibleForTest() const;
	bool IsInspectionOpenForTest() const { return bInspectionOpen; }
	FText GetTitleTextForTest() const { return FText::GetEmpty(); }
	bool HasInspectButtonForTest() const { return InspectButton != nullptr; }
	bool HasContinuePromptForTest() const;
	FString GetTaskIconPathForTest() const;
	FString GetInspectionTexturePathForTest() const;
	bool RequestInspectionForTest() { return RequestInspection(); }
	bool RequestCloseInspectionForTest() { return RequestCloseInspection(); }
	bool RequestContinueForTest() { return RequestContinue(); }
	void SetInspectRequestedForTest(FGameXXKPrologueMapInspectRequested InDelegate)
	{
		InspectRequested = MoveTemp(InDelegate);
	}
	void SetCloseRequestedForTest(FGameXXKPrologueMapCloseRequested InDelegate)
	{
		CloseRequested = MoveTemp(InDelegate);
	}
	void SetContinueRequestedForTest(FGameXXKPrologueMapContinueRequested InDelegate)
	{
		ContinueRequested = MoveTemp(InDelegate);
	}

private:
	void BuildProgrammaticLayout();
	void RefreshPresentation();

	UFUNCTION()
	void HandleInspectClicked();

	UFUNCTION()
	void HandleCloseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> DimMask;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> ThumbnailCard;

	UPROPERTY(Transient)
	TObjectPtr<UImage> ThumbnailImage;

	UPROPERTY(Transient)
	TObjectPtr<UButton> InspectButton;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ContinuePrompt;

	UPROPERTY(Transient)
	TObjectPtr<UBorder> InspectionPaper;

	UPROPERTY(Transient)
	TObjectPtr<UImage> InspectionImage;

	UPROPERTY(Transient)
	TObjectPtr<UButton> CloseButton;

	EGameXXKPrologueMapMode Mode = EGameXXKPrologueMapMode::StoryCard;
	bool bInspectionOpen = false;
	FGameXXKPrologueMapInspectRequested InspectRequested;
	FGameXXKPrologueMapCloseRequested CloseRequested;
	FGameXXKPrologueMapContinueRequested ContinueRequested;
};
