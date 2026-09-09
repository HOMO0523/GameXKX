#include "GameXXKCombatGemRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKResistanceRules.h"

namespace
{
	const FGameXXKCardCombatUnit* FindSource(const TArray<FGameXXKCardCombatUnit>& Units, FName Id)
	{
		return Id.IsNone() ? nullptr : Units.FindByPredicate([Id](const auto& Unit) { return Unit.UnitId == Id; });
	}
}

int64 FGameXXKCombatGemRules::GetRawBonus(const FGameXXKCardCombatUnit* Source, EGameXXKGemType Type)
{
	return Source && Source->Side == EGameXXKCardTargetSide::Party
		? FMath::Max(0, Source->GemBonusBasisPoints.FindRef(Type)) : 0;
}

bool FGameXXKCombatGemRules::IsCardBodyOrigin(EGameXXKCardResolutionOrigin Origin)
{
	return Origin == EGameXXKCardResolutionOrigin::ActivePlay
		|| Origin == EGameXXKCardResolutionOrigin::AutomaticReplay
		|| Origin == EGameXXKCardResolutionOrigin::MageTaskReplay
		|| Origin == EGameXXKCardResolutionOrigin::TaskNpcTaskReplay
		|| Origin == EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay
		|| Origin == EGameXXKCardResolutionOrigin::HeavyArrow;
}

EGameXXKCardDamageElement FGameXXKCombatGemRules::GetCardElement(const FGameXXKCardDefinition& Definition)
{
	if (Definition.DamageElement != EGameXXKCardDamageElement::None) return Definition.DamageElement;
	if (Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Fire
		|| Definition.SpellTaskReward == EGameXXKHeroSpellTaskReward::Fire) return EGameXXKCardDamageElement::Fire;
	if (Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Ice
		|| Definition.SpellTaskReward == EGameXXKHeroSpellTaskReward::Ice) return EGameXXKCardDamageElement::Frost;
	if (Definition.SorcererRule.Family == EGameXXKSorcererCardFamily::Lightning
		|| Definition.SpellTaskReward == EGameXXKHeroSpellTaskReward::Lightning) return EGameXXKCardDamageElement::Lightning;
	return EGameXXKCardDamageElement::None;
}

FString FGameXXKCombatGemRules::GetElementLabel(EGameXXKCardDamageElement Element)
{
	switch(Element)
	{
	case EGameXXKCardDamageElement::Fire: return TEXT("火焰");
	case EGameXXKCardDamageElement::Frost: return TEXT("冰霜");
	case EGameXXKCardDamageElement::Lightning: return TEXT("雷击");
	default: return TEXT("物理");
	}
}

int64 FGameXXKCombatGemRules::GetDirectPool(const FGameXXKCardCombatUnit* Source, const FGameXXKCardDamageContext& Context)
{
	const bool Attack = Context.Kind == EGameXXKCardDamageKind::SingleTargetAttack || Context.Kind == EGameXXKCardDamageKind::GroupAttack;
	const bool Spell = FGameXXKResistanceRules::IsSpell(Context.Element);
	if (!Attack && !(Context.Kind == EGameXXKCardDamageKind::FixedDamage && Spell)) return 0;
	const bool PhysicalEligible = Attack && !Spell
		&& Context.ResolutionOrigin != EGameXXKCardResolutionOrigin::TerrainListener
		&& (Context.ResolutionOrigin != EGameXXKCardResolutionOrigin::Equipment || Context.bEquipmentCounter);
	int64 Raw = PhysicalEligible ? GetRawBonus(Source, EGameXXKGemType::DirectDamage) : 0;
	if (Context.ResolutionOrigin == EGameXXKCardResolutionOrigin::Reaction || Context.bEquipmentCounter)
		Raw += GetRawBonus(Source, EGameXXKGemType::CounterDamage);
	switch (Context.Element)
	{
	case EGameXXKCardDamageElement::Fire: Raw += GetRawBonus(Source, EGameXXKGemType::FireDamage); break;
	case EGameXXKCardDamageElement::Frost: Raw += GetRawBonus(Source, EGameXXKGemType::FrostDamage); break;
	case EGameXXKCardDamageElement::Lightning: Raw += GetRawBonus(Source, EGameXXKGemType::LightningDamage); break;
	default: break;
	}
	return FMath::Min<int64>(Raw, MAX_int32);
}

int32 FGameXXKCombatGemRules::ApplyHealing(const FGameXXKCardCombatUnit& Source, int32 Amount, EGameXXKCardResolutionOrigin Origin)
{
	return FGameXXKGemRules::ApplyBonus(Amount, IsCardBodyOrigin(Origin) ? GetRawBonus(&Source, EGameXXKGemType::Healing) : 0,
		EGameXXKGemRounding::Down);
}

