#include "Town/GameXXKTownNpcActor.h"

#include "GameFramework/Pawn.h"
#include "PaperFlipbookComponent.h"

AGameXXKTownNpcActor::AGameXXKTownNpcActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Visual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);
}

void AGameXXKTownNpcActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	AActor* Target = FollowTarget.Get();
	if (!bFollowerActive || !Target)
	{
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	const FVector ToTarget = TargetLocation - CurrentLocation;
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget <= FollowDistance || DistanceToTarget <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	const FVector DesiredLocation = TargetLocation - ToTarget.GetSafeNormal() * FollowDistance;
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, DesiredLocation, DeltaSeconds, FollowSpeed);
	SetActorLocation(NewLocation);
}

void AGameXXKTownNpcActor::SetNpcRole(EGameXXKTownNpcRole NewRole)
{
	NpcRole = NewRole;
}

EGameXXKTownNpcRole AGameXXKTownNpcActor::GetNpcRole() const
{
	return NpcRole;
}

bool AGameXXKTownNpcActor::CanOfferQuest() const
{
	return NpcRole == EGameXXKTownNpcRole::Quest;
}

bool AGameXXKTownNpcActor::CanTrade() const
{
	return NpcRole == EGameXXKTownNpcRole::Merchant;
}

void AGameXXKTownNpcActor::ActivateFollower(AActor* Target, float Distance)
{
	FollowTarget = Target;
	FollowDistance = FMath::Max(0.0f, Distance);
	bFollowerActive = Target != nullptr;
}

void AGameXXKTownNpcActor::DismissFollower()
{
	bFollowerActive = false;
	FollowTarget = nullptr;
}

bool AGameXXKTownNpcActor::IsFollowerActive() const
{
	return bFollowerActive;
}

AActor* AGameXXKTownNpcActor::GetFollowTarget() const
{
	return FollowTarget.Get();
}

float AGameXXKTownNpcActor::GetFollowDistance() const
{
	return FollowDistance;
}

FText AGameXXKTownNpcActor::GetInteractionPrompt_Implementation() const
{
	if (CanOfferQuest())
	{
		return NSLOCTEXT("GameXXK", "QuestNpcPrompt", "F");
	}
	if (CanTrade())
	{
		return NSLOCTEXT("GameXXK", "MerchantNpcPrompt", "F");
	}
	return NSLOCTEXT("GameXXK", "GenericNpcPrompt", "F");
}

void AGameXXKTownNpcActor::Interact_Implementation(APawn* InstigatorPawn)
{
	if (CanOfferQuest())
	{
		OnQuestInteract(InstigatorPawn);
		return;
	}
	if (CanTrade())
	{
		OnMerchantInteract(InstigatorPawn);
	}
}
