#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKTrainingRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPermanentNpcFormationAuthorityTest,
	"GameXXK.PartyFormation.PermanentNpcAuthority",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPermanentNpcFormationAuthorityTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}

	const TArray<FGameXXKQuestNpcDefinition>& Definitions =
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions();
	TestEqual(TEXT("catalog exposes all six permanently owned NPCs"), Definitions.Num(), 6);

	FName ActiveNpcId;
	FString Error;
	TestTrue(TEXT("ordered formation resolves one permanent NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(), ActiveNpcId, &Error));
	TestEqual(TEXT("new game defaults to Tusi Chief"),
		ActiveNpcId,
		FName(TEXT("Npc.TusiChief")));
	TestTrue(TEXT("temporary route provenance is retired"),
		Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId.IsNone());

	for (const FGameXXKQuestNpcDefinition& Definition : Definitions)
	{
		FGameXXKRuntimeState Candidate = Subsystem->GetRuntimeState();
		TestTrue(
			*FString::Printf(
				TEXT("%s can be selected without unlock state"),
				*Definition.NpcId.ToString()),
			FGameXXKPartyFormationRules::SetQuestNpc(
				Candidate,
				Definition.NpcId,
				&Error));

		FName ResolvedNpcId;
		TestTrue(TEXT("selected NPC resolves from ordered formation"),
			FGameXXKPartyFormationRules::ResolveQuestNpcId(
				Candidate,
				ResolvedNpcId,
				&Error));
		TestEqual(TEXT("ordered identity matches the selected catalog NPC"),
			ResolvedNpcId,
			Definition.NpcId);
		TestEqual(TEXT("loadout projection matches ordered identity"),
			Candidate.CardRun.PartySelection.QuestNpc.NpcId,
			Definition.NpcId);
		TestTrue(TEXT("selection never revives temporary provenance"),
			Candidate.CardRun.ActiveTemporaryQuestNpcId.IsNone());
	}

	const FGameXXKRuntimeState BeforeInvalid = Subsystem->GetRuntimeStateCopy();
	FGameXXKRuntimeState InvalidCandidate = BeforeInvalid;
	TestFalse(TEXT("unknown NPC is rejected"),
		FGameXXKPartyFormationRules::SetQuestNpc(
			InvalidCandidate,
			TEXT("Npc.Unknown"),
			&Error));
	TestTrue(TEXT("unknown NPC rejection is atomic"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&InvalidCandidate,
			&BeforeInvalid,
			PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPermanentNpcChallengeLifecycleTest,
	"GameXXK.Training.PermanentNpcChallengeLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPermanentNpcChallengeLifecycleTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("challenge fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	TestTrue(TEXT("challenge fixture selects Yue Bai"),
		Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	const FName StageId =
		FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("challenge route starts"), Subsystem->StartTrainingChallenge(StageId));
	TestTrue(TEXT("challenge route locks formation immediately"),
		Subsystem->GetRuntimeState().CardRun.bLoadoutLockedForRoute);
	TestFalse(TEXT("challenge route rejects NPC replacement"),
		Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.JinGui")));

	FName NpcDuringChallenge;
	TestTrue(TEXT("challenge resolves its frozen NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcDuringChallenge));
	TestEqual(TEXT("challenge keeps Yue Bai"),
		NpcDuringChallenge,
		FName(TEXT("Npc.YueBai")));
	TestTrue(TEXT("challenge can return to Workbench"),
		Subsystem->CancelTrainingChallengeToWorkbench());

	FName NpcAfterCancel;
	TestTrue(TEXT("cancelled challenge still resolves NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			NpcAfterCancel));
	TestEqual(TEXT("challenge cancellation preserves Yue Bai"),
		NpcAfterCancel,
		FName(TEXT("Npc.YueBai")));
	return true;
}

#endif
