#include "Misc/AutomationTest.h"

#include "GameXXKTalentCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr float GridPitch = 180.0f;
	constexpr float NodeFootprintWidth = 130.0f;
	constexpr float NodeFootprintHeight = 132.0f;

	bool IsGridSnapped(const FVector2D Position)
	{
		return FMath::IsNearlyEqual(Position.X / GridPitch, FMath::RoundToFloat(Position.X / GridPitch), 0.001f)
			&& FMath::IsNearlyEqual(Position.Y / GridPitch, FMath::RoundToFloat(Position.Y / GridPitch), 0.001f);
	}

	bool IsAllowedConnection(const FVector2D Delta)
	{
		const float X = FMath::Abs(Delta.X);
		const float Y = FMath::Abs(Delta.Y);
		return (FMath::IsNearlyZero(X) && Y > 0.0f)
			|| (FMath::IsNearlyZero(Y) && X > 0.0f)
			|| (X > 0.0f && FMath::IsNearlyEqual(X, Y, 0.1f));
	}

	bool IsInsideBranchQuadrant(const FGameXXKTalentNodeDefinition& Node)
	{
		if (Node.bRoot)
		{
			return Node.GraphPosition.IsNearlyZero();
		}
		switch (Node.Branch)
		{
		case EGameXXKTalentBranch::Combat:
			return Node.GraphPosition.X < 0.0f && Node.GraphPosition.Y < 0.0f;
		case EGameXXKTalentBranch::CapacityChest:
			return Node.GraphPosition.X < 0.0f && Node.GraphPosition.Y > 0.0f;
		case EGameXXKTalentBranch::IdleOffline:
			return Node.GraphPosition.X > 0.0f && Node.GraphPosition.Y < 0.0f;
		case EGameXXKTalentBranch::Tools:
			return Node.GraphPosition.X > 0.0f && Node.GraphPosition.Y > 0.0f;
		default:
			return false;
		}
	}

	bool FootprintsOverlap(
		const FGameXXKTalentNodeDefinition& Left,
		const FGameXXKTalentNodeDefinition& Right)
	{
		const FVector2D Delta = (Left.GraphPosition - Right.GraphPosition).GetAbs();
		return Delta.X < NodeFootprintWidth && Delta.Y < NodeFootprintHeight;
	}

	void TestPrerequisites(
		FAutomationTestBase& Test,
		const FName NodeId,
		const TArray<FName>& Expected)
	{
		const FGameXXKTalentNodeDefinition* Node = FGameXXKTalentCatalog::Find(NodeId);
		if (!Test.TestNotNull(*FString::Printf(TEXT("%s exists"), *NodeId.ToString()), Node))
		{
			return;
		}
		Test.TestEqual(TEXT("prerequisite count"), Node->PrerequisiteIds.Num(), Expected.Num());
		for (const FName ExpectedId : Expected)
		{
			Test.TestTrue(
				*FString::Printf(TEXT("%s requires %s"), *NodeId.ToString(), *ExpectedId.ToString()),
				Node->PrerequisiteIds.Contains(ExpectedId));
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentGridLayoutTest,
	"GameXXK.Talents.Grid.FixedAnglesQuadrantsAndNoOverlap",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentGridLayoutTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKTalentNodeDefinition>& Nodes = FGameXXKTalentCatalog::GetDefinitions();
	for (int32 Index = 0; Index < Nodes.Num(); ++Index)
	{
		const FGameXXKTalentNodeDefinition& Node = Nodes[Index];
		TestTrue(
			*FString::Printf(TEXT("%s is snapped to the 180 grid"), *Node.Id.ToString()),
			IsGridSnapped(Node.GraphPosition));
		TestTrue(
			*FString::Printf(TEXT("%s stays in its quadrant"), *Node.Id.ToString()),
			IsInsideBranchQuadrant(Node));
		for (const FName PrerequisiteId : Node.PrerequisiteIds)
		{
			const FGameXXKTalentNodeDefinition* Prerequisite = FGameXXKTalentCatalog::Find(PrerequisiteId);
			if (TestNotNull(TEXT("connection prerequisite exists"), Prerequisite))
			{
				TestTrue(
					*FString::Printf(TEXT("%s connection uses an approved angle"), *Node.Id.ToString()),
					IsAllowedConnection(Node.GraphPosition - Prerequisite->GraphPosition));
			}
		}
		for (const FName VisualSourceId : Node.VisualConnectionIds)
		{
			const FGameXXKTalentNodeDefinition* VisualSource = FGameXXKTalentCatalog::Find(VisualSourceId);
			if (TestNotNull(TEXT("visual connection source exists"), VisualSource))
			{
				TestTrue(
					*FString::Printf(TEXT("%s visual connection uses an approved angle"), *Node.Id.ToString()),
					IsAllowedConnection(Node.GraphPosition - VisualSource->GraphPosition));
			}
		}
		for (int32 OtherIndex = Index + 1; OtherIndex < Nodes.Num(); ++OtherIndex)
		{
			TestFalse(
				*FString::Printf(TEXT("%s does not overlap %s"),
					*Node.Id.ToString(),
					*Nodes[OtherIndex].Id.ToString()),
				FootprintsOverlap(Node, Nodes[OtherIndex]));
		}
	}

	TestPrerequisites(*this, TEXT("Talent.Combat.FlatAttack.01"), {TEXT("Talent.Entry.Combat")});
	TestPrerequisites(*this, TEXT("Talent.Combat.FlatHealth.01"), {TEXT("Talent.Entry.Combat")});
	TestPrerequisites(*this, TEXT("Talent.Combat.FlatAttack.08"),
		{TEXT("Talent.Entry.Combat")});
	const FGameXXKTalentNodeDefinition* SecondCombatMain =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatAttack.08"));
	TestTrue(TEXT("next main draws its diagonal from the previous main"),
		SecondCombatMain
			&& SecondCombatMain->VisualConnectionIds.Num() == 1
			&& SecondCombatMain->VisualConnectionIds[0] == FName(TEXT("Talent.Entry.Combat")));
	TestPrerequisites(*this, TEXT("Talent.Tools.Experience.01"), {TEXT("Talent.Entry.Tools")});
	TestPrerequisites(*this, TEXT("Talent.Tools.Gold.01"), {TEXT("Talent.Entry.Tools")});
	return true;
}

#endif
