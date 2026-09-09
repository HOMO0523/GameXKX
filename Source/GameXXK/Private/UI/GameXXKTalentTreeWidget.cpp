#include "UI/GameXXKTalentTreeWidget.h"
#include "Audio/GameXXKSfx.h"
#include "UI/GameXXKPartyDeckUiStyle.h"
#include "UI/GameXXKInkScrollBar.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKTalentTotals.h"
#include "Brushes/SlateRoundedBoxBrush.h"

#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/PanelWidget.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "InputCoreTypes.h"
#include "Styling/CoreStyle.h"

namespace
{
	constexpr float WidgetWidth = 920.0f;
	constexpr float WidgetHeight = 450.0f;
	constexpr float GraphViewportWidth = 688.0f;
	constexpr float GraphCanvasExtent = 15000.0f;
	constexpr float GraphCenter = GraphCanvasExtent * 0.5f;
	constexpr float GraphScale = 1.0f;
	constexpr float NodeFootprintWidth = 130.0f;
	constexpr float NodeFootprintHeight = 132.0f;
	constexpr float NodeSlotSize = 90.0f;
	constexpr float NodeIconSize = 74.0f;
	constexpr float NodeSlotCenterYOffset =
		NodeSlotSize * 0.5f - NodeFootprintHeight * 0.5f;
	const TCHAR* TalentItemSlotPath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	const TCHAR* TalentSquareSelectedPath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SquareSelected.T_MasterV2_SquareSelected");
	const TCHAR* TalentUpgradeButtonPath =
		TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ButtonPurchase.T_MasterV2_ButtonPurchase");

	const FLinearColor Ink(0.16f, 0.13f, 0.09f, 1.0f);
	const FLinearColor Gold(0.92f, 0.66f, 0.18f, 1.0f);
	const FLinearColor GraphText(0.87f, 0.82f, 0.70f, 1.0f);
	const FLinearColor GraphMutedText(0.44f, 0.43f, 0.40f, 1.0f);

