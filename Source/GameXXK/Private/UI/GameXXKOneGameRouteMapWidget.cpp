#include "UI/GameXXKOneGameRouteMapWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ContentWidget.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKMVPCommandRouter.h"

namespace
{
	static constexpr int32 MaxRouteNodeButtons = 12;
	static const TCHAR* OneGameRouteWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择.UI_地图选择_C");
	static const TCHAR* OneGameNodeWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-关卡.UI_地图选择-关卡_C");
	static const TCHAR* OneGameLineWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-关卡-线.UI_地图选择-关卡-线_C");
	static const TCHAR* OneGameBossWidgetClassPath = TEXT("/Game/1Game/UI/UI_地图选择-Boss.UI_地图选择-Boss_C");
	static const TCHAR* OneGameStartTexturePath = TEXT("/Game/1Game/Texture/脚印.脚印");
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
	const FVector2D MinimumRouteContentSize(640.0f, 1040.0f);
	const float RouteHorizontalPadding = 96.0f;
	const float RouteTopPadding = 96.0f;
	const float RouteBottomPadding = 128.0f;
	const float RouteLayerGap = 180.0f;
	const float RouteInitialScrollTopPadding = 120.0f;

	FText RoomTypeLabel(EGameXXKOneGameRouteRoomType RoomType)
	{
		switch (RoomType)
		{
		case EGameXXKOneGameRouteRoomType::Start:
			return FText::FromString(TEXT("Start"));
		case EGameXXKOneGameRouteRoomType::SmallEnemy:
			return FText::FromString(TEXT("Battle"));
		case EGameXXKOneGameRouteRoomType::EliteEnemy:
			return FText::FromString(TEXT("Elite"));
		case EGameXXKOneGameRouteRoomType::Camp:
			return FText::FromString(TEXT("Camp"));
		case EGameXXKOneGameRouteRoomType::Chest:
			return FText::FromString(TEXT("Chest"));
		case EGameXXKOneGameRouteRoomType::Merchant:
			return FText::FromString(TEXT("Merchant"));
		case EGameXXKOneGameRouteRoomType::RandomEvent:
			return FText::FromString(TEXT("Event"));
		case EGameXXKOneGameRouteRoomType::Boss:
			return FText::FromString(TEXT("Boss"));
		default:
			return FText::FromString(TEXT("Node"));
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
}

UGameXXKOneGameRouteMapWidget::UGameXXKOneGameRouteMapWidget()
{
	OneGameRouteWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameRouteWidgetClassPath));
	OneGameNodeWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameNodeWidgetClassPath));
	OneGameLineWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameLineWidgetClassPath));
	OneGameBossWidgetClass = TSoftClassPtr<UUserWidget>(FSoftObjectPath(OneGameBossWidgetClassPath));
	OneGameStartTexture = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(OneGameStartTexturePath));
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
	return Super::RebuildWidget();
}

void UGameXXKOneGameRouteMapWidget::NativeConstruct()
{
	Super::NativeConstruct();
	BuildProgrammaticLayout();
	RefreshFromState();
}

void UGameXXKOneGameRouteMapWidget::RefreshFromState()
{
	BuildProgrammaticLayout();
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
	SetVisibility(Subsystem && Subsystem->GetRuntimeState().Screen == EGameXXKScreen::DungeonMap
		? ESlateVisibility::Visible
		: ESlateVisibility::Collapsed);
	ApplyInitialScrollOffset(Nodes);
}

TArray<FGameXXKOneGameRouteNode> UGameXXKOneGameRouteMapWidget::BuildAdapterNodes() const
{
	TArray<FGameXXKOneGameRouteNode> AdapterNodes;
	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKMVPRouteNodeDescriptor> RouteNodes = GameXXKMVPCommandRouter::BuildRouteMapNodes(Subsystem);
	const FGameXXKRuntimeState* State = Subsystem ? &Subsystem->GetRuntimeState() : nullptr;

	for (const FGameXXKMVPRouteNodeDescriptor& RouteNode : RouteNodes)
	{
		FGameXXKOneGameRouteNode AdapterNode;
		AdapterNode.CommandName = RouteNode.CommandName;
		AdapterNode.Label = RouteNode.Label.IsEmpty() ? RoomTypeLabel(MapRoomType(RouteNode.NodeKind)) : RouteNode.Label;
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
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	if (!Nodes.IsValidIndex(NodeIndex))
	{
		return false;
	}

	return ExecuteRouteNodeById(Nodes[NodeIndex].NodeIndex);
}

bool UGameXXKOneGameRouteMapWidget::ExecuteRouteNodeById(int32 NodeId)
{
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
		OnRouteNodeExecuted(Node);
		if (!NotifyPlayerFlowStateChanged())
		{
			RefreshFromState();
		}
	}
	return bExecuted;
}

