#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKPlayerOcclusionRevealComponent.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "HAL/PlatformTime.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Materials/MaterialInterface.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

namespace
{
	constexpr float HeroTownMovementAxisDeadZone = 0.20f;
	constexpr double HeroTownDiagonalReleaseGraceSeconds = 0.04;
	const TCHAR* PlayerOcclusionRevealMaterialPath = TEXT("/Game/GameXXK/Materials/Player/M_PlayerOcclusionReveal.M_PlayerOcclusionReveal");
	const TCHAR* HeroTownHorizontalIdlePath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Idle_Left.FB_Hero_Town_Idle_Left");
	const TCHAR* HeroTownHorizontalWalkStartPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStart_Left.FB_Hero_Town_WalkStart_Left");
	const TCHAR* HeroTownHorizontalWalkLoopPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkLoop_Left.FB_Hero_Town_WalkLoop_Left");
	const TCHAR* HeroTownHorizontalWalkStopPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStop_Left.FB_Hero_Town_WalkStop_Left");
	const TCHAR* HeroTownHorizontalDeepBreathPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_DeepBreath_Left.FB_Hero_Town_DeepBreath_Left");
	const TCHAR* HeroTownHorizontalAdjustBackpackPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_AdjustBackpack_Left.FB_Hero_Town_AdjustBackpack_Left");
	const TCHAR* HeroTownHorizontalCollectItemPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CollectItem_Left.FB_Hero_Town_CollectItem_Left");
	const TCHAR* HeroTownHorizontalCombatIdlePath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CombatIdle_Left.FB_Hero_Town_CombatIdle_Left");
	const TCHAR* HeroTownHorizontalPunchPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Punch_Left.FB_Hero_Town_Punch_Left");
	const TCHAR* HeroTownHorizontalKickPath = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Kick_Left.FB_Hero_Town_Kick_Left");

	FSoftObjectPath MakeHeroCharacterWalkFlipbookPath(const TCHAR* DirectionName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_%s.FB_Hero_Walk_%s"), DirectionName, DirectionName));
	}

	FSoftObjectPath MakeHeroCharacterIdleFlipbookPath(const TCHAR* DirectionName)
	{
		return FSoftObjectPath(FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_%s.FB_Hero_Idle_%s"), DirectionName, DirectionName));
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

	bool IsHeroTownDiagonalDirection(EGameXXKTownFacingDirection Direction)
	{
		return Direction == EGameXXKTownFacingDirection::SouthWest
			|| Direction == EGameXXKTownFacingDirection::NorthWest
			|| Direction == EGameXXKTownFacingDirection::NorthEast
			|| Direction == EGameXXKTownFacingDirection::SouthEast;
	}

	// This grace represents elapsed time between input events.  It must remain
	// independent of world simulation because editor/MCP input can arrive while
	// PIE world time is paused between synchronous requests.
	double GetHeroTownInputTimeSeconds()
	{
		return FPlatformTime::Seconds();
	}

	bool IsLoopingTownAction(const EGameXXKHeroTownAction Action)
	{
		return Action == EGameXXKHeroTownAction::Idle
			|| Action == EGameXXKHeroTownAction::WalkLoop
			|| Action == EGameXXKHeroTownAction::CombatIdle;
	}
}

AGameXXKHeroCharacter::AGameXXKHeroCharacter()
{
	PrimaryActorTick.bCanEverTick = true;

	UCapsuleComponent* Capsule = GetCapsuleComponent();
	Capsule->SetCapsuleRadius(36.0f);
	Capsule->SetCapsuleHalfHeight(72.0f);
	Capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	Capsule->SetCollisionObjectType(ECC_Pawn);
	Capsule->SetCollisionResponseToAllChannels(ECR_Ignore);
	Capsule->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Capsule->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Overlap);
	Capsule->SetGenerateOverlapEvents(true);

	UCharacterMovementComponent* Movement = GetCharacterMovement();
	Movement->GravityScale = 1.0f;
	Movement->DefaultLandMovementMode = MOVE_Walking;
	Movement->MaxWalkSpeed = 260.0f;
	Movement->MaxStepHeight = 45.0f;
	Movement->SetWalkableFloorAngle(50.0f);
	Movement->bOrientRotationToMovement = false;
	Movement->SetPlaneConstraintEnabled(false);
	Movement->bSnapToPlaneAtStart = false;

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 800.0f;
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;

	TopDownCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("TopDownCamera"));
	TopDownCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	TopDownCameraComponent->bUsePawnControlRotation = false;

