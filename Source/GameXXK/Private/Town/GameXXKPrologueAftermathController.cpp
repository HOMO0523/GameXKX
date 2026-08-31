#include "Town/GameXXKPrologueAftermathController.h"

#include "Blueprint/UserWidget.h"
#include "Components/SceneComponent.h"
#include "Components/SphereComponent.h"
#include "Components/WidgetComponent.h"
#include "Dialogue/GameXXKDialogueAsset.h"
#include "Dialogue/GameXXKDialogueCoordinator.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture2D.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Guide/GameXXKGuideAsset.h"
#include "InputCoreTypes.h"
#include "InputKeyEventArgs.h"
#include "Interaction/GameXXKInteractableComponent.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01SessionSubsystem.h"
#include "Misc/Parse.h"
#include "Misc/PackageName.h"
#include "PaperFlipbookComponent.h"
#include "Prologue/GameXXKPrologueAftermathRules.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKPrologueCarriageRig.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "UI/GameXXKDialoguePanelWidget.h"
#include "UI/GameXXKGuidePreferenceWidget.h"
#include "UI/GameXXKPrologueMapWidget.h"
#include "UI/GameXXKProloguePauseWidget.h"
#include "UI/GameXXKPrologueYueBaiWidget.h"
#include "UI/GameXXKSpeechBubbleWidget.h"

namespace GameXXKPrologueAftermathPrivate
{
	constexpr float YueBaiIntroGroundOffsetZ = -72.0f;
	constexpr float NarrativeFollowMinimumDistance = 220.0f;
	constexpr float NarrativeFollowTargetDistance = 260.0f;
	constexpr float NarrativeFollowMaximumDistance = 300.0f;

	FString WorldOptions(const UWorld* World)
	{
		return World
			? FString(TEXT("?")) + FString::Join(World->URL.Op, TEXT("?"))
			: FString();
	}

	FString DialogueAssetStem(const FName DialogueId)
	{
		FString Stem = DialogueId.ToString();
		Stem.ReplaceInline(TEXT("."), TEXT("_"));
		return FString(TEXT("DA_")) + Stem;
	}

	bool CanAddToViewport(const AGameXXKMVPPlayerController* Controller)
	{
		const UWorld* World = Controller ? Controller->GetWorld() : nullptr;
		return World && World->IsGameWorld()
			&& Controller->IsLocalPlayerController()
			&& Controller->Player;
	}

	bool ParseTutorialReturnReason(
		const FString& Options,
		EGameXXKTutorial01ReturnReason& OutReason)
	{
		OutReason = EGameXXKTutorial01ReturnReason::None;
		FString Value;
		if (!FParse::Value(
				*Options,
				TEXT("GameXXKTutorialReturn="),
				Value))
		{
			return false;
		}
		if (Value.Equals(TEXT("Victory"), ESearchCase::IgnoreCase))
		{
			OutReason = EGameXXKTutorial01ReturnReason::Victory;
			return true;
		}
		if (Value.Equals(TEXT("Defeat"), ESearchCase::IgnoreCase))
		{
			OutReason = EGameXXKTutorial01ReturnReason::Defeat;
			return true;
		}
		return false;
	}
}

AGameXXKPrologueAftermathController::AGameXXKPrologueAftermathController()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	SetRootComponent(Root);

	StatueInteractionArea = CreateDefaultSubobject<USphereComponent>(
		TEXT("StatueInteractionArea"));
	StatueInteractionArea->SetupAttachment(Root);
	StatueInteractionArea->SetRelativeLocation(
		FVector(3251.408f, -30.0f, 534.289f));
	StatueInteractionArea->SetSphereRadius(450.0f);
	StatueInteractionArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	StatueInteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	StatueInteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	StatueInteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	StatueInteractionArea->SetGenerateOverlapEvents(true);
	StatueInteractionMetadata = CreateDefaultSubobject<UGameXXKInteractableComponent>(
		TEXT("StatueInteractionMetadata"));
	// This metadata only supplies focus priority.  With no narrative sequence it
	// remains disabled, so F falls through to this actor's interactable interface.
	StatueInteractionMetadata->Configure(
		TEXT("Interaction.Prologue.Statue"),
		FText::FromString(TEXT("巨大雕塑")),
		NAME_None,
		100,
		StatueInteractionArea);

	YueBaiReveal = CreateDefaultSubobject<USceneComponent>(TEXT("YueBaiReveal"));
	YueBaiReveal->SetupAttachment(Root);
	YueBaiReveal->SetRelativeLocation(FVector(250.623f, 666.139f, 0.0f));

	YueBaiIntroDisplay = CreateDefaultSubobject<UWidgetComponent>(TEXT("YueBaiIntroDisplay"));
	YueBaiIntroDisplay->SetupAttachment(YueBaiReveal);
	YueBaiIntroDisplay->SetRelativeLocation(
		FVector(0.0f, 0.0f, GameXXKPrologueAftermathPrivate::YueBaiIntroGroundOffsetZ));
	YueBaiIntroDisplay->SetRelativeRotation(FRotator(0.0f, 180.0f, 0.0f));
	YueBaiIntroDisplay->SetRelativeScale3D(FVector(0.75f));
	YueBaiIntroDisplay->SetWidgetSpace(EWidgetSpace::World);
	YueBaiIntroDisplay->SetDrawSize(FVector2D(512.0f, 512.0f));
	YueBaiIntroDisplay->SetPivot(FVector2D(0.5f, 1.0f));
	YueBaiIntroDisplay->SetTwoSided(true);
	YueBaiIntroDisplay->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	YueBaiIntroDisplay->TranslucencySortPriority = 11;
	YueBaiIntroDisplay->SetVisibility(false);

	MapWidgetClass = UGameXXKPrologueMapWidget::StaticClass();
	YueBaiWidgetClass = UGameXXKPrologueYueBaiWidget::StaticClass();
	GuidePreferenceWidgetClass = UGameXXKGuidePreferenceWidget::StaticClass();
	YueBaiIntroDisplay->SetWidgetClass(YueBaiWidgetClass);
	YueBaiIntroTexture2K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		UGameXXKPrologueYueBaiWidget::GetTexturePathForTest(false)));
	YueBaiIntroTexture1K = TSoftObjectPtr<UTexture2D>(FSoftObjectPath(
		UGameXXKPrologueYueBaiWidget::GetTexturePathForTest(true)));
	StatuePromptText = FText::FromString(TEXT("前往巨大雕像旁按F交互"));
	SelectedGuidePreference = EGameXXKGuidePreference::Unset;
}

