#include "Interaction/GameXXKInteractableComponent.h"

#include "Components/SceneComponent.h"
#include "Narrative/GameXXKCharacterCatalog.h"
#include "UObject/UObjectGlobals.h"

namespace
{
	const TCHAR* CharacterCatalogObjectPath =
		TEXT("/Game/GameXXK/Narrative/Characters/DA_CharacterCatalog.DA_CharacterCatalog");

	FName DefaultSequenceIdForCharacter(const FName CharacterId)
	{
		return CharacterId.IsNone()
			? NAME_None
			: FName(*FString::Printf(TEXT("Sequence.%s.Default"), *CharacterId.ToString()));
	}
}

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
	bInteractionEnabled = !InteractionId.IsNone() && !NarrativeSequenceId.IsNone();
}

void UGameXXKInteractableComponent::ConfigureForCharacterId(
	const FName CharacterId,
	USceneComponent* InPromptAnchor,
	const int32 InPriority)
{
	FText ResolvedDisplayName = FText::FromName(CharacterId);
	FName ResolvedSequenceId = DefaultSequenceIdForCharacter(CharacterId);
	if (const UGameXXKCharacterCatalog* Catalog =
		LoadObject<UGameXXKCharacterCatalog>(nullptr, CharacterCatalogObjectPath))
	{
		if (const FGameXXKCharacterDefinition* Definition = Catalog->FindCharacter(CharacterId))
		{
			ResolvedDisplayName = Definition->DisplayName;
			ResolvedSequenceId = Definition->DefaultInteractionSequenceId;
		}
	}
	Configure(
		CharacterId,
		MoveTemp(ResolvedDisplayName),
		ResolvedSequenceId,
		InPriority,
		InPromptAnchor);
}

FName UGameXXKInteractableComponent::GetInteractionId() const { return InteractionId; }
FText UGameXXKInteractableComponent::GetDisplayName() const { return DisplayName; }
FName UGameXXKInteractableComponent::GetNarrativeSequenceId() const { return NarrativeSequenceId; }
int32 UGameXXKInteractableComponent::GetPriority() const { return Priority; }
USceneComponent* UGameXXKInteractableComponent::GetPromptAnchor() const { return PromptAnchor; }
bool UGameXXKInteractableComponent::IsInteractionEnabled() const { return bInteractionEnabled; }
void UGameXXKInteractableComponent::SetInteractionEnabled(const bool bEnabled)
{
	bInteractionEnabled = bEnabled && !InteractionId.IsNone() && !NarrativeSequenceId.IsNone();
}
