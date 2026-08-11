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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusEndPhaseRulesTest,
	"GameXXK.Data.CombatStatusRedesign.EndPhaseStatuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatStatusToxicExplosionTest,
	"GameXXK.Data.CombatStatusRedesign.ToxicExplosion",
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
	TestEqual(TEXT("Agility retains every requested layer without the obsolete two-stack cap"),
		GameXXKCardRules::AddCombatStatus(CappedUnit, EGameXXKCardStatus::Agility, 250), 250);
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

	FGameXXKCardBattleRuntime HeroDefeatWithLivingCompanion;
	HeroDefeatWithLivingCompanion.Phase = EGameXXKCardBattlePhase::Enemy;
	HeroDefeatWithLivingCompanion.Units = {
		MakeTerminalUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, false, 1),
		MakeTerminalUnit(TEXT("FormationMaster"), EGameXXKCardTargetSide::Party, true, 2),
		MakeTerminalUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, true, 10)};
	HeroDefeatWithLivingCompanion.Units[1].Role = EGameXXKCharacterRole::FormationMaster;
	GameXXKCardRules::RefreshCombatTerminalPhase(HeroDefeatWithLivingCompanion);
	TestEqual(TEXT("hero defeat ends the battle even while a companion remains alive"),
		HeroDefeatWithLivingCompanion.Phase, EGameXXKCardBattlePhase::Defeat);

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

bool FGameXXKCombatStatusEndPhaseRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> Units = {
		MakeTerminalUnit(TEXT("DotTarget"), EGameXXKCardTargetSide::Party, true, 1)};
	Units[0].HP = 100;
	Units[0].MaxHP = 100;
	Units[0].Defense = 50;
	Units[0].Armor = 99;
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Agility, 1);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Mark, 1);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Bleed, 4);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Poison, 3);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Burn, 2);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::DamageOverTime, 2);
	GameXXKCardRules::AddCombatStatus(Units[0], EGameXXKCardStatus::Weak, 2);

	TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;
	int32 HealthDamage = INDEX_NONE;
	TestTrue(TEXT("the differentiated owner-side end phase resolves"),
		GameXXKCardRules::ApplyCombatEndPhaseDot(
			Units, GuardLinks, TEXT("DotTarget"), HealthDamage));
	TestEqual(TEXT("only Poison plus one Rot amplification packet deals end-phase health damage"),
		HealthDamage, 5);
	TestEqual(TEXT("Poison plus Rot bypass defense and armor"), Units[0].HP, 95);
	TestEqual(TEXT("end-phase status damage does not consume armor"), Units[0].Armor, 99);
	TestEqual(TEXT("end-phase status damage does not consume Agility"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Agility), 1);
	TestEqual(TEXT("end-phase status damage does not consume Mark"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Mark), 1);
	TestEqual(TEXT("Poison deals damage and then loses one stack"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Poison), 2);
	TestEqual(TEXT("Burn loses one stack at owner-side end without dealing damage"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("Rot loses one stack at owner-side end after amplifying Poison"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::DamageOverTime), 1);
	TestEqual(TEXT("Weak duration loses one stack at owner-side end"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Weak), 1);
	TestEqual(TEXT("Bleed neither triggers nor decays at owner-side end"),
		GameXXKCardRules::GetCombatStatusStacks(Units[0], EGameXXKCardStatus::Bleed), 4);
	return true;
}

