#include "MVP/GameXXKMVPGameMode.h"

#include "MVP/GameXXKMVPPlayerController.h"
#include "Town/GameXXKTownPlayerPawn.h"
#include "UI/GameXXKMVPHUD.h"

AGameXXKMVPGameMode::AGameXXKMVPGameMode()
{
	PlayerControllerClass = AGameXXKMVPPlayerController::StaticClass();
	HUDClass = AGameXXKMVPHUD::StaticClass();
	DefaultPawnClass = AGameXXKTownPlayerPawn::StaticClass();
	TownNpcClass = AGameXXKTownNpcActor::StaticClass();
}

void AGameXXKMVPGameMode::BeginPlay()
{
	Super::BeginPlay();

	if (SpawnedTownNpcs.Num() > 0)
	{
		return;
	}

	SpawnTownNpc(EGameXXKTownNpcRole::Quest, FVector(260.0f, -120.0f, 120.0f));
	SpawnTownNpc(EGameXXKTownNpcRole::Merchant, FVector(260.0f, 120.0f, 120.0f));
	SpawnTownNpc(EGameXXKTownNpcRole::Follower, FVector(100.0f, -220.0f, 120.0f));
}

int32 AGameXXKMVPGameMode::GetSpawnedTownNpcCount() const
{
	return SpawnedTownNpcs.Num();
}

int32 AGameXXKMVPGameMode::GetConfiguredTownNpcCount() const
{
	return 3;
}

void AGameXXKMVPGameMode::SpawnTownNpc(EGameXXKTownNpcRole NpcRole, const FVector& Location)
{
	UWorld* World = GetWorld();
	if (!World || !TownNpcClass)
	{
		return;
	}

	FActorSpawnParameters SpawnParameters;
	SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKTownNpcActor* Npc = World->SpawnActor<AGameXXKTownNpcActor>(TownNpcClass, Location, FRotator::ZeroRotator, SpawnParameters);
	if (Npc)
	{
		Npc->SetNpcRole(NpcRole);
		SpawnedTownNpcs.Add(Npc);
	}
}
