#include "GameXXKCardCatalog.h"

#include "GameXXKCardQualityRules.h"

#include "UObject/Class.h"

namespace
{
	EGameXXKCardTargetPresentation TargetPresentationForMode(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::None:
			return EGameXXKCardTargetPresentation::NoSelection;
		case EGameXXKCardTargetMode::Self:
			return EGameXXKCardTargetPresentation::Self;
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::AnyLivingUnit:
			return EGameXXKCardTargetPresentation::PlayerSelectsUnit;
		case EGameXXKCardTargetMode::AllEnemies:
		case EGameXXKCardTargetMode::AllAllies:
		case EGameXXKCardTargetMode::AllOtherAllies:
			return EGameXXKCardTargetPresentation::Group;
		case EGameXXKCardTargetMode::RandomEnemy:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
			return EGameXXKCardTargetPresentation::AutomaticUnit;
		case EGameXXKCardTargetMode::Invalid:
		default:
			return EGameXXKCardTargetPresentation::Invalid;
		}
	}

	bool TargetModeRequiresDifferentFromOwner(const EGameXXKCardTargetMode Mode)
	{
		return Mode == EGameXXKCardTargetMode::OtherAlly
			|| Mode == EGameXXKCardTargetMode::LowestHealthOtherAlly
			|| Mode == EGameXXKCardTargetMode::AllOtherAllies;
	}

	bool CanResolveSelectedTarget(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::SingleEnemy:
		case EGameXXKCardTargetMode::SingleAlly:
		case EGameXXKCardTargetMode::OtherAlly:
		case EGameXXKCardTargetMode::RandomEnemy:
		case EGameXXKCardTargetMode::LowestHealthAlly:
		case EGameXXKCardTargetMode::LowestHealthOtherAlly:
		case EGameXXKCardTargetMode::AnyLivingUnit:
			return true;
		default:
			return false;
		}
	}

	bool IsConcreteStatus(const EGameXXKCardStatus Status)
	{
		return Status != EGameXXKCardStatus::Invalid && Status != EGameXXKCardStatus::None;
	}

	bool TerrainSetContains(
		const EGameXXKCardTerrain Terrain,
		const EGameXXKCardTerrain AlternateTerrain,
		const EGameXXKCardTerrain Candidate)
	{
		return Candidate != EGameXXKCardTerrain::Invalid
			&& (Candidate == Terrain || Candidate == AlternateTerrain);
	}

	bool CanEffectConditionApplyWhenOverrideApplies(
		const FGameXXKCardEffectCondition& EffectCondition,
		const FGameXXKCardTargetModeOverride& Override)
	{
		switch (Override.ConditionType)
		{
		case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
			if (EffectCondition.Type != EGameXXKCardEffectConditionType::TerrainIsAny)
			{
				return true;
			}
			{
				const bool bPrimaryMatches = TerrainSetContains(EffectCondition.Terrain, EffectCondition.AlternateTerrain, Override.Terrain);
				const bool bAlternateMatches = Override.AlternateTerrain != EGameXXKCardTerrain::Invalid
					&& TerrainSetContains(EffectCondition.Terrain, EffectCondition.AlternateTerrain, Override.AlternateTerrain);
				return EffectCondition.bNegate
					? !bPrimaryMatches || (Override.AlternateTerrain != EGameXXKCardTerrain::Invalid && !bAlternateMatches)
					: bPrimaryMatches || bAlternateMatches;
			}
		case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus:
			if (EffectCondition.Type == EGameXXKCardEffectConditionType::OwnerHasStatus
				&& EffectCondition.Status == Override.Status
				&& EffectCondition.bNegate)
			{
				return Override.MinimumStatusStacks < EffectCondition.MinimumStatusStacks;
			}
			return true;
		case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus:
			if (EffectCondition.Type == EGameXXKCardEffectConditionType::TargetHasStatus
				&& EffectCondition.Status == Override.Status
				&& EffectCondition.bNegate)
			{
				return Override.MinimumStatusStacks < EffectCondition.MinimumStatusStacks;
			}
			return true;
		default:
			return true;
		}
	}

	bool UsesSelectedTarget(const FGameXXKCardEffect& CardEffect)
	{
		if (CardEffect.Target == EGameXXKCardEffectTarget::SelectedTarget
			|| CardEffect.Target == EGameXXKCardEffectTarget::SelectedTargetSide
			|| CardEffect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsAlly
			|| CardEffect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsEnemy)
		{
			return true;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			return CardEffect.Modifier.Target == EGameXXKCardEffectTarget::SelectedTarget
				|| CardEffect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyGuardLink)
		{
			return CardEffect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget
				|| CardEffect.GuardLink.ProtectedUnit == EGameXXKCardEffectTarget::SelectedTarget;
		}
		return false;
	}

	bool UsesSelectedTargetWhenOverrideApplies(
		const FGameXXKCardEffect& CardEffect,
		const FGameXXKCardTargetModeOverride& Override)
	{
		if (!CanEffectConditionApplyWhenOverrideApplies(CardEffect.Condition, Override))
		{
			return false;
		}
		if (CardEffect.Target == EGameXXKCardEffectTarget::SelectedTarget
			|| CardEffect.Target == EGameXXKCardEffectTarget::SelectedTargetSide
			|| CardEffect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsAlly
			|| CardEffect.Condition.Type == EGameXXKCardEffectConditionType::TargetIsEnemy)
		{
			return true;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			return CanEffectConditionApplyWhenOverrideApplies(CardEffect.Modifier.Condition, Override)
				&& (CardEffect.Modifier.Target == EGameXXKCardEffectTarget::SelectedTarget
					|| CardEffect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::SelectedTarget);
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyGuardLink)
		{
			return CardEffect.GuardLink.Guardian == EGameXXKCardEffectTarget::SelectedTarget
				|| CardEffect.GuardLink.ProtectedUnit == EGameXXKCardEffectTarget::SelectedTarget;
		}
		return false;
	}

	FGameXXKCardTargetSpec Target(const EGameXXKCardTargetMode Mode)
	{
		FGameXXKCardTargetSpec Result;
		Result.Mode = Mode;
		Result.RequiredUnitState = Mode == EGameXXKCardTargetMode::None
			? EGameXXKCardUnitState::Any
			: EGameXXKCardUnitState::Living;
		Result.Presentation = TargetPresentationForMode(Mode);
		Result.bRequireDifferentFromOwner = TargetModeRequiresDifferentFromOwner(Mode);
		return Result;
	}

	FGameXXKCardEffectCondition TargetHasStatus(const EGameXXKCardStatus Status, const int32 MinimumStacks = 1)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetHasStatus;
		Result.Status = Status;
		Result.MinimumStatusStacks = MinimumStacks;
		return Result;
	}

	FGameXXKCardEffectCondition TargetHasAnyDamageOverTime()
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime;
		return Result;
	}

	FGameXXKCardEffectCondition TargetIsAlly()
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetIsAlly;
		return Result;
	}

	FGameXXKCardEffectCondition TargetIsEnemy()
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetIsEnemy;
		return Result;
	}

	FGameXXKCardEffectCondition OwnerHasStatus(const EGameXXKCardStatus Status, const int32 MinimumStacks = 1)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::OwnerHasStatus;
		Result.Status = Status;
		Result.MinimumStatusStacks = MinimumStacks;
		return Result;
	}

	FGameXXKCardEffectCondition OwnerArmorAtLeast(const int32 MinimumArmor, const bool bNegate = false)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::OwnerArmorAtLeast;
		Result.MinimumArmor = MinimumArmor;
		Result.bNegate = bNegate;
		return Result;
	}

	FGameXXKCardEffectCondition OwnerHealthBelow(const float HealthPercent, const bool bNegate = false)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::OwnerHealthBelowPercent;
		Result.HealthPercentThreshold = HealthPercent;
		Result.bNegate = bNegate;
		return Result;
	}

	FGameXXKCardEffectCondition TargetHealthBelow(const float HealthPercent)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TargetHealthBelowPercent;
		Result.HealthPercentThreshold = HealthPercent;
		return Result;
	}

	FGameXXKCardEffectCondition TerrainIs(const EGameXXKCardTerrain Terrain, const EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid, const bool bNegate = false)
	{
		FGameXXKCardEffectCondition Result;
		Result.Type = EGameXXKCardEffectConditionType::TerrainIsAny;
		Result.Terrain = Terrain;
		Result.AlternateTerrain = AlternateTerrain;
		Result.bNegate = bNegate;
		return Result;
	}

	FGameXXKCardTargetModeOverride TerrainTargetOverride(
		const EGameXXKCardTerrain Terrain,
		const EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid,
		const EGameXXKCardTargetMode Mode = EGameXXKCardTargetMode::AllAllies)
	{
		FGameXXKCardTargetModeOverride Result;
		Result.ConditionType = EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny;
		Result.Terrain = Terrain;
		Result.AlternateTerrain = AlternateTerrain;
		Result.Mode = Mode;
		Result.Presentation = TargetPresentationForMode(Mode);
		return Result;
	}

	FGameXXKCardEffectCondition ConsumeTargetStatus(const EGameXXKCardStatus Status, const int32 MaxStacks)
	{
		FGameXXKCardEffectCondition Result = TargetHasStatus(Status);
		Result.bConsumeStatus = true;
		Result.MaxConsumedStatusStacks = MaxStacks;
		Result.bScaleMagnitudeByConsumedStacks = true;
		return Result;
	}

	FGameXXKCardEffectCondition ConsumeOwnerStatus(const EGameXXKCardStatus Status, const int32 MaxStacks)
	{
		FGameXXKCardEffectCondition Result = OwnerHasStatus(Status);
		Result.bConsumeStatus = true;
		Result.MaxConsumedStatusStacks = MaxStacks;
		Result.bScaleMagnitudeByConsumedStacks = true;
		return Result;
	}

	FGameXXKCardEffectCondition ConsumeOwnerArmor(const int32 MaxArmor)
	{
		FGameXXKCardEffectCondition Result = OwnerArmorAtLeast(1);
		Result.bConsumeOwnerArmor = true;
		Result.MaxConsumedArmor = MaxArmor;
		return Result;
	}

	FGameXXKCardEffect Effect(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget EffectTarget,
		const int32 Magnitude = 0,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 HitCount = 1,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition())
	{
		FGameXXKCardEffect Result;
		Result.Type = Type;
		Result.Target = EffectTarget;
		Result.Magnitude = Magnitude;
		Result.Status = Status;
		Result.HitCount = HitCount;
		Result.Condition = Condition;
		return Result;
	}

	FGameXXKCardEffect EffectWithSecondary(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget EffectTarget,
		const int32 Magnitude,
		const int32 SecondaryMagnitude,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 HitCount = 1,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition())
	{
		FGameXXKCardEffect Result = Effect(Type, EffectTarget, Magnitude, Status, HitCount, Condition);
		Result.SecondaryMagnitude = SecondaryMagnitude;
		return Result;
	}

	FGameXXKCardEffect WithConsumptionProducer(FGameXXKCardEffect Result, const TCHAR* ConsumptionGroupId)
	{
		Result.ConsumptionGroupId = FName(ConsumptionGroupId);
		return Result;
	}

	FGameXXKCardEffect WithConsumedStackResult(FGameXXKCardEffect Result, const TCHAR* ConsumptionGroupId)
	{
		Result.ConsumedStackResultRef = FName(ConsumptionGroupId);
		return Result;
	}

	FGameXXKCardEffect WithResultProducer(FGameXXKCardEffect Result, const TCHAR* ResultGroupId)
	{
		Result.ResultGroupId = FName(ResultGroupId);
		return Result;
	}

	FGameXXKCardEffect WithResultReference(FGameXXKCardEffect Result, const TCHAR* ResultGroupId)
	{
		Result.ResultRef = FName(ResultGroupId);
		return Result;
	}

	FGameXXKCardEffect WithSource(FGameXXKCardEffect Result, const EGameXXKCardEffectSource Source)
	{
		Result.Source = Source;
		return Result;
	}

	FGameXXKCardEffect WithTerrain(FGameXXKCardEffect Result, const EGameXXKCardTerrain Terrain)
	{
		Result.TerrainOverride = Terrain;
		return Result;
	}

	FGameXXKCardEffect Reaction(
		const EGameXXKCardEffectTarget Target,
		const EGameXXKCardStatus Status,
		const int32 Stacks)
	{
		return Effect(EGameXXKCardEffectType::RegisterReaction, Target, Stacks, Status);
	}

	FGameXXKHeavyArrowRule HeavyArrow(
		const EGameXXKHeavyArrowKind Kind,
		const int32 MagnitudePerCharge,
		const int32 DrawPerCharge = 0,
		const int32 MinimumChargeForEnergy = 0,
		const int32 EnergyGain = 0,
		const EGameXXKHeavyArrowChargeSource ChargeSource = EGameXXKHeavyArrowChargeSource::CardOwner,
		const int32 ManaPerCharge = 0,
		const EGameXXKHeavyArrowLockTiming LockTiming = EGameXXKHeavyArrowLockTiming::BeforeBaseEffects)
	{
		FGameXXKHeavyArrowRule Rule;
		Rule.Kind = Kind;
		Rule.MagnitudePerCharge = MagnitudePerCharge;
		Rule.DrawPerCharge = DrawPerCharge;
		Rule.MinimumChargeForEnergy = MinimumChargeForEnergy;
		Rule.EnergyGain = EnergyGain;
		Rule.ChargeSource = ChargeSource;
		Rule.ManaPerCharge = ManaPerCharge;
		Rule.LockTiming = LockTiming;
		return Rule;
	}

	FGameXXKHeavyArrowRule WithHeavyArrowPrimaryBonus(
		FGameXXKHeavyArrowRule Rule,
		const int32 PercentPerCharge)
	{
		Rule.AdditionalPrimaryAttackPercentPerCharge = PercentPerCharge;
		return Rule;
	}

	FGameXXKHeavyArrowRule WithHeavyArrowDefenseIgnore(
		FGameXXKHeavyArrowRule Rule,
		const int32 DefensePerCharge)
	{
		Rule.IgnoreDefensePerCharge = DefensePerCharge;
		return Rule;
	}

	FGameXXKHeavyArrowRule WithHeavyArrowBleedTriggers(
		FGameXXKHeavyArrowRule Rule,
		const int32 TriggersPerCharge)
	{
		Rule.TriggeredBleedResolutionsPerCharge = TriggersPerCharge;
		return Rule;
	}

	FGameXXKHeavyArrowRule WithHeavyArrowStatus(
		FGameXXKHeavyArrowRule Rule,
		const EGameXXKCardStatus Status,
		const int32 StacksPerCharge,
		const EGameXXKCardEffectTarget Target)
	{
		Rule.BonusStatus = Status;
		Rule.BonusStatusStacksPerCharge = StacksPerCharge;
		Rule.BonusStatusTarget = Target;
		return Rule;
	}

	FGameXXKHeavyArrowRule WithHeavyArrowHealthThreshold(
		FGameXXKHeavyArrowRule Rule,
		const int32 PercentagePointsPerCharge)
	{
		Rule.HealthThresholdPointsPerCharge = PercentagePointsPerCharge;
		return Rule;
	}

	FGameXXKHunterCardRule HunterRule(
		const int32 PrimaryAttackPercentPerPriorActiveCard = 0,
		const int32 PriorActiveCardInterval = 0,
		const int32 DrawPerCompletedInterval = 0,
		const EGameXXKCardStatus StatusPerCompletedInterval = EGameXXKCardStatus::None,
		const int32 StatusStacksPerCompletedInterval = 0,
		const int32 NextHeavyArrowIgnoreDefense = 0,
		const int32 ChargeOnNextPerfectDodge = 0)
	{
		FGameXXKHunterCardRule Rule;
		Rule.PrimaryAttackPercentPerPriorActiveCard = PrimaryAttackPercentPerPriorActiveCard;
		Rule.PriorActiveCardInterval = PriorActiveCardInterval;
		Rule.DrawPerCompletedInterval = DrawPerCompletedInterval;
		Rule.StatusPerCompletedInterval = StatusPerCompletedInterval;
		Rule.StatusStacksPerCompletedInterval = StatusStacksPerCompletedInterval;
		Rule.NextHeavyArrowIgnoreDefense = NextHeavyArrowIgnoreDefense;
		Rule.ChargeOnNextPerfectDodge = ChargeOnNextPerfectDodge;
		return Rule;
	}

	FGameXXKHealerCardRule HealerRule(const EGameXXKHealerFormulaKind FormulaKind)
	{
		FGameXXKHealerCardRule Rule;
		Rule.UnopenedFormulaEnergySurcharge = 1;
		Rule.FormulaKind = FormulaKind;
		return Rule;
	}

	FGameXXKSorcererCardRule SorcererRule(
		const EGameXXKSorcererCardFamily Family,
		const EGameXXKSorcererSequenceRule SequenceRule,
		const EGameXXKSorcererRewardRule RewardRule)
	{
		FGameXXKSorcererCardRule Rule;
		Rule.Family = Family;
		Rule.SequenceRule = SequenceRule;
		Rule.RewardRule = RewardRule;
		return Rule;
	}

	FGameXXKCardEffect GuardLink(
		const EGameXXKCardEffectTarget Guardian,
		const EGameXXKCardEffectTarget ProtectedUnit,
		const int32 Stacks = 1)
	{
		FGameXXKCardEffect Result = Effect(EGameXXKCardEffectType::ApplyGuardLink, ProtectedUnit, Stacks);
		Result.GuardLink.Guardian = Guardian;
		Result.GuardLink.ProtectedUnit = ProtectedUnit;
		Result.GuardLink.Stacks = Stacks;
		Result.GuardLink.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
		return Result;
	}

	FGameXXKCardEffect Attack(
		const int32 Percent,
		const EGameXXKCardEffectTarget EffectTarget,
		const int32 HitCount = 1,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition())
	{
		return Effect(EGameXXKCardEffectType::DamagePercentAttack, EffectTarget, Percent, EGameXXKCardStatus::None, HitCount, Condition);
	}

	FGameXXKCardEffect Modifier(
		const EGameXXKCardBattleModifierTrigger Trigger,
		const EGameXXKCardEffectType ModifierEffectType,
		const EGameXXKCardEffectTarget ModifierTarget,
		const int32 Magnitude,
		const int32 RemainingTriggers,
		const int32 MinimumResult = 0,
		const FGameXXKCardEffectCondition& Condition = FGameXXKCardEffectCondition(),
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const EGameXXKCardModifierRecipientScope RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner,
		const EGameXXKCardEffectTarget RecipientTarget = EGameXXKCardEffectTarget::CardOwner,
		const EGameXXKCardModifierExpiry Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount,
		const EGameXXKCharacterRole RequiredTriggeredRole = EGameXXKCharacterRole::Invalid,
		const TCHAR* RequiredTriggeredOwnerId = nullptr,
		const EGameXXKCardTriggeredAttackTargetScope TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::AnyTarget)
	{
		FGameXXKCardEffect Result = Effect(EGameXXKCardEffectType::ApplyBattleModifier, RecipientTarget);
		Result.Modifier.Trigger = Trigger;
		Result.Modifier.EffectType = ModifierEffectType;
		Result.Modifier.Target = ModifierTarget;
		Result.Modifier.RecipientScope = RecipientScope;
		Result.Modifier.RecipientTarget = RecipientTarget;
		Result.Modifier.RequiredTriggeredRole = RequiredTriggeredRole;
		Result.Modifier.RequiredTriggeredOwnerId = RequiredTriggeredOwnerId ? FName(RequiredTriggeredOwnerId) : NAME_None;
		Result.Modifier.Expiry = Expiry;
		Result.Modifier.TriggeredAttackTargetScope = TriggeredAttackTargetScope;
		Result.Modifier.Status = Status;
		Result.Modifier.Magnitude = Magnitude;
		Result.Modifier.RemainingTriggers = RemainingTriggers;
		Result.Modifier.MinimumResult = MinimumResult;
		Result.Modifier.bPersistent = true;
		Result.Modifier.Condition = Condition;
		return Result;
	}

	FGameXXKCardEffect WithModifierPolicy(
		FGameXXKCardEffect Result,
		const bool bActivePlayOnly,
		const bool bExcludeSourceUnit = false,
		const bool bPreserveTriggeredStatus = false)
	{
		Result.Modifier.bActivePlayOnly = bActivePlayOnly;
		Result.Modifier.bExcludeSourceUnit = bExcludeSourceUnit;
		Result.Modifier.bPreserveTriggeredStatus = bPreserveTriggeredStatus;
		return Result;
	}

	bool ValidateEffectCondition(
		const FGameXXKCardEffectCondition& Condition,
		const FName CardId,
		const TCHAR* Context,
		FString& OutError)
	{
		switch (Condition.Type)
		{
		case EGameXXKCardEffectConditionType::TargetHasStatus:
		case EGameXXKCardEffectConditionType::OwnerHasStatus:
			if (!IsConcreteStatus(Condition.Status) || Condition.MinimumStatusStacks < 1)
			{
				OutError = FString::Printf(TEXT("%s status condition is incomplete: %s."), Context, *CardId.ToString());
				return false;
			}
			break;
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:
			if (Condition.MinimumArmor < 1 || Condition.MinimumStatusStacks != 0)
			{
				OutError = FString::Printf(TEXT("%s armor condition must use MinimumArmor only: %s."), Context, *CardId.ToString());
				return false;
			}
			break;
		case EGameXXKCardEffectConditionType::TerrainIsAny:
			if (Condition.Terrain == EGameXXKCardTerrain::Invalid)
			{
				OutError = FString::Printf(TEXT("%s TerrainIsAny condition has no terrain: %s."), Context, *CardId.ToString());
				return false;
			}
			break;
		case EGameXXKCardEffectConditionType::None:
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:
		case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:
		case EGameXXKCardEffectConditionType::TargetIsAlly:
		case EGameXXKCardEffectConditionType::TargetIsEnemy:
			break;
		default:
			OutError = FString::Printf(TEXT("%s condition type is invalid: %s."), Context, *CardId.ToString());
			return false;
		}

		if (Condition.bConsumeStatus)
		{
			if ((Condition.Type != EGameXXKCardEffectConditionType::TargetHasStatus && Condition.Type != EGameXXKCardEffectConditionType::OwnerHasStatus) || !IsConcreteStatus(Condition.Status) || Condition.MinimumStatusStacks < 1)
			{
				OutError = FString::Printf(TEXT("%s status consumption must use a status condition: %s."), Context, *CardId.ToString());
				return false;
			}
			if (Condition.MaxConsumedStatusStacks < 0)
			{
				OutError = FString::Printf(TEXT("%s status consumption maximum is negative: %s."), Context, *CardId.ToString());
				return false;
			}
		}
		else if (Condition.bScaleMagnitudeByConsumedStacks)
		{
			OutError = FString::Printf(TEXT("%s scales by consumed status stacks without consuming status: %s."), Context, *CardId.ToString());
			return false;
		}

		if (Condition.bConsumeOwnerArmor)
		{
			if (Condition.Type != EGameXXKCardEffectConditionType::OwnerArmorAtLeast || Condition.MinimumArmor < 1 || Condition.MaxConsumedArmor < 0)
			{
				OutError = FString::Printf(TEXT("%s armor consumption must use an armor condition: %s."), Context, *CardId.ToString());
				return false;
			}
			if (Condition.bConsumeStatus || Condition.bScaleMagnitudeByConsumedStacks)
			{
				OutError = FString::Printf(TEXT("%s cannot mix armor and status consumption: %s."), Context, *CardId.ToString());
				return false;
			}
		}

		return true;
	}

	void AddCard(
		TArray<FGameXXKCardDefinition>& Cards,
		const EGameXXKCardOwner Owner,
		const EGameXXKCardRarity Rarity,
		const EGameXXKCharacterRole Role,
		const TCHAR* OwnerId,
		const TCHAR* NpcId,
		const TCHAR* CardId,
		const TCHAR* DisplayName,
		const int32 EnergyCost,
		const int32 ManaCost,
		const EGameXXKCardTargetMode TargetMode,
		TArray<FGameXXKCardEffect> Effects,
		const TCHAR* FrameKey,
		const TCHAR* AcquisitionKey,
		const bool bCoreProfessionCard = false,
		const bool bIdentityLocked = false,
		TArray<FGameXXKCardTargetModeOverride> TargetModeOverrides = {},
		const EGameXXKCharacterRole LinkedRole = EGameXXKCharacterRole::Invalid,
		const int32 HeroUnlockLevel = 0,
		const bool bExhaustOnPlay = false,
		TArray<FGameXXKCardEffect> ChargeEffects = {},
		TArray<FGameXXKCardEffect> FinishEffects = {},
		FGameXXKHeavyArrowRule HeavyArrowRule = {},
		const EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None,
		TArray<FGameXXKCardEffect> TaskNpcRewardEffects = {},
		FGameXXKHunterCardRule HunterCardRule = {},
		FGameXXKHealerCardRule HealerCardRule = {},
		FGameXXKSorcererCardRule SorcererCardRule = {})
	{
		FGameXXKCardDefinition Definition;
		Definition.Id = FName(CardId);
		Definition.DisplayName = FText::FromString(FString(DisplayName));
		Definition.Owner = Owner;
		Definition.Rarity = Rarity;
		Definition.BaseQuality = FGameXXKCardQualityRules::GetCardBaseQuality(Definition.Id);
		Definition.Role = Role;
		Definition.OwnerId = FName(OwnerId);
		Definition.NpcId = NpcId ? FName(NpcId) : NAME_None;
		Definition.EnergyCost = EnergyCost;
		Definition.ManaCost = ManaCost;
		Definition.TargetSpec = Target(TargetMode);
		Definition.TargetSpec.ModeOverrides = MoveTemp(TargetModeOverrides);
		Definition.Effects = MoveTemp(Effects);
		Definition.VisualArtKey = FName(*FString::Printf(TEXT("Art.%s"), CardId));
		Definition.FrameKey = FName(FrameKey);
		Definition.AcquisitionKey = FName(AcquisitionKey);
		Definition.bCoreProfessionCard = bCoreProfessionCard;
		Definition.bIdentityLocked = bIdentityLocked;
		Definition.LinkedRole = LinkedRole;
		Definition.HeroUnlockLevel = HeroUnlockLevel;
		Definition.bExhaustOnPlay = bExhaustOnPlay;
		Definition.ChargeEffects = MoveTemp(ChargeEffects);
		Definition.FinishEffects = MoveTemp(FinishEffects);
		Definition.HeavyArrow = MoveTemp(HeavyArrowRule);
		Definition.HunterRule = MoveTemp(HunterCardRule);
		Definition.HealerRule = MoveTemp(HealerCardRule);
		Definition.SorcererRule = MoveTemp(SorcererCardRule);
		Definition.SpellTaskReward = SpellTaskReward;
		Definition.TaskNpcRewardEffects = MoveTemp(TaskNpcRewardEffects);
		Cards.Add(MoveTemp(Definition));
	}

	void AddQuestNpcCard(
		TArray<FGameXXKCardDefinition>& Cards,
		const TCHAR* OwnerId,
		const TCHAR* CardId,
		const TCHAR* DisplayName,
		const int32 EnergyCost,
		const int32 ManaCost,
		const EGameXXKCardTargetMode TargetMode,
		TArray<FGameXXKCardEffect> Effects,
		const EGameXXKCharacterRole LinkedRole = EGameXXKCharacterRole::Invalid,
		TArray<FGameXXKCardEffect> ChargeEffects = {},
		TArray<FGameXXKCardEffect> FinishEffects = {},
		FGameXXKHeavyArrowRule HeavyArrowRule = {},
		TArray<FGameXXKCardEffect> TaskNpcRewardEffects = {})
	{
		AddCard(
			Cards,
			EGameXXKCardOwner::QuestNpc,
			EGameXXKCardRarity::Permanent,
			EGameXXKCharacterRole::QuestNpc,
			OwnerId,
			OwnerId,
			CardId,
			DisplayName,
			EnergyCost,
			ManaCost,
			TargetMode,
			MoveTemp(Effects),
			TEXT("Style.QuestNpc"),
			OwnerId,
			false,
			true,
			{},
			LinkedRole,
			0,
			false,
			MoveTemp(ChargeEffects),
			MoveTemp(FinishEffects),
			MoveTemp(HeavyArrowRule),
			EGameXXKHeroSpellTaskReward::None,
			MoveTemp(TaskNpcRewardEffects));
	}

	void AddHeroCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Hero");
		constexpr const TCHAR* Frame = TEXT("Style.Hero");
		const auto AddHero = [&](
			const TCHAR* CardId,
			const TCHAR* DisplayName,
			const int32 EnergyCost,
			const int32 ManaCost,
			const EGameXXKCardTargetMode TargetMode,
			TArray<FGameXXKCardEffect> Effects,
			const EGameXXKCharacterRole LinkedRole,
			const int32 UnlockLevel,
			const bool bExhaustOnPlay = false,
			TArray<FGameXXKCardEffect> ChargeEffects = {},
			TArray<FGameXXKCardEffect> FinishEffects = {},
			FGameXXKHeavyArrowRule HeavyArrowRule = {},
			const EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None)
		{
			const FString AcquisitionKey = UnlockLevel == 1
				? TEXT("Unlock.Initial")
				: FString::Printf(TEXT("Unlock.Level.%02d"), UnlockLevel);
			AddCard(
				Cards,
				EGameXXKCardOwner::Hero,
				EGameXXKCardRarity::Permanent,
				EGameXXKCharacterRole::Hero,
				OwnerId,
				nullptr,
				CardId,
				DisplayName,
				EnergyCost,
				ManaCost,
				TargetMode,
				MoveTemp(Effects),
				Frame,
				*AcquisitionKey,
				false,
				true,
				{},
				LinkedRole,
				UnlockLevel,
				bExhaustOnPlay,
				MoveTemp(ChargeEffects),
				MoveTemp(FinishEffects),
				MoveTemp(HeavyArrowRule),
				SpellTaskReward);
		};

		FGameXXKCardEffect QingFengDiscount = Modifier(
			EGameXXKCardBattleModifierTrigger::OnCardPlayed,
			EGameXXKCardEffectType::ModifyEnergyCost,
			EGameXXKCardEffectTarget::PlayedCard,
			-1,
			1,
			0,
			{},
			EGameXXKCardStatus::None,
			EGameXXKCardModifierRecipientScope::SharedDeck,
			EGameXXKCardEffectTarget::PlayedCard);
		QingFengDiscount = WithModifierPolicy(MoveTemp(QingFengDiscount), true, true);
		AddHero(TEXT("Hero.Generic.QingFengYiShi"), TEXT("青锋一式"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(140, EGameXXKCardEffectTarget::SelectedTarget), MoveTemp(QingFengDiscount)}, EGameXXKCharacterRole::Invalid, 1);

		AddHero(TEXT("Hero.Generic.HeYuZhan"), TEXT("鹤羽斩"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(160, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::TriggerHighestDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget)}, EGameXXKCharacterRole::Invalid, 1);
		AddHero(TEXT("Hero.Generic.FengShenBu"), TEXT("风身步"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Invalid, 1, true);
		AddHero(TEXT("Hero.Generic.SuiYanJi"), TEXT("碎岩击"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark)}, EGameXXKCharacterRole::Invalid, 1);

		FGameXXKCardEffect GuiYuanDiscount = Modifier(
			EGameXXKCardBattleModifierTrigger::OnCardPlayed,
			EGameXXKCardEffectType::ModifyEnergyCost,
			EGameXXKCardEffectTarget::PlayedCard,
			-1,
			1,
			0,
			{},
			EGameXXKCardStatus::None,
			EGameXXKCardModifierRecipientScope::SelectedTarget,
			EGameXXKCardEffectTarget::SelectedTarget);
		GuiYuanDiscount = WithModifierPolicy(MoveTemp(GuiYuanDiscount), true);
		AddHero(TEXT("Hero.Generic.GuiYuanShu"), TEXT("归元术"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 12), Effect(EGameXXKCardEffectType::Cleanse, EGameXXKCardEffectTarget::SelectedTarget), MoveTemp(GuiYuanDiscount)}, EGameXXKCharacterRole::Invalid, 1);
		AddHero(TEXT("Hero.Generic.HengJianShouShi"), TEXT("横剑守势"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16), Reaction(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Block, 1)}, EGameXXKCharacterRole::Invalid, 1);
		AddHero(TEXT("Hero.Generic.NingShenTuNa"), TEXT("凝神吐纳"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 10)}, EGameXXKCharacterRole::Invalid, 1, true);
		AddHero(TEXT("Hero.Generic.GuanXi"), TEXT("观隙"), 0, 0, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Invalid, 1, true);

		constexpr const TCHAR* PoYunAgility = TEXT("Result.PoYun.Agility");
		AddHero(TEXT("Hero.Generic.PoYunYiShan"), TEXT("破云一闪"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(160, EGameXXKCardEffectTarget::SelectedTarget), WithConsumptionProducer(Attack(100, EGameXXKCardEffectTarget::SelectedTarget, 1, ConsumeOwnerStatus(EGameXXKCardStatus::Agility, 1)), PoYunAgility), WithConsumedStackResult(Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), PoYunAgility)}, EGameXXKCharacterRole::Invalid, 5);
		AddHero(TEXT("Hero.Generic.XingQiHuiHuan"), TEXT("行气回环"), 0, 0, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Invalid, 10, true);

		constexpr const TCHAR* JianYiMomentum = TEXT("Result.JianYi.Momentum");
		FGameXXKCardEffect JianYiEnergy = WithConsumedStackResult(Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1), JianYiMomentum);
		JianYiEnergy.SecondaryMagnitude = 3;
		AddHero(TEXT("Hero.Generic.JianYiGuanHong"), TEXT("剑意贯虹"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(260, EGameXXKCardEffectTarget::SelectedTarget), WithConsumptionProducer(Effect(EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus, EGameXXKCardEffectTarget::SelectedTarget, 20, EGameXXKCardStatus::None, 1, ConsumeOwnerStatus(EGameXXKCardStatus::Momentum, 0)), JianYiMomentum), MoveTemp(JianYiEnergy)}, EGameXXKCharacterRole::Invalid, 15);
		AddHero(TEXT("Hero.Generic.GuiYuanFanZhao"), TEXT("归元返照"), 2, 6, EGameXXKCardTargetMode::AllAllies,
			{Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 6), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 12), Effect(EGameXXKCardEffectType::Cleanse, EGameXXKCardEffectTarget::AllAllies), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2)}, EGameXXKCharacterRole::Invalid, 20);

		FGameXXKCardEffect ReplayNext = WithModifierPolicy(Modifier(
			EGameXXKCardBattleModifierTrigger::AfterNextActiveCard,
			EGameXXKCardEffectType::ReplayTriggeredCardBase,
			EGameXXKCardEffectTarget::PlayedCard,
			1,
			1,
			0,
			{},
			EGameXXKCardStatus::None,
			EGameXXKCardModifierRecipientScope::SharedDeck,
			EGameXXKCardEffectTarget::PlayedCard), true);
		FGameXXKCardEffect ReplaySourceNextRound = WithModifierPolicy(Modifier(
			EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound,
			EGameXXKCardEffectType::ReplaySourceCardBase,
			EGameXXKCardEffectTarget::CardOwner,
			1,
			1), true);
		AddHero(TEXT("Hero.Blade.TongFengYinShi"), TEXT("同锋引式"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Momentum)}, EGameXXKCharacterRole::Blade, 1, false, {MoveTemp(ReplayNext)}, {MoveTemp(ReplaySourceNextRound)});

		FGameXXKCardEffect TriggerBleed = WithModifierPolicy(Modifier(
			EGameXXKCardBattleModifierTrigger::OnNextAttack,
			EGameXXKCardEffectType::TriggerStatus,
			EGameXXKCardEffectTarget::SelectedTarget,
			1,
			1,
			0,
			TargetHasStatus(EGameXXKCardStatus::Bleed),
			EGameXXKCardStatus::Bleed,
			EGameXXKCardModifierRecipientScope::SharedDeck,
			EGameXXKCardEffectTarget::PlayedCard), true, false, true);
		FGameXXKCardEffect FinishBleedDraw = WithModifierPolicy(Modifier(
			EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound,
			EGameXXKCardEffectType::DrawCards,
			EGameXXKCardEffectTarget::CardOwner,
			2,
			1,
			0,
			TargetHasStatus(EGameXXKCardStatus::Bleed)), true);
		FGameXXKCardEffect FinishBleedEnergy = WithModifierPolicy(Modifier(
			EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound,
			EGameXXKCardEffectType::GainEnergy,
			EGameXXKCardEffectTarget::CardOwner,
			1,
			1,
			0,
			TargetHasStatus(EGameXXKCardStatus::Bleed)), true);
		AddHero(TEXT("Hero.Blade.XueLuXiangCheng"), TEXT("血路相承"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed)}, EGameXXKCharacterRole::Blade, 1, false, {MoveTemp(TriggerBleed)}, {MoveTemp(FinishBleedDraw), MoveTemp(FinishBleedEnergy)});

		FGameXXKCardEffect ChargeAgility = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::PlayedCard, 2, 1, 0, {}, EGameXXKCardStatus::Agility, EGameXXKCardModifierRecipientScope::SharedDeck, EGameXXKCardEffectTarget::PlayedCard), true);
		FGameXXKCardEffect ChargeCounter = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::PlayedCard, 1, 1, 0, {}, EGameXXKCardStatus::Counter, EGameXXKCardModifierRecipientScope::SharedDeck, EGameXXKCardEffectTarget::PlayedCard), true);
		FGameXXKCardEffect FinishMark = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, 1, 0, {}, EGameXXKCardStatus::Mark), false);
		FGameXXKCardEffect FinishCounter = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::RegisterReaction, EGameXXKCardEffectTarget::CardOwner, 2, 1, 0, {}, EGameXXKCardStatus::Counter), false);
		AddHero(TEXT("Hero.Blade.YingFengHuanBu"), TEXT("迎锋换步"), 1, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Agility), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 2)}, EGameXXKCharacterRole::Blade, 1, false, {MoveTemp(ChargeAgility), MoveTemp(ChargeCounter)}, {MoveTemp(FinishMark), MoveTemp(FinishCounter)});

		FGameXXKCardEffect MomentumAttack = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus, EGameXXKCardEffectTarget::SelectedTarget, 10, 1, 0, OwnerHasStatus(EGameXXKCardStatus::Momentum), EGameXXKCardStatus::Momentum), true, false, true);
		FGameXXKCardEffect ChargeMomentum = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::PlayedCard, 3, 1, 0, {}, EGameXXKCardStatus::Momentum, EGameXXKCardModifierRecipientScope::SharedDeck, EGameXXKCardEffectTarget::PlayedCard), true);
		FGameXXKCardEffect FinishFree = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound, EGameXXKCardEffectType::ModifyEnergyCost, EGameXXKCardEffectTarget::PlayedCard, -99, 1, 0, {}, EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::SelectedTarget, EGameXXKCardEffectTarget::SelectedTarget), true);
		AddHero(TEXT("Hero.Blade.TongPaoJuShi"), TEXT("同袍聚势"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Momentum), MoveTemp(MomentumAttack)}, EGameXXKCharacterRole::Blade, 1, false, {MoveTemp(ChargeMomentum)}, {MoveTemp(FinishFree)});

		AddHero(TEXT("Hero.Guard.TieBiTongShou"), TEXT("铁壁同守"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 18), Reaction(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Block, 2)}, EGameXXKCharacterRole::Guard, 1);
		AddHero(TEXT("Hero.Guard.JieJiaHuanFeng"), TEXT("借甲还锋"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{WithSource(Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), EGameXXKCardEffectSource::HighestArmorAlly), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestArmorAlly, 10), Reaction(EGameXXKCardEffectTarget::HighestArmorAlly, EGameXXKCardStatus::Block, 1)}, EGameXXKCharacterRole::Guard, 1);
		AddHero(TEXT("Hero.Guard.LieZhenChengFeng"), TEXT("列阵承锋"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8), Reaction(EGameXXKCardEffectTarget::AllAllies, EGameXXKCardStatus::Block, 1)}, EGameXXKCharacterRole::Guard, 1);
		AddHero(TEXT("Hero.Guard.XuanJiaZhenYue"), TEXT("玄甲镇岳"), 2, 6, EGameXXKCardTargetMode::SingleAlly,
			{WithSource(EffectWithSecondary(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 100, 20), EGameXXKCardEffectSource::SelectedTarget)}, EGameXXKCharacterRole::Guard, 1);

		AddHero(TEXT("Hero.Healer.YiXueCuiFang"), TEXT("以血催方"), 0, 0, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::LoseHealthNonlethal, EGameXXKCardEffectTarget::AllAllies, 1), EffectWithSecondary(EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, EGameXXKCardEffectTarget::CardOwner, 2, 6), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Healer, 1);
		AddHero(TEXT("Hero.Healer.HuiChunNiMai"), TEXT("回春逆脉"), 1, 3, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 10), Effect(EGameXXKCardEffectType::Cleanse, EGameXXKCardEffectTarget::SelectedTarget)}, EGameXXKCharacterRole::Healer, 1);
		AddHero(TEXT("Hero.Healer.DuHuoTongLu"), TEXT("毒火同炉"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(130, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine)}, EGameXXKCharacterRole::Healer, 1);
		AddHero(TEXT("Hero.Healer.BaiCaoJiZhen"), TEXT("百草济阵"), 2, 6, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 6), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn)}, EGameXXKCharacterRole::Healer, 1);

		AddHero(TEXT("Hero.Hunter.FengYanDingXian"), TEXT("风眼定弦"), 0, 3, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Charge)}, EGameXXKCharacterRole::Hunter, 1);
		AddHero(TEXT("Hero.Hunter.LieYuLianShi"), TEXT("裂羽连矢"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(140, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed)}, EGameXXKCharacterRole::Hunter, 1, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 50));
		AddHero(TEXT("Hero.Hunter.CuiDuChuanXin"), TEXT("淬毒穿心"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(130, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget)}, EGameXXKCharacterRole::Hunter, 1, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::ToxicExplosionPerCharge, 1));
		AddHero(TEXT("Hero.Hunter.HuiFengGuanRi"), TEXT("回风贯日"), 1, 6, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(150, EGameXXKCardEffectTarget::SelectedTarget)}, EGameXXKCharacterRole::Hunter, 1, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 40, 1, 3, 1));

		AddHero(TEXT("Hero.Mage.YanXuLiaoYuan"), TEXT("炎序燎原"), 1, 3, EGameXXKCardTargetMode::AllEnemies,
			{Attack(100, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Sorcerer, 1, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::Fire);
		AddHero(TEXT("Hero.Mage.HanXuNingChuan"), TEXT("寒序凝川"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::GainArmorFromCurrentManaPercent, EGameXXKCardEffectTarget::CardOwner, 25), EffectWithSecondary(EGameXXKCardEffectType::GainManaOverflowToArmor, EGameXXKCardEffectTarget::CardOwner, 100, 6)}, EGameXXKCharacterRole::Sorcerer, 1, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::Ice);
		AddHero(TEXT("Hero.Mage.LeiXuYinTing"), TEXT("雷序引霆"), 1, 3, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 30, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Sorcerer, 1, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::Lightning);
		AddHero(TEXT("Hero.Mage.GuiXuTongXuan"), TEXT("归序通玄"), 0, 0, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::Sorcerer, 1, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::Universal);

		AddHero(TEXT("Hero.Formation.GuanShiLuoZi"), TEXT("观势落子"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(80, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)}, EGameXXKCharacterRole::FormationMaster, 1);
		FGameXXKCardEffect TerrainBenefit = WithResultProducer(EffectWithSecondary(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 2, 3), TEXT("Result.TerrainChanged"));
		AddHero(TEXT("Hero.Formation.YiZhenHuiXiang"), TEXT("移阵回响"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{MoveTemp(TerrainBenefit), WithResultReference(Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1), TEXT("Result.TerrainChanged"))}, EGameXXKCharacterRole::FormationMaster, 1);
		FGameXXKCardEffect TerrainListener = WithModifierPolicy(Modifier(EGameXXKCardBattleModifierTrigger::AfterEachActiveCard, EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1, 3, 0, {}, EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::SelectedTarget, EGameXXKCardEffectTarget::SelectedTarget), true);
		AddHero(TEXT("Hero.Formation.LianYingBuShi"), TEXT("连营布势"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{MoveTemp(TerrainListener)}, EGameXXKCharacterRole::FormationMaster, 1);
		AddHero(TEXT("Hero.Formation.LiuHeGuiYi"), TEXT("六合归一"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::Plain), WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::Cliff), WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::Forest), WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::WaterShore), WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::Village), WithTerrain(Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1), EGameXXKCardTerrain::Cave), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1)}, EGameXXKCharacterRole::FormationMaster, 1);
	}

	void AddQuestNpcCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* Tusi = TEXT("Npc.TusiChief");
		const auto SharedPlayedModifier = [](
			const EGameXXKCardBattleModifierTrigger Trigger,
			const EGameXXKCardEffectType Type,
			const int32 Magnitude,
			const int32 RemainingTriggers,
			const EGameXXKCardStatus Status,
			const EGameXXKCardModifierExpiry Expiry)
		{
			return WithModifierPolicy(Modifier(
				Trigger,
				Type,
				EGameXXKCardEffectTarget::PlayedCard,
				Magnitude,
				RemainingTriggers,
				0,
				{},
				Status,
				EGameXXKCardModifierRecipientScope::SharedDeck,
				EGameXXKCardEffectTarget::PlayedCard,
				Expiry), true);
		};
		const auto OwnerModifier = [](
			const EGameXXKCardBattleModifierTrigger Trigger,
			const EGameXXKCardEffectType Type,
			const int32 Magnitude,
			const int32 RemainingTriggers,
			const EGameXXKCardStatus Status)
		{
			return WithModifierPolicy(Modifier(
				Trigger,
				Type,
				EGameXXKCardEffectTarget::CardOwner,
				Magnitude,
				RemainingTriggers,
				0,
				{},
				Status), true);
		};

		AddQuestNpcCard(Cards, Tusi, TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("寨主号令"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 1, EGameXXKCardStatus::Momentum),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestAttackAlly, 8),
			 Reaction(EGameXXKCardEffectTarget::HighestAttackAlly, EGameXXKCardStatus::Block, 1),
			 WithSource(Attack(100, EGameXXKCardEffectTarget::SelectedTarget), EGameXXKCardEffectSource::HighestAttackAlly)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::AfterNextActiveCard, EGameXXKCardEffectType::ReplayTriggeredCardBase, 1, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6)});
		AddQuestNpcCard(Cards, Tusi, TEXT("Npc.TusiChief.ShiMenShouShi"), TEXT("石门守势"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16),
			 Reaction(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Block, 2)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::AddArmor, 12, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::RegisterReaction, 1, 1, EGameXXKCardStatus::Block, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{Effect(EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks, EGameXXKCardEffectTarget::SelectedTarget, 1)});
		AddQuestNpcCard(Cards, Tusi, TEXT("Npc.TusiChief.TuSiJunLing"), TEXT("土司军令"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestAttackAlly, 8),
			 Reaction(EGameXXKCardEffectTarget::HighestAttackAlly, EGameXXKCardStatus::Block, 1),
			 WithSource(Attack(150, EGameXXKCardEffectTarget::SelectedTarget), EGameXXKCardEffectSource::HighestAttackAlly)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::WidenNextActiveSingleTarget, 1, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{Effect(EGameXXKCardEffectType::PreserveNextReactionUse, EGameXXKCardEffectTarget::AllAllies, 1)});
		AddQuestNpcCard(Cards, Tusi, TEXT("Npc.TusiChief.MengZhaiShiYue"), TEXT("盟寨誓约"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Momentum),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8),
			 Reaction(EGameXXKCardEffectTarget::AllAllies, EGameXXKCardStatus::Block, 1),
			 Effect(EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 60)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ModifyEnergyCost, -99, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ModifyManaCost, -99, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{Effect(EGameXXKCardEffectType::RetainArmorNextRound, EGameXXKCardEffectTarget::AllAllies, 1)});

		constexpr const TCHAR* Song = TEXT("Npc.SongJinBao");
		AddQuestNpcCard(Cards, Song, TEXT("Npc.SongJinBao.ShangQianGuWu"), TEXT("赏钱鼓舞"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Momentum),
			 Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6),
			 WithSource(Attack(100, EGameXXKCardEffectTarget::PriorityEnemy), EGameXXKCardEffectSource::SelectedTarget)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::AfterNextActiveCard, EGameXXKCardEffectType::ReplayTriggeredCardBase, 1, 1, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{OwnerModifier(EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound, EGameXXKCardEffectType::ReplaySourceCardBase, 1, 1, EGameXXKCardStatus::None)},
			{},
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 2, EGameXXKCardStatus::Momentum),
			 Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2),
			 Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 2)});
		AddQuestNpcCard(Cards, Song, TEXT("Npc.SongJinBao.ErMuMiBao"), TEXT("耳目密报"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::RevealEnemyIntent, EGameXXKCardEffectTarget::CardOwner, 99),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 100)});
		AddQuestNpcCard(Cards, Song, TEXT("Npc.SongJinBao.GuiKeLing"), TEXT("贵客令"), 1, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility),
			 Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 1),
			 Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Blade,
			{SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::ApplyStatus, 2, 1, EGameXXKCardStatus::Agility, EGameXXKCardModifierExpiry::AfterTriggerCount),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard, EGameXXKCardEffectType::RegisterReaction, 1, 1, EGameXXKCardStatus::Counter, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			{OwnerModifier(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::ApplyStatus, 2, 1, EGameXXKCardStatus::Mark),
			 OwnerModifier(EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart, EGameXXKCardEffectType::RegisterReaction, 2, 1, EGameXXKCardStatus::Counter)},
			{},
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::Agility),
			 Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 3)});
		AddQuestNpcCard(Cards, Song, TEXT("Npc.SongJinBao.YiNuoQianJin"), TEXT("一诺千金"), 1, 6, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyEnergyCost, -99, 2, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyManaCost, -99, 2, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::AfterTriggerCount)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3),
			 Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 2),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyEnergyCost, -99, 0, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::EndOfCurrentRound),
			 SharedPlayedModifier(EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyManaCost, -99, 0, EGameXXKCardStatus::None, EGameXXKCardModifierExpiry::EndOfCurrentRound)});

		constexpr const TCHAR* YueBai = TEXT("Npc.YueBai");
		AddQuestNpcCard(Cards, YueBai, TEXT("Npc.YueBai.QingYanDianDeng"), TEXT("青焰点灯"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Burn),
			 Effect(EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn),
			 Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 6, EGameXXKCardStatus::Burn),
			 Effect(EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn)});
		AddQuestNpcCard(Cards, YueBai, TEXT("Npc.YueBai.CanJuanPiZhu"), TEXT("残卷批注"), 0, 0, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2),
			 Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1),
			 Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 3)});
		AddQuestNpcCard(Cards, YueBai, TEXT("Npc.YueBai.YueBaiZhaoYe"), TEXT("月白照夜"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Burn),
			 Attack(100, EGameXXKCardEffectTarget::SelectedTarget),
			 Effect(EGameXXKCardEffectType::TriggerStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn),
			 Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 50, EGameXXKCardStatus::Mark)});
		AddQuestNpcCard(Cards, YueBai, TEXT("Npc.YueBai.ShanHeCanTu"), TEXT("山河残图"), 0, 6, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8),
			 Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3),
			 Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1),
			 Effect(EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid,
			{}, {}, {},
			{EffectWithSecondary(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 0, 20)});

		constexpr const TCHAR* Zhou = TEXT("Npc.ZhouGuangZu");
		AddQuestNpcCard(Cards, Zhou, TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), TEXT("异草辨识"), 0, 0, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 6),
			 Effect(EGameXXKCardEffectType::CleanseFriendlyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1)});
		AddQuestNpcCard(Cards, Zhou, TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), TEXT("黄山敷治"), 1, 3, EGameXXKCardTargetMode::AllAllies,
			{Effect(EGameXXKCardEffectType::LoseHealthNonlethal, EGameXXKCardEffectTarget::AllAllies, 1),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 6)});
		AddQuestNpcCard(Cards, Zhou, TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), TEXT("地志摹图"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2),
			 Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 2)});
		AddQuestNpcCard(Cards, Zhou, TEXT("Npc.ZhouGuangZu.YanFenFengMai"), TEXT("岩粉封脉"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1)});

		constexpr const TCHAR* JinGui = TEXT("Npc.JinGui");
		AddQuestNpcCard(Cards, JinGui, TEXT("Npc.JinGui.ShiJingErMu"), TEXT("市井耳目"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Charge)},
			EGameXXKCharacterRole::Invalid, {}, {},
			HeavyArrow(EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 50, 0, 0, 0, EGameXXKHeavyArrowChargeSource::HighestAttackAlly, 0, EGameXXKHeavyArrowLockTiming::AfterBaseEffects));
		AddQuestNpcCard(Cards, JinGui, TEXT("Npc.JinGui.QiaoYanZhouXuan"), TEXT("巧言周旋"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::HighestArmorAlly, 12),
			 Reaction(EGameXXKCardEffectTarget::HighestArmorAlly, EGameXXKCardStatus::Block, 2)});
		AddQuestNpcCard(Cards, JinGui, TEXT("Npc.JinGui.ZaYiChouBei"), TEXT("杂役筹备"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3),
			 Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1),
			 Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 3, EGameXXKCardStatus::Charge)},
			EGameXXKCharacterRole::Invalid, {}, {},
			HeavyArrow(EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 40, 0, 0, 0, EGameXXKHeavyArrowChargeSource::HighestAttackAlly, 1, EGameXXKHeavyArrowLockTiming::AfterBaseEffects));
		AddQuestNpcCard(Cards, JinGui, TEXT("Npc.JinGui.HouXiangTuoShen"), TEXT("后巷脱身"), 2, 6, EGameXXKCardTargetMode::None,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 2, EGameXXKCardStatus::Agility),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::LowestHealthAlly, 2, EGameXXKCardStatus::Mark),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 16),
			 Reaction(EGameXXKCardEffectTarget::LowestHealthAlly, EGameXXKCardStatus::Block, 2)});

		constexpr const TCHAR* Qiong = TEXT("Npc.QiongMeiEr");
		AddQuestNpcCard(Cards, Qiong, TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), TEXT("藤桥飞渡"), 0, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Agility),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::HighestAttackAlly, 2, EGameXXKCardStatus::Charge),
			 Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKCharacterRole::Invalid, {}, {},
			HeavyArrow(EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 50, 0, 0, 0, EGameXXKHeavyArrowChargeSource::HighestAttackAlly, 0, EGameXXKHeavyArrowLockTiming::AfterBaseEffects));
		AddQuestNpcCard(Cards, Qiong, TEXT("Npc.QiongMeiEr.GuWuMiZong"), TEXT("蛊雾迷踪"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Bleed),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1)});
		AddQuestNpcCard(Cards, Qiong, TEXT("Npc.QiongMeiEr.YinLingZhenXin"), TEXT("银铃镇心"), 1, 3, EGameXXKCardTargetMode::SingleAlly,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine),
			 Effect(EGameXXKCardEffectType::CleanseFriendlyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 12)});
		AddQuestNpcCard(Cards, Qiong, TEXT("Npc.QiongMeiEr.ShanGeHuanLing"), TEXT("山歌唤灵"), 2, 6, EGameXXKCardTargetMode::AllAllies,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::Medicine),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 6)});
	}

	void AddBladeCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Blade");
		constexpr const TCHAR* Frame = TEXT("Style.Blade");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Blade");
		const auto AddBlade = [&Cards, OwnerId, Frame, Pool](
			const TCHAR* CardId,
			const TCHAR* DisplayName,
			const int32 EnergyCost,
			const int32 ManaCost,
			const EGameXXKCardTargetMode TargetMode,
			TArray<FGameXXKCardEffect> Effects,
			const bool bCore,
			const EGameXXKBladeBaseRule BaseRule,
			const EGameXXKBladeChargeRule ChargeRule,
			const EGameXXKBladeFinishRule FinishRule)
		{
			AddCard(
				Cards,
				EGameXXKCardOwner::Profession,
				EGameXXKCardRarity::Permanent,
				EGameXXKCharacterRole::Blade,
				OwnerId,
				nullptr,
				CardId,
				DisplayName,
				EnergyCost,
				ManaCost,
				TargetMode,
				MoveTemp(Effects),
				Frame,
				Pool,
				bCore);
			FGameXXKBladeSequenceRule& Sequence = Cards.Last().BladeSequence;
			Sequence.BaseRule = BaseRule;
			Sequence.ChargeRule = ChargeRule;
			Sequence.FinishRule = FinishRule;
		};

		AddBlade(TEXT("Profession.Blade.LieFengZhan"), TEXT("裂风斩"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Bleed) }, true,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReplayNextActiveBase, EGameXXKBladeFinishRule::ReturnFirstActiveNextRound);
		AddBlade(TEXT("Profession.Blade.HuiFengJiaShi"), TEXT("回锋架势"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility) }, true,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::CopyNextActiveToHand, EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters);

		AddBlade(TEXT("Profession.Blade.FengHou"), TEXT("封喉"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 5, EGameXXKCardStatus::Bleed) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReturnNextActiveToHandOnce, EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers);
		AddBlade(TEXT("Profession.Blade.JiYuLianZhan"), TEXT("疾雨连斩"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Bleed), Attack(55, EGameXXKCardEffectTarget::SelectedTarget, 3) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReplayNextActiveNextRound, EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers);
		AddBlade(TEXT("Profession.Blade.YinXueDao"), TEXT("饮血刀"), 2, 4, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(120, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Bleed) }, false,
			EGameXXKBladeBaseRule::HealFromTriggeredBleed, EGameXXKBladeChargeRule::RestoreNextActiveOwnerState, EGameXXKBladeFinishRule::HealBladeBleedCapTwelve);
		AddBlade(TEXT("Profession.Blade.LangDuan"), TEXT("浪断"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget) }, false,
			EGameXXKBladeBaseRule::PreserveTriggeredBleed, EGameXXKBladeChargeRule::DuplicateNextSingleTargetOrDraw, EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding);

		AddBlade(TEXT("Profession.Blade.DuanYue"), TEXT("断岳"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(140, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Momentum) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::MakeNextActiveEnergyFree, EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay);
		AddBlade(TEXT("Profession.Blade.PoJun"), TEXT("破军"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(130, EGameXXKCardEffectTarget::SelectedTarget) }, false,
			EGameXXKBladeBaseRule::ConsumeVulnerabilityForExtraAttacks, EGameXXKBladeChargeRule::MakeNextActiveManaFree, EGameXXKBladeFinishRule::CopyFirstStatusConsumer);
		AddBlade(TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("战意沸腾"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 4) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RefundNextActiveCosts, EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo);
		AddBlade(TEXT("Profession.Blade.ZhanJin"), TEXT("斩尽"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(200, EGameXXKCardEffectTarget::SelectedTarget) }, false,
			EGameXXKBladeBaseRule::RefundCostsAndDrawOnKill, EGameXXKBladeChargeRule::CountNextActiveTwice, EGameXXKBladeFinishRule::CopyFirstKill);

		AddBlade(TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("借势回锋"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 1) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::CopyNextActiveNextRound, EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley);
		AddBlade(TEXT("Profession.Blade.ZhuYing"), TEXT("逐影"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(90, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 1) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RetainNextActiveNextRound, EGameXXKBladeFinishRule::FirstTwoDodgesFree);
		AddBlade(TEXT("Profession.Blade.PoLangTuJin"), TEXT("破浪突进"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(110, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Mark), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 1) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::PreserveFinishCandidate, EGameXXKBladeFinishRule::TransferMarkBeforeCounter);
		AddBlade(TEXT("Profession.Blade.YiShiDuanJiang"), TEXT("一式断江"), 2, 7, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(160, EGameXXKCardEffectTarget::SelectedTarget), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Counter, 1) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RetainRemainingHand, EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll);

		AddBlade(TEXT("Profession.Blade.JingHongChuQiao"), TEXT("惊鸿出鞘"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(90, EGameXXKCardEffectTarget::SelectedTarget) }, false,
			EGameXXKBladeBaseRule::OpenBladeExtraAttack, EGameXXKBladeChargeRule::LightLoad, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle);
		AddBlade(TEXT("Profession.Blade.HengYunKaiFeng"), TEXT("横云开锋"), 2, 6, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(70, EGameXXKCardEffectTarget::AllEnemies) }, false,
			EGameXXKBladeBaseRule::OpenBladeResidualStyle, EGameXXKBladeChargeRule::DrawTwoAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle);
		AddBlade(TEXT("Profession.Blade.LianXiGuiQiao"), TEXT("敛息归鞘"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 3) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle);
		AddBlade(TEXT("Profession.Blade.BaoDaoShouYe"), TEXT("抱刀守夜"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility) }, false,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle);
	}

	void AddGuardCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Guard");
		constexpr const TCHAR* Frame = TEXT("Style.Guard");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Guard");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieBi"), TEXT("铁壁"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 14), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 1) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.HuZhu"), TEXT("护主"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 8), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::SelectedTarget), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 1) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.ZhenDun"), TEXT("震盾"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.GuShou"), TEXT("固守"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.FanZhenJia"), TEXT("反震甲"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 12), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.ZhenYueLing"), TEXT("镇岳令"), 2, 6, EGameXXKCardTargetMode::AllEnemies,
			{ EffectWithSecondary(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 80, 20), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8), Reaction(EGameXXKCardEffectTarget::AllAllies, EGameXXKCardStatus::Block, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YuanHuBu"), TEXT("援护步"), 0, 0, EGameXXKCardTargetMode::LowestHealthAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 6), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::LowestHealthAlly), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.PiJiaXingJun"), TEXT("披甲行军"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6), Reaction(EGameXXKCardEffectTarget::AllAllies, EGameXXKCardStatus::Block, 1), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.QinWangDunJi"), TEXT("擒王盾击"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieBiRuShan"), TEXT("铁壁如山"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 24), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::CannotReceiveVulnerability), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.BiLeiFanGong"), TEXT("壁垒反攻"), 2, 6, EGameXXKCardTargetMode::AllEnemies,
			{ EffectWithSecondary(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 120, 20), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.BuDongRuShan"), TEXT("不动如山"), 3, 10, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 36), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 3), Effect(EGameXXKCardEffectType::RetainArmorNextRound, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.PanShiTuNa"), TEXT("磐石吐纳"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 5, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(8)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(8)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(8, true)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YuanJunBiLei"), TEXT("援军壁垒"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::SelectedTarget, 2), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.DunZhenTuiJin"), TEXT("盾阵推进"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieSuoHengJiang"), TEXT("铁锁横江"), 2, 6, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 20), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.SuiJiaHuiJi"), TEXT("碎甲回击"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 1), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(12)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YiFuDangGuan"), TEXT("一夫当关"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 20), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Reaction(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Block, 3) }, Frame, Pool);
	}

	void AddHealerCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Healer");
		constexpr const TCHAR* Frame = TEXT("Style.Healer");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Healer");
		const auto AddHealer = [&](const TCHAR* CardId, const TCHAR* DisplayName, const int32 EnergyCost,
			const int32 ManaCost, const EGameXXKCardTargetMode TargetMode, TArray<FGameXXKCardEffect> Effects,
			const EGameXXKHealerFormulaKind FormulaKind, const bool bCore = false)
		{
			AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent,
				EGameXXKCharacterRole::Healer, OwnerId, nullptr, CardId, DisplayName, EnergyCost, ManaCost,
				TargetMode, MoveTemp(Effects), Frame, Pool, bCore, false, {}, EGameXXKCharacterRole::Invalid,
				0, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::None, {}, {}, HealerRule(FormulaKind));
		};

		AddHealer(TEXT("Profession.Healer.YaoYin"), TEXT("阴阳药引"), 2, 0, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::None, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Poison, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Poison, 1, TargetIsEnemy()),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Burn, 1, TargetIsEnemy()),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::None, 1, TargetIsEnemy()),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TargetIsEnemy())},
			EGameXXKHealerFormulaKind::AnyHealthChangeMedicine, true);
		AddHealer(TEXT("Profession.Healer.XingQiZhen"), TEXT("行气活血"), 2, 3, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::LoseHealthNonlethal, EGameXXKCardEffectTarget::AllAllies, 1),
			 Effect(EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, EGameXXKCardEffectTarget::CardOwner, 1),
			 Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)},
			EGameXXKHealerFormulaKind::HighEnergyAndSixMedicine, true);

		AddHealer(TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("草木敷治"), 1, 2, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 8)}, EGameXXKHealerFormulaKind::FirstHealingMedicine);
		AddHealer(TEXT("Profession.Healer.QingXinSan"), TEXT("清心散"), 1, 3, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Bleed, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Poison, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 6)}, EGameXXKHealerFormulaKind::ThreeCleansedDotMedicine);
		AddHealer(TEXT("Profession.Healer.LingZhiXuMing"), TEXT("灵芝续命"), 2, 6, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 10),
			 Effect(EGameXXKCardEffectType::HealOrReverseFlat, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::None, 1, TargetHealthBelow(35.0f))}, EGameXXKHealerFormulaKind::LowHealthCrossMedicine);
		AddHealer(TEXT("Profession.Healer.HuiChunLu"), TEXT("回春露"), 2, 5, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTargetSide, 5)}, EGameXXKHealerFormulaKind::ThreeEffectiveHealsDraw);
		AddHealer(TEXT("Profession.Healer.ZhiXueCao"), TEXT("止血草"), 0, 2, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Bleed, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 4)}, EGameXXKHealerFormulaKind::BleedRemovedPartyArmor);
		AddHealer(TEXT("Profession.Healer.WenYangGao"), TEXT("温养膏"), 1, 3, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 10),
			 Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TargetIsAlly())}, EGameXXKHealerFormulaKind::LargeHealingArmorOrVulnerability);
		AddHealer(TEXT("Profession.Healer.JinChuangXuMing"), TEXT("金疮续命"), 2, 8, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTarget, 12),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility, 1, TargetIsAlly())}, EGameXXKHealerFormulaKind::LowHealthCrossAgility);
		AddHealer(TEXT("Profession.Healer.YaoWangGuiYuan"), TEXT("药王归元"), 2, 12, EGameXXKCardTargetMode::AnyLivingUnit,
			{Effect(EGameXXKCardEffectType::HealOrReverseWithMedicine, EGameXXKCardEffectTarget::SelectedTargetSide, 6),
			 Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTargetSide, 1, EGameXXKCardStatus::Bleed, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTargetSide, 1, EGameXXKCardStatus::Poison, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTargetSide, 1, EGameXXKCardStatus::Burn, 1, TargetIsAlly()),
			 Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTargetSide, 3, EGameXXKCardStatus::None, 1, TargetIsAlly())}, EGameXXKHealerFormulaKind::ThreeUnitHealthChangeDrawMana);

		AddHealer(TEXT("Profession.Healer.BaiCaoDu"), TEXT("百草毒"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison)}, EGameXXKHealerFormulaKind::PoisonDamageMedicine);
		AddHealer(TEXT("Profession.Healer.FuGuSan"), TEXT("腐骨散"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(60, EGameXXKCardEffectTarget::SelectedTarget),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Bleed),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability)}, EGameXXKHealerFormulaKind::BleedPoisonMark);
		AddHealer(TEXT("Profession.Healer.HuiQiXiang"), TEXT("蚀心香"), 1, 4, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Poison)}, EGameXXKHealerFormulaKind::GroupPoisonMedicineDraw);
		AddHealer(TEXT("Profession.Healer.LianQiaoJieDu"), TEXT("连翘引毒"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 5, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1)}, EGameXXKHealerFormulaKind::DualDotExplosionMedicine);
		AddHealer(TEXT("Profession.Healer.YaoJiuWenShen"), TEXT("红花透骨"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{Attack(70, EGameXXKCardEffectTarget::SelectedTarget),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed)}, EGameXXKHealerFormulaKind::TwoBleedPacketsMedicine);
		AddHealer(TEXT("Profession.Healer.YaoNangFeiTou"), TEXT("药囊飞投"), 2, 6, EGameXXKCardTargetMode::AllEnemies,
			{Attack(45, EGameXXKCardEffectTarget::AllEnemies),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Bleed),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Poison)}, EGameXXKHealerFormulaKind::GroupDirectDamageEnergy);
		AddHealer(TEXT("Profession.Healer.KuShenMaSan"), TEXT("苦参麻散"), 2, 4, EGameXXKCardTargetMode::SingleEnemy,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability)}, EGameXXKHealerFormulaKind::PoisonedVulnerabilityMedicineDraw);
		AddHealer(TEXT("Profession.Healer.WuWeiTiaoHe"), TEXT("五毒调和"), 2, 10, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Poison),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::AllEnemies, 1),
			 Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::AllEnemies, 1)}, EGameXXKHealerFormulaKind::TripleDotExplosionMomentumDraw);
	}

	void AddHunterCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Hunter");
		constexpr const TCHAR* Frame = TEXT("Style.Hunter");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Hunter");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.XunXiJian"), TEXT("寻隙箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(80, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowStatus(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25), EGameXXKCardStatus::Mark, 1, EGameXXKCardEffectTarget::SelectedTarget));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuBu"), TEXT("鹰眼"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Charge) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::None, {}, HunterRule(0, 0, 0, EGameXXKCardStatus::None, 0, 6));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ZhuiLie"), TEXT("追猎"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(75, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)), Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25, 0, 0, 0, EGameXXKHeavyArrowChargeSource::CardOwner, 2), EGameXXKHeroSpellTaskReward::None, {}, HunterRule(15));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YingYan"), TEXT("锐意感知"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::Charge) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LieWang"), TEXT("猎网"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ChuanYang"), TEXT("穿杨"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::IgnoreDefense, EGameXXKCardEffectTarget::SelectedTarget, 6) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowDefenseIgnore(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 50), 2));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LianZhuJian"), TEXT("连珠箭"), 1, 3, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison), Attack(50, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge) }, Frame, Pool, true, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::ExtraAttackPerCharge, 50));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuZuShi"), TEXT("淬毒矢"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(70, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ResolveToxicExplosion, EGameXXKCardEffectTarget::SelectedTarget, 1) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowPrimaryBonus(HeavyArrow(EGameXXKHeavyArrowKind::ToxicExplosionPerCharge, 1), 20));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YinZong"), TEXT("隐踪"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Charge) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, {}, EGameXXKHeroSpellTaskReward::None, {}, HunterRule(0, 0, 0, EGameXXKCardStatus::None, 0, 0, 2));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.DuanMaiShi"), TEXT("断脉矢"), 1, 4, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Bleed) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowBleedTriggers(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 30), 1));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ShouHun"), TEXT("狩魂"), 2, 8, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), EffectWithSecondary(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 20, MAX_int32, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowStatus(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 35), EGameXXKCardStatus::Mark, 1, EGameXXKCardEffectTarget::SelectedTarget));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.BaiBuChuanYang"), TEXT("百步穿杨"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(210, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark, 5)) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 40, 1));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LueYingJian"), TEXT("掠影箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(65, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)), Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowStatus(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 20), EGameXXKCardStatus::Agility, 1, EGameXXKCardEffectTarget::CardOwner), EGameXXKHeroSpellTaskReward::None, {}, HunterRule(0, 3, 0, EGameXXKCardStatus::Agility, 1));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LieHunBiao"), TEXT("猎魂标"), 0, 4, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Charge) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.PoJiaDing"), TEXT("破甲钉"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(75, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Poison) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowDefenseIgnore(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 25), 2));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.HuiHuanJian"), TEXT("回环箭"), 1, 2, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(60, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 15, 1, 0, 0, EGameXXKHeavyArrowChargeSource::CardOwner, 2), EGameXXKHeroSpellTaskReward::None, {}, HunterRule(0, 3, 1));
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuYeXianJing"), TEXT("腐叶陷阱"), 1, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Charge) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YingLuo"), TEXT("鹰落"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(200, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 100, EGameXXKCardStatus::None, 1, TargetHealthBelow(35.0f)) }, Frame, Pool, false, false, {}, EGameXXKCharacterRole::Invalid, 0, false, {}, {}, WithHeavyArrowHealthThreshold(HeavyArrow(EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge, 60), 5));
	}

	void AddSorcererCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Sorcerer");
		constexpr const TCHAR* Frame = TEXT("Style.Sorcerer");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Sorcerer");
		const auto AddSorcerer = [&Cards, OwnerId, Frame, Pool](
			const TCHAR* CardId,
			const TCHAR* DisplayName,
			const int32 EnergyCost,
			const int32 ManaCost,
			const EGameXXKCardTargetMode TargetMode,
			TArray<FGameXXKCardEffect> Effects,
			const bool bCore,
			const EGameXXKSorcererCardFamily Family,
			const EGameXXKSorcererSequenceRule SequenceRule,
			const EGameXXKSorcererRewardRule RewardRule)
		{
			AddCard(
				Cards,
				EGameXXKCardOwner::Profession,
				EGameXXKCardRarity::Permanent,
				EGameXXKCharacterRole::Sorcerer,
				OwnerId,
				nullptr,
				CardId,
				DisplayName,
				EnergyCost,
				ManaCost,
				TargetMode,
				MoveTemp(Effects),
				Frame,
				Pool,
				bCore);
			Cards.Last().SorcererRule = SorcererRule(Family, SequenceRule, RewardRule);
		};

		AddSorcerer(TEXT("Profession.Sorcerer.LingHuoFu"), TEXT("灵枢引法"), 1, 2, EGameXXKCardTargetMode::AllEnemies,
			{Attack(70, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, true,
			EGameXXKSorcererCardFamily::Core, EGameXXKSorcererSequenceRule::CoreSearch, EGameXXKSorcererRewardRule::CoreSearch);
		AddSorcerer(TEXT("Profession.Sorcerer.JuLing"), TEXT("周天归元"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 3)}, true,
			EGameXXKSorcererCardFamily::Core, EGameXXKSorcererSequenceRule::CoreManaEcho, EGameXXKSorcererRewardRule::CoreManaEcho);

		AddSorcerer(TEXT("Profession.Sorcerer.LiHuoYin"), TEXT("灵火点灯"), 0, 1, EGameXXKCardTargetMode::AllEnemies,
			{Attack(60, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Burn)}, false,
			EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireLamp, EGameXXKSorcererRewardRule::FireLamp);
		AddSorcerer(TEXT("Profession.Sorcerer.YanQiang"), TEXT("流焰传薪"), 0, 2, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Burn)}, false,
			EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireSpread, EGameXXKSorcererRewardRule::FireSpread);
		AddSorcerer(TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("焚脉爆炎"), 0, 4, EGameXXKCardTargetMode::AllEnemies,
			{Attack(80, EGameXXKCardEffectTarget::AllEnemies)}, false,
			EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireBurst, EGameXXKSorcererRewardRule::FireBurst);
		AddSorcerer(TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), TEXT("燎原寻诀"), 1, 2, EGameXXKCardTargetMode::AllEnemies,
			{Attack(40, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, false,
			EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireSearch, EGameXXKSorcererRewardRule::FireSearch);

		AddSorcerer(TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("寒息回流"), 1, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 1)}, false,
			EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceCurrentManaRestore, EGameXXKSorcererRewardRule::IceCurrentManaRestore);
		AddSorcerer(TEXT("Profession.Sorcerer.FenMaiFu"), TEXT("玄冰拓脉"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 4)}, false,
			EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceMaxMana, EGameXXKSorcererRewardRule::IceMaxMana);
		AddSorcerer(TEXT("Profession.Sorcerer.LingYanLianDan"), TEXT("霜镜叠甲"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 4)}, false,
			EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceArmorDouble, EGameXXKSorcererRewardRule::IceArmorDouble);
		AddSorcerer(TEXT("Profession.Sorcerer.HuLingMu"), TEXT("冰鉴索法"), 1, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::GainArmorFromCurrentManaPercent, EGameXXKCardEffectTarget::CardOwner, 25), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, false,
			EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceSearch, EGameXXKSorcererRewardRule::IceSearch);

		AddSorcerer(TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("引雷定标"), 0, 1, EGameXXKCardTargetMode::AllEnemies,
			{Attack(50, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Mark)}, false,
			EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningMark, EGameXXKSorcererRewardRule::LightningMark);
		AddSorcerer(TEXT("Profession.Sorcerer.FenTianJue"), TEXT("雷符索敌"), 1, 2, EGameXXKCardTargetMode::AllEnemies,
			{Attack(70, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Mark)}, false,
			EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningSearch, EGameXXKSorcererRewardRule::LightningSearch);
		AddSorcerer(TEXT("Profession.Sorcerer.NingYanChengRen"), TEXT("连霆穿云"), 0, 3, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 50, EGameXXKCardStatus::Mark)}, false,
			EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningMarkHits, EGameXXKSorcererRewardRule::LightningMarkHits);
		AddSorcerer(TEXT("Profession.Sorcerer.RanLingHuanYuan"), TEXT("雷走八方"), 0, 4, EGameXXKCardTargetMode::AllEnemies,
			{Effect(EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, EGameXXKCardEffectTarget::AllEnemies, 30, EGameXXKCardStatus::Mark)}, false,
			EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningStorm, EGameXXKSorcererRewardRule::LightningStorm);

		AddSorcerer(TEXT("Profession.Sorcerer.YanMuHuTi"), TEXT("万法归一"), 0, 5, EGameXXKCardTargetMode::AllEnemies,
			{Attack(60, EGameXXKCardEffectTarget::AllEnemies)}, false,
			EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalScalingAttack, EGameXXKSorcererRewardRule::UniversalScalingAttack);
		AddSorcerer(TEXT("Profession.Sorcerer.LieFu"), TEXT("照见五蕴"), 0, 0, EGameXXKCardTargetMode::Self,
			{Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1)}, false,
			EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalDraw, EGameXXKSorcererRewardRule::UniversalDraw);
		AddSorcerer(TEXT("Profession.Sorcerer.XingHuoHuiShou"), TEXT("六合护法"), 0, 4, EGameXXKCardTargetMode::AllAllies,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 3)}, false,
			EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalPartyArmor, EGameXXKSorcererRewardRule::UniversalPartyArmor);
		AddSorcerer(TEXT("Profession.Sorcerer.ChiYanFengJie"), TEXT("斗转星移"), 0, 2, EGameXXKCardTargetMode::AllEnemies,
			{Attack(65, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard, EGameXXKCardEffectTarget::CardOwner, 1)}, false,
			EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalSearch, EGameXXKSorcererRewardRule::UniversalSearch);
	}

	void AddFormationCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.FormationMaster");
		constexpr const TCHAR* Frame = TEXT("Style.FormationMaster");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.FormationMaster");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.GuanShi"), TEXT("平野观势"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::Plain), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.DingZhen"), TEXT("定阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::Village), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.YinShuiHuiYuan"), TEXT("引水回元"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::WaterShore), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.KunZhen"), TEXT("困阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::Cave), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinYingMiZong"), TEXT("林影迷踪"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::Forest), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.JieShanWeiZhang"), TEXT("借山为障"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ WithTerrain(Effect(EGameXXKCardEffectType::ChangeTerrain, EGameXXKCardEffectTarget::CardOwner, 1), EGameXXKCardTerrain::Cliff), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("村寨援阵"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 12), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("回声震杀"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(240, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("易位阵"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("八门轮转"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("镇煞阵"), 3, 10, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(320, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("万象归阵"), 3, 14, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 40), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextTerrainCardFree), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShanMenFengSuo"), TEXT("山门封锁"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShuiJingZheGuang"), TEXT("水镜折光"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 2), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinFengFuZhen"), TEXT("林风拂阵"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("阵旗鼓舞"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 20, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::AllAllies, EGameXXKCardEffectTarget::AllAllies), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.DiMaiJieLi"), TEXT("地脉借力"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(200, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::SelectedTarget, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.SiXiangLianHuan"), TEXT("四象连环"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 24), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::TriggerTerrainBenefit, EGameXXKCardEffectTarget::CardOwner, 2) }, Frame, Pool);
	}

	void AddRouteCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Route");
		constexpr const TCHAR* GeneralFrame = TEXT("Style.Route.General");
		constexpr const TCHAR* TerrainFrame = TEXT("Style.Route.Terrain");
		constexpr const TCHAR* RareFrame = TEXT("Style.Route.Rare");
		constexpr const TCHAR* BossFrame = TEXT("Style.Route.Boss");

		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.PoJiaTuCi"), TEXT("破甲突刺"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.ShouShiHuiYuan"), TEXT("守势回元"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 8), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 3) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.QingShenQuShi"), TEXT("轻身取势"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.TuNaJue"), TEXT("吐纳诀"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 5) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.ZhiXueSan"), TEXT("止血散"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 12), Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Bleed) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.FeiZhen"), TEXT("飞针"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(70, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.YanDun"), TEXT("烟遁"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 4) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.TieJiLi"), TEXT("铁蒺藜"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.LinZhenMoRen"), TEXT("临阵磨刃"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 25, 2, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::SelectedTarget, EGameXXKCardEffectTarget::SelectedTarget) }, GeneralFrame, TEXT("Route.General"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.General.HeJiLing"), TEXT("合击令"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget, EGameXXKCardEffectTarget::SelectedTarget, 50) }, GeneralFrame, TEXT("Route.General"));

		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.DuanYaLuoShi"), TEXT("断崖落石"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(130, EGameXXKCardEffectTarget::SelectedTarget, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Invalid, true)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Invalid, true)), Attack(180, EGameXXKCardEffectTarget::SelectedTarget, 1, TerrainIs(EGameXXKCardTerrain::Cliff)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.LinYingFuXi"), TEXT("林影伏袭"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Forest)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.DuKouHuiLiu"), TEXT("渡口回流"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.ZhaiHuoYuanShou"), TEXT("寨火援手"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 6, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.DongHuoZhaoMing"), TEXT("洞火照明"), 1, 0, EGameXXKCardTargetMode::AllEnemies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Burn, 1, TerrainIs(EGameXXKCardTerrain::Cave)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cave)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.JieShiTuXi"), TEXT("借势突袭"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(115, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark, 1, TerrainIs(EGameXXKCardTerrain::Forest)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Poison, 1, TerrainIs(EGameXXKCardTerrain::Cave)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Plain)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.XingJunBuZhen"), TEXT("行军布阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextTerrainCardEnergyReduction) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.DiMaiHuiXiang"), TEXT("地脉回响"), 0, 0, EGameXXKCardTargetMode::None,
			{ Effect(EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 2) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.LinShiZhaYing"), TEXT("临时扎营"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 8), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 2), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Forest, EGameXXKCardTerrain::Village)) }, TerrainFrame, TEXT("Route.Terrain"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Common, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Terrain.XianLuTuWei"), TEXT("险路突围"), 2, 6, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 8, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest)) }, TerrainFrame, TEXT("Route.Terrain"));

		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Rare, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Rare.GuJuanCanZhang"), TEXT("古卷残章"), 0, 0, EGameXXKCardTargetMode::None,
			{ Effect(EGameXXKCardEffectType::Insight, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::DiscoverCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ReorderCards, EGameXXKCardEffectTarget::CardOwner, 3) }, RareFrame, TEXT("Route.Rare"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Rare, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Rare.TieYiYiJue"), TEXT("铁衣遗诀"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 18), Modifier(EGameXXKCardBattleModifierTrigger::EndOfRound, EGameXXKCardEffectType::GainEnergy, EGameXXKCardEffectTarget::CardOwner, 1, 1, 0, OwnerArmorAtLeast(10)) }, RareFrame, TEXT("Route.Rare"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Rare, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Rare.LingQuanYiYin"), TEXT("灵泉一饮"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 20), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 5), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1) }, RareFrame, TEXT("Route.Rare"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Rare, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Rare.JueJingFanJi"), TEXT("绝境反击"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), Attack(220, EGameXXKCardEffectTarget::SelectedTarget, 1, OwnerHealthBelow(30.0f)), Attack(120, EGameXXKCardEffectTarget::SelectedTarget, 1, OwnerHealthBelow(30.0f, true)) }, RareFrame, TEXT("Route.Rare"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Rare, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Rare.TongXinHeBi"), TEXT("同心合璧"), 2, 8, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1) }, RareFrame, TEXT("Route.Rare"));

		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Boss, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Boss.XiongPiPiJia"), TEXT("熊罴皮甲"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 18), Modifier(EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound, EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::Attacker, 50, 1) }, BossFrame, TEXT("Route.Boss.BlackBear"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Boss, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Boss.HanDiYiShi"), TEXT("撼地遗势"), 3, 10, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(180, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10) }, BossFrame, TEXT("Route.Boss.BlackBear"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Boss, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Boss.HuPoZhenDan"), TEXT("虎魄镇胆"), 2, 8, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::AllAllies, 1) }, BossFrame, TEXT("Route.Boss.Tiger"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Boss, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Boss.DuKouLieFeng"), TEXT("渡口猎风"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(140, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 80, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, BossFrame, TEXT("Route.Boss.Tiger"));
		AddCard(Cards, EGameXXKCardOwner::Route, EGameXXKCardRarity::Boss, EGameXXKCharacterRole::Route, OwnerId, nullptr,
			TEXT("Route.Boss.FuHuDuanJiang"), TEXT("伏虎断江"), 3, 14, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(230, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus, EGameXXKCardEffectTarget::SelectedTarget, 25, EGameXXKCardStatus::None, 1, ConsumeTargetStatus(EGameXXKCardStatus::Vulnerability, 3)) }, BossFrame, TEXT("Route.Boss.Tiger"));
	}

	void TagProfessionCards(
		TArray<FGameXXKCardDefinition>& Cards,
		const FName ArchetypeId,
		const TArray<FName>& CardIds)
	{
		for (const FName CardId : CardIds)
		{
			FGameXXKCardDefinition* Definition = Cards.FindByPredicate([CardId](const FGameXXKCardDefinition& Candidate)
			{
				return Candidate.Id == CardId;
			});
			if (!Definition)
			{
				UE_LOG(LogTemp, Fatal, TEXT("Missing profession card while assigning birth archetype %s: %s"),
					*ArchetypeId.ToString(),
					*CardId.ToString());
				return;
			}
			Definition->ProfessionArchetypeIds.AddUnique(ArchetypeId);
		}
	}

	void AddProfessionArchetypeMetadata(TArray<FGameXXKCardDefinition>& Cards)
	{
		TagProfessionCards(Cards, TEXT("Archetype.Blade.BloodEdge"), {
			TEXT("Profession.Blade.FengHou"), TEXT("Profession.Blade.JiYuLianZhan"),
			TEXT("Profession.Blade.YinXueDao"), TEXT("Profession.Blade.LangDuan")});
		TagProfessionCards(Cards, TEXT("Archetype.Blade.MomentumBreak"), {
			TEXT("Profession.Blade.DuanYue"), TEXT("Profession.Blade.PoJun"),
			TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("Profession.Blade.ZhanJin")});
		TagProfessionCards(Cards, TEXT("Archetype.Blade.Counterflow"), {
			TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("Profession.Blade.ZhuYing"),
			TEXT("Profession.Blade.PoLangTuJin"), TEXT("Profession.Blade.YiShiDuanJiang")});
		TagProfessionCards(Cards, TEXT("Archetype.Blade.Sheathed"), {
			TEXT("Profession.Blade.JingHongChuQiao"), TEXT("Profession.Blade.HengYunKaiFeng"),
			TEXT("Profession.Blade.LianXiGuiQiao"), TEXT("Profession.Blade.BaoDaoShouYe")});

		TagProfessionCards(Cards, TEXT("Archetype.Guard.ArmorGrowth"), {
			TEXT("Profession.Guard.GuShou"), TEXT("Profession.Guard.FanZhenJia"),
			TEXT("Profession.Guard.TieBiRuShan"), TEXT("Profession.Guard.BuDongRuShan")});
		TagProfessionCards(Cards, TEXT("Archetype.Guard.Protection"), {
			TEXT("Profession.Guard.YuanHuBu"), TEXT("Profession.Guard.YuanJunBiLei"),
			TEXT("Profession.Guard.TieSuoHengJiang"), TEXT("Profession.Guard.YiFuDangGuan")});
		TagProfessionCards(Cards, TEXT("Archetype.Guard.ArmorConversion"), {
			TEXT("Profession.Guard.ZhenDun"), TEXT("Profession.Guard.QinWangDunJi"),
			TEXT("Profession.Guard.DunZhenTuiJin"), TEXT("Profession.Guard.SuiJiaHuiJi")});
		TagProfessionCards(Cards, TEXT("Archetype.Guard.ArmorRelease"), {
			TEXT("Profession.Guard.PanShiTuNa"), TEXT("Profession.Guard.PiJiaXingJun"),
			TEXT("Profession.Guard.ZhenYueLing"), TEXT("Profession.Guard.BiLeiFanGong")});

		TagProfessionCards(Cards, TEXT("Archetype.Healer.Medicine"), {
			TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("Profession.Healer.QingXinSan"),
			TEXT("Profession.Healer.LingZhiXuMing"), TEXT("Profession.Healer.HuiChunLu"),
			TEXT("Profession.Healer.ZhiXueCao"), TEXT("Profession.Healer.WenYangGao"),
			TEXT("Profession.Healer.JinChuangXuMing"), TEXT("Profession.Healer.YaoWangGuiYuan")});
		TagProfessionCards(Cards, TEXT("Archetype.Healer.ToxicExplosion"), {
			TEXT("Profession.Healer.BaiCaoDu"), TEXT("Profession.Healer.FuGuSan"),
			TEXT("Profession.Healer.HuiQiXiang"), TEXT("Profession.Healer.LianQiaoJieDu"),
			TEXT("Profession.Healer.YaoJiuWenShen"), TEXT("Profession.Healer.YaoNangFeiTou"),
			TEXT("Profession.Healer.KuShenMaSan"), TEXT("Profession.Healer.WuWeiTiaoHe")});

		TagProfessionCards(Cards, TEXT("Archetype.Hunter.BleedVolley"), {
			TEXT("Profession.Hunter.XunXiJian"), TEXT("Profession.Hunter.ZhuiLie"),
			TEXT("Profession.Hunter.FuZuShi"), TEXT("Profession.Hunter.DuanMaiShi")});
		TagProfessionCards(Cards, TEXT("Archetype.Hunter.HeavyArrow"), {
			TEXT("Profession.Hunter.LieWang"), TEXT("Profession.Hunter.ChuanYang"),
			TEXT("Profession.Hunter.ShouHun"), TEXT("Profession.Hunter.BaiBuChuanYang")});
		TagProfessionCards(Cards, TEXT("Archetype.Hunter.ToxicArrow"), {
			TEXT("Profession.Hunter.LieHunBiao"), TEXT("Profession.Hunter.PoJiaDing"),
			TEXT("Profession.Hunter.FuYeXianJing"), TEXT("Profession.Hunter.YingLuo")});
		TagProfessionCards(Cards, TEXT("Archetype.Hunter.AgilityCycle"), {
			TEXT("Profession.Hunter.FuBu"), TEXT("Profession.Hunter.YinZong"),
			TEXT("Profession.Hunter.LueYingJian"), TEXT("Profession.Hunter.HuiHuanJian")});

		TagProfessionCards(Cards, TEXT("Archetype.Sorcerer.FireSequence"), {
			TEXT("Profession.Sorcerer.LiHuoYin"), TEXT("Profession.Sorcerer.YanQiang"),
			TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("Profession.Sorcerer.XingHuoLiaoYuan")});
		TagProfessionCards(Cards, TEXT("Archetype.Sorcerer.IceSequence"), {
			TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("Profession.Sorcerer.FenMaiFu"),
			TEXT("Profession.Sorcerer.LingYanLianDan"), TEXT("Profession.Sorcerer.HuLingMu")});
		TagProfessionCards(Cards, TEXT("Archetype.Sorcerer.LightningSequence"), {
			TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("Profession.Sorcerer.FenTianJue"),
			TEXT("Profession.Sorcerer.NingYanChengRen"), TEXT("Profession.Sorcerer.RanLingHuanYuan")});
		TagProfessionCards(Cards, TEXT("Archetype.Sorcerer.GeneralTask"), {
			TEXT("Profession.Sorcerer.YanMuHuTi"), TEXT("Profession.Sorcerer.LieFu"),
			TEXT("Profession.Sorcerer.XingHuoHuiShou"), TEXT("Profession.Sorcerer.ChiYanFengJie")});

		TagProfessionCards(Cards, TEXT("Archetype.Formation.Assault"), {
			TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("Profession.FormationMaster.ZhenShaZhen"),
			TEXT("Profession.FormationMaster.ShanMenFengSuo")});
		TagProfessionCards(Cards, TEXT("Archetype.Formation.Support"), {
			TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("Profession.FormationMaster.ShuiJingZheGuang"),
			TEXT("Profession.FormationMaster.LinFengFuZhen")});
		TagProfessionCards(Cards, TEXT("Archetype.Formation.Cycle"), {
			TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("Profession.FormationMaster.BaMenLunZhuan"),
			TEXT("Profession.FormationMaster.ZhenQiGuWu")});
		TagProfessionCards(Cards, TEXT("Archetype.Formation.Convergence"), {
			TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("Profession.FormationMaster.DiMaiJieLi"),
			TEXT("Profession.FormationMaster.SiXiangLianHuan")});
	}
}

