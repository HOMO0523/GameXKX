#include "UI/GameXXKBattleMechanicPresentation.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardText.h"
#include "GameXXKCombatScalingRules.h"

namespace
{
	using Element = EGameXXKMechanicElement;
	using FormulaKind = EGameXXKHealerFormulaKind;

	TArray<FGameXXKCardInstance> Carried(const FGameXXKCardBattleRuntime& Runtime, const FName Owner)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (const auto* Zone : {&Runtime.Deck.Hand, &Runtime.Deck.DrawPile, &Runtime.Deck.DiscardPile,
			&Runtime.Deck.ExhaustPile, &Runtime.Deck.PendingAutomaticHandCards})
			for (const auto& Card : *Zone)
				if (Card.OwnerUnitId == Owner && !Card.bTemporary) Cards.Add(Card);
		Cards.Sort([](const auto& A, const auto& B) { return A.AcquisitionOrdinal != B.AcquisitionOrdinal
			? A.AcquisitionOrdinal < B.AcquisitionOrdinal : A.InstanceId.LexicalLess(B.InstanceId); });
		return Cards;
	}

	Element FromBranch(const EGameXXKSorcererTaskBranch Branch)
	{
		switch (Branch)
		{
		case EGameXXKSorcererTaskBranch::Fire: return Element::Fire;
		case EGameXXKSorcererTaskBranch::Ice: return Element::Ice;
		case EGameXXKSorcererTaskBranch::Lightning: return Element::Lightning;
		default: return Element::Universal;
		}
	}

	FString CardName(const FName Id)
	{
		const auto* Definition = FGameXXKCardCatalog::FindCardDefinition(Id);
		return Definition ? Definition->DisplayName.ToString() : TEXT("未知卡牌");
	}

	FString CardLocation(const FGameXXKCardBattleRuntime& Runtime, const FName Owner, const FName CardId)
	{
		const auto Contains = [&](const TArray<FGameXXKCardInstance>& Cards) { return Cards.ContainsByPredicate(
			[&](const auto& Card) { return Card.OwnerUnitId == Owner && Card.CardId == CardId && !Card.bTemporary; }); };
		if (Contains(Runtime.Deck.Hand)) return TEXT("手牌中");
		if (Contains(Runtime.Deck.DrawPile)) return TEXT("抽牌堆中");
		if (Contains(Runtime.Deck.DiscardPile)) return TEXT("弃牌堆中");
		if (Contains(Runtime.Deck.ExhaustPile)) return TEXT("已消耗");
		return TEXT("待入手");
	}

	FString RewardText(const FName Starter, const EGameXXKCardQuality Quality, const EGameXXKSorcererTaskBranch Branch)
	{
		const auto* Definition = FGameXXKCardCatalog::FindCardDefinition(Starter);
		if (!Definition) return TEXT("首张主动打出的任务牌决定阵赏。");
		FGameXXKCardTooltipContext Context;
		Context.LockedSpellBranch = Branch;
		TArray<FString> Lines, Rewards;
		GameXXKCardText::DescribeCompactTooltipBody(*Definition, Quality, nullptr, Context).ParseIntoArrayLines(Lines);
		for (const auto& Line : Lines) if (Line.StartsWith(TEXT("阵赏"))) Rewards.Add(Line);
		return FString::Join(Rewards, TEXT("\n"));
	}

	bool OncePerRound(const FormulaKind Kind)
	{
		switch (Kind)
		{
		case FormulaKind::FirstHealingMedicine: case FormulaKind::LowHealthCrossMedicine:
		case FormulaKind::BleedRemovedPartyArmor: case FormulaKind::LargeHealingArmorOrVulnerability:
		case FormulaKind::LowHealthCrossAgility: case FormulaKind::ThreeUnitHealthChangeDrawMana:
		case FormulaKind::GroupPoisonMedicineDraw: case FormulaKind::GroupDirectDamageEnergy:
		case FormulaKind::PoisonedVulnerabilityMedicineDraw: case FormulaKind::TripleDotExplosionMomentumDraw:
		case FormulaKind::HeroSixMedicineHealDraw: case FormulaKind::HeroGroupHealEnergy: return true;
		default: return false;
		}
	}

	FString FormulaEffect(const FormulaKind Kind, const EGameXXKCardQuality Quality, const int32 Level)
	{
		const int32 Heal = FGameXXKCombatScalingRules::ResolveMedicineHealing(10,0,Quality,Level);
		const int32 LargeHeal = FGameXXKCombatScalingRules::ResolveMedicineHealing(20,0,Quality,Level);
		switch (Kind)
		{
		case FormulaKind::AnyHealthChangeMedicine: return TEXT("每笔伤害或治疗使任一角色气血变化时，自身获得1点药效。");
		case FormulaKind::HighEnergyAndSixMedicine: return FString::Printf(TEXT("每回合首次打出耗气至少2的牌：全队失去1气血（最低保留1点），再恢复%d气血。每回合累计获得6药效时，回复1气；敌方回合达成则下回合到账。"),Heal);
		case FormulaKind::FirstHealingMedicine: return TEXT("每回合首次实际治疗或造成药效反转伤害时，自身获得2点药效。");
		case FormulaKind::ThreeCleansedDotMedicine: return TEXT("累计清除友方3点流血、中毒或灼烧，自身获得1点药效；余数保留。");
		case FormulaKind::LowHealthCrossMedicine: return TEXT("每回合首次有友方气血从不低于35%降至35%以下，自身获得3点药效。");
		case FormulaKind::ThreeEffectiveHealsDraw: return TEXT("每3笔有效友方治疗抽1张牌，每回合最多2张。");
		case FormulaKind::BleedRemovedPartyArmor: return TEXT("每回合首次清除友方流血，全队获得相当于药师20%防御的护甲，受开方品质增幅。");
		case FormulaKind::LargeHealingArmorOrVulnerability: return FString::Printf(TEXT("每回合首次治疗或药效反转达到%d点：友方目标获得药师20%%防御的护甲（受开方品质增幅）；敌方目标获得1层破绽。"),LargeHeal);
		case FormulaKind::LowHealthCrossAgility: return TEXT("每回合首次有友方气血从不低于30%降至30%以下，该友方获得2层灵动。");
		case FormulaKind::ThreeUnitHealthChangeDrawMana: return TEXT("同次结算使至少3个不同单位实际气血变化：抽1张牌，全队恢复2内力；每回合1次。");
		case FormulaKind::PoisonDamageMedicine: return TEXT("每笔中毒伤害生效时，自身获得1点药效。");
		case FormulaKind::BleedPoisonMark: return TEXT("每回合，具有流血和中毒的敌人各获得1层标记，每名敌人1次。");
		case FormulaKind::GroupPoisonMedicineDraw: return TEXT("每回合首次使至少2名敌人获得中毒，自身获得2点药效并抽1张牌。");
		case FormulaKind::DualDotExplosionMedicine: return TEXT("每次毒爆包含至少2类持续伤害，自身获得2点药效。");
		case FormulaKind::TwoBleedPacketsMedicine: return TEXT("每2笔流血伤害生效，自身获得1点药效；余数保留。");
		case FormulaKind::GroupDirectDamageEnergy: return TEXT("每回合首次直接攻击命中至少2名敌人，回复1气。");
		case FormulaKind::PoisonedVulnerabilityMedicineDraw: return TEXT("每回合首次给已中毒的敌人施加破绽，自身获得1点药效并抽1张牌。");
		case FormulaKind::TripleDotExplosionMomentumDraw: return TEXT("每回合首次毒爆同时包含流血、中毒、灼烧，自身获得1层气势并抽1张牌。");
		case FormulaKind::HeroFirstPartyHealthLossMedicine: return TEXT("每回合每名友方首次实际失血时，自身获得1点药效，最多计3名友方。");
		case FormulaKind::HeroSixMedicineHealDraw: return TEXT("每回合首次在治疗或反转中消耗至少6点药效，抽1张牌。");
		case FormulaKind::HeroDualDotExplosionMedicine: return TEXT("毒爆包含至少2类持续伤害时，自身获得2点药效，每回合最多2次。");
		case FormulaKind::HeroGroupHealEnergy: return TEXT("每回合首次在同次结算中有效治疗至少2名友方，回复1气。");
		default: return TEXT("本局持续生效。");
		}
	}
}

