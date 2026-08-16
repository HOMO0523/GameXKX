#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardCombatRulesTest,
	"GameXXK.Data.CardCombatRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeCombatUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 MaxHP,
		const int32 StableSortOrder,
		const EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = HP > 0;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.MaxMana = 10;
		Unit.Mana = 10;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardCombatUnit* FindCombatUnit(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	TArray<FGameXXKCardInstance> MakeRuntimeInstances(
		const TCHAR* CardId,
		const int32 Count,
		const TCHAR* OwnerUnitId)
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
}

bool FGameXXKCardCombatRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKCardCombatUnit Hero = MakeCombatUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 100, 1);
	TestEqual(TEXT("momentum accepts the full declared stack amount"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Momentum, 9), 9);
	TestEqual(TEXT("momentum stores its exact uncapped stack amount"), GameXXKCardRules::GetCombatStatusStacks(Hero, EGameXXKCardStatus::Momentum), 9);
	TestEqual(TEXT("agility retains all requested layers without the obsolete two-stack cap"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Agility, 8), 8);
	TestEqual(TEXT("vulnerability has a five-stack cap"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Vulnerability, 8), 5);
	TestEqual(TEXT("mark has a five-stack cap"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Mark, 8), 5);
	TestEqual(TEXT("bleed accepts the full declared stack amount"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Bleed, 12), 12);
	TestEqual(TEXT("bare guard status is rejected because guard must carry a unit binding"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Guard, 1), 0);
	TestEqual(TEXT("armor add clamps at ninety-nine"), GameXXKCardRules::AddCombatArmor(Hero, 120), 99);
	TestEqual(TEXT("armor stores the approved cap"), Hero.Armor, 99);
	GameXXKCardRules::BeginCombatUnitPhase(Hero);
	TestEqual(TEXT("armor clears at the owner phase start"), Hero.Armor, 0);
	TestEqual(TEXT("phase start does not erase persistent momentum"), GameXXKCardRules::GetCombatStatusStacks(Hero, EGameXXKCardStatus::Momentum), 9);
	TestEqual(TEXT("immunity fixture clears the earlier vulnerability-cap probe"), GameXXKCardRules::ConsumeCombatStatus(Hero, EGameXXKCardStatus::Vulnerability, 0), 5);
	TestEqual(TEXT("vulnerability-immunity status is applied"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::CannotReceiveVulnerability, 1), 1);
	TestEqual(TEXT("vulnerability immunity prevents a later vulnerability application"), GameXXKCardRules::AddCombatStatus(Hero, EGameXXKCardStatus::Vulnerability, 3), 0);
	TestEqual(TEXT("vulnerability immunity leaves no vulnerability stacks behind"), GameXXKCardRules::GetCombatStatusStacks(Hero, EGameXXKCardStatus::Vulnerability), 0);
	TestEqual(TEXT("zero maximum consumes every available status stack by catalog convention"), GameXXKCardRules::ConsumeCombatStatus(Hero, EGameXXKCardStatus::Momentum, 0), 9);
	TestEqual(TEXT("zero maximum leaves no consumed momentum behind"), GameXXKCardRules::GetCombatStatusStacks(Hero, EGameXXKCardStatus::Momentum), 0);

	TArray<FGameXXKCardCombatUnit> DirectDamageUnits;
	DirectDamageUnits.Add(MakeCombatUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	DirectDamageUnits.Add(MakeCombatUnit(TEXT("Guard"), EGameXXKCardTargetSide::Party, 73, 100, 2));
	DirectDamageUnits.Add(MakeCombatUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	FGameXXKCardDamageContext SingleTargetAttack;
	SingleTargetAttack.SourceUnitId = TEXT("Enemy");
	SingleTargetAttack.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TestEqual(TEXT("guard armor is added before a redirected direct hit"), GameXXKCardRules::AddCombatArmor(DirectDamageUnits[1], 10), 10);
	FGameXXKCardGuardLinkRuntime GuardLink;
	GuardLink.GuardianUnitId = TEXT("Guard");
	GuardLink.ProtectedUnitId = TEXT("Hero");
	GuardLink.Stacks = 1;
	GuardLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> GuardLinks = { GuardLink };
	FGameXXKCardDamageResult RedirectedHit;
	TestTrue(TEXT("a valid single-target attack resolves through the stable guard binding"), GameXXKCardRules::ApplyCombatDirectDamage(DirectDamageUnits, GuardLinks, SingleTargetAttack, TEXT("Hero"), 10, RedirectedHit));
	TestTrue(TEXT("guard link reports the redirect"), RedirectedHit.bRedirected);
	TestEqual(TEXT("direct hit reports its original stable target"), RedirectedHit.OriginalTargetUnitId, FName(TEXT("Hero")));
	TestEqual(TEXT("direct hit reports the redirect guardian as the resolved target"), RedirectedHit.ResolvedTargetUnitId, FName(TEXT("Guard")));
	TestEqual(TEXT("redirected armor fully absorbs the direct hit"), RedirectedHit.HealthDamage, 0);
	TestEqual(TEXT("redirect snapshot captures the resolved guardian health before mitigation"), RedirectedHit.TargetHealthBefore, 73);
	TestEqual(TEXT("redirect snapshot keeps the resolved guardian health after armor absorption"), RedirectedHit.TargetHealthAfter, 73);
	TestEqual(TEXT("hero remains unharmed behind the guard link"), FindCombatUnit(DirectDamageUnits, TEXT("Hero"))->HP, 100);
	TestEqual(TEXT("guardian armor is consumed once"), FindCombatUnit(DirectDamageUnits, TEXT("Guard"))->Armor, 0);
	TestEqual(TEXT("one-stack guard binding is consumed once"), GuardLinks.Num(), 0);

	FGameXXKCardCombatUnit* DirectHero = FindCombatUnit(DirectDamageUnits, TEXT("Hero"));
	TestNotNull(TEXT("direct-damage test retains the hero unit"), DirectHero);
	if (!DirectHero)
	{
		return false;
	}
	TestEqual(TEXT("vulnerability is applied before the next direct hit"), GameXXKCardRules::AddCombatStatus(*DirectHero, EGameXXKCardStatus::Vulnerability, 2), 2);
	FGameXXKCardDamageResult VulnerableHit;
	TestTrue(TEXT("unprotected direct hit resolves"), GameXXKCardRules::ApplyCombatDirectDamage(DirectDamageUnits, GuardLinks, SingleTargetAttack, TEXT("Hero"), 10, VulnerableHit));
	DirectHero = FindCombatUnit(DirectDamageUnits, TEXT("Hero"));
	TestNotNull(TEXT("direct-damage commit retains the hero under its stable UnitId"), DirectHero);
	if (!DirectHero)
	{
		return false;
	}
	TestEqual(TEXT("two vulnerability stacks amplify ten direct damage to twelve"), VulnerableHit.HealthDamage, 12);
	TestEqual(TEXT("ordinary hit snapshots target health before mutation"), VulnerableHit.TargetHealthBefore, 100);
	TestEqual(TEXT("ordinary hit snapshots target health after mutation"), VulnerableHit.TargetHealthAfter, 88);
	TestEqual(TEXT("direct damage consumes all vulnerability stacks"), GameXXKCardRules::GetCombatStatusStacks(*DirectHero, EGameXXKCardStatus::Vulnerability), 0);
	TestEqual(TEXT("hero receives the amplified health loss"), DirectHero->HP, 88);
	TestEqual(TEXT("agility setup adds exactly one layer"), GameXXKCardRules::AddCombatStatus(*DirectHero, EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardDamageContext AgilityHitContext = SingleTargetAttack;
	FGameXXKCardStatusStack& AvoidedHitVulnerability = AgilityHitContext.OnHitStatuses.AddDefaulted_GetRef();
	AvoidedHitVulnerability.Status = EGameXXKCardStatus::Vulnerability;
	AvoidedHitVulnerability.Stacks = 2;
	FGameXXKCardDamageResult AvoidedHit;
	TestTrue(TEXT("agility can resolve a direct hit without damage"), GameXXKCardRules::ApplyCombatDirectDamage(DirectDamageUnits, GuardLinks, AgilityHitContext, TEXT("Hero"), 80, AvoidedHit));
	DirectHero = FindCombatUnit(DirectDamageUnits, TEXT("Hero"));
	TestNotNull(TEXT("agility resolution retains the hero under its stable UnitId"), DirectHero);
	if (!DirectHero)
	{
		return false;
	}
	TestTrue(TEXT("agility reports that it avoided the direct hit"), AvoidedHit.bAvoidedByAgility);
	TestEqual(TEXT("agility avoids all direct damage before armor"), AvoidedHit.HealthDamage, 0);
	TestEqual(TEXT("agility snapshots health before the avoid"), AvoidedHit.TargetHealthBefore, 88);
	TestEqual(TEXT("agility leaves the immutable after snapshot unchanged"), AvoidedHit.TargetHealthAfter, 88);
	TestEqual(TEXT("agility is consumed exactly once"), GameXXKCardRules::GetCombatStatusStacks(*DirectHero, EGameXXKCardStatus::Agility), 0);
	TestEqual(TEXT("agility preserves the hero health"), DirectHero->HP, 88);
	TestEqual(TEXT("agility cancels attached direct-hit statuses"), GameXXKCardRules::GetCombatStatusStacks(*DirectHero, EGameXXKCardStatus::Vulnerability), 0);

	TArray<FGameXXKCardCombatUnit> GroupDamageUnits;
	GroupDamageUnits.Add(MakeCombatUnit(TEXT("GroupHero"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	GroupDamageUnits.Add(MakeCombatUnit(TEXT("GroupGuard"), EGameXXKCardTargetSide::Party, 100, 100, 2));
	GroupDamageUnits.Add(MakeCombatUnit(TEXT("GroupEnemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	TestEqual(TEXT("group target gains agility for the all-enemy exception"), GameXXKCardRules::AddCombatStatus(GroupDamageUnits[0], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardGuardLinkRuntime GroupGuardLink;
	GroupGuardLink.GuardianUnitId = TEXT("GroupGuard");
	GroupGuardLink.ProtectedUnitId = TEXT("GroupHero");
	GroupGuardLink.Stacks = 1;
	GroupGuardLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> GroupGuardLinks = { GroupGuardLink };
	FGameXXKCardDamageContext GroupAttack;
	GroupAttack.SourceUnitId = TEXT("GroupEnemy");
	GroupAttack.Kind = EGameXXKCardDamageKind::GroupAttack;
	FGameXXKCardDamageResult GroupHit;
	TestTrue(TEXT("a group attack resolves against each individual target"), GameXXKCardRules::ApplyCombatDirectDamage(GroupDamageUnits, GroupGuardLinks, GroupAttack, TEXT("GroupHero"), 10, GroupHit));
	TestFalse(TEXT("a group attack never consumes a single-target guard binding"), GroupHit.bRedirected);
	TestTrue(TEXT("agility can still avoid a direct group attack"), GroupHit.bAvoidedByAgility);
	TestEqual(TEXT("group attack leaves the unspent guard binding intact"), GroupGuardLinks.Num(), 1);
	FGameXXKCardCombatUnit* GroupHero = FindCombatUnit(GroupDamageUnits, TEXT("GroupHero"));
	TestNotNull(TEXT("group attack retains its stable target"), GroupHero);
	if (!GroupHero)
	{
		return false;
	}
	TestEqual(TEXT("group attack consumes the target agility once"), GameXXKCardRules::GetCombatStatusStacks(*GroupHero, EGameXXKCardStatus::Agility), 0);

	TArray<FGameXXKCardCombatUnit> SelfDamageUnits;
	SelfDamageUnits.Add(MakeCombatUnit(TEXT("SelfHero"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	SelfDamageUnits.Add(MakeCombatUnit(TEXT("SelfGuard"), EGameXXKCardTargetSide::Party, 100, 100, 2));
	TestEqual(TEXT("self-damage target gains agility for the exception"), GameXXKCardRules::AddCombatStatus(SelfDamageUnits[0], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardGuardLinkRuntime SelfGuardLink;
	SelfGuardLink.GuardianUnitId = TEXT("SelfGuard");
	SelfGuardLink.ProtectedUnitId = TEXT("SelfHero");
	SelfGuardLink.Stacks = 1;
	SelfGuardLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> SelfGuardLinks = { SelfGuardLink };
	FGameXXKCardDamageContext SelfHealthLoss;
	SelfHealthLoss.SourceUnitId = TEXT("SelfHero");
	SelfHealthLoss.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
	FGameXXKCardDamageResult SelfDamage;
	TestTrue(TEXT("self health loss resolves"), GameXXKCardRules::ApplyCombatDirectDamage(SelfDamageUnits, SelfGuardLinks, SelfHealthLoss, TEXT("SelfHero"), 10, SelfDamage));
	TestFalse(TEXT("self health loss does not redirect through guard"), SelfDamage.bRedirected);
	TestFalse(TEXT("self health loss does not consume agility"), SelfDamage.bAvoidedByAgility);
	FGameXXKCardCombatUnit* SelfHero = FindCombatUnit(SelfDamageUnits, TEXT("SelfHero"));
	TestNotNull(TEXT("self health loss retains its stable target"), SelfHero);
	if (!SelfHero)
	{
		return false;
	}
	TestEqual(TEXT("self health loss bypasses guard and agility"), SelfHero->HP, 90);
	TestEqual(TEXT("self health loss retains its unrelated guard binding"), SelfGuardLinks.Num(), 1);
	TestEqual(TEXT("self health loss retains agility"), GameXXKCardRules::GetCombatStatusStacks(*SelfHero, EGameXXKCardStatus::Agility), 1);
	const int32 SelfGuardHealthBeforeInvalidSelfLoss = FindCombatUnit(SelfDamageUnits, TEXT("SelfGuard"))->HP;
	FGameXXKCardDamageResult InvalidSelfLoss;
	TestFalse(TEXT("self health loss rejects a target other than its stable source unit"), GameXXKCardRules::ApplyCombatDirectDamage(SelfDamageUnits, SelfGuardLinks, SelfHealthLoss, TEXT("SelfGuard"), 10, InvalidSelfLoss));
	TestEqual(TEXT("rejected self health loss leaves the other unit unchanged"), FindCombatUnit(SelfDamageUnits, TEXT("SelfGuard"))->HP, SelfGuardHealthBeforeInvalidSelfLoss);

	TArray<FGameXXKCardCombatUnit> DefenseDamageUnits;
	DefenseDamageUnits.Add(MakeCombatUnit(TEXT("DefenseHero"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	DefenseDamageUnits.Add(MakeCombatUnit(TEXT("DefenseEnemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	DefenseDamageUnits[0].Defense = 10;
	FGameXXKCardDamageContext ArmorPiercingAttack;
	ArmorPiercingAttack.SourceUnitId = TEXT("DefenseEnemy");
	ArmorPiercingAttack.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	ArmorPiercingAttack.IgnoredDefense = 6;
	TArray<FGameXXKCardGuardLinkRuntime> NoDefenseGuardLinks;
	FGameXXKCardDamageResult ArmorPiercingHit;
	TestTrue(TEXT("an attack can ignore a fixed amount of defense"), GameXXKCardRules::ApplyCombatDirectDamage(DefenseDamageUnits, NoDefenseGuardLinks, ArmorPiercingAttack, TEXT("DefenseHero"), 20, ArmorPiercingHit));
	TestEqual(TEXT("direct attack applies defense after flat ignore-defense"), ArmorPiercingHit.DamageAfterDefense, 16);
	TestEqual(TEXT("defense result reaches health before armor"), ArmorPiercingHit.HealthDamage, 16);

	TArray<FGameXXKCardCombatUnit> ArmorOnlyUnits;
	ArmorOnlyUnits.Add(MakeCombatUnit(TEXT("ArmorTarget"), EGameXXKCardTargetSide::Party, 64, 100, 1));
	ArmorOnlyUnits.Add(MakeCombatUnit(TEXT("ArmorEnemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	TestEqual(TEXT("standalone armor-only fixture gains six armor"), GameXXKCardRules::AddCombatArmor(ArmorOnlyUnits[0], 6), 6);
	FGameXXKCardDamageContext ArmorOnlyContext;
	ArmorOnlyContext.SourceUnitId = TEXT("ArmorEnemy");
	ArmorOnlyContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> ArmorOnlyGuardLinks;
	FGameXXKCardDamageResult ArmorOnlyHit;
	TestTrue(TEXT("a standalone armor-only hit resolves"), GameXXKCardRules::ApplyCombatDirectDamage(
		ArmorOnlyUnits, ArmorOnlyGuardLinks, ArmorOnlyContext, TEXT("ArmorTarget"), 5, ArmorOnlyHit));
	TestEqual(TEXT("standalone armor absorbs the complete hit"), ArmorOnlyHit.ArmorAbsorbed, 5);
	TestEqual(TEXT("standalone armor-only hit records no health damage"), ArmorOnlyHit.HealthDamage, 0);
	TestEqual(TEXT("armor-only snapshot captures health before armor mutation"), ArmorOnlyHit.TargetHealthBefore, 64);
	TestEqual(TEXT("armor-only snapshot preserves health after armor mutation"), ArmorOnlyHit.TargetHealthAfter, 64);

	TArray<FGameXXKCardCombatUnit> LethalUnits;
	LethalUnits.Add(MakeCombatUnit(TEXT("LethalTarget"), EGameXXKCardTargetSide::Enemy, 7, 100, 10));
	LethalUnits.Add(MakeCombatUnit(TEXT("LethalAttacker"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	FGameXXKCardDamageContext LethalContext;
	LethalContext.SourceUnitId = TEXT("LethalAttacker");
	LethalContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> LethalGuardLinks;
	FGameXXKCardDamageResult LethalHit;
	TestTrue(TEXT("a lethal hit resolves through the direct-damage rules"), GameXXKCardRules::ApplyCombatDirectDamage(
		LethalUnits, LethalGuardLinks, LethalContext, TEXT("LethalTarget"), 20, LethalHit));
	TestEqual(TEXT("lethal damage is capped to remaining target health"), LethalHit.HealthDamage, 7);
	TestEqual(TEXT("lethal snapshot records the last positive health"), LethalHit.TargetHealthBefore, 7);
	TestEqual(TEXT("lethal snapshot records the zero-health transition"), LethalHit.TargetHealthAfter, 0);

	TArray<FGameXXKCardCombatUnit> MultiHitUnits;
	MultiHitUnits.Add(MakeCombatUnit(TEXT("MultiAttacker"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	MultiHitUnits.Add(MakeCombatUnit(TEXT("MultiTarget"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	TestEqual(TEXT("multi-hit target gains one agility layer"), GameXXKCardRules::AddCombatStatus(
		MultiHitUnits[1], EGameXXKCardStatus::Agility, 1), 1);
	FGameXXKCardDamageContext MultiHitContext;
	MultiHitContext.SourceUnitId = TEXT("MultiAttacker");
	MultiHitContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	TArray<FGameXXKCardGuardLinkRuntime> MultiHitGuardLinks;
	TArray<FGameXXKCardDamageResult> MultiHitResults;
	for (int32 HitIndex = 0; HitIndex < 3; ++HitIndex)
	{
		FGameXXKCardDamageResult& HitResult = MultiHitResults.AddDefaulted_GetRef();
		if (!TestTrue(
			FString::Printf(TEXT("multi-hit packet %d resolves through real combat rules"), HitIndex),
			GameXXKCardRules::ApplyCombatDirectDamage(
				MultiHitUnits, MultiHitGuardLinks, MultiHitContext, TEXT("MultiTarget"), 14, HitResult)))
		{
			return false;
		}
	}
	TestEqual(TEXT("first multi-hit packet snapshots health before agility"), MultiHitResults[0].TargetHealthBefore, 100);
	TestEqual(TEXT("first multi-hit packet snapshots unchanged health after agility"), MultiHitResults[0].TargetHealthAfter, 100);
	TestTrue(TEXT("first multi-hit packet is the avoided packet"), MultiHitResults[0].bAvoidedByAgility);
	TestEqual(TEXT("second multi-hit packet begins from one hundred"), MultiHitResults[1].TargetHealthBefore, 100);
	TestEqual(TEXT("second multi-hit packet ends at eighty-six"), MultiHitResults[1].TargetHealthAfter, 86);
	TestEqual(TEXT("third multi-hit packet begins from eighty-six"), MultiHitResults[2].TargetHealthBefore, 86);
	TestEqual(TEXT("third multi-hit packet ends at seventy-two"), MultiHitResults[2].TargetHealthAfter, 72);

	TArray<FGameXXKCardCombatUnit> DotUnits;
	DotUnits.Add(MakeCombatUnit(TEXT("DotTarget"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	TestEqual(TEXT("DoT target gets armor for the bypass test"), GameXXKCardRules::AddCombatArmor(DotUnits[0], 20), 20);
	TestEqual(TEXT("DoT target gets agility for the bypass test"), GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Agility, 1), 1);
	TestEqual(TEXT("DoT target gets bleed"), GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Bleed, 2), 2);
	TestEqual(TEXT("DoT target gets poison"), GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Poison, 2), 2);
	TestEqual(TEXT("DoT target gets burn"), GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Burn, 1), 1);
	TArray<FGameXXKCardGuardLinkRuntime> DotGuardLinks;
	int32 DotHealthDamage = 0;
	TestTrue(TEXT("DoT resolves against a stable battle unit"), GameXXKCardRules::ApplyCombatEndPhaseDot(DotUnits, DotGuardLinks, TEXT("DotTarget"), DotHealthDamage));
	TestEqual(TEXT("only poison deals owner-end health damage"), DotHealthDamage, 2);
	FGameXXKCardCombatUnit* DotTarget = FindCombatUnit(DotUnits, TEXT("DotTarget"));
	TestNotNull(TEXT("DoT retains its stable target"), DotTarget);
	if (!DotTarget)
	{
		return false;
	}
	TestEqual(TEXT("poison applies its bypass health damage"), DotTarget->HP, 98);
	TestEqual(TEXT("DoT leaves armor untouched"), DotTarget->Armor, 20);
	TestEqual(TEXT("DoT leaves agility untouched"), GameXXKCardRules::GetCombatStatusStacks(*DotTarget, EGameXXKCardStatus::Agility), 1);
	TestEqual(TEXT("bleed does not decay at owner end"), GameXXKCardRules::GetCombatStatusStacks(*DotTarget, EGameXXKCardStatus::Bleed), 2);
	TestEqual(TEXT("poison loses one stack after owner-end damage"), GameXXKCardRules::GetCombatStatusStacks(*DotTarget, EGameXXKCardStatus::Poison), 1);
	TestEqual(TEXT("burn loses one stack without owner-end damage"), GameXXKCardRules::GetCombatStatusStacks(*DotTarget, EGameXXKCardStatus::Burn), 0);

	TArray<FGameXXKCardCombatUnit> DotSnapshotUnits;
	DotSnapshotUnits.Add(MakeCombatUnit(
		TEXT("DotHero"), EGameXXKCardTargetSide::Party, 40, 100, 1, EGameXXKCharacterRole::Hero));
	DotSnapshotUnits.Add(MakeCombatUnit(TEXT("DotEnemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	TestEqual(TEXT("end-phase snapshot fixture gains three poison"), GameXXKCardRules::AddCombatStatus(
		DotSnapshotUnits[0], EGameXXKCardStatus::Poison, 3), 3);
	FGameXXKCardBattleRuntime DotSnapshotRuntime;
	if (!TestTrue(TEXT("end-phase snapshot runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		DotSnapshotRuntime,
		MakeRuntimeInstances(TEXT("Hero.Generic.QingFengYiShi"), 6, TEXT("DotHero")),
		DotSnapshotUnits,
		EGameXXKCardTerrain::Plain,
		8801)))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> DotSnapshotResults;
	if (!TestTrue(TEXT("player end phase resolves DoT audit results"), GameXXKCardRules::EndPlayerCardPhase(
		DotSnapshotRuntime, DotSnapshotResults)))
	{
		return false;
	}
	TestEqual(TEXT("one poisoned party unit creates one DoT audit result"), DotSnapshotResults.Num(), 1);
	if (DotSnapshotResults.Num() == 1)
	{
		TestEqual(TEXT("DoT audit snapshots health before damage"), DotSnapshotResults[0].TargetHealthBefore, 40);
		TestEqual(TEXT("DoT audit snapshots health after damage"), DotSnapshotResults[0].TargetHealthAfter, 37);
	}

	TArray<FGameXXKCardCombatUnit> DotGuardDeathUnits;
	DotGuardDeathUnits.Add(MakeCombatUnit(TEXT("DotProtected"), EGameXXKCardTargetSide::Party, 100, 100, 1));
	DotGuardDeathUnits.Add(MakeCombatUnit(TEXT("DotGuardian"), EGameXXKCardTargetSide::Party, 2, 100, 2));
	TestEqual(TEXT("doomed guardian gains poison"), GameXXKCardRules::AddCombatStatus(DotGuardDeathUnits[1], EGameXXKCardStatus::Poison, 2), 2);
	FGameXXKCardGuardLinkRuntime DotDeathGuardLink;
	DotDeathGuardLink.GuardianUnitId = TEXT("DotGuardian");
	DotDeathGuardLink.ProtectedUnitId = TEXT("DotProtected");
	DotDeathGuardLink.Stacks = 1;
	DotDeathGuardLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> DotDeathGuardLinks = { DotDeathGuardLink };
	int32 DoomedGuardianDotDamage = 0;
	TestTrue(TEXT("DoT resolves on a linked guardian"), GameXXKCardRules::ApplyCombatEndPhaseDot(DotGuardDeathUnits, DotDeathGuardLinks, TEXT("DotGuardian"), DoomedGuardianDotDamage));
	TestEqual(TEXT("DoT caps its health loss at the guardian remaining health"), DoomedGuardianDotDamage, 2);
	FGameXXKCardCombatUnit* DotGuardian = FindCombatUnit(DotGuardDeathUnits, TEXT("DotGuardian"));
	TestNotNull(TEXT("DoT retains the doomed guardian record"), DotGuardian);
	if (!DotGuardian)
	{
		return false;
	}
	TestFalse(TEXT("DoT can defeat a guardian"), DotGuardian->bLiving);
	TestEqual(TEXT("DoT removes bindings for a newly defeated guardian"), DotDeathGuardLinks.Num(), 0);
	const int32 DotTargetHealthBeforeInvalidDot = DotTarget->HP;
	int32 PreservedInvalidDotOutput = 73;
	TestFalse(TEXT("DoT rejects an absent stable target"), GameXXKCardRules::ApplyCombatEndPhaseDot(DotUnits, DotGuardLinks, TEXT("Absent"), PreservedInvalidDotOutput));
	TestEqual(TEXT("rejected DoT preserves the caller output"), PreservedInvalidDotOutput, 73);
	TestEqual(TEXT("rejected DoT preserves target health"), DotTarget->HP, DotTargetHealthBeforeInvalidDot);

	const int32 DeadHeroHealthBefore = DirectHero->HP;
	FGameXXKCardDamageResult InvalidHit;
	InvalidHit.SourceUnitId = TEXT("PriorSource");
	InvalidHit.OriginalTargetUnitId = TEXT("PriorResult");
	InvalidHit.ResolvedTargetUnitId = TEXT("PriorResolvedTarget");
	InvalidHit.RequestedDamage = 91;
	InvalidHit.BaseRequestedDamage = 90;
	InvalidHit.MomentumDamageBonus = 1;
	InvalidHit.DamageAfterWeak = 45;
	InvalidHit.WeakDamageReduction = 46;
	InvalidHit.DamageAfterDefense = 82;
	InvalidHit.DamageAfterVulnerability = 73;
	InvalidHit.MarkStacksBeforeHit = 5;
	InvalidHit.MarkDamageBonusPercent = 15;
	InvalidHit.MarkStacksConsumed = 1;
	InvalidHit.ArmorAbsorbed = 64;
	InvalidHit.HealthDamage = 55;
	InvalidHit.TargetHealthBefore = 731;
	InvalidHit.TargetHealthAfter = 419;
	InvalidHit.bRedirected = true;
	InvalidHit.bAvoidedByAgility = true;
	const FGameXXKCardDamageResult InvalidHitSentinel = InvalidHit;
	TestFalse(TEXT("direct damage rejects an absent stable UnitId"), GameXXKCardRules::ApplyCombatDirectDamage(DirectDamageUnits, GuardLinks, SingleTargetAttack, TEXT("Absent"), 5, InvalidHit));
	TestEqual(TEXT("rejected direct damage leaves battle unit health unchanged"), DirectHero->HP, DeadHeroHealthBefore);
	TestTrue(TEXT("rejected direct damage preserves the entire caller result including snapshot sentinels"),
		FGameXXKCardDamageResult::StaticStruct()->CompareScriptStruct(&InvalidHit, &InvalidHitSentinel, PPF_None));
	TArray<FGameXXKCardCombatUnit> DuplicateStatusUnits = DirectDamageUnits;
	FGameXXKCardStatusStack& FirstDuplicateMomentum = DuplicateStatusUnits[0].Statuses.AddDefaulted_GetRef();
	FirstDuplicateMomentum.Status = EGameXXKCardStatus::Momentum;
	FirstDuplicateMomentum.Stacks = 3;
	FGameXXKCardStatusStack& SecondDuplicateMomentum = DuplicateStatusUnits[0].Statuses.AddDefaulted_GetRef();
	SecondDuplicateMomentum.Status = EGameXXKCardStatus::Momentum;
	SecondDuplicateMomentum.Stacks = 1;
	const int32 DuplicateStatusHeroHP = DuplicateStatusUnits[0].HP;
	FGameXXKCardDamageResult DuplicateStatusHit;
	TestTrue(TEXT("duplicate uncapped momentum entries remain a valid persisted combat state"), GameXXKCardRules::ApplyCombatDirectDamage(DuplicateStatusUnits, GuardLinks, SingleTargetAttack, TEXT("Hero"), 5, DuplicateStatusHit));
	TestEqual(TEXT("a valid duplicate-status fixture resolves the declared direct damage"), DuplicateStatusUnits[0].HP, DuplicateStatusHeroHP - 5);
	TestEqual(TEXT("valid duplicate momentum entries preserve their exact combined stacks"),
		GameXXKCardRules::GetCombatStatusStacks(DuplicateStatusUnits[0], EGameXXKCardStatus::Momentum), 4);

	TArray<FGameXXKCardCombatUnit> ReflectUnits;
	ReflectUnits.Add(MakeCombatUnit(
		TEXT("ReflectGuard"), EGameXXKCardTargetSide::Party, 100, 100, 1, EGameXXKCharacterRole::Guard));
	ReflectUnits.Add(MakeCombatUnit(TEXT("ReflectEnemy"), EGameXXKCardTargetSide::Enemy, 100, 100, 10));
	FGameXXKCardBattleRuntime ReflectRuntime;
	if (!TestTrue(TEXT("real reflection runtime initializes"), GameXXKCardRules::InitializeCardBattleRuntime(
		ReflectRuntime,
		MakeRuntimeInstances(TEXT("Profession.Guard.FanZhenJia"), 6, TEXT("ReflectGuard")),
		ReflectUnits,
		EGameXXKCardTerrain::Plain,
		8802)))
	{
		return false;
	}
	FGameXXKCardPlayResult ReflectSetupResult;
	if (!TestTrue(TEXT("real reflection card registers its reactive modifier"), GameXXKCardRules::ResolveCardPlay(
		ReflectRuntime, ReflectRuntime.Deck.Hand[0].InstanceId, NAME_None, ReflectSetupResult)))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> ReflectPlayerDotResults;
	if (!TestTrue(TEXT("real reflection runtime enters the enemy phase"), GameXXKCardRules::EndPlayerCardPhase(
		ReflectRuntime, ReflectPlayerDotResults)))
	{
		return false;
	}
	FGameXXKCardDamageContext ReflectIncomingContext;
	ReflectIncomingContext.SourceUnitId = TEXT("ReflectEnemy");
	ReflectIncomingContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult ReflectIncomingResult;
	if (!TestTrue(TEXT("real enemy hit triggers the card-driven reflection"), GameXXKCardRules::ResolveEnemyDirectAttack(
		ReflectRuntime,
		ReflectIncomingContext,
		TEXT("ReflectGuard"),
		10,
		ReflectIncomingResult,
		nullptr,
		nullptr,
		true)))
	{
		return false;
	}
	TestEqual(TEXT("armored incoming reflection trigger snapshots health before"), ReflectIncomingResult.TargetHealthBefore, 100);
	TestEqual(TEXT("armored incoming reflection trigger snapshots unchanged health after"), ReflectIncomingResult.TargetHealthAfter, 100);
	FGameXXKCardCombatUnit* ReflectGuardAfterIncoming = FindCombatUnit(ReflectRuntime.Units, TEXT("ReflectGuard"));
	if (!TestNotNull(TEXT("the reflecting guard remains present after the incoming hit"), ReflectGuardAfterIncoming))
	{
		return false;
	}
	TestEqual(TEXT("the marked guard takes the amplified eleven-point hit and keeps one Armor"), ReflectGuardAfterIncoming->Armor, 1);
	TArray<FGameXXKCardDamageResult> ReflectReactionResults;
	if (!TestTrue(TEXT("the completed enemy card opens the reflection boundary"), GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
		ReflectRuntime,
		TEXT("ReflectEnemy"),
		EGameXXKCardDamageKind::SingleTargetAttack,
		TEXT("ReflectGuard"),
		ReflectReactionResults)))
	{
		return false;
	}
	TestEqual(TEXT("real reflection produces one separate damage result"), ReflectReactionResults.Num(), 1);
	if (ReflectReactionResults.Num() == 1)
	{
		TestEqual(TEXT("real reflection source is the defending guard"), ReflectReactionResults[0].SourceUnitId, FName(TEXT("ReflectGuard")));
		TestEqual(TEXT("real reflection resolves against the original attacker"), ReflectReactionResults[0].ResolvedTargetUnitId, FName(TEXT("ReflectEnemy")));
		TestEqual(TEXT("Block deals one hundred percent current Attack plus post-hit Armor"), ReflectReactionResults[0].BaseRequestedDamage, 21);
		TestEqual(TEXT("real reflection snapshots attacker health before retaliation"), ReflectReactionResults[0].TargetHealthBefore, 100);
		TestEqual(TEXT("real reflection snapshots attacker health after retaliation"), ReflectReactionResults[0].TargetHealthAfter, 79);
	}

	return true;
}

#endif
