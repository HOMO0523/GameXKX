#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

static_assert(
	FGameXXKSaveMigration::BattleRewardTieringIntroducedSaveVersion == 16,
	"Battle reward tiering persistence requires save version 16.");
static_assert(
	FGameXXKSaveMigration::CurrentSaveVersion == 29,
	"Narrative stage and guide persistence advances the current save version to twenty-nine.");

namespace
{
	FGameXXKRuntimeState MakeStartedSaveFixture()
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		return Subsystem && Subsystem->StartGame()
			? Subsystem->GetRuntimeStateCopy()
			: FGameXXKRuntimeState();
	}

	FGameXXKSaveState MakeVersionedSave(const FGameXXKRuntimeState& State, const int32 Version)
	{
		FGameXXKSaveState Save = UGameXXKMVPRules::MakeSaveState(State);
		Save.SaveVersion = Version;
		return Save;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRewardTieringMigrationTest,
	"GameXXK.MVP.SaveGame.BattleRewardTieringV16.Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRewardTieringMigrationTest::RunTest(const FString& Parameters)
{
	// A v15 save carrying an in-flight legacy three-route-card reward offer.
	FGameXXKRuntimeState LegacyState = MakeStartedSaveFixture();
	LegacyState.CardRun.PendingReward.SourceNodeId = 3;
	LegacyState.CardRun.PendingReward.ChoiceSeed = 7;
	LegacyState.CardRun.PendingReward.CardIds = {
		FName(TEXT("Route.General.FeiZhen")),
		FName(TEXT("Route.Terrain.LinShiZhaYing")),
		FName(TEXT("Route.Rare.DuKouHuiLiu"))};
	LegacyState.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	LegacyState.CardRun.bActiveBattleRewardResolved = false;

	const FGameXXKSaveState LegacySave = MakeVersionedSave(LegacyState, 15);
	FGameXXKSaveState MigratedSave;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v15 save migrates to the v16 schema"),
		FGameXXKSaveMigration::MigrateToCurrent(LegacySave, MigratedSave, Report));
	TestEqual(TEXT("migrated save reports the current version"), MigratedSave.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	TestEqual(TEXT("legacy in-flight reward offer is cleared"),
		MigratedSave.RuntimeState.CardRun.PendingReward.CardIds.Num(), 0);
	TestEqual(TEXT("legacy replacement demand is cleared"),
		MigratedSave.RuntimeState.CardRun.PendingReward.bRequiresRouteCardReplacement, false);
	TestEqual(TEXT("cleared reward can re-roll on the next victory"),
		MigratedSave.RuntimeState.CardRun.bActiveBattleRewardResolved, false);
	TestEqual(TEXT("typed reward options start empty"),
		MigratedSave.RuntimeState.CardRun.PendingReward.Options.Num(), 0);
	TestEqual(TEXT("upgraded card qualities start empty"),
		MigratedSave.RuntimeState.CardRun.UpgradedCardQualities.Num(), 0);
	TestEqual(TEXT("energy cap bonus starts at zero"),
		MigratedSave.RuntimeState.CardRun.BonusSharedEnergyCap, 0);
	TestEqual(TEXT("draw bonus starts at zero"),
		MigratedSave.RuntimeState.CardRun.BonusRoundDrawCount, 0);

	// A v16 save keeps the new tiering fields through a save/restore round trip.
	FGameXXKRuntimeState V16State = MakeStartedSaveFixture();
	V16State.CardRun.UpgradedCardQualities.Add(
		FName(TEXT("Hero.Generic.QingFengYiShi")), EGameXXKCardQuality::Rare);
	V16State.CardRun.BonusSharedEnergyCap = 1;
	V16State.CardRun.BonusRoundDrawCount = 1;

	const FGameXXKSaveState V16Save = MakeVersionedSave(V16State, 16);
	FGameXXKRuntimeState RestoredState;
	FGameXXKSaveMigrationReport RoundTripReport;
	TestTrue(TEXT("v16 save restores through the migration chain"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(V16Save, RestoredState, RoundTripReport));
	TestEqual(TEXT("restored energy cap bonus survives"), RestoredState.CardRun.BonusSharedEnergyCap, 1);
	TestEqual(TEXT("restored draw bonus survives"), RestoredState.CardRun.BonusRoundDrawCount, 1);
	TestEqual(TEXT("restored upgraded quality map survives"),
		RestoredState.CardRun.UpgradedCardQualities.Num(), 1);
	const EGameXXKCardQuality* RestoredQuality =
		RestoredState.CardRun.UpgradedCardQualities.Find(FName(TEXT("Hero.Generic.QingFengYiShi")));
	TestNotNull(TEXT("restored upgraded quality entry exists"), RestoredQuality);
	if (RestoredQuality)
	{
		TestEqual(TEXT("restored upgraded quality value matches"), *RestoredQuality, EGameXXKCardQuality::Rare);
	}

	return true;
}

#endif
