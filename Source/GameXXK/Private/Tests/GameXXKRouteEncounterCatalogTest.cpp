#include "Misc/AutomationTest.h"

#if __has_include("GameXXKRouteEncounterCatalog.h")
#define GAMEXXK_HAS_ROUTE_ENCOUNTER_CATALOG 1
#include "GameXXKRouteEncounterCatalog.h"
#else
#define GAMEXXK_HAS_ROUTE_ENCOUNTER_CATALOG 0
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterCatalogTest,
	"GameXXK.Route.Encounters.TwelvePositiveInteractiveEntries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterCatalogTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_ROUTE_ENCOUNTER_CATALOG
	AddError(TEXT("The required twelve-entry positive event/chest catalog has not been implemented."));
	return false;
#else
	const TArray<FGameXXKRouteEncounterDefinition>& Definitions = FGameXXKRouteEncounterCatalog::GetAllDefinitions();
	TestEqual(TEXT("the catalog contains exactly twelve different event/chest entries"), Definitions.Num(), 12);
	TSet<FName> UniqueIds;
	int32 EventCount = 0;
	int32 ChestCount = 0;
	for (const FGameXXKRouteEncounterDefinition& Definition : Definitions)
	{
		UniqueIds.Add(Definition.Id);
		TestTrue(TEXT("every encounter is explicitly positive"), Definition.bPositiveOnly);
		TestTrue(TEXT("every encounter presents at least two clickable choices"), Definition.Choices.Num() >= 2);
		TestFalse(TEXT("every encounter has player-facing copy"), Definition.Title.IsEmpty() || Definition.Body.IsEmpty());
		EventCount += Definition.Kind == EGameXXKRouteEncounterKind::Event ? 1 : 0;
		ChestCount += Definition.Kind == EGameXXKRouteEncounterKind::Chest ? 1 : 0;
		if (Definition.Kind == EGameXXKRouteEncounterKind::Event)
		{
			TestEqual(TEXT("every question-mark event offers only route-local character attributes"),
				Definition.Choices.FilterByPredicate([](const FGameXXKRouteEncounterChoiceDefinition& Choice)
				{
					return Choice.RewardKind == EGameXXKRouteEncounterRewardKind::RouteAttribute;
				}).Num(),
				Definition.Choices.Num());
		}
		else
		{
			TestEqual(TEXT("every treasure reward is a three-relic choice"), Definition.Choices.Num(), 3);
			TestTrue(TEXT("all three treasure choices award relics"),
				Definition.Choices.FilterByPredicate([](const FGameXXKRouteEncounterChoiceDefinition& Choice)
				{
					return Choice.RewardKind == EGameXXKRouteEncounterRewardKind::Relic;
				}).Num() == 3);
		}
	}
	TestEqual(TEXT("all twelve encounter ids are distinct"), UniqueIds.Num(), 12);
	TestTrue(TEXT("the pool contains random question-mark events"), EventCount > 0);
	TestTrue(TEXT("the pool contains random treasure-chest entries"), ChestCount > 0);
	return true;
#endif
}

#endif
