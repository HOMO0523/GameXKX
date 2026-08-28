#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"

#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDialogueAssetContractTest,
	"GameXXK.Dialogue.Core.AssetContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDialogueAssetContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDialogueAsset* Asset = NewObject<UGameXXKDialogueAsset>();
	Asset->DialogueId = TEXT("Dialogue.Test.Branching");
	Asset->DialogueVersion = 1;
	Asset->EntryNodeId = TEXT("start");

	FGameXXKDialogueNodeDefinition Start;
	Start.NodeId = TEXT("start");
	Start.Type = EGameXXKDialogueNodeType::Line;
	Start.NextNodeId = TEXT("end");
	Asset->Nodes.Add(Start);

	FGameXXKDialogueNodeDefinition End;
	End.NodeId = TEXT("end");
	End.Type = EGameXXKDialogueNodeType::End;
	End.EndOutcomeId = TEXT("Outcome.Test.Done");
	Asset->Nodes.Add(End);

	TestNotNull(TEXT("entry resolves"), Asset->FindNode(TEXT("start")));
	TestNull(TEXT("missing node rejects"), Asset->FindNode(TEXT("missing")));

	FDataValidationContext ValidContext;
	TestEqual(TEXT("unique node ids validate"), Asset->IsDataValid(ValidContext), EDataValidationResult::Valid);

	Asset->Nodes.Add(Start);
	FDataValidationContext DuplicateContext;
	TestEqual(TEXT("duplicate node ids reject"), Asset->IsDataValid(DuplicateContext), EDataValidationResult::Invalid);
	return true;
}

#endif
