#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKHeroMageRuntimeTest
{
	const FName HeroId(TEXT("Hero"));
	const FName AllyId(TEXT("Ally"));
	const FName NpcId(TEXT("Npc"));
	const FName EnemyAId(TEXT("EnemyA"));
	const FName EnemyBId(TEXT("EnemyB"));

	TArray<FName> CanonicalEight()
	{
		return {
			TEXT("Hero.Mage.HanXuNingChuan"),
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.PoYunYiShan"),
			TEXT("Hero.Blade.XueLuXiangCheng"),
			TEXT("Hero.Hunter.LieYuLianShi"),
			TEXT("Hero.Hunter.CuiDuChuanXin")};
	}

	TArray<FName> SearchEight()
	{
		return {
			TEXT("Hero.Mage.YanXuLiaoYuan"),
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"),
			TEXT("Hero.Guard.TieBiTongShou")};
	}

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 10000 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardCombatUnit> MakeUnits()
	{
		return {
			MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 10, 1),
			MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 8, 2),
			MakeUnit(NpcId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 6, 3),
			MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 10),
			MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 8, 11)};
	}

	FGameXXKCardInstance MakeCard(
		const FName InstanceId,
		const FName CardId,
		const FName OwnerUnitId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Mage.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	TArray<FGameXXKCardInstance> MakeHeroCards(const TArray<FName>& CardIds)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("HeroCard.%d"), Index)),
				CardIds[Index],
				HeroId,
				Index));
		}
		return Cards;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const TArray<FGameXXKCardInstance>& Cards,
		const TArray<FName>& HandIds,
		const TArray<FName>& DiscardIds,
		const TArray<FName>& EquippedIds,
		const int32 Seed)
	{
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			MakeUnits(),
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("Mage runtime failed to initialize: %s"), *Error));
			return false;
		}

		TSet<FName> HandSet;
		TSet<FName> DiscardSet;
		for (const FName Id : HandIds)
		{
			HandSet.Add(Id);
		}
		for (const FName Id : DiscardIds)
		{
			DiscardSet.Add(Id);
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			if (HandSet.Contains(Card.InstanceId))
			{
				OutRuntime.Deck.Hand.Add(Card);
			}
			else if (DiscardSet.Contains(Card.InstanceId))
			{
				OutRuntime.Deck.DiscardPile.Add(Card);
			}
			else
			{
				OutRuntime.Deck.DrawPile.Add(Card);
			}
		}
		OutRuntime.Deck.SharedEnergy = 99;
		OutRuntime.EquippedHeroCardIds = EquippedIds;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Deterministic Mage fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = FindUnit(Runtime, UnitId);
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	FName TargetForCard(const FName CardId)
	{
		if (CardId == TEXT("Hero.Mage.YanXuLiaoYuan")
			|| CardId == TEXT("Hero.Mage.HanXuNingChuan")
			|| CardId == TEXT("Hero.Mage.LeiXuYinTing")
			|| CardId == TEXT("Hero.Mage.GuiXuTongXuan")
			|| CardId == TEXT("Hero.Generic.NingShenTuNa")
			|| CardId == TEXT("Route.General.ShouShiHuiYuan"))
		{
			return NAME_None;
		}
		if (CardId == TEXT("Hero.Generic.GuiYuanShu")
			|| CardId == TEXT("Hero.Generic.HengJianShouShi")
			|| CardId == TEXT("Hero.Guard.TieBiTongShou")
			|| CardId == TEXT("Npc.TusiChief.ShiMenShouShi"))
		{
			return AllyId;
		}
		return EnemyAId;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}

	FGameXXKResolvedCardSnapshot MakeSnapshot(
		const FName CardId,
		const FName OwnerUnitId = HeroId,
		const TArray<FName>& Targets = {})
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = OwnerUnitId;
		Snapshot.OriginalTargetUnitIds = Targets;
		return Snapshot;
	}

	TArray<FGameXXKResolvedCardSnapshot> CanonicalSnapshots()
	{
		TArray<FGameXXKResolvedCardSnapshot> Snapshots;
		for (const FName CardId : CanonicalEight())
		{
			Snapshots.Add(MakeSnapshot(CardId, HeroId, TargetForCard(CardId).IsNone() ? TArray<FName>() : TArray<FName>{TargetForCard(CardId)}));
		}
		return Snapshots;
	}

	void SetActiveTask(
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FName>& LockedIds,
		const EGameXXKHeroSpellTaskReward Reward = EGameXXKHeroSpellTaskReward::Fire)
	{
		Runtime.EquippedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.bActive = true;
		Runtime.HeroSpellTask.LockedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.StarterReward = Reward;
		Runtime.HeroSpellTask.StarterOwnerUnitId = HeroId;
	}

	void SetCompletedTaskQueue(
		FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKHeroSpellTaskReward Reward,
		const int32 NextCardIndex = 8)
	{
		FName StarterCardId = NAME_None;
		switch (Reward)
		{
		case EGameXXKHeroSpellTaskReward::Fire:
			StarterCardId = TEXT("Hero.Mage.YanXuLiaoYuan");
			break;
		case EGameXXKHeroSpellTaskReward::Ice:
			StarterCardId = TEXT("Hero.Mage.HanXuNingChuan");
			break;
		case EGameXXKHeroSpellTaskReward::Lightning:
			StarterCardId = TEXT("Hero.Mage.LeiXuYinTing");
			break;
		case EGameXXKHeroSpellTaskReward::Universal:
			StarterCardId = TEXT("Hero.Mage.GuiXuTongXuan");
			break;
		case EGameXXKHeroSpellTaskReward::None:
		default:
			break;
		}
		TArray<FGameXXKResolvedCardSnapshot> Snapshots = {
			MakeSnapshot(StarterCardId, HeroId, TargetForCard(StarterCardId).IsNone() ? TArray<FName>() : TArray<FName>{TargetForCard(StarterCardId)}),
			MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Generic.HeYuZhan"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Generic.SuiYanJi"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Generic.PoYunYiShan"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Blade.XueLuXiangCheng"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Hunter.LieYuLianShi"), HeroId, {EnemyAId}),
			MakeSnapshot(TEXT("Hero.Hunter.CuiDuChuanXin"), HeroId, {EnemyAId})};
		TArray<FName> LockedIds;
		for (const FGameXXKResolvedCardSnapshot& Snapshot : Snapshots)
		{
			LockedIds.Add(Snapshot.CardId);
		}
		SetActiveTask(Runtime, LockedIds, Reward);
		Runtime.HeroSpellTask.CompletedHeroCardIds = LockedIds;
		Runtime.HeroSpellTask.FirstPlayOrder = Snapshots;
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::MageTaskReplay;
		Runtime.AutomaticResolutionQueue.PendingCards = Snapshots;
		Runtime.AutomaticResolutionQueue.NextCardIndex = NextCardIndex;
		Runtime.AutomaticResolutionQueue.PendingReward = Reward;
		Runtime.AutomaticResolutionQueue.RewardOwnerUnitId = HeroId;
	}

	int32 CountDamage(
		const TArray<FGameXXKCardDamageResult>& Results,
		const EGameXXKCardResolutionOrigin Origin,
		const EGameXXKCardDamageCause Cause)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.ResolutionOrigin == Origin && Result.Cause == Cause ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageTaskLockTest,
	"GameXXK.Data.HeroCards.Mage.FirstActiveHeroMageCardLocksExactlyEightEquippedHeroIds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageTaskLockTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	TArray<FName> Equipped = SearchEight();
	Equipped[0] = TEXT("Hero.Mage.GuiXuTongXuan");
	TArray<FGameXXKCardInstance> Cards = MakeHeroCards(Equipped);
	Cards.Add(MakeCard(TEXT("DrawA"), TEXT("Route.General.TuNaJue"), HeroId, 20));
	Cards.Add(MakeCard(TEXT("DrawB"), TEXT("Route.General.QingShenQuShi"), HeroId, 21));
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HeroCard.0")}, {}, Equipped, 56001)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("HeroCard.0"), NAME_None, Result, TEXT("Universal task starter"))) return true;
	TestTrue(TEXT("the first active Hero Mage card starts the task"), Runtime.HeroSpellTask.bActive);
	TestEqual(TEXT("the task locks exactly eight IDs"), Runtime.HeroSpellTask.LockedHeroCardIds.Num(), 8);
	TestEqual(TEXT("the exact equipped order is locked"), Runtime.HeroSpellTask.LockedHeroCardIds, Equipped);
	TestEqual(TEXT("the starter itself is completed before its search or choice"), Runtime.HeroSpellTask.CompletedHeroCardIds, TArray<FName>{Equipped[0]});
	TestEqual(TEXT("the starter play snapshot is recorded"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 1);
	TestEqual(TEXT("the starter reward is Universal"), Runtime.HeroSpellTask.StarterReward, EGameXXKHeroSpellTaskReward::Universal);
	TestEqual(TEXT("the stable starter owner is saved"), Runtime.HeroSpellTask.StarterOwnerUnitId, HeroId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageTaskOutsiderTest,
	"GameXXK.Data.HeroCards.Mage.PartnerNpcRouteAndTemporaryCardsNeverEnterTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageTaskOutsiderTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = SearchEight();
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Partner"), TEXT("Profession.Blade.FengHou"), AllyId, 1),
		MakeCard(TEXT("NpcCard"), TEXT("Npc.TusiChief.ShiMenShouShi"), NpcId, 2),
		MakeCard(TEXT("RouteCard"), TEXT("Route.General.ShouShiHuiYuan"), HeroId, 3),
		MakeCard(TEXT("TemporaryHero"), TEXT("Hero.Generic.PoYunYiShan"), HeroId, 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Partner"), TEXT("NpcCard"), TEXT("RouteCard"), TEXT("TemporaryHero")}, {}, Equipped, 56002)) return false;
	SetActiveTask(Runtime, Equipped);
	for (const FGameXXKCardInstance& Card : Cards)
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, Card.InstanceId, TargetForCard(Card.CardId), Result, *Card.InstanceId.ToString())) return true;
	}
	TestTrue(TEXT("the original task remains active"), Runtime.HeroSpellTask.bActive);
	TestEqual(TEXT("no outsider card enters completed IDs"), Runtime.HeroSpellTask.CompletedHeroCardIds.Num(), 0);
	TestEqual(TEXT("no outsider snapshot enters first-play order"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageTaskDuplicateTest,
	"GameXXK.Data.HeroCards.Mage.DuplicateHeroIdDoesNotReplaceMissingId",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageTaskDuplicateTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = SearchEight();
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("QingA"), TEXT("Hero.Generic.QingFengYiShi"), HeroId, 1),
		MakeCard(TEXT("QingB"), TEXT("Hero.Generic.QingFengYiShi"), HeroId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("QingA"), TEXT("QingB")}, {}, Equipped, 56003)) return false;
	SetActiveTask(Runtime, Equipped);
	for (const FName InstanceId : {FName(TEXT("QingA")), FName(TEXT("QingB"))})
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, InstanceId, EnemyAId, Result, TEXT("duplicate equipped Hero ID"))) return true;
	}
	TestEqual(TEXT("the repeated CardId completes only one slot"), Runtime.HeroSpellTask.CompletedHeroCardIds.Num(), 1);
	TestEqual(TEXT("the first play order contains only one snapshot"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 1);
	TestEqual(TEXT("the other seven equipped IDs remain unfinished"), Runtime.HeroSpellTask.LockedHeroCardIds.Num() - Runtime.HeroSpellTask.CompletedHeroCardIds.Num(), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageTaskFirstOrderTest,
	"GameXXK.Data.HeroCards.Mage.AllEightFirstPlaysAreRecordedInOrderWithOwnerAndTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageTaskFirstOrderTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	TArray<FName> Equipped = SearchEight();
	Equipped[0] = TEXT("Hero.Mage.HanXuNingChuan");
	Equipped[7] = TEXT("Hero.Mage.GuiXuTongXuan");
	TArray<FGameXXKCardInstance> Cards = MakeHeroCards(Equipped);
	Cards.Add(MakeCard(TEXT("DrawA"), TEXT("Route.General.TuNaJue"), HeroId, 20));
	Cards.Add(MakeCard(TEXT("DrawB"), TEXT("Route.General.QingShenQuShi"), HeroId, 21));
	TArray<FName> HandIds;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		HandIds.Add(FName(*FString::Printf(TEXT("HeroCard.%d"), Index)));
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, HandIds, {}, Equipped, 56004)) return false;
	for (int32 Index = 0; Index < 8; ++Index)
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, HandIds[Index], TargetForCard(Equipped[Index]), Result, TEXT("ordered equipped Hero play"))) return true;
	}
	TestEqual(TEXT("the eighth base list pauses on its forced discard"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestTrue(TEXT("the completed task remains persisted while the choice is open"), Runtime.HeroSpellTask.bActive);
	TestEqual(TEXT("all eight unique IDs are complete"), Runtime.HeroSpellTask.CompletedHeroCardIds, Equipped);
	TestEqual(TEXT("all eight first plays are ordered"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 8);
	for (int32 Index = 0; Index < Runtime.HeroSpellTask.FirstPlayOrder.Num(); ++Index)
	{
		const FGameXXKResolvedCardSnapshot& Snapshot = Runtime.HeroSpellTask.FirstPlayOrder[Index];
		TestEqual(FString::Printf(TEXT("snapshot %d retains CardId"), Index), Snapshot.CardId, Equipped[Index]);
		TestEqual(FString::Printf(TEXT("snapshot %d retains owner"), Index), Snapshot.OwnerUnitId, HeroId);
		const FName ExpectedTarget = TargetForCard(Equipped[Index]);
		TestEqual(FString::Printf(TEXT("snapshot %d retains target count"), Index), Snapshot.OriginalTargetUnitIds.Num(), ExpectedTarget.IsNone() ? 0 : 1);
		if (!ExpectedTarget.IsNone() && Snapshot.OriginalTargetUnitIds.Num() == 1)
		{
			TestEqual(FString::Printf(TEXT("snapshot %d retains stable target"), Index), Snapshot.OriginalTargetUnitIds[0], ExpectedTarget);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageTaskReplayOrderTest,
	"GameXXK.Data.HeroCards.Mage.TaskReplaysEightBaseEffectsInFirstPlayOrder",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageTaskReplayOrderTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = CanonicalEight();
	const TArray<FGameXXKCardInstance> Cards = MakeHeroCards(Equipped);
	TArray<FName> HandIds;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		HandIds.Add(Cards[Index].InstanceId);
	}
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, HandIds, {}, Equipped, 56005)) return false;
	FGameXXKCardPlayResult FinalResult;
	for (int32 Index = 0; Index < Cards.Num(); ++Index)
	{
		FGameXXKCardPlayResult Result;
		if (!Resolve(*this, Runtime, Cards[Index].InstanceId, TargetForCard(Cards[Index].CardId), Result, TEXT("eight-card Mage sequence"))) return true;
		if (Index == Cards.Num() - 1)
		{
			FinalResult = MoveTemp(Result);
		}
	}

	TArray<int32> ReplayDirectRequests;
	for (const FGameXXKCardDamageResult& DamageResult : FinalResult.DamageResults)
	{
		if (DamageResult.ResolutionOrigin == EGameXXKCardResolutionOrigin::MageTaskReplay
			&& DamageResult.Cause == EGameXXKCardDamageCause::DirectAttack)
		{
			ReplayDirectRequests.Add(DamageResult.BaseRequestedDamage);
		}
	}
	const TArray<int32> ExpectedRequests = {14, 16, 15, 16, 15, 14, 13};
	TestEqual(TEXT("the seven attacking base cards replay in their first-play order"), ReplayDirectRequests, ExpectedRequests);
	TestFalse(TEXT("the task resets only after replay and reward finish"), Runtime.HeroSpellTask.bActive);
	TestFalse(TEXT("the automatic queue is empty after the full task"), Runtime.AutomaticResolutionQueue.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageStarterRewardOnlyTest,
	"GameXXK.Data.HeroCards.Mage.OnlyStarterMageRewardRuns",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageStarterRewardOnlyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56006)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Fire);
	FindUnit(Runtime, HeroId)->Armor = 9;
	Runtime.Deck.SharedEnergy = 4;
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("starter-only reward queue resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("Fire is the only emitted reward result"), Results.Num(), 1);
	TestEqual(TEXT("an unselected Ice reward does not consume armor"), FindUnit(Runtime, HeroId)->Armor, 9);
	TestEqual(TEXT("an unselected Universal reward does not grant energy"), Runtime.Deck.SharedEnergy, 4);
	TestEqual(TEXT("an unselected Lightning reward does not grant Mark"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the selected Fire reward finishes at Burn6 after two triggers"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageNoRecursiveTaskTest,
	"GameXXK.Data.HeroCards.Mage.ReplayAndRewardNeverStartOrAdvanceAnotherTask",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageNoRecursiveTaskTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("DrawA"), TEXT("Route.General.TuNaJue"), HeroId, 1),
		MakeCard(TEXT("DrawB"), TEXT("Route.General.QingShenQuShi"), HeroId, 2),
		MakeCard(TEXT("DrawC"), TEXT("Route.General.ShouShiHuiYuan"), HeroId, 3),
		MakeCard(TEXT("DrawD"), TEXT("Route.General.FeiZhen"), HeroId, 4)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {}, {}, {}, 56007)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Ice, 0);
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("Mage replay plus Ice reward resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("automatic replay never advances the active-card counter"), Runtime.ActiveCardsPlayedThisRound, 0);
	TestFalse(TEXT("the replayed Mage card does not start a replacement task"), Runtime.HeroSpellTask.bActive);
	TestEqual(TEXT("the completed task leaves no recursive first-play snapshots"), Runtime.HeroSpellTask.FirstPlayOrder.Num(), 0);
	TestFalse(TEXT("the completed queue does not recursively enqueue itself"), Runtime.AutomaticResolutionQueue.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageDeadTargetFallbackTest,
	"GameXXK.Data.HeroCards.Mage.DeadOriginalTargetFallsBackToSameSideStableFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageDeadTargetFallbackTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56008)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Universal, 7);
	const int32 QingIndex = Runtime.HeroSpellTask.FirstPlayOrder.IndexOfByPredicate([](const FGameXXKResolvedCardSnapshot& Snapshot)
	{
		return Snapshot.CardId == TEXT("Hero.Generic.QingFengYiShi");
	});
	TestTrue(TEXT("the fixture finds Qing Feng"), QingIndex != INDEX_NONE);
	if (QingIndex == INDEX_NONE) return false;
	Runtime.HeroSpellTask.FirstPlayOrder.Swap(QingIndex, 7);
	Runtime.HeroSpellTask.CompletedHeroCardIds.Swap(QingIndex, 7);
	Runtime.HeroSpellTask.LockedHeroCardIds = Runtime.HeroSpellTask.CompletedHeroCardIds;
	Runtime.EquippedHeroCardIds = Runtime.HeroSpellTask.LockedHeroCardIds;
	Runtime.AutomaticResolutionQueue.PendingCards = Runtime.HeroSpellTask.FirstPlayOrder;
	FGameXXKCardCombatUnit* DeadOriginal = FindUnit(Runtime, EnemyAId);
	DeadOriginal->HP = 0;
	DeadOriginal->bLiving = false;
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("dead-target replay resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	const FGameXXKCardDamageResult* ReplayHit = nullptr;
	for (const FGameXXKCardPlayResult& Result : Results)
	{
		ReplayHit = Result.DamageResults.FindByPredicate([](const FGameXXKCardDamageResult& DamageResult)
		{
			return DamageResult.ResolutionOrigin == EGameXXKCardResolutionOrigin::MageTaskReplay
				&& DamageResult.Cause == EGameXXKCardDamageCause::DirectAttack;
		});
		if (ReplayHit) break;
	}
	TestNotNull(TEXT("the replay still emits its direct attack"), ReplayHit);
	if (ReplayHit)
	{
		TestEqual(TEXT("the dead enemy target falls back to stable-first EnemyB"), ReplayHit->OriginalTargetUnitId, EnemyBId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageMissingTargetSkipTest,
	"GameXXK.Data.HeroCards.Mage.MissingTargetSkipsOnlyDependentEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageMissingTargetSkipTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56009)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Fire, 7);
	const int32 QingIndex = Runtime.HeroSpellTask.FirstPlayOrder.IndexOfByPredicate([](const FGameXXKResolvedCardSnapshot& Snapshot)
	{
		return Snapshot.CardId == TEXT("Hero.Generic.QingFengYiShi");
	});
	if (QingIndex == INDEX_NONE) return false;
	Runtime.HeroSpellTask.FirstPlayOrder.Swap(QingIndex, 7);
	Runtime.HeroSpellTask.CompletedHeroCardIds.Swap(QingIndex, 7);
	Runtime.HeroSpellTask.LockedHeroCardIds = Runtime.HeroSpellTask.CompletedHeroCardIds;
	Runtime.EquippedHeroCardIds = Runtime.HeroSpellTask.LockedHeroCardIds;
	Runtime.AutomaticResolutionQueue.PendingCards = Runtime.HeroSpellTask.FirstPlayOrder;
	for (FGameXXKCardCombatUnit& Unit : Runtime.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("missing-target replay resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	int32 ReplayDamageCount = 0;
	for (const FGameXXKCardPlayResult& Result : Results)
	{
		for (const FGameXXKCardDamageResult& DamageResult : Result.DamageResults)
		{
			ReplayDamageCount += DamageResult.ResolutionOrigin == EGameXXKCardResolutionOrigin::MageTaskReplay ? 1 : 0;
		}
	}
	TestEqual(TEXT("selected-target damage is skipped when no same-side fallback lives"), ReplayDamageCount, 0);
	TestEqual(TEXT("the independent Qing Feng discount modifier still registers"), Runtime.Modifiers.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageSearchCandidatesTest,
	"GameXXK.Data.HeroCards.Mage.SearchOffersOnlyUnfinishedEquippedHeroCardsInDrawOrDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageSearchCandidatesTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = SearchEight();
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Starter"), TEXT("Hero.Mage.YanXuLiaoYuan"), HeroId, 0),
		MakeCard(TEXT("DiscardCandidate"), TEXT("Hero.Generic.HeYuZhan"), HeroId, 2),
		MakeCard(TEXT("DrawCandidate"), TEXT("Hero.Generic.QingFengYiShi"), HeroId, 5),
		MakeCard(TEXT("AlreadyInHand"), TEXT("Hero.Generic.SuiYanJi"), HeroId, 3),
		MakeCard(TEXT("NotEquipped"), TEXT("Hero.Generic.PoYunYiShan"), HeroId, 4),
		MakeCard(TEXT("RouteCard"), TEXT("Route.General.ShouShiHuiYuan"), HeroId, 6)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(
		*this,
		Runtime,
		Cards,
		{TEXT("Starter"), TEXT("AlreadyInHand")},
		{TEXT("DiscardCandidate")},
		Equipped,
		56010)) return false;
	SetActiveTask(Runtime, Equipped, EGameXXKHeroSpellTaskReward::Fire);
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Starter"), NAME_None, Result, TEXT("Mage unfinished-card search"))) return true;
	TestEqual(TEXT("search reuses the existing pending-choice kind"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	TestEqual(TEXT("only draw/discard unfinished equipped instances are offered"), Runtime.Deck.PendingChoice.Candidates.Num(), 2);
	if (Runtime.Deck.PendingChoice.Candidates.Num() == 2)
	{
		TestEqual(TEXT("search sorts candidates by acquisition order across zones"), Runtime.Deck.PendingChoice.Candidates[0].InstanceId, FName(TEXT("DiscardCandidate")));
		TestEqual(TEXT("the later draw candidate follows"), Runtime.Deck.PendingChoice.Candidates[1].InstanceId, FName(TEXT("DrawCandidate")));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageSearchNoCopyTest,
	"GameXXK.Data.HeroCards.Mage.SearchNeverCopiesCardsAlreadyInHandOrCurrentlyResolving",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageSearchNoCopyTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = SearchEight();
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Starter"), TEXT("Hero.Mage.YanXuLiaoYuan"), HeroId, 0),
		MakeCard(TEXT("Candidate"), TEXT("Hero.Generic.HeYuZhan"), HeroId, 1),
		MakeCard(TEXT("AlreadyInHand"), TEXT("Hero.Generic.SuiYanJi"), HeroId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Starter"), TEXT("AlreadyInHand")}, {}, Equipped, 56011)) return false;
	SetActiveTask(Runtime, Equipped, EGameXXKHeroSpellTaskReward::Fire);
	const int32 LedgerCountBefore = Runtime.Deck.ActiveInstanceIds.Num();
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Starter"), NAME_None, Result, TEXT("non-copying Mage search"))) return true;
	TArray<FName> CandidateIds;
	for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
	{
		CandidateIds.Add(Candidate.InstanceId);
	}
	TestFalse(TEXT("the currently resolving starter is excluded after moving to discard"), CandidateIds.Contains(TEXT("Starter")));
	TestFalse(TEXT("a locked card already in hand is excluded"), CandidateIds.Contains(TEXT("AlreadyInHand")));
	TestTrue(TEXT("the real draw-pile instance is offered"), CandidateIds.Contains(TEXT("Candidate")));
	TestEqual(TEXT("search does not add a copied ledger instance"), Runtime.Deck.ActiveInstanceIds.Num(), LedgerCountBefore);
	TestEqual(TEXT("the offered real card remains in its owning draw zone until selection"), Runtime.Deck.DrawPile.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageSearchResumeTest,
	"GameXXK.Data.HeroCards.Mage.SearchUsesExistingPendingChoiceAndResumesQueue",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageSearchResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FName> Equipped = SearchEight();
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("Starter"), TEXT("Hero.Mage.YanXuLiaoYuan"), HeroId, 0),
		MakeCard(TEXT("Candidate"), TEXT("Hero.Generic.HeYuZhan"), HeroId, 1)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Starter")}, {TEXT("Candidate")}, Equipped, 56012)) return false;
	SetActiveTask(Runtime, Equipped, EGameXXKHeroSpellTaskReward::Fire);
	FGameXXKCardPlayResult PlayResult;
	if (!Resolve(*this, Runtime, TEXT("Starter"), NAME_None, PlayResult, TEXT("Mage search before queued continuation"))) return true;
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = {MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), HeroId, {EnemyAId})};
	Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("Mage search selection resumes the queue: %s"), *Error), GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, TEXT("Candidate"), ResumedResults, &Error));
	TestEqual(TEXT("the selected real instance moves into hand"), Runtime.Deck.Hand.Num(), 1);
	if (Runtime.Deck.Hand.Num() == 1)
	{
		TestEqual(TEXT("the selected instance identity is preserved"), Runtime.Deck.Hand[0].InstanceId, FName(TEXT("Candidate")));
	}
	TestEqual(TEXT("the queued replay resumes exactly once"), ResumedResults.Num(), 1);
	TestFalse(TEXT("the old automatic queue completes"), Runtime.AutomaticResolutionQueue.bActive);
	TestEqual(TEXT("automatic replay does not advance the active Mage task"), Runtime.HeroSpellTask.CompletedHeroCardIds.Num(), 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageForcedDiscardResumeTest,
	"GameXXK.Data.HeroCards.Mage.ReplayForcedDiscardPausesAndResumesBeforeReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageForcedDiscardResumeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("DrawA"), TEXT("Route.General.TuNaJue"), HeroId, 1),
		MakeCard(TEXT("DrawB"), TEXT("Route.General.QingShenQuShi"), HeroId, 2)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {}, {}, {}, 56013)) return false;
	TArray<FGameXXKResolvedCardSnapshot> Snapshots = {
		MakeSnapshot(TEXT("Hero.Mage.YanXuLiaoYuan"), HeroId),
		MakeSnapshot(TEXT("Hero.Generic.QingFengYiShi"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Generic.HeYuZhan"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Generic.SuiYanJi"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Generic.PoYunYiShan"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Blade.XueLuXiangCheng"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Hunter.LieYuLianShi"), HeroId, {EnemyAId}),
		MakeSnapshot(TEXT("Hero.Mage.GuiXuTongXuan"), HeroId)};
	TArray<FName> LockedIds;
	for (const FGameXXKResolvedCardSnapshot& Snapshot : Snapshots)
	{
		LockedIds.Add(Snapshot.CardId);
	}
	SetActiveTask(Runtime, LockedIds, EGameXXKHeroSpellTaskReward::Fire);
	Runtime.HeroSpellTask.CompletedHeroCardIds = LockedIds;
	Runtime.HeroSpellTask.FirstPlayOrder = Snapshots;
	Runtime.AutomaticResolutionQueue.bActive = true;
	Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::MageTaskReplay;
	Runtime.AutomaticResolutionQueue.PendingCards = Snapshots;
	Runtime.AutomaticResolutionQueue.NextCardIndex = 7;
	Runtime.AutomaticResolutionQueue.PendingReward = EGameXXKHeroSpellTaskReward::Fire;
	Runtime.AutomaticResolutionQueue.RewardOwnerUnitId = HeroId;
	TArray<FGameXXKCardPlayResult> InitialResults;
	FString Error;
	TestTrue(FString::Printf(TEXT("replay reaches forced discard: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, InitialResults, &Error));
	TestEqual(TEXT("the replay pauses on the existing forced-discard choice"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("the reward has not applied before the choice"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 0);
	TestTrue(TEXT("the completed task remains active across the interruption"), Runtime.HeroSpellTask.bActive);
	TArray<FGameXXKCardPlayResult> ResumedResults;
	const FName DiscardId = Runtime.Deck.Hand[0].InstanceId;
	TestTrue(FString::Printf(TEXT("forced discard resumes replay and reward: %s"), *Error), GameXXKCardRules::SubmitForcedDiscard(Runtime, {DiscardId}, &Error, &ResumedResults));
	TestEqual(TEXT("the Fire reward runs only after discard confirmation"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 6);
	TestFalse(TEXT("the task resets after the resumed reward"), Runtime.HeroSpellTask.bActive);
	TestFalse(TEXT("the queue completes after the resumed reward"), Runtime.AutomaticResolutionQueue.bActive);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageIceBaseTest,
	"GameXXK.Data.HeroCards.Mage.IceBaseUsesCurrentManaAndConvertsOnlyOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageIceBaseTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Ice"), TEXT("Hero.Mage.HanXuNingChuan"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Ice")}, {}, {}, 56014)) return false;
	FGameXXKCardCombatUnit* Hero = FindUnit(Runtime, HeroId);
	Hero->Mana = 8;
	Hero->MaxMana = 10;
	Hero->Armor = 1;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("Ice"), NAME_None, Result, TEXT("Ice base effect"))) return true;
	Hero = FindUnit(Runtime, HeroId);
	TestEqual(TEXT("Ice restores Mana only to the existing cap"), Hero->Mana, 10);
	TestEqual(TEXT("armor is old1 plus floor(8x25%)2 plus overflow4"), Hero->Armor, 7);
	TestEqual(TEXT("the Ice base effect deals no damage"), Result.DamageResults.Num(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageFireRewardTest,
	"GameXXK.Data.HeroCards.Mage.FireRewardBurnsAndTriggersTwicePerEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageFireRewardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56015)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Fire);
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("Fire reward resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("Fire emits one reward audit"), Results.Num(), 1);
	if (Results.Num() == 1)
	{
		TestEqual(TEXT("two Burn triggers per two enemies emit four packets"), CountDamage(Results[0].DamageResults, EGameXXKCardResolutionOrigin::TaskReward, EGameXXKCardDamageCause::Burn), 4);
		for (const FGameXXKCardDamageResult& DamageResult : Results[0].DamageResults)
		{
			TestEqual(TEXT("each Fire trigger consumes one Burn layer"), DamageResult.StatusStacksConsumed, 1);
		}
	}
	TestEqual(TEXT("EnemyA retains Burn6 after 8 then two triggers"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 6);
	TestEqual(TEXT("EnemyB retains Burn6 after 8 then two triggers"), Status(Runtime, EnemyBId, EGameXXKCardStatus::Burn), 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageIceRewardTest,
	"GameXXK.Data.HeroCards.Mage.IceRewardConsumesArmorIntoRepeatedGroupHits",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageIceRewardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56016)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Ice);
	FindUnit(Runtime, HeroId)->Armor = 3;
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("Ice reward resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("Ice consumes the complete armor snapshot"), FindUnit(Runtime, HeroId)->Armor, 0);
	TestEqual(TEXT("Armor3 emits three 20% group hits against each of two enemies"), Results.Num() == 1 ? CountDamage(Results[0].DamageResults, EGameXXKCardResolutionOrigin::TaskReward, EGameXXKCardDamageCause::DirectAttack) : 0, 6);
	if (Results.Num() == 1)
	{
		for (const FGameXXKCardDamageResult& DamageResult : Results[0].DamageResults)
		{
			TestEqual(TEXT("each Ice shard requests 20% of Attack10"), DamageResult.BaseRequestedDamage, 2);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageLightningRewardTest,
	"GameXXK.Data.HeroCards.Mage.LightningRewardLocksMarkCountsBeforeLiveConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageLightningRewardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {MakeCard(TEXT("Filler"), TEXT("Route.General.TuNaJue"), HeroId, 0)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("Filler")}, {}, {}, 56017)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Lightning);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Runtime, EnemyAId), EGameXXKCardStatus::Mark, 2);
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("Lightning reward resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("EnemyA locks five strikes and EnemyB locks three"), Results.Num() == 1 ? CountDamage(Results[0].DamageResults, EGameXXKCardResolutionOrigin::TaskReward, EGameXXKCardDamageCause::DirectAttack) : 0, 8);
	if (Results.Num() == 1)
	{
		for (const FGameXXKCardDamageResult& DamageResult : Results[0].DamageResults)
		{
			TestEqual(TEXT("each lightning strike requests 60% of Attack10"), DamageResult.BaseRequestedDamage, 6);
			TestEqual(TEXT("each locked strike consumes one live Mark"), DamageResult.MarkStacksConsumed, 1);
		}
	}
	TestEqual(TEXT("EnemyA spends all five locked Mark"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("EnemyB spends all three locked Mark"), Status(Runtime, EnemyBId, EGameXXKCardStatus::Mark), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageAutomaticAreaTargetingTest,
	"GameXXK.Data.HeroCards.Mage.OffenseUsesAutomaticEnemyGroupAndSelfEffectsUseOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageAutomaticAreaTargetingTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const FGameXXKCardDefinition* FireDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Mage.YanXuLiaoYuan"));
	const FGameXXKCardDefinition* IceDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Mage.HanXuNingChuan"));
	const FGameXXKCardDefinition* LightningDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Mage.LeiXuYinTing"));
	TestNotNull(TEXT("Fire starter remains catalogued"), FireDefinition);
	TestNotNull(TEXT("Ice starter remains catalogued"), IceDefinition);
	TestNotNull(TEXT("Lightning starter remains catalogued"), LightningDefinition);
	if (!FireDefinition || !IceDefinition || !LightningDefinition)
	{
		return false;
	}
	TestEqual(TEXT("Fire never opens a manual pointer"), FireDefinition->TargetSpec.Mode, EGameXXKCardTargetMode::AllEnemies);
	TestEqual(TEXT("Ice applies automatically to its caster"), IceDefinition->TargetSpec.Mode, EGameXXKCardTargetMode::Self);
	TestEqual(TEXT("Lightning never opens a manual pointer"), LightningDefinition->TargetSpec.Mode, EGameXXKCardTargetMode::AllEnemies);
	TestEqual(TEXT("Lightning keeps exactly Mark, strike, and task-search effects"), LightningDefinition->Effects.Num(), 3);
	if (LightningDefinition->Effects.Num() == 3)
	{
		TestEqual(TEXT("Lightning first marks every enemy"), LightningDefinition->Effects[0].Type, EGameXXKCardEffectType::ApplyStatus);
		TestEqual(TEXT("Lightning Mark is an enemy-group effect"), LightningDefinition->Effects[0].Target, EGameXXKCardEffectTarget::AllEnemies);
		TestEqual(TEXT("Lightning adds three Mark"), LightningDefinition->Effects[0].Magnitude, 3);
		TestEqual(TEXT("Lightning then locks one strike per Mark"), LightningDefinition->Effects[1].Type, EGameXXKCardEffectType::LightningPerTargetStatusSnapshot);
		TestEqual(TEXT("Lightning strikes each enemy independently"), LightningDefinition->Effects[1].Target, EGameXXKCardEffectTarget::AllEnemies);
		TestEqual(TEXT("base Lightning uses a restrained 30 percent strike"), LightningDefinition->Effects[1].Magnitude, 30);
	}

	FGameXXKCardBattleRuntime FireRuntime;
	if (!BuildRuntime(
		*this,
		FireRuntime,
		{MakeCard(TEXT("Fire"), TEXT("Hero.Mage.YanXuLiaoYuan"), HeroId, 0)},
		{TEXT("Fire")},
		{},
		{},
		56019))
	{
		return false;
	}
	FGameXXKCardPlayPreview FirePreview;
	FString Error;
	TestTrue(FString::Printf(TEXT("group Fire preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(FireRuntime, TEXT("Fire"), FirePreview, &Error));
	TestFalse(TEXT("group Fire preview does not request a pointer"), FirePreview.TargetRequest.bRequiresManualSelection);
	FGameXXKCardPlayResult FireResult;
	TestTrue(FString::Printf(TEXT("group Fire resolves without a selected target: %s"), *Error), GameXXKCardRules::ResolveCardPlay(FireRuntime, TEXT("Fire"), NAME_None, FireResult, &Error));
	TestEqual(TEXT("group Fire directly attacks both enemies"), CountDamage(FireResult.DamageResults, EGameXXKCardResolutionOrigin::ActivePlay, EGameXXKCardDamageCause::DirectAttack), 2);
	TestEqual(TEXT("group Fire applies Burn to EnemyA"), Status(FireRuntime, EnemyAId, EGameXXKCardStatus::Burn), 4);
	TestEqual(TEXT("group Fire applies Burn to EnemyB"), Status(FireRuntime, EnemyBId, EGameXXKCardStatus::Burn), 4);

	FGameXXKCardBattleRuntime LightningRuntime;
	if (!BuildRuntime(
		*this,
		LightningRuntime,
		{MakeCard(TEXT("Lightning"), TEXT("Hero.Mage.LeiXuYinTing"), HeroId, 0)},
		{TEXT("Lightning")},
		{},
		{},
		56020))
	{
		return false;
	}
	FGameXXKCardPlayPreview LightningPreview;
	TestTrue(FString::Printf(TEXT("group Lightning preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(LightningRuntime, TEXT("Lightning"), LightningPreview, &Error));
	TestFalse(TEXT("group Lightning preview does not request a pointer"), LightningPreview.TargetRequest.bRequiresManualSelection);
	FGameXXKCardPlayResult LightningResult;
	TestTrue(FString::Printf(TEXT("group Lightning resolves without a selected target: %s"), *Error), GameXXKCardRules::ResolveCardPlay(LightningRuntime, TEXT("Lightning"), NAME_None, LightningResult, &Error));
	TestEqual(TEXT("three locked Mark strikes hit each of two enemies"), CountDamage(LightningResult.DamageResults, EGameXXKCardResolutionOrigin::ActivePlay, EGameXXKCardDamageCause::DirectAttack), 6);
	TestEqual(TEXT("EnemyA spends its three locked Mark"), Status(LightningRuntime, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("EnemyB spends its three locked Mark"), Status(LightningRuntime, EnemyBId, EGameXXKCardStatus::Mark), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMageUniversalRewardTest,
	"GameXXK.Data.HeroCards.Mage.UniversalRewardDrawsEnergizesAndDiscountsOnlyHeroCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMageUniversalRewardTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKHeroMageRuntimeTest;
	const TArray<FGameXXKCardInstance> Cards = {
		MakeCard(TEXT("HeroPreview"), TEXT("Hero.Generic.QingFengYiShi"), HeroId, 0),
		MakeCard(TEXT("RoutePreview"), TEXT("Route.General.PoJiaTuCi"), HeroId, 1),
		MakeCard(TEXT("DrawA"), TEXT("Route.General.TuNaJue"), HeroId, 2),
		MakeCard(TEXT("DrawB"), TEXT("Route.General.QingShenQuShi"), HeroId, 3),
		MakeCard(TEXT("DrawC"), TEXT("Route.General.ShouShiHuiYuan"), HeroId, 4),
		MakeCard(TEXT("DrawD"), TEXT("Route.General.FeiZhen"), HeroId, 5)};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, Cards, {TEXT("HeroPreview"), TEXT("RoutePreview")}, {}, {}, 56018)) return false;
	SetCompletedTaskQueue(Runtime, EGameXXKHeroSpellTaskReward::Universal);
	Runtime.Deck.SharedEnergy = 3;
	TArray<FGameXXKCardPlayResult> Results;
	FString Error;
	TestTrue(FString::Printf(TEXT("Universal reward resolves: %s"), *Error), GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error));
	TestEqual(TEXT("Universal draws four real cards"), Runtime.Deck.Hand.Num(), 6);
	TestEqual(TEXT("Universal restores two shared energy"), Runtime.Deck.SharedEnergy, 5);
	FGameXXKCardPlayPreview HeroPreview;
	FGameXXKCardPlayPreview RoutePreview;
	TestTrue(FString::Printf(TEXT("discounted Hero preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("HeroPreview"), HeroPreview, &Error));
	TestTrue(FString::Printf(TEXT("undiscounted Route preview builds: %s"), *Error), GameXXKCardRules::BuildCardPlayPreview(Runtime, TEXT("RoutePreview"), RoutePreview, &Error));
	TestEqual(TEXT("the rest-of-round Hero discount floors cost at zero"), HeroPreview.EffectiveEnergyCost, 0);
	TestEqual(TEXT("the same modifier does not discount a Route card"), RoutePreview.EffectiveEnergyCost, 1);
	return true;
}

#endif
