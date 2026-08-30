#include "Town/GameXXKPrologueCarriageRig.h"

#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "Prologue/GameXXKPrologueCarriageRules.h"
#include "TimerManager.h"
#include "Town/GameXXKHeroCharacter.h"
#include "UI/GameXXKPrologueCarriageWidget.h"

namespace
{
	constexpr int32 CarriageSortPriority = 9;
	const FVector2D CarriageDrawSize(512.0f, 512.0f);
	const FVector2D CarriagePivot(0.5f, 1.0f);

	const TCHAR* RunStop2KPath =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_run_stop_2k_atlas."
			"T_cinematic_carriage_run_stop_2k_atlas");
	const TCHAR* PostStopIdle2KPath =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_post_stop_idle_2k_atlas."
			"T_cinematic_carriage_post_stop_idle_2k_atlas");
	const TCHAR* RunStop1KPath =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_run_stop_1k_atlas."
			"T_cinematic_carriage_run_stop_1k_atlas");
	const TCHAR* PostStopIdle1KPath =
		TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/"
			"T_cinematic_carriage_post_stop_idle_1k_atlas."
			"T_cinematic_carriage_post_stop_idle_1k_atlas");
}

AGameXXKPrologueCarriageRig::AGameXXKPrologueCarriageRig()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	CarriageStart = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageStart"));
	CarriageStart->SetupAttachment(Root);
	CarriageStart->SetRelativeLocation(FVector(0.0f, -400.0f, 0.0f));
	CarriageStop = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageStop"));
	CarriageStop->SetupAttachment(Root);
	CarriageExit = CreateDefaultSubobject<USceneComponent>(TEXT("CarriageExit"));
	CarriageExit->SetupAttachment(Root);
	CarriageExit->SetRelativeLocation(FVector(0.0f, 800.0f, 0.0f));
	HeroReveal = CreateDefaultSubobject<USceneComponent>(TEXT("HeroReveal"));
	HeroReveal->SetupAttachment(Root);
	HeroReveal->SetRelativeLocation(FVector(-80.0f, 0.0f, 0.0f));

	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(Root);
	CameraBoom->SetUsingAbsoluteRotation(true);
	CameraBoom->TargetArmLength = 900.0f;
	CameraBoom->SetRelativeRotation(FRotator(-60.0f, 0.0f, 0.0f));
	CameraBoom->bDoCollisionTest = false;
	IntroCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("IntroCamera"));
	IntroCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
	IntroCamera->bUsePawnControlRotation = false;

	CarriageWidgetClass = UGameXXKPrologueCarriageWidget::StaticClass();
	CarriageDisplay = CreateDefaultSubobject<UWidgetComponent>(TEXT("CarriageDisplay"));
	CarriageDisplay->SetupAttachment(Root);
	CarriageDisplay->SetWidgetSpace(EWidgetSpace::World);
	CarriageDisplay->SetWidgetClass(CarriageWidgetClass);
	CarriageDisplay->SetDrawSize(CarriageDrawSize);
	CarriageDisplay->SetPivot(CarriagePivot);
	CarriageDisplay->SetBlendMode(EWidgetBlendMode::Transparent);
	CarriageDisplay->SetTwoSided(true);
	CarriageDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	CarriageDisplay->SetRelativeLocation(FVector(0.0f, 0.0f, -72.0f));
	CarriageDisplay->SetRelativeRotation(FRotator(0.0f, 90.0f, 0.0f));
	CarriageDisplay->SetRelativeScale3D(FVector(0.75f));
	CarriageDisplay->SetTranslucentSortPriority(CarriageSortPriority);
	CarriageDisplay->SetVisibility(false);

	RunStopTexture2K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(RunStop2KPath));
	PostStopIdleTexture2K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(PostStopIdle2KPath));
	RunStopTexture1K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(RunStop1KPath));
	PostStopIdleTexture1K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(PostStopIdle1KPath));
}

