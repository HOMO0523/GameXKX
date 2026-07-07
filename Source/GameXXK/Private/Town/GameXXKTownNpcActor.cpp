#include "Town/GameXXKTownNpcActor.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"

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

void AGameXXKTownNpcActor::BeginPlay()
{
	Super::BeginPlay();
	SpawnOrRefreshVisualCharacter();
}

void AGameXXKTownNpcActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	DestroyVisualCharacter();
	Super::EndPlay(EndPlayReason);
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
	FVector ToTarget = TargetLocation - CurrentLocation;
	ToTarget.Z = 0.0f;
	const float DistanceToTarget = ToTarget.Size();
	if (DistanceToTarget <= FollowDistance || DistanceToTarget <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	FVector DesiredLocation = TargetLocation - ToTarget.GetSafeNormal() * FollowDistance;
	DesiredLocation.Z = CurrentLocation.Z;
	const FVector NewLocation = FMath::VInterpConstantTo(CurrentLocation, DesiredLocation, DeltaSeconds, FollowSpeed);
	if (!NewLocation.Equals(CurrentLocation))
	{
		SetActorLocation(NewLocation);
	}
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

void AGameXXKTownNpcActor::SetVisualCharacterClass(TSubclassOf<AGameXXKHeroCharacter> NewVisualCharacterClass)
{
	if (VisualCharacterClass == NewVisualCharacterClass)
	{
		return;
	}

	VisualCharacterClass = NewVisualCharacterClass;
	if (HasActorBegunPlay())
	{
		SpawnOrRefreshVisualCharacter();
	}
}

TSubclassOf<AGameXXKHeroCharacter> AGameXXKTownNpcActor::GetVisualCharacterClass() const
{
	return VisualCharacterClass;
}

AGameXXKHeroCharacter* AGameXXKTownNpcActor::GetVisualCharacter() const
{
	return VisualCharacter.Get();
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
	if (bLastInteractionSuccessful && InstigatorPawn)
	{
		if (AGameXXKMVPPlayerController* PlayerController = Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()))
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
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

void AGameXXKTownNpcActor::SpawnOrRefreshVisualCharacter()
{
	if (!VisualCharacterClass)
	{
		DestroyVisualCharacter();
		return;
	}

	if (VisualCharacter && VisualCharacter->GetClass() == VisualCharacterClass.Get())
	{
		ConfigureVisualCharacter(VisualCharacter.Get());
		return;
	}

	DestroyVisualCharacter();

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Owner = this;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKHeroCharacter* SpawnedCharacter = World->SpawnActorDeferred<AGameXXKHeroCharacter>(
		VisualCharacterClass,
		GetActorTransform(),
		this,
		nullptr,
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!SpawnedCharacter)
	{
		return;
	}

	SpawnedCharacter->AutoPossessPlayer = EAutoReceiveInput::Disabled;
	SpawnedCharacter->AutoPossessAI = EAutoPossessAI::Disabled;
	ConfigureVisualCharacter(SpawnedCharacter);
	SpawnedCharacter->FinishSpawning(GetActorTransform());
	SpawnedCharacter->AttachToActor(this, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
	ConfigureVisualCharacter(SpawnedCharacter);
	SpawnedCharacter->ResetTownMovementInput();
	VisualCharacter = SpawnedCharacter;
}

void AGameXXKTownNpcActor::DestroyVisualCharacter()
{
	if (VisualCharacter)
	{
		VisualCharacter->Destroy();
		VisualCharacter = nullptr;
	}
}

void AGameXXKTownNpcActor::ConfigureVisualCharacter(AGameXXKHeroCharacter* Character)
{
	if (!Character)
	{
		return;
	}

	Character->SetActorEnableCollision(false);
	Character->SetActorTickEnabled(false);
	if (UCapsuleComponent* Capsule = Character->GetCapsuleComponent())
	{
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		Capsule->SetGenerateOverlapEvents(false);
	}
	if (UCharacterMovementComponent* Movement = Character->GetCharacterMovement())
	{
		Movement->StopMovementImmediately();
		Movement->DisableMovement();
		Movement->SetComponentTickEnabled(false);
	}
}
