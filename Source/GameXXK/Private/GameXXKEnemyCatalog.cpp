#include "GameXXKEnemyCatalog.h"
#include "GameXXKCharacterStatRules.h"

namespace
{
	FText EnemyText(const TCHAR* Value)
	{
		return FText::FromString(Value);
	}

	FGameXXKEnemyIntentEffectDefinition MakeEffect(
		const EGameXXKEnemyIntentEffectType Type,
		const EGameXXKEnemyIntentTargetRule Target,
		const int32 FlatMagnitude = 0,
		const int32 AttackPercent = 0,
		const int32 HitCount = 1,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 StatusStacks = 0)
	{
		FGameXXKEnemyIntentEffectDefinition Effect;
		Effect.Type = Type;
		Effect.Target = Target;
		Effect.FlatMagnitude = FlatMagnitude;
		Effect.AttackPercent = AttackPercent;
		Effect.HitCount = HitCount;
		Effect.Status = Status;
		Effect.StatusStacks = StatusStacks;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition Direct(
		const int32 AttackPercent,
		const EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom,
		const int32 HitCount = 1,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 StatusStacks = 0)
	{
		return MakeEffect(
			EGameXXKEnemyIntentEffectType::DirectDamage,
			Target,
			0,
			AttackPercent,
			HitCount,
			Status,
			StatusStacks);
	}

	FGameXXKEnemyIntentEffectDefinition DirectWithSourceStatusFlatBonus(
		const int32 AttackPercent,
		const EGameXXKCardStatus SourceStatus,
		const int32 FlatMagnitudePerStatusStack,
		const EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Direct(AttackPercent, Target);
		Effect.SourceStatusForFlatMagnitude = SourceStatus;
		Effect.FlatMagnitudePerSourceStatusStack = FlatMagnitudePerStatusStack;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition Armor(
		const int32 Amount,
		const EGameXXKEnemyIntentTargetRule Target = EGameXXKEnemyIntentTargetRule::Self)
	{
		return MakeEffect(EGameXXKEnemyIntentEffectType::AddArmor, Target, Amount);
	}

	FGameXXKEnemyIntentEffectDefinition Status(
		const EGameXXKCardStatus Value,
		const int32 Stacks,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		return MakeEffect(EGameXXKEnemyIntentEffectType::ApplyStatus, Target, 0, 0, 1, Value, Stacks);
	}

	FGameXXKEnemyIntentEffectDefinition PersistentTargetStatus(
		const EGameXXKCardStatus Value,
		const int32 Stacks,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Status(Value, Stacks, Target);
		Effect.bAssignsPersistentTarget = true;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition PersistentTargetDirect(
		const int32 AttackPercent,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = Direct(AttackPercent, Target);
		Effect.bPhaseTwoFallbackToLowestHealth = true;
		Effect.bClearsPersistentTargetAfterResolve = true;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition HealFromConsumedStatus(
		const EGameXXKCardStatus ConsumedStatus,
		const int32 MaximumConsumedStacks,
		const int32 PercentOfTargetMaxHealthPerStack)
	{
		FGameXXKEnemyIntentEffectDefinition Effect = MakeEffect(
			EGameXXKEnemyIntentEffectType::Heal,
			EGameXXKEnemyIntentTargetRule::Self);
		Effect.ConsumedStatus = ConsumedStatus;
		Effect.MaxConsumedStacks = MaximumConsumedStacks;
		Effect.MagnitudePerConsumedStack = PercentOfTargetMaxHealthPerStack;
		Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent = true;
		return Effect;
	}

	FGameXXKEnemyIntentEffectDefinition AttackModifier(
		const int32 Amount,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		return MakeEffect(EGameXXKEnemyIntentEffectType::ModifyAttack, Target, Amount);
	}

	FGameXXKEnemyIntentEffectDefinition SpeedModifier(
		const int32 Amount,
		const EGameXXKEnemyIntentTargetRule Target)
	{
		return MakeEffect(EGameXXKEnemyIntentEffectType::ModifySpeed, Target, Amount);
	}

	FGameXXKEnemyIntentDefinition MakeIntent(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		TArray<FGameXXKEnemyIntentEffectDefinition> Effects,
		const int32 ChargeRounds = 0,
		const bool bPhaseTwoOnly = false,
		const bool bRequiresSourceBelowHalf = false,
		const EGameXXKCardStatus RequiredTargetStatus = EGameXXKCardStatus::None,
		const int32 CooldownRounds = 0,
		const int32 PhaseTwoDirectDamagePercent = 100)
	{
		FGameXXKEnemyIntentDefinition Intent;
		Intent.Id = Id;
		Intent.DisplayName = EnemyText(DisplayName);
		Intent.Effects = MoveTemp(Effects);
		Intent.ChargeRounds = ChargeRounds;
		Intent.bPhaseTwoOnly = bPhaseTwoOnly;
		Intent.bRequiresSourceBelowHalf = bRequiresSourceBelowHalf;
		Intent.RequiredTargetStatus = RequiredTargetStatus;
		Intent.CooldownRounds = CooldownRounds;
		Intent.PhaseTwoDirectDamagePercent = PhaseTwoDirectDamagePercent;
		return Intent;
	}

	FGameXXKEnemyDefinition MakeEnemy(
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
		TArray<FGameXXKEnemyIntentDefinition> Intents,
		const EGameXXKEnemyPassiveId PassiveId = EGameXXKEnemyPassiveId::None,
		const EGameXXKEnemyPhaseId PhaseId = EGameXXKEnemyPhaseId::None,
		const EGameXXKCardStatus RoundStartStatus = EGameXXKCardStatus::None,
		const int32 RoundStartStatusStacks = 0,
		const int32 PhaseTwoRoundStartStatusStacks = 0,
		const int32 PhaseTwoDirectDamagePercent = 100,
		const int32 PhaseTwoAttackPercent = 100,
		const int32 PhaseTwoDefensePercent = 100,
		TArray<FName> PhaseTwoAdditionalHitIntentIds = {},
		const EGameXXKCardStatus HealOnDamagingTargetStatus = EGameXXKCardStatus::None,
		const int32 HealMissingHealthPercentOnDamagingTargetStatus = 0)
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
		Definition.Intents = MoveTemp(Intents);
		Definition.PassiveId = PassiveId;
		Definition.PhaseId = PhaseId;
		Definition.PhaseThresholdPercent = PhaseId == EGameXXKEnemyPhaseId::None ? 0 : 50;
		Definition.RoundStartStatus = RoundStartStatus;
		Definition.RoundStartStatusStacks = RoundStartStatusStacks;
		Definition.PhaseTwoRoundStartStatusStacks = PhaseTwoRoundStartStatusStacks;
		Definition.PhaseTwoDirectDamagePercent = PhaseTwoDirectDamagePercent;
		Definition.PhaseTwoAttackPercent = PhaseTwoAttackPercent;
		Definition.PhaseTwoDefensePercent = PhaseTwoDefensePercent;
		Definition.PhaseTwoAdditionalHitIntentIds = MoveTemp(PhaseTwoAdditionalHitIntentIds);
		Definition.HealOnDamagingTargetStatus = HealOnDamagingTargetStatus;
		Definition.HealMissingHealthPercentOnDamagingTargetStatus = HealMissingHealthPercentOnDamagingTargetStatus;
		Definition.CodexId = FName(*FString::Printf(TEXT("Codex.%s"), Id));
		const FString Leaf = FString(Id).Replace(TEXT("."), TEXT("_"));
		Definition.PortraitSoftPath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/UI/Codex/RouteEnemies/V1/T_%s.T_%s"), *Leaf, *Leaf));
		Definition.BattleVisualSoftPath = FSoftObjectPath(FString::Printf(
			TEXT("/Game/GameXXK/Characters/RouteEnemies/V1/FP_%s.FP_%s"), *Leaf, *Leaf));
		return Definition;
	}

	int32 RoundHalfAwayFromZero(const float Value)
	{
		return Value >= 0.0f
			? FMath::FloorToInt(Value + 0.5f)
			: FMath::CeilToInt(Value - 0.5f);
	}

	TArray<FGameXXKEnemyDefinition> BuildDefinitions()
	{
		TArray<FGameXXKEnemyDefinition> Definitions;
		Definitions.Reserve(21);

		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.Rooster"), TEXT("公鸡"), 1, EGameXXKEnemyTier::Normal, 46, 7.0f, 8, 1.1f, 1, 0.25f, 10,
			{
				MakeIntent(TEXT("Peck"), TEXT("啄击"), {Direct(100)}),
				MakeIntent(TEXT("DoublePeck"), TEXT("双重啄击"), {Direct(55, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 2)}),
				MakeIntent(TEXT("Crow"), TEXT("鸣啼"), {AttackModifier(2, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.Goat"), TEXT("山羊"), 1, EGameXXKEnemyTier::Normal, 58, 8.0f, 7, 1.0f, 3, 0.35f, 6,
			{
				MakeIntent(TEXT("Horn"), TEXT("顶角"), {Direct(90, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 1)}),
				MakeIntent(TEXT("Stomp"), TEXT("踏地"), {Armor(10)}),
				MakeIntent(TEXT("Charge"), TEXT("冲撞"), {Direct(170)}, 1)
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.Weasel"), TEXT("黄鼬"), 1, EGameXXKEnemyTier::Normal, 42, 6.0f, 9, 1.2f, 1, 0.20f, 11,
			{
				MakeIntent(TEXT("Harass"), TEXT("骚扰"), {Direct(80, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 1)}),
				MakeIntent(TEXT("StinkFog"), TEXT("臭雾"), {Status(EGameXXKCardStatus::Weak, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Escape"), TEXT("遁逃"), {SpeedModifier(1, EGameXXKEnemyIntentTargetRule::Self), Armor(5)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.Civet"), TEXT("狸猫"), 1, EGameXXKEnemyTier::Normal, 48, 7.0f, 8, 1.1f, 2, 0.25f, 9,
			{
				MakeIntent(TEXT("Claw"), TEXT("爪击"), {Direct(95)}),
				MakeIntent(TEXT("Feint"), TEXT("佯攻"), {Status(EGameXXKCardStatus::Mark, 2, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("Pickpocket"), TEXT("扒窃"), {Direct(70), MakeEffect(EGameXXKEnemyIntentEffectType::ConsumeSharedQi, EGameXXKEnemyIntentTargetRule::None, 1), Armor(8)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.IronfeatherRooster"), TEXT("铁羽斗鸡"), 1, EGameXXKEnemyTier::Elite, 118, 15.0f, 14, 1.7f, 5, 0.50f, 11,
			{
				MakeIntent(TEXT("RapidPeck"), TEXT("疾啄"), {Direct(50, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 3)}),
				MakeIntent(TEXT("IronGuard"), TEXT("铁羽守势"), {Armor(16)}),
				MakeIntent(TEXT("BattleCry"), TEXT("斗鸣"), {AttackModifier(3, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				MakeIntent(TEXT("BloodFight"), TEXT("血斗"), {Direct(170)}, 0, false, true)
			}, EGameXXKEnemyPassiveId::IronfeatherFirstHit));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.BluehornGoatKing"), TEXT("青角羊王"), 1, EGameXXKEnemyTier::Elite, 138, 17.0f, 13, 1.6f, 7, 0.65f, 7,
			{
				MakeIntent(TEXT("Pierce"), TEXT("贯角"), {Direct(120, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 2)}),
				MakeIntent(TEXT("HerdStomp"), TEXT("群踏"), {Direct(70, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("GuardHerd"), TEXT("护群"), {Armor(8, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				MakeIntent(TEXT("RageCharge"), TEXT("怒角冲撞"), {Direct(210)}, 1)
			}, EGameXXKEnemyPassiveId::BluehornArmorRetention));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch1.MoneyRat"), TEXT("金钱鼠"), 1, EGameXXKEnemyTier::Boss, 240, 24.0f, 17, 2.0f, 8, 0.75f, 10,
			{
				MakeIntent(TEXT("CoinVolley"), TEXT("撒币"), {Direct(70, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Hoard"), TEXT("敛财"), {Armor(18), Status(EGameXXKCardStatus::Wealth, 2, EGameXXKEnemyIntentTargetRule::Self)}),
				MakeIntent(TEXT("GreedyMark"), TEXT("贪印"), {Status(EGameXXKCardStatus::Mark, 2, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("Pickpocket"), TEXT("扒窃"), {Direct(90), MakeEffect(EGameXXKEnemyIntentEffectType::ConsumeSharedQi, EGameXXKEnemyIntentTargetRule::None, 1), Status(EGameXXKCardStatus::Wealth, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				MakeIntent(TEXT("BreakWealth"), TEXT("散财疗伤"), {HealFromConsumedStatus(EGameXXKCardStatus::Wealth, 3, 6)}),
				MakeIntent(TEXT("CoinCrash"), TEXT("钱潮冲击"), {DirectWithSourceStatusFlatBonus(100, EGameXXKCardStatus::Wealth, 15)})
			}, EGameXXKEnemyPassiveId::MoneyRatWealth, EGameXXKEnemyPhaseId::MoneyRatMadHoard,
				EGameXXKCardStatus::Wealth, 1, 2, 125));

		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.GrayWolf"), TEXT("灰狼"), 2, EGameXXKEnemyTier::Normal, 62, 9.0f, 11, 1.3f, 2, 0.30f, 12,
			{
				MakeIntent(TEXT("Bite"), TEXT("咬击"), {Direct(100, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 1)}),
				MakeIntent(TEXT("Pursuit"), TEXT("追猎"), {Direct(130)}, 0, false, false, EGameXXKCardStatus::Mark),
				MakeIntent(TEXT("CallPack"), TEXT("呼群"), {AttackModifier(2, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.Boar"), TEXT("野猪"), 2, EGameXXKEnemyTier::Normal, 76, 10.0f, 10, 1.2f, 5, 0.45f, 7,
			{
				MakeIntent(TEXT("Tusk"), TEXT("獠牙突刺"), {Direct(110)}),
				MakeIntent(TEXT("Bristle"), TEXT("鬃毛守势"), {Armor(12)}),
				MakeIntent(TEXT("ArmorBreakCharge"), TEXT("破甲冲锋"), {Direct(145, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Weak, 1)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.Macaque"), TEXT("猕猴"), 2, EGameXXKEnemyTier::Normal, 58, 8.0f, 10, 1.3f, 2, 0.25f, 13,
			{
				MakeIntent(TEXT("ThrowStone"), TEXT("掷石"), {Direct(90, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("Snatch"), TEXT("夺势"), {MakeEffect(EGameXXKEnemyIntentEffectType::RemovePositiveStatus, EGameXXKEnemyIntentTargetRule::LowestHealthParty, 1)}),
				MakeIntent(TEXT("Hasten"), TEXT("催行"), {SpeedModifier(3, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.Porcupine"), TEXT("豪猪"), 2, EGameXXKEnemyTier::Normal, 70, 9.0f, 9, 1.1f, 5, 0.50f, 8,
			{
				MakeIntent(TEXT("Quill"), TEXT("刺毛"), {Direct(85, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 1)}),
				MakeIntent(TEXT("BristleGuard"), TEXT("蓄刺"), {Armor(8), Status(EGameXXKCardStatus::Counter, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				MakeIntent(TEXT("QuillVolley"), TEXT("飞刺"), {Direct(55, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Bleed, 1)})
			}, EGameXXKEnemyPassiveId::PorcupineCounter));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.GraymaneWolfKing"), TEXT("苍鬃狼王"), 2, EGameXXKEnemyTier::Elite, 158, 18.0f, 18, 2.0f, 6, 0.55f, 13,
			{
				MakeIntent(TEXT("HuntMark"), TEXT("猎印"), {Direct(90, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Mark, 2)}),
				MakeIntent(TEXT("ContinuousHunt"), TEXT("连环狩猎"), {Direct(70, EGameXXKEnemyIntentTargetRule::MarkedParty, 3)}, 0, false, false, EGameXXKCardStatus::Mark),
				MakeIntent(TEXT("PackOrder"), TEXT("群猎号令"), {AttackModifier(3, EGameXXKEnemyIntentTargetRule::AllEnemyAllies), Status(EGameXXKCardStatus::Mark, 1, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("Sidestep"), TEXT("侧跃"), {SpeedModifier(1, EGameXXKEnemyIntentTargetRule::Self), Armor(6)})
			}, EGameXXKEnemyPassiveId::GraymaneMarkedHunt));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.RedtuskBoarKing"), TEXT("赤獠猪王"), 2, EGameXXKEnemyTier::Elite, 188, 20.0f, 17, 1.9f, 9, 0.75f, 8,
			{
				MakeIntent(TEXT("HeavyArmor"), TEXT("厚甲"), {Armor(18)}),
				MakeIntent(TEXT("Earthquake"), TEXT("震地"), {Direct(80, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 1)}),
				MakeIntent(TEXT("RageStrike"), TEXT("怒獠"), {Direct(140)}),
				MakeIntent(TEXT("RedCharge"), TEXT("赤牙冲锋"), {Direct(190)}, 1)
			}, EGameXXKEnemyPassiveId::RedtuskRage));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch2.BlackBear"), TEXT("黑熊"), 2, EGameXXKEnemyTier::Boss, 320, 30.0f, 23, 2.4f, 11, 0.90f, 7,
			{
				MakeIntent(TEXT("Sweep"), TEXT("横扫"), {Direct(80, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Pounce"), TEXT("扑杀"), {Direct(150)}),
				MakeIntent(TEXT("WeakRoar"), TEXT("震慑咆哮"), {Status(EGameXXKCardStatus::Weak, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Rend"), TEXT("撕裂"), {Direct(110, EGameXXKEnemyIntentTargetRule::LowestHealthParty, 1, EGameXXKCardStatus::Bleed, 2)}),
				MakeIntent(TEXT("CounterPosture"), TEXT("反击架势"), {Status(EGameXXKCardStatus::Counter, 1, EGameXXKEnemyIntentTargetRule::Self)}),
				MakeIntent(TEXT("Quake"), TEXT("裂地"), {Direct(100, EGameXXKEnemyIntentTargetRule::AllLivingParty, 1, EGameXXKCardStatus::Weak, 1)})
			}, EGameXXKEnemyPassiveId::BlackBearThickHide, EGameXXKEnemyPhaseId::BlackBearEnraged,
				EGameXXKCardStatus::None, 0, 0, 100, 130, 75, {TEXT("Pounce"), TEXT("Rend")}));

		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.VenomSnake"), TEXT("毒蛇"), 3, EGameXXKEnemyTier::Normal, 72, 9.0f, 12, 1.35f, 2, 0.25f, 14,
			{
				MakeIntent(TEXT("VenomBite"), TEXT("毒牙"), {Direct(70, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Poison, 2)}),
				MakeIntent(TEXT("Coil"), TEXT("盘缠"), {SpeedModifier(1, EGameXXKEnemyIntentTargetRule::Self), Armor(5)}),
				MakeIntent(TEXT("ToxicPursuit"), TEXT("毒袭"), {Direct(90)}, 0, false, false, EGameXXKCardStatus::Poison)
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.Wildcat"), TEXT("山猫"), 3, EGameXXKEnemyTier::Normal, 70, 9.0f, 14, 1.50f, 3, 0.30f, 14,
			{
				MakeIntent(TEXT("Rake"), TEXT("抓挠"), {Direct(95, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 1)}),
				MakeIntent(TEXT("Stalk"), TEXT("潜伏"), {Status(EGameXXKCardStatus::Mark, 2, EGameXXKEnemyIntentTargetRule::RandomLivingParty), SpeedModifier(1, EGameXXKEnemyIntentTargetRule::Self)}),
				MakeIntent(TEXT("BloodPursuit"), TEXT("血猎"), {Direct(140)}, 0, false, false, EGameXXKCardStatus::Bleed)
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.Vulture"), TEXT("秃鹫"), 3, EGameXXKEnemyTier::Normal, 74, 9.0f, 13, 1.45f, 3, 0.30f, 15,
			{
				MakeIntent(TEXT("Gaze"), TEXT("凝视"), {Status(EGameXXKCardStatus::Mark, 1, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("Dive"), TEXT("俯冲"), {Direct(120)}),
				MakeIntent(TEXT("WingCut"), TEXT("翼斩"), {Direct(60, EGameXXKEnemyIntentTargetRule::AllLivingParty)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.GiantToad"), TEXT("巨蟾"), 3, EGameXXKEnemyTier::Normal, 94, 12.0f, 11, 1.25f, 7, 0.60f, 6,
			{
				MakeIntent(TEXT("Tongue"), TEXT("卷舌"), {Direct(100)}),
				MakeIntent(TEXT("PoisonFog"), TEXT("毒雾"), {Status(EGameXXKCardStatus::Poison, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Inflate"), TEXT("鼓腹"), {Armor(14), Status(EGameXXKCardStatus::Medicine, 1, EGameXXKEnemyIntentTargetRule::Self)})
			}));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.WhiteApe"), TEXT("白猿"), 3, EGameXXKEnemyTier::Elite, 198, 21.0f, 21, 2.20f, 8, 0.65f, 12,
			{
				MakeIntent(TEXT("ThrowRock"), TEXT("掷岩"), {Direct(120)}),
				MakeIntent(TEXT("Disturb"), TEXT("扰乱"), {MakeEffect(EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy, EGameXXKEnemyIntentTargetRule::None, 1)}),
				MakeIntent(TEXT("BoulderCharge"), TEXT("巨岩冲撞"), {Direct(220)}, 1),
				MakeIntent(TEXT("WideSweep"), TEXT("广横扫"), {Direct(85, EGameXXKEnemyIntentTargetRule::AllLivingParty)})
			}, EGameXXKEnemyPassiveId::WhiteApeStatusGuard));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.SpiralHornDeer"), TEXT("盘角鹿"), 3, EGameXXKEnemyTier::Elite, 210, 22.0f, 20, 2.10f, 9, 0.75f, 11,
			{
				MakeIntent(TEXT("Horn"), TEXT("盘角突击"), {Direct(125)}),
				MakeIntent(TEXT("TerrainBless"), TEXT("地势祝福"), {AttackModifier(20, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				MakeIntent(TEXT("HerdArmor"), TEXT("群护"), {Armor(10, EGameXXKEnemyIntentTargetRule::AllEnemyAllies)}),
				MakeIntent(TEXT("SpringHeal"), TEXT("回春"), {MakeEffect(EGameXXKEnemyIntentEffectType::Heal, EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly, 12)}, 0, false, false, EGameXXKCardStatus::None, 2)
			}, EGameXXKEnemyPassiveId::DeerHealCooldown));
		Definitions.Add(MakeEnemy(TEXT("Enemy.Ch3.Tiger"), TEXT("老虎"), 3, EGameXXKEnemyTier::Boss, 380, 34.0f, 28, 2.70f, 12, 1.00f, 14,
			{
				MakeIntent(TEXT("MarkPrey"), TEXT("标记猎物"), {PersistentTargetStatus(EGameXXKCardStatus::Prey, 1, EGameXXKEnemyIntentTargetRule::RandomLivingParty)}),
				MakeIntent(TEXT("TigerPounce"), TEXT("虎扑"), {PersistentTargetDirect(160, EGameXXKEnemyIntentTargetRule::PreyTarget)}, 0, false, false, EGameXXKCardStatus::Prey, 0, 150),
				MakeIntent(TEXT("TailSweep"), TEXT("尾扫"), {Direct(95, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("BleedingRend"), TEXT("裂伤"), {Direct(120, EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom, 1, EGameXXKCardStatus::Bleed, 2)}),
				MakeIntent(TEXT("DreadRoar"), TEXT("威吓咆哮"), {Status(EGameXXKCardStatus::Weak, 1, EGameXXKEnemyIntentTargetRule::AllLivingParty)}),
				MakeIntent(TEXT("Ambush"), TEXT("伏杀"), {Direct(240)}, 1)
			}, EGameXXKEnemyPassiveId::TigerPredator, EGameXXKEnemyPhaseId::TigerDread,
				EGameXXKCardStatus::None, 0, 0, 100, 100, 100, {TEXT("TigerPounce")},
				EGameXXKCardStatus::Bleed, 8));

		return Definitions;
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

TArray<FName> FGameXXKEnemyCatalog::GetPool(const int32 Chapter, const EGameXXKEnemyTier Tier)
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

bool FGameXXKEnemyCatalog::Validate(FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	const TArray<FGameXXKEnemyDefinition>& Definitions = GetAllDefinitions();
	if (Definitions.Num() != 21)
	{
		if (OutError) { *OutError = TEXT("Enemy catalog does not contain exactly 21 definitions."); }
		return false;
	}

	TSet<FName> SeenIds;
	for (const FGameXXKEnemyDefinition& Definition : Definitions)
	{
		if (Definition.Id.IsNone() || SeenIds.Contains(Definition.Id)
			|| Definition.Chapter < 1 || Definition.Chapter > 3
			|| Definition.BaseHP < 1 || Definition.HPPerLevel <= 0.0f
			|| Definition.BaseAttack < 1 || Definition.AttackPerLevel <= 0.0f
			|| Definition.BaseDefense < 0 || Definition.DefensePerLevel <= 0.0f
			|| Definition.Speed < 1 || Definition.CodexId.IsNone()
			|| !Definition.PortraitSoftPath.IsValid() || !Definition.BattleVisualSoftPath.IsValid())
		{
			if (OutError) { *OutError = FString::Printf(TEXT("Enemy definition %s is incomplete."), *Definition.Id.ToString()); }
			return false;
		}
		SeenIds.Add(Definition.Id);

		const int32 ExpectedIntentCount = Definition.Tier == EGameXXKEnemyTier::Normal ? 3
			: Definition.Tier == EGameXXKEnemyTier::Elite ? 4 : 6;
		if (Definition.Intents.Num() != ExpectedIntentCount
			|| ((Definition.Tier == EGameXXKEnemyTier::Elite || Definition.Tier == EGameXXKEnemyTier::Boss)
				&& Definition.PassiveId == EGameXXKEnemyPassiveId::None)
			|| (Definition.Tier == EGameXXKEnemyTier::Boss
				&& (Definition.PhaseId == EGameXXKEnemyPhaseId::None || Definition.PhaseThresholdPercent != 50)))
		{
			if (OutError) { *OutError = FString::Printf(TEXT("Enemy definition %s has invalid tier mechanics."), *Definition.Id.ToString()); }
			return false;
		}
		const bool bHasRoundStartStatus = Definition.RoundStartStatus != EGameXXKCardStatus::None;
		const bool bRoundStartDefinitionValid = bHasRoundStartStatus
			? Definition.RoundStartStatus != EGameXXKCardStatus::Invalid
				&& Definition.RoundStartStatusStacks > 0
				&& Definition.PhaseTwoRoundStartStatusStacks >= 0
				&& (Definition.PhaseId != EGameXXKEnemyPhaseId::None || Definition.PhaseTwoRoundStartStatusStacks == 0)
			: Definition.RoundStartStatusStacks == 0 && Definition.PhaseTwoRoundStartStatusStacks == 0;
		const bool bPhaseDefinitionValid = Definition.PhaseTwoDirectDamagePercent > 0
			&& Definition.PhaseTwoAttackPercent > 0
			&& Definition.PhaseTwoDefensePercent > 0
			&& (Definition.PhaseId != EGameXXKEnemyPhaseId::None
				|| (Definition.PhaseTwoDirectDamagePercent == 100
					&& Definition.PhaseTwoAttackPercent == 100
					&& Definition.PhaseTwoDefensePercent == 100));
		const bool bHasDamagingStatusHeal = Definition.HealOnDamagingTargetStatus != EGameXXKCardStatus::None;
		const bool bDamagingStatusHealDefinitionValid = bHasDamagingStatusHeal
			? Definition.HealOnDamagingTargetStatus != EGameXXKCardStatus::Invalid
				&& Definition.HealMissingHealthPercentOnDamagingTargetStatus > 0
			: Definition.HealMissingHealthPercentOnDamagingTargetStatus == 0;
		TSet<FName> SeenPhaseAdditionalHitIntentIds;
		bool bPhaseAdditionalHitDefinitionValid = Definition.PhaseId != EGameXXKEnemyPhaseId::None
			|| Definition.PhaseTwoAdditionalHitIntentIds.IsEmpty();
		for (const FName IntentId : Definition.PhaseTwoAdditionalHitIntentIds)
		{
			const FGameXXKEnemyIntentDefinition* Intent = Definition.Intents.FindByPredicate([IntentId](const FGameXXKEnemyIntentDefinition& Candidate)
			{
				return Candidate.Id == IntentId;
			});
			if (IntentId.IsNone() || SeenPhaseAdditionalHitIntentIds.Contains(IntentId) || !Intent
				|| !Intent->Effects.ContainsByPredicate([](const FGameXXKEnemyIntentEffectDefinition& Effect)
				{
					return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
				}))
			{
				bPhaseAdditionalHitDefinitionValid = false;
				break;
			}
			SeenPhaseAdditionalHitIntentIds.Add(IntentId);
		}
		if (!bRoundStartDefinitionValid || !bPhaseDefinitionValid || !bPhaseAdditionalHitDefinitionValid || !bDamagingStatusHealDefinitionValid)
		{
			if (OutError) { *OutError = FString::Printf(TEXT("Enemy definition %s has an invalid phase or round-start rule."), *Definition.Id.ToString()); }
			return false;
		}

		TSet<FName> SeenIntentIds;
		for (const FGameXXKEnemyIntentDefinition& Intent : Definition.Intents)
		{
			const bool bHasDirectDamage = Intent.Effects.ContainsByPredicate([](const FGameXXKEnemyIntentEffectDefinition& Effect)
			{
				return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
			});
			if (Intent.Id.IsNone() || SeenIntentIds.Contains(Intent.Id) || Intent.Effects.IsEmpty() || Intent.Weight < 1
				|| Intent.ChargeRounds < 0 || Intent.CooldownRounds < 0 || Intent.PhaseTwoDirectDamagePercent <= 0
				|| (!bHasDirectDamage && Intent.PhaseTwoDirectDamagePercent != 100)
				|| (Definition.PhaseId == EGameXXKEnemyPhaseId::None && Intent.PhaseTwoDirectDamagePercent != 100))
			{
				if (OutError) { *OutError = FString::Printf(TEXT("Enemy definition %s has an invalid intent."), *Definition.Id.ToString()); }
				return false;
			}
			SeenIntentIds.Add(Intent.Id);
			for (const FGameXXKEnemyIntentEffectDefinition& Effect : Intent.Effects)
			{
				const bool bUsesConsumedStacks = Effect.ConsumedStatus != EGameXXKCardStatus::None;
				const bool bUsesSourceStatusFlatMagnitude = Effect.SourceStatusForFlatMagnitude != EGameXXKCardStatus::None;
				const bool bPersistentTargetAssignmentValid = !Effect.bAssignsPersistentTarget
					|| (Effect.Type == EGameXXKEnemyIntentEffectType::ApplyStatus
						&& Effect.Status != EGameXXKCardStatus::None
						&& Effect.Status != EGameXXKCardStatus::Invalid
						&& Effect.StatusStacks > 0
						&& Effect.Target != EGameXXKEnemyIntentTargetRule::None
						&& Effect.Target != EGameXXKEnemyIntentTargetRule::Self
						&& Effect.Target != EGameXXKEnemyIntentTargetRule::AllLivingParty
						&& Effect.Target != EGameXXKEnemyIntentTargetRule::AllEnemyAllies);
				const bool bPersistentTargetDamageRuleValid = (!Effect.bPhaseTwoFallbackToLowestHealth && !Effect.bClearsPersistentTargetAfterResolve)
					|| (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
						&& Effect.Target == EGameXXKEnemyIntentTargetRule::PreyTarget);
				if ((bUsesConsumedStacks
					&& (Effect.MaxConsumedStacks <= 0 || Effect.MagnitudePerConsumedStack <= 0))
					|| (!bUsesConsumedStacks
						&& (Effect.MaxConsumedStacks != 0 || Effect.MagnitudePerConsumedStack != 0
							|| Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent))
					|| (bUsesSourceStatusFlatMagnitude
						&& (Effect.SourceStatusForFlatMagnitude == EGameXXKCardStatus::Invalid
							|| Effect.FlatMagnitudePerSourceStatusStack <= 0))
					|| (!bUsesSourceStatusFlatMagnitude && Effect.FlatMagnitudePerSourceStatusStack != 0)
					|| !bPersistentTargetAssignmentValid
					|| !bPersistentTargetDamageRuleValid)
				{
					if (OutError) { *OutError = FString::Printf(TEXT("Enemy definition %s has an invalid consumed-status effect."), *Definition.Id.ToString()); }
					return false;
				}
			}
		}
	}

	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		if (GetPool(Chapter, EGameXXKEnemyTier::Normal).Num() != 4
			|| GetPool(Chapter, EGameXXKEnemyTier::Elite).Num() != 2
			|| GetPool(Chapter, EGameXXKEnemyTier::Boss).Num() != 1)
		{
			if (OutError) { *OutError = FString::Printf(TEXT("Enemy catalog chapter %d has an invalid pool shape."), Chapter); }
			return false;
		}
	}
	return true;
}

FGameXXKEnemyComputedStats FGameXXKEnemyCatalog::ComputeStats(const FName DefinitionId, const int32 CombatLevel)
{
	FGameXXKEnemyComputedStats Stats;
	const FGameXXKEnemyDefinition* Definition = Find(DefinitionId);
	if (!Definition)
	{
		return Stats;
	}
	const int32 ClampedLevel = FMath::Clamp(CombatLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
	const float GrowthSteps = static_cast<float>(ClampedLevel - 1);
	Stats.MaxHP = FMath::Max(1, Definition->BaseHP + RoundHalfAwayFromZero(Definition->HPPerLevel * GrowthSteps));
	Stats.Attack = FMath::Max(1, Definition->BaseAttack + RoundHalfAwayFromZero(Definition->AttackPerLevel * GrowthSteps));
	Stats.Defense = FMath::Max(0, Definition->BaseDefense + RoundHalfAwayFromZero(Definition->DefensePerLevel * GrowthSteps));
	Stats.Speed = FMath::Max(1, Definition->Speed);
	return Stats;
}
