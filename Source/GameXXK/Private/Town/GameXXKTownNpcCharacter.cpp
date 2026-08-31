#include "Town/GameXXKTownNpcCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/SphereComponent.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"
#include "Engine/GameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	FString TownNpcAssetStem(const FName NpcId)
	{
		if (NpcId == TEXT("Npc.TusiChief")) return TEXT("TusiChief");
		if (NpcId == TEXT("Npc.SongJinBao")) return TEXT("SongJinBao");
		if (NpcId == TEXT("Npc.YueBai")) return TEXT("YueBai");
		if (NpcId == TEXT("Npc.ZhouGuangZu")) return TEXT("ZhouGuangZu");
		if (NpcId == TEXT("Npc.JinGui")) return TEXT("JinGui");
		if (NpcId == TEXT("Npc.QiongMeiEr")) return TEXT("QiongMeiEr");
		return FString();
	}
}

AGameXXKTownNpcCharacter::AGameXXKTownNpcCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	AutoPossessPlayer = EAutoReceiveInput::Disabled;
	AutoPossessAI = EAutoPossessAI::Disabled;

	InteractionArea = CreateDefaultSubobject<USphereComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(RootComponent);
	InteractionArea->SetSphereRadius(300.0f);
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionArea->SetGenerateOverlapEvents(true);
	NarrativeInteraction = CreateDefaultSubobject<UGameXXKInteractableComponent>(TEXT("NarrativeInteraction"));

	ConfigureGroundedPlaneConstraint();
}

void AGameXXKTownNpcCharacter::BeginPlay()
{
	ConfigureGroundedPlaneConstraint();
	ConfigureStaticIdleVisual();
	Super::BeginPlay();
	RefreshNarrativeInteractionMetadata();
	RaiseRootToGroundedHeightIfNeeded();
}

void AGameXXKTownNpcCharacter::PostInitializeComponents()
{
	Super::PostInitializeComponents();
	RefreshNarrativeInteractionMetadata();
	ConfigureGroundedPlaneConstraint();
	RaiseRootToGroundedHeightIfNeeded();
}

void AGameXXKTownNpcCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	RaiseRootToGroundedHeightIfNeeded();

	AActor* Target = FollowTarget.Get();
	if (bNarrativeFollowerActive)
	{
		if (!Target)
		{
			DismissNarrativeFollower();
			UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
			return;
		}
		const FVector CurrentLocation = GetActorLocation();
		const FVector TargetLocation = Target->GetActorLocation();
		FVector ToTarget = TargetLocation - CurrentLocation;
		ToTarget.Z = 0.0f;
		const float DistanceToTarget = ToTarget.Size();
		const bool bNeedsChase = DistanceToTarget > NarrativeFollowMaximumDistance;
		const bool bNeedsRetreat = DistanceToTarget < NarrativeFollowMinimumDistance;
		if (bNeedsChase || bNeedsRetreat)
		{
			const FVector Direction = ToTarget.IsNearlyZero()
				? FVector::ForwardVector
				: ToTarget.GetSafeNormal();
			FVector DesiredLocation = TargetLocation - Direction * FollowDistance;
			DesiredLocation.Z = CurrentLocation.Z;
			const FVector NewLocation = DistanceToTarget > 1600.0f
				? DesiredLocation
				: FMath::VInterpConstantTo(
					CurrentLocation,
					DesiredLocation,
					DeltaSeconds,
					FollowSpeed);
			SetActorLocation(NewLocation);
		}
		// YueBai glides while keeping her authored hover-idle presentation.
		UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
		return;
	}
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
		UpdateTownVisualFromMovementIntent(
			FVector::DotProduct(MoveDirection, FVector::RightVector),
			FVector::DotProduct(MoveDirection, FVector::ForwardVector));
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
	if (NpcId == TEXT("Npc.TusiChief"))
	{
		NpcRole = EGameXXKTownNpcRole::Quest;
		RefreshNarrativeInteractionMetadata();
		return;
	}
	if (NpcId == TEXT("Npc.SongJinBao"))
	{
		NpcRole = EGameXXKTownNpcRole::Merchant;
		RefreshNarrativeInteractionMetadata();
		return;
	}
	NpcRole = NewRole;
	if (NpcId.IsNone())
	{
		if (NpcRole == EGameXXKTownNpcRole::Quest) NpcId = TEXT("Npc.TusiChief");
		else if (NpcRole == EGameXXKTownNpcRole::Merchant) NpcId = TEXT("Npc.SongJinBao");
	}
	RefreshNarrativeInteractionMetadata();
}

void AGameXXKTownNpcCharacter::SetNpcId(const FName NewNpcId)
{
	NpcId = NewNpcId;
	NpcRole = NpcId == TEXT("Npc.TusiChief")
		? EGameXXKTownNpcRole::Quest
		: NpcId == TEXT("Npc.SongJinBao")
			? EGameXXKTownNpcRole::Merchant
			: EGameXXKTownNpcRole::Generic;
	RefreshNarrativeInteractionMetadata();
	ConfigureStaticIdleVisual();
}

void AGameXXKTownNpcCharacter::RefreshNarrativeInteractionMetadata()
{
	if (NarrativeInteraction)
	{
		NarrativeInteraction->ConfigureForCharacterId(NpcId, InteractionArea);
	}
}

