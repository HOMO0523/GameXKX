#include "UI/GameXXKBattleStatusIconStyle.h"

#include "HAL/CriticalSection.h"
#include "Misc/ScopeLock.h"

namespace
{
	const FString StatusIconRoot(TEXT("/Game/GameXXK/UI/Battle/StatusIcons/"));
	TSet<uint8> LoggedFallbackStatusValues;
	FCriticalSection LoggedFallbackStatusValuesLock;

	FString ResolveFallbackGlyph(const FName IconId)
	{
		if (IconId == TEXT("ArmorShield")) return TEXT("◆");
		if (IconId == TEXT("MomentumSeal")) return TEXT("●");
		if (IconId == TEXT("AgilityWing")) return TEXT("›");
		if (IconId == TEXT("VulnerabilityMask")) return TEXT("◇");
		if (IconId == TEXT("BleedDrop")) return TEXT("▼");
		if (IconId == TEXT("PoisonVial")) return TEXT("◉");
		if (IconId == TEXT("BurnFlame")) return TEXT("▲");
		if (IconId == TEXT("MarkTarget")) return TEXT("◎");
		if (IconId == TEXT("GuardShield")) return TEXT("■");
		if (IconId == TEXT("RotSpiral")) return TEXT("⊙");
		if (IconId == TEXT("ImmunityTalisman")) return TEXT("✦");
		if (IconId == TEXT("TacticSeal")) return TEXT("✧");
		if (IconId == TEXT("TerrainAndRedirect")) return TEXT("◆");
		if (IconId == TEXT("MedicineHerbs")) return TEXT("✦");
		if (IconId == TEXT("WeakBrokenBlade")) return TEXT("⌁");
		if (IconId == TEXT("WealthCoin")) return TEXT("◉");
		if (IconId == TEXT("RageFlame")) return TEXT("▲");
		if (IconId == TEXT("PreyTargetEye")) return TEXT("◎");
		if (IconId == TEXT("ChargeSpiralHorn")) return TEXT("◌");
		if (IconId == TEXT("CounterHookBlade")) return TEXT("↶");
		return TEXT("?");
	}

	FGameXXKBattleStatusIconStyle MakeStyle(
		const TCHAR* IconId,
		const TCHAR* DisplayName,
		const TCHAR* Effect,
		const TCHAR* Timing,
		const FLinearColor& Tint,
		const int32 Priority)
	{
		FGameXXKBattleStatusIconStyle Style;
		Style.IconId = FName(IconId);
		Style.TexturePath = FSoftObjectPath(StatusIconRoot + FString::Printf(TEXT("T_BattleStatus_%s.T_BattleStatus_%s"), IconId, IconId));
		Style.DisplayName = DisplayName;
		Style.Effect = Effect;
		Style.Timing = Timing;
		Style.Tooltip = FString::Printf(TEXT("效果：%s\n时机：%s"), Effect, Timing);
		Style.Tint = Tint;
		Style.Priority = Priority;
		Style.FallbackGlyph = ResolveFallbackGlyph(Style.IconId);
		Style.bUsesPaperInkFallback = true;
		return Style;
	}

	FGameXXKBattleStatusIconStyle MakeFallbackStyle(const EGameXXKCardStatus Status)
	{
		FGameXXKBattleStatusIconStyle Style = MakeStyle(
			TEXT("UnknownStatus"),
			TEXT("未知状态"),
			TEXT("当前规则没有为该状态注册专用图标；它仍会保持可见。"),
			TEXT("仅展示当前权威层数；不会由界面自行结算或移除。"),
			FLinearColor(0.34f, 0.24f, 0.18f, 1.0f),
			10);
		Style.bFallback = true;
		Style.FallbackGlyph = TEXT("?");
		Style.DisplayName = FString::Printf(TEXT("未知状态（枚举值 %d）"), static_cast<int32>(Status));
		Style.Tooltip = FString::Printf(TEXT("%s\n%s"), *Style.DisplayName, *Style.Tooltip);
		if (Status != EGameXXKCardStatus::Invalid && Status != EGameXXKCardStatus::None)
		{
			const uint8 RawValue = static_cast<uint8>(Status);
			FScopeLock Lock(&LoggedFallbackStatusValuesLock);
			if (!LoggedFallbackStatusValues.Contains(RawValue))
			{
				LoggedFallbackStatusValues.Add(RawValue);
				UE_LOG(LogTemp, Warning, TEXT("GameXXK battle status icon fallback for unmapped status enum value %d."), static_cast<int32>(RawValue));
			}
		}
		return Style;
	}
}

