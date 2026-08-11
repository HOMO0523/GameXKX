#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBladePartnerCatalogTest,
	"GameXXK.Data.PartnerCards.BladeCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	struct FExpectedEffect
	{
		EGameXXKCardEffectType Type = EGameXXKCardEffectType::Invalid;
		EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::Invalid;
		int32 Magnitude = 0;
		int32 HitCount = 1;
		EGameXXKCardStatus Status = EGameXXKCardStatus::None;
	};

	struct FExpectedBladeCard
	{
		const TCHAR* Id = nullptr;
		const TCHAR* DisplayName = nullptr;
		int32 EnergyCost = 0;
		int32 ManaCost = 0;
		EGameXXKCardTargetMode TargetMode = EGameXXKCardTargetMode::Invalid;
		bool bCore = false;
		const TCHAR* ArchetypeId = nullptr;
		EGameXXKBladeBaseRule BaseRule = EGameXXKBladeBaseRule::None;
		EGameXXKBladeChargeRule ChargeRule = EGameXXKBladeChargeRule::None;
		EGameXXKBladeFinishRule FinishRule = EGameXXKBladeFinishRule::None;
		TArray<FExpectedEffect> BaseEffects;
	};

	FExpectedEffect Attack(
		const int32 Percent,
		const EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::SelectedTarget,
		const int32 HitCount = 1)
	{
		return { EGameXXKCardEffectType::DamagePercentAttack, Target, Percent, HitCount, EGameXXKCardStatus::None };
	}

	FExpectedEffect Status(
		const EGameXXKCardEffectTarget Target,
		const EGameXXKCardStatus Status,
		const int32 Stacks)
	{
		return { EGameXXKCardEffectType::ApplyStatus, Target, Stacks, 1, Status };
	}

	FExpectedEffect Mana(const int32 Amount)
	{
		return { EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, Amount, 1, EGameXXKCardStatus::None };
	}

	FExpectedEffect Reaction(const EGameXXKCardEffectTarget Target, const int32 Stacks)
	{
		return { EGameXXKCardEffectType::RegisterReaction, Target, Stacks, 1, EGameXXKCardStatus::Counter };
	}
}

