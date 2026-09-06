#include "UI/GameXXKOneGameRouteMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/ScrollBoxSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "Input/Reply.h"
#include "InputCoreTypes.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Styling/SlateBrush.h"
#include "Styling/SlateTypes.h"
#include "UI/GameXXKMVPCommandRouter.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

namespace
{
	static constexpr int32 MaxRouteNodeButtons = 24;
	static const TCHAR* OneGameRouteWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择.UI_地图选择_C");
	static const TCHAR* OneGameNodeWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡_C");
	static const TCHAR* OneGameLineWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-关卡-线.UI_地图选择-关卡-线_C");
	static const TCHAR* OneGameBossWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-Boss.UI_地图选择-Boss_C");
	static const TCHAR* OneGameRouteLineTexturePath = TEXT("/Game/1Game/Texture/脚印.脚印");
	static const TCHAR* OneGameBattleTexturePath = TEXT("/Game/1Game/Texture/小怪.小怪");
	static const TCHAR* OneGameBattleDisabledTexturePath = TEXT("/Game/1Game/Texture/小怪灰色.小怪灰色");
	static const TCHAR* OneGameEliteTexturePath = TEXT("/Game/1Game/Texture/精英怪.精英怪");
	static const TCHAR* OneGameEliteDisabledTexturePath = TEXT("/Game/1Game/Texture/精英怪灰色.精英怪灰色");
	static const TCHAR* OneGameCampTexturePath = TEXT("/Game/1Game/Texture/篝火.篝火");
	static const TCHAR* OneGameCampDisabledTexturePath = TEXT("/Game/1Game/Texture/篝火灰色.篝火灰色");
	static const TCHAR* OneGameChestTexturePath = TEXT("/Game/1Game/Texture/宝箱.宝箱");
	static const TCHAR* OneGameChestDisabledTexturePath = TEXT("/Game/1Game/Texture/宝箱灰色.宝箱灰色");
	static const TCHAR* OneGameMerchantTexturePath = TEXT("/Game/1Game/Texture/钱.钱");
	static const TCHAR* OneGameMerchantDisabledTexturePath = TEXT("/Game/1Game/Texture/钱灰色.钱灰色");
	static const TCHAR* OneGameEventTexturePath = TEXT("/Game/1Game/Texture/问号.问号");
	static const TCHAR* OneGameEventDisabledTexturePath = TEXT("/Game/1Game/Texture/问号灰色.问号灰色");
	static const TCHAR* OneGameRouteBackgroundTexturePath = TEXT("/Game/1Game/Texture/图层_1.图层_1");
	static const TCHAR* RouteActionButtonTexturePath = TEXT("/Game/GameXXK/UI/MainMenu/Textures/T_InkButtonBase.T_InkButtonBase");
	static const TCHAR* RouteCloseInkTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk");
	static const TCHAR* RouteModalPaperTexturePath = TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot.T_MasterV2_ItemSlot");
	const FVector2D RouteViewportDesignSize(1920.0f, 1080.0f);
	const FVector2D RouteCloseChallengePosition(1774.0f, 48.0f);
	const FVector2D RouteCloseChallengeSize(74.0f, 74.0f);
	const FVector2D RouteActionButtonBrushSize(192.0f, 64.0f);
	const FVector2D RouteAbandonModalSize(640.0f, 360.0f);
	const FVector2D MinimumRouteContentSize(640.0f, 1040.0f);
	const float RouteHorizontalPadding = 96.0f;
	const float RouteTopPadding = 96.0f;
	const float RouteBottomPadding = 128.0f;
	const float RouteLayerGap = 180.0f;
	const float DefaultRouteViewportHeight = 520.0f;
	const float RouteViewportHeightContentMultiplier = 1.8f;
	const float RouteCenteredLaneWidthFraction = 0.70f;
	const float RouteCenteredLaneMinWidth = 480.0f;
	const float RouteCenteredLaneMaxWidth = 960.0f;
	const float RouteLineThickness = 24.0f;
	const float RouteClickDragThresholdSq = 64.0f;

	FSlateBrush BuildRouteTextureBrush(
		UTexture2D* Texture,
		const FVector2D& ImageSize,
		const FLinearColor& Tint,
		const ESlateBrushDrawType::Type DrawAs = ESlateBrushDrawType::Image,
		const FMargin& Margin = FMargin(0.0f))
	{
		FSlateBrush Brush;
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = ImageSize;
		Brush.DrawAs = DrawAs;
		Brush.Margin = Margin;
		Brush.TintColor = FSlateColor(Tint);
		return Brush;
	}

	void StyleRouteActionButton(UButton* Button, UTexture2D* Texture)
	{
		if (!Button || !Texture)
		{
			return;
		}
		const FLinearColor NormalTint(0.18f, 0.29f, 0.24f, 0.94f);
		FButtonStyle Style;
		Style.SetNormal(BuildRouteTextureBrush(Texture, RouteActionButtonBrushSize, NormalTint));
		Style.SetHovered(BuildRouteTextureBrush(Texture, RouteActionButtonBrushSize, FLinearColor(0.24f, 0.38f, 0.31f, 1.0f)));
		Style.SetPressed(BuildRouteTextureBrush(Texture, RouteActionButtonBrushSize, FLinearColor(0.13f, 0.22f, 0.18f, 1.0f)));
		Style.SetDisabled(BuildRouteTextureBrush(Texture, RouteActionButtonBrushSize, FLinearColor(0.34f, 0.36f, 0.34f, 0.52f)));
		Style.NormalPadding = FMargin(18.0f, 10.0f);
		Style.PressedPadding = FMargin(18.0f, 12.0f, 18.0f, 8.0f);
		Button->SetStyle(Style);
	}

	void StyleRouteCloseInkButton(UButton* Button, UTexture2D* Texture)
	{
		if (!Button || !Texture)
		{
			return;
		}
		FButtonStyle Style;
		Style.SetNormal(BuildRouteTextureBrush(Texture, RouteCloseChallengeSize, FLinearColor::White));
		Style.SetHovered(BuildRouteTextureBrush(Texture, RouteCloseChallengeSize, FLinearColor(1.0f, 0.91f, 0.76f, 1.0f)));
		Style.SetPressed(BuildRouteTextureBrush(Texture, RouteCloseChallengeSize, FLinearColor(0.72f, 0.62f, 0.52f, 1.0f)));
		Style.SetDisabled(BuildRouteTextureBrush(Texture, RouteCloseChallengeSize, FLinearColor(0.42f, 0.42f, 0.42f, 0.48f)));
		Style.NormalPadding = FMargin(0.0f);
		Style.PressedPadding = FMargin(1.0f, 2.0f, 0.0f, 0.0f);
		Button->SetStyle(Style);
	}

	void ConfigureRouteCloseChallengeButton(
		UButton* Button,
		UTexture2D* CloseInkTexture,
		UTexture2D* FallbackTexture,
		UWidgetTree* WidgetTree)
	{
		if (!Button)
		{
			return;
		}
		Button->ClearChildren();
		if (CloseInkTexture)
		{
			StyleRouteCloseInkButton(Button, CloseInkTexture);
			return;
		}

		if (FallbackTexture)
		{
			StyleRouteActionButton(Button, FallbackTexture);
		}
		UTextBlock* FallbackLabel = WidgetTree
			? WidgetTree->ConstructWidget<UTextBlock>(
				UTextBlock::StaticClass(),
				TEXT("RouteCloseChallengeFallbackLabel"))
			: NewObject<UTextBlock>(Button);
		if (!FallbackLabel)
		{
			return;
		}
		FallbackLabel->SetText(FText::FromString(TEXT("X")));
		FallbackLabel->SetJustification(ETextJustify::Center);
		FallbackLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		FSlateFontInfo Font = FallbackLabel->GetFont();
		Font.Size = 34;
		Font.TypefaceFontName = TEXT("Bold");
		FallbackLabel->SetFont(Font);
		Button->SetContent(FallbackLabel);
	}

	uint32 CalculateRouteTopologyHash(const FGameXXKRuntimeState& State)
	{
		uint32 Hash = GetTypeHash(State.RouteMapNodes.Num());
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			Hash = HashCombine(Hash, GetTypeHash(Node.NodeId));
			Hash = HashCombine(Hash, GetTypeHash(Node.LayerIndex));
			Hash = HashCombine(Hash, GetTypeHash(Node.ColumnIndex));
			Hash = HashCombine(Hash, GetTypeHash(static_cast<uint8>(Node.NodeKind)));
			Hash = HashCombine(Hash, GetTypeHash(Node.NormalizedPosition.X));
			Hash = HashCombine(Hash, GetTypeHash(Node.NormalizedPosition.Y));
			Hash = HashCombine(Hash, GetTypeHash(Node.OutgoingNodeIds.Num()));
			for (const int32 OutgoingNodeId : Node.OutgoingNodeIds)
			{
				Hash = HashCombine(Hash, GetTypeHash(OutgoingNodeId));
			}
		}
		return Hash;
	}

	FText RoomTypeLabel(EGameXXKOneGameRouteRoomType RoomType)
	{
		switch (RoomType)
		{
		case EGameXXKOneGameRouteRoomType::Start:
			return FText::FromString(TEXT("起程"));
		case EGameXXKOneGameRouteRoomType::SmallEnemy:
			return FText::FromString(TEXT("战斗"));
		case EGameXXKOneGameRouteRoomType::EliteEnemy:
			return FText::FromString(TEXT("精英"));
		case EGameXXKOneGameRouteRoomType::Camp:
			return FText::FromString(TEXT("篝火"));
		case EGameXXKOneGameRouteRoomType::Chest:
			return FText::FromString(TEXT("宝匣"));
		case EGameXXKOneGameRouteRoomType::Merchant:
			return FText::FromString(TEXT("行商"));
		case EGameXXKOneGameRouteRoomType::RandomEvent:
			return FText::FromString(TEXT("奇遇"));
		case EGameXXKOneGameRouteRoomType::Boss:
			return FText::FromString(TEXT("Boss"));
		default:
			return FText::FromString(TEXT("节点"));
		}
	}

	FLinearColor RouteNodeColor(const FGameXXKOneGameRouteNode* Node)
	{
		if (!Node)
		{
			return FLinearColor(0.06f, 0.07f, 0.08f, 0.0f);
		}
		if (Node->bVisited)
		{
			return FLinearColor(0.11f, 0.18f, 0.20f, 0.82f);
		}
		if (Node->bEnabled)
		{
			return FLinearColor(0.15f, 0.48f, 0.46f, 0.96f);
		}
		if (Node->RoomType == EGameXXKOneGameRouteRoomType::Boss)
		{
			return FLinearColor(0.42f, 0.25f, 0.12f, 0.76f);
		}
		return FLinearColor(0.15f, 0.18f, 0.21f, 0.72f);
	}

	FLinearColor RouteLineColor(bool bRouteLineIsOpen)
	{
		return bRouteLineIsOpen
			? FLinearColor(0.36f, 0.68f, 0.67f, 0.96f)
			: FLinearColor(0.17f, 0.21f, 0.24f, 0.68f);
	}

	const FGameXXKOneGameRouteNode* FindRenderedRouteNodeById(
		const TArray<FGameXXKOneGameRouteNode>& Nodes,
		int32 RenderedNodeCount,
		int32 NodeId)
	{
		for (int32 NodeIndex = 0; NodeIndex < RenderedNodeCount && Nodes.IsValidIndex(NodeIndex); ++NodeIndex)
		{
			if (Nodes[NodeIndex].NodeIndex == NodeId)
			{
				return &Nodes[NodeIndex];
			}
		}
		return nullptr;
	}
}

UGameXXKOneGameRouteMapWidget::UGameXXKOneGameRouteMapWidget()
{
	OneGameRouteWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameRouteWidgetClassPath));
	OneGameNodeWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameNodeWidgetClassPath));
	OneGameLineWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameLineWidgetClassPath));
	OneGameBossWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameBossWidgetClassPath));
	OneGameRouteLineTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameRouteLineTexturePath));
	OneGameBattleTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameBattleTexturePath));
	OneGameBattleDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameBattleDisabledTexturePath));
	OneGameEliteTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameEliteTexturePath));
	OneGameEliteDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameEliteDisabledTexturePath));
	OneGameCampTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameCampTexturePath));
	OneGameCampDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameCampDisabledTexturePath));
	OneGameChestTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameChestTexturePath));
	OneGameChestDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameChestDisabledTexturePath));
	OneGameMerchantTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameMerchantTexturePath));
	OneGameMerchantDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameMerchantDisabledTexturePath));
	OneGameEventTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameEventTexturePath));
	OneGameEventDisabledTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameEventDisabledTexturePath));
	OneGameRouteBackgroundTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameRouteBackgroundTexturePath));
}

