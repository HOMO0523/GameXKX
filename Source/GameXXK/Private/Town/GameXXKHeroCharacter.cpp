#include "Town/GameXXKHeroCharacter.h"

#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	constexpr float HeroTownMovementAxisDeadZone = 0.20f;

	FSoftObjectPath MakeHeroCharacterWalkFlipbookPath(const TCHAR* DirectionName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_%s.FB_Hero_Walk_%s"), DirectionName, DirectionName));
	}

	float NormalizeHeroTownMovementAxis(float Value)
	{
		return FMath::Abs(Value) < HeroTownMovementAxisDeadZone ? 0.0f : FMath::Clamp(Value, -1.0f, 1.0f);
	}

	EGameXXKTownFacingDirection ResolveHeroTownFacingDirection(float Horizontal, float Vertical, EGameXXKTownFacingDirection Fallback)
	{
		const int32 HorizontalSign = FMath::IsNearlyZero(Horizontal) ? 0 : (Horizontal > 0.0f ? 1 : -1);
		const int32 VerticalSign = FMath::IsNearlyZero(Vertical) ? 0 : (Vertical > 0.0f ? 1 : -1);

		if (HorizontalSign == 0 && VerticalSign == 0)
		{
			return Fallback;
		}

		if (HorizontalSign < 0 && VerticalSign < 0)
		{
			return EGameXXKTownFacingDirection::SouthWest;
		}
		if (HorizontalSign > 0 && VerticalSign < 0)
		{
			return EGameXXKTownFacingDirection::SouthEast;
		}
		if (HorizontalSign < 0 && VerticalSign > 0)
		{
			return EGameXXKTownFacingDirection::NorthWest;
		}
		if (HorizontalSign > 0 && VerticalSign > 0)
		{
			return EGameXXKTownFacingDirection::NorthEast;
		}
		if (HorizontalSign < 0)
		{
			return EGameXXKTownFacingDirection::West;
		}
		if (HorizontalSign > 0)
		{
			return EGameXXKTownFacingDirection::East;
		}
		if (VerticalSign > 0)
		{
			return EGameXXKTownFacingDirection::North;
		}

		return EGameXXKTownFacingDirection::South;
	}
}

AGameXXKHeroCharacter::AGameXXKHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCapsuleRadius(36.0f);
	Capsule->SetCapsuleHalfHeight(72.0f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = 0.0f;
	Movement->DefaultLandMovementMode = MOVE_Flying;
	Movement->DefaultWaterMovementMode = MOVE_Flying;
	Movement->MaxWalkSpeed = 260.0f;
	Movement->MaxFlySpeed = 260.0f;
	Movement->bOrientRotationToMovement = false;
	Movement->SetPlaneConstraintEnabled(true);
	Movement->SetPlaneConstraintNormal(FVector::UpVector);
	Movement->bSnapToPlaneAtStart = true;

	Visual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Capsule);
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Interaction = CreateDefaultSubobject<UGameXXKInteractionComponent>(TEXT("Interaction"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	InitializeTownDirectionFlipbooks();
	DefaultTownFlipbookAsset = TownDirectionFlipbookAssets.FindRef(EGameXXKTownFacingDirection::South);
	ApplyTownFacingFlipbook();
}

void AGameXXKHeroCharacter::BeginPlay()
{
	Super::BeginPlay();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Flying);
	}
	ApplyTownFacingFlipbook();
}

void AGameXXKHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (!FMath::IsNearlyZero(HorizontalIntent))
	{
		AddMovementInput(FVector::RightVector, HorizontalIntent);
	}
	if (!FMath::IsNearlyZero(VerticalIntent))
	{
		AddMovementInput(FVector::ForwardVector, VerticalIntent);
	}
}

void AGameXXKHeroCharacter::UnPossessed()
{
	ResetTownMovementInput();
	Super::UnPossessed();
}

void AGameXXKHeroCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	if (!PlayerInputComponent)
	{
		return;
	}

	PlayerInputComponent->BindKey(EKeys::D, IE_Pressed, this, &AGameXXKHeroCharacter::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::D, IE_Released, this, &AGameXXKHeroCharacter::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Pressed, this, &AGameXXKHeroCharacter::MoveRightPressed);
	PlayerInputComponent->BindKey(EKeys::Right, IE_Released, this, &AGameXXKHeroCharacter::MoveRightReleased);
	PlayerInputComponent->BindKey(EKeys::A, IE_Pressed, this, &AGameXXKHeroCharacter::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::A, IE_Released, this, &AGameXXKHeroCharacter::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Pressed, this, &AGameXXKHeroCharacter::MoveLeftPressed);
	PlayerInputComponent->BindKey(EKeys::Left, IE_Released, this, &AGameXXKHeroCharacter::MoveLeftReleased);
	PlayerInputComponent->BindKey(EKeys::W, IE_Pressed, this, &AGameXXKHeroCharacter::MoveForwardPressed);
	PlayerInputComponent->BindKey(EKeys::W, IE_Released, this, &AGameXXKHeroCharacter::MoveForwardReleased);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Pressed, this, &AGameXXKHeroCharacter::MoveForwardPressed);
	PlayerInputComponent->BindKey(EKeys::Up, IE_Released, this, &AGameXXKHeroCharacter::MoveForwardReleased);
	PlayerInputComponent->BindKey(EKeys::S, IE_Pressed, this, &AGameXXKHeroCharacter::MoveBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::S, IE_Released, this, &AGameXXKHeroCharacter::MoveBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Pressed, this, &AGameXXKHeroCharacter::MoveBackwardPressed);
	PlayerInputComponent->BindKey(EKeys::Down, IE_Released, this, &AGameXXKHeroCharacter::MoveBackwardReleased);
	PlayerInputComponent->BindKey(EKeys::F, IE_Pressed, this, &AGameXXKHeroCharacter::Interact);
	PlayerInputComponent->BindAxis(TEXT("TownMoveHorizontal"), this, &AGameXXKHeroCharacter::MoveHorizontal);
	PlayerInputComponent->BindAxis(TEXT("TownMoveVertical"), this, &AGameXXKHeroCharacter::MoveVertical);
}

UGameXXKInteractionComponent* AGameXXKHeroCharacter::GetInteractionComponent() const
{
	return Interaction;
}

UPrimitiveComponent* AGameXXKHeroCharacter::GetTownCollisionComponent() const
{
	return GetCapsuleComponent();
}

bool AGameXXKHeroCharacter::IsSupportedMovementKey(FKey Key) const
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

bool AGameXXKHeroCharacter::IsInteractionKey(FKey Key) const
{
	return Interaction && Key == Interaction->GetInteractionKey();
}

bool AGameXXKHeroCharacter::HasTownVisual() const
{
	return Visual != nullptr;
}

bool AGameXXKHeroCharacter::HasAssignedTownFlipbook() const
{
	return GetCurrentTownFlipbook() != nullptr;
}

UPaperFlipbook* AGameXXKHeroCharacter::GetDefaultTownFlipbook() const
{
	return DefaultTownFlipbookOverride ? DefaultTownFlipbookOverride.Get() : DefaultTownFlipbookAsset.LoadSynchronous();
}

FSoftObjectPath AGameXXKHeroCharacter::GetDefaultTownFlipbookPath() const
{
	return DefaultTownFlipbookAsset.ToSoftObjectPath();
}

FString AGameXXKHeroCharacter::GetDefaultTownFlipbookPathString() const
{
	return GetDefaultTownFlipbookPath().ToString();
}

UPaperFlipbook* AGameXXKHeroCharacter::GetCurrentTownFlipbook() const
{
	return Visual ? Visual->GetFlipbook() : nullptr;
}

EGameXXKTownFacingDirection AGameXXKHeroCharacter::GetTownFacingDirection() const
{
	return CurrentTownFacingDirection;
}

FSoftObjectPath AGameXXKHeroCharacter::GetTownFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (Direction == EGameXXKTownFacingDirection::South)
	{
		return GetDefaultTownFlipbookPath();
	}

	if (const TSoftObjectPtr<UPaperFlipbook>* DirectionAsset = TownDirectionFlipbookAssets.Find(Direction))
	{
		return DirectionAsset->ToSoftObjectPath();
	}

	return FSoftObjectPath();
}

int32 AGameXXKHeroCharacter::CountTownInputBindingsForTest() const
{
	UInputComponent* TestInput = NewObject<UInputComponent>(const_cast<AGameXXKHeroCharacter*>(this));
	const_cast<AGameXXKHeroCharacter*>(this)->SetupPlayerInputComponent(TestInput);
	return TestInput->AxisBindings.Num() + TestInput->AxisKeyBindings.Num() + TestInput->KeyBindings.Num();
}

void AGameXXKHeroCharacter::SetTownDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook)
{
	if (InFlipbook)
	{
		TownDirectionFlipbookOverrides.Add(Direction, InFlipbook);
	}
	else
	{
		TownDirectionFlipbookOverrides.Remove(Direction);
	}

	if (Direction == CurrentTownFacingDirection)
	{
		ApplyTownFacingFlipbook();
	}
}

