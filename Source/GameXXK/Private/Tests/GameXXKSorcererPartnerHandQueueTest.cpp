#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerHandQueueTest
{
	const FName SorcererId(TEXT("Partner.Sorcerer"));
	const FName OtherSorcererId(TEXT("Partner.OtherSorcerer"));
	const FName HeroId(TEXT("Hero"));
	const FName EnemyId(TEXT("Enemy.A"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 1000 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName CardId,
		const FName OwnerUnitId,
		const FString& InstanceId,
		const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*InstanceId);
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Source.%s"), *InstanceId));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FGameXXKCardCombatUnit>& Units,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer hand-queue fixture failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.SharedEnergy = 20;
		return true;
	}

	bool InstallZones(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FGameXXKCardInstance>& Hand,
		const TArray<FGameXXKCardInstance>& Draw,
		const TArray<FGameXXKCardInstance>& Discard)
	{
		Runtime.Deck.Hand = Hand;
		Runtime.Deck.DrawPile = Draw;
		Runtime.Deck.DiscardPile = Discard;
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.PendingAutomaticHandCards.Reset();
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("installed Sorcerer hand-queue zones are invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool MoveFixtureInstanceToHand(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId)
	{
		for (TArray<FGameXXKCardInstance>* Zone : {&Runtime.Deck.DrawPile, &Runtime.Deck.DiscardPile})
		{
			const int32 Index = Zone->IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == InstanceId;
			});
			if (Index != INDEX_NONE)
			{
				Runtime.Deck.Hand.Add(MoveTemp((*Zone)[Index]));
				Zone->RemoveAt(Index, 1, EAllowShrinking::No);
				FString Error;
				if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
				{
					Test.AddError(FString::Printf(TEXT("fixture hand move invalidated runtime: %s"), *Error));
					return false;
				}
				return true;
			}
		}
		Test.AddError(FString::Printf(TEXT("fixture could not find %s in draw or discard"), *InstanceId.ToString()));
		return false;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Label)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, NAME_None, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Label, *Error), bResolved);
		return bResolved;
	}

	bool ContainsInstance(const TArray<FGameXXKCardInstance>& Zone, const FName InstanceId)
	{
		return Zone.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
	}

	const FGameXXKSorcererPartnerTaskRuntime* FindTask(const FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.SorcererPartnerTasks.FindByPredicate([](const FGameXXKSorcererPartnerTaskRuntime& Task)
		{
			return Task.OwnerUnitId == SorcererId;
		});
	}

	TArray<uint8> SerializeRuntime(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		Writer.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(
			Writer,
			const_cast<FGameXXKCardBattleRuntime*>(&Runtime),
			nullptr);
		Writer.Close();
		return Bytes;
	}

	bool RoundTrip(
		FAutomationTestBase& Test,
		const FGameXXKCardBattleRuntime& Source,
		FGameXXKCardBattleRuntime& OutLoaded)
	{
		const TArray<uint8> Bytes = SerializeRuntime(Source);
		FMemoryReader Reader(Bytes, true);
		Reader.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(Reader, &OutLoaded, nullptr);
		Reader.Close();
		FString Error;
		const bool bValid = GameXXKCardRules::ValidateCardBattleRuntime(OutLoaded, &Error);
		Test.TestTrue(FString::Printf(TEXT("round-tripped automatic hand queue validates: %s"), *Error), bValid);
		return bValid;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererNonStarterUniversalAutoHandTest,
	"GameXXK.Data.PartnerCards.Sorcerer.HandQueue.NonStarterUniversalMovesOncePerBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererNonStarterUniversalAutoHandTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerHandQueueTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], SorcererId, FString::Printf(TEXT("Sorcerer.Card.%d"), Index), Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59301, Runtime)
		|| !InstallZones(*this, Runtime, {Cards[0]}, {Cards[1], Cards[2], Cards[3], Cards[4]}, {}))
	{
		return false;
	}

	FGameXXKCardPlayResult StarterResult;
	if (!Resolve(*this, Runtime, Cards[0].InstanceId, StarterResult, TEXT("non-Universal starter")))
	{
		return true;
	}
	TestTrue(TEXT("nonstarter Universal moves from draw to hand"), ContainsInstance(Runtime.Deck.Hand, Cards[1].InstanceId));
	TestFalse(TEXT("moved Universal leaves draw"), ContainsInstance(Runtime.Deck.DrawPile, Cards[1].InstanceId));
	const FGameXXKSorcererPartnerTaskRuntime* Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("Sorcerer task exists after starter"), Task))
	{
		return true;
	}
	TestEqual(TEXT("automatic Universal move is remembered once"), Task->AutoHandedUniversalCardIds, TArray<FName>{CardIds[1]});

	FGameXXKCardPlayResult UniversalResult;
	if (!Resolve(*this, Runtime, Cards[1].InstanceId, UniversalResult, TEXT("auto-handed Universal")))
	{
		return true;
	}
	for (int32 Index = 2; Index < CardIds.Num(); ++Index)
	{
		if (!MoveFixtureInstanceToHand(*this, Runtime, Cards[Index].InstanceId))
		{
			return true;
		}
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, Cards[Index].InstanceId, Result, TEXT("remaining distinct task card")))
		{
			return true;
		}
	}
	Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("owner history survives completed task"), Task))
	{
		return true;
	}
	TestFalse(TEXT("first task completed and reset"), Task->bActive);
	TestEqual(TEXT("Universal auto-hand history survives reset"), Task->AutoHandedUniversalCardIds, TArray<FName>{CardIds[1]});

	if (!MoveFixtureInstanceToHand(*this, Runtime, Cards[2].InstanceId))
	{
		return true;
	}
	FGameXXKCardPlayResult RestartResult;
	if (!Resolve(*this, Runtime, Cards[2].InstanceId, RestartResult, TEXT("same-battle non-Universal restart")))
	{
		return true;
	}
	TestFalse(TEXT("already auto-handed Universal is not moved a second time"), ContainsInstance(Runtime.Deck.Hand, Cards[1].InstanceId));
	TestTrue(TEXT("already auto-handed Universal remains in discard"), ContainsInstance(Runtime.Deck.DiscardPile, Cards[1].InstanceId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererUniversalStarterOverflowQueueTest,
	"GameXXK.Data.PartnerCards.Sorcerer.HandQueue.UniversalStarterOverflowRoundTripsAndMaterializes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererUniversalStarterOverflowQueueTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerHandQueueTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> SorcererCards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		SorcererCards.Add(MakeCard(CardIds[Index], SorcererId, FString::Printf(TEXT("Sorcerer.Card.%d"), Index), Index));
	}
	TArray<FGameXXKCardInstance> Fillers;
	for (int32 Index = 0; Index < 18; ++Index)
	{
		Fillers.Add(MakeCard(
			TEXT("Hero.Generic.QingFengYiShi"),
			HeroId,
			FString::Printf(TEXT("Filler.%02d"), Index),
			100 + Index));
	}
	TArray<FGameXXKCardInstance> AllCards = SorcererCards;
	AllCards.Append(Fillers);
	TArray<FGameXXKCardInstance> FullHand = {SorcererCards[0], SorcererCards[1]};
	FullHand.Append(Fillers);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, AllCards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
		MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59302, Runtime)
		|| !InstallZones(*this, Runtime, FullHand, {SorcererCards[3], SorcererCards[4]}, {SorcererCards[2]}))
	{
		return false;
	}
	TestEqual(TEXT("fixture begins at hard hand capacity"), Runtime.Deck.Hand.Num(), 20);

	FGameXXKCardPlayResult StarterResult;
	if (!Resolve(*this, Runtime, SorcererCards[0].InstanceId, StarterResult, TEXT("Universal starter at full hand")))
	{
		return true;
	}
	TestEqual(TEXT("one stable candidate fills the played card's hand slot"), Runtime.Deck.Hand.Num(), 20);
	TestTrue(TEXT("lowest acquisition candidate materializes first"), ContainsInstance(Runtime.Deck.Hand, SorcererCards[2].InstanceId));
	TestEqual(TEXT("two overflow candidates remain queued"), Runtime.Deck.PendingAutomaticHandCards.Num(), 2);
	if (Runtime.Deck.PendingAutomaticHandCards.Num() == 2)
	{
		TestEqual(TEXT("overflow queue first ID"), Runtime.Deck.PendingAutomaticHandCards[0].InstanceId, SorcererCards[3].InstanceId);
		TestEqual(TEXT("overflow queue second ID"), Runtime.Deck.PendingAutomaticHandCards[1].InstanceId, SorcererCards[4].InstanceId);
	}
	int32 AlreadyHandCount = 0;
	for (const FGameXXKCardInstance& Card : Runtime.Deck.Hand)
	{
		AlreadyHandCount += Card.InstanceId == SorcererCards[1].InstanceId ? 1 : 0;
	}
	TestEqual(TEXT("already-hand carried card remains singular"), AlreadyHandCount, 1);
	TestFalse(TEXT("queued card leaves draw"), ContainsInstance(Runtime.Deck.DrawPile, SorcererCards[3].InstanceId));
	TestFalse(TEXT("queued card leaves discard"), ContainsInstance(Runtime.Deck.DiscardPile, SorcererCards[2].InstanceId));

	FGameXXKCardBattleRuntime Loaded;
	if (!RoundTrip(*this, Runtime, Loaded))
	{
		return true;
	}
	TestEqual(TEXT("queued overflow order survives save"), Loaded.Deck.PendingAutomaticHandCards.Num(), 2);
	if (Loaded.Deck.PendingAutomaticHandCards.Num() == 2)
	{
		TestEqual(TEXT("loaded queue first ID"), Loaded.Deck.PendingAutomaticHandCards[0].InstanceId, SorcererCards[3].InstanceId);
		TestEqual(TEXT("loaded queue second ID"), Loaded.Deck.PendingAutomaticHandCards[1].InstanceId, SorcererCards[4].InstanceId);
	}

	for (FGameXXKCardBattleRuntime* CandidateRuntime : {&Runtime, &Loaded})
	{
		FString Error;
		TestTrue(FString::Printf(TEXT("first hand slot materializes queued card: %s"), *Error),
			GameXXKCardRules::MoveHandCardToDiscard(CandidateRuntime->Deck, Fillers[0].InstanceId, &Error));
		TestTrue(TEXT("first queued instance enters first"), ContainsInstance(CandidateRuntime->Deck.Hand, SorcererCards[3].InstanceId));
		TestEqual(TEXT("one queued instance remains"), CandidateRuntime->Deck.PendingAutomaticHandCards.Num(), 1);
		TestTrue(FString::Printf(TEXT("second hand slot materializes queued card: %s"), *Error),
			GameXXKCardRules::MoveHandCardToDiscard(CandidateRuntime->Deck, Fillers[1].InstanceId, &Error));
		TestTrue(TEXT("second queued instance enters second"), ContainsInstance(CandidateRuntime->Deck.Hand, SorcererCards[4].InstanceId));
		TestTrue(TEXT("overflow queue drains exactly once"), CandidateRuntime->Deck.PendingAutomaticHandCards.IsEmpty());
		TestEqual(TEXT("materialization preserves hard hand capacity"), CandidateRuntime->Deck.Hand.Num(), 20);
	}
	TestTrue(TEXT("save/load continuation reaches the same deck state"),
		FGameXXKBattleDeckState::StaticStruct()->CompareScriptStruct(&Runtime.Deck, &Loaded.Deck, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererSearchOwnerScopeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.HandQueue.SearchIsOwnerScopedAndTransactional",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererSearchOwnerScopeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerHandQueueTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LingHuoFu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.SheLingHuo"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> SorcererCards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		SorcererCards.Add(MakeCard(CardIds[Index], SorcererId, FString::Printf(TEXT("Sorcerer.Card.%d"), Index), Index));
	}
	const FGameXXKCardInstance OtherOwnerCard = MakeCard(
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		OtherSorcererId,
		TEXT("OtherSorcerer.Card"),
		20);
	const FGameXXKCardInstance HeroCard = MakeCard(
		TEXT("Hero.Generic.QingFengYiShi"),
		HeroId,
		TEXT("Hero.Card"),
		21);
	TArray<FGameXXKCardInstance> AllCards = SorcererCards;
	AllCards.Add(OtherOwnerCard);
	AllCards.Add(HeroCard);
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, AllCards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(OtherSorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 2),
		MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 3),
		MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59303, Runtime)
		|| !InstallZones(
			*this,
			Runtime,
			{SorcererCards[0]},
			{SorcererCards[1], SorcererCards[2], OtherOwnerCard, HeroCard},
			{SorcererCards[3], SorcererCards[4]}))
	{
		return false;
	}

	FGameXXKCardPlayResult SearchResult;
	if (!Resolve(*this, Runtime, SorcererCards[0].InstanceId, SearchResult, TEXT("Sorcerer owner-scoped search")))
	{
		return true;
	}
	TestEqual(TEXT("Sorcerer search opens the shared task choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	TestEqual(TEXT("only four unfinished carried cards are offered"), Runtime.Deck.PendingChoice.Candidates.Num(), 4);
	for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
	{
		TestEqual(TEXT("every offered card belongs to the same Sorcerer"), Candidate.OwnerUnitId, SorcererId);
		TestTrue(TEXT("every offered card belongs to the locked five"), CardIds.Contains(Candidate.CardId));
		TestTrue(TEXT("completed starter is never re-offered"), Candidate.CardId != CardIds[0]);
	}

	const TArray<uint8> BeforeRejectedSubmit = SerializeRuntime(Runtime);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	TestFalse(TEXT("unoffered other-owner card is rejected transactionally"),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, OtherOwnerCard.InstanceId, ResumedResults, &Error));
	TestEqual(TEXT("rejected cross-owner submission leaves serialized state unchanged"), SerializeRuntime(Runtime), BeforeRejectedSubmit);

	TestTrue(FString::Printf(TEXT("valid same-owner search submission resolves: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, SorcererCards[1].InstanceId, ResumedResults, &Error));
	TestTrue(TEXT("chosen same-owner card moves into hand"), ContainsInstance(Runtime.Deck.Hand, SorcererCards[1].InstanceId));
	TestEqual(TEXT("search choice clears after valid submission"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererSearchFallbackTest,
	"GameXXK.Data.PartnerCards.Sorcerer.HandQueue.SearchFallbackRunsOnceWithoutCandidate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererSearchFallbackTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerHandQueueTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LingHuoFu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.SheLingHuo"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	TArray<FGameXXKCardInstance> Cards;
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		Cards.Add(MakeCard(CardIds[Index], SorcererId, FString::Printf(TEXT("Sorcerer.Card.%d"), Index), Index));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Cards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 59304, Runtime)
		|| !InstallZones(*this, Runtime, Cards, {}, {}))
	{
		return false;
	}

	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, Cards[0].InstanceId, Result, TEXT("candidate-free Sorcerer search")))
	{
		return true;
	}
	const FGameXXKCardCombatUnit* Enemy = Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == EnemyId;
	});
	if (!TestNotNull(TEXT("fallback fixture enemy remains addressable"), Enemy))
	{
		return true;
	}
	TestEqual(TEXT("70 percent base plus one 70 percent fallback resolves"), Enemy->HP, 986);
	TestEqual(TEXT("candidate-free search creates exactly two damage packets"), Result.DamageResults.Num(), 2);
	TestEqual(TEXT("candidate-free search opens no choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	return true;
}

#endif