bool FGameXXKCombatGemRules::TracksSource(EGameXXKCardStatus Status)
{
	return Status == EGameXXKCardStatus::Bleed || Status == EGameXXKCardStatus::Poison || Status == EGameXXKCardStatus::Burn;
}

void FGameXXKCombatGemRules::AddSource(FGameXXKCardStatusStack& Stack, FName Source, int32 Added)
{
	if (!TracksSource(Stack.Status) || Source.IsNone() || Added <= 0) return;
	FGameXXKCardStatusSource* Entry = Stack.Sources.FindByPredicate([Source](const auto& Part) { return Part.SourceUnitId == Source; });
	if (!Entry)
	{
		Entry = &Stack.Sources.AddDefaulted_GetRef();
		Entry->SourceUnitId = Source;
	}
	Entry->Stacks = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(Entry->Stacks) + Added));
}

void FGameXXKCombatGemRules::RetainSources(FGameXXKCardStatusStack& Stack, int32 Remaining)
{
	if (Remaining <= 0) { Stack.Sources.Reset(); return; }
	if (Stack.Stacks <= 0 || Remaining >= Stack.Stacks) return;
	int64 Prefix = 0;
	int64 RetainedPrefix = 0;
	for (auto& Part : Stack.Sources)
	{
		Prefix += Part.Stacks;
		const int64 Next = Prefix * Remaining / Stack.Stacks;
		Part.Stacks = static_cast<int32>(Next - RetainedPrefix);
		RetainedPrefix = Next;
	}
	Stack.Sources.RemoveAll([](const auto& Part) { return Part.Stacks <= 0; });
}

bool FGameXXKCombatGemRules::ValidateSources(const FGameXXKCardStatusStack& Stack)
{
	if (!TracksSource(Stack.Status)) return Stack.Sources.IsEmpty();
	TSet<FName> Seen;
	int64 Total = 0;
	for (const auto& Part : Stack.Sources)
	{
		if (Part.SourceUnitId.IsNone() || Part.Stacks <= 0 || Seen.Contains(Part.SourceUnitId)) return false;
		Seen.Add(Part.SourceUnitId);
		Total += Part.Stacks;
	}
	return Total <= Stack.Stacks;
}

EGameXXKCardStatus FGameXXKCombatGemRules::StatusForCause(EGameXXKCardDamageCause Cause)
{
	switch (Cause)
	{
	case EGameXXKCardDamageCause::Bleed: case EGameXXKCardDamageCause::ToxicExplosionBleed: return EGameXXKCardStatus::Bleed;
	case EGameXXKCardDamageCause::Poison: case EGameXXKCardDamageCause::ToxicExplosionPoison: return EGameXXKCardStatus::Poison;
	case EGameXXKCardDamageCause::Burn: case EGameXXKCardDamageCause::ToxicExplosionBurn: return EGameXXKCardStatus::Burn;
	default: return EGameXXKCardStatus::DamageOverTime;
	}
}

int64 FGameXXKCombatGemRules::GetDotPool(const FGameXXKCardCombatUnit* Source, EGameXXKCardStatus Status)
{
	if (!TracksSource(Status)) return 0;
	return FMath::Min<int64>(MAX_int32, GetRawBonus(Source, EGameXXKGemType::DamageOverTime)
		+ (Status == EGameXXKCardStatus::Burn ? GetRawBonus(Source, EGameXXKGemType::FireDamage) : 0));
}

int32 FGameXXKCombatGemRules::NaturalDot(const TArray<FGameXXKCardCombatUnit>& Units, const FGameXXKCardCombatUnit& Target,
	EGameXXKCardStatus Status, int32 BaseStacks)
{
	if (BaseStacks <= 0 || !TracksSource(Status)) return FMath::Max(0, BaseStacks);
	int64 Reservoir = 0;
	double Extra = 0;
	for (const auto& Stack : Target.Statuses)
	{
		if (Stack.Status != Status || Stack.Stacks <= 0) continue;
		Reservoir += Stack.Stacks;
		for (const auto& Part : Stack.Sources)
			Extra += Part.Stacks * FGameXXKGemRules::GetEffectiveBonusBasisPoints(GetDotPool(FindSource(Units, Part.SourceUnitId), Status)) / 10000.0;
	}
	// One final rounding for the original packet, including mixed and unknown sources.
	const double Damage = BaseStacks + (Reservoir > 0 ? Extra * BaseStacks / Reservoir : 0.0);
	return static_cast<int32>(FMath::Min<int64>(MAX_int32, FMath::RoundToInt64(Damage)));
}

int32 FGameXXKCombatGemRules::TriggeredDot(const TArray<FGameXXKCardCombatUnit>& Units, FName Source,
	EGameXXKCardStatus Status, int32 BaseStacks)
{
	return FGameXXKGemRules::ApplyBonus(BaseStacks, GetDotPool(FindSource(Units, Source), Status));
}
