#include "GameXXKGemRules.h"

namespace
{
	const TCHAR* TypeToken(const EGameXXKGemType Type)
	{
		switch (Type)
		{
		case EGameXXKGemType::Attack: return TEXT("Attack");
		case EGameXXKGemType::Defense: return TEXT("Defense");
		case EGameXXKGemType::MaxHealth: return TEXT("MaxHealth");
		case EGameXXKGemType::AttackPercent: return TEXT("AttackPercent");
		case EGameXXKGemType::DefensePercent: return TEXT("DefensePercent");
		case EGameXXKGemType::MaxHealthPercent: return TEXT("MaxHealthPercent");
		case EGameXXKGemType::DirectDamage: return TEXT("DirectDamage");
		case EGameXXKGemType::ArmorGain: return TEXT("ArmorGain");
		case EGameXXKGemType::Healing: return TEXT("Healing");
		case EGameXXKGemType::CounterDamage: return TEXT("CounterDamage");
		case EGameXXKGemType::FireDamage: return TEXT("FireDamage");
		case EGameXXKGemType::DamageOverTime: return TEXT("DamageOverTime");
		case EGameXXKGemType::FrostDamage: return TEXT("FrostDamage");
		case EGameXXKGemType::LightningDamage: return TEXT("LightningDamage");
		case EGameXXKGemType::FireResistance: return TEXT("FireResistance");
		case EGameXXKGemType::FrostResistance: return TEXT("FrostResistance");
		case EGameXXKGemType::LightningResistance: return TEXT("LightningResistance");
		default: return nullptr;
		}
	}

	const TCHAR* QualityToken(const EGameXXKGemQuality Quality)
	{
		switch (Quality)
		{
		case EGameXXKGemQuality::Common: return TEXT("Common");
		case EGameXXKGemQuality::Rare: return TEXT("Rare");
		case EGameXXKGemQuality::Epic: return TEXT("Epic");
		case EGameXXKGemQuality::Legendary: return TEXT("Legendary");
		case EGameXXKGemQuality::Immortal: return TEXT("Immortal");
		case EGameXXKGemQuality::Treasure: return TEXT("Treasure");
		case EGameXXKGemQuality::Transcendent: return TEXT("Transcendent");
		case EGameXXKGemQuality::Celestial: return TEXT("Celestial");
		case EGameXXKGemQuality::Ascendant: return TEXT("Ascendant");
		case EGameXXKGemQuality::Cosmic: return TEXT("Cosmic");
		default: return nullptr;
		}
	}

	EGameXXKGemType TypeFromToken(const FString& Token)
	{
		for (int32 Rank = 1; Rank <= FGameXXKGemRules::MaximumTypeRank; ++Rank)
		{
			const auto Type = static_cast<EGameXXKGemType>(Rank);
			if (Token == TypeToken(Type)) return Type;
		}
		return EGameXXKGemType::Invalid;
	}

	EGameXXKGemQuality QualityFromToken(const FString& Token)
	{
		for (int32 Rank = FGameXXKGemRules::MinimumQualityRank; Rank <= FGameXXKGemRules::MaximumQualityRank; ++Rank)
		{
			const EGameXXKGemQuality Quality = FGameXXKGemRules::QualityFromRank(Rank);
			if (const TCHAR* StableToken = QualityToken(Quality); Token == StableToken)
			{
				return Quality;
			}
		}
		return EGameXXKGemQuality::Invalid;
	}
}

bool FGameXXKGemRules::IsValidType(const EGameXXKGemType Type)
{
	return Type >= EGameXXKGemType::Attack && Type <= EGameXXKGemType::LightningResistance;
}

bool FGameXXKGemRules::IsValidQuality(const EGameXXKGemQuality Quality)
{
	return GetQualityRank(Quality) >= MinimumQualityRank;
}

