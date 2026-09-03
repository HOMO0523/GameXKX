#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcSpellTaskRuntimeTest
{
	const FName AllyUnitId(TEXT("Ally.Task"));
	const FName EnemyAId(TEXT("Enemy.A"));
	const FName EnemyBId(TEXT("Enemy.B"));

	struct FCardSpec
	{
		const TCHAR* InstanceId;
		const TCHAR* CardId;
		bool bStartsInHand;
	};

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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 2000 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = 10;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 60 : 0;
		Unit.MaxMana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardInstance MakeCard(
		const FName OwnerUnitId,
		const TCHAR* InstanceId,
		const TCHAR* CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(InstanceId);
		Card.CardId = FName(CardId);
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("TaskNpc.Spell.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool BuildRuntime(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& OutRuntime,
		const FName OwnerUnitId,
		const TArray<FCardSpec>& TaskCards,
		const int32 Seed)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < TaskCards.Num(); ++Index)
		{
			Cards.Add(MakeCard(OwnerUnitId, TaskCards[Index].InstanceId, TaskCards[Index].CardId, Index));
		}
		const TCHAR* FillerIds[] = {
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi")};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FillerIds)); ++Index)
		{
			Cards.Add(MakeCard(
				OwnerUnitId,
				*FString::Printf(TEXT("Filler.%d"), Index),
				FillerIds[Index],
				TaskCards.Num() + Index));
		}

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 1),
			MakeUnit(AllyUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
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
			Test.AddError(FString::Printf(TEXT("task-NPC spell runtime failed to initialize: %s"), *Error));
			return false;
		}

		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			const FCardSpec* TaskSpec = TaskCards.FindByPredicate([&Card](const FCardSpec& Candidate)
			{
				return Card.InstanceId == FName(Candidate.InstanceId);
			});
			(TaskSpec && TaskSpec->bStartsInHand ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("deterministic task-NPC spell fixture is invalid: %s"), *Error));
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

	int32 Status(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus StatusType)
	{
		const FGameXXKCardCombatUnit* Unit = Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
		return Unit ? GameXXKCardRules::GetCombatStatusStacks(*Unit, StatusType) : INDEX_NONE;
	}

	bool Resolve(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FName TargetUnitId,
		FGameXXKCardPlayResult& OutResult,
		const TCHAR* Context)
	{
		FString Error;
		const bool bResolved = GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetUnitId, OutResult, &Error);
		Test.TestTrue(FString::Printf(TEXT("%s resolves: %s"), Context, *Error), bResolved);
		return bResolved;
	}

	int32 CountOrigin(const TArray<FGameXXKCardDamageResult>& Results, const EGameXXKCardResolutionOrigin Origin)
	{
		int32 Count = 0;
		for (const FGameXXKCardDamageResult& Result : Results)
		{
			Count += Result.ResolutionOrigin == Origin ? 1 : 0;
		}
		return Count;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcSpellReplayTest,
	"GameXXK.Data.TaskNpcCards.Runtime.SpellTask.ThreeDistinctCardsReplayInOrderAndUseStarterRewardOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcSpellReplayTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcSpellTaskRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.YueBai"));
	const TArray<FCardSpec> Cards = {
		{TEXT("QingYan"), TEXT("Npc.YueBai.QingYanDianDeng"), true},
		{TEXT("CanJuan"), TEXT("Npc.YueBai.CanJuanPiZhu"), true},
		{TEXT("ShanHe"), TEXT("Npc.YueBai.ShanHeCanTu"), true}};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, OwnerUnitId, Cards, 58401)) return false;
	FindUnit(Runtime, OwnerUnitId)->Defense = 100;
	for (FGameXXKCardInstance& Card : Runtime.Deck.Hand)
	{
		if (Card.CardId == FName(TEXT("Npc.YueBai.ShanHeCanTu"))) Card.CurrentQuality = EGameXXKCardQuality::Epic;
	}

	FGameXXKCardPlayResult FirstResult;
	if (!Resolve(*this, Runtime, TEXT("QingYan"), EnemyAId, FirstResult, TEXT("月白任务首牌"))) return true;
	TestEqual(TEXT("first task card creates one named-NPC task"), Runtime.TaskNpcSpellTasks.Num(), 1);
	if (Runtime.TaskNpcSpellTasks.Num() == 1)
	{
		TestEqual(TEXT("task locks exactly the three carried CardIds"), Runtime.TaskNpcSpellTasks[0].LockedCardIds.Num(), 3);
		TestEqual(TEXT("first distinct CardId is completed"), Runtime.TaskNpcSpellTasks[0].CompletedCardIds, TArray<FName>{TEXT("Npc.YueBai.QingYanDianDeng")});
		TestEqual(TEXT("first-play order records the original enemy"), Runtime.TaskNpcSpellTasks[0].FirstPlayOrder[0].OriginalTargetUnitIds, TArray<FName>{EnemyAId});
	}
	else
	{
		return true;
	}

	FGameXXKCardPlayResult SecondResult;
	if (!Resolve(*this, Runtime, TEXT("CanJuan"), NAME_None, SecondResult, TEXT("月白任务次牌"))) return true;
	if (Runtime.TaskNpcSpellTasks.Num() != 1)
	{
		AddError(TEXT("task state disappeared before the three-card sequence completed"));
		return true;
	}
	TestEqual(TEXT("two distinct carried cards advance progress to two"), Runtime.TaskNpcSpellTasks[0].CompletedCardIds.Num(), 2);

	FGameXXKCardPlayResult ThirdResult;
	if (!Resolve(*this, Runtime, TEXT("ShanHe"), NAME_None, ThirdResult, TEXT("月白任务末牌"))) return true;
	TestEqual(TEXT("task state resets only after three replays and the starter reward"), Runtime.TaskNpcSpellTasks.Num(), 0);
	TestEqual(TEXT("three real plays remain exactly three active cards"), Runtime.ActiveCardsPlayedThisRound, 3);
	TestEqual(TEXT("completion audits three base replays plus one reward"), ThirdResult.AutomaticResolutionCount, 4);
	TestEqual(TEXT("only replayed 青焰 contributes one task-replay damage packet"), CountOrigin(ThirdResult.DamageResults, EGameXXKCardResolutionOrigin::TaskNpcTaskReplay), 1);
	TestEqual(TEXT("青焰 starter reward contributes one packet per enemy"), CountOrigin(ThirdResult.DamageResults, EGameXXKCardResolutionOrigin::TaskReward), 2);
	TestEqual(TEXT("山河 base and replay each grant the native Epic fifty-six Armor"), FindUnit(Runtime, OwnerUnitId)->Armor, 112);
	TestEqual(TEXT("ally also receives both full fifty-six Armor grants"), FindUnit(Runtime, AllyUnitId)->Armor, 112);
	TestEqual(TEXT("enemy A Burn reaches its level-one cap after ordered terrain and starter effects"), Status(Runtime, EnemyAId, EGameXXKCardStatus::Burn), 25);
	TestEqual(TEXT("enemy B receives only the starter's group Burn under the current terrain rules"), Status(Runtime, EnemyBId, EGameXXKCardStatus::Burn), 7);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcSpellSearchTest,
	"GameXXK.Data.TaskNpcCards.Runtime.SpellTask.SearchOffersOnlyCarriedUnfinishedCards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcSpellSearchTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcSpellTaskRuntimeTest;
	const FName OwnerUnitId(TEXT("Npc.SongJinBao"));
	const TArray<FCardSpec> Cards = {
		{TEXT("ErMu"), TEXT("Npc.SongJinBao.ErMuMiBao"), true},
		{TEXT("GuiKe"), TEXT("Npc.SongJinBao.GuiKeLing"), false},
		{TEXT("YiNuo"), TEXT("Npc.SongJinBao.YiNuoQianJin"), false}};
	FGameXXKCardBattleRuntime Runtime;
	if (!BuildRuntime(*this, Runtime, OwnerUnitId, Cards, 58402)) return false;
	FGameXXKCardPlayResult Result;
	if (!Resolve(*this, Runtime, TEXT("ErMu"), EnemyAId, Result, TEXT("耳目密报任务检索"))) return true;

	TestEqual(TEXT("search card starts one named-NPC task"), Runtime.TaskNpcSpellTasks.Num(), 1);
	TestTrue(TEXT("search opens the existing choose-to-hand interaction"), Result.bOpenedPendingChoice);
	TestEqual(TEXT("task-NPC search reuses the existing choice kind"), Runtime.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand);
	TestEqual(TEXT("only the two carried unfinished cards are offered"), Runtime.Deck.PendingChoice.Candidates.Num(), 2);
	TSet<FName> CandidateCardIds;
	for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
	{
		CandidateCardIds.Add(Candidate.CardId);
	}
	TestTrue(TEXT("贵客令 is offered"), CandidateCardIds.Contains(TEXT("Npc.SongJinBao.GuiKeLing")));
	TestTrue(TEXT("一诺千金 is offered"), CandidateCardIds.Contains(TEXT("Npc.SongJinBao.YiNuoQianJin")));
	TestFalse(TEXT("unselected fourth card 赏钱鼓舞 cannot be generated"), CandidateCardIds.Contains(TEXT("Npc.SongJinBao.ShangQianGuWu")));
	TestFalse(TEXT("already completed 耳目密报 cannot be offered"), CandidateCardIds.Contains(TEXT("Npc.SongJinBao.ErMuMiBao")));

	FString Error;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	TestTrue(
		FString::Printf(TEXT("existing search submission accepts the task-NPC candidate: %s"), *Error),
		GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, TEXT("GuiKe"), ResumedResults, &Error));
	TestTrue(TEXT("chosen carried card moves to hand"), Runtime.Deck.Hand.ContainsByPredicate([](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == FName(TEXT("GuiKe"));
	}));
	if (Runtime.TaskNpcSpellTasks.Num() == 1)
	{
		TestEqual(TEXT("search itself does not advance a second task CardId"), Runtime.TaskNpcSpellTasks[0].CompletedCardIds.Num(), 1);
	}
	return true;
}

#endif