void AGameXXKPrologueAftermathController::BeginPlay()
{
	Super::BeginPlay();
	SetActorTickEnabled(false);
	if (YueBaiIntroDisplay)
	{
		YueBaiIntroDisplay->SetVisibility(false);
	}
	SetStatueInteractionEnabled(false);

	UWorld* World = GetWorld();
	const FString Options =
		GameXXKPrologueAftermathPrivate::WorldOptions(World);
	if (!World || !ShouldActivateForOptionsForTest(Options))
	{
		return;
	}
	EGameXXKTutorial01ReturnReason ReturnReason =
		EGameXXKTutorial01ReturnReason::None;
	if (GameXXKPrologueAftermathPrivate::ParseTutorialReturnReason(
			Options,
			ReturnReason))
	{
		if (!ResumeFromTutorialReturn())
		{
			UE_LOG(LogTemp, Error,
				TEXT("Tutorial 0-1 town return could not restore follower state."));
		}
		return;
	}

	AGameXXKPrologueCarriageRig* UniqueRig = nullptr;
	int32 RigCount = 0;
	for (TActorIterator<AGameXXKPrologueCarriageRig> It(World); It; ++It)
	{
		UniqueRig = *It;
		++RigCount;
	}
	if (RigCount != 1 || !UniqueRig)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Prologue aftermath requires exactly one carriage rig; found %d."),
			RigCount);
		return;
	}

	BoundCarriageRig = UniqueRig;
	CarriageFinishedHandle = UniqueRig->OnFinished().AddUObject(
		this,
		&AGameXXKPrologueAftermathController::HandleCarriageFinished);
}

void AGameXXKPrologueAftermathController::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (AftermathState.bPaused)
	{
		return;
	}
	if (AftermathState.Phase == EGameXXKPrologueAftermathPhase::YueBaiIntro)
	{
		AdvanceYueBaiIntro(DeltaSeconds);
	}
	else if (AftermathState.Phase == EGameXXKPrologueAftermathPhase::StatuePrompt)
	{
		UpdatePassivePrompt();
	}
}

void AGameXXKPrologueAftermathController::EndPlay(
	const EEndPlayReason::Type EndPlayReason)
{
	CancelPresentation();
	if (AGameXXKPrologueCarriageRig* Rig = BoundCarriageRig.Get();
		Rig && CarriageFinishedHandle.IsValid())
	{
		Rig->OnFinished().Remove(CarriageFinishedHandle);
	}
	CarriageFinishedHandle.Reset();
	BoundCarriageRig.Reset();
	Super::EndPlay(EndPlayReason);
}

void AGameXXKPrologueAftermathController::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);
	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn
		? Pawn->FindComponentByClass<UGameXXKInteractionComponent>()
		: nullptr;
	if (Interaction && CanOpenGuideChoiceForTest())
	{
		Interaction->AddFocusedActor(this);
	}
}

void AGameXXKPrologueAftermathController::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);
	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn
		? Pawn->FindComponentByClass<UGameXXKInteractionComponent>()
		: nullptr;
	if (Interaction)
	{
		Interaction->RemoveFocusedActor(this);
	}
}

FText AGameXXKPrologueAftermathController::GetInteractionPrompt_Implementation() const
{
	return CanOpenGuideChoiceForTest()
		? NSLOCTEXT("GameXXK", "PrologueStatuePrompt", "F")
		: FText::GetEmpty();
}

void AGameXXKPrologueAftermathController::Interact_Implementation(APawn* InstigatorPawn)
{
	if (!InstigatorPawn || InstigatorPawn != Hero.Get() || !CanOpenGuideChoiceForTest())
	{
		return;
	}
	ShowGuideChoice();
}

