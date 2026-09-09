#include "GameXXKResistanceRules.h"

namespace
{
	struct FProfileEntry { FName Key; FGameXXKResistanceProfile Profile; };
	const TArray<FProfileEntry>& Entries()
	{
		static const TArray<FProfileEntry> Values = {
#include "Data/GameXXKResistanceProfiles.inl"
		};
		return Values;
	}
	const FGameXXKResistanceProfile* FindProfile(FName Key)
	{
		const auto* Entry = Entries().FindByPredicate([Key](const auto& Row) { return Row.Key == Key; });
		return Entry ? &Entry->Profile : nullptr;
	}
	FName RoleKey(EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Hero: return TEXT("Hero");
		case EGameXXKCharacterRole::Blade: return TEXT("Role.Blade");
		case EGameXXKCharacterRole::Guard: return TEXT("Role.Guard");
		case EGameXXKCharacterRole::Healer: return TEXT("Role.Healer");
		case EGameXXKCharacterRole::Hunter: return TEXT("Role.Hunter");
		case EGameXXKCharacterRole::Sorcerer: return TEXT("Role.Sorcerer");
		case EGameXXKCharacterRole::FormationMaster: return TEXT("Role.FormationMaster");
		default: return NAME_None;
		}
	}
}

int32 FGameXXKResistanceProfile::Get(EGameXXKCardDamageElement Element) const
{
	switch (Element)
	{
	case EGameXXKCardDamageElement::Fire: return FireBasisPoints;
	case EGameXXKCardDamageElement::Frost: return FrostBasisPoints;
	case EGameXXKCardDamageElement::Lightning: return LightningBasisPoints;
	default: return 0;
	}
}

bool FGameXXKResistanceRules::IsSpell(EGameXXKCardDamageElement Element)
{
	return Element >= EGameXXKCardDamageElement::Fire && Element <= EGameXXKCardDamageElement::Lightning;
}

EGameXXKGemType FGameXXKResistanceRules::GemForElement(EGameXXKCardDamageElement Element)
{
	switch (Element)
	{
	case EGameXXKCardDamageElement::Fire: return EGameXXKGemType::FireResistance;
	case EGameXXKCardDamageElement::Frost: return EGameXXKGemType::FrostResistance;
	case EGameXXKCardDamageElement::Lightning: return EGameXXKGemType::LightningResistance;
	default: return EGameXXKGemType::Invalid;
	}
}

TArray<FName> FGameXXKResistanceRules::GetProfileKeys()
{
	TArray<FName> Keys;
	for (const auto& Entry : Entries()) Keys.Add(Entry.Key);
	return Keys;
}

FGameXXKResistanceProfile FGameXXKResistanceRules::GetBaseProfile(FName CharacterId, EGameXXKCharacterRole Role, FName EnemyDefinitionId)
{
	if (!EnemyDefinitionId.IsNone())
	{
		const auto* Profile = FindProfile(EnemyDefinitionId);
		return Profile ? *Profile : FGameXXKResistanceProfile();
	}
	if (const auto* Profile = FindProfile(CharacterId)) return *Profile;
	if (const auto* Profile = FindProfile(RoleKey(Role))) return *Profile;
	return {};
}

void FGameXXKResistanceRules::InitializeUnitProfile(FGameXXKCardCombatUnit& Unit)
{
	const auto Profile = GetBaseProfile(Unit.UnitId, Unit.Role, Unit.EnemyDefinitionId);
	Unit.InnateResistanceBasisPoints.Reset();
	for (auto Element : {EGameXXKCardDamageElement::Fire, EGameXXKCardDamageElement::Frost, EGameXXKCardDamageElement::Lightning})
		Unit.InnateResistanceBasisPoints.Add(Element, Profile.Get(Element));
}

bool FGameXXKResistanceRules::ValidateUnitProfile(const FGameXXKCardCombatUnit& Unit)
{
	// Empty is the explicit zero-resistance contract for pure rules callers and legacy inputs.
	if (Unit.InnateResistanceBasisPoints.IsEmpty()) return true;
	if (Unit.InnateResistanceBasisPoints.Num() != 3) return false;
	for (auto Element : {EGameXXKCardDamageElement::Fire, EGameXXKCardDamageElement::Frost, EGameXXKCardDamageElement::Lightning})
	{
		const auto* Value = Unit.InnateResistanceBasisPoints.Find(Element);
		if (!Value || *Value < MinimumBasisPoints || *Value > MaximumBasisPoints) return false;
	}
	return true;
}

double FGameXXKResistanceRules::ResolveBasisPoints(int32 InnateBasisPoints, int64 NominalBonusBasisPoints, int32 ReductionBasisPoints)
{
	const int32 Base = FMath::Clamp(InnateBasisPoints, MinimumBasisPoints, MaximumBasisPoints);
	const int64 Bonus = FMath::Clamp<int64>(NominalBonusBasisPoints, 0, MAX_int32);
	const int64 Room = MaximumBasisPoints - Base;
	const double Positive = Room > 0 ? Base + static_cast<double>(Room) * Bonus / (Room + Bonus) : MaximumBasisPoints;
	return FMath::Clamp(Positive - FMath::Max(0, ReductionBasisPoints),
		static_cast<double>(MinimumBasisPoints), static_cast<double>(MaximumBasisPoints));
}

double FGameXXKResistanceRules::GetEffectiveBasisPoints(const FGameXXKCardCombatUnit& Unit, EGameXXKCardDamageElement Element)
{
	if (!IsSpell(Element)) return 0.0;
	return ResolveBasisPoints(Unit.InnateResistanceBasisPoints.FindRef(Element),
		Unit.GemBonusBasisPoints.FindRef(GemForElement(Element)));
}

int32 FGameXXKResistanceRules::ApplyDamage(const FGameXXKCardCombatUnit& Target, EGameXXKCardDamageElement Element, int32 Damage)
{
	if (Damage <= 0) return 0;
	if (!IsSpell(Element)) return Damage;
	const double Multiplier = 1.0 - GetEffectiveBasisPoints(Target, Element) / 10000.0;
	return static_cast<int32>(FMath::Clamp<int64>(FMath::RoundToInt64(Damage * Multiplier), 1, MAX_int32));
}