TSharedRef<SWidget> UGameXXKOneGameRouteMapWidget::RebuildWidget()
{
	BuildProgrammaticLayout();
	RegisterGuideTargets();
	return Super::RebuildWidget();
}

void UGameXXKOneGameRouteMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKOneGameRouteMapWidget::NativeDestruct()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	for (UButton* Button : NodeButtons)
	{
		Registry.UnregisterTarget(TEXT("Route.Tutorial.NextNode"), Button);
	}
	Registry.UnregisterTarget(TEXT("Route.Settlement.Confirm"), RouteAbandonConfirmButton);
	Registry.UnregisterTarget(TEXT("Route.Settlement.Confirm"), RouteCloseChallengeButton);
	ClearTransientRouteProjection();
	Super::NativeDestruct();
}

void UGameXXKOneGameRouteMapWidget::SetTransientRouteProjection(
	const TArray<FGameXXKRouteMapNode>& Nodes,
	const TArray<FGameXXKRouteMapEdge>& Edges,
	const TMap<int32, FText>& Labels,
	const FText& CompletionNotice,
	const TSet<int32>& VisitedNodeIds,
	const TSet<int32>& ReachableNodeIds,
	FGameXXKTransientRouteNodeExecuted OnExecuted)
{
	bUsingTransientRouteProjection = true;
	TransientRouteNodes = Nodes;
	TransientRouteEdges = Edges;
	TransientRouteLabels = Labels;
	TransientCompletionNotice = CompletionNotice;
	TransientVisitedNodeIds = VisitedNodeIds;
	TransientReachableNodeIds = ReachableNodeIds;
	TransientNodeExecutedDelegate = MoveTemp(OnExecuted);
	bHasRememberedRouteIdentity = false;
	bHasAppliedInitialScrollOffset = false;
}

void UGameXXKOneGameRouteMapWidget::ClearTransientRouteProjection()
{
	bUsingTransientRouteProjection = false;
	TransientRouteNodes.Reset();
	TransientRouteEdges.Reset();
	TransientRouteLabels.Reset();
	TransientVisitedNodeIds.Reset();
	TransientReachableNodeIds.Reset();
	TransientCompletionNotice = FText::GetEmpty();
	TransientNodeExecutedDelegate.Unbind();
	bHasRememberedRouteIdentity = false;
	bHasAppliedInitialScrollOffset = false;
}

bool UGameXXKOneGameRouteMapWidget::IsUsingTransientRouteProjectionForTest() const
{
	return bUsingTransientRouteProjection;
}

bool UGameXXKOneGameRouteMapWidget::IsOrdinaryRouteSummaryVisibleForTest() const
{
	return RouteSummaryBorder
		&& RouteSummaryBorder->GetVisibility() != ESlateVisibility::Collapsed
		&& RouteSummaryBorder->GetVisibility() != ESlateVisibility::Hidden;
}

bool UGameXXKOneGameRouteMapWidget::IsTransientCompletionNoticeVisibleForTest() const
{
	return TransientCompletionNoticeBorder
		&& TransientCompletionNoticeBorder->GetVisibility() != ESlateVisibility::Collapsed
		&& TransientCompletionNoticeBorder->GetVisibility() != ESlateVisibility::Hidden;
}

FText UGameXXKOneGameRouteMapWidget::GetTransientCompletionNoticeForTest() const
{
	return TransientCompletionNoticeText
		? TransientCompletionNoticeText->GetText()
		: FText::GetEmpty();
}

