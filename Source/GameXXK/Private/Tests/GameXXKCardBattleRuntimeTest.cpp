#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleRuntimeTest,
	"GameXXK.Data.CardBattleRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeRuntimeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 HP,
		const int32 MaxHP,
		const int32 Attack,
		const int32 Mana,
		const int32 MaxMana,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = Attack;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeRuntimeInstances(const TCHAR* CardId, const int32 Count, const TCHAR* OwnerUnitId = TEXT("Hero"))
	{
		TArray<FGameXXKCardInstance> Instances;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("Instance.%s.%d"), CardId, Index));
			Instance.CardId = FName(CardId);
			Instance.OwnerUnitId = FName(OwnerUnitId);
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Source.%s.%d"), CardId, Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	FGameXXKCardCombatUnit* FindRuntimeUnit(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool IsInDiscard(const FGameXXKBattleDeckState& Deck, const FName InstanceId)
	{
		return Deck.DiscardPile.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == InstanceId;
		});
	}

	bool EnsureCardIsInHand(FGameXXKBattleDeckState& InOutDeck, const FName CardId)
	{
		if (InOutDeck.Hand.ContainsByPredicate([CardId](const FGameXXKCardInstance& Instance)
		{
			return Instance.CardId == CardId;
		}))
		{
			return true;
		}
		const int32 DrawPileIndex = InOutDeck.DrawPile.IndexOfByPredicate([CardId](const FGameXXKCardInstance& Instance)
		{
			return Instance.CardId == CardId;
		});
		if (DrawPileIndex == INDEX_NONE || InOutDeck.Hand.IsEmpty())
		{
			return false;
		}
		Swap(InOutDeck.Hand.Last(), InOutDeck.DrawPile[DrawPileIndex]);
		return true;
	}

	const FGameXXKCardInstance* FindHandCardById(const FGameXXKBattleDeckState& Deck, const FName CardId)
	{
		return Deck.Hand.FindByPredicate([CardId](const FGameXXKCardInstance& Instance)
		{
			return Instance.CardId == CardId;
		});
	}

	FGameXXKCardDamageContext MakeEnemyAttackContext(
		const TCHAR* EnemyUnitId,
		const EGameXXKCardDamageKind Kind = EGameXXKCardDamageKind::SingleTargetAttack)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = FName(EnemyUnitId);
		Context.Kind = Kind;
		return Context;
	}

	void AddOneShotReflectModifier(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName RecipientUnitId,
		const int32 Percent,
		const TCHAR* ModifierId)
	{
		FGameXXKCardBattleModifierRuntime& Modifier = InOutRuntime.Modifiers.AddDefaulted_GetRef();
		Modifier.ModifierId = FName(ModifierId);
		Modifier.SourceCardInstanceId = InOutRuntime.Deck.ActiveInstanceIds[0];
		Modifier.SourceUnitId = RecipientUnitId;
		Modifier.RecipientUnitIds = { RecipientUnitId };
		Modifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound;
		Modifier.Definition.EffectType = EGameXXKCardEffectType::DamagePercentAttack;
		Modifier.Definition.Target = EGameXXKCardEffectTarget::Attacker;
		Modifier.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
		Modifier.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Modifier.Definition.Magnitude = Percent;
		Modifier.Definition.RemainingTriggers = 1;
		Modifier.Definition.bPersistent = true;
	}
}

bool FGameXXKCardBattleRuntimeTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> AttackUnits;
	AttackUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 10, 20, 1));
	AttackUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime AttackRuntime;
	TestTrue(TEXT("battle runtime initializes a serializable player phase from materialized card instances"),
		GameXXKCardRules::InitializeCardBattleRuntime(AttackRuntime, MakeRuntimeInstances(TEXT("Hero.QingFengYiShi"), 6), AttackUnits, EGameXXKCardTerrain::Plain, 771));
	TestEqual(TEXT("new battle runtime starts in the player card phase"), AttackRuntime.Phase, EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("new battle runtime starts with the shared three energy"), AttackRuntime.Deck.SharedEnergy, 3);
	TestEqual(TEXT("new battle runtime materializes the first five cards into hand"), AttackRuntime.Deck.Hand.Num(), 5);
	const FName AttackInstanceId = AttackRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview AttackPreview;
	TestTrue(TEXT("a hand attack builds a manual target preview without mutating battle state"), GameXXKCardRules::BuildCardPlayPreview(AttackRuntime, AttackInstanceId, AttackPreview));
	TestTrue(TEXT("single-enemy attack preview explicitly requires a player target"), AttackPreview.TargetRequest.bRequiresManualSelection);
	TestTrue(TEXT("single-enemy attack preview exposes the stable enemy UnitId"), GameXXKCardRules::IsManualTargetLegal(AttackPreview.TargetRequest, TEXT("Enemy")));
	TestFalse(TEXT("single-enemy attack preview refuses a friendly stable UnitId"), GameXXKCardRules::IsManualTargetLegal(AttackPreview.TargetRequest, TEXT("Hero")));
	const int32 AttackEnergyBeforeRejectedTarget = AttackRuntime.Deck.SharedEnergy;
	const int32 AttackHandBeforeRejectedTarget = AttackRuntime.Deck.Hand.Num();
	FGameXXKCardPlayResult RejectedAttackResult;
	RejectedAttackResult.CardInstanceId = TEXT("PriorResult");
	TestFalse(TEXT("submitting an illegal selected UnitId does not spend the card"), GameXXKCardRules::ResolveCardPlay(AttackRuntime, AttackInstanceId, TEXT("Hero"), RejectedAttackResult));
	TestEqual(TEXT("illegal selected UnitId leaves shared energy intact"), AttackRuntime.Deck.SharedEnergy, AttackEnergyBeforeRejectedTarget);
	TestEqual(TEXT("illegal selected UnitId leaves the hand intact"), AttackRuntime.Deck.Hand.Num(), AttackHandBeforeRejectedTarget);
	TestEqual(TEXT("illegal selected UnitId preserves caller output"), RejectedAttackResult.CardInstanceId, FName(TEXT("PriorResult")));
	FGameXXKCardPlayResult AttackResult;
	TestTrue(TEXT("submitting a fresh legal stable enemy UnitId resolves the played card"), GameXXKCardRules::ResolveCardPlay(AttackRuntime, AttackInstanceId, TEXT("Enemy"), AttackResult));
	TestEqual(TEXT("a resolved attack spends its shared energy exactly once"), AttackRuntime.Deck.SharedEnergy, 2);
	TestEqual(TEXT("the source gains the declared mana after costs are paid"), FindRuntimeUnit(AttackRuntime.Units, TEXT("Hero"))->Mana, 12);
	TestEqual(TEXT("the target takes the source attack percentage as direct damage"), FindRuntimeUnit(AttackRuntime.Units, TEXT("Enemy"))->HP, 80);
	TestTrue(TEXT("the successful play records the selected stable target"), AttackResult.TargetUnitIds.Contains(TEXT("Enemy")));
	TestTrue(TEXT("the successful play moves the exact hand instance into discard"), IsInDiscard(AttackRuntime.Deck, AttackInstanceId));

	TArray<FGameXXKCardCombatUnit> HealUnits;
	HealUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1));
	HealUnits.Add(MakeRuntimeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 30, 100, 10, 10, 10, 2));
	HealUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("healing fixture adds a removable bleed stack"), GameXXKCardRules::AddCombatStatus(HealUnits[1], EGameXXKCardStatus::Bleed, 1), 1);
	FGameXXKCardBattleRuntime HealRuntime;
	TestTrue(TEXT("healing runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(HealRuntime, MakeRuntimeInstances(TEXT("Hero.GuiYuanShu"), 6), HealUnits, EGameXXKCardTerrain::Plain, 772));
	const FName HealInstanceId = HealRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview HealPreview;
	TestTrue(TEXT("a friendly card builds a fresh manual target preview"), GameXXKCardRules::BuildCardPlayPreview(HealRuntime, HealInstanceId, HealPreview));
	TestTrue(TEXT("friendly preview includes the stable wounded ally"), GameXXKCardRules::IsManualTargetLegal(HealPreview.TargetRequest, TEXT("Ally")));
	const int32 HealEnergyBeforeRejectedTarget = HealRuntime.Deck.SharedEnergy;
	FGameXXKCardPlayResult RejectedHealResult;
	TestFalse(TEXT("enemy target cannot resolve a single-ally card"), GameXXKCardRules::ResolveCardPlay(HealRuntime, HealInstanceId, TEXT("Enemy"), RejectedHealResult));
	TestEqual(TEXT("rejected friendly-card target leaves energy intact"), HealRuntime.Deck.SharedEnergy, HealEnergyBeforeRejectedTarget);
	FGameXXKCardPlayResult HealResult;
	TestTrue(TEXT("a valid friendly target resolves healing and cleansing"), GameXXKCardRules::ResolveCardPlay(HealRuntime, HealInstanceId, TEXT("Ally"), HealResult));
	TestEqual(TEXT("healing clamps at its fixed declared amount"), FindRuntimeUnit(HealRuntime.Units, TEXT("Ally"))->HP, 66);
	TestEqual(TEXT("healing card removes its declared one DoT stack"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(HealRuntime.Units, TEXT("Ally")), EGameXXKCardStatus::Bleed), 0);
	TestEqual(TEXT("healing card spends both shared energy and owner mana"), HealRuntime.Deck.SharedEnergy, 1);
	TestEqual(TEXT("healing card pays owner mana before effects"), FindRuntimeUnit(HealRuntime.Units, TEXT("Hero"))->Mana, 10);

	TArray<FGameXXKCardCombatUnit> SelfUnits;
	SelfUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 10, 20, 1));
	SelfUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime SelfRuntime;
	TestTrue(TEXT("self-target runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(SelfRuntime, MakeRuntimeInstances(TEXT("Hero.FengShenBu"), 6), SelfUnits, EGameXXKCardTerrain::Plain, 773));
	const FName SelfInstanceId = SelfRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview SelfPreview;
	TestTrue(TEXT("self-target card preview succeeds"), GameXXKCardRules::BuildCardPlayPreview(SelfRuntime, SelfInstanceId, SelfPreview));
	TestFalse(TEXT("self-target card preview does not enter the mouse-arrow selection mode"), SelfPreview.TargetRequest.bRequiresManualSelection);
	TestEqual(TEXT("self-target card automatically resolves its owner UnitId"), SelfPreview.TargetRequest.AutomaticTargetUnitIds, TArray<FName>{TEXT("Hero")});
	FGameXXKCardPlayResult SelfResult;
	TestTrue(TEXT("self-target card resolves without a submitted UnitId"), GameXXKCardRules::ResolveCardPlay(SelfRuntime, SelfInstanceId, NAME_None, SelfResult));
	TestEqual(TEXT("self-target card applies its owner status"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(SelfRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::Agility), 1);
	TestEqual(TEXT("moving a card to discard before its draw lets the replacement return to a five-card hand"), SelfRuntime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("self-target card creates one discard entry while drawing its replacement"), SelfRuntime.Deck.DiscardPile.Num(), 1);

	TArray<FGameXXKCardCombatUnit> PacketUnits;
	PacketUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1));
	PacketUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime PacketRuntime;
	TestTrue(TEXT("fixed-damage attack packet runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(PacketRuntime, MakeRuntimeInstances(TEXT("Hero.HeYuZhan"), 6), PacketUnits, EGameXXKCardTerrain::Plain, 774));
	FGameXXKCardPlayResult PacketResult;
	TestTrue(TEXT("one attack packet combines percentage attack and its declared flat damage before mitigation"), GameXXKCardRules::ResolveCardPlay(PacketRuntime, PacketRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), PacketResult));
	TestEqual(TEXT("160 percent of 20 plus six is one thirty-eight damage packet"), FindRuntimeUnit(PacketRuntime.Units, TEXT("Enemy"))->HP, 62);
	TestEqual(TEXT("one attack packet produces one auditable direct-damage result"), PacketResult.DamageResults.Num(), 1);
	if (PacketResult.DamageResults.Num() == 1)
	{
		TestEqual(TEXT("the combined packet reports its pre-mitigation amount"), PacketResult.DamageResults[0].RequestedDamage, 38);
	}

	TArray<FGameXXKCardCombatUnit> OnHitUnits;
	OnHitUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1));
	OnHitUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("on-hit fixture gives its enemy one agility layer"), GameXXKCardRules::AddCombatStatus(OnHitUnits[1], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardBattleRuntime OnHitRuntime;
	TestTrue(TEXT("on-hit attack runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(OnHitRuntime, MakeRuntimeInstances(TEXT("Hero.SuiYanJi"), 6), OnHitUnits, EGameXXKCardTerrain::Plain, 775));
	FGameXXKCardPlayResult OnHitResult;
	TestTrue(TEXT("attack-linked status card resolves against an agile target"), GameXXKCardRules::ResolveCardPlay(OnHitRuntime, OnHitRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), OnHitResult));
	TestEqual(TEXT("agility avoids the whole combined attack packet"), FindRuntimeUnit(OnHitRuntime.Units, TEXT("Enemy"))->HP, 100);
	TestEqual(TEXT("agility also cancels packet-linked vulnerability"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(OnHitRuntime.Units, TEXT("Enemy")), EGameXXKCardStatus::Vulnerability), 0);

	TArray<FGameXXKCardCombatUnit> ConsumptionUnits;
	ConsumptionUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 20, 20, 1));
	ConsumptionUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("consumption fixture gives the hero agility"), GameXXKCardRules::AddCombatStatus(ConsumptionUnits[0], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardBattleRuntime ConsumptionRuntime;
	TestTrue(TEXT("consumption attack runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(ConsumptionRuntime, MakeRuntimeInstances(TEXT("Hero.PoYunYiShan"), 6), ConsumptionUnits, EGameXXKCardTerrain::Plain, 776));
	FGameXXKCardPlayResult ConsumptionResult;
	TestTrue(TEXT("consuming agility upgrades the declared attack packet and enables its dependent draw"), GameXXKCardRules::ResolveCardPlay(ConsumptionRuntime, ConsumptionRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), ConsumptionResult));
	TestEqual(TEXT("consumed agility upgrades 110 percent to 160 percent before mitigation"), FindRuntimeUnit(ConsumptionRuntime.Units, TEXT("Enemy"))->HP, 68);
	TestEqual(TEXT("packet consumption removes exactly the declared agility stack"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(ConsumptionRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::Agility), 0);
	TestEqual(TEXT("dependent draw returns the post-play hand to five"), ConsumptionRuntime.Deck.Hand.Num(), 5);

	TArray<FGameXXKCardCombatUnit> ManaConsumptionUnits;
	ManaConsumptionUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 12, 30, 1));
	ManaConsumptionUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("mana-consumption fixture gives the hero two momentum stacks"), GameXXKCardRules::AddCombatStatus(ManaConsumptionUnits[0], EGameXXKCardStatus::Momentum, 2), 2);
	FGameXXKCardBattleRuntime ManaConsumptionRuntime;
	TestTrue(TEXT("mana-per-consumed-status runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(ManaConsumptionRuntime, MakeRuntimeInstances(TEXT("Hero.JianYiGuanHong"), 6), ManaConsumptionUnits, EGameXXKCardTerrain::Plain, 779));
	FGameXXKCardPlayResult ManaConsumptionResult;
	TestTrue(TEXT("mana-per-consumed-status consumes every declared momentum stack and resolves"), GameXXKCardRules::ResolveCardPlay(ManaConsumptionRuntime, ManaConsumptionRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), ManaConsumptionResult));
	TestEqual(TEXT("cost is paid first, then each consumed momentum grants four mana"), FindRuntimeUnit(ManaConsumptionRuntime.Units, TEXT("Hero"))->Mana, 8);
	TestEqual(TEXT("mana-per-consumed-status removes the consumed momentum stacks"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(ManaConsumptionRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::Momentum), 0);

	TArray<FGameXXKCardCombatUnit> MultiHitUnits;
	MultiHitUnits.Add(MakeRuntimeUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 100, 20, 20, 20, 1));
	MultiHitUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("multi-hit fixture gives its enemy one agility layer"), GameXXKCardRules::AddCombatStatus(MultiHitUnits[1], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardBattleRuntime MultiHitRuntime;
	TestTrue(TEXT("multi-hit runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(MultiHitRuntime, MakeRuntimeInstances(TEXT("Profession.Blade.CanYueSanDie"), 6, TEXT("Blade")), MultiHitUnits, EGameXXKCardTerrain::Plain, 777));
	FGameXXKCardPlayResult MultiHitResult;
	FString MultiHitError;
	const bool bResolvedMultiHit = GameXXKCardRules::ResolveCardPlay(MultiHitRuntime, MultiHitRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), MultiHitResult, &MultiHitError);
	TestTrue(FString::Printf(TEXT("each multi-hit packet resolves and attaches its per-hit bleeding only when the hit lands: %s"), *MultiHitError), bResolvedMultiHit);
	TestEqual(TEXT("the first of three seventy-percent hits is avoided and the next two deal fourteen each"), FindRuntimeUnit(MultiHitRuntime.Units, TEXT("Enemy"))->HP, 72);
	TestEqual(TEXT("the two landed multi-hits apply two bleed layers"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(MultiHitRuntime.Units, TEXT("Enemy")), EGameXXKCardStatus::Bleed), 2);
	TestEqual(TEXT("multi-hit result carries all three hit attempts"), MultiHitResult.DamageResults.Num(), 3);

	TArray<FGameXXKCardCombatUnit> GuardLinkUnits;
	GuardLinkUnits.Add(MakeRuntimeUnit(TEXT("Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 100, 100, 12, 0, 0, 1));
	GuardLinkUnits.Add(MakeRuntimeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 0, 0, 2));
	GuardLinkUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime GuardLinkRuntime;
	TestTrue(TEXT("guard-link runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(GuardLinkRuntime, MakeRuntimeInstances(TEXT("Profession.Guard.HuZhu"), 6, TEXT("Guard")), GuardLinkUnits, EGameXXKCardTerrain::Plain, 778));
	FGameXXKCardPlayResult GuardLinkResult;
	TestTrue(TEXT("a guard card resolves its data-bound guardian and protected ally"), GameXXKCardRules::ResolveCardPlay(GuardLinkRuntime, GuardLinkRuntime.Deck.Hand[0].InstanceId, TEXT("Ally"), GuardLinkResult));
	TestEqual(TEXT("guard card adds exactly one bound redirect record"), GuardLinkRuntime.GuardLinks.Num(), 1);
	if (GuardLinkRuntime.GuardLinks.Num() == 1)
	{
		TestEqual(TEXT("guard card records the stable guardian UnitId"), GuardLinkRuntime.GuardLinks[0].GuardianUnitId, FName(TEXT("Guard")));
		TestEqual(TEXT("guard card records the selected stable protected UnitId"), GuardLinkRuntime.GuardLinks[0].ProtectedUnitId, FName(TEXT("Ally")));
		TestEqual(TEXT("guard card keeps its declared single redirect stack"), GuardLinkRuntime.GuardLinks[0].Stacks, 1);
	}

	TArray<FGameXXKCardCombatUnit> JointAttackUnits;
	JointAttackUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 6, 6, 1));
	JointAttackUnits.Add(MakeRuntimeUnit(TEXT("Partner"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 100, 15, 0, 0, 2));
	JointAttackUnits.Add(MakeRuntimeUnit(TEXT("TaskNpc"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 100, 100, 10, 0, 0, 3));
	JointAttackUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 12, 0, 0, 10));
	FGameXXKCardBattleRuntime JointAttackRuntime;
	TestTrue(TEXT("joint-attack runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(JointAttackRuntime, MakeRuntimeInstances(TEXT("Route.General.HeJiLing"), 6), JointAttackUnits, EGameXXKCardTerrain::Plain, 780));
	FGameXXKCardPlayResult JointAttackResult;
	TestTrue(TEXT("each living party member attacks the one selected enemy from a data-only joint-attack effect"), GameXXKCardRules::ResolveCardPlay(JointAttackRuntime, JointAttackRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), JointAttackResult));
	TestEqual(TEXT("joint attack uses each current party member's own attack value"), FindRuntimeUnit(JointAttackRuntime.Units, TEXT("Enemy"))->HP, 78);
	TestEqual(TEXT("joint attack creates one stable damage audit per living party member"), JointAttackResult.DamageResults.Num(), 3);

	TArray<FGameXXKCardCombatUnit> DiscardChoiceUnits;
	DiscardChoiceUnits.Add(MakeRuntimeUnit(TEXT("JinGui"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 100, 100, 10, 0, 0, 1));
	DiscardChoiceUnits.Add(MakeRuntimeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 80, 100, 20, 0, 0, 2));
	DiscardChoiceUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime DiscardChoiceRuntime;
	TestTrue(TEXT("draw-then-discard runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(DiscardChoiceRuntime, MakeRuntimeInstances(TEXT("Npc.JinGui.ZaYiChouBei"), 7, TEXT("JinGui")), DiscardChoiceUnits, EGameXXKCardTerrain::Plain, 781));
	FGameXXKCardPlayResult DiscardChoiceResult;
	TestTrue(TEXT("draw two discard one card opens an explicit blocking forced-discard choice"), GameXXKCardRules::ResolveCardPlay(DiscardChoiceRuntime, DiscardChoiceRuntime.Deck.Hand[0].InstanceId, TEXT("Ally"), DiscardChoiceResult));
	TestTrue(TEXT("draw-then-discard result reports its pending choice to the HUD"), DiscardChoiceResult.bOpenedPendingChoice);
	TestEqual(TEXT("draw-then-discard is represented by the serialized forced-discard choice"), DiscardChoiceRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("draw-then-discard allows exactly one card to be selected for discard"), DiscardChoiceRuntime.Deck.PendingChoice.RequiredDiscardCount, 1);
	const FName DiscardedAfterDrawId = DiscardChoiceRuntime.Deck.Hand[0].InstanceId;
	TestTrue(TEXT("submitting one stable hand instance completes the forced-discard choice"), GameXXKCardRules::SubmitForcedDiscard(DiscardChoiceRuntime.Deck, { DiscardedAfterDrawId }));
	TestEqual(TEXT("discard choice returns the hand to the normal five-card limit"), DiscardChoiceRuntime.Deck.Hand.Num(), 5);

	TArray<FGameXXKCardCombatUnit> DiscoverUnits;
	DiscoverUnits.Add(MakeRuntimeUnit(TEXT("YueBai"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 100, 100, 10, 0, 0, 1));
	DiscoverUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime DiscoverRuntime;
	TestTrue(TEXT("discover runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(DiscoverRuntime, MakeRuntimeInstances(TEXT("Npc.YueBai.CanJuanPiZhu"), 8, TEXT("YueBai")), DiscoverUnits, EGameXXKCardTerrain::Plain, 782));
	FGameXXKCardPlayResult DiscoverResult;
	TestTrue(TEXT("insight followed by discover preserves one explicit top-card choice"), GameXXKCardRules::ResolveCardPlay(DiscoverRuntime, DiscoverRuntime.Deck.Hand[0].InstanceId, NAME_None, DiscoverResult));
	TestTrue(TEXT("discover result reports its pending insight choice"), DiscoverResult.bOpenedPendingChoice);
	TestEqual(TEXT("discover remains a serialized insight choose-to-hand operation"), DiscoverRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::InsightChooseToHand);
	TestEqual(TEXT("discover exposes the requested top three card candidates"), DiscoverRuntime.Deck.PendingChoice.Candidates.Num(), 3);
	const FName DiscoverPickId = DiscoverRuntime.Deck.PendingChoice.Candidates[0].InstanceId;
	TArray<FName> DiscoverRemainingIds;
	for (int32 DiscoverIndex = 1; DiscoverIndex < DiscoverRuntime.Deck.PendingChoice.Candidates.Num(); ++DiscoverIndex)
	{
		DiscoverRemainingIds.Add(DiscoverRuntime.Deck.PendingChoice.Candidates[DiscoverIndex].InstanceId);
	}
	TestTrue(TEXT("submitting a discover choice moves one offered card to hand and commits the remaining order"), GameXXKCardRules::SubmitInsightChoice(DiscoverRuntime.Deck, DiscoverPickId, DiscoverRemainingIds));
	TestEqual(TEXT("discover returns to a legal no-choice deck state"), DiscoverRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);

	TArray<FGameXXKCardCombatUnit> CostModifierUnits;
	CostModifierUnits.Add(MakeRuntimeUnit(TEXT("SongJinBao"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 100, 100, 10, 30, 30, 1));
	CostModifierUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime CostModifierRuntime;
	TestTrue(TEXT("future-card cost modifier runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(CostModifierRuntime, MakeRuntimeInstances(TEXT("Npc.SongJinBao.YiNuoQianJin"), 6, TEXT("SongJinBao")), CostModifierUnits, EGameXXKCardTerrain::Plain, 783));
	const FName CostModifierFirstInstanceId = CostModifierRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult CostModifierFirstResult;
	TestTrue(TEXT("a delayed shared-deck cost card resolves and registers a persistent modifier"), GameXXKCardRules::ResolveCardPlay(CostModifierRuntime, CostModifierFirstInstanceId, NAME_None, CostModifierFirstResult));
	TestEqual(TEXT("the first delayed card creates one persisted modifier record"), CostModifierRuntime.Modifiers.Num(), 1);
	const FName CostModifierId = CostModifierRuntime.Modifiers.Num() == 1 ? CostModifierRuntime.Modifiers[0].ModifierId : NAME_None;
	const FName CostModifierSecondInstanceId = CostModifierRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayPreview CostModifierPreview;
	TestTrue(TEXT("a subsequent hand card previews with the registered shared-deck energy reduction"), GameXXKCardRules::BuildCardPlayPreview(CostModifierRuntime, CostModifierSecondInstanceId, CostModifierPreview));
	TestEqual(TEXT("the delayed modifier reduces the next card's energy cost without changing its mana cost"), CostModifierPreview.EffectiveEnergyCost, 1);
	FGameXXKCardPlayResult CostModifierSecondResult;
	TestTrue(TEXT("the discounted second card resolves with the one remaining shared energy"), GameXXKCardRules::ResolveCardPlay(CostModifierRuntime, CostModifierSecondInstanceId, NAME_None, CostModifierSecondResult));
	const FGameXXKCardBattleModifierRuntime* ConsumedCostModifier = CostModifierRuntime.Modifiers.FindByPredicate([CostModifierId](const FGameXXKCardBattleModifierRuntime& Modifier)
	{
		return Modifier.ModifierId == CostModifierId;
	});
	TestNotNull(TEXT("the first cost modifier remains persisted after consuming one of its two triggers"), ConsumedCostModifier);
	if (ConsumedCostModifier)
	{
		TestEqual(TEXT("the first cost modifier decrements exactly once after the discounted card commits"), ConsumedCostModifier->Definition.RemainingTriggers, 1);
	}

	TArray<FGameXXKCardInstance> AttackModifierInstances = MakeRuntimeInstances(TEXT("Profession.Blade.JieShiHuiFeng"), 1, TEXT("Blade"));
	AttackModifierInstances.Append(MakeRuntimeInstances(TEXT("Profession.Blade.LieFengZhan"), 6, TEXT("Blade")));
	TArray<FGameXXKCardCombatUnit> AttackModifierUnits;
	AttackModifierUnits.Add(MakeRuntimeUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 100, 100, 20, 0, 0, 1));
	AttackModifierUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime AttackModifierRuntime;
	TestTrue(TEXT("next-attack modifier runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(AttackModifierRuntime, AttackModifierInstances, AttackModifierUnits, EGameXXKCardTerrain::Plain, 784));
	TestTrue(TEXT("next-attack modifier fixture can expose its setup card in the current hand"), EnsureCardIsInHand(AttackModifierRuntime.Deck, TEXT("Profession.Blade.JieShiHuiFeng")));
	TestTrue(TEXT("next-attack modifier fixture can expose its attack card in the current hand"), EnsureCardIsInHand(AttackModifierRuntime.Deck, TEXT("Profession.Blade.LieFengZhan")));
	const FGameXXKCardInstance* AttackModifierSetupInstance = AttackModifierRuntime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Instance)
	{
		return Instance.CardId == TEXT("Profession.Blade.JieShiHuiFeng");
	});
	TestNotNull(TEXT("next-attack modifier fixture retains the setup card in hand"), AttackModifierSetupInstance);
	if (AttackModifierSetupInstance)
	{
		FGameXXKCardPlayResult AttackModifierSetupResult;
		TestTrue(TEXT("a next-attack setup card registers its recipient-bound persistent modifier"), GameXXKCardRules::ResolveCardPlay(AttackModifierRuntime, AttackModifierSetupInstance->InstanceId, NAME_None, AttackModifierSetupResult));
		TestEqual(TEXT("next-attack setup leaves one persistent modifier ready for the blade"), AttackModifierRuntime.Modifiers.Num(), 1);
		const FGameXXKCardInstance* AttackModifierAttackInstance = AttackModifierRuntime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Instance)
		{
			return Instance.CardId == TEXT("Profession.Blade.LieFengZhan");
		});
		TestNotNull(TEXT("next-attack modifier fixture retains an attack card after the setup card leaves hand"), AttackModifierAttackInstance);
		if (AttackModifierAttackInstance)
		{
			FGameXXKCardPlayResult AttackModifierAttackResult;
			TestTrue(TEXT("the next blade attack consumes the registered modifier and resolves"), GameXXKCardRules::ResolveCardPlay(AttackModifierRuntime, AttackModifierAttackInstance->InstanceId, TEXT("Enemy"), AttackModifierAttackResult));
			TestEqual(TEXT("the next-attack modifier upgrades 110 percent to 150 percent before mitigation"), FindRuntimeUnit(AttackModifierRuntime.Units, TEXT("Enemy"))->HP, 70);
			TestEqual(TEXT("a one-trigger next-attack modifier expires after the qualified attack"), AttackModifierRuntime.Modifiers.Num(), 0);
		}
	}

	TArray<FGameXXKCardCombatUnit> LifestealUnits;
	LifestealUnits.Add(MakeRuntimeUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 50, 100, 20, 0, 0, 1));
	LifestealUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime LifestealRuntime;
	TestTrue(TEXT("lifesteal runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(LifestealRuntime, MakeRuntimeInstances(TEXT("Profession.Blade.YinXueDao"), 6, TEXT("Blade")), LifestealUnits, EGameXXKCardTerrain::Plain, 785));
	FGameXXKCardPlayResult LifestealResult;
	TestTrue(TEXT("lifesteal card resolves its attack before calculating its recovery"), GameXXKCardRules::ResolveCardPlay(LifestealRuntime, LifestealRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), LifestealResult));
	TestEqual(TEXT("lifesteal heals 25 percent of direct health damage rather than its percentage field as a fixed amount"), FindRuntimeUnit(LifestealRuntime.Units, TEXT("Blade"))->HP, 56);
	TestEqual(TEXT("lifesteal still applies its declared one-hundred-twenty-percent attack"), FindRuntimeUnit(LifestealRuntime.Units, TEXT("Enemy"))->HP, 76);

	TArray<FGameXXKCardInstance> HealingBonusInstances = MakeRuntimeInstances(TEXT("Profession.Healer.YaoYin"), 1, TEXT("Healer"));
	HealingBonusInstances.Append(MakeRuntimeInstances(TEXT("Profession.Healer.CaoMuFuZhi"), 6, TEXT("Healer")));
	TArray<FGameXXKCardCombatUnit> HealingBonusUnits;
	HealingBonusUnits.Add(MakeRuntimeUnit(TEXT("Healer"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 50, 100, 10, 0, 10, 1));
	HealingBonusUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime HealingBonusRuntime;
	TestTrue(TEXT("next-healing bonus runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(HealingBonusRuntime, HealingBonusInstances, HealingBonusUnits, EGameXXKCardTerrain::Plain, 786));
	TestTrue(TEXT("next-healing fixture can expose its setup card in the current hand"), EnsureCardIsInHand(HealingBonusRuntime.Deck, TEXT("Profession.Healer.YaoYin")));
	TestTrue(TEXT("next-healing fixture can expose its heal card in the current hand"), EnsureCardIsInHand(HealingBonusRuntime.Deck, TEXT("Profession.Healer.CaoMuFuZhi")));
	const FGameXXKCardInstance* HealingBonusSetupInstance = HealingBonusRuntime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Instance)
	{
		return Instance.CardId == TEXT("Profession.Healer.YaoYin");
	});
	TestNotNull(TEXT("next-healing fixture retains the setup card in hand"), HealingBonusSetupInstance);
	if (HealingBonusSetupInstance)
	{
		FGameXXKCardPlayResult HealingBonusSetupResult;
		TestTrue(TEXT("healing setup applies an explicit six-point next-healing bonus to its selected owner"), GameXXKCardRules::ResolveCardPlay(HealingBonusRuntime, HealingBonusSetupInstance->InstanceId, TEXT("Healer"), HealingBonusSetupResult));
		TestEqual(TEXT("next-healing bonus stores its declared magnitude instead of collapsing to one flag stack"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(HealingBonusRuntime.Units, TEXT("Healer")), EGameXXKCardStatus::NextHealingBonus), 6);
		const FGameXXKCardInstance* HealingBonusHealInstance = HealingBonusRuntime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Instance)
		{
			return Instance.CardId == TEXT("Profession.Healer.CaoMuFuZhi");
		});
		TestNotNull(TEXT("next-healing fixture retains the healing card after setup resolves"), HealingBonusHealInstance);
		if (HealingBonusHealInstance)
		{
			FGameXXKCardPlayResult HealingBonusHealResult;
			TestTrue(TEXT("the next healing card consumes the stored flat healing bonus"), GameXXKCardRules::ResolveCardPlay(HealingBonusRuntime, HealingBonusHealInstance->InstanceId, TEXT("Healer"), HealingBonusHealResult));
			TestEqual(TEXT("the next healing bonus adds six to the card's declared sixteen healing"), FindRuntimeUnit(HealingBonusRuntime.Units, TEXT("Healer"))->HP, 72);
			TestEqual(TEXT("next-healing bonus is consumed after the qualified healing action"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(HealingBonusRuntime.Units, TEXT("Healer")), EGameXXKCardStatus::NextHealingBonus), 0);
		}
	}

	TArray<FGameXXKCardCombatUnit> ReflectUnits;
	ReflectUnits.Add(MakeRuntimeUnit(TEXT("Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 100, 100, 20, 10, 10, 1));
	ReflectUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime ReflectRuntime;
	TestTrue(TEXT("first-direct-damage reflection runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(ReflectRuntime, MakeRuntimeInstances(TEXT("Profession.Guard.FanZhenJia"), 6, TEXT("Guard")), ReflectUnits, EGameXXKCardTerrain::Plain, 787));
	FGameXXKCardPlayResult ReflectSetupResult;
	TestTrue(TEXT("reflection armor registers its first-direct-damage modifier"), GameXXKCardRules::ResolveCardPlay(ReflectRuntime, ReflectRuntime.Deck.Hand[0].InstanceId, NAME_None, ReflectSetupResult));
	TArray<FGameXXKCardDamageResult> ReflectPlayerDotResults;
	TestTrue(TEXT("reflection setup can end into one enemy phase"), GameXXKCardRules::EndPlayerCardPhase(ReflectRuntime, ReflectPlayerDotResults));
	FGameXXKCardDamageResult ReflectIncomingResult;
	TArray<FGameXXKCardDamageResult> ReflectReactionResults;
	TestTrue(TEXT("a first enemy direct attack resolves through the reactive modifier"), GameXXKCardRules::ResolveEnemyDirectAttack(ReflectRuntime, MakeEnemyAttackContext(TEXT("Enemy")), TEXT("Guard"), 10, ReflectIncomingResult, &ReflectReactionResults));
	TestEqual(TEXT("armor can absorb the triggering hit while reflection still triggers"), FindRuntimeUnit(ReflectRuntime.Units, TEXT("Guard"))->HP, 100);
	TestEqual(TEXT("the reflection uses the defended unit attack percentage against the attacker"), FindRuntimeUnit(ReflectRuntime.Units, TEXT("Enemy"))->HP, 88);
	TestEqual(TEXT("the reaction creates one separate stable audit packet"), ReflectReactionResults.Num(), 1);
	if (ReflectReactionResults.Num() == 1)
	{
		TestEqual(TEXT("the reflection audit records the guard as its true source"), ReflectReactionResults[0].SourceUnitId, FName(TEXT("Guard")));
	}
	TestEqual(TEXT("a one-shot first-direct-damage modifier is consumed after the first hit"), ReflectRuntime.Modifiers.Num(), 0);

	TArray<FGameXXKCardCombatUnit> RedirectUnits;
	RedirectUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 10, 10, 1));
	RedirectUnits.Add(MakeRuntimeUnit(TEXT("Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 100, 100, 16, 10, 10, 2));
	RedirectUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime RedirectRuntime;
	TestTrue(TEXT("single-target redirect runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(RedirectRuntime, MakeRuntimeInstances(TEXT("Profession.Guard.TieSuoHengJiang"), 6, TEXT("Guard")), RedirectUnits, EGameXXKCardTerrain::Plain, 788));
	FGameXXKCardPlayResult RedirectSetupResult;
	TestTrue(TEXT("guard redirect card resolves before the enemy phase"), GameXXKCardRules::ResolveCardPlay(RedirectRuntime, RedirectRuntime.Deck.Hand[0].InstanceId, NAME_None, RedirectSetupResult));
	TArray<FGameXXKCardDamageResult> RedirectPlayerDotResults;
	TestTrue(TEXT("redirect setup enters the enemy phase without a legacy counterattack"), GameXXKCardRules::EndPlayerCardPhase(RedirectRuntime, RedirectPlayerDotResults));
	FGameXXKCardDamageContext RedirectContext = MakeEnemyAttackContext(TEXT("Enemy"));
	FGameXXKCardStatusStack& RedirectBurn = RedirectContext.OnHitStatuses.AddDefaulted_GetRef();
	RedirectBurn.Status = EGameXXKCardStatus::Burn;
	RedirectBurn.Stacks = 1;
	FGameXXKCardDamageResult RedirectIncomingResult;
	TestTrue(TEXT("an enemy packet redirects through the guard status before normal direct-damage mitigation"), GameXXKCardRules::ResolveEnemyDirectAttack(RedirectRuntime, RedirectContext, TEXT("Hero"), 10, RedirectIncomingResult));
	TestEqual(TEXT("redirect audit preserves the enemy intent's original selected hero"), RedirectIncomingResult.OriginalTargetUnitId, FName(TEXT("Hero")));
	TestEqual(TEXT("redirect audit exposes the stable final guard target"), RedirectIncomingResult.ResolvedTargetUnitId, FName(TEXT("Guard")));
	TestTrue(TEXT("redirect audit flags the interception"), RedirectIncomingResult.bRedirected);
	TestEqual(TEXT("the guard's armor receives the intercepted packet"), FindRuntimeUnit(RedirectRuntime.Units, TEXT("Guard"))->Armor, 4);
	TestEqual(TEXT("one of the card's two redirect layers is consumed"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(RedirectRuntime.Units, TEXT("Guard")), EGameXXKCardStatus::RedirectSingleTargetEnemyAttack), 1);
	TestEqual(TEXT("enemy on-hit statuses remain inside the redirected packet and land on the final target"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(RedirectRuntime.Units, TEXT("Guard")), EGameXXKCardStatus::Burn), 1);

	TArray<FGameXXKCardCombatUnit> EndRoundUnits;
	EndRoundUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 0, 0, 1));
	EndRoundUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime EndRoundRuntime;
	TestTrue(TEXT("end-of-round energy runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(EndRoundRuntime, MakeRuntimeInstances(TEXT("Route.Rare.TieYiYiJue"), 6), EndRoundUnits, EGameXXKCardTerrain::Plain, 789));
	FGameXXKCardPlayResult EndRoundSetupResult;
	TestTrue(TEXT("iron-clad legacy card registers its full-round modifier"), GameXXKCardRules::ResolveCardPlay(EndRoundRuntime, EndRoundRuntime.Deck.Hand[0].InstanceId, NAME_None, EndRoundSetupResult));
	TArray<FGameXXKCardDamageResult> EndRoundPlayerDotResults;
	TestTrue(TEXT("iron-clad card can close the player phase"), GameXXKCardRules::EndPlayerCardPhase(EndRoundRuntime, EndRoundPlayerDotResults));
	TestEqual(TEXT("player armor remains through the intervening enemy phase"), FindRuntimeUnit(EndRoundRuntime.Units, TEXT("Hero"))->Armor, 18);
	TArray<FGameXXKCardDamageResult> EndRoundEnemyDotResults;
	TestTrue(TEXT("the new player phase resolves after the empty enemy phase"), GameXXKCardRules::BeginNextPlayerCardRound(EndRoundRuntime, EndRoundEnemyDotResults));
	TestEqual(TEXT("iron-clad checks armor only after the full enemy phase and grants next-round energy"), EndRoundRuntime.Deck.SharedEnergy, 4);
	TestEqual(TEXT("party armor clears at the next party phase start after the end-round check"), FindRuntimeUnit(EndRoundRuntime.Units, TEXT("Hero"))->Armor, 0);
	TestEqual(TEXT("the full-round energy modifier expires after its one declared trigger"), EndRoundRuntime.Modifiers.Num(), 0);
	TestEqual(TEXT("a new player phase refills the normal hand after unused cards were discarded"), EndRoundRuntime.Deck.Hand.Num(), 5);

	TArray<FGameXXKCardCombatUnit> TimedTerrainUnits;
	TimedTerrainUnits.Add(MakeRuntimeUnit(TEXT("Zhou"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 100, 100, 12, 0, 0, 1));
	TimedTerrainUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime TimedTerrainRuntime;
	TestTrue(TEXT("timed terrain-bonus runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(TimedTerrainRuntime, MakeRuntimeInstances(TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), 6, TEXT("Zhou")), TimedTerrainUnits, EGameXXKCardTerrain::Plain, 790));
	FGameXXKCardPlayResult TimedTerrainResult;
	TestTrue(TEXT("the Zhou card creates its explicit one-round terrain bonus window"), GameXXKCardRules::ResolveCardPlay(TimedTerrainRuntime, TimedTerrainRuntime.Deck.Hand[0].InstanceId, NAME_None, TimedTerrainResult));
	TestEqual(TEXT("Zhou terrain doubling uses the round-scoped status rather than the persistent variant"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TimedTerrainRuntime.Units, TEXT("Zhou")), EGameXXKCardStatus::TerrainBonusDoubleThisRound), 1);
	TArray<FGameXXKCardDamageResult> TimedTerrainPlayerDots;
	TArray<FGameXXKCardDamageResult> TimedTerrainEnemyDots;
	TestTrue(TEXT("timed terrain bonus can pass through the player phase"), GameXXKCardRules::EndPlayerCardPhase(TimedTerrainRuntime, TimedTerrainPlayerDots));
	TestTrue(TEXT("timed terrain bonus expires before the subsequent player phase"), GameXXKCardRules::BeginNextPlayerCardRound(TimedTerrainRuntime, TimedTerrainEnemyDots));
	TestEqual(TEXT("an unused current-round terrain bonus cannot leak into the next round"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TimedTerrainRuntime.Units, TEXT("Zhou")), EGameXXKCardStatus::TerrainBonusDoubleThisRound), 0);

	TArray<FGameXXKCardCombatUnit> TerrainCostUnits;
	TerrainCostUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 0, 20, 1));
	TerrainCostUnits.Add(MakeRuntimeUnit(TEXT("Formation"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 100, 100, 10, 0, 20, 2));
	TerrainCostUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime TerrainCostRuntime;
	TestTrue(TEXT("terrain-cost status runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(TerrainCostRuntime, MakeRuntimeInstances(TEXT("Route.Terrain.DuanYaLuoShi"), 6), TerrainCostUnits, EGameXXKCardTerrain::Plain, 791));
	TestEqual(TEXT("terrain-cost fixture applies a party reduction source"), GameXXKCardRules::AddCombatStatus(*FindRuntimeUnit(TerrainCostRuntime.Units, TEXT("Formation")), EGameXXKCardStatus::NextTerrainCardEnergyReduction, 1), 1);
	TestEqual(TEXT("terrain-cost fixture applies a higher-priority free source"), GameXXKCardRules::AddCombatStatus(*FindRuntimeUnit(TerrainCostRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::NextTerrainCardFree, 1), 1);
	FGameXXKCardPlayPreview TerrainFreePreview;
	TestTrue(TEXT("a route terrain card sees the party free-cost source in preview"), GameXXKCardRules::BuildCardPlayPreview(TerrainCostRuntime, TerrainCostRuntime.Deck.Hand[0].InstanceId, TerrainFreePreview));
	TestEqual(TEXT("a terrain free-cost source overrides the ordinary terrain reduction"), TerrainFreePreview.EffectiveEnergyCost, 0);
	FGameXXKCardPlayResult TerrainFreeResult;
	TestTrue(TEXT("the free terrain card resolves through its stable enemy target"), GameXXKCardRules::ResolveCardPlay(TerrainCostRuntime, TerrainCostRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), TerrainFreeResult));
	TestEqual(TEXT("the chosen free source is consumed exactly once"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TerrainCostRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::NextTerrainCardFree), 0);
	TestEqual(TEXT("the unused reduction source is preserved for the following terrain card"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TerrainCostRuntime.Units, TEXT("Formation")), EGameXXKCardStatus::NextTerrainCardEnergyReduction), 1);
	const FGameXXKCardInstance* TerrainDiscountInstance = FindHandCardById(TerrainCostRuntime.Deck, TEXT("Route.Terrain.DuanYaLuoShi"));
	TestNotNull(TEXT("a second terrain card remains available after the free terrain card resolves"), TerrainDiscountInstance);
	if (TerrainDiscountInstance)
	{
		FGameXXKCardPlayPreview TerrainDiscountPreview;
		TestTrue(TEXT("a remaining terrain reduction lowers the next terrain card by one energy"), GameXXKCardRules::BuildCardPlayPreview(TerrainCostRuntime, TerrainDiscountInstance->InstanceId, TerrainDiscountPreview));
		TestEqual(TEXT("a two-energy terrain card previews at one energy after one reduction"), TerrainDiscountPreview.EffectiveEnergyCost, 1);
		FGameXXKCardPlayResult TerrainDiscountResult;
		TestTrue(TEXT("the discounted terrain card resolves"), GameXXKCardRules::ResolveCardPlay(TerrainCostRuntime, TerrainDiscountInstance->InstanceId, TEXT("Enemy"), TerrainDiscountResult));
		TestEqual(TEXT("the applied terrain reduction consumes after its actual contribution"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TerrainCostRuntime.Units, TEXT("Formation")), EGameXXKCardStatus::NextTerrainCardEnergyReduction), 0);
	}

	TArray<FGameXXKCardCombatUnit> TerrainDoubleUnits;
	TerrainDoubleUnits.Add(MakeRuntimeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 100, 100, 20, 0, 20, 1));
	TerrainDoubleUnits.Add(MakeRuntimeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 100, 100, 10, 0, 20, 2));
	TerrainDoubleUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime TerrainDoubleRuntime;
	TestTrue(TEXT("terrain-bonus double runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(TerrainDoubleRuntime, MakeRuntimeInstances(TEXT("Route.Terrain.DuKouHuiLiu"), 6), TerrainDoubleUnits, EGameXXKCardTerrain::WaterShore, 792));
	TestEqual(TEXT("terrain-bonus fixture adds one persistent doubling status"), GameXXKCardRules::AddCombatStatus(*FindRuntimeUnit(TerrainDoubleRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::TerrainBonusDouble, 1), 1);
	FGameXXKCardPlayResult TerrainDoubleResult;
	TestTrue(TEXT("terrain-bonus double duplicates only the declared water-shore bonus effects"), GameXXKCardRules::ResolveCardPlay(TerrainDoubleRuntime, TerrainDoubleRuntime.Deck.Hand[0].InstanceId, NAME_None, TerrainDoubleResult));
	TestEqual(TEXT("base plus two water-shore mana bonuses give the hero nine mana"), FindRuntimeUnit(TerrainDoubleRuntime.Units, TEXT("Hero"))->Mana, 9);
	TestEqual(TEXT("base plus two water-shore mana bonuses give each ally nine mana"), FindRuntimeUnit(TerrainDoubleRuntime.Units, TEXT("Ally"))->Mana, 9);
	TestEqual(TEXT("terrain-bonus doubling consumes exactly one ready status"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(TerrainDoubleRuntime.Units, TEXT("Hero")), EGameXXKCardStatus::TerrainBonusDouble), 0);

	TArray<FGameXXKCardCombatUnit> XingHuoUnits;
	XingHuoUnits.Add(MakeRuntimeUnit(TEXT("Sorcerer"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 100, 100, 20, 0, 20, 1));
	XingHuoUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	TestEqual(TEXT("starfire fixture gives the target four burn stacks"), GameXXKCardRules::AddCombatStatus(XingHuoUnits[1], EGameXXKCardStatus::Burn, 4), 4);
	FGameXXKCardBattleRuntime XingHuoRuntime;
	TestTrue(TEXT("starfire recovery runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(XingHuoRuntime, MakeRuntimeInstances(TEXT("Profession.Sorcerer.XingHuoHuiShou"), 6, TEXT("Sorcerer")), XingHuoUnits, EGameXXKCardTerrain::Plain, 793));
	FGameXXKCardPlayResult XingHuoResult;
	TestTrue(TEXT("starfire recovery has a base attack packet after consuming burn"), GameXXKCardRules::ResolveCardPlay(XingHuoRuntime, XingHuoRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), XingHuoResult));
	TestEqual(TEXT("starfire recovery turns four consumed burns into an eighty-percent attack bonus"), FindRuntimeUnit(XingHuoRuntime.Units, TEXT("Enemy"))->HP, 64);
	TestEqual(TEXT("starfire recovery gains two mana per consumed burn stack"), FindRuntimeUnit(XingHuoRuntime.Units, TEXT("Sorcerer"))->Mana, 8);
	TestEqual(TEXT("starfire recovery removes exactly the declared burn stacks"), GameXXKCardRules::GetCombatStatusStacks(*FindRuntimeUnit(XingHuoRuntime.Units, TEXT("Enemy")), EGameXXKCardStatus::Burn), 0);

	TArray<FGameXXKCardCombatUnit> LifestealReflectUnits;
	LifestealReflectUnits.Add(MakeRuntimeUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 50, 100, 20, 0, 0, 1));
	LifestealReflectUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 10, 0, 0, 10));
	FGameXXKCardBattleRuntime LifestealReflectRuntime;
	TestTrue(TEXT("lifesteal reflection runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(LifestealReflectRuntime, MakeRuntimeInstances(TEXT("Profession.Blade.YinXueDao"), 6, TEXT("Blade")), LifestealReflectUnits, EGameXXKCardTerrain::Plain, 794));
	AddOneShotReflectModifier(LifestealReflectRuntime, TEXT("Enemy"), 50, TEXT("Modifier.LifestealReflect"));
	FGameXXKCardPlayResult LifestealReflectResult;
	TestTrue(TEXT("lifesteal resolves through an enemy reflection without rolling back its hit"), GameXXKCardRules::ResolveCardPlay(LifestealReflectRuntime, LifestealReflectRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), LifestealReflectResult));
	TestEqual(TEXT("lifesteal only counts the card owner's outgoing enemy health damage, not the enemy counter"), FindRuntimeUnit(LifestealReflectRuntime.Units, TEXT("Blade"))->HP, 51);
	TestEqual(TEXT("the enemy still receives the full lifesteal card attack"), FindRuntimeUnit(LifestealReflectRuntime.Units, TEXT("Enemy"))->HP, 76);
	TestEqual(TEXT("primary damage remains before the reactive counter in the UI audit order"), LifestealReflectResult.DamageResults.Num(), 2);
	if (LifestealReflectResult.DamageResults.Num() == 2)
	{
		TestEqual(TEXT("the first audit packet belongs to the played blade card"), LifestealReflectResult.DamageResults[0].SourceUnitId, FName(TEXT("Blade")));
		TestEqual(TEXT("the second audit packet belongs to the enemy reflection"), LifestealReflectResult.DamageResults[1].SourceUnitId, FName(TEXT("Enemy")));
	}

	TArray<FGameXXKCardCombatUnit> DefeatDuringCardUnits;
	DefeatDuringCardUnits.Add(MakeRuntimeUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 5, 100, 20, 8, 8, 1));
	DefeatDuringCardUnits.Add(MakeRuntimeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100, 100, 20, 0, 0, 10));
	FGameXXKCardBattleRuntime DefeatDuringCardRuntime;
	TestTrue(TEXT("defeat-during-card runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(DefeatDuringCardRuntime, MakeRuntimeInstances(TEXT("Profession.Blade.CanYueSanDie"), 6, TEXT("Blade")), DefeatDuringCardUnits, EGameXXKCardTerrain::Plain, 795));
	const FName DefeatCardInstanceId = DefeatDuringCardRuntime.Deck.Hand[0].InstanceId;
	AddOneShotReflectModifier(DefeatDuringCardRuntime, TEXT("Enemy"), 100, TEXT("Modifier.DefeatReflect"));
	FGameXXKCardPlayResult DefeatDuringCardResult;
	TestTrue(TEXT("a lethal counterattack commits the already-resolved card hit instead of rolling it back"), GameXXKCardRules::ResolveCardPlay(DefeatDuringCardRuntime, DefeatCardInstanceId, TEXT("Enemy"), DefeatDuringCardResult));
	TestEqual(TEXT("a lethal counterattack sets the serializable battle terminal phase"), DefeatDuringCardRuntime.Phase, EGameXXKCardBattlePhase::Defeat);
	TestEqual(TEXT("the first multi-hit packet remains committed before the card owner falls"), FindRuntimeUnit(DefeatDuringCardRuntime.Units, TEXT("Enemy"))->HP, 86);
	TestFalse(TEXT("the reflected source card owner is defeated"), FindRuntimeUnit(DefeatDuringCardRuntime.Units, TEXT("Blade"))->bLiving);
	TestTrue(TEXT("the committed card instance remains in the discard pile after defeat"), IsInDiscard(DefeatDuringCardRuntime.Deck, DefeatCardInstanceId));

	return true;
}

#endif
