#include "Town/GameXXKTownNpcActor.h"

#include "Components/SphereComponent.h"
#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"

AGameXXKTownNpcActor::AGameXXKTownNpcActor()
{
	PrimaryActorTick.bCanEverTick = true;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	Visual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);

	InteractionArea = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(SceneRoot);
	InteractionArea->SetSphereRadius(128.0f);
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionArea->SetGenerateOverlapEvents(true);
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

void AGameXXKTownNpcActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->AddFocusedActor(this);
	}
}

void AGameXXKTownNpcActor::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->RemoveFocusedActor(this);
	}
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

USphereComponent* AGameXXKTownNpcActor::GetInteractionArea() const
{
	return InteractionArea;
}

bool AGameXXKTownNpcActor::WasLastInteractionSuccessful() const
{
	return bLastInteractionSuccessful;
}

void AGameXXKTownNpcActor::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
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
	bLastInteractionSuccessful = ApplyDefaultInteraction(InstigatorPawn);
	if (CanOfferQuest())
	{
		OnQuestInteract(InstigatorPawn);
	}
	else if (CanTrade())
	{
		OnMerchantInteract(InstigatorPawn);
	}
	OnDefaultInteractionResolved(InstigatorPawn, bLastInteractionSuccessful);
}

bool AGameXXKTownNpcActor::ApplyDefaultInteraction(APawn* InstigatorPawn)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(InstigatorPawn);
	if (!Subsystem)
	{
		bLastInteractionSuccessful = false;
		return false;
	}

	if (CanOfferQuest())
	{
		const bool bAccepted = Subsystem->AcceptQuest();
		const bool bSaved = bAccepted && SaveInteractionProgress(Subsystem);
		if (bAccepted && InstigatorPawn)
		{
			ActivateFollower(InstigatorPawn, FollowDistance);
		}
		bLastInteractionSuccessful = bSaved;
		return bSaved;
	}
	if (CanTrade())
	{
		bLastInteractionSuccessful = Subsystem->BuyItem(UGameXXKMVPRules::ItemHealingPowder(), 1)
			&& SaveInteractionProgress(Subsystem);
		return bLastInteractionSuccessful;
	}
	bLastInteractionSuccessful = false;
	return false;
}

UGameXXKMVPSubsystem* AGameXXKTownNpcActor::ResolveMVPSubsystem(APawn* InstigatorPawn) const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	UGameInstance* GameInstance = nullptr;
	if (InstigatorPawn && InstigatorPawn->GetWorld())
	{
		GameInstance = InstigatorPawn->GetWorld()->GetGameInstance();
	}
	if (!GameInstance && GetWorld())
	{
		GameInstance = GetWorld()->GetGameInstance();
	}

	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

bool AGameXXKTownNpcActor::SaveInteractionProgress(UGameXXKMVPSubsystem* Subsystem) const
{
	return Subsystem ? Subsystem->SaveCurrentGame(TEXT(""), 0) : false;
}
