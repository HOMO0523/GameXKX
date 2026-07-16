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

	return true;
}

#endif
