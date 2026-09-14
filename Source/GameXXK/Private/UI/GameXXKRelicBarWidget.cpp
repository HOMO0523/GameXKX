#include "UI/GameXXKRelicBarWidget.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/ScrollBox.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"
#include "Components/UniformGridSlot.h"
#include "UI/GameXXKCardTooltipPresentation.h"
#include "Engine/Texture2D.h"
#include "GameXXKRelicCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"

namespace
{
	constexpr float IconSize = 52.0f;
	constexpr float SlotGap = 6.0f;
	constexpr int32 ColumnCount = 6;
}

TSharedRef<SWidget> UGameXXKRelicBarWidget::RebuildWidget()
{
	EnsureWidgetTree();
	return Super::RebuildWidget();
}

void UGameXXKRelicBarWidget::NativeConstruct()
{
	Super::NativeConstruct();
	EnsureWidgetTree();
	RefreshFromState();
}

bool UGameXXKRelicBarWidget::PrepareForEmbedding()
{
	EnsureWidgetTree();
	return RootCanvas && RelicGrid && WidgetTree && WidgetTree->RootWidget == RootCanvas;
}

void UGameXXKRelicBarWidget::EnsureWidgetTree()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("RelicBarWidgetTree"));
	if (!WidgetTree || RootCanvas) return;
	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RelicBarRootCanvas"));
	WidgetTree->RootWidget = RootCanvas;
	RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	// Match the battle's centered 1920x1080 safe stage, including letterboxing.
	// Only the compact icon panel receives input; the full-screen host stays transparent.
	UScaleBox* SafeStage = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass(), TEXT("RelicBarSafeStage"));
	SafeStage->SetStretch(EStretch::ScaleToFit);
	SafeStage->SetStretchDirection(EStretchDirection::Both);
	SafeStage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	USizeBox* DesignSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RelicBarDesignSize"));
	DesignSize->SetWidthOverride(1920.0f);
	DesignSize->SetHeightOverride(1080.0f);
	DesignSize->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	UCanvasPanel* DesignStage = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RelicBarDesignStage"));
	DesignStage->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	DesignSize->SetContent(DesignStage);
	SafeStage->SetContent(DesignSize);
	if (auto* ContentSlot = Cast<UScaleBoxSlot>(DesignSize->Slot))
	{
		ContentSlot->SetHorizontalAlignment(HAlign_Center);
		ContentSlot->SetVerticalAlignment(VAlign_Center);
	}
	if (auto* SafeStageSlot = RootCanvas->AddChildToCanvas(SafeStage))
	{
		SafeStageSlot->SetAnchors(FAnchors(0, 0, 1, 1));
		SafeStageSlot->SetOffsets(FMargin(0));
	}
	RelicGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass(), TEXT("RelicBarSixColumnGrid"));
	RelicGrid->SetSlotPadding(FMargin(SlotGap * 0.5f));
	UScrollBox* Scroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("RelicBarOverflow"));
	Scroll->SetOrientation(Orient_Vertical); FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(Scroll,12.0f,36.0f);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	Scroll->AddChild(RelicGrid);
	if (UCanvasPanelSlot* GridCanvasSlot = DesignStage->AddChildToCanvas(Scroll))
	{
		GridCanvasSlot->SetAnchors(FAnchors(0.0f,0.0f));
		GridCanvasSlot->SetAlignment(FVector2D::ZeroVector);
		// Compact log ends at X=246; expanded log starts at Y=334.
		GridCanvasSlot->SetPosition(FVector2D(352.0f,36.0f));
		GridCanvasSlot->SetSize(FVector2D(ColumnCount*(IconSize+SlotGap)+20.0f,116.0f));
		GridCanvasSlot->SetZOrder(1);
	}
	SetVisibility(ESlateVisibility::Collapsed);
}

void UGameXXKRelicBarWidget::RefreshFromState()
{
	EnsureWidgetTree();
	if (!RelicGrid) return;
	RelicGrid->ClearChildren();
	RenderedRelicCount = 0;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const bool bRouteOrBattle = State.bDungeonActive && (State.Screen == EGameXXKScreen::DungeonMap
		|| State.Screen == EGameXXKScreen::RouteEvent
		|| State.Screen == EGameXXKScreen::RouteCamp
		|| State.Screen == EGameXXKScreen::RouteMerchant
		|| State.Screen == EGameXXKScreen::Battle);
	if (!bRouteOrBattle)
	{
		SetVisibility(ESlateVisibility::Collapsed);
		return;
	}

	for (int32 Index = 0; Index < State.CardRun.Relics.Num(); ++Index)
	{
		const FGameXXKRelicInstance& Instance = State.CardRun.Relics[Index];
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Instance.RelicId);
		if (!Definition) continue;
		USizeBox* IconBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), *FString::Printf(TEXT("RelicSlot_%d"), Index));
		IconBox->SetWidthOverride(IconSize);
		IconBox->SetHeightOverride(IconSize);
		// SizeBox defaults to SelfHitTestInvisible and every image below it is
		// decorative. The slot itself must receive hover for Slate tooltips.
		IconBox->SetVisibility(ESlateVisibility::Visible);
		IconBox->SetToolTipText(FText::Format(NSLOCTEXT("GameXXKRelics", "RelicTooltip", "{0}\n{1}"), Definition->DisplayName, Definition->Description));
		IconBox->SetToolTip(GameXXKCardTooltipPresentation::BuildCompactTooltip(
			WidgetTree, Definition->DisplayName, Definition->Description.ToString()));
		UOverlay* Overlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass());
		IconBox->SetContent(Overlay);
		UImage* Icon = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass());
		if (UTexture2D* Texture = Cast<UTexture2D>(Definition->IconTexturePath.TryLoad())) Icon->SetBrushFromTexture(Texture, true);
		Icon->SetDesiredSizeOverride(FVector2D(IconSize, IconSize));
		Icon->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UOverlaySlot* IconSlot = Overlay->AddChildToOverlay(Icon))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Fill);
			IconSlot->SetVerticalAlignment(VAlign_Fill);
		}
		if (Definition->bStackable && Instance.Stacks > 1)
		{
			UTextBlock* StackText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass());
			StackText->SetText(FText::AsNumber(Instance.Stacks));
			StackText->SetColorAndOpacity(FSlateColor(FLinearColor(0.10f, 0.08f, 0.05f, 1.0f)));
			FSlateFontInfo Font = FGameXXKInRunUiStyle::BodyFont(18, true);
			StackText->SetFont(Font);
			StackText->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UOverlaySlot* TextSlot = Overlay->AddChildToOverlay(StackText))
			{
				TextSlot->SetHorizontalAlignment(HAlign_Right);
				TextSlot->SetVerticalAlignment(VAlign_Bottom);
				TextSlot->SetPadding(FMargin(0.0f, 0.0f, 5.0f, 3.0f));
			}
		}
		RelicGrid->AddChildToUniformGrid(IconBox, Index / ColumnCount, Index % ColumnCount);
		++RenderedRelicCount;
	}
	if (auto* Overflow = Cast<UScrollBox>(WidgetTree->FindWidget(TEXT("RelicBarOverflow"))))
	{
		Overflow->SetScrollBarVisibility(RenderedRelicCount > 12 ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	SetVisibility(RenderedRelicCount > 0 ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
}