void AGameXXKHeroCharacter::SetDefaultTownFlipbookForTest(UPaperFlipbook* InFlipbook)
{
	DefaultTownFlipbookOverride = InFlipbook;
	ApplyDefaultTownFlipbook();
}

void AGameXXKHeroCharacter::ApplyDefaultTownFlipbook()
{
	CurrentTownFacingDirection = EGameXXKTownFacingDirection::South;
	UPaperFlipbook* FlipbookToApply = GetDefaultTownFlipbook();
	if (Visual && FlipbookToApply)
	{
		Visual->SetFlipbook(FlipbookToApply);
	}
}

void AGameXXKHeroCharacter::ApplyTownFacingFlipbook()
{
	UPaperFlipbook* FlipbookToApply = GetTownFlipbookForDirection(CurrentTownFacingDirection);
	if (Visual && FlipbookToApply)
	{
		Visual->SetFlipbook(FlipbookToApply);
	}
}

UPaperFlipbook* AGameXXKHeroCharacter::GetTownFlipbookForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (const TObjectPtr<UPaperFlipbook>* Override = TownDirectionFlipbookOverrides.Find(Direction))
	{
		return Override->Get();
	}

	if (Direction == EGameXXKTownFacingDirection::South)
	{
		return DefaultTownFlipbookOverride ? DefaultTownFlipbookOverride.Get() : DefaultTownFlipbookAsset.LoadSynchronous();
	}

	if (const TSoftObjectPtr<UPaperFlipbook>* DirectionAsset = TownDirectionFlipbookAssets.Find(Direction))
	{
		return DirectionAsset->LoadSynchronous();
	}

	return nullptr;
}

void AGameXXKHeroCharacter::InitializeTownDirectionFlipbooks()
{
	TownDirectionFlipbookAssets.Reset();
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::South, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("South"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("SouthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::West, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("West"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("NorthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::North, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("North"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("NorthEast"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::East, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("East"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("SouthEast"))));
}

void AGameXXKHeroCharacter::UpdateTownFacingFromIntent(float Horizontal, float Vertical)
{
	const EGameXXKTownFacingDirection NewDirection = ResolveHeroTownFacingDirection(Horizontal, Vertical, CurrentTownFacingDirection);
	if (NewDirection != CurrentTownFacingDirection)
	{
		CurrentTownFacingDirection = NewDirection;
		ApplyTownFacingFlipbook();
		return;
	}

	if (!GetCurrentTownFlipbook())
	{
		ApplyTownFacingFlipbook();
	}
}

void AGameXXKHeroCharacter::RefreshTownMovementIntent()
{
	HorizontalIntent = FMath::Clamp(GetKeyboardHorizontalIntent() + AxisHorizontalIntent, -1.0f, 1.0f);
	VerticalIntent = FMath::Clamp(GetKeyboardVerticalIntent() + AxisVerticalIntent, -1.0f, 1.0f);
	UpdateTownFacingFromIntent(HorizontalIntent, VerticalIntent);
}

void AGameXXKHeroCharacter::ResetTownMovementInput()
{
	RightInputPressCount = 0;
	LeftInputPressCount = 0;
	ForwardInputPressCount = 0;
	BackwardInputPressCount = 0;
	AxisHorizontalIntent = 0.0f;
	AxisVerticalIntent = 0.0f;
	RefreshTownMovementIntent();
}

float AGameXXKHeroCharacter::GetKeyboardHorizontalIntent() const
{
	return (RightInputPressCount > 0 ? 1.0f : 0.0f) - (LeftInputPressCount > 0 ? 1.0f : 0.0f);
}

float AGameXXKHeroCharacter::GetKeyboardVerticalIntent() const
{
	return (ForwardInputPressCount > 0 ? 1.0f : 0.0f) - (BackwardInputPressCount > 0 ? 1.0f : 0.0f);
}

void AGameXXKHeroCharacter::MoveHorizontal(float Value)
{
	AxisHorizontalIntent = NormalizeHeroTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveVertical(float Value)
{
	AxisVerticalIntent = NormalizeHeroTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveRightPressed()
{
	++RightInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveRightReleased()
{
	RightInputPressCount = FMath::Max(0, RightInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveLeftPressed()
{
	++LeftInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveLeftReleased()
{
	LeftInputPressCount = FMath::Max(0, LeftInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveForwardPressed()
{
	++ForwardInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveForwardReleased()
{
	ForwardInputPressCount = FMath::Max(0, ForwardInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveBackwardPressed()
{
	++BackwardInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveBackwardReleased()
{
	BackwardInputPressCount = FMath::Max(0, BackwardInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::Interact()
{
	if (Interaction)
	{
		Interaction->Interact();
	}
}
