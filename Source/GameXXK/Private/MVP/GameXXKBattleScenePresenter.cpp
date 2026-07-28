#include "MVP/GameXXKBattleScenePresenter.h"

#include "GameXXKBattlePresentation.h"
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

	const TArray<FGameXXKBattleSceneUnitPlacement> Placements = BuildUnitPlacementsForStateAtAnchor(
		Subsystem->GetRuntimeState(),
		GetActorLocation());
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
		UnitActor->ConfigureFromRuntimeUnit(Placement.bEnemy, Placement.UnitIndex, Units[Placement.UnitIndex], Placement.SlotNumber);
			SpawnedUnitObjects.Add(UnitActor);
		}
	}

	return SpawnedUnitObjects.Num() > 0;
}

bool AGameXXKBattleScenePresenter::RefreshBattleScene()
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

	const TArray<FGameXXKBattleSceneUnitPlacement> Placements = BuildUnitPlacementsForStateAtAnchor(
		Subsystem->GetRuntimeState(),
		GetActorLocation());
	TArray<FName> CurrentUnitIds;
	for (const TObjectPtr<AGameXXKBattleSceneUnitActor>& Candidate : SpawnedUnitObjects)
	{
		if (Candidate && !Candidate->GetUnitId().IsNone())
		{
			CurrentUnitIds.Add(Candidate->GetUnitId());
		}
	}
	const TArray<FGameXXKBattleSceneUnitRefreshDecision> RefreshDecisions = BuildUnitRefreshDecisions(CurrentUnitIds, Placements);
	TSet<FName> RemoveUnitIds;
	for (const FGameXXKBattleSceneUnitRefreshDecision& Decision : RefreshDecisions)
	{
		if (Decision.Action == EGameXXKBattleSceneRefreshAction::Remove)
		{
			RemoveUnitIds.Add(Decision.UnitId);
		}
	}

	TMap<FName, AGameXXKBattleSceneUnitActor*> ExistingActorsById;
	for (TObjectPtr<AGameXXKBattleSceneUnitActor>& Candidate : SpawnedUnitObjects)
	{
		AGameXXKBattleSceneUnitActor* UnitActor = Candidate.Get();
		if (!UnitActor || UnitActor->GetUnitId().IsNone() || RemoveUnitIds.Contains(UnitActor->GetUnitId()))
		{
			if (UnitActor)
			{
				UnitActor->Destroy();
			}
			continue;
		}
		if (ExistingActorsById.Contains(UnitActor->GetUnitId()))
		{
			// A duplicate has no authoritative UnitId ownership.  Keep the first
			// retained actor and remove only this stale duplicate.
			UnitActor->Destroy();
			continue;
		}
		ExistingActorsById.Add(UnitActor->GetUnitId(), UnitActor);
	}

	TArray<TObjectPtr<AGameXXKBattleSceneUnitActor>> RefreshedActors;
	RefreshedActors.Reserve(Placements.Num());
	UClass* SpawnClass = UnitActorClass ? UnitActorClass.Get() : AGameXXKBattleSceneUnitActor::StaticClass();
	for (const FGameXXKBattleSceneUnitPlacement& Placement : Placements)
	{
		const TArray<FGameXXKBattleRuntimeUnit>& Units = Placement.bEnemy
			? Subsystem->GetRuntimeState().ActiveBattleEnemies
			: Subsystem->GetRuntimeState().ActiveBattleParty;
		if (!Units.IsValidIndex(Placement.UnitIndex))
		{
			continue;
		}

		AGameXXKBattleSceneUnitActor* UnitActor = ExistingActorsById.FindRef(Placement.UnitId);
		if (!UnitActor)
		{
			FActorSpawnParameters SpawnParameters;
			SpawnParameters.Owner = this;
			UnitActor = World->SpawnActor<AGameXXKBattleSceneUnitActor>(
				SpawnClass,
				Placement.Location,
				FRotator::ZeroRotator,
				SpawnParameters);
			if (!UnitActor)
			{
				continue;
			}
#if WITH_EDITOR
			UnitActor->SetActorLabel(FString::Printf(TEXT("GameXXK_Battle_%s_%d"), Placement.bEnemy ? TEXT("Enemy") : TEXT("Party"), Placement.UnitIndex));
#endif
		}

		UnitActor->SetActorLocation(Placement.Location);
		UnitActor->SetMVPSubsystemForTest(Subsystem);
		UnitActor->ConfigureFromRuntimeUnit(Placement.bEnemy, Placement.UnitIndex, Units[Placement.UnitIndex], Placement.SlotNumber);
		RefreshedActors.Add(UnitActor);
	}

	SpawnedUnitObjects = MoveTemp(RefreshedActors);
	return SpawnedUnitObjects.Num() > 0;
}

