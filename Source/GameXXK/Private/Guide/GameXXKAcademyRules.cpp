#include "Guide/GameXXKAcademyRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"

void FGameXXKAcademyRules::ObserveCommittedResult(const FGameXXKCardPlayResult& Result,FName FocusUnitId,FGameXXKAcademyEvidence& Evidence)
{
	if(Result.OwnerUnitId!=FocusUnitId)return;
	for(const auto& Change:Result.StatusChanges)
	{
		if(Change.Status==EGameXXKCardStatus::Charge && Change.AppliedStacks>0)Evidence.Record(EGameXXKAcademyGoal::Charge);
	}
	if(Result.HeavyArrowChargeConsumed>0)Evidence.Record(EGameXXKAcademyGoal::HeavyArrow);
}

bool FGameXXKAcademyEvidence::Satisfies(const FGameXXKAcademyLesson& Lesson) const
{
	return Lesson.Goals.Num()>0 && !Lesson.Goals.ContainsByPredicate([this](const auto& Goal){return Counts.FindRef(Goal.Kind)<Goal.Required;});
}

const FGameXXKAcademyCourse* FGameXXKAcademyRules::Find(FName Id)
{
	return Courses().FindByPredicate([Id](const auto& Course){return Course.Id==Id;});
}

bool FGameXXKAcademyRules::CompleteLesson(FName CourseId,int32 LessonIndex,bool bWon,const FGameXXKAcademyEvidence& Evidence,
	FGameXXKGuideProgress& Progress,int32& Gold,int32& Award,FString& Error)
{
	Award=0;Error.Reset();
	if (Gold<0 || !ValidateProgress(Progress,Error)) {if(Error.IsEmpty())Error=TEXT("金币或教学进度无效。");return false;}
	const auto* Course=Find(CourseId);
	if (!Course || !Course->Lessons.IsValidIndex(LessonIndex)) {Error=TEXT("教学任务不存在。");return false;}
	if (!bWon || !Evidence.Satisfies(Course->Lessons[LessonIndex])) {Error=TEXT("请完成本节操作目标并赢得战斗。");return false;}
	const int32 Completed=Progress.AcademyCompletedLessons.FindRef(CourseId);
	if (LessonIndex>Completed) {Error=TEXT("请先完成前一节教学。");return false;}
	if (LessonIndex<Completed) return true;
	const bool bFirstClear=Completed+1==Course->Lessons.Num() && !Progress.AcademyRewardedCourses.Contains(CourseId);
	if (bFirstClear && Gold>MAX_int32-FirstClearGold) {Error=TEXT("金币已达到上限，请稍后重试结算。");return false;}
	Progress.AcademyCompletedLessons.Add(CourseId,Completed+1);
	if (bFirstClear) {Progress.AcademyRewardedCourses.Add(CourseId);Gold+=FirstClearGold;Award=FirstClearGold;}
	return true;
}

bool FGameXXKAcademyRules::ValidateProgress(const FGameXXKGuideProgress& Progress,FString& Error)
{
	for (const auto& Pair:Progress.AcademyCompletedLessons)
	{
		const auto* Course=Find(Pair.Key);
		if (!Course || Pair.Value<0 || Pair.Value>Course->Lessons.Num()) {Error=TEXT("教学进度无效。");return false;}
	}
	for (FName Id:Progress.AcademyRewardedCourses)
	{
		const auto* Course=Find(Id);
		if (!Course || Progress.AcademyCompletedLessons.FindRef(Id)<=0) {Error=TEXT("教学奖励记录与通关进度不一致。");return false;}
	}
	return true;
}

