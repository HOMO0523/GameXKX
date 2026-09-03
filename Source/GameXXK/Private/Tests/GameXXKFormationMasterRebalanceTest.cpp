#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKFormationMasterRebalanceTest
{
	const TArray<FName> LevelOne = {
		TEXT("Profession.FormationMaster.GuanShi"), TEXT("Profession.FormationMaster.CunZhaiYuanZhen"),
		TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("Profession.FormationMaster.DingZhen"),
		TEXT("Profession.FormationMaster.YiWeiZhen"), TEXT("Profession.FormationMaster.YinShuiHuiYuan"),
		TEXT("Profession.FormationMaster.ShanMenFengSuo"), TEXT("Profession.FormationMaster.KunZhen"),
		TEXT("Profession.FormationMaster.LinFengFuZhen"), TEXT("Profession.FormationMaster.LinYingMiZong"),
		TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("Profession.FormationMaster.JieShanWeiZhang")};
	const TArray<FName> LevelFive = {TEXT("Profession.FormationMaster.BaMenLunZhuan"), TEXT("Profession.FormationMaster.ShuiJingZheGuang")};
	const TArray<FName> LevelTen = {TEXT("Profession.FormationMaster.DiMaiJieLi"), TEXT("Profession.FormationMaster.SiXiangLianHuan")};
	const TArray<FName> LevelFifteen = {TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("Profession.FormationMaster.WanXiangGuiZhen")};

	FGameXXKCardCombatUnit MakeUnit(const FName Id, const EGameXXKCardTargetSide Side, const EGameXXKCharacterRole Role, const int32 Order)
	{
		FGameXXKCardCombatUnit Result;
		Result.UnitId = Id;
		Result.Side = Side;
		Result.Role = Role;
		Result.bLiving = true;
		Result.HP = Result.MaxHP = Side == EGameXXKCardTargetSide::Party ? 500 : 100000;
		Result.Mana = Result.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Result.Attack = 100;
		Result.Defense = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Result.Speed = 1;
		Result.StableSortOrder = Order;
		Result.CombatLevel = 100;
		return Result;
	}

	bool BuildRuntime(FAutomationTestBase& Test, const FName CardId, const EGameXXKCardQuality Quality, FGameXXKCardBattleRuntime& Out)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Formation.Rebalance.%d"), Index));
			Card.CardId = CardId;
			Card.CurrentQuality = Quality;
			Card.OwnerUnitId = TEXT("Formation");
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Formation.Rebalance.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		FString Error;
		const bool bOk = GameXXKCardRules::InitializeCardBattleRuntime(Out, Cards,
			{MakeUnit(TEXT("Formation"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 1),
			 MakeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
			 MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)},
			EGameXXKCardTerrain::Plain, 609042, &Error);
		Test.TestTrue(TEXT("formation runtime initializes: ") + Error, bOk);
		if (bOk) Out.Deck.SharedEnergy = 20;
		return bOk;
	}

	bool Play(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const FName Target, FGameXXKCardPlayResult& Result)
	{
		FString Error;
		const bool bOk = GameXXKCardRules::ResolveCardPlay(Runtime, Runtime.Deck.Hand[0].InstanceId, Target, Result, &Error);
		Test.TestTrue(TEXT("formation card resolves: ") + Error, bOk);
		return bOk;
	}

	const FGameXXKCardEffect* Effect(const FName CardId, const EGameXXKCardEffectType Type)
	{
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return Card ? Card->Effects.FindByPredicate([Type](const FGameXXKCardEffect& Candidate) { return Candidate.Type == Type; }) : nullptr;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFormationMasterUnlocksTest,
	"GameXXK.Data.PartnerCards.Formation.Rebalance.Unlocks", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterUnlocksTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterRebalanceTest;
	const FGameXXKCompanionTemplateDefinition* Template = FGameXXKCompanionCatalog::GetRecruitTemplates().FindByPredicate([](const FGameXXKCompanionTemplateDefinition& Row)
	{
		return Row.Role == EGameXXKCharacterRole::FormationMaster;
	});
	TestNotNull(TEXT("formation recruit template exists"), Template);
	if (!Template) return false;
	FGameXXKCompanionRosterState Roster;
	FGameXXKCompanionRecruitResult Recruit;
	TestTrue(TEXT("formation companion recruits"), FGameXXKCompanionRules::RecruitPermanentCompanion(Roster, Template->TemplateId, 50420, Recruit, nullptr));
	FGameXXKPermanentCompanion& Companion = Roster.PermanentCompanions[0];
	TestEqual(TEXT("formation full pool contains eighteen cards"), Companion.PersonalCardIds.Num(), 18);
	const auto CheckLevel = [this, &Companion](const int32 Level, const TArray<FName>& Expected)
	{
		Companion.Level = Level;
		Companion.Experience = 0;
		TestTrue(TEXT("level refresh succeeds"), FGameXXKCompanionRules::AwardExperience(Companion, 0, nullptr));
		TestEqual(*FString::Printf(TEXT("level %d unlocked cards"), Level), Companion.UnlockedPersonalCardIds, Expected);
	};
	TArray<FName> Expected = LevelOne;
	CheckLevel(1, Expected);
	Expected.Append(LevelFive); CheckLevel(5, Expected);
	Expected.Append(LevelTen); CheckLevel(10, Expected);
	Expected.Append(LevelFifteen); CheckLevel(15, Expected);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFormationMasterDefinitionsTest,
	"GameXXK.Data.PartnerCards.Formation.Rebalance.Definitions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterDefinitionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterRebalanceTest;
	for (const FName Id : {FName(TEXT("Profession.FormationMaster.GuanShi")), FName(TEXT("Profession.FormationMaster.DingZhen")),
		FName(TEXT("Profession.FormationMaster.YinShuiHuiYuan")), FName(TEXT("Profession.FormationMaster.KunZhen")),
		FName(TEXT("Profession.FormationMaster.LinYingMiZong")), FName(TEXT("Profession.FormationMaster.JieShanWeiZhang"))})
	{
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(Id);
		TestNotNull(TEXT("switch exists"), Card);
		if (!Card) continue;
		TestEqual(TEXT("switch costs one Energy"), Card->EnergyCost, 1);
		TestEqual(TEXT("switch needs no target"), Card->TargetSpec.Mode, EGameXXKCardTargetMode::None);
		TestEqual(TEXT("switch has change, one benefit, and quality Mana"), Card->Effects.Num(), 3);
		const FGameXXKCardDefinition Rare = FGameXXKCardQualityRules::BuildEffectiveDefinition(*Card, EGameXXKCardQuality::Rare);
		const FGameXXKCardDefinition Epic = FGameXXKCardQualityRules::BuildEffectiveDefinition(*Card, EGameXXKCardQuality::Epic);
		TestEqual(TEXT("Rare switch restores two Mana"), Rare.Effects.Last().Magnitude, 2);
		TestEqual(TEXT("Epic switch restores four Mana"), Epic.Effects.Last().Magnitude, 4);
	}
	struct FRow { FName Id; EGameXXKCardEffectType Type; int32 Magnitude; EGameXXKCardMagnitudePolicy Policy; int32 Secondary; };
	const TArray<FRow> Rows = {
		{TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), EGameXXKCardEffectType::HealOrReverseWithMedicine, 20, EGameXXKCardMagnitudePolicy::MedicineCoefficient, 0},
		{TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), EGameXXKCardEffectType::AddArmor, 125, EGameXXKCardMagnitudePolicy::DefensePercent, 3},
		{TEXT("Profession.FormationMaster.HuiShengZhenSha"), EGameXXKCardEffectType::DamagePercentAttack, 240, EGameXXKCardMagnitudePolicy::ContinuousQuality, 0},
		{TEXT("Profession.FormationMaster.ZhenShaZhen"), EGameXXKCardEffectType::DamagePercentAttack, 320, EGameXXKCardMagnitudePolicy::ContinuousQuality, 0},
		{TEXT("Profession.FormationMaster.WanXiangGuiZhen"), EGameXXKCardEffectType::AddArmor, 1000, EGameXXKCardMagnitudePolicy::DefensePercent, 7},
		{TEXT("Profession.FormationMaster.ShuiJingZheGuang"), EGameXXKCardEffectType::AddArmor, 200, EGameXXKCardMagnitudePolicy::DefensePercent, 3},
		{TEXT("Profession.FormationMaster.DiMaiJieLi"), EGameXXKCardEffectType::DamagePercentAttack, 200, EGameXXKCardMagnitudePolicy::ContinuousQuality, 0},
		{TEXT("Profession.FormationMaster.SiXiangLianHuan"), EGameXXKCardEffectType::AddArmor, 600, EGameXXKCardMagnitudePolicy::DefensePercent, 7}};
	for (const FRow& Row : Rows)
	{
		const FGameXXKCardEffect* Found = Effect(Row.Id, Row.Type);
		TestNotNull(*Row.Id.ToString(), Found);
		if (Found)
		{
			TestEqual(TEXT("raw magnitude"), Found->Magnitude, Row.Magnitude);
			TestEqual(TEXT("magnitude policy"), Found->MagnitudePolicy, Row.Policy);
			TestEqual(TEXT("rational percentage denominator"), Found->SecondaryMagnitude, Row.Secondary);
		}
	}
	TestEqual(TEXT("town swap removes one/two/three Vulnerability"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.YiWeiZhen")), EGameXXKCardQuality::Epic).Effects[1].Magnitude, 3);
	TestEqual(TEXT("mountain seal applies two/three/four Vulnerability"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.ShanMenFengSuo")), EGameXXKCardQuality::Epic).Effects[0].Magnitude, 4);
	TestEqual(TEXT("Epic wind grants two Agility"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.FormationMaster.LinFengFuZhen")), EGameXXKCardQuality::Epic).Effects[0].Magnitude, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKFormationMasterRuntimeValuesTest,
	"GameXXK.Data.PartnerCards.Formation.Rebalance.RuntimeValues", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFormationMasterRuntimeValuesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKFormationMasterRebalanceTest;
	FGameXXKCardPlayResult Result;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), EGameXXKCardQuality::Rare, Runtime)) return false;
	FGameXXKCardCombatUnit* Owner = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Formation")); });
	FGameXXKCardCombatUnit* Ally = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Ally")); });
	Owner->HP = Ally->HP = 50;
	GameXXKCardRules::AddCombatStatus(*Owner, EGameXXKCardStatus::Medicine, 5);
	if (!Play(*this, Runtime, NAME_None, Result)) return false;
	Owner = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Formation")); });
	Ally = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Ally")); });
	TestEqual(TEXT("Rare coefficient twenty plus five Medicine heals one hundred fifty"), Ally->HP, 200);
	TestEqual(TEXT("Medicine is consumed once for the group action"), GameXXKCardRules::GetCombatStatusStacks(*Owner, EGameXXKCardStatus::Medicine), 0);
	TestEqual(TEXT("native Rare group Armor resolves to fifty percent of caster Defense"), Ally->Armor, 50);
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.ZhenShaZhen"), EGameXXKCardQuality::Epic, Runtime) || !Play(*this, Runtime, NAME_None, Result)) return false;
	TestEqual(TEXT("Epic town-killing formation deals 448 to every enemy"), Runtime.Units.Last().HP, 99552);
	TestEqual(TEXT("it applies the five-stack Vulnerability cap"), GameXXKCardRules::GetCombatStatusStacks(Runtime.Units.Last(), EGameXXKCardStatus::Vulnerability), 5);
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.WanXiangGuiZhen"), EGameXXKCardQuality::Epic, Runtime) || !Play(*this, Runtime, NAME_None, Result)) return false;
	TestEqual(TEXT("native Epic group Armor resolves to two hundred percent Defense"), Runtime.Units[1].Armor, 200);
	if (!BuildRuntime(*this, TEXT("Profession.FormationMaster.ShuiJingZheGuang"), EGameXXKCardQuality::Rare, Runtime)) return false;
	Ally = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Ally")); });
	for (const EGameXXKCardStatus Status : {EGameXXKCardStatus::Bleed, EGameXXKCardStatus::Poison, EGameXXKCardStatus::Burn, EGameXXKCardStatus::DamageOverTime})
		GameXXKCardRules::AddCombatStatus(*Ally, Status, 10);
	if (!Play(*this, Runtime, TEXT("Ally"), Result)) return false;
	Ally = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& U) { return U.UnitId == FName(TEXT("Ally")); });
	TestEqual(TEXT("native Rare target Armor resolves to eighty percent Defense"), Ally->Armor, 80);
	for (const EGameXXKCardStatus Status : {EGameXXKCardStatus::Bleed, EGameXXKCardStatus::Poison, EGameXXKCardStatus::Burn, EGameXXKCardStatus::DamageOverTime})
		TestEqual(TEXT("all four DOT reservoirs are cleared"), GameXXKCardRules::GetCombatStatusStacks(*Ally, Status), 0);
	return true;
}

#endif