int32 FGameXXKMechanicTaskView::CompletedCount() const
{
	int32 Count=0;for (const auto& Card:Cards) Count+=Card.PlayOrder>0 ? 1 : 0;return Count;
}

FString FGameXXKUnitMechanicView::Signature() const
{
	FString Result = FString::Printf(TEXT("%s:%d:%d:%d:%d:%d:%d:%s"), *OwnerUnitId.ToString(), bLiving,
		bReserveSpace, Task.bActive, Task.bReplaying, static_cast<int32>(Task.Element), RoundNumber, *Task.Tooltip);
	for (const auto& Card : Task.Cards) Result += FString::Printf(TEXT("|%s:%d:%s"), *Card.CardId.ToString(),Card.PlayOrder,*Card.Tooltip);
	for (const auto& Formula : Formulas) Result += FString::Printf(TEXT("|%s:%d:%d:%s:%s"), *Formula.CardId.ToString(),Formula.Number,Formula.bSpentThisRound,*Formula.Tooltip,*Formula.TriggerState);
	return Result;
}

FString GameXXKBattleMechanicPresentation::ElementName(const Element Value)
{
	switch (Value) { case Element::Fire:return TEXT("火焰");case Element::Ice:return TEXT("寒霜");case Element::Lightning:return TEXT("雷电");case Element::Formula:return TEXT("药方");default:return TEXT("通用"); }
}

