#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerUniversalRuntimeTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;

	bool BuildSingleCardRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		return BuildRuntime(
			Test,
			{MakeCard(CardId, 0)},
			{
				MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
				MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
				MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)
			},
			Seed,
			OutRuntime);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerUniversalScalingDrawTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.Universal.ScalingAttackAndDraw",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerUniversalScalingDrawTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRuntimeTest;
	const int32 ExpectedDamageByPosition[] = {12, 17, 22, 27, 32};
	for (int32 Position = 1; Position <= 5; ++Position)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.YanMuHuTi"), 59600 + Position, Runtime))
		{
			return false;
		}
		const int32 HpBefore = FindUnit(Runtime, EnemyAId)->HP;
		FGameXXKCardPlayResult Result;
		if (!ResolveAutomaticSnapshot(
			*this,
			Runtime,
			TEXT("Profession.Sorcerer.YanMuHuTi"),
			Position,
			Position == 1 ? EGameXXKSorcererCardFamily::None : EGameXXKSorcererCardFamily::Universal,
			EGameXXKSorcererTaskBranch::Normal,
			Result))
		{
			return true;
		}
		TestEqual(
			FString::Printf(TEXT("Universal scaling attack position %d"), Position),
			HpBefore - FindUnit(Runtime, EnemyAId)->HP,
			ExpectedDamageByPosition[Position - 1]);
	}

	const auto RunDrawPosition = [this](const int32 Position, const int32 Seed, const int32 ExpectedMana) -> bool
	{
		TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("Profession.Sorcerer.LieFu"), 0),
			MakeCard(TEXT("Route.General.PoJiaTuCi"), 1),
			MakeCard(TEXT("Route.General.PoJiaTuCi"), 2),
			MakeCard(TEXT("Route.General.PoJiaTuCi"), 3)};
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, Seed, Runtime))
		{
			return false;
		}
		Runtime.Deck.Hand = {Cards[0]};
		Runtime.Deck.DrawPile = {Cards[1], Cards[2], Cards[3]};
		Runtime.Deck.DiscardPile.Reset();
		FGameXXKCardCombatUnit* Owner = FindUnit(Runtime, SorcererId);
		Owner->Mana = 0;
		Owner->MaxMana = 20;
		FGameXXKCardPlayResult Result;
		if (!ResolveAutomaticSnapshot(
			*this,
			Runtime,
			TEXT("Profession.Sorcerer.LieFu"),
			Position,
			EGameXXKSorcererCardFamily::Universal,
			EGameXXKSorcererTaskBranch::Normal,
			Result))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("Universal draw position %d draws one"), Position), Runtime.Deck.Hand.Num(), 2);
		TestEqual(FString::Printf(TEXT("Universal draw position %d Mana"), Position), FindUnit(Runtime, SorcererId)->Mana, ExpectedMana);
		return true;
	};
	RunDrawPosition(2, 59611, 0);
	RunDrawPosition(3, 59612, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerUniversalArmorSearchTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.Universal.PreviousDamageArmorAndSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerUniversalArmorSearchTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRuntimeTest;
	const auto RunArmorSequence = [this](
		const FName PreviousCardId,
		const int32 Seed,
		const int32 ExpectedAllyArmor,
		const TCHAR* Label) -> bool
	{
		const TArray<FName> CardIds = {
			PreviousCardId,
			TEXT("Profession.Sorcerer.XingHuoHuiShou"),
			TEXT("Profession.Sorcerer.JuLing"),
			TEXT("Profession.Sorcerer.YanQiang"),
			TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			Cards.Add(MakeCard(CardIds[Index], Index));
		}
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Cards, {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 2),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, Seed, Runtime)
			|| !InstallAllCardsInHand(*this, Runtime, Cards))
		{
			return false;
		}
		FGameXXKCardPlayResult PreviousResult;
		if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, PreviousResult, Label))
		{
			return false;
		}
		FGameXXKCardPlayResult ArmorResult;
		if (!ResolveActive(*this, Runtime, Cards[1].InstanceId, ArmorResult, TEXT("Universal party armor")))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("%s determines party armor"), Label), FindUnit(Runtime, AllyId)->Armor, ExpectedAllyArmor);
		return true;
	};
	RunArmorSequence(TEXT("Profession.Sorcerer.SheLingHuo"), 59621, 13, TEXT("non-damaging Ice predecessor"));
	RunArmorSequence(TEXT("Profession.Sorcerer.LiHuoYin"), 59622, 4, TEXT("direct-damage Fire predecessor"));

	FGameXXKCardPlayResult Result;
	FGameXXKCardBattleRuntime SearchThird;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.ChiYanFengJie"), 59623, SearchThird);
	const int32 SearchThirdHp = FindUnit(SearchThird, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, SearchThird, TEXT("Profession.Sorcerer.ChiYanFengJie"), 3, EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererTaskBranch::Normal, Result);
	TestEqual(TEXT("position-three candidate-free Universal search resolves two sixty-five-percent hits"), SearchThirdHp - FindUnit(SearchThird, EnemyAId)->HP, 26);

	FGameXXKCardBattleRuntime SearchFourth;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.ChiYanFengJie"), 59624, SearchFourth);
	const int32 SearchFourthHp = FindUnit(SearchFourth, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, SearchFourth, TEXT("Profession.Sorcerer.ChiYanFengJie"), 4, EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererTaskBranch::Normal, Result);
	TestEqual(TEXT("position-four candidate-free Universal search resolves two ninety-percent hits"), SearchFourthHp - FindUnit(SearchFourth, EnemyAId)->HP, 36);
	return true;
}

#endif