void AGameXXKTownNpcCharacter::ConfigureStaticIdleVisual()
{
	const FString AssetStem = TownNpcAssetStem(NpcId);
	if (AssetStem.IsEmpty())
	{
		return;
	}

	const FString AssetPath = FString::Printf(
		TEXT("/Game/GameXXK/Characters/PartyDeckNPC/%s/Flipbooks/FB_PartyDeckNPC_%s_Idle_South.FB_PartyDeckNPC_%s_Idle_South"),
		*AssetStem,
		*AssetStem,
		*AssetStem);
	DefaultTownFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(AssetPath));
	TownDirectionFlipbookAssets.Reset();
	TownIdleDirectionFlipbookAssets.Reset();
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::South, DefaultTownFlipbookAsset);
	CurrentTownFacingDirection = EGameXXKTownFacingDirection::South;

	if (Visual)
	{
		Visual->SetFlipbook(DefaultTownFlipbookAsset.LoadSynchronous());
	}
}

FName AGameXXKTownNpcCharacter::GetNpcId() const
{
	return NpcId;
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

bool AGameXXKTownNpcCharacter::HasPrimaryInteractionAction() const
{
	return CanOfferQuest() || CanTrade();
}

bool AGameXXKTownNpcCharacter::CanJoinParty() const
{
	return FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcId) != nullptr;
}

void AGameXXKTownNpcCharacter::ActivateFollower(AActor* Target, float Distance)
{
	if (bNarrativeFollowerActive)
	{
		DismissNarrativeFollower();
	}
	FollowTarget = Target;
	FollowDistance = FMath::Max(0.0f, Distance);
	bFollowerActive = Target != nullptr;
	if (bFollowerActive && CanOfferQuest())
	{
		if (UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(Cast<APawn>(Target)))
		{
			Subsystem->RecordQuestNpcLocation(GetActorLocation());
		}
	}
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

void AGameXXKTownNpcCharacter::ActivateNarrativeFollower(
	AActor* Target,
	const float MinimumDistance,
	const float TargetDistance,
	const float MaximumDistance)
{
	DismissNarrativeFollower();
	if (!Target)
	{
		return;
	}
	NarrativeFollowMinimumDistance = FMath::Max(0.0f, MinimumDistance);
	NarrativeFollowMaximumDistance = FMath::Max(
		NarrativeFollowMinimumDistance,
		MaximumDistance);
	FollowDistance = FMath::Clamp(
		TargetDistance,
		NarrativeFollowMinimumDistance,
		NarrativeFollowMaximumDistance);
	FollowTarget = Target;
	bFollowerActive = false;
	bNarrativeFollowerActive = true;
	if (UCapsuleComponent* Capsule = GetCapsuleComponent())
	{
		NarrativePreviousCapsuleCollision = Capsule->GetCollisionEnabled();
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	if (InteractionArea)
	{
		NarrativePreviousInteractionCollision = InteractionArea->GetCollisionEnabled();
		InteractionArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}
	bNarrativeCollisionSnapshotValid = true;
	UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
}

void AGameXXKTownNpcCharacter::DismissNarrativeFollower()
{
	if (bNarrativeCollisionSnapshotValid)
	{
		if (UCapsuleComponent* Capsule = GetCapsuleComponent())
		{
			Capsule->SetCollisionEnabled(NarrativePreviousCapsuleCollision);
		}
		if (InteractionArea)
		{
			InteractionArea->SetCollisionEnabled(NarrativePreviousInteractionCollision);
		}
	}
	bNarrativeCollisionSnapshotValid = false;
	bNarrativeFollowerActive = false;
	if (!bFollowerActive)
	{
		FollowTarget = nullptr;
	}
	UpdateTownVisualFromMovementIntent(0.0f, 0.0f);
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
	if (AGameXXKMVPPlayerController* PlayerController = InstigatorPawn ? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()) : nullptr)
	{
		bLastInteractionSuccessful = PlayerController->OpenTownNpcInteractionForNpc(this, InstigatorPawn);
		return;
	}

	// Headless compatibility path. A real player controller always opens the
	// contextual choice menu first.
	bLastInteractionSuccessful = ApplyDefaultInteraction(InstigatorPawn);
	if (CanTrade())
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

bool AGameXXKTownNpcCharacter::ConfirmQuestDialogInteraction(APawn* InstigatorPawn)
{
	if (!CanOfferQuest())
	{
		bLastInteractionSuccessful = false;
		return false;
	}

	bLastInteractionSuccessful = ApplyDefaultInteraction(InstigatorPawn);
	OnQuestInteract(InstigatorPawn);
	OnDefaultInteractionResolved(InstigatorPawn, bLastInteractionSuccessful);
	if (bLastInteractionSuccessful && InstigatorPawn)
	{
		if (AGameXXKMVPPlayerController* PlayerController = Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()))
		{
			PlayerController->RefreshPlayerFlowWidgetsFromState();
		}
	}
	return bLastInteractionSuccessful;
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
		if (bAccepted)
		{
			// Accepting the quest keeps the guide NPC at its town spot. Any later
			// story-follow behavior is narrative-owned, never an F-interaction party edit.
			Subsystem->RecordQuestNpcLocation(GetActorLocation());
		}
		bLastInteractionSuccessful = bAccepted;
		return bAccepted;
	}
	if (CanTrade())
	{
		if (InstigatorPawn)
		{
			if (AGameXXKMVPPlayerController* PlayerController = Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()))
			{
				bLastInteractionSuccessful = PlayerController->OpenMetaShopWindow();
				return bLastInteractionSuccessful;
			}
		}
		// The retired subsystem trade panel is intentionally not a fallback. A
		// player controller is required because it owns the new modal shop widget.
		bLastInteractionSuccessful = false;
		return false;
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