	Visual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("Visual"));
	Visual->SetupAttachment(Capsule);
	Visual->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
	Visual->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	Visual->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	Visual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Visual->SetCastShadow(false);
	Visual->SetTranslucentSortPriority(10);
	Visual->SetRenderCustomDepth(true);
	Visual->SetCustomDepthStencilValue(13);

	OcclusionRevealVisual = CreateDefaultSubobject<UPaperFlipbookComponent>(TEXT("OcclusionRevealVisual"));
	OcclusionRevealVisual->SetupAttachment(Capsule);
	OcclusionRevealVisual->SetRelativeLocation(FVector(0.0f, 0.0f, -80.0f));
	OcclusionRevealVisual->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	OcclusionRevealVisual->SetRelativeScale3D(FVector(1.0f, 1.0f, 1.0f));
	OcclusionRevealVisual->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	OcclusionRevealVisual->SetCastShadow(false);
	OcclusionRevealVisual->SetTranslucentSortPriority(11);
	OcclusionRevealVisual->SetVisibility(false);
	OcclusionRevealVisual->SetHiddenInGame(true);

	OcclusionReveal = CreateDefaultSubobject<UGameXXKPlayerOcclusionRevealComponent>(TEXT("OcclusionReveal"));
	OcclusionReveal->BindRevealVisual(OcclusionRevealVisual);

	Interaction = CreateDefaultSubobject<UGameXXKInteractionComponent>(TEXT("Interaction"));

	AutoPossessPlayer = EAutoReceiveInput::Player0;

	InitializeTownDirectionFlipbooks();
	DefaultTownFlipbookAsset = TownHorizontalIdleFlipbookAsset;
	CurrentTownFacingDirection = EGameXXKTownFacingDirection::West;
	ApplyTownFacingFlipbook();
}

void AGameXXKHeroCharacter::BeginPlay()
{
	Super::BeginPlay();
	InitializeOcclusionRevealMaterial();

	if (UCharacterMovementComponent* Movement = GetCharacterMovement())
	{
		Movement->SetMovementMode(MOVE_Walking);
	}
	ApplyTownFacingFlipbook();
}

FString AGameXXKHeroCharacter::GetOcclusionRevealMaterialPathString() const
{
	return PlayerOcclusionRevealMaterialPath;
}

void AGameXXKHeroCharacter::InitializeOcclusionRevealMaterialForTest()
{
	InitializeOcclusionRevealMaterial();
}

void AGameXXKHeroCharacter::InitializeOcclusionRevealMaterial()
{
	if (!OcclusionRevealVisual)
	{
		return;
	}

	UMaterialInterface* RevealMaterial = LoadObject<UMaterialInterface>(nullptr, PlayerOcclusionRevealMaterialPath, nullptr, LOAD_NoWarn);
	if (!RevealMaterial)
	{
		OcclusionRevealVisual->SetVisibility(false);
		UE_LOG(LogTemp, Warning, TEXT("GameXXK player occlusion reveal material is unavailable: %s"), PlayerOcclusionRevealMaterialPath);
		return;
	}

	OcclusionRevealVisual->SetMaterial(0, RevealMaterial);
}

void AGameXXKHeroCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	AdvanceHorizontalTownLocomotion();
	TickHorizontalTownAmbient(DeltaSeconds);
	SynchronizeOcclusionRevealVisual();

	const FVector MoveDirection = (FVector::RightVector * HorizontalIntent) + (FVector::ForwardVector * VerticalIntent);
	if (!MoveDirection.IsNearlyZero())
	{
		AddMovementInput(MoveDirection.GetClampedToMaxSize(1.0f));
		if (bHasPendingStopDiagonalFacingDirection
			&& !IsHeroTownDiagonalDirection(CurrentTownFacingDirection)
			&& GetHeroTownInputTimeSeconds() - PendingStopDiagonalReleaseTimeSeconds > HeroTownDiagonalReleaseGraceSeconds)
		{
			bHasPendingStopDiagonalFacingDirection = false;
		}
	}
}