FReply UGameXXKOneGameRouteMapWidget::NativeOnKeyDown(
	const FGeometry& InGeometry,
	const FKeyEvent& InKeyEvent)
{
	if (InKeyEvent.GetKey() == EKeys::Escape && bRouteAbandonConfirmationOpen)
	{
		CancelRouteAbandonConfirmation();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

FEventReply UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseButtonDown(
	FGeometry MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (bRouteAbandonConfirmationOpen)
	{
		return FEventReply(true);
	}
	if (MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton || !RouteDragSurface)
	{
		return FEventReply(false);
	}

	bRouteMapDragActive = true;
	bRouteMapDragMoved = false;
	RouteMapDragStartScreenPosition = MouseEvent.GetScreenSpacePosition();
	LastRouteMapDragScreenPosition = RouteMapDragStartScreenPosition;
	FEventReply Reply(true);
	Reply.NativeReply = FReply::Handled().CaptureMouse(RouteDragSurface->TakeWidget());
	return Reply;
}

FEventReply UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseButtonUp(
	FGeometry MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (bRouteAbandonConfirmationOpen)
	{
		bRouteMapDragActive = false;
		return FEventReply(true);
	}
	if (!bRouteMapDragActive || MouseEvent.GetEffectingButton() != EKeys::LeftMouseButton)
	{
		return FEventReply(false);
	}

	const FVector2D ReleasePosition = MouseEvent.GetScreenSpacePosition();
	const bool bWasClick = !bRouteMapDragMoved
		&& (ReleasePosition - RouteMapDragStartScreenPosition).SizeSquared() <= RouteClickDragThresholdSq;

	bRouteMapDragActive = false;
	bRouteMapDragMoved = false;
	FEventReply Reply(true);
	Reply.NativeReply = FReply::Handled().ReleaseMouseCapture();
	if (bWasClick)
	{
		TryExecuteRouteNodeAtScreenPosition(ReleasePosition);
	}
	return Reply;
}

FEventReply UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseMove(
	FGeometry MyGeometry,
	const FPointerEvent& MouseEvent)
{
	if (bRouteAbandonConfirmationOpen)
	{
		bRouteMapDragActive = false;
		return FEventReply(true);
	}
	if (!bRouteMapDragActive)
	{
		return FEventReply(false);
	}

	if (!MouseEvent.IsMouseButtonDown(EKeys::LeftMouseButton))
	{
		bRouteMapDragActive = false;
		FEventReply Reply(true);
		Reply.NativeReply = FReply::Handled().ReleaseMouseCapture();
		return Reply;
	}

	const FVector2D CurrentPosition = MouseEvent.GetScreenSpacePosition();
	const FVector2D PointerDelta = CurrentPosition - LastRouteMapDragScreenPosition;
	LastRouteMapDragScreenPosition = CurrentPosition;
	if ((CurrentPosition - RouteMapDragStartScreenPosition).SizeSquared() > RouteClickDragThresholdSq)
	{
		bRouteMapDragMoved = true;
	}
	ApplyRouteMapDragDeltaForTest(PointerDelta.Y);

	return FEventReply(true);
}

void UGameXXKOneGameRouteMapWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
	UpdateRouteSummary();
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	for (int32 LineIndex = 0; LineIndex < LineVisualWidgets.Num(); ++LineIndex)
	{
		ConfigureLineVisual(LineIndex, Nodes);
	}
	for (int32 ButtonIndex = 0; ButtonIndex < NodeButtons.Num(); ++ButtonIndex)
	{
		const FGameXXKOneGameRouteNode* Node = Nodes.IsValidIndex(ButtonIndex) ? &Nodes[ButtonIndex] : nullptr;
		ConfigureNodeButton(ButtonIndex, Node);
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || !State->bHasGeneratedRouteMap)
	{
		bHasRememberedRouteIdentity = false;
		RememberedRouteSeed = 0;
		RememberedRouteTopologyHash = 0;
		bHasAppliedInitialScrollOffset = false;
	}
	else
	{
		const uint32 RouteTopologyHash = CalculateRouteTopologyHash(*State);
		if (!bHasRememberedRouteIdentity
			|| RememberedRouteSeed != State->RouteSeed
			|| RememberedRouteTopologyHash != RouteTopologyHash)
		{
			bHasRememberedRouteIdentity = true;
			RememberedRouteSeed = State->RouteSeed;
			RememberedRouteTopologyHash = RouteTopologyHash;
			bHasAppliedInitialScrollOffset = false;
		}
	}
	const EGameXXKScreen ActiveScreen = State ? State->Screen : EGameXXKScreen::MainMenu;
	const bool bKeepRouteVisibleUnderEncounter = ActiveScreen == EGameXXKScreen::RouteEvent
		|| ActiveScreen == EGameXXKScreen::RouteCamp
		|| ActiveScreen == EGameXXKScreen::RouteMerchant;
	SetVisibility(Subsystem && (ActiveScreen == EGameXXKScreen::DungeonMap || bKeepRouteVisibleUnderEncounter)
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
	RefreshFixedControls();
	RegisterGuideTargets();
	const bool bGuideRouteVisible = Subsystem && ActiveScreen == EGameXXKScreen::DungeonMap;
	if (bGuideRouteVisible && !bGuideRouteOpenedEmitted)
	{
		bGuideRouteOpenedEmitted = true;
		FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.RouteMap.Opened"));
	}
	else if (!bGuideRouteVisible)
	{
		bGuideRouteOpenedEmitted = false;
	}
	if (!bHasAppliedInitialScrollOffset && RouteScrollBox && GetRenderedRouteNodeCount(Nodes) > 0)
	{
		ApplyInitialScrollOffset(Nodes);
		bHasAppliedInitialScrollOffset = true;
	}
}

FBox2D UGameXXKOneGameRouteMapWidget::ResolveRouteCloseChallengeRect(const FVector2D ViewportSize) const
{
	if (ViewportSize.X <= 0.0f || ViewportSize.Y <= 0.0f)
	{
		return FBox2D(EForceInit::ForceInit);
	}
	const double Scale = FMath::Min(
		ViewportSize.X / RouteViewportDesignSize.X,
		ViewportSize.Y / RouteViewportDesignSize.Y);
	const FVector2D Minimum(
		ViewportSize.X - (RouteViewportDesignSize.X - RouteCloseChallengePosition.X) * Scale,
		RouteCloseChallengePosition.Y * Scale);
	return FBox2D(Minimum, Minimum + RouteCloseChallengeSize * Scale);
}

bool UGameXXKOneGameRouteMapWidget::CanConfirmRouteAbandon(FString* OutReason) const
{
	if (OutReason)
	{
		OutReason->Reset();
	}
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!bRouteAbandonConfirmationOpen || bRouteSettlementInProgress || !Subsystem)
	{
		if (OutReason) *OutReason = TEXT("当前没有可结算的路线挑战。");
		return false;
	}
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (!bRouteAbandonPreviewValid
		|| !RouteAbandonPreview.SettlementId.IsValid()
		|| RouteAbandonPreview.Outcome != EGameXXKRouteTerminalOutcome::Abandoned)
	{
		if (OutReason) *OutReason = RouteAbandonError.IsEmpty()
			? TEXT("无法计算本次挑战的结算奖励。")
			: RouteAbandonError;
		return false;
	}
	if (State.Screen != EGameXXKScreen::DungeonMap
		|| !State.bDungeonActive
		|| !State.bHasGeneratedRouteMap
		|| State.CardRun.RouteTravelMoney != RouteAbandonPreview.SourceTravelMoney
		|| State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount
			!= RouteAbandonPreview.SourceCardAcquisitionCount)
	{
		if (OutReason) *OutReason = TEXT("路线进度已变化，请关闭弹窗后重新确认结算。");
		return false;
	}
	return true;
}

void UGameXXKOneGameRouteMapWidget::RefreshRouteAbandonConfirmation()
{
	if (!RouteAbandonModalOverlay)
	{
		return;
	}
	RouteAbandonModalOverlay->SetVisibility(
		bRouteAbandonConfirmationOpen
			? ESlateVisibility::Visible
			: ESlateVisibility::Collapsed);
	if (!bRouteAbandonConfirmationOpen)
	{
		return;
	}

	if (RouteAbandonPreviewText)
	{
		const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
		const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
		const int32 CompletedNodeCount = State ? State->VisitedRouteNodeIds.Num() : 0;
		const int32 TotalNodeCount = State ? State->RouteMapNodes.Num() : 0;
		const int32 ForfeitedNodeCount = FMath::Max(0, TotalNodeCount - CompletedNodeCount);
		RouteAbandonPreviewText->SetText(bRouteAbandonPreviewValid
			? FText::FromString(FString::Printf(
				TEXT("已获奖励：普通金币 +%d，强化石 +%d\n已完成进度：%d / %d\n未解决或未访问节点：%d，提前结束将失去其中内容"),
				RouteAbandonPreview.PermanentGoldAward,
				RouteAbandonPreview.EnhancementStoneAward,
				CompletedNodeCount,
				TotalNodeCount,
				ForfeitedNodeCount))
			: FText::FromString(TEXT("已获奖励：普通金币 --，强化石 --\n已完成进度：--\n未解决内容不会被结算")));
	}
	FString GateReason;
	const bool bCanConfirm = CanConfirmRouteAbandon(&GateReason);
	if (RouteAbandonConfirmButton)
	{
		RouteAbandonConfirmButton->SetIsEnabled(bCanConfirm);
	}
	if (RouteAbandonCancelButton)
	{
		RouteAbandonCancelButton->SetIsEnabled(true);
	}
	if (RouteAbandonErrorText)
	{
		const FString DisplayError = RouteAbandonError.IsEmpty() ? GateReason : RouteAbandonError;
		RouteAbandonErrorText->SetText(FText::FromString(DisplayError));
		RouteAbandonErrorText->SetVisibility(
			DisplayError.IsEmpty()
				? ESlateVisibility::Collapsed
				: ESlateVisibility::HitTestInvisible);
	}
}

void UGameXXKOneGameRouteMapWidget::RefreshFixedControls()
{
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* const State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const bool bCanShowClose = !bUsingTransientRouteProjection && State
		&& State->Screen == EGameXXKScreen::DungeonMap
		&& State->bDungeonActive
		&& State->bHasGeneratedRouteMap;
	if (bUsingTransientRouteProjection && bRouteAbandonConfirmationOpen)
	{
		bRouteAbandonConfirmationOpen = false;
		bRouteAbandonPreviewValid = false;
		bRouteSettlementInProgress = false;
		RouteAbandonPreview = FGameXXKRouteSettlementReceipt{};
		RouteAbandonError.Reset();
	}
	if (State && State->Screen != EGameXXKScreen::DungeonMap && bRouteAbandonConfirmationOpen)
	{
		bRouteAbandonConfirmationOpen = false;
		bRouteAbandonPreviewValid = false;
		bRouteSettlementInProgress = false;
		RouteAbandonPreview = FGameXXKRouteSettlementReceipt{};
		RouteAbandonError.Reset();
	}
	if (RouteCloseChallengeContainer)
	{
		RouteCloseChallengeContainer->SetVisibility(
			bCanShowClose ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	}
	if (RouteCloseChallengeButton)
	{
		RouteCloseChallengeButton->SetIsEnabled(bCanShowClose && !bRouteAbandonConfirmationOpen);
	}
	if (RouteSummaryBorder)
	{
		RouteSummaryBorder->SetVisibility(
			bUsingTransientRouteProjection
				? ESlateVisibility::Collapsed
				: ESlateVisibility::SelfHitTestInvisible);
	}
	if (TransientCompletionNoticeText)
	{
		TransientCompletionNoticeText->SetText(TransientCompletionNotice);
	}
	if (TransientCompletionNoticeBorder)
	{
		TransientCompletionNoticeBorder->SetVisibility(
			bUsingTransientRouteProjection && !TransientCompletionNotice.IsEmpty()
				? ESlateVisibility::HitTestInvisible
				: ESlateVisibility::Collapsed);
	}
	if (RouteScrollBox)
	{
		RouteScrollBox->SetIsEnabled(!bRouteAbandonConfirmationOpen);
	}
	if (bRouteAbandonConfirmationOpen)
	{
		bRouteMapDragActive = false;
		bRouteMapDragMoved = false;
		for (UButton* NodeButton : NodeButtons)
		{
			if (NodeButton) NodeButton->SetIsEnabled(false);
		}
	}
	RefreshRouteAbandonConfirmation();
}

bool UGameXXKOneGameRouteMapWidget::OpenRouteAbandonConfirmation()
{
	if (bUsingTransientRouteProjection || bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::DungeonMap)
	{
		return false;
	}
	bRouteAbandonConfirmationOpen = true;
	FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Settlement.Opened"));
	bRouteAbandonPreviewValid = false;
	bRouteSettlementInProgress = false;
	RouteAbandonPreview = FGameXXKRouteSettlementReceipt{};
	RouteAbandonError.Reset();
	FString PreviewError;
	bRouteAbandonPreviewValid = Subsystem->PreviewAbandonedRouteSettlement(
		RouteAbandonPreview,
		&PreviewError);
	if (!bRouteAbandonPreviewValid)
	{
		RouteAbandonError = PreviewError.IsEmpty()
			? TEXT("无法计算本次挑战的结算奖励。")
			: PreviewError;
	}
	RefreshFixedControls();
	return true;
}

bool UGameXXKOneGameRouteMapWidget::CancelRouteAbandonConfirmation()
{
	if (!bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	bRouteAbandonConfirmationOpen = false;
	bRouteAbandonPreviewValid = false;
	bRouteSettlementInProgress = false;
	RouteAbandonPreview = FGameXXKRouteSettlementReceipt{};
	RouteAbandonError.Reset();
	RefreshFromState();
	return true;
}

bool UGameXXKOneGameRouteMapWidget::ConfirmRouteAbandon()
{
	if (!FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Route.SettlementConfirm")))
	{
		return false;
	}
	if (!bRouteAbandonConfirmationOpen || bRouteSettlementInProgress)
	{
		return false;
	}
	FString GateReason;
	if (!CanConfirmRouteAbandon(&GateReason))
	{
		RefreshRouteAbandonConfirmation();
		return false;
	}
	UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	bRouteSettlementInProgress = true;
	RefreshRouteAbandonConfirmation();
	FGameXXKRouteSettlementReceipt AppliedReceipt;
	FString SettlementError;
	if (!Subsystem || !Subsystem->SettleAndExitActiveRoute(AppliedReceipt, SettlementError))
	{
		bRouteSettlementInProgress = false;
		RouteAbandonError = SettlementError.IsEmpty()
			? TEXT("结算失败，路线进度与奖励均未改变。")
			: FString::Printf(TEXT("结算失败：%s"), *SettlementError);
		RefreshRouteAbandonConfirmation();
		return false;
	}
	bRouteSettlementInProgress = false;
	bRouteAbandonConfirmationOpen = false;
	bRouteAbandonPreviewValid = false;
	RouteAbandonPreview = FGameXXKRouteSettlementReceipt{};
	RouteAbandonError.Reset();
	FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Settlement.Confirmed"));
	GameXXKLevelFlow::OpenMapForRuntimeState(Subsystem);
	NotifyPlayerFlowStateChanged();
	return true;
}

void UGameXXKOneGameRouteMapWidget::HandleCloseChallengeClicked()
{
	OpenRouteAbandonConfirmation();
}

void UGameXXKOneGameRouteMapWidget::HandleRouteAbandonConfirmClicked()
{
	ConfirmRouteAbandon();
}

void UGameXXKOneGameRouteMapWidget::HandleRouteAbandonCancelClicked()
{
	CancelRouteAbandonConfirmation();
}

void UGameXXKOneGameRouteMapWidget::HandleRouteUserScrolled(float CurrentOffset)
{
	if (bRouteAbandonConfirmationOpen)
	{
		if (RouteScrollBox)
		{
			RouteScrollBox->SetScrollOffset(LastAppliedScrollOffset);
		}
		return;
	}
	LastAppliedScrollOffset = FMath::Clamp(CurrentOffset, 0.0f, CalculateMaxScrollOffset());
}

FGameXXKRouteMapSummaryView UGameXXKOneGameRouteMapWidget::BuildRouteSummaryView() const
{
	FGameXXKRouteMapSummaryView Summary;
	Summary.CapacityLimit = FGameXXKCardRunState::MaxBossCardSlots;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return Summary;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	Summary.RouteTravelMoney = State.CardRun.RouteTravelMoney;
	for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
	{
		if (Node.NodeKind == EGameXXKNodeKind::Start)
		{
			continue;
		}
		++Summary.TotalNodeCount;
		if (State.VisitedRouteNodeIds.Contains(Node.NodeId))
		{
			++Summary.CompletedNodeCount;
		}
	}

	Summary.CapacityUsed = State.CardRun.BossCardSlots.Num();
	Summary.bCapacityValid = true;
	return Summary;
}

FGameXXKRouteMapSummaryView UGameXXKOneGameRouteMapWidget::GetRouteSummaryViewForTest() const
{
	return BuildRouteSummaryView();
}

FText UGameXXKOneGameRouteMapWidget::GetRouteMoneySummaryTextForTest() const
{
	return RouteMoneySummaryText ? RouteMoneySummaryText->GetText() : FText::GetEmpty();
}

void UGameXXKOneGameRouteMapWidget::UpdateRouteSummary()
{
	const FGameXXKRouteMapSummaryView Summary = BuildRouteSummaryView();
	if (RouteMoneySummaryText)
	{
		RouteMoneySummaryText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMap", "TravelMoneySummary", "行旅钱  {0}"),
			FText::AsNumber(Summary.RouteTravelMoney)));
	}
	if (RouteProgressSummaryText)
	{
		RouteProgressSummaryText->SetText(FText::Format(
			NSLOCTEXT("GameXXKRouteMap", "ProgressSummary", "路线进度  {0} / {1}"),
			FText::AsNumber(Summary.CompletedNodeCount),
			FText::AsNumber(Summary.TotalNodeCount)));
	}
	if (RouteCapacitySummaryText)
	{
		RouteCapacitySummaryText->SetText(Summary.bCapacityValid
			? FText::Format(
				NSLOCTEXT("GameXXKRouteMap", "CapacitySummary", "首领卡槽  {0} / {1}"),
				FText::AsNumber(Summary.CapacityUsed),
				FText::AsNumber(Summary.CapacityLimit))
			: NSLOCTEXT("GameXXKRouteMap", "CapacitySummaryInvalid", "首领卡槽  -- / 3"));
	}
}

TArray<FGameXXKOneGameRouteNode> UGameXXKOneGameRouteMapWidget::BuildAdapterNodes() const
{
	TArray<FGameXXKOneGameRouteNode> AdapterNodes;
	if (bUsingTransientRouteProjection)
	{
		AdapterNodes.Reserve(TransientRouteNodes.Num());
		for (const FGameXXKRouteMapNode& RouteNode : TransientRouteNodes)
		{
			FGameXXKOneGameRouteNode AdapterNode;
			AdapterNode.CommandName = FName(*FString::Printf(
				TEXT("Tutorial01.Node.%d"),
				RouteNode.NodeId));
			const FText* Label = TransientRouteLabels.Find(RouteNode.NodeId);
			AdapterNode.Label = Label && !Label->IsEmpty()
				? *Label
				: RoomTypeLabel(MapRoomType(RouteNode.NodeKind));
			AdapterNode.NodeKind = RouteNode.NodeKind;
			AdapterNode.RoomType = MapRoomType(RouteNode.NodeKind);
			AdapterNode.NodeIndex = RouteNode.NodeId;
			AdapterNode.bVisited = TransientVisitedNodeIds.Contains(RouteNode.NodeId);
			AdapterNode.bEnabled = !AdapterNode.bVisited
				&& TransientReachableNodeIds.Contains(RouteNode.NodeId);
			AdapterNode.NormalizedPosition = RouteNode.NormalizedPosition;
			AdapterNodes.Add(MoveTemp(AdapterNode));
		}
		return AdapterNodes;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKMVPRouteNodeDescriptor> RouteNodes = GameXXKMVPCommandRouter::BuildRouteMapNodes(Subsystem);
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;

	for (const FGameXXKMVPRouteNodeDescriptor& RouteNode : RouteNodes)
	{
		FGameXXKOneGameRouteNode AdapterNode;
		AdapterNode.CommandName = RouteNode.CommandName;
		AdapterNode.Label = RouteNode.Label.IsEmpty() ? RoomTypeLabel(MapRoomType(RouteNode.NodeKind)) : RouteNode.Label;
		const FString OldLabel = AdapterNode.Label.ToString();
		if (OldLabel == TEXT("Start") || OldLabel == TEXT("Battle") || OldLabel == TEXT("Elite")
			|| OldLabel == TEXT("Camp") || OldLabel == TEXT("Chest") || OldLabel == TEXT("Merchant") || OldLabel == TEXT("Event"))
			AdapterNode.Label = RoomTypeLabel(MapRoomType(RouteNode.NodeKind));
		AdapterNode.NodeKind = RouteNode.NodeKind;
		AdapterNode.RoomType = MapRoomType(RouteNode.NodeKind);
		AdapterNode.NodeIndex = RouteNode.NodeIndex;
		AdapterNode.bEnabled = RouteNode.bEnabled;
		AdapterNode.bVisited = State && State->VisitedRouteNodeIds.Contains(RouteNode.NodeIndex);
		AdapterNode.NormalizedPosition = RouteNode.NormalizedPosition;
		AdapterNodes.Add(AdapterNode);
	}

	return AdapterNodes;
}

bool UGameXXKOneGameRouteMapWidget::ExecuteRouteNode(int32 NodeIndex)
{
	if (bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	if (!Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	return ExecuteRouteNodeById(Nodes[NodeIndex].NodeIndex);
}

bool UGameXXKOneGameRouteMapWidget::ExecuteRouteNodeById(int32 NodeId)
{
	if (!FGameXXKGuideTargetRegistry::Get().IsActionAllowed(TEXT("Action.Route.SelectNext")))
	{
		return false;
	}
	if (bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	if (bUsingTransientRouteProjection)
	{
		const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
		const FGameXXKOneGameRouteNode* NodeById = Nodes.FindByPredicate(
			[NodeId](const FGameXXKOneGameRouteNode& Node)
			{
				return Node.NodeIndex == NodeId;
			});
		if (!NodeById
			|| !NodeById->bEnabled
			|| NodeById->bVisited
			|| !TransientNodeExecutedDelegate.IsBound())
		{
			return false;
		}
		const FGameXXKOneGameRouteNode Node = *NodeById;
		const bool bExecuted = TransientNodeExecutedDelegate.Execute(NodeId);
		if (bExecuted)
		{
			FGameXXKGuideTargetRegistry::Get().EmitEvent(
				TEXT("Event.Route.NextNodeSelected"));
			OnRouteNodeExecuted(Node);
			if (!NotifyPlayerFlowStateChanged())
			{
				RefreshFromState();
			}
		}
		return bExecuted;
	}
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem)
	{
		return false;
	}

	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	const FGameXXKOneGameRouteNode* NodeById = Nodes.FindByPredicate([NodeId](const FGameXXKOneGameRouteNode& Node)
	{
		return Node.NodeIndex == NodeId;
	});
	if (!NodeById || !NodeById->bEnabled || NodeById->CommandName.IsNone())
	{
		return false;
	}

	const FGameXXKOneGameRouteNode Node = *NodeById;
	const bool bExecuted = GameXXKMVPCommandRouter::ExecuteVisibleCommand(Subsystem, Node.CommandName);
	if (bExecuted)
	{
		FGameXXKGuideTargetRegistry::Get().EmitEvent(TEXT("Event.Route.NextNodeSelected"));
		OnRouteNodeExecuted(Node);
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

void UGameXXKOneGameRouteMapWidget::RegisterGuideTargets()
{
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	for (UButton* Button : NodeButtons)
	{
		Registry.UnregisterTarget(TEXT("Route.Tutorial.NextNode"), Button);
	}
	Registry.UnregisterTarget(TEXT("Route.Settlement.Confirm"), RouteAbandonConfirmButton);
	Registry.UnregisterTarget(TEXT("Route.Settlement.Confirm"), RouteCloseChallengeButton);
	const UGameXXKMVPSubsystem* const Subsystem = ResolveMVPSubsystem();
	if (!Subsystem
		|| Subsystem->GetRuntimeState().Screen != EGameXXKScreen::DungeonMap
		|| GetVisibility() != ESlateVisibility::Visible)
	{
		// A hidden/generated route projection must not collide with the canonical
		// desktop tutorial route panel or leave a stale semantic target in Battle.
		return;
	}
	UButton* NextButton = nullptr;
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	for (int32 Index = 0; Index < NodeButtons.Num(); ++Index)
	{
		if (Nodes.IsValidIndex(Index) && Nodes[Index].bEnabled && NodeButtons[Index])
		{
			NextButton = NodeButtons[Index];
			break;
		}
	}
	if (!NextButton && !NodeButtons.IsEmpty())
	{
		NextButton = NodeButtons[0];
	}
	if (NextButton)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Tutorial.NextNode"), NextButton);
	}
	UButton* SettlementButton = RouteAbandonConfirmButton
		? RouteAbandonConfirmButton.Get()
		: RouteCloseChallengeButton.Get();
	if (SettlementButton)
	{
		Registry.RegisterWidgetTarget(TEXT("Route.Settlement.Confirm"), SettlementButton);
	}
}

TArray<FGameXXKOneGameRouteNodeVisualState> UGameXXKOneGameRouteMapWidget::GetRouteNodeVisualStatesForTest() const
{
	TArray<FGameXXKOneGameRouteNodeVisualState> VisualStates;
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
	VisualStates.Reserve(RenderedNodeCount);
	for (int32 VisualIndex = 0; VisualIndex < RenderedNodeCount; ++VisualIndex)
	{
		const FGameXXKOneGameRouteNode& Node = Nodes[VisualIndex];
		FGameXXKOneGameRouteNodeVisualState VisualState;
		VisualState.NodeId = Node.NodeIndex;
		VisualState.VisualIndex = VisualIndex;
		VisualState.CommandName = Node.CommandName;
		VisualState.Label = Node.Label;
		VisualState.NodeKind = Node.NodeKind;
		VisualState.RoomType = Node.RoomType;
		VisualState.bEnabled = Node.bEnabled;
		VisualState.bVisited = Node.bVisited;
		VisualState.NormalizedPosition = Node.NormalizedPosition;
		VisualState.CanvasPosition = GetNodeCanvasPosition(Node);
		VisualState.HitBoxPosition = VisualState.CanvasPosition + FVector2D(-55.0f, -36.0f);
		VisualState.HitBoxSize = FVector2D(110.0f, 72.0f);
		VisualState.ViewportHitBoxPosition = RouteMapViewportPosition + VisualState.HitBoxPosition - FVector2D(0.0f, LastAppliedScrollOffset);
		VisualState.ViewportHitBoxCenter = VisualState.ViewportHitBoxPosition + VisualState.HitBoxSize * 0.5f;
		VisualState.ScreenHitBoxPosition = VisualState.ViewportHitBoxPosition;
		VisualState.ScreenHitBoxCenter = VisualState.ViewportHitBoxCenter;
		if (const UButton* Button = NodeButtons.IsValidIndex(VisualIndex) ? NodeButtons[VisualIndex].Get() : nullptr)
		{
			const FGeometry& ButtonGeometry = Button->GetCachedGeometry();
			if (ButtonGeometry.GetLocalSize().X > 0.0f && ButtonGeometry.GetLocalSize().Y > 0.0f)
			{
				VisualState.ScreenHitBoxPosition = ButtonGeometry.LocalToAbsolute(FVector2D::ZeroVector);
				VisualState.ScreenHitBoxCenter = ButtonGeometry.LocalToAbsolute(ButtonGeometry.GetLocalSize() * 0.5f);
			}
		}
		VisualState.IconPath = GetTextureForNode(Node).ToSoftObjectPath().ToString();
		VisualStates.Add(VisualState);
	}
	return VisualStates;
}

void UGameXXKOneGameRouteMapWidget::SetRouteMapViewportGeometry(FVector2D InViewportPosition, FVector2D InViewportSize)
{
	RouteMapViewportPosition = InViewportPosition;
	RouteMapViewportSize = InViewportSize;
	if (bHasAppliedInitialScrollOffset)
	{
		SetRouteScrollOffset(LastAppliedScrollOffset);
	}
}

float UGameXXKOneGameRouteMapWidget::GetCurrentScrollOffset() const
{
	if (RouteScrollBox && RouteScrollBox->GetCachedWidget().IsValid())
	{
		return FMath::Clamp(RouteScrollBox->GetScrollOffset(), 0.0f, CalculateMaxScrollOffset());
	}
	return FMath::Clamp(LastAppliedScrollOffset, 0.0f, CalculateMaxScrollOffset());
}

void UGameXXKOneGameRouteMapWidget::RestoreScrollOffset(float InOffset)
{
	SetRouteScrollOffset(InOffset);
}

bool UGameXXKOneGameRouteMapWidget::IsOneGameRouteWidgetClassConfigured() const
{
	return !OneGameRouteWidgetClass.IsNull();
}

FString UGameXXKOneGameRouteMapWidget::GetOneGameRouteWidgetClassPath() const
{
	return OneGameRouteWidgetClass.ToSoftObjectPath().ToString();
}

bool UGameXXKOneGameRouteMapWidget::AreOneGameVisualClassesConfigured() const
{
	return !OneGameNodeWidgetClass.IsNull() && !OneGameLineWidgetClass.IsNull() && !OneGameBossWidgetClass.IsNull();
}

bool UGameXXKOneGameRouteMapWidget::AreOneGameTextureVisualsConfigured() const
{
	return !OneGameRouteLineTexture.IsNull()
		&& !OneGameBattleTexture.IsNull()
		&& !OneGameBattleDisabledTexture.IsNull()
		&& !OneGameEliteTexture.IsNull()
		&& !OneGameEliteDisabledTexture.IsNull()
		&& !OneGameCampTexture.IsNull()
		&& !OneGameCampDisabledTexture.IsNull()
		&& !OneGameChestTexture.IsNull()
		&& !OneGameChestDisabledTexture.IsNull()
		&& !OneGameMerchantTexture.IsNull()
		&& !OneGameMerchantDisabledTexture.IsNull()
		&& !OneGameEventTexture.IsNull()
		&& !OneGameEventDisabledTexture.IsNull();
}

FString UGameXXKOneGameRouteMapWidget::GetOneGameNodeWidgetClassPath() const
{
	return OneGameNodeWidgetClass.ToSoftObjectPath().ToString();
}

FString UGameXXKOneGameRouteMapWidget::GetOneGameLineWidgetClassPath() const
{
	return OneGameLineWidgetClass.ToSoftObjectPath().ToString();
}

FString UGameXXKOneGameRouteMapWidget::GetOneGameBossWidgetClassPath() const
{
	return OneGameBossWidgetClass.ToSoftObjectPath().ToString();
}

int32 UGameXXKOneGameRouteMapWidget::GetCreatedNodeVisualWidgetCount() const
{
	return NodeVisualWidgets.Num();
}

int32 UGameXXKOneGameRouteMapWidget::GetCreatedLineVisualWidgetCount() const
{
	return LineVisualWidgets.Num();
}

bool UGameXXKOneGameRouteMapWidget::ShouldUseOneGameBlueprintVisualWidgets() const
{
	return bUseOneGameBlueprintVisualWidgets;
}

FString UGameXXKOneGameRouteMapWidget::GetCreatedNodeVisualWidgetClassName(int32 WidgetIndex) const
{
	if (!NodeVisualWidgets.IsValidIndex(WidgetIndex) || !NodeVisualWidgets[WidgetIndex])
	{
		return FString();
	}
	return NodeVisualWidgets[WidgetIndex]->GetClass()->GetName();
}

FString UGameXXKOneGameRouteMapWidget::GetCreatedLineVisualWidgetClassName(int32 WidgetIndex) const
{
	if (!LineVisualWidgets.IsValidIndex(WidgetIndex) || !LineVisualWidgets[WidgetIndex])
	{
		return FString();
	}
	return LineVisualWidgets[WidgetIndex]->GetClass()->GetName();
}

FText UGameXXKOneGameRouteMapWidget::GetCreatedNodeVisualLabel(int32 WidgetIndex) const
{
	const UTextBlock* NodeLabel = NodeVisualLabels.IsValidIndex(WidgetIndex) ? NodeVisualLabels[WidgetIndex].Get() : nullptr;
	return NodeLabel ? NodeLabel->GetText() : FText::GetEmpty();
}

FString UGameXXKOneGameRouteMapWidget::GetCreatedNodeVisualIconPath(int32 WidgetIndex) const
{
	return NodeVisualIconPaths.IsValidIndex(WidgetIndex) ? NodeVisualIconPaths[WidgetIndex] : FString();
}

bool UGameXXKOneGameRouteMapWidget::HasRouteBackgroundVisualForTest() const
{
	return RouteBackgroundImage != nullptr && !OneGameRouteBackgroundTexture.IsNull();
}

FString UGameXXKOneGameRouteMapWidget::GetRouteBackgroundTexturePathForTest() const
{
	return OneGameRouteBackgroundTexture.ToSoftObjectPath().ToString();
}

FVector2D UGameXXKOneGameRouteMapWidget::GetRouteContentSizeForTest() const
{
	return RouteContentSize;
}

float UGameXXKOneGameRouteMapWidget::GetLastAppliedScrollOffsetForTest() const
{
	return LastAppliedScrollOffset;
}

bool UGameXXKOneGameRouteMapWidget::IsRouteNodeButtonBoundForTest(int32 ButtonIndex) const
{
	const UButton* Button = NodeButtons.IsValidIndex(ButtonIndex) ? NodeButtons[ButtonIndex].Get() : nullptr;
	return Button && Button->OnClicked.IsBound();
}

#if WITH_DEV_AUTOMATION_TESTS
UScrollBox* UGameXXKOneGameRouteMapWidget::GetRouteScrollBoxForTest() const
{
	return RouteScrollBox;
}

UOverlay* UGameXXKOneGameRouteMapWidget::GetRouteRootOverlayForTest() const
{
	return RootOverlay;
}

USizeBox* UGameXXKOneGameRouteMapWidget::GetRouteCloseChallengeContainerForTest() const
{
	return RouteCloseChallengeContainer;
}

UButton* UGameXXKOneGameRouteMapWidget::GetRouteCloseChallengeButtonForTest() const
{
	return RouteCloseChallengeButton;
}

FBox2D UGameXXKOneGameRouteMapWidget::ResolveRouteCloseChallengeRectForTest(const FVector2D ViewportSize) const
{
	return ResolveRouteCloseChallengeRect(ViewportSize);
}

bool UGameXXKOneGameRouteMapWidget::OpenRouteAbandonConfirmationForTest()
{
	return OpenRouteAbandonConfirmation();
}

bool UGameXXKOneGameRouteMapWidget::CancelRouteAbandonConfirmationForTest()
{
	return CancelRouteAbandonConfirmation();
}

bool UGameXXKOneGameRouteMapWidget::ConfirmRouteAbandonForTest()
{
	return ConfirmRouteAbandon();
}

bool UGameXXKOneGameRouteMapWidget::IsRouteAbandonConfirmationOpenForTest() const
{
	return bRouteAbandonConfirmationOpen
		&& RouteAbandonModalOverlay
		&& RouteAbandonModalOverlay->GetVisibility() == ESlateVisibility::Visible;
}

bool UGameXXKOneGameRouteMapWidget::IsRouteAbandonConfirmEnabledForTest() const
{
	return RouteAbandonConfirmButton && RouteAbandonConfirmButton->GetIsEnabled();
}

FText UGameXXKOneGameRouteMapWidget::GetRouteAbandonPreviewTextForTest() const
{
	return RouteAbandonPreviewText ? RouteAbandonPreviewText->GetText() : FText::GetEmpty();
}

FString UGameXXKOneGameRouteMapWidget::GetRouteAbandonErrorForTest() const
{
	if (!RouteAbandonError.IsEmpty())
	{
		return RouteAbandonError;
	}
	FString GateReason;
	CanConfirmRouteAbandon(&GateReason);
	return GateReason;
}

void UGameXXKOneGameRouteMapWidget::ApplyMissingRouteCloseInkResourceForTest()
{
	ConfigureRouteCloseChallengeButton(
		RouteCloseChallengeButton,
		nullptr,
		LoadObject<UTexture2D>(nullptr, RouteActionButtonTexturePath),
		WidgetTree);
}
#endif

void UGameXXKOneGameRouteMapWidget::HandleNodeButton0Clicked()
{
	ExecuteNodeButtonAtIndex(0);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton1Clicked()
{
	ExecuteNodeButtonAtIndex(1);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton2Clicked()
{
	ExecuteNodeButtonAtIndex(2);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton3Clicked()
{
	ExecuteNodeButtonAtIndex(3);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton4Clicked()
{
	ExecuteNodeButtonAtIndex(4);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton5Clicked()
{
	ExecuteNodeButtonAtIndex(5);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton6Clicked()
{
	ExecuteNodeButtonAtIndex(6);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton7Clicked()
{
	ExecuteNodeButtonAtIndex(7);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton8Clicked()
{
	ExecuteNodeButtonAtIndex(8);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton9Clicked()
{
	ExecuteNodeButtonAtIndex(9);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton10Clicked()
{
	ExecuteNodeButtonAtIndex(10);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton11Clicked()
{
	ExecuteNodeButtonAtIndex(11);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton12Clicked()
{
	ExecuteNodeButtonAtIndex(12);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton13Clicked()
{
	ExecuteNodeButtonAtIndex(13);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton14Clicked()
{
	ExecuteNodeButtonAtIndex(14);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton15Clicked()
{
	ExecuteNodeButtonAtIndex(15);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton16Clicked()
{
	ExecuteNodeButtonAtIndex(16);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton17Clicked()
{
	ExecuteNodeButtonAtIndex(17);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton18Clicked()
{
	ExecuteNodeButtonAtIndex(18);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton19Clicked()
{
	ExecuteNodeButtonAtIndex(19);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton20Clicked()
{
	ExecuteNodeButtonAtIndex(20);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton21Clicked()
{
	ExecuteNodeButtonAtIndex(21);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton22Clicked()
{
	ExecuteNodeButtonAtIndex(22);
}

void UGameXXKOneGameRouteMapWidget::HandleNodeButton23Clicked()
{
	ExecuteNodeButtonAtIndex(23);
}

void UGameXXKOneGameRouteMapWidget::BuildProgrammaticLayout()
{
	SetIsFocusable(true);
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("OneGameRouteMapWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}

	const TArray<FGameXXKOneGameRouteNode> AdapterNodes = BuildAdapterNodes();
	RouteContentSize = CalculateRouteContentSize(AdapterNodes);
	const int32 RouteNodeCount = GetRenderedRouteNodeCount(AdapterNodes);
	const int32 RouteLineCount = GetRenderedRouteLineCount(AdapterNodes);
	bool bRouteLayoutWasCreated = false;

	if (!RootOverlay || WidgetTree->RootWidget != RootOverlay)
	{
		RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameXXKOneGameRouteMapRoot"));
		WidgetTree->RootWidget = RootOverlay;
		bRouteLayoutWasCreated = true;

		RouteScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GameXXKOneGameRouteMapScroll"));
		RouteScrollBox->SetScrollBarVisibility(ESlateVisibility::Visible);
		RouteScrollBox->OnUserScrolled.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleRouteUserScrolled);
		FGameXXKPartyDeckUiStyle::ApplyPaperInkScrollBar(RouteScrollBox);
		if (UOverlaySlot* ScrollSlot = RootOverlay->AddChildToOverlay(RouteScrollBox))
		{
			ScrollSlot->SetHorizontalAlignment(HAlign_Fill);
			ScrollSlot->SetVerticalAlignment(VAlign_Fill);
		}

		RouteContentSizeBox = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("GameXXKOneGameRouteMapContentSize"));
		RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("GameXXKOneGameRouteMapContent"));
		RouteContentSizeBox->AddChild(RootCanvas);
		RouteScrollBox->AddChild(RouteContentSizeBox);
	}

	if (RootOverlay && !RouteSummaryBorder)
	{
		RouteSummaryBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("GameXXKRouteMapFixedSummary"));
		RouteSummaryBorder->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RouteSummaryBorder->SetPadding(FMargin(18.0f, 14.0f));
		RouteSummaryBorder->SetBrushColor(FLinearColor(0.055f, 0.045f, 0.035f, 0.84f));
		if (UOverlaySlot* SummarySlot = RootOverlay->AddChildToOverlay(RouteSummaryBorder))
		{
			SummarySlot->SetHorizontalAlignment(HAlign_Left);
			SummarySlot->SetVerticalAlignment(VAlign_Top);
			SummarySlot->SetPadding(FMargin(28.0f, 24.0f, 0.0f, 0.0f));
		}

		RouteSummaryStack = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("GameXXKRouteMapSummaryStack"));
		RouteSummaryStack->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		RouteSummaryBorder->SetContent(RouteSummaryStack);

		auto AddSummaryLine = [this](const FName Name, TObjectPtr<UTextBlock>& OutText)
		{
			OutText = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			OutText->SetVisibility(ESlateVisibility::HitTestInvisible);
			OutText->SetColorAndOpacity(FSlateColor(FLinearColor(0.94f, 0.86f, 0.68f, 1.0f)));
			FSlateFontInfo Font = OutText->GetFont();
			Font.Size = 20;
			OutText->SetFont(Font);
			if (UVerticalBoxSlot* LineSlot = RouteSummaryStack->AddChildToVerticalBox(OutText))
			{
				LineSlot->SetPadding(FMargin(0.0f, 2.0f));
			}
		};
		AddSummaryLine(TEXT("GameXXKRouteMoneySummary"), RouteMoneySummaryText);
		AddSummaryLine(TEXT("GameXXKRouteProgressSummary"), RouteProgressSummaryText);
		AddSummaryLine(TEXT("GameXXKRouteCapacitySummary"), RouteCapacitySummaryText);
		UpdateRouteSummary();
	}

	if (RootOverlay && !TransientCompletionNoticeBorder)
	{
		TransientCompletionNoticeBorder = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("Tutorial01RouteCompletionNotice"));
		TransientCompletionNoticeBorder->SetPadding(FMargin(26.0f, 12.0f));
		TransientCompletionNoticeBorder->SetBrushColor(
			FLinearColor(0.88f, 0.80f, 0.64f, 0.96f));
		TransientCompletionNoticeBorder->SetVisibility(ESlateVisibility::Collapsed);
		TransientCompletionNoticeText = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("Tutorial01RouteCompletionNoticeText"));
		TransientCompletionNoticeText->SetJustification(ETextJustify::Center);
		TransientCompletionNoticeText->SetColorAndOpacity(
			FSlateColor(FLinearColor(0.12f, 0.08f, 0.04f, 1.0f)));
		FSlateFontInfo NoticeFont = TransientCompletionNoticeText->GetFont();
		NoticeFont.Size = 28;
		NoticeFont.TypefaceFontName = TEXT("Bold");
		TransientCompletionNoticeText->SetFont(NoticeFont);
		TransientCompletionNoticeBorder->SetContent(TransientCompletionNoticeText);
		if (UOverlaySlot* NoticeSlot =
			RootOverlay->AddChildToOverlay(TransientCompletionNoticeBorder))
		{
			NoticeSlot->SetHorizontalAlignment(HAlign_Center);
			NoticeSlot->SetVerticalAlignment(VAlign_Top);
			NoticeSlot->SetPadding(FMargin(0.0f, 72.0f, 0.0f, 0.0f));
		}
	}

	if (RootOverlay && !RouteCloseChallengeContainer)
	{
		UTexture2D* ActionTexture = LoadObject<UTexture2D>(nullptr, RouteActionButtonTexturePath);
		UTexture2D* CloseInkTexture = LoadObject<UTexture2D>(nullptr, RouteCloseInkTexturePath);
		RouteCloseChallengeContainer = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("RouteCloseChallengeContainer"));
		RouteCloseChallengeContainer->SetWidthOverride(RouteCloseChallengeSize.X);
		RouteCloseChallengeContainer->SetHeightOverride(RouteCloseChallengeSize.Y);
		RouteCloseChallengeButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("RouteCloseChallengeButton"));
		ConfigureRouteCloseChallengeButton(
			RouteCloseChallengeButton,
			CloseInkTexture,
			ActionTexture,
			WidgetTree);
		RouteCloseChallengeButton->SetToolTipText(
			NSLOCTEXT("GameXXKRouteMap", "OpenSettlement", "结算本次路线并返回挂机"));
		RouteCloseChallengeButton->OnClicked.AddDynamic(
			this,
			&UGameXXKOneGameRouteMapWidget::HandleCloseChallengeClicked);
		RouteCloseChallengeContainer->AddChild(RouteCloseChallengeButton);
		if (UOverlaySlot* CloseSlot = RootOverlay->AddChildToOverlay(RouteCloseChallengeContainer))
		{
			CloseSlot->SetHorizontalAlignment(HAlign_Right);
			CloseSlot->SetVerticalAlignment(VAlign_Top);
			CloseSlot->SetPadding(FMargin(0.0f, 48.0f, 72.0f, 0.0f));
		}

		RouteAbandonModalOverlay = WidgetTree->ConstructWidget<UOverlay>(
			UOverlay::StaticClass(),
			TEXT("RouteAbandonModalOverlay"));
		RouteAbandonModalOverlay->SetVisibility(ESlateVisibility::Collapsed);
		UBorder* ModalBackdrop = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("RouteAbandonModalBackdrop"));
		ModalBackdrop->SetBrushColor(FLinearColor(0.0f, 0.0f, 0.0f, 0.68f));
		ModalBackdrop->SetPadding(FMargin(0.0f));
		if (UOverlaySlot* BackdropSlot = RouteAbandonModalOverlay->AddChildToOverlay(ModalBackdrop))
		{
			BackdropSlot->SetHorizontalAlignment(HAlign_Fill);
			BackdropSlot->SetVerticalAlignment(VAlign_Fill);
		}

		USizeBox* ModalPanelSize = WidgetTree->ConstructWidget<USizeBox>(
			USizeBox::StaticClass(),
			TEXT("RouteAbandonModalPanelSize"));
		ModalPanelSize->SetWidthOverride(RouteAbandonModalSize.X);
		ModalPanelSize->SetHeightOverride(RouteAbandonModalSize.Y);
		UBorder* ModalPanel = WidgetTree->ConstructWidget<UBorder>(
			UBorder::StaticClass(),
			TEXT("RouteAbandonModalPanel"));
		if (UTexture2D* PaperTexture = LoadObject<UTexture2D>(nullptr, RouteModalPaperTexturePath))
		{
			ModalPanel->SetBrush(BuildRouteTextureBrush(
				PaperTexture,
				FVector2D(100.0f, 101.0f),
				FLinearColor::White,
				ESlateBrushDrawType::Box,
				FMargin(0.065f)));
		}
		ModalPanel->SetBrushColor(FLinearColor::White);
		ModalPanel->SetPadding(FMargin(38.0f, 30.0f, 38.0f, 26.0f));
		UVerticalBox* ModalBody = WidgetTree->ConstructWidget<UVerticalBox>(
			UVerticalBox::StaticClass(),
			TEXT("RouteAbandonModalBody"));
		ModalPanel->SetContent(ModalBody);

		auto AddModalText = [this, ModalBody](
			const FName Name,
			const FText& Text,
			const int32 FontSize,
			const FLinearColor& Color,
			const float BottomPadding,
			const bool bBold)
		{
			UTextBlock* TextBlock = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
			TextBlock->SetText(Text);
			TextBlock->SetAutoWrapText(true);
			TextBlock->SetJustification(ETextJustify::Center);
			TextBlock->SetColorAndOpacity(FSlateColor(Color));
			FSlateFontInfo Font = TextBlock->GetFont();
			Font.Size = FontSize;
			if (bBold) Font.TypefaceFontName = TEXT("Bold");
			TextBlock->SetFont(Font);
			if (UVerticalBoxSlot* TextSlot = ModalBody->AddChildToVerticalBox(TextBlock))
			{
				TextSlot->SetPadding(FMargin(0.0f, 0.0f, 0.0f, BottomPadding));
				TextSlot->SetHorizontalAlignment(HAlign_Fill);
			}
			return TextBlock;
		};
		AddModalText(
			TEXT("RouteAbandonModalTitle"),
			NSLOCTEXT("GameXXKRouteMap", "SettlementTitle", "本次路线结算"),
			28,
			FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
			14.0f,
			true);
		AddModalText(
			TEXT("RouteAbandonModalDescription"),
			NSLOCTEXT("GameXXKRouteMap", "SettlementDescription", "仅结算当前已获得的奖励；未解决与未访问节点不会计入。"),
			18,
			FLinearColor(0.12f, 0.09f, 0.06f, 1.0f),
			12.0f,
			false);
		RouteAbandonPreviewText = AddModalText(
			TEXT("RouteAbandonModalPreview"),
			FText::GetEmpty(),
			21,
			FLinearColor(0.15f, 0.28f, 0.22f, 1.0f),
			10.0f,
			true);
		RouteAbandonErrorText = AddModalText(
			TEXT("RouteAbandonModalError"),
			FText::GetEmpty(),
			16,
			FLinearColor(0.55f, 0.08f, 0.05f, 1.0f),
			14.0f,
			false);

		UHorizontalBox* ModalButtons = WidgetTree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("RouteAbandonModalButtons"));
		RouteAbandonConfirmButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("RouteAbandonConfirmButton"));
		StyleRouteActionButton(RouteAbandonConfirmButton, ActionTexture);
		UTextBlock* ConfirmLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("RouteAbandonConfirmLabel"));
		ConfirmLabel->SetText(NSLOCTEXT("GameXXKRouteMap", "SettlementConfirm", "确认结算并返回挂机"));
		ConfirmLabel->SetJustification(ETextJustify::Center);
		ConfirmLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RouteAbandonConfirmButton->AddChild(ConfirmLabel);
		RouteAbandonConfirmButton->OnClicked.AddDynamic(
			this,
			&UGameXXKOneGameRouteMapWidget::HandleRouteAbandonConfirmClicked);
		USizeBox* ConfirmSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RouteAbandonConfirmSize"));
		ConfirmSize->SetWidthOverride(260.0f);
		ConfirmSize->SetHeightOverride(58.0f);
		ConfirmSize->AddChild(RouteAbandonConfirmButton);
		if (UHorizontalBoxSlot* ConfirmSlot = ModalButtons->AddChildToHorizontalBox(ConfirmSize))
		{
			ConfirmSlot->SetPadding(FMargin(0.0f, 0.0f, 18.0f, 0.0f));
		}

		RouteAbandonCancelButton = WidgetTree->ConstructWidget<UButton>(
			UButton::StaticClass(),
			TEXT("RouteAbandonCancelButton"));
		StyleRouteActionButton(RouteAbandonCancelButton, ActionTexture);
		UTextBlock* CancelLabel = WidgetTree->ConstructWidget<UTextBlock>(
			UTextBlock::StaticClass(),
			TEXT("RouteAbandonCancelLabel"));
		CancelLabel->SetText(NSLOCTEXT("GameXXKRouteMap", "SettlementCancel", "继续路线"));
		CancelLabel->SetJustification(ETextJustify::Center);
		CancelLabel->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		RouteAbandonCancelButton->AddChild(CancelLabel);
		RouteAbandonCancelButton->OnClicked.AddDynamic(
			this,
			&UGameXXKOneGameRouteMapWidget::HandleRouteAbandonCancelClicked);
		USizeBox* CancelSize = WidgetTree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("RouteAbandonCancelSize"));
		CancelSize->SetWidthOverride(210.0f);
		CancelSize->SetHeightOverride(58.0f);
		CancelSize->AddChild(RouteAbandonCancelButton);
		ModalButtons->AddChildToHorizontalBox(CancelSize);
		if (UVerticalBoxSlot* ButtonsSlot = ModalBody->AddChildToVerticalBox(ModalButtons))
		{
			ButtonsSlot->SetHorizontalAlignment(HAlign_Center);
		}
		ModalPanelSize->AddChild(ModalPanel);
		if (UOverlaySlot* PanelSlot = RouteAbandonModalOverlay->AddChildToOverlay(ModalPanelSize))
		{
			PanelSlot->SetHorizontalAlignment(HAlign_Center);
			PanelSlot->SetVerticalAlignment(VAlign_Center);
		}
		if (UOverlaySlot* ModalSlot = RootOverlay->AddChildToOverlay(RouteAbandonModalOverlay))
		{
			ModalSlot->SetHorizontalAlignment(HAlign_Fill);
			ModalSlot->SetVerticalAlignment(VAlign_Fill);
		}
	}

	if (!RootCanvas || !RouteContentSizeBox)
	{
		return;
	}

	if (UScrollBoxSlot* ContentScrollSlot = Cast<UScrollBoxSlot>(RouteContentSizeBox->Slot))
	{
		ContentScrollSlot->SetHorizontalAlignment(HAlign_Center);
		ContentScrollSlot->SetVerticalAlignment(VAlign_Top);
	}

	RouteContentSizeBox->SetWidthOverride(RouteContentSize.X);
	RouteContentSizeBox->SetHeightOverride(RouteContentSize.Y);

	const bool bNeedsRebuildRouteChildren = bRouteLayoutWasCreated
		|| !RouteDragSurface
		|| LastBuiltRouteNodeCount != RouteNodeCount
		|| LastBuiltRouteLineCount != RouteLineCount
		|| !LastBuiltRouteContentSize.Equals(RouteContentSize, 0.1f)
		|| bLastBuiltUseOneGameBlueprintVisualWidgets != bUseOneGameBlueprintVisualWidgets;
	if (!bNeedsRebuildRouteChildren)
	{
		return;
	}

	RootCanvas->ClearChildren();
	RouteBackgroundImage = nullptr;
	RouteDragSurface = nullptr;

	RouteBackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GameXXKOneGameRouteMapBackground"));
	if (RouteBackgroundImage)
	{
		if (UTexture2D* BackgroundTexture = OneGameRouteBackgroundTexture.LoadSynchronous())
		{
			RouteBackgroundImage->SetBrushFromTexture(BackgroundTexture, true);
		}
		RouteBackgroundImage->SetColorAndOpacity(FLinearColor::White);
		RouteBackgroundImage->SetVisibility(ESlateVisibility::HitTestInvisible);
		if (UCanvasPanelSlot* BackgroundSlot = RootCanvas->AddChildToCanvas(RouteBackgroundImage))
		{
			BackgroundSlot->SetPosition(FVector2D::ZeroVector);
			BackgroundSlot->SetSize(RouteContentSize);
			BackgroundSlot->SetZOrder(0);
		}
	}

	RouteDragSurface = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		TEXT("GameXXKOneGameRouteMapDragSurface"));
	if (RouteDragSurface)
	{
		RouteDragSurface->SetPadding(FMargin(0.0f));
		RouteDragSurface->SetBrushColor(FLinearColor(1.0f, 1.0f, 1.0f, 0.01f));
		RouteDragSurface->SetVisibility(ESlateVisibility::Visible);
		RouteDragSurface->OnMouseButtonDownEvent.BindDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseButtonDown);
		RouteDragSurface->OnMouseButtonUpEvent.BindDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseButtonUp);
		RouteDragSurface->OnMouseMoveEvent.BindDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleRouteDragSurfaceMouseMove);
		if (UCanvasPanelSlot* DragSlot = RootCanvas->AddChildToCanvas(RouteDragSurface))
		{
			DragSlot->SetPosition(FVector2D::ZeroVector);
			DragSlot->SetSize(RouteContentSize);
			DragSlot->SetZOrder(1);
		}
	}

	LineVisualWidgets.Reset();
	LineVisualBorders.Reset();
	for (int32 LineIndex = 0; LineIndex < RouteLineCount; ++LineIndex)
	{
		if (UWidget* LineVisual = ConstructLineVisualWidget(LineIndex))
		{
			LineVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(LineVisual))
			{
				LineSlot->SetSize(FVector2D(128.0f, RouteLineThickness));
				LineSlot->SetZOrder(2);
			}
			LineVisualWidgets.Add(LineVisual);
		}
	}

	NodeVisualWidgets.Reset();
	NodeVisualBorders.Reset();
	NodeVisualLabels.Reset();
	NodeVisualImages.Reset();
	NodeVisualIconPaths.Reset();
	for (int32 NodeIndex = 0; NodeIndex < RouteNodeCount; ++NodeIndex)
	{
		if (UWidget* NodeVisual = ConstructNodeVisualWidget(NodeIndex))
		{
			NodeVisual->SetVisibility(ESlateVisibility::HitTestInvisible);
			if (UCanvasPanelSlot* NodeSlot = RootCanvas->AddChildToCanvas(NodeVisual))
			{
				NodeSlot->SetSize(FVector2D(96.0f, 96.0f));
				NodeSlot->SetZOrder(3);
			}
			NodeVisualWidgets.Add(NodeVisual);
		}
	}

	NodeButtons.Reset();
	NodeButtonLabels.Reset();
	NodeButtonIndices.Reset();
	for (int32 ButtonIndex = 0; ButtonIndex < RouteNodeCount; ++ButtonIndex)
	{
		UButton* NodeButton = WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), *FString::Printf(TEXT("RouteNodeButton%d"), ButtonIndex));
		UTextBlock* NodeLabel = WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *FString::Printf(TEXT("RouteNodeLabel%d"), ButtonIndex));
		const FSlateColorBrush TransparentNodeBrush(FLinearColor(1.0f, 1.0f, 1.0f, 0.01f));
		FButtonStyle TransparentButtonStyle;
		TransparentButtonStyle.SetNormal(TransparentNodeBrush);
		TransparentButtonStyle.SetHovered(TransparentNodeBrush);
		TransparentButtonStyle.SetPressed(TransparentNodeBrush);
		TransparentButtonStyle.SetDisabled(TransparentNodeBrush);
		NodeButton->SetStyle(TransparentButtonStyle);
		NodeButton->SetBackgroundColor(FLinearColor::Transparent);
		NodeButton->SetClickMethod(EButtonClickMethod::PreciseClick);
		NodeButton->SetTouchMethod(EButtonTouchMethod::PreciseTap);
		NodeButton->AddChild(NodeLabel);
		BindNodeButton(NodeButton, ButtonIndex);
		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(NodeButton))
		{
			CanvasSlot->SetSize(FVector2D(110.0f, 72.0f));
			CanvasSlot->SetZOrder(4);
		}

		NodeButtons.Add(NodeButton);
		NodeButtonLabels.Add(NodeLabel);
		NodeButtonIndices.Add(INDEX_NONE);
	}

	LastBuiltRouteNodeCount = RouteNodeCount;
	LastBuiltRouteLineCount = RouteLineCount;
	LastBuiltRouteContentSize = RouteContentSize;
	bLastBuiltUseOneGameBlueprintVisualWidgets = bUseOneGameBlueprintVisualWidgets;
}

void UGameXXKOneGameRouteMapWidget::ConfigureNodeButton(int32 ButtonIndex, const FGameXXKOneGameRouteNode* Node)
{
	if (!NodeButtons.IsValidIndex(ButtonIndex))
	{
		return;
	}

	UButton* Button = NodeButtons[ButtonIndex];
	UTextBlock* Label = NodeButtonLabels.IsValidIndex(ButtonIndex) ? NodeButtonLabels[ButtonIndex].Get() : nullptr;
	if (!Button)
	{
		return;
	}

	Button->SetVisibility(Node ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	Button->SetIsEnabled(Node && Node->bEnabled);
	Button->SetRenderOpacity(1.0f);
	if (NodeButtonIndices.IsValidIndex(ButtonIndex))
	{
		NodeButtonIndices[ButtonIndex] = Node ? Node->NodeIndex : INDEX_NONE;
	}
	if (Label)
	{
		Label->SetText(FText::GetEmpty());
	}

	if (UCanvasPanelSlot* CanvasSlot = Cast<UCanvasPanelSlot>(Button->Slot))
	{
		const FVector2D Position = Node ? GetNodeCanvasPosition(*Node) + FVector2D(-55.0f, -36.0f) : FVector2D::ZeroVector;
		CanvasSlot->SetPosition(Position);
	}

	if (NodeVisualWidgets.IsValidIndex(ButtonIndex))
	{
		UWidget* NodeVisual = NodeVisualWidgets[ButtonIndex];
		if (NodeVisual)
		{
			NodeVisual->SetVisibility(Node ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
			NodeVisual->SetRenderOpacity(Node && Node->bVisited ? 0.52f : 1.0f);
			if (UBorder* NodeBorder = NodeVisualBorders.IsValidIndex(ButtonIndex) ? NodeVisualBorders[ButtonIndex].Get() : nullptr)
			{
				NodeBorder->SetBrushColor(FLinearColor::Transparent);
			}
			if (UTextBlock* NodeLabel = NodeVisualLabels.IsValidIndex(ButtonIndex) ? NodeVisualLabels[ButtonIndex].Get() : nullptr)
			{
				NodeLabel->SetText(Node ? Node->Label : FText::GetEmpty());
			}
			if (UTextBlock* NodeText = Cast<UTextBlock>(NodeVisual))
			{
				NodeText->SetText(Node ? Node->Label : FText::GetEmpty());
			}
			if (UImage* NodeImage = NodeVisualImages.IsValidIndex(ButtonIndex) ? NodeVisualImages[ButtonIndex].Get() : nullptr)
			{
				if (Node)
				{
					const TSoftObjectPtr<UTexture2D> IconTexture = GetTextureForNode(*Node);
					NodeVisualIconPaths[ButtonIndex] = IconTexture.ToSoftObjectPath().ToString();
					if (UTexture2D* LoadedTexture = IconTexture.LoadSynchronous())
					{
						NodeImage->SetBrushFromTexture(LoadedTexture, true);
					}
					NodeImage->SetColorAndOpacity(Node->bEnabled || Node->bVisited
						? FLinearColor::White
						: FLinearColor(0.78f, 0.82f, 0.84f, 0.76f));
					NodeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
				}
				else
				{
					NodeVisualIconPaths[ButtonIndex].Reset();
					NodeImage->SetVisibility(ESlateVisibility::Collapsed);
				}
			}
			if (UCanvasPanelSlot* VisualSlot = Cast<UCanvasPanelSlot>(NodeVisual->Slot))
			{
				const FVector2D VisualPosition = Node ? GetNodeCanvasPosition(*Node) + FVector2D(-48.0f, -48.0f) : FVector2D::ZeroVector;
				VisualSlot->SetPosition(VisualPosition);
			}
		}
	}
}

void UGameXXKOneGameRouteMapWidget::ConfigureLineVisual(int32 LineIndex, const TArray<FGameXXKOneGameRouteNode>& Nodes)
{
	if (!LineVisualWidgets.IsValidIndex(LineIndex))
	{
		return;
	}

	UWidget* LineVisual = LineVisualWidgets[LineIndex];
	if (!LineVisual)
	{
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	const FGameXXKOneGameRouteNode* StartNode = nullptr;
	const FGameXXKOneGameRouteNode* EndNode = nullptr;
	const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
	if (State && State->bHasGeneratedRouteMap)
	{
		FGameXXKRouteMapEdge Edge;
		if (TryGetRenderedRouteEdge(LineIndex, Nodes, Edge))
		{
			StartNode = FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.FromNodeId);
			EndNode = FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.ToNodeId);
		}
	}
	else if (LineIndex >= 0 && LineIndex + 1 < RenderedNodeCount)
	{
		StartNode = &Nodes[LineIndex];
		EndNode = &Nodes[LineIndex + 1];
	}

	const bool bHasLine = StartNode && EndNode;
	LineVisual->SetVisibility(bHasLine ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	if (!bHasLine)
	{
		return;
	}

	const FVector2D Start = GetNodeCanvasPosition(*StartNode);
	const FVector2D End = GetNodeCanvasPosition(*EndNode);
	const FVector2D Delta = End - Start;
	const float Length = Delta.Size();
	const float Angle = FMath::RadiansToDegrees(FMath::Atan2(Delta.Y, Delta.X));
	if (UCanvasPanelSlot* LineSlot = Cast<UCanvasPanelSlot>(LineVisual->Slot))
	{
		LineSlot->SetPosition(Start + Delta * 0.5f + FVector2D(-Length * 0.5f, -RouteLineThickness * 0.5f));
		LineSlot->SetSize(FVector2D(FMath::Max(24.0f, Length), RouteLineThickness));
	}
	LineVisual->SetRenderTransformAngle(Angle);
	const bool bRouteLineIsOpen = StartNode->bVisited || StartNode->bEnabled;
	if (UImage* LineImage = Cast<UImage>(LineVisual))
	{
		FSlateBrush LineBrush = LineImage->GetBrush();
		if (const UTexture2D* RouteLineTexture = Cast<UTexture2D>(LineBrush.GetResourceObject()))
		{
			const int32 RouteLineTextureWidth = RouteLineTexture->GetSizeX() > 0
				? RouteLineTexture->GetSizeX()
				: RouteLineTexture->GetImportedSize().X;
			const float TextureWidth = static_cast<float>(RouteLineTextureWidth);
			const float VisibleSourceWidth = TextureWidth > 0.0f
				? FMath::Clamp(Length / TextureWidth, 0.0f, 1.0f)
				: 1.0f;
			LineBrush.SetImageSize(FVector2f(FMath::Max(24.0f, Length), RouteLineThickness));
			LineBrush.SetUVRegion(FBox2f(FVector2f(0.0f, 0.0f), FVector2f(VisibleSourceWidth, 1.0f)));
			LineImage->SetBrush(LineBrush);
		}
		LineImage->SetColorAndOpacity(RouteLineColor(bRouteLineIsOpen));
	}
	if (UBorder* LineBorder = LineVisualBorders.IsValidIndex(LineIndex) ? LineVisualBorders[LineIndex].Get() : nullptr)
	{
		LineBorder->SetBrushColor(RouteLineColor(bRouteLineIsOpen));
	}
	LineVisual->SetRenderOpacity(bRouteLineIsOpen ? 1.0f : 0.42f);
}

void UGameXXKOneGameRouteMapWidget::ExecuteNodeButtonAtIndex(int32 ButtonIndex)
{
	if (!NodeButtonIndices.IsValidIndex(ButtonIndex) || NodeButtonIndices[ButtonIndex] == INDEX_NONE)
	{
		return;
	}
	ExecuteRouteNodeById(NodeButtonIndices[ButtonIndex]);
}

bool UGameXXKOneGameRouteMapWidget::TryExecuteRouteNodeAtScreenPosition(const FVector2D& ScreenPosition)
{
	if (bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
	for (int32 NodeIndex = RenderedNodeCount - 1; NodeIndex >= 0; --NodeIndex)
	{
		if (!Nodes.IsValidIndex(NodeIndex) || !Nodes[NodeIndex].bEnabled)
		{
			continue;
		}

		const UButton* Button = NodeButtons.IsValidIndex(NodeIndex) ? NodeButtons[NodeIndex].Get() : nullptr;
		if (!Button || Button->GetVisibility() == ESlateVisibility::Collapsed)
		{
			continue;
		}

		const FGeometry& ButtonGeometry = Button->GetCachedGeometry();
		const FVector2D ButtonSize = ButtonGeometry.GetLocalSize();
		if (ButtonSize.X <= 0.0f || ButtonSize.Y <= 0.0f)
		{
			continue;
		}

		const FVector2D LocalPosition = ButtonGeometry.AbsoluteToLocal(ScreenPosition);
		if (LocalPosition.X >= 0.0f
			&& LocalPosition.Y >= 0.0f
			&& LocalPosition.X <= ButtonSize.X
			&& LocalPosition.Y <= ButtonSize.Y)
		{
			return ExecuteRouteNodeById(Nodes[NodeIndex].NodeIndex);
		}
	}

	return false;
}

void UGameXXKOneGameRouteMapWidget::BindNodeButton(UButton* Button, int32 ButtonIndex)
{
	if (!Button || ButtonIndex < 0 || ButtonIndex >= MaxRouteNodeButtons)
	{
		return;
	}

	// UMG dynamic delegates cannot carry a slot index through AddDynamic, so
	// the per-node UFUNCTION is bound by name; the 24-case switch is gone and
	// adding a node only needs a matching UFUNCTION.
	const FName HandlerName(*FString::Printf(TEXT("HandleNodeButton%dClicked"), ButtonIndex));
	FOnButtonClickedEvent::FDelegate Handler;
	Handler.BindUFunction(this, HandlerName);
	Button->OnClicked.Add(Handler);
}

UWidget* UGameXXKOneGameRouteMapWidget::ConstructNodeVisualWidget(int32 NodeIndex)
{
	NodeVisualBorders.Add(nullptr);
	NodeVisualLabels.Add(nullptr);
	NodeVisualImages.Add(nullptr);
	NodeVisualIconPaths.Add(FString());
	if (UWidget* OneGameVisual = ConstructOneGameVisualWidget(
		OneGameNodeWidgetClass,
		FString::Printf(TEXT("OneGameRouteNodeVisual%d"), NodeIndex)))
	{
		return OneGameVisual;
	}
	return ConstructFallbackNodeVisualWidget(NodeIndex);
}

UWidget* UGameXXKOneGameRouteMapWidget::ConstructLineVisualWidget(int32 LineIndex)
{
	LineVisualBorders.Add(nullptr);
	if (UWidget* OneGameVisual = ConstructOneGameVisualWidget(
		OneGameLineWidgetClass,
		FString::Printf(TEXT("OneGameRouteLineVisual%d"), LineIndex)))
	{
		return OneGameVisual;
	}
	return ConstructFallbackLineVisualWidget(LineIndex);
}

UWidget* UGameXXKOneGameRouteMapWidget::ConstructOneGameVisualWidget(TSoftClassPtr<UUserWidget>& WidgetClass, const FString& Name)
{
	if (!WidgetTree || WidgetClass.IsNull() || !bUseOneGameBlueprintVisualWidgets)
	{
		return nullptr;
	}

	UClass* LoadedClass = WidgetClass.LoadSynchronous();
	if (!LoadedClass || !LoadedClass->IsChildOf(UUserWidget::StaticClass()))
	{
		return nullptr;
	}

	return WidgetTree->ConstructWidget<UUserWidget>(LoadedClass, FName(*Name));
}

UWidget* UGameXXKOneGameRouteMapWidget::ConstructFallbackNodeVisualWidget(int32 NodeIndex)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UBorder* NodeBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		*FString::Printf(TEXT("RouteNodeFallbackVisual%d"), NodeIndex));
	UOverlay* NodeOverlay = WidgetTree->ConstructWidget<UOverlay>(
		UOverlay::StaticClass(),
		*FString::Printf(TEXT("RouteNodeFallbackOverlay%d"), NodeIndex));
	UImage* NodeImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("RouteNodeFallbackIcon%d"), NodeIndex));
	UTextBlock* NodeText = WidgetTree->ConstructWidget<UTextBlock>(
		UTextBlock::StaticClass(),
		*FString::Printf(TEXT("RouteNodeFallbackLabel%d"), NodeIndex));
	if (NodeBorder)
	{
		NodeBorder->SetPadding(FMargin(8.0f, 6.0f));
		NodeBorder->SetBrushColor(RouteNodeColor(nullptr));
	}
	if (NodeText)
	{
		NodeText->SetJustification(ETextJustify::Center);
		NodeText->SetText(FText::FromString(TEXT("Node")));
		NodeText->SetAutoWrapText(true);
		NodeText->SetColorAndOpacity(FSlateColor(FLinearColor::White));
		NodeText->SetFontSize(13);
	}
	if (NodeImage)
	{
		NodeImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	if (NodeOverlay && NodeImage)
	{
		if (UOverlaySlot* IconSlot = NodeOverlay->AddChildToOverlay(NodeImage))
		{
			IconSlot->SetHorizontalAlignment(HAlign_Center);
			IconSlot->SetVerticalAlignment(VAlign_Center);
		}
	}
	if (NodeOverlay && NodeText)
	{
		if (UOverlaySlot* LabelSlot = NodeOverlay->AddChildToOverlay(NodeText))
		{
			LabelSlot->SetHorizontalAlignment(HAlign_Fill);
			LabelSlot->SetVerticalAlignment(VAlign_Bottom);
		}
	}
	if (NodeBorder && NodeOverlay)
	{
		NodeBorder->SetContent(NodeOverlay);
	}
	if (NodeVisualBorders.IsValidIndex(NodeIndex))
	{
		NodeVisualBorders[NodeIndex] = NodeBorder;
	}
	if (NodeVisualLabels.IsValidIndex(NodeIndex))
	{
		NodeVisualLabels[NodeIndex] = NodeText;
	}
	if (NodeVisualImages.IsValidIndex(NodeIndex))
	{
		NodeVisualImages[NodeIndex] = NodeImage;
	}
	return NodeBorder;
}

UWidget* UGameXXKOneGameRouteMapWidget::ConstructFallbackLineVisualWidget(int32 LineIndex)
{
	if (!WidgetTree)
	{
		return nullptr;
	}

	UImage* LineImage = WidgetTree->ConstructWidget<UImage>(
		UImage::StaticClass(),
		*FString::Printf(TEXT("RouteLineFallbackVisual%d"), LineIndex));
	if (LineImage)
	{
		if (UTexture2D* RouteLineTexture = OneGameRouteLineTexture.LoadSynchronous())
		{
			LineImage->SetBrushFromTexture(RouteLineTexture, true);
			FSlateBrush LineBrush = LineImage->GetBrush();
			const FIntPoint ImportedSize = RouteLineTexture->GetImportedSize();
			LineBrush.SetImageSize(FVector2f(
				static_cast<float>(RouteLineTexture->GetSizeX() > 0 ? RouteLineTexture->GetSizeX() : ImportedSize.X),
				static_cast<float>(RouteLineTexture->GetSizeY() > 0 ? RouteLineTexture->GetSizeY() : ImportedSize.Y)));
			LineImage->SetBrush(LineBrush);
		}
		LineImage->SetColorAndOpacity(RouteLineColor(false));
		LineImage->SetVisibility(ESlateVisibility::HitTestInvisible);
	}
	return LineImage;
}

TSoftObjectPtr<UTexture2D> UGameXXKOneGameRouteMapWidget::GetTextureForNode(const FGameXXKOneGameRouteNode& Node) const
{
	const bool bDisabled = !Node.bEnabled && !Node.bVisited;
	switch (Node.RoomType)
	{
	case EGameXXKOneGameRouteRoomType::Start:
		return OneGameCampTexture;
	case EGameXXKOneGameRouteRoomType::SmallEnemy:
		return bDisabled ? OneGameBattleDisabledTexture : OneGameBattleTexture;
	case EGameXXKOneGameRouteRoomType::EliteEnemy:
	case EGameXXKOneGameRouteRoomType::Boss:
		return bDisabled ? OneGameEliteDisabledTexture : OneGameEliteTexture;
	case EGameXXKOneGameRouteRoomType::Camp:
		return bDisabled ? OneGameCampDisabledTexture : OneGameCampTexture;
	case EGameXXKOneGameRouteRoomType::Chest:
		return bDisabled ? OneGameChestDisabledTexture : OneGameChestTexture;
	case EGameXXKOneGameRouteRoomType::Merchant:
		return bDisabled ? OneGameMerchantDisabledTexture : OneGameMerchantTexture;
	case EGameXXKOneGameRouteRoomType::RandomEvent:
		return bDisabled ? OneGameEventDisabledTexture : OneGameEventTexture;
	default:
		return OneGameBattleTexture;
	}
}

FVector2D UGameXXKOneGameRouteMapWidget::GetNodeCanvasPosition(const FGameXXKOneGameRouteNode& Node) const
{
	const float CenteredLaneWidth = FMath::Min(
		RouteContentSize.X,
		FMath::Clamp(
			RouteContentSize.X * RouteCenteredLaneWidthFraction,
			RouteCenteredLaneMinWidth,
			RouteCenteredLaneMaxWidth));
	const float CenteredLaneLeft = (RouteContentSize.X - CenteredLaneWidth) * 0.5f;
	const float LanePadding = FMath::Min(RouteHorizontalPadding, CenteredLaneWidth * 0.2f);
	const float UsableWidth = FMath::Max(1.0f, CenteredLaneWidth - LanePadding * 2.0f);
	const float UsableHeight = FMath::Max(1.0f, RouteContentSize.Y - RouteTopPadding - RouteBottomPadding);
	return FVector2D(
		CenteredLaneLeft + LanePadding + Node.NormalizedPosition.X * UsableWidth,
		RouteTopPadding + (1.0f - Node.NormalizedPosition.Y) * UsableHeight);
}

FVector2D UGameXXKOneGameRouteMapWidget::CalculateRouteContentSize(const TArray<FGameXXKOneGameRouteNode>& Nodes) const
{
	int32 MaxLayerIndex = 0;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (State && State->bHasGeneratedRouteMap)
	{
		for (const FGameXXKRouteMapNode& RouteNode : State->RouteMapNodes)
		{
			MaxLayerIndex = FMath::Max(MaxLayerIndex, RouteNode.LayerIndex);
		}
	}
	else
	{
		MaxLayerIndex = FMath::Max(0, Nodes.Num() - 1);
	}

	const double DynamicHeight = RouteTopPadding + RouteBottomPadding + static_cast<double>(MaxLayerIndex + 1) * RouteLayerGap;
	const double ViewportDrivenHeight = RouteMapViewportSize.Y * RouteViewportHeightContentMultiplier;
	return FVector2D(
		FMath::Max(MinimumRouteContentSize.X, RouteMapViewportSize.X),
		FMath::Max3(MinimumRouteContentSize.Y, DynamicHeight, ViewportDrivenHeight));
}

int32 UGameXXKOneGameRouteMapWidget::GetRenderedRouteNodeCount(const TArray<FGameXXKOneGameRouteNode>& Nodes) const
{
	return FMath::Clamp(Nodes.Num(), 0, MaxRouteNodeButtons);
}

int32 UGameXXKOneGameRouteMapWidget::GetRenderedRouteLineCount(const TArray<FGameXXKOneGameRouteNode>& Nodes) const
{
	if (bUsingTransientRouteProjection)
	{
		int32 RenderedLineCount = 0;
		const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
		for (const FGameXXKRouteMapEdge& Edge : TransientRouteEdges)
		{
			if (FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.FromNodeId)
				&& FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.ToNodeId))
			{
				++RenderedLineCount;
			}
		}
		return RenderedLineCount;
	}
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || !State->bHasGeneratedRouteMap)
	{
		return FMath::Max(0, GetRenderedRouteNodeCount(Nodes) - 1);
	}

	int32 RenderedLineCount = 0;
	const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
	for (const FGameXXKRouteMapEdge& Edge : State->RouteMapEdges)
	{
		if (FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.FromNodeId)
			&& FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.ToNodeId))
		{
			++RenderedLineCount;
		}
	}
	return RenderedLineCount;
}

bool UGameXXKOneGameRouteMapWidget::TryGetRenderedRouteEdge(
	int32 LineIndex,
	const TArray<FGameXXKOneGameRouteNode>& Nodes,
	FGameXXKRouteMapEdge& OutEdge) const
{
	if (LineIndex < 0)
	{
		return false;
	}
	if (bUsingTransientRouteProjection)
	{
		int32 RenderedLineIndex = 0;
		const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
		for (const FGameXXKRouteMapEdge& Edge : TransientRouteEdges)
		{
			if (!FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.FromNodeId)
				|| !FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.ToNodeId))
			{
				continue;
			}
			if (RenderedLineIndex == LineIndex)
			{
				OutEdge = Edge;
				return true;
			}
			++RenderedLineIndex;
		}
		return false;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;
	if (!State || !State->bHasGeneratedRouteMap)
	{
		return false;
	}

	int32 RenderedLineIndex = 0;
	const int32 RenderedNodeCount = GetRenderedRouteNodeCount(Nodes);
	for (const FGameXXKRouteMapEdge& Edge : State->RouteMapEdges)
	{
		if (!FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.FromNodeId)
			|| !FindRenderedRouteNodeById(Nodes, RenderedNodeCount, Edge.ToNodeId))
		{
			continue;
		}

		if (RenderedLineIndex == LineIndex)
		{
			OutEdge = Edge;
			return true;
		}
		++RenderedLineIndex;
	}
	return false;
}

