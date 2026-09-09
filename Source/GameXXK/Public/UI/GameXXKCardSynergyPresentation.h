#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

enum class EGameXXKCardSynergyKind : uint8
{
	FormulaOpening, MedicineReady, SpellTask, BladeOpening, BladeFinishCandidate,
	BladeFollowup, HeavyArrow, ArmorConversion, Terrain, TerrainEnhanced, DotDetonation
};

struct FGameXXKCardSynergyCue
{
	EGameXXKCardSynergyKind Kind = EGameXXKCardSynergyKind::Terrain;
	FLinearColor Color = FLinearColor::White;
	FString Reason;
	float Strength = 1.0f;
	int32 Progress = 0;
	int32 Total = 0;
};

/** Read-only presentation of real card mechanics. Never resolves a play or changes the save. */
namespace GameXXKCardSynergyPresentation
{
	GAMEXXK_API TArray<FGameXXKCardSynergyCue> Build(const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardInstance& Card, const FGameXXKCardDefinition& Definition,
		const FGameXXKCardPlayPreview* Preview = nullptr);
	GAMEXXK_API FString FinisherHint(const FGameXXKCardBattleRuntime& Runtime);
	GAMEXXK_API FLinearColor Color(EGameXXKCardSynergyKind Kind);
	GAMEXXK_API FString Describe(const TArray<FGameXXKCardSynergyCue>& Cues);
}
