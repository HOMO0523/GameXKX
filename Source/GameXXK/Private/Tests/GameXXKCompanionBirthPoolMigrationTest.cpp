#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKCompanionBirthPoolMigrationTest
{
	struct FRoleMigrationFixture
	{
		EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
		FName TemplateId;
		int32 CardSeed = 0;
	};

	const TArray<FRoleMigrationFixture>& AllRoleMigrationFixtures()
	{
		static const TArray<FRoleMigrationFixture> Fixtures = {
			{EGameXXKCharacterRole::Blade, TEXT("Companion.Blade.01"), 9101},
			{EGameXXKCharacterRole::Guard, TEXT("Companion.Guard.01"), 9102},
			{EGameXXKCharacterRole::Healer, TEXT("Companion.Healer.01"), 9103},
			{EGameXXKCharacterRole::Hunter, TEXT("Companion.Hunter.01"), 9104},
			{EGameXXKCharacterRole::Sorcerer, TEXT("Companion.Sorcerer.01"), 9105},
			{EGameXXKCharacterRole::FormationMaster, TEXT("Companion.FormationMaster.01"), 9106}};
		return Fixtures;
	}

	const TSet<FName>& FormationSwitchCardIds()
	{
		static const TSet<FName> CardIds = {
			TEXT("Profession.FormationMaster.GuanShi"),
			TEXT("Profession.FormationMaster.DingZhen"),
			TEXT("Profession.FormationMaster.YinShuiHuiYuan"),
			TEXT("Profession.FormationMaster.KunZhen"),
			TEXT("Profession.FormationMaster.LinYingMiZong"),
			TEXT("Profession.FormationMaster.JieShanWeiZhang")};
		return CardIds;
	}

	TArray<FName> BuildLegacyTwelveCardPool(const EGameXXKCharacterRole Role)
	{
		TArray<FName> Result;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Profession && Definition.Role == Role)
			{
				Result.Add(Definition.Id);
				if (Result.Num() == 12)
				{
					break;
				}
			}
		}
		return Result;
	}

	TArray<FName> BuildLegacySelection(
		const TArray<FName>& LegacyPool,
		const TArray<FName>& NewBirthPool)
	{
		TArray<FName> Result;
		for (const FName CardId : LegacyPool)
		{
			if (NewBirthPool.Contains(CardId))
			{
				Result.Add(CardId);
				if (Result.Num() == 2)
				{
					break;
				}
			}
		}
		for (const FName CardId : LegacyPool)
		{
			if (!NewBirthPool.Contains(CardId))
			{
				Result.Add(CardId);
				if (Result.Num() == 5)
				{
					break;
				}
			}
		}
		return Result;
	}

	TArray<FName> BuildExpectedSelection(
		const TArray<FName>& LegacySelection,
		const TArray<FName>& NewBirthPool)
	{
		TArray<FName> Result;
		for (const FName CardId : LegacySelection)
		{
			if (NewBirthPool.Contains(CardId) && !Result.Contains(CardId))
			{
				Result.Add(CardId);
			}
		}
		for (const FName CardId : NewBirthPool)
		{
			if (Result.Num() >= 5)
			{
				break;
			}
			if (!Result.Contains(CardId))
			{
				Result.Add(CardId);
			}
		}
		return Result;
	}

	bool RecruitLegacyFormationSupport(
		FGameXXKRuntimeState& State,
		const int32 RequiredPermanentMemberCount)
	{
		static const FName SupportTemplates[] = {
			TEXT("Companion.Guard.01"),
			TEXT("Companion.Healer.01"),
			TEXT("Companion.Hunter.01"),
			TEXT("Companion.Sorcerer.01")};
		for (int32 TemplateIndex = 0;
			State.CardRun.CompanionRoster.PermanentCompanions.Num() < RequiredPermanentMemberCount
				&& TemplateIndex < UE_ARRAY_COUNT(SupportTemplates);
			++TemplateIndex)
		{
			FGameXXKCompanionRecruitResult Result;
			FString Error;
			if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				State.CardRun.CompanionRoster,
				SupportTemplates[TemplateIndex],
				9200 + TemplateIndex,
				Result,
				&Error)
				|| Result.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
			{
				return false;
			}
		}
		return State.CardRun.CompanionRoster.PermanentCompanions.Num() >= RequiredPermanentMemberCount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionBirthPoolMigrationTest,
	"GameXXK.MVP.SaveGame.CompanionBirthPoolV13.Roster",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPendingCompanionBirthPoolMigrationTest,
	"GameXXK.MVP.SaveGame.CompanionBirthPoolV13.PendingCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAllRoleCompanionBirthPoolMigrationTest,
	"GameXXK.MVP.SaveGame.CompanionBirthPoolV13.AllSixRoles",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionBirthPoolMigrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCompanionBirthPoolMigrationTest;

	TestEqual(TEXT("the fixed six-card companion birth pool was introduced by save version thirteen"),
		FGameXXKSaveMigration::CompanionBirthPoolIntroducedSaveVersion, 13);
	TestEqual(TEXT("the current save schema includes persistent inventory locks"),
		FGameXXKSaveMigration::CurrentSaveVersion, 28);

	FGameXXKRuntimeState LegacyRuntime = UGameXXKMVPRules::CreateNewGame();
	FGameXXKCompanionRecruitResult RecruitResult;
	FString RecruitError;
	TestTrue(TEXT("the migration fixture recruits one permanent companion through the production rule"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			LegacyRuntime.CardRun.CompanionRoster,
			TEXT("Companion.Blade.01"),
			7331,
			RecruitResult,
			&RecruitError));
	TestEqual(TEXT("the migration fixture recruit is a permanent-companion result"),
		RecruitResult.Outcome,
		EGameXXKCompanionRecruitOutcome::Recruited);
	if (!TestTrue(TEXT("the migration fixture contains a permanent companion"),
		!LegacyRuntime.CardRun.CompanionRoster.PermanentCompanions.IsEmpty()))
	{
		return false;
	}
	if (!TestTrue(TEXT("legacy roster fixture has three legal formation members"),
		RecruitLegacyFormationSupport(LegacyRuntime, 3)))
	{
		return false;
	}

	FGameXXKPermanentCompanion& LegacyCompanion = LegacyRuntime.CardRun.CompanionRoster.PermanentCompanions[0];
	const int32 LegacyLevel = LegacyCompanion.Level;
	const int32 LegacyStar = LegacyCompanion.Star;
	const FName LegacyInstanceId = LegacyCompanion.InstanceId;
	TArray<FName> ExpectedBirthPool;
	if (!TestTrue(TEXT("the immutable seed builds the expected six-card destination pool"),
		FGameXXKCompanionRules::BuildPersonalCardPool(
			LegacyCompanion.Role,
			LegacyCompanion.CardSeed,
			ExpectedBirthPool,
			nullptr)))
	{
		return false;
	}
	TArray<FName> ExpectedFullPool;
	if (!TestTrue(TEXT("the immutable seed builds the expected eighteen-card destination pool"),
		FGameXXKCompanionRules::BuildFullProfessionCardPool(
			LegacyCompanion.Role,
			LegacyCompanion.CardSeed,
			ExpectedFullPool,
			nullptr)))
	{
		return false;
	}

	const TArray<FName> LegacyTwelveCardPool = BuildLegacyTwelveCardPool(LegacyCompanion.Role);
	const TArray<FName> LegacySelection = BuildLegacySelection(LegacyTwelveCardPool, ExpectedBirthPool);
	TestEqual(TEXT("the v12 fixture has twelve same-role personal cards"), LegacyTwelveCardPool.Num(), 12);
	TestEqual(TEXT("the v12 fixture has five selected cards"), LegacySelection.Num(), 5);
	if (LegacyTwelveCardPool.Num() != 12 || LegacySelection.Num() != 5)
	{
		return false;
	}
	LegacyCompanion.PersonalCardIds = LegacyTwelveCardPool;
	LegacyCompanion.UnlockedPersonalCardIds = LegacyTwelveCardPool;
	LegacyCompanion.SelectedCardIds = LegacySelection;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyRuntime);
	LegacySave.SaveVersion = 12;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("a valid v12 twelve-card companion save migrates to the fixed birth-pool schema"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}

	TestEqual(TEXT("the migrated save writes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	if (!TestTrue(TEXT("the migrated roster still contains the same companion slot"),
		!Migrated.RuntimeState.CardRun.CompanionRoster.PermanentCompanions.IsEmpty()))
	{
		return false;
	}
	const FGameXXKPermanentCompanion& MigratedCompanion =
		Migrated.RuntimeState.CardRun.CompanionRoster.PermanentCompanions[0];
	TestEqual(TEXT("migration preserves the companion identity"), MigratedCompanion.InstanceId, LegacyInstanceId);
	TestEqual(TEXT("migration preserves companion level"), MigratedCompanion.Level, LegacyLevel);
	TestEqual(TEXT("migration preserves companion star"), MigratedCompanion.Star, LegacyStar);
	TestEqual(TEXT("migration rebuilds the deterministic eighteen-card profession pool"),
		MigratedCompanion.PersonalCardIds, ExpectedFullPool);
	TestEqual(TEXT("all six migrated birth cards are initially unlocked"),
		MigratedCompanion.UnlockedPersonalCardIds, ExpectedBirthPool);
	TestEqual(TEXT("migration preserves surviving selections in order and deterministically fills to five"),
		MigratedCompanion.SelectedCardIds,
		BuildExpectedSelection(LegacySelection, ExpectedBirthPool));
	TestTrue(TEXT("the migrated companion passes the complete new profile contract"),
		FGameXXKCompanionRules::ValidatePermanentCompanionProfile(MigratedCompanion, nullptr));

	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(TEXT("the migrated v14 save roundtrips without another rewrite"),
		FGameXXKSaveMigration::MigrateToCurrent(Migrated, RoundTrip, RoundTripReport));
	TestEqual(TEXT("the current-schema roundtrip is byte-semantically stable for the migrated birth pool"),
		RoundTrip.RuntimeState.CardRun.CompanionRoster.PermanentCompanions[0].PersonalCardIds,
		ExpectedFullPool);
	return true;
}

