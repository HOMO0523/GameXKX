#include "UI/GameXXKMainStoryPanelWidget.h"
#include "Audio/GameXXKSfx.h"
#include "UI/GameXXKAsyncStoryImage.h"
#include "UI/GameXXKDesktopPaperStyle.h"
#include "UI/GameXXKLocalization.h"

#include "Narrative/GameXXKMainStorySubsystem.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKPartyDeckUiStyle.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/ButtonSlot.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/ScaleBox.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Rendering/DrawElements.h"
#include "InputCoreTypes.h"
#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
	const FVector2D PanelSize(945, 533);
	void Place(UCanvasPanel* Canvas, UWidget* Widget, FVector2D Position, FVector2D Size)
	{
		if (UCanvasPanelSlot* Slot = Canvas->AddChildToCanvas(Widget)) { Slot->SetPosition(Position); Slot->SetSize(Size); }
	}
	UTextBlock* Label(UWidgetTree* Tree, const FText& Text, int32 Size, bool Bold = false, EGameXXKFontRole Role = EGameXXKFontRole::Body)
	{
		auto* Result = Tree->ConstructWidget<UTextBlock>();
		Result->SetText(GameXXKLocalization::Localize(Text)); Result->SetFont(FGameXXKInRunUiStyle::Font(Role, Size, Bold));
		Result->SetColorAndOpacity(FSlateColor(FGameXXKInRunUiStyle::Ink()));
		Result->SetAutoWrapText(true); Result->SetVisibility(ESlateVisibility::HitTestInvisible);
		return Result;
	}
	FString StateLabel(EGameXXKTaskState State)
	{
		switch (State)
		{
		case EGameXXKTaskState::Available: return TEXT("可进行");
		case EGameXXKTaskState::Active: return TEXT("进行中");
		case EGameXXKTaskState::Completed: return TEXT("待领奖");
		case EGameXXKTaskState::Rewarded: return TEXT("已领奖");
		default: return TEXT("未开放");
		}
	}
}

void UGameXXKMainStoryActionButton::Configure(UGameXXKMainStoryPanelWidget* InOwner, int32 InAction, FName InNode)
{
	OwnerPanel = InOwner; Action = InAction; NodeId = InNode;
	OnClicked.RemoveAll(this); OnClicked.AddDynamic(this, &UGameXXKMainStoryActionButton::Clicked);
}
void UGameXXKMainStoryActionButton::Clicked() { if (OwnerPanel) OwnerPanel->HandleAction(Action, NodeId); }

