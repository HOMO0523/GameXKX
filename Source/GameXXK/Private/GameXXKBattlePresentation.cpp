#include "GameXXKBattlePresentation.h"

namespace
{
	bool IsPermanentCompanionRole(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade:
		case EGameXXKCharacterRole::Guard:
		case EGameXXKCharacterRole::Healer:
		case EGameXXKCharacterRole::Hunter:
		case EGameXXKCharacterRole::Sorcerer:
		case EGameXXKCharacterRole::FormationMaster:
			return true;
		default:
			return false;
		}
	}

	int32 GetUnitSlotNumber(const FGameXXKCardCombatUnit& Unit)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Party)
		{
			if (Unit.Role == EGameXXKCharacterRole::Hero)
			{
				return 2;
			}
			if (IsPermanentCompanionRole(Unit.Role))
			{
				return 1;
			}
			return Unit.Role == EGameXXKCharacterRole::QuestNpc ? 3 : INDEX_NONE;
		}

		if (Unit.Side != EGameXXKCardTargetSide::Enemy)
		{
			return INDEX_NONE;
		}
		if (Unit.BattleSlotNumber >= 1 && Unit.BattleSlotNumber <= 3)
		{
			return Unit.BattleSlotNumber;
		}
		return Unit.StableSortOrder >= 0 && Unit.StableSortOrder <= 2
			? Unit.StableSortOrder + 1
			: INDEX_NONE;
	}
}

int32 FGameXXKBattlePresentation::GetSlotNumber(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
{
	if (UnitId.IsNone())
	{
		return INDEX_NONE;
	}

	const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
	{
		return Candidate.UnitId == UnitId;
	});
	return Unit ? GetUnitSlotNumber(*Unit) : INDEX_NONE;
}

bool FGameXXKBattlePresentation::BuildUnitHudView(
	const FGameXXKCardBattleRuntime& Runtime,
	const FName UnitId,
	const FText& DisplayName,
	FGameXXKBattleUnitHudView& OutView)
{
	OutView = FGameXXKBattleUnitHudView();
	if (UnitId.IsNone())
	{
		return false;
	}

	const FGameXXKCardCombatUnit* const Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
	{
		return Candidate.UnitId == UnitId;
	});
	if (!Unit)
	{
		return false;
	}

	OutView.UnitId = Unit->UnitId;
	OutView.Side = Unit->Side;
	OutView.Role = Unit->Role;
	OutView.DisplayName = DisplayName;
	OutView.SlotNumber = GetSlotNumber(Runtime, UnitId);
	OutView.bLiving = Unit->bLiving;
	OutView.bShowMana = Unit->Side == EGameXXKCardTargetSide::Party;
	OutView.CurrentHP = Unit->HP;
	OutView.MaxHP = Unit->MaxHP;
	OutView.CurrentMana = Unit->Mana;
	OutView.MaxMana = Unit->MaxMana;
	OutView.Armor = Unit->Armor;
	if (Unit->Side == EGameXXKCardTargetSide::Enemy)
	{
		if (const FGameXXKEnemyBattleState* EnemyState = Runtime.EnemyStates.Find(Unit->UnitId))
		{
			OutView.CurrentEnemyPhase = EnemyState->CurrentPhase;
			OutView.TotalEnemyPhases = EnemyState->TotalPhases;
		}
	}
	OutView.Statuses = Unit->Statuses;
	return true;
}

FString FGameXXKBattlePresentation::FormatSlotLabel(const EGameXXKCardTargetSide Side, const int32 SlotNumber)
{
	if (SlotNumber < 1 || SlotNumber > 3)
	{
		return FString();
	}
	if (Side == EGameXXKCardTargetSide::Party)
	{
		return FString::Printf(TEXT("我 %dP"), SlotNumber);
	}
	if (Side == EGameXXKCardTargetSide::Enemy)
	{
		return FString::Printf(TEXT("敌 %dP"), SlotNumber);
	}
	return FString();
}

TArray<FGameXXKBattlePresentationSlot> FGameXXKBattlePresentation::BuildSlots(const FGameXXKCardBattleRuntime& Runtime)
{
	TArray<FGameXXKBattlePresentationSlot> Result;
	for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
	{
		const int32 SlotNumber = GetUnitSlotNumber(Unit);
		if (Unit.UnitId.IsNone() || SlotNumber == INDEX_NONE)
		{
			continue;
		}

		FGameXXKBattlePresentationSlot& Slot = Result.AddDefaulted_GetRef();
		Slot.UnitId = Unit.UnitId;
		Slot.Side = Unit.Side;
		Slot.SlotNumber = SlotNumber;
	}
	return Result;
}