void FGameXXKAcademyRules::Observe(const FGameXXKCardBattleRuntime& Before,const FGameXXKCardBattleRuntime& After,
	const TArray<FGameXXKCardDamageResult>& Damage,FName PlayedInstance,FName FocusUnitId,FGameXXKAcademyEvidence& Evidence)
{
	using G=EGameXXKAcademyGoal;
	const auto* Card=Before.Deck.Hand.FindByPredicate([&](const auto& C){return C.InstanceId==PlayedInstance;});
	const bool bFocusPlay=Card && Card->OwnerUnitId==FocusUnitId && After.SessionStats.ActiveCardsPlayed>Before.SessionStats.ActiveCardsPlayed;
	const auto* Definition=bFocusPlay ? FGameXXKCardCatalog::FindCardDefinition(Card->CardId) : nullptr;
	if(bFocusPlay){Evidence.ActiveCardIds.Add(Card->CardId);Evidence.Counts.Add(G::ActiveCards,Evidence.ActiveCardIds.Num());}
	if(After.RoundNumber>Before.RoundNumber || (Before.Phase==EGameXXKCardBattlePhase::Player && After.Phase==EGameXXKCardBattlePhase::Enemy)) Evidence.Record(G::EndRound);
	if(Before.Terrain!=After.Terrain)Evidence.Record(G::TerrainChange);
	const int32 Benefits=After.ResolvedTerrainBenefitsByOwner.FindRef(FocusUnitId)-Before.ResolvedTerrainBenefitsByOwner.FindRef(FocusUnitId);
	if(Benefits>0)Evidence.Record(G::TerrainBenefit,Benefits);
	const int32 Finishes=After.ResolvedBladeFinishesByOwner.FindRef(FocusUnitId)-Before.ResolvedBladeFinishesByOwner.FindRef(FocusUnitId);
	if(Finishes>0)Evidence.Record(G::BladeFinish,Finishes);
	auto Stacks=[](const FGameXXKCardCombatUnit& U,EGameXXKCardStatus Status){int32 N=0;for(const auto& S:U.Statuses)if(S.Status==Status)N+=S.Stacks;return N;};
	bool bChargeConsumed=false;
	for(const auto& U:After.Units)
	{
		const auto* Old=Before.Units.FindByPredicate([&](const auto& B){return B.UnitId==U.UnitId;});
		if(!Old || U.Side!=EGameXXKCardTargetSide::Party)continue;
		if(bFocusPlay && U.SettlementHealingReceived>Old->SettlementHealingReceived)Evidence.Record(G::Healing);
		if(bFocusPlay && U.SettlementArmorGenerated>Old->SettlementArmorGenerated)Evidence.Record(G::Armor);
		if(Stacks(U,EGameXXKCardStatus::Charge)>Stacks(*Old,EGameXXKCardStatus::Charge))Evidence.Record(G::Charge);
		bChargeConsumed|=Stacks(U,EGameXXKCardStatus::Charge)<Stacks(*Old,EGameXXKCardStatus::Charge);
		if(Stacks(U,EGameXXKCardStatus::Medicine)>Stacks(*Old,EGameXXKCardStatus::Medicine))Evidence.Record(G::Medicine);
		if(bFocusPlay)for(auto Status:{EGameXXKCardStatus::Bleed,EGameXXKCardStatus::Poison,EGameXXKCardStatus::Burn})
			if(Stacks(U,Status)<Stacks(*Old,Status))Evidence.Record(G::Cleanse);
	}
	if(Definition && Definition->HeavyArrow.Kind!=EGameXXKHeavyArrowKind::None && bChargeConsumed)Evidence.Record(G::HeavyArrow);
	if(After.HealerFormulas.Num()>Before.HealerFormulas.Num())Evidence.Record(G::Formula);
	for(const auto& D:Damage)
	{
		if(Definition && D.SourceUnitId==FocusUnitId && D.HealthDamage+D.ArmorAbsorbed>0)
		{
			const auto* Owner=Before.Units.FindByPredicate([&](const auto& U){return U.UnitId==FocusUnitId;});
			if(Owner && Owner->Armor>0 && Definition->Effects.ContainsByPredicate([](const auto& E){return E.Type==EGameXXKCardEffectType::DamagePercentAttackPlusArmor || E.Type==EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor;}))Evidence.Record(G::ArmorDamage);
		}
		const auto* Victim=Before.Units.FindByPredicate([&](const auto& U){return U.UnitId==D.ResolvedTargetUnitId;});
		// Command/joint-attack cards belong to the teaching character even when
		// a selected ally supplies the attack stat or executes the damage packet.
		if((D.SourceUnitId==FocusUnitId || bFocusPlay) && Victim && Victim->Side==EGameXXKCardTargetSide::Enemy
			&& D.HealthDamage+D.ArmorAbsorbed>0)Evidence.Record(G::Damage);
		if(D.Cause==EGameXXKCardDamageCause::Block || D.Cause==EGameXXKCardDamageCause::Counter || D.OriginalTargetUnitId!=D.ResolvedTargetUnitId)Evidence.Record(G::Reaction);
		if(D.Cause==EGameXXKCardDamageCause::ToxicExplosionBleed || D.Cause==EGameXXKCardDamageCause::ToxicExplosionPoison || D.Cause==EGameXXKCardDamageCause::ToxicExplosionBurn || D.Cause==EGameXXKCardDamageCause::ToxicExplosionRot)
			if(D.HealthDamage+D.ArmorAbsorbed>0)Evidence.Record(G::ToxicExplosion);
	}
	auto CompletedTask=[&](const auto& TasksBefore,const auto& TasksAfter)
	{
		for(const auto& T:TasksBefore)if(T.OwnerUnitId==FocusUnitId && T.bActive)
		{
			const auto* Next=TasksAfter.FindByPredicate([&](const auto& N){return N.OwnerUnitId==FocusUnitId;});
			if((!Next || !Next->bActive) && !T.LockedCardIds.IsEmpty() && (T.CompletedCardIds.Num()==T.LockedCardIds.Num() || (bFocusPlay && T.CompletedCardIds.Num()+1==T.LockedCardIds.Num())))Evidence.Record(G::SpellTask);
		}
	};
	CompletedTask(Before.SorcererPartnerTasks,After.SorcererPartnerTasks);CompletedTask(Before.TaskNpcSpellTasks,After.TaskNpcSpellTasks);
    if(Before.HeroSpellTask.bActive&&Before.HeroSpellTask.StarterOwnerUnitId==FocusUnitId
        &&After.HeroSpellTaskLastCompletedRound>Before.HeroSpellTaskLastCompletedRound)Evidence.Record(G::SpellTask);
	if(bFocusPlay && Before.ActiveCardsPlayedThisRound==0 && After.PendingBladeCharge.Rule!=EGameXXKBladeChargeRule::None)Evidence.Record(G::BladeOpening);
}