bool FGameXXKGemRules::IsValidSocketValue(const FGameXXKSocketedGem& Gem)
{
	return Gem.IsEmpty() || (IsValidType(Gem.Type) && IsValidQuality(Gem.Quality));
}

int32 FGameXXKGemRules::GetQualityRank(const EGameXXKGemQuality Quality)
{
	const int32 Rank = static_cast<int32>(Quality);
	return Rank >= MinimumQualityRank && Rank <= MaximumQualityRank ? Rank : 0;
}

EGameXXKGemQuality FGameXXKGemRules::QualityFromRank(const int32 Rank)
{
	return Rank >= MinimumQualityRank && Rank <= MaximumQualityRank
		? static_cast<EGameXXKGemQuality>(Rank)
		: EGameXXKGemQuality::Invalid;
}

EGameXXKGemQuality FGameXXKGemRules::GetNextQuality(const EGameXXKGemQuality Quality)
{
	const int32 Rank = GetQualityRank(Quality);
	return Rank >= MinimumQualityRank && Rank < MaximumQualityRank
		? QualityFromRank(Rank + 1)
		: EGameXXKGemQuality::Invalid;
}

int32 FGameXXKGemRules::GetSocketCapacity(const EGameXXKEquipmentQuality EquipmentQuality)
{
	const int32 Rank = FGameXXKEquipmentQualityRules::GetRank(EquipmentQuality);
	return Rank > 0 ? 1 + FMath::Max(0, Rank - 5) : 0;
}

int32 FGameXXKGemRules::GetStatBonus(const EGameXXKGemType Type, const EGameXXKGemQuality Quality)
{
	const int32 Rank = GetQualityRank(Quality);
	if (!IsValidType(Type) || IsPercentType(Type) || Rank == 0)
	{
		return 0;
	}
	// docs/design/2026-09-09-gem-quality-marginal-curve.md
	// After Immortal the upgrade multiplier declines evenly from 2 to 1.25,
	// rounding up each tier. Multiple gems still add normally.
	static constexpr int32 QualityBonuses[] = {1, 2, 4, 8, 16, 32, 58, 95, 137, 172};
	static_assert(UE_ARRAY_COUNT(QualityBonuses) == MaximumQualityRank);
	const int32 Bonus = QualityBonuses[Rank - 1];
	return Type == EGameXXKGemType::MaxHealth ? Bonus * 5 : Bonus;
}

bool FGameXXKGemRules::IsPercentType(const EGameXXKGemType Type)
{
	return IsValidType(Type) && Type >= EGameXXKGemType::AttackPercent;
}

bool FGameXXKGemRules::IsResistanceType(const EGameXXKGemType Type)
{
	return Type >= EGameXXKGemType::FireResistance && Type <= EGameXXKGemType::LightningResistance;
}

int32 FGameXXKGemRules::GetBonusBasisPoints(const EGameXXKGemType Type, const EGameXXKGemQuality Quality)
{
	if (!IsPercentType(Type) || !IsValidQuality(Quality)) return 0;
	return GetStatBonus(EGameXXKGemType::Attack, Quality)
		* (Type <= EGameXXKGemType::MaxHealthPercent ? StatPercentBasisPointsPerStep : MechanicBasisPointsPerStep);
}

double FGameXXKGemRules::GetEffectiveBonusBasisPoints(const int64 RawBasisPoints)
{
	const int64 Raw = FMath::Clamp<int64>(RawBasisPoints, 0, MAX_int32);
	return static_cast<double>(BonusCapBasisPoints) * Raw / (BonusCapBasisPoints + Raw);
}

