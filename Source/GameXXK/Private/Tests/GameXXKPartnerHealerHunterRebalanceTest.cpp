#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKPartnerHealerHunterRebalanceTest
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

	int32 Status(FGameXXKCardBattleRuntime& Runtime, const TCHAR* Id, EGameXXKCardStatus Kind)
	{
		return GameXXKCardRules::GetCombatStatusStacks(*Unit(Runtime, Id), Kind);
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
			EGameXXKCardTerrain::Plain, 60501, &Error))
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
		return Test.TestTrue(FString::Printf(TEXT("rebalance fixture validates: %s"), *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
	}

	bool Play(FAutomationTestBase& Test, FGameXXKCardBattleRuntime& Runtime, FName Target, FGameXXKCardPlayResult& Result)
	{
		FString Error;
		const bool bPlayed = GameXXKCardRules::ResolveCardPlay(Runtime, TEXT("Main"), Target, Result, &Error);
		return Test.TestTrue(FString::Printf(TEXT("approved Healer/Hunter play: %s"), *Error), bPlayed);
	}

	void OpenFormula(FGameXXKCardBattleRuntime& Runtime, const TCHAR* CardId, const TCHAR* OwnerId = TEXT("Owner"))
	{
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(CardId);
		FGameXXKHealerFormulaRuntime& Formula = Runtime.HealerFormulas.AddDefaulted_GetRef();
		Formula.SourceCardId = Card->Id;
		Formula.OwnerUnitId = OwnerId;
		Formula.Kind = Card->HealerRule.FormulaKind;
	}

	void ReturnMainToHand(FGameXXKCardBattleRuntime& Runtime)
	{
		const int32 Index = Runtime.Deck.DiscardPile.IndexOfByPredicate([](const FGameXXKCardInstance& Card) { return Card.InstanceId == TEXT("Main"); });
		if (Index != INDEX_NONE)
		{
			Runtime.Deck.Hand.Add(Runtime.Deck.DiscardPile[Index]);
			Runtime.Deck.DiscardPile.RemoveAt(Index);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPartnerHealerHunterCatalogRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HealerHunterCatalog", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartnerHealerHunterCatalogRebalanceTest::RunTest(const FString& Parameters)
{
	int32 HealerCount = 0;
	int32 HunterCount = 0;
	for (const FGameXXKCardDefinition& Card : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		const bool bHealer = Card.OwnerId == TEXT("Profession.Healer");
		const bool bHunter = Card.OwnerId == TEXT("Profession.Hunter");
		if (!bHealer && !bHunter) continue;
		HealerCount += bHealer ? 1 : 0;
		HunterCount += bHunter ? 1 : 0;
		for (const FGameXXKCardEffect& Effect : Card.Effects)
		{
			if (Effect.Type == EGameXXKCardEffectType::ApplyStatus
				&& (Effect.Status == EGameXXKCardStatus::Bleed || Effect.Status == EGameXXKCardStatus::Poison || Effect.Status == EGameXXKCardStatus::Burn))
				TestEqual(Card.Id.ToString() + TEXT(" DOT policy"), Effect.MagnitudePolicy, EGameXXKCardMagnitudePolicy::DotCoefficient);
			if (Effect.Type == EGameXXKCardEffectType::HealOrReverseWithMedicine || Effect.Type == EGameXXKCardEffectType::HealOrReverseFlat)
			{
				TestEqual(Card.Id.ToString() + TEXT(" healing coefficient policy"), Effect.MagnitudePolicy, EGameXXKCardMagnitudePolicy::MedicineCoefficient);
				TestEqual(Card.Id.ToString() + TEXT(" healing coefficients are raw values at every native quality"),
					Effect.CoefficientReferenceQuality, EGameXXKCardQuality::Common);
			}
		}
		if (bHunter && (Card.HeavyArrow.Kind == EGameXXKHeavyArrowKind::ExtraAttackPerCharge || Card.HeavyArrow.Kind == EGameXXKHeavyArrowKind::AddPrimaryAttackPercentPerCharge))
			TestEqual(Card.Id.ToString() + TEXT(" Heavy Arrow policy"), Card.HeavyArrow.MagnitudePolicy, EGameXXKCardMagnitudePolicy::ContinuousQuality);
	}
	TestEqual(TEXT("eighteen Healer cards"), HealerCount, 18);
	TestEqual(TEXT("eighteen Hunter cards"), HunterCount, 18);
	struct FHealingRow { const TCHAR* Id; int32 Coefficient; };
	for (const FHealingRow& Row : {FHealingRow{TEXT("YaoYin"), 30}, {TEXT("CaoMuFuZhi"), 25}, {TEXT("QingXinSan"), 20},
		{TEXT("LingZhiXuMing"), 40}, {TEXT("HuiChunLu"), 25}, {TEXT("ZhiXueCao"), 10}, {TEXT("WenYangGao"), 25},
		{TEXT("JinChuangXuMing"), 45}, {TEXT("YaoWangGuiYuan"), 30}})
	{
		const FName Id(*(FString(TEXT("Profession.Healer.")) + Row.Id));
		const FGameXXKCardEffect* Healing = FGameXXKCardCatalog::FindCardDefinition(Id)->Effects.FindByPredicate([](const FGameXXKCardEffect& Effect)
			{ return Effect.Type == EGameXXKCardEffectType::HealOrReverseWithMedicine; });
		if (TestNotNull(Id.ToString(), Healing)) TestEqual(Id.ToString() + TEXT(" authored coefficient"), Healing->Magnitude, Row.Coefficient);
	}
	for (const TCHAR* Name : {TEXT("LieWang"), TEXT("LieHunBiao")})
	{
		const FName Id(*(FString(TEXT("Profession.Hunter.")) + Name));
		const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(Id);
		const int32 CommonCost = FString(Name) == TEXT("LieWang") ? 3 : 4;
		TestEqual(TEXT("Rare explicit Mana discount"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*Card, EGameXXKCardQuality::Rare).ManaCost, CommonCost - 1);
		TestEqual(TEXT("Epic explicit Mana discount"), FGameXXKCardQualityRules::BuildEffectiveDefinition(*Card, EGameXXKCardQuality::Epic).ManaCost, CommonCost - 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHealerNativeQualityHealingRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HealerNativeQualityHealing", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerNativeQualityHealingRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	struct FCase { EGameXXKCardQuality Quality; int32 Medicine; int32 Healing; };
	for (const FCase& Case : {FCase{EGameXXKCardQuality::Rare, 0, 150}, {EGameXXKCardQuality::Rare, 5, 180},
		{EGameXXKCardQuality::Epic, 0, 175}, {EGameXXKCardQuality::Epic, 5, 210}})
		for (const bool bEnemyTarget : {false, true})
		{
			FGameXXKCardBattleRuntime Runtime;
			if (!Build(*this, Runtime, TEXT("Profession.Healer.HuiChunLu"), Case.Quality, EGameXXKCharacterRole::Healer)) return false;
			for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")}) Unit(Runtime, Id)->HP = 500;
			GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Owner")), EGameXXKCardStatus::Medicine, Case.Medicine);
			GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Medicine, 7);
			FGameXXKCardPlayResult Result;
			if (!Play(*this, Runtime, bEnemyTarget ? TEXT("EnemyA") : TEXT("Hero"), Result)) continue;
			if (bEnemyTarget)
				for (const TCHAR* Id : {TEXT("EnemyA"), TEXT("EnemyB")}) TestEqual(TEXT("group reversal uses the same uniformly scaled amount"), 1000000 - Unit(Runtime, Id)->HP, Case.Healing);
			else
				for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")}) TestEqual(TEXT("each ally receives the complete amount"), Unit(Runtime, Id)->HP, 500 + Case.Healing);
			TestEqual(TEXT("owner Medicine consumed once"), Status(Runtime, TEXT("Owner"), EGameXXKCardStatus::Medicine), 0);
			TestEqual(TEXT("other owner Medicine untouched"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::Medicine), 7);
			int32 Spent = 0;
			for (const FGameXXKCardStatusChangeResult& Change : Result.StatusChanges)
				if (Change.Status == EGameXXKCardStatus::Medicine && Change.TargetUnitId == TEXT("Owner")) Spent += Change.RemovedStacks;
			TestEqual(TEXT("group has one Medicine spend"), Spent, Case.Medicine);
		}
	FGameXXKCardBattleRuntime LowHealth;
	if (!Build(*this, LowHealth, TEXT("Profession.Healer.LingZhiXuMing"), EGameXXKCardQuality::Rare, EGameXXKCharacterRole::Healer)) return false;
	Unit(LowHealth, TEXT("Hero"))->HP = 50;
	GameXXKCardRules::AddCombatStatus(*Unit(LowHealth, TEXT("Owner")), EGameXXKCardStatus::Medicine, 5);
	FGameXXKCardPlayResult Result;
	if (Play(*this, LowHealth, TEXT("Hero"), Result))
		TestEqual(TEXT("50 HP plus Rare raw40 and Medicine5 healing270, then Medicine-free raw10 healing60"), Unit(LowHealth, TEXT("Hero"))->HP, 380);
	if (!Build(*this, LowHealth, TEXT("Profession.Healer.LingZhiXuMing"), EGameXXKCardQuality::Rare, EGameXXKCharacterRole::Healer)) return false;
	Unit(LowHealth, TEXT("Hero"))->HP = 100;
	GameXXKCardRules::AddCombatStatus(*Unit(LowHealth, TEXT("Owner")), EGameXXKCardStatus::Medicine, 5);
	if (Play(*this, LowHealth, TEXT("Hero"), Result))
		TestEqual(TEXT("primary healing above 35 percent does not grant the conditional supplemental heal"), Unit(LowHealth, TEXT("Hero"))->HP, 370);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHealerCleanseRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HealerCleanseTypesAndArmor", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerCleanseRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime, TEXT("Profession.Healer.QingXinSan"), EGameXXKCardQuality::Common, EGameXXKCharacterRole::Healer)) return false;
	OpenFormula(Runtime, TEXT("Profession.Healer.QingXinSan"));
	OpenFormula(Runtime, TEXT("Profession.Healer.ZhiXueCao"));
	FGameXXKCardPlayResult Result;
	for (int32 Pass = 0; Pass < 3; ++Pass)
	{
		if (Pass > 0) ReturnMainToHand(Runtime);
		if (Pass == 2) ++Runtime.RoundNumber;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Bleed, 70);
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Hero")), EGameXXKCardStatus::DamageOverTime, 60);
		if (!Play(*this, Runtime, TEXT("Hero"), Result)) continue;
		TestEqual(TEXT("all Bleed removed"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::Bleed), 0);
		TestEqual(TEXT("all Rot removed"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::DamageOverTime), 0);
		TestEqual(TEXT("one Medicine per cleared type, three per round"), Status(Runtime, TEXT("Owner"), EGameXXKCardStatus::Medicine), Pass == 1 ? 1 : 2);
		for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")})
			TestEqual(TEXT("first successful Bleed clear grants 20 percent caster Defense each round"), Unit(Runtime, Id)->Armor, Pass == 2 ? 144 : 72);
	}
	FGameXXKCardBattleRuntime YaoWang;
	if (!Build(*this, YaoWang, TEXT("Profession.Healer.YaoWangGuiYuan"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Healer)) return false;
	for (EGameXXKCardStatus Dot : {EGameXXKCardStatus::Bleed, EGameXXKCardStatus::Poison, EGameXXKCardStatus::Burn, EGameXXKCardStatus::DamageOverTime})
		GameXXKCardRules::AddCombatStatus(*Unit(YaoWang, TEXT("Hero")), Dot, 70);
	if (Play(*this, YaoWang, TEXT("Hero"), Result))
	{
		for (EGameXXKCardStatus Dot : {EGameXXKCardStatus::Bleed, EGameXXKCardStatus::Poison, EGameXXKCardStatus::Burn})
			TestEqual(TEXT("YaoWang clears all of each authored DOT type"), Status(YaoWang, TEXT("Hero"), Dot), 0);
		TestEqual(TEXT("YaoWang preserves Rot"), Status(YaoWang, TEXT("Hero"), EGameXXKCardStatus::DamageOverTime), 70);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHealerFormulaIsolationRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HealerFormulaIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerFormulaIsolationRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime, TEXT("Profession.Healer.YaoNangFeiTou"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Healer)) return false;
	OpenFormula(Runtime, TEXT("Profession.Healer.XingQiZhen"));
	OpenFormula(Runtime, TEXT("Profession.Healer.CaoMuFuZhi"));
	OpenFormula(Runtime, TEXT("Profession.Healer.HuiChunLu"));
	OpenFormula(Runtime, TEXT("Hero.Healer.YiXueCuiFang"), TEXT("Hero"));
	for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")}) Unit(Runtime, Id)->HP = 500;
	FGameXXKCardPlayResult Result;
	if (Play(*this, Runtime, NAME_None, Result))
	{
		for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")})
			TestEqual(TEXT("XingQi formula loses one then heals Common coefficient10 at level100"), Unit(Runtime, Id)->HP, 549);
		TestEqual(TEXT("formula heal does not satisfy CaoMu"), Status(Runtime, TEXT("Owner"), EGameXXKCardStatus::Medicine), 0);
		TestEqual(TEXT("formula self-loss does not satisfy Hero formula"), Status(Runtime, TEXT("Hero"), EGameXXKCardStatus::Medicine), 0);
		TestEqual(TEXT("formula heals do not draw through HuiChun"), Runtime.Deck.Hand.Num(), 0);
	}
	if (!Build(*this, Runtime, TEXT("Profession.Healer.HuiChunLu"), EGameXXKCardQuality::Rare, EGameXXKCharacterRole::Healer)) return false;
	OpenFormula(Runtime, TEXT("Profession.Healer.XingQiZhen"));
	Runtime.HealerFormulas[0].PhaseProgress = 5;
	OpenFormula(Runtime, TEXT("Profession.Healer.CaoMuFuZhi"));
	Unit(Runtime, TEXT("Hero"))->HP = 500;
	if (Play(*this, Runtime, TEXT("Hero"), Result))
	{
		TestEqual(TEXT("CaoMu still reacts to original healing"), Status(Runtime, TEXT("Owner"), EGameXXKCardStatus::Medicine), 2);
		TestEqual(TEXT("formula-generated Medicine cannot advance another formula"), Runtime.HealerFormulas[0].PhaseProgress, 5);
		TestEqual(TEXT("no recursive Energy refund"), Runtime.Deck.SharedEnergy, 96);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHunterHeavyRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HunterHeavyScalingAndIgnore", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterHeavyRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	for (EGameXXKCardQuality Quality : {EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime, TEXT("Profession.Hunter.ChuanYang"), Quality, EGameXXKCharacterRole::Hunter)) return false;
		Unit(Runtime, TEXT("EnemyA"))->Defense = 100;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Owner")), EGameXXKCardStatus::Charge, 2);
		FGameXXKCardPlayResult Result;
		if (Play(*this, Runtime, TEXT("EnemyA"), Result))
			TestEqual(TEXT("one quality/level generation for base and per-Charge ignored Defense"), 1000000 - Unit(Runtime, TEXT("EnemyA"))->HP, Quality == EGameXXKCardQuality::Rare ? 260 : 320);
		if (!Build(*this, Runtime, TEXT("Profession.Hunter.LianZhuJian"), Quality, EGameXXKCharacterRole::Hunter)) return false;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Owner")), EGameXXKCardStatus::Charge, 2);
		if (Play(*this, Runtime, TEXT("EnemyA"), Result))
		{
			TestEqual(TEXT("base and two Heavy hits scale, each resolves the unconsumed Bleed reservoir"), 1000000 - Unit(Runtime, TEXT("EnemyA"))->HP, Quality == EGameXXKCardQuality::Rare ? 324 : 378);
			TestEqual(TEXT("Poison uses coefficient6 without an extra tick"), Status(Runtime, TEXT("EnemyA"), EGameXXKCardStatus::Poison), Quality == EGameXXKCardQuality::Rare ? 36 : 42);
		}
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime, TEXT("Profession.Hunter.FuBu"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Hunter)) return false;
	FGameXXKCardPlayResult Result;
	if (Play(*this, Runtime, NAME_None, Result))
		TestEqual(TEXT("setup snapshots coefficient6 as42 ignored Defense at level100/Epic"), Runtime.PendingHunterHeavyArrowIgnoreDefense.FindRef(TEXT("Owner")), 42);
	if (!Build(*this, Runtime, TEXT("Profession.Hunter.FuZuShi"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Hunter)) return false;
	GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Owner")), EGameXXKCardStatus::Charge, 2);
	if (Play(*this, Runtime, TEXT("EnemyA"), Result))
	{
		TestEqual(TEXT("additional primary coefficient20 scales to28 per Charge"), Result.HeavyArrowPrimaryBonusPercent, 56);
		TestEqual(TEXT("two Charge remain exactly two extra explosions"), Result.HeavyArrowToxicExplosionCount, 2);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHunterResourceRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HunterResourcesAndAgility", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHunterResourceRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	for (int32 Charge = 1; Charge <= 6; ++Charge)
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime, TEXT("Profession.Hunter.LueYingJian"), EGameXXKCardQuality::Common, EGameXXKCharacterRole::Hunter)) return false;
		GameXXKCardRules::AddCombatStatus(*Unit(Runtime, TEXT("Owner")), EGameXXKCardStatus::Charge, Charge);
		FGameXXKCardPlayResult Result;
		if (Play(*this, Runtime, TEXT("EnemyA"), Result))
			TestEqual(TEXT("one Agility per two Charge with cap two"), Status(Runtime, TEXT("Owner"), EGameXXKCardStatus::Agility), FMath::Min(2, Charge / 2));
	}
	for (const TCHAR* CardId : {TEXT("Profession.Hunter.YingYan"), TEXT("Profession.Hunter.HuiHuanJian")})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!Build(*this, Runtime, CardId, EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Hunter)) return false;
		FGameXXKCardPlayResult Result;
		if (Play(*this, Runtime, FString(CardId).EndsWith(TEXT("YingYan")) ? NAME_None : FName(TEXT("EnemyA")), Result))
			TestEqual(TEXT("printed Energy1 has no base refund"), Runtime.Deck.SharedEnergy, 98);
	}
	FGameXXKCardBattleRuntime YinZong;
	if (!Build(*this, YinZong, TEXT("Profession.Hunter.YinZong"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Hunter)) return false;
	FGameXXKCardPlayResult Result;
	if (Play(*this, YinZong, NAME_None, Result))
		TestEqual(TEXT("Epic YinZong initial Charge is explicitly two"), Status(YinZong, TEXT("Owner"), EGameXXKCardStatus::Charge), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKHealerFormulaQualityRebalanceTest,
	"GameXXK.Data.PartnerCards.Rebalance.HealerFormulaQualityAndThreshold", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKHealerFormulaQualityRebalanceTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKPartnerHealerHunterRebalanceTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!Build(*this, Runtime, TEXT("Profession.Healer.XingQiZhen"), EGameXXKCardQuality::Epic, EGameXXKCharacterRole::Healer)) return false;
	FGameXXKCardPlayResult Result;
	if (Play(*this, Runtime, NAME_None, Result))
	{
		ReturnMainToHand(Runtime);
		for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")}) Unit(Runtime, Id)->HP = 500;
		if (Play(*this, Runtime, NAME_None, Result))
			for (const TCHAR* Id : {TEXT("Owner"), TEXT("Hero"), TEXT("Npc")})
				TestEqual(TEXT("opened Epic formula retains its own quality for coefficient10"), Unit(Runtime, Id)->HP, 568);
	}
	if (!Build(*this, Runtime, TEXT("Profession.Healer.WenYangGao"), EGameXXKCardQuality::Rare, EGameXXKCharacterRole::Healer)) return false;
	Unit(Runtime, TEXT("Hero"))->HP = 500;
	if (Play(*this, Runtime, TEXT("Hero"), Result))
		TestEqual(TEXT("Rare WenYang secondary Armor is30 percent caster Defense times1.2"), Unit(Runtime, TEXT("Hero"))->Armor, 129);
	for (const bool bAboveThreshold : {false, true})
	{
		if (!Build(*this, Runtime, bAboveThreshold ? TEXT("Profession.Healer.CaoMuFuZhi") : TEXT("Profession.Healer.ZhiXueCao"), EGameXXKCardQuality::Common, EGameXXKCharacterRole::Healer)) return false;
		OpenFormula(Runtime, TEXT("Profession.Healer.WenYangGao"));
		Unit(Runtime, TEXT("Hero"))->HP = 500;
		if (Play(*this, Runtime, TEXT("Hero"), Result))
			TestEqual(TEXT("Rare formula threshold is coefficient20 at level100, yielding20 percent Defense Armor"), Unit(Runtime, TEXT("Hero"))->Armor, bAboveThreshold ? 86 : 0);
	}
	return true;
}

#endif
