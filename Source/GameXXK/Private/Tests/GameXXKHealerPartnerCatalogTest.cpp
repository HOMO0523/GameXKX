#include "GameXXKCardCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHealerPartnerCatalogTest
{
	struct FExpectedCard
	{
		const TCHAR* CardId;
		const TCHAR* DisplayName;
		int32 PostFormulaEnergyCost;
		int32 ManaCost;
		EGameXXKCardTargetMode TargetMode;
		bool bCore;
	};

	const TArray<FExpectedCard>& ExpectedCards()
	{
		static const TArray<FExpectedCard> Cards = {
			{TEXT("Profession.Healer.YaoYin"), TEXT("阴阳药引"), 2, 0, EGameXXKCardTargetMode::AnyLivingUnit, true},
			{TEXT("Profession.Healer.XingQiZhen"), TEXT("行气活血"), 2, 3, EGameXXKCardTargetMode::Self, true},

			{TEXT("Profession.Healer.CaoMuFuZhi"), TEXT("草木敷治"), 1, 2, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.QingXinSan"), TEXT("清心散"), 1, 3, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.LingZhiXuMing"), TEXT("灵芝续命"), 2, 6, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.HuiChunLu"), TEXT("回春露"), 2, 5, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.ZhiXueCao"), TEXT("止血草"), 0, 2, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.WenYangGao"), TEXT("温养膏"), 1, 3, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.JinChuangXuMing"), TEXT("金疮续命"), 2, 8, EGameXXKCardTargetMode::AnyLivingUnit, false},
			{TEXT("Profession.Healer.YaoWangGuiYuan"), TEXT("药王归元"), 2, 12, EGameXXKCardTargetMode::AnyLivingUnit, false},

			{TEXT("Profession.Healer.BaiCaoDu"), TEXT("百草毒"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false},
			{TEXT("Profession.Healer.FuGuSan"), TEXT("腐骨散"), 2, 5, EGameXXKCardTargetMode::SingleEnemy, false},
			{TEXT("Profession.Healer.HuiQiXiang"), TEXT("蚀心香"), 1, 4, EGameXXKCardTargetMode::AllEnemies, false},
			{TEXT("Profession.Healer.LianQiaoJieDu"), TEXT("连翘引毒"), 1, 3, EGameXXKCardTargetMode::SingleEnemy, false},
			{TEXT("Profession.Healer.YaoJiuWenShen"), TEXT("红花透骨"), 1, 2, EGameXXKCardTargetMode::SingleEnemy, false},
			{TEXT("Profession.Healer.YaoNangFeiTou"), TEXT("药囊飞投"), 2, 6, EGameXXKCardTargetMode::AllEnemies, false},
			{TEXT("Profession.Healer.KuShenMaSan"), TEXT("苦参麻散"), 2, 4, EGameXXKCardTargetMode::SingleEnemy, false},
			{TEXT("Profession.Healer.WuWeiTiaoHe"), TEXT("五毒调和"), 2, 10, EGameXXKCardTargetMode::AllEnemies, false},
		};
		return Cards;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKHealerPartnerAll18CatalogTest,
	"GameXXK.Data.PartnerCards.Healer.All18Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerPartnerAll18CatalogTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHealerPartnerCatalogTest;
	TSet<FName> SeenIds;
	int32 CoreCount = 0;
	for (const FExpectedCard& Expected : ExpectedCards())
	{
		const FName CardId(Expected.CardId);
		TestFalse(FString::Printf(TEXT("Healer card ID is unique: %s"), Expected.CardId), SeenIds.Contains(CardId));
		SeenIds.Add(CardId);
		const FGameXXKCardDefinition* Actual = FGameXXKCardCatalog::FindCardDefinition(CardId);
		if (!TestNotNull(FString::Printf(TEXT("Healer card exists: %s"), Expected.CardId), Actual))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s display name"), Expected.CardId), Actual->DisplayName.ToString(), FString(Expected.DisplayName));
		TestEqual(FString::Printf(TEXT("%s owner"), Expected.CardId), Actual->Owner, EGameXXKCardOwner::Profession);
		TestEqual(FString::Printf(TEXT("%s role"), Expected.CardId), Actual->Role, EGameXXKCharacterRole::Healer);
		TestEqual(FString::Printf(TEXT("%s owner ID"), Expected.CardId), Actual->OwnerId, FName(TEXT("Profession.Healer")));
		TestEqual(FString::Printf(TEXT("%s post-formula energy"), Expected.CardId), Actual->EnergyCost, Expected.PostFormulaEnergyCost);
		TestEqual(FString::Printf(TEXT("%s mana"), Expected.CardId), Actual->ManaCost, Expected.ManaCost);
		TestEqual(FString::Printf(TEXT("%s target"), Expected.CardId), Actual->TargetSpec.Mode, Expected.TargetMode);
		TestEqual(FString::Printf(TEXT("%s core flag"), Expected.CardId), Actual->bCoreProfessionCard, Expected.bCore);
		CoreCount += Actual->bCoreProfessionCard ? 1 : 0;
	}
	TestEqual(TEXT("the permanent Healer pool remains exactly eighteen stable cards"), SeenIds.Num(), 18);
	TestEqual(TEXT("the permanent Healer pool exposes exactly two fixed core cards"), CoreCount, 2);
	return true;
}

#endif