int32 FGameXXKGemRules::ApplyBonus(const int32 Base, const int64 RawBasisPoints, const EGameXXKGemRounding Rounding)
{
	if (Base <= 0) return FMath::Max(0, Base);
	const int64 Raw = FMath::Clamp<int64>(RawBasisPoints, 0, MAX_int32);
	const int64 Denominator = BonusCapBasisPoints + Raw;
	const int64 Numerator = static_cast<int64>(BonusCapBasisPoints) * Raw;
	const int64 RemainderProduct = static_cast<int64>(Base) * (Numerator % Denominator);
	const int64 Scaled = static_cast<int64>(Base) * (10000 + Numerator / Denominator) + RemainderProduct / Denominator;
	int64 Result = Scaled / 10000;
	if (Rounding == EGameXXKGemRounding::Nearest) Result = (Scaled + 5000) / 10000;
	else if (Rounding == EGameXXKGemRounding::Up && (Scaled % 10000 != 0 || RemainderProduct % Denominator != 0)) ++Result;
	return static_cast<int32>(FMath::Min<int64>(Result, MAX_int32));
}

FText FGameXXKGemRules::GetBonusText(const EGameXXKGemType Type, const EGameXXKGemQuality Quality)
{
	if (!IsValidType(Type) || !IsValidQuality(Quality)) return FText::GetEmpty();
	return FText::FromString(IsPercentType(Type)
		? FString::Printf(TEXT("+%.2f%%"), GetBonusBasisPoints(Type,Quality) / 100.0)
		: FString::Printf(TEXT("+%d"), GetStatBonus(Type,Quality)));
}

EGameXXKEquipmentQuality FGameXXKGemRules::GetPresentationQuality(const EGameXXKGemQuality Quality)
{
	return FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(GetQualityRank(Quality));
}

EGameXXKEquipmentQuality FGameXXKGemRules::GetItemPresentationQuality(FName ItemId)
{
	EGameXXKGemType Type;
	EGameXXKGemQuality Quality;
	return TryParseItemId(ItemId, Type, Quality) ? GetPresentationQuality(Quality) : EGameXXKEquipmentQuality::Invalid;
}

FText FGameXXKGemRules::GetSocketText(EGameXXKGemType Type, EGameXXKGemQuality Quality)
{
	return FText::FromString(GetQualityDisplayName(Quality).ToString() + TEXT("·") + GetTypeDisplayName(Type).ToString()
		+ TEXT(" ") + GetBonusText(Type, Quality).ToString());
}

FText FGameXXKGemRules::GetDescription(EGameXXKGemType Type, EGameXXKGemQuality Quality)
{
	if (!IsValidType(Type) || !IsValidQuality(Quality)) return FText::GetEmpty();
	FString Text = GetTypeDisplayName(Type).ToString() + TEXT(" ") + GetBonusText(Type, Quality).ToString();
	if (IsResistanceType(Type))
		return FText::FromString(Text + TEXT("（名义抗性）\n增加对应抗性，对佩戴者基础抗性按边际递减计算，最终上限75%。\n可在详细属性查看实际抗性；不改变防御与临时护甲。"));
	if (!IsPercentType(Type)) return FText::FromString(Text + TEXT("\n镶嵌后增加对应属性，固定数值直接相加。"));
	Text += FString::Printf(TEXT("（名义值）\n单颗实际增幅 +%.2f%%"), GetEffectiveBonusBasisPoints(GetBonusBasisPoints(Type, Quality)) / 100.0);
	const TCHAR* Scope = TEXT("");
	switch (Type)
	{
	case EGameXXKGemType::AttackPercent: case EGameXXKGemType::DefensePercent: case EGameXXKGemType::MaxHealthPercent:
		Scope = TEXT("对裸身、装备和固定宝石合计属性生效，再计算永久天赋。"); break;
	case EGameXXKGemType::DirectDamage: Scope = TEXT("增加本人物理攻击与物理反击伤害，法术伤害另算。"); break;
	case EGameXXKGemType::ArmorGain: Scope = TEXT("增加本人生成的护甲；复制护甲不重复加成。"); break;
	case EGameXXKGemType::Healing: Scope = TEXT("增加本人主动治疗；吸血、复活和固定套装恢复除外。"); break;
	case EGameXXKGemType::CounterDamage: Scope = TEXT("增加本人已有反击与玄甲格挡追击的伤害。"); break;
	case EGameXXKGemType::FireDamage: Scope = TEXT("增加本人火系直伤、施加的灼烧跳伤及主动引燃伤害。"); break;
	case EGameXXKGemType::DamageOverTime: Scope = TEXT("增加本人流血、中毒、灼烧的跳伤与主动引爆伤害。"); break;
	case EGameXXKGemType::FrostDamage: Scope = TEXT("增加本人冰系攻击、冰爆和冰系阵赏伤害。"); break;
	case EGameXXKGemType::LightningDamage: Scope = TEXT("增加本人雷系攻击与落雷伤害，雷击次数不变。"); break;
	default: break;
	}
	return FText::FromString(Text + TEXT("\n") + Scope + TEXT("\n同次适用的宝石增幅合并后边际递减，额外收益上限75%。"));
}