void FGameXXKAcademyRules::Recommend(const FGameXXKRuntimeState& State,FName FocusUnitId,const FGameXXKAcademyLesson& Lesson,
 const FGameXXKAcademyEvidence& Evidence,FName RestrictedCard,FName& CueCard,FName& Target,bool& EndTurn)
{
 CueCard=NAME_None;Target=NAME_None;EndTurn=false;const auto& Battle=State.CardRun.ActiveBattle;
	const auto* Goal=Lesson.Goals.FindByPredicate([&](const auto& G){return Evidence.Counts.FindRef(G.Kind)<G.Required;});
	const bool Targeting=!RestrictedCard.IsNone();
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
			{EndTurn=true;return;}
		}
	}
	int32 BestScore=MIN_int32;FName BestTarget;
	for(const auto& Card:Battle.Deck.Hand)
	{
		if((Goal && Card.OwnerUnitId!=FocusUnitId) || (Targeting && Card.InstanceId!=RestrictedCard))continue;
		FGameXXKCardPlayPreview Preview;FString Error;
		if(!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State,Card.InstanceId,Preview,&Error)||!Preview.bCanPlay)continue;
		TArray<FName> Targets;
		if(Preview.TargetRequest.bRequiresManualSelection){for(const auto& View:Preview.TargetRequest.CandidateViews)if(View.bCanSelect)Targets.Add(View.UnitId);}
		else Targets.Add(NAME_None);
		for(FName TrialTarget:Targets)
		{
			auto Trial=State;FGameXXKCardPlayResult Result;
			if(!FGameXXKCardBattleAdapter::ResolveCardPlay(Trial,Card.InstanceId,TrialTarget,Result,&Error))continue;
			auto TrialEvidence=Evidence;
			FGameXXKAcademyRules::ObserveCommittedResult(Result,FocusUnitId,TrialEvidence);
			FGameXXKAcademyRules::Observe(Battle,Trial.CardRun.ActiveBattle,Result.DamageResults,Card.InstanceId,FocusUnitId,TrialEvidence);
			int32 Score=Evidence.ActiveCardIds.Contains(Card.CardId)?0:1000;
			if(Goal)Score+=10000*(TrialEvidence.Counts.FindRef(Goal->Kind)-Evidence.Counts.FindRef(Goal->Kind));
			const int32 Order=Lesson.Cards.IndexOfByKey(Card.CardId);Score-=Order==INDEX_NONE?50:Order;
			if(Evidence.ActiveCardIds.IsEmpty() && Order==0)Score+=200;
			if(!Goal)
			{
				// Keep coaching after the lesson's mechanic is demonstrated. Prefer
				// concrete damage, then healing/draw support from the whole party.
				int64 Damage=0,Healing=0;
				for(const auto& Packet:Result.DamageResults)
				{
					const auto* Victim=Battle.Units.FindByPredicate([&](const auto& U){return U.UnitId==Packet.ResolvedTargetUnitId;});
					if(Victim && Victim->Side==EGameXXKCardTargetSide::Enemy)Damage+=Packet.HealthDamage+Packet.ArmorAbsorbed;
				}
				for(const auto& Unit:Trial.CardRun.ActiveBattle.Units)
				{
					const auto* Old=Battle.Units.FindByPredicate([&](const auto& U){return U.UnitId==Unit.UnitId;});
					if(Old && Unit.Side==EGameXXKCardTargetSide::Party)Healing+=FMath::Max(0,Unit.HP-Old->HP);
				}
				Score=static_cast<int32>(FMath::Min<int64>(1000000,Damage*10+Healing));
				Score+=20*FMath::Max(0,Trial.CardRun.ActiveBattle.Deck.Hand.Num()-Battle.Deck.Hand.Num()+1);
				if(Trial.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory)Score+=10000000;
			}
			if(Trial.CardRun.ActiveBattle.Phase==EGameXXKCardBattlePhase::Victory && !TrialEvidence.Satisfies(Lesson))Score-=100000;
			if(Score>BestScore){BestScore=Score;CueCard=Card.InstanceId;BestTarget=TrialTarget;}
		}
	}
	Target=BestTarget;EndTurn=CueCard.IsNone();
}
