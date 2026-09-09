#include "UI/GameXXKBattleStatusIconStyle.h"
#include "GameXXKCardPillText.h"

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
		const FLinearColor& Tint,
		const int32 Priority)
	{
		FGameXXKBattleStatusIconStyle Style;
		Style.IconId = FName(IconId);
		Style.TexturePath = FSoftObjectPath(StatusIconRoot + FString::Printf(TEXT("T_BattleStatus_%s.T_BattleStatus_%s"), IconId, IconId));
		Style.DisplayName = DisplayName;
		Style.Tooltip = GameXXKCardPillText::DescribeTerm(Style.DisplayName);
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
		FLinearColor(0.40f, 0.48f, 0.53f, 1.0f),
		1000);
}

FGameXXKBattleStatusIconStyle FGameXXKBattleStatusIconStyle::ResolveEnemyPhaseIconStyle(const int32 PhaseNumber)
{
	FGameXXKBattleStatusIconStyle Style;
	Style.IconId = FName(*FString::Printf(TEXT("EnemyPhase.%d"), PhaseNumber));
	Style.DisplayName = PhaseNumber == 3 ? TEXT("第三阶段") : TEXT("第二阶段");
	Style.Tooltip = TEXT("生命降至1%时消耗，进入下一阶段并回满生命。");
	Style.Tint = FLinearColor(0.43f, 0.22f, 0.18f, 1.0f);
	Style.Priority = 1400 + PhaseNumber;
	Style.FallbackGlyph = PhaseNumber == 3 ? TEXT("三") : TEXT("二");
	Style.bUsesPaperInkFallback = true;
	return Style;
}

FGameXXKBattleStatusIconStyle FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(const EGameXXKCardStatus Status)
{
	switch (Status)
	{
	case EGameXXKCardStatus::Momentum:
		return MakeStyle(TEXT("MomentumSeal"), TEXT("气势"), FLinearColor(0.57f, 0.42f, 0.27f, 1.0f), 520);
	case EGameXXKCardStatus::Agility:
		return MakeStyle(TEXT("AgilityWing"), TEXT("灵动"), FLinearColor(0.38f, 0.50f, 0.52f, 1.0f), 800);
	case EGameXXKCardStatus::Vulnerability:
		return MakeStyle(TEXT("VulnerabilityMask"), TEXT("破绽"), FLinearColor(0.56f, 0.37f, 0.44f, 1.0f), 920);
	case EGameXXKCardStatus::Bleed:
		return MakeStyle(TEXT("BleedDrop"), TEXT("流血"), FLinearColor(0.55f, 0.31f, 0.29f, 1.0f), 980);
	case EGameXXKCardStatus::Poison:
		return MakeStyle(TEXT("PoisonVial"), TEXT("中毒"), FLinearColor(0.41f, 0.49f, 0.32f, 1.0f), 970);
	case EGameXXKCardStatus::Burn:
		return MakeStyle(TEXT("BurnFlame"), TEXT("灼烧"), FLinearColor(0.65f, 0.39f, 0.30f, 1.0f), 960);
	case EGameXXKCardStatus::Mark:
		return MakeStyle(TEXT("MarkTarget"), TEXT("标记"), FLinearColor(0.55f, 0.33f, 0.34f, 1.0f), 900);
	case EGameXXKCardStatus::Guard:
		return MakeStyle(TEXT("GuardShield"), TEXT("援护"), FLinearColor(0.33f, 0.44f, 0.52f, 1.0f), 810);
	case EGameXXKCardStatus::DamageOverTime:
		return MakeStyle(TEXT("RotSpiral"), TEXT("蚀伤"), FLinearColor(0.40f, 0.35f, 0.50f, 1.0f), 950);
	case EGameXXKCardStatus::CannotReceiveVulnerability:
		return MakeStyle(TEXT("ImmunityTalisman"), TEXT("破绽免疫"), FLinearColor(0.40f, 0.52f, 0.50f, 1.0f), 850);
	case EGameXXKCardStatus::NextAttackBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("追击标记"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 740);
	case EGameXXKCardStatus::NextAttackAppliesVulnerability:
		return MakeStyle(TEXT("TacticSeal"), TEXT("破绽追击"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 750);
	case EGameXXKCardStatus::NextHealingBonus:
		return MakeStyle(TEXT("TacticSeal"), TEXT("疗愈增幅"), FLinearColor(0.43f, 0.37f, 0.51f, 1.0f), 730);
	case EGameXXKCardStatus::TerrainBonusDouble:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地势双效"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 700);
	case EGameXXKCardStatus::NextTerrainCardFree:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地势免耗"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 710);
	case EGameXXKCardStatus::NextTerrainCardEnergyReduction:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("地势减耗"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 720);
	case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("代挡"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 830);
	case EGameXXKCardStatus::TerrainBonusDoubleThisRound:
		return MakeStyle(TEXT("TerrainAndRedirect"), TEXT("本回合地势双效"), FLinearColor(0.44f, 0.52f, 0.38f, 1.0f), 760);
	case EGameXXKCardStatus::Medicine:
		return MakeStyle(TEXT("MedicineHerbs"), TEXT("药效"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 540);
	case EGameXXKCardStatus::Weak:
		return MakeStyle(TEXT("WeakBrokenBlade"), TEXT("虚弱"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 910);
	case EGameXXKCardStatus::Wealth:
		return MakeStyle(TEXT("WealthCoin"), TEXT("财富"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 550);
	case EGameXXKCardStatus::Rage:
		return MakeStyle(TEXT("RageFlame"), TEXT("狂怒"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 890);
	case EGameXXKCardStatus::Prey:
		return MakeStyle(TEXT("PreyTargetEye"), TEXT("猎物"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 900);
	case EGameXXKCardStatus::Charge:
		return MakeStyle(TEXT("ChargeSpiralHorn"), TEXT("蓄力"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 880);
	case EGameXXKCardStatus::Counter:
		return MakeStyle(TEXT("CounterHookBlade"), TEXT("反击"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 870);
	case EGameXXKCardStatus::Block:
		return MakeStyle(TEXT("BlockShield"), TEXT("格挡"), FLinearColor(0.39f, 0.48f, 0.50f, 1.0f), 860);
	case EGameXXKCardStatus::Invalid:
	case EGameXXKCardStatus::None:
	default:
		return MakeFallbackStyle(Status);
	}
}

FString FGameXXKBattleStatusIconStyle::DescribeStatusTooltip(const FGameXXKBattleStatusIconStyle& Style, const int32 Stacks)
{
	const bool bAmount = Style.IconId == TEXT("ArmorShield") || Style.IconId == TEXT("BleedDrop")
		|| Style.IconId == TEXT("PoisonVial") || Style.IconId == TEXT("BurnFlame") || Style.IconId == TEXT("RotSpiral")
		|| Style.IconId == TEXT("MedicineHerbs") || Style.IconId == TEXT("ChargeSpiralHorn");
	const bool bUses = Style.IconId == TEXT("CounterHookBlade") || Style.IconId == TEXT("BlockShield");
	return FString::Printf(
		TEXT("%s\n%s：%d\n%s"),
		*Style.DisplayName,
		bAmount ? TEXT("数值") : bUses ? TEXT("次数") : TEXT("层数"),
		Stacks,
		*Style.Tooltip);
}