bool FGameXXKPendingCompanionBirthPoolMigrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCompanionBirthPoolMigrationTest;

	FGameXXKRuntimeState LegacyRuntime = UGameXXKMVPRules::CreateNewGame();
	FGameXXKCompanionRosterState CandidateRoster;
	FGameXXKCompanionRecruitResult RecruitResult;
	FString RecruitError;
	if (!TestTrue(TEXT("the pending-candidate fixture recruits through the production rule"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			CandidateRoster,
			TEXT("Companion.Guard.01"),
			8113,
			RecruitResult,
			&RecruitError)))
	{
		AddError(RecruitError);
		return false;
	}

	FGameXXKPermanentCompanion LegacyCandidate = RecruitResult.Companion;
	TArray<FName> ExpectedBirthPool;
	if (!TestTrue(TEXT("the pending candidate has a deterministic six-card destination pool"),
		FGameXXKCompanionRules::BuildPersonalCardPool(
			LegacyCandidate.Role,
			LegacyCandidate.CardSeed,
			ExpectedBirthPool,
			nullptr)))
	{
		return false;
	}
	TArray<FName> ExpectedFullPool;
	if (!TestTrue(TEXT("the pending candidate has a deterministic eighteen-card destination pool"),
		FGameXXKCompanionRules::BuildFullProfessionCardPool(
			LegacyCandidate.Role,
			LegacyCandidate.CardSeed,
			ExpectedFullPool,
			nullptr)))
	{
		return false;
	}
	LegacyCandidate.PersonalCardIds = BuildLegacyTwelveCardPool(LegacyCandidate.Role);
	LegacyCandidate.UnlockedPersonalCardIds = LegacyCandidate.PersonalCardIds;
	LegacyCandidate.SelectedCardIds = BuildLegacySelection(LegacyCandidate.PersonalCardIds, ExpectedBirthPool);
	if (!TestTrue(TEXT("pending-candidate fixture has three permanent formation members"),
		RecruitLegacyFormationSupport(LegacyRuntime, 3)))
	{
		return false;
	}
	LegacyRuntime.CardRun.CompanionRoster.PendingRecruitment.bHasPendingRecruitment = true;
	LegacyRuntime.CardRun.CompanionRoster.PendingRecruitment.Candidate = LegacyCandidate;

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyRuntime);
	LegacySave.SaveVersion = 12;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("a v12 pending replacement candidate migrates to its fixed six-card pool"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}

	const FGameXXKPendingCompanionRecruitment& MigratedPending =
		Migrated.RuntimeState.CardRun.CompanionRoster.PendingRecruitment;
	TestTrue(TEXT("migration preserves the pending-replacement flag"), MigratedPending.bHasPendingRecruitment);
	TestEqual(TEXT("migration preserves the pending candidate identity"),
		MigratedPending.Candidate.InstanceId,
		LegacyCandidate.InstanceId);
	TestEqual(TEXT("migration rebuilds the pending candidate's full profession pool"),
		MigratedPending.Candidate.PersonalCardIds,
		ExpectedFullPool);
	TestEqual(TEXT("migration initially unlocks all six pending-candidate birth cards"),
		MigratedPending.Candidate.UnlockedPersonalCardIds,
		ExpectedBirthPool);
	TestEqual(TEXT("migration preserves and fills the pending candidate's selected cards"),
		MigratedPending.Candidate.SelectedCardIds,
		BuildExpectedSelection(LegacyCandidate.SelectedCardIds, ExpectedBirthPool));
	return true;
}