const TArray<FGameXXKCardDefinition>& FGameXXKCardCatalog::GetAllCardDefinitions()
{
	static const TArray<FGameXXKCardDefinition> Definitions = []
	{
		TArray<FGameXXKCardDefinition> Cards;
		Cards.Reserve(174);
		AddHeroCards(Cards);
		AddQuestNpcCards(Cards);
		AddBladeCards(Cards);
		AddGuardCards(Cards);
		AddHealerCards(Cards);
		AddHunterCards(Cards);
		AddSorcererCards(Cards);
		AddFormationCards(Cards);
		AddProfessionArchetypeMetadata(Cards);
		AddRouteCards(Cards);
		FString QualityValidationError;
		if (!FGameXXKCardQualityRules::ValidateCardCatalog(Cards, QualityValidationError))
		{
			UE_LOG(LogTemp, Fatal, TEXT("Invalid card quality catalog: %s"), *QualityValidationError);
		}
		return Cards;
	}();
	return Definitions;
}

const FGameXXKCardDefinition* FGameXXKCardCatalog::FindCardDefinition(const FName CardId)
{
	return GetAllCardDefinitions().FindByPredicate([CardId](const FGameXXKCardDefinition& Definition)
	{
		return Definition.Id == CardId;
	});
}

bool FGameXXKCardCatalog::FindCardDefinition(const FName CardId, FGameXXKCardDefinition& OutDefinition)
{
	if (const FGameXXKCardDefinition* Definition = FindCardDefinition(CardId))
	{
		OutDefinition = *Definition;
		return true;
	}

	OutDefinition = FGameXXKCardDefinition();
	return false;
}