bool AGameXXKPrologueAftermathController::HandleInputKey(
	const FInputKeyEventArgs& Params)
{
	if (!bPresentationActive || !IsBlockingPresentation())
	{
		return false;
	}
	if (Params.Event != IE_Pressed)
	{
		return false;
	}
	if (GuidePreferenceWidget)
	{
		if (Params.Key == EKeys::Escape)
		{
			DismissGuideChoice(true);
			return true;
		}
		return !Params.Key.IsMouseButton();
	}
	if (Params.Key == EKeys::Escape)
	{
		return SetPaused(!AftermathState.bPaused);
	}
	if (AftermathState.bPaused)
	{
		return !Params.Key.IsMouseButton();
	}

	const bool bAdvanceKey = Params.Key == EKeys::SpaceBar
		|| Params.Key == EKeys::Enter
		|| Params.Key == EKeys::LeftMouseButton;
	if (!bAdvanceKey)
	{
		return !Params.Key.IsMouseButton();
	}

	switch (AftermathState.Phase)
	{
	case EGameXXKPrologueAftermathPhase::HeroNotice:
	case EGameXXKPrologueAftermathPhase::FoodDialogue:
	case EGameXXKPrologueAftermathPhase::GuideDialogue:
		if (DialogueCoordinator)
		{
			const bool bAdvanced = DialogueCoordinator->Advance(nullptr);
			if (bAdvanced && bPresentationActive)
			{
				RefreshDialoguePhaseFromCurrentNode();
			}
			return bAdvanced;
		}
		return false;
	case EGameXXKPrologueAftermathPhase::MapThumbnail:
		if (Params.Key == EKeys::SpaceBar || Params.Key == EKeys::Enter)
		{
			return MapWidget && MapWidget->RequestContinue();
		}
		return false;
	case EGameXXKPrologueAftermathPhase::MapInspection:
	case EGameXXKPrologueAftermathPhase::YueBaiIntro:
		return true;
	default:
		return false;
	}
}

bool AGameXXKPrologueAftermathController::TogglePauseFromController()
{
	return bPresentationActive
		&& IsBlockingPresentation()
		&& SetPaused(!AftermathState.bPaused);
}

bool AGameXXKPrologueAftermathController::CancelPresentation()
{
	if (!bPresentationActive && !bYueBaiSnapshotValid)
	{
		return false;
	}
	FGameXXKPrologueAftermathRules::ApplyEvent(
		EGameXXKPrologueAftermathEvent::Cancel,
		AftermathState);
	CleanupPresentation(false);
	return true;
}

bool AGameXXKPrologueAftermathController::IsBlockingPresentation() const
{
	return bPresentationActive
		&& (GuidePreferenceWidget
			|| FGameXXKPrologueAftermathRules::IsBlockingPhase(AftermathState.Phase));
}

FVector AGameXXKPrologueAftermathController::GetYueBaiRevealOffsetForTest() const
{
	return YueBaiReveal ? YueBaiReveal->GetRelativeLocation() : FVector::ZeroVector;
}

FVector AGameXXKPrologueAftermathController::GetStatueInteractionOffsetForTest() const
{
	return StatueInteractionArea
		? StatueInteractionArea->GetRelativeLocation()
		: FVector::ZeroVector;
}

float AGameXXKPrologueAftermathController::GetStatueInteractionRadiusForTest() const
{
	return StatueInteractionArea
		? StatueInteractionArea->GetUnscaledSphereRadius()
		: 0.0f;
}

bool AGameXXKPrologueAftermathController::CanOpenGuideChoiceForTest() const
{
	return AftermathState.Phase == EGameXXKPrologueAftermathPhase::StatuePrompt
		&& !GuidePreferenceWidget;
}

bool AGameXXKPrologueAftermathController::ShouldActivateForOptionsForTest(
	const FString& Options) const
{
	EGameXXKTutorial01ReturnReason ReturnReason =
		EGameXXKTutorial01ReturnReason::None;
	return GameXXKLevelFlow::HasCarriagePreviewTravelOption(Options)
		|| GameXXKPrologueAftermathPrivate::ParseTutorialReturnReason(
			Options,
			ReturnReason);
}

bool AGameXXKPrologueAftermathController::ApplyTutorialReturnReasonForTest(
	const EGameXXKTutorial01ReturnReason ReturnReason)
{
	AftermathState = FGameXXKPrologueAftermathState();
	if (ReturnReason == EGameXXKTutorial01ReturnReason::Victory)
	{
		AftermathState.Phase = EGameXXKPrologueAftermathPhase::Finished;
		return true;
	}
	if (ReturnReason == EGameXXKTutorial01ReturnReason::Defeat)
	{
		AftermathState.Phase = EGameXXKPrologueAftermathPhase::StatuePrompt;
		return true;
	}
	return false;
}

