#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerTaskLifecycleTest
{
	const FName SorcererId(TEXT("Partner.Sorcerer"));
	const FName EnemyAId(TEXT("Enemy.A"));
	const FName EnemyBId(TEXT("Enemy.B"));

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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 100000 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(const FName CardId, const int32 Ordinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Sorcerer.Card.%d"), Ordinal));
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = SorcererId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Sorcerer.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		const TArray<FName>& CardIds,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			Cards.Add(MakeCard(CardIds[Index], Index));
		}
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer lifecycle fixture failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand = Cards;
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic Sorcerer lifecycle fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
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

	bool MoveDiscardToHand(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId)
	{
		const int32 Index = Runtime.Deck.DiscardPile.IndexOfByPredicate([InstanceId](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == InstanceId;
		});
		if (!Test.TestTrue(TEXT("requested replay fixture card is in discard"), Index != INDEX_NONE))
		{
			return false;
		}
		Runtime.Deck.Hand.Add(MoveTemp(Runtime.Deck.DiscardPile[Index]));
		Runtime.Deck.DiscardPile.RemoveAt(Index, 1, EAllowShrinking::No);
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("fixture card return invalidated runtime: %s"), *Error));
			return false;
		}
		return true;
	}

	const FGameXXKSorcererPartnerTaskRuntime* FindTask(const FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.SorcererPartnerTasks.FindByPredicate([](const FGameXXKSorcererPartnerTaskRuntime& Task)
		{
			return Task.OwnerUnitId == SorcererId;
		});
	}

	int32 CountDamageOrigin(const FGameXXKCardPlayResult& Result, const EGameXXKCardResolutionOrigin Origin)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Damage : Result.DamageResults)
		{
			Count += Damage.ResolutionOrigin == Origin ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerFiveCardLifecycleTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskLifecycle.FiveDistinctReplayAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerFiveCardLifecycleTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskLifecycleTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing"),
		TEXT("Profession.Sorcerer.NingYanChengRen")};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, CardIds, 59201, Runtime))
	{
		return false;
	}

	FGameXXKCardPlayResult FirstResult;
	if (!Resolve(*this, Runtime, TEXT("Sorcerer.Card.0"), FirstResult, TEXT("first distinct Fire card")))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("first active Sorcerer card creates owner task"), Task))
	{
		return true;
	}
	TestTrue(TEXT("task is active"), Task->bActive);
	TestEqual(TEXT("task locks exactly five carried IDs"), Task->LockedCardIds.Num(), 5);
	for (const FName CardId : CardIds)
	{
		TestTrue(FString::Printf(TEXT("locked loadout contains %s"), *CardId.ToString()), Task->LockedCardIds.Contains(CardId));
	}
	TestEqual(TEXT("first distinct card advances once"), Task->CompletedCardIds, TArray<FName>{CardIds[0]});
	TestEqual(TEXT("first snapshot pays catalog Mana"), Task->FirstPlayOrder[0].PaidManaCost, 1);
	TestEqual(TEXT("first snapshot position"), Task->FirstPlayOrder[0].SorcererSequencePosition, 1);
	TestEqual(TEXT("first snapshot has no previous family"), Task->FirstPlayOrder[0].PreviousSorcererFamily, EGameXXKSorcererCardFamily::None);
	TestEqual(TEXT("Fire starter locks Fire branch"), Task->LockedBranch, EGameXXKSorcererTaskBranch::Fire);

	if (!MoveDiscardToHand(*this, Runtime, TEXT("Sorcerer.Card.0")))
	{
		return true;
	}
	FGameXXKCardPlayResult DuplicateResult;
	if (!Resolve(*this, Runtime, TEXT("Sorcerer.Card.0"), DuplicateResult, TEXT("duplicate active Fire card")))
	{
		return true;
	}
	Task = FindTask(Runtime);
	if (!Task)
	{
		AddError(TEXT("duplicate play unexpectedly removed the task"));
		return true;
	}
	TestEqual(TEXT("duplicate CardId does not advance progress"), Task->CompletedCardIds.Num(), 1);
	TestEqual(TEXT("duplicate CardId does not add a first-play snapshot"), Task->FirstPlayOrder.Num(), 1);
	TestEqual(TEXT("duplicate active snapshot receives no sequence position"), Runtime.LastActiveCard.SorcererSequencePosition, 0);

	for (int32 Index = 1; Index < CardIds.Num(); ++Index)
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, FName(*FString::Printf(TEXT("Sorcerer.Card.%d"), Index)), Result, TEXT("next distinct Sorcerer card")))
		{
			return true;
		}
		if (Index < CardIds.Num() - 1)
		{
			Task = FindTask(Runtime);
			if (!TestNotNull(TEXT("task persists before fifth distinct card"), Task))
			{
				return true;
			}
			TestEqual(TEXT("distinct progress matches sequence position"), Task->CompletedCardIds.Num(), Index + 1);
			TestEqual(TEXT("snapshot position is contiguous"), Task->FirstPlayOrder.Last().SorcererSequencePosition, Index + 1);
			const FGameXXKCardDefinition* PreviousDefinition = FGameXXKCardCatalog::FindCardDefinition(CardIds[Index - 1]);
			TestEqual(TEXT("previous family is locked from recorded card"), Task->FirstPlayOrder.Last().PreviousSorcererFamily, PreviousDefinition->SorcererRule.Family);
		}
		else
		{
			TestEqual(TEXT("five replays plus one starter reward are automatic"), Result.AutomaticResolutionCount, 6);
			TestTrue(TEXT("automatic base replays use Sorcerer origin"), CountDamageOrigin(Result, EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay) > 0);
		}
	}

	Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("owner history entry remains after completion"), Task))
	{
		return true;
	}
	TestFalse(TEXT("completed task resets active progress"), Task->bActive);
	TestTrue(TEXT("completed task clears locked IDs"), Task->LockedCardIds.IsEmpty());
	TestTrue(TEXT("completed task clears completed IDs"), Task->CompletedCardIds.IsEmpty());
	TestTrue(TEXT("completed task clears replay snapshots"), Task->FirstPlayOrder.IsEmpty());
	TestEqual(TEXT("automatic replays never count as active cards"), Runtime.ActiveCardsPlayedThisRound, 6);
	TestFalse(TEXT("automatic queue is empty after synchronous completion"), Runtime.AutomaticResolutionQueue.bActive);

	if (!MoveDiscardToHand(*this, Runtime, TEXT("Sorcerer.Card.1")))
	{
		return true;
	}
	FGameXXKCardPlayResult RestartResult;
	if (!Resolve(*this, Runtime, TEXT("Sorcerer.Card.1"), RestartResult, TEXT("same-battle task restart")))
	{
		return true;
	}
	Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("next active Sorcerer card restarts task"), Task))
	{
		return true;
	}
	TestTrue(TEXT("restarted task is active"), Task->bActive);
	TestEqual(TEXT("restarted task starts at one distinct card"), Task->CompletedCardIds, TArray<FName>{CardIds[1]});
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerUniversalBranchTimingTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskLifecycle.UniversalSecondCardLocksBranchBeforeBase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerUniversalBranchTimingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskLifecycleTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.YanMuHuTi"),
		TEXT("Profession.Sorcerer.SheLingHuo"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing"),
		TEXT("Profession.Sorcerer.LieFu")};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, CardIds, 59202, Runtime))
	{
		return false;
	}

	FGameXXKCardPlayResult FirstResult;
	if (!Resolve(*this, Runtime, TEXT("Sorcerer.Card.0"), FirstResult, TEXT("Universal starter")))
	{
		return true;
	}
	const FGameXXKSorcererPartnerTaskRuntime* Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("Universal starter creates task"), Task))
	{
		return true;
	}
	TestEqual(TEXT("Universal starter defers branch"), Task->LockedBranch, EGameXXKSorcererTaskBranch::None);
	TestEqual(TEXT("Universal starter snapshot defers branch"), Task->FirstPlayOrder[0].SorcererTaskBranch, EGameXXKSorcererTaskBranch::None);
	TestEqual(TEXT("Universal starter records paid Mana5"), Task->FirstPlayOrder[0].PaidManaCost, 5);

	FGameXXKCardPlayResult SecondResult;
	if (!Resolve(*this, Runtime, TEXT("Sorcerer.Card.1"), SecondResult, TEXT("Ice second card")))
	{
		return true;
	}
	Task = FindTask(Runtime);
	if (!TestNotNull(TEXT("task persists after branch selection"), Task))
	{
		return true;
	}
	TestEqual(TEXT("Ice second card locks Ice branch"), Task->LockedBranch, EGameXXKSorcererTaskBranch::Ice);
	TestEqual(TEXT("starter snapshot is updated to Ice branch"), Task->FirstPlayOrder[0].SorcererTaskBranch, EGameXXKSorcererTaskBranch::Ice);
	TestEqual(TEXT("second snapshot carries Ice branch"), Task->FirstPlayOrder[1].SorcererTaskBranch, EGameXXKSorcererTaskBranch::Ice);
	TestEqual(TEXT("second snapshot previous family is Universal"), Task->FirstPlayOrder[1].PreviousSorcererFamily, EGameXXKSorcererCardFamily::Universal);
	TestEqual(TEXT("second snapshot position is two"), Task->FirstPlayOrder[1].SorcererSequencePosition, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerUniversalExtraReplayCountTest,
	"GameXXK.Data.PartnerCards.Sorcerer.TaskLifecycle.UniversalRewardExtraReplayCounts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerUniversalExtraReplayCountTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerTaskLifecycleTest;
	const TArray<FName> CardIds = {
		TEXT("Profession.Sorcerer.ChiYanFengJie"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.FenMaiFu"),
		TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, CardIds, 59203, Runtime))
	{
		return false;
	}
	for (int32 Index = 0; Index < CardIds.Num(); ++Index)
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, FName(*FString::Printf(TEXT("Sorcerer.Card.%d"), Index)), Result, TEXT("Universal replay-count card")))
		{
			return true;
		}
		if (Index == CardIds.Num() - 1)
		{
			TestEqual(TEXT("five task replays, one starter reward, and its extra replay all count"), Result.AutomaticResolutionCount, 7);
		}
	}
	return true;
}

#endif