void UGameXXKMainStoryGraphWidget::SetContext(UGameXXKMainStoryPanelWidget* InPanel, UGameXXKMainStorySubsystem* InStory, FName InChapter, float InTextScale)
{
	Panel = InPanel; Story = InStory; ChapterId = InChapter; TextScale = InTextScale;
}
TSharedRef<SWidget> UGameXXKMainStoryGraphWidget::RebuildWidget()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("StoryGraphTree"));
	BuildGraph(); return Super::RebuildWidget();
}
void UGameXXKMainStoryGraphWidget::BuildGraph()
{
	Connectors.Reset(); ConnectorColors.Reset();
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), MakeUniqueObjectName(WidgetTree,UCanvasPanel::StaticClass(),TEXT("StoryGraphCanvas")));
	auto* Size = WidgetTree->ConstructWidget<USizeBox>(); Size->SetContent(Canvas); WidgetTree->RootWidget=Size;
	const auto* Chapter=FGameXXKMainStoryCatalog::FindChapter(ChapterId);
	const auto* State=Story?Story->State():nullptr;
	if(!Chapter || !State){Size->SetWidthOverride(880);Size->SetHeightOverride(380);return;}
	TMap<FName,int32> Depths;
	for(int32 Pass=0;Pass<Chapter->Nodes.Num();++Pass) for(FName Id:Chapter->Nodes)
	{
		const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N)continue;
		int32 Depth=0;
		for(FName Parent:N->RequiresAll)Depth=FMath::Max(Depth,Depths.FindRef(Parent)+1);
		for(FName Parent:N->RequiresAny)Depth=FMath::Max(Depth,Depths.FindRef(Parent)+1);
		Depths.Add(Id,Depth);
	}
	const FVector2D CardSize(254,164);
	TMap<FName,FVector2D> Positions;
	TMap<int32,int32> Rows;
	TArray<FName> Ordered=Chapter->Nodes;
	Ordered.StableSort([](const FName A,const FName B)
	{
		return !FGameXXKMainStoryCatalog::FindNode(A)->bOptional && FGameXXKMainStoryCatalog::FindNode(B)->bOptional;
	});
	int32 MaxDepth=0;
	for(FName Id:Ordered)
	{
		const int32 Depth=Depths.FindRef(Id);const int32 Row=Rows.FindOrAdd(Depth)++;
		Positions.Add(Id,FVector2D(22+Depth*306,34+Row*190));MaxDepth=FMath::Max(MaxDepth,Depth);
	}
	for(FName Id:Chapter->Nodes)
	{
		const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N)continue;
		TArray<FName> Parents=N->RequiresAll;Parents.Append(N->RequiresAny);
		const auto Status=FGameXXKMainStoryRules::NodeState(*State,Id);
		for(FName Parent:Parents)
		{
			if(!Positions.Contains(Parent))continue;
			const FVector2D Start=Positions[Parent]+FVector2D(CardSize.X,CardSize.Y*.5f);
			const FVector2D End=Positions[Id]+FVector2D(0,CardSize.Y*.5f);
			const float MidX=Start.X+(End.X-Start.X)*.5f;
			Connectors.Add({Start,FVector2D(MidX,Start.Y),FVector2D(MidX,End.Y),End});
			FLinearColor Color=Status==EGameXXKTaskState::Locked?FGameXXKInRunUiStyle::MutedInk():FGameXXKInRunUiStyle::Jade();
			Color.A=Status==EGameXXKTaskState::Locked?.32f:.88f;ConnectorColors.Add(Color);
		}
	}
	for(FName Id:Chapter->Nodes)
	{
		const auto* N=FGameXXKMainStoryCatalog::FindNode(Id);if(!N)continue;
		const auto Status=FGameXXKMainStoryRules::NodeState(*State,Id);
		auto* Button=WidgetTree->ConstructWidget<UGameXXKMainStoryActionButton>(UGameXXKMainStoryActionButton::StaticClass(),MakeUniqueObjectName(WidgetTree,UGameXXKMainStoryActionButton::StaticClass(),FName(*(TEXT("StoryNode_")+Id.ToString()))));
		Button->Configure(Panel,100,Id);
		FSlateBrush None;None.DrawAs=ESlateBrushDrawType::NoDrawType;FButtonStyle CardStyle; FGameXXKSfx::SetButtonSound(CardStyle);CardStyle.SetNormal(None);CardStyle.SetHovered(None);CardStyle.SetPressed(None);CardStyle.SetNormalPadding(FMargin(0));CardStyle.SetPressedPadding(FMargin(0,1,0,-1));Button->SetStyle(CardStyle);
		auto* Card=WidgetTree->ConstructWidget<UCanvasPanel>();
		auto* Art=WidgetTree->ConstructWidget<UGameXXKAsyncStoryImage>();Art->SetSoftEdges(true);Art->SetStoryTexture(N->Illustration);
		Art->SetVisibility(ESlateVisibility::HitTestInvisible);Art->SetRenderOpacity(Status==EGameXXKTaskState::Locked?.42f:1.f);
		Place(Card,Art,FVector2D(4,4),FVector2D(246,82));
		const bool bEnglish=GameXXKLocalization::IsEnglish();
		auto* Title=Label(WidgetTree,GameXXKLocalization::Compact(N->Title),FMath::RoundToInt((bEnglish?17:21)*FMath::Min(TextScale,1.15f)),true,EGameXXKFontRole::Title);
		Title->SetAutoWrapText(false);Title->SetJustification(ETextJustify::Center);
		if(bEnglish)
		{
			auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);Fit->SetStretchDirection(EStretchDirection::DownOnly);Fit->SetContent(Title);
			Place(Card,Fit,FVector2D(10,95),FVector2D(234,32));
		}
		else Place(Card,Title,FVector2D(10,95),FVector2D(234,32));
		Button->SetToolTipText(GameXXKLocalization::Localize(N->Title));
		const FString Badge=(N->bOptional?TEXT("支线 · "):TEXT(""))+StateLabel(Status);
		auto* StateText=Label(WidgetTree,FText::FromString(Badge),16);
		StateText->SetAutoWrapText(false);StateText->SetJustification(ETextJustify::Center);
		StateText->SetColorAndOpacity(FSlateColor(Status==EGameXXKTaskState::Completed?FGameXXKInRunUiStyle::Vermilion():FGameXXKInRunUiStyle::MutedInk()));
		Place(Card,StateText,FVector2D(10,132),FVector2D(234,22));
		auto* Accent=WidgetTree->ConstructWidget<UBorder>();
		FLinearColor Color=Status==EGameXXKTaskState::Available || Status==EGameXXKTaskState::Active?FGameXXKInRunUiStyle::Jade():FGameXXKInRunUiStyle::MutedInk();Color.A=Status==EGameXXKTaskState::Locked?.2f:.8f;
		Accent->SetBrushColor(Color);Place(Card,Accent,FVector2D(18,158),FVector2D(218,3));
		Card->SetVisibility(ESlateVisibility::HitTestInvisible);Button->SetContent(Card);
		if(auto* ContentSlot=Cast<UButtonSlot>(Card->Slot)){ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Fill);ContentSlot->SetPadding(FMargin(0));}
		Place(Canvas,Button,Positions[Id],CardSize);
	}
	Size->SetWidthOverride(FMath::Max(880.f,44+MaxDepth*306+CardSize.X));Size->SetHeightOverride(392);
}
void UGameXXKMainStoryGraphWidget::NativeDestruct()
{
	if(WidgetTree)WidgetTree->ForEachWidget([](UWidget* Widget){if(auto* Art=Cast<UGameXXKAsyncStoryImage>(Widget))Art->ClearStoryTexture();});
	Super::NativeDestruct();
}
int32 UGameXXKMainStoryGraphWidget::NativePaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling,
	FSlateWindowElementList& Elements, int32 Layer, const FWidgetStyle& Style, bool Enabled) const
{
	const int32 Result = Super::NativePaint(Args, Geometry, Culling, Elements, Layer, Style, Enabled);
	for (int32 Index=0;Index<Connectors.Num();++Index)
		FSlateDrawElement::MakeLines(Elements, Result+1, Geometry.ToPaintGeometry(), Connectors[Index], ESlateDrawEffect::None, ConnectorColors[Index], true, 4.5f);
	return Result+1;
}