void AGameXXKHeroCharacter::SynchronizeOcclusionRevealVisualForTest()
{
	SynchronizeOcclusionRevealVisual();
}

void AGameXXKHeroCharacter::SynchronizeOcclusionRevealVisual()
{
	if (!Visual || !OcclusionRevealVisual || !OcclusionRevealVisual->IsVisible())
	{
		return;
	}

	OcclusionRevealVisual->SetFlipbook(Visual->GetFlipbook());
	OcclusionRevealVisual->SetPlaybackPositionInFrames(Visual->GetPlaybackPositionInFrames(), false);
	OcclusionRevealVisual->SetSpriteColor(Visual->GetSpriteColor());
	OcclusionRevealVisual->SetRelativeTransform(Visual->GetRelativeTransform());
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
	if (DefaultTownFlipbookOverride)
	{
		return DefaultTownFlipbookOverride.Get();
	}
	if (bUseHorizontalHeroLocomotion)
	{
		return GetHorizontalTownIdleFlipbook();
	}
	return DefaultTownFlipbookAsset.LoadSynchronous();
}

FSoftObjectPath AGameXXKHeroCharacter::GetDefaultTownFlipbookPath() const
{
	if (bUseHorizontalHeroLocomotion)
	{
		return TownHorizontalIdleFlipbookAsset.ToSoftObjectPath();
	}
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

FVector AGameXXKHeroCharacter::GetTownMovementIntentVector() const
{
	return ((FVector::RightVector * HorizontalIntent) + (FVector::ForwardVector * VerticalIntent)).GetClampedToMaxSize(1.0f);
}

FSoftObjectPath AGameXXKHeroCharacter::GetTownFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const
{
	return GetTownWalkFlipbookPathForDirection(Direction);
}

FSoftObjectPath AGameXXKHeroCharacter::GetTownIdleFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (bUseHorizontalHeroLocomotion)
	{
		return TownHorizontalIdleFlipbookAsset.ToSoftObjectPath();
	}
	if (Direction == EGameXXKTownFacingDirection::South)
	{
		return GetDefaultTownFlipbookPath();
	}

	if (const TSoftObjectPtr<UPaperFlipbook>* DirectionAsset = TownIdleDirectionFlipbookAssets.Find(Direction))
	{
		return DirectionAsset->ToSoftObjectPath();
	}

	return FSoftObjectPath();
}

FSoftObjectPath AGameXXKHeroCharacter::GetTownWalkFlipbookPathForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (bUseHorizontalHeroLocomotion)
	{
		return TownHorizontalWalkLoopFlipbookAsset.ToSoftObjectPath();
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
	SetTownWalkDirectionFlipbookForTest(Direction, InFlipbook);
}

void AGameXXKHeroCharacter::SetTownWalkDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook)
{
	if (InFlipbook)
	{
		TownDirectionFlipbookOverrides.Add(Direction, InFlipbook);
	}
	else
	{
		TownDirectionFlipbookOverrides.Remove(Direction);
	}

	if (bTownMoving && Direction == CurrentTownFacingDirection)
	{
		ApplyTownFacingFlipbook();
	}
}

void AGameXXKHeroCharacter::SetTownIdleDirectionFlipbookForTest(EGameXXKTownFacingDirection Direction, UPaperFlipbook* InFlipbook)
{
	if (InFlipbook)
	{
		TownIdleDirectionFlipbookOverrides.Add(Direction, InFlipbook);
	}
	else
	{
		TownIdleDirectionFlipbookOverrides.Remove(Direction);
	}

	if (!bTownMoving && Direction == CurrentTownFacingDirection)
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
	CurrentTownFacingDirection = bUseHorizontalHeroLocomotion
		? EGameXXKTownFacingDirection::West
		: EGameXXKTownFacingDirection::South;
	bTownMoving = false;
	CurrentTownAction = EGameXXKHeroTownAction::Idle;
	TownAmbientElapsedSeconds = 0.0f;
	TownAmbientDelaySeconds = TownAmbientRandom.FRandRange(8.0f, 14.0f);
	UPaperFlipbook* FlipbookToApply = GetDefaultTownFlipbook();
	if (Visual && FlipbookToApply)
	{
		Visual->SetLooping(true);
		if (Visual->GetFlipbook() != FlipbookToApply)
		{
			Visual->SetFlipbook(FlipbookToApply);
			Visual->PlayFromStart();
		}
		ApplyHorizontalTownFacingMirror();
	}
}

