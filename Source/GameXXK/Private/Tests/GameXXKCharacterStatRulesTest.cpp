#include "Misc/AutomationTest.h"

#include "GameXXKCharacterStatRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterStatRulesTest,
	"GameXXK.Equipment.CharacterStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterStatRulesTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("the shared permanent character level cap is one hundred"), FGameXXKCharacterStatRules::MaxCharacterLevel, 100);
	TestEqual(TEXT("hero level-ten experience threshold is level times one hundred"),
		UGameXXKMVPRules::GetPlayerExperienceRequiredForNextLevel(10), 1000);
	TestEqual(TEXT("companion level-ten experience threshold matches the hero"),
		FGameXXKCompanionRules::GetExperienceRequiredForNextLevel(10), 1000);

	const FGameXXKCharacterStats LevelOneHero = FGameXXKCharacterStatRules::GetBareHeroStats(1);
	TestEqual(TEXT("level-one hero max health matches the former formula"), LevelOneHero.MaxHealth, 100);
	TestEqual(TEXT("level-one hero max mana matches the former formula"), LevelOneHero.MaxMana, 30);
	TestEqual(TEXT("level-one hero attack matches the former formula"), LevelOneHero.Attack, 15);
	TestEqual(TEXT("level-one hero defense matches the former formula"), LevelOneHero.Defense, 8);
	TestEqual(TEXT("level-one hero speed matches the former formula"), LevelOneHero.Speed, 10);

	const FGameXXKCharacterStats LevelTwentyHero = FGameXXKCharacterStatRules::GetBareHeroStats(20);
	TestEqual(TEXT("level-twenty hero max health matches the former formula"), LevelTwentyHero.MaxHealth, 385);
	TestEqual(TEXT("level-twenty hero keeps base Mana thirty"), LevelTwentyHero.MaxMana, 30);
	TestEqual(TEXT("level-twenty hero attack matches the former formula"), LevelTwentyHero.Attack, 72);
	TestEqual(TEXT("level-twenty hero defense matches the former formula"), LevelTwentyHero.Defense, 46);
	TestEqual(TEXT("level-twenty hero speed matches the former formula"), LevelTwentyHero.Speed, 29);
	const FGameXXKCharacterStats LevelHundredHero = FGameXXKCharacterStatRules::GetBareHeroStats(100);
	TestEqual(TEXT("level-one-hundred hero max health keeps the linear formula"), LevelHundredHero.MaxHealth, 1585);
	TestEqual(TEXT("level-one-hundred hero keeps base Mana thirty"), LevelHundredHero.MaxMana, 30);
	TestEqual(TEXT("level-one-hundred hero attack keeps the linear formula"), LevelHundredHero.Attack, 312);
	TestEqual(TEXT("level-one-hundred hero defense keeps the linear formula"), LevelHundredHero.Defense, 206);
	TestEqual(TEXT("level-one-hundred hero speed keeps the linear formula"), LevelHundredHero.Speed, 109);
	TestEqual(TEXT("hero naked stats clamp levels above the permanent cap"), FGameXXKCharacterStatRules::GetBareHeroStats(101).MaxHealth, LevelHundredHero.MaxHealth);
	TestEqual(TEXT("hero naked stats clamp non-positive levels to one"), FGameXXKCharacterStatRules::GetBareHeroStats(0).MaxHealth, LevelOneHero.MaxHealth);

	struct FRoleExpectation
	{
		EGameXXKCharacterRole Role;
		int32 Health;
		int32 Mana;
		int32 Attack;
		int32 Defense;
		int32 Speed;
	};
	const FRoleExpectation RoleExpectations[] = {
		{EGameXXKCharacterRole::Blade, 92, 22, 17, 6, 11},
		{EGameXXKCharacterRole::Guard, 120, 18, 11, 12, 8},
		{EGameXXKCharacterRole::Healer, 90, 30, 10, 7, 9},
		{EGameXXKCharacterRole::Hunter, 86, 24, 16, 6, 13},
		{EGameXXKCharacterRole::Sorcerer, 80, 34, 15, 5, 9},
		{EGameXXKCharacterRole::FormationMaster, 94, 30, 12, 8, 10},
	};
	for (const FRoleExpectation& Expected : RoleExpectations)
	{
		FGameXXKCharacterStats Stats;
		TestTrue(FString::Printf(TEXT("role %d exposes shared naked stats"), static_cast<int32>(Expected.Role)), FGameXXKCharacterStatRules::GetBareCompanionStats(Expected.Role, 1, 1, Stats, nullptr));
		TestEqual(FString::Printf(TEXT("role %d level-one health"), static_cast<int32>(Expected.Role)), Stats.MaxHealth, Expected.Health);
		TestEqual(FString::Printf(TEXT("role %d level-one mana"), static_cast<int32>(Expected.Role)), Stats.MaxMana, Expected.Mana);
		TestEqual(FString::Printf(TEXT("role %d level-one attack"), static_cast<int32>(Expected.Role)), Stats.Attack, Expected.Attack);
		TestEqual(FString::Printf(TEXT("role %d level-one defense"), static_cast<int32>(Expected.Role)), Stats.Defense, Expected.Defense);
		TestEqual(FString::Printf(TEXT("role %d level-one speed"), static_cast<int32>(Expected.Role)), Stats.Speed, Expected.Speed);
	}

	FGameXXKCharacterStats ProgressedBlade;
	TestTrue(TEXT("a level-six two-star blade has shared progressed stats"), FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Blade, 6, 2, ProgressedBlade, nullptr));
	TestEqual(TEXT("companion speed gains one point at level six before star scaling"), ProgressedBlade.Speed, 12);
	FGameXXKCharacterStats ClampedBlade;
	TestTrue(TEXT("permanent companion naked stats clamp levels above one hundred"), FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Blade, 101, 1, ClampedBlade, nullptr));
	FGameXXKCharacterStats LevelHundredBlade;
	TestTrue(TEXT("level-one-hundred blade naked stats resolve"), FGameXXKCharacterStatRules::GetBareCompanionStats(EGameXXKCharacterRole::Blade, 100, 1, LevelHundredBlade, nullptr));
	TestEqual(TEXT("clamped companion stats equal level one hundred"), ClampedBlade.MaxHealth, LevelHundredBlade.MaxHealth);

	FGameXXKCompanionAttributes EquipmentBonus;
	EquipmentBonus.Health = 3;
	EquipmentBonus.Mana = 4;
	EquipmentBonus.Attack = 5;
	EquipmentBonus.Defense = 6;
	EquipmentBonus.Speed = 7;
	FGameXXKCompanionAttributes LegacyAttributes;
	TestTrue(TEXT("the legacy companion API delegates to shared naked stats"), FGameXXKCompanionRules::GetCompanionAttributes(EGameXXKCharacterRole::Blade, 6, 2, EquipmentBonus, LegacyAttributes, nullptr));
	TestEqual(TEXT("legacy API adds health equipment after shared naked stats"), LegacyAttributes.Health, ProgressedBlade.MaxHealth + EquipmentBonus.Health);
	TestEqual(TEXT("legacy API excludes all equipment Mana"), LegacyAttributes.Mana, ProgressedBlade.MaxMana);
	TestEqual(TEXT("legacy API adds attack equipment after shared naked stats"), LegacyAttributes.Attack, ProgressedBlade.Attack + EquipmentBonus.Attack);
	TestEqual(TEXT("legacy API adds defense equipment after shared naked stats"), LegacyAttributes.Defense, ProgressedBlade.Defense + EquipmentBonus.Defense);
	TestEqual(TEXT("legacy API adds speed equipment after shared naked stats"), LegacyAttributes.Speed, ProgressedBlade.Speed + EquipmentBonus.Speed);

	// Profile validation occurs before XP application, so use a real recruited profile for the cap behavior.
	FGameXXKCompanionRosterState Roster;
	FGameXXKCompanionRecruitResult RecruitResult;
	TestTrue(TEXT("a real companion profile can be recruited for cap verification"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, TEXT("Companion.Blade.01"), 4201, RecruitResult, nullptr));
	if (Roster.PermanentCompanions.Num() == 1)
	{
		Roster.PermanentCompanions[0].Level = 100;
		Roster.PermanentCompanions[0].Experience = 0;
		TestTrue(TEXT("awarding XP at companion cap remains a valid no-op"), FGameXXKCompanionRules::AwardExperience(Roster.PermanentCompanions[0], MAX_int32, nullptr));
		TestEqual(TEXT("permanent companion cannot pass level one hundred"), Roster.PermanentCompanions[0].Level, 100);
		TestEqual(TEXT("permanent companion XP is zero at level one hundred"), Roster.PermanentCompanions[0].Experience, 0);
	}

	FGameXXKRuntimeState HeroCapState = UGameXXKMVPRules::CreateNewGame();
	FString HeroCapSetupError;
	FGameXXKCompanionRecruitResult HeroCapRecruit;
	if (!TestTrue(TEXT("cap fixture recruits an active partner"), FGameXXKCompanionRules::RecruitPermanentCompanion(HeroCapState.CardRun.CompanionRoster, TEXT("Companion.Blade.01"), 7311, HeroCapRecruit, &HeroCapSetupError))
		|| !TestTrue(TEXT("cap fixture selects that partner"), FGameXXKCompanionRules::SetActivePermanentCompanion(HeroCapState.CardRun.CompanionRoster, HeroCapRecruit.Companion.InstanceId, &HeroCapSetupError))
		|| !TestTrue(TEXT("cap fixture owns a reserve partner"), FGameXXKCompanionRules::RecruitPermanentCompanion(HeroCapState.CardRun.CompanionRoster, TEXT("Companion.Guard.01"), 7312, HeroCapRecruit, &HeroCapSetupError))
		|| !TestTrue(TEXT("cap fixture selects the fixed NPC"), GameXXKPermanentPartyTestFixtures::SelectNpc(HeroCapState, TEXT("Npc.TusiChief"), &HeroCapSetupError))
		|| !TestTrue(TEXT("cap fixture has a legal three-person party"), FGameXXKPartyFormationRules::Normalize(HeroCapState, &HeroCapSetupError)))
	{
		AddError(HeroCapSetupError);
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(HeroCapState);
	TestTrue(TEXT("the fixed-route cap fixture opens the world map"), UGameXXKMVPRules::OpenWorldMap(HeroCapState));
	TestTrue(TEXT("the fixed-route cap fixture enters Qingshan"), UGameXXKMVPRules::EnterWorldRegion(HeroCapState, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("the fixed-route cap fixture accepts the town quest"), UGameXXKMVPRules::AcceptTownQuest(HeroCapState));
	HeroCapState.PlayerLevel = 100;
	HeroCapState.PlayerXP = 9999;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(HeroCapState);
	TestTrue(TEXT("the fixed-route cap fixture enters a real route"), UGameXXKMVPRules::EnterDungeon(HeroCapState));
	HeroCapState.bHasGeneratedRouteMap = false;
	HeroCapState.RouteMapNodes.Reset();
	HeroCapState.RouteMapEdges.Reset();
	HeroCapState.ReachableRouteNodeIds.Reset();
	HeroCapState.DungeonNodeIndex = 1;
	TestTrue(TEXT("the fixed-route cap fixture begins a real card battle"),
		UGameXXKMVPRules::AdvanceDungeonNode(HeroCapState, EGameXXKNodeKind::Battle));
	for (FGameXXKCardCombatUnit& Unit : HeroCapState.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	HeroCapState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("the fixed-route cap fixture opens its tiered reward gate"),
		UGameXXKMVPRules::ResolveBattleVictory(HeroCapState, false));
	FString HeroCapRewardError;
	TestTrue(TEXT("the fixed-route cap fixture skips its tiered reward gate"),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(HeroCapState, &HeroCapRewardError));
	TestTrue(TEXT("a fixed-route battle can award XP at the hero cap after its reward gate"),
		UGameXXKMVPRules::ResolveBattleVictory(HeroCapState, false));
	TestEqual(TEXT("hero cannot pass level one hundred"), HeroCapState.PlayerLevel, 100);
	TestEqual(TEXT("hero XP is zero at level one hundred"), HeroCapState.PlayerXP, 0);

	FGameXXKRuntimeState LegacyRuntimeState = UGameXXKMVPRules::CreateNewGame();
	FGameXXKCompanionRecruitResult LoadedRecruitResult;
	TestTrue(
		TEXT("a valid companion can be placed in the legacy runtime state"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			LegacyRuntimeState.CardRun.CompanionRoster,
			TEXT("Companion.Blade.01"),
			7301,
			LoadedRecruitResult,
			nullptr));
	LegacyRuntimeState.PlayerLevel = 21;
	LegacyRuntimeState.PlayerXP = 777;
	LegacyRuntimeState.PlayerMaxHP = 400;
	LegacyRuntimeState.PlayerHP = 350;
	LegacyRuntimeState.PlayerMaxMP = 140;
	LegacyRuntimeState.PlayerMP = 130;
	// A real v3 payload predates the instance-backed equipment collection. Keep the
	// legacy Inventory/equipped-item mirrors below, but do not carry current starter
	// instances into a synthetic old-version fixture.
	LegacyRuntimeState.EquipmentCollection = FGameXXKEquipmentCollectionState();
	const FName LegacyWeaponId = UGameXXKMVPRules::ItemIronSword();
	LegacyRuntimeState.Inventory.Add(LegacyWeaponId, 1);
	LegacyRuntimeState.EquippedWeapon = LegacyWeaponId;
	LegacyRuntimeState.ItemEnhancementLevels.Add(LegacyWeaponId, 2);
	FName LegacyCompanionInstanceId = NAME_None;
	if (LegacyRuntimeState.CardRun.CompanionRoster.PermanentCompanions.Num() == 1)
	{
		FGameXXKPermanentCompanion& LegacyCompanion =
			LegacyRuntimeState.CardRun.CompanionRoster.PermanentCompanions[0];
		LegacyCompanionInstanceId = LegacyCompanion.InstanceId;
		LegacyCompanion.Level = 20;
		LegacyCompanion.Experience = 0;
		TestTrue(
			TEXT("the synthetic legacy companion has the valid level-twenty unlock frontier before injecting an over-cap level"),
			FGameXXKCompanionRules::RefreshUnlockedPersonalCards(LegacyCompanion, nullptr));
		LegacyCompanion.Level = 21;
		LegacyCompanion.Experience = 888;
	}

	FGameXXKSaveState LegacySaveState = UGameXXKMVPRules::MakeSaveState(LegacyRuntimeState);
	LegacySaveState.SaveVersion = 3;
	FGameXXKRuntimeState RestoredLegacyState;
	FGameXXKSaveMigrationReport LegacyMigrationReport;
	const bool bLegacyMigrationSucceeded = FGameXXKSaveMigration::TryRestoreRuntimeState(
		LegacySaveState,
		RestoredLegacyState,
		LegacyMigrationReport);
	TestTrue(
		FString::Printf(TEXT("the realistic version-three fixture migrates successfully: %s"), *LegacyMigrationReport.Error),
		bLegacyMigrationSucceeded);
	const FGameXXKCharacterStats ExpectedLevelTwentyOneHero = FGameXXKCharacterStatRules::GetBareHeroStats(21);
	TestEqual(TEXT("loading a version-three runtime preserves valid hero level twenty-one"), RestoredLegacyState.PlayerLevel, 21);
	TestEqual(TEXT("loading a non-capped hero preserves stored XP"), RestoredLegacyState.PlayerXP, 777);
	TestEqual(TEXT("loaded hero max health uses the level-twenty-one formula"), RestoredLegacyState.PlayerMaxHP, ExpectedLevelTwentyOneHero.MaxHealth);
	TestEqual(TEXT("loaded hero max mana uses the level-twenty-one formula"), RestoredLegacyState.PlayerMaxMP, ExpectedLevelTwentyOneHero.MaxMana);
	TestEqual(TEXT("loaded hero attack retains the legacy weapon and enhancement bonuses"), RestoredLegacyState.PlayerAttack, ExpectedLevelTwentyOneHero.Attack + 10);
	TestEqual(TEXT("loaded hero defense uses the level-twenty-one formula"), RestoredLegacyState.PlayerDefense, ExpectedLevelTwentyOneHero.Defense);
	TestEqual(TEXT("loaded hero speed uses the level-twenty-one formula"), RestoredLegacyState.PlayerSpeed, ExpectedLevelTwentyOneHero.Speed);
	TestEqual(TEXT("loading preserves the legacy equipped weapon mirror"), RestoredLegacyState.EquippedWeapon, LegacyWeaponId);
	TestTrue(TEXT("loaded hero health remains within its normalized maximum"), RestoredLegacyState.PlayerHP >= 0 && RestoredLegacyState.PlayerHP <= RestoredLegacyState.PlayerMaxHP);
	TestTrue(TEXT("loaded hero mana remains within its normalized maximum"), RestoredLegacyState.PlayerMP >= 0 && RestoredLegacyState.PlayerMP <= RestoredLegacyState.PlayerMaxMP);
	const FGameXXKPermanentCompanion* RestoredLegacyCompanion =
		RestoredLegacyState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[LegacyCompanionInstanceId](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == LegacyCompanionInstanceId;
			});
	if (RestoredLegacyCompanion)
	{
		TestEqual(
			TEXT("loading a version-three runtime preserves companion level twenty-one"),
			RestoredLegacyCompanion->Level,
			21);
		TestEqual(
			TEXT("loading a non-capped permanent companion preserves stored XP"),
			RestoredLegacyCompanion->Experience,
			888);
	}
	else
	{
		AddError(TEXT("The legacy runtime lost its valid permanent companion during restore."));
	}
	TestEqual(
		TEXT("the legacy public companion cap matches the shared character cap"),
		FGameXXKCompanionRules::MaxCompanionLevel,
		FGameXXKCharacterStatRules::MaxCharacterLevel);

	FGameXXKCompanionAttributes HighLevelNpcAttributes;
	TestTrue(TEXT("the standalone task NPC helper remains valid for level one hundred"), FGameXXKCompanionRules::GetQuestNpcAttributes(TEXT("Npc.TusiChief"), 100, HighLevelNpcAttributes, nullptr));
	return true;
}

#endif