TArray<FGameXXKCardDefinition> FGameXXKCardCatalog::GetCardDefinitionsForOwner(const FName OwnerId)
{
	TArray<FGameXXKCardDefinition> Result;
	for (const FGameXXKCardDefinition& Definition : GetAllCardDefinitions())
	{
		if (Definition.OwnerId == OwnerId)
		{
			Result.Add(Definition);
		}
	}
	return Result;
}

TArray<FName> FGameXXKCardCatalog::GetHeroCardIdsUnlockedAtLevel(const int32 HeroLevel)
{
	const int32 ClampedLevel = FMath::Clamp(HeroLevel, 1, 20);
	TArray<FName> Result;
	TSet<FName> Seen;
	for (const FGameXXKCardDefinition& Definition : GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Hero
			|| Definition.HeroUnlockLevel > ClampedLevel
			|| Seen.Contains(Definition.Id))
		{
			continue;
		}
		Seen.Add(Definition.Id);
		Result.Add(Definition.Id);
	}
	return Result;
}

const TArray<FGameXXKCardVisualDefinition>& FGameXXKCardCatalog::GetCardVisualDefinitions()
{
	static const TArray<FGameXXKCardVisualDefinition> Visuals = []
	{
		TArray<FGameXXKCardVisualDefinition> Result;
		const TArray<FGameXXKCardDefinition>& Definitions = GetAllCardDefinitions();
		Result.Reserve(Definitions.Num());
		for (const FGameXXKCardDefinition& Definition : Definitions)
		{
			FGameXXKCardVisualDefinition Visual;
			Visual.CardId = Definition.Id;
			Visual.ArtKey = Definition.VisualArtKey;
			Visual.FrameKey = Definition.FrameKey;
			Visual.IdentitySubjectKey = FName(*FString::Printf(TEXT("Identity.Subject.%s"), *Definition.OwnerId.ToString()));
			if (Definition.bIdentityLocked)
			{
				Visual.SourceArtKey = FName(*FString::Printf(TEXT("Source.Identity.%s"), *Definition.OwnerId.ToString()));
			}
			else
			{
				Visual.SourceArtKey = FName(*FString::Printf(TEXT("Source.Role.%s"), *Definition.OwnerId.ToString()));
			}
			Visual.OverlayKey = FName(*FString::Printf(TEXT("Overlay.%s"), *Definition.FrameKey.ToString()));
			Visual.bIdentityLocked = Definition.bIdentityLocked;
			Result.Add(Visual);
		}
		return Result;
	}();
	return Visuals;
}

