#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentSetCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKQingNangSetRuntimeTest
{
	constexpr uint8 QingNangHookValue = 17;
	constexpr uint8 QingNangModifierKindValue = 39;

	EGameXXKEquipmentSetBonusHook ExpectedHook()
	{
		return static_cast<EGameXXKEquipmentSetBonusHook>(QingNangHookValue);
	}

	EGameXXKEquipmentModifierKind ExpectedModifierKind()
	{
		return static_cast<EGameXXKEquipmentModifierKind>(QingNangModifierKindValue);
	}

	EGameXXKEquipmentSetBonusKind ExpectedBonusKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 22 : Pieces == 4 ? 23 : 24);
	}

	const TCHAR* ExpectedDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("每回合首次打出2费及以上牌：抽1张牌。");
		case 4: return TEXT("每回合首次打出2费及以上牌：抽1张牌；全队失去至多1点气血，再回复2点。");
		case 6: return TEXT("每回合首次打出2费及以上牌：抽1张牌；全队失去至多1点气血，再回复2点；回复1点气力。");
		default: return TEXT("");
		}
	}

	FGameXXKEquipmentActiveEffect MakeEffect(const int32 Pieces, const FName SourceId)
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = FName(*FString::Printf(TEXT("Set.QingNang.%d"), Pieces));
		Effect.SourceCharacterId = SourceId;
		Effect.Set = EGameXXKEquipmentSet::QingNang;
		Effect.RequiredPieces = Pieces;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Team;
		Effect.Hook = ExpectedHook();
		Effect.ModifierKind = ExpectedModifierKind();
		Effect.Magnitude = 1;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::FlatCount;
		Effect.MaxTriggersPerRound = 1;
		return Effect;
	}

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Sort,
		const EGameXXKCharacterRole Role = EGameXXKCharacterRole::Healer)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party ? Role : EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 100 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = Sort;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const TCHAR* InstanceId, const TCHAR* CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = TEXT("Hero");
		Card.SourceEntryId = FName(*FString::Printf(TEXT("QingNang.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	bool BuildRuntime(FAutomationTestBase& Test, const int32 Pieces, FGameXXKCardBattleRuntime& OutRuntime)
	{
		const TArray<FGameXXKCardInstance> Cards = {
			MakeCard(TEXT("HighA"), TEXT("Hero.Generic.JianYiGuanHong"), 0),
			MakeCard(TEXT("HighB"), TEXT("Hero.Generic.JianYiGuanHong"), 1),
			MakeCard(TEXT("Cheap"), TEXT("Hero.Generic.QingFengYiShi"), 2),
			MakeCard(TEXT("RewardA"), TEXT("Hero.Generic.FengShenBu"), 3),
			MakeCard(TEXT("RewardB"), TEXT("Hero.Generic.GuanXi"), 4)};
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 1),
			MakeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, 2, EGameXXKCharacterRole::Hunter),
			MakeUnit(TEXT("Fragile"), EGameXXKCardTargetSide::Party, 3, EGameXXKCharacterRole::Blade),
			MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, 10, EGameXXKCharacterRole::Invalid)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, 61201 + Pieces, &Error))
		{
			Test.AddError(FString::Printf(TEXT("QingNang fixture initializes: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = {Cards[0], Cards[1], Cards[2]};
		OutRuntime.Deck.DrawPile = {Cards[4], Cards[3]};
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 3;
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = OutRuntime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = MakeEffect(Pieces, TEXT("Hero"));
		RuntimeEffect.SourceCharacterId = TEXT("Hero");
		return true;
	}

	bool PlayHighCost(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Label)
	{
		FString Error;
		return Test.TestTrue(
			FString::Printf(TEXT("%s resolves: %s"), Label, *Error),
			GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TEXT("Enemy"), OutResult, &Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingNangCatalogContractTest,
	"GameXXK.Equipment.QingNang.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingNangCatalogContractTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKQingNangSetRuntimeTest;
	for (const int32 Pieces : {2, 4, 6})
	{
		const FName Id(*FString::Printf(TEXT("Set.QingNang.%d"), Pieces));
		const FGameXXKEquipmentSetBonusDefinition* Definition = FGameXXKEquipmentSetCatalog::FindDefinition(Id);
		TestNotNull(FString::Printf(TEXT("%s resolves"), *Id.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(TEXT("QingNang uses its redesigned serialized kind"), Definition->BonusKind, ExpectedBonusKind(Pieces));
		TestEqual(TEXT("QingNang is team-unique at every threshold"), Definition->Scope, EGameXXKEquipmentSetBonusScope::Team);
		TestEqual(TEXT("QingNang uses the paid-high-cost active-card hook"), Definition->Hook, ExpectedHook());
		TestEqual(TEXT("QingNang uses one cumulative tier payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
		TestEqual(TEXT("QingNang uses one trigger payload"), Definition->Value, 1);
		TestEqual(TEXT("QingNang triggers once per player round"), Definition->TriggersPerRound, 1);
		TestEqual(TEXT("QingNang exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedDescription(Pieces)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingNangTeamUniqueSelectionTest,
	"GameXXK.Equipment.QingNang.TeamUniqueChoosesHighestTier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingNangTeamUniqueSelectionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKQingNangSetRuntimeTest;
	FGameXXKEquipmentLoadoutSnapshot Hero;
	Hero.CharacterId = TEXT("Hero");
	Hero.TeamEffectSourceScore = 10;
	Hero.CandidateTeamEffects = {
		MakeEffect(2, TEXT("Hero")),
		MakeEffect(4, TEXT("Hero")),
		MakeEffect(6, TEXT("Hero"))};
	FGameXXKEquipmentLoadoutSnapshot Companion;
	Companion.CharacterId = TEXT("Companion");
	Companion.TeamEffectSourceScore = 99;
	Companion.CandidateTeamEffects = {MakeEffect(4, TEXT("Companion"))};
	const TArray<FGameXXKEquipmentActiveEffect> Effects = FGameXXKEquipmentRules::ResolveTeamEffects({Hero, Companion});
	TestEqual(TEXT("QingNang materializes exactly one team effect"), Effects.Num(), 1);
	if (Effects.Num() == 1)
	{
		TestEqual(TEXT("the highest unlocked tier wins before source score"), Effects[0].RequiredPieces, 6);
		TestEqual(TEXT("the winning six-piece source is stable"), Effects[0].SourceCharacterId, FName(TEXT("Hero")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingNangTierRuntimeTest,
	"GameXXK.Equipment.QingNang.CumulativeTierRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingNangTierRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKQingNangSetRuntimeTest;
	for (const int32 Pieces : {2, 4, 6})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Pieces, Runtime))
		{
			continue;
		}
		FindUnit(Runtime, TEXT("Hero"))->HP = 100;
		FindUnit(Runtime, TEXT("Ally"))->HP = 50;
		FindUnit(Runtime, TEXT("Fragile"))->HP = 1;
		FGameXXKCardPlayResult Result;
		if (!PlayHighCost(*this, Runtime, TEXT("HighA"), Result, TEXT("the first paid two-cost card")))
		{
			continue;
		}
		TestEqual(TEXT("every QingNang tier draws exactly one card"), Runtime.Deck.Hand.Num(), 3);
		TestEqual(TEXT("two-piece never changes party health"), FindUnit(Runtime, TEXT("Hero"))->HP, 100);
		TestEqual(TEXT("four-piece and above heal a wounded ally by one net point"), FindUnit(Runtime, TEXT("Ally"))->HP, Pieces >= 4 ? 51 : 50);
		TestEqual(TEXT("nonlethal loss leaves a one-HP ally alive before healing two"), FindUnit(Runtime, TEXT("Fragile"))->HP, Pieces >= 4 ? 3 : 1);
		TestEqual(TEXT("only six-piece refunds one shared Energy"), Runtime.Deck.SharedEnergy, Pieces >= 6 ? 2 : 1);
		TestEqual(TEXT("the team-unique trigger is spent once"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 1);

		if (Pieces == 6)
		{
			const int32 HandBeforeSecond = Runtime.Deck.Hand.Num();
			FGameXXKCardPlayResult SecondResult;
			if (PlayHighCost(*this, Runtime, TEXT("HighB"), SecondResult, TEXT("the second paid two-cost card")))
			{
				TestEqual(TEXT("a second qualifying card does not draw again"), Runtime.Deck.Hand.Num(), HandBeforeSecond - 1);
				TestEqual(TEXT("a second qualifying card does not refund Energy again"), Runtime.Deck.SharedEnergy, 0);
				TestEqual(TEXT("the trigger count remains one"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 1);
			}
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingNangMedicineSynergyTest,
	"GameXXK.Equipment.QingNang.HealthCycleFeedsMedicineFormula",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingNangMedicineSynergyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKQingNangSetRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, 4, Runtime))
	{
		return true;
	}
	const FGameXXKCardDefinition* FormulaDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Profession.Healer.YaoYin"));
	TestNotNull(TEXT("the Yin-Yang health-change formula resolves"), FormulaDefinition);
	if (!FormulaDefinition)
	{
		return true;
	}
	FGameXXKHealerFormulaRuntime& Formula = Runtime.HealerFormulas.AddDefaulted_GetRef();
	Formula.OwnerUnitId = TEXT("Hero");
	Formula.SourceCardId = FormulaDefinition->Id;
	Formula.Kind = FormulaDefinition->HealerRule.FormulaKind;
	FindUnit(Runtime, TEXT("Hero"))->HP = 100;
	FindUnit(Runtime, TEXT("Ally"))->HP = 50;
	FindUnit(Runtime, TEXT("Fragile"))->HP = 1;

	FGameXXKCardPlayResult Result;
	if (PlayHighCost(*this, Runtime, TEXT("HighA"), Result, TEXT("the Medicine-synergy high-cost card")))
	{
		TestEqual(TEXT("one attack plus two real losses and three real heals grant Medicine6"),
			GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Medicine),
			6);
		TestEqual(TEXT("the cumulative Medicine6 threshold grants Momentum1"),
			GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Momentum),
			1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQingNangPendingChoiceDrawTest,
	"GameXXK.Equipment.QingNang.PendingChoiceDefersTriggeredDrawWithoutRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQingNangPendingChoiceDrawTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKQingNangSetRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, 2, Runtime))
	{
		return false;
	}
	FGameXXKCardInstance* ChoiceCard = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == FName(TEXT("HighA"));
	});
	TestNotNull(TEXT("the qualifying two-cost choice card resolves"), ChoiceCard);
	if (!ChoiceCard)
	{
		return false;
	}
	ChoiceCard->CardId = TEXT("Profession.FormationMaster.BaMenLunZhuan");

	FGameXXKCardPlayResult ChoiceResult;
	FString ChoiceError;
	const bool bChoiceResolved = GameXXKCardRules::ResolveCardPlay(
		Runtime,
		TEXT("HighA"),
		NAME_None,
		ChoiceResult,
		&ChoiceError);
	TestTrue(FString::Printf(TEXT("the qualifying card may open a blocking choice: %s"), *ChoiceError), bChoiceResolved);
	if (!bChoiceResolved)
	{
		return true;
	}
	TestEqual(TEXT("the card's own forced discard remains active"),
		Runtime.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("the QingNang once-per-round trigger is committed"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 1);
	TestEqual(TEXT("BaMen draws the two available non-resolving cards before its forced discard while the QingNang draw waits"), Runtime.Deck.Hand.Num(), 4);
	TestFalse(TEXT("BaMen cannot redraw its own still-resolving instance"),
		Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == FName(TEXT("HighA"));
		}));
	TestTrue(TEXT("BaMen remains in discard until its synchronous transaction finishes"),
		Runtime.Deck.DiscardPile.ContainsByPredicate([](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == FName(TEXT("HighA"));
		}));
	TestEqual(TEXT("exactly one QingNang draw is deferred behind the choice"), Runtime.PendingTriggeredDrawCount, 1);
	if (Runtime.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard || Runtime.Deck.Hand.IsEmpty())
	{
		return true;
	}

	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("forced discard releases the deferred QingNang draw: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(
			Runtime,
			{Runtime.Deck.Hand[0].InstanceId},
			&Error,
			&ResumedResults));
	TestEqual(TEXT("discard one then materialize the one deferred QingNang draw"), Runtime.Deck.Hand.Num(), 4);
	TestEqual(TEXT("the deferred QingNang draw is consumed exactly once"), Runtime.PendingTriggeredDrawCount, 0);
	return true;
}

#endif