	bool IsPurchaseContextAllowed(const FGameXXKRuntimeState& State)
	{
		return State.Screen == EGameXXKScreen::Town
			&& !State.bDungeonActive && !State.CardRun.bHasActiveCardBattle;
	}

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FText& Text,
		const int32 Size,
		const FLinearColor& Color,
		const FName Name = NAME_None)
	{
		UTextBlock* Block = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		Block->SetText(Text);
		Block->SetColorAndOpacity(FSlateColor(Color));
		Block->SetFont(FGameXXKInRunUiStyle::Font(Size,true));
		Block->SetVisibility(ESlateVisibility::HitTestInvisible);
		Block->SetAutoWrapText(true);
		return Block;
	}

	UCanvasPanelSlot* AddCanvas(
		UCanvasPanel* Canvas,
		UWidget* Widget,
		const FVector2D Position,
		const FVector2D Size)
	{
		UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget);
		Slot->SetPosition(Position);
		Slot->SetSize(Size);
		return Slot;
	}

	UTexture2D* LoadTexture(const TCHAR* Path)
	{
		return LoadObject<UTexture2D>(nullptr, Path);
	}

	FLinearColor StateIconTint(const FGameXXKTalentNodeView& View)
	{
		return View.State == EGameXXKTalentNodeState::Locked
			? FLinearColor(0.30f, 0.30f, 0.28f, 0.92f)
			: FLinearColor::White;
	}

	FText NodeTooltip(const FGameXXKTalentNodeView& View)
	{
		return FText::FromString(FString::Printf(TEXT("%s\n%s\n%s"),
			*View.Definition.DisplayName.ToString(),
			*FGameXXKTalentRules::DescribeEffect(View.Definition).ToString(),
			View.State == EGameXXKTalentNodeState::Locked
				? *View.LockReason.ToString() : TEXT("点击查看详情")));
	}

	const TCHAR* TalentIconPath(const EGameXXKTalentIcon Icon)
	{
		switch (Icon)
		{
		case EGameXXKTalentIcon::Root:
			return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
		case EGameXXKTalentIcon::Attack:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Attack.T_TalentFlat_Attack");
		case EGameXXKTalentIcon::Health:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Health.T_TalentFlat_Health");
		case EGameXXKTalentIcon::Defense:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Defense.T_TalentFlat_Defense");
		case EGameXXKTalentIcon::Critical:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Critical.T_TalentFlat_Critical");
		case EGameXXKTalentIcon::Movement:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Movement.T_TalentFlat_Movement");
		case EGameXXKTalentIcon::Backpack:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Backpack.T_TalentFlat_Backpack");
		case EGameXXKTalentIcon::Warehouse:
			return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavWarehouse.T_TrainingNavWarehouse");
		case EGameXXKTalentIcon::Gold:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Gold.T_TalentFlat_Gold");
		case EGameXXKTalentIcon::Experience:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Experience.T_TalentFlat_Experience");
		case EGameXXKTalentIcon::Offline:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Offline.T_TalentFlat_Offline");
		case EGameXXKTalentIcon::Time:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Time.T_TalentFlat_Time");
		case EGameXXKTalentIcon::Chest:
		case EGameXXKTalentIcon::ChestTime:
			return TEXT("/Game/GameXXK/UI/Talents/Flat/T_TalentFlat_Chest.T_TalentFlat_Chest");
		case EGameXXKTalentIcon::Tools:
		case EGameXXKTalentIcon::ToolReward:
			return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTools.T_TrainingNavTools");
		default:
			return TEXT("/Game/GameXXK/UI/ImageTruth/Training/T_TrainingNavTalents.T_TrainingNavTalents");
		}
	}

	FButtonStyle MakeTextureButtonStyle(
		const TCHAR* TexturePath,
		const FVector2D Size)
	{
		FButtonStyle Style = FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"); FGameXXKSfx::SetButtonSound(Style);
		if (UTexture2D* Texture = LoadTexture(TexturePath))
		{
			FSlateBrush Brush;
			Brush.SetResourceObject(Texture);
			Brush.DrawAs = ESlateBrushDrawType::Box;
			Brush.ImageSize = Size;
			Brush.Margin = FMargin(0.08f);
			Style.Normal = Brush;
			Style.Hovered = Brush;
			Style.Hovered.TintColor = FSlateColor(FLinearColor(1.08f, 1.08f, 1.08f, 1.0f));
			Style.Pressed = Brush;
			Style.Pressed.TintColor = FSlateColor(FLinearColor(0.82f, 0.82f, 0.82f, 1.0f));
			Style.Disabled = Brush;
			Style.Disabled.TintColor = FSlateColor(FLinearColor(0.48f, 0.48f, 0.48f, 0.78f));
		}
		return Style;
	}

	bool ContainsTextWidget(UWidget* Widget)
	{
		if (!Widget)
		{
			return false;
		}
		if (Cast<UTextBlock>(Widget))
		{
			return true;
		}
		if (UPanelWidget* Panel = Cast<UPanelWidget>(Widget))
		{
			for (int32 Index = 0; Index < Panel->GetChildrenCount(); ++Index)
			{
				if (ContainsTextWidget(Panel->GetChildAt(Index)))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool ResolveConnectionAngle(const FVector2D Delta, float& OutAngle)
	{
		const float X = FMath::Abs(Delta.X);
		const float Y = FMath::Abs(Delta.Y);
		if (FMath::IsNearlyZero(Y) && X > 0.0f)
		{
			OutAngle = Delta.X >= 0.0f ? 0.0f : 180.0f;
			return true;
		}
		if (FMath::IsNearlyZero(X) && Y > 0.0f)
		{
			OutAngle = Delta.Y >= 0.0f ? 90.0f : -90.0f;
			return true;
		}
		if (X > 0.0f && FMath::IsNearlyEqual(X, Y, 0.1f))
		{
			OutAngle = Delta.X * Delta.Y >= 0.0f ? 45.0f : -45.0f;
			if (Delta.X < 0.0f)
			{
				OutAngle += 180.0f;
			}
			return true;
		}
		return false;
	}

	FVector2D SlotCenterForGraphPosition(const FVector2D GraphPosition)
	{
		return FVector2D(GraphCenter, GraphCenter)
			+ GraphPosition * GraphScale
			+ FVector2D(0.0f, NodeSlotCenterYOffset);
	}

	bool ResolveSlotBoundaryOffset(const FVector2D CenterDelta, FVector2D& OutOffset)
	{
		const float MaxAxis = FMath::Max(
			FMath::Abs(CenterDelta.X),
			FMath::Abs(CenterDelta.Y));
		if (MaxAxis <= UE_KINDA_SMALL_NUMBER)
		{
			OutOffset = FVector2D::ZeroVector;
			return false;
		}
		OutOffset = CenterDelta * ((NodeSlotSize * 0.5f) / MaxAxis);
		return true;
	}
}

void UGameXXKTalentNodeButton::Configure(
	UGameXXKTalentTreeWidget* InOwner,
	const FName InNodeId)
{
	Owner = InOwner;
	NodeId = InNodeId;
	OnClicked.Clear();
	OnClicked.AddDynamic(this, &UGameXXKTalentNodeButton::HandleClicked);
}

void UGameXXKTalentNodeButton::HandleClicked()
{
	if (Owner)
	{
		Owner->HandleNodeClicked(NodeId);
	}
}

void UGameXXKTalentTreeWidget::SetMVPSubsystem(UGameXXKMVPSubsystem* InSubsystem)
{
	MVPSubsystem = InSubsystem;
	if(RootCanvas)bSlateRebuildPending=true;
}

TSharedRef<SWidget> UGameXXKTalentTreeWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	return Super::RebuildWidget();
}

void UGameXXKTalentTreeWidget::RebuildForTest()
{
	if(RootCanvas) RebuildGraphAndDetails(); else TakeWidget();
}

void UGameXXKTalentTreeWidget::TickForTest(const float DeltaSeconds)
{
	NativeTick(FGeometry(), FMath::Max(0.0f, DeltaSeconds));
}

FVector2D UGameXXKTalentTreeWidget::GetGraphScrollOffsetForTest() const
{
	return RequestedGraphScrollOffset;
}

void UGameXXKTalentTreeWidget::PanGraphForTest(const FVector2D& DragDelta)
{
	ApplyGraphPanDelta(DragDelta);
}

FVector2D UGameXXKTalentTreeWidget::ResolveGraphPanOffset(
	const FVector2D& CurrentOffset,
	const FVector2D& DragDelta,
	const FVector2D& MaximumOffset)
{
	return FVector2D(
		FMath::Clamp(CurrentOffset.X - DragDelta.X, 0.0f, FMath::Max(0.0f, MaximumOffset.X)),
		FMath::Clamp(CurrentOffset.Y - DragDelta.Y, 0.0f, FMath::Max(0.0f, MaximumOffset.Y)));
}

bool UGameXXKTalentTreeWidget::ClickPurchaseButtonForTest()
{
	if (!PurchaseButton || !MVPSubsystem)
	{
		return false;
	}
	const int32 RankBefore = MVPSubsystem->GetRuntimeState().Talents.NodeRanks.FindRef(SelectedNodeId);
	PurchaseButton->OnClicked.Broadcast();
	return MVPSubsystem->GetRuntimeState().Talents.NodeRanks.FindRef(SelectedNodeId) > RankBefore;
}

bool UGameXXKTalentTreeWidget::SelectNodeForTest(const FName NodeId)
{
	if (!VisibleNodeIds.Contains(NodeId))
	{
		return false;
	}
	bShowTotals=false;
	LastPurchaseError = FText::GetEmpty();
	SelectedNodeId = NodeId;
	for (const TPair<FName, TObjectPtr<UGameXXKTalentNodeButton>>& Pair : NodeButtons)
	{
		if (Pair.Value)
		{
			Pair.Value->SetStyle(MakeTextureButtonStyle(
				Pair.Key == SelectedNodeId ? TalentSquareSelectedPath : TalentItemSlotPath,
				FVector2D(NodeSlotSize, NodeSlotSize)));
		}
	}
	BuildDetails(MVPSubsystem ? MVPSubsystem->GetTalentNodeViews() : TArray<FGameXXKTalentNodeView>());
	return true;
}

bool UGameXXKTalentTreeWidget::PurchaseSelectedForTest()
{
	if (!MVPSubsystem || SelectedNodeId.IsNone())
	{
		return false;
	}
	FGameXXKTalentPurchaseResult Result;
	if (!MVPSubsystem->PurchaseTalentNode(SelectedNodeId, Result))
	{
		BuildDetails(MVPSubsystem->GetTalentNodeViews());
		return false;
	}
	RebuildForTest();
	return true;
}

int32 UGameXXKTalentTreeWidget::GetVisibleNodeCountForTest() const
{
	return VisibleNodeIds.Num();
}

bool UGameXXKTalentTreeWidget::IsNodeVisibleForTest(const FName NodeId) const
{
	return VisibleNodeIds.Contains(NodeId);
}

FString UGameXXKTalentTreeWidget::GetNodeFrameResourcePathForTest(const FName NodeId) const
{
	const TObjectPtr<UGameXXKTalentNodeButton>* Button = NodeButtons.Find(NodeId);
	const UObject* Resource = Button && *Button
		? (*Button)->GetStyle().Normal.GetResourceObject()
		: nullptr;
	return Resource ? Resource->GetPathName() : FString();
}

FString UGameXXKTalentTreeWidget::GetNodeIconResourcePathForTest(const FName NodeId) const
{
	const TObjectPtr<UImage>* Image = NodeIconImages.Find(NodeId);
	const UObject* Resource = Image && *Image ? (*Image)->GetBrush().GetResourceObject() : nullptr;
	return Resource ? Resource->GetPathName() : FString();
}

FVector2D UGameXXKTalentTreeWidget::GetNodeIconDesiredSizeForTest(const FName NodeId) const
{
	const TObjectPtr<UImage>* Image = NodeIconImages.Find(NodeId);
	return Image && *Image ? (*Image)->GetBrush().ImageSize : FVector2D::ZeroVector;
}

FText UGameXXKTalentTreeWidget::GetNodeNameForTest(const FName NodeId) const
{
	const TObjectPtr<UTextBlock>* Text = NodeNameTexts.Find(NodeId);
	return Text && *Text ? (*Text)->GetText() : FText::GetEmpty();
}

FText UGameXXKTalentTreeWidget::GetNodeRankForTest(const FName NodeId) const
{
	const TObjectPtr<UTextBlock>* Text = NodeRankTexts.Find(NodeId);
	return Text && *Text ? (*Text)->GetText() : FText::GetEmpty();
}

bool UGameXXKTalentTreeWidget::IsNodeTextInsideButtonForTest(const FName NodeId) const
{
	const TObjectPtr<UGameXXKTalentNodeButton>* Button = NodeButtons.Find(NodeId);
	return Button && *Button && ContainsTextWidget((*Button)->GetContent());
}

FString UGameXXKTalentTreeWidget::GetPurchaseButtonResourcePathForTest() const
{
	const UObject* Resource = PurchaseButton
		? PurchaseButton->GetStyle().Normal.GetResourceObject()
		: nullptr;
	return Resource ? Resource->GetPathName() : FString();
}

FText UGameXXKTalentTreeWidget::GetPurchaseButtonLabelForTest() const
{
	const UTextBlock* Label = PurchaseButton ? Cast<UTextBlock>(PurchaseButton->GetContent()) : nullptr;
	return Label ? Label->GetText() : FText::GetEmpty();
}

FLinearColor UGameXXKTalentTreeWidget::GetNodeIconTintForTest(const FName NodeId) const
{
	const TObjectPtr<UImage>* Image = NodeIconImages.Find(NodeId);
	return Image && *Image ? (*Image)->GetColorAndOpacity() : FLinearColor::Transparent;
}

void UGameXXKTalentTreeWidget::HandleNodeClicked(const FName NodeId)
{
	if (SelectNodeForTest(NodeId))
	{
		for (const TPair<FName, TObjectPtr<UImage>>& Pair : NodeSelectedFrames)
		{
			if (Pair.Value)
			{
				Pair.Value->SetVisibility(
					Pair.Key == SelectedNodeId
						? ESlateVisibility::HitTestInvisible
						: ESlateVisibility::Collapsed);
			}
		}
	}
}

bool UGameXXKTalentTreeWidget::ToggleTotalsForTest()
{
	if(!TotalsButton)return false;
	TotalsButton->OnClicked.Broadcast();return bShowTotals;
}

void UGameXXKTalentTreeWidget::HandleTotalsClicked()
{
	bShowTotals=!bShowTotals;
	BuildDetails(MVPSubsystem?MVPSubsystem->GetTalentNodeViews():TArray<FGameXXKTalentNodeView>());
	if(bShowTotals && TotalsScroll)TotalsScroll->ScrollToStart();
}

void UGameXXKTalentTreeWidget::BuildTotals()
{
	if(!TotalsColumn || !MVPSubsystem)return;
	TotalsColumn->ClearChildren();
	TArray<FGameXXKTalentTotalGroup> Groups;
	const bool Valid=GameXXKTalentTotals::Build(MVPSubsystem->GetRuntimeState().Talents,Groups);
	if(!Valid || Groups.IsEmpty())
	{
		auto* Empty=MakeText(WidgetTree,FText::FromString(Valid?TEXT("尚未学习天赋"):TEXT("暂时无法读取合计")),17,Ink);
		TotalsColumn->AddChildToVerticalBox(Empty)->SetPadding(FMargin(0,12));return;
	}
	for(const auto& Group:Groups)
	{
		auto* Heading=MakeText(WidgetTree,Group.Title,19,FGameXXKInRunUiStyle::Jade());
		TotalsColumn->AddChildToVerticalBox(Heading)->SetPadding(FMargin(0,10,0,3));
		auto* Rule=WidgetTree->ConstructWidget<UBorder>();Rule->SetPadding(FMargin(0));Rule->SetBrushColor(FLinearColor(.17f,.14f,.1f,.3f));Rule->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* RuleSize=WidgetTree->ConstructWidget<USizeBox>();RuleSize->SetHeightOverride(1);RuleSize->SetContent(Rule);
		TotalsColumn->AddChildToVerticalBox(RuleSize)->SetPadding(FMargin(0,0,0,5));
		for(const auto& Entry:Group.Entries)
		{
			auto* Row=WidgetTree->ConstructWidget<UHorizontalBox>();
			auto* Name=MakeText(WidgetTree,Entry.Label,17,Ink);Name->SetAutoWrapText(false);
			auto* NameSlot=Row->AddChildToHorizontalBox(Name);NameSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));NameSlot->SetVerticalAlignment(VAlign_Center);
			auto* Value=MakeText(WidgetTree,Entry.Value,17,Ink);Value->SetAutoWrapText(false);Value->SetJustification(ETextJustify::Right);
			Row->AddChildToHorizontalBox(Value)->SetVerticalAlignment(VAlign_Center);
			TotalsColumn->AddChildToVerticalBox(Row)->SetPadding(FMargin(0,4));
		}
	}
}

