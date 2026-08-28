#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

#include "GameXXKTutorialRouteRules.h"
#include "Guide/GameXXKGuideAsset.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialRouteExactGraphTest,
	"GameXXK.Guide.TutorialRoute.ExactGraph",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialRouteExactGraphTest::RunTest(const FString& Parameters)
{
	const FGameXXKTutorialRouteDefinition First = FGameXXKTutorialRouteRules::BuildDefinition(1);
	const FGameXXKTutorialRouteDefinition OtherSeed = FGameXXKTutorialRouteRules::BuildDefinition(987654321);
	TestEqual(TEXT("fixed route ID"), First.RouteId, FName(TEXT("Route.Tutorial.CombatBasics")));
	TestEqual(TEXT("fixed route has eight nodes"), First.Nodes.Num(), 8);
	TestEqual(TEXT("fixed route has seven adjacent edges"), First.Edges.Num(), 7);

	const TArray<FName> ExpectedIds = {
		TEXT("Tutorial.Start"),
		TEXT("Tutorial.Battle.0-1"),
		TEXT("Tutorial.Merchant.0-1"),
		TEXT("Tutorial.Event.0-1"),
		TEXT("Tutorial.Camp.0-1"),
		TEXT("Tutorial.Chest.0-1"),
		TEXT("Tutorial.Boss.0-1"),
		TEXT("Tutorial.Settlement")};
	const TArray<EGameXXKTutorialRouteNodeKind> ExpectedKinds = {
		EGameXXKTutorialRouteNodeKind::Start,
		EGameXXKTutorialRouteNodeKind::Battle,
		EGameXXKTutorialRouteNodeKind::Merchant,
		EGameXXKTutorialRouteNodeKind::Event,
		EGameXXKTutorialRouteNodeKind::Camp,
		EGameXXKTutorialRouteNodeKind::Chest,
		EGameXXKTutorialRouteNodeKind::Boss,
		EGameXXKTutorialRouteNodeKind::Settlement};
	const TArray<FName> ExpectedGuides = {
		TEXT("Guide.RouteMap.Basic"),
		TEXT("Guide.Battle.Basic"),
		TEXT("Guide.Merchant.Basic"),
		TEXT("Guide.Event.Basic"),
		TEXT("Guide.Camp.Basic"),
		TEXT("Guide.Chest.Basic"),
		TEXT("Guide.Boss.Basic"),
		TEXT("Guide.Settlement.Basic")};
	for (int32 Index = 0; Index < ExpectedIds.Num(); ++Index)
	{
		TestEqual(FString::Printf(TEXT("node %d ID"), Index), First.Nodes[Index].NodeId, ExpectedIds[Index]);
		TestEqual(FString::Printf(TEXT("node %d kind"), Index), First.Nodes[Index].Kind, ExpectedKinds[Index]);
		TestEqual(FString::Printf(TEXT("node %d guide"), Index), First.Nodes[Index].GuideId, ExpectedGuides[Index]);
		TestEqual(FString::Printf(TEXT("seed cannot change node %d"), Index), OtherSeed.Nodes[Index].NodeId, First.Nodes[Index].NodeId);
		if (Index < ExpectedIds.Num() - 1)
		{
			TestEqual(FString::Printf(TEXT("edge %d source"), Index), First.Edges[Index].FromNodeId, ExpectedIds[Index]);
			TestEqual(FString::Printf(TEXT("edge %d target"), Index), First.Edges[Index].ToNodeId, ExpectedIds[Index + 1]);
		}
	}
	TestEqual(TEXT("seed cannot change edge count"), OtherSeed.Edges.Num(), First.Edges.Num());
	for (int32 Index = 0; Index < First.Edges.Num(); ++Index)
	{
		TestEqual(TEXT("seed cannot change edge source"), OtherSeed.Edges[Index].FromNodeId, First.Edges[Index].FromNodeId);
		TestEqual(TEXT("seed cannot change edge target"), OtherSeed.Edges[Index].ToNodeId, First.Edges[Index].ToNodeId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialRouteProgressionTest,
	"GameXXK.Guide.TutorialRoute.LinearProgression",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialRouteProgressionTest::RunTest(const FString& Parameters)
{
	const FGameXXKTutorialRouteDefinition Definition = FGameXXKTutorialRouteRules::BuildDefinition();
	FGameXXKTutorialRouteProgress Progress = FGameXXKTutorialRouteRules::CreateInitialProgress(Definition);
	TestEqual(TEXT("start is current"), Progress.CurrentNodeId, FName(TEXT("Tutorial.Start")));
	TestTrue(TEXT("start is auto occupied"), Progress.CompletedNodeIds.Contains(TEXT("Tutorial.Start")));
	TestFalse(TEXT("start is never reachable"), Progress.ReachableNodeIds.Contains(TEXT("Tutorial.Start")));
	TestEqual(TEXT("only battle is initially reachable"), Progress.ReachableNodeIds.Array(), TArray<FName>{TEXT("Tutorial.Battle.0-1")});
	FString Error;
	TestFalse(TEXT("already occupied start cannot be selected"),
		FGameXXKTutorialRouteRules::CompleteReachableNode(
			Definition, TEXT("Tutorial.Start"), Progress, &Error));

	for (int32 Index = 1; Index < Definition.Nodes.Num(); ++Index)
	{
		const FName NodeId = Definition.Nodes[Index].NodeId;
		TestEqual(TEXT("exactly one next node is reachable before completion"), Progress.ReachableNodeIds.Num(), 1);
		TestTrue(TEXT("the expected next node is reachable"), Progress.ReachableNodeIds.Contains(NodeId));
		TestTrue(FString::Printf(TEXT("complete %s: %s"), *NodeId.ToString(), *Error),
			FGameXXKTutorialRouteRules::CompleteReachableNode(Definition, NodeId, Progress, &Error));
		TestTrue(TEXT("completed node is recorded"), Progress.CompletedNodeIds.Contains(NodeId));
		if (Index + 1 < Definition.Nodes.Num())
		{
			TestEqual(TEXT("only following node becomes reachable"), Progress.ReachableNodeIds.Num(), 1);
			TestTrue(TEXT("following node is reachable"),
				Progress.ReachableNodeIds.Contains(Definition.Nodes[Index + 1].NodeId));
		}
	}
	TestTrue(TEXT("settlement completes tutorial route"), Progress.bCompleted);
	TestTrue(TEXT("completed route exposes no reachable nodes"), Progress.ReachableNodeIds.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialCampChoicesTest,
	"GameXXK.Guide.TutorialRoute.CampChoices",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialCampChoicesTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTutorialCampActionDefinition> Actions = FGameXXKTutorialRouteRules::GetCampActions();
	TestEqual(TEXT("tutorial camp has exactly two actions"), Actions.Num(), 2);
	TestEqual(TEXT("first action is party healing"), Actions[0].Choice, EGameXXKTutorialCampChoice::HealPartyThirtyPercent);
	TestEqual(TEXT("healing action is exactly 30 percent"), Actions[0].HealingPercent, 30);
	TestEqual(TEXT("second action is route gold"), Actions[1].Choice, EGameXXKTutorialCampChoice::GainRouteGold);
	TestEqual(TEXT("route-gold action grants exactly 100"), Actions[1].RouteGold, 100);
	const UScriptStruct* ActionStruct = FGameXXKTutorialCampActionDefinition::StaticStruct();
	TestNull(TEXT("camp action exposes no relic reward"), ActionStruct->FindPropertyByName(TEXT("RelicId")));
	TestNull(TEXT("camp action exposes no item reward"), ActionStruct->FindPropertyByName(TEXT("ItemId")));

	TArray<FGameXXKTutorialPartyHealth> Party = {
		{TEXT("Hero"), 15, 100},
		{TEXT("Companion"), 95, 100},
		{TEXT("QuestNpc"), 0, 7}};
	int32 RouteGold = 20;
	FString Error;
	TestTrue(TEXT("heal choice resolves"), FGameXXKTutorialRouteRules::ResolveCampChoice(
		EGameXXKTutorialCampChoice::HealPartyThirtyPercent, Party, RouteGold, &Error));
	TestEqual(TEXT("hero gains ceil 30 percent max health"), Party[0].CurrentHealth, 45);
	TestEqual(TEXT("healing clamps at maximum"), Party[1].CurrentHealth, 100);
	TestEqual(TEXT("ceil applies to small maximum"), Party[2].CurrentHealth, 3);
	TestEqual(TEXT("healing does not change route gold"), RouteGold, 20);

	const TArray<FGameXXKTutorialPartyHealth> BeforeGold = Party;
	TestTrue(TEXT("gold choice resolves"), FGameXXKTutorialRouteRules::ResolveCampChoice(
		EGameXXKTutorialCampChoice::GainRouteGold, Party, RouteGold, &Error));
	TestEqual(TEXT("gold choice grants 100"), RouteGold, 120);
	for (int32 Index = 0; Index < Party.Num(); ++Index)
	{
		TestEqual(TEXT("gold choice does not change health"), Party[Index].CurrentHealth, BeforeGold[Index].CurrentHealth);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorialGuideImportedAssetsTest,
	"GameXXK.Guide.TutorialRoute.ImportedAssets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorialGuideImportedAssetsTest::RunTest(const FString& Parameters)
{
	const TArray<FName> GuideIds = {
		TEXT("Guide.RouteMap.Basic"),
		TEXT("Guide.Battle.Basic"),
		TEXT("Guide.Merchant.Basic"),
		TEXT("Guide.Event.Basic"),
		TEXT("Guide.Camp.Basic"),
		TEXT("Guide.Chest.Basic"),
		TEXT("Guide.Boss.Basic"),
		TEXT("Guide.Settlement.Basic")};
	for (const FName GuideId : GuideIds)
	{
		FString AssetName = GuideId.ToString();
		AssetName.ReplaceInline(TEXT("."), TEXT("_"));
		AssetName = TEXT("DA_") + AssetName;
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/GameXXK/Narrative/Guides/%s.%s"),
			*AssetName,
			*AssetName);
		UGameXXKGuideAsset* Asset = LoadObject<UGameXXKGuideAsset>(nullptr, *ObjectPath);
		if (!TestNotNull(FString::Printf(TEXT("guide asset loads: %s"), *GuideId.ToString()), Asset))
		{
			continue;
		}
		TestEqual(TEXT("loaded guide keeps semantic ID"), Asset->GuideId, GuideId);
		TestTrue(TEXT("loaded guide has an entry step"), Asset->FindStep(Asset->EntryStepId) != nullptr);
		if (GuideId == TEXT("Guide.Boss.Basic"))
		{
			for (const FGameXXKGuideStepDefinition& Step : Asset->Steps)
			{
				TestEqual(TEXT("boss guide remains soft-only"), Step.InputPolicy, EGameXXKGuideInputPolicy::Soft);
			}
		}
	}
	return true;
}

#endif