void AGameXXKPrologueCarriageRig::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	CarriageDisplay->SetVisibility(false);
	const UWorld* World = GetWorld();
	const FString WorldOptions = World
		? FString(TEXT("?")) + FString::Join(World->URL.Op, TEXT("?"))
		: FString();
	if (!World || !ShouldActivateForOptionsForTest(WorldOptions))
	{
		return;
	}
	GetWorldTimerManager().SetTimerForNextTick(this, &AGameXXKPrologueCarriageRig::TryStartPresentation);
}

void AGameXXKPrologueCarriageRig::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (bPresentationActive)
	{
		AdvancePresentation(DeltaSeconds);
	}
}

void AGameXXKPrologueCarriageRig::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	CancelPresentation();
	Super::EndPlay(EndPlayReason);
}

bool AGameXXKPrologueCarriageRig::CancelPresentation()
{
	if (!bPresentationActive && !bHeroSnapshotValid)
	{
		return false;
	}
	FGameXXKPrologueCarriageRules::Cancel(TimelineState);
	CleanupPresentation(false);
	return true;
}

FVector AGameXXKPrologueCarriageRig::GetStartOffsetForTest() const
{
	return CarriageStart ? CarriageStart->GetRelativeLocation() : FVector::ZeroVector;
}

FVector AGameXXKPrologueCarriageRig::GetStopOffsetForTest() const
{
	return CarriageStop ? CarriageStop->GetRelativeLocation() : FVector::ZeroVector;
}

FVector AGameXXKPrologueCarriageRig::GetExitOffsetForTest() const
{
	return CarriageExit ? CarriageExit->GetRelativeLocation() : FVector::ZeroVector;
}

FVector AGameXXKPrologueCarriageRig::GetHeroRevealOffsetForTest() const
{
	return HeroReveal ? HeroReveal->GetRelativeLocation() : FVector::ZeroVector;
}

int32 AGameXXKPrologueCarriageRig::GetCarriageSortPriorityForTest() const
{
	return CarriageDisplay ? CarriageDisplay->TranslucencySortPriority : 0;
}

FVector2D AGameXXKPrologueCarriageRig::GetCarriageDrawSizeForTest() const
{
	const FVector2D DrawSize = CarriageDisplay
		? CarriageDisplay->GetDrawSize()
		: FVector2D::ZeroVector;
	return DrawSize;
}

FVector2D AGameXXKPrologueCarriageRig::GetCarriagePivotForTest() const
{
	return CarriageDisplay ? CarriageDisplay->GetPivot() : FVector2D::ZeroVector;
}

TSubclassOf<UGameXXKPrologueCarriageWidget>
AGameXXKPrologueCarriageRig::GetCarriageWidgetClassForTest() const
{
	return CarriageWidgetClass;
}

bool AGameXXKPrologueCarriageRig::ShouldActivateForOptionsForTest(
	const FString& Options) const
{
	return GameXXKLevelFlow::HasCarriagePreviewTravelOption(Options);
}

FString AGameXXKPrologueCarriageRig::GetRunStopTexturePathForTest(
	const bool bLowResolution) const
{
	return (bLowResolution ? RunStopTexture1K : RunStopTexture2K)
		.ToSoftObjectPath().ToString();
}

FString AGameXXKPrologueCarriageRig::GetPostStopIdleTexturePathForTest(
	const bool bLowResolution) const
{
	return (bLowResolution ? PostStopIdleTexture1K : PostStopIdleTexture2K)
		.ToSoftObjectPath().ToString();
}

bool AGameXXKPrologueCarriageRig::StartTimelineForTest()
{
	return FGameXXKPrologueCarriageRules::Start(TimelineState);
}

bool AGameXXKPrologueCarriageRig::AdvanceTimelineForTest(
	const float DeltaSeconds)
{
	FGameXXKPrologueCarriageStepOutput Step;
	return FGameXXKPrologueCarriageRules::Advance(
		DeltaSeconds,
		TimelineConfig,
		TimelineState,
		Step);
}

