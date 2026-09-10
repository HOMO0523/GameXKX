#include "Guide/GameXXKAcademySubsystem.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "UI/GameXXKLocalization.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameInstance.h"

void UGameXXKAcademySubsystem::UpdateGuidance(UGameXXKBattleBoardWidget* Board,bool bWatching)
{
	bGuideCueDirty=false;CueTargets.Reset();CueCard=NAME_None;
	const auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
	if(!MVP || !Lesson())return;
	const auto& State=MVP->GetRuntimeState();const auto& Battle=State.CardRun.ActiveBattle;
    bPractice=Evidence.Satisfies(*Lesson());
	if(!Battle.Deck.PendingChoice.Candidates.IsEmpty())
	{
		CueMode=4;CueText=GameXXKLocalization::Text(Battle.Deck.PendingChoice.Kind==EGameXXKCardPendingChoiceKind::ForcedDiscard?TEXT("Academy.Cue.Discard"):TEXT("Academy.Cue.Choose"));return;
	}
	if(bWatching || Battle.Phase==EGameXXKCardBattlePhase::Enemy)
	{
		CueMode=3;CueText=GameXXKLocalization::Text(Battle.Phase==EGameXXKCardBattlePhase::Enemy?TEXT("Academy.Cue.WatchEnemy"):TEXT("Academy.Cue.WatchCard"));
		const FName Target=Board->GetActiveBattlePresentationTargetUnitIdForTest();if(!Target.IsNone())CueTargets.Add(Target);return;
	}
	if(Battle.Phase==EGameXXKCardBattlePhase::Victory || Battle.Phase==EGameXXKCardBattlePhase::Defeat)
	{
		CueMode=3;CueText=Message.IsEmpty()?GameXXKLocalization::Text(TEXT("Academy.Cue.Finished")):Message;return;
	}
    const bool Targeting=Board->IsCardTargetingActive();
    FName BestTarget;bool EndTurn=false;
    FGameXXKAcademyRules::Recommend(State,FocusUnitId,*Lesson(),Evidence,
        Targeting?Board->GetPendingCardInstanceIdForTest():NAME_None,CueCard,BestTarget,EndTurn);
	if(CueCard.IsNone())
	{
		CueMode=Targeting?6:2;CueText=GameXXKLocalization::Text(Targeting?TEXT("Academy.Cue.Cancel"):TEXT("Academy.Cue.EndTurn"));return;
	}
	if(Targeting)
	{
		CueMode=1;CueTargets.Add(BestTarget);
		const auto* Target=Battle.Units.FindByPredicate([&](const auto& U){return U.UnitId==BestTarget;});
		CueText=GameXXKLocalization::Text(Target && Target->Side==EGameXXKCardTargetSide::Party?TEXT("Academy.Cue.Ally"):TEXT("Academy.Cue.Enemy"));
	}
	else
	{
		CueMode=0;const auto* Card=Battle.Deck.Hand.FindByPredicate([&](const auto& C){return C.InstanceId==CueCard;});
		const auto* Definition=Card?FGameXXKCardCatalog::FindCardDefinition(Card->CardId):nullptr;
		CueText=Definition?FText::Format(GameXXKLocalization::Text(TEXT("Academy.Cue.Card")),GameXXKLocalization::Localize(Definition->DisplayName)):GameXXKLocalization::Text(TEXT("Academy.Cue.Highlight"));
	}
}

