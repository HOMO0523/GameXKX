#include "GameXXKMVPRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcRouteEventCatalogRetirementTest,
	"GameXXK.Route.Event.NpcCatalogRetired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcRouteEventCatalogRetirementTest::RunTest(const FString& Parameters)
{
	const TSet<FName> RemovedIds = {
		TEXT("Encounter.Event.TusiChief"),
		TEXT("Encounter.Event.SongJinBao"),
		TEXT("Encounter.Event.YueBai"),
		TEXT("Encounter.Event.ZhouGuangZu"),
		TEXT("Encounter.Event.JinGui"),
		TEXT("Encounter.Event.QiongMeiEr"),
		TEXT("Encounter.Event.NiuHuan")};
	const TArray<const FGameXXKRouteEncounterDefinition*> Events =
		FGameXXKRouteEncounterCatalog::GetDefinitionsOfKind(
			EGameXXKRouteEncounterKind::Event);
	TestEqual(TEXT("only one environmental event remains"), Events.Num(), 1);
	for (const FName RemovedId : RemovedIds)
	{
		TestTrue(TEXT("removed event remains recognizable for migration"),
			FGameXXKRouteEncounterCatalog::IsRetiredNpcEncounterId(RemovedId));
		TestNull(TEXT("removed NPC event has no live definition"),
			FGameXXKRouteEncounterCatalog::FindDefinition(RemovedId));
	}
	for (const FGameXXKRouteEncounterDefinition* Event : Events)
	{
		TestNotNull(TEXT("event definition exists"), Event);
		if (!Event)
		{
			continue;
		}
		TestFalse(TEXT("removed NPC event cannot be generated"), RemovedIds.Contains(Event->Id));
		TestEqual(TEXT("remaining event is Mountain Spring"),
			Event->Id,
			FName(TEXT("Encounter.Event.MountainSpring")));
		for (const FGameXXKRouteEncounterChoiceDefinition& Choice : Event->Choices)
		{
			TestEqual(TEXT("environment choice is route-attribute only"),
				Choice.RewardKind,
				EGameXXKRouteEncounterRewardKind::RouteAttribute);
			TestTrue(TEXT("environment choice has no NPC owner"), Choice.QuestNpcId.IsNone());
		}
	}
	TestEqual(TEXT("route attribute ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::RouteAttribute),
		uint8(0));
	TestEqual(TEXT("temporary support tombstone ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport),
		uint8(1));
	TestEqual(TEXT("relic ordinal is frozen"),
		static_cast<uint8>(EGameXXKRouteEncounterRewardKind::Relic),
		uint8(2));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcRouteEventSupportFacadeRetirementTest,
	"GameXXK.Route.Event.NpcSupportFacadeRetired",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcRouteEventSupportFacadeRetirementTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState RuleState = UGameXXKMVPRules::CreateNewGame();
	const FGameXXKRuntimeState RuleBefore = RuleState;
	TestFalse(TEXT("retired rules facade always rejects"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(RuleState));
	TestTrue(TEXT("retired rules facade is observational"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&RuleState,
			&RuleBefore,
			PPF_None));

	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("subsystem facade fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FGameXXKRuntimeState SubsystemBefore = Subsystem->GetRuntimeStateCopy();
	TestFalse(TEXT("retired subsystem facade always rejects"),
		Subsystem->AcceptRouteEventNpcSupport());
	TestTrue(TEXT("retired subsystem facade is observational"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&Subsystem->GetRuntimeState(),
			&SubsystemBefore,
			PPF_None));
	return true;
}

#endif