void UGameXXKTalentTreeWidget::HandlePurchaseClicked()
{
	if (!MVPSubsystem || SelectedNodeId.IsNone())
	{
		return;
	}
	FGameXXKTalentPurchaseResult Result;
	if (!MVPSubsystem->PurchaseTalentNode(SelectedNodeId, Result))
	{
		LastPurchaseError = Result.Message;
		BuildDetails(MVPSubsystem->GetTalentNodeViews());
		return;
	}

	LastPurchaseError = FText::GetEmpty();
	BuildDetails(MVPSubsystem->GetTalentNodeViews());
	// Mutating the graph while the purchase button callback is unwinding is
	// unsafe. Rebuild only the existing graph panel on the next NativeTick.
	bSlateRebuildPending = true;
	InvalidateLayoutAndVolatility();
	PurchaseCommitted.Broadcast();
}

void UGameXXKTalentTreeWidget::NativeTick(
	const FGeometry& MyGeometry,
	const float InDeltaTime)
{
	Super::NativeTick(MyGeometry, InDeltaTime);
	if (VerticalInkScrollbar) VerticalInkScrollbar->RefreshFromTarget();
	if (HorizontalInkScrollbar) HorizontalInkScrollbar->RefreshFromTarget();
	if (!bSlateRebuildPending)
	{
		if (MVPSubsystem
			&& (LastPresentedGold != MVPSubsystem->GetRuntimeState().PlayerGold
				|| bLastPurchaseContextAllowed != IsPurchaseContextAllowed(MVPSubsystem->GetRuntimeState())))
		{
			// Income can arrive while this panel stays open. Update affordability
			// in place so an insufficient-gold button does not stay disabled.
			LastPurchaseError = FText::GetEmpty();
			const auto Views = MVPSubsystem->GetTalentNodeViews();
			BuildDetails(Views);
			for (const auto& View : Views)
			{
				if (UImage* Icon = NodeIconImages.FindRef(View.Definition.Id)) Icon->SetColorAndOpacity(StateIconTint(View));
				if (UGameXXKTalentNodeButton* Button = NodeButtons.FindRef(View.Definition.Id)) Button->SetToolTipText(NodeTooltip(View));
				const auto Color = View.State == EGameXXKTalentNodeState::Locked ? GraphMutedText : GraphText;
				if (UTextBlock* Name = NodeNameTexts.FindRef(View.Definition.Id)) Name->SetColorAndOpacity(FSlateColor(Color));
				if (UTextBlock* Rank = NodeRankTexts.FindRef(View.Definition.Id)) Rank->SetColorAndOpacity(FSlateColor(Color));
			}
		}
		return;
	}
	bSlateRebuildPending = false;
	RebuildGraphAndDetails();
}

