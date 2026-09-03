#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcBossRebalanceTest
{
	const FGameXXKCardEffect* Effect(const FName CardId, const EGameXXKCardEffectType Type, const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return Card ? Card->Effects.FindByPredicate([Type, Status](const FGameXXKCardEffect& Candidate)
		{
			return Candidate.Type == Type && (Status == EGameXXKCardStatus::None || Candidate.Status == Status);
		}) : nullptr;
	}

	void Expect(FAutomationTestBase& Test, const FName CardId, const EGameXXKCardEffectType Type,
		const int32 Magnitude, const EGameXXKCardMagnitudePolicy Policy, const EGameXXKCardStatus Status = EGameXXKCardStatus::None)
	{
		const FGameXXKCardEffect* Found = Effect(CardId, Type, Status);
		Test.TestNotNull(*CardId.ToString(), Found);
		if (Found)
		{
			Test.TestEqual(TEXT("raw effect magnitude"), Found->Magnitude, Magnitude);
			Test.TestEqual(TEXT("effect policy"), Found->MagnitudePolicy, Policy);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTaskNpcConfirmedDefinitionsTest,
	"GameXXK.Data.TaskNpcCards.Rebalance.ConfirmedDefinitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcConfirmedDefinitionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBossRebalanceTest;
	Expect(*this, TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), EGameXXKCardEffectType::AddArmor, 40, EGameXXKCardMagnitudePolicy::DefensePercent);
	Expect(*this, TEXT("Npc.TusiChief.ShiMenShouShi"), EGameXXKCardEffectType::AddArmor, 80, EGameXXKCardMagnitudePolicy::DefensePercent);
	Expect(*this, TEXT("Npc.TusiChief.TuSiJunLing"), EGameXXKCardEffectType::AddArmor, 40, EGameXXKCardMagnitudePolicy::DefensePercent);
	Expect(*this, TEXT("Npc.TusiChief.MengZhaiShiYue"), EGameXXKCardEffectType::AddArmor, 50, EGameXXKCardMagnitudePolicy::DefensePercent);
	const FGameXXKCardDefinition* Report = FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.SongJinBao.ErMuMiBao"));
	TestEqual(TEXT("report applies group Weak first"), Report->Effects[0].Status, EGameXXKCardStatus::Weak);
	TestEqual(TEXT("report applies group Mark second"), Report->Effects[1].Target, EGameXXKCardEffectTarget::AllEnemies);
	for (const FName Id : {FName(TEXT("Npc.YueBai.CanJuanPiZhu")), FName(TEXT("Npc.YueBai.ShanHeCanTu")), FName(TEXT("Npc.ZhouGuangZu.DiZhiMoTu"))})
		TestEqual(TEXT("terrain-only NPC support requires no target"), FGameXXKCardCatalog::FindCardDefinition(Id)->TargetSpec.Mode, EGameXXKCardTargetMode::None);
	Expect(*this, TEXT("Npc.YueBai.QingYanDianDeng"), EGameXXKCardEffectType::ApplyStatus, 6, EGameXXKCardMagnitudePolicy::DotCoefficient, EGameXXKCardStatus::Burn);
	Expect(*this, TEXT("Npc.YueBai.YueBaiZhaoYe"), EGameXXKCardEffectType::ApplyStatus, 4, EGameXXKCardMagnitudePolicy::DotCoefficient, EGameXXKCardStatus::Burn);
	Expect(*this, TEXT("Npc.YueBai.ShanHeCanTu"), EGameXXKCardEffectType::AddArmor, 40, EGameXXKCardMagnitudePolicy::DefensePercent);
	Expect(*this, TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), EGameXXKCardEffectType::HealOrReverseWithMedicine, 15, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
	Expect(*this, TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), EGameXXKCardEffectType::HealOrReverseWithMedicine, 15, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
	Expect(*this, TEXT("Npc.ZhouGuangZu.YanFenFengMai"), EGameXXKCardEffectType::ApplyStatus, 11, EGameXXKCardMagnitudePolicy::DotCoefficient, EGameXXKCardStatus::Poison);
	Expect(*this, TEXT("Npc.JinGui.QiaoYanZhouXuan"), EGameXXKCardEffectType::AddArmor, 80, EGameXXKCardMagnitudePolicy::DefensePercent);
	Expect(*this, TEXT("Npc.QiongMeiEr.GuWuMiZong"), EGameXXKCardEffectType::ApplyStatus, 4, EGameXXKCardMagnitudePolicy::DotCoefficient, EGameXXKCardStatus::Bleed);
	Expect(*this, TEXT("Npc.QiongMeiEr.GuWuMiZong"), EGameXXKCardEffectType::ApplyStatus, 6, EGameXXKCardMagnitudePolicy::DotCoefficient, EGameXXKCardStatus::Poison);
	Expect(*this, TEXT("Npc.QiongMeiEr.YinLingZhenXin"), EGameXXKCardEffectType::HealOrReverseWithMedicine, 30, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
	Expect(*this, TEXT("Npc.QiongMeiEr.ShanGeHuanLing"), EGameXXKCardEffectType::HealOrReverseWithMedicine, 25, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
	TestEqual(TEXT("correct public name remains 市井耳目"), FGameXXKCardCatalog::FindCardDefinition(TEXT("Npc.JinGui.ShiJingErMu"))->DisplayName.ToString(), FString(TEXT("市井耳目")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBossCardRebalanceTest,
	"GameXXK.Data.TaskNpcCards.Rebalance.BossCards", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBossCardRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcBossRebalanceTest;
	struct FRow { FName Id; int32 EffectiveAttack; int32 ArmorBase; };
	for (const FRow& Row : TArray<FRow>{
		{TEXT("Route.Boss.XiongPiPiJia"), 0, 140}, {TEXT("Route.Boss.HanDiYiShi"), 252, 50},
		{TEXT("Route.Boss.HuPoZhenDan"), 0, 140}, {TEXT("Route.Boss.DuKouLieFeng"), 196, 0},
		{TEXT("Route.Boss.FuHuDuanJiang"), 322, 0}})
	{
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(Row.Id);
		TestNotNull(*Row.Id.ToString(), Card);
		if (!Card) continue;
		TestEqual(TEXT("Boss card is Epic"), Card->BaseQuality, EGameXXKCardQuality::Epic);
		const FGameXXKCardDefinition Epic = FGameXXKCardQualityRules::BuildEffectiveDefinition(*Card, EGameXXKCardQuality::Epic);
		if (Row.EffectiveAttack > 0)
		{
			const FGameXXKCardEffect* Attack = Epic.Effects.FindByPredicate([](const FGameXXKCardEffect& E) { return E.Type == EGameXXKCardEffectType::DamagePercentAttack; });
			TestEqual(TEXT("Boss attack has the approved Epic coefficient"), Attack ? Attack->Magnitude : 0, Row.EffectiveAttack);
		}
		if (Row.ArmorBase > 0) Expect(*this, Row.Id, EGameXXKCardEffectType::AddArmor, Row.ArmorBase, EGameXXKCardMagnitudePolicy::DefensePercent);
	}
	const FGameXXKCardEffect* Vulnerability = Effect(TEXT("Route.Boss.HanDiYiShi"), EGameXXKCardEffectType::ApplyStatus);
	const FGameXXKCardEffect* Cleanse = Effect(TEXT("Route.Boss.HuPoZhenDan"), EGameXXKCardEffectType::CleanseFriendlyDamageOverTime);
	TestNotNull(TEXT("HanDi has Vulnerability"), Vulnerability);
	TestNotNull(TEXT("HuPo has the four-DOT cleanse"), Cleanse);
	if (Vulnerability) TestEqual(TEXT("HanDi Vulnerability is five"), Vulnerability->Magnitude, 5);
	if (Cleanse) TestEqual(TEXT("HuPo clears all four DOT reservoirs"), Cleanse->Magnitude, 1);
	return true;
}

#endif