void AGameXXKHeroCharacter::ApplyTownFacingFlipbook()
{
	if (bUseHorizontalHeroLocomotion)
	{
		UPaperFlipbook* FlipbookToApply = GetHorizontalTownActionFlipbook(CurrentTownAction);
		const bool bLooping = IsLoopingTownAction(CurrentTownAction);

		if (Visual && FlipbookToApply)
		{
			const bool bClipChanged = Visual->GetFlipbook() != FlipbookToApply;
			Visual->SetLooping(bLooping);
			if (bClipChanged)
			{
				Visual->SetFlipbook(FlipbookToApply);
				Visual->PlayFromStart();
			}
			else if (bLooping && !Visual->IsPlaying())
			{
				Visual->Play();
			}
		}
		ApplyHorizontalTownFacingMirror();
		return;
	}

	UPaperFlipbook* FlipbookToApply = bTownMoving
		? GetTownWalkFlipbookForDirection(CurrentTownFacingDirection)
		: GetTownIdleFlipbookForDirection(CurrentTownFacingDirection);
	if (Visual && FlipbookToApply)
	{
		Visual->SetFlipbook(FlipbookToApply);
	}
}

UPaperFlipbook* AGameXXKHeroCharacter::GetTownFlipbookForDirection(EGameXXKTownFacingDirection Direction) const
{
	return GetTownWalkFlipbookForDirection(Direction);
}

UPaperFlipbook* AGameXXKHeroCharacter::GetTownIdleFlipbookForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (bUseHorizontalHeroLocomotion)
	{
		return GetHorizontalTownIdleFlipbook();
	}
	if (const TObjectPtr<UPaperFlipbook>* Override = TownIdleDirectionFlipbookOverrides.Find(Direction))
	{
		return Override->Get();
	}

	if (Direction == EGameXXKTownFacingDirection::South)
	{
		return DefaultTownFlipbookOverride ? DefaultTownFlipbookOverride.Get() : DefaultTownFlipbookAsset.LoadSynchronous();
	}

	if (const TSoftObjectPtr<UPaperFlipbook>* DirectionAsset = TownIdleDirectionFlipbookAssets.Find(Direction))
	{
		return DirectionAsset->LoadSynchronous();
	}

	return nullptr;
}

UPaperFlipbook* AGameXXKHeroCharacter::GetTownWalkFlipbookForDirection(EGameXXKTownFacingDirection Direction) const
{
	if (bUseHorizontalHeroLocomotion)
	{
		return GetHorizontalTownWalkLoopFlipbook();
	}
	if (const TObjectPtr<UPaperFlipbook>* Override = TownDirectionFlipbookOverrides.Find(Direction))
	{
		return Override->Get();
	}

	if (const TSoftObjectPtr<UPaperFlipbook>* DirectionAsset = TownDirectionFlipbookAssets.Find(Direction))
	{
		return DirectionAsset->LoadSynchronous();
	}

	return nullptr;
}

UPaperFlipbook* AGameXXKHeroCharacter::GetHorizontalTownIdleFlipbook() const
{
	return DefaultTownFlipbookOverride
		? DefaultTownFlipbookOverride.Get()
		: TownHorizontalIdleFlipbookAsset.LoadSynchronous();
}

UPaperFlipbook* AGameXXKHeroCharacter::GetHorizontalTownWalkStartFlipbook() const
{
	return TownHorizontalWalkStartFlipbookAsset.LoadSynchronous();
}

UPaperFlipbook* AGameXXKHeroCharacter::GetHorizontalTownWalkLoopFlipbook() const
{
	return TownHorizontalWalkLoopFlipbookAsset.LoadSynchronous();
}

