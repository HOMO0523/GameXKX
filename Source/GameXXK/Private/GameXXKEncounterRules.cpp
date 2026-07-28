#include "GameXXKEncounterRules.h"

namespace
{
	bool SetFailure(FString* OutError, const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	uint32 MixSeed(const uint32 Value)
	{
		uint32 Mixed = Value;
		Mixed ^= Mixed >> 16;
		Mixed *= 0x7FEB352Du;
		Mixed ^= Mixed >> 15;
		Mixed *= 0x846CA68Bu;
		Mixed ^= Mixed >> 16;
		return Mixed;
	}

	int32 MakeLocalFormationSeed(const int32 ChapterSeed, const int32 NodeId, const EGameXXKNodeKind NodeKind)
	{
		uint32 Mixed = static_cast<uint32>(ChapterSeed);
		Mixed ^= 0x9E3779B9u * static_cast<uint32>(NodeId);
		Mixed ^= 0x85EBCA6Bu * (static_cast<uint32>(NodeKind) + 1u);
		Mixed = MixSeed(Mixed);
		return static_cast<int32>(Mixed == 0u ? 0x6D2B79F5u : Mixed);
	}

	bool SelectWithoutReplacement(
		const TArray<FName>& Pool,
		const int32 Count,
		FRandomStream& Stream,
		TArray<FName>& OutSelection,
		FString* OutError)
	{
		if (Count < 0 || Pool.Num() < Count)
		{
			return SetFailure(OutError, TEXT("The requested encounter selection exceeds its chapter pool."));
		}

		TArray<FName> Candidates = Pool;
		OutSelection.Reset();
		OutSelection.Reserve(Count);
		for (int32 Index = 0; Index < Count; ++Index)
		{
			const int32 ChoiceIndex = Stream.RandRange(0, Candidates.Num() - 1);
			OutSelection.Add(Candidates[ChoiceIndex]);
			Candidates.RemoveAtSwap(ChoiceIndex, 1, EAllowShrinking::No);
		}
		return true;
	}

	void AddSlot(
		TArray<FGameXXKEncounterSlot>& InOutSlots,
		const FName DefinitionId,
		const int32 BattleSlotNumber,
		const int32 RouteCombatLevel)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(DefinitionId);
		FGameXXKEncounterSlot Slot;
		Slot.EnemyDefinitionId = DefinitionId;
		Slot.BattleSlotNumber = BattleSlotNumber;
		Slot.CombatLevel = Definition
			? FGameXXKEncounterRules::GetCombatLevel(Definition->Tier, RouteCombatLevel)
			: 1;
		InOutSlots.Add(Slot);
	}
}

int32 FGameXXKEncounterRules::DeriveChapterSeed(const int32 RootSeed, const int32 Chapter)
{
	if (Chapter < 1 || Chapter > 3)
	{
		return 0;
	}
	const uint32 Mixed = MixSeed(static_cast<uint32>(RootSeed) ^ (0x9E3779B9u * static_cast<uint32>(Chapter)));
	return static_cast<int32>(Mixed == 0u ? 0x6D2B79F5u : Mixed);
}

int32 FGameXXKEncounterRules::GetCombatLevel(const EGameXXKEnemyTier Tier, const int32 RouteCombatLevel)
{
	const int32 Snapshot = FMath::Clamp(RouteCombatLevel, 1, 20);
	switch (Tier)
	{
	case EGameXXKEnemyTier::Normal:
		return Snapshot;
	case EGameXXKEnemyTier::Elite:
		return FMath::Min(Snapshot + 1, 20);
	case EGameXXKEnemyTier::Boss:
		return FMath::Min(Snapshot + 2, 20);
	default:
		return Snapshot;
	}
}

FGameXXKEncounterStatScale FGameXXKEncounterRules::GetAuthoredStatScale(const int32 Chapter, const EGameXXKNodeKind NodeKind)
{
	FGameXXKEncounterStatScale Scale;
	if (Chapter < 1 || Chapter > 3)
	{
		return Scale;
	}

	if (NodeKind == EGameXXKNodeKind::Battle)
	{
		Scale.MaxHPPercent = 140;
		Scale.AttackPercent = 250;
	}
	else if (NodeKind == EGameXXKNodeKind::Elite)
	{
		Scale.MaxHPPercent = 160;
		Scale.AttackPercent = Chapter == 1 ? 270 : Chapter == 2 ? 170 : 180;
		Scale.DefensePercent = Chapter == 1 ? 100 : Chapter == 2 ? 105 : 110;
	}
	else if (NodeKind == EGameXXKNodeKind::Boss)
	{
		Scale.MaxHPPercent = Chapter == 1 ? 120 : Chapter == 2 ? 100 : 80;
		Scale.AttackPercent = Chapter == 1 ? 120 : Chapter == 2 ? 100 : 90;
	}
	return Scale;
}

