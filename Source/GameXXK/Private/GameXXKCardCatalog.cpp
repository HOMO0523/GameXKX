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
		if (CardEffect.Target == EGameXXKCardEffectTarget::SelectedTarget)
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
		if (CardEffect.Target == EGameXXKCardEffectTarget::SelectedTarget)
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
		const int32 EnergyGain = 0)
	{
		FGameXXKHeavyArrowRule Rule;
		Rule.Kind = Kind;
		Rule.MagnitudePerCharge = MagnitudePerCharge;
		Rule.DrawPerCharge = DrawPerCharge;
		Rule.MinimumChargeForEnergy = MinimumChargeForEnergy;
		Rule.EnergyGain = EnergyGain;
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
		const EGameXXKHeroSpellTaskReward SpellTaskReward = EGameXXKHeroSpellTaskReward::None)
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
		Definition.SpellTaskReward = SpellTaskReward;
		Cards.Add(MoveTemp(Definition));
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
		constexpr const TCHAR* Frame = TEXT("Style.QuestNpc");
		constexpr const TCHAR* Tusi = TEXT("Npc.TusiChief");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Tusi, Tusi,
			TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("寨主号令"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Village)) }, Frame, TEXT("Npc.TusiChief"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Tusi, Tusi,
			TEXT("Npc.TusiChief.ShiMenShouShi"), TEXT("石门守势"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 10) }, Frame, TEXT("Npc.TusiChief"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Tusi, Tusi,
			TEXT("Npc.TusiChief.TuSiJunLing"), TEXT("土司军令"), 2, 6, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 3) }, Frame, TEXT("Npc.TusiChief"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Tusi, Tusi,
			TEXT("Npc.TusiChief.MengZhaiShiYue"), TEXT("盟寨誓约"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 4) }, Frame, TEXT("Npc.TusiChief"), false, true);

		constexpr const TCHAR* Song = TEXT("Npc.SongJinBao");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Song, Song,
			TEXT("Npc.SongJinBao.ShangQianGuWu"), TEXT("赏钱鼓舞"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 4) }, Frame, TEXT("Npc.SongJinBao"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Song, Song,
			TEXT("Npc.SongJinBao.ErMuMiBao"), TEXT("耳目密报"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::RevealEnemyIntent, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, TEXT("Npc.SongJinBao"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Song, Song,
			TEXT("Npc.SongJinBao.GuiKeLing"), TEXT("贵客令"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5) }, Frame, TEXT("Npc.SongJinBao"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Song, Song,
			TEXT("Npc.SongJinBao.YiNuoQianJin"), TEXT("一诺千金"), 2, 8, EGameXXKCardTargetMode::None,
			{ Modifier(EGameXXKCardBattleModifierTrigger::OnCardPlayed, EGameXXKCardEffectType::ModifyEnergyCost, EGameXXKCardEffectTarget::PlayedCard, -1, 2, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::SharedDeck, EGameXXKCardEffectTarget::PlayedCard, EGameXXKCardModifierExpiry::AfterTriggerCount) }, Frame, TEXT("Npc.SongJinBao"), false, true);

		constexpr const TCHAR* YueBai = TEXT("Npc.YueBai");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, YueBai, YueBai,
			TEXT("Npc.YueBai.QingYanDianDeng"), TEXT("青焰点灯"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark) }, Frame, TEXT("Npc.YueBai"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, YueBai, YueBai,
			TEXT("Npc.YueBai.CanJuanPiZhu"), TEXT("残卷批注"), 0, 0, EGameXXKCardTargetMode::None,
			{ Effect(EGameXXKCardEffectType::Insight, EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::DiscoverCards, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, TEXT("Npc.YueBai"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, YueBai, YueBai,
			TEXT("Npc.YueBai.YueBaiZhaoYe"), TEXT("月白照夜"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(140, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, TEXT("Npc.YueBai"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, YueBai, YueBai,
			TEXT("Npc.YueBai.ShanHeCanTu"), TEXT("山河残图"), 3, 10, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 4), Effect(EGameXXKCardEffectType::RevealEnemyIntent, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, TEXT("Npc.YueBai"), false, true);

		constexpr const TCHAR* Zhou = TEXT("Npc.ZhouGuangZu");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Zhou, Zhou,
			TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), TEXT("异草辨识"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1), Effect(EGameXXKCardEffectType::Insight, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, TEXT("Npc.ZhouGuangZu"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Zhou, Zhou,
			TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), TEXT("黄山敷治"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 16), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest)) }, Frame, TEXT("Npc.ZhouGuangZu"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Zhou, Zhou,
			TEXT("Npc.ZhouGuangZu.DiZhiMoTu"), TEXT("地志摹图"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::TerrainBonusDoubleThisRound) }, Frame, TEXT("Npc.ZhouGuangZu"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Zhou, Zhou,
			TEXT("Npc.ZhouGuangZu.YanFenFengMai"), TEXT("岩粉封脉"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Poison, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Cave)) }, Frame, TEXT("Npc.ZhouGuangZu"), false, true);

		constexpr const TCHAR* JinGui = TEXT("Npc.JinGui");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, JinGui, JinGui,
			TEXT("Npc.JinGui.ShiJingErMu"), TEXT("市井耳目"), 0, 0, EGameXXKCardTargetMode::AllEnemies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Mark), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, TEXT("Npc.JinGui"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, JinGui, JinGui,
			TEXT("Npc.JinGui.QiaoYanZhouXuan"), TEXT("巧言周旋"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 3) }, Frame, TEXT("Npc.JinGui"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, JinGui, JinGui,
			TEXT("Npc.JinGui.ZaYiChouBei"), TEXT("杂役筹备"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 3) }, Frame, TEXT("Npc.JinGui"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, JinGui, JinGui,
			TEXT("Npc.JinGui.HouXiangTuoShen"), TEXT("后巷脱身"), 2, 6, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 8) }, Frame, TEXT("Npc.JinGui"), false, true);

		constexpr const TCHAR* Qiong = TEXT("Npc.QiongMeiEr");
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Qiong, Qiong,
			TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), TEXT("藤桥飞渡"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest, true)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest, true)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest)) }, Frame, TEXT("Npc.QiongMeiEr"), false, true, { TerrainTargetOverride(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest) });
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Qiong, Qiong,
			TEXT("Npc.QiongMeiEr.GuWuMiZong"), TEXT("蛊雾迷踪"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark) }, Frame, TEXT("Npc.QiongMeiEr"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Qiong, Qiong,
			TEXT("Npc.QiongMeiEr.YinLingZhenXin"), TEXT("银铃镇心"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8) }, Frame, TEXT("Npc.QiongMeiEr"), false, true);
		AddCard(Cards, EGameXXKCardOwner::QuestNpc, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::QuestNpc, Qiong, Qiong,
			TEXT("Npc.QiongMeiEr.ShanGeHuanLing"), TEXT("山歌唤灵"), 3, 10, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Forest)) }, Frame, TEXT("Npc.QiongMeiEr"), false, true);
	}

	void AddBladeCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Blade");
		constexpr const TCHAR* Frame = TEXT("Style.Blade");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Blade");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.LieFengZhan"), TEXT("裂风斩"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(110, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 30, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.FengHou"), TEXT("封喉"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(90, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Bleed) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.JiYuLianZhan"), TEXT("疾雨连斩"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(75, EGameXXKCardEffectTarget::SelectedTarget, 2) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("借势回锋"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility), Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 40, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::CardOwner, EGameXXKCardEffectTarget::CardOwner, EGameXXKCardModifierExpiry::AfterTriggerCount, EGameXXKCharacterRole::Blade, TEXT("Profession.Blade")) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.YiShangHuanShi"), TEXT("以伤换势"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::LoseHealth, EGameXXKCardEffectTarget::CardOwner, 6), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 4) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.DuanYue"), TEXT("断岳"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(140, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.ZhuYing"), TEXT("逐影"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility), Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 50, 1, 0, ConsumeOwnerStatus(EGameXXKCardStatus::Agility, 1), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::CardOwner, EGameXXKCardEffectTarget::CardOwner, EGameXXKCardModifierExpiry::AfterTriggerCount, EGameXXKCharacterRole::Blade, TEXT("Profession.Blade")) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.YinXueDao"), TEXT("饮血刀"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(120, EGameXXKCardEffectTarget::SelectedTarget), EffectWithSecondary(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::CardOwner, 25, 18) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.PoJun"), TEXT("破军"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 50, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Vulnerability, 2)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.CanYueSanDie"), TEXT("残月三叠"), 3, 8, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(70, EGameXXKCardEffectTarget::SelectedTarget, 3), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Bleed, 3) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("战意沸腾"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Momentum), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.ZhanJin"), TEXT("斩尽"), 3, 14, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(220, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 80, EGameXXKCardStatus::None, 1, TargetHealthBelow(30.0f)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.LangDuan"), TEXT("浪断"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(90, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 90, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Bleed)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.HuiFengJiaShi"), TEXT("回锋架势"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 8), Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 40, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::CardOwner, EGameXXKCardEffectTarget::CardOwner, EGameXXKCardModifierExpiry::AfterTriggerCount, EGameXXKCharacterRole::Blade, TEXT("Profession.Blade")) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.XiaoJiaLianJi"), TEXT("削甲连击"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(70, EGameXXKCardEffectTarget::SelectedTarget, 2), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.PoLangTuJin"), TEXT("破浪突进"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility), Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 50, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.DaoYiShouShu"), TEXT("刀意收束"), 0, 0, EGameXXKCardTargetMode::Self,
			{ WithConsumptionProducer(Effect(EGameXXKCardEffectType::GainManaPerConsumedStatus, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::None, 1, ConsumeOwnerStatus(EGameXXKCardStatus::Momentum, 3)), TEXT("Consumption.DaoYiShouShu.Momentum")), WithConsumedStackResult(Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), TEXT("Consumption.DaoYiShouShu.Momentum")) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Blade, OwnerId, nullptr,
			TEXT("Profession.Blade.YiShiDuanJiang"), TEXT("一式断江"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(190, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility, 1, OwnerArmorAtLeast(1)) }, Frame, Pool);
	}

	void AddGuardCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Guard");
		constexpr const TCHAR* Frame = TEXT("Style.Guard");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Guard");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieBi"), TEXT("铁壁"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 12) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.HuZhu"), TEXT("护主"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 4) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.ZhenDun"), TEXT("震盾"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.GuShou"), TEXT("固守"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 2) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.FanZhenJia"), TEXT("反震甲"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), Modifier(EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound, EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::Attacker, 60, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.ZhenYueLing"), TEXT("镇岳令"), 2, 6, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Village)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YuanHuBu"), TEXT("援护步"), 1, 0, EGameXXKCardTargetMode::LowestHealthAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::LowestHealthAlly) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.PiJiaXingJun"), TEXT("披甲行军"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.QinWangDunJi"), TEXT("擒王盾击"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(130, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieBiRuShan"), TEXT("铁壁如山"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 20), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::CannotReceiveVulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.BiLeiFanGong"), TEXT("壁垒反攻"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(80, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor, EGameXXKCardEffectTarget::SelectedTarget, 10, EGameXXKCardStatus::None, 1, ConsumeOwnerArmor(12)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.BuDongRuShan"), TEXT("不动如山"), 3, 12, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 24), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllOtherAllies, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.PanShiTuNa"), TEXT("磐石吐纳"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 5, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(8)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::None, 1, OwnerArmorAtLeast(8, true)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YuanJunBiLei"), TEXT("援军壁垒"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 10), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 4) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.DunZhenTuiJin"), TEXT("盾阵推进"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(105, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.TieSuoHengJiang"), TEXT("铁锁横江"), 2, 6, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 14), Effect(EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks, EGameXXKCardEffectTarget::CardOwner, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.SuiJiaHuiJi"), TEXT("碎甲回击"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(80, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor, EGameXXKCardEffectTarget::SelectedTarget, 10, EGameXXKCardStatus::None, 1, ConsumeOwnerArmor(8)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Guard, OwnerId, nullptr,
			TEXT("Profession.Guard.YiFuDangGuan"), TEXT("一夫当关"), 3, 12, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 24), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllOtherAllies, 6) }, Frame, Pool);
	}

	void AddHealerCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Healer");
		constexpr const TCHAR* Frame = TEXT("Style.Healer");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Healer");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("草木敷治"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 16) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.QingXinSan"), TEXT("清心散"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 6) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.YaoYin"), TEXT("药引"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 5), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::NextHealingBonus) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.BaiCaoDu"), TEXT("百草毒"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Poison) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.LingZhiXuMing"), TEXT("灵芝续命"), 2, 6, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 28), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 10, EGameXXKCardStatus::None, 1, TargetHealthBelow(30.0f)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.HuiChunLu"), TEXT("回春露"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 8) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.ZhiXueCao"), TEXT("止血草"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Bleed), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.XingQiZhen"), TEXT("行气针"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 8), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::None, 1, TargetHasAnyDamageOverTime()) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.WenYangGao"), TEXT("温养膏"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 10), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.FuGuSan"), TEXT("腐骨散"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 5, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.JinChuangXuMing"), TEXT("金疮续命"), 2, 8, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 24), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.YaoWangGuiYuan"), TEXT("药王归元"), 3, 14, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 18), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::AllAllies, 1), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 5) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.HuiQiXiang"), TEXT("回气香"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::None, 1, TargetHasAnyDamageOverTime()) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.LianQiaoJieDu"), TEXT("连翘解毒"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 2), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 6) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.YaoJiuWenShen"), TEXT("药酒温身"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::SelectedTarget, 10), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 3) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.YaoNangFeiTou"), TEXT("药囊飞投"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 7), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::LowestHealthAlly, 7) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.KuShenMaSan"), TEXT("苦参麻散"), 2, 4, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 4, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Healer, OwnerId, nullptr,
			TEXT("Profession.Healer.WuWeiTiaoHe"), TEXT("五味调和"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 12), Modifier(EGameXXKCardBattleModifierTrigger::OnNextHealing, EGameXXKCardEffectType::ModifyHealingPercent, EGameXXKCardEffectTarget::PlayedCard, 50, 0, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::AllAllies, EGameXXKCardEffectTarget::AllAllies, EGameXXKCardModifierExpiry::EndOfCurrentRound) }, Frame, Pool);
	}

	void AddHunterCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Hunter");
		constexpr const TCHAR* Frame = TEXT("Style.Hunter");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Hunter");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.XunXiJian"), TEXT("寻隙箭"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuBu"), TEXT("伏步"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ZhuiLie"), TEXT("追猎"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(130, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YingYan"), TEXT("鹰眼"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::Insight, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextAttackBonus) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LieWang"), TEXT("猎网"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ChuanYang"), TEXT("穿杨"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::IgnoreDefense, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark, 3)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LianZhuJian"), TEXT("连珠箭"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(65, EGameXXKCardEffectTarget::SelectedTarget, 2), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark, 2) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuZuShi"), TEXT("缚足矢"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(80, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YinZong"), TEXT("隐踪"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::Agility), Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 30, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.DuanMaiShi"), TEXT("断脉矢"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(110, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Bleed) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.ShouHun"), TEXT("狩魂"), 2, 8, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(160, EGameXXKCardEffectTarget::SelectedTarget), EffectWithSecondary(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 10, 5, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.BaiBuChuanYang"), TEXT("百步穿杨"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(210, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark, 5)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LueYingJian"), TEXT("掠影箭"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(75, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.LieHunBiao"), TEXT("猎魂标"), 0, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark), Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 40, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::CardOwner, EGameXXKCardEffectTarget::CardOwner, EGameXXKCardModifierExpiry::AfterTriggerCount, EGameXXKCharacterRole::Hunter, TEXT("Profession.Hunter")) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.PoJiaDing"), TEXT("破甲钉"), 2, 4, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(110, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.HuiHuanJian"), TEXT("回环箭"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 3, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.FuYeXianJing"), TEXT("腐叶陷阱"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Poison), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Hunter, OwnerId, nullptr,
			TEXT("Profession.Hunter.YingLuo"), TEXT("鹰落"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(190, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 90, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Mark, 3)) }, Frame, Pool);
	}

	void AddSorcererCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.Sorcerer");
		constexpr const TCHAR* Frame = TEXT("Style.Sorcerer");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.Sorcerer");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.LingHuoFu"), TEXT("灵火符"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Burn) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.JuLing"), TEXT("聚灵"), 0, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 7), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TargetHasStatus(EGameXXKCardStatus::Burn)) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.LiHuoYin"), TEXT("离火印"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(130, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Burn) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.YanQiang"), TEXT("炎墙"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 8), Modifier(EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::Attacker, 2, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::Burn) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("爆炎术"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TargetHasStatus(EGameXXKCardStatus::Burn, 3)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), TEXT("星火燎原"), 3, 10, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(70, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Burn) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("摄灵火"), 0, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::GainManaPerConsumedStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, 1, ConsumeTargetStatus(EGameXXKCardStatus::Burn, 4)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.FenMaiFu"), TEXT("焚脉符"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(90, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.LingYanLianDan"), TEXT("灵焰连弹"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(70, EGameXXKCardEffectTarget::SelectedTarget, 2), Effect(EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::SelectedTarget, 20, EGameXXKCardStatus::None, 2, TargetHasStatus(EGameXXKCardStatus::Burn)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.HuLingMu"), TEXT("护灵幕"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Agility) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("赤霄焚星"), 3, 12, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(190, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 5, EGameXXKCardStatus::Burn) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.FenTianJue"), TEXT("焚天诀"), 3, 14, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(100, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Burn), Effect(EGameXXKCardEffectType::LoseHealth, EGameXXKCardEffectTarget::CardOwner, 8) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.NingYanChengRen"), TEXT("凝焰成刃"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(110, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Burn) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.RanLingHuanYuan"), TEXT("燃灵换元"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::LoseHealth, EGameXXKCardEffectTarget::CardOwner, 6), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 8) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.YanMuHuTi"), TEXT("焰幕护体"), 1, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), Modifier(EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound, EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::Attacker, 2, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::Burn) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.LieFu"), TEXT("裂符"), 2, 5, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(150, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TargetHasStatus(EGameXXKCardStatus::Burn)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.XingHuoHuiShou"), TEXT("星火回收"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ WithConsumptionProducer(Effect(EGameXXKCardEffectType::GainManaPerConsumedStatus, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, 1, ConsumeTargetStatus(EGameXXKCardStatus::Burn, 4)), TEXT("Consumption.XingHuoHuiShou.Burn")), Attack(100, EGameXXKCardEffectTarget::SelectedTarget), WithConsumedStackResult(Effect(EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus, EGameXXKCardEffectTarget::SelectedTarget, 20), TEXT("Consumption.XingHuoHuiShou.Burn")) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::Sorcerer, OwnerId, nullptr,
			TEXT("Profession.Sorcerer.ChiYanFengJie"), TEXT("赤焰封界"), 3, 12, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(70, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Burn) }, Frame, Pool);
	}

	void AddFormationCards(TArray<FGameXXKCardDefinition>& Cards)
	{
		constexpr const TCHAR* OwnerId = TEXT("Profession.FormationMaster");
		constexpr const TCHAR* Frame = TEXT("Style.FormationMaster");
		constexpr const TCHAR* Pool = TEXT("Pool.Profession.FormationMaster");
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.GuanShi"), TEXT("观势"), 0, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::Insight, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextTerrainCardEnergyReduction) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.DingZhen"), TEXT("定阵"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.YinShuiHuiYuan"), TEXT("引水回元"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry, true)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)) }, Frame, Pool, true, false, { TerrainTargetOverride(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry) });
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.KunZhen"), TEXT("困阵"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark) }, Frame, Pool, true);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinYingMiZong"), TEXT("林影迷踪"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest, EGameXXKCardTerrain::Invalid, true)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest)) }, Frame, Pool, false, false, { TerrainTargetOverride(EGameXXKCardTerrain::Forest) });
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.JieShanWeiZhang"), TEXT("借山为障"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("村寨援阵"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 6), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("回声震杀"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(120, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cave)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("易位阵"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("八门轮转"), 2, 0, EGameXXKCardTargetMode::Self,
			{ Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2), Effect(EGameXXKCardEffectType::DiscardCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("镇煞阵"), 3, 10, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(80, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Poison, 1, TerrainIs(EGameXXKCardTerrain::Cave)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("万象归阵"), 3, 14, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::NextTerrainCardFree) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShanMenFengSuo"), TEXT("山门封锁"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff, EGameXXKCardTerrain::Invalid, true)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShuiJingZheGuang"), TEXT("水镜折光"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinFengFuZhen"), TEXT("林风拂阵"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest, EGameXXKCardTerrain::Invalid, true)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest)) }, Frame, Pool, false, false, { TerrainTargetOverride(EGameXXKCardTerrain::Forest) });
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("阵旗鼓舞"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 20, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::AllAllies, EGameXXKCardEffectTarget::AllAllies), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.DiMaiJieLi"), TEXT("地脉借力"), 2, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(100, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark, 1, TerrainIs(EGameXXKCardTerrain::Forest)), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Poison, 1, TerrainIs(EGameXXKCardTerrain::Cave)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Plain)) }, Frame, Pool);
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.SiXiangLianHuan"), TEXT("四象连环"), 3, 12, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1), Effect(EGameXXKCardEffectType::DoubleTerrainBonus, EGameXXKCardEffectTarget::CardOwner, 1) }, Frame, Pool);
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

	if (Definition.Owner != EGameXXKCardOwner::Hero
		&& (Definition.HeroUnlockLevel != 0
			|| Definition.LinkedRole != EGameXXKCharacterRole::Invalid
			|| Definition.bExhaustOnPlay
			|| !Definition.ChargeEffects.IsEmpty()
			|| !Definition.FinishEffects.IsEmpty()
			|| Definition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None
			|| Definition.SpellTaskReward != EGameXXKHeroSpellTaskReward::None))
	{
		OutError = FString::Printf(TEXT("Non-hero card carries protagonist-only metadata: %s."), *Definition.Id.ToString());
		return false;
	}

	const FGameXXKHeavyArrowRule& HeavyArrowRule = Definition.HeavyArrow;
	if (HeavyArrowRule.Kind == EGameXXKHeavyArrowKind::None)
	{
		if (HeavyArrowRule.MagnitudePerCharge != 0
			|| HeavyArrowRule.DrawPerCharge != 0
			|| HeavyArrowRule.MinimumChargeForEnergy != 0
			|| HeavyArrowRule.EnergyGain != 0)
		{
			OutError = FString::Printf(TEXT("Disabled Heavy Arrow metadata is not empty: %s."), *Definition.Id.ToString());
			return false;
		}
	}
	else if (Definition.Owner != EGameXXKCardOwner::Hero
		|| Definition.LinkedRole != EGameXXKCharacterRole::Hunter
		|| HeavyArrowRule.MagnitudePerCharge <= 0
		|| HeavyArrowRule.DrawPerCharge < 0
		|| HeavyArrowRule.MinimumChargeForEnergy < 0
		|| HeavyArrowRule.EnergyGain < 0
		|| ((HeavyArrowRule.MinimumChargeForEnergy == 0) != (HeavyArrowRule.EnergyGain == 0))
		|| ((HeavyArrowRule.Kind == EGameXXKHeavyArrowKind::ExtraAttackPerCharge
				|| HeavyArrowRule.Kind == EGameXXKHeavyArrowKind::ToxicExplosionPerCharge)
			&& (HeavyArrowRule.DrawPerCharge != 0 || HeavyArrowRule.MinimumChargeForEnergy != 0 || HeavyArrowRule.EnergyGain != 0)))
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
		Supplemental.SpellTaskReward = EGameXXKHeroSpellTaskReward::None;
		FString SupplementalError;
		if (!FGameXXKCardCatalog::ValidateCardDefinition(Supplemental, SupplementalError))
		{
			OutError = FString::Printf(TEXT("%s effects are invalid for %s: %s"), Label, *Definition.Id.ToString(), *SupplementalError);
			return false;
		}
		return true;
	};
	if (!ValidateSupplementalEffects(Definition.ChargeEffects, TEXT("Charge"))
		|| !ValidateSupplementalEffects(Definition.FinishEffects, TEXT("Finish")))
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
