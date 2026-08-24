#include "GameXXKEquipmentTypes.h"

namespace
{
	int32 ValidatedRank(const uint8 Value)
	{
		const int32 Rank = static_cast<int32>(Value);
		return Rank >= FGameXXKEquipmentQualityRules::MinimumRank
			&& Rank <= FGameXXKEquipmentQualityRules::MaximumRank
			? Rank
			: 0;
	}
}

bool FGameXXKEquipmentQualityRules::IsValid(const EGameXXKEquipmentQuality Quality)
{
	return GetRank(Quality) != 0;
}

bool FGameXXKEquipmentQualityRules::IsValid(const EGameXXKAffixTier Tier)
{
	return GetRank(Tier) != 0;
}

int32 FGameXXKEquipmentQualityRules::GetRank(const EGameXXKEquipmentQuality Quality)
{
	return ValidatedRank(static_cast<uint8>(Quality));
}

int32 FGameXXKEquipmentQualityRules::GetRank(const EGameXXKAffixTier Tier)
{
	return ValidatedRank(static_cast<uint8>(Tier));
}

EGameXXKEquipmentQuality FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(const int32 Rank)
{
	return Rank >= MinimumRank && Rank <= MaximumRank
		? static_cast<EGameXXKEquipmentQuality>(Rank)
		: EGameXXKEquipmentQuality::Invalid;
}

EGameXXKAffixTier FGameXXKEquipmentQualityRules::AffixTierFromRank(const int32 Rank)
{
	return Rank >= MinimumRank && Rank <= MaximumRank
		? static_cast<EGameXXKAffixTier>(Rank)
		: EGameXXKAffixTier::Invalid;
}

FText FGameXXKEquipmentQualityRules::GetDisplayName(const EGameXXKEquipmentQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKEquipmentQuality::Common: return NSLOCTEXT("GameXXKEquipmentQuality", "Common", "普通");
	case EGameXXKEquipmentQuality::Rare: return NSLOCTEXT("GameXXKEquipmentQuality", "Rare", "稀有");
	case EGameXXKEquipmentQuality::Epic: return NSLOCTEXT("GameXXKEquipmentQuality", "Epic", "珍稀");
	case EGameXXKEquipmentQuality::Legendary: return NSLOCTEXT("GameXXKEquipmentQuality", "Legendary", "传奇");
	case EGameXXKEquipmentQuality::Immortal: return NSLOCTEXT("GameXXKEquipmentQuality", "Immortal", "不朽");
	case EGameXXKEquipmentQuality::Treasure: return NSLOCTEXT("GameXXKEquipmentQuality", "Treasure", "至宝");
	case EGameXXKEquipmentQuality::Transcendent: return NSLOCTEXT("GameXXKEquipmentQuality", "Transcendent", "超凡");
	case EGameXXKEquipmentQuality::Celestial: return NSLOCTEXT("GameXXKEquipmentQuality", "Celestial", "天界");
	case EGameXXKEquipmentQuality::Ascendant: return NSLOCTEXT("GameXXKEquipmentQuality", "Ascendant", "登神");
	case EGameXXKEquipmentQuality::Cosmic: return NSLOCTEXT("GameXXKEquipmentQuality", "Cosmic", "宇宙");
	default: return NSLOCTEXT("GameXXKEquipmentQuality", "Invalid", "未知");
	}
}

FText FGameXXKEquipmentQualityRules::GetDisplayName(const EGameXXKAffixTier Tier)
{
	return GetDisplayName(EquipmentQualityFromRank(GetRank(Tier)));
}

EGameXXKEquipmentQuality FGameXXKEquipmentQualityRules::GetNext(const EGameXXKEquipmentQuality Quality)
{
	const int32 Rank = GetRank(Quality);
	return Rank != 0 ? EquipmentQualityFromRank(Rank + 1) : EGameXXKEquipmentQuality::Invalid;
}

EGameXXKAffixTier FGameXXKEquipmentQualityRules::GetNext(const EGameXXKAffixTier Tier)
{
	const int32 Rank = GetRank(Tier);
	return Rank != 0 ? AffixTierFromRank(Rank + 1) : EGameXXKAffixTier::Invalid;
}

int32 FGameXXKEquipmentQualityRules::GetAffixCount(const EGameXXKEquipmentQuality Quality)
{
	const int32 Rank = GetRank(Quality);
	return Rank != 0 ? FMath::Min(Rank, MaximumAffixCount) : 0;
}
