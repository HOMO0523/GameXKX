#include "Town/GameXXKTownPlayerPawn.h"

#include "Components/InputComponent.h"
#include "Components/SphereComponent.h"
#include "GameFramework/FloatingPawnMovement.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	constexpr float TownMovementAxisDeadZone = 0.20f;

	FSoftObjectPath MakeHeroWalkFlipbookPath(const TCHAR* DirectionName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_%s.FB_Hero_Walk_%s"), DirectionName, DirectionName));
	}

	float NormalizeTownMovementAxis(float Value)
	{
		return FMath::Abs(Value) < TownMovementAxisDeadZone ? 0.0f : FMath::Clamp(Value, -1.0f, 1.0f);
	}

	EGameXXKTownFacingDirection ResolveTownFacingDirection(float Horizontal, float Vertical, EGameXXKTownFacingDirection Fallback)
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

	InitializeTownDirectionFlipbooks();
	DefaultTownFlipbookAsset = TownDirectionFlipbookAssets.FindRef(EGameXXKTownFacingDirection::South);
	ApplyTownFacingFlipbook();
}

void AGameXXKTownPlayerPawn::BeginPlay()
{
	Super::BeginPlay();
	ApplyTownFacingFlipbook();
}

void AGameXXKTownPlayerPawn::UnPossessed()
{
	ResetTownMovementInput();
	Super::UnPossessed();
}

void AGameXXKTownPlayerPawn::Tick(float DeltaSeconds)
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
	PlayerInputComponent->BindAxis(TEXT("TownMoveHorizontal"), this, &AGameXXKTownPlayerPawn::MoveHorizontal);
	PlayerInputComponent->BindAxis(TEXT("TownMoveVertical"), this, &AGameXXKTownPlayerPawn::MoveVertical);
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

bool AGameXXKTownPlayerPawn::HasAssignedTownFlipbook() const
{
	return GetCurrentTownFlipbook() != nullptr;
}

UPaperFlipbook* AGameXXKTownPlayerPawn::GetDefaultTownFlipbook() const
{
	return DefaultTownFlipbookOverride ? DefaultTownFlipbookOverride.Get() : DefaultTownFlipbookAsset.LoadSynchronous();
}

FSoftObjectPath AGameXXKTownPlayerPawn::GetDefaultTownFlipbookPath() const
{
	return DefaultTownFlipbookAsset.ToSoftObjectPath();
}

FString AGameXXKTownPlayerPawn::GetDefaultTownFlipbookPathString() const
{
	return GetDefaultTownFlipbookPath().ToString();
}

UPaperFlipbook* AGameXXKTownPlayerPawn::GetCurrentTownFlipbook() const
{
	return Visual ? Visual->GetFlipbook() : nullptr;
}

EGameXXKTownFacingDirection AGameXXKTownPlayerPawn::GetTownFacingDirection() const
{
	return CurrentTownFacingDirection;
}

FSoftObjectPath AGameXXKTownPlayerPawn::GetTownFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const
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

int32 AGameXXKTownPlayerPawn::CountTownInputBindingsForTest() const
{
	UInputComponent* TestInput = NewObject<UInputComponent>(const_cast<AGameXXKTownPlayerPawn*>(this));
	const_cast<AGameXXKTownPlayerPawn*>(this)->SetupPlayerInputComponent(TestInput);
	return TestInput->AxisBindings.Num() + TestInput->AxisKeyBindings.Num() + TestInput->KeyBindings.Num();
}

void AGameXXKTownPlayerPawn::SetTownDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook)
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

void AGameXXKTownPlayerPawn::SetDefaultTownFlipbookForTest(UPaperFlipbook* InFlipbook)
{
	DefaultTownFlipbookOverride = InFlipbook;
	ApplyDefaultTownFlipbook();
}

void AGameXXKTownPlayerPawn::ApplyDefaultTownFlipbook()
{
	CurrentTownFacingDirection = EGameXXKTownFacingDirection::South;
	UPaperFlipbook* FlipbookToApply = GetDefaultTownFlipbook();
	if (Visual && FlipbookToApply)
	{
		Visual->SetFlipbook(FlipbookToApply);
	}
}