FString GameXXKBattleMechanicPresentation::MaterialPath(const Element Value)
{
	const TCHAR* Name = Value==Element::Fire ? TEXT("Fire") : Value==Element::Ice ? TEXT("Ice") : Value==Element::Lightning ? TEXT("Lightning") : Value==Element::Formula ? TEXT("Formula") : TEXT("Universal");
	return FString::Printf(TEXT("/Game/GameXXK/UI/Battle/MechanicIcons/MI_Mechanic_%s.MI_Mechanic_%s"),Name,Name);
}

FGameXXKUnitMechanicView GameXXKBattleMechanicPresentation::Build(const FGameXXKCardBattleRuntime& Runtime, const FName OwnerUnitId)
{
	FGameXXKUnitMechanicView View;
	View.OwnerUnitId=OwnerUnitId;
	View.RoundNumber=Runtime.RoundNumber;
	View.ActivePlayCount=Runtime.ActiveCardsPlayedThisRound;
	View.LastActiveCardId=Runtime.LastActiveCard.CardId;
	View.LastActiveOwnerId=Runtime.LastActiveCard.OwnerUnitId;
	const auto* Owner=Runtime.Units.FindByPredicate([&](const auto& Unit) {return Unit.UnitId==OwnerUnitId;});
	if (!Owner || Owner->Side!=EGameXXKCardTargetSide::Party || !Owner->bLiving) return View;
	View.bLiving=true;
	const auto Cards=Carried(Runtime,OwnerUnitId);
	TArray<FName> HeroIds, PartnerIds, NpcIds, FormulaIds;
	for (const auto& Card:Cards) if (const auto* D=FGameXXKCardCatalog::FindCardDefinition(Card.CardId))
	{
		if (D->Owner==EGameXXKCardOwner::Hero && D->SpellTaskReward!=EGameXXKHeroSpellTaskReward::None) HeroIds.AddUnique(Card.CardId);
		if (D->Owner==EGameXXKCardOwner::Profession && D->Role==EGameXXKCharacterRole::Sorcerer) PartnerIds.AddUnique(Card.CardId);
		if (D->Owner==EGameXXKCardOwner::QuestNpc && !D->TaskNpcRewardEffects.IsEmpty()) NpcIds.AddUnique(Card.CardId);
		if (D->HealerRule.FormulaKind!=FormulaKind::None) FormulaIds.AddUnique(Card.CardId);
	}
	View.bReserveSpace=!FormulaIds.IsEmpty();
	TArray<FName> Locked, Completed;
	TArray<FGameXXKResolvedCardSnapshot> Order;
	EGameXXKSorcererTaskBranch Branch=EGameXXKSorcererTaskBranch::None;
	if (const auto* Task=Runtime.SorcererPartnerTasks.FindByPredicate([&](const auto& T){return T.OwnerUnitId==OwnerUnitId && T.bActive;}))
	{
		Locked=Task->LockedCardIds;Completed=Task->CompletedCardIds;Order=Task->FirstPlayOrder;
		View.Task.bActive=true;Branch=Task->LockedBranch;View.Task.Element=FromBranch(Branch);
	}
	else if (Runtime.HeroSpellTask.bActive && Runtime.HeroSpellTask.StarterOwnerUnitId==OwnerUnitId)
	{
		const auto& HeroTask=Runtime.HeroSpellTask;
		Locked=HeroTask.LockedHeroCardIds;Completed=HeroTask.CompletedHeroCardIds;Order=HeroTask.FirstPlayOrder;View.Task.bActive=true;
		View.Task.Element=HeroTask.StarterReward==EGameXXKHeroSpellTaskReward::Fire ? Element::Fire : HeroTask.StarterReward==EGameXXKHeroSpellTaskReward::Ice ? Element::Ice : HeroTask.StarterReward==EGameXXKHeroSpellTaskReward::Lightning ? Element::Lightning : Element::Universal;
	}
	else if (const auto* NpcTask=Runtime.TaskNpcSpellTasks.FindByPredicate([&](const auto& T){return T.OwnerUnitId==OwnerUnitId && T.bActive;}))
	{
		Locked=NpcTask->LockedCardIds;Completed=NpcTask->CompletedCardIds;Order=NpcTask->FirstPlayOrder;View.Task.bActive=true;
	}
	else if (PartnerIds.Num()==5) Locked=PartnerIds;
	else if (HeroIds.Num()==4) Locked=HeroIds;
	else if (NpcIds.Num()==3) Locked=NpcIds;
	View.Task.bAvailable=!Locked.IsEmpty();
	View.bReserveSpace|=View.Task.bAvailable;
	if (View.Task.bAvailable)
	{
		TArray<FString> Unfinished;
		for (const FName CardId:Locked)
		{
			FGameXXKMechanicCardMark& Mark=View.Task.Cards.AddDefaulted_GetRef();Mark.CardId=CardId;Mark.Name=CardName(CardId);
			const int32 Index=Order.IndexOfByPredicate([&](const auto& Snapshot){return Snapshot.CardId==CardId;});
			Mark.PlayOrder=Completed.Contains(CardId) && Index!=INDEX_NONE ? Index+1 : 0;
			Mark.Tooltip=Mark.PlayOrder>0 ? FString::Printf(TEXT("已打出 · 本次任务第%d张。"),Mark.PlayOrder) : TEXT("尚未打出 · ")+CardLocation(Runtime,OwnerUnitId,CardId);
			if (!Mark.PlayOrder) Unfinished.Add(Mark.Name);
		}
		View.Task.bReplaying=View.Task.bActive && Runtime.AutomaticResolutionQueue.bActive && Runtime.AutomaticResolutionQueue.RewardOwnerUnitId==OwnerUnitId && View.Task.CompletedCount()==Locked.Num();
		View.Task.StarterCardId=Order.IsEmpty()?NAME_None:Order[0].CardId;
		View.Task.Title=ElementName(View.Task.Element)+TEXT("法术任务");
		View.Task.Tooltip=FString::Printf(TEXT("进度 %d/%d\n%s"),View.Task.CompletedCount(),Locked.Num(),View.Task.bActive ? TEXT("各牌首次主动打出后记录牌序。") : TEXT("主动打出其中任意一张即可开启。"));
		if (View.Task.bActive && Branch==EGameXXKSorcererTaskBranch::None && PartnerIds.Num()==5) View.Task.Tooltip+=TEXT("\n通用先手，分支由下一张任务牌锁定。");
		if (!Order.IsEmpty()) View.Task.Tooltip+=TEXT("\n首牌：")+CardName(Order[0].CardId);
		if (!Unfinished.IsEmpty()) View.Task.Tooltip+=TEXT("\n待出：")+FString::Join(Unfinished,TEXT("、"));
		else View.Task.Tooltip+=View.Task.bReplaying?TEXT("\n依记录顺序重放中，随后结算阵赏。"):TEXT("\n本组卡牌已全部记录。");
		View.Task.Tooltip+=TEXT("\n")+RewardText(View.Task.StarterCardId,Order.IsEmpty()?EGameXXKCardQuality::Common:Order[0].Quality,Branch);
	}
	TArray<const FGameXXKHealerFormulaRuntime*> Opened;
	for (const auto& Formula:Runtime.HealerFormulas) if (Formula.OwnerUnitId==OwnerUnitId && !Formula.SourceCardId.IsNone())
	{
		if (!FormulaIds.Contains(Formula.SourceCardId)) FormulaIds.Add(Formula.SourceCardId);
		if (!Opened.ContainsByPredicate([&](const auto* Existing){return Existing->SourceCardId==Formula.SourceCardId;})) Opened.Add(&Formula);
	}
	Opened.Sort([&](const auto& A,const auto& B){return FormulaIds.IndexOfByKey(A.SourceCardId)<FormulaIds.IndexOfByKey(B.SourceCardId);});
	for (const auto* Formula:Opened)
	{
		auto& Model=View.Formulas.AddDefaulted_GetRef();Model.CardId=Formula->SourceCardId;Model.Number=FormulaIds.IndexOfByKey(Model.CardId)+1;
		Model.Quality=Formula->SourceQuality==EGameXXKCardQuality::Invalid ? FGameXXKCardQualityRules::GetCardBaseQuality(Model.CardId) : Formula->SourceQuality;
		Model.Title=FString::Printf(TEXT("药方%d · %s"),Model.Number,*CardName(Model.CardId));
		Model.Tooltip=TEXT("已开方 · ")+FGameXXKCardQualityRules::GetDisplayName(Model.Quality).ToString()+TEXT(" · 本局持续\n")+FormulaEffect(Formula->Kind,Model.Quality,Runtime.TeamMaxLevelSnapshot);
		Model.bSpentThisRound=OncePerRound(Formula->Kind) && Formula->LastTriggeredRound==Runtime.RoundNumber;
		if (Formula->Kind==FormulaKind::ThreeEffectiveHealsDraw || Formula->Kind==FormulaKind::HeroDualDotExplosionMedicine)
		{
			const int32 Count=Formula->UnitBudgetRound==Runtime.RoundNumber ? Formula->Progress : 0;
			Model.Tooltip+=FString::Printf(TEXT("\n本回合已触发 %d/2 次。"),Count);Model.bSpentThisRound=Count>=2;
		}
		else if (Formula->Kind==FormulaKind::ThreeCleansedDotMedicine) Model.Tooltip+=FString::Printf(TEXT("\n累计清除进度 %d/3。"),Formula->Progress);
		else if (Formula->Kind==FormulaKind::TwoBleedPacketsMedicine) Model.Tooltip+=FString::Printf(TEXT("\n流血伤害进度 %d/2。"),Formula->Progress);
		else if (OncePerRound(Formula->Kind)) Model.Tooltip+=Model.bSpentThisRound?TEXT("\n本回合已触发，下回合恢复。"):TEXT("\n本回合尚未触发。");
		Model.TriggerState=FString::Printf(TEXT("%d:%d:%d:%d:%d"),Formula->Progress,Formula->PhaseProgress,Formula->LastTriggeredRound,Formula->SecondaryLastTriggeredRound,Formula->TriggeredUnitIdsThisRound.Num());
	}
	View.bReserveSpace|=!View.Formulas.IsEmpty();
	return View;
}

