#include "GameXXKCharacterStatRules.h"

namespace
{
	struct FCompanionStatDefinition
	{
		int32 BaseHealth = 0;
		float HealthGrowth = 0.0f;
		int32 BaseMana = 0;
		float ManaGrowth = 0.0f;
		int32 BaseAttack = 0;
		float AttackGrowth = 0.0f;
		int32 BaseDefense = 0;
		float DefenseGrowth = 0.0f;
		int32 BaseSpeed = 0;
	};

	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
	}

	bool TryGetCompanionStatDefinition(const EGameXXKCharacterRole Role, FCompanionStatDefinition& OutDefinition)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Blade:
			OutDefinition = {92, 9.0f, 22, 1.0f, 17, 2.0f, 6, 0.7f, 11};
			return true;
		case EGameXXKCharacterRole::Guard:
			OutDefinition = {120, 12.0f, 18, 1.0f, 11, 1.0f, 12, 1.2f, 8};
			return true;
		case EGameXXKCharacterRole::Healer:
			OutDefinition = {90, 8.0f, 30, 2.0f, 10, 1.0f, 7, 0.7f, 9};
			return true;
		case EGameXXKCharacterRole::Hunter:
			OutDefinition = {86, 8.0f, 24, 1.0f, 16, 2.0f, 6, 0.6f, 13};
			return true;
		case EGameXXKCharacterRole::Sorcerer:
			OutDefinition = {80, 7.0f, 34, 2.0f, 15, 1.5f, 5, 0.5f, 9};
			return true;
		case EGameXXKCharacterRole::FormationMaster:
			OutDefinition = {94, 9.0f, 30, 2.0f, 12, 1.2f, 8, 0.8f, 10};
			return true;
		default:
			return false;
		}
	}

	int32 ComputeProgressedAttribute(
		const int32 BaseValue,
		const float GrowthValue,
		const int32 Level,
		const float StarMultiplier)
	{
		const float PreStarValue = static_cast<float>(BaseValue) + GrowthValue * static_cast<float>(Level - 1);
		return FMath::FloorToInt(PreStarValue * StarMultiplier);
	}
}

FGameXXKCharacterStats FGameXXKCharacterStatRules::GetBareHeroStats(const int32 Level)
{
	const int32 ClampedLevel = FMath::Clamp(Level, 1, MaxCharacterLevel);
	const int32 LevelOffset = ClampedLevel - 1;
	FGameXXKCharacterStats Stats;
	Stats.MaxHealth = 100 + LevelOffset * 15;
	Stats.MaxMana = 30;
	Stats.Attack = 15 + LevelOffset * 3;
	Stats.Defense = 8 + LevelOffset * 2;
	Stats.Speed = 10 + LevelOffset;
	return Stats;
}

bool FGameXXKCharacterStatRules::GetBareCompanionStats(
	const EGameXXKCharacterRole Role,
	const int32 Level,
	const int32 Star,
	FGameXXKCharacterStats& OutStats,
	FString* OutError)
{
	OutStats = FGameXXKCharacterStats();
	FCompanionStatDefinition Definition;
	if (Star < 1 || Star > 5 || !TryGetCompanionStatDefinition(Role, Definition))
	{
		SetError(OutError, TEXT("Companion attributes require a permanent role and star 1-5."));
		return false;
	}

	const int32 ClampedLevel = FMath::Clamp(Level, 1, MaxCharacterLevel);
	const float StarMultiplier = 1.0f + 0.08f * static_cast<float>(Star - 1);
	OutStats.MaxHealth = ComputeProgressedAttribute(Definition.BaseHealth, Definition.HealthGrowth, ClampedLevel, StarMultiplier);
	OutStats.MaxMana = Role == EGameXXKCharacterRole::Sorcerer
		? Definition.BaseMana
		: ComputeProgressedAttribute(Definition.BaseMana, Definition.ManaGrowth, ClampedLevel, StarMultiplier);
	OutStats.Attack = ComputeProgressedAttribute(Definition.BaseAttack, Definition.AttackGrowth, ClampedLevel, StarMultiplier);
	OutStats.Defense = ComputeProgressedAttribute(Definition.BaseDefense, Definition.DefenseGrowth, ClampedLevel, StarMultiplier);
	const int32 PreStarSpeed = Definition.BaseSpeed + (ClampedLevel - 1) / 5;
	OutStats.Speed = FMath::FloorToInt(static_cast<float>(PreStarSpeed) * StarMultiplier);
	return true;
}
