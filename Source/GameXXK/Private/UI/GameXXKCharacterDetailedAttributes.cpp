#include "UI/GameXXKCharacterDetailedAttributes.h"

#include "GameXXKEquipmentBonusRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKResistanceRules.h"
#include "GameXXKCombatGemRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

TArray<FGameXXKCharacterDetailRow> GameXXKCharacterDetailedAttributes::Build(
	const UGameXXKMVPSubsystem* Subsystem, const FName CharacterId)
{
	TArray<FGameXXKCharacterDetailRow> Rows;
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	if (!Subsystem || !Subsystem->GetEquipmentLoadoutSnapshot(CharacterId, Snapshot))
	{
		Rows.Add({TEXT("Unavailable"), TEXT("暂无角色属性"), {}, TEXT("当前没有可用的角色属性。\n\n· 加成来源：暂无"), false, true});
		return Rows;
	}
	const auto Talents = Subsystem->GetTalentProjection();
	const auto& State = Subsystem->GetRuntimeState();
	const auto Percent = [](const double Value) { return FString::Printf(TEXT("+%.2f%%"), Value); };
	const auto RawGem = [&Snapshot](EGameXXKGemType Type) -> int64 { return Snapshot.SocketGemBasisPoints.FindRef(Type); };
	const auto EffectiveGem = [](int64 Raw) { return FGameXXKGemRules::GetEffectiveBonusBasisPoints(Raw) / 100.0; };
	const auto CombinedPercent = [&EffectiveGem](int64 Raw, int32 TalentPercent)
	{
		return ((1.0 + EffectiveGem(Raw) / 100.0) * (1.0 + FMath::Clamp(TalentPercent, 0, 100) / 100.0) - 1.0) * 100.0;
	};
	const auto DamagePercent = [&Talents, &CombinedPercent](int64 Raw)
	{
		return CombinedPercent(Raw, Talents.RouteFinalDamagePercent);
	};

	Rows.Add({TEXT("CriticalChance"), TEXT("暴击率"), FString::Printf(TEXT("%d%%"), FMath::Clamp(Talents.CriticalChancePercent, 0, 20))});
	Rows.Add({TEXT("CriticalDamage"), TEXT("暴击伤害倍率"), FString::Printf(TEXT("%d%%"), 150 + FMath::Clamp(Talents.CriticalDamagePercent, 0, 50))});
	Rows.Add({TEXT("RouteAttack"), TEXT("攻击加成"), Percent(CombinedPercent(RawGem(EGameXXKGemType::AttackPercent), Talents.RouteAttackPercent))});
	Rows.Add({TEXT("RouteDefense"), TEXT("防御加成"), Percent(CombinedPercent(RawGem(EGameXXKGemType::DefensePercent), Talents.RouteDefensePercent))});
	Rows.Add({TEXT("RouteHealth"), TEXT("气血加成"), Percent(CombinedPercent(RawGem(EGameXXKGemType::MaxHealthPercent), Talents.RouteMaxHPPercent))});
	Rows.Add({TEXT("DirectDamage"), TEXT("物理伤害加成"), Percent(DamagePercent(RawGem(EGameXXKGemType::DirectDamage)))});
	for (const auto& Element : TArray<TPair<EGameXXKGemType, FString>>{
		{EGameXXKGemType::FireDamage, TEXT("火焰")}, {EGameXXKGemType::FrostDamage, TEXT("冰霜")}, {EGameXXKGemType::LightningDamage, TEXT("雷击")}})
	{
		Rows.Add({FName(*FString::Printf(TEXT("GemElement.%d"), static_cast<int32>(Element.Key))), Element.Value + TEXT("伤害加成"),
			Percent(DamagePercent(RawGem(Element.Key)))});
	}
	Rows.Add({TEXT("GemCounter"), TEXT("物理反击伤害加成"), Percent(DamagePercent(RawGem(EGameXXKGemType::DirectDamage) + RawGem(EGameXXKGemType::CounterDamage)))});
	Rows.Add({TEXT("GemDot"), TEXT("流血／中毒伤害加成"), Percent(EffectiveGem(RawGem(EGameXXKGemType::DamageOverTime)))});
	Rows.Add({TEXT("GemBurn"), TEXT("灼烧伤害加成"), Percent(EffectiveGem(RawGem(EGameXXKGemType::DamageOverTime) + RawGem(EGameXXKGemType::FireDamage)))});

	FGameXXKEquipmentArmorBonus Armor;
	Armor.GemBasisPoints = RawGem(EGameXXKGemType::ArmorGain);
	for (const auto& Effect : Snapshot.ActivePersonalEffects) Armor.AddEffect(Effect, CharacterId);
	Rows.Add({TEXT("ArmorGain"), TEXT("护甲获得量加成"), Percent(Armor.GetFinalPercent())});
	const auto* RetentionSet = Snapshot.ActivePersonalEffects.FindByPredicate([](const auto& Effect)
		{ return Effect.EffectId == FName(TEXT("Set.XuanJia.4")); });
	Rows.Add({TEXT("FinalArmorRetention"), TEXT("回合开始护甲保留"), FString::Printf(TEXT("%.2f%%"), RetentionSet ? RetentionSet->Magnitude / 100.0 : 0.0)});
	Rows.Add({TEXT("GemHealing"), TEXT("主动治疗加成"), Percent(EffectiveGem(RawGem(EGameXXKGemType::Healing)))});

	FGameXXKCardCombatUnit ResistanceUnit;
	ResistanceUnit.UnitId = CharacterId;
	ResistanceUnit.Side = EGameXXKCardTargetSide::Party;
	if (const auto* Companion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([CharacterId](const auto& C) { return C.InstanceId == CharacterId; }))
		ResistanceUnit.Role = Companion->Role;
	else if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
		ResistanceUnit.Role = EGameXXKCharacterRole::Hero;
	FGameXXKResistanceRules::InitializeUnitProfile(ResistanceUnit);
	ResistanceUnit.GemBonusBasisPoints = Snapshot.SocketGemBasisPoints;
	for (const auto Element : {EGameXXKCardDamageElement::Fire, EGameXXKCardDamageElement::Frost, EGameXXKCardDamageElement::Lightning})
	{
		Rows.Add({FName(*FString::Printf(TEXT("Resistance.%d"), static_cast<int32>(Element))), FGameXXKCombatGemRules::GetElementLabel(Element) + TEXT("抗性"),
			FString::Printf(TEXT("%.2f%%"), FGameXXKResistanceRules::GetEffectiveBasisPoints(ResistanceUnit, Element) / 100.0)});
	}

	const auto Source = [](const FString& Label, const FString& Value) { return TEXT("· ") + Label + TEXT("：") + Value; };
	const auto GemSource = [&Source, &Percent, &EffectiveGem](const FString& Label, int64 Raw)
	{
		return Source(Label, FString::Printf(TEXT("%s（递减后%s）"), *Percent(Raw / 100.0), *Percent(EffectiveGem(Raw))));
	};
	const auto Describe = [&Rows](FName Id, const FString& Rule, const TArray<FString>& Sources)
	{
		if (auto* Row = Rows.FindByPredicate([Id](const auto& Candidate) { return Candidate.Id == Id; }))
			Row->Note = Rule + TEXT("\n\n") + FString::Join(Sources, TEXT("\n"));
	};
	const auto AddInactiveAffix = [&Snapshot, &Source](TArray<FString>& Sources, EGameXXKEquipmentModifierKind Kind, const TCHAR* Label)
	{
		if (Snapshot.SetModifiers.FindRef(Kind) > 0)
			Sources.Add(Source(Label, TEXT("未生效，不计入")));
	};
	Describe(TEXT("CriticalChance"), TEXT("直接攻击的暴击概率，最高20%。本页不计局内临时效果。"),
		{Source(TEXT("基础暴击率"), TEXT("0%")), Source(TEXT("永久天赋"), Percent(FMath::Clamp(Talents.CriticalChancePercent, 0, 20)))});
	Describe(TEXT("CriticalDamage"), TEXT("暴击时使用的伤害倍率。基础150%，天赋最多增加50个百分点。"),
		{Source(TEXT("基础倍率"), TEXT("150%")), Source(TEXT("永久天赋"), FString::Printf(TEXT("+%d个百分点"), FMath::Clamp(Talents.CriticalDamagePercent, 0, 50)))});

	const FName StatRows[] = {TEXT("RouteAttack"), TEXT("RouteDefense"), TEXT("RouteHealth")};
	const EGameXXKGemType StatTypes[] = {EGameXXKGemType::AttackPercent, EGameXXKGemType::DefensePercent, EGameXXKGemType::MaxHealthPercent};
	const int32 StatTalents[] = {Talents.RouteAttackPercent, Talents.RouteDefensePercent, Talents.RouteMaxHPPercent};
	for (int32 I = 0; I < UE_ARRAY_COUNT(StatRows); ++I)
		Describe(StatRows[I], TEXT("宝石作用于角色、装备与固定宝石合计的数值，再乘永久天赋百分比；两类不是直接相加。宝石部分边际递减，上限75%。"),
			{GemSource(TEXT("百分比宝石"), RawGem(StatTypes[I])), Source(TEXT("永久天赋"), Percent(FMath::Clamp(StatTalents[I], 0, 100)))});

	TArray<FString> PhysicalSources = {GemSource(TEXT("物理伤害宝石"), RawGem(EGameXXKGemType::DirectDamage)),
		Source(TEXT("永久终伤天赋"), Percent(FMath::Clamp(Talents.RouteFinalDamagePercent, 0, 100)))};
	AddInactiveAffix(PhysicalSources, EGameXXKEquipmentModifierKind::DirectDamage, TEXT("装备物理伤害词缀"));
	Describe(TEXT("DirectDamage"), TEXT("物理直击先受防御减伤，再由护甲吸收。宝石先按75%上限递减，再乘终伤天赋；暴击和局内临时效果另算。"), PhysicalSources);
	for (const auto& Element : TArray<TPair<EGameXXKGemType, FString>>{
		{EGameXXKGemType::FireDamage, TEXT("火焰")}, {EGameXXKGemType::FrostDamage, TEXT("冰霜")}, {EGameXXKGemType::LightningDamage, TEXT("雷击")}})
	{
		Describe(FName(*FString::Printf(TEXT("GemElement.%d"), static_cast<int32>(Element.Key))),
			TEXT("对应法术直击跳过防御，受同系抗性减伤，护甲仍可吸收。元素宝石按75%上限递减后乘终伤天赋，不叠加物理宝石；暴击另算。"),
			{GemSource(Element.Value + TEXT("伤害宝石"), RawGem(Element.Key)), Source(TEXT("永久终伤天赋"), Percent(FMath::Clamp(Talents.RouteFinalDamagePercent, 0, 100)))});
	}
	TArray<FString> CounterSources = {
		Source(TEXT("物理宝石（名义）"), Percent(RawGem(EGameXXKGemType::DirectDamage) / 100.0)),
		Source(TEXT("反击宝石（名义）"), Percent(RawGem(EGameXXKGemType::CounterDamage) / 100.0)),
		Source(TEXT("宝石合计（递减后）"), Percent(EffectiveGem(RawGem(EGameXXKGemType::DirectDamage) + RawGem(EGameXXKGemType::CounterDamage)))),
		Source(TEXT("永久终伤天赋"), Percent(FMath::Clamp(Talents.RouteFinalDamagePercent, 0, 100)))};
	AddInactiveAffix(CounterSources, EGameXXKEquipmentModifierKind::CounterDamage, TEXT("装备反击词缀"));
	Describe(TEXT("GemCounter"), TEXT("仅放大已触发的物理反击，不增加触发次数。物理与反击宝石合并递减，上限75%，再乘终伤天赋。"), CounterSources);
	Describe(TEXT("GemDot"), TEXT("提高流血、中毒及对应引爆的伤害，不增加层数，不含蚀伤。自然伤害看施加者，主动引爆看触发者；宝石边际递减，上限75%。"),
		{GemSource(TEXT("持续伤害宝石"), RawGem(EGameXXKGemType::DamageOverTime))});
	Describe(TEXT("GemBurn"), TEXT("持续伤害与火焰宝石合并递减，上限75%。灼烧与引燃受火焰抗性减伤，仍直接扣气血。"),
		{Source(TEXT("持续伤害宝石（名义）"), Percent(RawGem(EGameXXKGemType::DamageOverTime) / 100.0)),
		 Source(TEXT("火焰宝石（名义）"), Percent(RawGem(EGameXXKGemType::FireDamage) / 100.0))});

	Describe(TEXT("ArmorGain"), FString::Printf(TEXT("词缀按一半数值计入，与护甲宝石合并后按75%%上限递减，再加固定套装。当前共享部分递减后为%s。"),
		*Percent(FGameXXKEquipmentBonusRules::GetEffectiveAffixBasisPoints(Armor.RawAffixBasisPoints + 2 * Armor.GemBasisPoints) / 100.0)),
		{Source(TEXT("装备词缀"), FString::Printf(TEXT("%s（折半后%s）"), *Percent(Armor.RawAffixBasisPoints / 100.0), *Percent(Armor.RawAffixBasisPoints / 200.0))),
		 Source(TEXT("护甲宝石（名义）"), Percent(Armor.GemBasisPoints / 100.0)), Source(TEXT("固定套装"), Percent(Armor.FixedBasisPoints / 100.0))});
	TArray<FString> RetentionSources = {Source(TEXT("基础保留"), TEXT("0%")),
		Source(TEXT("玄甲四件套"), RetentionSet ? FString::Printf(TEXT("%.2f%%"), RetentionSet->Magnitude / 100.0) : TEXT("未启用"))};
	AddInactiveAffix(RetentionSources, EGameXXKEquipmentModifierKind::ArmorRetention, TEXT("装备护甲保留词缀"));
	Describe(TEXT("FinalArmorRetention"), TEXT("每回合开始保留上回合护甲的比例。不包含本局临时获得的全部保留效果。"), RetentionSources);
	TArray<FString> HealingSources = {GemSource(TEXT("治疗宝石"), RawGem(EGameXXKGemType::Healing))};
	AddInactiveAffix(HealingSources, EGameXXKEquipmentModifierKind::Healing, TEXT("装备治疗词缀"));
	Describe(TEXT("GemHealing"), TEXT("提高主动治疗牌及其重放产生的治疗量；不含吸血、复活和固定套装额外恢复。宝石边际递减，上限75%。"), HealingSources);
	for (const auto Element : {EGameXXKCardDamageElement::Fire, EGameXXKCardDamageElement::Frost, EGameXXKCardDamageElement::Lightning})
	{
		const double Base = ResistanceUnit.InnateResistanceBasisPoints.FindRef(Element) / 100.0;
		const double Final = FGameXXKResistanceRules::GetEffectiveBasisPoints(ResistanceUnit, Element) / 100.0;
		Describe(FName(*FString::Printf(TEXT("Resistance.%d"), static_cast<int32>(Element))),
			TEXT("只减免同系法术伤害。宝石根据基础抗性向75%上限递减，不直接相加；护甲仍能吸收法术直击，战斗中的临时减抗另算。"),
			{Source(TEXT("角色基础抗性"), FString::Printf(TEXT("%.2f%%"), Base)),
			 Source(TEXT("抗性宝石（名义）"), Percent(RawGem(FGameXXKResistanceRules::GemForElement(Element)) / 100.0)),
			 Source(TEXT("宝石实际提升"), FString::Printf(TEXT("+%.2f个百分点"), Final - Base))});
	}
	return Rows;
}
