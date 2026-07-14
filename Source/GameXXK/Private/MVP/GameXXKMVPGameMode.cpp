#include "MVP/GameXXKMVPGameMode.h"

#include "MVP/GameXXKLevelFlow.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownExitActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "UI/GameXXKMVPHUD.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

namespace
{
	const FVector QingshanTownExitLocation(0.0f, 1380.0f, 120.0f);
	const FName QingshanTownExitActorName(TEXT("QingshanInn_TownExit"));

	bool IsTownGameplayMap(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		const FString PackageName = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		return GameXXKLevelFlow::IsTownGameplayMapPackage(PackageName);
	}

	bool IsLegacyQingshanInnMap(const UWorld* World)
	{
		if (!World)
		{
			return false;
		}

		const FString PackageName = World->GetOutermost() ? World->GetOutermost()->GetName() : FString();
		return GameXXKLevelFlow::MapPackageMatches(PackageName, FName(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
	}
}

AGameXXKMVPGameMode::AGameXXKMVPGameMode()
{
	PlayerControllerClass = AGameXXKMVPPlayerController::StaticClass();
	HUDClass = AGameXXKMVPHUD::StaticClass();
	UClass* HeroCharacterBlueprintClass = LoadClass<AGameXXKHeroCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C"), nullptr, LOAD_NoWarn);
	if (HeroCharacterBlueprintClass)
	{
		DefaultPawnClass = HeroCharacterBlueprintClass;
	}
	else
	{
		DefaultPawnClass = AGameXXKHeroCharacter::StaticClass();
	}
	MerchantTownNpcCharacterClass = LoadClass<AGameXXKTownNpcCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C"), nullptr, LOAD_NoWarn);
	PersonTownNpcCharacterClass = LoadClass<AGameXXKTownNpcCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C"), nullptr, LOAD_NoWarn);
}

void AGameXXKMVPGameMode::BeginPlay()
{
	Super::BeginPlay();

	// This game mode is also used by prototype maps.  Only the gameplay town
	// should restore its persisted actors and player location; a showcase map
	// must keep its own PlayerStart and placed scene intact.
	if (!IsTownGameplayMap(GetWorld()))
	{
		return;
	}

	UGameXXKMVPSubsystem* Subsystem = GetGameInstance() ? GetGameInstance()->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
	if (Subsystem)
	{
		Subsystem->EnsureQingshanTownRuntimeForDirectMap();
	}

	AGameXXKTownNpcCharacter* QuestNpc = FindSpawnedTownNpc(EGameXXKTownNpcRole::Quest);
	if (IsLegacyQingshanInnMap(GetWorld()))
	{
		if (!QuestNpc)
		{
			QuestNpc = SpawnTownNpc(EGameXXKTownNpcRole::Quest, FVector(260.0f, -120.0f, 120.0f), PersonTownNpcCharacterClass);
		}
		if (!FindSpawnedTownNpc(EGameXXKTownNpcRole::Merchant))
		{
			SpawnTownNpc(EGameXXKTownNpcRole::Merchant, FVector(260.0f, 120.0f, 120.0f), MerchantTownNpcCharacterClass);
		}
		if (!FindSpawnedTownNpc(EGameXXKTownNpcRole::Follower))
		{
			SpawnTownNpc(EGameXXKTownNpcRole::Follower, FVector(100.0f, -220.0f, 120.0f), PersonTownNpcCharacterClass);
		}
	}

	EnsureTownExit();

	APawn* PlayerPawn = UGameplayStatics::GetPlayerPawn(this, 0);
	ApplyPlayerRuntimeState(PlayerPawn, Subsystem);
	ApplyQuestNpcRuntimeState(QuestNpc, Subsystem, PlayerPawn);
}

int32 AGameXXKMVPGameMode::GetSpawnedTownNpcCount() const
{
	return SpawnedTownNpcs.Num();
}

int32 AGameXXKMVPGameMode::GetConfiguredTownNpcCount() const
{
	return 3;
}

int32 AGameXXKMVPGameMode::GetConfiguredTownExitCount() const
{
	return 1;
}

FVector AGameXXKMVPGameMode::GetConfiguredTownExitLocation() const
{
	return QingshanTownExitLocation;
}

TSubclassOf<AGameXXKHeroCharacter> AGameXXKMVPGameMode::GetMerchantTownNpcVisualClass() const
{
	return MerchantTownNpcCharacterClass.Get();
}

TSubclassOf<AGameXXKHeroCharacter> AGameXXKMVPGameMode::GetPersonTownNpcVisualClass() const
{
	return PersonTownNpcCharacterClass.Get();
}

TSubclassOf<AGameXXKTownNpcCharacter> AGameXXKMVPGameMode::GetMerchantTownNpcCharacterClass() const
{
	return MerchantTownNpcCharacterClass;
}

TSubclassOf<AGameXXKTownNpcCharacter> AGameXXKMVPGameMode::GetPersonTownNpcCharacterClass() const
{
	return PersonTownNpcCharacterClass;
}

void AGameXXKMVPGameMode::ApplyQuestNpcRuntimeState(AGameXXKTownNpcCharacter* QuestNpc, const UGameXXKMVPSubsystem* Subsystem, APawn* FollowTarget) const
{
	if (!QuestNpc || !Subsystem)
	{
		return;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	const bool bQuestFollowerShouldRestore = State.QuestState == EGameXXKQuestState::Accepted && State.bFollowerJoined;
	if (bQuestFollowerShouldRestore && State.bHasQuestNpcLocation)
	{
		QuestNpc->SetActorLocation(State.QuestNpcLocation, false, nullptr, ETeleportType::TeleportPhysics);
	}

	if (bQuestFollowerShouldRestore && FollowTarget)
	{
		QuestNpc->ActivateFollower(FollowTarget, QuestNpc->GetFollowDistance());
		return;
	}

	QuestNpc->DismissFollower();
}

void AGameXXKMVPGameMode::ApplyPlayerRuntimeState(APawn* PlayerPawn, const UGameXXKMVPSubsystem* Subsystem) const
{
	if (!PlayerPawn || !Subsystem)
	{
		return;
	}

	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	if (State.Screen != EGameXXKScreen::Town || !State.bHasPlayerLocation)
	{
		return;
	}

	PlayerPawn->SetActorLocation(State.PlayerLocation, false, nullptr, ETeleportType::TeleportPhysics);
}

AGameXXKTownNpcCharacter* AGameXXKMVPGameMode::SpawnTownNpc(EGameXXKTownNpcRole NpcRole, const FVector& Location, TSubclassOf<AGameXXKTownNpcCharacter> NpcClass)
{
	UWorld* World = GetWorld();
	if (!World || !NpcClass)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKTownNpcCharacter* Npc = World->SpawnActor<AGameXXKTownNpcCharacter>(NpcClass, Location, FRotator::ZeroRotator, SpawnParameters);
	if (Npc)
	{
		Npc->SetNpcRole(NpcRole);
		SpawnedTownNpcs.Add(Npc);
	}
	return Npc;
}

AGameXXKTownNpcCharacter* AGameXXKMVPGameMode::FindSpawnedTownNpc(EGameXXKTownNpcRole NpcRole) const
{
	for (AGameXXKTownNpcCharacter* Npc : SpawnedTownNpcs)
	{
		if (Npc && Npc->GetNpcRole() == NpcRole)
		{
			return Npc;
		}
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGameXXKTownNpcCharacter> It(World); It; ++It)
	{
		if (AGameXXKTownNpcCharacter* Npc = *It; Npc && Npc->GetNpcRole() == NpcRole)
		{
			return Npc;
		}
	}
	return nullptr;
}

AGameXXKTownExitActor* AGameXXKMVPGameMode::EnsureTownExit()
{
	if (SpawnedTownExit)
	{
		return SpawnedTownExit;
	}

	if (AGameXXKTownExitActor* ExistingTownExit = FindExistingTownExit())
	{
		SpawnedTownExit = ExistingTownExit;
		return ExistingTownExit;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.Name = QingshanTownExitActorName;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	SpawnedTownExit = World->SpawnActor<AGameXXKTownExitActor>(AGameXXKTownExitActor::StaticClass(), GetConfiguredTownExitLocation(), FRotator::ZeroRotator, SpawnParameters);
#if WITH_EDITOR
	if (SpawnedTownExit)
	{
		SpawnedTownExit->SetActorLabel(QingshanTownExitActorName.ToString());
	}
#endif
	return SpawnedTownExit;
}

AGameXXKTownExitActor* AGameXXKMVPGameMode::FindExistingTownExit() const
{
	UWorld* World = GetWorld();
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<AGameXXKTownExitActor> It(World); It; ++It)
	{
		return *It;
	}
	return nullptr;
}
