#include "Misc/AutomationTest.h"
#include "GameXXKAffixCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFixedManaBaseStatsTest,
	"GameXXK.Equipment.FixedMana.BaseStats", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFixedManaBaseStatsTest::RunTest(const FString& Parameters)
{
	for (int32 Level : {1, 20, 100})
	{
		const FGameXXKCharacterStats Hero = FGameXXKCharacterStatRules::GetBareHeroStats(Level);
		TestEqual(TEXT("Hero base Mana remains thirty at every level"), Hero.MaxMana, 30);
		TestEqual(TEXT("Hero Attack still follows its approved progression"), Hero.Attack, 15 + (Level - 1) * 3);
		for (int32 Star : {1, 3, 5})
		{
			FGameXXKCharacterStats Sorcerer;
			if (!TestTrue(TEXT("Sorcerer stat input is legal"), FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Sorcerer, Level, Star, Sorcerer))) return false;
			TestEqual(TEXT("Sorcerer base Mana ignores level and star multipliers"), Sorcerer.MaxMana, 34);
		}
	}
	FGameXXKCompanionAttributes Equipment;
	Equipment.Mana = 100;
	Equipment.Attack = 7;
	FGameXXKCompanionAttributes Actual;
	if (TestTrue(TEXT("legacy companion stat facade resolves"), FGameXXKCompanionRules::GetCompanionAttributes(EGameXXKCharacterRole::Sorcerer, 1, 1, Equipment, Actual)))
	{
		TestEqual(TEXT("legacy facade cannot add equipment Mana to Sorcerer"), Actual.Mana, 34);
		TestEqual(TEXT("other equipment attributes remain active"), Actual.Attack, 22);
	}
	FGameXXKCharacterStats Blade;
	FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Blade, 100, 1, Blade);
	TestEqual(TEXT("all partner Mana uses the level-one base"), Blade.MaxMana, 22);
	if (TestTrue(TEXT("non-Mage legacy facade remains available"), FGameXXKCompanionRules::GetCompanionAttributes(EGameXXKCharacterRole::Blade, 100, 1, Equipment, Actual)))
		TestEqual(TEXT("equipment cannot add Mana to any profession"), Actual.Mana, 22);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFixedManaEquipmentTest,
	"GameXXK.Equipment.FixedMana.EquipmentAndPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFixedManaEquipmentTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.PlayerLevel = 100;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
	FString Error;
	FName LastItemId;
	for (EGameXXKEquipmentSlot Slot : {EGameXXKEquipmentSlot::Belt, EGameXXKEquipmentSlot::Accessory})
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::ShanHe;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 100;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;
		if (!TestTrue(TEXT("create real level100 Mana equipment"), FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, LastItemId, &Error))) { AddError(Error); return false; }
		FGameXXKEquipmentInstance* Item = State.EquipmentCollection.EquipmentInstances.FindByPredicate([LastItemId](const FGameXXKEquipmentInstance& Candidate) { return Candidate.InstanceId == LastItemId; });
		if (!TestNotNull(TEXT("new equipment instance"), Item) || !TestTrue(TEXT("Rare equipment has affixes"), !Item->RolledAffixes.IsEmpty())) return false;
		Item->EnhancementLevel = 10;
		int32 ManaAffix = Item->RolledAffixes.IndexOfByPredicate([](const FGameXXKEquipmentAffixRoll& Roll) { return Roll.AffixId == TEXT("Affix.Universal.MaxMana"); });
		if (ManaAffix == INDEX_NONE) ManaAffix = 0;
		FGameXXKEquipmentAffixRoll& Roll = Item->RolledAffixes[ManaAffix];
		Roll.AffixId = TEXT("Affix.Universal.MaxMana");
		Roll.Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
		Roll.Magnitude = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.Unit, Roll.Tier).Maximum;
		FGameXXKEquipmentTransactionResult Result;
		if (!TestTrue(TEXT("equip Mana gear through the authoritative transaction"), FGameXXKEquipmentEconomyRules::Equip(State, FGameXXKEquipmentRules::HeroCharacterId(), Slot, LastItemId, Result))) { AddError(Result.Message.ToString()); return false; }
	}
	for (bool bSorcerer : {false, true})
	{
		FGameXXKCharacterStats Bare = FGameXXKCharacterStatRules::GetBareHeroStats(100);
		if (bSorcerer) FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Sorcerer, 100, 5, Bare);
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if (!TestTrue(TEXT("shared loadout projector resolves each base stat policy"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(State.EquipmentCollection, FGameXXKEquipmentRules::HeroCharacterId(), Bare, Snapshot, &Error))) { AddError(Error); return false; }
		TestEqual(TEXT("item level, enhancement and percentage affix cannot inflate fixed Mana"), Snapshot.AttributesBeforeRoute.MaxMana, bSorcerer ? 34 : 30);
		TestEqual(TEXT("equipment itself has no resolved Mana contribution"), Snapshot.EnhancedEquipmentBaseStats.MaxMana, 0);
		TestFalse(TEXT("retired Mana affix is not an active modifier"), Snapshot.UniversalModifiers.Contains(EGameXXKEquipmentModifierKind::MaxMana));
		FGameXXKEquipmentTooltipSnapshot Preview;
		if (!TestTrue(TEXT("equipment comparison preview uses the same projection"), FGameXXKEquipmentRules::BuildTooltipSnapshot(State.EquipmentCollection, LastItemId, FGameXXKEquipmentRules::HeroCharacterId(), Bare, Preview, &Error))) { AddError(Error); return false; }
		TestEqual(TEXT("equipment preview preserves the fixed Mana value"), Preview.CandidateCharacterStats.MaxMana, bSorcerer ? 34 : 30);
		TestEqual(TEXT("item tooltip cannot advertise Mana from enhancement"), Preview.ItemCurrentStats.MaxMana, 0);
	}
	FGameXXKCharacterStats Blade;
	FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Blade, 100, 1, Blade);
	FGameXXKEquipmentLoadoutSnapshot BladeSnapshot;
	if (TestTrue(TEXT("non-Mage equipment projection resolves"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(State.EquipmentCollection, FGameXXKEquipmentRules::HeroCharacterId(), Blade, BladeSnapshot, &Error)))
		TestEqual(TEXT("equipment does not add Mana to non-Mage inputs either"), BladeSnapshot.AttributesBeforeRoute.MaxMana, Blade.MaxMana);
	FGameXXKRuntimeState Legacy = UGameXXKMVPRules::CreateNewGame();
	FGameXXKEquipmentTransactionResult Result;
	if (!TestTrue(TEXT("legacy flat Mana item can still be owned"), FGameXXKEquipmentEconomyRules::GrantLegacyEquipmentForCompatibility(Legacy, TEXT("Item.InkstonePendant"), 1, Result))) return false;
	const FName LegacyId = FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(Legacy, TEXT("Item.InkstonePendant"), false);
	if (TestTrue(TEXT("legacy pendant remains equipable"), FGameXXKEquipmentEconomyRules::Equip(Legacy, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Accessory, LegacyId, Result)))
		TestEqual(TEXT("legacy flat plus20 is also equipment and does not increase Hero Mana"), Legacy.PlayerMaxMP, 30);
	FGameXXKEquipmentTooltipSnapshot LegacyPreview;
	if (TestTrue(TEXT("legacy pendant tooltip resolves"), FGameXXKEquipmentRules::BuildTooltipSnapshot(Legacy.EquipmentCollection, LegacyId, FGameXXKEquipmentRules::HeroCharacterId(), FGameXXKCharacterStatRules::GetBareHeroStats(1), LegacyPreview, &Error)))
		TestEqual(TEXT("legacy item tooltip has no Mana bonus"), LegacyPreview.ItemCurrentStats.MaxMana, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFixedManaMirrorsTest,
	"GameXXK.Equipment.FixedMana.MirrorsAndFixedRouteBonus", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFixedManaMirrorsTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.PlayerLevel = 100;
	State.PlayerMaxMP = 30;
	State.PlayerMP = 21;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
	TestEqual(TEXT("level growth keeps Hero maximum thirty"), State.PlayerMaxMP, 30);
	TestEqual(TEXT("level growth does not restore or remove current Mana"), State.PlayerMP, 21);
	State.CardRun.RouteAttributeBonuses.MaxMana = 6;
	if (!TestTrue(TEXT("fixed route bonus can be synchronized"), FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))) return false;
	TestEqual(TEXT("route bonus stays outside the permanent baseline"), State.PlayerMaxMP, 30);
	TestEqual(TEXT("explicit route plus6 is preserved"), State.CardRun.RouteAttributeBonuses.MaxMana, 6);
	TestEqual(TEXT("raising capacity does not invent a recovery"), State.PlayerMP, 21);
	State.PlayerMaxMP = 525;
	State.PlayerMP = 500;
	if (!TestTrue(TEXT("old grown Mana projection can be reconciled"), FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))) return false;
	TestEqual(TEXT("old grown maximum becomes the fixed baseline"), State.PlayerMaxMP, 30);
	TestEqual(TEXT("existing Mana clamps to new maximum including explicit route bonus"), State.PlayerMP, 36);
	if (!TestTrue(TEXT("reconciliation repeats safely"), FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))) return false;
	TestEqual(TEXT("repeat reconciliation does not multiply or remove the route bonus"), State.PlayerMP, 36);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFixedManaBattleProjectionTest,
	"GameXXK.Equipment.FixedMana.BattleProjectionAndSavedCapacity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFixedManaBattleProjectionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("card run initializes"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error))) { AddError(Error); return false; }
	FGameXXKCompanionRosterState& Roster = State.CardRun.CompanionRoster;
	FGameXXKPermanentCompanion* Sorcerer = Roster.PermanentCompanions.FindByPredicate([](const FGameXXKPermanentCompanion& Entry) { return Entry.Role == EGameXXKCharacterRole::Sorcerer; });
	if (!Sorcerer)
	{
		FGameXXKCompanionRecruitResult Recruited;
		if (!TestTrue(TEXT("recruit a real Sorcerer profile"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, TEXT("Companion.Sorcerer.01"), 6210601, Recruited, &Error))) { AddError(Error); return false; }
		Sorcerer = Roster.PermanentCompanions.FindByPredicate([](const FGameXXKPermanentCompanion& Entry) { return Entry.Role == EGameXXKCharacterRole::Sorcerer; });
	}
	if (!TestNotNull(TEXT("owned Sorcerer"), Sorcerer)) return false;
	Sorcerer->Level = 100;
	Sorcerer->Star = 5;
	Sorcerer->Experience = 0;
	if (!TestTrue(TEXT("refresh the level100 unlock frontier"), FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*Sorcerer, &Error))) { AddError(Error); return false; }
	const FName SorcererId = Sorcerer->InstanceId;
	if (Roster.PermanentCompanions.Num() < 2)
	{
		FGameXXKCompanionRecruitResult Recruited;
		if (!TestTrue(TEXT("owned reserve profile satisfies save roster"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, TEXT("Companion.Blade.01"), 6210602, Recruited, &Error))) { AddError(Error); return false; }
	}
	if (!TestTrue(TEXT("select one active Sorcerer"), FGameXXKCompanionRules::SetActivePermanentCompanion(Roster, SorcererId, &Error))
		|| !TestTrue(TEXT("select the fixed NPC"), GameXXKPermanentPartyTestFixtures::SelectNpc(State, TEXT("Npc.TusiChief"), &Error))
		|| !TestTrue(TEXT("normalize the legal three-person formation"), FGameXXKPartyFormationRules::Normalize(State, &Error))) { AddError(Error); return false; }
	FGameXXKPartyFormationRules::ProjectCompatibility(State);
	State.PlayerLevel = 100;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
	State.CardRun.RouteAttributeBonuses.MaxMana = 6;
	State.PlayerMaxMP = 525;
	State.PlayerMP = 500;
	FGameXXKBattleRuntimeUnit Enemy;
	Enemy.Id = TEXT("FixedMana.Enemy");
	Enemy.DisplayName = FText::FromString(TEXT("测试敌人"));
	Enemy.HP = Enemy.MaxHP = 100000;
	Enemy.Attack = 1;
	Enemy.Speed = 1;
	Enemy.bEnemy = true;
	State.ActiveBattleParty.Reset();
	State.ActiveBattleEnemies = {Enemy};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = INDEX_NONE;
	if (!TestTrue(TEXT("real adapter starts the battle"), FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 62106, &Error))) { AddError(Error); return false; }
	const auto Find = [](FGameXXKRuntimeState& Runtime, FName Id) -> FGameXXKCardCombatUnit*
	{
		return Runtime.CardRun.ActiveBattle.Units.FindByPredicate([Id](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == Id; });
	};
	FGameXXKCardCombatUnit* Hero = Find(State, FGameXXKEquipmentRules::HeroCharacterId());
	FGameXXKCardCombatUnit* Mage = Find(State, SorcererId);
	if (!TestNotNull(TEXT("projected Hero"), Hero) || !TestNotNull(TEXT("projected Sorcerer"), Mage)) return false;
	TestEqual(TEXT("battle Hero includes exactly the fixed route plus6"), Hero->MaxMana, 36);
	TestEqual(TEXT("old current Mana clamps to the new battle maximum"), Hero->Mana, 36);
	TestEqual(TEXT("level100 star5 Sorcerer enters at34"), Mage->MaxMana, 34);
	// Represent the already-earned +4 and +8 battle effects; save must preserve them.
	Mage->MaxMana += 12;
	Mage->Mana = 43;
	if (!TestTrue(TEXT("refreshing permanent mirrors is valid during battle"), FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))) return false;
	TestEqual(TEXT("mirror refresh preserves explicit battle capacity"), Find(State, SorcererId)->MaxMana, 46);
	Hero = Find(State, FGameXXKEquipmentRules::HeroCharacterId());
	Hero->MaxMana += 12;
	Hero->Mana = 40;
	if (!TestTrue(TEXT("synchronize a battle-local Hero capacity bonus"), FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error))) { AddError(Error); return false; }
	TestEqual(TEXT("permanent mirror is bounded by base plus route capacity"), State.PlayerMP, 36);
	TestEqual(TEXT("synchronizing mirrors preserves current battle Mana"), Hero->Mana, 40);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("save and restore the active battle"), FGameXXKSaveMigration::TryRestoreRuntimeState(UGameXXKMVPRules::MakeSaveState(State), Restored, Report))) { AddError(Report.Error); return false; }
	const FGameXXKCardCombatUnit* RestoredMage = Find(Restored, SorcererId);
	if (TestNotNull(TEXT("restored Sorcerer"), RestoredMage))
	{
		TestEqual(TEXT("load preserves the explicit +12 battle capacity"), RestoredMage->MaxMana, 46);
		TestEqual(TEXT("load preserves current battle Mana"), RestoredMage->Mana, 43);
	}
	const FGameXXKCardCombatUnit* RestoredHero = Find(Restored, FGameXXKEquipmentRules::HeroCharacterId());
	if (TestNotNull(TEXT("restored Hero"), RestoredHero))
	{
		TestEqual(TEXT("load preserves explicit Hero battle capacity"), RestoredHero->MaxMana, 48);
		TestEqual(TEXT("load preserves Hero battle Mana beyond the permanent mirror"), RestoredHero->Mana, 40);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKNoEquipmentManaCatalogTest,
	"GameXXK.Equipment.FixedMana.NoEquipmentManaCatalogAndRolls", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNoEquipmentManaCatalogTest::RunTest(const FString& Parameters)
{
	for (const FGameXXKEquipmentDefinition& Definition : FGameXXKEquipmentCatalog::GetPackageDefinitions())
		for (int32 Level : {1, 20, 100})
			TestEqual(Definition.Id.ToString() + TEXT(" never generates base equipment Mana"), Definition.BaseStatCoefficients.Resolve(Level).MaxMana, 0);
	TestFalse(TEXT("Mana affix is absent from new-roll pools"), FGameXXKAffixCatalog::GetUniversalDefinitions().ContainsByPredicate([](const FGameXXKAffixDefinition& Affix) { return Affix.ModifierKind == EGameXXKEquipmentModifierKind::MaxMana; }));
	TestNotNull(TEXT("old Mana affix remains readable for saved equipment"), FGameXXKAffixCatalog::FindDefinition(TEXT("Affix.Universal.MaxMana")));
	bool bFound = false;
	const FGameXXKItemDef Pendant = UGameXXKMVPRules::GetItemDef(TEXT("Item.InkstonePendant"), bFound);
	if (TestTrue(TEXT("legacy item remains identifiable"), bFound)) TestEqual(TEXT("legacy facade no longer advertises plus20 Mana"), Pendant.MaxMPBonus, 0);
	FGameXXKEquipmentCollectionState Collection;
	Collection.CollectionSeed = 62107;
	FGameXXKEquipmentCreateRequest Request;
	Request.Set = EGameXXKEquipmentSet::ShanHe;
	Request.Quality = EGameXXKEquipmentQuality::Immortal;
	Request.ItemLevel = 100;
	FString Error;
	for (int32 Index = 0; Index < 24; ++Index)
	{
		FName Id;
		if (!TestTrue(TEXT("real five-affix combat equipment generation remains valid"), FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, Id, &Error))) { AddError(Error); return false; }
		const FGameXXKEquipmentInstance* Item = FGameXXKEquipmentRules::FindInstance(Collection, Id);
		if (TestNotNull(TEXT("generated item"), Item))
			TestFalse(TEXT("generated equipment never rolls retired Mana"), Item->RolledAffixes.ContainsByPredicate([](const FGameXXKEquipmentAffixRoll& Roll) { return Roll.AffixId == TEXT("Affix.Universal.MaxMana"); }));
	}
	Request.Set = EGameXXKEquipmentSet::Starter;
	Request.Quality = EGameXXKEquipmentQuality::Legendary;
	FName Id;
	TestFalse(TEXT("a request without enough distinct active affixes rejects safely"), FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, Id, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFixedManaAllCharactersTest,
 "GameXXK.Equipment.FixedMana.AllCharacters",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKFixedManaAllCharactersTest::RunTest(const FString&)
{
 for(auto Role:{EGameXXKCharacterRole::Blade,EGameXXKCharacterRole::Guard,EGameXXKCharacterRole::Healer,EGameXXKCharacterRole::Hunter,EGameXXKCharacterRole::Sorcerer,EGameXXKCharacterRole::FormationMaster})
 {
  FGameXXKCharacterStats Base;FGameXXKCharacterStatRules::GetBareCompanionStats(Role,1,1,Base);
  for(int32 Level:{1,20,50,100})for(int32 Star:{1,3,5}){FGameXXKCharacterStats Current;TestTrue(TEXT("valid progressed stats"),FGameXXKCharacterStatRules::GetBareCompanionStats(Role,Level,Star,Current));TestEqual(TEXT("level and stars never increase Mana"),Current.MaxMana,Base.MaxMana);}
 }
 for(const auto& N:FGameXXKCompanionCatalog::GetQuestNpcDefinitions())for(int32 Level:{1,50,100})
 {FGameXXKCompanionAttributes A;TestTrue(TEXT("NPC stats resolve"),FGameXXKCompanionRules::GetQuestNpcAttributes(N.NpcId,Level,A));TestEqual(TEXT("NPC level never increases Mana"),A.Mana,N.BaseAttributes.Mana);}
 return true;
}

#endif
