#include "Interaction/GameXXKInteractionComponent.h"

#include "Interaction/GameXXKInteractable.h"
#include "GameFramework/Pawn.h"

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
	if (!Actor || !Actor->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
	{
		return;
	}

	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	IGameXXKInteractable::Execute_Interact(Actor, OwnerPawn);
}

void UGameXXKInteractionComponent::SetFocusedActor(AActor* Actor)
{
	FocusedActor = Actor;
}

void UGameXXKInteractionComponent::SetFocusedActorForTest(AActor* Actor)
{
	SetFocusedActor(Actor);
}