bool AGameXXKPrologueAftermathController::ResumeFromTutorialReturn()
{
	UGameInstance* GameInstance = GetGameInstance();
	UGameXXKTutorial01SessionSubsystem* TutorialSession = GameInstance
		? GameInstance->GetSubsystem<UGameXXKTutorial01SessionSubsystem>()
		: nullptr;
	if (!TutorialSession || !TutorialSession->HasActiveSession()
		|| TutorialSession->GetContextForTest().ReturnReason
			== EGameXXKTutorial01ReturnReason::None
		|| !ResolveRuntimeActors())
	{
		return false;
	}

	const FGameXXKTutorial01ReturnContext PendingContext =
		TutorialSession->GetContextForTest();
	AGameXXKHeroCharacter* PlayerHero = Hero.Get();
	AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get();
	if (!PlayerHero || !ExistingYueBai
		|| !ApplyTutorialReturnReasonForTest(PendingContext.ReturnReason))
	{
		return false;
	}
	PlayerHero->SetActorTransform(
		PendingContext.StatueReturnTransform,
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ExistingYueBai->SetActorHiddenInGame(false);
	ExistingYueBai->ActivateNarrativeFollower(
		PlayerHero,
		GameXXKPrologueAftermathPrivate::NarrativeFollowMinimumDistance,
		GameXXKPrologueAftermathPrivate::NarrativeFollowTargetDistance,
		GameXXKPrologueAftermathPrivate::NarrativeFollowMaximumDistance);
	if (!ExistingYueBai->IsNarrativeFollowerActive())
	{
		return false;
	}

	FGameXXKTutorial01ReturnContext ConsumedContext;
	if (!TutorialSession->ConsumeTownReturn(ConsumedContext))
	{
		ExistingYueBai->DismissNarrativeFollower();
		return false;
	}
	SelectedGuidePreference = ConsumedContext.GuidePreference;
	bPresentationActive = true;
	if (ConsumedContext.ReturnReason == EGameXXKTutorial01ReturnReason::Defeat)
	{
		SetStatueInteractionEnabled(true);
		ShowPassivePrompt();
		SetActorTickEnabled(true);
	}
	else
	{
		SetStatueInteractionEnabled(false);
		HidePassivePrompt();
		SetActorTickEnabled(false);
	}
	return true;
}

bool AGameXXKPrologueAftermathController::StartRulesForTest()
{
	return FGameXXKPrologueAftermathRules::Start(AftermathState);
}

bool AGameXXKPrologueAftermathController::ApplyEventForTest(
	const EGameXXKPrologueAftermathEvent Event)
{
	return FGameXXKPrologueAftermathRules::ApplyEvent(Event, AftermathState);
}

AGameXXKTownNpcCharacter*
AGameXXKPrologueAftermathController::FindUniqueYueBaiForTest(
	const TArray<AGameXXKTownNpcCharacter*>& Candidates)
{
	AGameXXKTownNpcCharacter* Match = nullptr;
	int32 MatchCount = 0;
	for (AGameXXKTownNpcCharacter* Candidate : Candidates)
	{
		if (IsValid(Candidate) && Candidate->GetNpcId() == TEXT("Npc.YueBai"))
		{
			Match = Candidate;
			++MatchCount;
		}
	}
	return MatchCount == 1 ? Match : nullptr;
}

void AGameXXKPrologueAftermathController::HandleCarriageFinished()
{
	if (!StartPresentation())
	{
		UE_LOG(LogTemp, Error, TEXT("Prologue aftermath failed open after carriage handoff."));
		CleanupPresentation(false);
	}
}

bool AGameXXKPrologueAftermathController::StartPresentation()
{
	if (bPresentationActive || !ResolveRuntimeActors()
		|| !FGameXXKPrologueAftermathRules::Start(AftermathState))
	{
		return false;
	}

	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!Controller
		|| !Controller->BeginPrologueAftermathPresentation(this)
		|| !EnsurePresentationWidgets())
	{
		return false;
	}

	bPresentationActive = true;
	bFoodGestureRequested = false;
	return StartDialogue(NoticeDialogueId);
}

bool AGameXXKPrologueAftermathController::ResolveRuntimeActors()
{
	AGameXXKMVPPlayerController* Controller = Cast<AGameXXKMVPPlayerController>(
		UGameplayStatics::GetPlayerController(this, 0));
	AGameXXKHeroCharacter* PlayerHero = Controller
		? Cast<AGameXXKHeroCharacter>(Controller->GetPawn())
		: nullptr;
	if (!Controller || !PlayerHero || !GetWorld())
	{
		return false;
	}

	TArray<AGameXXKTownNpcCharacter*> Candidates;
	for (TActorIterator<AGameXXKTownNpcCharacter> It(GetWorld()); It; ++It)
	{
		Candidates.Add(*It);
	}
	AGameXXKTownNpcCharacter* ExistingYueBai = FindUniqueYueBaiForTest(Candidates);
	if (!ExistingYueBai)
	{
		return false;
	}

	PlayerController = Controller;
	Hero = PlayerHero;
	YueBai = ExistingYueBai;
	YueBaiOriginalTransform = ExistingYueBai->GetActorTransform();
	bYueBaiWasHidden = ExistingYueBai->IsHidden();
	bYueBaiSnapshotValid = true;
	return true;
}

bool AGameXXKPrologueAftermathController::EnsurePresentationWidgets()
{
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!Controller)
	{
		return false;
	}
	const bool bCanAddToViewport =
		GameXXKPrologueAftermathPrivate::CanAddToViewport(Controller);
	DialoguePanel = bCanAddToViewport
		? CreateWidget<UGameXXKDialoguePanelWidget>(
			Controller,
			UGameXXKDialoguePanelWidget::StaticClass())
		: NewObject<UGameXXKDialoguePanelWidget>(this, TEXT("AftermathDialoguePanel"));
	DialogueCoordinator = NewObject<UGameXXKDialogueCoordinator>(
		this,
		TEXT("AftermathDialogueCoordinator"));
	if (!DialoguePanel || !DialogueCoordinator)
	{
		return false;
	}
	if (bCanAddToViewport)
	{
		DialoguePanel->AddToViewport(220);
	}
	TransientDialogueSession = FGameXXKDialogueSessionState();
	DialogueCoordinator->Bind(
		TransientDialogueSession,
		DialoguePanel,
		nullptr,
		nullptr);
	DialogueCoordinator->SetAutoEnabled(false);
	return true;
}

bool AGameXXKPrologueAftermathController::StartDialogue(const FName DialogueId)
{
	UGameXXKDialogueAsset* Asset = ResolveDialogueAsset(DialogueId);
	if (!Asset || !DialogueCoordinator)
	{
		return false;
	}
	FGameXXKDialogueStartContext Context;
	Context.StoryId = TEXT("Story.Tutorial.Prologue");
	Context.StoryVersion = 1;
	Context.TaskId = TEXT("Task.Tutorial.Prologue");
	Context.StepId = DialogueId;
	Context.SequenceId = TEXT("Sequence.Tutorial.Prologue");
	Context.StageContractId = TEXT("StageContract.Tutorial.Prologue");
	FString Error;
	const bool bStarted = DialogueCoordinator->StartDialogue(
		*Asset,
		Context,
		FGameXXKDialogueFinished::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleDialogueFinished),
		&Error);
	if (!bStarted)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("Prologue aftermath dialogue %s failed: %s"),
			*DialogueId.ToString(),
			*Error);
	}
	return bStarted;
}

