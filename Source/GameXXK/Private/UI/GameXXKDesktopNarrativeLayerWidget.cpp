#include "UI/GameXXKDesktopNarrativeLayerWidget.h"

#include "Algo/AllOf.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SafeZone.h"
#include "Components/ScaleBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "UI/GameXXKDialogueHistoryWidget.h"
#include "UI/GameXXKDialoguePanelWidget.h"

namespace
{
	constexpr float ReferenceWidth = 1920.0f;
	constexpr float ReferenceHeight = 1080.0f;

	FVector4 NormalizeReferenceRect(const FVector4& Rect)
	{
		return FVector4(
			Rect.X / ReferenceWidth,
			Rect.Y / ReferenceHeight,
			Rect.Z / ReferenceWidth,
			Rect.W / ReferenceHeight);
	}

	FVector4 ResolvePhysicalRect(const FVector4& NormalizedRect, const FVector2D& HostSize)
	{
		return FVector4(
			NormalizedRect.X * HostSize.X,
			NormalizedRect.Y * HostSize.Y,
			NormalizedRect.Z * HostSize.X,
			NormalizedRect.W * HostSize.Y);
	}

	void ConfigureCanvasRect(UWidget* Widget, const FVector4& Rect, const int32 ZOrder)
	{
		if (UCanvasPanelSlot* const Slot = Widget ? Cast<UCanvasPanelSlot>(Widget->Slot) : nullptr)
		{
			Slot->SetAnchors(FAnchors(0.0f, 0.0f));
			Slot->SetAlignment(FVector2D::ZeroVector);
			Slot->SetAutoSize(false);
			Slot->SetPosition(FVector2D(Rect.X, Rect.Y));
			Slot->SetSize(FVector2D(Rect.Z, Rect.W));
			Slot->SetZOrder(ZOrder);
		}
	}

	FName NarrativeSlotName(const EGameXXKDesktopNarrativeSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKDesktopNarrativeSlot::Left:
			return TEXT("Left");
		case EGameXXKDesktopNarrativeSlot::Center:
			return TEXT("Center");
		case EGameXXKDesktopNarrativeSlot::Right:
			return TEXT("Right");
		case EGameXXKDesktopNarrativeSlot::Prop:
			return TEXT("Prop");
		case EGameXXKDesktopNarrativeSlot::Vfx:
			return TEXT("Vfx");
		default:
			return NAME_None;
		}
	}
}

static FGameXXKDesktopNarrativeLayout ResolveNarrativeLayoutForHost(
	const FVector2D& HostSize)
{
	FGameXXKDesktopNarrativeLayout Result;
	Result.HostSize = FVector2D(
		FMath::Max(1.0f, HostSize.X),
		FMath::Max(1.0f, HostSize.Y));
	Result.StageRectNormalized = NormalizeReferenceRect(FVector4(160.0f, 80.0f, 1600.0f, 620.0f));
	Result.DialogueHostRectNormalized = NormalizeReferenceRect(FVector4(192.0f, 735.0f, 1536.0f, 285.0f));
	Result.PauseRectNormalized = NormalizeReferenceRect(FVector4(1770.0f, 40.0f, 110.0f, 48.0f));
	Result.HistoryRectNormalized = NormalizeReferenceRect(FVector4(120.0f, 120.0f, 720.0f, 760.0f));
	Result.StageRect = ResolvePhysicalRect(Result.StageRectNormalized, Result.HostSize);
	Result.DialogueHostRect = ResolvePhysicalRect(Result.DialogueHostRectNormalized, Result.HostSize);
	Result.PauseRect = ResolvePhysicalRect(Result.PauseRectNormalized, Result.HostSize);
	Result.HistoryRect = ResolvePhysicalRect(Result.HistoryRectNormalized, Result.HostSize);

	const TPair<FName, FVector4> ReferenceSlots[] = {
		{TEXT("Left"), FVector4(80.0f, 100.0f, 440.0f, 440.0f)},
		{TEXT("Center"), FVector4(580.0f, 70.0f, 440.0f, 480.0f)},
		{TEXT("Right"), FVector4(1080.0f, 100.0f, 440.0f, 440.0f)},
		{TEXT("Prop"), FVector4(640.0f, 400.0f, 320.0f, 180.0f)},
		{TEXT("Vfx"), FVector4(0.0f, 0.0f, 1600.0f, 620.0f)}};
	for (const TPair<FName, FVector4>& Slot : ReferenceSlots)
	{
		const FVector4 StageNormalized(
			Slot.Value.X / 1600.0f,
			Slot.Value.Y / 620.0f,
			Slot.Value.Z / 1600.0f,
			Slot.Value.W / 620.0f);
		Result.SlotRectsNormalized.Add(Slot.Key, StageNormalized);
		const FVector4 AbsoluteReferenceRect(
			160.0f + Slot.Value.X,
			80.0f + Slot.Value.Y,
			Slot.Value.Z,
			Slot.Value.W);
		Result.SlotRects.Add(Slot.Key, FVector4(
			AbsoluteReferenceRect.X * (Result.HostSize.X / ReferenceWidth),
			AbsoluteReferenceRect.Y * (Result.HostSize.Y / ReferenceHeight),
			AbsoluteReferenceRect.Z * (Result.HostSize.X / ReferenceWidth),
			AbsoluteReferenceRect.W * (Result.HostSize.Y / ReferenceHeight)));
	}
	return Result;
}

