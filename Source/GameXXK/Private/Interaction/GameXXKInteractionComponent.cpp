#include "Interaction/GameXXKInteractionComponent.h"

#include "Interaction/GameXXKInteractable.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionRules.h"
#include "GameFramework/Pawn.h"
#include "Town/GameXXKTownExitActor.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"

namespace
{
	bool IsAvailableInteractionTarget(const AActor* Actor)
	{
		if (!Actor || Actor->IsPendingKillPending())
		{
			return false;
		}
		const UGameXXKInteractableComponent* Metadata =
			Actor->FindComponentByClass<UGameXXKInteractableComponent>();
		if ((!Metadata || !Metadata->IsInteractionEnabled())
			&& !Actor->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
		{
			return false;
		}

		if (const AGameXXKTownNpcActor* TownNpc = Cast<AGameXXKTownNpcActor>(Actor))
		{
			return !TownNpc->IsFollowerActive();
		}

		if (const AGameXXKTownNpcCharacter* TownNpc = Cast<AGameXXKTownNpcCharacter>(Actor))
		{
			return !TownNpc->IsFollowerActive();
		}

		return true;
	}

	bool BuildCandidate(
		const APawn& OwnerPawn,
		AActor& Actor,
		FGameXXKInteractionCandidate& OutCandidate)
	{
		if (!IsAvailableInteractionTarget(&Actor))
		{
			return false;
		}
		const FVector2D OwnerLocation(OwnerPawn.GetActorLocation());
		const FVector2D TargetLocation(Actor.GetActorLocation());
		const FVector2D ToTarget = TargetLocation - OwnerLocation;
		const float Distance = static_cast<float>(ToTarget.Size());
		const UGameXXKInteractableComponent* Metadata =
			Actor.FindComponentByClass<UGameXXKInteractableComponent>();
		OutCandidate.InteractionId = Metadata && !Metadata->GetInteractionId().IsNone()
			? Metadata->GetInteractionId()
			: FName(*Actor.GetPathName());
		OutCandidate.Priority = Metadata ? Metadata->GetPriority() : 0;
		OutCandidate.Distance = Distance;
		return true;
	}

	AActor* ChooseActor(
		const APawn& OwnerPawn,
		const TArray<AActor*>& Actors)
	{
		TArray<FGameXXKInteractionCandidate> Candidates;
		TMap<FName, AActor*> ActorById;
		for (AActor* Actor : Actors)
		{
			if (!Actor || Actor == &OwnerPawn)
			{
				continue;
			}
			FGameXXKInteractionCandidate Candidate;
			if (BuildCandidate(OwnerPawn, *Actor, Candidate))
			{
				Candidates.Add(Candidate);
				ActorById.FindOrAdd(Candidate.InteractionId, Actor);
			}
		}
		const TOptional<FGameXXKInteractionCandidate> Chosen =
			FGameXXKInteractionRules::Choose(Candidates);
		return Chosen.IsSet() ? ActorById.FindRef(Chosen->InteractionId) : nullptr;
	}
}

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
	RefreshFocusedActorFromStack();
	AActor* Actor = FocusedActor.Get();
	if (!IsAvailableInteractionTarget(Actor))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (UGameXXKInteractableComponent* Metadata =
		Actor->FindComponentByClass<UGameXXKInteractableComponent>())
	{
		if (Metadata->IsInteractionEnabled() && InteractionRequestedDelegate.IsBound())
		{
			InteractionRequestedDelegate.Broadcast(
				Actor,
				Metadata->GetInteractionId(),
				Metadata->GetNarrativeSequenceId());
			return;
		}
	}
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

void UGameXXKInteractionComponent::RefreshFocusedActor()
{
	RefreshFocusedActorFromStack();
}

void UGameXXKInteractionComponent::SetFocusedActor(AActor* Actor)
{
	FocusStack.Reset();
	if (Actor)
	{
		FocusStack.Add(Actor);
	}
	SetFocusedActorInternal(Actor);
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
	RefreshFocusedActorFromStack();
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
	FocusStack.RemoveAll([](const TWeakObjectPtr<AActor>& Candidate)
	{
		return !Candidate.IsValid();
	});
	const APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (!OwnerPawn)
	{
		SetFocusedActorInternal(nullptr);
		return;
	}
	TArray<AActor*> Actors;
	for (const TWeakObjectPtr<AActor>& Candidate : FocusStack)
	{
		if (Candidate.IsValid())
		{
			Actors.Add(Candidate.Get());
		}
	}
	SetFocusedActorInternal(ChooseActor(
		*OwnerPawn,
		Actors));
}

void UGameXXKInteractionComponent::SetFocusedActorInternal(AActor* Actor)
{
	AActor* Previous = FocusedActor.Get();
	if (Previous == Actor)
	{
		return;
	}
	FocusedActor = Actor;
	FName InteractionId = NAME_None;
	if (const UGameXXKInteractableComponent* Metadata =
		Actor ? Actor->FindComponentByClass<UGameXXKInteractableComponent>() : nullptr)
	{
		InteractionId = Metadata->GetInteractionId();
	}
	TargetChangedDelegate.Broadcast(Actor, InteractionId);
}