UPaperFlipbook* AGameXXKHeroCharacter::GetHorizontalTownActionFlipbook(
	const EGameXXKHeroTownAction Action) const
{
	switch (Action)
	{
	case EGameXXKHeroTownAction::WalkStart:
		return GetHorizontalTownWalkStartFlipbook();
	case EGameXXKHeroTownAction::WalkLoop:
		return GetHorizontalTownWalkLoopFlipbook();
	case EGameXXKHeroTownAction::WalkStop:
		return TownHorizontalWalkStopFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::DeepBreath:
		return TownHorizontalDeepBreathFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::AdjustBackpack:
		return TownHorizontalAdjustBackpackFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::CollectItem:
		return TownHorizontalCollectItemFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::CombatIdle:
		return TownHorizontalCombatIdleFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::Punch:
		return TownHorizontalPunchFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::Kick:
		return TownHorizontalKickFlipbookAsset.LoadSynchronous();
	case EGameXXKHeroTownAction::Idle:
	default:
		return GetHorizontalTownIdleFlipbook();
	}
}

void AGameXXKHeroCharacter::InitializeTownDirectionFlipbooks()
{
	TownHorizontalIdleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalIdlePath));
	TownHorizontalWalkStartFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalWalkStartPath));
	TownHorizontalWalkLoopFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalWalkLoopPath));
	TownHorizontalWalkStopFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalWalkStopPath));
	TownHorizontalDeepBreathFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalDeepBreathPath));
	TownHorizontalAdjustBackpackFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalAdjustBackpackPath));
	TownHorizontalCollectItemFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalCollectItemPath));
	TownHorizontalCombatIdleFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalCombatIdlePath));
	TownHorizontalPunchFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalPunchPath));
	TownHorizontalKickFlipbookAsset = TSoftObjectPtr<UPaperFlipbook>(FSoftObjectPath(HeroTownHorizontalKickPath));
	TownDirectionFlipbookAssets.Reset();
	TownIdleDirectionFlipbookAssets.Reset();
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::South, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("South"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("SouthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::West, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("West"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("NorthWest"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::North, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("North"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("NorthEast"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::East, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("East"))));
	TownDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterWalkFlipbookPath(TEXT("SouthEast"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::South, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("South"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("SouthWest"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::West, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("West"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthWest, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("NorthWest"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::North, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("North"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::NorthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("NorthEast"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::East, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("East"))));
	TownIdleDirectionFlipbookAssets.Add(EGameXXKTownFacingDirection::SouthEast, TSoftObjectPtr<UPaperFlipbook>(MakeHeroCharacterIdleFlipbookPath(TEXT("SouthEast"))));
}

void AGameXXKHeroCharacter::ApplyHorizontalTownFacingMirror()
{
	if (!bUseHorizontalHeroLocomotion || !Visual)
	{
		return;
	}

	FVector Scale = Visual->GetRelativeScale3D();
	Scale.X = CurrentTownFacingDirection == EGameXXKTownFacingDirection::East
		? -FMath::Abs(Scale.X)
		: FMath::Abs(Scale.X);
	Visual->SetRelativeScale3D(Scale);
}

void AGameXXKHeroCharacter::UpdateHorizontalTownLocomotion(float Horizontal, float Vertical)
{
	const bool bNewTownMoving = !FMath::IsNearlyZero(Horizontal) || !FMath::IsNearlyZero(Vertical);
	EGameXXKTownFacingDirection NewDirection = CurrentTownFacingDirection;
	if (Horizontal < 0.0f)
	{
		NewDirection = EGameXXKTownFacingDirection::West;
	}
	else if (Horizontal > 0.0f)
	{
		NewDirection = EGameXXKTownFacingDirection::East;
	}
	else if (NewDirection != EGameXXKTownFacingDirection::West
		&& NewDirection != EGameXXKTownFacingDirection::East)
	{
		NewDirection = EGameXXKTownFacingDirection::West;
	}

	const bool bDirectionChanged = NewDirection != CurrentTownFacingDirection;
	const bool bMovementStarted = !bTownMoving && bNewTownMoving;
	const bool bMovementStopped = bTownMoving && !bNewTownMoving;
	CurrentTownFacingDirection = NewDirection;
	bTownMoving = bNewTownMoving;
	if (bMovementStarted)
	{
		CurrentTownAction = EGameXXKHeroTownAction::WalkStart;
		TownAmbientElapsedSeconds = 0.0f;
	}
	else if (bMovementStopped)
	{
		CurrentTownAction = EGameXXKHeroTownAction::WalkStop;
		TownAmbientElapsedSeconds = 0.0f;
	}

	if (bDirectionChanged || bMovementStarted || bMovementStopped || !GetCurrentTownFlipbook())
	{
		ApplyTownFacingFlipbook();
	}
}

void AGameXXKHeroCharacter::AdvanceHorizontalTownLocomotion()
{
	if (!bUseHorizontalHeroLocomotion
		|| !Visual
		|| IsLoopingTownAction(CurrentTownAction)
		|| Visual->IsPlaying())
	{
		return;
	}

	if (CurrentTownAction == EGameXXKHeroTownAction::WalkStart && bTownMoving)
	{
		CurrentTownAction = EGameXXKHeroTownAction::WalkLoop;
		ApplyTownFacingFlipbook();
		return;
	}
	ReturnToTownIdle();
}

void AGameXXKHeroCharacter::TickHorizontalTownAmbient(const float DeltaSeconds)
{
	if (!bUseHorizontalHeroLocomotion
		|| bTownMoving
		|| CurrentTownAction != EGameXXKHeroTownAction::Idle
		|| DeltaSeconds <= 0.0f)
	{
		return;
	}

	TownAmbientElapsedSeconds += DeltaSeconds;
	if (TownAmbientElapsedSeconds < TownAmbientDelaySeconds)
	{
		return;
	}

	const EGameXXKHeroTownAction AmbientAction = TownAmbientRandom.RandRange(0, 1) == 0
		? EGameXXKHeroTownAction::DeepBreath
		: EGameXXKHeroTownAction::AdjustBackpack;
	PlayTownAction(AmbientAction);
}

void AGameXXKHeroCharacter::UpdateTownFacingFromIntent(float Horizontal, float Vertical)
{
	if (bUseHorizontalHeroLocomotion)
	{
		UpdateHorizontalTownLocomotion(Horizontal, Vertical);
		return;
	}

	const bool bNewTownMoving = !FMath::IsNearlyZero(Horizontal) || !FMath::IsNearlyZero(Vertical);
	const double InputTimeSeconds = GetHeroTownInputTimeSeconds();
	EGameXXKTownFacingDirection NewDirection = bNewTownMoving
		? ResolveHeroTownFacingDirection(Horizontal, Vertical, CurrentTownFacingDirection)
		: CurrentTownFacingDirection;
	if (bTownMoving && bNewTownMoving)
	{
		if (IsHeroTownDiagonalDirection(NewDirection))
		{
			bHasPendingStopDiagonalFacingDirection = false;
		}
		else if (IsHeroTownDiagonalDirection(CurrentTownFacingDirection))
		{
			PendingStopDiagonalFacingDirection = CurrentTownFacingDirection;
			PendingStopDiagonalReleaseTimeSeconds = InputTimeSeconds;
			bHasPendingStopDiagonalFacingDirection = true;
		}
	}
	else if (!bNewTownMoving)
	{
		if (bHasPendingStopDiagonalFacingDirection
			&& InputTimeSeconds - PendingStopDiagonalReleaseTimeSeconds <= HeroTownDiagonalReleaseGraceSeconds)
		{
			NewDirection = PendingStopDiagonalFacingDirection;
		}
		bHasPendingStopDiagonalFacingDirection = false;
	}
	const bool bDirectionChanged = NewDirection != CurrentTownFacingDirection;
	const bool bMovementStateChanged = bTownMoving != bNewTownMoving;

	CurrentTownFacingDirection = NewDirection;
	bTownMoving = bNewTownMoving;

	if (bDirectionChanged || bMovementStateChanged)
	{
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
	if (IsTownMovementBlockedByModalWindow())
	{
		ReleaseHeldTownAutomationKeys();
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

bool AGameXXKHeroCharacter::IsTownMovementBlockedByModalWindow() const
{
	const AGameXXKMVPPlayerController* MVPPlayerController = Cast<AGameXXKMVPPlayerController>(GetController());
	return MVPPlayerController && MVPPlayerController->IsInventoryWindowModalInputLocked();
}

void AGameXXKHeroCharacter::UpdateTownVisualFromMovementIntent(float Horizontal, float Vertical)
{
	UpdateTownFacingFromIntent(NormalizeHeroTownMovementAxis(Horizontal), NormalizeHeroTownMovementAxis(Vertical));
}

void AGameXXKHeroCharacter::ResetTownMovementInput()
{
	ReleaseHeldTownAutomationKeys();
	RightInputPressCount = 0;
	LeftInputPressCount = 0;
	ForwardInputPressCount = 0;
	BackwardInputPressCount = 0;
	AxisHorizontalIntent = 0.0f;
	AxisVerticalIntent = 0.0f;
	RefreshTownMovementIntent();
}

bool AGameXXKHeroCharacter::SetTownAutomationKeyState(FName KeyName, bool bPressed)
{
	if (KeyName == EKeys::D.GetFName())
	{
		if (bTownAutomationRightHeld == bPressed)
		{
			return true;
		}
		bTownAutomationRightHeld = bPressed;
		bPressed ? MoveRightPressed() : MoveRightReleased();
		return true;
	}
	if (KeyName == EKeys::A.GetFName())
	{
		if (bTownAutomationLeftHeld == bPressed)
		{
			return true;
		}
		bTownAutomationLeftHeld = bPressed;
		bPressed ? MoveLeftPressed() : MoveLeftReleased();
		return true;
	}
	if (KeyName == EKeys::W.GetFName())
	{
		if (bTownAutomationForwardHeld == bPressed)
		{
			return true;
		}
		bTownAutomationForwardHeld = bPressed;
		bPressed ? MoveForwardPressed() : MoveForwardReleased();
		return true;
	}
	if (KeyName == EKeys::S.GetFName())
	{
		if (bTownAutomationBackwardHeld == bPressed)
		{
			return true;
		}
		bTownAutomationBackwardHeld = bPressed;
		bPressed ? MoveBackwardPressed() : MoveBackwardReleased();
		return true;
	}
	return false;
}

void AGameXXKHeroCharacter::ReleaseHeldTownAutomationKeys()
{
	const bool bReleaseRight = bTownAutomationRightHeld;
	const bool bReleaseLeft = bTownAutomationLeftHeld;
	const bool bReleaseForward = bTownAutomationForwardHeld;
	const bool bReleaseBackward = bTownAutomationBackwardHeld;

	// Clear every held bit before dispatching releases because each release refreshes modal input state.
	bTownAutomationRightHeld = false;
	bTownAutomationLeftHeld = false;
	bTownAutomationForwardHeld = false;
	bTownAutomationBackwardHeld = false;

	if (bReleaseRight)
	{
		MoveRightReleased();
	}
	if (bReleaseLeft)
	{
		MoveLeftReleased();
	}
	if (bReleaseForward)
	{
		MoveForwardReleased();
	}
	if (bReleaseBackward)
	{
		MoveBackwardReleased();
	}
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
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	AxisHorizontalIntent = NormalizeHeroTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveVertical(float Value)
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
	AxisVerticalIntent = NormalizeHeroTownMovementAxis(Value);
	RefreshTownMovementIntent();
}

void AGameXXKHeroCharacter::MoveRightPressed()
{
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
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
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
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
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
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
	if (IsTownMovementBlockedByModalWindow())
	{
		ResetTownMovementInput();
		return;
	}
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

bool AGameXXKHeroCharacter::PlayTownAction(const EGameXXKHeroTownAction Action)
{
	if (!bUseHorizontalHeroLocomotion)
	{
		return false;
	}
	if (Action == EGameXXKHeroTownAction::Idle)
	{
		ReturnToTownIdle();
		return true;
	}
	const bool bLocomotionAction = Action == EGameXXKHeroTownAction::WalkStart
		|| Action == EGameXXKHeroTownAction::WalkLoop
		|| Action == EGameXXKHeroTownAction::WalkStop;
	if (bTownMoving && !bLocomotionAction)
	{
		return false;
	}
	if (!GetHorizontalTownActionFlipbook(Action))
	{
		return false;
	}

	CurrentTownAction = Action;
	TownAmbientElapsedSeconds = 0.0f;
	ApplyTownFacingFlipbook();
	return true;
}

void AGameXXKHeroCharacter::ReturnToTownIdle()
{
	if (!bUseHorizontalHeroLocomotion)
	{
		ApplyDefaultTownFlipbook();
		return;
	}
	CurrentTownAction = bTownMoving
		? EGameXXKHeroTownAction::WalkLoop
		: EGameXXKHeroTownAction::Idle;
	TownAmbientElapsedSeconds = 0.0f;
	TownAmbientDelaySeconds = TownAmbientRandom.FRandRange(8.0f, 14.0f);
	ApplyTownFacingFlipbook();
}