void AGameXXKPrologueCarriageRig::TryStartPresentation()
{
	if (bPresentationActive)
	{
		return;
	}
	AGameXXKMVPPlayerController* PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(this, 0));
	AGameXXKHeroCharacter* PlayerHero = PlayerController
		? Cast<AGameXXKHeroCharacter>(PlayerController->GetPawn())
		: nullptr;
	if (!PlayerController || !PlayerHero)
	{
		++StartRetryCount;
		if (StartRetryCount < MaximumStartRetries)
		{
			GetWorldTimerManager().SetTimerForNextTick(
				this,
				&AGameXXKPrologueCarriageRig::TryStartPresentation);
			return;
		}
		FailOpen(TEXT("player controller or hero was unavailable"));
		return;
	}

	Controller = PlayerController;
	Hero = PlayerHero;
	if (!LoadCarriageTextures() || !ResolveCarriageWidget()
		|| !PlayerController->BeginPrologueCarriagePresentation(this)
		|| !CaptureAndHideHero()
		|| !FGameXXKPrologueCarriageRules::Start(TimelineState))
	{
		FailOpen(TEXT("required carriage presentation dependency failed"));
		return;
	}

	bPresentationActive = true;
	CarriageDisplay->SetWorldLocation(CarriageStart->GetComponentLocation());
	CarriageDisplay->SetVisibility(true);
	CarriageWidget->SetAtlasFrame(LoadedRunStopTexture, 0);
	SetActorTickEnabled(true);
}

bool AGameXXKPrologueCarriageRig::LoadCarriageTextures()
{
	LoadedRunStopTexture = RunStopTexture2K.LoadSynchronous();
	LoadedPostStopIdleTexture = PostStopIdleTexture2K.LoadSynchronous();
	if (!LoadedRunStopTexture || !LoadedPostStopIdleTexture)
	{
		LoadedRunStopTexture = RunStopTexture1K.LoadSynchronous();
		LoadedPostStopIdleTexture = PostStopIdleTexture1K.LoadSynchronous();
	}
	return LoadedRunStopTexture && LoadedPostStopIdleTexture;
}

bool AGameXXKPrologueCarriageRig::CaptureAndHideHero()
{
	AGameXXKHeroCharacter* PlayerHero = Hero.Get();
	if (!PlayerHero)
	{
		return false;
	}
	HeroOriginalTransform = PlayerHero->GetActorTransform();
	bHeroWasHidden = PlayerHero->IsHidden();
	HeroOriginalCollision = PlayerHero->GetCapsuleComponent()->GetCollisionEnabled();
	if (const UCharacterMovementComponent* Movement = PlayerHero->GetCharacterMovement())
	{
		HeroOriginalMovementMode = static_cast<uint8>(Movement->MovementMode);
	}
	bHeroSnapshotValid = true;
	PlayerHero->ResetTownMovementInput();
	PlayerHero->SetActorHiddenInGame(true);
	PlayerHero->GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	if (UCharacterMovementComponent* Movement = PlayerHero->GetCharacterMovement())
	{
		Movement->DisableMovement();
	}
	return true;
}

void AGameXXKPrologueCarriageRig::AdvancePresentation(
	const float DeltaSeconds)
{
	FGameXXKPrologueCarriageStepOutput Step;
	if (FGameXXKPrologueCarriageRules::Advance(
		DeltaSeconds,
		TimelineConfig,
		TimelineState,
		Step))
	{
		ApplyStep(Step);
	}
}