void UGameXXKOneGameRouteMapWidget::ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes)
{
	if (GetRenderedRouteNodeCount(Nodes) <= 0)
	{
		SetRouteScrollOffset(0.0f);
		return;
	}

	SetRouteScrollOffset(CalculateMaxScrollOffset());
}

float UGameXXKOneGameRouteMapWidget::CalculateMaxScrollOffset() const
{
	if (RouteScrollBox
		&& RouteScrollBox->GetCachedWidget().IsValid()
		&& RouteScrollBox->GetCachedGeometry().GetLocalSize().Y > 0.0f)
	{
		return FMath::Max(0.0f, RouteScrollBox->GetScrollOffsetOfEnd());
	}
	const float EffectiveViewportHeight = RouteMapViewportSize.Y > 0.0f ? RouteMapViewportSize.Y : DefaultRouteViewportHeight;
	return FMath::Max(0.0f, RouteContentSize.Y - EffectiveViewportHeight);
}

void UGameXXKOneGameRouteMapWidget::SetRouteScrollOffset(float NewScrollOffset)
{
	LastAppliedScrollOffset = FMath::Clamp(NewScrollOffset, 0.0f, CalculateMaxScrollOffset());
	if (RouteScrollBox)
	{
		RouteScrollBox->SetScrollOffset(LastAppliedScrollOffset);
	}
}

