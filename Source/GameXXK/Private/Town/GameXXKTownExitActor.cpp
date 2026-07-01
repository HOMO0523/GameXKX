#include "Town/GameXXKTownExitActor.h"

#include "Components/BoxComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "GameFramework/Pawn.h"
#include "Interaction/GameXXKInteractionComponent.h"
#include "MVP/GameXXKMVPSubsystem.h"

AGameXXKTownExitActor::AGameXXKTownExitActor()
{
	PrimaryActorTick.bCanEverTick = false;

	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);

	InteractionArea = CreateDefaultSubobject<UBoxComponent>(TEXT("InteractionArea"));
	InteractionArea->SetupAttachment(SceneRoot);
	InteractionArea->SetBoxExtent(FVector(160.0f, 96.0f, 128.0f));
	InteractionArea->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	InteractionArea->SetCollisionObjectType(ECC_WorldDynamic);
	InteractionArea->SetCollisionResponseToAllChannels(ECR_Ignore);
	InteractionArea->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
	InteractionArea->SetGenerateOverlapEvents(true);

	FeedbackText = CreateDefaultSubobject<UTextRenderComponent>(TEXT("FeedbackText"));
	FeedbackText->SetupAttachment(SceneRoot);
	FeedbackText->SetRelativeLocation(FVector(0.0f, 0.0f, 160.0f));
	FeedbackText->SetHorizontalAlignment(EHTA_Center);
	FeedbackText->SetVerticalAlignment(EVRTA_TextCenter);
	FeedbackText->SetWorldSize(36.0f);
	FeedbackText->SetText(FText::GetEmpty());

	LastFailureReason = NSLOCTEXT("GameXXK", "TownExitNoFailure", "");
}

void AGameXXKTownExitActor::NotifyActorBeginOverlap(AActor* OtherActor)
{
	Super::NotifyActorBeginOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->AddFocusedActor(this);
	}
}

void AGameXXKTownExitActor::NotifyActorEndOverlap(AActor* OtherActor)
{
	Super::NotifyActorEndOverlap(OtherActor);

	APawn* Pawn = Cast<APawn>(OtherActor);
	UGameXXKInteractionComponent* Interaction = Pawn ? Pawn->FindComponentByClass<UGameXXKInteractionComponent>() : nullptr;
	if (Interaction)
	{
		Interaction->RemoveFocusedActor(this);
	}
}

UBoxComponent* AGameXXKTownExitActor::GetInteractionArea() const
{
	return InteractionArea;
}

UTextRenderComponent* AGameXXKTownExitActor::GetFeedbackTextComponent() const
{
	return FeedbackText;
}

bool AGameXXKTownExitActor::WasLastInteractionSuccessful() const
{
	return bLastInteractionSuccessful;
}

FText AGameXXKTownExitActor::GetLastFailureReason() const
{
	return LastFailureReason;
}

bool AGameXXKTownExitActor::ApplyDefaultInteraction(APawn* InstigatorPawn)
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem(InstigatorPawn);
	if (!Subsystem)
	{
		bLastInteractionSuccessful = false;
		LastFailureReason = NSLOCTEXT("GameXXK", "TownExitNoSubsystem", "请先接任务");
		if (FeedbackText)
		{
			FeedbackText->SetText(LastFailureReason);
		}
		return false;
	}

	bLastInteractionSuccessful = Subsystem->OpenDungeonFromTownExit();
	LastFailureReason = bLastInteractionSuccessful
		? NSLOCTEXT("GameXXK", "TownExitNoFailureAfterSuccess", "")
		: NSLOCTEXT("GameXXK", "TownExitQuestRequired", "请先接任务");
	if (FeedbackText)
	{
		FeedbackText->SetText(LastFailureReason);
	}
	return bLastInteractionSuccessful;
}

void AGameXXKTownExitActor::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

FText AGameXXKTownExitActor::GetInteractionPrompt_Implementation() const
{
	return NSLOCTEXT("GameXXK", "TownExitPrompt", "F");
}

void AGameXXKTownExitActor::Interact_Implementation(APawn* InstigatorPawn)
{
	const bool bSucceeded = ApplyDefaultInteraction(InstigatorPawn);
	OnExitInteractionResolved(InstigatorPawn, bSucceeded, LastFailureReason);
}

UGameXXKMVPSubsystem* AGameXXKTownExitActor::ResolveMVPSubsystem(APawn* InstigatorPawn) const
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
