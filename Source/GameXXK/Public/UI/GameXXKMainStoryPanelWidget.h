#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GameXXKMainStoryPanelWidget.generated.h"

class UCanvasPanel;
class UScrollBox;
class USizeBox;
class UGameXXKMainStorySubsystem;
class UGameXXKMainStoryPanelWidget;

DECLARE_DELEGATE(FGameXXKMainStoryPanelClosed);

UCLASS()
class GAMEXXK_API UGameXXKMainStoryActionButton : public UButton
{
	GENERATED_BODY()
public:
	void Configure(UGameXXKMainStoryPanelWidget* InOwner, int32 InAction, FName InNode = NAME_None);
private:
	UFUNCTION() void Clicked();
	UPROPERTY(Transient) TObjectPtr<UGameXXKMainStoryPanelWidget> OwnerPanel;
	int32 Action = 0;
	FName NodeId;
};

/** Scroll-content-local connectors; never round-trip through desktop absolute geometry. */
UCLASS()
class GAMEXXK_API UGameXXKMainStoryGraphWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetContext(UGameXXKMainStoryPanelWidget* InPanel, UGameXXKMainStorySubsystem* InStory, FName InChapter, float InTextScale);
protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry, const FSlateRect& MyCullingRect,
		FSlateWindowElementList& OutDrawElements, int32 LayerId, const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;
private:
	void BuildGraph();
	UPROPERTY(Transient) TObjectPtr<UGameXXKMainStoryPanelWidget> Panel;
	UPROPERTY(Transient) TObjectPtr<UGameXXKMainStorySubsystem> Story;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	FName ChapterId;
	float TextScale = 1.f;
	TArray<TArray<FVector2D>> Connectors;
	TArray<FLinearColor> ConnectorColors;
};

/** The same paper content is hosted in the desktop backpack area and in-route modal. */
UCLASS()
class GAMEXXK_API UGameXXKMainStoryPanelWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void SetContext(UGameXXKMainStorySubsystem* InStory, FName InChapter, bool bInFullscreen = false);
	void HandleAction(int32 Action, FName NodeId = NAME_None);
	void SelectNode(FName NodeId);
	FGameXXKMainStoryPanelClosed OnClosed;
	FName GetSelectedNode() const { return SelectedNode; }
	FName GetSelectedChapter() const { return ChapterId; }
	bool IsDialogueReplayActive() const { return bReplay; }
	int32 GetDialogueReplayIndex() const { return ReplayIndex; }

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	virtual void NativeTick(const FGeometry& Geometry, float DeltaTime) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
private:
	void EnsureRoot();
	void RebuildContent();
	void OnStoryChanged();
	void BuildTree();
	void BuildDetail();
	void BuildResult();
	void BuildHeader(const FText& Title);
	void AddText(const FText& Text, FVector2D Position, FVector2D Size, int32 FontSize, bool bBold = false, FLinearColor Color = FLinearColor(-1,-1,-1,-1));
	void AddScrollableText(const FText& Text, FVector2D Position, FVector2D Size, int32 FontSize, bool bBold = false);
	UGameXXKMainStoryActionButton* AddAction(FName Name, const FText& Text, int32 Action, FVector2D Position, FVector2D Size, bool bPrimary = true, FName NodeId = NAME_None);
	void ClosePanel();
	FText RewardText(FName NodeId) const;
	UPROPERTY(Transient) TObjectPtr<UGameXXKMainStorySubsystem> Story;
	UPROPERTY(Transient) TObjectPtr<UCanvasPanel> Canvas;
	UPROPERTY(Transient) TObjectPtr<USizeBox> DesignSize;
	UPROPERTY(Transient) TObjectPtr<UScrollBox> TreeScroll;
	FName ChapterId;
	FName SelectedNode;
	bool bFullscreen = false;
	bool bDetail = false;
	bool bDirty = true;
	bool bRefreshQueued = false;
	bool bReplay = false;
	int32 ReplayIndex = 0;
	float TreeScrollOffset = 0.f;
	float TextScale = 1.f;
};