void UGameXXKTalentTreeWidget::BuildProgrammaticLayout()
{
	if(RootCanvas){RebuildGraphAndDetails();return;}
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("TalentWidgetTree"));
	}
	WidgetTree->RootWidget = nullptr;
	NodeIconImages.Reset();
	NodeButtons.Reset();
	NodeNameTexts.Reset();
	NodeRankTexts.Reset();
	NodeSelectedFrames.Reset();
	RenderedConnectionAngles.Reset();
	RenderedConnectionBoundaryOffsets.Reset();
	VisibleNodeIds.Reset();

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("TalentTreeRoot"));
	WidgetTree->RootWidget = RootCanvas;

	GraphFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TalentGraphFrame"));
	// Preserve a clear node-workspace boundary against the paper detail area.
	const FSlateRoundedBoxBrush GraphBackground(
		FLinearColor(0.045f, 0.045f, 0.045f, 1.0f), 2.0f,
		FLinearColor(0.11f, 0.11f, 0.11f, 1.0f), 1.0f);
	GraphFrame->SetBrush(GraphBackground);
	GraphFrame->SetBrushColor(FLinearColor::White);
	GraphFrame->SetPadding(FMargin(3.0f,3.0f,21.0f,21.0f));
	GraphFrame->SetClipping(EWidgetClipping::ClipToBounds);
	AddCanvas(RootCanvas, GraphFrame, FVector2D::ZeroVector, FVector2D(GraphViewportWidth, WidgetHeight));

	HorizontalScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("TalentHorizontalScroll"));
	HorizontalScroll->SetOrientation(Orient_Horizontal);
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(HorizontalScroll,14.0f,60.0f);
	HorizontalScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);

	HorizontalScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	HorizontalScroll->OnUserScrolled.AddDynamic(
		this,
		&UGameXXKTalentTreeWidget::HandleHorizontalGraphScrolled);
	GraphFrame->SetContent(HorizontalScroll);

	USizeBox* HorizontalExtent = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("TalentHorizontalExtent"));
	HorizontalExtent->SetWidthOverride(GraphCanvasExtent);
	HorizontalExtent->SetHeightOverride(WidgetHeight - 24.0f);
	HorizontalScroll->AddChild(HorizontalExtent);

	VerticalScroll = WidgetTree->ConstructWidget<UScrollBox>(
		UScrollBox::StaticClass(),
		TEXT("TalentVerticalScroll"));
	VerticalScroll->SetOrientation(Orient_Vertical);
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(VerticalScroll,14.0f,60.0f);
	VerticalScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);

	VerticalScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	VerticalScroll->OnUserScrolled.AddDynamic(
		this,
		&UGameXXKTalentTreeWidget::HandleVerticalGraphScrolled);
	HorizontalExtent->SetContent(VerticalScroll);

	USizeBox* VerticalExtent = WidgetTree->ConstructWidget<USizeBox>(
		USizeBox::StaticClass(),
		TEXT("TalentVerticalExtent"));
	VerticalExtent->SetWidthOverride(GraphCanvasExtent);
	VerticalExtent->SetHeightOverride(GraphCanvasExtent);
	VerticalScroll->AddChild(VerticalExtent);
	VerticalInkScrollbar=WidgetTree->ConstructWidget<UGameXXKInkScrollBar>(UGameXXKInkScrollBar::StaticClass(),TEXT("TalentVerticalInkScrollbar"));
	VerticalInkScrollbar->Configure(VerticalScroll,80.0f);
	AddCanvas(RootCanvas,VerticalInkScrollbar,FVector2D(GraphViewportWidth-18.0f,3.0f),FVector2D(14.0f,WidgetHeight-24.0f));
	HorizontalInkScrollbar=WidgetTree->ConstructWidget<UGameXXKInkScrollBar>(UGameXXKInkScrollBar::StaticClass(),TEXT("TalentHorizontalInkScrollbar"));
	HorizontalInkScrollbar->Configure(HorizontalScroll,80.0f);
	AddCanvas(RootCanvas,HorizontalInkScrollbar,FVector2D(3.0f,WidgetHeight-18.0f),FVector2D(GraphViewportWidth-24.0f,14.0f));

	GraphCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("TalentTreeGraphCanvas"));
	VerticalExtent->SetContent(GraphCanvas);

	UBorder* DetailFrame = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("TalentDetailPanel"));
	FSlateBrush TransparentDetailBrush;
	TransparentDetailBrush.DrawAs = ESlateBrushDrawType::NoDrawType;
	DetailFrame->SetBrush(TransparentDetailBrush);
	DetailFrame->SetBrushColor(FLinearColor::Transparent);
	DetailFrame->SetPadding(FMargin(12.0f));
	AddCanvas(
		RootCanvas,
		DetailFrame,
		FVector2D(GraphViewportWidth + 8.0f, 0.0f),
		FVector2D(WidgetWidth - GraphViewportWidth - 8.0f, WidgetHeight));

	UVerticalBox* DetailColumn = WidgetTree->ConstructWidget<UVerticalBox>(
		UVerticalBox::StaticClass(),
		TEXT("TalentDetailColumn"));
	DetailFrame->SetContent(DetailColumn);
	DetailNameText = MakeText(WidgetTree, FText::GetEmpty(), 24, Ink, TEXT("TalentDetailName"));
	DetailColumn->AddChildToVerticalBox(DetailNameText)->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 8.0f));
	DetailBodyText = MakeText(WidgetTree, FText::GetEmpty(), 18, Ink, TEXT("TalentDetailBody"));
	UVerticalBoxSlot* BodySlot = DetailColumn->AddChildToVerticalBox(DetailBodyText);
	BodySlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, 10.0f));
	BodySlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	TotalsScroll=WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(),TEXT("TalentTotalsScroll"));
	TotalsScroll->SetScrollBarVisibility(ESlateVisibility::Collapsed);
	TotalsScroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	FSlateBrush None;None.DrawAs=ESlateBrushDrawType::NoDrawType;FScrollBoxStyle Clean;Clean.SetTopShadowBrush(None).SetBottomShadowBrush(None);TotalsScroll->SetWidgetStyle(Clean);
	TotalsColumn=WidgetTree->ConstructWidget<UVerticalBox>();TotalsScroll->AddChild(TotalsColumn);
	TotalsScroll->SetVisibility(ESlateVisibility::Collapsed);
	DetailColumn->AddChildToVerticalBox(TotalsScroll)->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
	UpgradePriceText = MakeText(
		WidgetTree,
		FText::GetEmpty(),
		16,
		Ink,
		TEXT("TalentUpgradePriceText"));
	DetailColumn->AddChildToVerticalBox(UpgradePriceText)->SetPadding(
		FMargin(0.0f, 0.0f, 0.0f, 6.0f));
	PurchaseStatusText = MakeText(WidgetTree, FText::GetEmpty(), 16, Ink, TEXT("TalentPurchaseStatus"));
	auto* StatusBox = WidgetTree->ConstructWidget<USizeBox>();
	StatusBox->SetHeightOverride(42.0f);
	StatusBox->SetContent(PurchaseStatusText);
	DetailColumn->AddChildToVerticalBox(StatusBox);
	PurchaseButton = WidgetTree->ConstructWidget<UButton>(
		UButton::StaticClass(),
		TEXT("TalentPurchaseButton"));
	PurchaseButton->SetStyle(MakeTextureButtonStyle(
		TalentUpgradeButtonPath,
		FVector2D(200.0f, 64.0f)));
	PurchaseButton->SetBackgroundColor(FLinearColor::White);
	PurchaseButton->OnClicked.AddDynamic(this, &UGameXXKTalentTreeWidget::HandlePurchaseClicked);
	UTextBlock* PurchaseLabel = MakeText(WidgetTree, FText::FromString(TEXT("升级")),
		24, Ink, TEXT("TalentPurchaseLabel"));
	PurchaseLabel->SetAutoWrapText(false);
	PurchaseLabel->SetJustification(ETextJustify::Center);
	PurchaseButton->SetContent(PurchaseLabel);
	if (auto* LabelSlot = Cast<UButtonSlot>(PurchaseButton->GetContentSlot()))
	{
		LabelSlot->SetHorizontalAlignment(HAlign_Center);
		LabelSlot->SetVerticalAlignment(VAlign_Center);
		LabelSlot->SetPadding(FMargin(10.0f, 4.0f));
	}
	PurchaseButtonContainer = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("TalentPurchaseButtonSize"));
	PurchaseButtonContainer->SetHeightOverride(64.0f);
	PurchaseButtonContainer->SetContent(PurchaseButton);
	DetailColumn->AddChildToVerticalBox(PurchaseButtonContainer)->SetPadding(FMargin(0.0f, 6.0f, 0.0f, 0.0f));
	TotalsButton=WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("TalentTotalsButton"));
	TotalsButton->SetStyle(FGameXXKInRunUiStyle::Action(FVector2D(190,36),true));
	auto* TotalsLabel=MakeText(WidgetTree,FText::FromString(TEXT("合计属性")),18,FLinearColor(.96f,.92f,.82f,1));
	TotalsLabel->SetAutoWrapText(false);TotalsLabel->SetJustification(ETextJustify::Center);TotalsButton->SetContent(TotalsLabel);
	TotalsButton->SetToolTipText(FText::FromString(TEXT("合并统计已学天赋的有效加成")));
	TotalsButton->OnClicked.AddDynamic(this,&UGameXXKTalentTreeWidget::HandleTotalsClicked);
	DetailColumn->AddChildToVerticalBox(TotalsButton)->SetPadding(FMargin(0,10,0,0));

	const TArray<FGameXXKTalentNodeView> Views = MVPSubsystem
		? MVPSubsystem->GetTalentNodeViews()
		: TArray<FGameXXKTalentNodeView>();
	const bool bSelectionVisible = Views.ContainsByPredicate([this](const FGameXXKTalentNodeView& View)
	{
		return View.Definition.Id == SelectedNodeId
			&& View.State != EGameXXKTalentNodeState::Hidden;
	});
	if (!bSelectionVisible)
	{
		const bool bRootVisible = Views.ContainsByPredicate([](const FGameXXKTalentNodeView& View)
		{
			return View.Definition.Id == TEXT("Talent.Root")
				&& View.State != EGameXXKTalentNodeState::Hidden;
		});
		SelectedNodeId = bRootVisible
			? FName(TEXT("Talent.Root"))
			: NAME_None;
	}
	BuildGraph(GraphCanvas, Views);
	BuildDetails(Views);

	RequestedGraphScrollOffset = FVector2D(
		GraphCenter - GraphViewportWidth * 0.5f,
		GraphCenter - WidgetHeight * 0.5f);
	HorizontalScroll->SetScrollOffset(RequestedGraphScrollOffset.X);
	VerticalScroll->SetScrollOffset(RequestedGraphScrollOffset.Y);
}