void UGameXXKMainStoryPanelWidget::SetContext(UGameXXKMainStorySubsystem* InStory, FName InChapter, bool bInFullscreen)
{
	const bool bChapterChanged=ChapterId!=InChapter && !InChapter.IsNone();
	if (Story) Story->OnChanged.RemoveAll(this);
	Story = InStory; ChapterId = InChapter; bFullscreen = bInFullscreen;
	if (ChapterId.IsNone() && Story) ChapterId = Story->PreferredChapter();
	if (Story) Story->OnChanged.AddUObject(this, &UGameXXKMainStoryPanelWidget::OnStoryChanged);
	if(bChapterChanged){SelectedNode=NAME_None;bDetail=false;bReplay=false;TreeScrollOffset=0;}
	bDirty = true;
}
void UGameXXKMainStoryPanelWidget::NativeConstruct()
{
	Super::NativeConstruct();
	GameXXKLocalization::OnLanguageChanged().RemoveAll(this);
	GameXXKLocalization::OnLanguageChanged().AddUObject(this,&UGameXXKMainStoryPanelWidget::OnStoryChanged);
	if(Story){Story->OnChanged.RemoveAll(this);Story->OnChanged.AddUObject(this,&UGameXXKMainStoryPanelWidget::OnStoryChanged);}
	bDirty=true;
}
TSharedRef<SWidget> UGameXXKMainStoryPanelWidget::RebuildWidget()
{
	EnsureRoot(); RebuildContent(); return Super::RebuildWidget();
}
void UGameXXKMainStoryPanelWidget::EnsureRoot()
{
	if (!WidgetTree) WidgetTree = NewObject<UWidgetTree>(this, TEXT("MainStoryWidgetTree"));
	if (Canvas) return;
	Canvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("MainStoryDesignCanvas"));
	DesignSize = WidgetTree->ConstructWidget<USizeBox>(); DesignSize->SetWidthOverride(PanelSize.X); DesignSize->SetHeightOverride(PanelSize.Y); DesignSize->SetContent(Canvas);
	if (!bFullscreen) WidgetTree->RootWidget = DesignSize;
	else
	{
		auto* Full = WidgetTree->ConstructWidget<UCanvasPanel>();
		auto* Dim = WidgetTree->ConstructWidget<UBorder>(); Dim->SetBrushColor(FLinearColor(.02f,.015f,.01f,.65f));
		auto* DimSlot = Full->AddChildToCanvas(Dim); DimSlot->SetAnchors(FAnchors(0,0,1,1)); DimSlot->SetOffsets(FMargin(0));
		auto* Scale = WidgetTree->ConstructWidget<UScaleBox>(); Scale->SetStretch(EStretch::ScaleToFit); Scale->SetContent(DesignSize);
		auto* ScaleSlot = Full->AddChildToCanvas(Scale); ScaleSlot->SetAnchors(FAnchors(.06f,.10f,.94f,.90f)); ScaleSlot->SetOffsets(FMargin(0));
		WidgetTree->RootWidget = Full;
	}
	SetIsFocusable(true);
}
void UGameXXKMainStoryPanelWidget::NativeDestruct()
{
	GameXXKLocalization::OnLanguageChanged().RemoveAll(this);
	if (Story) Story->OnChanged.RemoveAll(this);
	if(WidgetTree) WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if(auto* Image=Cast<UGameXXKAsyncStoryImage>(Widget)) Image->ClearStoryTexture();
	});
	Super::NativeDestruct();
}
void UGameXXKMainStoryPanelWidget::NativeTick(const FGeometry& Geometry, float DeltaTime)
{
	Super::NativeTick(Geometry, DeltaTime);
	const float VisualScale = Geometry.GetLocalSize().X > 1 ? Geometry.GetAbsoluteSize().X / Geometry.GetLocalSize().X : 1.f;
	const float Wanted = bFullscreen ? 1.f : FMath::Clamp(.75f/FMath::Max(.1f,VisualScale),1.f,1.5f);
	if (!FMath::IsNearlyEqual(Wanted,TextScale,.02f)) { TextScale = Wanted; bDirty = true; }
	if(bDirty)
	{
		if(GetWorld()) OnStoryChanged();
		else RebuildContent();
	}
}
void UGameXXKMainStoryPanelWidget::OnStoryChanged()
{
	bDirty=true;
	if(!bRefreshQueued) if(UWorld* World=GetWorld())
	{
		bRefreshQueued=true;
		World->GetTimerManager().SetTimerForNextTick(FTimerDelegate::CreateWeakLambda(this,[this]()
		{
			bRefreshQueued=false;
			if(bDirty && Canvas && GetCachedWidget().IsValid()) RebuildContent();
		}));
	}
}
void UGameXXKMainStoryPanelWidget::AddText(const FText& Text, FVector2D Position, FVector2D Size, int32 FontSize, bool Bold, EGameXXKFontRole Role, FLinearColor Color)
{
	int32 ActualFont=FMath::RoundToInt(FontSize*TextScale);
	const FText Localized=GameXXKLocalization::Localize(Text);
	if(Size.Y<=65 && !Localized.IsEmpty())ActualFont=FMath::Min(ActualFont,FMath::Max(12,FMath::FloorToInt(Size.X*.72f/Localized.ToString().Len())));
	auto* TextWidget = Label(WidgetTree, Text, ActualFont, Bold, Role);
	if(Size.Y<=65)TextWidget->SetAutoWrapText(false);
	if (Color.A >= 0) TextWidget->SetColorAndOpacity(FSlateColor(Color));
	Place(Canvas, TextWidget, Position, Size);
}
void UGameXXKMainStoryPanelWidget::AddScrollableText(const FText& Text,FVector2D Position,FVector2D Size,int32 FontSize,bool Bold)
{
	auto* Scroll=WidgetTree->ConstructWidget<UScrollBox>();
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(Scroll,5);Scroll->SetScrollbarThickness(FVector2D(5,5));
	FSlateBrush None;None.DrawAs=ESlateBrushDrawType::NoDrawType;FScrollBoxStyle ReadingStyle;ReadingStyle.SetTopShadowBrush(None).SetBottomShadowBrush(None).SetLeftShadowBrush(None).SetRightShadowBrush(None);Scroll->SetWidgetStyle(ReadingStyle);
	Scroll->SetConsumeMouseWheel(EConsumeMouseWheel::WhenScrollingPossible);
	auto* Body=Label(WidgetTree,Text,FMath::RoundToInt(FontSize*TextScale),Bold);
	Body->SetWrapTextAt(Size.X-22);Scroll->AddChild(Body);Place(Canvas,Scroll,Position,Size);
}
UGameXXKMainStoryActionButton* UGameXXKMainStoryPanelWidget::AddAction(FName Name, const FText& Text, int32 Action, FVector2D Position, FVector2D Size, bool Primary, FName NodeId)
{
	// Old Slate buttons may still paint this frame and point into their UButton style.
	// Never reconstruct a new UObject over the same named button while that view is alive.
	auto* Button = WidgetTree->ConstructWidget<UGameXXKMainStoryActionButton>(UGameXXKMainStoryActionButton::StaticClass(),MakeUniqueObjectName(WidgetTree,UGameXXKMainStoryActionButton::StaticClass(),Name));
	Button->Configure(this,Action,NodeId); Button->SetStyle(FGameXXKInRunUiStyle::Action(Size,Primary));
	if(!Primary)
	{
		FSlateBrush Clear;Clear.DrawAs=ESlateBrushDrawType::NoDrawType;
		FButtonStyle Quiet; FGameXXKSfx::SetButtonSound(Quiet);Quiet.SetNormal(Clear);Quiet.SetHovered(Clear);Quiet.SetPressed(Clear);Quiet.SetNormalPadding(FMargin(0));Quiet.SetPressedPadding(FMargin(0,1,0,-1));Button->SetStyle(Quiet);
	}
	const int32 FitFont=FMath::Min3(FMath::RoundToInt(18*TextScale),FMath::Max(12,static_cast<int32>(FMath::FloorToInt((Size.X-26)*.72f/FMath::Max(1,GameXXKLocalization::Localize(Text).ToString().Len())))),FMath::Max(15,static_cast<int32>(FMath::FloorToInt((Size.Y-10)*.62f))));
	auto* Caption = Label(WidgetTree,Text,FitFont,true); Caption->SetJustification(ETextJustify::Center);Caption->SetAutoWrapText(false);
	if (Primary) Caption->SetColorAndOpacity(FSlateColor(FLinearColor(.96f,.91f,.79f,1)));
	Button->SetContent(Caption);
	if(auto* ContentSlot=Cast<UButtonSlot>(Caption->Slot)){ContentSlot->SetHorizontalAlignment(HAlign_Fill);ContentSlot->SetVerticalAlignment(VAlign_Center);}
	Place(Canvas,Button,Position,Size); return Button;
}
void UGameXXKMainStoryPanelWidget::BuildHeader(const FText& Title)
{
	auto* Paper=GameXXKDesktopPaperStyle::MakePanel(WidgetTree,NAME_None,PanelSize,FLinearColor(.8f,.74f,.61f,1),true); Paper->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Canvas,Paper,FVector2D::ZeroVector,PanelSize);
	if(GameXXKLocalization::IsEnglish())
	{
		FString Location,Subtitle;
		if(GameXXKLocalization::Localize(Title).ToString().Split(TEXT(" · "),&Location,&Subtitle))
		{
			auto* Name=Label(WidgetTree,FText::FromString(Location),22,true,EGameXXKFontRole::Title);Name->SetAutoWrapText(false);
			auto* Sub=Label(WidgetTree,FText::FromString(Subtitle),14);Sub->SetAutoWrapText(false);
			Place(Canvas,Name,FVector2D(28,8),FVector2D(520,32));
			Place(Canvas,Sub,FVector2D(28,41),FVector2D(520,20));
		}
		else
		{
			auto* Heading=Label(WidgetTree,Title,20,true,EGameXXKFontRole::Title);Heading->SetAutoWrapText(false);
			auto* Fit=WidgetTree->ConstructWidget<UScaleBox>();Fit->SetStretch(EStretch::ScaleToFit);Fit->SetStretchDirection(EStretchDirection::DownOnly);Fit->SetContent(Heading);
			Place(Canvas,Fit,FVector2D(28,12),FVector2D(520,48));
		}
	}
	else AddText(Title,FVector2D(28,12),FVector2D(520,48),28,true,EGameXXKFontRole::Title);
	auto* Close=AddAction(TEXT("StoryPanelClose"),FText::GetEmpty(),0,FVector2D(871,8),FVector2D(54,54),false);
	FSlateBrush Empty; Empty.DrawAs=ESlateBrushDrawType::NoDrawType;
	FButtonStyle CloseStyle; FGameXXKSfx::SetButtonSound(CloseStyle); CloseStyle.SetNormal(Empty); CloseStyle.SetHovered(Empty); CloseStyle.SetPressed(Empty);
	CloseStyle.SetNormalPadding(FMargin(0)); CloseStyle.SetPressedPadding(FMargin(0)); Close->SetStyle(CloseStyle);
	Close->SetToolTipText(GameXXKLocalization::Source(TEXT("关闭任务树")));
	auto* Icon=WidgetTree->ConstructWidget<UImage>();
	Icon->SetBrushFromTexture(LoadObject<UTexture2D>(nullptr,TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk.T_MasterV2_CloseInk")));
	Icon->SetVisibility(ESlateVisibility::HitTestInvisible); Close->SetContent(Icon);
}
void UGameXXKMainStoryPanelWidget::RebuildContent()
{
	EnsureRoot(); bDirty = false;
	if (TreeScroll) TreeScrollOffset = TreeScroll->GetScrollOffset();
	// Detached result images must release both their request and material texture reference.
	WidgetTree->ForEachWidget([](UWidget* Widget)
	{
		if(auto* Image=Cast<UGameXXKAsyncStoryImage>(Widget)) Image->ClearStoryTexture();
	});
	TreeScroll = nullptr; Canvas->ClearChildren();
	const auto* State = Story ? Story->State() : nullptr;
	const auto* Chapter = FGameXXKMainStoryCatalog::FindChapter(ChapterId);
	if (!State || !Chapter) { BuildHeader(FText::FromString(TEXT("任务树"))); AddText(FText::FromString(TEXT("当前没有可展开的主线。")),FVector2D(40,110),FVector2D(850,100),24); return; }
	const auto& Session = State->NarrativeProgress.MainStory;
	if (!Session.ActiveNodeId.IsNone())
	{
		if (const auto* Active = FGameXXKMainStoryCatalog::FindNode(Session.ActiveNodeId))
		{
			if (Active->ChapterId == ChapterId && Session.Phase != EGameXXKMainStoryActivityPhase::None && Session.Phase != EGameXXKMainStoryActivityPhase::AwaitingGate)
			{
				SelectedNode = Active->Id;
				if (Session.Phase == EGameXXKMainStoryActivityPhase::Result) { BuildResult(); return; }
				// Live dialogue/choices are owned by the existing lightweight presenter.
				BuildTree(); return;
			}
		}
	}
	if (bReplay) { BuildTree(); return; }
	if (bDetail) { BuildDetail(); return; }
	BuildTree();
}
void UGameXXKMainStoryPanelWidget::BuildTree()
{
	const auto* Chapter = FGameXXKMainStoryCatalog::FindChapter(ChapterId); if (!Chapter) return;
	BuildHeader(Chapter->Title);
	TreeScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), MakeUniqueObjectName(WidgetTree,UScrollBox::StaticClass(),TEXT("MainStoryTreeScroll")));
	TreeScroll->SetOrientation(Orient_Horizontal);
	TreeScroll->SetConsumeMouseWheel(EConsumeMouseWheel::Always);
	FGameXXKPartyDeckUiStyle::ApplyBackpackInkScrollBar(TreeScroll,7);TreeScroll->SetScrollbarThickness(FVector2D(7,7));
	auto* Graph = WidgetTree->ConstructWidget<UGameXXKMainStoryGraphWidget>(); Graph->SetContext(this,Story,ChapterId,TextScale);
	TreeScroll->AddChild(Graph); Place(Canvas,TreeScroll,FVector2D(24,90),FVector2D(897,410)); TreeScroll->SetScrollOffset(TreeScrollOffset);
	AddText(FText::FromString(TEXT("向右浏览 · 点选节点")),FVector2D(32,504),FVector2D(480,23),14,false,EGameXXKFontRole::Body,FGameXXKInRunUiStyle::MutedInk());
}
FText UGameXXKMainStoryPanelWidget::RewardText(FName NodeId) const
{
	const auto* Node = FGameXXKMainStoryCatalog::FindNode(NodeId); if (!Node) return FText::GetEmpty();
	FString Result = FString::Printf(TEXT("报酬：%d万金币"),Node->Gold/10000);
	if (Node->AdvancedBoxes || Node->NormalBoxes) Result += FString::Printf(TEXT(" · %d级高级箱%d个、普通箱%d个"),Node->BoxLevel,Node->AdvancedBoxes,Node->NormalBoxes);
	return FText::FromString(Result);
}
void UGameXXKMainStoryPanelWidget::BuildDetail()
{
	const auto* Node=FGameXXKMainStoryCatalog::FindNode(SelectedNode);const auto* State=Story?Story->State():nullptr;
	if(!Node || !State){bDetail=false;BuildTree();return;}
	if(FGameXXKMainStoryRules::IsNodeCompleted(*State,Node->Id)){BuildResult();return;}
	BuildHeader(Node->Title);
	auto* Art=WidgetTree->ConstructWidget<UGameXXKAsyncStoryImage>();Art->SetStoryTexture(Node->Illustration);Art->SetVisibility(ESlateVisibility::HitTestInvisible);
	Place(Canvas,Art,FVector2D(45,76),FVector2D(855,285));
	AddScrollableText(Node->Summary,FVector2D(36,365),FVector2D(873,74),18);
	const auto Status=FGameXXKMainStoryRules::NodeState(*State,Node->Id);
	FString Objective=Node->Objective.ToString();
	if(Status==EGameXXKTaskState::Locked)
	{
		TArray<FString> Missing;
		for(FName Id:Node->RequiresAll)if(!FGameXXKMainStoryRules::IsNodeCompleted(*State,Id))if(const auto* N=FGameXXKMainStoryCatalog::FindNode(Id))Missing.Add(GameXXKLocalization::Localize(N->Title).ToString());
		if(!Node->RequiresAny.IsEmpty())
		{
			TArray<FString> Names;bool Any=false;
			for(FName Id:Node->RequiresAny){Any|=FGameXXKMainStoryRules::IsNodeCompleted(*State,Id);if(const auto* N=FGameXXKMainStoryCatalog::FindNode(Id))Names.Add(GameXXKLocalization::Localize(N->Title).ToString());}
			if(!Any)Missing.Add(GameXXKLocalization::Source(TEXT("任选：")).ToString()+FString::Join(Names,TEXT(" / ")));
		}
		Objective=GameXXKLocalization::Source(TEXT("先完成：")).ToString()+FString::Join(Missing,GameXXKLocalization::IsEnglish()?TEXT(", "):TEXT("、"));
	}
	else if(!Story->Feedback().IsEmpty())Objective=Story->Feedback().ToString();
	AddText(FText::FromString(Objective),FVector2D(36,440),FVector2D(873,27),16,false,EGameXXKFontRole::Body,FGameXXKInRunUiStyle::MutedInk());
	AddText(RewardText(Node->Id),FVector2D(36,472),FVector2D(590,28),16);
	auto* Start=AddAction(TEXT("StoryStartTask"),FText::FromString(Status==EGameXXKTaskState::Active?TEXT("继续任务"):TEXT("开始任务")),1,FVector2D(668,470),FVector2D(236,46),true,Node->Id);
	Start->SetIsEnabled(Status!=EGameXXKTaskState::Locked);
	AddAction(TEXT("StoryBackToTree"),FText::FromString(TEXT("返回流程树")),2,FVector2D(28,497),FVector2D(175,28),false);
}
void UGameXXKMainStoryPanelWidget::BuildResult()
{
	const auto* Node = FGameXXKMainStoryCatalog::FindNode(SelectedNode); const auto* State = Story ? Story->State() : nullptr;
	if (!Node || !State) { BuildTree(); return; }
	BuildHeader(Node->Title);
	auto* Illustration = WidgetTree->ConstructWidget<UGameXXKAsyncStoryImage>(UGameXXKAsyncStoryImage::StaticClass(),MakeUniqueObjectName(WidgetTree,UGameXXKAsyncStoryImage::StaticClass(),TEXT("StoryResultIllustration")));
	Illustration->SetStoryTexture(Node->Illustration);
	Illustration->SetVisibility(ESlateVisibility::HitTestInvisible);
	auto* Fit = WidgetTree->ConstructWidget<UScaleBox>(); Fit->SetStretch(EStretch::ScaleToFit); Fit->SetContent(Illustration);
	Place(Canvas,Fit,FVector2D(45,76),FVector2D(855,285));
	AddScrollableText(Node->Result,FVector2D(36,367),FVector2D(873,70),18);
	AddText(RewardText(Node->Id),FVector2D(36,445),FVector2D(873,28),16);
	const auto Status = FGameXXKMainStoryRules::NodeState(*State,Node->Id);
	AddAction(TEXT("StoryResultBack"),FText::FromString(TEXT("返回流程树")),2,FVector2D(28,486),FVector2D(190,32),false);
	AddAction(TEXT("StoryReplay"),FText::FromString(TEXT("回看对白")),8,FVector2D(246,486),FVector2D(190,32),false,Node->Id);
	if (Status == EGameXXKTaskState::Completed)
		AddAction(TEXT("StoryClaimReward"),FText::FromString(TEXT("领取奖励")),4,FVector2D(668,476),FVector2D(236,46),true,Node->Id);
	else AddAction(TEXT("StoryContinueMainline"),FText::FromString(TEXT("继续主线")),7,FVector2D(668,476),FVector2D(236,46),true);
}
void UGameXXKMainStoryPanelWidget::SelectNode(FName NodeId)
{
	if (const auto* Node = FGameXXKMainStoryCatalog::FindNode(NodeId))
	{
		ChapterId = Node->ChapterId; SelectedNode = NodeId; bDetail = true; bReplay = false; OnStoryChanged();
	}
}
void UGameXXKMainStoryPanelWidget::HandleAction(int32 Action, FName NodeId)
{
	if (!Story) return;
	if (Action==100) { SelectNode(NodeId); return; }
	if (Action>=200 && Action<210) { Story->ChooseAnswer(Action-200); bDirty=true; return; }
	switch (Action)
	{
	case 0: ClosePanel(); break;
	case 1: Story->StartTask(NodeId); break;
	case 2: Story->PauseActivity(); bReplay=false; bDetail=false; if (bFullscreen) Story->OpenJourneyTree(); break;
	case 3: Story->AdvanceDialogue(); break;
	case 4: Story->ClaimReward(NodeId); break;
	case 5: Story->RevealHint(); break;
	case 6: Story->BeginTaskBattle(); break;
	case 7:
		if (const auto* State=Story->State())
		{
			const FName Next = FGameXXKMainStoryRules::NextMainlineNode(*State,ChapterId);
			Story->PauseActivity();
			if (!Next.IsNone()) { SelectNode(Next); } else { bDetail=false; bReplay=false; }
			if (bFullscreen) Story->OpenJourneyTree();
		}
		break;
	case 8: Story->PauseActivity(); SelectedNode=NodeId; bReplay=true; bDetail=false; ReplayIndex=0; if (bFullscreen) Story->OpenJourneyTree(); break;
	case 9:
		if (const auto* N=FGameXXKMainStoryCatalog::FindNode(SelectedNode)) { ++ReplayIndex; if (ReplayIndex>=N->ReplayLineCount()) { bReplay=false; bDetail=true; } }
		break;
	default: break;
	}
	OnStoryChanged();
}
void UGameXXKMainStoryPanelWidget::ClosePanel()
{
	if (Story) { if(Story->ActiveNode()) Story->PauseActivity(); if (bFullscreen) Story->CloseJourneyTree(); }
	bDetail=false; bReplay=false;
	OnClosed.ExecuteIfBound();
}
FReply UGameXXKMainStoryPanelWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey()==EKeys::Escape) { if (bDetail || bReplay || (Story && Story->ActiveNode())) HandleAction(2); else ClosePanel(); return FReply::Handled(); }
	if ((Event.GetKey()==EKeys::SpaceBar || Event.GetKey()==EKeys::Enter) && Story && Story->State()
		&& Story->State()->NarrativeProgress.MainStory.Phase==EGameXXKMainStoryActivityPhase::Dialogue) { HandleAction(3); return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry,Event);
}
FReply UGameXXKMainStoryPanelWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return bFullscreen ? FReply::Handled() : Super::NativeOnMouseButtonDown(Geometry,Event);
}