UGameXXKDialogueAsset* AGameXXKPrologueAftermathController::ResolveDialogueAsset(
	const FName DialogueId) const
{
	if (DialogueId.IsNone())
	{
		return nullptr;
	}
	const FString AssetStem =
		GameXXKPrologueAftermathPrivate::DialogueAssetStem(DialogueId);
	const FString ObjectPath = FString::Printf(
		TEXT("/Game/GameXXK/Narrative/Dialogues/%s.%s"),
		*AssetStem,
		*AssetStem);
	UGameXXKDialogueAsset* Asset =
		LoadObject<UGameXXKDialogueAsset>(nullptr, *ObjectPath);
	return Asset && Asset->DialogueId == DialogueId ? Asset : nullptr;
}

void AGameXXKPrologueAftermathController::HandleDialogueFinished(
	const FName DialogueId,
	const FName OutcomeId)
{
	if (!bPresentationActive)
	{
		return;
	}
	if (DialogueId == NoticeDialogueId
		&& OutcomeId == TEXT("Outcome.Tutorial.MapReady")
		&& AftermathState.Phase == EGameXXKPrologueAftermathPhase::HeroNotice)
	{
		if (!FGameXXKPrologueAftermathRules::ApplyEvent(
				EGameXXKPrologueAftermathEvent::DialogueCompleted,
				AftermathState))
		{
			CleanupPresentation(false);
			return;
		}
		FString Error;
		UGameXXKMVPSubsystem* Subsystem = GetGameInstance()
			? GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>()
			: nullptr;
		if (!Subsystem || !Subsystem->GrantTutorialRiverMap(&Error))
		{
			UE_LOG(
				LogTemp,
				Error,
				TEXT("Tutorial route map grant failed: %s"),
				*Error);
			CleanupPresentation(false);
			return;
		}
		ShowMapCard();
		return;
	}

	if (DialogueId == MeetingDialogueId
		&& OutcomeId == TEXT("Outcome.Tutorial.YueBaiFollowing"))
	{
		if (AftermathState.Phase == EGameXXKPrologueAftermathPhase::FoodDialogue)
		{
			FGameXXKPrologueAftermathRules::ApplyEvent(
				EGameXXKPrologueAftermathEvent::GuideDialogueStarted,
				AftermathState);
		}
		if (!FGameXXKPrologueAftermathRules::ApplyEvent(
				EGameXXKPrologueAftermathEvent::DialogueCompleted,
				AftermathState))
		{
			CleanupPresentation(false);
			return;
		}
		StartFollowingAndPrompt();
		return;
	}

	UE_LOG(
		LogTemp,
		Error,
		TEXT("Unexpected prologue aftermath dialogue outcome: %s / %s"),
		*DialogueId.ToString(),
		*OutcomeId.ToString());
	CleanupPresentation(false);
}

void AGameXXKPrologueAftermathController::RefreshDialoguePhaseFromCurrentNode()
{
	if (!DialogueCoordinator || !bPresentationActive)
	{
		return;
	}
	const FName CurrentNode = DialogueCoordinator->GetCurrentNodeIdForTest();
	if (AftermathState.Phase == EGameXXKPrologueAftermathPhase::FoodDialogue
		&& CurrentNode == TEXT("guide.yuebai.destination"))
	{
		FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::GuideDialogueStarted,
			AftermathState);
	}
	if (!bFoodGestureRequested && CurrentNode == TEXT("food.hero.offer"))
	{
		bFoodGestureRequested = true;
		if (AGameXXKHeroCharacter* PlayerHero = Hero.Get())
		{
			PlayerHero->PlayTownAction(EGameXXKHeroTownAction::AdjustBackpack);
		}
	}
}

void AGameXXKPrologueAftermathController::ShowMapCard()
{
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!Controller || !MapWidgetClass)
	{
		CleanupPresentation(false);
		return;
	}
	const bool bCanAddToViewport =
		GameXXKPrologueAftermathPrivate::CanAddToViewport(Controller);
	MapWidget = bCanAddToViewport
		? CreateWidget<UGameXXKPrologueMapWidget>(Controller, MapWidgetClass)
		: NewObject<UGameXXKPrologueMapWidget>(this, MapWidgetClass);
	if (!MapWidget)
	{
		CleanupPresentation(false);
		return;
	}
	MapWidget->Configure(EGameXXKPrologueMapMode::StoryCard);
	MapWidget->SetInspectRequestedForTest(
		FGameXXKPrologueMapInspectRequested::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleMapInspectRequested));
	MapWidget->SetCloseRequestedForTest(
		FGameXXKPrologueMapCloseRequested::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleMapCloseRequested));
	MapWidget->SetContinueRequestedForTest(
		FGameXXKPrologueMapContinueRequested::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleMapContinueRequested));
	if (bCanAddToViewport)
	{
		MapWidget->AddToViewport(225);
	}
}

void AGameXXKPrologueAftermathController::HandleMapInspectRequested()
{
	FGameXXKPrologueAftermathRules::ApplyEvent(
		EGameXXKPrologueAftermathEvent::OpenInspection,
		AftermathState);
}

