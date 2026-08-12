#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

struct FGameXXKRuntimeState;

enum class EGameXXKCardOutcomePreviewClass : uint8
{
	None,
	ManualUnit,
	PureEnemyGroup
};

enum class EGameXXKCardOutcomeTone : uint8
{
	Neutral,
	Damage,
	Dot,
	Medicine,
	Healing,
	Armor,
	Lethal
};

struct GAMEXXK_API FGameXXKCardOutcomeTextSegment
{
	FText Text;
	EGameXXKCardOutcomeTone Tone = EGameXXKCardOutcomeTone::Neutral;
};

struct GAMEXXK_API FGameXXKCardOutcomeTextLine
{
	TArray<FGameXXKCardOutcomeTextSegment> Segments;
};

struct GAMEXXK_API FGameXXKCardOutcomeTarget
{
	FName UnitId = NAME_None;
	EGameXXKCardTargetSide Side = EGameXXKCardTargetSide::Invalid;
	int32 SlotNumber = INDEX_NONE;
	int32 DirectDamage = 0;
	int32 GroupDamage = 0;
	int32 BleedDamage = 0;
	int32 PoisonDamage = 0;
	int32 BurnDamage = 0;
	int32 ToxicExplosionDamage = 0;
	int32 MedicineDamage = 0;
	int32 LinkedDamage = 0;
	int32 EffectiveHealing = 0;
	int32 EffectiveArmor = 0;
	bool bLethal = false;
	bool bAvoided = false;
	bool bRedirected = false;
};

struct GAMEXXK_API FGameXXKCardOutcomePreview
{
	FName CardInstanceId = NAME_None;
	FName HoveredTargetUnitId = NAME_None;
	EGameXXKCardOutcomePreviewClass Classification = EGameXXKCardOutcomePreviewClass::None;
	bool bSuccess = false;
	bool bUsesEnemyPositionList = false;
	FString FailureText;
	TOptional<FGameXXKCardOutcomeTarget> FocusedTarget;
	TArray<FGameXXKCardOutcomeTarget> EnemyPositionTargets;
	TArray<FGameXXKCardOutcomeTextLine> FocusedLines;
	TArray<FGameXXKCardOutcomeTextLine> EnemyPositionLines;
};

class GAMEXXK_API FGameXXKCardOutcomePreviewRules final
{
public:
	static bool Build(
		const FGameXXKRuntimeState& State,
		FName CardInstanceId,
		FName HoveredTargetUnitId,
		FGameXXKCardOutcomePreview& OutPreview,
		FString* OutError = nullptr);
};
