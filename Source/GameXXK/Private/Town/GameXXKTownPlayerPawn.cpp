#include "Town/GameXXKTownPlayerPawn.h"

#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "PaperFlipbookComponent.h"

AGameXXKTownPlayerPawn::AGameXXKTownPlayerPawn()
{
	PrimaryActorTick.bCanEverTick = true;

	USphereComponent* CollisionRoot = CreateDefaultSubobject<USphereComponent>(TEXT("CollisionRoot"));
	CollisionRoot->SetSphereRadius(36.0f);
	CollisionRoot->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	CollisionRoot->SetCollisionObjectType(ECC_Pawn);
	CollisionRoot->SetCollisionResponseToAllChannels(ECR_Ignore);
	CollisionRoot->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	CollisionRoot->SetGenerateOverlapEvents(true);
	SceneRoot = CollisionRoot;
	SetRootComponent(SceneRoot);

	Visual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Visual"));
	Visual->SetupAttachment(SceneRoot);

	FloatingMovement = CreateDefaultSubobject<UFloatingPawnMovement>(TEXT("Movement"));
	Interaction = CreateDefaultSubobject<UGameXXKInteractionComponent>(TEXT("Interaction"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;
}

void AGameXXKTownPlayerPawn::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	MoveHorizontal(HorizontalIntent);
	MoveVertical(VerticalIntent);
}

UPawnMovementComponent* AGameXXKTownPlayerPawn::GetMovementComponent() const
{
	return FloatingMovement;
}

void AGameXXKTownPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &AGameXXKTownPlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &AGameXXKTownPlayerPawn::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &AGameXXKTownPlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Released, this, &AGameXXKTownPlayerPawn::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &AGameXXKTownPlayerPawn::MoveForwardReleased);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveForwardPressed);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Released, this, &AGameXXKTownPlayerPawn::MoveForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &AGameXXKTownPlayerPawn::MoveBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AGameXXKTownPlayerPawn::MoveBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Released, this, &AGameXXKTownPlayerPawn::MoveBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AGameXXKTownPlayerPawn::Interact);
}

UGameXXKInteractionComponent* AGameXXKTownPlayerPawn::GetInteractionComponent() const
{
	return Interaction;
}

UPrimitiveComponent* AGameXXKTownPlayerPawn::GetTownCollisionComponent() const
{
	return Cast<UPrimitiveComponent>(SceneRoot);
}

bool AGameXXKTownPlayerPawn::IsSupportedMovementKey(FKey Key) const
{
	return Key == EKeys::W
		|| Key == EKeys::A
		|| Key == EKeys::S
		|| Key == EKeys::D
		|| Key == EKeys::Up
		|| Key == EKeys::Left
		|| Key == EKeys::Down
		|| Key == EKeys::Right;
}

bool AGameXXKTownPlayerPawn::IsInteractionKey(FKey Key) const
{
	return Interaction && Key == Interaction->GetInteractionKey();
}

bool AGameXXKTownPlayerPawn::HasTownVisual() const
{
	return Visual != nullptr;
}

int32 AGameXXKTownPlayerPawn::CountTownInputBindingsForTest() const
{
	UInputComponent* TestInput = NewObject<UInputComponent>(const_cast<AGameXXKTownPlayerPawn*>(this));
	const_cast<AGameXXKTownPlayerPawn*>(this)->SetupPlayerInputComponent(TestInput);
	return TestInput->AxisKeyBindings.Num() + TestInput->KeyBindings.Num();
}

void AGameXXKTownPlayerPawn::MoveHorizontal(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(FVector::RightVector, Value);
	}
}

void AGameXXKTownPlayerPawn::MoveVertical(float Value)
{
	if (!FMath::IsNearlyZero(Value))
	{
		AddMovementInput(FVector::ForwardVector, Value);
	}
}

void AGameXXKTownPlayerPawn::MoveRightPressed()
{
	HorizontalIntent = 1.0f;
}

void AGameXXKTownPlayerPawn::MoveRightReleased()
{
	if (HorizontalIntent > 0.0f)
	{
		HorizontalIntent = 0.0f;
	}
}

void AGameXXKTownPlayerPawn::MoveLeftPressed()
{
	HorizontalIntent = -1.0f;
}

void AGameXXKTownPlayerPawn::MoveLeftReleased()
{
	if (HorizontalIntent < 0.0f)
	{
		HorizontalIntent = 0.0f;
	}
}

void AGameXXKTownPlayerPawn::MoveForwardPressed()
{
	VerticalIntent = 1.0f;
}

void AGameXXKTownPlayerPawn::MoveForwardReleased()
{
	if (VerticalIntent > 0.0f)
	{
		VerticalIntent = 0.0f;
	}
}

void AGameXXKTownPlayerPawn::MoveBackwardPressed()
{
	VerticalIntent = -1.0f;
}

void AGameXXKTownPlayerPawn::MoveBackwardReleased()
{
	if (VerticalIntent < 0.0f)
	{
		VerticalIntent = 0.0f;
	}
}

void AGameXXKTownPlayerPawn::Interact()
{
	if (Interaction)
	{
		Interaction->Interact();
	}
}
