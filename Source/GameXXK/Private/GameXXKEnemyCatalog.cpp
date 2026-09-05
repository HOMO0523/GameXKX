#include "GameXXKEnemyCatalog.h"

namespace
{
	FText EnemyText(const TCHAR* Value)
	{
		return FText::FromString(Value);
	}

	FGameXXKEnemyDifficultyInt Difficulty(
		const int32 Normal,
		const int32 Hard,
		const int32 Hell)
	{
		FGameXXKEnemyDifficultyInt Value;
		Value.Normal = Normal;
		Value.Hard = Hard;
		Value.Hell = Hell;
		return Value;
	}

	FGameXXKEnemyIntentEffectDefinition BaseEffect(
		const EGameXXKEnemyIntentEffectType Type,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect;
		Effect.Type = Type;
		Effect.Target = Target;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition Direct(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom,
		const int32 HitCount = 1,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 NormalStatus = 0,
		const int32 HardStatus = 0,
		const int32 HellStatus = 0)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::DirectDamage,
			Target);
		Effect.AttackPercent = NormalPercent;
		Effect.AttackPercentByDifficulty = Difficulty(NormalPercent, HardPercent, HellPercent);
		Effect.HitCount = HitCount;
		Effect.Status = Status;
		Effect.StatusStacks = NormalStatus;
		Effect.StatusAmountByDifficulty = Difficulty(NormalStatus, HardStatus, HellStatus);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition ApplyStatus(
		const EGameXXKCardStatus Status,
		const int32 NormalStacks,
		const int32 HardStacks,
		const int32 HellStacks,
		const EGameXXKEnemyIntentTargetRule Target,
		const bool bRequiresPreviousDirectHit = false)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::ApplyStatus,
			Target);
		Effect.Status = Status;
		Effect.StatusStacks = NormalStacks;
		Effect.StatusAmountByDifficulty = Difficulty(NormalStacks, HardStacks, HellStacks);
		Effect.bRequiresPreviousDirectHit = bRequiresPreviousDirectHit;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition PersistentStatus(
		const EGameXXKCardStatus Status,
		const int32 NormalStacks,
		const int32 HardStacks,
		const int32 HellStacks,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = ApplyStatus(
			Status,
			NormalStacks,
			HardStacks,
			HellStacks,
			Target);
		Effect.bAssignsPersistentTarget = true;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition PersistentDirect(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const bool bClearAfterResolve,
		const int32 HitCount = 1)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Direct(
			NormalPercent,
			HardPercent,
			HellPercent,
			EGameXXKEnemyIntentTargetRule::PreyTarget,
			HitCount);
		Effect.bPhaseTwoFallbackToLowestHealth = true;
		Effect.bClearsPersistentTargetAfterResolve = bClearAfterResolve;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition ArmorFromDefense(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::Self)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::AddArmorDefensePercent,
			Target);
		Effect.FlatMagnitude = NormalPercent;
		Effect.DefensePercentByDifficulty = Difficulty(NormalPercent, HardPercent, HellPercent);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition HealMaxHealth(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::HealMaxHealthPercent,
			Target);
		Effect.FlatMagnitude = NormalPercent;
		Effect.ResourceAmountByDifficulty = Difficulty(NormalPercent, HardPercent, HellPercent);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition QueueEnergyPenalty(
		const int32 Normal,
		const int32 Hard,
		const int32 Hell)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::QueueNextRoundEnergyPenalty,
			EGameXXKEnemyIntentTargetRule::None);
		Effect.FlatMagnitude = Normal;
		Effect.ResourceAmountByDifficulty = Difficulty(Normal, Hard, Hell);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition QueueCardSurcharge()
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy,
			EGameXXKEnemyIntentTargetRule::None);
		Effect.FlatMagnitude = 1;
		Effect.ResourceAmountByDifficulty = Difficulty(1, 1, 1);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition AttackBuffFromSource(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::ModifyAttack,
			EGameXXKEnemyIntentTargetRule::AllEnemyAllies);
		Effect.AttackPercent = NormalPercent;
		Effect.AttackPercentByDifficulty = Difficulty(NormalPercent, HardPercent, HellPercent);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition NextDirectFlatBonus(
		const int32 Normal,
		const int32 Hard,
		const int32 Hell)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::ModifyAttack,
			EGameXXKEnemyIntentTargetRule::AllEnemyAllies);
		Effect.FlatMagnitude = Normal;
		Effect.ResourceAmountByDifficulty = Difficulty(Normal, Hard, Hell);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition RemovePositive(
		const int32 Normal,
		const int32 Hard,
		const int32 Hell,
		const EGameXXKEnemyIntentTargetRule Target,
		const bool bRequiresPreviousDirectHit = false)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::RemovePositiveStatus,
			Target);
		Effect.FlatMagnitude = Normal;
		Effect.ResourceAmountByDifficulty = Difficulty(Normal, Hard, Hell);
		Effect.bRequiresPreviousDirectHit = bRequiresPreviousDirectHit;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition RemoveNegative(
		const int32 Count,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::RemoveNegativeStatus,
			Target);
		Effect.FlatMagnitude = Count;
		Effect.ResourceAmountByDifficulty = Difficulty(Count, Count, Count);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition TriggerDot(
		const EGameXXKCardStatus Status,
		const EGameXXKEnemyIntentTargetRule Target,
		const int32 TriggerCount = 1,
		const bool bRequiresPreviousDirectHit = true)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::TriggerDamageOverTime,
			Target);
		Effect.Status = Status;
		Effect.HitCount = TriggerCount;
		Effect.bRequiresPreviousDirectHit = bRequiresPreviousDirectHit;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition RefreshHealingAmplification(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::RefreshHealingAmplification,
			EGameXXKEnemyIntentTargetRule::Self);
		Effect.FlatMagnitude = NormalPercent;
		Effect.ResourceAmountByDifficulty = Difficulty(NormalPercent, HardPercent, HellPercent);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition DirectWithSourceStatusBonus(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const EGameXXKCardStatus SourceStatus,
		const int32 NormalPerStack,
		const int32 HardPerStack,
		const int32 HellPerStack,
		const int32 MaximumCountedStacks,
		const EGameXXKEnemyIntentTargetRule Target,
		const int32 HitCount = 1)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Direct(
			NormalPercent,
			HardPercent,
			HellPercent,
			Target,
			HitCount);
		Effect.SourceStatusForFlatMagnitude = SourceStatus;
		Effect.FlatMagnitudePerSourceStatusStack = NormalPerStack;
		Effect.ResourceAmountByDifficulty = Difficulty(NormalPerStack, HardPerStack, HellPerStack);
		Effect.MaxConsumedStacks = MaximumCountedStacks;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition DirectWithConsumedStatus(
		const int32 NormalPercent,
		const int32 HardPercent,
		const int32 HellPercent,
		const EGameXXKCardStatus ConsumedStatus,
		const int32 MaximumConsumedStacks,
		const int32 MagnitudePerStack,
		const EGameXXKEnemyIntentTargetRule Target,
		const int32 HitCount = 1,
		const EGameXXKCardStatus OnHitStatus = EGameXXKCardStatus::None,
		const int32 NormalStatus = 0,
		const int32 HardStatus = 0,
		const int32 HellStatus = 0)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Direct(
			NormalPercent,
			HardPercent,
			HellPercent,
			Target,
			HitCount,
			OnHitStatus,
			NormalStatus,
			HardStatus,
			HellStatus);
		Effect.ConsumedStatus = ConsumedStatus;
		Effect.MaxConsumedStacks = MaximumConsumedStacks;
		Effect.MagnitudePerConsumedStack = MagnitudePerStack;
		Effect.ResourceAmountByDifficulty = Difficulty(
			MagnitudePerStack,
			MagnitudePerStack,
			MagnitudePerStack);
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition HealFromConsumedWealth(
		const int32 MaximumConsumedStacks,
		const int32 NormalPercentPerStack,
		const int32 HardPercentPerStack,
		const int32 HellPercentPerStack)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = BaseEffect(
			EGameXXKEnemyIntentEffectType::ConsumeWealthForHealing,
			EGameXXKEnemyIntentTargetRule::Self);
		Effect.ConsumedStatus = EGameXXKCardStatus::Wealth;
		Effect.MaxConsumedStacks = MaximumConsumedStacks;
		Effect.MagnitudePerConsumedStack = NormalPercentPerStack;
		Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent = true;
		Effect.ResourceAmountByDifficulty = Difficulty(
			NormalPercentPerStack,
			HardPercentPerStack,
			HellPercentPerStack);
		return Effect;
	}

	FGameXXKEnemyIntentDefinition Intent(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		TArray<FGameXXKEnemyIntentEffectDefinition> Effects,
		const int32 ChargeRounds = 0,
		const bool bRequiresSourceBelowHalf = false,
		const EGameXXKCardStatus RequiredTargetStatus = EGameXXKCardStatus::None,
		const int32 CooldownRounds = 0)
	{
		FGameXXKEnemyIntentDefinition Result;
		Result.Id = Id;
		Result.DisplayName = EnemyText(DisplayName);
		Result.Effects = MoveTemp(Effects);
		Result.ChargeRounds = ChargeRounds;
		Result.bRequiresSourceBelowHalf = bRequiresSourceBelowHalf;
		Result.RequiredTargetStatus = RequiredTargetStatus;
		Result.CooldownRounds = CooldownRounds;
		return Result;
	}

	FGameXXKEnemyPhaseDefinition Phase(
		const int32 Number,
		const TCHAR* DisplayName,
		TArray<FGameXXKEnemyIntentDefinition> Intents,
		const int32 ArmorRetentionPercent = 0,
		const int32 FirstStatusGuardDefensePercent = 0,
		const int32 HealMissingHealthPercentOnBleedingPrey = 0)
	{
		FGameXXKEnemyPhaseDefinition Result;
		Result.PhaseNumber = Number;
		Result.DisplayName = EnemyText(DisplayName);
		Result.Intents = MoveTemp(Intents);
		Result.ArmorRetentionPercent = ArmorRetentionPercent;
		Result.FirstStatusGuardDefensePercent = FirstStatusGuardDefensePercent;
		Result.HealMissingHealthPercentOnBleedingPrey = HealMissingHealthPercentOnBleedingPrey;
		return Result;
	}

	FGameXXKEnemyDefinition Enemy(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const int32 Chapter,
		const EGameXXKEnemyTier Tier,
		const int32 BaseHP,
		const float HPPerLevel,
		const int32 BaseAttack,
		const float AttackPerLevel,
		const int32 BaseDefense,
		const float DefensePerLevel,
		const int32 Speed,
		TArray<FGameXXKEnemyPhaseDefinition> Phases,
		const EGameXXKEnemyPassiveId PassiveId = EGameXXKEnemyPassiveId::None)
	{
		FGameXXKEnemyDefinition Definition;
		Definition.Id = Id;
		Definition.DisplayName = EnemyText(DisplayName);
		Definition.Chapter = Chapter;
		Definition.Tier = Tier;
		Definition.BaseHP = BaseHP;
		Definition.HPPerLevel = HPPerLevel;
		Definition.BaseAttack = BaseAttack;
		Definition.AttackPerLevel = AttackPerLevel;
		Definition.BaseDefense = BaseDefense;
		Definition.DefensePerLevel = DefensePerLevel;
		Definition.Speed = Speed;
		Definition.Phases = MoveTemp(Phases);
		Definition.PassiveId = PassiveId;
		Definition.CodexId = FName(*FString::Printf(TEXT("Codex.%s"), Id));
		const FString Leaf = FString(Id).Replace(TEXT("."), TEXT("_"));
		Definition.PortraitSoftPath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/UI/Codex/RouteEnemies/V1/T_%s.T_%s"),
			*Leaf,
			*Leaf));
		Definition.BattleVisualSoftPath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/Characters/RouteEnemies/V1/FP_%s.FP_%s"),
			*Leaf,
			*Leaf));
		return Definition;
	}

	TArray<FGameXXKEnemyDefinition> BuildChapterOne()
	{
		TArray<FGameXXKEnemyDefinition> Result;
		Result.Add(Enemy(TEXT("Enemy.Ch1.Rooster"), TEXT("公鸡"), 1, EGameXXKEnemyTier::Normal, 46, 7.0f, 8, 1.1f, 1, 0.25f, 10, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Peck"), TEXT("啄击"), {Direct(150, 230, 310, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("DoublePeck"), TEXT("双重啄击"), {Direct(100, 175, 240, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 2)}),
				Intent(TEXT("Crow"), TEXT("鸣啼"), {AttackBuffFromSource(20, 35, 50)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch1.Goat"), TEXT("山羊"), 1, EGameXXKEnemyTier::Normal, 58, 8.0f, 7, 1.0f, 3, 0.35f, 6, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Horn"), TEXT("顶角"), {Direct(140, 210, 280, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 2, 3, 4)}),
				Intent(TEXT("Stomp"), TEXT("踏地"), {ArmorFromDefense(200, 280, 360)}),
				Intent(TEXT("Charge"), TEXT("冲撞"), {Direct(240, 340, 450)}, 1)
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch1.Weasel"), TEXT("黄鼬"), 1, EGameXXKEnemyTier::Normal, 42, 6.0f, 9, 1.2f, 1, 0.20f, 11, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Harass"), TEXT("骚扰"), {Direct(130, 200, 270, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 2, 3, 5)}),
				Intent(TEXT("StinkFog"), TEXT("臭雾"), {Direct(80, 140, 200, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 1, 2, 3)}),
				Intent(TEXT("Escape"), TEXT("遁逃"), {ArmorFromDefense(200, 280, 360), ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch1.Civet"), TEXT("狸猫"), 1, EGameXXKEnemyTier::Normal, 48, 7.0f, 8, 1.1f, 2, 0.25f, 9, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Claw"), TEXT("爪击"), {Direct(150, 225, 300, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("Feint"), TEXT("佯攻"), {Direct(100, 165, 230, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 2, 3, 5)}),
				Intent(TEXT("Pickpocket"), TEXT("扒窃"), {
					Direct(120, 185, 250),
					QueueEnergyPenalty(1, 2, 2),
					ArmorFromDefense(120, 180, 240)})
			})
		}));

		Result.Add(Enemy(TEXT("Enemy.Ch1.IronfeatherRooster"), TEXT("铁羽斗鸡"), 1, EGameXXKEnemyTier::Elite, 118, 15.0f, 14, 1.7f, 5, 0.50f, 11, {
			Phase(1, TEXT("铁羽本相"), {
				Intent(TEXT("RapidPeck"), TEXT("疾啄"), {Direct(80, 115, 150, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3)}),
				Intent(TEXT("IronGuard"), TEXT("铁羽守势"), {ArmorFromDefense(160, 220, 280)}),
				Intent(TEXT("BattleCry"), TEXT("斗鸣"), {AttackBuffFromSource(20, 35, 50)}),
				Intent(TEXT("BloodFight"), TEXT("血斗"), {Direct(160, 190, 210, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Burn, 2, 4, 6)}, 0, true)
			}),
			Phase(2, TEXT("烈羽焚阵"), {
				Intent(TEXT("IronfeatherBurningFormation"), TEXT("烈羽焚阵"), {
					Direct(80, 80, 80, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Burn, 2, 2, 3),
					ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("ChaseFireRapidPeck"), TEXT("逐火疾啄"), {
					Direct(160, 160, 160, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("IronfeatherGuardFlock"), TEXT("铁羽护群"), {
					ArmorFromDefense(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("BloodFeatherPounce"), TEXT("血羽扑杀"), {
					Direct(220, 220, 220, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Burn, 4, 4, 6)})
			}),
			Phase(3, TEXT("血羽不熄"), {
				Intent(TEXT("BloodFeatherBurnsSky"), TEXT("血羽焚天"), {
					Direct(105, 105, 105, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Burn, 3, 3, 3),
					ApplyStatus(EGameXXKCardStatus::Mark, 5, 5, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("LifeBoundRapidPeck"), TEXT("搏命疾啄"), {
					Direct(180, 180, 180, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 4),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("UnfallenIronfeather"), TEXT("铁羽不坠"), {
					ArmorFromDefense(150, 150, 150, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("DeathBurningBeak"), TEXT("焚命喙"), {
					Direct(260, 260, 260, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Burn, 6, 6, 6),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			})
		}, EGameXXKEnemyPassiveId::IronfeatherFirstHit));

		Result.Add(Enemy(TEXT("Enemy.Ch1.BluehornGoatKing"), TEXT("青角羊王"), 1, EGameXXKEnemyTier::Elite, 138, 17.0f, 13, 1.6f, 7, 0.65f, 7, {
			Phase(1, TEXT("青角本相"), {
				Intent(TEXT("Pierce"), TEXT("贯角"), {Direct(120, 150, 180, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 2, 3, 4)}),
				Intent(TEXT("HerdStomp"), TEXT("群踏"), {Direct(80, 110, 140, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("GuardHerd"), TEXT("护群"), {ArmorFromDefense(80, 100, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("RageCharge"), TEXT("怒角冲撞"), {Direct(210, 240, 270)}, 1)
			}, 50),
			Phase(2, TEXT("青角镇山"), {
				Intent(TEXT("FireSteppingPierce"), TEXT("踏火贯角"), {
					Direct(190, 190, 190, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 3, 3, 4),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("BluehornSuppressesMountains"), TEXT("青角镇山"), {ArmorFromDefense(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("FormationBreakingHerdStomp"), TEXT("破阵群踏"), {Direct(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 2, 2, 3)}),
				Intent(TEXT("KingHornBreaksPass"), TEXT("王角破关"), {
					Direct(280, 280, 280),
					ApplyStatus(EGameXXKCardStatus::Vulnerability, 3, 3, 5, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, true)}, 1)
			}, 75),
			Phase(3, TEXT("王角踏天关"), {
				Intent(TEXT("HeavenlyPassPierce"), TEXT("天关贯角"), {
					Direct(230, 230, 230, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 4, 4, 4),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("TenThousandHornsGuardMountain"), TEXT("万角护山"), {ArmorFromDefense(180, 180, 180, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("FallingPeaks"), TEXT("群峰坠"), {
					Direct(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 3, 3, 3),
					RemovePositive(1, 1, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("KingHornTreadsHeavenlyPass"), TEXT("王角踏天关"), {
					Direct(340, 340, 340),
					ApplyStatus(EGameXXKCardStatus::Vulnerability, 5, 5, 5, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, true),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}, 1)
			}, 100)
		}, EGameXXKEnemyPassiveId::BluehornArmorRetention));

		Result.Add(Enemy(TEXT("Enemy.Ch1.MoneyRat"), TEXT("金钱鼠"), 1, EGameXXKEnemyTier::Boss, 240, 24.0f, 17, 2.0f, 8, 0.75f, 10, {
			Phase(1, TEXT("聚财本相"), {
				Intent(TEXT("CoinVolley"), TEXT("撒币"), {Direct(80, 100, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("Hoard"), TEXT("敛财"), {
					ArmorFromDefense(160, 200, 240),
					ApplyStatus(EGameXXKCardStatus::Wealth, 2, 2, 3, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("GreedyMark"), TEXT("贪印"), {ApplyStatus(EGameXXKCardStatus::Mark, 2, 3, 5, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				Intent(TEXT("Pickpocket"), TEXT("扒窃"), {
					Direct(100, 125, 150),
					QueueEnergyPenalty(1, 2, 2),
					ApplyStatus(EGameXXKCardStatus::Wealth, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("BreakWealth"), TEXT("散财疗伤"), {HealFromConsumedWealth(3, 3, 4, 4)}),
				Intent(TEXT("CoinCrash"), TEXT("钱潮冲击"), {
					DirectWithSourceStatusBonus(110, 125, 140, EGameXXKCardStatus::Wealth, 10, 12, 15, 4, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			}),
			Phase(2, TEXT("闭门催收"), {
				Intent(TEXT("CloseGateCollection"), TEXT("闭门催收"), {
					Direct(150, 150, 150),
					QueueEnergyPenalty(2, 2, 2),
					ApplyStatus(EGameXXKCardStatus::Vulnerability, 3, 3, 5, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, true),
					ApplyStatus(EGameXXKCardStatus::Wealth, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("HoardBehindClosedGates"), TEXT("闭门聚财"), {
					ApplyStatus(EGameXXKCardStatus::Wealth, 3, 3, 3, EGameXXKEnemyIntentTargetRule::Self),
					ArmorFromDefense(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("CompoundInterest"), TEXT("利滚利"), {
					DirectWithSourceStatusBonus(95, 95, 95, EGameXXKCardStatus::Wealth, 20, 20, 20, 4, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("SpendWealthContinueLifeP2"), TEXT("散财续命"), {
					HealFromConsumedWealth(3, 5, 5, 5),
					ArmorFromDefense(80, 80, 80, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("CoinTideCrushesVault"), TEXT("钱潮压库"), {
					DirectWithConsumedStatus(240, 240, 240, EGameXXKCardStatus::Wealth, 4, 25, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}, 1)
			}),
			Phase(3, TEXT("万钱压库"), {
				Intent(TEXT("EmptyGoldenMountain"), TEXT("倾尽金山"), {
					Direct(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty),
					ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 3, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("HeavyInterestLockdown"), TEXT("重利封锁"), {
					ApplyStatus(EGameXXKCardStatus::Wealth, 3, 3, 3, EGameXXKEnemyIntentTargetRule::Self),
					ArmorFromDefense(180, 180, 180, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					QueueCardSurcharge()}),
				Intent(TEXT("LifePressingCollection"), TEXT("逼命催收"), {
					Direct(200, 200, 200),
					QueueEnergyPenalty(3, 3, 3),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom),
					ApplyStatus(EGameXXKCardStatus::Wealth, 2, 2, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("SpendWealthContinueLifeP3"), TEXT("散财续命"), {
					HealFromConsumedWealth(3, 4, 4, 4),
					ArmorFromDefense(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("TenThousandCoinCrush"), TEXT("万钱压库"), {
					DirectWithConsumedStatus(110, 110, 110, EGameXXKCardStatus::Wealth, 6, 10, EGameXXKEnemyIntentTargetRule::AllLivingParty)}, 1)
			})
		}, EGameXXKEnemyPassiveId::MoneyRatWealth));
		return Result;
	}

	TArray<FGameXXKEnemyDefinition> BuildChapterTwo()
	{
		TArray<FGameXXKEnemyDefinition> Result;
		Result.Add(Enemy(TEXT("Enemy.Ch2.GrayWolf"), TEXT("灰狼"), 2, EGameXXKEnemyTier::Normal, 62, 9.0f, 11, 1.3f, 2, 0.30f, 12, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Bite"), TEXT("咬击"), {Direct(160, 230, 300, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 2, 3, 5)}),
				Intent(TEXT("Pursuit"), TEXT("追猎"), {Direct(140, 200, 260)}),
				Intent(TEXT("CallPack"), TEXT("呼群"), {AttackBuffFromSource(20, 35, 50)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch2.Boar"), TEXT("野猪"), 2, EGameXXKEnemyTier::Normal, 76, 10.0f, 10, 1.2f, 5, 0.45f, 7, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Tusk"), TEXT("獠牙突刺"), {Direct(180, 250, 330)}),
				Intent(TEXT("Bristle"), TEXT("鬃毛守势"), {ArmorFromDefense(200, 280, 360)}),
				Intent(TEXT("ArmorBreakCharge"), TEXT("破阵冲锋"), {Direct(240, 340, 450, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 2, 3, 4)}, 1)
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch2.Macaque"), TEXT("猕猴"), 2, EGameXXKEnemyTier::Normal, 58, 8.0f, 10, 1.3f, 2, 0.25f, 13, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("ThrowStone"), TEXT("掷石"), {Direct(160, 230, 300, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				Intent(TEXT("Snatch"), TEXT("夺势"), {
					Direct(100, 160, 220, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					RemovePositive(1, 1, 2, EGameXXKEnemyIntentTargetRule::LowestHealthParty, true)}),
				Intent(TEXT("Hasten"), TEXT("催行"), {
					ArmorFromDefense(150, 220, 300, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch2.Porcupine"), TEXT("豪猪"), 2, EGameXXKEnemyTier::Normal, 70, 9.0f, 9, 1.1f, 5, 0.50f, 8, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Quill"), TEXT("刺毛"), {Direct(160, 230, 300, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 3, 5, 8)}),
				Intent(TEXT("BristleGuard"), TEXT("蓄刺"), {
					ArmorFromDefense(200, 280, 360),
					ApplyStatus(EGameXXKCardStatus::Block, 1, 2, 3, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("QuillVolley"), TEXT("飞刺"), {Direct(100, 155, 210, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Bleed, 1, 3, 5)})
			})
		}, EGameXXKEnemyPassiveId::PorcupineCounter));

		Result.Add(Enemy(TEXT("Enemy.Ch2.GraymaneWolfKing"), TEXT("苍鬃狼王"), 2, EGameXXKEnemyTier::Elite, 158, 18.0f, 18, 2.0f, 6, 0.55f, 13, {
			Phase(1, TEXT("苍鬃本相"), {
				Intent(TEXT("HuntMark"), TEXT("猎印"), {Direct(110, 130, 150, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 2, 3, 5)}),
				Intent(TEXT("ContinuousHunt"), TEXT("连环狩猎"), {Direct(80, 105, 130, EGameXXKEnemyIntentTargetRule::MarkedParty, 3)}, 0, false, EGameXXKCardStatus::Mark),
				Intent(TEXT("PackOrder"), TEXT("群猎号令"), {
					AttackBuffFromSource(20, 35, 50),
					ApplyStatus(EGameXXKCardStatus::Mark, 2, 3, 5, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				Intent(TEXT("Sidestep"), TEXT("侧跃"), {
					ArmorFromDefense(160, 220, 280),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 2, EGameXXKEnemyIntentTargetRule::Self)})
			}),
			Phase(2, TEXT("月下猎令"), {
				Intent(TEXT("MoonlitHuntingOrder"), TEXT("月下猎令"), {Direct(90, 90, 90, EGameXXKEnemyIntentTargetRule::LowestHealthParty, 1, EGameXXKCardStatus::Mark, 3, 3, 5)}),
				Intent(TEXT("ThroatRendingChainHunt"), TEXT("裂喉连猎"), {Direct(150, 150, 150, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3, EGameXXKCardStatus::Bleed, 5, 5, 8)}),
				Intent(TEXT("PackPositionSwap"), TEXT("群位换形"), {
					ArmorFromDefense(80, 80, 80, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 2, 2, 2, EGameXXKEnemyIntentTargetRule::Self),
					ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("BloodMoonPursuit"), TEXT("血月追猎"), {
					Direct(230, 230, 230),
					TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			}),
			Phase(3, TEXT("血月断喉"), {
				Intent(TEXT("BloodMoonHuntMark"), TEXT("血月猎印"), {
					Direct(90, 90, 90, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Mark, 3, 3, 3),
					ApplyStatus(EGameXXKCardStatus::Bleed, 5, 5, 5, EGameXXKEnemyIntentTargetRule::AllLivingParty, true)}),
				Intent(TEXT("ThroatSeveringPackHunt"), TEXT("断喉群猎"), {
					Direct(180, 180, 180, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3),
					TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("AfterimageHuntSwap"), TEXT("残影猎换"), {
					ArmorFromDefense(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Mark, 5, 5, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("BloodMoonSeversThroat"), TEXT("血月断喉"), {
					Direct(260, 260, 260, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 8, 8, 8),
					TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			})
		}, EGameXXKEnemyPassiveId::GraymaneMarkedHunt));

		Result.Add(Enemy(TEXT("Enemy.Ch2.RedtuskBoarKing"), TEXT("赤獠猪王"), 2, EGameXXKEnemyTier::Elite, 188, 20.0f, 17, 1.9f, 9, 0.75f, 8, {
			Phase(1, TEXT("赤獠本相"), {
				Intent(TEXT("HeavyArmor"), TEXT("厚甲"), {ArmorFromDefense(200, 280, 360)}),
				Intent(TEXT("Earthquake"), TEXT("震地"), {Direct(80, 100, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 1, 2, 3)}),
				Intent(TEXT("RageStrike"), TEXT("怒獠"), {DirectWithSourceStatusBonus(150, 180, 210, EGameXXKCardStatus::Rage, 20, 20, 20, 5, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("RedCharge"), TEXT("赤牙冲锋"), {Direct(210, 250, 290)}, 1)
			}),
			Phase(2, TEXT("赤焰怒踏"), {
				Intent(TEXT("RageTuskBreaksFormation"), TEXT("怒獠破阵"), {
					Direct(170, 170, 170, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 3, 3, 4),
					ApplyStatus(EGameXXKCardStatus::Burn, 4, 4, 6, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, true)}),
				Intent(TEXT("RedFlameRageStomp"), TEXT("赤焰怒踏"), {
					Direct(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 2, 2, 3),
					ApplyStatus(EGameXXKCardStatus::Burn, 2, 2, 3, EGameXXKEnemyIntentTargetRule::AllLivingParty, true)}),
				Intent(TEXT("HeavyArmorStoresRage"), TEXT("厚甲蓄怒"), {
					ArmorFromDefense(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Rage, 2, 2, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("RedTuskCharge"), TEXT("赤獠冲锋"), {
					DirectWithConsumedStatus(250, 250, 250, EGameXXKCardStatus::Rage, 5, 20, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}, 1)
			}),
			Phase(3, TEXT("赤獠焚山"), {
				Intent(TEXT("RedtuskBurnsMountain"), TEXT("赤獠焚山"), {
					Direct(210, 210, 210, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 4, 4, 4),
					ApplyStatus(EGameXXKCardStatus::Burn, 6, 6, 6, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, true),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("MadTuskChainStomp"), TEXT("狂獠连踏"), {
					DirectWithSourceStatusBonus(105, 105, 105, EGameXXKCardStatus::Rage, 8, 8, 8, 5, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3)}),
				Intent(TEXT("FireBathedHeavyArmor"), TEXT("浴火厚甲"), {
					ArmorFromDefense(150, 150, 150, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Rage, 2, 2, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("MountainBurningFinalCharge"), TEXT("焚山终冲"), {
					DirectWithConsumedStatus(280, 280, 280, EGameXXKCardStatus::Rage, 5, 20, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Burn, 6, 6, 6),
					TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}, 1)
			})
		}, EGameXXKEnemyPassiveId::RedtuskRage));

		Result.Add(Enemy(TEXT("Enemy.Ch2.BlackBear"), TEXT("黑熊"), 2, EGameXXKEnemyTier::Boss, 320, 30.0f, 23, 2.4f, 11, 0.90f, 7, {
			Phase(1, TEXT("厚皮本相"), {
				Intent(TEXT("Sweep"), TEXT("横扫"), {Direct(90, 110, 130, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("Pounce"), TEXT("扑杀"), {Direct(170, 200, 230)}),
				Intent(TEXT("WeakRoar"), TEXT("震慑咆哮"), {ApplyStatus(EGameXXKCardStatus::Weak, 1, 2, 3, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("Rend"), TEXT("撕裂"), {Direct(150, 180, 210, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 3, 5, 8)}),
				Intent(TEXT("CounterPosture"), TEXT("反击架势"), {ApplyStatus(EGameXXKCardStatus::Counter, 1, 2, 3, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("Quake"), TEXT("裂地"), {Direct(110, 135, 160, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 1, 2, 3)})
			}),
			Phase(2, TEXT("裂地狂熊"), {
				Intent(TEXT("BloodClawRend"), TEXT("血爪撕裂"), {Direct(170, 170, 170, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 5, 5, 8)}),
				Intent(TEXT("AngryBearCounterstance"), TEXT("怒熊反架"), {
					ArmorFromDefense(180, 180, 180),
					ApplyStatus(EGameXXKCardStatus::Counter, 2, 2, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("EarthSplittingMadBear"), TEXT("裂地狂熊"), {
					Direct(110, 110, 110, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 2, 2, 3),
					ApplyStatus(EGameXXKCardStatus::Bleed, 3, 3, 5, EGameXXKEnemyIntentTargetRule::AllLivingParty, true)}),
				Intent(TEXT("CorneredBeastPounce"), TEXT("困兽扑杀"), {Direct(260, 260, 260)}, 1),
				Intent(TEXT("MountainShakingSweep"), TEXT("撼山横扫"), {Direct(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty)})
			}),
			Phase(3, TEXT("困兽撼山"), {
				Intent(TEXT("LastStandCounterstance"), TEXT("背水反架"), {
					ArmorFromDefense(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Counter, 3, 3, 3, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("BloodBattleThroatRend"), TEXT("血战裂喉"), {Direct(220, 220, 220, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("CorneredBeastShakesMountain"), TEXT("困兽撼山"), {
					Direct(130, 130, 130, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 3, 3, 3),
					ApplyStatus(EGameXXKCardStatus::Bleed, 5, 5, 5, EGameXXKEnemyIntentTargetRule::AllLivingParty, true)}),
				Intent(TEXT("DeathPounce"), TEXT("死斗扑杀"), {Direct(290, 290, 290, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 8, 8, 8)}, 1),
				Intent(TEXT("BloodBattleNeverRetreats"), TEXT("血战不退"), {Direct(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty)})
			})
		}, EGameXXKEnemyPassiveId::BlackBearThickHide));
		return Result;
	}

	TArray<FGameXXKEnemyDefinition> BuildChapterThree()
	{
		TArray<FGameXXKEnemyDefinition> Result;
		Result.Add(Enemy(TEXT("Enemy.Ch3.VenomSnake"), TEXT("毒蛇"), 3, EGameXXKEnemyTier::Normal, 72, 9.0f, 12, 1.35f, 2, 0.25f, 14, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("VenomBite"), TEXT("毒牙"), {Direct(150, 215, 280, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Poison, 3, 6, 9)}),
				Intent(TEXT("Coil"), TEXT("盘缠"), {ArmorFromDefense(200, 280, 360), ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("ToxicPursuit"), TEXT("毒袭"), {
					Direct(160, 230, 300),
					TriggerDot(EGameXXKCardStatus::Poison, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch3.Wildcat"), TEXT("山猫"), 3, EGameXXKEnemyTier::Normal, 70, 9.0f, 14, 1.50f, 3, 0.30f, 14, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Rake"), TEXT("抓挠"), {Direct(160, 230, 300, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 3, 5, 8)}),
				Intent(TEXT("Stalk"), TEXT("潜伏"), {
					ApplyStatus(EGameXXKCardStatus::Mark, 2, 3, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("BloodPursuit"), TEXT("血猎"), {
					Direct(160, 230, 300),
					TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch3.Vulture"), TEXT("秃鹫"), 3, EGameXXKEnemyTier::Normal, 74, 9.0f, 13, 1.45f, 3, 0.30f, 15, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Gaze"), TEXT("凝视"), {
					ApplyStatus(EGameXXKCardStatus::Mark, 2, 3, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					ApplyStatus(EGameXXKCardStatus::Burn, 2, 4, 6, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("Dive"), TEXT("俯冲"), {Direct(180, 260, 340), TriggerDot(EGameXXKCardStatus::Burn, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)}),
				Intent(TEXT("WingCut"), TEXT("翼斩"), {Direct(110, 160, 210, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Burn, 1, 2, 3)})
			})
		}));
		Result.Add(Enemy(TEXT("Enemy.Ch3.GiantToad"), TEXT("巨蟾"), 3, EGameXXKEnemyTier::Normal, 94, 12.0f, 11, 1.25f, 7, 0.60f, 6, {
			Phase(1, TEXT("本相"), {
				Intent(TEXT("Tongue"), TEXT("卷舌"), {Direct(160, 230, 300), HealMaxHealth(4, 5, 6, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("PoisonFog"), TEXT("毒雾"), {Direct(90, 140, 190, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Poison, 2, 4, 6)}),
				Intent(TEXT("Inflate"), TEXT("鼓腹"), {ArmorFromDefense(200, 280, 360), RefreshHealingAmplification(4, 5, 6)})
			})
		}));

		Result.Add(Enemy(TEXT("Enemy.Ch3.WhiteApe"), TEXT("白猿"), 3, EGameXXKEnemyTier::Elite, 198, 21.0f, 21, 2.20f, 8, 0.65f, 12, {
			Phase(1, TEXT("白猿本相"), {
				Intent(TEXT("ThrowRock"), TEXT("掷岩"), {PersistentDirect(120, 140, 160, false)}),
				Intent(TEXT("Disturb"), TEXT("扰乱"), {QueueCardSurcharge()}),
				Intent(TEXT("BoulderCharge"), TEXT("巨岩冲撞"), {PersistentDirect(220, 245, 260, false)}, 1),
				Intent(TEXT("WideSweep"), TEXT("广横扫"), {Direct(85, 100, 115, EGameXXKEnemyIntentTargetRule::AllLivingParty)})
			}, 0, 80),
			Phase(2, TEXT("乱石封脉"), {
				Intent(TEXT("ChaoticRocksSealMeridians"), TEXT("乱石封脉"), {
					PersistentDirect(160, 160, 160, false),
					RemovePositive(1, 1, 1, EGameXXKEnemyIntentTargetRule::PreyTarget, true),
					QueueCardSurcharge()}),
				Intent(TEXT("ApeHowlDisruptsFormation"), TEXT("猿啸乱阵"), {Direct(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 2, 2, 3)}),
				Intent(TEXT("FlyingRocksGuardGroup"), TEXT("飞石护群"), {ArmorFromDefense(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies), ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("GiantRockCharge"), TEXT("巨岩冲阵"), {PersistentDirect(270, 270, 270, false), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget)}, 1)
			}, 0, 100),
			Phase(3, TEXT("猿王碎阵"), {
				Intent(TEXT("ApeKingSealsMeridians"), TEXT("猿王封脉"), {
					PersistentDirect(200, 200, 200, false),
					RemovePositive(2, 2, 2, EGameXXKEnemyIntentTargetRule::PreyTarget, true),
					QueueCardSurcharge(),
					TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget)}),
				Intent(TEXT("ChaoticRocksFallHeaven"), TEXT("乱石天降"), {Direct(125, 125, 125, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 3, 3, 3), QueueCardSurcharge()}),
				Intent(TEXT("TenThousandStonesGuardFormation"), TEXT("万石护阵"), {ArmorFromDefense(180, 180, 180, EGameXXKEnemyIntentTargetRule::AllEnemyAllies), ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				Intent(TEXT("MountainCrushingBoulder"), TEXT("压山巨岩"), {PersistentDirect(310, 310, 310, false), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget)}, 1)
			}, 0, 160)
		}, EGameXXKEnemyPassiveId::WhiteApeStatusGuard));

		Result.Add(Enemy(TEXT("Enemy.Ch3.SpiralHornDeer"), TEXT("盘角鹿"), 3, EGameXXKEnemyTier::Elite, 210, 22.0f, 20, 2.10f, 9, 0.75f, 11, {
			Phase(1, TEXT("盘角本相"), {
				Intent(TEXT("Horn"), TEXT("盘角突击"), {PersistentDirect(110, 125, 140, false)}),
				Intent(TEXT("TerrainBless"), TEXT("地势祝福"), {NextDirectFlatBonus(20, 30, 40)}),
				Intent(TEXT("HerdArmor"), TEXT("群护"), {ArmorFromDefense(80, 100, 120, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("SpringHeal"), TEXT("回春"), {HealMaxHealth(6, 8, 10, EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly)}, 0, false, EGameXXKCardStatus::None, 2)
			}),
			Phase(2, TEXT("群峰回春阵"), {
				Intent(TEXT("PeaksGuardFormation"), TEXT("群峰护阵"), {ArmorFromDefense(150, 150, 150, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("SpiralHornInterceptsHunt"), TEXT("盘角截猎"), {PersistentDirect(140, 140, 140, false), ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 5, EGameXXKEnemyIntentTargetRule::PreyTarget, true)}),
				Intent(TEXT("ForestBreathRejuvenation"), TEXT("林息回春"), {
					HealMaxHealth(12, 12, 12, EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly),
					RemoveNegative(1, EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly)}, 0, false, EGameXXKCardStatus::None, 3),
				Intent(TEXT("DeerCryGuardstance"), TEXT("鹿鸣守势"), {
					ArmorFromDefense(100, 100, 100, EGameXXKEnemyIntentTargetRule::AllEnemyAllies),
					ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly)})
			}),
			Phase(3, TEXT("万木镇山河"), {
				Intent(TEXT("TenThousandTreesSuppress"), TEXT("万木镇山河"), {ArmorFromDefense(200, 200, 200, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				Intent(TEXT("SpiralHornBreaksHunt"), TEXT("盘角破猎"), {PersistentDirect(170, 170, 170, false), ApplyStatus(EGameXXKCardStatus::Mark, 5, 5, 5, EGameXXKEnemyIntentTargetRule::PreyTarget, true)}),
				Intent(TEXT("PeaksRejuvenation"), TEXT("群峰回春"), {HealMaxHealth(6, 6, 6, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}, 0, false, EGameXXKCardStatus::None, 4),
				Intent(TEXT("AncientForestShelter"), TEXT("古林庇护"), {ArmorFromDefense(160, 160, 160, EGameXXKEnemyIntentTargetRule::AllEnemyAllies), ApplyStatus(EGameXXKCardStatus::Agility, 1, 1, 1, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)})
			})
		}, EGameXXKEnemyPassiveId::DeerHealCooldown));

		Result.Add(Enemy(TEXT("Enemy.Ch3.Tiger"), TEXT("老虎"), 3, EGameXXKEnemyTier::Boss, 380, 34.0f, 28, 2.70f, 12, 1.00f, 14, {
			Phase(1, TEXT("猛虎本相"), {
				Intent(TEXT("MarkPrey"), TEXT("标记猎物"), {PersistentStatus(EGameXXKCardStatus::Prey, 1, 1, 1, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				Intent(TEXT("TigerPounce"), TEXT("虎扑"), {PersistentDirect(150, 170, 190, true)}, 0, false, EGameXXKCardStatus::Prey),
				Intent(TEXT("TailSweep"), TEXT("尾扫"), {Direct(95, 110, 125, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("BleedingRend"), TEXT("裂伤"), {Direct(120, 150, 180, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 3, 5, 8)}),
				Intent(TEXT("DreadRoar"), TEXT("威吓咆哮"), {ApplyStatus(EGameXXKCardStatus::Weak, 1, 2, 3, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("Ambush"), TEXT("伏杀"), {Direct(240, 270, 300)}, 1)
			}, 0, 0, 8),
			Phase(2, TEXT("血猎封锁"), {
				Intent(TEXT("BloodHuntLockdown"), TEXT("血猎封锁"), {
					PersistentStatus(EGameXXKCardStatus::Prey, 1, 1, 1, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					PersistentDirect(160, 160, 160, false),
					ApplyStatus(EGameXXKCardStatus::Bleed, 5, 5, 8, EGameXXKEnemyIntentTargetRule::PreyTarget, true),
					ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 5, EGameXXKEnemyIntentTargetRule::PreyTarget, true)}),
				Intent(TEXT("TailSweepChangesPrey"), TEXT("尾扫换猎"), {
					Direct(110, 110, 110, EGameXXKEnemyIntentTargetRule::AllLivingParty),
					PersistentStatus(EGameXXKCardStatus::Prey, 1, 1, 1, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					ApplyStatus(EGameXXKCardStatus::Mark, 3, 3, 5, EGameXXKEnemyIntentTargetRule::LowestHealthParty)}),
				Intent(TEXT("WoundPursuit"), TEXT("逐伤"), {PersistentDirect(120, 120, 120, false, 2), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget)}),
				Intent(TEXT("IntimidatingRoar"), TEXT("震胆咆哮"), {ApplyStatus(EGameXXKCardStatus::Weak, 2, 2, 3, EGameXXKEnemyIntentTargetRule::AllLivingParty), ApplyStatus(EGameXXKCardStatus::Vulnerability, 3, 3, 5, EGameXXKEnemyIntentTargetRule::PreyTarget)}),
				Intent(TEXT("GroundedPounce"), TEXT("伏地扑杀"), {PersistentDirect(290, 290, 290, false)}, 1)
			}, 0, 0, 8),
			Phase(3, TEXT("百兽血月"), {
				Intent(TEXT("BloodMoonHundredBeasts"), TEXT("百兽血月"), {
					PersistentStatus(EGameXXKCardStatus::Prey, 1, 1, 1, EGameXXKEnemyIntentTargetRule::LowestHealthParty),
					Direct(120, 120, 120, EGameXXKEnemyIntentTargetRule::AllLivingParty),
					ApplyStatus(EGameXXKCardStatus::Bleed, 8, 8, 8, EGameXXKEnemyIntentTargetRule::PreyTarget),
					ApplyStatus(EGameXXKCardStatus::Mark, 5, 5, 5, EGameXXKEnemyIntentTargetRule::PreyTarget)}),
				Intent(TEXT("TigerDeathPounce"), TEXT("夺命扑杀"), {PersistentDirect(135, 135, 135, false, 2), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget)}),
				Intent(TEXT("TailSeversMountains"), TEXT("虎尾断山"), {Direct(140, 140, 140, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 3, 3, 3), RemovePositive(1, 1, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				Intent(TEXT("FeedOnWounds"), TEXT("噬伤"), {PersistentDirect(210, 210, 210, false), TriggerDot(EGameXXKCardStatus::Bleed, EGameXXKEnemyIntentTargetRule::PreyTarget, 2)}),
				Intent(TEXT("FatalAmbush"), TEXT("绝命伏杀"), {PersistentDirect(320, 320, 320, false)}, 1)
			}, 0, 0, 12)
		}, EGameXXKEnemyPassiveId::TigerPredator));
		return Result;
	}

	TArray<FGameXXKEnemyDefinition> BuildDefinitions()
	{
		TArray<FGameXXKEnemyDefinition> Definitions;
		Definitions.Reserve(21);
		Definitions.Append(BuildChapterOne());
		Definitions.Append(BuildChapterTwo());
		Definitions.Append(BuildChapterThree());
		return Definitions;
	}

	int32 RoundHalfAwayFromZero(const float Value)
	{
		return Value >= 0.0f
			? FMath::FloorToInt(Value + 0.5f)
			: FMath::CeilToInt(Value - 0.5f);
	}

	bool IsValidEffect(const FGameXXKEnemyIntentEffectDefinition& Effect)
	{
		const auto AllPositive = [](const FGameXXKEnemyDifficultyInt& Value)
		{
			return Value.Normal > 0 && Value.Hard > 0 && Value.Hell > 0;
		};
		switch (Effect.Type)
		{
		case EGameXXKEnemyIntentEffectType::DirectDamage:
			return AllPositive(Effect.AttackPercentByDifficulty)
				&& Effect.HitCount > 0
				&& (Effect.Status == EGameXXKCardStatus::None
					|| (Effect.Status != EGameXXKCardStatus::Invalid
						&& AllPositive(Effect.StatusAmountByDifficulty)));
		case EGameXXKEnemyIntentEffectType::ApplyStatus:
			return Effect.Status != EGameXXKCardStatus::None
				&& Effect.Status != EGameXXKCardStatus::Invalid
				&& AllPositive(Effect.StatusAmountByDifficulty);
		case EGameXXKEnemyIntentEffectType::AddArmorDefensePercent:
			return AllPositive(Effect.DefensePercentByDifficulty);
		case EGameXXKEnemyIntentEffectType::HealMaxHealthPercent:
		case EGameXXKEnemyIntentEffectType::QueueNextRoundEnergyPenalty:
		case EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy:
		case EGameXXKEnemyIntentEffectType::RemovePositiveStatus:
		case EGameXXKEnemyIntentEffectType::RemoveNegativeStatus:
		case EGameXXKEnemyIntentEffectType::RefreshHealingAmplification:
			return AllPositive(Effect.ResourceAmountByDifficulty);
		case EGameXXKEnemyIntentEffectType::ModifyAttack:
			return AllPositive(Effect.AttackPercentByDifficulty)
				|| AllPositive(Effect.ResourceAmountByDifficulty);
		case EGameXXKEnemyIntentEffectType::TriggerDamageOverTime:
			return (Effect.Status == EGameXXKCardStatus::Bleed
					|| Effect.Status == EGameXXKCardStatus::Poison
					|| Effect.Status == EGameXXKCardStatus::Burn
					|| Effect.Status == EGameXXKCardStatus::DamageOverTime)
				&& Effect.HitCount > 0;
		case EGameXXKEnemyIntentEffectType::ConsumeWealthForHealing:
			return Effect.ConsumedStatus == EGameXXKCardStatus::Wealth
				&& Effect.MaxConsumedStacks > 0
				&& Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent
				&& AllPositive(Effect.ResourceAmountByDifficulty);
		default:
			return false;
		}
	}
}

const TArray<FGameXXKEnemyDefinition>& FGameXXKEnemyCatalog::GetAllDefinitions()
{
	static const TArray<FGameXXKEnemyDefinition> Definitions = BuildDefinitions();
	return Definitions;
}

const FGameXXKEnemyDefinition* FGameXXKEnemyCatalog::Find(const FName DefinitionId)
{
	return GetAllDefinitions().FindByPredicate([DefinitionId](const FGameXXKEnemyDefinition& Definition)
	{
		return Definition.Id == DefinitionId;
	});
}

TArray<FName> FGameXXKEnemyCatalog::GetPool(
	const int32 Chapter,
	const EGameXXKEnemyTier Tier)
{
	TArray<FName> Pool;
	for (const FGameXXKEnemyDefinition& Definition : GetAllDefinitions())
	{
		if (Definition.Chapter == Chapter && Definition.Tier == Tier)
		{
			Pool.Add(Definition.Id);
		}
	}
	return Pool;
}

int32 FGameXXKEnemyCatalog::ResolveTotalPhases(
	const EGameXXKEnemyTier Tier,
	const EGameXXKEnemyDifficulty Difficulty)
{
	if (Tier == EGameXXKEnemyTier::Normal)
	{
		return 1;
	}
	if (Difficulty == EGameXXKEnemyDifficulty::Hell)
	{
		return 3;
	}
	return Difficulty == EGameXXKEnemyDifficulty::Hard ? 2 : 1;
}

const FGameXXKEnemyPhaseDefinition* FGameXXKEnemyCatalog::GetPhaseDefinition(
	const FGameXXKEnemyDefinition& Definition,
	const int32 PhaseNumber)
{
	return Definition.Phases.FindByPredicate([PhaseNumber](const FGameXXKEnemyPhaseDefinition& Phase)
	{
		return Phase.PhaseNumber == PhaseNumber;
	});
}

const TArray<FGameXXKEnemyIntentDefinition>* FGameXXKEnemyCatalog::GetPhaseIntents(
	const FGameXXKEnemyDefinition& Definition,
	const int32 PhaseNumber)
{
	const FGameXXKEnemyPhaseDefinition* Phase = GetPhaseDefinition(Definition, PhaseNumber);
	return Phase ? &Phase->Intents : nullptr;
}

bool FGameXXKEnemyCatalog::Validate(FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	const TArray<FGameXXKEnemyDefinition>& Definitions = GetAllDefinitions();
	if (Definitions.Num() != 21)
	{
		if (OutError) *OutError = TEXT("Enemy catalog does not contain exactly 21 definitions.");
		return false;
	}

	TSet<FName> SeenDefinitionIds;
	int32 OrdinaryIntentCount = 0;
	int32 PhaseOneIntentCount = 0;
	int32 PhaseTwoIntentCount = 0;
	int32 PhaseThreeIntentCount = 0;
	for (const FGameXXKEnemyDefinition& Definition : Definitions)
	{
		const int32 ExpectedPhaseCount = Definition.Tier == EGameXXKEnemyTier::Normal ? 1 : 3;
		if (Definition.Id.IsNone()
			|| SeenDefinitionIds.Contains(Definition.Id)
			|| Definition.Chapter < 1
			|| Definition.Chapter > 3
			|| Definition.BaseHP < 1
			|| Definition.HPPerLevel <= 0.0f
			|| Definition.BaseAttack < 1
			|| Definition.AttackPerLevel <= 0.0f
			|| Definition.BaseDefense < 0
			|| Definition.DefensePerLevel <= 0.0f
			|| Definition.Speed < 1
			|| Definition.Phases.Num() != ExpectedPhaseCount
			|| !Definition.Intents.IsEmpty()
			|| Definition.PhaseId != EGameXXKEnemyPhaseId::None
			|| Definition.PhaseThresholdPercent != 0
			|| Definition.PhaseTwoDirectDamagePercent != 100
			|| Definition.PhaseTwoAttackPercent != 100
			|| Definition.PhaseTwoDefensePercent != 100
			|| !Definition.PhaseTwoAdditionalHitIntentIds.IsEmpty()
			|| Definition.RoundStartStatus != EGameXXKCardStatus::None
			|| Definition.RoundStartStatusStacks != 0
			|| Definition.PhaseTwoRoundStartStatusStacks != 0
			|| Definition.HealOnDamagingTargetStatus != EGameXXKCardStatus::None
			|| Definition.HealMissingHealthPercentOnDamagingTargetStatus != 0
			|| Definition.CodexId.IsNone()
			|| !Definition.PortraitSoftPath.IsValid()
			|| !Definition.BattleVisualSoftPath.IsValid())
		{
			if (OutError) *OutError = FString::Printf(TEXT("Enemy definition %s violates the approved phase schema."), *Definition.Id.ToString());
			return false;
		}
		if (Definition.Tier != EGameXXKEnemyTier::Normal
			&& Definition.PassiveId == EGameXXKEnemyPassiveId::None)
		{
			if (OutError) *OutError = FString::Printf(TEXT("Phase enemy %s is missing its passive."), *Definition.Id.ToString());
			return false;
		}
		SeenDefinitionIds.Add(Definition.Id);
		TSet<FName> SeenIntentIds;
		for (int32 PhaseIndex = 0; PhaseIndex < Definition.Phases.Num(); ++PhaseIndex)
		{
			const FGameXXKEnemyPhaseDefinition& PhaseDefinition = Definition.Phases[PhaseIndex];
			const int32 ExpectedIntentCount = PhaseIndex == 0
				? (Definition.Tier == EGameXXKEnemyTier::Normal ? 3 : Definition.Tier == EGameXXKEnemyTier::Elite ? 4 : 6)
				: (Definition.Tier == EGameXXKEnemyTier::Elite ? 4 : 5);
			if (PhaseDefinition.PhaseNumber != PhaseIndex + 1
				|| PhaseDefinition.Intents.Num() != ExpectedIntentCount
				|| PhaseDefinition.DisplayName.IsEmpty())
			{
				if (OutError) *OutError = FString::Printf(TEXT("Enemy %s phase %d has the wrong deck shape."), *Definition.Id.ToString(), PhaseIndex + 1);
				return false;
			}
			for (const FGameXXKEnemyIntentDefinition& IntentDefinition : PhaseDefinition.Intents)
			{
				if (IntentDefinition.Id.IsNone()
					|| SeenIntentIds.Contains(IntentDefinition.Id)
					|| IntentDefinition.DisplayName.IsEmpty()
					|| IntentDefinition.Effects.IsEmpty()
					|| IntentDefinition.ChargeRounds < 0
					|| IntentDefinition.CooldownRounds < 0
					|| IntentDefinition.PhaseTwoDirectDamagePercent != 100
					|| IntentDefinition.bPhaseTwoOnly)
				{
					if (OutError) *OutError = FString::Printf(TEXT("Enemy %s has an invalid or duplicate intent."), *Definition.Id.ToString());
					return false;
				}
				SeenIntentIds.Add(IntentDefinition.Id);
				bool bSawDirect = false;
				for (const FGameXXKEnemyIntentEffectDefinition& Effect : IntentDefinition.Effects)
				{
					if (!IsValidEffect(Effect)
						|| (Effect.bRequiresPreviousDirectHit && !bSawDirect)
						|| (Effect.bAssignsPersistentTarget
							&& (Effect.Type != EGameXXKEnemyIntentEffectType::ApplyStatus
								|| Effect.Target == EGameXXKEnemyIntentTargetRule::None
								|| Effect.Target == EGameXXKEnemyIntentTargetRule::Self
								|| Effect.Target == EGameXXKEnemyIntentTargetRule::AllLivingParty
								|| Effect.Target == EGameXXKEnemyIntentTargetRule::AllEnemyAllies)))
					{
						if (OutError) *OutError = FString::Printf(TEXT("Enemy %s intent %s has an invalid effect."), *Definition.Id.ToString(), *IntentDefinition.Id.ToString());
						return false;
					}
					bSawDirect |= Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
				}
			}
			if (Definition.Tier == EGameXXKEnemyTier::Normal)
			{
				OrdinaryIntentCount += PhaseDefinition.Intents.Num();
			}
			else if (PhaseIndex == 0)
			{
				PhaseOneIntentCount += PhaseDefinition.Intents.Num();
			}
			else if (PhaseIndex == 1)
			{
				PhaseTwoIntentCount += PhaseDefinition.Intents.Num();
			}
			else
			{
				PhaseThreeIntentCount += PhaseDefinition.Intents.Num();
			}
		}
	}

	if (OrdinaryIntentCount != 36
		|| PhaseOneIntentCount != 42
		|| PhaseTwoIntentCount != 39
		|| PhaseThreeIntentCount != 39)
	{
		if (OutError) *OutError = TEXT("Enemy catalog intent totals do not match the approved 36/42/39/39 matrix.");
		return false;
	}
	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		if (GetPool(Chapter, EGameXXKEnemyTier::Normal).Num() != 4
			|| GetPool(Chapter, EGameXXKEnemyTier::Elite).Num() != 2
			|| GetPool(Chapter, EGameXXKEnemyTier::Boss).Num() != 1)
		{
			if (OutError) *OutError = FString::Printf(TEXT("Enemy chapter %d has an invalid 4/2/1 roster."), Chapter);
			return false;
		}
	}
	return true;
}

FGameXXKEnemyComputedStats FGameXXKEnemyCatalog::ComputeStats(
	const FName DefinitionId,
	const int32 CombatLevel)
{
	FGameXXKEnemyComputedStats Stats;
	const FGameXXKEnemyDefinition* Definition = Find(DefinitionId);
	if (!Definition)
	{
		return Stats;
	}
	constexpr int32 MaxEnemyCombatLevel = 135;
	const int32 ClampedLevel = FMath::Clamp(CombatLevel, 1, MaxEnemyCombatLevel);
	const float GrowthSteps = static_cast<float>(ClampedLevel - 1);
	Stats.MaxHP = FMath::Max(1, Definition->BaseHP + RoundHalfAwayFromZero(Definition->HPPerLevel * GrowthSteps));
	Stats.Attack = FMath::Max(1, Definition->BaseAttack + RoundHalfAwayFromZero(Definition->AttackPerLevel * GrowthSteps));
	Stats.Defense = FMath::Max(0, Definition->BaseDefense + RoundHalfAwayFromZero(Definition->DefensePerLevel * GrowthSteps));
	Stats.Speed = FMath::Max(1, Definition->Speed);
	return Stats;
}