bool FGameXXKAllRoleCompanionBirthPoolMigrationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCompanionBirthPoolMigrationTest;

	FGameXXKRuntimeState LegacyRuntime = UGameXXKMVPRules::CreateNewGame();
	TArray<TArray<FName>> ExpectedBirthPools;
	TArray<TArray<FName>> ExpectedFullPools;
	TArray<TArray<FName>> ExpectedSelections;
	TArray<FName> ExpectedInstanceIds;
	TArray<TArray<FName>> LegacyPools;
	TArray<TArray<FName>> LegacySelections;

	const TArray<FRoleMigrationFixture>& Fixtures = AllRoleMigrationFixtures();
	for (int32 FixtureIndex = 0; FixtureIndex < Fixtures.Num(); ++FixtureIndex)
	{
		const FRoleMigrationFixture& Fixture = Fixtures[FixtureIndex];
		FGameXXKCompanionRecruitResult RecruitResult;
		FString RecruitError;
		if (!TestTrue(
			FString::Printf(TEXT("role %d recruits through the production companion rule"), static_cast<int32>(Fixture.Role)),
			FGameXXKCompanionRules::RecruitPermanentCompanion(
				LegacyRuntime.CardRun.CompanionRoster,
				Fixture.TemplateId,
				Fixture.CardSeed,
				RecruitResult,
				&RecruitError)))
		{
			AddError(RecruitError);
			return false;
		}

		FGameXXKPermanentCompanion& LegacyCompanion =
			LegacyRuntime.CardRun.CompanionRoster.PermanentCompanions.Last();
		TestEqual(
			FString::Printf(TEXT("role %d fixture preserves the requested role"), static_cast<int32>(Fixture.Role)),
			LegacyCompanion.Role,
			Fixture.Role);

		TArray<FName> ExpectedBirthPool;
		FString PoolError;
		if (!TestTrue(
			FString::Printf(TEXT("role %d rebuilds its deterministic six-card destination pool"), static_cast<int32>(Fixture.Role)),
			FGameXXKCompanionRules::BuildPersonalCardPool(
				Fixture.Role,
				Fixture.CardSeed,
				ExpectedBirthPool,
				&PoolError)))
		{
			AddError(PoolError);
			return false;
		}
		TArray<FName> ExpectedFullPool;
		if (!TestTrue(
			FString::Printf(TEXT("role %d rebuilds its deterministic eighteen-card destination pool"), static_cast<int32>(Fixture.Role)),
			FGameXXKCompanionRules::BuildFullProfessionCardPool(
				Fixture.Role,
				Fixture.CardSeed,
				ExpectedFullPool,
				&PoolError)))
		{
			AddError(PoolError);
			return false;
		}

		const TArray<FName> LegacyTwelveCardPool = BuildLegacyTwelveCardPool(Fixture.Role);
		const TArray<FName> LegacySelection = BuildLegacySelection(LegacyTwelveCardPool, ExpectedBirthPool);
		TestEqual(
			FString::Printf(TEXT("role %d exposes a valid legacy twelve-card fixture"), static_cast<int32>(Fixture.Role)),
			LegacyTwelveCardPool.Num(),
			12);
		TestEqual(
			FString::Printf(TEXT("role %d exposes a valid legacy five-card selection"), static_cast<int32>(Fixture.Role)),
			LegacySelection.Num(),
			5);
		if (LegacyTwelveCardPool.Num() != 12 || LegacySelection.Num() != 5)
		{
			return false;
		}

		ExpectedBirthPools.Add(ExpectedBirthPool);
		ExpectedFullPools.Add(ExpectedFullPool);
		ExpectedSelections.Add(BuildExpectedSelection(LegacySelection, ExpectedBirthPool));
		ExpectedInstanceIds.Add(LegacyCompanion.InstanceId);
		LegacyPools.Add(LegacyTwelveCardPool);
		LegacySelections.Add(LegacySelection);
	}

	for (int32 FixtureIndex = 0; FixtureIndex < Fixtures.Num(); ++FixtureIndex)
	{
		FGameXXKPermanentCompanion& LegacyCompanion =
			LegacyRuntime.CardRun.CompanionRoster.PermanentCompanions[FixtureIndex];
		LegacyCompanion.PersonalCardIds = LegacyPools[FixtureIndex];
		LegacyCompanion.UnlockedPersonalCardIds = LegacyPools[FixtureIndex];
		LegacyCompanion.SelectedCardIds = LegacySelections[FixtureIndex];
	}

	FGameXXKSaveState LegacySave = UGameXXKMVPRules::MakeSaveState(LegacyRuntime);
	LegacySave.SaveVersion = 12;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("one v12 save migrates all six permanent companion roles together"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}

	const TArray<FGameXXKPermanentCompanion>& MigratedCompanions =
		Migrated.RuntimeState.CardRun.CompanionRoster.PermanentCompanions;
	TestEqual(TEXT("the all-role migration preserves all six companion slots"), MigratedCompanions.Num(), Fixtures.Num());
	if (MigratedCompanions.Num() != Fixtures.Num())
	{
		return false;
	}

	for (int32 FixtureIndex = 0; FixtureIndex < Fixtures.Num(); ++FixtureIndex)
	{
		const FRoleMigrationFixture& Fixture = Fixtures[FixtureIndex];
		const FGameXXKPermanentCompanion& Companion = MigratedCompanions[FixtureIndex];
		const TArray<FName>& ExpectedBirthPool = ExpectedBirthPools[FixtureIndex];
		const TArray<FName>& ExpectedFullPool = ExpectedFullPools[FixtureIndex];
		TestEqual(
			FString::Printf(TEXT("role %d migration preserves companion identity"), static_cast<int32>(Fixture.Role)),
			Companion.InstanceId,
			ExpectedInstanceIds[FixtureIndex]);
		TestEqual(
			FString::Printf(TEXT("role %d migration preserves role"), static_cast<int32>(Fixture.Role)),
			Companion.Role,
			Fixture.Role);
		TestEqual(
			FString::Printf(TEXT("role %d migration rebuilds the deterministic eighteen-card profession pool"), static_cast<int32>(Fixture.Role)),
			Companion.PersonalCardIds,
			ExpectedFullPool);
		TestEqual(
			FString::Printf(TEXT("role %d migration permanently unlocks the full birth pool"), static_cast<int32>(Fixture.Role)),
			Companion.UnlockedPersonalCardIds,
			ExpectedBirthPool);
		TestEqual(
			FString::Printf(TEXT("role %d migration preserves survivors and fills exactly five configured cards"), static_cast<int32>(Fixture.Role)),
			Companion.SelectedCardIds,
			ExpectedSelections[FixtureIndex]);
		TestTrue(
			FString::Printf(TEXT("role %d migrated profile satisfies the production contract"), static_cast<int32>(Fixture.Role)),
			FGameXXKCompanionRules::ValidatePermanentCompanionProfile(Companion, nullptr));

		int32 CoreCardCount = 0;
		int32 FormationSwitchCount = 0;
		bool bAllCardsMatchRole = true;
		for (int32 CardIndex = 0; CardIndex < Companion.PersonalCardIds.Num(); ++CardIndex)
		{
			const FName CardId = Companion.PersonalCardIds[CardIndex];
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			bAllCardsMatchRole &= Definition
				&& Definition->Owner == EGameXXKCardOwner::Profession
				&& Definition->Role == Fixture.Role;
			if (CardIndex < ExpectedBirthPool.Num())
			{
				CoreCardCount += Definition && Definition->bCoreProfessionCard ? 1 : 0;
				FormationSwitchCount += FormationSwitchCardIds().Contains(CardId) ? 1 : 0;
			}
		}
		TestTrue(
			FString::Printf(TEXT("role %d migration never crosses profession card ownership"), static_cast<int32>(Fixture.Role)),
			bAllCardsMatchRole);
		if (Fixture.Role == EGameXXKCharacterRole::FormationMaster)
		{
			TestEqual(TEXT("formation migration retains exactly two terrain-switch cards"), FormationSwitchCount, 2);
			TestEqual(TEXT("formation migration retains exactly four terrain-benefit birth cards"), ExpectedBirthPool.Num() - FormationSwitchCount, 4);
		}
		else
		{
			TestEqual(
				FString::Printf(TEXT("role %d migration retains exactly two fixed core cards"), static_cast<int32>(Fixture.Role)),
				CoreCardCount,
				2);
			TestEqual(
				FString::Printf(TEXT("role %d migration retains exactly four random birth cards"), static_cast<int32>(Fixture.Role)),
				ExpectedBirthPool.Num() - CoreCardCount,
				4);
		}
	}

	FGameXXKSaveState RoundTrip;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(TEXT("the migrated all-role save roundtrips without rewriting any birth pool"),
		FGameXXKSaveMigration::MigrateToCurrent(Migrated, RoundTrip, RoundTripReport));
	for (int32 FixtureIndex = 0; FixtureIndex < Fixtures.Num(); ++FixtureIndex)
	{
		TestEqual(
			FString::Printf(TEXT("role %d current-schema roundtrip preserves its eighteen-card pool"), static_cast<int32>(Fixtures[FixtureIndex].Role)),
			RoundTrip.RuntimeState.CardRun.CompanionRoster.PermanentCompanions[FixtureIndex].PersonalCardIds,
			ExpectedFullPools[FixtureIndex]);
	}
	return true;
}

#endif