FText FGameXXKGemRules::GetTypeDisplayName(const EGameXXKGemType Type)
{
	switch (Type)
	{
	case EGameXXKGemType::Attack: return NSLOCTEXT("GameXXKGems", "Attack", "攻击");
	case EGameXXKGemType::Defense: return NSLOCTEXT("GameXXKGems", "Defense", "防御");
	case EGameXXKGemType::MaxHealth: return NSLOCTEXT("GameXXKGems", "MaxHealth", "生命");
	case EGameXXKGemType::AttackPercent: return NSLOCTEXT("GameXXKGems", "AttackPercent", "攻击百分比");
	case EGameXXKGemType::DefensePercent: return NSLOCTEXT("GameXXKGems", "DefensePercent", "防御百分比");
	case EGameXXKGemType::MaxHealthPercent: return NSLOCTEXT("GameXXKGems", "MaxHealthPercent", "生命百分比");
	case EGameXXKGemType::DirectDamage: return NSLOCTEXT("GameXXKGems", "DirectDamage", "物理伤害");
	case EGameXXKGemType::ArmorGain: return NSLOCTEXT("GameXXKGems", "ArmorGain", "护甲获得量");
	case EGameXXKGemType::Healing: return NSLOCTEXT("GameXXKGems", "Healing", "治疗效果");
	case EGameXXKGemType::CounterDamage: return NSLOCTEXT("GameXXKGems", "CounterDamage", "反击伤害");
	case EGameXXKGemType::FireDamage: return NSLOCTEXT("GameXXKGems", "FireDamage", "火焰伤害");
	case EGameXXKGemType::DamageOverTime: return NSLOCTEXT("GameXXKGems", "DamageOverTime", "持续伤害");
	case EGameXXKGemType::FrostDamage: return NSLOCTEXT("GameXXKGems", "FrostDamage", "冰霜伤害");
	case EGameXXKGemType::LightningDamage: return NSLOCTEXT("GameXXKGems", "LightningDamage", "雷击伤害");
	case EGameXXKGemType::FireResistance: return NSLOCTEXT("GameXXKGems", "FireResistance", "火焰抗性");
	case EGameXXKGemType::FrostResistance: return NSLOCTEXT("GameXXKGems", "FrostResistance", "冰霜抗性");
	case EGameXXKGemType::LightningResistance: return NSLOCTEXT("GameXXKGems", "LightningResistance", "雷击抗性");
	default: return FText::GetEmpty();
	}
}

