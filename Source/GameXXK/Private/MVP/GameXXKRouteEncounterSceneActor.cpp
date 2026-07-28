#include "MVP/GameXXKRouteEncounterSceneActor.h"

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"

AGameXXKRouteEncounterSceneActor::AGameXXKRouteEncounterSceneActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(SceneRoot);
	InteractionArea->SetBoxExtent(FVector(160.0f, 160.0f, 128.0f));
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionArea->SetGenerateOverlapEvents(true);

	FeedbackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FeedbackText"));
	FeedbackText->SetupAttachment(SceneRoot);
	FeedbackText->SetRelativeLocation(FVector(0.0f, 0.0f, 150.0f));
	FeedbackText->SetHorizontalAlignment(EHTA_Center);
	FeedbackText->SetVerticalAlignment(EVRTA_TextCenter);
	FeedbackText->SetWorldSize(34.0f);

	LastFailureReason = FText::GetEmpty();
	RefreshDefaultLabel();
}

void AGameXXKRouteEncounterSceneActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->AddFocusedActor(this);
	}
}

void AGameXXKRouteEncounterSceneActor::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->RemoveFocusedActor(this);
	}
}

EGameXXKScreen AGameXXKRouteEncounterSceneActor::GetEncounterScreen() const
{
	return EncounterScreen;
}

bool AGameXXKRouteEncounterSceneActor::MatchesRuntimeScreen(EGameXXKScreen Screen) const
{
	return EncounterScreen == Screen
		&& (Screen == EGameXXKScreen::RouteEvent
			|| Screen == EGameXXKScreen::RouteCamp
			|| Screen == EGameXXKScreen::RouteMerchant);
}

UBoxComponent* AGameXXKRouteEncounterSceneActor::GetInteractionArea() const
{
	return InteractionArea;
}

UTextRenderComponent* AGameXXKRouteEncounterSceneActor::GetFeedbackTextComponent() const
{
	return FeedbackText;
}

bool AGameXXKRouteEncounterSceneActor::WasLastInteractionSuccessful() const
{
	return bLastInteractionSuccessful;
}

FText AGameXXKRouteEncounterSceneActor::GetLastFailureReason() const
{
	return LastFailureReason;
}

bool AGameXXKRouteEncounterSceneActor::ApplyDefaultInteraction(APawn* InstigatorPawn)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(InstigatorPawn);
	if (!Subsystem)
	{
		bLastInteractionSuccessful = false;
		LastFailureReason = NSLOCTEXT("GameXXK", "RouteEncounterNoSubsystem", "无法读取流程状态");
		if (FeedbackText)
		{
			FeedbackText->SetText(LastFailureReason);
		}
		return false;
	}

	const EGameXXKScreen ActiveScreen = Subsystem->GetRuntimeState().Screen;
	if (!MatchesRuntimeScreen(ActiveScreen))
	{
		bLastInteractionSuccessful = false;
		LastFailureReason = NSLOCTEXT("GameXXK", "RouteEncounterWrongScreen", "当前节点不匹配");
		if (FeedbackText)
		{
			FeedbackText->SetText(LastFailureReason);
		}
		return false;
	}

	// Resolution is deliberately controller-owned.  Existing Blueprint callers
	// still get a useful interaction here--opening the modal--but can never
	// mutate a route node until the player clicks a visible panel action.
	AGameXXKMVPPlayerController* PlayerController = InstigatorPawn
		? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController())
		: nullptr;
	if (!PlayerController && GetWorld())
	{
		PlayerController = Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}

	// This actor is the only production route-panel source.  The controller
	// re-checks that this exact actor is still the instigator's focused target,
	// which also prevents InteractionComponent's nearby-target fallback from
	// turning an unfocused F press into a route resolution path.
	const bool bOpened = PlayerController && PlayerController->OpenRouteEncounterPanelFromActor(this);
	bLastInteractionSuccessful = false;
	LastFailureReason = bOpened
		? FText::GetEmpty()
		: NSLOCTEXT("GameXXK", "RouteEncounterOpenFailed", "无法打开遭遇选择");
	if (FeedbackText)
	{
		FeedbackText->SetText(bOpened ? BuildDefaultLabel() : LastFailureReason);
	}
	return bOpened;
}

void AGameXXKRouteEncounterSceneActor::RecordExplicitChoiceResolved(APawn* InstigatorPawn)
{
	bLastInteractionSuccessful = true;
	LastFailureReason = FText::GetEmpty();
	if (FeedbackText)
	{
		FeedbackText->SetText(BuildDefaultLabel());
	}
	OnEncounterInteractionResolved(InstigatorPawn, true, LastFailureReason);
}

void AGameXXKRouteEncounterSceneActor::SetEncounterScreenForTest(EGameXXKScreen InEncounterScreen)
{
	EncounterScreen = InEncounterScreen;
	RefreshDefaultLabel();
}

void AGameXXKRouteEncounterSceneActor::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

FText AGameXXKRouteEncounterSceneActor::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("GameXXK", "RouteEncounterPrompt", "F");
}

void AGameXXKRouteEncounterSceneActor::Interact_Implementation(APawn* InstigatorPawn)
{
	const bool bOpened = ApplyDefaultInteraction(InstigatorPawn);
	// Opening a modal is not a route-node resolution.  Blueprints that consume
	// OnEncounterInteractionResolved must only observe a successful choice.
	if (!bOpened)
	{
		OnEncounterInteractionResolved(InstigatorPawn, false, LastFailureReason);
	}
}

UGameXXKMVPSubsystem* AGameXXKRouteEncounterSceneActor::ResolveMVPSubsystem(APawn* InstigatorPawn) const
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

FText AGameXXKRouteEncounterSceneActor::BuildDefaultLabel() const
{
	switch (EncounterScreen)
	{
	case EGameXXKScreen::RouteEvent:
		return NSLOCTEXT("GameXXK", "RouteEncounterEventLabel", "F: 查看事件选择");
	case EGameXXKScreen::RouteCamp:
		return NSLOCTEXT("GameXXK", "RouteEncounterCampLabel", "F: 查看营火选择");
	case EGameXXKScreen::RouteMerchant:
		return NSLOCTEXT("GameXXK", "RouteEncounterMerchantLabel", "F: 查看行商选择");
	default:
		return NSLOCTEXT("GameXXK", "RouteEncounterUnknownLabel", "F");
	}
}

void AGameXXKRouteEncounterSceneActor::RefreshDefaultLabel()
{
	if (FeedbackText)
	{
		FeedbackText->SetText(BuildDefaultLabel());
	}
}

void AGameXXKRouteEncounterSceneActor::RefreshPlayerFlowWidgets(APawn* InstigatorPawn) const
{
	AGameXXKMVPPlayerController* PlayerController = InstigatorPawn ? Cast<AGameXXKMVPPlayerController>(InstigatorPawn->GetController()) : nullptr;
	if (!PlayerController && GetWorld())
	{
		PlayerController = Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0));
	}
	if (PlayerController)
	{
		PlayerController->RefreshPlayerFlowWidgetsFromState();
	}
}