TArray<FGameXXKOneGameRouteNodeVisualState> UGameXXKOneGameRouteMapWidget::GetRouteNodeVisualStatesForTest() const
{
	TArray<FGameXXKOneGameRouteNodeVisualState> VisualStates;
	const TArray<FGameXXKOneGameRouteNode> Nodes = BuildAdapterNodes();
	VisualStates.Reserve(Nodes.Num());
	for (int32 VisualIndex = 0; VisualIndex < Nodes.Num(); ++VisualIndex)
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
		if (NodeButtons.IsValidIndex(VisualIndex) && NodeButtons[VisualIndex])
		{
			const FGeometry ButtonGeometry = NodeButtons[VisualIndex]->GetCachedGeometry();
			const FVector2D GeometrySize = ButtonGeometry.GetLocalSize();
			if (GeometrySize.X > 0.0f && GeometrySize.Y > 0.0f)
			{
				VisualState.HitBoxSize = GeometrySize;
				VisualState.ViewportHitBoxPosition = ButtonGeometry.LocalToAbsolute(FVector2D::ZeroVector);
				VisualState.ViewportHitBoxCenter = ButtonGeometry.LocalToAbsolute(GeometrySize * 0.5f);
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
	return !OneGameStartTexture.IsNull()
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

void UGameXXKOneGameRouteMapWidget::BuildProgrammaticLayout()
{
	if (!WidgetTree)
	{
		WidgetTree = NewObject<UWidgetTree>(this, TEXT("OneGameRouteMapWidgetTree"));
	}
	if (!WidgetTree)
	{
		return;
	}

	const UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	const TArray<FGameXXKOneGameRouteNode> AdapterNodes = BuildAdapterNodes();
	RouteContentSize = CalculateRouteContentSize(AdapterNodes);
	const int32 RouteNodeCount = FMath::Clamp(AdapterNodes.Num(), 0, MaxRouteNodeButtons);
	const int32 RouteLineCount = Subsystem && Subsystem->GetRuntimeState().bHasGeneratedRouteMap
		? Subsystem->GetRuntimeState().RouteMapEdges.Num()
		: FMath::Max(0, RouteNodeCount - 1);

	if (!RootOverlay || WidgetTree->RootWidget != RootOverlay)
	{
		RootOverlay = WidgetTree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("GameXXKOneGameRouteMapRoot"));
		WidgetTree->RootWidget = RootOverlay;

		RouteBackgroundImage = WidgetTree->ConstructWidget<UImage>(UImage::StaticClass(), TEXT("GameXXKOneGameRouteMapBackground"));
		if (UTexture2D* BackgroundTexture = OneGameRouteBackgroundTexture.LoadSynchronous())
		{
			RouteBackgroundImage->SetBrushFromTexture(BackgroundTexture, true);
		}
		RouteBackgroundImage->SetColorAndOpacity(FLinearColor::White);
		if (UOverlaySlot* BackgroundSlot = RootOverlay->AddChildToOverlay(RouteBackgroundImage))
		{
			BackgroundSlot->SetHorizontalAlignment(HAlign_Fill);
			BackgroundSlot->SetVerticalAlignment(VAlign_Fill);
		}

		RouteScrollBox = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("GameXXKOneGameRouteMapScroll"));
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
	else if (RootCanvas)
	{
		RootCanvas->ClearChildren();
	}

	if (!RootCanvas || !RouteContentSizeBox)
	{
		return;
	}

	RouteContentSizeBox->SetWidthOverride(RouteContentSize.X);
	RouteContentSizeBox->SetHeightOverride(RouteContentSize.Y);

	LineVisualWidgets.Reset();
	LineVisualBorders.Reset();
	for (int32 LineIndex = 0; LineIndex < RouteLineCount; ++LineIndex)
	{
		if (UWidget* LineVisual = ConstructLineVisualWidget(LineIndex))
		{
			if (UCanvasPanelSlot* LineSlot = RootCanvas->AddChildToCanvas(LineVisual))
			{
				LineSlot->SetSize(FVector2D(128.0f, 18.0f));
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
			if (UCanvasPanelSlot* NodeSlot = RootCanvas->AddChildToCanvas(NodeVisual))
			{
				NodeSlot->SetSize(FVector2D(96.0f, 96.0f));
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
		NodeButton->AddChild(NodeLabel);
		BindNodeButton(NodeButton, ButtonIndex);
		if (UCanvasPanelSlot* CanvasSlot = RootCanvas->AddChildToCanvas(NodeButton))
		{
			CanvasSlot->SetSize(FVector2D(110.0f, 72.0f));
		}

		NodeButtons.Add(NodeButton);
		NodeButtonLabels.Add(NodeLabel);
		NodeButtonIndices.Add(INDEX_NONE);
	}
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
	Button->SetRenderOpacity(Node && Node->bEnabled ? 0.08f : 0.0f);
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
			NodeVisual->SetVisibility(Node ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
			NodeVisual->SetRenderOpacity(Node && Node->bVisited ? 0.52f : 1.0f);
			if (UBorder* NodeBorder = NodeVisualBorders.IsValidIndex(ButtonIndex) ? NodeVisualBorders[ButtonIndex].Get() : nullptr)
			{
				NodeBorder->SetBrushColor(RouteNodeColor(Node));
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
	if (State && State->bHasGeneratedRouteMap && State->RouteMapEdges.IsValidIndex(LineIndex))
	{
		const FGameXXKRouteMapEdge& Edge = State->RouteMapEdges[LineIndex];
		StartNode = Nodes.FindByPredicate([&Edge](const FGameXXKOneGameRouteNode& Node)
		{
			return Node.NodeIndex == Edge.FromNodeId;
		});
		EndNode = Nodes.FindByPredicate([&Edge](const FGameXXKOneGameRouteNode& Node)
		{
			return Node.NodeIndex == Edge.ToNodeId;
		});
	}
	else if (Nodes.IsValidIndex(LineIndex) && Nodes.IsValidIndex(LineIndex + 1))
	{
		StartNode = &Nodes[LineIndex];
		EndNode = &Nodes[LineIndex + 1];
	}

	const bool bHasLine = StartNode && EndNode;
	LineVisual->SetVisibility(bHasLine ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
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
		LineSlot->SetPosition(Start + Delta * 0.5f + FVector2D(-Length * 0.5f, -9.0f));
		LineSlot->SetSize(FVector2D(FMath::Max(24.0f, Length), 10.0f));
	}
	LineVisual->SetRenderTransformAngle(Angle);
	const bool bRouteLineIsOpen = StartNode->bVisited || StartNode->bEnabled;
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

void UGameXXKOneGameRouteMapWidget::BindNodeButton(UButton* Button, int32 ButtonIndex)
{
	if (!Button)
	{
		return;
	}

	switch (ButtonIndex)
	{
	case 0:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton0Clicked);
		break;
	case 1:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton1Clicked);
		break;
	case 2:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton2Clicked);
		break;
	case 3:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton3Clicked);
		break;
	case 4:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton4Clicked);
		break;
	case 5:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton5Clicked);
		break;
	case 6:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton6Clicked);
		break;
	case 7:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton7Clicked);
		break;
	case 8:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton8Clicked);
		break;
	case 9:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton9Clicked);
		break;
	case 10:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton10Clicked);
		break;
	case 11:
		Button->OnClicked.AddDynamic(this, &UGameXXKOneGameRouteMapWidget::HandleNodeButton11Clicked);
		break;
	default:
		break;
	}
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

	UBorder* LineBorder = WidgetTree->ConstructWidget<UBorder>(
		UBorder::StaticClass(),
		*FString::Printf(TEXT("RouteLineFallbackVisual%d"), LineIndex));
	if (LineBorder)
	{
		LineBorder->SetBrushColor(RouteLineColor(false));
		LineBorder->SetPadding(FMargin(0.0f));
	}
	if (LineVisualBorders.IsValidIndex(LineIndex))
	{
		LineVisualBorders[LineIndex] = LineBorder;
	}
	return LineBorder;
}