void AGameXXKPrologueAftermathController::HandleMapCloseRequested()
{
	FGameXXKPrologueAftermathRules::ApplyEvent(
		EGameXXKPrologueAftermathEvent::CloseInspection,
		AftermathState);
}

void AGameXXKPrologueAftermathController::HandleMapContinueRequested()
{
	if (!FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::ContinuePressed,
			AftermathState))
	{
		return;
	}
	if (MapWidget)
	{
		MapWidget->RemoveFromParent();
		MapWidget = nullptr;
	}
	if (!BeginYueBaiIntro())
	{
		CleanupPresentation(false);
	}
}

bool AGameXXKPrologueAftermathController::BeginYueBaiIntro()
{
	AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get();
	if (!ExistingYueBai || !YueBaiReveal || !YueBaiIntroDisplay)
	{
		return false;
	}
	LoadedYueBaiIntroTexture = YueBaiIntroTexture2K.LoadSynchronous();
	if (!LoadedYueBaiIntroTexture)
	{
		LoadedYueBaiIntroTexture = YueBaiIntroTexture1K.LoadSynchronous();
	}
	UGameXXKPrologueYueBaiWidget* IntroWidget = ResolveYueBaiIntroWidget();
	if (!LoadedYueBaiIntroTexture || !IntroWidget
		|| !IntroWidget->SetAtlasFrame(LoadedYueBaiIntroTexture, 0))
	{
		return false;
	}

	ExistingYueBai->SetActorTransform(
		YueBaiReveal->GetComponentTransform(),
		false,
		nullptr,
		ETeleportType::TeleportPhysics);
	ExistingYueBai->SetActorHiddenInGame(true);
	YueBaiIntroElapsedSeconds = 0.0f;
	YueBaiIntroDisplay->SetVisibility(true);
	SetActorTickEnabled(true);
	return true;
}

void AGameXXKPrologueAftermathController::AdvanceYueBaiIntro(
	const float DeltaSeconds)
{
	YueBaiIntroElapsedSeconds += FMath::Max(0.0f, DeltaSeconds);
	UGameXXKPrologueYueBaiWidget* IntroWidget = ResolveYueBaiIntroWidget();
	const int32 FrameIndex = FMath::Clamp(
		FMath::FloorToInt(
			YueBaiIntroElapsedSeconds
			* UGameXXKPrologueYueBaiWidget::GetFramesPerSecondForTest()),
		0,
		UGameXXKPrologueYueBaiWidget::GetFrameCountForTest() - 1);
	if (!IntroWidget || !IntroWidget->SetAtlasFrame(
		LoadedYueBaiIntroTexture,
		FrameIndex))
	{
		CleanupPresentation(false);
		return;
	}
	if (YueBaiIntroElapsedSeconds
		>= UGameXXKPrologueYueBaiWidget::GetIntroDurationSecondsForTest())
	{
		FinishYueBaiIntro();
	}
}

void AGameXXKPrologueAftermathController::FinishYueBaiIntro()
{
	if (YueBaiIntroDisplay)
	{
		YueBaiIntroDisplay->SetVisibility(false);
	}
	if (AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get())
	{
		ExistingYueBai->SetActorHiddenInGame(false);
	}
	if (!FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::YueBaiIntroCompleted,
			AftermathState)
		|| !StartDialogue(MeetingDialogueId))
	{
		CleanupPresentation(false);
	}
}

UGameXXKPrologueYueBaiWidget*
AGameXXKPrologueAftermathController::ResolveYueBaiIntroWidget()
{
	if (!YueBaiIntroDisplay)
	{
		return nullptr;
	}
	YueBaiIntroDisplay->InitWidget();
	return Cast<UGameXXKPrologueYueBaiWidget>(
		YueBaiIntroDisplay->GetUserWidgetObject());
}

void AGameXXKPrologueAftermathController::StartFollowingAndPrompt()
{
	AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get();
	AGameXXKHeroCharacter* PlayerHero = Hero.Get();
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!ExistingYueBai || !PlayerHero || !Controller)
	{
		CleanupPresentation(false);
		return;
	}

	ExistingYueBai->SetActorHiddenInGame(false);
	ExistingYueBai->ActivateNarrativeFollower(
		PlayerHero,
		GameXXKPrologueAftermathPrivate::NarrativeFollowMinimumDistance,
		GameXXKPrologueAftermathPrivate::NarrativeFollowTargetDistance,
		GameXXKPrologueAftermathPrivate::NarrativeFollowMaximumDistance);
	if (!ExistingYueBai->IsNarrativeFollowerActive()
		|| !FGameXXKPrologueAftermathRules::ApplyEvent(
			EGameXXKPrologueAftermathEvent::FollowerActivated,
			AftermathState))
	{
		CleanupPresentation(false);
		return;
	}

	Controller->EndPrologueAftermathPresentation(this);
	SetStatueInteractionEnabled(true);
	ShowPassivePrompt();
	SetActorTickEnabled(true);
}

