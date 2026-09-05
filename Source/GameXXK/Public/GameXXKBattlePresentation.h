#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** One display-only battle slot. UnitId remains the authoritative gameplay identity. */
struct GAMEXXK_API FGameXXKBattlePresentationSlot
{
	FName UnitId = NAME_None;
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
	int32 SlotNumber = INDEX_NONE;
};

/** Display-only projection of one authoritative card combat unit for an ordinary UMG HUD child. */
struct GAMEXXK_API FGameXXKBattleUnitHudView
{
	FName UnitId = NAME_None;
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
	FText DisplayName;
	int32 SlotNumber = INDEX_NONE;
	bool bLiving = false;
	bool bShowMana = false;
	int32 CurrentHP = 0;
	int32 MaxHP = 0;
	int32 CurrentMana = 0;
	int32 MaxMana = 0;
	int32 Armor = 0;
	int32 CurrentEnemyPhase = 1;
	int32 TotalEnemyPhases = 1;
	TArray<FGameXXKCardStatusStack> Statuses;
};

/** Stateless mapping between authoritative combat units and the fixed player/enemy P slots. */
class GAMEXXK_API FGameXXKBattlePresentation final
{
public:
	/** Returns a display P-slot number for a stable UnitId, or INDEX_NONE when the unit cannot be displayed. */
	static int32 GetSlotNumber(const FGameXXKCardBattleRuntime& Runtime, FName UnitId);

	/** Formats a valid display P slot as 我 nP or 敌 nP; invalid inputs return an empty label. */
	static FString FormatSlotLabel(EGameXXKCardTargetSide Side, int32 SlotNumber);

	/** Builds the valid fixed display slots without changing combat order or gameplay identity. */
	static TArray<FGameXXKBattlePresentationSlot> BuildSlots(const FGameXXKCardBattleRuntime& Runtime);

	/** Builds a display-only HUD view from one authoritative card-runtime unit. */
	static bool BuildUnitHudView(
		const FGameXXKCardBattleRuntime& Runtime,
		FName UnitId,
		const FText& DisplayName,
		FGameXXKBattleUnitHudView& OutView);
};
