#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "UI/GameXXKDesktopNarrativeStagePresenterWidget.h"
#include "GameXXKDesktopNarrativeLayerWidget.generated.h"

class UButton;
class UCanvasPanel;
class UWidget;
class UGameXXKDialogueHistoryWidget;
class UGameXXKDialoguePanelWidget;

UENUM(BlueprintType)
enum class EGameXXKDesktopOverlaySurface : uint8
{
	Workbench,
	NarrativeFullscreen
};

/** Host-relative narrative rectangles in both normalized and physical coordinates. */
struct GAMEXXK_API FGameXXKDesktopNarrativeLayout
{
	FVector2D HostSize = FVector2D(1920.0f, 1080.0f);
	FVector4 StageRectNormalized = FVector4::Zero();
	FVector4 DialogueHostRectNormalized = FVector4::Zero();
	FVector4 PauseRectNormalized = FVector4::Zero();
	FVector4 HistoryRectNormalized = FVector4::Zero();
	FVector4 StageRect = FVector4::Zero();
	FVector4 DialogueHostRect = FVector4::Zero();
	FVector4 PauseRect = FVector4::Zero();
	FVector4 HistoryRect = FVector4::Zero();
	TMap<FName, FVector4> SlotRectsNormalized;
	TMap<FName, FVector4> SlotRects;
};

/** Pure normalized-baseline projection for Slate-space programmatic UMG geometry. */
GAMEXXK_API FGameXXKDesktopNarrativeLayout ResolveGameXXKDesktopNarrativeSlateLayout(
	const FVector2D& SlateHostSize);

/** The same normalized baseline projected into native physical-pixel geometry. */
GAMEXXK_API FGameXXKDesktopNarrativeLayout ResolveGameXXKDesktopNarrativePhysicalLayout(
	const FVector2D& PhysicalHostSize);

DECLARE_DELEGATE(FGameXXKDesktopNarrativePauseRequestedDelegate);

/**
 * Full-host sibling presentation layer for desktop narrative scenes.
 * Dialogue/sequence ownership deliberately remains outside this Task 7 shell.
 */
UCLASS()
class GAMEXXK_API UGameXXKDesktopNarrativeLayerWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	virtual TSharedRef<SWidget> RebuildWidget() override;

	void ApplyHostSize(const FVector2D& HostSize);
	void ShowLayer();
	void HideLayer();
	bool IsLayerVisible() const;
	void SetPauseRequested(FGameXXKDesktopNarrativePauseRequestedDelegate InDelegate);
	UCanvasPanel* FindNarrativeSlot(FName SlotName) const;
	UGameXXKDialoguePanelWidget* GetDialoguePanel() const { return DialoguePanel; }
	UGameXXKDialogueHistoryWidget* GetDialogueHistory() const { return DialogueHistory; }
	bool IsPresentationReady() const;
	UGameXXKDesktopNarrativeStagePresenterWidget* GetStagePresenter(
		EGameXXKDesktopNarrativeSlot SemanticSlot) const;
	void ResetStagePresentation();
	void ApplyStageRolePresentation(
		FName RoleId,
		FName ResourceId,
		EGameXXKDesktopNarrativeSlot SemanticSlot,
		EGameXXKDesktopNarrativeFacing Facing,
		EGameXXKDesktopNarrativeRoleActionState ActionState,
		FName ActionId,
		bool bVisible);
	void ApplyStagePropPresentation(FName ResourceId);
	void ApplyStageVfxPresentation(FName ResourceId);
	void ApplyStageFlashPresentation(FName ResourceId);
	void ApplyStageToastPresentation(FName ResourceId);

	void ConstructForTest();
	UWidget* GetNamedWidgetForTest(FName WidgetName) const;
	UButton* GetPauseButtonForTest() const { return PauseButton; }
	FGameXXKDesktopNarrativeLayout GetResolvedLayoutForTest() const { return ResolvedLayout; }
	FVector4 GetNarrativeSlotRectForTest(FName SlotName) const;

private:
	UFUNCTION()
	void HandlePauseClicked();

	void BuildProgrammaticLayout();
	void ApplyResolvedLayout();

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> NarrativeRoot;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> StageCanvas;

	UPROPERTY(Transient)
	TObjectPtr<UCanvasPanel> DialogueHost;

	UPROPERTY(Transient)
	TObjectPtr<UButton> PauseButton;

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> PaperFallback;

	UPROPERTY(Transient)
	TObjectPtr<class UBorder> StageBacking;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDialoguePanelWidget> DialoguePanel;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKDialogueHistoryWidget> DialogueHistory;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UCanvasPanel>> NarrativeSlots;

	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameXXKDesktopNarrativeStagePresenterWidget>> StagePresenters;

	FGameXXKDesktopNarrativeLayout ResolvedLayout;
	FGameXXKDesktopNarrativePauseRequestedDelegate PauseRequestedDelegate;
};
