#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKInterfaceHelpWidget.generated.h"

class UBorder;
class UButton;
class UCanvasPanel;
class UTextBlock;
class USizeBox;
class UGameXXKGuideSpotlightWidget;

/** Context help and an operation-driven UI course; neither owns a gameplay input gate. */
UCLASS()
class GAMEXXK_API UGameXXKInterfaceHelpWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	void ShowForHost(UUserWidget* Host, FName Surface, bool bDesktopFrame = false, int32 HudPercent = 100);
    void ShowTutorial(UUserWidget* Host, const TSet<FName>& Completed, int32 HudPercent,
        TFunction<void(FName)> PrepareContext, TFunction<bool(FName)> RecordCompletion);
    bool IsTutorial() const { return bTutorial; }
	void Dismiss();
	bool IsOpen() const { return bOpen; }
	void SetDesktopHudPercent(int32 Percent) { DesktopHudPercent = Percent; }
	int32 GetStepCountForTest() const { return Steps.Num(); }
    int32 GetCurrentStepForTest() const { return StepIndex; }
    FName GetCurrentCompletionIdForTest() const;
    UWidget* GetCurrentTargetForTest() const;
    void ConfirmReadingForTest();
    FVector4 GetReadingRectForTest() const { return ReadingRect; }
	void SetLayoutChangedDelegate(FSimpleDelegate Delegate) { LayoutChanged = MoveTemp(Delegate); }
	void SetDismissedDelegate(FSimpleDelegate Delegate) { Dismissed = MoveTemp(Delegate); }
private:
	struct FStep
	{
		FString HeadingKey;
		FString BodyKey;
		TArray<FName> WidgetNames;
		FName RegisteredTarget;
        FName CompletionId;
        FName Context;
        bool bAction = false;
        bool bHover = false;
	};
	void Build();
	void AddVisibleStep(const TCHAR* Key, TArray<FName> Names, FName RegisteredTarget = NAME_None);
	UWidget* FindTarget(const FStep& Step) const;
	bool ResolveTargetRect(const FStep& Step, FSlateRect& Rect) const;
	void RefreshPage();
	void UpdateReadingLayout();
    void PrepareCurrentStep();
    bool CompleteCurrentStep();
    void ClearTargetBinding();
    UFUNCTION() void TargetClicked();
    UFUNCTION() void RecoverTarget();
	UFUNCTION() void Previous();
	UFUNCTION() void Next();
	UFUNCTION() void CloseClicked();
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> HelpCanvas;
	UPROPERTY(Transient) TObjectPtr<UBorder> ReadingPanel;
	UPROPERTY(Transient) TObjectPtr<UGameXXKGuideSpotlightWidget> Spotlight;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Body;
	UPROPERTY(Transient) TObjectPtr<UTextBlock> Counter;
	UPROPERTY(Transient) TObjectPtr<UButton> PreviousButton;
	UPROPERTY(Transient) TObjectPtr<UButton> NextButton;
	UPROPERTY(Transient) TObjectPtr<USizeBox> BodySize;
    UPROPERTY(Transient) TObjectPtr<UButton> BoundTargetButton;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> NextLabel;
    UPROPERTY(Transient) TObjectPtr<UTextBlock> PreviousLabel;
	TWeakObjectPtr<UUserWidget> TargetHost;
	TArray<FStep> Steps;
	FSimpleDelegate Dismissed;
    FSimpleDelegate LayoutChanged;
	int32 StepIndex = 0;
	int32 DesktopHudPercent = 100;
	bool bUseDesktopFrame = false;
	bool bOpen = false;
	bool bBrowseAllInterfaces = false;
    bool bTutorial = false;
    bool bReplay = false;
    bool bAdvancePending = false;
    bool bPreparePending = false;
    bool bPreparing = false;
    float HoverSeconds = 0;
    bool bHoverObserved = false;
    uint64 PresentedLanguageRevision = 0;
    FVector4 ReadingRect = FVector4(0,0,0,0);
    TFunction<void(FName)> PrepareContextCallback;
    TFunction<bool(FName)> RecordCompletionCallback;
};