void UGameXXKTalentTreeWidget::ApplyGraphPanDelta(const FVector2D& DragDelta)
{
	if (!HorizontalScroll || !VerticalScroll)
	{
		return;
	}
	const FVector2D CurrentOffset = RequestedGraphScrollOffset;
	const float HorizontalEnd = HorizontalScroll->GetScrollOffsetOfEnd();
	const float VerticalEnd = VerticalScroll->GetScrollOffsetOfEnd();
	const FVector2D MaximumOffset(
		HorizontalEnd > 0.0f ? HorizontalEnd : GraphCanvasExtent - GraphViewportWidth,
		VerticalEnd > 0.0f ? VerticalEnd : GraphCanvasExtent - WidgetHeight);
	const FVector2D Resolved = ResolveGraphPanOffset(
		CurrentOffset,
		DragDelta,
		MaximumOffset);
	RequestedGraphScrollOffset = Resolved;
	HorizontalScroll->SetScrollOffset(Resolved.X);
	VerticalScroll->SetScrollOffset(Resolved.Y);
}

void UGameXXKTalentTreeWidget::HandleHorizontalGraphScrolled(const float CurrentOffset)
{
	RequestedGraphScrollOffset.X = FMath::Max(0.0f, CurrentOffset);
}

void UGameXXKTalentTreeWidget::HandleVerticalGraphScrolled(const float CurrentOffset)
{
	RequestedGraphScrollOffset.Y = FMath::Max(0.0f, CurrentOffset);
}

