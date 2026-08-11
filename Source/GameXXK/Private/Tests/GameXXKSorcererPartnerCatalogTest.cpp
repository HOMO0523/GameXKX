#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerCatalogTest
{
	struct FExpectedCard
	{
		const TCHAR* CardId = nullptr;
		const TCHAR* DisplayName = nullptr;
		int32 EnergyCost = 0;
		int32 ManaCost = 0;
		EGameXXKCardTargetMode TargetMode = EGameXXKCardTargetMode::Invalid;
		bool bCore = false;
		EGameXXKSorcererCardFamily Family = EGameXXKSorcererCardFamily::None;
		EGameXXKSorcererSequenceRule SequenceRule = EGameXXKSorcererSequenceRule::None;
		EGameXXKSorcererRewardRule RewardRule = EGameXXKSorcererRewardRule::None;
		const TCHAR* ArchetypeId = nullptr;
	};

	const TArray<FExpectedCard>& ExpectedCards()
	{
		static const TArray<FExpectedCard> Cards = {
			{TEXT("Profession.Sorcerer.LingHuoFu"), TEXT("灵枢引法"), 1, 2, EGameXXKCardTargetMode::AllEnemies, true,
				EGameXXKSorcererCardFamily::Core, EGameXXKSorcererSequenceRule::CoreSearch, EGameXXKSorcererRewardRule::CoreSearch},
			{TEXT("Profession.Sorcerer.JuLing"), TEXT("周天归元"), 0, 0, EGameXXKCardTargetMode::Self, true,
				EGameXXKSorcererCardFamily::Core, EGameXXKSorcererSequenceRule::CoreManaEcho, EGameXXKSorcererRewardRule::CoreManaEcho},

			{TEXT("Profession.Sorcerer.LiHuoYin"), TEXT("灵火点灯"), 0, 1, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireLamp, EGameXXKSorcererRewardRule::FireLamp, TEXT("Archetype.Sorcerer.FireSequence")},
			{TEXT("Profession.Sorcerer.YanQiang"), TEXT("流焰传薪"), 0, 2, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireSpread, EGameXXKSorcererRewardRule::FireSpread, TEXT("Archetype.Sorcerer.FireSequence")},
			{TEXT("Profession.Sorcerer.BaoYanShu"), TEXT("焚脉爆炎"), 0, 4, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireBurst, EGameXXKSorcererRewardRule::FireBurst, TEXT("Archetype.Sorcerer.FireSequence")},
			{TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), TEXT("燎原寻诀"), 1, 2, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererSequenceRule::FireSearch, EGameXXKSorcererRewardRule::FireSearch, TEXT("Archetype.Sorcerer.FireSequence")},

			{TEXT("Profession.Sorcerer.SheLingHuo"), TEXT("寒息回流"), 1, 0, EGameXXKCardTargetMode::Self, false,
				EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceCurrentManaRestore, EGameXXKSorcererRewardRule::IceCurrentManaRestore, TEXT("Archetype.Sorcerer.IceSequence")},
			{TEXT("Profession.Sorcerer.FenMaiFu"), TEXT("玄冰拓脉"), 0, 0, EGameXXKCardTargetMode::Self, false,
				EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceMaxMana, EGameXXKSorcererRewardRule::IceMaxMana, TEXT("Archetype.Sorcerer.IceSequence")},
			{TEXT("Profession.Sorcerer.LingYanLianDan"), TEXT("霜镜叠甲"), 0, 0, EGameXXKCardTargetMode::Self, false,
				EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceArmorDouble, EGameXXKSorcererRewardRule::IceArmorDouble, TEXT("Archetype.Sorcerer.IceSequence")},
			{TEXT("Profession.Sorcerer.HuLingMu"), TEXT("冰鉴索法"), 1, 0, EGameXXKCardTargetMode::Self, false,
				EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererSequenceRule::IceSearch, EGameXXKSorcererRewardRule::IceSearch, TEXT("Archetype.Sorcerer.IceSequence")},

			{TEXT("Profession.Sorcerer.ChiXiaoFenXing"), TEXT("引雷定标"), 0, 1, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningMark, EGameXXKSorcererRewardRule::LightningMark, TEXT("Archetype.Sorcerer.LightningSequence")},
			{TEXT("Profession.Sorcerer.FenTianJue"), TEXT("雷符索敌"), 1, 2, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningSearch, EGameXXKSorcererRewardRule::LightningSearch, TEXT("Archetype.Sorcerer.LightningSequence")},
			{TEXT("Profession.Sorcerer.NingYanChengRen"), TEXT("连霆穿云"), 0, 3, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningMarkHits, EGameXXKSorcererRewardRule::LightningMarkHits, TEXT("Archetype.Sorcerer.LightningSequence")},
			{TEXT("Profession.Sorcerer.RanLingHuanYuan"), TEXT("雷走八方"), 0, 4, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererSequenceRule::LightningStorm, EGameXXKSorcererRewardRule::LightningStorm, TEXT("Archetype.Sorcerer.LightningSequence")},

			{TEXT("Profession.Sorcerer.YanMuHuTi"), TEXT("万法归一"), 0, 5, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalScalingAttack, EGameXXKSorcererRewardRule::UniversalScalingAttack, TEXT("Archetype.Sorcerer.GeneralTask")},
			{TEXT("Profession.Sorcerer.LieFu"), TEXT("照见五蕴"), 0, 0, EGameXXKCardTargetMode::Self, false,
				EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalDraw, EGameXXKSorcererRewardRule::UniversalDraw, TEXT("Archetype.Sorcerer.GeneralTask")},
			{TEXT("Profession.Sorcerer.XingHuoHuiShou"), TEXT("六合护法"), 0, 4, EGameXXKCardTargetMode::AllAllies, false,
				EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalPartyArmor, EGameXXKSorcererRewardRule::UniversalPartyArmor, TEXT("Archetype.Sorcerer.GeneralTask")},
			{TEXT("Profession.Sorcerer.ChiYanFengJie"), TEXT("斗转星移"), 0, 2, EGameXXKCardTargetMode::AllEnemies, false,
				EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererSequenceRule::UniversalSearch, EGameXXKSorcererRewardRule::UniversalSearch, TEXT("Archetype.Sorcerer.GeneralTask")},
		};
		return Cards;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerAll18CatalogTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Catalog.All18",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerAll18CatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerCatalogTest;

	int32 ActualCount = 0;
	int32 CoreCount = 0;
	int32 FireCount = 0;
	int32 IceCount = 0;
	int32 LightningCount = 0;
	int32 UniversalCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Profession
			|| Definition.Role != EGameXXKCharacterRole::Sorcerer)
		{
			continue;
		}
		++ActualCount;
		CoreCount += Definition.bCoreProfessionCard ? 1 : 0;
		switch (Definition.SorcererRule.Family)
		{
		case EGameXXKSorcererCardFamily::Fire:
			++FireCount;
			break;
		case EGameXXKSorcererCardFamily::Ice:
			++IceCount;
			break;
		case EGameXXKSorcererCardFamily::Lightning:
			++LightningCount;
			break;
		case EGameXXKSorcererCardFamily::Universal:
			++UniversalCount;
			break;
		default:
			break;
		}
	}

	TestEqual(TEXT("exact Sorcerer count"), ActualCount, 18);
	TestEqual(TEXT("exact Sorcerer core count"), CoreCount, 2);
	TestEqual(TEXT("four Fire cards"), FireCount, 4);
	TestEqual(TEXT("four Ice cards"), IceCount, 4);
	TestEqual(TEXT("four Lightning cards"), LightningCount, 4);
	TestEqual(TEXT("four Universal cards"), UniversalCount, 4);

	TSet<FName> SeenIds;
	for (const FExpectedCard& Expected : ExpectedCards())
	{
		const FName CardId(Expected.CardId);
		TestFalse(FString::Printf(TEXT("stable CardId is unique: %s"), Expected.CardId), SeenIds.Contains(CardId));
		SeenIds.Add(CardId);
		const FGameXXKCardDefinition* Actual = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("Sorcerer card exists: %s"), Expected.CardId), Actual))
		{
			continue;
		}

		const FString Prefix(Expected.CardId);
		TestEqual(Prefix + TEXT(" display name"), Actual->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(Prefix + TEXT(" owner"), Actual->Owner, EGameXXKCardOwner::Profession);
		TestEqual(Prefix + TEXT(" rarity"), Actual->Rarity, EGameXXKCardRarity::Permanent);
		TestEqual(Prefix + TEXT(" role"), Actual->Role, EGameXXKCharacterRole::Sorcerer);
		TestEqual(Prefix + TEXT(" owner id"), Actual->OwnerId, FName(TEXT("Profession.Sorcerer")));
		TestEqual(Prefix + TEXT(" Energy"), Actual->EnergyCost, Expected.EnergyCost);
		TestEqual(Prefix + TEXT(" Mana"), Actual->ManaCost, Expected.ManaCost);
		TestEqual(Prefix + TEXT(" target mode"), Actual->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(Prefix + TEXT(" core flag"), Actual->bCoreProfessionCard, Expected.bCore);
		TestEqual(Prefix + TEXT(" family"), Actual->SorcererRule.Family, Expected.Family);
		TestEqual(Prefix + TEXT(" sequence rule"), Actual->SorcererRule.SequenceRule, Expected.SequenceRule);
		TestEqual(Prefix + TEXT(" reward rule"), Actual->SorcererRule.RewardRule, Expected.RewardRule);
		TestEqual(Prefix + TEXT(" archetype count"), Actual->ProfessionArchetypeIds.Num(), Expected.ArchetypeId ? 1 : 0);
		if (Expected.ArchetypeId)
		{
			TestTrue(Prefix + TEXT(" approved archetype"), Actual->ProfessionArchetypeIds.Contains(FName(Expected.ArchetypeId)));
		}

		if (Expected.Family == EGameXXKSorcererCardFamily::Ice)
		{
			TestFalse(Prefix + TEXT(" Ice base has no direct damage"), Actual->Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Type == EGameXXKCardEffectType::DamagePercentAttack
					|| Effect.Type == EGameXXKCardEffectType::DamageFlat
					|| Effect.Type == EGameXXKCardEffectType::LightningPerTargetStatusSnapshot;
			}));
		}
		if (Actual->TargetSpec.Mode == EGameXXKCardTargetMode::AllEnemies)
		{
			TestFalse(Prefix + TEXT(" enemy-group card has no selected-enemy effect"), Actual->Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Target == EGameXXKCardEffectTarget::SelectedTarget
					|| Effect.Target == EGameXXKCardEffectTarget::SelectedTargetSide;
			}));
		}
	}

	TestEqual(TEXT("the expected table contains exactly eighteen stable CardIds"), SeenIds.Num(), 18);
	return true;
}

#endif
