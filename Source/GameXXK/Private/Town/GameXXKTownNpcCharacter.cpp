#include "Town/GameXXKTownNpcCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

AGameXXKTownNpcCharacter::AGameXXKTownNpcCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	InteractionArea = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(RootComponent);
	InteractionArea->SetSphereRadius(128.0f);
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionArea->SetGenerateOverlapEvents(true);

	ConfigureGroundedPlaneConstraint();
}

void AGameXXKTownNpcCharacter::BeginPlay()
{
	ConfigureGroundedPlaneConstraint();
	Super::BeginPlay();
	RaiseRootToGroundedHeightIfNeeded();
}

void AGameXXKTownNpcCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	ConfigureGroundedPlaneConstraint();
	RaiseRootToGroundedHeightIfNeeded();
}

void AGameXXKTownNpcCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RaiseRootToGroundedHeightIfNeeded();

	AActor* Target = FollowTarget.Get();
	if (!bFollowerActive || !Target)
	{
		UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
		return;
	}

	const FVector CurrentLocation = GetActorLocation();
	const FVector TargetLocation = Target->GetActorLocation();
	FVector ToTarget = TargetLocation - CurrentLocation;
	ToTarget.Z = 0.0f;
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget <= FollowDistance || DistanceToTarget <= KINDA_SMALL_NUMBER)
	{
		UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
		return;
	}

	FVector DesiredLocation = TargetLocation - ToTarget.GetSafeNormal() * FollowDistance;
	DesiredLocation.Z = CurrentLocation.Z;
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, DesiredLocation, DeltaSeconds, FollowSpeed);
	FVector MovementDelta = NewLocation - CurrentLocation;
	MovementDelta.Z = 0.0f;
	if (!NewLocation.Equals(CurrentLocation))
	{
		SetActorLocation(NewLocation);
		if (CanOfferQuest())
		{
			RecordQuestNpcMovedLocation(ResolveMVPSubsystem(Cast<APawn>(Target)), NewLocation);
		}
	}
	if (!MovementDelta.IsNearlyZero())
	{
		const FVector MoveDirection = MovementDelta.GetSafeNormal();
		UpdateTownVisualFromMovementIntent(FVector::DotProduct(MoveDirection, FVector::RightVector), FVector::DotProduct(MoveDirection, FVector::ForwardVector));
	}
	else
	{
		UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
	}
}

void AGameXXKTownNpcCharacter::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* InteractionComponent = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (InteractionComponent)
	{
		InteractionComponent->AddFocusedActor(this);
	}
}

void AGameXXKTownNpcCharacter::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* InteractionComponent = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (InteractionComponent)
	{
		InteractionComponent->RemoveFocusedActor(this);
	}
}

void AGameXXKTownNpcCharacter::SetNpcRole(EGameXXKTownNpcRole NewRole)
{
	NpcRole = NewRole;
}

EGameXXKTownNpcRole AGameXXKTownNpcCharacter::GetNpcRole() const
{
	return NpcRole;
}

bool AGameXXKTownNpcCharacter::CanOfferQuest() const
{
	return NpcRole == EGameXXKTownNpcRole::Quest;
}

bool AGameXXKTownNpcCharacter::CanTrade() const
{
	return NpcRole == EGameXXKTownNpcRole::Merchant;
}

void AGameXXKTownNpcCharacter::ActivateFollower(AActor* Target, float Distance)
{
	FollowTarget = Target;
	FollowDistance = FMath::Max(0.0f, Distance);
	bFollowerActive = Target != nullptr;
}

void AGameXXKTownNpcCharacter::DismissFollower()
{
	bFollowerActive = false;
	FollowTarget = nullptr;
	UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
}

bool AGameXXKTownNpcCharacter::IsFollowerActive() const
{
	return bFollowerActive;
}

AActor* AGameXXKTownNpcCharacter::GetFollowTarget() const
{
	return FollowTarget.Get();
}

float AGameXXKTownNpcCharacter::GetFollowDistance() const
{
	return FollowDistance;
}

USphereComponent* AGameXXKTownNpcCharacter::GetInteractionArea() const
{
	return InteractionArea;
}

bool AGameXXKTownNpcCharacter::WasLastInteractionSuccessful() const
{
	return bLastInteractionSuccessful;
}

void AGameXXKTownNpcCharacter::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

FText AGameXXKTownNpcCharacter::GetInteractionPrompt_Implementation() const
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

void AGameXXKTownNpcCharacter::Interact_Implementation(APawn* InstigatorPawn)
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
	if (bLastInteractionSuccessful && InstigatorPawn)
	{
		if (AGameXXKMVPPlayerController* PlayerController = Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()))
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
}

bool AGameXXKTownNpcCharacter::ApplyDefaultInteraction(APawn* InstigatorPawn)
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
		if (bAccepted && InstigatorPawn)
		{
			ActivateFollower(InstigatorPawn, FollowDistance);
		}
		if (bAccepted)
		{
			Subsystem->RecordQuestNpcLocation(GetActorLocation());
		}
		bLastInteractionSuccessful = bAccepted;
		return bAccepted;
	}
	if (CanTrade())
	{
		bLastInteractionSuccessful = Subsystem->OpenTownPanel(EGameXXKTownPanelMode::Trade);
		return bLastInteractionSuccessful;
	}
	bLastInteractionSuccessful = false;
	return false;
}

UGameXXKMVPSubsystem* AGameXXKTownNpcCharacter::ResolveMVPSubsystem(APawn* InstigatorPawn) const
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

void AGameXXKTownNpcCharacter::RecordQuestNpcMovedLocation(UGameXXKMVPSubsystem* Subsystem, const FVector& Location)
{
	if (!Subsystem)
	{
		return;
	}

	Subsystem->RecordQuestNpcLocation(Location);
}

float AGameXXKTownNpcCharacter::GetGroundedRootZ() const
{
	const UCapsuleComponent* Capsule = GetCapsuleComponent();
	return Capsule ? Capsule->GetScaledCapsuleHalfHeight() : 72.0f;
}

void AGameXXKTownNpcCharacter::ConfigureGroundedPlaneConstraint()
{
	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetPlaneConstraintOrigin(FVector(0.0f, 0.0f, GetGroundedRootZ()));
	}
}

void AGameXXKTownNpcCharacter::RaiseRootToGroundedHeightIfNeeded()
{
	const float GroundedRootZ = GetGroundedRootZ();
	FVector Location = GetActorLocation();
	if (Location.Z >= GroundedRootZ - 1.0f)
	{
		return;
	}

	Location.Z = GroundedRootZ;
	SetActorLocation(Location, false, nullptr, ETeleportType::TeleportPhysics);
}