FReply UGameXXKTalentTreeWidget::NativeOnMouseButtonDown(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton
		&& GraphFrame
		&& GraphFrame->GetCachedGeometry().IsUnderLocation(InMouseEvent.GetScreenSpacePosition()))
	{
		bGraphPanning = true;
		LastGraphPanScreenPosition = InMouseEvent.GetScreenSpacePosition();
		return FReply::Handled().CaptureMouse(GetCachedWidget().ToSharedRef());
	}
	return Super::NativeOnMouseButtonDown(InGeometry, InMouseEvent);
}

FReply UGameXXKTalentTreeWidget::NativeOnMouseButtonUp(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bGraphPanning && InMouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		bGraphPanning = false;
		return FReply::Handled().ReleaseMouseCapture();
	}
	return Super::NativeOnMouseButtonUp(InGeometry, InMouseEvent);
}

FReply UGameXXKTalentTreeWidget::NativeOnMouseMove(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent)
{
	if (bGraphPanning)
	{
		const FVector2D CurrentScreenPosition = InMouseEvent.GetScreenSpacePosition();
		const float GeometryScale = FMath::Max(
			0.01f,
			static_cast<float>(InGeometry.GetAccumulatedLayoutTransform().GetScale()));
		ApplyGraphPanDelta((CurrentScreenPosition - LastGraphPanScreenPosition) / GeometryScale);
		LastGraphPanScreenPosition = CurrentScreenPosition;
		return FReply::Handled();
	}
	return Super::NativeOnMouseMove(InGeometry, InMouseEvent);
}

void UGameXXKTalentTreeWidget::NativeOnMouseCaptureLost(
	const FCaptureLostEvent& CaptureLostEvent)
{
	bGraphPanning = false;
	Super::NativeOnMouseCaptureLost(CaptureLostEvent);
}

