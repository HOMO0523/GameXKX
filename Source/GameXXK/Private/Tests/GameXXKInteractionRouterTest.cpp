#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Dialogue/GameXXKDialogueAsset.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Actor.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Interaction/GameXXKInteractionRules.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKNarrativeSequenceAsset.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKSpeechBubbleWidget.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKInteractionRouterTestPrivate
{
	UGameXXKNarrativeSequenceAsset* MakeNpcWaitSequence()
	{
		const FString AssetName(TEXT("DA_Sequence_Npc_TusiChief_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_TusiChief_Default"));
		UGameXXKNarrativeSequenceAsset* Asset = FindObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->SequenceId = TEXT("Sequence.Npc.TusiChief.Default");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Town.NpcInteraction");
		Asset->EntryStepId = TEXT("wait_for_input");
		Asset->Steps.Reset();
		FGameXXKNarrativeSequenceStepDefinition Wait;
		Wait.StepId = TEXT("wait_for_input");
		Wait.Type = EGameXXKNarrativeStepType::Wait;
		Wait.WaitType = TEXT("input");
		Wait.NextStepId = TEXT("end");
		Asset->Steps.Add(Wait);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKDialogueAsset* MakeNpcDialogueAsset()
	{
		const FString AssetName(TEXT("DA_Dialogue_Npc_YueBai_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Npc_YueBai_Default"));
		UGameXXKDialogueAsset* Asset = FindObject<UGameXXKDialogueAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKDialogueAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->DialogueId = TEXT("Dialogue.Npc.YueBai.Default");
		Asset->DialogueVersion = 1;
		Asset->EntryNodeId = TEXT("line");
		Asset->Nodes.Reset();
		FGameXXKDialogueNodeDefinition Line;
		Line.NodeId = TEXT("line");
		Line.Type = EGameXXKDialogueNodeType::Line;
		Line.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		Line.SpeakerId = TEXT("Npc.YueBai");
		Line.TextId = TEXT("Text.Npc.YueBai.Test");
		Line.Text = FText::FromString(TEXT("你是谁？"));
		Line.NextNodeId = TEXT("end");
		Asset->Nodes.Add(Line);
		FGameXXKDialogueNodeDefinition End;
		End.NodeId = TEXT("end");
		End.Type = EGameXXKDialogueNodeType::End;
		End.EndOutcomeId = TEXT("Outcome.Done");
		Asset->Nodes.Add(End);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeNpcDialogueSequence()
	{
		const FString AssetName(TEXT("DA_Sequence_Npc_YueBai_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_YueBai_Default"));
		UGameXXKNarrativeSequenceAsset* Asset = FindObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->SequenceId = TEXT("Sequence.Npc.YueBai.Default");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Town.NpcInteraction");
		Asset->EntryStepId = TEXT("dialogue");
		Asset->Steps.Reset();
		FGameXXKNarrativeSequenceStepDefinition Dialogue;
		Dialogue.StepId = TEXT("dialogue");
		Dialogue.Type = EGameXXKNarrativeStepType::Dialogue;
		Dialogue.DialogueId = TEXT("Dialogue.Npc.YueBai.Default");
		Dialogue.NextStepId = TEXT("end");
		Asset->Steps.Add(Dialogue);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKDialogueAsset* MakeNpcBubbleDialogueAsset()
	{
		const FString AssetName(TEXT("DA_Dialogue_Npc_JinGui_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Npc_JinGui_Default"));
		UGameXXKDialogueAsset* Asset = FindObject<UGameXXKDialogueAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKDialogueAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->DialogueId = TEXT("Dialogue.Npc.JinGui.Default");
		Asset->DialogueVersion = 1;
		Asset->EntryNodeId = TEXT("bubble");
		Asset->Nodes.Reset();
		FGameXXKDialogueNodeDefinition Bubble;
		Bubble.NodeId = TEXT("bubble");
		Bubble.Type = EGameXXKDialogueNodeType::Line;
		Bubble.Presentation = EGameXXKDialoguePresentation::Bubble;
		Bubble.SpeakerId = TEXT("Npc.JinGui");
		Bubble.TextId = TEXT("Text.Npc.JinGui.Test");
		Bubble.Text = FText::FromString(TEXT("这边请。"));
		Bubble.NextNodeId = TEXT("end");
		Asset->Nodes.Add(Bubble);
		FGameXXKDialogueNodeDefinition End;
		End.NodeId = TEXT("end");
		End.Type = EGameXXKDialogueNodeType::End;
		End.EndOutcomeId = TEXT("Outcome.Done");
		Asset->Nodes.Add(End);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeNpcBubbleSequence()
	{
		const FString AssetName(TEXT("DA_Sequence_Npc_JinGui_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_JinGui_Default"));
		UGameXXKNarrativeSequenceAsset* Asset = FindObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->SequenceId = TEXT("Sequence.Npc.JinGui.Default");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Town.NpcInteraction");
		Asset->EntryStepId = TEXT("dialogue");
		Asset->Steps.Reset();
		FGameXXKNarrativeSequenceStepDefinition Dialogue;
		Dialogue.StepId = TEXT("dialogue");
		Dialogue.Type = EGameXXKNarrativeStepType::Dialogue;
		Dialogue.DialogueId = TEXT("Dialogue.Npc.JinGui.Default");
		Dialogue.NextStepId = TEXT("end");
		Asset->Steps.Add(Dialogue);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKDialogueAsset* MakeMerchantChoiceDialogueAsset()
	{
		const FString AssetName(TEXT("DA_Dialogue_Npc_SongJinBao_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Dialogues/DA_Dialogue_Npc_SongJinBao_Default"));
		UGameXXKDialogueAsset* Asset = FindObject<UGameXXKDialogueAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKDialogueAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->DialogueId = TEXT("Dialogue.Npc.SongJinBao.Default");
		Asset->DialogueVersion = 1;
		Asset->EntryNodeId = TEXT("choice");
		Asset->Nodes.Reset();
		FGameXXKDialogueNodeDefinition Choice;
		Choice.NodeId = TEXT("choice");
		Choice.Type = EGameXXKDialogueNodeType::Choice;
		Choice.Presentation = EGameXXKDialoguePresentation::DialoguePanel;
		Choice.SpeakerId = TEXT("Npc.SongJinBao");
		Choice.TextId = TEXT("Text.Npc.SongJinBao.Menu");
		Choice.Text = FText::FromString(TEXT("想看看什么？"));
		FGameXXKDialogueOptionDefinition Shop;
		Shop.OptionId = TEXT("Option.Shop");
		Shop.TextId = TEXT("Text.Option.Shop");
		Shop.Text = FText::FromString(TEXT("商店"));
		Shop.NextNodeId = TEXT("end_shop");
		Choice.Options.Add(Shop);
		FGameXXKDialogueOptionDefinition Leave;
		Leave.OptionId = TEXT("Option.Leave");
		Leave.TextId = TEXT("Text.Option.Leave");
		Leave.Text = FText::FromString(TEXT("暂且离开"));
		Leave.NextNodeId = TEXT("end_leave");
		Choice.Options.Add(Leave);
		Asset->Nodes.Add(Choice);
		FGameXXKDialogueNodeDefinition ShopEnd;
		ShopEnd.NodeId = TEXT("end_shop");
		ShopEnd.Type = EGameXXKDialogueNodeType::End;
		ShopEnd.EndOutcomeId = TEXT("Outcome.Shop");
		Asset->Nodes.Add(ShopEnd);
		FGameXXKDialogueNodeDefinition LeaveEnd;
		LeaveEnd.NodeId = TEXT("end_leave");
		LeaveEnd.Type = EGameXXKDialogueNodeType::End;
		LeaveEnd.EndOutcomeId = TEXT("Outcome.Leave");
		Asset->Nodes.Add(LeaveEnd);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeMerchantChoiceSequence()
	{
		const FString AssetName(TEXT("DA_Sequence_Npc_SongJinBao_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_SongJinBao_Default"));
		UGameXXKNarrativeSequenceAsset* Asset = FindObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->SequenceId = TEXT("Sequence.Npc.SongJinBao.Default");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Town.NpcInteraction");
		Asset->EntryStepId = TEXT("dialogue");
		Asset->Steps.Reset();
		FGameXXKNarrativeSequenceStepDefinition Dialogue;
		Dialogue.StepId = TEXT("dialogue");
		Dialogue.Type = EGameXXKNarrativeStepType::Dialogue;
		Dialogue.DialogueId = TEXT("Dialogue.Npc.SongJinBao.Default");
		Dialogue.NextStepId = TEXT("branch");
		Asset->Steps.Add(Dialogue);
		FGameXXKNarrativeSequenceStepDefinition Branch;
		Branch.StepId = TEXT("branch");
		Branch.Type = EGameXXKNarrativeStepType::BranchOnOutcome;
		Branch.OutcomeToStepId.Add(TEXT("Outcome.Shop"), TEXT("shop"));
		Branch.OutcomeToStepId.Add(TEXT("Outcome.Leave"), TEXT("end"));
		Asset->Steps.Add(Branch);
		FGameXXKNarrativeSequenceStepDefinition Shop;
		Shop.StepId = TEXT("shop");
		Shop.Type = EGameXXKNarrativeStepType::Command;
		Shop.Command.CommandId = TEXT("open_shop");
		Shop.Command.CommandType = TEXT("openShop");
		Shop.NextStepId = TEXT("end");
		Asset->Steps.Add(Shop);
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}

	UGameXXKNarrativeSequenceAsset* MakeImmediateNpcSequence()
	{
		const FString AssetName(TEXT("DA_Sequence_Npc_QiongMeiEr_Default"));
		UPackage* Package = CreatePackage(TEXT("/Game/GameXXK/Narrative/Sequences/DA_Sequence_Npc_QiongMeiEr_Default"));
		UGameXXKNarrativeSequenceAsset* Asset = FindObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName);
		if (!Asset)
		{
			Asset = NewObject<UGameXXKNarrativeSequenceAsset>(Package, *AssetName, RF_Public | RF_Standalone);
		}
		Asset->SequenceId = TEXT("Sequence.Npc.QiongMeiEr.Default");
		Asset->SequenceVersion = 1;
		Asset->StageContractId = TEXT("Stage.Town.NpcInteraction");
		Asset->EntryStepId = TEXT("end");
		Asset->Steps.Reset();
		FGameXXKNarrativeSequenceStepDefinition End;
		End.StepId = TEXT("end");
		End.Type = EGameXXKNarrativeStepType::End;
		Asset->Steps.Add(End);
		return Asset;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKInteractionCircleSelectionTest,
	"GameXXK.Interaction.Router.CircleAndStableOrdering",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInteractionCircleSelectionTest::RunTest(const FString& Parameters)
{
	FGameXXKInteractionCandidate A{TEXT("Npc.A"), 1, 200.0f};
	FGameXXKInteractionCandidate B{TEXT("Npc.B"), 2, 100.0f};
	TOptional<FGameXXKInteractionCandidate> Chosen = FGameXXKInteractionRules::Choose({A, B});
	TestTrue(TEXT("priority fixture chooses a candidate"), Chosen.IsSet());
	if (Chosen.IsSet())
	{
		TestEqual(TEXT("priority wins"), Chosen->InteractionId, FName(TEXT("Npc.B")));
	}

	FGameXXKInteractionCandidate RegisteredOverlap{TEXT("Npc.RegisteredOverlap"), 1, 336.0f};
	Chosen = FGameXXKInteractionRules::Choose({RegisteredOverlap});
	TestTrue(TEXT("an overlap-registered candidate is not filtered again by center distance"), Chosen.IsSet());
	if (Chosen.IsSet())
	{
		TestEqual(TEXT("the overlap event is authoritative"), Chosen->InteractionId, RegisteredOverlap.InteractionId);
	}

	const TArray<FGameXXKInteractionCandidate> StableCandidates = {
		{TEXT("Npc.C"), 3, 80.0f},
		{TEXT("Npc.B"), 3, 120.0f},
		{TEXT("Npc.A"), 3, 120.0f},
		{TEXT("Npc.D"), 3, 100.0f}};
	Chosen = FGameXXKInteractionRules::Choose(StableCandidates);
	TestTrue(TEXT("stable ordering chooses candidate"), Chosen.IsSet());
	if (Chosen.IsSet())
	{
		TestEqual(TEXT("distance then ID order ignores facing angle"), Chosen->InteractionId, FName(TEXT("Npc.C")));
	}
	TArray<FGameXXKInteractionCandidate> Reversed = StableCandidates;
	Algo::Reverse(Reversed);
	const TOptional<FGameXXKInteractionCandidate> ReversedChoice = FGameXXKInteractionRules::Choose(Reversed);
	TestTrue(TEXT("reverse input still chooses"), ReversedChoice.IsSet());
	if (Chosen.IsSet() && ReversedChoice.IsSet())
	{
		TestEqual(TEXT("selection is independent from scan order"), ReversedChoice->InteractionId, Chosen->InteractionId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKInteractableMetadataTest,
	"GameXXK.Interaction.Router.InteractableMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInteractableMetadataTest::RunTest(const FString& Parameters)
{
	AActor* Actor = NewObject<AActor>();
	USceneComponent* Anchor = NewObject<USceneComponent>(Actor);
	Actor->SetRootComponent(Anchor);
	UGameXXKInteractableComponent* Interactable = NewObject<UGameXXKInteractableComponent>(Actor);
	TestFalse(TEXT("unconfigured interaction metadata starts disabled"), Interactable->IsInteractionEnabled());
	Interactable->SetInteractionEnabled(true);
	TestFalse(TEXT("incomplete metadata cannot be force-enabled"), Interactable->IsInteractionEnabled());
	Interactable->Configure(
		TEXT("Npc.YueBai"),
		FText::FromString(TEXT("月白")),
		TEXT("Sequence.Npc.YueBai.Talk"),
		7,
		Anchor);
	TestEqual(TEXT("stable interaction ID"), Interactable->GetInteractionId(), FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("display name"), Interactable->GetDisplayName(), FText::FromString(TEXT("月白")));
	TestEqual(TEXT("sequence ID"), Interactable->GetNarrativeSequenceId(), FName(TEXT("Sequence.Npc.YueBai.Talk")));
	TestEqual(TEXT("priority"), Interactable->GetPriority(), 7);
	TestEqual(TEXT("prompt anchor"), Interactable->GetPromptAnchor(), Anchor);
	TestTrue(TEXT("configured interaction enabled"), Interactable->IsInteractionEnabled());
	Interactable->SetInteractionEnabled(false);
	TestFalse(TEXT("interaction may be disabled"), Interactable->IsInteractionEnabled());

	const UScriptStruct* CandidateStruct = FGameXXKInteractionCandidate::StaticStruct();
	TestNull(TEXT("pure candidate stores no actor pointer"), CandidateStruct->FindPropertyByName(TEXT("Actor")));
	TestNull(TEXT("pure candidate stores no widget pointer"), CandidateStruct->FindPropertyByName(TEXT("Widget")));
	UGameXXKInteractionComponent* Router = NewObject<UGameXXKInteractionComponent>(Actor);
	TestNull(
		TEXT("overlap-driven router has no global scan radius"),
		Router->GetClass()->FindPropertyByName(TEXT("ProximityInteractionRadius")));
	TestNull(
		TEXT("radial router has no facing-angle setting"),
		Router->GetClass()->FindPropertyByName(TEXT("InteractionHalfAngleDegrees")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcCircularOverlapContractTest,
	"GameXXK.Interaction.Router.NpcCircularOverlapContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcCircularOverlapContractTest::RunTest(const FString& Parameters)
{
	const auto VerifyArea = [this](const TCHAR* Label, const USphereComponent* Area)
	{
		TestNotNull(*FString::Printf(TEXT("%s has a circular interaction area"), Label), Area);
		if (!Area)
		{
			return;
		}
		TestEqual(*FString::Printf(TEXT("%s radius is 300"), Label), Area->GetUnscaledSphereRadius(), 300.0f);
		TestEqual(
			*FString::Printf(TEXT("%s area is query-only"), Label),
			Area->GetCollisionEnabled(),
			ECollisionEnabled::QueryOnly);
		TestEqual(
			*FString::Printf(TEXT("%s area overlaps pawns"), Label),
			Area->GetCollisionResponseToChannel(ECC_Pawn),
			ECollisionResponse::ECR_Overlap);
		TestTrue(*FString::Printf(TEXT("%s area generates overlap events"), Label), Area->GetGenerateOverlapEvents());
	};

	const AGameXXKTownNpcActor* ActorNpc = NewObject<AGameXXKTownNpcActor>();
	VerifyArea(TEXT("actor NPC"), ActorNpc->GetInteractionArea());
	const AGameXXKTownNpcCharacter* CharacterNpc = NewObject<AGameXXKTownNpcCharacter>();
	VerifyArea(TEXT("character NPC"), CharacterNpc->GetInteractionArea());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeMetadataTest,
	"GameXXK.Interaction.Router.NpcNarrativeMetadata",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeMetadataTest::RunTest(const FString& Parameters)
{
	AGameXXKTownNpcCharacter* CharacterNpc = NewObject<AGameXXKTownNpcCharacter>();
	CharacterNpc->SetNpcId(TEXT("Npc.TusiChief"));
	const UGameXXKInteractableComponent* CharacterMetadata =
		CharacterNpc->FindComponentByClass<UGameXXKInteractableComponent>();
	TestNotNull(TEXT("character NPC owns narrative interaction metadata"), CharacterMetadata);
	if (CharacterMetadata)
	{
		TestEqual(TEXT("character NPC interaction id"), CharacterMetadata->GetInteractionId(), FName(TEXT("Npc.TusiChief")));
		TestEqual(
			TEXT("character NPC default sequence id"),
			CharacterMetadata->GetNarrativeSequenceId(),
			FName(TEXT("Sequence.Npc.TusiChief.Default")));
		TestTrue(TEXT("character NPC prompt anchor"), CharacterMetadata->GetPromptAnchor() == CharacterNpc->GetInteractionArea());
	}

	AGameXXKTownNpcActor* ActorNpc = NewObject<AGameXXKTownNpcActor>();
	ActorNpc->SetNpcId(TEXT("Npc.SongJinBao"));
	const UGameXXKInteractableComponent* ActorMetadata =
		ActorNpc->FindComponentByClass<UGameXXKInteractableComponent>();
	TestNotNull(TEXT("actor NPC owns narrative interaction metadata"), ActorMetadata);
	if (ActorMetadata)
	{
		TestEqual(TEXT("actor NPC interaction id"), ActorMetadata->GetInteractionId(), FName(TEXT("Npc.SongJinBao")));
		TestEqual(
			TEXT("actor NPC default sequence id"),
			ActorMetadata->GetNarrativeSequenceId(),
			FName(TEXT("Sequence.Npc.SongJinBao.Default")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeControllerRoutingTest,
	"GameXXK.Interaction.Router.NpcNarrativeControllerRouting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeControllerRoutingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcWaitSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestTrue(TEXT("narrative routing fixture starts in town"), Subsystem->StartGame());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.TusiChief"));

	TestTrue(TEXT("inventory opens for interaction gate fixture"), Controller->OpenFreeInventoryWindow());
	TestFalse(TEXT("open inventory blocks NPC narrative"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestFalse(TEXT("blocked inventory request does not start a sequence"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	Controller->CloseQuestDialog();
	Controller->CloseInventoryWindow();

	TestTrue(TEXT("meta shop opens for interaction gate fixture"), Controller->OpenMetaShopWindow());
	TestFalse(TEXT("open shop blocks NPC narrative"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestFalse(TEXT("blocked shop request does not start a sequence"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	Controller->CloseQuestDialog();
	Controller->CloseMetaShopWindow();

	TestTrue(TEXT("NPC interaction starts its configured narrative sequence"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(TEXT("configured sequence becomes active"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	TestEqual(
		TEXT("configured sequence id is authoritative"),
		Subsystem->GetRuntimeState().NarrativeSequenceSession.SequenceId,
		FName(TEXT("Sequence.Npc.TusiChief.Default")));
	TestFalse(TEXT("new NPC route never opens the legacy quest dialog"), Controller->IsQuestDialogOpenForTest());
	TestFalse(TEXT("a second NPC narrative cannot replace the active sequence"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestNull(
		TEXT("controller no longer exposes NPC recruitment"),
		Controller->GetClass()->FindFunctionByName(TEXT("RecruitPendingTownNpc")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeDialogueInputTest,
	"GameXXK.Interaction.Router.NpcNarrativeDialogueInput",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeDialogueInputTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcDialogueAsset();
	MakeNpcDialogueSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestTrue(TEXT("dialogue fixture starts in town"), Subsystem->StartGame());
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.YueBai"));

	TestTrue(TEXT("NPC dialogue sequence starts"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(TEXT("dialogue session becomes active"), Subsystem->GetRuntimeState().DialogueSession.bActive);
	const FObjectProperty* CoordinatorProperty =
		FindFProperty<FObjectProperty>(Controller->GetClass(), TEXT("DialogueCoordinator"));
	TestNotNull(TEXT("controller owns a dialogue coordinator"), CoordinatorProperty);
	TestTrue(
		TEXT("Space advances the active formal dialogue"),
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Pressed, 1.0f)));
	TestFalse(TEXT("dialogue completion clears its session"), Subsystem->GetRuntimeState().DialogueSession.bActive);
	TestFalse(TEXT("dialogue outcome completes the narrative sequence"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativePossessionBindingTest,
	"GameXXK.Interaction.Router.NpcNarrativePossessionBinding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativePossessionBindingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcWaitSequence();
	UWorld* World = GWorld;
	TestNotNull(TEXT("possession binding fixture has a world"), World);
	if (!World)
	{
		return false;
	}
	const FVector Origin(880000.0f, 880000.0f, 100000.0f);
	AGameXXKMVPPlayerController* Controller = World->SpawnActor<AGameXXKMVPPlayerController>();
	AGameXXKTownPlayerPawn* Pawn = World->SpawnActor<AGameXXKTownPlayerPawn>(Origin, FRotator::ZeroRotator);
	AGameXXKTownNpcCharacter* Npc = World->SpawnActor<AGameXXKTownNpcCharacter>(Origin + FVector(100.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
	TestNotNull(TEXT("possession binding controller"), Controller);
	TestNotNull(TEXT("possession binding pawn"), Pawn);
	TestNotNull(TEXT("possession binding NPC"), Npc);
	if (Controller && Pawn && Npc)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		Subsystem->StartGame();
		Controller->SetMVPSubsystemForTest(Subsystem);
		Controller->Possess(Pawn);
		TestTrue(
			TEXT("possessing a pawn binds its narrative interaction requests"),
			Pawn->GetInteractionComponent()->OnInteractionRequested().IsBound());
		Npc->SetNpcId(TEXT("Npc.TusiChief"));
		Npc->NotifyActorBeginOverlap(Pawn);
		Pawn->Interact();
		TestEqual(
			TEXT("F request reaches the configured sequence"),
			Subsystem->GetRuntimeState().NarrativeSequenceSession.SequenceId,
			FName(TEXT("Sequence.Npc.TusiChief.Default")));
	}
	if (Npc) Npc->Destroy();
	if (Pawn) Pawn->Destroy();
	if (Controller) Controller->Destroy();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeBubbleAnchorTest,
	"GameXXK.Interaction.Router.NpcNarrativeBubbleAnchor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeBubbleAnchorTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcBubbleDialogueAsset();
	MakeNpcBubbleSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.JinGui"));
	TestTrue(TEXT("bubble narrative starts"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(TEXT("bubble dialogue remains active"), Subsystem->GetRuntimeState().DialogueSession.bActive);
	const FObjectProperty* BubbleProperty =
		FindFProperty<FObjectProperty>(Controller->GetClass(), TEXT("DialogueBubbleWidget"));
	UGameXXKSpeechBubbleWidget* BubbleWidget = BubbleProperty
		? Cast<UGameXXKSpeechBubbleWidget>(BubbleProperty->GetObjectPropertyValue_InContainer(Controller))
		: nullptr;
	TestNotNull(TEXT("controller owns bubble presenter"), BubbleWidget);
	TestTrue(TEXT("bubble presenter resolves the selected NPC anchor"), BubbleWidget && BubbleWidget->IsBubbleVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeResumeAfterExitTest,
	"GameXXK.Interaction.Router.NpcNarrativeResumeAfterExit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeResumeAfterExitTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcWaitSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.TusiChief"));
	TestTrue(TEXT("resumable NPC sequence starts"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(
		TEXT("Escape releases the presentation without discarding progress"),
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
	TestTrue(TEXT("paused sequence progress remains active"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	AGameXXKMVPPlayerController* RestoredController = NewObject<AGameXXKMVPPlayerController>();
	RestoredController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("a rebuilt controller resumes the same NPC sequence"), RestoredController->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestEqual(
		TEXT("resume keeps the same sequence id"),
		Subsystem->GetRuntimeState().NarrativeSequenceSession.SequenceId,
		FName(TEXT("Sequence.Npc.TusiChief.Default")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeDialogueResumeAfterControllerRebuildTest,
	"GameXXK.Interaction.Router.NpcNarrativeDialogueResumeAfterControllerRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeDialogueResumeAfterControllerRebuildTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcDialogueAsset();
	MakeNpcDialogueSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.YueBai"));
	AGameXXKMVPPlayerController* FirstController = NewObject<AGameXXKMVPPlayerController>();
	FirstController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("dialogue starts before controller rebuild"), FirstController->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(
		TEXT("Escape pauses dialogue before controller rebuild"),
		FirstController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::Escape, IE_Pressed, 1.0f)));
	AGameXXKMVPPlayerController* RestoredController = NewObject<AGameXXKMVPPlayerController>();
	RestoredController->SetMVPSubsystemForTest(Subsystem);
	TestTrue(TEXT("rebuilt controller restores the paused dialogue"), RestoredController->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(
		TEXT("restored dialogue accepts normal advance input"),
		RestoredController->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::SpaceBar, IE_Pressed, 1.0f)));
	TestFalse(TEXT("restored dialogue completion clears dialogue state"), Subsystem->GetRuntimeState().DialogueSession.bActive);
	TestFalse(TEXT("restored dialogue completion advances and ends its sequence"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeShopCommandTest,
	"GameXXK.Interaction.Router.NpcNarrativeShopCommand",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeShopCommandTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeMerchantChoiceDialogueAsset();
	MakeMerchantChoiceSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.SongJinBao"));
	TestTrue(TEXT("merchant narrative starts"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestTrue(
		TEXT("number key chooses the shop outcome"),
		Controller->InputKey(FInputKeyEventArgs::CreateSimulated(EKeys::One, IE_Pressed, 1.0f)));
	TestTrue(TEXT("shop outcome opens the existing meta shop"), Controller->IsMetaShopOpenForTest());
	TestTrue(TEXT("sequence remains pending while shop is open"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	TestTrue(TEXT("closing the shop completes its pending narrative command"), Controller->CloseMetaShopWindow());
	TestFalse(TEXT("sequence ends after the shop closes"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeImmediateEndInputLockTest,
	"GameXXK.Interaction.Router.NpcNarrativeImmediateEndInputLock",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeImmediateEndInputLockTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeImmediateNpcSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.QiongMeiEr"));
	TestTrue(TEXT("immediate NPC sequence starts and resolves"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestFalse(TEXT("immediate sequence is no longer active"), Subsystem->GetRuntimeState().NarrativeSequenceSession.bActive);
	TestFalse(TEXT("immediate sequence does not leave movement locked"), Controller->IsMoveInputIgnored());
	TestFalse(TEXT("immediate sequence does not leave look locked"), Controller->IsLookInputIgnored());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNpcNarrativeBlocksGameplayUiTest,
	"GameXXK.Interaction.Router.NpcNarrativeBlocksGameplayUi",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNpcNarrativeBlocksGameplayUiTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKInteractionRouterTestPrivate;
	MakeNpcWaitSequence();
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	Subsystem->StartGame();
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Subsystem);
	AGameXXKTownPlayerPawn* Pawn = NewObject<AGameXXKTownPlayerPawn>();
	AGameXXKTownNpcCharacter* Npc = NewObject<AGameXXKTownNpcCharacter>();
	Npc->SetNpcId(TEXT("Npc.TusiChief"));
	TestTrue(TEXT("blocking narrative starts"), Controller->OpenTownNpcInteractionForNpc(Npc, Pawn));
	TestFalse(TEXT("blocking narrative rejects backpack opening"), Controller->OpenFreeInventoryWindow());
	TestFalse(TEXT("blocking narrative rejects direct shop opening"), Controller->OpenMetaShopWindow());
	TestFalse(TEXT("blocking narrative rejects formation opening"), Controller->OpenCompanionRoster());
	TestFalse(TEXT("blocking narrative rejects task-list opening"), Controller->OpenTaskPanel());
	return true;
}

#endif