bool FGameXXKBladePartnerCatalogTest::RunTest(const FString& Parameters)
{
	const TCHAR* BloodEdge = TEXT("Archetype.Blade.BloodEdge");
	const TCHAR* MomentumBreak = TEXT("Archetype.Blade.MomentumBreak");
	const TCHAR* Counterflow = TEXT("Archetype.Blade.Counterflow");
	const TCHAR* Sheathed = TEXT("Archetype.Blade.Sheathed");

	const TArray<FExpectedBladeCard> ExpectedCards = {
		{ TEXT("Profession.Blade.LieFengZhan"), TEXT("裂风斩"), 1, 0, EGameXXKCardTargetMode::SingleEnemy, true, nullptr,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReplayNextActiveBase, EGameXXKBladeFinishRule::ReturnFirstActiveNextRound,
			{ Attack(100), Status(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Bleed, 1) } },
		{ TEXT("Profession.Blade.HuiFengJiaShi"), TEXT("回锋架势"), 1, 0, EGameXXKCardTargetMode::Self, true, nullptr,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::CopyNextActiveToHand, EGameXXKBladeFinishRule::MarkAndPrepareTwoCounters,
			{ Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Agility, 1) } },

		{ TEXT("Profession.Blade.FengHou"), TEXT("封喉"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, BloodEdge,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReturnNextActiveToHandOnce, EGameXXKBladeFinishRule::PreserveFirstTwoBleedTriggers,
			{ Attack(100), Status(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Bleed, 5) } },
		{ TEXT("Profession.Blade.JiYuLianZhan"), TEXT("疾雨连斩"), 2, 5, EGameXXKCardTargetMode::SingleEnemy, false, BloodEdge,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::ReplayNextActiveNextRound, EGameXXKBladeFinishRule::DrawOnFirstThreeBleedTriggers,
			{ Status(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Bleed, 3), Attack(55, EGameXXKCardEffectTarget::SelectedTarget, 3) } },
		{ TEXT("Profession.Blade.YinXueDao"), TEXT("饮血刀"), 2, 4, EGameXXKCardTargetMode::SingleEnemy, false, BloodEdge,
			EGameXXKBladeBaseRule::HealFromTriggeredBleed, EGameXXKBladeChargeRule::RestoreNextActiveOwnerState, EGameXXKBladeFinishRule::HealBladeBleedCapTwelve,
			{ Attack(120), Status(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Bleed, 2) } },
		{ TEXT("Profession.Blade.LangDuan"), TEXT("浪断"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, false, BloodEdge,
			EGameXXKBladeBaseRule::PreserveTriggeredBleed, EGameXXKBladeChargeRule::DuplicateNextSingleTargetOrDraw, EGameXXKBladeFinishRule::ReturnFirstActiveAgainstBleeding,
			{ Attack(100) } },

		{ TEXT("Profession.Blade.DuanYue"), TEXT("断岳"), 2, 5, EGameXXKCardTargetMode::SingleEnemy, false, MomentumBreak,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::MakeNextActiveEnergyFree, EGameXXKBladeFinishRule::FreezeVulnerabilityAndReplay,
			{ Attack(140), Status(EGameXXKCardEffectTarget::SelectedTarget, EGameXXKCardStatus::Vulnerability, 3), Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Momentum, 1) } },
		{ TEXT("Profession.Blade.PoJun"), TEXT("破军"), 2, 6, EGameXXKCardTargetMode::SingleEnemy, false, MomentumBreak,
			EGameXXKBladeBaseRule::ConsumeVulnerabilityForExtraAttacks, EGameXXKBladeChargeRule::MakeNextActiveManaFree, EGameXXKBladeFinishRule::CopyFirstStatusConsumer,
			{ Attack(130) } },
		{ TEXT("Profession.Blade.ZhanYiFeiTeng"), TEXT("战意沸腾"), 1, 0, EGameXXKCardTargetMode::Self, false, MomentumBreak,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RefundNextActiveCosts, EGameXXKBladeFinishRule::RefundFirstHighCostAndDrawTwo,
			{ Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Momentum, 2), Mana(4) } },
		{ TEXT("Profession.Blade.ZhanJin"), TEXT("斩尽"), 3, 12, EGameXXKCardTargetMode::SingleEnemy, false, MomentumBreak,
			EGameXXKBladeBaseRule::RefundCostsAndDrawOnKill, EGameXXKBladeChargeRule::CountNextActiveTwice, EGameXXKBladeFinishRule::CopyFirstKill,
			{ Attack(200) } },

		{ TEXT("Profession.Blade.JieShiHuiFeng"), TEXT("借势回锋"), 1, 0, EGameXXKCardTargetMode::Self, false, Counterflow,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::CopyNextActiveNextRound, EGameXXKBladeFinishRule::MarkAndReregisterCounterVolley,
			{ Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Agility, 1), Reaction(EGameXXKCardEffectTarget::CardOwner, 1) } },
		{ TEXT("Profession.Blade.ZhuYing"), TEXT("逐影"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false, Counterflow,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RetainNextActiveNextRound, EGameXXKBladeFinishRule::FirstTwoDodgesFree,
			{ Attack(90), Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Agility, 2), Reaction(EGameXXKCardEffectTarget::CardOwner, 1) } },
		{ TEXT("Profession.Blade.PoLangTuJin"), TEXT("破浪突进"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, false, Counterflow,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::PreserveFinishCandidate, EGameXXKBladeFinishRule::TransferMarkBeforeCounter,
			{ Attack(110), Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Mark, 2), Reaction(EGameXXKCardEffectTarget::CardOwner, 1) } },
		{ TEXT("Profession.Blade.YiShiDuanJiang"), TEXT("一式断江"), 2, 7, EGameXXKCardTargetMode::SingleEnemy, false, Counterflow,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::RetainRemainingHand, EGameXXKBladeFinishRule::FirstCounterVolleyHitsAll,
			{ Attack(160), Reaction(EGameXXKCardEffectTarget::CardOwner, 1) } },

		{ TEXT("Profession.Blade.JingHongChuQiao"), TEXT("惊鸿出鞘"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, false, Sheathed,
			EGameXXKBladeBaseRule::OpenBladeExtraAttack, EGameXXKBladeChargeRule::LightLoad, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle,
			{ Attack(90) } },
		{ TEXT("Profession.Blade.HengYunKaiFeng"), TEXT("横云开锋"), 2, 6, EGameXXKCardTargetMode::AllEnemies, false, Sheathed,
			EGameXXKBladeBaseRule::OpenBladeResidualStyle, EGameXXKBladeChargeRule::DrawTwoAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle,
			{ Attack(70, EGameXXKCardEffectTarget::AllEnemies) } },
		{ TEXT("Profession.Blade.LianXiGuiQiao"), TEXT("敛息归鞘"), 0, 0, EGameXXKCardTargetMode::Self, false, Sheathed,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::DrawSameOwnerAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle,
			{ Mana(3) } },
		{ TEXT("Profession.Blade.BaoDaoShouYe"), TEXT("抱刀守夜"), 1, 0, EGameXXKCardTargetMode::Self, false, Sheathed,
			EGameXXKBladeBaseRule::None, EGameXXKBladeChargeRule::DrawOtherOwnerAfterNextActive, EGameXXKBladeFinishRule::StoreChargeAsNativeStyle,
			{ Status(EGameXXKCardEffectTarget::CardOwner, EGameXXKCardStatus::Agility, 2) } },
	};

	int32 BladeProfessionCardCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::Profession && Definition.Role == EGameXXKCharacterRole::Blade)
		{
			++BladeProfessionCardCount;
		}
	}
	TestEqual(TEXT("Blade partner catalog keeps exactly eighteen cards"), BladeProfessionCardCount, ExpectedCards.Num());

	for (const FExpectedBladeCard& Expected : ExpectedCards)
	{
		const FString CardLabel(Expected.Id);
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(FName(Expected.Id));
		TestNotNull(FString::Printf(TEXT("%s exists"), *CardLabel), Definition);
		if (!Definition)
		{
			continue;
		}

		TestEqual(FString::Printf(TEXT("%s name"), *CardLabel), Definition->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(FString::Printf(TEXT("%s owner"), *CardLabel), Definition->Owner, EGameXXKCardOwner::Profession);
		TestEqual(FString::Printf(TEXT("%s rarity"), *CardLabel), Definition->Rarity, EGameXXKCardRarity::Permanent);
		TestEqual(FString::Printf(TEXT("%s role"), *CardLabel), Definition->Role, EGameXXKCharacterRole::Blade);
		TestEqual(FString::Printf(TEXT("%s owner id"), *CardLabel), Definition->OwnerId, FName(TEXT("Profession.Blade")));
		TestEqual(FString::Printf(TEXT("%s energy cost"), *CardLabel), Definition->EnergyCost, Expected.EnergyCost);
		TestEqual(FString::Printf(TEXT("%s mana cost"), *CardLabel), Definition->ManaCost, Expected.ManaCost);
		TestEqual(FString::Printf(TEXT("%s target mode"), *CardLabel), Definition->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(FString::Printf(TEXT("%s core flag"), *CardLabel), Definition->bCoreProfessionCard, Expected.bCore);
		TestEqual(FString::Printf(TEXT("%s base Blade rule"), *CardLabel), Definition->BladeSequence.BaseRule, Expected.BaseRule);
		TestEqual(FString::Printf(TEXT("%s Charge rule"), *CardLabel), Definition->BladeSequence.ChargeRule, Expected.ChargeRule);
		TestEqual(FString::Printf(TEXT("%s Finish rule"), *CardLabel), Definition->BladeSequence.FinishRule, Expected.FinishRule);

		const int32 ExpectedArchetypeCount = Expected.ArchetypeId ? 1 : 0;
		TestEqual(FString::Printf(TEXT("%s archetype count"), *CardLabel), Definition->ProfessionArchetypeIds.Num(), ExpectedArchetypeCount);
		if (Expected.ArchetypeId)
		{
			TestTrue(
				FString::Printf(TEXT("%s belongs to %s"), *CardLabel, Expected.ArchetypeId),
				Definition->ProfessionArchetypeIds.Contains(FName(Expected.ArchetypeId)));
		}

		TestEqual(FString::Printf(TEXT("%s base effect count"), *CardLabel), Definition->Effects.Num(), Expected.BaseEffects.Num());
		const int32 ComparableEffectCount = FMath::Min(Definition->Effects.Num(), Expected.BaseEffects.Num());
		for (int32 EffectIndex = 0; EffectIndex < ComparableEffectCount; ++EffectIndex)
		{
			const FGameXXKCardEffect& ActualEffect = Definition->Effects[EffectIndex];
			const FExpectedEffect& ExpectedEffect = Expected.BaseEffects[EffectIndex];
			const FString EffectLabel = FString::Printf(TEXT("%s base effect %d"), *CardLabel, EffectIndex);
			TestEqual(EffectLabel + TEXT(" type"), ActualEffect.Type, ExpectedEffect.Type);
			TestEqual(EffectLabel + TEXT(" target"), ActualEffect.Target, ExpectedEffect.Target);
			TestEqual(EffectLabel + TEXT(" magnitude"), ActualEffect.Magnitude, ExpectedEffect.Magnitude);
			TestEqual(EffectLabel + TEXT(" hit count"), ActualEffect.HitCount, ExpectedEffect.HitCount);
			TestEqual(EffectLabel + TEXT(" status"), ActualEffect.Status, ExpectedEffect.Status);
		}

	}

	const TArray<FName> RetiredBladeIds = {
		TEXT("Profession.Blade.YiShangHuanShi"),
		TEXT("Profession.Blade.DaoYiShouShu"),
		TEXT("Profession.Blade.XiaoJiaLianJi"),
		TEXT("Profession.Blade.CanYueSanDie"),
	};
	for (const FName RetiredId : RetiredBladeIds)
	{
		TestNull(
			FString::Printf(TEXT("retired Blade card %s is absent from the current catalog"), *RetiredId.ToString()),
			FGameXXKCardCatalog::FindCardDefinition(RetiredId));
	}

	const TArray<TPair<FName, EGameXXKCardQuality>> ReplacementQualities = {
		{ TEXT("Profession.Blade.JingHongChuQiao"), EGameXXKCardQuality::Common },
		{ TEXT("Profession.Blade.HengYunKaiFeng"), EGameXXKCardQuality::Rare },
		{ TEXT("Profession.Blade.LianXiGuiQiao"), EGameXXKCardQuality::Rare },
		{ TEXT("Profession.Blade.BaoDaoShouYe"), EGameXXKCardQuality::Epic },
	};
	for (const TPair<FName, EGameXXKCardQuality>& ExpectedQuality : ReplacementQualities)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(ExpectedQuality.Key);
		if (TestNotNull(FString::Printf(TEXT("replacement quality card %s exists"), *ExpectedQuality.Key.ToString()), Definition))
		{
			TestEqual(
				FString::Printf(TEXT("replacement card %s inherits its compatibility quality"), *ExpectedQuality.Key.ToString()),
				Definition->BaseQuality,
				ExpectedQuality.Value);
		}
	}

	return true;
}

#endif