FGameXXKBattleStatusIconStyle FGameXXKBattleStatusIconStyle::ResolveArmorIconStyle()
{
	return MakeStyle(
		TEXT("ArmorShield"),
		TEXT("护甲"),
		TEXT("当前护甲在直接攻击时先于生命吸收伤害；每次受击会按实际吸收量减少。"),
		TEXT("该单位所属阵营的阶段开始时清空护甲。"),
		FLinearColor(0.40f, 0.48f, 0.53f, 1.0f),
		1000);
}

FGameXXKBattleStatusIconStyle FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(const EGameXXKCardStatus Status)
{
	switch (Status)
	{
	case EGameXXKCardStatus::Momentum:
		return MakeStyle(TEXT("MomentumSeal"), TEXT("气势"), TEXT("供带有气势条件或消耗条款的卡牌读取和消耗。"), TEXT("不会自动造成伤害；由对应卡牌结算时消耗。"), FLinearColor(0.57f, 0.42f, 0.27f, 1.0f), 520);
	case EGameXXKCardStatus::Agility:
		return MakeStyle(TEXT("AgilityWing"), TEXT("敏捷"), TEXT("使一次直接攻击完全闪避。"), TEXT("被单体与群体直接攻击命中前消耗 1 层；环境伤害不触发。"), FLinearColor(0.38f, 0.50f, 0.52f, 1.0f), 800);
	case EGameXXKCardStatus::Vulnerability:
		return MakeStyle(TEXT("VulnerabilityMask"), TEXT("易伤"), TEXT("直接攻击伤害每层提高 10%。"), TEXT("下一次直接攻击结算后清除全部易伤层数。"), FLinearColor(0.56f, 0.37f, 0.44f, 1.0f), 920);
	case EGameXXKCardStatus::Bleed:
		return MakeStyle(TEXT("BleedDrop"), TEXT("流血"), TEXT("每层在回合结束时造成 3 点生命伤害。"), TEXT("每个阵营结算其回合结束阶段时触发；不消耗层数。"), FLinearColor(0.55f, 0.31f, 0.29f, 1.0f), 980);
	case EGameXXKCardStatus::Poison:
		return MakeStyle(TEXT("PoisonVial"), TEXT("中毒"), TEXT("每层在回合结束时造成 2 点生命伤害。"), TEXT("每个阵营结算其回合结束阶段时触发；不消耗层数。"), FLinearColor(0.41f, 0.49f, 0.32f, 1.0f), 970);
	case EGameXXKCardStatus::Burn:
		return MakeStyle(TEXT("BurnFlame"), TEXT("灼烧"), TEXT("每层在回合结束时造成 3 点生命伤害。"), TEXT("每个阵营结算其回合结束阶段时触发；不消耗层数。"), FLinearColor(0.65f, 0.39f, 0.30f, 1.0f), 960);
	case EGameXXKCardStatus::Mark:
		return MakeStyle(TEXT("MarkTarget"), TEXT("标记"), TEXT("供带有标记条件的卡牌读取。"), TEXT("不会自动造成伤害；由对应卡牌条件或效果结算时使用。"), FLinearColor(0.55f, 0.33f, 0.34f, 1.0f), 900);
	case EGameXXKCardStatus::Guard:
		return MakeStyle(TEXT("GuardShield"), TEXT("守护"), TEXT("守护实际由绑定的守护者与受护者重定向关系结算。"), TEXT("该兼容状态不独立结算；守护绑定耗尽或任一单位倒下时移除。"), FLinearColor(0.33f, 0.44f, 0.52f, 1.0f), 810);
	case EGameXXKCardStatus::DamageOverTime:
		return MakeStyle(TEXT("RotSpiral"), TEXT("蚀伤"), TEXT("每层在回合结束时造成 3 点生命伤害。"), TEXT("每个阵营结算其回合结束阶段时触发；不消耗层数。"), FLinearColor(0.40f, 0.35f, 0.50f, 1.0f), 950);
	case EGameXXKCardStatus::CannotReceiveVulnerability:
		return MakeStyle(TEXT("ImmunityTalisman"), TEXT("易伤免疫"), TEXT("阻止获得新的易伤层数。"), TEXT("在添加易伤时检查；当前状态不会自行消耗。"), FLinearColor(0.40f, 0.52f, 0.50f, 1.0f), 850);
	case EGameXXKCardStatus::NextAttackBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("追击标记"), TEXT("下一次攻击的首段命中额外施加 1 层标记。"), TEXT("准备攻击后消耗 1 层。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 740);
	case EGameXXKCardStatus::NextAttackAppliesVulnerability:
		return MakeStyle(TEXT("TacticSeal"), TEXT("破绽追击"), TEXT("下一次攻击的首段命中额外施加 1 层易伤。"), TEXT("准备攻击后消耗 1 层。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 750);
	case EGameXXKCardStatus::NextHealingBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("疗愈增幅"), TEXT("下一次治疗动作额外增加等同当前层数的治疗量。"), TEXT("开始结算一次治疗动作时消耗全部层数。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 730);
	case EGameXXKCardStatus::TerrainBonusDouble:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形双效"), TEXT("下一张地形卡会复制其地形条件攻击与挂接效果。"), TEXT("任一存活友方打出地形卡时，按稳定顺序使用并消耗 1 层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 700);
	case EGameXXKCardStatus::NextTerrainCardFree:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形免耗"), TEXT("下一张地形卡的能量费用变为 0。"), TEXT("任一存活友方提交地形卡时，按稳定顺序使用并消耗 1 层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 710);
	case EGameXXKCardStatus::NextTerrainCardEnergyReduction:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形减耗"), TEXT("每个持有者使下一张地形卡能量费用降低 1。"), TEXT("地形卡提交时每名持有者消耗 1 层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 720);
	case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("代挡"), TEXT("将一次敌方单体攻击改为命中此单位。"), TEXT("敌方单体攻击结算时消耗 1 层；群体攻击不会触发。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 830);
	case EGameXXKCardStatus::TerrainBonusDoubleThisRound:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("本回合地形双效"), TEXT("本回合内下一张地形卡会复制其地形条件攻击与挂接效果。"), TEXT("使用时消耗 1 层；若未使用，会在下一玩家阶段前清除。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 760);
	case EGameXXKCardStatus::Medicine:
		return MakeStyle(TEXT("MedicineHerbs"), TEXT("药材"), TEXT("由特定敌人技能作为治疗资源读取或消耗。"), TEXT("只在对应敌人技能结算时改变。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 540);
	case EGameXXKCardStatus::Weak:
		return MakeStyle(TEXT("WeakBrokenBlade"), TEXT("虚弱"), TEXT("敌人施加的不利状态，供战斗规则与卡牌条件读取。"), TEXT("按施加它的技能和对应规则结算；界面不自行修改数值。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 910);
	case EGameXXKCardStatus::Wealth:
		return MakeStyle(TEXT("WealthCoin"), TEXT("财富"), TEXT("金钱鼠技能积累和消耗的财富层数。"), TEXT("在对应财富技能结算时改变。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 550);
	case EGameXXKCardStatus::Rage:
		return MakeStyle(TEXT("RageFlame"), TEXT("狂怒"), TEXT("敌人技能积累并读取的攻击强化层数。"), TEXT("在对应狂怒技能或阶段规则结算时改变。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 890);
	case EGameXXKCardStatus::Prey:
		return MakeStyle(TEXT("PreyTargetEye"), TEXT("猎物"), TEXT("标识捕猎类敌人当前锁定的目标。"), TEXT("由捕猎技能建立，并按对应目标规则清除。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 900);
	case EGameXXKCardStatus::Charge:
		return MakeStyle(TEXT("ChargeSpiralHorn"), TEXT("蓄力"), TEXT("表示敌人已准备一个延迟发动的技能。"), TEXT("倒计时完成时结算已保存的蓄力技能。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 880);
	case EGameXXKCardStatus::Counter:
		return MakeStyle(TEXT("CounterHookBlade"), TEXT("反击"), TEXT("表示敌人已准备响应直接攻击的反击。"), TEXT("受到符合条件的直接攻击时按对应被动规则触发。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 870);
	case EGameXXKCardStatus::Invalid:
	case EGameXXKCardStatus::None:
	default:
		return MakeFallbackStyle(Status);
	}
}

FString FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(const FGameXXKBattleStatusIconStyle& Style, const int32 Stacks)
{
	return FString::Printf(
		TEXT("%s\n层数：%d\n%s"),
		*Style.DisplayName,
		FMath::Max(1, Stacks),
		*Style.Tooltip);
}
