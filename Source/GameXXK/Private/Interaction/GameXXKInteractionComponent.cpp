#include "Interaction/GameXXKInteractionComponent.h"

#include "Interaction/GameXXKInteractable.h"
#include "EngineUtils.h"
#include "GameFramework/Pawn.h"
#include "Town/GameXXKTownExitActor.h"
#include "Town/GameXXKTownNpcActor.h"

UGameXXKInteractionComponent::UGameXXKInteractionComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	InteractionKey = EKeys::F;
}

AActor* UGameXXKInteractionComponent::GetFocusedActor() const
{
	return FocusedActor.Get();
}

FKey UGameXXKInteractionComponent::GetInteractionKey() const
{
	return InteractionKey;
}

void UGameXXKInteractionComponent::Interact()
{
	AActor* Actor = FocusedActor.Get();
	if (!Actor)
	{
		Actor = FindNearbyInteractableActor();
		if (!Actor)
		{
			return;
		}
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (Actor->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
	{
		if (Actor->GetClass() == AGameXXKTownNpcActor::StaticClass())
		{
			CastChecked<AGameXXKTownNpcActor>(Actor)->Interact_Implementation(OwnerPawn);
			return;
		}
		if (Actor->GetClass() == AGameXXKTownExitActor::StaticClass())
		{
			CastChecked<AGameXXKTownExitActor>(Actor)->Interact_Implementation(OwnerPawn);
			return;
		}
		IGameXXKInteractable::Execute_Interact(Actor, OwnerPawn);
		return;
	}

	if (AGameXXKTownNpcActor* TownNpc = Cast<AGameXXKTownNpcActor>(Actor))
	{
		TownNpc->Interact_Implementation(OwnerPawn);
	}
	else if (AGameXXKTownExitActor* TownExit = Cast<AGameXXKTownExitActor>(Actor))
	{
		TownExit->Interact_Implementation(OwnerPawn);
	}
}

AActor* UGameXXKInteractionComponent::FindNearbyInteractableActor() const
{
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	UWorld* World = OwnerPawn ? OwnerPawn->GetWorld() : GetWorld();
	if (!OwnerPawn || !World || ProximityInteractionRadius <= 0.0f)
	{
		return nullptr;
	}

	const FVector OwnerLocation = OwnerPawn->GetActorLocation();
	const float MaxDistanceSquared = FMath::Square(ProximityInteractionRadius);
	float BestDistanceSquared = MaxDistanceSquared;
	AActor* BestActor = nullptr;
	for (TActorIterator<AActor> It(World); It; ++It)
	{
		AActor* Candidate = *It;
		if (!Candidate || Candidate == OwnerPawn || Candidate->IsPendingKillPending())
		{
			continue;
		}
		if (!Candidate->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
		{
			continue;
		}

		const float DistanceSquared = FVector::DistSquared2D(OwnerLocation, Candidate->GetActorLocation());
		if (DistanceSquared <= BestDistanceSquared)
		{
			BestDistanceSquared = DistanceSquared;
			BestActor = Candidate;
		}
	}
	return BestActor;
}

void UGameXXKInteractionComponent::SetFocusedActor(AActor* Actor)
{
	FocusStack.Reset();
	FocusedActor = Actor;
	if (Actor)
	{
		FocusStack.Add(Actor);
	}
}

void UGameXXKInteractionComponent::AddFocusedActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	FocusStack.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid() || Candidate.Get() == Actor;
	});
	FocusStack.Add(Actor);
	FocusedActor = Actor;
}

void UGameXXKInteractionComponent::RemoveFocusedActor(AActor* Actor)
{
	if (!Actor)
	{
		return;
	}

	FocusStack.RemoveAll([Actor](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid() || Candidate.Get() == Actor;
	});
	if (FocusedActor.Get() == Actor)
	{
		RefreshFocusedActorFromStack();
	}
}

void UGameXXKInteractionComponent::SetFocusedActorForTest(AActor* Actor)
{
	SetFocusedActor(Actor);
}

void UGameXXKInteractionComponent::RefreshFocusedActorFromStack()
{
	FocusedActor = nullptr;
	while (FocusStack.Num() > 0)
	{
		TWeakObjectPtr<AActor> Candidate = FocusStack.Last();
		if (Candidate.IsValid())
		{
			FocusedActor = Candidate.Get();
			return;
		}
		FocusStack.Pop(EAllowShrinking::No);
	}
}
