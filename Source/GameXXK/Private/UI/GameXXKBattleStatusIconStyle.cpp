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
		if (IconId == TEXT("BlockShield")) return TEXT("▣");
		return TEXT("?");
	}

	FGameXXKBattleStatusIconStyle MakeStyle(
		const TCHAR* IconId,
		const TCHAR* DisplayName,
		const TCHAR* Rule,
		const FLinearColor& Tint,
		const int32 Priority)
	{
		FGameXXKBattleStatusIconStyle Style;
		Style.IconId = FName(IconId);
		Style.TexturePath = FSoftObjectPath(StatusIconRoot + FString::Printf(TEXT("T_BattleStatus_%s.T_BattleStatus_%s"), IconId, IconId));
		Style.DisplayName = DisplayName;
		Style.Tooltip = Rule;
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
			TEXT("当前规则没有为该状态注册专用图标；仅展示权威层数。"),
			FLinearColor(0.34f, 0.24f, 0.18f, 1.0f),
			10);
		Style.bFallback = true;
		Style.FallbackGlyph = TEXT("?");
		Style.DisplayName = FString::Printf(TEXT("未知状态（枚举值 %d）"), static_cast<int32>(Status));
		Style.Tooltip = FString::Printf(TEXT("未配置状态枚举 %d；仅展示权威层数。"), static_cast<int32>(Status));
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
		TEXT("优先抵挡直接攻击伤害；所属阵营回合开始时清空。"),
		FLinearColor(0.40f, 0.48f, 0.53f, 1.0f),
		1000);
}