TArray<FGameXXKBattleSceneUnitRefreshDecision> AGameXXKBattleScenePresenter::BuildUnitRefreshDecisions(
	const TArray<FName>& CurrentUnitIds,
	const TArray<FGameXXKBattleSceneUnitPlacement>& NextPlacements)
{
	TArray<FGameXXKBattleSceneUnitRefreshDecision> Decisions;
	TSet<FName> CurrentIds;
	for (const FName UnitId : CurrentUnitIds)
	{
		if (!UnitId.IsNone())
		{
			CurrentIds.Add(UnitId);
		}
	}
	TSet<FName> NextIds;
	for (const FGameXXKBattleSceneUnitPlacement& Placement : NextPlacements)
	{
		if (Placement.UnitId.IsNone() || NextIds.Contains(Placement.UnitId))
		{
			continue;
		}
		NextIds.Add(Placement.UnitId);
		FGameXXKBattleSceneUnitRefreshDecision& Decision = Decisions.AddDefaulted_GetRef();
		Decision.UnitId = Placement.UnitId;
		Decision.Action = CurrentIds.Contains(Placement.UnitId)
			? EGameXXKBattleSceneRefreshAction::Retain
			: EGameXXKBattleSceneRefreshAction::Spawn;
	}
	TSet<FName> RemovedIds;
	for (const FName UnitId : CurrentUnitIds)
	{
		if (!UnitId.IsNone() && !NextIds.Contains(UnitId) && !RemovedIds.Contains(UnitId))
		{
			RemovedIds.Add(UnitId);
			FGameXXKBattleSceneUnitRefreshDecision& Decision = Decisions.AddDefaulted_GetRef();
			Decision.UnitId = UnitId;
			Decision.Action = EGameXXKBattleSceneRefreshAction::Remove;
		}
	}
	return Decisions;
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
	if (State.Screen != EGameXXKScreen::Battle || !State.bHasActiveBattle || !State.CardRun.bHasActiveCardBattle)
	{
		return Placements;
	}

	// The battle camera looks down the +X axis.  Keep the outer P lanes inside
	// its safe viewport so the foot HUD never competes with the hand or clips.
	// P1 remains outer, P2 middle, and P3 closest to the central card space.
	const FVector EnemySlotLocations[] = {
		FVector(-80.0f, -295.0f, 90.0f), FVector(-20.0f, -225.0f, 90.0f), FVector(40.0f, -155.0f, 90.0f)};
	const FVector PartySlotLocations[] = {
		FVector(-80.0f, 295.0f, 90.0f), FVector(-20.0f, 225.0f, 90.0f), FVector(40.0f, 155.0f, 90.0f)};
	TSet<int32> UsedEnemySlots;
	TSet<int32> UsedPartySlots;
	for (const FGameXXKBattlePresentationSlot& Slot : FGameXXKBattlePresentation::BuildSlots(State.CardRun.ActiveBattle))
	{
		const bool bEnemy = Slot.Side == EGameXXKCardTargetSide::Enemy;
		if ((!bEnemy && Slot.Side != EGameXXKCardTargetSide::Party) || Slot.SlotNumber < 1 || Slot.SlotNumber > 3)
		{
			continue;
		}
		TSet<int32>& UsedSlots = bEnemy ? UsedEnemySlots : UsedPartySlots;
		if (UsedSlots.Contains(Slot.SlotNumber))
		{
			continue;
		}

		const TArray<FGameXXKBattleRuntimeUnit>& Units = bEnemy ? State.ActiveBattleEnemies : State.ActiveBattleParty;
		const int32 UnitIndex = Units.IndexOfByPredicate([&Slot](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == Slot.UnitId;
		});
		if (UnitIndex == INDEX_NONE)
		{
			continue;
		}

		FGameXXKBattleSceneUnitPlacement& Placement = Placements.AddDefaulted_GetRef();
		Placement.bEnemy = bEnemy;
		Placement.UnitIndex = UnitIndex;
		Placement.UnitId = Slot.UnitId;
		Placement.SlotNumber = Slot.SlotNumber;
		Placement.Location = bEnemy ? EnemySlotLocations[Slot.SlotNumber - 1] : PartySlotLocations[Slot.SlotNumber - 1];
		UsedSlots.Add(Slot.SlotNumber);
	}
	return Placements;
}

TArray<FGameXXKBattleSceneUnitPlacement> AGameXXKBattleScenePresenter::BuildUnitPlacementsForStateAtAnchor(
	const FGameXXKRuntimeState& State,
	const FVector& SceneAnchor)
{
	TArray<FGameXXKBattleSceneUnitPlacement> Placements = BuildUnitPlacementsForState(State);
	for (FGameXXKBattleSceneUnitPlacement& Placement : Placements)
	{
		Placement.Location += SceneAnchor;
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