FGameXXKDesktopNarrativeLayout ResolveGameXXKDesktopNarrativeSlateLayout(
	const FVector2D& SlateHostSize)
{
	return ResolveNarrativeLayoutForHost(SlateHostSize);
}

FGameXXKDesktopNarrativeLayout ResolveGameXXKDesktopNarrativePhysicalLayout(
	const FVector2D& PhysicalHostSize)
{
	return ResolveNarrativeLayoutForHost(PhysicalHostSize);
}

TSharedRef<SWidget> UGameXXKDesktopNarrativeLayerWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKDesktopNarrativeLayerWidget::ConstructForTest()
{
	BuildProgrammaticLayout();
}

void UGameXXKDesktopNarrativeLayerWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("DesktopNarrativeWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}

	NarrativeRoot = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("DesktopNarrativeRoot"));
	StageCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("DesktopNarrativeStageCanvas"));
	DialogueHost = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(), TEXT("DesktopNarrativeDialogueHost"));
	PauseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(), TEXT("DesktopNarrativePauseButton"));
	if (!NarrativeRoot || !StageCanvas || !DialogueHost || !PauseButton)
	{
		return;
	}
	WidgetTree->RootWidget = NarrativeRoot;
	NarrativeRoot->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	StageCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	NarrativeRoot->AddChildToCanvas(StageCanvas);

	StageBacking = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DesktopNarrativeStageBacking"));
	StageBacking->SetBrushColor(FLinearColor(0.025f, 0.02f, 0.015f, 0.34f));
	StageBacking->SetVisibility(ESlateVisibility::HitTestInvisible);
	StageCanvas->AddChildToCanvas(StageBacking);

	NarrativeSlots.Reset();
	StagePresenters.Reset();
	for (const EGameXXKDesktopNarrativeSlot SemanticSlot : {
		EGameXXKDesktopNarrativeSlot::Vfx,
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeSlot::Center,
		EGameXXKDesktopNarrativeSlot::Right,
		EGameXXKDesktopNarrativeSlot::Prop})
	{
		const FName SlotName = NarrativeSlotName(SemanticSlot);
		UCanvasPanel* const SlotPanel = WidgetTree->ConstructWidget<UCanvasPanel>(
			UCanvasPanel::StaticClass(), *FString::Printf(TEXT("DesktopNarrative%sSlot"), *SlotName.ToString()));
		SlotPanel->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		StageCanvas->AddChildToCanvas(SlotPanel);
		NarrativeSlots.Add(SlotName, SlotPanel);
		UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
			WidgetTree->ConstructWidget<UGameXXKDesktopNarrativeStagePresenterWidget>(
				UGameXXKDesktopNarrativeStagePresenterWidget::StaticClass(),
				*FString::Printf(TEXT("DesktopNarrative%sPresenter"), *SlotName.ToString()));
		if (Presenter)
		{
			Presenter->ConfigureSlot(SemanticSlot);
			Presenter->ResetPresentation();
			if (UCanvasPanelSlot* const PresenterSlot = SlotPanel->AddChildToCanvas(Presenter))
			{
				PresenterSlot->SetAnchors(FAnchors(0.0f, 0.0f, 1.0f, 1.0f));
				PresenterSlot->SetOffsets(FMargin(0.0f));
			}
			StagePresenters.Add(SlotName, Presenter);
		}
	}

	USafeZone* const DialogueSafeArea = WidgetTree->ConstructWidget<USafeZone>(
		USafeZone::StaticClass(), TEXT("DesktopNarrativeDialogueSafeArea"));
	DialogueSafeArea->SetSidesToPad(true, true, false, true);
	DialogueSafeArea->SetContent(DialogueHost);
	NarrativeRoot->AddChildToCanvas(DialogueSafeArea);
	PaperFallback = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(), TEXT("DesktopNarrativePaperFallback"));
	PaperFallback->SetBrushColor(FLinearColor(0.055f, 0.045f, 0.035f, 0.82f));
	PaperFallback->SetVisibility(ESlateVisibility::HitTestInvisible);
	DialogueHost->AddChildToCanvas(PaperFallback);

	USafeZone* const PauseSafeArea = WidgetTree->ConstructWidget<USafeZone>(
		USafeZone::StaticClass(), TEXT("DesktopNarrativePauseSafeArea"));
	PauseSafeArea->SetSidesToPad(false, true, true, false);
	PauseButton->SetBackgroundColor(FLinearColor(0.18f, 0.12f, 0.08f, 0.94f));
	UTextBlock* const PauseLabel = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(), TEXT("DesktopNarrativePauseLabel"));
	PauseLabel->SetText(FText::FromString(TEXT("Ⅱ")));
	PauseLabel->SetColorAndOpacity(FSlateColor(FLinearColor(0.92f, 0.84f, 0.66f, 1.0f)));
	PauseLabel->SetJustification(ETextJustify::Center);
	PauseButton->SetContent(PauseLabel);
	PauseButton->OnClicked.RemoveAll(this);
	PauseButton->OnClicked.AddDynamic(this, &UGameXXKDesktopNarrativeLayerWidget::HandlePauseClicked);
	PauseSafeArea->SetContent(PauseButton);
	NarrativeRoot->AddChildToCanvas(PauseSafeArea);

	UScaleBox* const DialoguePresenterHost = WidgetTree->ConstructWidget<UScaleBox>(
		UScaleBox::StaticClass(), TEXT("DesktopNarrativeDialoguePresenterHost"));
	DialoguePresenterHost->SetStretch(EStretch::ScaleToFit);
	DialoguePresenterHost->SetStretchDirection(EStretchDirection::Both);
	DialoguePresenterHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* const DialogueReferenceBox = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DesktopNarrativeDialoguePresenterReference"));
	DialogueReferenceBox->SetWidthOverride(ReferenceWidth);
	DialogueReferenceBox->SetHeightOverride(ReferenceHeight);
	DialoguePanel = WidgetTree->ConstructWidget<UGameXXKDialoguePanelWidget>(
		UGameXXKDialoguePanelWidget::StaticClass(), TEXT("DesktopNarrativeDialoguePanel"));
	DialogueReferenceBox->SetContent(DialoguePanel);
	DialoguePresenterHost->SetContent(DialogueReferenceBox);
	NarrativeRoot->AddChildToCanvas(DialoguePresenterHost);

	USizeBox* const HistoryPresenterHost = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(), TEXT("DesktopNarrativeHistoryPresenterHost"));
	HistoryPresenterHost->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DialogueHistory = WidgetTree->ConstructWidget<UGameXXKDialogueHistoryWidget>(
		UGameXXKDialogueHistoryWidget::StaticClass(), TEXT("DesktopNarrativeDialogueHistory"));
	DialogueHistory->HideHistory();
	HistoryPresenterHost->SetContent(DialogueHistory);
	NarrativeRoot->AddChildToCanvas(HistoryPresenterHost);

	ApplyHostSize(ResolvedLayout.HostSize);
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyHostSize(const FVector2D& HostSize)
{
	ResolvedLayout = ResolveGameXXKDesktopNarrativeSlateLayout(HostSize);
	ApplyResolvedLayout();
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyResolvedLayout()
{
	if (!NarrativeRoot || !StageCanvas)
	{
		return;
	}
	ConfigureCanvasRect(StageCanvas, ResolvedLayout.StageRect, 1);
	ConfigureCanvasRect(
		StageBacking,
		FVector4(0.0f, 0.0f, ResolvedLayout.StageRect.Z, ResolvedLayout.StageRect.W),
		0);
	ConfigureCanvasRect(
		PaperFallback,
		FVector4(0.0f, 0.0f, ResolvedLayout.DialogueHostRect.Z, ResolvedLayout.DialogueHostRect.W),
		0);
	if (UWidget* const DialogueSafeArea = GetNamedWidgetForTest(TEXT("DesktopNarrativeDialogueSafeArea")))
	{
		ConfigureCanvasRect(DialogueSafeArea, ResolvedLayout.DialogueHostRect, 10);
	}
	if (UWidget* const PauseSafeArea = GetNamedWidgetForTest(TEXT("DesktopNarrativePauseSafeArea")))
	{
		ConfigureCanvasRect(PauseSafeArea, ResolvedLayout.PauseRect, 20);
	}
	if (UWidget* const DialoguePresenterHost = GetNamedWidgetForTest(TEXT("DesktopNarrativeDialoguePresenterHost")))
	{
		ConfigureCanvasRect(
			DialoguePresenterHost,
			FVector4(0.0f, 0.0f, ResolvedLayout.HostSize.X, ResolvedLayout.HostSize.Y),
			30);
	}
	if (UWidget* const HistoryPresenterHost = GetNamedWidgetForTest(TEXT("DesktopNarrativeHistoryPresenterHost")))
	{
		ConfigureCanvasRect(HistoryPresenterHost, ResolvedLayout.HistoryRect, 40);
	}
	for (const TPair<FName, TObjectPtr<UCanvasPanel>>& Pair : NarrativeSlots)
	{
		const FVector4* const AbsoluteRect = ResolvedLayout.SlotRects.Find(Pair.Key);
		if (!AbsoluteRect)
		{
			continue;
		}
		ConfigureCanvasRect(
			Pair.Value,
			FVector4(
				AbsoluteRect->X - ResolvedLayout.StageRect.X,
				AbsoluteRect->Y - ResolvedLayout.StageRect.Y,
				AbsoluteRect->Z,
				AbsoluteRect->W),
			Pair.Key == TEXT("Vfx") ? 1 : (Pair.Key == TEXT("Prop") ? 3 : 2));
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ShowLayer()
{
	if (GetVisibility() != ESlateVisibility::Visible)
	{
		SetVisibility(ESlateVisibility::Visible);
	}
}

void UGameXXKDesktopNarrativeLayerWidget::HideLayer()
{
	if (GetVisibility() != ESlateVisibility::Collapsed)
	{
		SetVisibility(ESlateVisibility::Collapsed);
	}
}

bool UGameXXKDesktopNarrativeLayerWidget::IsPresentationReady() const
{
	UWidget* const DialogueSafeArea = GetNamedWidgetForTest(TEXT("DesktopNarrativeDialogueSafeArea"));
	UWidget* const PauseSafeArea = GetNamedWidgetForTest(TEXT("DesktopNarrativePauseSafeArea"));
	UWidget* const DialoguePresenterHost = GetNamedWidgetForTest(TEXT("DesktopNarrativeDialoguePresenterHost"));
	UWidget* const HistoryPresenterHost = GetNamedWidgetForTest(TEXT("DesktopNarrativeHistoryPresenterHost"));
	return NarrativeRoot
		&& StageCanvas
		&& DialogueHost
		&& PauseButton
		&& PaperFallback
		&& StageBacking
		&& DialoguePanel
		&& DialogueHistory
		&& StagePresenters.Num() == 5
		&& DialoguePanel->GetParent()
		&& DialogueHistory->GetParent()
		&& Cast<UCanvasPanelSlot>(StageCanvas->Slot)
		&& Cast<UCanvasPanelSlot>(DialogueSafeArea ? DialogueSafeArea->Slot : nullptr)
		&& Cast<UCanvasPanelSlot>(PauseSafeArea ? PauseSafeArea->Slot : nullptr)
		&& Cast<UCanvasPanelSlot>(DialoguePresenterHost ? DialoguePresenterHost->Slot : nullptr)
		&& Cast<UCanvasPanelSlot>(HistoryPresenterHost ? HistoryPresenterHost->Slot : nullptr)
		&& Algo::AllOf(StagePresenters, [](const TPair<FName,
			TObjectPtr<UGameXXKDesktopNarrativeStagePresenterWidget>>& Pair)
			{
				return Pair.Value
					&& Pair.Value->GetParent()
					&& Pair.Value->IsPresentationReady();
			});
}

bool UGameXXKDesktopNarrativeLayerWidget::IsLayerVisible() const
{
	return GetVisibility() != ESlateVisibility::Collapsed
		&& GetVisibility() != ESlateVisibility::Hidden;
}

void UGameXXKDesktopNarrativeLayerWidget::SetPauseRequested(
	FGameXXKDesktopNarrativePauseRequestedDelegate InDelegate)
{
	PauseRequestedDelegate = MoveTemp(InDelegate);
}

UCanvasPanel* UGameXXKDesktopNarrativeLayerWidget::FindNarrativeSlot(
	const FName SlotName) const
{
	const TObjectPtr<UCanvasPanel>* const FoundSlot = NarrativeSlots.Find(SlotName);
	return FoundSlot ? FoundSlot->Get() : nullptr;
}

UGameXXKDesktopNarrativeStagePresenterWidget*
UGameXXKDesktopNarrativeLayerWidget::GetStagePresenter(
	const EGameXXKDesktopNarrativeSlot SemanticSlot) const
{
	const TObjectPtr<UGameXXKDesktopNarrativeStagePresenterWidget>* const Found =
		StagePresenters.Find(NarrativeSlotName(SemanticSlot));
	return Found ? Found->Get() : nullptr;
}

void UGameXXKDesktopNarrativeLayerWidget::ResetStagePresentation()
{
	for (const TPair<FName, TObjectPtr<UGameXXKDesktopNarrativeStagePresenterWidget>>& Pair :
		StagePresenters)
	{
		if (Pair.Value)
		{
			Pair.Value->ResetPresentation();
		}
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyStageRolePresentation(
	const FName RoleId,
	const FName ResourceId,
	const EGameXXKDesktopNarrativeSlot SemanticSlot,
	const EGameXXKDesktopNarrativeFacing Facing,
	const EGameXXKDesktopNarrativeRoleActionState ActionState,
	const FName ActionId,
	const bool bVisible)
{
	if (!bVisible)
	{
		for (const EGameXXKDesktopNarrativeSlot RoleSlot : {
			EGameXXKDesktopNarrativeSlot::Left,
			EGameXXKDesktopNarrativeSlot::Center,
			EGameXXKDesktopNarrativeSlot::Right})
		{
			if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
				GetStagePresenter(RoleSlot);
				Presenter && Presenter->GetPresentedRoleId() == RoleId)
			{
				Presenter->ClearRole();
			}
		}
		return;
	}
	for (const EGameXXKDesktopNarrativeSlot RoleSlot : {
		EGameXXKDesktopNarrativeSlot::Left,
		EGameXXKDesktopNarrativeSlot::Center,
		EGameXXKDesktopNarrativeSlot::Right})
	{
		if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
			GetStagePresenter(RoleSlot);
			Presenter
				&& (Presenter->GetPresentedRoleId() == RoleId || RoleSlot == SemanticSlot))
		{
			Presenter->ClearRole();
		}
	}
	if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
		GetStagePresenter(SemanticSlot))
	{
		Presenter->PresentRole(RoleId, ResourceId, Facing, ActionState, ActionId);
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyStagePropPresentation(
	const FName ResourceId)
{
	if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
		GetStagePresenter(EGameXXKDesktopNarrativeSlot::Prop))
	{
		Presenter->PresentProp(ResourceId);
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyStageVfxPresentation(
	const FName ResourceId)
{
	if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
		GetStagePresenter(EGameXXKDesktopNarrativeSlot::Vfx))
	{
		Presenter->PresentVfx(ResourceId);
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyStageFlashPresentation(
	const FName ResourceId)
{
	if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
		GetStagePresenter(EGameXXKDesktopNarrativeSlot::Vfx))
	{
		Presenter->PresentFlash(ResourceId);
	}
}

void UGameXXKDesktopNarrativeLayerWidget::ApplyStageToastPresentation(
	const FName ResourceId)
{
	if (UGameXXKDesktopNarrativeStagePresenterWidget* const Presenter =
		GetStagePresenter(EGameXXKDesktopNarrativeSlot::Vfx))
	{
		Presenter->PresentToast(ResourceId);
	}
}

UWidget* UGameXXKDesktopNarrativeLayerWidget::GetNamedWidgetForTest(
	const FName WidgetName) const
{
	return WidgetTree ? WidgetTree->FindWidget(WidgetName) : nullptr;
}

FVector4 UGameXXKDesktopNarrativeLayerWidget::GetNarrativeSlotRectForTest(
	const FName SlotName) const
{
	return ResolvedLayout.SlotRects.FindRef(SlotName);
}

void UGameXXKDesktopNarrativeLayerWidget::HandlePauseClicked()
{
	if (PauseRequestedDelegate.IsBound())
	{
		PauseRequestedDelegate.Execute();
	}
}