void AGameXXKPrologueCarriageRig::ApplyStep(
	const FGameXXKPrologueCarriageStepOutput& Step)
{
	if (!CarriageDisplay || !CarriageWidget)
	{
		FailOpen(TEXT("carriage display disappeared during playback"));
		return;
	}

	if (Step.PreviousPhase == EGameXXKPrologueCarriagePhase::Arriving)
	{
		const float Alpha = FMath::InterpEaseOut(0.0f, 1.0f, Step.MotionAlpha, 2.0f);
		CarriageDisplay->SetWorldLocation(FMath::Lerp(
			CarriageStart->GetComponentLocation(),
			CarriageStop->GetComponentLocation(),
			Alpha));
	}
	else if (Step.PreviousPhase == EGameXXKPrologueCarriagePhase::Departing)
	{
		CarriageDisplay->SetWorldLocation(FMath::Lerp(
			CarriageStop->GetComponentLocation(),
			CarriageExit->GetComponentLocation(),
			Step.MotionAlpha));
	}

	UTexture2D* Texture = Step.Atlas == EGameXXKPrologueCarriageAtlas::PostStopIdle
		? LoadedPostStopIdleTexture.Get()
		: LoadedRunStopTexture.Get();
	if (Step.Atlas != EGameXXKPrologueCarriageAtlas::None
		&& !CarriageWidget->SetAtlasFrame(Texture, Step.AtlasFrameIndex))
	{
		FailOpen(TEXT("carriage frame application failed"));
		return;
	}
	if (Step.bRevealHero)
	{
		RevealHeroAtMarker();
	}
	if (Step.bBeginHandoff)
	{
		CompletePresentation();
	}
}

void AGameXXKPrologueCarriageRig::RevealHeroAtMarker()
{
	if (AGameXXKHeroCharacter* PlayerHero = Hero.Get())
	{
		PlayerHero->SetActorTransform(
			HeroReveal->GetComponentTransform(),
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		PlayerHero->SetActorHiddenInGame(false);
	}
}

void AGameXXKPrologueCarriageRig::CompletePresentation()
{
	FGameXXKPrologueCarriageStepOutput FinishStep;
	FGameXXKPrologueCarriageRules::Advance(
		0.0f,
		TimelineConfig,
		TimelineState,
		FinishStep);
	const bool bBroadcast =
		FGameXXKPrologueCarriageRules::ConsumeFinishBroadcast(TimelineState);
	CleanupPresentation(true);
	if (bBroadcast)
	{
		FinishedDelegate.Broadcast();
	}
}

void AGameXXKPrologueCarriageRig::FailOpen(const TCHAR* Reason)
{
	UE_LOG(LogTemp, Error, TEXT("Prologue carriage preview failed open: %s"), Reason);
	FGameXXKPrologueCarriageRules::Cancel(TimelineState);
	CleanupPresentation(false);
}

void AGameXXKPrologueCarriageRig::CleanupPresentation(
	const bool bSuccessfulHandoff)
{
	if (bCleanupInProgress)
	{
		return;
	}
	bCleanupInProgress = true;
	SetActorTickEnabled(false);
	if (CarriageDisplay)
	{
		CarriageDisplay->SetVisibility(false);
	}

	if (AGameXXKHeroCharacter* PlayerHero = Hero.Get();
		PlayerHero && bHeroSnapshotValid)
	{
		if (!bSuccessfulHandoff)
		{
			PlayerHero->SetActorTransform(
				HeroOriginalTransform,
				false,
				nullptr,
				ETeleportType::TeleportPhysics);
		}
		PlayerHero->SetActorHiddenInGame(bHeroWasHidden);
		PlayerHero->GetCapsuleComponent()->SetCollisionEnabled(HeroOriginalCollision);
		if (UCharacterMovementComponent* Movement = PlayerHero->GetCharacterMovement())
		{
			Movement->SetMovementMode(
				static_cast<EMovementMode>(HeroOriginalMovementMode));
		}
	}
	if (AGameXXKMVPPlayerController* PlayerController = Controller.Get())
	{
		PlayerController->EndPrologueCarriagePresentation(this);
	}

	bPresentationActive = false;
	bHeroSnapshotValid = false;
	Hero.Reset();
	Controller.Reset();
	bCleanupInProgress = false;
}

UGameXXKPrologueCarriageWidget*
AGameXXKPrologueCarriageRig::ResolveCarriageWidget()
{
	if (!CarriageDisplay)
	{
		return nullptr;
	}
	CarriageDisplay->InitWidget();
	CarriageWidget = Cast<UGameXXKPrologueCarriageWidget>(
		CarriageDisplay->GetUserWidgetObject());
	return CarriageWidget;
}
