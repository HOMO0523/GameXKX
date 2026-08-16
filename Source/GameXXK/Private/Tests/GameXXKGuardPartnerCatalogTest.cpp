#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKGuardPartnerCatalogTest
{
	struct FExpectedEffect
	{
		EGameXXKCardEffectType Type = EGameXXKCardEffectType::Invalid;
		EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;
		int32 Magnitude = 0;
		int32 SecondaryMagnitude = 0;
		EGameXXKCardStatus Status = EGameXXKCardStatus::None;
		EGameXXKCardEffectConditionType ConditionType = EGameXXKCardEffectConditionType::None;
		int32 MinimumArmor = 0;
		bool bNegateCondition = false;
		EGameXXKCardEffectTarget Guardian = EGameXXKCardEffectTarget::Invalid;
		EGameXXKCardEffectTarget ProtectedUnit = EGameXXKCardEffectTarget::Invalid;
		int32 GuardStacks = 0;
	};

	struct FExpectedCard
	{
		const TCHAR* Id = nullptr;
		const TCHAR* DisplayName = nullptr;
		int32 EnergyCost = 0;
		int32 ManaCost = 0;
		EGameXXKCardTargetMode TargetMode = EGameXXKCardTargetMode::Invalid;
		bool bCore = false;
		const TCHAR* ArchetypeId = nullptr;
		TArray<FExpectedEffect> Effects;
	};

	FExpectedEffect Effect(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude = 0,
		const EGameXXKCardStatus Status = EGameXXKCardStatus::None,
		const int32 SecondaryMagnitude = 0)
	{
		FExpectedEffect Result;
		Result.Type = Type;
		Result.Target = Target;
		Result.Magnitude = Magnitude;
		Result.SecondaryMagnitude = SecondaryMagnitude;
		Result.Status = Status;
		return Result;
	}

	FExpectedEffect ArmorConditionEffect(
		const EGameXXKCardEffectType Type,
		const EGameXXKCardEffectTarget Target,
		const int32 Magnitude,
		const int32 MinimumArmor,
		const bool bNegate = false)
	{
		FExpectedEffect Result = Effect(Type, Target, Magnitude);
		Result.ConditionType = EGameXXKCardEffectConditionType::OwnerArmorAtLeast;
		Result.MinimumArmor = MinimumArmor;
		Result.bNegateCondition = bNegate;
		return Result;
	}

	FExpectedEffect Block(const EGameXXKCardEffectTarget Target, const int32 Stacks)
	{
		return Effect(EGameXXKCardEffectType::RegisterReaction, Target, Stacks, EGameXXKCardStatus::Block);
	}

	/** Armor-granting guard cards also taunt: the guard marks itself. */
	FExpectedEffect Mark()
	{
		return Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::Mark);
	}

	FExpectedEffect GuardLink(
		const EGameXXKCardEffectTarget Guardian,
		const EGameXXKCardEffectTarget ProtectedUnit,
		const int32 Stacks)
	{
		FExpectedEffect Result = Effect(EGameXXKCardEffectType::ApplyGuardLink, ProtectedUnit, Stacks);
		Result.Guardian = Guardian;
		Result.ProtectedUnit = ProtectedUnit;
		Result.GuardStacks = Stacks;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGuardPartnerAll18CatalogTest,
	"GameXXK.Data.PartnerCards.Guard.All18Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardPartnerAll18CatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKGuardPartnerCatalogTest;
	const TCHAR* ArmorGrowth = TEXT("Archetype.Guard.ArmorGrowth");
	const TCHAR* Protection = TEXT("Archetype.Guard.Protection");
	const TCHAR* ArmorConversion = TEXT("Archetype.Guard.ArmorConversion");
	const TCHAR* ArmorRelease = TEXT("Archetype.Guard.ArmorRelease");
	const TArray<FExpectedCard> ExpectedCards = {
		{TEXT("Profession.Guard.TieBi"), TEXT("铁壁"), 1, 0, EGameXXKCardTargetMode::Self, true, nullptr,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 14), Block(EGameXXKCardEffectTarget::CardOwner, 1), Mark()}},
		{TEXT("Profession.Guard.HuZhu"), TEXT("护主"), 1, 0, EGameXXKCardTargetMode::SingleAlly, true, nullptr,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 8), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::SelectedTarget, 1), Block(EGameXXKCardEffectTarget::CardOwner, 1), Mark()}},

		{TEXT("Profession.Guard.GuShou"), TEXT("固守"), 0, 0, EGameXXKCardTargetMode::Self, false, ArmorGrowth,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 2), Mark()}},
		{TEXT("Profession.Guard.FanZhenJia"), TEXT("反震甲"), 1, 0, EGameXXKCardTargetMode::Self, false, ArmorGrowth,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 12), Block(EGameXXKCardEffectTarget::CardOwner, 2), Mark()}},
		{TEXT("Profession.Guard.TieBiRuShan"), TEXT("铁壁如山"), 2, 0, EGameXXKCardTargetMode::Self, false, ArmorGrowth,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 24), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::CannotReceiveVulnerability), Block(EGameXXKCardEffectTarget::CardOwner, 2), Mark()}},
		{TEXT("Profession.Guard.BuDongRuShan"), TEXT("不动如山"), 3, 10, EGameXXKCardTargetMode::Self, false, ArmorGrowth,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 36), Block(EGameXXKCardEffectTarget::CardOwner, 3), Effect(EGameXXKCardEffectType::RetainArmorNextRound, EGameXXKCardEffectTarget::CardOwner, 1), Mark()}},

		{TEXT("Profession.Guard.YuanHuBu"), TEXT("援护步"), 0, 0, EGameXXKCardTargetMode::LowestHealthAlly, false, Protection,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::LowestHealthAlly, 6), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::LowestHealthAlly, 1), Block(EGameXXKCardEffectTarget::CardOwner, 1), Mark()}},
		{TEXT("Profession.Guard.YuanJunBiLei"), TEXT("援军壁垒"), 1, 0, EGameXXKCardTargetMode::SingleAlly, false, Protection,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::SelectedTarget, 2), Block(EGameXXKCardEffectTarget::CardOwner, 2), Mark()}},
		{TEXT("Profession.Guard.TieSuoHengJiang"), TEXT("铁锁横江"), 2, 6, EGameXXKCardTargetMode::Self, false, Protection,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 20), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Block(EGameXXKCardEffectTarget::CardOwner, 2), Mark()}},
		{TEXT("Profession.Guard.YiFuDangGuan"), TEXT("一夫当关"), 3, 12, EGameXXKCardTargetMode::AllAllies, false, Protection,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 10), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 20), GuardLink(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardEffectTarget::AllOtherAllies, 2), Block(EGameXXKCardEffectTarget::CardOwner, 3), Mark()}},

		{TEXT("Profession.Guard.ZhenDun"), TEXT("震盾"), 1, 0, EGameXXKCardTargetMode::SingleEnemy, false, ArmorConversion,
			{Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6), Mark()}},
		{TEXT("Profession.Guard.QinWangDunJi"), TEXT("擒王盾击"), 2, 5, EGameXXKCardTargetMode::SingleEnemy, false, ArmorConversion,
			{Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark)}},
		{TEXT("Profession.Guard.DunZhenTuiJin"), TEXT("盾阵推进"), 2, 0, EGameXXKCardTargetMode::SingleEnemy, false, ArmorConversion,
			{Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6), Mark()}},
		{TEXT("Profession.Guard.SuiJiaHuiJi"), TEXT("碎甲回击"), 1, 0, EGameXXKCardTargetMode::SingleEnemy, false, ArmorConversion,
			{Effect(EGameXXKCardEffectType::DamagePercentAttackPlusArmor, EGameXXKCardEffectTarget::SelectedTarget, 100), Block(EGameXXKCardEffectTarget::CardOwner, 1), ArmorConditionEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, 12)}},

		{TEXT("Profession.Guard.PanShiTuNa"), TEXT("磐石吐纳"), 0, 0, EGameXXKCardTargetMode::Self, false, ArmorRelease,
			{ArmorConditionEffect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 5, 8), ArmorConditionEffect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, 8), ArmorConditionEffect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 6, 8, true), Mark()}},
		{TEXT("Profession.Guard.PiJiaXingJun"), TEXT("披甲行军"), 1, 0, EGameXXKCardTargetMode::AllAllies, false, ArmorRelease,
			{Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6), Block(EGameXXKCardEffectTarget::AllAllies, 1), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 6), Mark()}},
		{TEXT("Profession.Guard.ZhenYueLing"), TEXT("镇岳令"), 2, 6, EGameXXKCardTargetMode::AllEnemies, false, ArmorRelease,
			{Effect(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 80, EGameXXKCardStatus::None, 20), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8), Block(EGameXXKCardEffectTarget::AllAllies, 1), Mark()}},
		{TEXT("Profession.Guard.BiLeiFanGong"), TEXT("壁垒反攻"), 2, 6, EGameXXKCardTargetMode::AllEnemies, false, ArmorRelease,
			{Effect(EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor, EGameXXKCardEffectTarget::AllEnemies, 120, EGameXXKCardStatus::None, 20), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::CardOwner, 10), Block(EGameXXKCardEffectTarget::CardOwner, 1), Mark()}},
	};

	int32 GuardCardCount = 0;
	int32 CoreCount = 0;
	int32 SingleConversionCount = 0;
	int32 AllArmorReleaseCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession || Definition.Role != EGameXXKCharacterRole::Guard)
		{
			continue;
		}
		++GuardCardCount;
		CoreCount += Definition.bCoreProfessionCard ? 1 : 0;
		for (const FGameXXKCardEffect& ActualEffect : Definition.Effects)
		{
			TestNotEqual(FString::Printf(TEXT("%s never uses partial-Armor bonus damage"), *Definition.Id.ToString()), ActualEffect.Type, EGameXXKCardEffectType::BonusDamagePercentPerConsumedArmor);
			SingleConversionCount += ActualEffect.Type == EGameXXKCardEffectType::DamagePercentAttackPlusArmor ? 1 : 0;
			AllArmorReleaseCount += ActualEffect.Type == EGameXXKCardEffectType::DamageAllPercentAttackPerConsumedArmor ? 1 : 0;
		}
	}
	TestEqual(TEXT("the Guard partner pool contains exactly eighteen cards"), GuardCardCount, ExpectedCards.Num());
	TestEqual(TEXT("the Guard partner pool contains exactly two core cards"), CoreCount, 2);
	TestEqual(TEXT("exactly four Guard cards convert Attack plus Armor without consuming Armor"), SingleConversionCount, 4);
	TestEqual(TEXT("exactly two Guard cards consume all Armor for an all-enemy multiplier"), AllArmorReleaseCount, 2);

	for (const FExpectedCard& Expected : ExpectedCards)
	{
		const FString Label(Expected.Id);
		const FGameXXKCardDefinition* Actual = FGameXXKCardCatalog::FindCardDefinition(FName(Expected.Id));
		if (!TestNotNull(FString::Printf(TEXT("%s exists"), *Label), Actual))
		{
			continue;
		}
		TestEqual(Label + TEXT(" display name"), Actual->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(Label + TEXT(" owner"), Actual->Owner, EGameXXKCardOwner::Profession);
		TestEqual(Label + TEXT(" rarity"), Actual->Rarity, EGameXXKCardRarity::Permanent);
		TestEqual(Label + TEXT(" role"), Actual->Role, EGameXXKCharacterRole::Guard);
		TestEqual(Label + TEXT(" owner ID"), Actual->OwnerId, FName(TEXT("Profession.Guard")));
		TestEqual(Label + TEXT(" Energy"), Actual->EnergyCost, Expected.EnergyCost);
		TestEqual(Label + TEXT(" Mana"), Actual->ManaCost, Expected.ManaCost);
		TestEqual(Label + TEXT(" target mode"), Actual->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(Label + TEXT(" core flag"), Actual->bCoreProfessionCard, Expected.bCore);
		TestEqual(Label + TEXT(" archetype count"), Actual->ProfessionArchetypeIds.Num(), Expected.ArchetypeId ? 1 : 0);
		if (Expected.ArchetypeId)
		{
			TestTrue(Label + TEXT(" belongs to its approved archetype"), Actual->ProfessionArchetypeIds.Contains(FName(Expected.ArchetypeId)));
		}
		TestEqual(Label + TEXT(" effect count"), Actual->Effects.Num(), Expected.Effects.Num());
		for (int32 EffectIndex = 0; EffectIndex < Actual->Effects.Num() && EffectIndex < Expected.Effects.Num(); ++EffectIndex)
		{
			const FGameXXKCardEffect& ActualEffect = Actual->Effects[EffectIndex];
			const FExpectedEffect& ExpectedEffect = Expected.Effects[EffectIndex];
			const FString Prefix = FString::Printf(TEXT("%s effect %d"), *Label, EffectIndex);
			TestEqual(Prefix + TEXT(" type"), ActualEffect.Type, ExpectedEffect.Type);
			TestEqual(Prefix + TEXT(" target"), ActualEffect.Target, ExpectedEffect.Target);
			TestEqual(Prefix + TEXT(" source"), ActualEffect.Source, EGameXXKCardEffectSource::CardOwner);
			TestEqual(Prefix + TEXT(" magnitude"), ActualEffect.Magnitude, ExpectedEffect.Magnitude);
			TestEqual(Prefix + TEXT(" secondary"), ActualEffect.SecondaryMagnitude, ExpectedEffect.SecondaryMagnitude);
			TestEqual(Prefix + TEXT(" hit count"), ActualEffect.HitCount, 1);
			TestEqual(Prefix + TEXT(" status"), ActualEffect.Status, ExpectedEffect.Status);
			TestEqual(Prefix + TEXT(" condition"), ActualEffect.Condition.Type, ExpectedEffect.ConditionType);
			TestEqual(Prefix + TEXT(" minimum Armor"), ActualEffect.Condition.MinimumArmor, ExpectedEffect.MinimumArmor);
			TestEqual(Prefix + TEXT(" negated condition"), ActualEffect.Condition.bNegate, ExpectedEffect.bNegateCondition);
			TestEqual(Prefix + TEXT(" never consumes partial Armor"), ActualEffect.Condition.bConsumeOwnerArmor, false);
			TestEqual(Prefix + TEXT(" guard guardian"), ActualEffect.GuardLink.Guardian, ExpectedEffect.Guardian);
			TestEqual(Prefix + TEXT(" guard protected unit"), ActualEffect.GuardLink.ProtectedUnit, ExpectedEffect.ProtectedUnit);
			TestEqual(Prefix + TEXT(" guard stacks"), ActualEffect.GuardLink.Stacks, ExpectedEffect.GuardStacks);
		}
	}
	return true;
}

#endif
