#include "UI/GameXXKCardSynergyPresentation.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

namespace
{
	using K=EGameXXKCardSynergyKind;
	using Unit=FGameXXKCardCombatUnit;
	const Unit* Find(const FGameXXKCardBattleRuntime& R,FName Id)
	{
		return R.Units.FindByPredicate([Id](const Unit& U){return U.UnitId==Id && U.bLiving;});
	}
	int32 Stacks(const Unit* U,EGameXXKCardStatus S) { return U ? GameXXKCardRules::GetCombatStatusStacks(*U,S) : 0; }
	bool HasDot(const Unit* U)
	{
		return Stacks(U,EGameXXKCardStatus::Bleed)>0 || Stacks(U,EGameXXKCardStatus::Poison)>0 || Stacks(U,EGameXXKCardStatus::Burn)>0;
	}
	const Unit* Highest(const FGameXXKCardBattleRuntime& R,const Unit& Owner,bool Armor)
	{
		const Unit* Best=nullptr;
		for(const auto& U:R.Units)
		{
			if(!U.bLiving || U.Side!=Owner.Side)continue;
			const int32 Value=Armor ? U.Armor : U.Attack;
			const int32 Prior=Best ? (Armor ? Best->Armor : Best->Attack) : MIN_int32;
			if(!Best || Value>Prior || (Value==Prior && U.StableSortOrder<Best->StableSortOrder))Best=&U;
		}
		return Best;
	}
	TArray<const Unit*> Selected(const FGameXXKCardBattleRuntime& R,const FGameXXKCardPlayPreview* P)
	{
		TArray<const Unit*> Result;
		if(P)
		{
			for(const auto& C:P->TargetRequest.CandidateViews)if(C.bCanSelect)if(const auto* U=Find(R,C.UnitId))Result.AddUnique(U);
			for(const auto Id:P->TargetRequest.AutomaticTargetUnitIds)if(const auto* U=Find(R,Id))Result.AddUnique(U);
		}
		return Result;
	}
	bool BaseGate(const FGameXXKCardEffectCondition& C,const FGameXXKCardBattleRuntime& R,const Unit& Owner,const TArray<const Unit*>& Targets)
	{
		switch(C.Type)
		{
		case EGameXXKCardEffectConditionType::None:return true;
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:return Owner.Armor>=C.MinimumArmor;
		case EGameXXKCardEffectConditionType::OwnerHasStatus:return Stacks(&Owner,C.Status)>=C.MinimumStatusStacks;
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:return HasDot(&Owner);
		case EGameXXKCardEffectConditionType::TerrainIsAny:return R.Terrain==C.Terrain || R.Terrain==C.AlternateTerrain;
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:return Owner.MaxHP>0 && 100.0*Owner.HP<Owner.MaxHP*C.HealthPercentThreshold;
		default:break;
		}
		return Targets.ContainsByPredicate([&](const Unit* U)
		{
			switch(C.Type)
			{
			case EGameXXKCardEffectConditionType::TargetHasStatus:return Stacks(U,C.Status)>=C.MinimumStatusStacks;
			case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:return HasDot(U);
			case EGameXXKCardEffectConditionType::TargetIsAlly:return U->Side==Owner.Side;
			case EGameXXKCardEffectConditionType::TargetIsEnemy:return U->Side!=Owner.Side;
			case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:return U->MaxHP>0 && 100.0*U->HP<U->MaxHP*C.HealthPercentThreshold;
			default:return false;
			}
		});
	}
	bool Gate(const FGameXXKCardEffectCondition& C,const FGameXXKCardBattleRuntime& R,const Unit& Owner,const TArray<const Unit*>& Targets)
	{
		const bool Value=BaseGate(C,R,Owner,Targets);return C.bNegate ? !Value : Value;
	}
	TSet<FName> Carried(const FGameXXKCardBattleRuntime& R,FName Owner,FName DefinitionOwner)
	{
		TSet<FName> Ids;
		for(const auto* Zone:{&R.Deck.Hand,&R.Deck.DrawPile,&R.Deck.DiscardPile,&R.Deck.ExhaustPile,&R.Deck.PendingAutomaticHandCards})
			for(const auto& C:*Zone)if(!C.bTemporary && C.OwnerUnitId==Owner)
				if(const auto* D=FGameXXKCardCatalog::FindCardDefinition(C.CardId))if(D->OwnerId==DefinitionOwner)Ids.Add(C.CardId);
		return Ids;
	}
	bool HasFinish(const FGameXXKCardDefinition& D)
	{
		return (D.LinkedRole==EGameXXKCharacterRole::Blade && !D.FinishEffects.IsEmpty()) || D.BladeSequence.FinishRule!=EGameXXKBladeFinishRule::None;
	}
	int32 Rank(K Kind)
	{
		switch(Kind)
		{
		case K::FormulaOpening:case K::BladeOpening:return 100;
		case K::HeavyArrow:return 95;
		case K::SpellTask:case K::TerrainEnhanced:return 90;
		case K::ArmorConversion:return 85;
		case K::BladeFollowup:return 80;
		case K::MedicineReady:return 75;
		case K::DotDetonation:return 65;
		case K::Terrain:return 20;
		default:return 15;
		}
	}
}

