#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "GameXXKTalentTypes.h"
#include "GameXXKTalentTreeWidget.generated.h"

class UCanvasPanel;
class UGameXXKMVPSubsystem;
class UImage;
class UScrollBox;
class UTextBlock;
class UTexture2D;
class UGameXXKTalentTreeWidget;

DECLARE_MULTICAST_DELEGATE(FGameXXKTalentPurchaseCommitted);

UCLASS()
class GAMEXXK_API UGameXXKTalentNodeButton : public UButton
{
	GENERATED_BODY()

public:
	void Configure(UGameXXKTalentTreeWidget* InOwner, FName InNodeId);

private:
	UFUNCTION()
	void HandleClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKTalentTreeWidget> Owner;

	FName NodeId = NAME_None;
};

/** Scrollable, save-authoritative four-branch permanent talent graph. */
UCLASS()
class GAMEXXK_API UGameXXKTalentTreeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	void SetMVPSubsystem(UGameXXKMVPSubsystem* InSubsystem);
	void RebuildForTest();
	void TickForTest(float DeltaSeconds);
	bool ClickPurchaseButtonForTest();
	bool SelectNodeForTest(FName NodeId);
	bool PurchaseSelectedForTest();
	int32 GetVisibleNodeCountForTest() const;
	bool IsNodeVisibleForTest(FName NodeId) const;
	FName GetSelectedNodeIdForTest() const { return SelectedNodeId; }
	FString GetNodeFrameResourcePathForTest(FName NodeId) const;
	FString GetNodeIconResourcePathForTest(FName NodeId) const;
	FVector2D GetNodeIconDesiredSizeForTest(FName NodeId) const;
	FText GetNodeNameForTest(FName NodeId) const;
	FText GetNodeRankForTest(FName NodeId) const;
	bool IsNodeTextInsideButtonForTest(FName NodeId) const;
	FString GetPurchaseButtonResourcePathForTest() const;
	FText GetPurchaseButtonLabelForTest() const;
	TArray<float> GetRenderedConnectionAnglesForTest() const { return RenderedConnectionAngles; }
	TArray<FVector2D> GetRenderedConnectionBoundaryOffsetsForTest() const
	{
		return RenderedConnectionBoundaryOffsets;
	}
	FLinearColor GetNodeIconTintForTest(FName NodeId) const;
	FGameXXKTalentPurchaseCommitted& OnPurchaseCommitted() { return PurchaseCommitted; }

	void HandleNodeClicked(FName NodeId);

protected:
	virtual TSharedRef<SWidget> RebuildWidget() override;
	virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
	void BuildProgrammaticLayout();
	void RebuildGraphAndDetails();
	void BuildGraph(UCanvasPanel* GraphCanvas, const TArray<FGameXXKTalentNodeView>& Views);
	void BuildDetails(const TArray<FGameXXKTalentNodeView>& Views);
	const FGameXXKTalentNodeView* FindSelectedView(const TArray<FGameXXKTalentNodeView>& Views) const;

	UFUNCTION()
	void HandlePurchaseClicked();

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> MVPSubsystem;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> RootCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> GraphCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> HorizontalScroll;

	UPROPERTY(Transient)
	TObjectPtr<UScrollBox> VerticalScroll;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailNameText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> DetailBodyText;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> UpgradePriceText;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PurchaseButton;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UImage>> NodeIconImages;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKTalentNodeButton>> NodeButtons;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextBlock>> NodeNameTexts;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UTextBlock>> NodeRankTexts;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UImage>> NodeSelectedFrames;

	FName SelectedNodeId = TEXT("Talent.Root");
	TSet<FName> VisibleNodeIds;
	TArray<float> RenderedConnectionAngles;
	TArray<FVector2D> RenderedConnectionBoundaryOffsets;
	bool bSlateRebuildPending = false;
	FGameXXKTalentPurchaseCommitted PurchaseCommitted;
};
