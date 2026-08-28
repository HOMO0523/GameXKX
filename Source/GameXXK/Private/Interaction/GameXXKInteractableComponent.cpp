#include "Interaction/GameXXKInteractableComponent.h"

#include "Components/SceneComponent.h"

UGameXXKInteractableComponent::UGameXXKInteractableComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UGameXXKInteractableComponent::Configure(
	const FName InInteractionId,
	FText InDisplayName,
	const FName InNarrativeSequenceId,
	const int32 InPriority,
	USceneComponent* InPromptAnchor)
{
	InteractionId = InInteractionId;
	DisplayName = MoveTemp(InDisplayName);
	NarrativeSequenceId = InNarrativeSequenceId;
	Priority = InPriority;
	PromptAnchor = InPromptAnchor;
	if (InteractionId.IsNone())
	{
		bInteractionEnabled = false;
	}
}

FName UGameXXKInteractableComponent::GetInteractionId() const { return InteractionId; }
FText UGameXXKInteractableComponent::GetDisplayName() const { return DisplayName; }
FName UGameXXKInteractableComponent::GetNarrativeSequenceId() const { return NarrativeSequenceId; }
int32 UGameXXKInteractableComponent::GetPriority() const { return Priority; }
USceneComponent* UGameXXKInteractableComponent::GetPromptAnchor() const { return PromptAnchor; }
bool UGameXXKInteractableComponent::IsInteractionEnabled() const { return bInteractionEnabled; }
void UGameXXKInteractableComponent::SetInteractionEnabled(const bool bEnabled) { bInteractionEnabled = bEnabled; }
