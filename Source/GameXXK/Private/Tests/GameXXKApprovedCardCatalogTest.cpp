#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FExpectedHeroCard
	{
		const TCHAR* CardId;
		int32 EnergyCost;
		int32 ManaCost;
		int32 EffectIndex;
		EGameXXKCardEffectType EffectType;
		int32 PrimaryMagnitude;
		EGameXXKCardMagnitudePolicy MagnitudePolicy;
		bool bNestedModifier = false;
	};

	const TArray<FExpectedHeroCard>& GetExpectedHeroCards()
	{
		static const TArray<FExpectedHeroCard> Rows = {
			{TEXT("Hero.Generic.QingFengYiShi"), 1, 0, 0, EGameXXKCardEffectType::DamagePercentAttack, 100, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Generic.HeYuZhan"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 160, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Generic.FengShenBu"), 0, 0, 0, EGameXXKCardEffectType::ApplyStatus, 2, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Generic.SuiYanJi"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 150, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Generic.GuiYuanShu"), 1, 0, 0, EGameXXKCardEffectType::HealOrReverseWithMedicine, 15, EGameXXKCardMagnitudePolicy::MedicineCoefficient},
			{TEXT("Hero.Generic.HengJianShouShi"), 1, 0, 0, EGameXXKCardEffectType::AddArmor, 80, EGameXXKCardMagnitudePolicy::PrintedCostArmor},
			{TEXT("Hero.Generic.NingShenTuNa"), 0, 0, 0, EGameXXKCardEffectType::ApplyStatus, 2, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Generic.GuanXi"), 0, 0, 0, EGameXXKCardEffectType::DrawCards, 3, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Generic.PoYunYiShan"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 160, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Generic.XingQiHuiHuan"), 0, 0, 0, EGameXXKCardEffectType::DrawCards, 2, EGameXXKCardMagnitudePolicy::ExplicitByQuality},
			{TEXT("Hero.Generic.JianYiGuanHong"), 2, 6, 0, EGameXXKCardEffectType::DamagePercentAttack, 260, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Generic.GuiYuanFanZhao"), 2, 6, 0, EGameXXKCardEffectType::HealOrReverseWithMedicine, 15, EGameXXKCardMagnitudePolicy::MedicineCoefficient},

			{TEXT("Hero.Blade.TongFengYinShi"), 1, 0, 1, EGameXXKCardEffectType::ApplyStatus, 2, EGameXXKCardMagnitudePolicy::ExplicitByQuality},
			{TEXT("Hero.Blade.XueLuXiangCheng"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 150, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Blade.YingFengHuanBu"), 1, 0, 0, EGameXXKCardEffectType::ApplyStatus, 2, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Blade.TongPaoJuShi"), 1, 0, 0, EGameXXKCardEffectType::ApplyStatus, 2, EGameXXKCardMagnitudePolicy::Unscaled},

			{TEXT("Hero.Guard.TieBiTongShou"), 1, 0, 0, EGameXXKCardEffectType::AddArmor, 80, EGameXXKCardMagnitudePolicy::PrintedCostArmor},
			{TEXT("Hero.Guard.JieJiaHuanFeng"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttackPlusArmor, 100, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Guard.LieZhenChengFeng"), 2, 0, 0, EGameXXKCardEffectType::AddArmor, 140, EGameXXKCardMagnitudePolicy::PrintedCostArmor},
			{TEXT("Hero.Guard.XuanJiaZhenYue"), 2, 6, 0, EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, 200, EGameXXKCardMagnitudePolicy::ContinuousQuality},

			{TEXT("Hero.Healer.YiXueCuiFang"), 0, 0, 1, EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, 2, EGameXXKCardMagnitudePolicy::ExplicitByQuality},
			{TEXT("Hero.Healer.HuiChunNiMai"), 1, 3, 0, EGameXXKCardEffectType::HealOrReverseWithMedicine, 25, EGameXXKCardMagnitudePolicy::MedicineCoefficient},
			{TEXT("Hero.Healer.DuHuoTongLu"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 130, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Healer.BaiCaoJiZhen"), 2, 6, 0, EGameXXKCardEffectType::HealOrReverseWithMedicine, 20, EGameXXKCardMagnitudePolicy::MedicineCoefficient},

			{TEXT("Hero.Hunter.FengYanDingXian"), 0, 3, 0, EGameXXKCardEffectType::DrawCards, 2, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Hunter.LieYuLianShi"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 140, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Hunter.CuiDuChuanXin"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 130, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Hunter.HuiFengGuanRi"), 1, 6, 0, EGameXXKCardEffectType::DamagePercentAttack, 150, EGameXXKCardMagnitudePolicy::ContinuousQuality},

			{TEXT("Hero.Mage.YanXuLiaoYuan"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 100, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Mage.HanXuNingChuan"), 1, 0, 0, EGameXXKCardEffectType::AddArmor, 40, EGameXXKCardMagnitudePolicy::DefensePercent},
			{TEXT("Hero.Mage.LeiXuYinTing"), 1, 3, 1, EGameXXKCardEffectType::LightningPerTargetStatusSnapshot, 50, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Mage.GuiXuTongXuan"), 0, 0, 0, EGameXXKCardEffectType::DrawCards, 2, EGameXXKCardMagnitudePolicy::Unscaled},

			{TEXT("Hero.Formation.GuanShiLuoZi"), 1, 3, 0, EGameXXKCardEffectType::DamagePercentAttack, 80, EGameXXKCardMagnitudePolicy::ContinuousQuality},
			{TEXT("Hero.Formation.YiZhenHuiXiang"), 1, 3, 0, EGameXXKCardEffectType::TriggerTerrainBenefit, 1, EGameXXKCardMagnitudePolicy::Unscaled},
			{TEXT("Hero.Formation.LianYingBuShi"), 1, 0, 0, EGameXXKCardEffectType::TriggerTerrainBenefit, 2, EGameXXKCardMagnitudePolicy::ExplicitByQuality, true},
			{TEXT("Hero.Formation.LiuHeGuiYi"), 2, 6, 0, EGameXXKCardEffectType::TriggerTerrainBenefit, 1, EGameXXKCardMagnitudePolicy::Unscaled}
		};
		return Rows;
	}

	const FGameXXKCardEffect* FindEffect(
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardEffectType Type,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		return Definition.Effects.FindByPredicate([Type, Status](const FGameXXKCardEffect& Effect)
		{
			return Effect.Type == Type && (Status == EGameXXKCardStatus::None || Effect.Status == Status);
		});
	}

	void TestExplicitEffect(
		FAutomationTestBase& Test,
		const TCHAR* CardId,
		const EGameXXKCardEffectType Type,
		const int32 Base,
		const int32 Rare,
		const int32 Epic,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s exists for explicit quality audit"), CardId), Definition))
		{
			return;
		}
		const FGameXXKCardEffect* Effect = FindEffect(*Definition, Type, Status);
		if (!Test.TestNotNull(FString::Printf(TEXT("%s owns its explicit effect"), CardId), Effect))
		{
			return;
		}
		Test.TestEqual(FString::Printf(TEXT("%s Common value"), CardId), Effect->Magnitude, Base);
		Test.TestEqual(FString::Printf(TEXT("%s Rare value"), CardId), Effect->RareMagnitude, Rare);
		Test.TestEqual(FString::Printf(TEXT("%s Epic value"), CardId), Effect->EpicMagnitude, Epic);
		Test.TestEqual(FString::Printf(TEXT("%s uses explicit quality"), CardId), Effect->MagnitudePolicy, EGameXXKCardMagnitudePolicy::ExplicitByQuality);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKApprovedHeroCardCatalogTest,
	"GameXXK.Data.HeroCards.ApprovedCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKApprovedHeroCardCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FExpectedHeroCard>& ExpectedRows = GetExpectedHeroCards();
	TestEqual(TEXT("approved Hero authority contains exactly thirty-six rows"), ExpectedRows.Num(), 36);
	TSet<FName> ExpectedIds;
	for (const FExpectedHeroCard& Expected : ExpectedRows)
	{
		const FName CardId(Expected.CardId);
		TestFalse(FString::Printf(TEXT("approved Hero CardId is unique: %s"), Expected.CardId), ExpectedIds.Contains(CardId));
		ExpectedIds.Add(CardId);
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("approved Hero card exists: %s"), Expected.CardId), Definition))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s remains Hero-owned"), Expected.CardId), Definition->Owner, EGameXXKCardOwner::Hero);
		TestEqual(FString::Printf(TEXT("%s Energy cost"), Expected.CardId), Definition->EnergyCost, Expected.EnergyCost);
		TestEqual(FString::Printf(TEXT("%s Mana cost"), Expected.CardId), Definition->ManaCost, Expected.ManaCost);
		if (!TestTrue(FString::Printf(TEXT("%s has its keyed effect index"), Expected.CardId), Definition->Effects.IsValidIndex(Expected.EffectIndex)))
		{
			continue;
		}
		const FGameXXKCardEffect& Effect = Definition->Effects[Expected.EffectIndex];
		if (Expected.bNestedModifier)
		{
			TestEqual(FString::Printf(TEXT("%s primary nested type"), Expected.CardId), Effect.Modifier.EffectType, Expected.EffectType);
			TestEqual(FString::Printf(TEXT("%s primary nested magnitude"), Expected.CardId), Effect.Modifier.Magnitude, Expected.PrimaryMagnitude);
			TestEqual(FString::Printf(TEXT("%s primary nested policy"), Expected.CardId), Effect.Modifier.MagnitudePolicy, Expected.MagnitudePolicy);
		}
		else
		{
			TestEqual(FString::Printf(TEXT("%s primary type"), Expected.CardId), Effect.Type, Expected.EffectType);
			TestEqual(FString::Printf(TEXT("%s primary magnitude"), Expected.CardId), Effect.Magnitude, Expected.PrimaryMagnitude);
			TestEqual(FString::Printf(TEXT("%s primary policy"), Expected.CardId), Effect.MagnitudePolicy, Expected.MagnitudePolicy);
		}
	}

	int32 ActualHeroCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Hero)
		{
			continue;
		}
		++ActualHeroCount;
		TestTrue(FString::Printf(TEXT("active Hero card is approved: %s"), *Definition.Id.ToString()), ExpectedIds.Contains(Definition.Id));
		const auto AuditPolicy = [this, &Definition](const FGameXXKCardEffect& Effect, const TCHAR* Context)
		{
			const EGameXXKCardEffectType Type = Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				? Effect.Modifier.EffectType
				: Effect.Type;
			const EGameXXKCardMagnitudePolicy Policy = Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				? Effect.Modifier.MagnitudePolicy
				: Effect.MagnitudePolicy;
			const EGameXXKCardStatus Status = Effect.Type == EGameXXKCardEffectType::ApplyBattleModifier
				? Effect.Modifier.Status
				: Effect.Status;
			const FString Label = FString::Printf(TEXT("%s %s effect %s has its approved policy"),
				*Definition.Id.ToString(), Context, *UEnum::GetValueAsString(Type));
			switch (Type)
			{
			case EGameXXKCardEffectType::DamagePercentAttack:
			case EGameXXKCardEffectType::DamageFlat:
			case EGameXXKCardEffectType::DamagePercentAttackPlusArmor:
			case EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor:
			case EGameXXKCardEffectType::LightningPerTargetStatusSnapshot:
			case EGameXXKCardEffectType::EachLivingAllyAttackSelectedTarget:
				TestEqual(Label, Policy, EGameXXKCardMagnitudePolicy::ContinuousQuality);
				break;
			case EGameXXKCardEffectType::Heal:
			case EGameXXKCardEffectType::HealOrReverseWithMedicine:
				TestEqual(Label, Policy, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
				break;
			case EGameXXKCardEffectType::AddArmor:
				TestTrue(Label, Policy == EGameXXKCardMagnitudePolicy::PrintedCostArmor
					|| Policy == EGameXXKCardMagnitudePolicy::DefensePercent);
				break;
			case EGameXXKCardEffectType::ApplyStatus:
				if (Status == EGameXXKCardStatus::Bleed
					|| Status == EGameXXKCardStatus::Poison
					|| Status == EGameXXKCardStatus::Burn
					|| Status == EGameXXKCardStatus::DamageOverTime)
				{
					TestEqual(Label, Policy, EGameXXKCardMagnitudePolicy::DotCoefficient);
				}
				else
				{
					TestTrue(Label, Policy == EGameXXKCardMagnitudePolicy::Unscaled
						|| Policy == EGameXXKCardMagnitudePolicy::ExplicitByQuality);
				}
				break;
			default:
				TestTrue(Label, Policy == EGameXXKCardMagnitudePolicy::Unscaled
					|| Policy == EGameXXKCardMagnitudePolicy::ExplicitByQuality);
				break;
			}
		};
		for (const FGameXXKCardEffect& Effect : Definition.Effects)
		{
			AuditPolicy(Effect, TEXT("base"));
		}
		for (const FGameXXKCardEffect& Effect : Definition.ChargeEffects)
		{
			AuditPolicy(Effect, TEXT("Charge"));
		}
		for (const FGameXXKCardEffect& Effect : Definition.FinishEffects)
		{
			AuditPolicy(Effect, TEXT("Finish"));
		}
	}
	TestEqual(TEXT("active catalog contains exactly the approved thirty-six Hero cards"), ActualHeroCount, 36);

	TestExplicitEffect(*this, TEXT("Hero.Generic.FengShenBu"), EGameXXKCardEffectType::DrawCards, 2, 3, 4);
	TestExplicitEffect(*this, TEXT("Hero.Generic.XingQiHuiHuan"), EGameXXKCardEffectType::DrawCards, 2, 3, 4);
	TestExplicitEffect(*this, TEXT("Hero.Blade.TongFengYinShi"), EGameXXKCardEffectType::ApplyStatus, 2, 3, 4, EGameXXKCardStatus::Momentum);
	TestExplicitEffect(*this, TEXT("Hero.Healer.YiXueCuiFang"), EGameXXKCardEffectType::GainMedicineFromPartyHealthLoss, 2, 3, 4);
	TestExplicitEffect(*this, TEXT("Hero.Mage.GuiXuTongXuan"), EGameXXKCardEffectType::GainMana, 0, 2, 4);

	const FGameXXKCardDefinition* FengYan = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Hunter.FengYanDingXian"));
	if (TestNotNull(TEXT("FengYanDingXian exists for quality-cost audit"), FengYan))
	{
		TestEqual(TEXT("FengYan Common Mana cost"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FengYan, EGameXXKCardQuality::Common).ManaCost, 3);
		TestEqual(TEXT("FengYan Rare Mana cost"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FengYan, EGameXXKCardQuality::Rare).ManaCost, 2);
		TestEqual(TEXT("FengYan Epic Mana cost"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FengYan, EGameXXKCardQuality::Epic).ManaCost, 1);
	}

	const FGameXXKCardDefinition* LieYu = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Hunter.LieYuLianShi"));
	if (TestNotNull(TEXT("LieYuLianShi exists for Heavy Arrow audit"), LieYu))
	{
		TestEqual(TEXT("LieYu Common Heavy Arrow percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*LieYu, EGameXXKCardQuality::Common).HeavyArrow.MagnitudePerCharge, 50);
		TestEqual(TEXT("LieYu Rare Heavy Arrow percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*LieYu, EGameXXKCardQuality::Rare).HeavyArrow.MagnitudePerCharge, 60);
		TestEqual(TEXT("LieYu Epic Heavy Arrow percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*LieYu, EGameXXKCardQuality::Epic).HeavyArrow.MagnitudePerCharge, 70);
	}
	const FGameXXKCardDefinition* HuiFeng = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Hunter.HuiFengGuanRi"));
	if (TestNotNull(TEXT("HuiFengGuanRi exists for Heavy Arrow audit"), HuiFeng))
	{
		TestEqual(TEXT("HuiFeng Common per-Charge percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*HuiFeng, EGameXXKCardQuality::Common).HeavyArrow.MagnitudePerCharge, 40);
		TestEqual(TEXT("HuiFeng Rare per-Charge percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*HuiFeng, EGameXXKCardQuality::Rare).HeavyArrow.MagnitudePerCharge, 48);
		TestEqual(TEXT("HuiFeng Epic per-Charge percent"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*HuiFeng, EGameXXKCardQuality::Epic).HeavyArrow.MagnitudePerCharge, 56);
	}

	TSet<EGameXXKHealerFormulaKind> HeroFormulaKinds;
	for (const TCHAR* CardId : {
		TEXT("Hero.Healer.YiXueCuiFang"),
		TEXT("Hero.Healer.HuiChunNiMai"),
		TEXT("Hero.Healer.DuHuoTongLu"),
		TEXT("Hero.Healer.BaiCaoJiZhen")})
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("%s exists for Hero formula audit"), CardId), Definition))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s unopened formula surcharge"), CardId), Definition->HealerRule.UnopenedFormulaEnergySurcharge, 1);
		TestNotEqual(FString::Printf(TEXT("%s installs a formula"), CardId), Definition->HealerRule.FormulaKind, EGameXXKHealerFormulaKind::None);
		HeroFormulaKinds.Add(Definition->HealerRule.FormulaKind);
	}
	TestEqual(TEXT("the four Hero medicine cards install four distinct formulas"), HeroFormulaKinds.Num(), 4);

	return true;
}

#endif