bool AGameXXKPrologueAftermathController::ShowPassivePrompt()
{
	if (PassivePromptWidget)
	{
		return true;
	}
	AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get();
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!ExistingYueBai || !Controller)
	{
		return false;
	}
	const bool bCanAddToViewport =
		GameXXKPrologueAftermathPrivate::CanAddToViewport(Controller);
	PassivePromptWidget = bCanAddToViewport
		? CreateWidget<UGameXXKSpeechBubbleWidget>(
			Controller,
			UGameXXKSpeechBubbleWidget::StaticClass())
		: NewObject<UGameXXKSpeechBubbleWidget>(this, TEXT("AftermathPassivePrompt"));
	if (!PassivePromptWidget)
	{
		return false;
	}
	if (bCanAddToViewport)
	{
		PassivePromptWidget->AddToViewport(219);
	}
	FGameXXKDialoguePresentationView PromptView;
	PromptView.NodeId = TEXT("tutorial.statue.prompt");
	PromptView.SpeakerDisplayName = FText::FromString(TEXT("月白"));
	PromptView.Text = StatuePromptText;
	if (!PassivePromptWidget->PresentBubbleAtVisualTop(
		PromptView,
		ExistingYueBai->GetTownVisualComponent()))
	{
		PassivePromptWidget->RemoveFromParent();
		PassivePromptWidget = nullptr;
		return false;
	}
	UpdatePassivePrompt();
	return true;
}

void AGameXXKPrologueAftermathController::UpdatePassivePrompt()
{
	if (PassivePromptWidget)
	{
		PassivePromptWidget->UpdateAnchor(PlayerController.Get());
	}
}

void AGameXXKPrologueAftermathController::HidePassivePrompt()
{
	if (PassivePromptWidget)
	{
		PassivePromptWidget->ClearBubble();
		PassivePromptWidget->RemoveFromParent();
		PassivePromptWidget = nullptr;
	}
}

bool AGameXXKPrologueAftermathController::ShowGuideChoice()
{
	if (!CanOpenGuideChoiceForTest() || !GuidePreferenceWidgetClass)
	{
		return false;
	}
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!Controller)
	{
		return false;
	}
	const bool bCanAddToViewport =
		GameXXKPrologueAftermathPrivate::CanAddToViewport(Controller);
	GuidePreferenceWidget = bCanAddToViewport
		? CreateWidget<UGameXXKGuidePreferenceWidget>(
			Controller,
			GuidePreferenceWidgetClass)
		: NewObject<UGameXXKGuidePreferenceWidget>(this, GuidePreferenceWidgetClass);
	if (!GuidePreferenceWidget)
	{
		return false;
	}
	GuidePreferenceWidget->SetPreferenceChosenDelegate(
		FGameXXKGuidePreferenceChosen::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleGuidePreferenceChosen));
	if (!Controller->BeginPrologueAftermathPresentation(this))
	{
		GuidePreferenceWidget = nullptr;
		return false;
	}
	SetStatueInteractionEnabled(false);
	HidePassivePrompt();
	if (bCanAddToViewport)
	{
		GuidePreferenceWidget->AddToViewport(4000);
	}
	GuidePreferenceWidget->PresentPrompt();
	return true;
}

void AGameXXKPrologueAftermathController::DismissGuideChoice(
	const bool bRestorePrompt)
{
	if (GuidePreferenceWidget)
	{
		GuidePreferenceWidget->DismissPrompt();
		GuidePreferenceWidget->RemoveFromParent();
		GuidePreferenceWidget = nullptr;
	}
	if (AGameXXKMVPPlayerController* Controller = PlayerController.Get())
	{
		Controller->EndPrologueAftermathPresentation(this);
	}
	if (bRestorePrompt
		&& AftermathState.Phase == EGameXXKPrologueAftermathPhase::StatuePrompt)
	{
		SetStatueInteractionEnabled(true);
		ShowPassivePrompt();
	}
}

void AGameXXKPrologueAftermathController::HandleGuidePreferenceChosen(
	const EGameXXKGuidePreference Preference)
{
	if (!BeginTutorial01Travel(Preference))
	{
		DismissGuideChoice(true);
	}
}

bool AGameXXKPrologueAftermathController::BeginTutorial01Travel(
	const EGameXXKGuidePreference Preference)
{
	UWorld* World = GetWorld();
	UGameInstance* GameInstance = GetGameInstance();
	AGameXXKHeroCharacter* PlayerHero = Hero.Get();
	if (!World || !World->IsGameWorld() || !GameInstance || !PlayerHero
		|| !FPackageName::DoesPackageExist(
			GameXXKLevelFlow::Tutorial01Map().ToString()))
	{
		return false;
	}

	UGameXXKMVPSubsystem* MVPSubsystem =
		GameInstance->GetSubsystem<UGameXXKMVPSubsystem>();
	UGameXXKTutorial01SessionSubsystem* TutorialSession =
		GameInstance->GetSubsystem<UGameXXKTutorial01SessionSubsystem>();
	if (!PrepareTutorial01TravelForTest(
		Preference,
		MVPSubsystem,
		TutorialSession,
		PlayerHero->GetActorTransform()))
	{
		return false;
	}

	DismissGuideChoice(false);
	SetStatueInteractionEnabled(false);
	HidePassivePrompt();
	SetActorTickEnabled(false);
	UGameplayStatics::OpenLevel(
		World,
		GameXXKLevelFlow::Tutorial01Map(),
		true,
		GameXXKLevelFlow::Tutorial01TravelOptions());
	return true;
}

bool AGameXXKPrologueAftermathController::PrepareTutorial01TravelForTest(
	const EGameXXKGuidePreference Preference,
	UGameXXKMVPSubsystem* MVPSubsystem,
	UGameXXKTutorial01SessionSubsystem* TutorialSession,
	const FTransform& StatueReturnTransform)
{
	if (bTutorialTravelPending || !MVPSubsystem || !TutorialSession
		|| Preference == EGameXXKGuidePreference::Unset
		|| AftermathState.Phase != EGameXXKPrologueAftermathPhase::StatuePrompt)
	{
		return false;
	}
	if (!TutorialSession->BeginFromTown(
		MVPSubsystem->GetRuntimeState(),
		StatueReturnTransform,
		Preference))
	{
		return false;
	}
	if (!FGameXXKPrologueAftermathRules::ApplyEvent(
		EGameXXKPrologueAftermathEvent::StatueInteracted,
		AftermathState))
	{
		TutorialSession->CancelSession();
		return false;
	}

	SelectedGuidePreference = Preference;
	bTutorialTravelPending = true;
	return true;
}