bool GameXXKBattleMechanicPresentation::CaptureCompletion(const FGameXXKUnitMechanicView& Before,
	const FGameXXKUnitMechanicView& After, FGameXXKMechanicTaskView& OutCompleted)
{
	if (!Before.Task.bAvailable || !Before.Task.bActive || After.Task.bActive || !After.bLiving || Before.OwnerUnitId!=After.OwnerUnitId) return false;
	const int32 Count=Before.Task.CompletedCount();
	if (Count==Before.Task.Cards.Num()) OutCompleted=Before.Task;
	else if (Count==Before.Task.Cards.Num()-1 && After.ActivePlayCount>Before.ActivePlayCount && After.LastActiveOwnerId==Before.OwnerUnitId)
	{
		const auto* Missing=Before.Task.Cards.FindByPredicate([&](const auto& Card){return Card.PlayOrder==0 && Card.CardId==After.LastActiveCardId;});
		if (!Missing) return false;
		OutCompleted=Before.Task;
		for (auto& Card:OutCompleted.Cards) if (Card.PlayOrder==0) {Card.PlayOrder=Count+1;Card.Tooltip=FString::Printf(TEXT("已打出 · 本次任务第%d张。"),Card.PlayOrder);}
	}
	else return false;
	OutCompleted.bReplaying=false;OutCompleted.Title=TEXT("法术任务完成");
	OutCompleted.Tooltip=TEXT("本组任务牌已全部打出。\n首牌：")+CardName(OutCompleted.StarterCardId);
	return true;
}
