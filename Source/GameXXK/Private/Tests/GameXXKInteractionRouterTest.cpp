#include "Misc/AutomationTest.h"

#include "Algo/Reverse.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/Actor.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Interaction/GameXXKInteractionRules.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

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

#endif
