#include "Misc/AutomationTest.h"

#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentCatalogTopologyTest,
	"GameXXK.Talents.Catalog.RootFourBranchesAndThirtyFiveLayers",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentCatalogTopologyTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTalentNodeDefinition>& Nodes = FGameXXKTalentCatalog::GetDefinitions();
	int32 RootCount = 0;
	int32 EntryCount = 0;
	for (const FGameXXKTalentNodeDefinition& Node : Nodes)
	{
		RootCount += Node.bRoot ? 1 : 0;
		EntryCount += Node.bBranchEntry ? 1 : 0;
	}
	TestEqual(TEXT("catalog contains one root"), RootCount, 1);
	TestEqual(TEXT("catalog contains four one-time entries"), EntryCount, 4);
	TestEqual(TEXT("maximum authored depth is thirty-five"), FGameXXKTalentCatalog::GetMaxCostTier(), 35);
	FString Error;
	TestTrue(FString::Printf(TEXT("catalog validates: %s"), *Error), FGameXXKTalentCatalog::Validate(&Error));

	const FGameXXKTalentNodeDefinition* Root = FGameXXKTalentCatalog::Find(TEXT("Talent.Root"));
	TestTrue(TEXT("root is one-time and costs 2500"),
		Root && Root->MaxRank == 1 && FGameXXKTalentRules::GetRankPrice(*Root, 0) == 2500);
	for (const EGameXXKTalentBranch Branch : {
		EGameXXKTalentBranch::Combat,
		EGameXXKTalentBranch::CapacityChest,
		EGameXXKTalentBranch::IdleOffline,
		EGameXXKTalentBranch::Tools})
	{
		const FGameXXKTalentNodeDefinition* Entry = FGameXXKTalentCatalog::FindBranchEntry(Branch);
		TestTrue(TEXT("every branch owns one one-time entry"),
			Entry && Entry->MaxRank == 1 && Entry->CostTier == 0);
	}
	return true;
}

#endif
