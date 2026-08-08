#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusUnlimitedCapacityTest,
	"GameXXK.Data.CombatStatusRedesign.Capacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusTerminalPrecedenceTest,
	"GameXXK.Data.CombatStatusRedesign.TerminalPrecedence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusDirectAttackOrderTest,
	"GameXXK.Data.CombatStatusRedesign.DirectAttackOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeStatusCapacityUnit()
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = TEXT("Status.Capacity.Unit");
		Unit.Side = EGameXXKCardTargetSide::Party;
		Unit.Role = EGameXXKCharacterRole::Hero;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = 1;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeTerminalUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const bool bLiving,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit = MakeStatusCapacityUnit();
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party
			? EGameXXKCharacterRole::Hero
			: EGameXXKCharacterRole::Invalid;
		Unit.bLiving = bLiving;
		Unit.HP = bLiving ? 100 : 0;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}
}

bool FGameXXKCombatStatusUnlimitedCapacityTest::RunTest(const FString& Parameters)
{
	const TArray<EGameXXKCardStatus> UnlimitedStatuses = {
		EGameXXKCardStatus::Momentum,
		EGameXXKCardStatus::Bleed,
		EGameXXKCardStatus::Poison,
		EGameXXKCardStatus::Burn,
		EGameXXKCardStatus::DamageOverTime};

	for (const EGameXXKCardStatus Status : UnlimitedStatuses)
	{
		FGameXXKCardCombatUnit Unit = MakeStatusCapacityUnit();
		TestEqual(
			TEXT("an approved unlimited status accepts a large application"),
			GameXXKCardRules::AddCombatStatus(Unit, Status, 250),
			250);
		TestEqual(
			TEXT("an approved unlimited status preserves the exact large stack count"),
			GameXXKCardRules::GetCombatStatusStacks(Unit, Status),
			250);
	}

	FGameXXKCardCombatUnit CappedUnit = MakeStatusCapacityUnit();
	TestEqual(TEXT("Mark retains its existing five-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Mark, 250), 5);
	TestEqual(TEXT("Weak retains its existing five-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Weak, 250), 5);
	TestEqual(TEXT("Agility retains its existing two-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Agility, 250), 2);
	return true;
}

bool FGameXXKCombatStatusTerminalPrecedenceTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime SimultaneousDefeat;
	SimultaneousDefeat.Phase = EGameXXKCardBattlePhase::Enemy;
	SimultaneousDefeat.PendingNextPlayerHandEnergySurcharge = 2;
	SimultaneousDefeat.PendingNextPlayerHandEnergySurchargeSourceUnitId = TEXT("Enemy");
	SimultaneousDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, false, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, false, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(SimultaneousDefeat);
	TestEqual(TEXT("simultaneous party and enemy elimination resolves as player victory"),
		SimultaneousDefeat.Phase, EGameXXKCardBattlePhase::Victory);
	TestEqual(TEXT("a terminal transition clears a pending hand surcharge"),
		SimultaneousDefeat.PendingNextPlayerHandEnergySurcharge, 0);
	TestTrue(TEXT("a terminal transition clears the pending surcharge source"),
		SimultaneousDefeat.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone());

	FGameXXKCardBattleRuntime PartyOnlyDefeat;
	PartyOnlyDefeat.Phase = EGameXXKCardBattlePhase::Enemy;
	PartyOnlyDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, false, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, true, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(PartyOnlyDefeat);
	TestEqual(TEXT("party elimination with a living enemy remains defeat"),
		PartyOnlyDefeat.Phase, EGameXXKCardBattlePhase::Defeat);

	FGameXXKCardBattleRuntime EnemyOnlyDefeat;
	EnemyOnlyDefeat.Phase = EGameXXKCardBattlePhase::Player;
	EnemyOnlyDefeat.Units = {
		MakeTerminalUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, true, 1),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, false, 10)};
	GameXXKCardRules::RefreshCombatTerminalPhase(EnemyOnlyDefeat);
	TestEqual(TEXT("enemy elimination with a living party remains victory"),
		EnemyOnlyDefeat.Phase, EGameXXKCardBattlePhase::Victory);
	return true;
}

bool FGameXXKCombatStatusDirectAttackOrderTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeTerminalUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, true, 1),
		MakeTerminalUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, true, 10)};
	Units[0].Attack = 20;
	Units[1].HP = 200;
	Units[1].MaxHP = 200;
	Units[1].Defense = 5;
	Units[1].Armor = 7;
	TestEqual(TEXT("the attacker receives six Momentum"),
		GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Momentum, 6), 6);
	TestEqual(TEXT("the attacker receives two Weak duration stacks"),
		GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Weak, 2), 2);
	TestEqual(TEXT("the target receives two Vulnerability stacks"),
		GameXXKCardRules::AddCombatStatus(Units[1], EGameXXKCardStatus::Vulnerability, 2), 2);
	TestEqual(TEXT("the target receives one Mark stack"),
		GameXXKCardRules::AddCombatStatus(Units[1], EGameXXKCardStatus::Mark, 1), 1);

	FGameXXKCardDamageContext Context;
	Context.SourceUnitId = TEXT("Attacker");
	Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;
	FGameXXKCardDamageResult Result;
	TestTrue(TEXT("the ordered direct hit resolves"), GameXXKCardRules::ApplyCombatDirectDamage(
		Units, GuardLinks, Context, TEXT("Target"), 34, Result));
	TestEqual(TEXT("Momentum is added to base requested damage before Weak"), Result.RequestedDamage, 40);
	TestEqual(TEXT("the direct-hit audit retains the pre-Momentum base damage"), Result.BaseRequestedDamage, 34);
	TestEqual(TEXT("the direct-hit audit records the six-point Momentum contribution"), Result.MomentumDamageBonus, 6);
	TestEqual(TEXT("the direct-hit audit records damage after Weak and before defense"), Result.DamageAfterWeak, 20);
	TestEqual(TEXT("the direct-hit audit records the twenty damage prevented by Weak"), Result.WeakDamageReduction, 20);
	TestEqual(TEXT("Weak halves the Momentum-inclusive damage before defense"), Result.DamageAfterDefense, 15);
	TestEqual(TEXT("Vulnerability and Mark amplify after defense with deterministic flooring"),
		Result.DamageAfterVulnerability, 20);
	TestEqual(TEXT("armor absorbs after all direct-hit amplification"), Result.ArmorAbsorbed, 7);
	TestEqual(TEXT("the remaining thirteen damage reaches health"), Result.HealthDamage, 13);
	TestEqual(TEXT("the target ends at the expected health"), Units[1].HP, 187);
	TestEqual(TEXT("a normal attack does not consume Momentum"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Momentum), 6);
	TestEqual(TEXT("an attack does not consume Weak duration"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Weak), 2);
	TestEqual(TEXT("the direct hit consumes its one Mark"),
		GameXXKCardRules::GetCombatStatusStacks(Units[1], EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the direct hit clears all Vulnerability"),
		GameXXKCardRules::GetCombatStatusStacks(Units[1], EGameXXKCardStatus::Vulnerability), 0);

	TArray<FGameXXKCardCombatUnit> SnapshotUnits = {
		MakeTerminalUnit(TEXT("SnapshotAttacker"), EGameXXKCardTargetSide::Party, true, 1),
		MakeTerminalUnit(TEXT("SnapshotTarget"), EGameXXKCardTargetSide::Enemy, true, 10)};
	FGameXXKCardDamageContext SnapshotContext;
	SnapshotContext.SourceUnitId = TEXT("SnapshotAttacker");
	SnapshotContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	SnapshotContext.MomentumStacksOverride = 4;
	FGameXXKCardDamageResult SnapshotResult;
	TestTrue(TEXT("an explicit packet-start Momentum snapshot resolves"),
		GameXXKCardRules::ApplyCombatDirectDamage(
			SnapshotUnits, GuardLinks, SnapshotContext, TEXT("SnapshotTarget"), 20, SnapshotResult));
	TestEqual(TEXT("the packet-start snapshot supplies Momentum after a card has consumed live stacks"),
		SnapshotResult.RequestedDamage, 24);
	TestEqual(TEXT("the packet-start snapshot is visible in the audit"), SnapshotResult.MomentumDamageBonus, 4);
	return true;
}

#endif