void UGameXXKTalentTreeWidget::RebuildGraphAndDetails()
{
	if (!GraphCanvas || !MVPSubsystem)
	{
		return;
	}
	const float HorizontalOffset = RequestedGraphScrollOffset.X;
	const float VerticalOffset = RequestedGraphScrollOffset.Y;
	GraphCanvas->ClearChildren();
	NodeIconImages.Reset();
	NodeButtons.Reset();
	NodeNameTexts.Reset();
	NodeRankTexts.Reset();
	NodeSelectedFrames.Reset();
	RenderedConnectionAngles.Reset();
	RenderedConnectionBoundaryOffsets.Reset();
	VisibleNodeIds.Reset();
	const TArray<FGameXXKTalentNodeView> Views = MVPSubsystem->GetTalentNodeViews();
	BuildGraph(GraphCanvas, Views);
	BuildDetails(Views);
	if (HorizontalScroll)
	{
		HorizontalScroll->SetScrollOffset(HorizontalOffset);
	}
	if (VerticalScroll)
	{
		VerticalScroll->SetScrollOffset(VerticalOffset);
	}
}

void UGameXXKTalentTreeWidget::BuildGraph(
	UCanvasPanel* InGraphCanvas,
	const TArray<FGameXXKTalentNodeView>& Views)
{
	TMap<FName, const FGameXXKTalentNodeView*> VisibleViews;
	for (const FGameXXKTalentNodeView& View : Views)
	{
		if (View.State != EGameXXKTalentNodeState::Hidden)
		{
			VisibleViews.Add(View.Definition.Id, &View);
			VisibleNodeIds.Add(View.Definition.Id);
		}
	}

	// Connections are inserted first so every Item Slot renders above them.
	for (const FGameXXKTalentNodeView& View : Views)
	{
		if (!VisibleViews.Contains(View.Definition.Id))
		{
			continue;
		}
		const TArray<FName>& ConnectionIds = View.Definition.VisualConnectionIds.IsEmpty()
			? View.Definition.PrerequisiteIds
			: View.Definition.VisualConnectionIds;
		for (const FName ConnectionId : ConnectionIds)
		{
			const FGameXXKTalentNodeView* const* ConnectionSource = VisibleViews.Find(ConnectionId);
			if (!ConnectionSource || !*ConnectionSource)
			{
				continue;
			}
			const FVector2D SourceCenter = SlotCenterForGraphPosition(
				(*ConnectionSource)->Definition.GraphPosition);
			const FVector2D TargetCenter = SlotCenterForGraphPosition(
				View.Definition.GraphPosition);
			const FVector2D CenterDelta = TargetCenter - SourceCenter;
			float Angle = 0.0f;
			if (!ensureMsgf(
				ResolveConnectionAngle(CenterDelta, Angle),
				TEXT("Unsupported talent connection angle: %s -> %s"),
				*ConnectionId.ToString(),
				*View.Definition.Id.ToString()))
			{
				continue;
			}
			FVector2D BoundaryOffset = FVector2D::ZeroVector;
			if (!ensure(ResolveSlotBoundaryOffset(CenterDelta, BoundaryOffset)))
			{
				continue;
			}
			const FVector2D Start = SourceCenter + BoundaryOffset;
			const FVector2D End = TargetCenter - BoundaryOffset;
			const FVector2D Delta = End - Start;
			RenderedConnectionAngles.Add(Angle);
			RenderedConnectionBoundaryOffsets.Add(BoundaryOffset);
			RenderedConnectionBoundaryOffsets.Add(-BoundaryOffset);
			UBorder* Line = WidgetTree->ConstructWidget<UBorder>(UBorder::StaticClass());
			Line->SetBrushColor(
				View.State == EGameXXKTalentNodeState::Locked
					? FLinearColor(0.30f, 0.29f, 0.25f, 0.72f)
					: FLinearColor(0.12f, 0.30f, 0.22f, 1.0f));
			Line->SetRenderTransformPivot(FVector2D(0.0f, 0.5f));
			Line->SetRenderTransformAngle(Angle);
			AddCanvas(InGraphCanvas, Line, Start-FVector2D(0,2.25f), FVector2D(Delta.Size(), 4.5f));
		}
	}

	for (const FGameXXKTalentNodeView& View : Views)
	{
		if (!VisibleViews.Contains(View.Definition.Id))
		{
			continue;
		}
		const FVector2D Center = FVector2D(GraphCenter, GraphCenter)
			+ View.Definition.GraphPosition * GraphScale;
		const FVector2D FootprintTopLeft = Center
			- FVector2D(NodeFootprintWidth * 0.5f, NodeFootprintHeight * 0.5f);
		UGameXXKTalentNodeButton* Button =
			WidgetTree->ConstructWidget<UGameXXKTalentNodeButton>(
				UGameXXKTalentNodeButton::StaticClass(),
				MakeUniqueObjectName(WidgetTree,UGameXXKTalentNodeButton::StaticClass(),FName(*FString::Printf(TEXT("TalentNode_%s"), *View.Definition.Id.ToString().Replace(TEXT("."), TEXT("_"))))));
		Button->Configure(this, View.Definition.Id);
		Button->SetStyle(MakeTextureButtonStyle(
			View.Definition.Id == SelectedNodeId
				? TalentSquareSelectedPath
				: TalentItemSlotPath,
			FVector2D(NodeSlotSize, NodeSlotSize)));
		Button->SetBackgroundColor(FLinearColor::White);

		UImage* Icon = WidgetTree->ConstructWidget<UImage>(
			UImage::StaticClass(),
			MakeUniqueObjectName(WidgetTree,UImage::StaticClass(),FName(*FString::Printf(TEXT("TalentIcon_%s"), *View.Definition.Id.ToString().Replace(TEXT("."), TEXT("_"))))));
		FSlateBrush IconBrush;
		IconBrush.SetResourceObject(LoadTexture(TalentIconPath(View.Definition.Icon)));
		IconBrush.DrawAs = ESlateBrushDrawType::Image;
		IconBrush.ImageSize = FVector2D(NodeIconSize, NodeIconSize);
		Icon->SetBrush(IconBrush);
		Icon->SetColorAndOpacity(StateIconTint(View));
		UScaleBox* IconScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		IconScale->SetStretch(EStretch::ScaleToFit);
		IconScale->SetStretchDirection(EStretchDirection::Both);
		IconScale->SetContent(Icon);
		USizeBox* IconSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass());
		IconSizeBox->SetWidthOverride(NodeIconSize);
		IconSizeBox->SetHeightOverride(NodeIconSize);
		IconSizeBox->SetContent(IconScale);
		Button->SetContent(IconSizeBox);
		if (UButtonSlot* ContentSlot = Cast<UButtonSlot>(Button->GetContentSlot()))
		{
			ContentSlot->SetHorizontalAlignment(HAlign_Center);
			ContentSlot->SetVerticalAlignment(VAlign_Center);
			ContentSlot->SetPadding(FMargin(0.0f));
		}
		Button->SetToolTipText(NodeTooltip(View));
		AddCanvas(
			InGraphCanvas,
			Button,
			FootprintTopLeft + FVector2D(20.0f, 0.0f),
			FVector2D(NodeSlotSize, NodeSlotSize));
		NodeButtons.Add(View.Definition.Id, Button);
		NodeIconImages.Add(View.Definition.Id, Icon);

		UTextBlock* NameText = MakeText(
			WidgetTree,
			View.Definition.DisplayName,
			16,
			View.State == EGameXXKTalentNodeState::Locked
				? GraphMutedText
				: GraphText);
		NameText->SetJustification(ETextJustify::Center);
		NameText->SetAutoWrapText(false);
		UScaleBox* NameScale = WidgetTree->ConstructWidget<UScaleBox>(UScaleBox::StaticClass());
		NameScale->SetStretch(EStretch::ScaleToFitX);
		NameScale->SetStretchDirection(EStretchDirection::DownOnly);
		NameScale->SetContent(NameText);
		AddCanvas(
			InGraphCanvas,
			NameScale,
			FootprintTopLeft + FVector2D(0.0f, 94.0f),
			FVector2D(NodeFootprintWidth, 18.0f));
		NodeNameTexts.Add(View.Definition.Id, NameText);

		UTextBlock* Rank = MakeText(
			WidgetTree,
			FText::FromString(FString::Printf(TEXT("%d/%d"), View.Rank, View.Definition.MaxRank)),
			14,
			View.State == EGameXXKTalentNodeState::Locked
				? GraphMutedText
				: GraphText);
		Rank->SetJustification(ETextJustify::Center);
		Rank->SetAutoWrapText(false);
		AddCanvas(
			InGraphCanvas,
			Rank,
			FootprintTopLeft + FVector2D(0.0f, 114.0f),
			FVector2D(NodeFootprintWidth, 18.0f));
		NodeRankTexts.Add(View.Definition.Id, Rank);
	}
}