void AGameXXKTownPlayerPawn::ApplyTownFacingFlipbook()
{
	UPaperFlipbook* FlipbookToApply = GetTownFlipbookForDirection(CurrentTownFacingDirection);
	if (Visual && FlipbookToApply)
	{
		Visual->SetFlipbook(FlipbookToApply);
	}
}

UPaperFlipbook* AGameXXKTownPlayerPawn::GetTownFlipbookForDirection(EGameXXKTownFacingDirection Direction) const
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

void AGameXXKTownPlayerPawn::InitializeTownDirectionFlipbooks()
{
	TownDirectionFlipbookAssets.Reset();
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::South, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("South"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("SouthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::West, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("West"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("NorthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::North, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("North"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("NorthEast"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::East, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("East"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroWalkFlipbookPath(TEXT("SouthEast"))));
}

void AGameXXKTownPlayerPawn::UpdateTownFacingFromIntent(float Horizontal, float Vertical)
{
	const EGameXXKTownFacingDirection NewDirection = ResolveTownFacingDirection(Horizontal, Vertical, CurrentTownFacingDirection);
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

void AGameXXKTownPlayerPawn::RefreshTownMovementIntent()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		RightInputPressCount = 0;
		LeftInputPressCount = 0;
		ForwardInputPressCount = 0;
		BackwardInputPressCount = 0;
		AxisHorizontalIntent = 0.0f;
		AxisVerticalIntent = 0.0f;
		HorizontalIntent = 0.0f;
		VerticalIntent = 0.0f;
		UpdateTownFacingFromIntent(0.0f, 0.0f);
		return;
	}

	HorizontalIntent = FMath::Clamp(GetKeyboardHorizontalIntent() + AxisHorizontalIntent, -1.0f, 1.0f);
	VerticalIntent = FMath::Clamp(GetKeyboardVerticalIntent() + AxisVerticalIntent, -1.0f, 1.0f);
	UpdateTownFacingFromIntent(HorizontalIntent, VerticalIntent);
}

bool AGameXXKTownPlayerPawn::IsTownMovementBlockedByModalWindow() const
{
	const AGameXXKMVPPlayerController* MVPPlayerController = Cast<AGameXXKMVPPlayerController>(GetController());
	return MVPPlayerController && MVPPlayerController->IsInventoryWindowModalInputLocked();
}

void AGameXXKTownPlayerPawn::ResetTownMovementInput()
{
	RightInputPressCount = 0;
	LeftInputPressCount = 0;
	ForwardInputPressCount = 0;
	BackwardInputPressCount = 0;
	AxisHorizontalIntent = 0.0f;
	AxisVerticalIntent = 0.0f;
	RefreshTownMovementIntent();
}

float AGameXXKTownPlayerPawn::GetKeyboardHorizontalIntent() const
{
	return (RightInputPressCount > 0 ? 1.0f : 0.0f) - (LeftInputPressCount > 0 ? 1.0f : 0.0f);
}

float AGameXXKTownPlayerPawn::GetKeyboardVerticalIntent() const
{
	return (ForwardInputPressCount > 0 ? 1.0f : 0.0f) - (BackwardInputPressCount > 0 ? 1.0f : 0.0f);
}

void AGameXXKTownPlayerPawn::MoveHorizontal(float Value)
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	AxisHorizontalIntent = NormalizeTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveVertical(float Value)
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	AxisVerticalIntent = NormalizeTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveRightPressed()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	++RightInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveRightReleased()
{
	RightInputPressCount = FMath::Max(0, RightInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveLeftPressed()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	++LeftInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveLeftReleased()
{
	LeftInputPressCount = FMath::Max(0, LeftInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveForwardPressed()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	++ForwardInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveForwardReleased()
{
	ForwardInputPressCount = FMath::Max(0, ForwardInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveBackwardPressed()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	++BackwardInputPressCount;
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::MoveBackwardReleased()
{
	BackwardInputPressCount = FMath::Max(0, BackwardInputPressCount - 1);
	RefreshTownMovementIntent();
}

void AGameXXKTownPlayerPawn::Interact()
{
	if (Interaction)
	{
		Interaction->Interact();
	}
}