TSoftObjectPtr<UTexture2D> UGameXXKOneGameRouteMapWidget::GetTextureForNode(const FGameXXKOneGameRouteNode& Node) const
{
	const bool bDisabled = !Node.bEnabled && !Node.bVisited;
	switch (Node.RoomType)
	{
	case EGameXXKOneGameRouteRoomType::Start:
		return OneGameStartTexture;
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
	const float UsableWidth = FMath::Max(1.0f, RouteContentSize.X - RouteHorizontalPadding * 2.0f);
	const float UsableHeight = FMath::Max(1.0f, RouteContentSize.Y - RouteTopPadding - RouteBottomPadding);
	return FVector2D(
		RouteHorizontalPadding + Node.NormalizedPosition.X * UsableWidth,
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

	const float DynamicHeight = RouteTopPadding + RouteBottomPadding + static_cast<float>(MaxLayerIndex + 1) * RouteLayerGap;
	return FVector2D(
		FMath::Max(MinimumRouteContentSize.X, RouteMapViewportSize.X),
		FMath::Max(MinimumRouteContentSize.Y, DynamicHeight));
}

void UGameXXKOneGameRouteMapWidget::ApplyInitialScrollOffset(const TArray<FGameXXKOneGameRouteNode>& Nodes)
{
	const FGameXXKOneGameRouteNode* TargetNode = Nodes.FindByPredicate([](const FGameXXKOneGameRouteNode& Node)
	{
		return Node.bEnabled;
	});
	if (!TargetNode)
	{
		TargetNode = Nodes.FindByPredicate([](const FGameXXKOneGameRouteNode& Node)
		{
			return Node.bVisited;
		});
	}
	if (!TargetNode && !Nodes.IsEmpty())
	{
		TargetNode = &Nodes[0];
	}

	if (!TargetNode)
	{
		LastAppliedScrollOffset = 0.0f;
		return;
	}

	const FVector2D NodePosition = GetNodeCanvasPosition(*TargetNode);
	LastAppliedScrollOffset = FMath::Max(0.0f, NodePosition.Y - RouteInitialScrollTopPadding);
	if (RouteScrollBox)
	{
		RouteScrollBox->SetScrollOffset(LastAppliedScrollOffset);
	}
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
