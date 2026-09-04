#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkDirectDamageRulesTest,
	"GameXXK.Data.MarkRules.DirectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkEdgeRulesTest,
	"GameXXK.Data.MarkRules.Edges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeMarkUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.bLiving = true;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardCombatUnit* FindMarkUnit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool ResolveMarkedHit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = TEXT("Attacker");
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;
		return GameXXKCardRules::ApplyCombatDirectDamage(
			Units, GuardLinks, Context, TEXT("Target"), RequestedDamage, OutResult);
	}
}

bool FGameXXKMarkDirectDamageRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> FiveMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("five Mark stacks can be prepared"),
		GameXXKCardRules::AddCombatStatus(FiveMarkUnits[1], EGameXXKCardStatus::Mark, 5), 5);
	FGameXXKCardDamageResult FiveMarkResult;
	TestTrue(TEXT("a five-Mark direct hit resolves"), ResolveMarkedHit(FiveMarkUnits, 100, FiveMarkResult));
	TestEqual(TEXT("five stacks still grant one fixed fifteen-percent hit"), FiveMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("one hit consumes only one of five Mark stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(FiveMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 4);

	TArray<FGameXXKCardCombatUnit> OneMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(OneMarkUnits[1], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageResult OneMarkResult;
	TestTrue(TEXT("a one-Mark direct hit resolves"), ResolveMarkedHit(OneMarkUnits, 100, OneMarkResult));
	TestEqual(TEXT("one Mark stack grants the same fixed fifteen percent"), OneMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("the final Mark stack is consumed"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(OneMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 0);

	TArray<FGameXXKCardCombatUnit> CombinedUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Vulnerability, 2);
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Mark, 3);
	FGameXXKCardDamageResult CombinedResult;
	TestTrue(TEXT("Vulnerability and Mark resolve together"), ResolveMarkedHit(CombinedUnits, 100, CombinedResult));
	TestEqual(TEXT("two Vulnerability plus Mark use one additive thirty-five-percent multiplier"),
		CombinedResult.DamageAfterVulnerability, 135);
	FGameXXKCardCombatUnit* CombinedTarget = FindMarkUnit(CombinedUnits, TEXT("Target"));
	TestEqual(TEXT("the hit clears all Vulnerability"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Vulnerability), 0);
	TestEqual(TEXT("the hit removes only one Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Mark), 2);

	return true;
}

bool FGameXXKMarkEdgeRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> ConsecutiveHitUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("two Mark stacks can be prepared for consecutive hits"),
		GameXXKCardRules::AddCombatStatus(ConsecutiveHitUnits[1], EGameXXKCardStatus::Mark, 2), 2);
	FGameXXKCardDamageResult ConsecutiveHitResults[3];
	for (int32 HitIndex = 0; HitIndex < 3; ++HitIndex)
	{
		TestTrue(*FString::Printf(TEXT("consecutive hit %d resolves"), HitIndex + 1),
			ResolveMarkedHit(ConsecutiveHitUnits, 100, ConsecutiveHitResults[HitIndex]));
	}
	TestEqual(TEXT("the first marked hit deals fixed fifteen-percent bonus damage"),
		ConsecutiveHitResults[0].DamageAfterVulnerability, 115);
	TestEqual(TEXT("the second marked hit deals fixed fifteen-percent bonus damage"),
		ConsecutiveHitResults[1].DamageAfterVulnerability, 115);
	TestEqual(TEXT("the third unmarked hit deals base damage"),
		ConsecutiveHitResults[2].DamageAfterVulnerability, 100);
	TestEqual(TEXT("the first hit audits two Mark stacks on its resolved receiver"),
		ConsecutiveHitResults[0].MarkStacksBeforeHit, 2);
	TestEqual(TEXT("the first hit audits the fixed Mark bonus"),
		ConsecutiveHitResults[0].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("the first hit audits one consumed Mark stack"),
		ConsecutiveHitResults[0].MarkStacksConsumed, 1);
	TestEqual(TEXT("the third hit audits no consumed Mark stack"),
		ConsecutiveHitResults[2].MarkStacksConsumed, 0);

	TArray<FGameXXKCardCombatUnit> AvoidedHitUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("the avoided-hit target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(AvoidedHitUnits[1], EGameXXKCardStatus::Mark, 1), 1);
	TestEqual(TEXT("the avoided-hit target gains one Agility"),
		GameXXKCardRules::AddCombatStatus(AvoidedHitUnits[1], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardDamageResult AvoidedHitResult;
	TestTrue(TEXT("a marked hit avoided by Agility resolves"), ResolveMarkedHit(AvoidedHitUnits, 100, AvoidedHitResult));
	TestTrue(TEXT("the avoided hit reports Agility avoidance"), AvoidedHitResult.bAvoidedByAgility);
	TestEqual(TEXT("an avoided hit does not audit Mark stacks before a landed hit"), AvoidedHitResult.MarkStacksBeforeHit, 0);
	TestEqual(TEXT("an avoided hit does not audit a Mark damage bonus"), AvoidedHitResult.MarkDamageBonusPercent, 0);
	TestEqual(TEXT("an avoided hit does not audit consumed Mark"), AvoidedHitResult.MarkStacksConsumed, 0);
	TestEqual(TEXT("Agility avoidance preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(AvoidedHitUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> ArmorHitUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("the armor target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(ArmorHitUnits[1], EGameXXKCardStatus::Mark, 1), 1);
	TestEqual(TEXT("the armor target gains ninety-nine armor"), GameXXKCardRules::AddCombatArmor(ArmorHitUnits[1], 99), 99);
	FGameXXKCardDamageResult ArmorHitResult;
	TestTrue(TEXT("a marked armor-only hit resolves"), ResolveMarkedHit(ArmorHitUnits, 80, ArmorHitResult));
	TestEqual(TEXT("armor absorbs the full amplified marked hit"), ArmorHitResult.ArmorAbsorbed, 92);
	TestEqual(TEXT("the armor-only marked hit deals no health damage"), ArmorHitResult.HealthDamage, 0);
	TestEqual(TEXT("the armor-only marked hit audits one consumed Mark"), ArmorHitResult.MarkStacksConsumed, 1);

	TArray<FGameXXKCardCombatUnit> RedirectUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Enemy, 500, 10),
		MakeMarkUnit(TEXT("Protected"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Guardian"), EGameXXKCardTargetSide::Party, 500, 2)};
	TestEqual(TEXT("the protected unit gains one Mark"),
		GameXXKCardRules::AddCombatStatus(RedirectUnits[1], EGameXXKCardStatus::Mark, 1), 1);
	TestEqual(TEXT("the guardian gains one Mark"),
		GameXXKCardRules::AddCombatStatus(RedirectUnits[2], EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardGuardLinkRuntime RedirectLink;
	RedirectLink.GuardianUnitId = TEXT("Guardian");
	RedirectLink.ProtectedUnitId = TEXT("Protected");
	RedirectLink.Stacks = 1;
	RedirectLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> RedirectLinks = { RedirectLink };
	FGameXXKCardDamageContext RedirectContext;
	RedirectContext.SourceUnitId = TEXT("Attacker");
	RedirectContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult RedirectResult;
	TestTrue(TEXT("a marked guardian receives the redirected direct hit"), GameXXKCardRules::ApplyCombatDirectDamage(
		RedirectUnits, RedirectLinks, RedirectContext, TEXT("Protected"), 100, RedirectResult));
	TestTrue(TEXT("the marked guardian hit reports redirect"), RedirectResult.bRedirected);
	TestEqual(TEXT("the marked guardian is the resolved receiver"), RedirectResult.ResolvedTargetUnitId, FName(TEXT("Guardian")));
	TestEqual(TEXT("the guardian Mark amplifies the redirected hit"), RedirectResult.HealthDamage, 115);
	TestEqual(TEXT("the redirected hit audits the guardian Mark"), RedirectResult.MarkStacksBeforeHit, 1);
	TestEqual(TEXT("the redirected hit audits the fixed Mark bonus"), RedirectResult.MarkDamageBonusPercent, 15);
	TestEqual(TEXT("the redirected hit audits one consumed guardian Mark"), RedirectResult.MarkStacksConsumed, 1);
	TestEqual(TEXT("the redirected hit consumes the guardian Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(RedirectUnits, TEXT("Guardian")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the redirected hit preserves the protected unit Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(RedirectUnits, TEXT("Protected")), EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> GroupHitUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("MarkedEnemy"), EGameXXKCardTargetSide::Enemy, 500, 10),
		MakeMarkUnit(TEXT("UnmarkedEnemy"), EGameXXKCardTargetSide::Enemy, 500, 11)};
	TestEqual(TEXT("the group target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(GroupHitUnits[1], EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardDamageContext GroupContext;
	GroupContext.SourceUnitId = TEXT("Attacker");
	GroupContext.Kind = EGameXXKCardDamageKind::GroupAttack;
	TArray<FGameXXKCardGuardLinkRuntime> GroupLinks;
	FGameXXKCardDamageResult MarkedGroupResult;
	FGameXXKCardDamageResult UnmarkedGroupResult;
	TestTrue(TEXT("the marked group target resolves independently"), GameXXKCardRules::ApplyCombatDirectDamage(
		GroupHitUnits, GroupLinks, GroupContext, TEXT("MarkedEnemy"), 100, MarkedGroupResult));
	TestTrue(TEXT("the unmarked group target resolves independently"), GameXXKCardRules::ApplyCombatDirectDamage(
		GroupHitUnits, GroupLinks, GroupContext, TEXT("UnmarkedEnemy"), 100, UnmarkedGroupResult));
	TestEqual(TEXT("the marked group target takes amplified damage"), MarkedGroupResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("the unmarked group target takes base damage"), UnmarkedGroupResult.DamageAfterVulnerability, 100);
	TestEqual(TEXT("the marked group hit consumes its target Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(GroupHitUnits, TEXT("MarkedEnemy")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the unmarked group target remains unmarked"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(GroupHitUnits, TEXT("UnmarkedEnemy")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the marked group hit audits one consumed Mark"), MarkedGroupResult.MarkStacksConsumed, 1);
	TestEqual(TEXT("the unmarked group hit audits no Mark bonus"), UnmarkedGroupResult.MarkDamageBonusPercent, 0);

	TArray<FGameXXKCardCombatUnit> SelfLossUnits = {
		MakeMarkUnit(TEXT("Self"), EGameXXKCardTargetSide::Party, 500, 1)};
	TestEqual(TEXT("the self-loss target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(SelfLossUnits[0], EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardDamageContext SelfLossContext;
	SelfLossContext.SourceUnitId = TEXT("Self");
	SelfLossContext.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
	TArray<FGameXXKCardGuardLinkRuntime> SelfLossLinks;
	FGameXXKCardDamageResult SelfLossResult;
	TestTrue(TEXT("marked self health loss resolves"), GameXXKCardRules::ApplyCombatDirectDamage(
		SelfLossUnits, SelfLossLinks, SelfLossContext, TEXT("Self"), 100, SelfLossResult));
	TestEqual(TEXT("Mark does not amplify self health loss"), SelfLossResult.DamageAfterVulnerability, 100);
	TestEqual(TEXT("self health loss does not audit Mark before a direct hit"), SelfLossResult.MarkStacksBeforeHit, 0);
	TestEqual(TEXT("self health loss does not audit a Mark bonus"), SelfLossResult.MarkDamageBonusPercent, 0);
	TestEqual(TEXT("self health loss does not audit consumed Mark"), SelfLossResult.MarkStacksConsumed, 0);
	TestEqual(TEXT("self health loss preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(SelfLossUnits[0], EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> EnvironmentalLossUnits = {
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("the environmental-loss target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(EnvironmentalLossUnits[0], EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardDamageContext EnvironmentalLossContext;
	EnvironmentalLossContext.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
	TArray<FGameXXKCardGuardLinkRuntime> EnvironmentalLossLinks;
	FGameXXKCardDamageResult EnvironmentalLossResult;
	TestTrue(TEXT("marked environmental health loss resolves"), GameXXKCardRules::ApplyCombatDirectDamage(
		EnvironmentalLossUnits, EnvironmentalLossLinks, EnvironmentalLossContext, TEXT("Target"), 100, EnvironmentalLossResult));
	TestEqual(TEXT("Mark does not amplify environmental health loss"), EnvironmentalLossResult.DamageAfterVulnerability, 100);
	TestEqual(TEXT("environmental health loss does not audit Mark before a direct hit"), EnvironmentalLossResult.MarkStacksBeforeHit, 0);
	TestEqual(TEXT("environmental health loss does not audit a Mark bonus"), EnvironmentalLossResult.MarkDamageBonusPercent, 0);
	TestEqual(TEXT("environmental health loss does not audit consumed Mark"), EnvironmentalLossResult.MarkStacksConsumed, 0);
	TestEqual(TEXT("environmental health loss preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(EnvironmentalLossUnits[0], EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> DotUnits = {
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("the DoT target gains one Mark"),
		GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Mark, 1), 1);
	TestEqual(TEXT("the DoT target gains one Burn"),
		GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Burn, 1), 1);
	TArray<FGameXXKCardGuardLinkRuntime> DotLinks;
	int32 DotHealthDamage = 0;
	TestTrue(TEXT("end-phase Burn resolves on a marked target"),
		GameXXKCardRules::ApplyCombatEndPhaseDot(DotUnits, DotLinks, TEXT("Target"), DotHealthDamage));
	TestEqual(TEXT("Burn deals no owner-end damage under the trigger-on-card rule"), DotHealthDamage, 0);
	TestEqual(TEXT("owner-end cleanup preserves the non-consuming Burn reservoir"),
		GameXXKCardRules::GetCombatStatusStacks(DotUnits[0], EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("end-phase DoT preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(DotUnits[0], EGameXXKCardStatus::Mark), 1);

	FGameXXKCardCombatUnit PersistentMarkUnit =
		MakeMarkUnit(TEXT("Persistent"), EGameXXKCardTargetSide::Party, 500, 1);
	TestEqual(TEXT("Mark clamps at five stacks"),
		GameXXKCardRules::AddCombatStatus(PersistentMarkUnit, EGameXXKCardStatus::Mark, 9), 5);
	GameXXKCardRules::BeginCombatUnitPhase(PersistentMarkUnit);
	TestEqual(TEXT("Mark persists through its owner's phase start"),
		GameXXKCardRules::GetCombatStatusStacks(PersistentMarkUnit, EGameXXKCardStatus::Mark), 5);

	return true;
}

#endif
