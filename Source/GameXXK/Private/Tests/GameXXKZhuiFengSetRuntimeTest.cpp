#include "GameXXKCardRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentSetCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKZhuiFengSetRuntimeTest
{
	constexpr uint8 ZhuiFengHookValue = 21;
	constexpr uint8 ZhuiFengModifierKindValue = 41;

	constexpr EGameXXKEquipmentSlot OrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	EGameXXKEquipmentSetBonusHook ExpectedHook()
	{
		return static_cast<EGameXXKEquipmentSetBonusHook>(ZhuiFengHookValue);
	}

	EGameXXKEquipmentModifierKind ExpectedModifierKind()
	{
		return static_cast<EGameXXKEquipmentModifierKind>(ZhuiFengModifierKindValue);
	}

	EGameXXKEquipmentSetBonusKind ExpectedBonusKind(const int32 Pieces)
	{
		return static_cast<EGameXXKEquipmentSetBonusKind>(Pieces == 2 ? 28 : Pieces == 4 ? 29 : 30);
	}

	const TCHAR* ExpectedDescription(const int32 Pieces)
	{
		switch (Pieces)
		{
		case 2: return TEXT("全队每主动打出2张牌，抽1张牌。");
		case 4: return TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回复1点气力。");
		case 6: return TEXT("全队每主动打出2张牌，抽1张牌；每回合第2张回1气，第4张再回1气、全队蓄力1并抽1张。");
		default: return TEXT("");
		}
	}

	FGameXXKEquipmentActiveEffect MakeExpectedEffect(
		const int32 Pieces,
		const FName SourceCharacterId)
	{
		FGameXXKEquipmentActiveEffect Effect;
		Effect.EffectId = FName(*FString::Printf(TEXT("Set.ZhuiFeng.%d"), Pieces));
		Effect.SourceCharacterId = SourceCharacterId;
		Effect.Set = EGameXXKEquipmentSet::ZhuiFeng;
		Effect.RequiredPieces = Pieces;
		Effect.Scope = EGameXXKEquipmentSetBonusScope::Team;
		Effect.Hook = ExpectedHook();
		Effect.ModifierKind = ExpectedModifierKind();
		Effect.Magnitude = 1;
		Effect.Unit = EGameXXKEquipmentMagnitudeUnit::FlatCount;
		Effect.MaxTriggersPerRound = 0;
		return Effect;
	}

	FGameXXKCardCombatUnit MakeUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Side == EGameXXKCardTargetSide::Party
			? EGameXXKCharacterRole::Hero
			: EGameXXKCharacterRole::Invalid;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Party ? 200 : 5000;
		Unit.MaxHP = Unit.HP;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FString& InstanceId,
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*InstanceId);
		Card.CardId = TEXT("Hero.Generic.HeYuZhan");
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = TEXT("Hero");
		Card.SourceEntryId = FName(*FString::Printf(TEXT("ZhuiFeng.Source.%d"), Ordinal));
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

	bool ProjectHighestZhuiFengEffect(
		FAutomationTestBase& Test,
		const int32 Pieces,
		FGameXXKEquipmentActiveEffect& OutEffect)
	{
		FGameXXKEquipmentCollectionState Collection;
		Collection.CollectionSeed = 0x5A4647;
		FGameXXKCompanionRosterState EmptyRoster;
		for (int32 Index = 0; Index < Pieces; ++Index)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = EGameXXKEquipmentSet::ZhuiFeng;
			Request.Quality = EGameXXKEquipmentQuality::Common;
			Request.ItemLevel = 1;
			Request.bForceSlot = true;
			Request.ForcedSlot = OrderedSlots[Index];
			FName InstanceId;
			FString Error;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, InstanceId, &Error))
			{
				Test.AddError(FString::Printf(TEXT("ZhuiFeng fixture item creation failed: %s"), *Error));
				return false;
			}
			const FGameXXKEquipmentTransactionResult EquipResult = FGameXXKEquipmentRules::EquipInstance(
				Collection,
				EmptyRoster,
				FGameXXKEquipmentRules::HeroCharacterId(),
				OrderedSlots[Index],
				InstanceId);
			if (!EquipResult.bSucceeded)
			{
				Test.AddError(FString::Printf(TEXT("ZhuiFeng fixture equip failed: %s"), *EquipResult.Message.ToString()));
				return false;
			}
		}

		FGameXXKCharacterStats BareStats;
		BareStats.MaxHealth = 200;
		BareStats.MaxMana = 100;
		BareStats.Attack = 10;
		BareStats.Defense = 0;
		BareStats.Speed = 1;
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		FString Error;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			Collection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			BareStats,
			Snapshot,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("ZhuiFeng fixture projection failed: %s"), *Error));
			return false;
		}

		TArray<FGameXXKEquipmentActiveEffect> Candidates = Snapshot.ActivePersonalEffects;
		Candidates.Append(Snapshot.CandidateTeamEffects);
		const FGameXXKEquipmentActiveEffect* Highest = nullptr;
		for (const FGameXXKEquipmentActiveEffect& Candidate : Candidates)
		{
			if (Candidate.Set == EGameXXKEquipmentSet::ZhuiFeng
				&& Candidate.RequiredPieces <= Pieces
				&& (!Highest || Candidate.RequiredPieces > Highest->RequiredPieces))
			{
				Highest = &Candidate;
			}
		}
		Test.TestNotNull(TEXT("ZhuiFeng fixture projects its highest reached threshold"), Highest);
		if (!Highest)
		{
			return false;
		}
		OutEffect = *Highest;
		OutEffect.SourceCharacterId = TEXT("Hero");
		return true;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const int32 Pieces,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			Cards.Add(MakeCard(FString::Printf(TEXT("Card%d"), Index + 1), Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, 1),
			MakeUnit(TEXT("Ally"), EGameXXKCardTargetSide::Party, 2),
			MakeUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, 10)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			72200 + Pieces,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("ZhuiFeng runtime failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand = {Cards[0], Cards[1], Cards[2], Cards[3], Cards[4], Cards[5]};
		OutRuntime.Deck.DrawPile = {Cards[9], Cards[8], Cards[7], Cards[6]};
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 10;
		FGameXXKEquipmentActiveEffect Effect;
		if (!ProjectHighestZhuiFengEffect(Test, Pieces, Effect))
		{
			return false;
		}
		FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = OutRuntime.EquipmentEffects.AddDefaulted_GetRef();
		RuntimeEffect.ActiveEffect = Effect;
		RuntimeEffect.SourceCharacterId = TEXT("Hero");
		return true;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const TCHAR* Label)
	{
		FGameXXKCardPlayResult Result;
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(
			Runtime,
			InstanceId,
			TEXT("Enemy"),
			Result,
			&Error);
		Test.TestTrue(Label, bResolved);
		if (!bResolved)
		{
			Test.AddError(Error);
		}
		return bResolved;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuiFengCatalogContractTest,
	"GameXXK.Equipment.ZhuiFeng.CatalogContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuiFengCatalogContractTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKZhuiFengSetRuntimeTest;
	for (const int32 Pieces : {2, 4, 6})
	{
		const FName Id(*FString::Printf(TEXT("Set.ZhuiFeng.%d"), Pieces));
		const FGameXXKEquipmentSetBonusDefinition* Definition = FGameXXKEquipmentSetCatalog::FindDefinition(Id);
		TestNotNull(FString::Printf(TEXT("%s resolves"), *Id.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		TestEqual(TEXT("ZhuiFeng uses its redesigned serialized kind"), Definition->BonusKind, ExpectedBonusKind(Pieces));
		TestEqual(TEXT("ZhuiFeng is team-unique at every threshold"), Definition->Scope, EGameXXKEquipmentSetBonusScope::Team);
		TestEqual(TEXT("ZhuiFeng uses the active-card count hook"), Definition->Hook, ExpectedHook());
		TestEqual(TEXT("ZhuiFeng uses one flat reward payload"), Definition->Unit, EGameXXKEquipmentMagnitudeUnit::FlatCount);
		TestEqual(TEXT("ZhuiFeng uses a one-count reward"), Definition->Value, 1);
		TestEqual(TEXT("ZhuiFeng owns a custom repeatable round counter"), Definition->TriggersPerRound, 0);
		TestEqual(TEXT("ZhuiFeng exposes the approved concise tooltip"), Definition->Description.ToString(), FString(ExpectedDescription(Pieces)));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuiFengTeamUniqueSelectionTest,
	"GameXXK.Equipment.ZhuiFeng.TeamUniqueChoosesHighestTier",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuiFengTeamUniqueSelectionTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKZhuiFengSetRuntimeTest;
	FGameXXKEquipmentLoadoutSnapshot Hero;
	Hero.CharacterId = TEXT("Hero");
	Hero.TeamEffectSourceScore = 10;
	Hero.CandidateTeamEffects = {
		MakeExpectedEffect(2, TEXT("Hero")),
		MakeExpectedEffect(4, TEXT("Hero")),
		MakeExpectedEffect(6, TEXT("Hero"))};
	FGameXXKEquipmentLoadoutSnapshot Companion;
	Companion.CharacterId = TEXT("Companion");
	Companion.TeamEffectSourceScore = 99;
	Companion.CandidateTeamEffects = {MakeExpectedEffect(4, TEXT("Companion"))};
	const TArray<FGameXXKEquipmentActiveEffect> Effects = FGameXXKEquipmentRules::ResolveTeamEffects({Hero, Companion});
	TestEqual(TEXT("ZhuiFeng materializes exactly one team effect"), Effects.Num(), 1);
	if (Effects.Num() == 1)
	{
		TestEqual(TEXT("the highest unlocked tier wins before source score"), Effects[0].RequiredPieces, 6);
		TestEqual(TEXT("the winning six-piece source is stable"), Effects[0].SourceCharacterId, FName(TEXT("Hero")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuiFengCumulativeRuntimeTest,
	"GameXXK.Equipment.ZhuiFeng.CumulativeActiveCardRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuiFengCumulativeRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKZhuiFengSetRuntimeTest;
	for (const int32 Pieces : {2, 4, 6})
	{
		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(*this, Pieces, Runtime))
		{
			continue;
		}
		for (int32 Index = 1; Index <= 6; ++Index)
		{
			if (!Resolve(*this, Runtime, FName(*FString::Printf(TEXT("Card%d"), Index)), TEXT("a counted active card resolves")))
			{
				break;
			}
		}
		TestEqual(TEXT("six real card submissions are counted exactly"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 6);
		TestEqual(TEXT("two-piece draw repeats at cards two, four, and six, while six-piece adds one threshold draw"),
			Runtime.Deck.Hand.Num(),
			Pieces >= 6 ? 4 : 3);
		TestEqual(TEXT("only four-piece and above refund on card two, and six-piece refunds again on card four"),
			Runtime.Deck.SharedEnergy,
			Pieces >= 6 ? 6 : Pieces >= 4 ? 5 : 4);
		TestEqual(TEXT("only six-piece grants one Charge to the Hero"),
			GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, TEXT("Hero")), EGameXXKCardStatus::Charge),
			Pieces >= 6 ? 1 : 0);
		TestEqual(TEXT("only six-piece grants one Charge to the ally"),
			GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, TEXT("Ally")), EGameXXKCardStatus::Charge),
			Pieces >= 6 ? 1 : 0);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuiFengBladeDoubleCountTest,
	"GameXXK.Equipment.ZhuiFeng.BladeDoubleCountAdvancesThreshold",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuiFengBladeDoubleCountTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKZhuiFengSetRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, 2, Runtime))
	{
		return false;
	}
	Runtime.PendingBladeCharge.Rule = EGameXXKBladeChargeRule::CountNextActiveTwice;
	Runtime.PendingBladeCharge.SourceCardId = TEXT("Profession.Blade.ZhanJin");
	Runtime.PendingBladeCharge.SourceQuality = EGameXXKCardQuality::Common;
	Runtime.PendingBladeCharge.SourceOwnerUnitId = TEXT("Hero");
	Runtime.PendingBladeCharge.CreatedRound = Runtime.RoundNumber;
	if (!Resolve(*this, Runtime, TEXT("Card1"), TEXT("a Blade double-count consumer resolves")))
	{
		return true;
	}
	TestEqual(TEXT("the Blade mechanism may count the card twice for Blade sequencing"), Runtime.ActiveCardsPlayedThisRound, 2);
	TestEqual(TEXT("Blade Double Count advances equipment thresholds by two"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 2);
	TestEqual(TEXT("Blade Double Count reaches the two-card ZhuiFeng draw threshold"), Runtime.Deck.Hand.Num(), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKZhuiFengPendingChoiceDrawTest,
	"GameXXK.Equipment.ZhuiFeng.PendingChoiceDefersThresholdDrawWithoutRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKZhuiFengPendingChoiceDrawTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKZhuiFengSetRuntimeTest;
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, 2, Runtime))
	{
		return false;
	}
	FGameXXKCardInstance* ChoiceCard = Runtime.Deck.Hand.FindByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == FName(TEXT("Card2"));
	});
	TestNotNull(TEXT("the second active-card fixture resolves"), ChoiceCard);
	if (!ChoiceCard)
	{
		return false;
	}
	ChoiceCard->CardId = TEXT("Hero.Generic.FengShenBu");
	if (!Resolve(*this, Runtime, TEXT("Card1"), TEXT("the first counted active card resolves")))
	{
		return true;
	}
	FGameXXKCardPlayResult ChoiceResult;
	FString ChoiceError;
	const bool bChoiceResolved = GameXXKCardRules::ResolveCardPlay(
		Runtime,
		TEXT("Card2"),
		TEXT("Ally"),
		ChoiceResult,
		&ChoiceError);
	TestTrue(FString::Printf(TEXT("the threshold card may open a blocking choice: %s"), *ChoiceError), bChoiceResolved);
	if (!bChoiceResolved)
	{
		return true;
	}
	TestEqual(TEXT("the card's own forced-discard choice remains active"),
		Runtime.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("the successful threshold card is still counted"), Runtime.EquipmentEffects[0].CurrentRoundTriggerCount, 2);
	TestEqual(TEXT("the ZhuiFeng draw waits while the card's choice is blocking"), Runtime.Deck.Hand.Num(), 6);
	if (Runtime.Deck.PendingChoice.Kind != EGameXXKCardPendingChoiceKind::ForcedDiscard || Runtime.Deck.Hand.IsEmpty())
	{
		return true;
	}
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("the forced discard resolves and releases the deferred ZhuiFeng draw: %s"), *Error),
		GameXXKCardRules::SubmitForcedDiscard(
			Runtime,
			{Runtime.Deck.Hand[0].InstanceId},
			&Error,
			&ResumedResults));
	TestEqual(TEXT("the deferred one-card reward materializes after the discard"), Runtime.Deck.Hand.Num(), 6);
	return true;
}

#endif
