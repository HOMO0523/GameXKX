#include "Interaction/GameXXKInteractionComponent.h"

#include "Interaction/GameXXKInteractable.h"
#include "GameFramework/Pawn.h"
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
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (Actor->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
	{
		if (Actor->GetClass() == AGameXXKTownNpcActor::StaticClass())
		{
			CastChecked<AGameXXKTownNpcActor>(Actor)->Interact_Implementation(OwnerPawn);
			return;
		}
		IGameXXKInteractable::Execute_Interact(Actor, OwnerPawn);
		return;
	}

	if (AGameXXKTownNpcActor* TownNpc = Cast<AGameXXKTownNpcActor>(Actor))
	{
		TownNpc->Interact_Implementation(OwnerPawn);
	}
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