FGameXXKBattleStatusIconStyle FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(const EGameXXKCardStatus Status)
{
	switch (Status)
	{
	case EGameXXKCardStatus::Momentum:
		return MakeStyle(TEXT("MomentumSeal"), TEXT("气势"), TEXT("每层使每段攻击伤害+1；仅指定牌与驱散会消耗。"), FLinearColor(0.57f, 0.42f, 0.27f, 1.0f), 520);
	case EGameXXKCardStatus::Agility:
		return MakeStyle(TEXT("AgilityWing"), TEXT("灵动"), TEXT("25%概率消耗1层完美闪避；失败时可消耗2层闪避。"), FLinearColor(0.38f, 0.50f, 0.52f, 1.0f), 800);
	case EGameXXKCardStatus::Vulnerability:
		return MakeStyle(TEXT("VulnerabilityMask"), TEXT("破绽"), TEXT("每层使下一段直接攻击伤害提高10%；结算后清空。"), FLinearColor(0.56f, 0.37f, 0.44f, 1.0f), 920);
	case EGameXXKCardStatus::Bleed:
		return MakeStyle(TEXT("BleedDrop"), TEXT("流血"), TEXT("受到直接攻击后，失去等同层数的生命并减少1层；回合结束不衰减。"), FLinearColor(0.55f, 0.31f, 0.29f, 1.0f), 980);
	case EGameXXKCardStatus::Poison:
		return MakeStyle(TEXT("PoisonVial"), TEXT("中毒"), TEXT("任意一方回合结束时，失去等同中毒值的生命。"), FLinearColor(0.41f, 0.49f, 0.32f, 1.0f), 970);
	case EGameXXKCardStatus::Burn:
		return MakeStyle(TEXT("BurnFlame"), TEXT("灼烧"), TEXT("打出牌或执行意图后，失去等同层数的生命并减少1层；回合结束再减少1层。"), FLinearColor(0.65f, 0.39f, 0.30f, 1.0f), 960);
	case EGameXXKCardStatus::Mark:
		return MakeStyle(TEXT("MarkTarget"), TEXT("标记"), TEXT("直接攻击伤害提高15%；每段有效命中后减少1层。"), FLinearColor(0.55f, 0.33f, 0.34f, 1.0f), 900);
	case EGameXXKCardStatus::Guard:
		return MakeStyle(TEXT("GuardShield"), TEXT("守护"), TEXT("下一次针对本单位的单体攻击由守护者承受；触发后减少1层。"), FLinearColor(0.33f, 0.44f, 0.52f, 1.0f), 810);
	case EGameXXKCardStatus::DamageOverTime:
		return MakeStyle(TEXT("RotSpiral"), TEXT("蚀伤"), TEXT("流血、中毒或灼烧造成伤害时，额外失去等同层数的生命；回合结束减少1层。"), FLinearColor(0.40f, 0.35f, 0.50f, 1.0f), 950);
	case EGameXXKCardStatus::CannotReceiveVulnerability:
		return MakeStyle(TEXT("ImmunityTalisman"), TEXT("破绽免疫"), TEXT("无法获得新的破绽；不会自行消耗。"), FLinearColor(0.40f, 0.52f, 0.50f, 1.0f), 850);
	case EGameXXKCardStatus::NextAttackBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("追击标记"), TEXT("下一次攻击的首段命中施加1层标记；出手后减少1层。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 740);
	case EGameXXKCardStatus::NextAttackAppliesVulnerability:
		return MakeStyle(TEXT("TacticSeal"), TEXT("破绽追击"), TEXT("下一次攻击的首段命中施加1层破绽；出手后减少1层。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 750);
	case EGameXXKCardStatus::NextHealingBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("疗愈增幅"), TEXT("下一次治疗中，每个目标的治疗量增加等同层数的数值；结算后清空。"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 730);
	case EGameXXKCardStatus::TerrainBonusDouble:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形双效"), TEXT("队伍下一张地形牌的地形条件效果额外结算1次；使用后减少1层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 700);
	case EGameXXKCardStatus::NextTerrainCardFree:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形免耗"), TEXT("队伍下一张地形牌的气力消耗变为0；使用后减少1层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 710);
	case EGameXXKCardStatus::NextTerrainCardEnergyReduction:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地形减耗"), TEXT("队伍下一张地形牌的气力消耗-1；使用后减少1层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 720);
	case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("代挡"), TEXT("替队友承受下一次敌方单体攻击；触发后减少1层。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 830);
	case EGameXXKCardStatus::TerrainBonusDoubleThisRound:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("本回合地形双效"), TEXT("本回合队伍下一张地形牌的地形条件效果额外结算1次；使用或回合结束时清除。"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 760);
	case EGameXXKCardStatus::Medicine:
		return MakeStyle(TEXT("MedicineHerbs"), TEXT("药效"), TEXT("下一次治疗或治疗反转每层+1；结算时全部消耗。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 540);
	case EGameXXKCardStatus::Weak:
		return MakeStyle(TEXT("WeakBrokenBlade"), TEXT("虚弱"), TEXT("直接攻击伤害降低50%；回合结束减少1层。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 910);
	case EGameXXKCardStatus::Wealth:
		return MakeStyle(TEXT("WealthCoin"), TEXT("财富"), TEXT("钱潮冲击每层伤害+15；散财疗伤最多消耗3层，每层回复6%最大生命。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 550);
	case EGameXXKCardStatus::Rage:
		return MakeStyle(TEXT("RageFlame"), TEXT("狂怒"), TEXT("受到玩家牌的生命伤害时增加1层；怒獠每层伤害+20。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 890);
	case EGameXXKCardStatus::Prey:
		return MakeStyle(TEXT("PreyTargetEye"), TEXT("猎物"), TEXT("老虎锁定的目标；虎扑将攻击该单位。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 900);
	case EGameXXKCardStatus::Charge:
		return MakeStyle(TEXT("ChargeSpiralHorn"), TEXT("蓄力"), TEXT("层数表示剩余蓄力回合；归零后执行已准备的意图。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 880);
	case EGameXXKCardStatus::Counter:
		return MakeStyle(TEXT("CounterHookBlade"), TEXT("反击"), TEXT("敌方单体攻击牌结算后，造成100%攻击并消耗1次。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 870);
	case EGameXXKCardStatus::Block:
		return MakeStyle(TEXT("BlockShield"), TEXT("格挡"), TEXT("敌方单体攻击牌结算后，造成100%攻击＋当前护甲并消耗1次。"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 860);
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
		Stacks,
		*Style.Tooltip);
}