bool FGameXXKCombatStatusToxicExplosionTest::RunTest(const FString& Parameters)
{
	auto MakeExplosionRuntime = []()
	{
		FGameXXKCardBattleRuntime Runtime;
		Runtime.Phase = EGameXXKCardBattlePhase::Player;
		Runtime.Units = {
			MakeTerminalUnit(TEXT("ExplosionSource"), EGameXXKCardTargetSide::Party, true, 1),
			MakeTerminalUnit(TEXT("ExplosionTarget"), EGameXXKCardTargetSide::Enemy, true, 10)};
		Runtime.Units[1].Defense = 50;
		Runtime.Units[1].Armor = 99;
		GameXXKCardRules::AddCombatStatus(Runtime.Units[1], EGameXXKCardStatus::Agility, 2);
		GameXXKCardRules::AddCombatStatus(Runtime.Units[1], EGameXXKCardStatus::Bleed, 4);
		GameXXKCardRules::AddCombatStatus(Runtime.Units[1], EGameXXKCardStatus::Poison, 3);
		GameXXKCardRules::AddCombatStatus(Runtime.Units[1], EGameXXKCardStatus::Burn, 2);
		GameXXKCardRules::AddCombatStatus(Runtime.Units[1], EGameXXKCardStatus::DamageOverTime, 2);
		return Runtime;
	};

	FGameXXKCardBattleRuntime Runtime = MakeExplosionRuntime();
	TArray<FGameXXKCardDamageResult> Results;
	TestTrue(TEXT("toxic explosion resolves all three snapshot packets"),
		GameXXKCardRules::ResolveToxicExplosion(
			Runtime, TEXT("ExplosionSource"), TEXT("ExplosionTarget"), false, Results));
	TestEqual(TEXT("toxic explosion emits Bleed, Poison, then Burn"), Results.Num(), 3);
	if (Results.Num() != 3)
	{
		return false;
	}
	const TArray<EGameXXKCardDamageCause> ExpectedCauses = {
		EGameXXKCardDamageCause::ToxicExplosionBleed,
		EGameXXKCardDamageCause::ToxicExplosionPoison,
		EGameXXKCardDamageCause::ToxicExplosionBurn};
	const TArray<int32> ExpectedStacks = {4, 3, 2};
	const TArray<int32> ExpectedHealthDamage = {6, 5, 4};
	for (int32 Index = 0; Index < Results.Num(); ++Index)
	{
		TestEqual(TEXT("toxic explosion records the fixed packet cause"), Results[Index].Cause, ExpectedCauses[Index]);
		TestEqual(TEXT("toxic explosion records the pre-explosion status snapshot"), Results[Index].StatusStacksBefore, ExpectedStacks[Index]);
		TestEqual(TEXT("Rot adds its full current value to every present DoT packet"), Results[Index].RotDamageBonus, 2);
		TestEqual(TEXT("each non-preserved packet consumes exactly one matching status stack"), Results[Index].StatusStacksConsumed, 1);
		TestEqual(TEXT("each toxic explosion packet bypasses mitigation"), Results[Index].HealthDamage, ExpectedHealthDamage[Index]);
	}
	TestEqual(TEXT("B4 P3 Burn2 Rot2 toxic explosion deals fifteen total health damage"), Runtime.Units[1].HP, 85);
	TestEqual(TEXT("toxic explosion leaves Bleed at three"),
		GameXXKCardRules::GetCombatStatusStacks(Runtime.Units[1], EGameXXKCardStatus::Bleed), 3);
	TestEqual(TEXT("toxic explosion leaves Poison at two"),
		GameXXKCardRules::GetCombatStatusStacks(Runtime.Units[1], EGameXXKCardStatus::Poison), 2);
	TestEqual(TEXT("toxic explosion leaves Burn at one"),
		GameXXKCardRules::GetCombatStatusStacks(Runtime.Units[1], EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("toxic explosion never directly consumes Rot"),
		GameXXKCardRules::GetCombatStatusStacks(Runtime.Units[1], EGameXXKCardStatus::DamageOverTime), 2);
	TestEqual(TEXT("status health loss leaves armor untouched"), Runtime.Units[1].Armor, 99);
	TestEqual(TEXT("status health loss leaves agility untouched"),
		GameXXKCardRules::GetCombatStatusStacks(Runtime.Units[1], EGameXXKCardStatus::Agility), 2);

	FGameXXKCardBattleRuntime PreservedRuntime = MakeExplosionRuntime();
	TArray<FGameXXKCardDamageResult> PreservedResults;
	TestTrue(TEXT("six-piece preservation resolves the same toxic explosion"),
		GameXXKCardRules::ResolveToxicExplosion(
			PreservedRuntime, TEXT("ExplosionSource"), TEXT("ExplosionTarget"), true, PreservedResults));
	TestEqual(TEXT("preserved toxic explosion still deals fifteen"), PreservedRuntime.Units[1].HP, 85);
	TestEqual(TEXT("preserved toxic explosion keeps Bleed four"),
		GameXXKCardRules::GetCombatStatusStacks(PreservedRuntime.Units[1], EGameXXKCardStatus::Bleed), 4);
	TestEqual(TEXT("preserved toxic explosion keeps Poison three"),
		GameXXKCardRules::GetCombatStatusStacks(PreservedRuntime.Units[1], EGameXXKCardStatus::Poison), 3);
	TestEqual(TEXT("preserved toxic explosion keeps Burn two"),
		GameXXKCardRules::GetCombatStatusStacks(PreservedRuntime.Units[1], EGameXXKCardStatus::Burn), 2);
	TestEqual(TEXT("preserved toxic explosion keeps Rot two"),
		GameXXKCardRules::GetCombatStatusStacks(PreservedRuntime.Units[1], EGameXXKCardStatus::DamageOverTime), 2);
	for (const FGameXXKCardDamageResult& Result : PreservedResults)
	{
		TestEqual(TEXT("preserved toxic explosion reports no consumed status stack"), Result.StatusStacksConsumed, 0);
	}

	FGameXXKCardBattleRuntime LethalRuntime = MakeExplosionRuntime();
	LethalRuntime.Units[1].HP = 1;
	TArray<FGameXXKCardDamageResult> LethalResults;
	TestTrue(TEXT("toxic explosion completes its atomic snapshot after lethal first packet"),
		GameXXKCardRules::ResolveToxicExplosion(
			LethalRuntime, TEXT("ExplosionSource"), TEXT("ExplosionTarget"), false, LethalResults));
	TestEqual(TEXT("lethal toxic explosion still emits all three snapshot packets"), LethalResults.Num(), 3);
	TestEqual(TEXT("lethal toxic explosion consumes one Bleed"),
		GameXXKCardRules::GetCombatStatusStacks(LethalRuntime.Units[1], EGameXXKCardStatus::Bleed), 3);
	TestEqual(TEXT("lethal toxic explosion consumes one Poison"),
		GameXXKCardRules::GetCombatStatusStacks(LethalRuntime.Units[1], EGameXXKCardStatus::Poison), 2);
	TestEqual(TEXT("lethal toxic explosion consumes one Burn"),
		GameXXKCardRules::GetCombatStatusStacks(LethalRuntime.Units[1], EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("lethal toxic explosion keeps Rot"),
		GameXXKCardRules::GetCombatStatusStacks(LethalRuntime.Units[1], EGameXXKCardStatus::DamageOverTime), 2);
	return true;
}

#endif