const FGameXXKTalentNodeView* UGameXXKTalentTreeWidget::FindSelectedView(
	const TArray<FGameXXKTalentNodeView>& Views) const
{
	return Views.FindByPredicate([this](const FGameXXKTalentNodeView& View)
	{
		return View.Definition.Id == SelectedNodeId;
	});
}

void UGameXXKTalentTreeWidget::BuildDetails(
	const TArray<FGameXXKTalentNodeView>& Views)
{
	if (!DetailNameText || !DetailBodyText || !UpgradePriceText || !PurchaseButton)
	{
		return;
	}
	LastPresentedGold = MVPSubsystem ? MVPSubsystem->GetRuntimeState().PlayerGold : INDEX_NONE;
	bLastPurchaseContextAllowed = MVPSubsystem && IsPurchaseContextAllowed(MVPSubsystem->GetRuntimeState());
	if(TotalsScroll)TotalsScroll->SetVisibility(bShowTotals?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
	DetailBodyText->SetVisibility(bShowTotals?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
	UpgradePriceText->SetVisibility(bShowTotals?ESlateVisibility::Collapsed:ESlateVisibility::HitTestInvisible);
	PurchaseButton->SetVisibility(bShowTotals?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
	if (PurchaseButtonContainer) PurchaseButtonContainer->SetVisibility(bShowTotals ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	if (PurchaseStatusText)
	{
		PurchaseStatusText->SetText(FText::GetEmpty());
		PurchaseStatusText->GetParent()->SetVisibility(bShowTotals ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);
	}
	if(TotalsButton)if(auto* Label=Cast<UTextBlock>(TotalsButton->GetContent()))Label->SetText(FText::FromString(bShowTotals?TEXT("返回天赋"):TEXT("合计属性")));
	if(bShowTotals){DetailNameText->SetText(FText::FromString(TEXT("合计属性")));BuildTotals();return;}
	const FGameXXKTalentNodeView* View = FindSelectedView(Views);
	if (!View)
	{
		DetailNameText->SetText(FText::FromString(TEXT("选择一个天赋")));
		DetailBodyText->SetText(FText::GetEmpty());
		UpgradePriceText->SetText(FText::GetEmpty());
		PurchaseButton->SetIsEnabled(false);
		if (UTextBlock* Label = Cast<UTextBlock>(PurchaseButton->GetContent()))
		{
			Label->SetText(FText::FromString(TEXT("升级")));
		}
		return;
	}
	DetailNameText->SetText(View->Definition.DisplayName);
	DetailBodyText->SetText(FGameXXKTalentRules::DescribeEffect(View->Definition));
	UpgradePriceText->SetText(FText::FromString(
		View->Rank >= View->Definition.MaxRank
			? TEXT("升级售价：已满级")
			: FString::Printf(TEXT("升级售价：%lld"), View->NextPrice)));
	PurchaseButton->SetIsEnabled(bLastPurchaseContextAllowed && (
		View->State == EGameXXKTalentNodeState::Available
		|| View->State == EGameXXKTalentNodeState::Purchased));
	if (PurchaseStatusText)
	{
		const FText Status = !LastPurchaseError.IsEmpty() ? LastPurchaseError
			: !bLastPurchaseContextAllowed ? FText::FromString(TEXT("返回桌面后可升级"))
			: !View->LockReason.IsEmpty() ? View->LockReason
			: FText::FromString(FString::Printf(TEXT("当前等级：%d/%d"), View->Rank, View->Definition.MaxRank));
		PurchaseStatusText->SetText(Status);
	}
	if (UTextBlock* Label = Cast<UTextBlock>(PurchaseButton->GetContent()))
	{
		Label->SetText(FText::FromString(TEXT("升级")));
	}
}