void AGameXXKPrologueAftermathController::SetStatueInteractionEnabled(
	const bool bEnabled)
{
	if (!StatueInteractionArea)
	{
		return;
	}
	APawn* PlayerHero = Hero.Get();
	UGameXXKInteractionComponent* Interaction = PlayerHero
		? PlayerHero->FindComponentByClass<UGameXXKInteractionComponent>()
		: nullptr;
	if (!bEnabled)
	{
		if (Interaction)
		{
			Interaction->RemoveFocusedActor(this);
		}
		StatueInteractionArea->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		return;
	}

	StatueInteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	StatueInteractionArea->UpdateOverlaps();
	if (Interaction
		&& StatueInteractionArea->IsOverlappingActor(PlayerHero)
		&& CanOpenGuideChoiceForTest())
	{
		Interaction->AddFocusedActor(this);
	}
}

bool AGameXXKPrologueAftermathController::SetPaused(const bool bPaused)
{
	if (!bPresentationActive || !IsBlockingPresentation()
		|| AftermathState.bPaused == bPaused)
	{
		return false;
	}
	if (bPaused && !ShowPauseOverlay())
	{
		CleanupPresentation(false);
		return false;
	}
	FGameXXKPrologueAftermathRules::SetPaused(AftermathState, bPaused);
	if (!bPaused)
	{
		HidePauseOverlay();
	}
	if (AGameXXKMVPPlayerController* Controller = PlayerController.Get())
	{
		Controller->SetPrologueAftermathPaused(
			this,
			bPaused,
			PauseWidget.Get());
	}
	return AftermathState.bPaused == bPaused;
}

bool AGameXXKPrologueAftermathController::ShowPauseOverlay()
{
	if (PauseWidget)
	{
		return true;
	}
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	if (!Controller)
	{
		return false;
	}
	const bool bCanAddToViewport =
		GameXXKPrologueAftermathPrivate::CanAddToViewport(Controller);
	PauseWidget = bCanAddToViewport
		? CreateWidget<UGameXXKProloguePauseWidget>(
			Controller,
			UGameXXKProloguePauseWidget::StaticClass())
		: NewObject<UGameXXKProloguePauseWidget>(this, TEXT("AftermathPauseWidget"));
	if (!PauseWidget)
	{
		return false;
	}
	PauseWidget->SetResumeRequested(
		FGameXXKPrologueResumeRequested::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleResumeRequested));
	PauseWidget->SetReturnDesktopRequested(
		FGameXXKPrologueReturnDesktopRequested::CreateUObject(
			this,
			&AGameXXKPrologueAftermathController::HandleReturnDesktopRequested));
	if (bCanAddToViewport)
	{
		PauseWidget->AddToViewport(5000);
	}
	return true;
}

void AGameXXKPrologueAftermathController::HidePauseOverlay()
{
	if (PauseWidget)
	{
		PauseWidget->RemoveFromParent();
		PauseWidget = nullptr;
	}
}

void AGameXXKPrologueAftermathController::HandleResumeRequested()
{
	SetPaused(false);
}

void AGameXXKPrologueAftermathController::HandleReturnDesktopRequested()
{
	AGameXXKMVPPlayerController* Controller = PlayerController.Get();
	CancelPresentation();
	if (Controller)
	{
		Controller->RequestDesktopReturnFromPrologue();
	}
}

void AGameXXKPrologueAftermathController::CleanupPresentation(
	const bool bKeepFollower)
{
	if (bCleanupInProgress)
	{
		return;
	}
	bCleanupInProgress = true;
	SetActorTickEnabled(false);
	DismissGuideChoice(false);
	SetStatueInteractionEnabled(false);
	HidePauseOverlay();
	HidePassivePrompt();
	if (MapWidget)
	{
		MapWidget->RemoveFromParent();
		MapWidget = nullptr;
	}
	if (DialogueCoordinator)
	{
		DialogueCoordinator->PauseAndExit();
		DialogueCoordinator = nullptr;
	}
	if (DialoguePanel)
	{
		DialoguePanel->RemoveFromParent();
		DialoguePanel = nullptr;
	}
	if (YueBaiIntroDisplay)
	{
		YueBaiIntroDisplay->SetVisibility(false);
	}
	LoadedYueBaiIntroTexture = nullptr;

	if (AGameXXKTownNpcCharacter* ExistingYueBai = YueBai.Get();
		ExistingYueBai && bYueBaiSnapshotValid && !bKeepFollower)
	{
		ExistingYueBai->DismissNarrativeFollower();
		ExistingYueBai->SetActorTransform(
			YueBaiOriginalTransform,
			false,
			nullptr,
			ETeleportType::TeleportPhysics);
		ExistingYueBai->SetActorHiddenInGame(bYueBaiWasHidden);
	}
	if (AGameXXKMVPPlayerController* Controller = PlayerController.Get())
	{
		Controller->EndPrologueAftermathPresentation(this);
	}

	bPresentationActive = false;
	bYueBaiSnapshotValid = false;
	bFoodGestureRequested = false;
	Hero.Reset();
	YueBai.Reset();
	PlayerController.Reset();
	bCleanupInProgress = false;
}