FText FGameXXKGemRules::GetQualityDisplayName(const EGameXXKGemQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKGemQuality::Common: return NSLOCTEXT("GameXXKGems", "Common", "普通");
	case EGameXXKGemQuality::Rare: return NSLOCTEXT("GameXXKGems", "Rare", "稀有");
	case EGameXXKGemQuality::Epic: return NSLOCTEXT("GameXXKGems", "Epic", "珍稀");
	case EGameXXKGemQuality::Legendary: return NSLOCTEXT("GameXXKGems", "Legendary", "传奇");
	case EGameXXKGemQuality::Immortal: return NSLOCTEXT("GameXXKGems", "Immortal", "不朽");
	case EGameXXKGemQuality::Treasure: return NSLOCTEXT("GameXXKGems", "Treasure", "至宝");
	case EGameXXKGemQuality::Transcendent: return NSLOCTEXT("GameXXKGems", "Transcendent", "超凡");
	case EGameXXKGemQuality::Celestial: return NSLOCTEXT("GameXXKGems", "Celestial", "天界");
	case EGameXXKGemQuality::Ascendant: return NSLOCTEXT("GameXXKGems", "Ascendant", "登神");
	case EGameXXKGemQuality::Cosmic: return NSLOCTEXT("GameXXKGems", "Cosmic", "宇宙");
	default: return FText::GetEmpty();
	}
}

FText FGameXXKGemRules::GetDisplayName(const EGameXXKGemType Type, const EGameXXKGemQuality Quality)
{
	if (!IsValidType(Type) || !IsValidQuality(Quality))
	{
		return FText::GetEmpty();
	}
	return FText::Format(
		NSLOCTEXT("GameXXKGems", "DisplayFormat", "{0}{1}宝石"),
		GetQualityDisplayName(Quality),
		GetTypeDisplayName(Type));
}

FName FGameXXKGemRules::MakeItemId(const EGameXXKGemType Type, const EGameXXKGemQuality Quality)
{
	const TCHAR* StableType = TypeToken(Type);
	const TCHAR* StableQuality = QualityToken(Quality);
	return StableType && StableQuality
		? FName(*FString::Printf(TEXT("Item.Gem.%s.%s"), StableType, StableQuality))
		: NAME_None;
}

bool FGameXXKGemRules::TryParseItemId(
	const FName ItemId,
	EGameXXKGemType& OutType,
	EGameXXKGemQuality& OutQuality)
{
	OutType = EGameXXKGemType::Invalid;
	OutQuality = EGameXXKGemQuality::Invalid;
	TArray<FString> Parts;
	ItemId.ToString().ParseIntoArray(Parts, TEXT("."), false);
	if (Parts.Num() != 4 || Parts[0] != TEXT("Item") || Parts[1] != TEXT("Gem"))
	{
		return false;
	}
	OutType = TypeFromToken(Parts[2]);
	OutQuality = QualityFromToken(Parts[3]);
	return IsValidType(OutType) && IsValidQuality(OutQuality);
}

FSoftObjectPath FGameXXKGemRules::GetIconTexturePath(
	const EGameXXKGemType Type,
	const EGameXXKGemQuality Quality)
{
	const TCHAR* StableType = TypeToken(Type);
	const TCHAR* StableQuality = QualityToken(Quality);
	if (!StableType || !StableQuality)
	{
		return FSoftObjectPath();
	}
	const FString Name = FString::Printf(TEXT("T_Item_Gem_%s"), StableType);
	return FSoftObjectPath(FString::Printf(TEXT("/Game/GameXXK/UI/Items/Gems/%s.%s"), *Name, *Name));
}

FSoftObjectPath FGameXXKGemRules::GetIconTexturePathForItemId(const FName ItemId)
{
	EGameXXKGemType Type;
	EGameXXKGemQuality Quality;
	return TryParseItemId(ItemId, Type, Quality) ? GetIconTexturePath(Type, Quality) : FSoftObjectPath();
}

TArray<FName> FGameXXKGemRules::GetAllItemIds()
{
	TArray<FName> Result;
	Result.Reserve(MaximumTypeRank * MaximumQualityRank);
	for (int32 TypeRank = 1; TypeRank <= MaximumTypeRank; ++TypeRank)
	{
		for (int32 QualityRank = MinimumQualityRank; QualityRank <= MaximumQualityRank; ++QualityRank)
		{
			Result.Add(MakeItemId(static_cast<EGameXXKGemType>(TypeRank), QualityFromRank(QualityRank)));
		}
	}
	return Result;
}