int32 FGameXXKEncounterRules::ScaleStat(const int32 Value, const int32 Percent, const int32 Minimum)
{
	const int32 SafePercent = FMath::Clamp(Percent, 1, 1000);
	const int64 ScaledValue = (static_cast<int64>(Value) * SafePercent + 50) / 100;
	return FMath::Max(Minimum, static_cast<int32>(FMath::Min<int64>(ScaledValue, MAX_int32)));
}

bool FGameXXKEncounterRules::BuildFormation(
	const int32 Chapter,
	const EGameXXKNodeKind NodeKind,
	const int32 ChapterSeed,
	const int32 NodeId,
	const int32 RouteCombatLevel,
	TArray<FGameXXKEncounterSlot>& OutSlots,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (Chapter < 1 || Chapter > 3)
	{
		return SetFailure(OutError, TEXT("Encounter formation requires a chapter from one through three."));
	}
	if (NodeKind != EGameXXKNodeKind::Battle && NodeKind != EGameXXKNodeKind::Elite && NodeKind != EGameXXKNodeKind::Boss)
	{
		return SetFailure(OutError, TEXT("Encounter formation requires a battle, elite, or boss route node."));
	}

	const TArray<FName> NormalPool = FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Normal);
	const TArray<FName> ElitePool = FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Elite);
	const TArray<FName> BossPool = FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Boss);
	if (NormalPool.Num() != 4 || ElitePool.Num() != 2 || BossPool.Num() != 1)
	{
		return SetFailure(OutError, TEXT("Encounter formation requires the complete chapter enemy pools."));
	}

	FRandomStream Stream(MakeLocalFormationSeed(ChapterSeed, NodeId, NodeKind));
	TArray<FGameXXKEncounterSlot> NewSlots;
	NewSlots.Reserve(NodeKind == EGameXXKNodeKind::Battle ? 2 : 3);
	TArray<FName> SelectedNormals;
	if (!SelectWithoutReplacement(NormalPool, NodeKind == EGameXXKNodeKind::Battle ? 2 : 2, Stream, SelectedNormals, OutError))
	{
		return false;
	}

	if (NodeKind == EGameXXKNodeKind::Battle)
	{
		AddSlot(NewSlots, SelectedNormals[0], 1, RouteCombatLevel);
		AddSlot(NewSlots, SelectedNormals[1], 3, RouteCombatLevel);
	}
	else if (NodeKind == EGameXXKNodeKind::Elite)
	{
		TArray<FName> SelectedElite;
		if (!SelectWithoutReplacement(ElitePool, 1, Stream, SelectedElite, OutError))
		{
			return false;
		}
		AddSlot(NewSlots, SelectedNormals[0], 1, RouteCombatLevel);
		AddSlot(NewSlots, SelectedElite[0], 2, RouteCombatLevel);
		AddSlot(NewSlots, SelectedNormals[1], 3, RouteCombatLevel);
	}
	else
	{
		AddSlot(NewSlots, ElitePool[0], 1, RouteCombatLevel);
		AddSlot(NewSlots, BossPool[0], 2, RouteCombatLevel);
		AddSlot(NewSlots, ElitePool[1], 3, RouteCombatLevel);
	}

	NewSlots.Sort([](const FGameXXKEncounterSlot& Left, const FGameXXKEncounterSlot& Right)
	{
		return Left.BattleSlotNumber < Right.BattleSlotNumber;
	});
	for (const FGameXXKEncounterSlot& Slot : NewSlots)
	{
		if (Slot.EnemyDefinitionId.IsNone() || Slot.BattleSlotNumber < 1 || Slot.BattleSlotNumber > 3)
		{
			return SetFailure(OutError, TEXT("Encounter formation generated an invalid slot."));
		}
	}

	OutSlots = MoveTemp(NewSlots);
	return true;
}
