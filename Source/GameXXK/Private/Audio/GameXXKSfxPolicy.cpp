#include "Audio/GameXXKSfxPolicy.h"

EGameXXKSfxCue FGameXXKSfxPolicy::ResolveImpact(const FGameXXKBattlePresentationEvent& Event)
{
	if (Event.bAvoided || (Event.HealthDamage <= 0 && Event.ArmorAbsorbed <= 0))
	{
		return EGameXXKSfxCue::None;
	}
	if (Event.bLightningStrike || Event.Element == EGameXXKCardDamageElement::Lightning)
	{
		return EGameXXKSfxCue::Lightning;
	}
	if (Event.Element == EGameXXKCardDamageElement::Fire) return EGameXXKSfxCue::Fire;
	if (Event.Element == EGameXXKCardDamageElement::Frost) return EGameXXKSfxCue::Frost;
	if (Event.ArmorAbsorbed > 0 || Event.DamageCause == EGameXXKCardDamageCause::Block) return EGameXXKSfxCue::Block;
	const auto Tier = FGameXXKBattleAnimationPresentation::ResolveCombatRhythm(Event).ImpactTier;
	return Tier == EGameXXKBattlePresentationImpactTier::Heavy || Tier == EGameXXKBattlePresentationImpactTier::Lethal
		? EGameXXKSfxCue::HitHeavy : EGameXXKSfxCue::HitLight;
}

bool FGameXXKSfxPolicy::HasActualHealing(const FGameXXKCardBattleRuntime& Before, const FGameXXKCardBattleRuntime& After)
{
	for (const FGameXXKCardCombatUnit& Unit : After.Units)
	{
		const auto* Previous = Before.Units.FindByPredicate([&Unit](const FGameXXKCardCombatUnit& Other)
		{
			return Other.UnitId == Unit.UnitId;
		});
		if (Previous && Unit.SettlementHealingReceived > Previous->SettlementHealingReceived)
		{
			return true;
		}
	}
	return false;
}

bool FGameXXKSfxPolicy::Accept(EGameXXKSfxCue Cue, double NowSeconds, bool bMuted)
{
	if (Cue == EGameXXKSfxCue::None || bMuted || !FMath::IsFinite(NowSeconds) || NowSeconds < 0)
	{
		return false;
	}
	const double Cooldown = Cue == EGameXXKSfxCue::Button ? 0.08
		: Cue == EGameXXKSfxCue::Heal || Cue == EGameXXKSfxCue::Reward || Cue == EGameXXKSfxCue::Tool ? 0.15 : 0.035;
	if (const double* Previous = LastAccepted.Find(Cue))
	{
		if (NowSeconds >= *Previous && NowSeconds - *Previous < Cooldown) return false;
	}
	LastAccepted.Add(Cue, NowSeconds);
	return true;
}
