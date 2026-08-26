#include "Misc/AutomationTest.h"

#include "GameXXKTrainingRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int64 TotalExperience(const int32 Level, const int32 Experience)
	{
		const int64 Completed = FMath::Max(0, Level - 1);
		return Completed * (Completed + 1) * 50 + FMath::Max(0, Experience);
	}

	void UnlockIdleEntry(FGameXXKRuntimeState& State)
	{
		State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
		State.Talents.NodeRanks.Add(TEXT("Talent.Entry.IdleOffline"), 1);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentTravelPartyAndMovementIntegrationTest,
	"GameXXK.Talents.Training.TravelPartyStatsAndMovementCadence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentTravelPartyAndMovementIntegrationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Baseline =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	UGameXXKMVPSubsystem* Talented =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("baseline travel fixture starts"), Baseline && Baseline->StartGame())
		|| !TestTrue(TEXT("talented travel fixture exists"), Talented != nullptr))
	{
		return false;
	}
	Talented->GetMutableRuntimeState() = Baseline->GetRuntimeStateCopy();
	FGameXXKRuntimeState& TalentedState = Talented->GetMutableRuntimeState();
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Entry.Combat"), 1);
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Combat.FlatAttack.01"), 5);
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Combat.FlatHealth.01"), 5);
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Combat.FlatAttack.08"), 1);
	TalentedState.Talents.NodeRanks.Add(TEXT("Talent.Combat.Movement.01"), 5);

	const FName StageOne = FGameXXKTrainingRules::MakeStageId(
		EGameXXKTrainingDifficulty::Normal,
		1);
	if (!TestTrue(TEXT("baseline travel starts"), Baseline->StartTrainingTravel(StageOne))
		|| !TestTrue(TEXT("talented travel starts"), Talented->StartTrainingTravel(StageOne)))
	{
		return false;
	}
	const FGameXXKTrainingTravelRuntime BaselineRuntime = Baseline->GetTrainingTravelRuntimeCopy();
	const FGameXXKTrainingTravelRuntime TalentedRuntime = Talented->GetTrainingTravelRuntimeCopy();
	if (!TestEqual(TEXT("both travel parties contain three units"), TalentedRuntime.PartyUnits.Num(), 3)
		|| BaselineRuntime.PartyUnits.Num() != 3)
	{
		return false;
	}
	for (int32 Index = 0; Index < 3; ++Index)
	{
		TestTrue(TEXT("flat health talent increases every travel party member"),
			TalentedRuntime.PartyUnits[Index].MaxHP > BaselineRuntime.PartyUnits[Index].MaxHP);
		TestTrue(TEXT("flat attack talent increases every travel party member"),
			TalentedRuntime.PartyUnits[Index].Attack > BaselineRuntime.PartyUnits[Index].Attack);
	}

	UGameXXKDesktopTrainingWorkbenchWidget* Widget =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Widget->SetMVPSubsystem(Talented);
	if (!TestTrue(TEXT("talented workbench opens"), Widget->OpenWorkbench()))
	{
		return false;
	}
	Widget->TickForTest(2.49f);
	TestEqual(TEXT("rank five movement remains walking immediately before 2.5 seconds"),
		Talented->GetTrainingTravelRuntimeCopy().Phase,
		EGameXXKTrainingTravelPhase::Walking);
	Widget->TickForTest(0.02f);
	TestEqual(TEXT("rank five movement enters combat at the 2.5-second boundary"),
		Talented->GetTrainingTravelRuntimeCopy().Phase,
		EGameXXKTrainingTravelPhase::Combat);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentTrainingIntegrationTest,
	"GameXXK.Talents.Training.RelativeChestOnlineRewardsAndOfflineUnlock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentTrainingIntegrationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("base chest chance remains twenty-five percent"),
		FMath::IsNearlyEqual(FGameXXKTrainingRules::ResolveRelativeChestChance(0.25f, 0.0f), 0.25f));
	TestTrue(TEXT("plus one hundred percent doubles twenty-five to fifty"),
		FMath::IsNearlyEqual(FGameXXKTrainingRules::ResolveRelativeChestChance(0.25f, 1.0f), 0.50f));
	TestTrue(TEXT("plus three hundred fifty percent clamps at one hundred"),
		FMath::IsNearlyEqual(FGameXXKTrainingRules::ResolveRelativeChestChance(0.25f, 3.5f), 1.0f));

	UGameXXKMVPSubsystem* Baseline =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("baseline Training fixture starts"), Baseline && Baseline->StartGame()))
	{
		return false;
	}
	UGameXXKMVPSubsystem* Boosted =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	Boosted->GetMutableRuntimeState() = Baseline->GetRuntimeStateCopy();
	UnlockIdleEntry(Boosted->GetMutableRuntimeState());
	Boosted->GetMutableRuntimeState().Talents.NodeRanks.Add(TEXT("Talent.Idle.OnlineGold.01"), 5);
	Boosted->GetMutableRuntimeState().Talents.NodeRanks.Add(TEXT("Talent.Idle.OnlineExperience.01"), 5);

	const int32 BaselineGoldBefore = Baseline->GetRuntimeState().PlayerGold;
	const int64 BaselineXpBefore = TotalExperience(
		Baseline->GetRuntimeState().PlayerLevel,
		Baseline->GetRuntimeState().PlayerXP);
	const int32 BoostedGoldBefore = Boosted->GetRuntimeState().PlayerGold;
	const int64 BoostedXpBefore = TotalExperience(
		Boosted->GetRuntimeState().PlayerLevel,
		Boosted->GetRuntimeState().PlayerXP);
	bool bBaselineStage = false;
	bool bBoostedStage = false;
	FGameXXKTrainingReward BaselineReward;
	FGameXXKTrainingReward BoostedReward;
	TestTrue(TEXT("baseline settles one Training encounter"),
		Baseline->AdvanceTrainingTravelEncounter(bBaselineStage, BaselineReward));
	TestTrue(TEXT("boosted settles the matching Training encounter"),
		Boosted->AdvanceTrainingTravelEncounter(bBoostedStage, BoostedReward));
	const int32 BaselineGoldGain = Baseline->GetRuntimeState().PlayerGold - BaselineGoldBefore;
	const int64 BaselineXpGain = TotalExperience(
		Baseline->GetRuntimeState().PlayerLevel,
		Baseline->GetRuntimeState().PlayerXP) - BaselineXpBefore;
	const int32 BoostedGoldGain = Boosted->GetRuntimeState().PlayerGold - BoostedGoldBefore;
	const int64 BoostedXpGain = TotalExperience(
		Boosted->GetRuntimeState().PlayerLevel,
		Boosted->GetRuntimeState().PlayerXP) - BoostedXpBefore;
	TestEqual(TEXT("first online gold node applies its ten-percent total"),
		BoostedGoldGain,
		FMath::RoundToInt(BaselineGoldGain * 1.10f));
	TestEqual(TEXT("first online experience node applies its ten-percent total"),
		BoostedXpGain,
		static_cast<int64>(FMath::RoundToInt(static_cast<float>(BaselineXpGain) * 1.10f)));

	UGameXXKMVPSubsystem* OfflineLocked =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("offline-lock fixture starts"), OfflineLocked && OfflineLocked->StartGame());
	FGameXXKTrainingOfflineReward OfflineReward;
	TestFalse(TEXT("offline simulation is locked before the idle entry"),
		OfflineLocked->SimulateTrainingTravelOffline(64, OfflineReward));
	UnlockIdleEntry(OfflineLocked->GetMutableRuntimeState());
	TestTrue(TEXT("idle entry unlocks offline simulation"),
		OfflineLocked->SimulateTrainingTravelOffline(64, OfflineReward));
	return true;
}

#endif
