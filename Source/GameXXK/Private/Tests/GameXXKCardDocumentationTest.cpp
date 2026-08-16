#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardText.h"

#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardDocumentationTest,
	"GameXXK.Data.CardDocumentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace GameXXKCardDocumentationTest
{
	constexpr const TCHAR* MarkdownRelativePath = TEXT("docs/design/2026-08-11-full-card-catalog.md");
	constexpr const TCHAR* TextRelativePath = TEXT("docs/design/2026-08-11-full-card-catalog.txt");

	template <typename TEnum>
	FString EnumToken(const TEnum Value)
	{
		const UEnum* Enum = StaticEnum<TEnum>();
		return Enum
			? Enum->GetNameStringByValue(static_cast<int64>(Value))
			: FString::Printf(TEXT("%d"), static_cast<int32>(Value));
	}

	FString DescribeRole(const EGameXXKCharacterRole Role)
	{
		switch (Role)
		{
		case EGameXXKCharacterRole::Hero: return TEXT("主角");
		case EGameXXKCharacterRole::Blade: return TEXT("刀客");
		case EGameXXKCharacterRole::Guard: return TEXT("守卫");
		case EGameXXKCharacterRole::Healer: return TEXT("药师");
		case EGameXXKCharacterRole::Hunter: return TEXT("弓手");
		case EGameXXKCharacterRole::Sorcerer: return TEXT("法师");
		case EGameXXKCharacterRole::FormationMaster: return TEXT("阵师");
		case EGameXXKCharacterRole::QuestNpc: return TEXT("任务 NPC");
		case EGameXXKCharacterRole::Route: return TEXT("路线");
		case EGameXXKCharacterRole::Invalid:
		default: return TEXT("泛用");
		}
	}

	FString DescribeRarity(const EGameXXKCardRarity Rarity)
	{
		switch (Rarity)
		{
		case EGameXXKCardRarity::Permanent: return TEXT("固定");
		case EGameXXKCardRarity::Common: return TEXT("普通");
		case EGameXXKCardRarity::Rare: return TEXT("稀有");
		case EGameXXKCardRarity::Boss: return TEXT("首领");
		case EGameXXKCardRarity::Invalid:
		default: return TEXT("无效");
		}
	}

	FString DescribeStatus(const EGameXXKCardStatus Status)
	{
		switch (Status)
		{
		case EGameXXKCardStatus::Momentum: return TEXT("气势");
		case EGameXXKCardStatus::Bleed: return TEXT("流血");
		case EGameXXKCardStatus::Poison: return TEXT("中毒");
		case EGameXXKCardStatus::Burn: return TEXT("灼烧");
		case EGameXXKCardStatus::DamageOverTime: return TEXT("蚀伤");
		case EGameXXKCardStatus::Agility: return TEXT("灵动");
		case EGameXXKCardStatus::Medicine: return TEXT("药效");
		case EGameXXKCardStatus::Charge: return TEXT("蓄力");
		case EGameXXKCardStatus::Vulnerability: return TEXT("破绽");
		case EGameXXKCardStatus::Mark: return TEXT("标记");
		case EGameXXKCardStatus::CannotReceiveVulnerability: return TEXT("破绽免疫");
		case EGameXXKCardStatus::Counter: return TEXT("反击");
		case EGameXXKCardStatus::Block: return TEXT("格挡");
		case EGameXXKCardStatus::NextTerrainCardFree: return TEXT("地形免耗");
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction: return TEXT("地形减耗");
		case EGameXXKCardStatus::Weak: return TEXT("虚弱");
		case EGameXXKCardStatus::Rage: return TEXT("怒气");
		case EGameXXKCardStatus::None:
		case EGameXXKCardStatus::Invalid:
		default: return EnumToken(Status);
		}
	}

	FString DescribeNpc(const FName OwnerId)
	{
		if (OwnerId == TEXT("Npc.TusiChief")) return TEXT("土司首领");
		if (OwnerId == TEXT("Npc.SongJinBao")) return TEXT("宋金宝");
		if (OwnerId == TEXT("Npc.YueBai")) return TEXT("月白");
		if (OwnerId == TEXT("Npc.ZhouGuangZu")) return TEXT("周光祖");
		if (OwnerId == TEXT("Npc.JinGui")) return TEXT("金贵");
		if (OwnerId == TEXT("Npc.QiongMeiEr")) return TEXT("琼么儿");
		return OwnerId.ToString();
	}

	FString DescribeGroup(const FGameXXKCardDefinition& Definition)
	{
		switch (Definition.Owner)
		{
		case EGameXXKCardOwner::Hero:
			return Definition.LinkedRole == EGameXXKCharacterRole::Invalid
				? TEXT("主角·泛用")
				: FString::Printf(TEXT("主角·%s联动"), *DescribeRole(Definition.LinkedRole));
		case EGameXXKCardOwner::Profession:
			return FString::Printf(TEXT("永久伙伴·%s"), *DescribeRole(Definition.Role));
		case EGameXXKCardOwner::QuestNpc:
			return FString::Printf(TEXT("任务 NPC·%s"), *DescribeNpc(Definition.OwnerId));
		case EGameXXKCardOwner::Route:
			return FString::Printf(TEXT("路线卡·%s"), *DescribeRarity(Definition.Rarity));
		case EGameXXKCardOwner::Invalid:
		default:
			return TEXT("无效归属");
		}
	}

	FString DescribeAcquisition(const FGameXXKCardDefinition& Definition)
	{
		switch (Definition.Owner)
		{
		case EGameXXKCardOwner::Hero:
			return Definition.HeroUnlockLevel <= 1
				? TEXT("初始解锁")
				: FString::Printf(TEXT("主角 Lv.%d 解锁"), Definition.HeroUnlockLevel);
		case EGameXXKCardOwner::Profession:
			return Definition.bCoreProfessionCard ? TEXT("固定核心牌") : TEXT("职业随机池");
		case EGameXXKCardOwner::QuestNpc:
			return TEXT("NPC 固定 4 选 3 卡池");
		case EGameXXKCardOwner::Route:
			return FString::Printf(TEXT("%s路线临时卡"), *DescribeRarity(Definition.Rarity));
		case EGameXXKCardOwner::Invalid:
		default:
			return Definition.AcquisitionKey.ToString();
		}
	}

	FString NormalizeLineBreaks(FString Value, const TCHAR* Replacement)
	{
		Value.ReplaceInline(TEXT("\r\n"), TEXT("\n"));
		Value.ReplaceInline(TEXT("\r"), TEXT("\n"));
		Value.ReplaceInline(TEXT("\n"), Replacement);
		Value.TrimStartAndEndInline();
		return Value;
	}

	FString EscapeMarkdownCell(FString Value)
	{
		Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
		return NormalizeLineBreaks(MoveTemp(Value), TEXT("<br>"));
	}

	FString DescribeConditionContract(const FGameXXKCardEffectCondition& Condition)
	{
		TArray<FString> Parts;
		if (Condition.Type != EGameXXKCardEffectConditionType::None)
		{
			Parts.Add(FString::Printf(TEXT("Type=%s"), *EnumToken(Condition.Type)));
		}
		if (Condition.Status != EGameXXKCardStatus::None)
		{
			Parts.Add(FString::Printf(TEXT("Status=%s"), *EnumToken(Condition.Status)));
		}
		if (Condition.MinimumStatusStacks != 0) Parts.Add(FString::Printf(TEXT("MinStacks=%d"), Condition.MinimumStatusStacks));
		if (Condition.MinimumArmor != 0) Parts.Add(FString::Printf(TEXT("MinArmor=%d"), Condition.MinimumArmor));
		if (!FMath::IsNearlyZero(Condition.HealthPercentThreshold)) Parts.Add(FString::Printf(TEXT("Health=%.0f%%"), Condition.HealthPercentThreshold));
		if (Condition.Terrain != EGameXXKCardTerrain::Invalid) Parts.Add(FString::Printf(TEXT("Terrain=%s"), *EnumToken(Condition.Terrain)));
		if (Condition.AlternateTerrain != EGameXXKCardTerrain::Invalid) Parts.Add(FString::Printf(TEXT("AltTerrain=%s"), *EnumToken(Condition.AlternateTerrain)));
		if (Condition.bConsumeStatus)
		{
			Parts.Add(Condition.MaxConsumedStatusStacks == 0
				? TEXT("ConsumeStatus=All(Max=0)")
				: FString::Printf(TEXT("ConsumeStatus<=%d"), Condition.MaxConsumedStatusStacks));
		}
		if (Condition.bScaleMagnitudeByConsumedStacks) Parts.Add(TEXT("ScaleByConsumedStatus=true"));
		if (Condition.bConsumeOwnerArmor)
		{
			Parts.Add(Condition.MaxConsumedArmor == 0
				? TEXT("ConsumeArmor=All(Max=0)")
				: FString::Printf(TEXT("ConsumeArmor<=%d"), Condition.MaxConsumedArmor));
		}
		if (Condition.bNegate) Parts.Add(TEXT("Negate=true"));
		return Parts.IsEmpty() ? FString() : FString::Printf(TEXT("Condition{%s}"), *FString::Join(Parts, TEXT(",")));
	}

	FString DescribeModifierContract(const FGameXXKCardBattleModifier& Modifier)
	{
		TArray<FString> Parts = {
			FString::Printf(TEXT("Trigger=%s"), *EnumToken(Modifier.Trigger)),
			FString::Printf(TEXT("Effect=%s"), *EnumToken(Modifier.EffectType)),
			FString::Printf(TEXT("Target=%s"), *EnumToken(Modifier.Target)),
			FString::Printf(TEXT("RecipientScope=%s"), *EnumToken(Modifier.RecipientScope)),
			FString::Printf(TEXT("RecipientTarget=%s"), *EnumToken(Modifier.RecipientTarget)),
			FString::Printf(TEXT("Expiry=%s"), *EnumToken(Modifier.Expiry)),
			FString::Printf(TEXT("Magnitude=%d"), Modifier.Magnitude),
			FString::Printf(TEXT("Triggers=%d"), Modifier.RemainingTriggers)};
		if (Modifier.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid) Parts.Add(FString::Printf(TEXT("RequiredRole=%s"), *EnumToken(Modifier.RequiredTriggeredRole)));
		if (!Modifier.RequiredTriggeredOwnerId.IsNone()) Parts.Add(FString::Printf(TEXT("RequiredOwner=%s"), *Modifier.RequiredTriggeredOwnerId.ToString()));
		if (Modifier.TriggeredAttackTargetScope != EGameXXKCardTriggeredAttackTargetScope::Invalid) Parts.Add(FString::Printf(TEXT("AttackScope=%s"), *EnumToken(Modifier.TriggeredAttackTargetScope)));
		if (Modifier.Status != EGameXXKCardStatus::None) Parts.Add(FString::Printf(TEXT("Status=%s"), *EnumToken(Modifier.Status)));
		if (Modifier.MinimumResult != 0) Parts.Add(FString::Printf(TEXT("MinResult=%d"), Modifier.MinimumResult));
		if (Modifier.bPersistent) Parts.Add(TEXT("Persistent=true"));
		if (Modifier.bActivePlayOnly) Parts.Add(TEXT("ActivePlayOnly=true"));
		if (Modifier.bExcludeSourceUnit) Parts.Add(TEXT("ExcludeSource=true"));
		if (Modifier.bPreserveTriggeredStatus) Parts.Add(TEXT("PreserveStatus=true"));
		const FString Condition = DescribeConditionContract(Modifier.Condition);
		if (!Condition.IsEmpty()) Parts.Add(Condition);
		return FString::Printf(TEXT("Modifier{%s}"), *FString::Join(Parts, TEXT(",")));
	}

	FString DescribeEffectContract(const FGameXXKCardEffect& Effect)
	{
		TArray<FString> Parts = {
			FString::Printf(TEXT("Type=%s"), *EnumToken(Effect.Type)),
			FString::Printf(TEXT("Target=%s"), *EnumToken(Effect.Target)),
			FString::Printf(TEXT("Magnitude=%d"), Effect.Magnitude)};
		if (Effect.Source != EGameXXKCardEffectSource::CardOwner) Parts.Add(FString::Printf(TEXT("Source=%s"), *EnumToken(Effect.Source)));
		if (Effect.SecondaryMagnitude != 0) Parts.Add(FString::Printf(TEXT("Secondary=%d"), Effect.SecondaryMagnitude));
		if (Effect.HitCount != 1) Parts.Add(FString::Printf(TEXT("Hits=%d"), Effect.HitCount));
		if (Effect.Status != EGameXXKCardStatus::None) Parts.Add(FString::Printf(TEXT("Status=%s"), *EnumToken(Effect.Status)));
		if (Effect.TerrainOverride != EGameXXKCardTerrain::Invalid) Parts.Add(FString::Printf(TEXT("TerrainOverride=%s"), *EnumToken(Effect.TerrainOverride)));
		const FString Condition = DescribeConditionContract(Effect.Condition);
		if (!Condition.IsEmpty()) Parts.Add(Condition);
		if (!Effect.ResultGroupId.IsNone()) Parts.Add(FString::Printf(TEXT("ResultGroup=%s"), *Effect.ResultGroupId.ToString()));
		if (!Effect.ResultRef.IsNone()) Parts.Add(FString::Printf(TEXT("ResultRef=%s"), *Effect.ResultRef.ToString()));
		if (!Effect.ConsumptionGroupId.IsNone()) Parts.Add(FString::Printf(TEXT("ConsumptionGroup=%s"), *Effect.ConsumptionGroupId.ToString()));
		if (!Effect.ConsumedStackResultRef.IsNone()) Parts.Add(FString::Printf(TEXT("ConsumedRef=%s"), *Effect.ConsumedStackResultRef.ToString()));
		if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier) Parts.Add(DescribeModifierContract(Effect.Modifier));
		if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
		{
			Parts.Add(FString::Printf(
				TEXT("Guard{Guardian=%s,Protected=%s,Stacks=%d,Policy=%s}"),
				*EnumToken(Effect.GuardLink.Guardian),
				*EnumToken(Effect.GuardLink.ProtectedUnit),
				Effect.GuardLink.Stacks,
				*EnumToken(Effect.GuardLink.RedirectPolicy)));
		}
		return FString::Printf(TEXT("Effect{%s}"), *FString::Join(Parts, TEXT(",")));
	}

	FString DescribeEffectArrayContract(const TArray<FGameXXKCardEffect>& Effects)
	{
		TArray<FString> Parts;
		Parts.Reserve(Effects.Num());
		for (const FGameXXKCardEffect& Effect : Effects)
		{
			Parts.Add(DescribeEffectContract(Effect));
		}
		return FString::Join(Parts, TEXT("; "));
	}

	FString DescribeHeavyArrow(const FGameXXKHeavyArrowRule& Rule)
	{
		switch (Rule.Kind)
		{
		case EGameXXKHeavyArrowKind::ExtraAttackPerCharge:
			return FString::Printf(TEXT("重箭：消耗全部蓄力；每消耗1层，追加1段%d%%攻击伤害。"), Rule.MagnitudePerCharge);
		case EGameXXKHeavyArrowKind::ToxicExplosionPerCharge:
			return TEXT("重箭：消耗全部蓄力；每消耗1层，再触发1次毒爆。");
		case EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge:
		{
			FString Text = FString::Printf(
				TEXT("重箭：消耗全部蓄力；每消耗1层，本牌首段攻击倍率+%d个百分点"),
				Rule.MagnitudePerCharge);
			if (Rule.DrawPerCharge > 0)
			{
				Text += FString::Printf(TEXT("并抽%d张牌"), Rule.DrawPerCharge);
			}
			if (Rule.MinimumChargeForEnergy > 0)
			{
				Text += FString::Printf(TEXT("；消耗至少%d层时回复%d点气力一次"), Rule.MinimumChargeForEnergy, Rule.EnergyGain);
			}
			if (Rule.BonusStatus != EGameXXKCardStatus::None && Rule.BonusStatusStacksPerCharge > 0)
			{
				Text += FString::Printf(
					TEXT("；%s获得%d层%s"),
					Rule.BonusStatusTarget == EGameXXKCardEffectTarget::CardOwner ? TEXT("出牌者") : TEXT("所选目标"),
					Rule.BonusStatusStacksPerCharge,
					*DescribeStatus(Rule.BonusStatus));
			}
			return Text + TEXT("。");
		}
		case EGameXXKHeavyArrowKind::None:
		default:
			return FString();
		}
	}

	FString DescribeSpellTaskReward(const EGameXXKHeroSpellTaskReward Reward)
	{
		switch (Reward)
		{
		case EGameXXKHeroSpellTaskReward::Fire:
			return TEXT("法术任务阵赏·炎：全体敌方获得8层灼烧，再触发2次灼烧伤害。");
		case EGameXXKHeroSpellTaskReward::Ice:
			return TEXT("法术任务阵赏·冰：消耗自身全部护甲；按护甲快照层数，对敌方全体逐段造成20%攻击伤害。");
		case EGameXXKHeroSpellTaskReward::Lightning:
			return TEXT("法术任务阵赏·雷：全体敌方获得3层标记，再按各目标标记快照逐层造成60%攻击伤害。");
		case EGameXXKHeroSpellTaskReward::Universal:
			return TEXT("法术任务阵赏·通用：抽4张牌、回复2点气力；本回合后续主角牌气力消耗-1。");
		case EGameXXKHeroSpellTaskReward::None:
		default:
			return FString();
		}
	}

	FString DescribeEffectTarget(const EGameXXKCardEffectTarget Target)
	{
		switch (Target)
		{
		case EGameXXKCardEffectTarget::CardOwner: return TEXT("出牌者");
		case EGameXXKCardEffectTarget::SelectedTarget: return TEXT("所选目标");
		case EGameXXKCardEffectTarget::AllEnemies: return TEXT("全体敌方");
		case EGameXXKCardEffectTarget::AllAllies: return TEXT("全体友方");
		case EGameXXKCardEffectTarget::AllOtherAllies: return TEXT("其他全体友方");
		case EGameXXKCardEffectTarget::EachLivingAlly: return TEXT("每名存活友方");
		case EGameXXKCardEffectTarget::LowestHealthAlly: return TEXT("生命最低友方");
		case EGameXXKCardEffectTarget::LowestHealthOtherAlly: return TEXT("生命最低的其他友方");
		case EGameXXKCardEffectTarget::Attacker: return TEXT("攻击者");
		case EGameXXKCardEffectTarget::PlayedCard: return TEXT("本次打出的牌");
		case EGameXXKCardEffectTarget::HighestArmorAlly: return TEXT("护甲最高友方");
		case EGameXXKCardEffectTarget::HighestAttackAlly: return TEXT("攻击最高友方");
		case EGameXXKCardEffectTarget::PriorityEnemy: return TEXT("标记最高敌方（同层优先生命比例最低）");
		case EGameXXKCardEffectTarget::SelectedTargetSide: return TEXT("所选目标同阵营全体");
		case EGameXXKCardEffectTarget::Invalid:
		default: return EnumToken(Target);
		}
	}

	FString DescribeEffectSource(const EGameXXKCardEffectSource Source)
	{
		switch (Source)
		{
		case EGameXXKCardEffectSource::CardOwner: return TEXT("出牌者");
		case EGameXXKCardEffectSource::SelectedTarget: return TEXT("所选目标");
		case EGameXXKCardEffectSource::HighestArmorAlly: return TEXT("护甲最高友方");
		case EGameXXKCardEffectSource::HighestAttackAlly: return TEXT("攻击最高友方");
		case EGameXXKCardEffectSource::Invalid:
		default: return EnumToken(Source);
		}
	}

	FString DescribeTerrain(const EGameXXKCardTerrain Terrain)
	{
		switch (Terrain)
		{
		case EGameXXKCardTerrain::Plain: return TEXT("平原");
		case EGameXXKCardTerrain::Cliff: return TEXT("山崖");
		case EGameXXKCardTerrain::Forest: return TEXT("山林");
		case EGameXXKCardTerrain::WaterShore: return TEXT("水岸");
		case EGameXXKCardTerrain::Ferry: return TEXT("渡口");
		case EGameXXKCardTerrain::Village: return TEXT("村寨");
		case EGameXXKCardTerrain::Cave: return TEXT("洞穴");
		case EGameXXKCardTerrain::Invalid:
		default: return EnumToken(Terrain);
		}
	}

	FString DescribeTargetMode(const EGameXXKCardTargetMode Mode)
	{
		switch (Mode)
		{
		case EGameXXKCardTargetMode::None: return TEXT("无需选择（直接施放）");
		case EGameXXKCardTargetMode::Self: return TEXT("自身（直接施放）");
		case EGameXXKCardTargetMode::SingleEnemy: return TEXT("单体敌方（点击后选择目标）");
		case EGameXXKCardTargetMode::SingleAlly: return TEXT("单体友方（点击后选择目标）");
		case EGameXXKCardTargetMode::OtherAlly: return TEXT("其他单体友方（点击后选择目标）");
		case EGameXXKCardTargetMode::AllEnemies: return TEXT("全体敌方（直接施放）");
		case EGameXXKCardTargetMode::AllAllies: return TEXT("全体友方（直接施放）");
		case EGameXXKCardTargetMode::AllOtherAllies: return TEXT("除自身外全体友方（直接施放）");
		case EGameXXKCardTargetMode::RandomEnemy: return TEXT("随机敌方（自动结算）");
		case EGameXXKCardTargetMode::LowestHealthAlly: return TEXT("生命最低友方（自动结算）");
		case EGameXXKCardTargetMode::LowestHealthOtherAlly: return TEXT("生命最低的其他友方（自动结算）");
		case EGameXXKCardTargetMode::AnyLivingUnit: return TEXT("任意存活单位（点击后选择目标）");
		case EGameXXKCardTargetMode::Invalid:
		default: return EnumToken(Mode);
		}
	}

	FString DescribeTargetOverride(const FGameXXKCardTargetModeOverride& Override)
	{
		FString Gate;
		switch (Override.ConditionType)
		{
		case EGameXXKCardTargetModeOverrideConditionType::TerrainIsAny:
			Gate = Override.AlternateTerrain == EGameXXKCardTerrain::Invalid
				? FString::Printf(TEXT("地势为%s"), *DescribeTerrain(Override.Terrain))
				: FString::Printf(TEXT("地势为%s或%s"), *DescribeTerrain(Override.Terrain), *DescribeTerrain(Override.AlternateTerrain));
			break;
		case EGameXXKCardTargetModeOverrideConditionType::OwnerHasStatus:
			Gate = FString::Printf(TEXT("出牌者具有至少%d层%s"), Override.MinimumStatusStacks, *DescribeStatus(Override.Status));
			break;
		case EGameXXKCardTargetModeOverrideConditionType::TargetHasStatus:
			Gate = FString::Printf(TEXT("所选目标具有至少%d层%s"), Override.MinimumStatusStacks, *DescribeStatus(Override.Status));
			break;
		case EGameXXKCardTargetModeOverrideConditionType::Invalid:
		default:
			Gate = EnumToken(Override.ConditionType);
			break;
		}
		return FString::Printf(TEXT("当%s时，改为%s"), *Gate, *DescribeTargetMode(Override.Mode));
	}

	FString DescribeTargetSpec(const FGameXXKCardTargetSpec& TargetSpec)
	{
		TArray<FString> Lines;
		Lines.Add(DescribeTargetMode(TargetSpec.Mode));
		if (TargetSpec.RequiredStatus != EGameXXKCardStatus::None)
		{
			Lines.Add(FString::Printf(TEXT("目标需具有至少%d层%s"), TargetSpec.RequiredStatusMinimumStacks, *DescribeStatus(TargetSpec.RequiredStatus)));
		}
		if (TargetSpec.ForbiddenStatus != EGameXXKCardStatus::None)
		{
			Lines.Add(FString::Printf(TEXT("目标不能具有%s"), *DescribeStatus(TargetSpec.ForbiddenStatus)));
		}
		if (TargetSpec.MinimumHealthPercent > 0.0f || TargetSpec.MaximumHealthPercent < 100.0f)
		{
			Lines.Add(FString::Printf(TEXT("目标生命范围%.0f%%–%.0f%%"), TargetSpec.MinimumHealthPercent, TargetSpec.MaximumHealthPercent));
		}
		if (TargetSpec.RequiredTerrain != EGameXXKCardTerrain::Invalid)
		{
			Lines.Add(TargetSpec.AlternateRequiredTerrain == EGameXXKCardTerrain::Invalid
				? FString::Printf(TEXT("需要地势：%s"), *DescribeTerrain(TargetSpec.RequiredTerrain))
				: FString::Printf(TEXT("需要地势：%s或%s"), *DescribeTerrain(TargetSpec.RequiredTerrain), *DescribeTerrain(TargetSpec.AlternateRequiredTerrain)));
		}
		for (const FGameXXKCardTargetModeOverride& Override : TargetSpec.ModeOverrides)
		{
			Lines.Add(DescribeTargetOverride(Override));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString DescribeCondition(const FGameXXKCardEffectCondition& Condition)
	{
		TArray<FString> Clauses;
		FString Gate;
		switch (Condition.Type)
		{
		case EGameXXKCardEffectConditionType::None:
			break;
		case EGameXXKCardEffectConditionType::TargetHasStatus:
			Gate = FString::Printf(
				TEXT("所选目标具有%s%s"),
				*DescribeStatus(Condition.Status),
				Condition.MinimumStatusStacks > 1
					? *FString::Printf(TEXT("至少%d层"), Condition.MinimumStatusStacks)
					: TEXT(""));
			break;
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:
			Gate = TEXT("所选目标具有流血、中毒、灼烧或蚀伤");
			break;
		case EGameXXKCardEffectConditionType::OwnerHasStatus:
			Gate = FString::Printf(
				TEXT("出牌者具有%s%s"),
				*DescribeStatus(Condition.Status),
				Condition.MinimumStatusStacks > 1
					? *FString::Printf(TEXT("至少%d层"), Condition.MinimumStatusStacks)
					: TEXT(""));
			break;
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:
			Gate = FString::Printf(TEXT("出牌者护甲不少于%d"), Condition.MinimumArmor);
			break;
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:
			Gate = FString::Printf(TEXT("出牌者生命低于%.0f%%"), Condition.HealthPercentThreshold);
			break;
		case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:
			Gate = FString::Printf(TEXT("所选目标生命低于%.0f%%"), Condition.HealthPercentThreshold);
			break;
		case EGameXXKCardEffectConditionType::TerrainIsAny:
			Gate = Condition.AlternateTerrain == EGameXXKCardTerrain::Invalid
				? FString::Printf(TEXT("当前地势为%s"), *DescribeTerrain(Condition.Terrain))
				: FString::Printf(TEXT("当前地势为%s或%s"), *DescribeTerrain(Condition.Terrain), *DescribeTerrain(Condition.AlternateTerrain));
			break;
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:
			Gate = TEXT("出牌者具有流血、中毒、灼烧或蚀伤");
			break;
		case EGameXXKCardEffectConditionType::TargetIsAlly:
			Gate = TEXT("所选目标是友方");
			break;
		case EGameXXKCardEffectConditionType::TargetIsEnemy:
			Gate = TEXT("所选目标是敌方");
			break;
		default:
			Gate = EnumToken(Condition.Type);
			break;
		}
		if (!Gate.IsEmpty())
		{
			Clauses.Add(Condition.bNegate ? FString::Printf(TEXT("不满足“%s”时"), *Gate) : FString::Printf(TEXT("当%s时"), *Gate));
		}
		if (Condition.bConsumeStatus)
		{
			Clauses.Add(Condition.MaxConsumedStatusStacks == 0
				? FString::Printf(TEXT("消耗全部%s"), *DescribeStatus(Condition.Status))
				: FString::Printf(TEXT("消耗至多%d层%s"), Condition.MaxConsumedStatusStacks, *DescribeStatus(Condition.Status)));
		}
		if (Condition.bScaleMagnitudeByConsumedStacks) Clauses.Add(TEXT("数值按实际消耗层数结算"));
		if (Condition.bConsumeOwnerArmor)
		{
			Clauses.Add(Condition.MaxConsumedArmor == 0
				? TEXT("消耗出牌者全部护甲")
				: FString::Printf(TEXT("消耗出牌者至多%d点护甲"), Condition.MaxConsumedArmor));
		}
		return FString::Join(Clauses, TEXT("；"));
	}

	FString DescribeModifierTrigger(const EGameXXKCardBattleModifierTrigger Trigger)
	{
		switch (Trigger)
		{
		case EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound: return TEXT("本回合首次受到直接伤害时");
		case EGameXXKCardBattleModifierTrigger::OnCardPlayed: return TEXT("打出牌时");
		case EGameXXKCardBattleModifierTrigger::OnNextAttack: return TEXT("下次攻击时");
		case EGameXXKCardBattleModifierTrigger::OnNextHealing: return TEXT("下次治疗时");
		case EGameXXKCardBattleModifierTrigger::EndOfRound: return TEXT("本回合结束时");
		case EGameXXKCardBattleModifierTrigger::OnSingleTargetEnemyAttack: return TEXT("敌方单体攻击牌结算时");
		case EGameXXKCardBattleModifierTrigger::BeforeNextActiveCard: return TEXT("下一张主动牌结算前");
		case EGameXXKCardBattleModifierTrigger::AfterNextActiveCard: return TEXT("下一张主动牌结算后");
		case EGameXXKCardBattleModifierTrigger::NextPlayerRoundStart: return TEXT("下个玩家回合开始时");
		case EGameXXKCardBattleModifierTrigger::BeforeFirstActiveCardNextPlayerRound: return TEXT("下个玩家回合第一张主动牌结算前");
		case EGameXXKCardBattleModifierTrigger::AfterFirstActiveCardNextPlayerRound: return TEXT("下个玩家回合第一张主动牌结算后");
		case EGameXXKCardBattleModifierTrigger::FirstActiveAttackAgainstStatusNextPlayerRound: return TEXT("下个玩家回合首次主动攻击指定状态目标时");
		case EGameXXKCardBattleModifierTrigger::AfterEachActiveCard: return TEXT("每张主动牌结算后");
		case EGameXXKCardBattleModifierTrigger::Invalid:
		default: return EnumToken(Trigger);
		}
	}

	FString DescribeModifierExpiry(const EGameXXKCardModifierExpiry Expiry, const int32 RemainingTriggers)
	{
		switch (Expiry)
		{
		case EGameXXKCardModifierExpiry::AfterTriggerCount: return FString::Printf(TEXT("触发%d次后失效"), RemainingTriggers);
		case EGameXXKCardModifierExpiry::EndOfCurrentRound: return TEXT("本回合结束时失效");
		case EGameXXKCardModifierExpiry::EndOfCurrentRoundOrTriggerCount: return FString::Printf(TEXT("本回合结束或触发%d次后失效"), RemainingTriggers);
		case EGameXXKCardModifierExpiry::Invalid:
		default: return EnumToken(Expiry);
		}
	}

	FString DescribeOperation(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget TargetValue,
		const EGameXXKCardEffectSource SourceValue,
		const int32 Magnitude,
		const int32 SecondaryMagnitude,
		const int32 HitCount,
		const EGameXXKCardStatus Status)
	{
		const FString Target = DescribeEffectTarget(TargetValue);
		const FString Source = DescribeEffectSource(SourceValue);
		const FString HitSuffix = HitCount > 1 ? FString::Printf(TEXT("，共%d段"), HitCount) : FString();
		switch (Type)
		{
		case EGameXXKCardEffectType::DamagePercentAttack: return FString::Printf(TEXT("%s造成%d%%攻击伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::DamageFlat: return FString::Printf(TEXT("%s受到%d点直接伤害%s"), *Target, Magnitude, *HitSuffix);
		case EGameXXKCardEffectType::LoseHealth: return FString::Printf(TEXT("%s失去%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::Heal:
			return SecondaryMagnitude > 0
				? FString::Printf(TEXT("%s按本牌造成生命伤害的%d%%恢复生命，最多%d点"), *Target, Magnitude, SecondaryMagnitude)
				: FString::Printf(TEXT("%s恢复%d点生命"), *Target, Magnitude);
		case EGameXXKCardEffectType::AddArmor: return FString::Printf(TEXT("%s获得%d点护甲"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainMana: return FString::Printf(TEXT("%s获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainEnergy: return FString::Printf(TEXT("%s回复%d点气力"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainManaPerConsumedStatus: return FString::Printf(TEXT("%s每消耗1层状态获得%d点内力"), *Target, Magnitude);
		case EGameXXKCardEffectType::DrawCards: return FString::Printf(TEXT("%s抽%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ApplyStatus: return FString::Printf(TEXT("%s获得%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveStatus: return FString::Printf(TEXT("移除%s%d层%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::RemoveAnyDamageOverTime: return FString::Printf(TEXT("从%s的流血、中毒、灼烧、蚀伤中依次清除至多%d层"), *Target, Magnitude);
		case EGameXXKCardEffectType::Insight: return FString::Printf(TEXT("%s洞察牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscoverCards: return FString::Printf(TEXT("%s发现%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::ReorderCards: return FString::Printf(TEXT("%s重排牌堆顶%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::DiscardCards: return FString::Printf(TEXT("%s弃置%d张牌"), *Target, Magnitude);
		case EGameXXKCardEffectType::IgnoreDefense: return FString::Printf(TEXT("%s本段伤害无视%d点防御"), *Target, Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercent:
			if (SecondaryMagnitude <= 0)
			{
				return FString::Printf(TEXT("本段攻击倍率+%d个百分点"), Magnitude);
			}
			// MAX_int32 is the uncapped sentinel; never leak it to the player.
			return SecondaryMagnitude >= MAX_int32
				? FString::Printf(TEXT("目标每有1层指定状态，本段攻击倍率+%d个百分点"), Magnitude)
				: FString::Printf(TEXT("目标每有1层指定状态，本段攻击倍率+%d个百分点，最多计算%d层"), Magnitude, SecondaryMagnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedStatus: return FString::Printf(TEXT("每消耗1层状态，本段攻击倍率+%d个百分点"), Magnitude);
		case EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor: return FString::Printf(TEXT("每消耗1点护甲，本段攻击倍率+%d个百分点"), Magnitude);
		case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget: return FString::Printf(TEXT("每名存活友方对%s造成%d%%各自攻击伤害"), *Target, Magnitude);
		case EGameXXKCardEffectType::ModifyHealingPercent: return FString::Printf(TEXT("%s治疗量调整%d%%"), *Target, Magnitude);
		case EGameXXKCardEffectType::ModifyEnergyCost: return FString::Printf(TEXT("%s气力消耗%+d"), *Target, Magnitude);
		case EGameXXKCardEffectType::RevealEnemyIntent: return FString::Printf(TEXT("%s揭示%d个敌方意图"), *Target, Magnitude);
		case EGameXXKCardEffectType::DoubleTerrainBonus: return FString::Printf(TEXT("%s接下来%d次地势收益翻倍"), *Target, Magnitude);
		case EGameXXKCardEffectType::RedirectSingleTargetEnemyAttacks: return FString::Printf(TEXT("%s转移%d次敌方单体攻击"), *Target, Magnitude);
		case EGameXXKCardEffectType::RegisterReaction: return FString::Printf(TEXT("%s登记%d次%s"), *Target, Magnitude, *DescribeStatus(Status));
		case EGameXXKCardEffectType::LoseHealthNonlethal: return FString::Printf(TEXT("%s失去%d点生命，最低保留1点"), *Target, Magnitude);
		case EGameXXKCardEffectType::Cleanse: return FString::Printf(TEXT("清除%s的全部流血、中毒和灼烧"), *Target);
		case EGameXXKCardEffectType::TriggerHighestDamageOverTime: return FString::Printf(TEXT("触发%s层数最高的流血、中毒或灼烧1次，并减少对应状态1层"), *Target);
		case EGameXXKCardEffectType::ResolveToxicExplosion: return FString::Printf(TEXT("对%s毒爆：分别结算流血、中毒、灼烧并各减少1层；蚀伤只追加伤害"), *Target);
		case EGameXXKCardEffectType::HealOrReverseWithMedicine: return FString::Printf(TEXT("消耗出牌者全部药效；若%s为友方，恢复%d+药效层数生命并清除流血、中毒、灼烧；若为敌方，失去%d+药效层数生命"), *Target, Magnitude, Magnitude);
		case EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss: return FString::Printf(TEXT("本牌每实际扣血1名友方，%s获得%d层药效；本次获得至少%d层时再获得1层气势"), *Target, Magnitude, SecondaryMagnitude);
		case EGameXXKCardEffectType::DamagePercentAttackPlusArmor: return FString::Printf(TEXT("由%s对%s造成%d%%攻击+当前护甲的伤害，不消耗护甲"), *Source, *Target, Magnitude);
		case EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor: return FString::Printf(TEXT("消耗%s全部护甲，对%s造成（%d%%+每点护甲%d个百分点）攻击伤害"), *Source, *Target, Magnitude, SecondaryMagnitude);
		case EGameXXKCardEffectType::GainArmorFromCurrentManaPercent: return FString::Printf(TEXT("%s获得等于当前内力%d%%的护甲"), *Target, Magnitude);
		case EGameXXKCardEffectType::GainManaOverflowToArmor: return FString::Printf(TEXT("%s回复%d点内力；溢出内力按%d%%转为护甲"), *Target, SecondaryMagnitude, Magnitude);
		case EGameXXKCardEffectType::SearchUnfinishedHeroTaskCard: return FString::Printf(TEXT("从抽牌堆或弃牌堆检索%d张尚未完成的主角法术任务牌加入手牌"), Magnitude);
		case EGameXXKCardEffectType::SearchUnfinishedTaskNpcCard: return FString::Printf(TEXT("从抽牌堆或弃牌堆检索%d张该任务 NPC 尚未完成的任务牌加入手牌"), Magnitude);
		case EGameXXKCardEffectType::TriggerStatus: return FString::Printf(TEXT("触发%s的%s%d次；每次按当前层数造成生命伤害并减少1层"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot: return FString::Printf(TEXT("按%s当前%s层数，逐层造成%d%%攻击伤害"), *Target, *DescribeStatus(Status), Magnitude);
		case EGameXXKCardEffectType::ReplayTriggeredCardBase: return TEXT("重放本次触发牌的基础效果");
		case EGameXXKCardEffectType::ReplaySourceCardBase: return TEXT("重放本牌的基础效果");
		case EGameXXKCardEffectType::ModifyManaCost: return FString::Printf(TEXT("%s内力消耗%+d"), *Target, Magnitude);
		case EGameXXKCardEffectType::WidenNextActiveSingleTarget: return FString::Printf(TEXT("%s的单体效果扩展为目标所在阵营全体"), *Target);
		case EGameXXKCardEffectType::PreserveNextReactionUse: return TEXT("全队下一次反击或格挡不消耗次数");
		case EGameXXKCardEffectType::RetainArmorNextRound: return FString::Printf(TEXT("%s下回合保留当前护甲"), *Target);
		case EGameXXKCardEffectType::CleanseFriendlyDamageOverTime: return FString::Printf(TEXT("若%s为友方，清除其全部流血、中毒和灼烧"), *Target);
		case EGameXXKCardEffectType::HealOrReverseFlat: return FString::Printf(TEXT("若%s为友方，恢复%d点生命；若为敌方，失去%d点生命"), *Target, Magnitude, Magnitude);
		case EGameXXKCardEffectType::ChangeTerrain: return TEXT("切换地势");
		case EGameXXKCardEffectType::TriggerTerrainBenefit: return FString::Printf(TEXT("触发当前地势收益%d次"), Magnitude);
		case EGameXXKCardEffectType::ApplyGuardLink:
		case EGameXXKCardEffectType::ApplyBattleModifier:
		case EGameXXKCardEffectType::Invalid:
		default: return EnumToken(Type);
		}
	}

	FString DescribeModifier(const FGameXXKCardBattleModifier& Modifier)
	{
		TArray<FString> Clauses;
		Clauses.Add(FString::Printf(
			TEXT("持续效果：%s，%s"),
			*DescribeModifierTrigger(Modifier.Trigger),
			*DescribeOperation(Modifier.EffectType, Modifier.Target, EGameXXKCardEffectSource::CardOwner, Modifier.Magnitude, 0, 1, Modifier.Status)));
		if (Modifier.RequiredTriggeredRole != EGameXXKCharacterRole::Invalid) Clauses.Add(FString::Printf(TEXT("仅%s牌触发"), *DescribeRole(Modifier.RequiredTriggeredRole)));
		if (!Modifier.RequiredTriggeredOwnerId.IsNone()) Clauses.Add(FString::Printf(TEXT("仅%s的牌触发"), *Modifier.RequiredTriggeredOwnerId.ToString()));
		if (Modifier.bActivePlayOnly) Clauses.Add(TEXT("仅主动出牌触发"));
		if (Modifier.bExcludeSourceUnit) Clauses.Add(TEXT("不作用于效果来源单位"));
		if (Modifier.bPreserveTriggeredStatus) Clauses.Add(TEXT("触发状态伤害时不减层"));
		if (Modifier.MinimumResult > 0) Clauses.Add(FString::Printf(TEXT("前序结果至少为%d"), Modifier.MinimumResult));
		const FString Condition = DescribeCondition(Modifier.Condition);
		if (!Condition.IsEmpty()) Clauses.Add(Condition);
		Clauses.Add(DescribeModifierExpiry(Modifier.Expiry, Modifier.RemainingTriggers));
		return FString::Join(Clauses, TEXT("；"));
	}

	FString DescribeEffect(const FGameXXKCardEffect& Effect)
	{
		FString Text;
		if (Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier)
		{
			Text = DescribeModifier(Effect.Modifier);
		}
		else if (Effect.Type == EGameXXKCardEffectType::ApplyGuardLink)
		{
			Text = FString::Printf(
				TEXT("由%s守护%s，转移接下来%d次单体直接攻击"),
				*DescribeEffectTarget(Effect.GuardLink.Guardian),
				*DescribeEffectTarget(Effect.GuardLink.ProtectedUnit),
				Effect.GuardLink.Stacks);
		}
		else if (Effect.Type == EGameXXKCardEffectType::ChangeTerrain)
		{
			Text = FString::Printf(TEXT("切换至%s"), *DescribeTerrain(Effect.TerrainOverride));
		}
		else if (Effect.Type == EGameXXKCardEffectType::TriggerTerrainBenefit)
		{
			const FString Terrain = Effect.TerrainOverride == EGameXXKCardTerrain::Invalid
				? TEXT("当前地势")
				: DescribeTerrain(Effect.TerrainOverride);
			Text = Effect.SecondaryMagnitude > 0
				? FString::Printf(TEXT("触发%s收益%d次；若本回合已实际换场，改为%d次"), *Terrain, Effect.Magnitude, Effect.SecondaryMagnitude)
				: FString::Printf(TEXT("触发%s收益%d次"), *Terrain, Effect.Magnitude);
		}
		else
		{
			Text = DescribeOperation(
				Effect.Type,
				Effect.Target,
				Effect.Source,
				Effect.Magnitude,
				Effect.SecondaryMagnitude,
				Effect.HitCount,
				Effect.Status);
		}

		const FString Condition = DescribeCondition(Effect.Condition);
		if (!Condition.IsEmpty()) Text += FString::Printf(TEXT("（%s）"), *Condition);
		if (!Effect.ResultRef.IsNone()) Text += TEXT("（仅当前述结果成功时）");
		if (!Effect.ConsumedStackResultRef.IsNone())
		{
			Text += Effect.Type == EGameXXKCardEffectType::GainEnergy && Effect.SecondaryMagnitude > 0
				? FString::Printf(TEXT("（前述消耗至少%d层时，仅结算一次）"), Effect.SecondaryMagnitude)
				: TEXT("（仅当前述消耗成功时）");
		}
		return Text;
	}

	FString DescribeEffectArray(
		const FGameXXKCardDefinition& Definition,
		const TArray<FGameXXKCardEffect>& Effects)
	{
		TArray<FString> Lines;
		Lines.Reserve(Effects.Num());
		for (const FGameXXKCardEffect& Effect : Effects)
		{
			Lines.Add(DescribeEffect(Effect));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString DescribeCompleteEffects(const FGameXXKCardDefinition& Definition)
	{
		const FGameXXKCardDefinition EffectiveDefinition = FGameXXKCardQualityRules::BuildEffectiveDefinition(
			Definition,
			Definition.BaseQuality);
		if (EffectiveDefinition.SorcererRule.Family != EGameXXKSorcererCardFamily::None)
		{
			return GameXXKCardText::DescribeEffects(Definition, Definition.BaseQuality);
		}

		TArray<FString> Lines;
		Lines.Add(FString::Printf(TEXT("基础：%s"), *NormalizeLineBreaks(DescribeEffectArray(EffectiveDefinition, EffectiveDefinition.Effects), TEXT("；"))));
		if (!EffectiveDefinition.ChargeEffects.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("冲锋：%s"), *NormalizeLineBreaks(DescribeEffectArray(EffectiveDefinition, EffectiveDefinition.ChargeEffects), TEXT("；"))));
		}
		if (!EffectiveDefinition.FinishEffects.IsEmpty())
		{
			Lines.Add(FString::Printf(TEXT("收招：%s"), *NormalizeLineBreaks(DescribeEffectArray(EffectiveDefinition, EffectiveDefinition.FinishEffects), TEXT("；"))));
		}
		const FString HeavyArrow = DescribeHeavyArrow(EffectiveDefinition.HeavyArrow);
		if (!HeavyArrow.IsEmpty()) Lines.Add(HeavyArrow);
		if (EffectiveDefinition.HunterRule.PriorActiveCardInterval > 0)
		{
			const FGameXXKHunterCardRule& Hunter = EffectiveDefinition.HunterRule;
			TArray<FString> IntervalRewards;
			if (Hunter.DrawPerCompletedInterval > 0)
			{
				IntervalRewards.Add(FString::Printf(TEXT("出牌者抽%d张牌"), Hunter.DrawPerCompletedInterval));
			}
			if (Hunter.StatusPerCompletedInterval != EGameXXKCardStatus::None
				&& Hunter.StatusStacksPerCompletedInterval > 0)
			{
				IntervalRewards.Add(FString::Printf(
					TEXT("出牌者获得%d层%s"),
					Hunter.StatusStacksPerCompletedInterval,
					*DescribeStatus(Hunter.StatusPerCompletedInterval)));
			}
			if (!IntervalRewards.IsEmpty())
			{
				Lines.Add(FString::Printf(
					TEXT("打出本牌时，本回合此前每打出%d张主动牌，%s。"),
					Hunter.PriorActiveCardInterval,
					*FString::Join(IntervalRewards, TEXT("、"))));
			}
		}
		const FString SpellReward = DescribeSpellTaskReward(EffectiveDefinition.SpellTaskReward);
		if (!SpellReward.IsEmpty()) Lines.Add(SpellReward);
		for (const FGameXXKCardEffect& Effect : EffectiveDefinition.Effects)
		{
			if (Effect.Type == EGameXXKCardEffectType::GainEnergy
				&& !Effect.ConsumedStackResultRef.IsNone()
				&& Effect.SecondaryMagnitude > 0)
			{
				Lines.Add(FString::Printf(
					TEXT("结算阈值：同次消耗达到至少%d层时，回复%d点气力一次。"),
					Effect.SecondaryMagnitude,
					Effect.Magnitude));
			}
		}
		if (EffectiveDefinition.bExhaustOnPlay)
		{
			Lines.Add(TEXT("消耗：打出后进入本局消耗区。"));
		}
		return FString::Join(Lines, TEXT("\n"));
	}

	FString DescribeImplementationContract(const FGameXXKCardDefinition& Definition)
	{
		TArray<FString> Parts;
		Parts.Add(FString::Printf(TEXT("TargetMode=%s"), *EnumToken(Definition.TargetSpec.Mode)));
		Parts.Add(FString::Printf(TEXT("Base=[%s]"), *DescribeEffectArrayContract(Definition.Effects)));
		if (!Definition.ChargeEffects.IsEmpty()) Parts.Add(FString::Printf(TEXT("Charge=[%s]"), *DescribeEffectArrayContract(Definition.ChargeEffects)));
		if (!Definition.FinishEffects.IsEmpty()) Parts.Add(FString::Printf(TEXT("Finish=[%s]"), *DescribeEffectArrayContract(Definition.FinishEffects)));
		if (Definition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None)
		{
			Parts.Add(FString::Printf(
				TEXT("HeavyArrow{Kind=%s,PerCharge=%d,DrawPerCharge=%d,MinCharge=%d,Energy=%d}"),
				*EnumToken(Definition.HeavyArrow.Kind),
				Definition.HeavyArrow.MagnitudePerCharge,
				Definition.HeavyArrow.DrawPerCharge,
				Definition.HeavyArrow.MinimumChargeForEnergy,
				Definition.HeavyArrow.EnergyGain));
		}
		if (Definition.SpellTaskReward != EGameXXKHeroSpellTaskReward::None) Parts.Add(FString::Printf(TEXT("SpellReward=%s"), *EnumToken(Definition.SpellTaskReward)));
		if (Definition.SorcererRule.Family != EGameXXKSorcererCardFamily::None)
		{
			Parts.Add(FString::Printf(
				TEXT("Sorcerer{Family=%s,Sequence=%s,Reward=%s}"),
				*EnumToken(Definition.SorcererRule.Family),
				*EnumToken(Definition.SorcererRule.SequenceRule),
				*EnumToken(Definition.SorcererRule.RewardRule)));
		}
		if (Definition.bExhaustOnPlay) Parts.Add(TEXT("Exhaust=true"));
		return FString::Join(Parts, TEXT(" | "));
	}

	struct FDocumentBundle
	{
		FString Markdown;
		FString PlainText;
	};

	FDocumentBundle BuildDocuments(const TArray<FGameXXKCardDefinition>& Definitions)
	{
		TMap<EGameXXKCardOwner, int32> OwnerCounts;
		TMap<FString, int32> GroupCounts;
		for (const FGameXXKCardDefinition& Definition : Definitions)
		{
			++OwnerCounts.FindOrAdd(Definition.Owner);
			++GroupCounts.FindOrAdd(DescribeGroup(Definition));
		}

		FDocumentBundle Result;
		Result.Markdown = TEXT(
			"# GameXXK 全卡牌目录（2026-08-11 当前实现基线）\n\n"
			"> 数据源：`FGameXXKCardCatalog::GetAllCardDefinitions()` 与当前品质解析、卡牌文本格式器。本文档列出当前代码实际登记的全部卡牌，并保留实现签名用于核对。\n\n"
			"> 数值口径：“完整效果”按卡牌当前基础品质换算为局内实际数值（普通 ×1、稀有伤害/治疗/护甲 ×2、珍稀 ×4；层数类按品质阶数递增）。“实现签名”保留目录中的未缩放底值，供程序核对；两列数值不同不是冲突。\n\n"
			"> 验收边界：卡牌目录、特殊卡牌规则及流血、中毒、灼烧、蚀伤等全局状态触发时点均以当前代码与自动化测试为准；实现签名只用于程序核对，不作为局内 Tooltip 展示。\n\n"
			"## 核对结论\n\n"
			"- 当前目录总数：198 张。\n"
			"- 主角：36 张；永久伙伴：108 张；任务 NPC：24 张；路线临时卡：30 张。\n"
			"- 当前代码没有独立的“剑意”状态。《剑意贯虹》消耗开牌瞬间的全部气势；每层使本段攻击倍率增加 20 个百分点，同时保留该层气势原本的 +1 固定伤害；消耗至少 3 层时回复 1 点气力一次。\n"
			"- 《剑意贯虹》有 N 层气势时，防御前请求值为：`攻击力 × (260% + 20% × N) + N`；结算后气势归零。\n"
			"- 冲锋栏只在该牌是本回合第一张主动牌时生效；收招栏在玩家确认结束回合后，由本回合最后一张主动牌触发。\n"
			"- 重箭先锁定并消耗全部蓄力，再按每层执行牌面重箭收益。\n"
			"- 主角法术任务要求其装备的 8 张牌各主动打出一次，随后按首次打出顺序重放 8 张基础效果，并只结算首张牌的任务奖励。\n\n"
			"## 数量分布\n\n"
			"| 归属 | 数量 |\n"
			"| --- | ---: |\n");
		Result.Markdown += FString::Printf(TEXT("| 主角 | %d |\n"), OwnerCounts.FindRef(EGameXXKCardOwner::Hero));
		Result.Markdown += FString::Printf(TEXT("| 永久伙伴 | %d |\n"), OwnerCounts.FindRef(EGameXXKCardOwner::Profession));
		Result.Markdown += FString::Printf(TEXT("| 任务 NPC | %d |\n"), OwnerCounts.FindRef(EGameXXKCardOwner::QuestNpc));
		Result.Markdown += FString::Printf(TEXT("| 路线临时卡 | %d |\n\n"), OwnerCounts.FindRef(EGameXXKCardOwner::Route));

		Result.PlainText = TEXT(
			"GameXXK 全卡牌目录（2026-08-11 当前实现基线）\n"
			"============================================================\n"
			"数据源：FGameXXKCardCatalog::GetAllCardDefinitions() 与当前品质解析、卡牌文本格式器。\n"
			"数值口径：完整效果按基础品质换算为局内实际值；实现签名保留未缩放底值。伤害/治疗/护甲为普通×1、稀有×2、珍稀×4，层数类按品质阶数递增。\n"
			"验收边界：卡牌目录、特殊规则与全局状态触发时点均以当前代码和自动化测试为准；实现签名不作为局内 Tooltip 展示。\n\n"
			"核对结论\n"
			"- 总数 198：主角 36 / 永久伙伴 108 / 任务 NPC 24 / 路线临时卡 30。\n"
			"- 当前代码没有独立的“剑意”状态。剑意贯虹消耗全部气势，每层攻击倍率 +20 个百分点，并保留每层气势 +1 固定伤害；消耗至少 3 层时回复 1 点气力一次。\n"
			"- 剑意贯虹 N 层气势公式：攻击力 × (260% + 20% × N) + N；结算后气势归零。\n"
			"- 冲锋：本回合第一张主动牌。收招：确认结束回合后结算本回合最后一张主动牌。\n"
			"- 重箭：锁定并消耗全部蓄力，再逐层执行重箭收益。\n"
			"- 法术任务：主角装备的 8 张牌各主动打出一次后，依首次打出顺序重放基础效果，只执行首张牌任务奖励。\n");

		FString PreviousGroup;
		TSet<FString> EmittedGroups;
		int32 GroupIndex = 0;
		for (int32 CardIndex = 0; CardIndex < Definitions.Num(); ++CardIndex)
		{
			const FGameXXKCardDefinition& Definition = Definitions[CardIndex];
			const FString Group = DescribeGroup(Definition);
			if (Group != PreviousGroup)
			{
				++GroupIndex;
				EmittedGroups.Add(Group);
				Result.Markdown += FString::Printf(
					TEXT("## %d. %s（%d 张）\n\n| # | 卡牌 | CardId | 品质 | 费用 | 目标 | 获取 | 完整效果 | 实现签名 |\n| ---: | --- | --- | --- | --- | --- | --- | --- | --- |\n"),
					GroupIndex,
					*Group,
					GroupCounts.FindRef(Group));
				Result.PlainText += FString::Printf(
					TEXT("\n\n[%d] %s（%d 张）\n%s\n"),
					GroupIndex,
					*Group,
					GroupCounts.FindRef(Group),
					TEXT("------------------------------------------------------------"));
				PreviousGroup = Group;
			}

			const FString Quality = FGameXXKCardQualityRules::GetDisplayName(Definition.BaseQuality).ToString();
			const FString Target = DescribeTargetSpec(Definition.TargetSpec);
			const FString Effects = DescribeCompleteEffects(Definition);
			const FString Contract = DescribeImplementationContract(Definition);
			const FString Acquisition = DescribeAcquisition(Definition);

			Result.Markdown += FString::Printf(
				TEXT("| %03d | %s | `%s` | %s | %d 气 / %d 内 | %s | %s | %s | `%s` |\n"),
				CardIndex + 1,
				*EscapeMarkdownCell(Definition.DisplayName.ToString()),
				*Definition.Id.ToString(),
				*EscapeMarkdownCell(Quality),
				Definition.EnergyCost,
				Definition.ManaCost,
				*EscapeMarkdownCell(Target),
				*EscapeMarkdownCell(Acquisition),
				*EscapeMarkdownCell(Effects),
				*EscapeMarkdownCell(Contract));

			Result.PlainText += FString::Printf(
				TEXT("\n[%03d] %s\nID：%s\n品质：%s\n费用：%d 气力 / %d 内力\n目标：%s\n获取：%s\n完整效果：\n%s\n实现签名：%s\n"),
				CardIndex + 1,
				*Definition.DisplayName.ToString(),
				*Definition.Id.ToString(),
				*Quality,
				Definition.EnergyCost,
				Definition.ManaCost,
				*NormalizeLineBreaks(Target, TEXT("；")),
				*Acquisition,
				*Effects,
				*Contract);
		}
		return Result;
	}

	bool SaveDocument(const FString& AbsolutePath, const FString& Content)
	{
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(AbsolutePath), true);
		return FFileHelper::SaveStringToFile(
			Content,
			*AbsolutePath,
			FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM);
	}
}

bool FGameXXKCardDocumentationTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKCardDocumentationTest;
	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("documentation exports the complete catalog"), Definitions.Num(), 198);
	if (Definitions.Num() != 198)
	{
		return false;
	}

	const FDocumentBundle Expected = BuildDocuments(Definitions);
	struct FVisibleCardTextCase
	{
		FName CardId;
		const TCHAR* ExpectedText;
		const TCHAR* ForbiddenToken;
	};
	const TArray<FVisibleCardTextCase> VisibleCardTextCases = {
		{TEXT("Hero.Formation.LianYingBuShi"), TEXT("触发当前地势收益1次"), TEXT("TriggerTerrainBenefit")},
		{TEXT("Profession.Guard.TieBiRuShan"), TEXT("破绽免疫"), TEXT("CannotReceiveVulnerability")},
		{TEXT("Profession.FormationMaster.WanXiangGuiZhen"), TEXT("地形免耗"), TEXT("NextTerrainCardFree")},
		{TEXT("Route.Terrain.XingJunBuZhen"), TEXT("地形减耗"), TEXT("NextTerrainCardEnergyReduction")}};
	for (const FVisibleCardTextCase& TextCase : VisibleCardTextCases)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(TextCase.CardId);
		TestNotNull(*FString::Printf(TEXT("%s documentation fixture exists"), *TextCase.CardId.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		const FString Effects = DescribeCompleteEffects(*Definition);
		TestTrue(
			*FString::Printf(TEXT("%s documentation uses concise Chinese visible text"), *TextCase.CardId.ToString()),
			Effects.Contains(TextCase.ExpectedText));
		TestFalse(
			*FString::Printf(TEXT("%s documentation does not expose an internal enum token"), *TextCase.CardId.ToString()),
			Effects.Contains(TextCase.ForbiddenToken));
	}
	const TArray<FString> FinalSorcererNames = {
		TEXT("灵枢引法"), TEXT("周天归元"), TEXT("灵火点灯"), TEXT("流焰传薪"),
		TEXT("焚脉爆炎"), TEXT("燎原寻诀"), TEXT("寒息回流"), TEXT("玄冰拓脉"),
		TEXT("霜镜叠甲"), TEXT("冰鉴索法"), TEXT("引雷定标"), TEXT("雷符索敌"),
		TEXT("连霆穿云"), TEXT("雷走八方"), TEXT("万法归一"), TEXT("照见五蕴"),
		TEXT("六合护法"), TEXT("斗转星移")};
	for (const FString& Name : FinalSorcererNames)
	{
		TestTrue(
			FString::Printf(TEXT("Markdown exports final Sorcerer card %s"), *Name),
			Expected.Markdown.Contains(FString::Printf(TEXT("| %s |"), *Name)));
		TestTrue(
			FString::Printf(TEXT("plain text exports final Sorcerer card %s"), *Name),
			Expected.PlainText.Contains(FString::Printf(TEXT("] %s\n"), *Name)));
	}
	TestTrue(
		TEXT("documentation exposes Sorcerer family, sequence, and reward metadata"),
		Expected.Markdown.Contains(TEXT("Sorcerer{Family=Core,Sequence=CoreSearch,Reward=CoreSearch}")));
	TestTrue(
		TEXT("documentation describes the five-card completion rule"),
		Expected.Markdown.Contains(TEXT("携带的5张法师牌各主动打出一次")));
	TestFalse(
		TEXT("Markdown does not claim the verified global status rules remain under audit"),
		Expected.Markdown.Contains(TEXT("正在逐条代码/测试审计")));
	TestFalse(
		TEXT("plain text does not claim the verified global status rules remain under audit"),
		Expected.PlainText.Contains(TEXT("仍在逐条审计")));
	const FString MarkdownPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), MarkdownRelativePath);
	const FString TextPath = FPaths::ConvertRelativePathToFull(FPaths::ProjectDir(), TextRelativePath);
	if (FParse::Param(FCommandLine::Get(), TEXT("GameXXKUpdateCardDocs")))
	{
		TestTrue(TEXT("Markdown card catalog writes successfully"), SaveDocument(MarkdownPath, Expected.Markdown));
		TestTrue(TEXT("plain-text card catalog writes successfully"), SaveDocument(TextPath, Expected.PlainText));
	}

	FString ActualMarkdown;
	FString ActualText;
	const bool bLoadedMarkdown = FFileHelper::LoadFileToString(ActualMarkdown, *MarkdownPath);
	const bool bLoadedText = FFileHelper::LoadFileToString(ActualText, *TextPath);
	TestTrue(TEXT("Markdown card catalog exists"), bLoadedMarkdown);
	TestTrue(TEXT("plain-text card catalog exists"), bLoadedText);
	if (!bLoadedMarkdown || !bLoadedText)
	{
		AddError(TEXT("Run this test once with -GameXXKUpdateCardDocs to regenerate both checked-in documents."));
		return false;
	}

	TestTrue(TEXT("Markdown card catalog matches the live catalog exactly"), ActualMarkdown == Expected.Markdown);
	TestTrue(TEXT("plain-text card catalog matches the live catalog exactly"), ActualText == Expected.PlainText);
	TestTrue(TEXT("Markdown explicitly locks the implemented Jian Yi rule"), ActualMarkdown.Contains(TEXT("攻击力 × (260% + 20% × N) + N")));
	TestTrue(TEXT("plain text explicitly locks the implemented Jian Yi rule"), ActualText.Contains(TEXT("攻击力 × (260% + 20% × N) + N")));
	TestFalse(TEXT("Markdown contains no unresolved formatter fallback"), ActualMarkdown.Contains(TEXT("未知")));
	TestFalse(TEXT("plain text contains no unresolved formatter fallback"), ActualText.Contains(TEXT("未知")));
	TestFalse(TEXT("Markdown contains no invalid effect, status, target, trigger, or terrain fallback"), ActualMarkdown.Contains(TEXT("无效")));
	TestFalse(TEXT("plain text contains no invalid effect, status, target, trigger, or terrain fallback"), ActualText.Contains(TEXT("无效")));
	TestFalse(TEXT("Markdown never describes zero as a finite consumption cap"), ActualMarkdown.Contains(TEXT("至多0")));
	TestFalse(TEXT("plain text never describes zero as a finite consumption cap"), ActualText.Contains(TEXT("至多0")));
	return true;
}

#endif
