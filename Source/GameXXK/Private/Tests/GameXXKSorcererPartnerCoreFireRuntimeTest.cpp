#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerCoreFireRuntimeTest
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
	FGameXXKSorcererPartnerCoreRuntimeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.CoreFire.CoreSearchAndManaEcho",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerCoreRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	const TArray<FName> SearchLoadoutIds = {
		TEXT("Profession.Sorcerer.LingHuoFu"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang")};
	TArray<FGameXXKCardInstance> SearchCards;
	for (int32 Index = 0; Index < SearchLoadoutIds.Num(); ++Index)
	{
		SearchCards.Add(MakeCard(SearchLoadoutIds[Index], Index));
	}
	FGameXXKCardBattleRuntime SearchRuntime;
	if (!BuildRuntime(*this, SearchCards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59401, SearchRuntime))
	{
		return false;
	}
	SearchRuntime.Deck.Hand = {SearchCards[0]};
	SearchRuntime.Deck.DrawPile = {SearchCards[1], SearchCards[2], SearchCards[3], SearchCards[4]};
	SearchRuntime.Deck.DiscardPile.Reset();
	SearchRuntime.Deck.ExhaustPile.Reset();
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("core-search fixture validates: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(SearchRuntime, &Error)))
	{
		return false;
	}

	FGameXXKCardPlayResult SearchResult;
	if (!ResolveActive(*this, SearchRuntime, SearchCards[0].InstanceId, SearchResult, TEXT("position-one Core search")))
	{
		return true;
	}
	TestEqual(TEXT("Core search opens one shared choice kind"), SearchRuntime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(FString::Printf(TEXT("Core search picks the four-Mana card: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(SearchRuntime, SearchCards[1].InstanceId, ResumedResults, &Error));
	FGameXXKCardPlayPreview DiscountPreview;
	TestTrue(FString::Printf(TEXT("discounted searched card previews: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(SearchRuntime, SearchCards[1].InstanceId, DiscountPreview, &Error));
	TestEqual(TEXT("position-one Core search reduces selected Mana by three"), DiscountPreview.EffectiveManaCost, 1);
	FGameXXKCardPlayResult DiscountedResult;
	if (!ResolveActive(*this, SearchRuntime, SearchCards[1].InstanceId, DiscountedResult, TEXT("discounted searched card")))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* DiscountTask = SearchRuntime.SorcererPartnerTasks.FindByPredicate([](const FGameXXKSorcererPartnerTaskRuntime& Task)
	{
		return Task.OwnerUnitId == SorcererId;
	});
	if (!TestNotNull(TEXT("Core search task remains active"), DiscountTask))
	{
		return true;
	}
	TestEqual(TEXT("discounted card records actual one Mana paid"), DiscountTask->FirstPlayOrder.Last().PaidManaCost, 1);

	const TArray<FName> EchoLoadoutIds = {
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> EchoCards;
	for (int32 Index = 0; Index < EchoLoadoutIds.Num(); ++Index)
	{
		EchoCards.Add(MakeCard(EchoLoadoutIds[Index], Index));
	}
	FGameXXKCardBattleRuntime EchoRuntime;
	if (!BuildRuntime(*this, EchoCards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59402, EchoRuntime)
		|| !InstallAllCardsInHand(*this, EchoRuntime, EchoCards))
	{
		return false;
	}
	FGameXXKCardCombatUnit* EchoOwner = FindUnit(EchoRuntime, SorcererId);
	EchoOwner->Mana = 20;
	EchoOwner->MaxMana = 100;
	FGameXXKCardPlayResult PaidFourResult;
	if (!ResolveActive(*this, EchoRuntime, EchoCards[0].InstanceId, PaidFourResult, TEXT("four-Mana predecessor")))
	{
		return true;
	}
	TestEqual(TEXT("predecessor pays four Mana"), FindUnit(EchoRuntime, SorcererId)->Mana, 16);
	FGameXXKCardPlayResult EchoResult;
	if (!ResolveActive(*this, EchoRuntime, EchoCards[1].InstanceId, EchoResult, TEXT("Core Mana echo")))
	{
		return true;
	}
	TestEqual(TEXT("Core echo restores three plus half previous paid Mana"), FindUnit(EchoRuntime, SorcererId)->Mana, 21);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererIndependentCoreSearchDiscountsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.CoreFire.TwoUnplayedSearchesKeepIndependentManaDiscounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererIndependentCoreSearchDiscountsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LingHuoFu"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], Index));
	}

	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59403, Runtime))
	{
		return false;
	}
	Runtime.Deck.Hand = {Cards[0]};
	Runtime.Deck.DrawPile = {Cards[1], Cards[2], Cards[3], Cards[4]};
	Runtime.Deck.DiscardPile.Reset();
	Runtime.Deck.ExhaustPile.Reset();

	FGameXXKCardPlayResult FirstSearchResult;
	if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, FirstSearchResult, TEXT("first Core search")))
	{
		return true;
	}
	FString Error;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(FString::Printf(TEXT("first Core search selects the four-Mana card: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, Cards[1].InstanceId, ResumedResults, &Error));

	const int32 CoreDiscardIndex = Runtime.Deck.DiscardPile.IndexOfByPredicate([&Cards](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == Cards[0].InstanceId;
	});
	TestTrue(TEXT("the repeated-search fixture finds the resolved Core card in discard"), CoreDiscardIndex != INDEX_NONE);
	if (CoreDiscardIndex == INDEX_NONE)
	{
		return true;
	}
	Runtime.Deck.Hand.Add(MoveTemp(Runtime.Deck.DiscardPile[CoreDiscardIndex]));
	Runtime.Deck.DiscardPile.RemoveAt(CoreDiscardIndex, 1, EAllowShrinking::No);
	TestTrue(FString::Printf(TEXT("the first bound discount remains valid while Core returns to hand: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));

	FGameXXKCardPlayResult SecondSearchResult;
	if (!ResolveActive(*this, Runtime, Cards[0].InstanceId, SecondSearchResult, TEXT("second Core search")))
	{
		return true;
	}
	TestTrue(FString::Printf(TEXT("second Core search selects a different two-Mana card: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, Cards[4].InstanceId, ResumedResults, &Error));
	TestTrue(FString::Printf(TEXT("two independently bound searched cards form a valid runtime: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));

	FGameXXKCardPlayPreview FirstPreview;
	FGameXXKCardPlayPreview SecondPreview;
	TestTrue(FString::Printf(TEXT("the first searched card previews with its own discount: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, Cards[1].InstanceId, FirstPreview, &Error));
	TestTrue(FString::Printf(TEXT("the second searched card previews with its own discount: %s"), *Error),
		GameXXKCardRules::BuildCardPlayPreview(Runtime, Cards[4].InstanceId, SecondPreview, &Error));
	TestEqual(TEXT("the first four-Mana card remains reduced by three"), FirstPreview.EffectiveManaCost, 1);
	TestEqual(TEXT("the second two-Mana card is independently reduced to zero"), SecondPreview.EffectiveManaCost, 0);

	TSet<FName> DiscountedInstanceIds;
	for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
	{
		if (Modifier.Definition.EffectType == EGameXXKCardEffectType::ModifyManaCost
			&& Modifier.Definition.Magnitude == -3
			&& !Modifier.RequiredPlayedCardInstanceId.IsNone())
		{
			DiscountedInstanceIds.Add(Modifier.RequiredPlayedCardInstanceId);
		}
	}
	TestEqual(TEXT("two discounts remain bound to two distinct hand instances"), DiscountedInstanceIds.Num(), 2);
	TestTrue(TEXT("the first searched instance owns one binding"), DiscountedInstanceIds.Contains(Cards[1].InstanceId));
	TestTrue(TEXT("the second searched instance owns one binding"), DiscountedInstanceIds.Contains(Cards[4].InstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerFireRuntimeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.CoreFire.FireSequenceBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerFireRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerCoreFireRuntimeTest;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime LampEarly;
	if (!BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.LiHuoYin"), 59411, LampEarly)
		|| !ResolveAutomaticSnapshot(*this, LampEarly, TEXT("Profession.Sorcerer.LiHuoYin"), 1, EGameXXKSorcererCardFamily::None, EGameXXKSorcererTaskBranch::Fire, Result))
	{
		return false;
	}
	TestEqual(TEXT("level-one Fire lamp coefficient four generates five Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(LampEarly, EnemyAId), EGameXXKCardStatus::Burn), 5);

	FGameXXKCardBattleRuntime LampLate;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.LiHuoYin"), 59412, LampLate);
	ResolveAutomaticSnapshot(*this, LampLate, TEXT("Profession.Sorcerer.LiHuoYin"), 3, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("level-one late Fire lamp coefficient two generates three Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(LampLate, EnemyAId), EGameXXKCardStatus::Burn), 3);

	FGameXXKCardBattleRuntime SpreadFire;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.YanQiang"), 59413, SpreadFire);
	ResolveAutomaticSnapshot(*this, SpreadFire, TEXT("Profession.Sorcerer.YanQiang"), 2, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("level-one Fire following Fire generates four Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(SpreadFire, EnemyAId), EGameXXKCardStatus::Burn), 4);

	FGameXXKCardBattleRuntime SpreadIce;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.YanQiang"), 59414, SpreadIce);
	ResolveAutomaticSnapshot(*this, SpreadIce, TEXT("Profession.Sorcerer.YanQiang"), 2, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("level-one non-Fire predecessor keeps coefficient one and generates two Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(SpreadIce, EnemyAId), EGameXXKCardStatus::Burn), 2);

	FGameXXKCardBattleRuntime BurstEarly;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.BaoYanShu"), 59415, BurstEarly);
	GameXXKCardRules::AddCombatStatus(*FindUnit(BurstEarly, EnemyAId), EGameXXKCardStatus::Burn, 3);
	const int32 BurstEarlyHp = FindUnit(BurstEarly, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, BurstEarly, TEXT("Profession.Sorcerer.BaoYanShu"), 2, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("early burst remains eighty percent"), BurstEarlyHp - FindUnit(BurstEarly, EnemyAId)->HP, 16);
	TestEqual(TEXT("early burst preserves Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(BurstEarly, EnemyAId), EGameXXKCardStatus::Burn), 3);

	FGameXXKCardBattleRuntime BurstLate;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.BaoYanShu"), 59416, BurstLate);
	GameXXKCardRules::AddCombatStatus(*FindUnit(BurstLate, EnemyAId), EGameXXKCardStatus::Burn, 3);
	const int32 BurstLateHp = FindUnit(BurstLate, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, BurstLate, TEXT("Profession.Sorcerer.BaoYanShu"), 3, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("late burst adds two percentage points per stored Burn"), BurstLateHp - FindUnit(BurstLate, EnemyAId)->HP, 17);
	TestEqual(TEXT("late burst does not consume Burn"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(BurstLate, EnemyAId), EGameXXKCardStatus::Burn), 3);

	FGameXXKCardBattleRuntime SearchThird;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), 59417, SearchThird);
	const int32 SearchThirdHp = FindUnit(SearchThird, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, SearchThird, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), 3, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("position three candidate-free Fire search resolves two forty-percent hits"), SearchThirdHp - FindUnit(SearchThird, EnemyAId)->HP, 16);

	FGameXXKCardBattleRuntime SearchFourth;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), 59418, SearchFourth);
	const int32 SearchFourthHp = FindUnit(SearchFourth, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, SearchFourth, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), 4, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Fire, Result);
	TestEqual(TEXT("position four candidate-free Fire search resolves two seventy-percent hits"), SearchFourthHp - FindUnit(SearchFourth, EnemyAId)->HP, 28);
	return true;
}

#endif
