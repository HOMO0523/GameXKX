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
		if (Token == TEXT("Attack")) return EGameXXKGemType::Attack;
		if (Token == TEXT("Defense")) return EGameXXKGemType::Defense;
		if (Token == TEXT("MaxHealth")) return EGameXXKGemType::MaxHealth;
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
	return Type >= EGameXXKGemType::Attack && Type <= EGameXXKGemType::MaxHealth;
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
	if (!IsValidType(Type) || Rank == 0)
	{
		return 0;
	}
	const int32 Multiplier = 1 << (Rank - 1);
	return Type == EGameXXKGemType::MaxHealth ? Multiplier * 10 : Multiplier;
}

FText FGameXXKGemRules::GetTypeDisplayName(const EGameXXKGemType Type)
{
	switch (Type)
	{
	case EGameXXKGemType::Attack: return NSLOCTEXT("GameXXKGems", "Attack", "攻击");
	case EGameXXKGemType::Defense: return NSLOCTEXT("GameXXKGems", "Defense", "防御");
	case EGameXXKGemType::MaxHealth: return NSLOCTEXT("GameXXKGems", "MaxHealth", "生命");
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
	const FString Name = FString::Printf(TEXT("T_Item_Gem_%s_%s"), StableType, StableQuality);
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
	Result.Reserve(3 * MaximumQualityRank);
	for (int32 TypeRank = static_cast<int32>(EGameXXKGemType::Attack); TypeRank <= static_cast<int32>(EGameXXKGemType::MaxHealth); ++TypeRank)
	{
		for (int32 QualityRank = MinimumQualityRank; QualityRank <= MaximumQualityRank; ++QualityRank)
		{
			Result.Add(MakeItemId(static_cast<EGameXXKGemType>(TypeRank), QualityFromRank(QualityRank)));
		}
	}
	return Result;
}
