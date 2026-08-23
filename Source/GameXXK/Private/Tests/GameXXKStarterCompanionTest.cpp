#include "GameXXKCompanionRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 CountActivePermanentCompanions(const FGameXXKCompanionRosterState& Roster)
	{
		int32 ActiveCount = 0;
		for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
		{
			if (Companion.bIsActive)
			{
				++ActiveCount;
			}
		}
		return ActiveCount;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStarterCompanionTest,
	"GameXXK.MVP.StarterCompanion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStarterCompanionTest::RunTest(const FString& Parameters)
{
	UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("starter companion subsystem exists"), Subsystem);
	if (!Subsystem || !TestTrue(TEXT("StartNewGame succeeds with a starter companion"), Subsystem->StartNewGame()))
	{
		return false;
	}

	const FGameXXKRuntimeState StartedState = Subsystem->GetRuntimeStateCopy();
	const FGameXXKCompanionRosterState& StartedRoster = StartedState.CardRun.CompanionRoster;
	TestEqual(TEXT("a new game owns one permanent companion for each of the six roles"), StartedRoster.PermanentCompanions.Num(), 6);
	TestEqual(TEXT("a new game exposes exactly one active permanent companion"), CountActivePermanentCompanions(StartedRoster), 1);
	TestTrue(TEXT("a new game persists a non-zero recruit sequence seed"), StartedRoster.RecruitSequenceSeed != 0);
	TestNotEqual(TEXT("a new game never uses the reserved minimum recruit sequence seed"), StartedRoster.RecruitSequenceSeed, MIN_int32);
	if (StartedRoster.PermanentCompanions.Num() != 6)
	{
		return false;
	}

	const FGameXXKPermanentCompanion& StarterCompanion = StartedRoster.PermanentCompanions[0];
	TestFalse(TEXT("the starter companion has a stable instance id"), StarterCompanion.InstanceId.IsNone());
	TestTrue(TEXT("the first starter companion is the single active partner"), StarterCompanion.bIsActive);
	TestEqual(TEXT("the default active starter is Blade"), StarterCompanion.Role, EGameXXKCharacterRole::Blade);
	TSet<EGameXXKCharacterRole> StarterRoles;
	for (const FGameXXKPermanentCompanion& Companion : StartedRoster.PermanentCompanions)
	{
		StarterRoles.Add(Companion.Role);
	}
	TestEqual(TEXT("the starter roster has six distinct role identities"), StarterRoles.Num(), 6);
	for (const EGameXXKCharacterRole RequiredRole : {
		EGameXXKCharacterRole::Blade,
		EGameXXKCharacterRole::Guard,
		EGameXXKCharacterRole::Healer,
		EGameXXKCharacterRole::Hunter,
		EGameXXKCharacterRole::Sorcerer,
		EGameXXKCharacterRole::FormationMaster})
	{
		TestTrue(TEXT("the starter roster contains every required role"), StarterRoles.Contains(RequiredRole));
	}
	TestEqual(
		TEXT("party selection points at the active starter companion"),
		StartedState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		StarterCompanion.InstanceId);
	TestEqual(TEXT("the starter companion owns six birth cards"), StarterCompanion.PersonalCardIds.Num(), 6);
	TestEqual(TEXT("the starter companion equips five selected cards"), StarterCompanion.SelectedCardIds.Num(), 5);
	TestEqual(TEXT("the default NPC party slot is Tusi Chief"),
		StartedState.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));
	TestEqual(TEXT("Tusi Chief keeps the NPC three-card rule"),
		StartedState.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(StartedState);
	FGameXXKRuntimeState RestoredState;
	FGameXXKSaveMigrationReport RestoreReport;
	if (!TestTrue(
		TEXT("starter-companion save restores through the typed migration boundary"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, RestoredState, RestoreReport)))
	{
		return false;
	}
	const FGameXXKCompanionRosterState& RestoredRoster = RestoredState.CardRun.CompanionRoster;
	TestEqual(TEXT("save restore keeps all six starter companions"), RestoredRoster.PermanentCompanions.Num(), 6);
	TestEqual(TEXT("save restore keeps the recruit sequence seed"), RestoredRoster.RecruitSequenceSeed, StartedRoster.RecruitSequenceSeed);
	TestEqual(
		TEXT("save restore keeps the active party selection"),
		RestoredState.CardRun.PartySelection.ActivePermanentCompanionInstanceId,
		StarterCompanion.InstanceId);
	if (RestoredRoster.PermanentCompanions.Num() != 6)
	{
		return false;
	}

	const FGameXXKPermanentCompanion& RestoredStarter = RestoredRoster.PermanentCompanions[0];
	TestEqual(TEXT("save restore keeps the starter instance id"), RestoredStarter.InstanceId, StarterCompanion.InstanceId);
	TestEqual(TEXT("save restore keeps the starter personal cards"), RestoredStarter.PersonalCardIds, StarterCompanion.PersonalCardIds);
	TestEqual(TEXT("save restore keeps the starter selected cards"), RestoredStarter.SelectedCardIds, StarterCompanion.SelectedCardIds);
	TestEqual(TEXT("save restore keeps the starter card seed"), RestoredStarter.CardSeed, StarterCompanion.CardSeed);
	TestEqual(TEXT("save restore keeps the starter active flag"), RestoredStarter.bIsActive, StarterCompanion.bIsActive);

	// Exact-three formations may retire their task NPC, so every current roster
	// must retain two permanent companions for deterministic replacement.
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const TArray<FGameXXKPermanentCompanion> DismissalRoster =
		Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions;
	for (int32 Index = 0; Index < DismissalRoster.Num() - 2; ++Index)
	{
		TestTrue(TEXT("a companion can be dismissed while two starter companions remain"),
			Subsystem->DismissPermanentCompanion(DismissalRoster[Index].InstanceId));
	}
	TestEqual(TEXT("dismissal leaves exactly two permanent companions"),
		Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.Num(), 2);
	const FGameXXKRuntimeState BeforeRejectedDismissal = Subsystem->GetRuntimeStateCopy();
	const FName ProtectedStarterId = DismissalRoster[DismissalRoster.Num() - 2].InstanceId;
	TestFalse(TEXT("a roster of two rejects another dismissal"),
		Subsystem->DismissPermanentCompanion(ProtectedStarterId));
	TestTrue(TEXT("the rejected two-to-one dismissal leaves the complete runtime bit-identical"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(),
			&BeforeRejectedDismissal,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStartNewGameAtomicTransactionTest,
	"GameXXK.MVP.StartNewGame.AtomicTransaction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStartNewGameAtomicTransactionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestNotNull(TEXT("atomic new-game subsystem exists"), Subsystem)
		|| !TestTrue(TEXT("atomic new-game baseline starts"), Subsystem->StartNewGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().PlayerGold = 54321;
	const FGameXXKRuntimeState RuntimeBeforeFailure = Subsystem->GetRuntimeStateCopy();
	const FGameXXKTrainingTravelRuntime TravelBeforeFailure = Subsystem->GetTrainingTravelRuntimeCopy();
	Subsystem->SetStartNewGameCommitGateForTest([]()
	{
		return false;
	});
	TestFalse(TEXT("injected final new-game gate rejects initialization"), Subsystem->StartNewGame());
	Subsystem->ResetStartNewGameCommitGateForTest();
	TestTrue(TEXT("failed new-game attempt preserves every runtime field"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(),
			&RuntimeBeforeFailure,
			PPF_None));
	const FGameXXKTrainingTravelRuntime TravelAfterFailure = Subsystem->GetTrainingTravelRuntimeCopy();
	TestTrue(TEXT("failed new-game attempt preserves transient travel runtime"),
		FGameXXKTrainingTravelRuntime::StaticStruct()->CompareScriptStruct(
			&TravelAfterFailure,
			&TravelBeforeFailure,
			PPF_None));

	TestTrue(TEXT("new-game succeeds after failure gate resets"), Subsystem->StartNewGame());
	TestTrue(TEXT("repeated new-game succeeds without appending old state"), Subsystem->StartNewGame());
	const FGameXXKRuntimeState& Repeated = Subsystem->GetRuntimeState();
	TestEqual(TEXT("repeated new-game owns exactly six starter companions"),
		Repeated.CardRun.CompanionRoster.PermanentCompanions.Num(), 6);
	TestEqual(TEXT("repeated new-game owns exact three-member formation"),
		Repeated.CardRun.OrderedFormation.Members.Num(), FGameXXKPartyFormationRules::PartySize);
	FString ValidationError;
	TestTrue(TEXT("repeated new-game remains save-authoritatively valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(Repeated, ValidationError));
	return true;
}

#endif