TArray<FGameXXKCardSynergyCue> GameXXKCardSynergyPresentation::Build(const FGameXXKCardBattleRuntime& Runtime,
	const FGameXXKCardInstance& Card, const FGameXXKCardDefinition& Definition, const FGameXXKCardPlayPreview* Preview)
{
	TArray<FGameXXKCardSynergyCue> Cues;
	const Unit* Owner=Find(Runtime,Card.OwnerUnitId);
	if(!Owner || Owner->Side!=EGameXXKCardTargetSide::Party || Runtime.Phase!=EGameXXKCardBattlePhase::Player)return Cues;
	const float Availability=Preview && !Preview->bCanPlay ? 0.42f : 1.0f;
	auto Add=[&](K Kind,const FString& Reason,float Strength=1.0f,int32 Progress=0,int32 Total=0)
	{
		if(Cues.ContainsByPredicate([Kind](const auto& C){return C.Kind==Kind;}))return;
		FGameXXKCardSynergyCue Cue;Cue.Kind=Kind;Cue.Color=Color(Kind);Cue.Reason=Reason;Cue.Strength=Strength*Availability;Cue.Progress=Progress;Cue.Total=Total;Cues.Add(Cue);
	};
	if(Definition.HealerRule.FormulaKind!=EGameXXKHealerFormulaKind::None
		&& !Runtime.HealerFormulas.ContainsByPredicate([&](const auto& F){return F.OwnerUnitId==Card.OwnerUnitId && F.SourceCardId==Card.CardId;}))
		Add(K::FormulaOpening,TEXT("药方未开启 · 首次主动打出激活本场效果"));

	auto TaskCue=[&](const auto& Locked,const auto& Done)
	{
		if(Locked.Contains(Card.CardId) && !Done.Contains(Card.CardId))
			Add(K::SpellTask,FString::Printf(TEXT("法术任务待完成 · %d/%d"),Done.Num(),Locked.Num()),0.85f,Done.Num(),Locked.Num());
	};
	if(Definition.Owner==EGameXXKCardOwner::Hero && Runtime.EquippedHeroCardIds.Contains(Card.CardId))
	{
		const auto& T=Runtime.HeroSpellTask;
		if(T.bActive && T.StarterOwnerUnitId==Card.OwnerUnitId)TaskCue(T.LockedHeroCardIds,T.CompletedHeroCardIds);
		else if(!T.bActive && (Runtime.HeroSpellTaskLastCompletedRound==0 || Runtime.HeroSpellTaskLastCompletedRound!=Runtime.RoundNumber))
		{
			TArray<FName> Ids;
			for(auto Id:Runtime.EquippedHeroCardIds)if(const auto* D=FGameXXKCardCatalog::FindCardDefinition(Id))
				if(D->LinkedRole==EGameXXKCharacterRole::Sorcerer && D->SpellTaskReward!=EGameXXKHeroSpellTaskReward::None)Ids.AddUnique(Id);
			if(Ids.Num()==4)TaskCue(Ids,TArray<FName>());
		}
	}
	if(!Card.bTemporary && Definition.Owner==EGameXXKCardOwner::Profession && Definition.Role==EGameXXKCharacterRole::Sorcerer)
	{
		const auto* T=Runtime.SorcererPartnerTasks.FindByPredicate([&](const auto& X){return X.bActive && X.OwnerUnitId==Card.OwnerUnitId;});
		if(T)TaskCue(T->LockedCardIds,T->CompletedCardIds);
		else {auto Ids=Carried(Runtime,Card.OwnerUnitId,Definition.OwnerId);if(Ids.Num()==5)TaskCue(Ids,TArray<FName>());}
	}
	if(Definition.Owner==EGameXXKCardOwner::QuestNpc && !Definition.TaskNpcRewardEffects.IsEmpty())
	{
		const auto* T=Runtime.TaskNpcSpellTasks.FindByPredicate([&](const auto& X){return X.bActive && X.OwnerUnitId==Card.OwnerUnitId;});
		if(T)TaskCue(T->LockedCardIds,T->CompletedCardIds);
		else {auto Ids=Carried(Runtime,Card.OwnerUnitId,Definition.OwnerId);if(Ids.Num()==3)TaskCue(Ids,TArray<FName>());}
	}
	const bool Opening=Definition.BladeSequence.ChargeRule!=EGameXXKBladeChargeRule::None || (Definition.LinkedRole==EGameXXKCharacterRole::Blade && !Definition.ChargeEffects.IsEmpty());
	if(Opening && Runtime.ActiveCardsPlayedThisRound==0)Add(K::BladeOpening,TEXT("冲锋就绪 · 本回合第一张主动牌"));
	else if(HasFinish(Definition))Add(K::BladeFinishCandidate,TEXT("收招候选 · 留作最后一张主动牌时生效"),0.46f);
	if(Runtime.PendingBladeCharge.Rule!=EGameXXKBladeChargeRule::None || Runtime.PendingBladeNativeStyle.Rule!=EGameXXKBladeChargeRule::None || Runtime.PendingBladeResidualStyle.Rule!=EGameXXKBladeChargeRule::None)
		Add(K::BladeFollowup,TEXT("承接刀式 · 下一张主动牌可触发已登记的冲锋联动"),0.74f);

	const auto Targets=Selected(Runtime,Preview);
	const Unit* ArrowOwner=Definition.HeavyArrow.ChargeSource==EGameXXKHeavyArrowChargeSource::HighestAttackAlly ? Highest(Runtime,*Owner,false) : Owner;
	int32 ArrowCharge=Stacks(ArrowOwner,EGameXXKCardStatus::Charge);
	int32 Medicine=Stacks(Owner,EGameXXKCardStatus::Medicine);
	bool TerrainPayload=false,TerrainExtra=false,MedicinePayload=false,DotPayload=false;
	TArray<FString> TerrainReasons;
	for(const auto& E:Definition.Effects)
	{
		if(!Gate(E.Condition,Runtime,*Owner,Targets))continue;
		if(E.Type==EGameXXKCardEffectType::ApplyStatus && E.Status==EGameXXKCardStatus::Medicine && E.Target==EGameXXKCardEffectTarget::CardOwner)Medicine=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(Medicine)+FMath::Max(0,E.Magnitude)));
		if(Definition.HeavyArrow.LockTiming==EGameXXKHeavyArrowLockTiming::AfterBaseEffects && E.Type==EGameXXKCardEffectType::ApplyStatus && E.Status==EGameXXKCardStatus::Charge)
		{
			const Unit* Recipient=E.Target==EGameXXKCardEffectTarget::HighestAttackAlly ? Highest(Runtime,*Owner,false) : E.Target==EGameXXKCardEffectTarget::CardOwner ? Owner : nullptr;
			if(Recipient && Recipient==ArrowOwner)ArrowCharge=static_cast<int32>(FMath::Min<int64>(MAX_int32,static_cast<int64>(ArrowCharge)+FMath::Max(0,E.Magnitude)));
		}
		if(E.Type==EGameXXKCardEffectType::HealOrReverseWithMedicine && Medicine>0)
		{
			const bool Useful=Runtime.Units.ContainsByPredicate([&](const auto& U){return U.bLiving && (U.Side!=Owner->Side || U.HP<U.MaxHP || HasDot(&U));});
			MedicinePayload|=Useful;
		}
		if(E.Type==EGameXXKCardEffectType::DamagePercentAttackPlusArmor || E.Type==EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor || E.Type==EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor)
		{
			const Unit* Source=E.Source==EGameXXKCardEffectSource::HighestArmorAlly ? Highest(Runtime,*Owner,true) : E.Source==EGameXXKCardEffectSource::HighestAttackAlly ? Highest(Runtime,*Owner,false) : Owner;
			const bool HasArmor=E.Source==EGameXXKCardEffectSource::SelectedTarget ? Targets.ContainsByPredicate([](const Unit* U){return U->Armor>0;}) : Source && Source->Armor>0;
			if(HasArmor)Add(K::ArmorConversion,TEXT("护甲转伤 · 伤害来源已有可用护甲"),0.9f);
		}
		if(E.Type==EGameXXKCardEffectType::TriggerTerrainBenefit)
		{
			TerrainPayload=true;TerrainExtra|=Runtime.bTerrainChangedThisRound && E.SecondaryMagnitude>E.Magnitude;
			if(Runtime.bTerrainChangedThisRound && E.SecondaryMagnitude>E.Magnitude)TerrainReasons.AddUnique(TEXT("本回合换势增益"));
		}
		if(E.Type==EGameXXKCardEffectType::ChangeTerrain && E.TerrainOverride!=Runtime.Terrain) {TerrainPayload=true;TerrainExtra=true;TerrainReasons.AddUnique(TEXT("将切换地势"));}
		if(E.Condition.Type==EGameXXKCardEffectConditionType::TerrainIsAny) {TerrainPayload=true;TerrainExtra=true;TerrainReasons.AddUnique(TEXT("当前地势相合"));}
		if(E.Type==EGameXXKCardEffectType::ResolveToxicExplosion)
			DotPayload|=Targets.ContainsByPredicate([](const Unit* U){return HasDot(U);});
	}
	if(Definition.HeavyArrow.Kind!=EGameXXKHeavyArrowKind::None && ArrowCharge>0)
		Add(K::HeavyArrow,FString::Printf(TEXT("重箭就绪 · %s%d层蓄力"),Definition.HeavyArrow.ChargeSource==EGameXXKHeavyArrowChargeSource::HighestAttackAlly ? TEXT("最高攻击友方可提供") : TEXT("自身可提供"),ArrowCharge),1.0f,ArrowCharge,0);
	if(MedicinePayload)Add(K::MedicineReady,TEXT("药效联动 · 本牌可使用药效治疗或反转"),0.78f);
	if(DotPayload)Add(K::DotDetonation,TEXT("毒爆联动 · 可选目标已有持续伤害"),0.7f);
	if(TerrainPayload)
	{
		TerrainExtra|=Stacks(Owner,EGameXXKCardStatus::TerrainBonusDouble)>0 || Stacks(Owner,EGameXXKCardStatus::TerrainBonusDoubleThisRound)>0
			|| Stacks(Owner,EGameXXKCardStatus::NextTerrainCardFree)>0 || Stacks(Owner,EGameXXKCardStatus::NextTerrainCardEnergyReduction)>0
			|| (Preview && !Preview->AppliedShanHeFourPieceEffectId.IsNone());
		if(Stacks(Owner,EGameXXKCardStatus::TerrainBonusDouble)>0 || Stacks(Owner,EGameXXKCardStatus::TerrainBonusDoubleThisRound)>0)TerrainReasons.AddUnique(TEXT("地势双效"));
		if(Stacks(Owner,EGameXXKCardStatus::NextTerrainCardFree)>0)TerrainReasons.AddUnique(TEXT("地势免耗"));
		if(Stacks(Owner,EGameXXKCardStatus::NextTerrainCardEnergyReduction)>0)TerrainReasons.AddUnique(TEXT("地势减耗"));
		if(Preview && !Preview->AppliedShanHeFourPieceEffectId.IsNone())TerrainReasons.AddUnique(TEXT("山河减耗"));
		if(TerrainExtra)Add(K::TerrainEnhanced,TEXT("地势联动 · ")+FString::Join(TerrainReasons,TEXT("、")),0.94f);
		Add(K::Terrain,TEXT("地势收益 · 本牌会触发或切换地势"),0.38f);
	}
	Cues.StableSort([](const auto& A,const auto& B){return Rank(A.Kind)>Rank(B.Kind);});
	return Cues;
}
FString GameXXKCardSynergyPresentation::FinisherHint(const FGameXXKCardBattleRuntime& Runtime)
{
	if(Runtime.Phase!=EGameXXKCardBattlePhase::Player || Runtime.ActiveCardsPlayedThisRound<=0 || !Find(Runtime,Runtime.LastActiveCard.OwnerUnitId))return FString();
	const auto* D=FGameXXKCardCatalog::FindCardDefinition(Runtime.LastActiveCard.CardId);
	return D && HasFinish(*D) ? FString::Printf(TEXT("结束回合将触发〈%s〉的收招"),*D->DisplayName.ToString()) : FString();
}
FLinearColor GameXXKCardSynergyPresentation::Color(K Kind)
{
	switch(Kind)
	{
	case K::FormulaOpening:case K::MedicineReady:return FLinearColor(0.22f,0.72f,0.34f,1);
	case K::SpellTask:return FLinearColor::FromSRGBColor(FColor(105, 35, 202));
	case K::BladeOpening:return FLinearColor(0.92f,0.16f,0.10f,1);
	case K::BladeFinishCandidate:case K::BladeFollowup:return FLinearColor(0.98f,0.44f,0.48f,1);
	case K::HeavyArrow:return FLinearColor(1.0f,0.70f,0.10f,1);
	case K::ArmorConversion:return FLinearColor(0.15f,0.53f,0.96f,1);
	case K::DotDetonation:return FLinearColor(0.29f,0.69f,0.47f,1);
	default:return FLinearColor::FromSRGBColor(FColor(137, 28, 183));
	}
}
FString GameXXKCardSynergyPresentation::Describe(const TArray<FGameXXKCardSynergyCue>& Cues)
{
	TArray<FString> Lines;for(const auto& C:Cues)Lines.Add(C.Reason);return FString::Join(Lines,TEXT("\n"));
}
