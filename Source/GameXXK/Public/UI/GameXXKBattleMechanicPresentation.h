#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

enum class EGameXXKMechanicElement : uint8 { Universal, Fire, Ice, Lightning, Formula };

struct FGameXXKMechanicCardMark
{
	FName CardId;
	FString Name;
	FString Tooltip;
	int32 PlayOrder = 0;
};

struct FGameXXKMechanicTaskView
{
	bool bAvailable = false;
	bool bActive = false;
	bool bReplaying = false;
	EGameXXKMechanicElement Element = EGameXXKMechanicElement::Universal;
	FName StarterCardId;
	FString Title;
	FString Tooltip;
	TArray<FGameXXKMechanicCardMark> Cards;
	int32 CompletedCount() const;
};

struct FGameXXKMechanicFormulaView
{
	FName CardId;
	int32 Number = 0;
	bool bSpentThisRound = false;
	FString Title;
	FString Tooltip;
	FString TriggerState;
	EGameXXKCardQuality Quality = EGameXXKCardQuality::Common;
};

struct FGameXXKUnitMechanicView
{
	FName OwnerUnitId;
	bool bLiving = false;
	bool bReserveSpace = false;
	int32 RoundNumber = 0;
	int32 ActivePlayCount = 0;
	FName LastActiveCardId;
	FName LastActiveOwnerId;
	FGameXXKMechanicTaskView Task;
	TArray<FGameXXKMechanicFormulaView> Formulas;
	FString Signature() const;
	float LogicalHeight() const {return !bLiving || !bReserveSpace ? 0.0f : Task.bAvailable && !Formulas.IsEmpty() ? 68.0f : 34.0f;}
};

/** Read-only projection of task/formula records. No play, draw, reward or save mutation. */
namespace GameXXKBattleMechanicPresentation
{
	GAMEXXK_API FGameXXKUnitMechanicView Build(const FGameXXKCardBattleRuntime& Runtime, FName OwnerUnitId);
	GAMEXXK_API FString ElementName(EGameXXKMechanicElement Element);
	GAMEXXK_API FString MaterialPath(EGameXXKMechanicElement Element);
	GAMEXXK_API bool CaptureCompletion(const FGameXXKUnitMechanicView& Before,
		const FGameXXKUnitMechanicView& After, FGameXXKMechanicTaskView& OutCompleted);
}