bool UGameXXKOneGameRouteMapWidget::IsRouteScrollBarVisibleForTest() const
{
	return RouteScrollBox && RouteScrollBox->GetScrollBarVisibility() == ESlateVisibility::Visible;
}

float UGameXXKOneGameRouteMapWidget::GetMaxScrollOffsetForTest() const
{
	return CalculateMaxScrollOffset();
}

bool UGameXXKOneGameRouteMapWidget::HasRouteDragSurfaceForTest() const
{
	return RouteDragSurface && RouteDragSurface->GetVisibility() == ESlateVisibility::Visible;
}

bool UGameXXKOneGameRouteMapWidget::ApplyRouteMapDragDeltaForTest(float PointerDeltaY)
{
	if (bRouteAbandonConfirmationOpen)
	{
		return false;
	}
	const float PreviousScrollOffset = LastAppliedScrollOffset;
	SetRouteScrollOffset(LastAppliedScrollOffset - PointerDeltaY);
	return !FMath::IsNearlyEqual(PreviousScrollOffset, LastAppliedScrollOffset);
}

EGameXXKOneGameRouteRoomType UGameXXKOneGameRouteMapWidget::MapRoomType(EGameXXKNodeKind NodeKind)
{
	switch (NodeKind)
	{
	case EGameXXKNodeKind::Start:
		return EGameXXKOneGameRouteRoomType::Start;
	case EGameXXKNodeKind::Battle:
		return EGameXXKOneGameRouteRoomType::SmallEnemy;
	case EGameXXKNodeKind::Elite:
		return EGameXXKOneGameRouteRoomType::EliteEnemy;
	case EGameXXKNodeKind::Event:
		return EGameXXKOneGameRouteRoomType::RandomEvent;
	case EGameXXKNodeKind::Camp:
		return EGameXXKOneGameRouteRoomType::Camp;
	case EGameXXKNodeKind::Chest:
		return EGameXXKOneGameRouteRoomType::Chest;
	case EGameXXKNodeKind::Merchant:
		return EGameXXKOneGameRouteRoomType::Merchant;
	case EGameXXKNodeKind::Boss:
		return EGameXXKOneGameRouteRoomType::Boss;
	default:
		return EGameXXKOneGameRouteRoomType::SmallEnemy;
	}
}
