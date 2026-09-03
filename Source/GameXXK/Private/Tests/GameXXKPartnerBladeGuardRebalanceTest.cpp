#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKPartnerBladeGuardRebalanceTest
{
	FGameXXKCardCombatUnit MakeUnit(const TCHAR* Id, EGameXXKCardTargetSide Side, EGameXXKCharacterRole Role, int32 Order)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = Id;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Unit.MaxHP = Side == EGameXXKCardTargetSide::Enemy ? 1000000 : 1000;
		Unit.Attack = 100;
		Unit.Defense = 0;
		Unit.Mana = Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.CombatLevel = 100;
		Unit.StableSortOrder = Order;
		return Unit;
	}

	FGameXXKCardCombatUnit* Unit(FGameXXKCardBattleRuntime& Runtime, const TCHAR* Id)
	{
		return Runtime.Units.FindByPredicate([Id](const FGameXXKCardCombatUnit& Entry) { return Entry.UnitId == FName(Id); });
	}

	bool Build(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, const TCHAR* CardId,
		EGameXXKCardQuality Quality, EGameXXKCharacterRole Role)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = TEXT("Main");
		Card.SourceEntryId = TEXT("Rebalance.Main");
		Card.CardId = CardId;
		Card.OwnerUnitId = TEXT("Owner");
		Card.CurrentQuality = Quality;
		Card.AcquisitionOrdinal = 0;
		FGameXXKCardInstance Reserve;
		Reserve.InstanceId = TEXT("Reserve");
		Reserve.SourceEntryId = TEXT("Rebalance.Reserve");
		Reserve.CardId = TEXT("Hero.Generic.QingFengYiShi");
		Reserve.OwnerUnitId = TEXT("Hero");
		Reserve.CurrentQuality = EGameXXKCardQuality::Common;
		Reserve.AcquisitionOrdinal = 1;
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(Runtime, {Card, Reserve},
			{MakeUnit(TEXT("Owner"), EGameXXKCardTargetSide::Party, Role, 1),
			 MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
			 MakeUnit(TEXT("Npc"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 3),
			 MakeUnit(TEXT("EnemyA"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			 MakeUnit(TEXT("EnemyB"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)},
			EGameXXKCardTerrain::Plain, 60401, &Error))
		{
			Test.AddError(Error);
			return false;
		}
		Runtime.Deck.Hand = {Card};
		Runtime.Deck.DrawPile = {Reserve};
		Runtime.Deck.DiscardPile.Reset();
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.SharedEnergy = 99;
		Unit(Runtime, TEXT("Owner"))->Defense = 358;
		Unit(Runtime, TEXT("Hero"))->Defense = 1;
		Unit(Runtime, TEXT("Npc"))->Defense = 2;
		return Test.TestTrue(FString::Printf(TEXT("exact partner fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	}

	bool Play(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, FName Target, FGameXXKCardPlayResult& Result)
	{
		FString Error;
		const bool bPlayed = GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Main"), Target, Result, &Error);
		return Test.TestTrue(FString::Printf(TEXT("approved partner transaction: %s"), *Error), bPlayed);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerBladeGuardCatalogRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.BladeGuardCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerBladeGuardCatalogRebalanceTest::RunTest(const FString& Parameters)
{
	int32 BladeCount = 0;
	int32 GuardCount = 0;
	for (const FGameXXKCardDefinition& Card : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Card.OwnerId == TEXT("Profession.Blade"))
		{
			++BladeCount;
			for (const FGameXXKCardEffect& Effect : Card.Effects)
			{
				if (Effect.Type == EGameXXKCardEffectType::ApplyStatus && Effect.Status == EGameXXKCardStatus::Bleed)
					TestEqual(TEXT("Blade Bleed uses generation coefficients"), Effect.MagnitudePolicy, EGameXXKCardMagnitudePolicy::DotCoefficient);
			}
		}
		if (Card.OwnerId != TEXT("Profession.Guard")) continue;
		++GuardCount;
		for (int32 Index = 0; Index < Card.Effects.Num(); ++Index)
		{
			const FGameXXKCardEffect& Effect = Card.Effects[Index];
			if (Effect.Type == EGameXXKCardEffectType::AddArmor)
			{
				const bool bSecondary = Card.Id == TEXT("Profession.Guard.ZhenDun")
					|| Card.Id == TEXT("Profession.Guard.ZhenYueLing") || Card.Id == TEXT("Profession.Guard.BiLeiFanGong")
					|| Card.Id == TEXT("Profession.Guard.DunZhenTuiJin")
					|| (Card.Id == TEXT("Profession.Guard.YuanJunBiLei") && Index == 1);
				TestEqual(FString::Printf(TEXT("%s Armor policy"), *Card.Id.ToString()), Effect.MagnitudePolicy,
					bSecondary ? EGameXXKCardMagnitudePolicy::DefensePercent : EGameXXKCardMagnitudePolicy::PrintedCostArmor);
			}
			if (Effect.Type == EGameXXKCardEffectType::DamagePercentAttackPlusArmor)
				TestEqual(TEXT("only the direct Attack base is quality-scaled"), Effect.MagnitudePolicy, EGameXXKCardMagnitudePolicy::ContinuousQuality);
		}
	}
	TestEqual(TEXT("all eighteen Blade definitions remain"), BladeCount, 18);
	TestEqual(TEXT("all eighteen Guard definitions remain"), GuardCount, 18);
	TestEqual(TEXT("ZhanJin raw base is three hundred"), FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.ZhanJin"))->Effects[0].Magnitude, 300);
	TestEqual(TEXT("HengYun raw base is one hundred"), FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.HengYunKaiFeng"))->Effects[0].Magnitude, 100);
	const FGameXXKCardDefinition* Recovery = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.LianXiGuiQiao"));
	TestEqual(TEXT("Rare LianXi recovers four Mana"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*Recovery, EGameXXKCardQuality::Rare).Effects[0].Magnitude, 4);
	TestEqual(TEXT("Epic LianXi recovers six Mana"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*Recovery, EGameXXKCardQuality::Epic).Effects[0].Magnitude, 6);
	TestEqual(TEXT("BaoDao retains exactly two Agility"), FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Blade.BaoDaoShouYe"))->Effects[0].Magnitude, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGuardArmorRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.GuardDefenseArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardArmorRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerBladeGuardRebalanceTest;
	struct FCase { const TCHAR* Card; EGameXXKCardQuality Quality; int32 OwnerArmor; int32 AllyArmor; const TCHAR* Target; };
	const FCase Cases[] = {
		{TEXT("GuShou"), EGameXXKCardQuality::Common, 144, 0, nullptr},
		{TEXT("TieBi"), EGameXXKCardQuality::Common, 287, 0, nullptr},
		{TEXT("FanZhenJia"), EGameXXKCardQuality::Rare, 344, 0, nullptr},
		{TEXT("TieBiRuShan"), EGameXXKCardQuality::Rare, 602, 0, nullptr},
		{TEXT("BuDongRuShan"), EGameXXKCardQuality::Epic, 1003, 0, nullptr},
		{TEXT("TieSuoHengJiang"), EGameXXKCardQuality::Epic, 702, 0, nullptr},
		{TEXT("YiFuDangGuan"), EGameXXKCardQuality::Epic, 1003, 1003, nullptr},
		{TEXT("PiJiaXingJun"), EGameXXKCardQuality::Common, 287, 287, nullptr},
		{TEXT("YuanJunBiLei"), EGameXXKCardQuality::Rare, 172, 344, TEXT("Hero")},
		{TEXT("HuZhu"), EGameXXKCardQuality::Common, 287, 287, TEXT("Hero")},
		{TEXT("YuanHuBu"), EGameXXKCardQuality::Common, 144, 144, nullptr},
		{TEXT("ZhenDun"), EGameXXKCardQuality::Common, 144, 0, TEXT("EnemyA")},
		{TEXT("DunZhenTuiJin"), EGameXXKCardQuality::Rare, 172, 172, TEXT("EnemyA")}};
	for (const FCase& Case : Cases)
	{
		FGameXXKCardBattleRuntime Runtime;
		const FString Id = FString(TEXT("Profession.Guard.")) + Case.Card;
		if (!Build(*this, Runtime, *Id, Case.Quality, EGameXXKCharacterRole::Guard)) return false;
		Unit(Runtime, TEXT("Hero"))->HP = 500;
		FGameXXKCardPlayResult Result;
		if (!Play(*this, Runtime, Case.Target ? FName(Case.Target) : NAME_None, Result)) continue;
		TestEqual(Id + TEXT(" owner Armor"), Unit(Runtime, TEXT("Owner"))->Armor, Case.OwnerArmor);
		TestEqual(Id + TEXT(" Hero Armor uses caster Defense"), Unit(Runtime, TEXT("Hero"))->Armor, Case.AllyArmor);
		if (FString(Case.Card) == TEXT("YiFuDangGuan"))
			TestEqual(TEXT("YiFu grants one full packet to the third ally too"), Unit(Runtime, TEXT("Npc"))->Armor, 1003);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKGuardConversionRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.GuardArmorConversionAndPanShi", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGuardConversionRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerBladeGuardRebalanceTest;
	const TCHAR* Cards[] = {TEXT("Profession.Guard.ZhenYueLing"), TEXT("Profession.Guard.BiLeiFanGong")};
	const EGameXXKCardQuality Qualities[] = {EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic};
	const int32 ExpectedDamage[][2] = {{516, 552}, {564, 608}};
	const int32 ExpectedArmor[] = {215, 251};
	for (int32 CardIndex = 0; CardIndex < 2; ++CardIndex)
		for (int32 QualityIndex = 0; QualityIndex < 2; ++QualityIndex)
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!Build(*this, Runtime, Cards[CardIndex], Qualities[QualityIndex], EGameXXKCharacterRole::Guard)) return false;
			Unit(Runtime, TEXT("Owner"))->Armor = 300;
			FGameXXKCardPlayResult Result;
			if (!Play(*this, Runtime, NAME_None, Result)) continue;
			TestEqual(TEXT("armor conversion emits one hit per enemy"), Result.DamageResults.Num(), 2);
			for (const FGameXXKCardDamageResult& Hit : Result.DamageResults)
				TestEqual(TEXT("quality scales the base, consumed Armor adds one point"), Hit.BaseRequestedDamage, ExpectedDamage[CardIndex][QualityIndex]);
			TestEqual(TEXT("consumed owner Armor is replaced by its secondary grant"), Unit(Runtime, TEXT("Owner"))->Armor, ExpectedArmor[QualityIndex]);
			TestEqual(TEXT("only ZhenYue grants group Armor"), Unit(Runtime, TEXT("Hero"))->Armor, CardIndex == 0 ? ExpectedArmor[QualityIndex] : 0);
		}
	for (const int32 Armor : {0, 1})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime, TEXT("Profession.Guard.PanShiTuNa"), EGameXXKCardQuality::Common, EGameXXKCharacterRole::Guard)) return false;
		Unit(Runtime, TEXT("Owner"))->Armor = Armor;
		Unit(Runtime, TEXT("Owner"))->Mana = 10;
		FGameXXKCardPlayResult Result;
		if (!Play(*this, Runtime, NAME_None, Result)) continue;
		TestEqual(TEXT("one Armor is enough for the resource branch"), Unit(Runtime, TEXT("Owner"))->Mana, Armor ? 15 : 10);
		TestEqual(TEXT("zero Armor gains its printed-cost grant"), Unit(Runtime, TEXT("Owner"))->Armor, Armor ? 1 : 144);
		TestEqual(TEXT("only positive-Armor branch draws the reserve"), Runtime.Deck.Hand.Num(), Armor ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBladeConversionRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.BladeConversionsAndFinish", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBladeConversionRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerBladeGuardRebalanceTest;
	FGameXXKCardBattleRuntime Blood;
	if (!Build(*this, Blood, TEXT("Profession.Blade.FengHou"), EGameXXKCardQuality::Common, EGameXXKCharacterRole::Blade)) return false;
	GameXXKCardRules::AddCombatStatus(*Unit(Blood, TEXT("EnemyA")), EGameXXKCardStatus::Bleed, 5);
	FGameXXKCardPlayResult Result;
	if (Play(*this, Blood, TEXT("EnemyA"), Result))
	{
		const FGameXXKCardDamageResult* Direct = Result.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& Hit) { return Hit.Cause == EGameXXKCardDamageCause::DirectAttack; });
		if (TestNotNull(TEXT("Blood Edge emits its direct hit"), Direct))
			TestEqual(TEXT("five resolved Bleed adds ten attack percentage points"), Direct->BaseRequestedDamage, 110);
	}
	const EGameXXKCardQuality Qualities[] = {EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic};
	for (int32 Index = 0; Index < 2; ++Index)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime, TEXT("Profession.Blade.PoJun"), Qualities[Index], EGameXXKCharacterRole::Blade)) return false;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("EnemyA")), EGameXXKCardStatus::Vulnerability, 2);
		if (Play(*this, Runtime, TEXT("EnemyA"), Result))
		{
			TestEqual(TEXT("PoJun has its primary and two bonus attacks"), Result.DamageResults.Num(), 3);
			for (int32 Hit = 1; Hit < Result.DamageResults.Num(); ++Hit)
				TestEqual(TEXT("PoJun bonus uses sixty/seventy percent by quality"), Result.DamageResults[Hit].BaseRequestedDamage, Index == 0 ? 60 : 70);
		}
		if (!Build(*this, Runtime, TEXT("Profession.Blade.YinXueDao"), Qualities[Index], EGameXXKCharacterRole::Blade)) return false;
		if (!Play(*this, Runtime, TEXT("EnemyA"), Result)) continue;
		TArray<FGameXXKCardDamageResult> EndResults;
		FString Error;
		if (TestTrue(FString::Printf(TEXT("YinXue Finish begins: %s"), *Error), GameXXKCardRules::EndPlayerCardPhase(Runtime, EndResults, &Error)))
			TestEqual(TEXT("level-one-hundred Finish healing budget uses coefficient twenty"), Runtime.PendingBladeFinish.RemainingTriggers, Index == 0 ? 120 : 140);
	}
	return true;
}

#endif
