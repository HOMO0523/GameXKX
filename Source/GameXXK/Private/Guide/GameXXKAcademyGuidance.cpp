#include "Guide/GameXXKAcademySubsystem.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKGuideOverlayWidget.h"
#include "UI/GameXXKInRunUiStyle.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/TextBlock.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/GameInstance.h"

void UGameXXKAcademySubsystem::UpdateGuidance(UGameXXKBattleBoardWidget* Board,bool bWatching)
{
	bGuideCueDirty=false;CueTargets.Reset();CueCard=NAME_None;
	const auto* MVP=GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>();
	if(!MVP || !Lesson())return;
	const auto& State=MVP->GetRuntimeState();const auto& Battle=State.CardRun.ActiveBattle;
	if(!Battle.Deck.PendingChoice.Candidates.IsEmpty())
	{
		CueMode=4;CueText=FText::FromString(Battle.Deck.PendingChoice.Kind==EGameXXKCardPendingChoiceKind::ForcedDiscard?TEXT("选择高亮区域中的牌弃掉。"):TEXT("选择一张牌加入手牌。"));return;
	}
	if(bWatching || Battle.Phase==EGameXXKCardBattlePhase::Enemy)
	{
		CueMode=3;CueText=FText::FromString(Battle.Phase==EGameXXKCardBattlePhase::Enemy?TEXT("观察敌方行动。"):TEXT("观察卡牌结算。"));
		const FName Target=Board->GetActiveBattlePresentationTargetUnitIdForTest();if(!Target.IsNone())CueTargets.Add(Target);return;
	}
	if(Battle.Phase==EGameXXKCardBattlePhase::Victory || Battle.Phase==EGameXXKCardBattlePhase::Defeat)
	{
		CueMode=3;CueText=Message.IsEmpty()?FText::FromString(TEXT("本节结束。")):Message;return;
	}
	if(Evidence.Satisfies(*Lesson())){CueMode=5;CueText=FText::FromString(TEXT("自由出牌，击败剩余敌人。"));return;}
	const auto* Goal=Lesson()->Goals.FindByPredicate([&](const auto& G){return Evidence.Counts.FindRef(G.Kind)<G.Required;});
	const bool Targeting=Board->IsCardTargetingActive();
	if(!Targeting && Goal && (Goal->Kind==EGameXXKAcademyGoal::EndRound || Goal->Kind==EGameXXKAcademyGoal::Reaction || Goal->Kind==EGameXXKAcademyGoal::BladeFinish))
	{
		auto Trial=State;auto TrialEvidence=Evidence;FString Error;TArray<FGameXXKCardDamageResult> Damage;
		if(FGameXXKCardBattleAdapter::EndPlayerCardPhase(Trial,Damage,&Error))
		{
			FGameXXKAcademyRules::Observe(Battle,Trial.CardRun.ActiveBattle,Damage,NAME_None,FocusUnitId,TrialEvidence);
			const auto BeforeEnemy=Trial.CardRun.ActiveBattle;Damage.Reset();
			FGameXXKCardBattleAdapter::ResolveEnemyPhase(Trial,Damage,&Error);
			FGameXXKAcademyRules::Observe(BeforeEnemy,Trial.CardRun.ActiveBattle,Damage,NAME_None,FocusUnitId,TrialEvidence);
			if(TrialEvidence.Counts.FindRef(Goal->Kind)>Evidence.Counts.FindRef(Goal->Kind))
			{CueMode=2;CueText=FText::FromString(TEXT("点击结束回合。"));return;}
		}
	}
	int32 BestScore=MIN_int32;FName BestTarget;
	for(const auto& Card:Battle.Deck.Hand)
	{
		if(Card.OwnerUnitId!=FocusUnitId || (Targeting && Card.InstanceId!=Board->GetPendingCardInstanceIdForTest()))continue;
		FGameXXKCardPlayPreview Preview;FString Error;
		if(!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State,Card.InstanceId,Preview,&Error)||!Preview.bCanPlay)continue;
		TArray<FName> Targets;
		if(Preview.TargetRequest.bRequiresManualSelection){for(const auto& View:Preview.TargetRequest.CandidateViews)if(View.bCanSelect)Targets.Add(View.UnitId);}
		else Targets.Add(NAME_None);
		for(FName Target:Targets)
		{
			auto Trial=State;FGameXXKCardPlayResult Result;
			if(!FGameXXKCardBattleAdapter::ResolveCardPlay(Trial,Card.InstanceId,Target,Result,&Error))continue;
			auto TrialEvidence=Evidence;
			FGameXXKAcademyRules::ObserveCommittedResult(Result,FocusUnitId,TrialEvidence);
			FGameXXKAcademyRules::Observe(Battle,Trial.CardRun.ActiveBattle,Result.DamageResults,Card.InstanceId,FocusUnitId,TrialEvidence);
			int32 Score=Evidence.ActiveCardIds.Contains(Card.CardId)?0:1000;
			if(Goal)Score+=10000*(TrialEvidence.Counts.FindRef(Goal->Kind)-Evidence.Counts.FindRef(Goal->Kind));
			const int32 Order=Lesson()->Cards.IndexOfByKey(Card.CardId);Score-=Order==INDEX_NONE?50:Order;
			if(Evidence.ActiveCardIds.IsEmpty() && Order==0)Score+=200;
			if(Trial.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory && !TrialEvidence.Satisfies(*Lesson()))Score-=100000;
			if(Score>BestScore){BestScore=Score;CueCard=Card.InstanceId;BestTarget=Target;}
		}
	}
	if(CueCard.IsNone())
	{
		CueMode=Targeting?1:2;CueText=FText::FromString(Targeting?TEXT("右键取消选牌。"):TEXT("点击结束回合，补充手牌。"));return;
	}
	if(Targeting)
	{
		CueMode=1;CueTargets.Add(BestTarget);
		const auto* Target=Battle.Units.FindByPredicate([&](const auto& U){return U.UnitId==BestTarget;});
		CueText=FText::FromString(Target && Target->Side==EGameXXKCardTargetSide::Party?TEXT("选择高亮角色。"):TEXT("选择高亮敌人。"));
	}
	else
	{
		CueMode=0;const auto* Card=Battle.Deck.Hand.FindByPredicate([&](const auto& C){return C.InstanceId==CueCard;});
		const auto* Definition=Card?FGameXXKCardCatalog::FindCardDefinition(Card->CardId):nullptr;
		CueText=FText::FromString(Definition?FString::Printf(TEXT("点击「%s」。"),*Definition->DisplayName.ToString()):TEXT("点击高亮卡牌。"));
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
	if(bGuideCueDirty)UpdateGuidance(Board,Watching);
	auto* Stage=Board->GetBattleDesignStageForTest();
	if(!Overlay || OverlayBoard.Get()!=Board)
	{
		if(Overlay)Overlay->RemoveFromParent();
		Overlay=NewObject<UCanvasPanel>(Board);OverlayBoard=Board;Overlay->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
		auto* RootSlot=Stage->AddChildToCanvas(Overlay);RootSlot->SetAnchors(FAnchors(0,0,1,1));RootSlot->SetOffsets(FMargin(0));RootSlot->SetZOrder(100);
		GuideSpotlight=NewObject<UGameXXKGuideSpotlightWidget>(Board);GuideSpotlight->SetVisibility(ESlateVisibility::HitTestInvisible);
		auto* MaskSlot=Overlay->AddChildToCanvas(GuideSpotlight);MaskSlot->SetAnchors(FAnchors(0,0,1,1));MaskSlot->SetOffsets(FMargin(0));
		GuideCaption=NewObject<UBorder>(Board);GuideCaption->SetBrush(FSlateRoundedBoxBrush(FLinearColor(0,0,0,.76f),8.0f));GuideCaption->SetPadding(FMargin(18,10));
		GoalText=NewObject<UTextBlock>(Board);GoalText->SetFont(FGameXXKInRunUiStyle::Font(26,true));GoalText->SetColorAndOpacity(FLinearColor(.98f,.96f,.87f,1));GoalText->SetAutoWrapText(false);GuideCaption->SetContent(GoalText);
		auto* CaptionSlot=Overlay->AddChildToCanvas(GuideCaption);CaptionSlot->SetZOrder(2);
		for(int32 I=0;I<2;++I)
		{
			auto* B=NewObject<UButton>(Board);FButtonStyle Style;
			Style.SetNormal(FSlateRoundedBoxBrush(FLinearColor(0,0,0,.72f),6.0f));Style.SetHovered(FSlateRoundedBoxBrush(FLinearColor(.1f,.1f,.1f,.9f),6.0f));B->SetStyle(Style);
			auto* Text=NewObject<UTextBlock>(Board);Text->SetText(FText::FromString(I==0?TEXT("退出教程"):TEXT("重试")));Text->SetFont(FGameXXKInRunUiStyle::Font(22,true));Text->SetColorAndOpacity(FLinearColor::White);B->SetContent(Text);
			auto* BS=Overlay->AddChildToCanvas(B);BS->SetPosition(FVector2D(1510+I*180,22));BS->SetSize(FVector2D(164,44));BS->SetZOrder(3);
			if(I==0)B->OnClicked.AddDynamic(this,&UGameXXKAcademySubsystem::OnExit);else B->OnClicked.AddDynamic(this,&UGameXXKAcademySubsystem::OnRetry);
		}
	}
	TArray<FSlateRect> Rects;
	const auto AddWidget=[&](UWidget* Widget)
	{
		if(!Widget)return;const auto& G=Widget->GetCachedGeometry();const auto Size=G.GetLocalSize();if(Size.X<1 || Size.Y<1)return;
		const auto& Host=Stage->GetCachedGeometry();
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
	if(CueMode==4)Rects.Add(FSlateRect(350,230,1550,1020));
	FGameXXKGuideOutput Output;Output.bActive=!Rects.IsEmpty();Output.InputPolicy=(CueMode==0 || CueMode==1 || CueMode==2 || CueMode==4)?EGameXXKGuideInputPolicy::Forced:EGameXXKGuideInputPolicy::Soft;
	GuideSpotlight->PresentSpotlight(Output,Rects);
	GoalText->SetText(CueText);
	const float Width=FMath::Clamp(CueText.ToString().Len()*25.0f+40,280.0f,650.0f);
	FVector2D Position(1030,22);
	if((CueMode==0 || CueMode==2) && !Rects.IsEmpty())Position=FVector2D(FMath::Clamp((Rects[0].Left+Rects[0].Right-Width)*.5f,20.0f,1880.0f-Width),FMath::Max(80.0f,Rects[0].Top-80));
	if(auto* Slot=Cast<UCanvasPanelSlot>(GuideCaption->Slot)){Slot->SetPosition(Position);Slot->SetSize(FVector2D(Width,62));}
}
