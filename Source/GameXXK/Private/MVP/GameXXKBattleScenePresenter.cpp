#include "MVP/GameXXKBattleScenePresenter.h"

#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "MVP/GameXXKMVPSubsystem.h"

AGameXXKBattleScenePresenter::AGameXXKBattleScenePresenter()
{
	PrimaryActorTick.bCanEverTick = false;
	SceneRoot = CreateDefaultSubobject<USceneComponent>(TEXT("SceneRoot"));
	SetRootComponent(SceneRoot);
	UnitActorClass = AGameXXKBattleSceneUnitActor::StaticClass();
}

void AGameXXKBattleScenePresenter::BeginPlay()
{
	Super::BeginPlay();
	EnsureBattleScene();
}

bool AGameXXKBattleScenePresenter::EnsureBattleScene()
{
	UGameXXKMVPSubsystem* Subsystem = ResolveMVPSubsystem();
	if (!Subsystem || Subsystem->GetRuntimeState().Screen != EGameXXKScreen::Battle || !Subsystem->GetRuntimeState().bHasActiveBattle)
	{
		return false;
	}

	UWorld* World = GetWorld();
	if (!World || !World->IsGameWorld())
	{
		return false;
	}

	ClearSpawnedUnits();

	const TArray<FGameXXKBattleSceneUnitPlacement> Placements = BuildUnitPlacementsForState(Subsystem->GetRuntimeState());
	UClass* SpawnClass = UnitActorClass ? UnitActorClass.Get() : AGameXXKBattleSceneUnitActor::StaticClass();
	for (const FGameXXKBattleSceneUnitPlacement& Placement : Placements)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Owner = this;
		AGameXXKBattleSceneUnitActor* UnitActor = World->SpawnActor<AGameXXKBattleSceneUnitActor>(
			SpawnClass,
			Placement.Location,
			FRotator::ZeroRotator,
			SpawnParameters);
		if (!UnitActor)
		{
			continue;
		}

		const TArray<FGameXXKBattleRuntimeUnit>& Units = Placement.bEnemy
			? Subsystem->GetRuntimeState().ActiveBattleEnemies
			: Subsystem->GetRuntimeState().ActiveBattleParty;
		if (Units.IsValidIndex(Placement.UnitIndex))
		{
#if WITH_EDITOR
			UnitActor->SetActorLabel(FString::Printf(TEXT("GameXXK_Battle_%s_%d"), Placement.bEnemy ? TEXT("Enemy") : TEXT("Party"), Placement.UnitIndex));
#endif
			UnitActor->SetMVPSubsystemForTest(Subsystem);
			UnitActor->ConfigureFromRuntimeUnit(Placement.bEnemy, Placement.UnitIndex, Units[Placement.UnitIndex]);
			SpawnedUnitObjects.Add(UnitActor);
		}
	}

	return SpawnedUnitObjects.Num() > 0;
}

TArray<AGameXXKBattleSceneUnitActor*> AGameXXKBattleScenePresenter::GetSpawnedUnitsForTest() const
{
	TArray<AGameXXKBattleSceneUnitActor*> Result;
	Result.Reserve(SpawnedUnitObjects.Num());
	for (const TObjectPtr<AGameXXKBattleSceneUnitActor>& UnitActor : SpawnedUnitObjects)
	{
		if (UnitActor)
		{
			Result.Add(UnitActor.Get());
		}
	}
	return Result;
}

void AGameXXKBattleScenePresenter::SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem)
{
	OverrideSubsystem = InSubsystem;
}

TArray<FGameXXKBattleSceneUnitPlacement> AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(const FGameXXKRuntimeState& State)
{
	TArray<FGameXXKBattleSceneUnitPlacement> Placements;
	const auto AddPlacements = [&Placements](const TArray<FGameXXKBattleRuntimeUnit>& Units, bool bEnemy)
	{
		const int32 Count = FMath::Min(Units.Num(), 3);
		const float SideY = bEnemy ? -220.0f : 260.0f;
		const float DepthCenterX = -70.0f;
		const float DepthSpacing = 120.0f;
		const float StartX = DepthCenterX - DepthSpacing * static_cast<float>(Count - 1) * 0.5f;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKBattleSceneUnitPlacement Placement;
			Placement.bEnemy = bEnemy;
			Placement.UnitIndex = Index;
			Placement.UnitId = Units[Index].Id;
			Placement.Location = FVector(StartX + DepthSpacing * Index, SideY, 90.0f);
			Placements.Add(Placement);
		}
	};

	if (State.Screen == EGameXXKScreen::Battle && State.bHasActiveBattle)
	{
		AddPlacements(State.ActiveBattleEnemies, true);
		AddPlacements(State.ActiveBattleParty, false);
	}
	return Placements;
}

UGameXXKMVPSubsystem* AGameXXKBattleScenePresenter::ResolveMVPSubsystem() const
{
	if (OverrideSubsystem)
	{
		return OverrideSubsystem;
	}

	const UWorld* World = GetWorld();
	UGameInstance* GameInstance = World ? World->GetGameInstance() : nullptr;
	return GameInstance ? GameInstance->GetSubsystem<UGameXXKMVPSubsystem>() : nullptr;
}

void AGameXXKBattleScenePresenter::ClearSpawnedUnits()
{
	for (TObjectPtr<AGameXXKBattleSceneUnitActor>& UnitActor : SpawnedUnitObjects)
	{
		if (UnitActor)
		{
			UnitActor->Destroy();
		}
	}
	SpawnedUnitObjects.Reset();
}