const FGameXXKCardVisualDefinition* FGameXXKCardCatalog::FindCardVisualDefinition(const FName CardId)
{
	return GetCardVisualDefinitions().FindByPredicate([CardId](const FGameXXKCardVisualDefinition& Definition)
	{
		return Definition.CardId == CardId;
	});
}

bool FGameXXKCardCatalog::FindCardVisualDefinition(const FName CardId, FGameXXKCardVisualDefinition& OutDefinition)
{
	if (const FGameXXKCardVisualDefinition* Definition = FindCardVisualDefinition(CardId))
	{
		OutDefinition = *Definition;
		return true;
	}

	OutDefinition = FGameXXKCardVisualDefinition();
	return false;
}

bool FGameXXKCardCatalog::ValidateCardDefinition(const FGameXXKCardDefinition& Definition, FString& OutError)
{
	OutError.Reset();
	if (Definition.Id.IsNone() || Definition.DisplayName.IsEmpty() || Definition.Owner == EGameXXKCardOwner::Invalid || Definition.Rarity == EGameXXKCardRarity::Invalid || Definition.Role == EGameXXKCharacterRole::Invalid || Definition.OwnerId.IsNone() || Definition.AcquisitionKey.IsNone() || Definition.TargetSpec.Mode == EGameXXKCardTargetMode::Invalid || Definition.TargetSpec.Presentation == EGameXXKCardTargetPresentation::Invalid || Definition.Effects.IsEmpty() || Definition.VisualArtKey.IsNone() || Definition.FrameKey.IsNone())
	{
		OutError = FString::Printf(TEXT("Card definition is incomplete: %s."), *Definition.Id.ToString());
		return false;
	}

	const EGameXXKCardTargetPresentation ExpectedPresentation = TargetPresentationForMode(Definition.TargetSpec.Mode);
	if (Definition.TargetSpec.Presentation != ExpectedPresentation)
	{
		OutError = FString::Printf(TEXT("Target presentation does not match target mode: %s."), *Definition.Id.ToString());
		return false;
	}
	const EGameXXKCardUnitState ExpectedUnitState = Definition.TargetSpec.Mode == EGameXXKCardTargetMode::None
		? EGameXXKCardUnitState::Any
		: EGameXXKCardUnitState::Living;
	if (Definition.TargetSpec.RequiredUnitState != ExpectedUnitState)
	{
		OutError = FString::Printf(TEXT("Target unit state does not match target mode: %s."), *Definition.Id.ToString());
		return false;
	}
	if (Definition.TargetSpec.bRequireDifferentFromOwner != TargetModeRequiresDifferentFromOwner(Definition.TargetSpec.Mode))
	{
		OutError = FString::Printf(TEXT("Target owner-difference requirement does not match target mode: %s."), *Definition.Id.ToString());
		return false;
	}
	if (Definition.TargetSpec.RequiredStatus == EGameXXKCardStatus::Invalid || Definition.TargetSpec.RequiredStatusMinimumStacks < 0 || (IsConcreteStatus(Definition.TargetSpec.RequiredStatus) && Definition.TargetSpec.RequiredStatusMinimumStacks < 1) || (!IsConcreteStatus(Definition.TargetSpec.RequiredStatus) && Definition.TargetSpec.RequiredStatusMinimumStacks != 0))
	{
		OutError = FString::Printf(TEXT("Target status requirement is invalid: %s."), *Definition.Id.ToString());
		return false;
	}
	if (Definition.TargetSpec.ForbiddenStatus == EGameXXKCardStatus::Invalid || (IsConcreteStatus(Definition.TargetSpec.RequiredStatus) && Definition.TargetSpec.RequiredStatus == Definition.TargetSpec.ForbiddenStatus))
	{
		OutError = FString::Printf(TEXT("Target forbidden status requirement is invalid: %s."), *Definition.Id.ToString());
		return false;
	}
	if (!FMath::IsFinite(Definition.TargetSpec.MinimumHealthPercent)
		|| !FMath::IsFinite(Definition.TargetSpec.MaximumHealthPercent)
		|| Definition.TargetSpec.MinimumHealthPercent < 0.0f
		|| Definition.TargetSpec.MaximumHealthPercent > 100.0f
		|| Definition.TargetSpec.MinimumHealthPercent > Definition.TargetSpec.MaximumHealthPercent)
	{
		OutError = FString::Printf(TEXT("Target health-percent range is invalid: %s."), *Definition.Id.ToString());
		return false;
	}
	const bool bHasRequiredTerrain = Definition.TargetSpec.RequiredTerrain != EGameXXKCardTerrain::Invalid;
	const bool bHasAlternateRequiredTerrain = Definition.TargetSpec.AlternateRequiredTerrain != EGameXXKCardTerrain::Invalid;
	if ((!bHasRequiredTerrain && bHasAlternateRequiredTerrain)
		|| (bHasRequiredTerrain && bHasAlternateRequiredTerrain && Definition.TargetSpec.RequiredTerrain == Definition.TargetSpec.AlternateRequiredTerrain))
	{
		OutError = FString::Printf(TEXT("Target terrain requirement is invalid: %s."), *Definition.Id.ToString());
		return false;
	}

	for (const FGameXXKCardTargetModeOverride& Override : Definition.TargetSpec.ModeOverrides)
	{
		if (Override.ConditionType == EGameXXKCardTargetModeOverrideConditionType::Invalid || Override.Mode == EGameXXKCardTargetMode::Invalid || Override.Presentation == EGameXXKCardTargetPresentation::Invalid)
		{
			OutError = FString::Printf(TEXT("Target-mode override is incomplete: %s."), *Definition.Id.ToString());
			return false;
		}
		if (Override.Presentation != TargetPresentationForMode(Override.Mode))
		{
			OutError = FString::Printf(TEXT("Target-mode override presentation does not match its mode: %s."), *Definition.Id.ToString());
			return false;
		}
		switch (Override.ConditionType)
		{
		case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
			if (Override.Terrain == EGameXXKCardTerrain::Invalid
				|| Override.Terrain == Override.AlternateTerrain
				|| Override.Status != EGameXXKCardStatus::None
				|| Override.MinimumStatusStacks != 0)
			{
				OutError = FString::Printf(TEXT("Terrain target-mode override is invalid: %s."), *Definition.Id.ToString());
				return false;
			}
			break;
		case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus:
		case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus:
			if (!IsConcreteStatus(Override.Status)
				|| Override.MinimumStatusStacks < 1
				|| Override.Terrain != EGameXXKCardTerrain::Invalid
				|| Override.AlternateTerrain != EGameXXKCardTerrain::Invalid)
			{
				OutError = FString::Printf(TEXT("Status target-mode override is incomplete: %s."), *Definition.Id.ToString());
				return false;
			}
			break;
		default:
			OutError = FString::Printf(TEXT("Target-mode override condition is invalid: %s."), *Definition.Id.ToString());
			return false;
		}
	}

	if ((Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::QuestNpc) && !Definition.bIdentityLocked)
	{
		OutError = FString::Printf(TEXT("Identity-bound card is not locked: %s."), *Definition.Id.ToString());
		return false;
	}
	const FString OwnerIdText = Definition.OwnerId.ToString();
	const FString AcquisitionText = Definition.AcquisitionKey.ToString();
	switch (Definition.Owner)
	{
	case EGameXXKCardOwner::Hero:
		if (Definition.Role != EGameXXKCharacterRole::Hero
			|| Definition.Rarity != EGameXXKCardRarity::Permanent
			|| Definition.OwnerId != FName(TEXT("Hero"))
			|| (Definition.HeroUnlockLevel != 1
				&& Definition.HeroUnlockLevel != 5
				&& Definition.HeroUnlockLevel != 10
				&& Definition.HeroUnlockLevel != 15
				&& Definition.HeroUnlockLevel != 20)
			|| (Definition.LinkedRole != EGameXXKCharacterRole::Invalid
				&& Definition.LinkedRole != EGameXXKCharacterRole::Blade
				&& Definition.LinkedRole != EGameXXKCharacterRole::Guard
				&& Definition.LinkedRole != EGameXXKCharacterRole::Healer
				&& Definition.LinkedRole != EGameXXKCharacterRole::Hunter
				&& Definition.LinkedRole != EGameXXKCharacterRole::Sorcerer
				&& Definition.LinkedRole != EGameXXKCharacterRole::FormationMaster)
			|| AcquisitionText != (Definition.HeroUnlockLevel == 1
				? TEXT("Unlock.Initial")
				: FString::Printf(TEXT("Unlock.Level.%02d"), Definition.HeroUnlockLevel)))
		{
			OutError = FString::Printf(TEXT("Hero metadata is invalid: %s."), *Definition.Id.ToString());
			return false;
		}
		break;
	case EGameXXKCardOwner::Profession:
		if (Definition.Rarity != EGameXXKCardRarity::Permanent || !OwnerIdText.StartsWith(TEXT("Profession.")) || AcquisitionText != FString::Printf(TEXT("Pool.%s"), *OwnerIdText))
		{
			OutError = FString::Printf(TEXT("Profession metadata is invalid: %s."), *Definition.Id.ToString());
			return false;
		}
		break;
	case EGameXXKCardOwner::QuestNpc:
		if (Definition.Rarity != EGameXXKCardRarity::Permanent || Definition.NpcId != Definition.OwnerId || !OwnerIdText.StartsWith(TEXT("Npc.")) || Definition.AcquisitionKey != Definition.OwnerId)
		{
			OutError = FString::Printf(TEXT("Quest NPC metadata is invalid: %s."), *Definition.Id.ToString());
			return false;
		}
		break;
	case EGameXXKCardOwner::Route:
		if (Definition.Role != EGameXXKCharacterRole::Route || Definition.OwnerId != FName(TEXT("Route")) || !AcquisitionText.StartsWith(TEXT("Route.")))
		{
			OutError = FString::Printf(TEXT("Route metadata is invalid: %s."), *Definition.Id.ToString());
			return false;
		}
		break;
	default:
		OutError = FString::Printf(TEXT("Card owner is invalid: %s."), *Definition.Id.ToString());
		return false;
	}

	const bool bHeroCard = Definition.Owner == EGameXXKCardOwner::Hero;
	const bool bTaskNpcCard = Definition.Owner == EGameXXKCardOwner::QuestNpc;
	const bool bHunterProfessionCard = Definition.Owner == EGameXXKCardOwner::Profession
		&& Definition.Role == EGameXXKCharacterRole::Hunter
		&& Definition.OwnerId == FName(TEXT("Profession.Hunter"));
	const bool bHealerProfessionCard = Definition.Owner == EGameXXKCardOwner::Profession
		&& Definition.Role == EGameXXKCharacterRole::Healer
		&& Definition.OwnerId == FName(TEXT("Profession.Healer"));
	if (!bHeroCard
		&& (Definition.HeroUnlockLevel != 0
			|| Definition.bExhaustOnPlay
			|| Definition.SpellTaskReward != EGameXXKHeroSpellTaskReward::None))
	{
		OutError = FString::Printf(TEXT("Non-hero card carries protagonist-only metadata: %s."), *Definition.Id.ToString());
		return false;
	}
	if (!bHeroCard && !bTaskNpcCard && !bHunterProfessionCard
		&& (Definition.LinkedRole != EGameXXKCharacterRole::Invalid
			|| !Definition.ChargeEffects.IsEmpty()
			|| !Definition.FinishEffects.IsEmpty()
			|| Definition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None
			|| !Definition.TaskNpcRewardEffects.IsEmpty()))
	{
		OutError = FString::Printf(TEXT("Card carries hero/task-NPC sequence metadata outside those owners: %s."), *Definition.Id.ToString());
		return false;
	}
	if (!bTaskNpcCard && !Definition.TaskNpcRewardEffects.IsEmpty())
	{
		OutError = FString::Printf(TEXT("Task-NPC reward belongs to a non-task-NPC card: %s."), *Definition.Id.ToString());
		return false;
	}
	if (bTaskNpcCard
		&& (!Definition.ChargeEffects.IsEmpty() || !Definition.FinishEffects.IsEmpty())
		&& Definition.LinkedRole != EGameXXKCharacterRole::Blade)
	{
		OutError = FString::Printf(TEXT("Task-NPC Charge/Finish metadata requires its Blade pairing: %s."), *Definition.Id.ToString());
		return false;
	}

	const FGameXXKHeavyArrowRule& HeavyArrowRule = Definition.HeavyArrow;
	if (HeavyArrowRule.Kind == EGameXXKHeavyArrowKind::None)
	{
		if (HeavyArrowRule.MagnitudePerCharge != 0
			|| HeavyArrowRule.DrawPerCharge != 0
			|| HeavyArrowRule.MinimumChargeForEnergy != 0
			|| HeavyArrowRule.EnergyGain != 0
			|| HeavyArrowRule.ChargeSource != EGameXXKHeavyArrowChargeSource::CardOwner
			|| HeavyArrowRule.ManaPerCharge != 0
			|| HeavyArrowRule.AdditionalPrimaryAttackPercentPerCharge != 0
			|| HeavyArrowRule.IgnoreDefensePerCharge != 0
			|| HeavyArrowRule.TriggeredBleedResolutionsPerCharge != 0
			|| HeavyArrowRule.BonusStatus != EGameXXKCardStatus::None
			|| HeavyArrowRule.BonusStatusStacksPerCharge != 0
			|| HeavyArrowRule.BonusStatusTarget != EGameXXKCardEffectTarget::Invalid
			|| HeavyArrowRule.HealthThresholdPointsPerCharge != 0)
		{
			OutError = FString::Printf(TEXT("Disabled Heavy Arrow metadata is not empty: %s."), *Definition.Id.ToString());
			return false;
		}
	}
	else if (!((bHeroCard
				&& Definition.LinkedRole == EGameXXKCharacterRole::Hunter
				&& HeavyArrowRule.ChargeSource == EGameXXKHeavyArrowChargeSource::CardOwner
				&& HeavyArrowRule.ManaPerCharge == 0
				&& HeavyArrowRule.LockTiming == EGameXXKHeavyArrowLockTiming::BeforeBaseEffects)
			|| (bHunterProfessionCard
				&& HeavyArrowRule.ChargeSource == EGameXXKHeavyArrowChargeSource::CardOwner
				&& HeavyArrowRule.LockTiming == EGameXXKHeavyArrowLockTiming::BeforeBaseEffects)
			|| (bTaskNpcCard
				&& HeavyArrowRule.Kind == EGameXXKHeavyArrowKind::ExtraAttackPerCharge
				&& HeavyArrowRule.ChargeSource == EGameXXKHeavyArrowChargeSource::HighestAttackAlly
				&& HeavyArrowRule.LockTiming == EGameXXKHeavyArrowLockTiming::AfterBaseEffects))
		|| HeavyArrowRule.MagnitudePerCharge <= 0
		|| HeavyArrowRule.DrawPerCharge < 0
		|| HeavyArrowRule.MinimumChargeForEnergy < 0
		|| HeavyArrowRule.EnergyGain < 0
		|| HeavyArrowRule.ManaPerCharge < 0
		|| HeavyArrowRule.AdditionalPrimaryAttackPercentPerCharge < 0
		|| HeavyArrowRule.IgnoreDefensePerCharge < 0
		|| HeavyArrowRule.TriggeredBleedResolutionsPerCharge < 0
		|| HeavyArrowRule.BonusStatusStacksPerCharge < 0
		|| HeavyArrowRule.HealthThresholdPointsPerCharge < 0
		|| ((HeavyArrowRule.MinimumChargeForEnergy == 0) != (HeavyArrowRule.EnergyGain == 0))
		|| ((HeavyArrowRule.BonusStatus == EGameXXKCardStatus::None)
			!= (HeavyArrowRule.BonusStatusStacksPerCharge == 0
				&& HeavyArrowRule.BonusStatusTarget == EGameXXKCardEffectTarget::Invalid))
		|| (HeavyArrowRule.BonusStatus != EGameXXKCardStatus::None
			&& (!IsConcreteStatus(HeavyArrowRule.BonusStatus)
				|| HeavyArrowRule.BonusStatusStacksPerCharge <= 0
				|| (HeavyArrowRule.BonusStatusTarget != EGameXXKCardEffectTarget::CardOwner
					&& HeavyArrowRule.BonusStatusTarget != EGameXXKCardEffectTarget::SelectedTarget))))
	{
		OutError = FString::Printf(TEXT("Heavy Arrow metadata is incomplete: %s."), *Definition.Id.ToString());
		return false;
	}
	if (Definition.SpellTaskReward != EGameXXKHeroSpellTaskReward::None
		&& (Definition.Owner != EGameXXKCardOwner::Hero || Definition.LinkedRole != EGameXXKCharacterRole::Sorcerer))
	{
		OutError = FString::Printf(TEXT("Spell-task reward belongs to a non-Sorcerer protagonist card: %s."), *Definition.Id.ToString());
		return false;
	}
	const FGameXXKHunterCardRule& HunterRuleData = Definition.HunterRule;
	const bool bHasHunterRule = HunterRuleData.PrimaryAttackPercentPerPriorActiveCard != 0
		|| HunterRuleData.PriorActiveCardInterval != 0
		|| HunterRuleData.DrawPerCompletedInterval != 0
		|| HunterRuleData.StatusPerCompletedInterval != EGameXXKCardStatus::None
		|| HunterRuleData.StatusStacksPerCompletedInterval != 0
		|| HunterRuleData.NextHeavyArrowIgnoreDefense != 0
		|| HunterRuleData.ChargeOnNextPerfectDodge != 0;
	const bool bHasHunterIntervalPayload = HunterRuleData.DrawPerCompletedInterval > 0
		|| HunterRuleData.StatusPerCompletedInterval != EGameXXKCardStatus::None
		|| HunterRuleData.StatusStacksPerCompletedInterval > 0;
	if ((bHasHunterRule && !bHunterProfessionCard)
		|| HunterRuleData.PrimaryAttackPercentPerPriorActiveCard < 0
		|| HunterRuleData.PriorActiveCardInterval < 0
		|| HunterRuleData.DrawPerCompletedInterval < 0
		|| HunterRuleData.StatusStacksPerCompletedInterval < 0
		|| HunterRuleData.NextHeavyArrowIgnoreDefense < 0
		|| HunterRuleData.ChargeOnNextPerfectDodge < 0
		|| ((HunterRuleData.PriorActiveCardInterval > 0) != bHasHunterIntervalPayload)
		|| ((HunterRuleData.StatusPerCompletedInterval == EGameXXKCardStatus::None)
			!= (HunterRuleData.StatusStacksPerCompletedInterval == 0))
		|| (HunterRuleData.StatusPerCompletedInterval != EGameXXKCardStatus::None
			&& !IsConcreteStatus(HunterRuleData.StatusPerCompletedInterval)))
	{
		OutError = FString::Printf(TEXT("Permanent Hunter sequencing metadata is incomplete: %s."), *Definition.Id.ToString());
		return false;
	}
	const FGameXXKHealerCardRule& HealerRuleData = Definition.HealerRule;
	const bool bHasHealerRule = HealerRuleData.UnopenedFormulaEnergySurcharge != 0
		|| HealerRuleData.FormulaKind != EGameXXKHealerFormulaKind::None;
	if ((bHasHealerRule && !bHealerProfessionCard)
		|| HealerRuleData.UnopenedFormulaEnergySurcharge < 0
		|| ((HealerRuleData.FormulaKind == EGameXXKHealerFormulaKind::None)
			!= (HealerRuleData.UnopenedFormulaEnergySurcharge == 0)))
	{
		OutError = FString::Printf(TEXT("Permanent Healer formula metadata is incomplete: %s."), *Definition.Id.ToString());
		return false;
	}

	TSet<FName> ConsumptionGroups;
	TSet<FName> ResultGroups;
	for (const FGameXXKCardEffect& CardEffect : Definition.Effects)
	{
		if (CardEffect.Type == EGameXXKCardEffectType::Invalid
			|| CardEffect.Target == EGameXXKCardEffectTarget::Invalid
			|| CardEffect.Source == EGameXXKCardEffectSource::Invalid)
		{
			OutError = FString::Printf(TEXT("Card has an invalid effect: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.HitCount <= 0)
		{
			OutError = FString::Printf(TEXT("Card effect hit count must be positive: %s."), *Definition.Id.ToString());
			return false;
		}
		if ((CardEffect.Type == EGameXXKCardEffectType::ApplyStatus
				|| CardEffect.Type == EGameXXKCardEffectType::RemoveStatus
				|| CardEffect.Type == EGameXXKCardEffectType::RegisterReaction
				|| CardEffect.Type == EGameXXKCardEffectType::TriggerStatus
				|| CardEffect.Type == EGameXXKCardEffectType::LightningPerTargetStatusSnapshot)
			&& !IsConcreteStatus(CardEffect.Status))
		{
			OutError = FString::Printf(TEXT("Status effect has no status payload: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::GainArmorFromCurrentManaPercent
			&& (CardEffect.Target != EGameXXKCardEffectTarget::CardOwner
				|| CardEffect.Magnitude <= 0
				|| CardEffect.SecondaryMagnitude != 0))
		{
			OutError = FString::Printf(TEXT("Current-Mana armor conversion is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::GainManaOverflowToArmor
			&& (CardEffect.Target != EGameXXKCardEffectTarget::CardOwner
				|| CardEffect.Magnitude <= 0
				|| CardEffect.SecondaryMagnitude <= 0))
		{
			OutError = FString::Printf(TEXT("Mana-overflow armor conversion is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard
			&& (CardEffect.Target != EGameXXKCardEffectTarget::CardOwner
				|| CardEffect.Magnitude != 1
				|| CardEffect.SecondaryMagnitude != 0))
		{
			OutError = FString::Printf(TEXT("Hero spell-task search is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::TriggerStatus
			&& (CardEffect.Magnitude <= 0
				|| (CardEffect.Status != EGameXXKCardStatus::Bleed
					&& CardEffect.Status != EGameXXKCardStatus::Poison
					&& CardEffect.Status != EGameXXKCardStatus::Burn)))
		{
			OutError = FString::Printf(TEXT("Triggered DoT effect is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::LightningPerTargetStatusSnapshot
			&& (CardEffect.Status != EGameXXKCardStatus::Mark || CardEffect.Magnitude <= 0))
		{
			OutError = FString::Printf(TEXT("Lightning Mark snapshot effect is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ChangeTerrain
			&& (CardEffect.Target != EGameXXKCardEffectTarget::CardOwner
				|| CardEffect.Magnitude != 1
				|| CardEffect.SecondaryMagnitude != 0
				|| CardEffect.Status != EGameXXKCardStatus::None
				|| CardEffect.TerrainOverride == EGameXXKCardTerrain::Invalid
				|| CardEffect.Condition.Type != EGameXXKCardEffectConditionType::None))
		{
			OutError = FString::Printf(TEXT("Terrain switch effect is malformed: %s."), *Definition.Id.ToString());
			return false;
		}
		const bool bAppliesBareGuardStatus = (CardEffect.Type == EGameXXKCardEffectType::ApplyStatus && CardEffect.Status == EGameXXKCardStatus::Guard)
			|| (CardEffect.Type == EGameXXKCardEffectType::ApplyBattleModifier && CardEffect.Modifier.EffectType == EGameXXKCardEffectType::ApplyStatus && CardEffect.Modifier.Status == EGameXXKCardStatus::Guard);
		if (bAppliesBareGuardStatus)
		{
			OutError = FString::Printf(TEXT("Guard must use an explicit guard link: %s."), *Definition.Id.ToString());
			return false;
		}
		if (UsesSelectedTarget(CardEffect) && !CanResolveSelectedTarget(Definition.TargetSpec.Mode))
		{
			OutError = FString::Printf(TEXT("SelectedTarget use is incompatible with %s on %s."), *UEnum::GetValueAsString(Definition.TargetSpec.Mode), *Definition.Id.ToString());
			return false;
		}
		if (!ValidateEffectCondition(CardEffect.Condition, Definition.Id, TEXT("Effect"), OutError))
		{
			return false;
		}
		if (!CardEffect.ResultRef.IsNone() && !ResultGroups.Contains(CardEffect.ResultRef))
		{
			OutError = FString::Printf(TEXT("Effect result must reference an earlier producer: %s."), *Definition.Id.ToString());
			return false;
		}
		if (!CardEffect.ResultGroupId.IsNone())
		{
			if (ResultGroups.Contains(CardEffect.ResultGroupId))
			{
				OutError = FString::Printf(TEXT("Effect result producer is duplicated: %s."), *Definition.Id.ToString());
				return false;
			}
			ResultGroups.Add(CardEffect.ResultGroupId);
		}
		if (!CardEffect.ConsumedStackResultRef.IsNone() && !ConsumptionGroups.Contains(CardEffect.ConsumedStackResultRef))
		{
			OutError = FString::Printf(TEXT("Consumed-stack result must reference an earlier producer: %s."), *Definition.Id.ToString());
			return false;
		}
		if (!CardEffect.ConsumptionGroupId.IsNone())
		{
			if (!CardEffect.Condition.bConsumeStatus || ConsumptionGroups.Contains(CardEffect.ConsumptionGroupId))
			{
				OutError = FString::Printf(TEXT("Consumption result producer is invalid: %s."), *Definition.Id.ToString());
				return false;
			}
			ConsumptionGroups.Add(CardEffect.ConsumptionGroupId);
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyGuardLink && (CardEffect.GuardLink.Guardian == EGameXXKCardEffectTarget::Invalid || CardEffect.GuardLink.ProtectedUnit == EGameXXKCardEffectTarget::Invalid || CardEffect.GuardLink.Stacks < 1 || CardEffect.GuardLink.RedirectPolicy == EGameXXKCardGuardRedirectPolicy::Invalid))
		{
			OutError = FString::Printf(TEXT("Guard link is incomplete: %s."), *Definition.Id.ToString());
			return false;
		}
		if (CardEffect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			if (CardEffect.Modifier.Trigger == EGameXXKCardBattleModifierTrigger::Invalid || CardEffect.Modifier.EffectType == EGameXXKCardEffectType::Invalid || CardEffect.Modifier.Target == EGameXXKCardEffectTarget::Invalid || CardEffect.Modifier.RecipientScope == EGameXXKCardModifierRecipientScope::Invalid || CardEffect.Modifier.RecipientTarget == EGameXXKCardEffectTarget::Invalid || CardEffect.Modifier.Expiry == EGameXXKCardModifierExpiry::Invalid || CardEffect.Modifier.TriggeredAttackTargetScope == EGameXXKCardTriggeredAttackTargetScope::Invalid || !CardEffect.Modifier.bPersistent)
			{
				OutError = FString::Printf(TEXT("Persistent modifier is incomplete: %s."), *Definition.Id.ToString());
				return false;
			}
			if ((CardEffect.Modifier.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid && CardEffect.Modifier.RequiredTriggeredOwnerId.IsNone()) || (CardEffect.Modifier.RequiredTriggeredRole == EGameXXKCharacterRole::Invalid && !CardEffect.Modifier.RequiredTriggeredOwnerId.IsNone()))
			{
				OutError = FString::Printf(TEXT("Modifier role binding is incomplete: %s."), *Definition.Id.ToString());
				return false;
			}
			if ((CardEffect.Modifier.EffectType == EGameXXKCardEffectType::ApplyStatus
					|| CardEffect.Modifier.EffectType == EGameXXKCardEffectType::RemoveStatus
					|| CardEffect.Modifier.EffectType == EGameXXKCardEffectType::RegisterReaction
					|| CardEffect.Modifier.EffectType == EGameXXKCardEffectType::TriggerStatus)
				&& !IsConcreteStatus(CardEffect.Modifier.Status))
			{
				OutError = FString::Printf(TEXT("Status modifier has no status payload: %s."), *Definition.Id.ToString());
				return false;
			}
			if (!ValidateEffectCondition(CardEffect.Modifier.Condition, Definition.Id, TEXT("Modifier"), OutError))
			{
				return false;
			}
		}
	}
	const auto ValidateSupplementalEffects = [&Definition, &OutError](
		const TArray<FGameXXKCardEffect>& Effects,
		const TCHAR* Label)
	{
		if (Effects.IsEmpty())
		{
			return true;
		}
		FGameXXKCardDefinition Supplemental = Definition;
		Supplemental.Effects = Effects;
		Supplemental.ChargeEffects.Reset();
		Supplemental.FinishEffects.Reset();
		Supplemental.HeavyArrow = FGameXXKHeavyArrowRule();
		Supplemental.HunterRule = FGameXXKHunterCardRule();
		Supplemental.HealerRule = FGameXXKHealerCardRule();
		Supplemental.SpellTaskReward = EGameXXKHeroSpellTaskReward::None;
		Supplemental.TaskNpcRewardEffects.Reset();
		FString SupplementalError;
		if (!FGameXXKCardCatalog::ValidateCardDefinition(Supplemental, SupplementalError))
		{
			OutError = FString::Printf(TEXT("%s effects are invalid for %s: %s"), Label, *Definition.Id.ToString(), *SupplementalError);
			return false;
		}
		return true;
	};
	if (!ValidateSupplementalEffects(Definition.ChargeEffects, TEXT("Charge"))
		|| !ValidateSupplementalEffects(Definition.FinishEffects, TEXT("Finish"))
		|| !ValidateSupplementalEffects(Definition.TaskNpcRewardEffects, TEXT("Task NPC reward")))
	{
		return false;
	}
	for (const FGameXXKCardTargetModeOverride& Override : Definition.TargetSpec.ModeOverrides)
	{
		if (CanResolveSelectedTarget(Override.Mode))
		{
			continue;
		}
		for (const FGameXXKCardEffect& CardEffect : Definition.Effects)
		{
			if (UsesSelectedTargetWhenOverrideApplies(CardEffect, Override))
			{
				OutError = FString::Printf(TEXT("SelectedTarget effect is incompatible with a target-mode override on %s."), *Definition.Id.ToString());
				return false;
			}
		}
	}
	return true;
}

bool FGameXXKCardCatalog::ValidateCardDefinitions(FString& OutError)
{
	OutError.Reset();
	const TArray<FGameXXKCardDefinition>& Definitions = GetAllCardDefinitions();
	if (Definitions.Num() != 198)
	{
		OutError = FString::Printf(TEXT("Expected 198 card definitions but found %d."), Definitions.Num());
		return false;
	}

	TSet<FName> CardIds;
	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone() || CardIds.Contains(Definition.Id))
		{
			OutError = FString::Printf(TEXT("Card id is missing or duplicated: %s."), *Definition.Id.ToString());
			return false;
		}
		CardIds.Add(Definition.Id);
		if (!ValidateCardDefinition(Definition, OutError))
		{
			return false;
		}
	}

	const TArray<FGameXXKCardVisualDefinition>& Visuals = GetCardVisualDefinitions();
	if (Visuals.Num() != Definitions.Num())
	{
		OutError = FString::Printf(TEXT("Expected %d visual recipes but found %d."), Definitions.Num(), Visuals.Num());
		return false;
	}

	for (const FGameXXKCardDefinition& Definition : Definitions)
	{
		const FGameXXKCardVisualDefinition* Visual = FindCardVisualDefinition(Definition.Id);
		if (!Visual || Visual->ArtKey != Definition.VisualArtKey || Visual->FrameKey != Definition.FrameKey || Visual->SourceArtKey.IsNone() || Visual->OverlayKey.IsNone() || Visual->bIdentityLocked != Definition.bIdentityLocked)
		{
			OutError = FString::Printf(TEXT("Visual recipe is incomplete or mismatched: %s."), *Definition.Id.ToString());
			return false;
		}
		if ((Definition.Owner == EGameXXKCardOwner::Hero || Definition.Owner == EGameXXKCardOwner::QuestNpc) && (Visual->IdentitySubjectKey.IsNone() || Visual->SourceArtKey == Definition.VisualArtKey))
		{
			OutError = FString::Printf(TEXT("Identity-bound visual source is not locked: %s."), *Definition.Id.ToString());
			return false;
		}
	}

	return true;
}