void UGameXXKAcademySubsystem::RefreshOverlay(UGameXXKBattleBoardWidget* Board)
{
	if(!IsActive() || !Lesson() || !Board || !Board->GetBattleDesignStageForTest())return;
	if(Board->IsAutoBattleEnabled())Board->SetAutoBattleEnabled(false);
	const bool Targeting=Board->IsCardTargetingActive();
	const bool Watching=Board->GetBattlePresentationQueueCountForTest()>0 || Board->GetActiveBattlePresentationEventIdForTest()!=0 || FPlatformTime::Seconds()<GuideObserveUntil;
	if(Targeting!=bLastGuideTargeting || Watching!=bLastGuideWatching)bGuideCueDirty=true;
	bLastGuideTargeting=Targeting;bLastGuideWatching=Watching;
    if(GuidanceLanguageRevision!=GameXXKLocalization::GetRevision())bGuideCueDirty=true;
	if(bGuideCueDirty)UpdateGuidance(Board,Watching);
	auto* Stage=Board->GetBattleDesignStageForTest();auto* ViewportRoot=Board->GetBattleViewportRootForTest();if(!ViewportRoot)return;
	if(!Overlay || OverlayBoard.Get()!=Board)
	{
		if(Overlay)Overlay->RemoveFromParent();
		Overlay=NewObject<UCanvasPanel>(Board);OverlayBoard=Board;Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* RootSlot=ViewportRoot->AddChildToCanvas(Overlay);RootSlot->SetAnchors(FAnchors(0,0,1,1));RootSlot->SetOffsets(FMargin(0));RootSlot->SetZOrder(100);
		GuideSpotlight=NewObject<UGameXXKGuideSpotlightWidget>(Board);GuideSpotlight->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* MaskSlot=Overlay->AddChildToCanvas(GuideSpotlight);MaskSlot->SetAnchors(FAnchors(0,0,1,1));MaskSlot->SetOffsets(FMargin(0));
        GuideCaption=NewObject<UBorder>(Board,TEXT("AcademyTeachingPaper"));
        FSlateBrush NoBacking;NoBacking.DrawAs=ESlateBrushDrawType::NoDrawType;
        GuideCaption->SetBrush(NoBacking);GuideCaption->SetPadding(FMargin(18,12));
        auto* Stack=NewObject<UVerticalBox>(Board);GuideCaption->SetContent(Stack);
        auto AddLine=[&](const TCHAR* Name,int32 Size,FLinearColor Color,bool Wrap)
        {
            auto* Text=NewObject<UTextBlock>(Board,Name);Text->SetFont(FGameXXKInRunUiStyle::Font(Size,true));
            Text->SetColorAndOpacity(Color);Text->SetAutoWrapText(Wrap);
            Stack->AddChildToVerticalBox(Text)->SetPadding(FMargin(0,0,0,4));return Text;
        };
        LessonTitleText=AddLine(TEXT("AcademyLessonTitle"),21,FLinearColor::White,false);
        MechanismText=AddLine(TEXT("AcademyMechanism"),19,FLinearColor::White,true);
        GoalText=AddLine(TEXT("AcademyAction"),24,FLinearColor::White,false);
        ObjectiveText=AddLine(TEXT("AcademyObjective"),18,FLinearColor(1,1,1,.75f),false);
		auto* CaptionSlot=Overlay->AddChildToCanvas(GuideCaption);CaptionSlot->SetZOrder(2);
		for(int32 I=0;I<2;++I)
		{
			auto* B=NewObject<UButton>(Board);FButtonStyle Style;
            Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(.025f,.03f,.035f,.78f),4.f));
            Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(.15f,.16f,.17f,.9f),4.f));Style.SetPressed(FSlateRoundedBoxBrush(FLinearColor(.2f,.21f,.22f,.95f),4.f));B->SetStyle(Style);
			auto* Text=NewObject<UTextBlock>(Board);Text->SetText(GameXXKLocalization::Text(I==0?TEXT("Academy.Cue.Exit"):TEXT("Academy.Cue.Retry")));Text->SetFont(FGameXXKInRunUiStyle::Font(22,true));Text->SetColorAndOpacity(FLinearColor::White);B->SetContent(Text);
			auto* BS=Overlay->AddChildToCanvas(B);BS->SetPosition(FVector2D(1510+I*180,22));BS->SetSize(FVector2D(164,44));BS->SetZOrder(3);
			if(I==0)B->OnClicked.AddDynamic(this,&UGameXXKAcademySubsystem::OnExit);else B->OnClicked.AddDynamic(this,&UGameXXKAcademySubsystem::OnRetry);
		}
	}
	TArray<FSlateRect> Rects;
	const auto AddWidget=[&](UWidget* Widget)
	{
		if(!Widget)return;const auto& G=Widget->GetCachedGeometry();const auto Size=G.GetLocalSize();if(Size.X<1 || Size.Y<1)return;
		const auto& Host=ViewportRoot->GetCachedGeometry();
		const auto A=Host.AbsoluteToLocal(G.LocalToAbsolute(FVector2D::ZeroVector));const auto B=Host.AbsoluteToLocal(G.LocalToAbsolute(Size));
		Rects.Add(FSlateRect(A.X,A.Y,B.X,B.Y));
	};
	if(CueMode==0)
	{
		const auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
		const int32 Index=MVP->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.IndexOfByPredicate([&](const auto& C){return C.InstanceId==CueCard;});
		if(Index!=INDEX_NONE)AddWidget(Board->GetHandCardButtonForTest(Index));
	}
	if(CueMode==1 || CueMode==3)for(FName Id:CueTargets)AddWidget(Board->GetUnitTargetProxyForTest(Id));
	if(CueMode==2)AddWidget(Board->GetEndTurnButtonForTest());
    const auto AddDesignRect=[&](const FSlateRect& Rect)
    {
        const auto& Host=ViewportRoot->GetCachedGeometry();const auto& Design=Stage->GetCachedGeometry();
        const auto A=Host.AbsoluteToLocal(Design.LocalToAbsolute(FVector2D(Rect.Left,Rect.Top)));
        const auto B=Host.AbsoluteToLocal(Design.LocalToAbsolute(FVector2D(Rect.Right,Rect.Bottom)));
        Rects.Add(FSlateRect(A.X,A.Y,B.X,B.Y));
    };
    if(CueMode==4)AddDesignRect(FSlateRect(350,230,1550,1020));
    if(CueMode==6)AddDesignRect(FSlateRect(350,730,1510,1080));
    // Never leave a featureless dark screen while animations/target widgets
    // settle. Keep the observed combat area or actionable hand area visible.
    if(Rects.IsEmpty())AddDesignRect(CueMode==3?FSlateRect(80,250,1840,1010):FSlateRect(340,730,1880,1080));
	FGameXXKGuideOutput Output;Output.bActive=true;Output.InputPolicy=(CueMode==0 || CueMode==1 || CueMode==2 || CueMode==4)?EGameXXKGuideInputPolicy::Forced:EGameXXKGuideInputPolicy::Soft;
	GuideSpotlight->PresentSpotlight(Output,Rects);
    GoalText->SetText(GameXXKLocalization::Localize(CueText));
    LessonTitleText->SetText(FText::Format(GameXXKLocalization::Text(TEXT("Academy.Cue.Lesson")),GameXXKLocalization::Localize(Lesson()->Title),ActiveLessonIndex+1,Course()->Lessons.Num()));
    MechanismText->SetText(GameXXKLocalization::Localize(Lesson()->Instruction));
    GuideCaption->SetToolTipText(GameXXKLocalization::Localize(Course()->Summary));
    const auto* Goal=Lesson()->Goals.FindByPredicate([&](const auto& G){return Evidence.Counts.FindRef(G.Kind)<G.Required;});
    ObjectiveText->SetText(Goal?FText::Format(GameXXKLocalization::Text(TEXT("Academy.Cue.Progress")),GameXXKLocalization::Localize(Goal->Text),FMath::Min(Evidence.Counts.FindRef(Goal->Kind),Goal->Required),Goal->Required):GameXXKLocalization::Text(TEXT("Academy.Cue.ObjectivesMet")));
    // Text/controls retain design-stage sizing while the mask covers the entire
    // viewport, including letterboxed margins. Keep copy clear of both intents.
    const auto& HostGeometry=ViewportRoot->GetCachedGeometry();const auto& DesignGeometry=Stage->GetCachedGeometry();
    const float TextScale=DesignGeometry.GetAccumulatedLayoutTransform().GetScale()/FMath::Max(.01f,HostGeometry.GetAccumulatedLayoutTransform().GetScale());
    const auto PlaceDesign=[&](UWidget* Widget,FVector2D Position,FVector2D Size)
    {
        if(auto* Slot=Cast<UCanvasPanelSlot>(Widget->Slot))
        {
            Slot->SetPosition(HostGeometry.AbsoluteToLocal(DesignGeometry.LocalToAbsolute(Position)));Slot->SetSize(Size);
            Widget->SetRenderTransformPivot(FVector2D::ZeroVector);Widget->SetRenderScale(FVector2D(TextScale));
        }
    };
    PlaceDesign(GuideCaption,FVector2D(815,20),FVector2D(520,210));
    int32 ControlIndex=0;
    for(int32 Index=0;Index<Overlay->GetChildrenCount();++Index)
        if(auto* Button=Cast<UButton>(Overlay->GetChildAt(Index)))PlaceDesign(Button,FVector2D(1510+ControlIndex++*180,22),FVector2D(164,44));
    if(GuidanceLanguageRevision!=GameXXKLocalization::GetRevision())
        for(int32 Index=0;Index<Overlay->GetChildrenCount();++Index)if(auto* Button=Cast<UButton>(Overlay->GetChildAt(Index)))
            if(auto* Label=Cast<UTextBlock>(Button->GetContent()))Label->SetText(GameXXKLocalization::Localize(Label->GetText()));
    GuidanceLanguageRevision=GameXXKLocalization::GetRevision();
}
