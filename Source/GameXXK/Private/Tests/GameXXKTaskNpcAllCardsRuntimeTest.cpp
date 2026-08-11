#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTaskNpcAllCardsRuntimeTest
{
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
		Unit.HP = Side == EGameXXKCardTargetSide::Enemy ? 20000 : 500;
		Unit.MaxHP = Unit.HP;
		Unit.Attack = Attack;
		Unit.Defense = 0;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 200 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
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
		Card.SourceEntryId = FName(*FString::Printf(TEXT("TaskNpc.AllCards.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	bool UsesThreeCardTask(const FName OwnerId)
	{
		return OwnerId == FName(TEXT("Npc.SongJinBao"))
			|| OwnerId == FName(TEXT("Npc.YueBai"));
	}

	bool BuildRuntimeForCard(
		FAutomationTestBase& Test,
		const FGameXXKCardDefinition& Definition,
		FGameXXKCardBattleRuntime& OutRuntime,
		FName& OutPlayedInstanceId)
	{
		TArray<FGameXXKCardInstance> Cards;
		OutPlayedInstanceId = FName(*FString::Printf(TEXT("Played.%s"), *Definition.Id.ToString()));
		Cards.Add(MakeCard(OutPlayedInstanceId, Definition.Id, Definition.OwnerId, 0));
		if (UsesThreeCardTask(Definition.OwnerId))
		{
			const TArray<FGameXXKCardDefinition> OwnerCards = FGameXXKCardCatalog::GetCardDefinitionsForOwner(Definition.OwnerId);
			for (const FGameXXKCardDefinition& Candidate : OwnerCards)
			{
				if (Candidate.Id == Definition.Id || Cards.Num() >= 3)
				{
					continue;
				}
				Cards.Add(MakeCard(
					FName(*FString::Printf(TEXT("Task.%d"), Cards.Num())),
					Candidate.Id,
					Definition.OwnerId,
					Cards.Num()));
			}
		}

		const TCHAR* FillerCardIds[] = {
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"),
			TEXT("Hero.Generic.GuanXi")};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FillerCardIds)); ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("Filler.%d"), Index)),
				FName(FillerCardIds[Index]),
				TEXT("Hero"),
				Cards.Num()));
		}

		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(Definition.OwnerId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 12, 1),
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 30, 2),
			MakeUnit(TEXT("Ally.Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 20, 3),
			MakeUnit(TEXT("Enemy.A"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 10),
			MakeUnit(TEXT("Enemy.B"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			59000 + Definition.Id.GetNumber(),
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("%s fixture initialization failed: %s"), *Definition.Id.ToString(), *Error));
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(Card.InstanceId == OutPlayedInstanceId ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 20;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s deterministic fixture is invalid: %s"), *Definition.Id.ToString(), *Error));
			return false;
		}
		return true;
	}

	bool ContainsCardId(const TArray<FGameXXKCardInstance>& Cards, const FName CardId)
	{
		return Cards.ContainsByPredicate([CardId](const FGameXXKCardInstance& Card)
		{
			return Card.CardId == CardId;
		});
	}

	bool RuntimeReferencesCardId(const FGameXXKCardBattleRuntime& Runtime, const FName CardId)
	{
		if (ContainsCardId(Runtime.Deck.Hand, CardId)
			|| ContainsCardId(Runtime.Deck.DrawPile, CardId)
			|| ContainsCardId(Runtime.Deck.DiscardPile, CardId)
			|| ContainsCardId(Runtime.Deck.ExhaustPile, CardId)
			|| ContainsCardId(Runtime.Deck.PendingAutomaticHandCards, CardId)
			|| ContainsCardId(Runtime.Deck.PendingChoice.Candidates, CardId))
		{
			return true;
		}
		for (const FGameXXKTaskNpcSpellTaskRuntime& Task : Runtime.TaskNpcSpellTasks)
		{
			if (Task.LockedCardIds.Contains(CardId) || Task.CompletedCardIds.Contains(CardId))
			{
				return true;
			}
			for (const FGameXXKResolvedCardSnapshot& Snapshot : Task.FirstPlayOrder)
			{
				if (Snapshot.CardId == CardId)
				{
					return true;
				}
			}
		}
		return false;
	}

	bool HasSelfContainedNpcPayload(const FGameXXKCardDefinition& Definition)
	{
		const auto HasStatusProducer = [&Definition](const EGameXXKCardStatus Status, const int32 BeforeIndex = MAX_int32)
		{
			for (int32 Index = 0; Index < Definition.Effects.Num() && Index < BeforeIndex; ++Index)
			{
				const FGameXXKCardEffect& Effect = Definition.Effects[Index];
				if (Effect.Type == EGameXXKCardEffectType::ApplyStatus && Effect.Status == Status && Effect.Magnitude > 0)
				{
					return true;
				}
			}
			return false;
		};

		if (Definition.HeavyArrow.Kind != EGameXXKHeavyArrowKind::None
			&& !HasStatusProducer(EGameXXKCardStatus::Charge))
		{
			return false;
		}
		for (int32 Index = 0; Index < Definition.Effects.Num(); ++Index)
		{
			const FGameXXKCardEffect& Effect = Definition.Effects[Index];
			if (Effect.Type == EGameXXKCardEffectType::HealOrReverseWithMedicine
				&& !HasStatusProducer(EGameXXKCardStatus::Medicine, Index))
			{
				return false;
			}
			if (Effect.Type == EGameXXKCardEffectType::ResolveToxicExplosion
				&& !HasStatusProducer(EGameXXKCardStatus::Bleed, Index)
				&& !HasStatusProducer(EGameXXKCardStatus::Poison, Index)
				&& !HasStatusProducer(EGameXXKCardStatus::Burn, Index))
			{
				return false;
			}
			if (Effect.Type == EGameXXKCardEffectType::TriggerStatus
				&& !HasStatusProducer(Effect.Status, Index))
			{
				return false;
			}
		}
		return true;
	}

	bool BuildRuntimeForSelection(
		FAutomationTestBase& Test,
		const FName OwnerUnitId,
		const TArray<FName>& SelectedCardIds,
		const int32 PlayedIndex,
		const bool bAllSelectedInHand,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime,
		TArray<FName>& OutSelectedInstanceIds)
	{
		TArray<FGameXXKCardInstance> Cards;
		OutSelectedInstanceIds.Reset();
		for (int32 Index = 0; Index < SelectedCardIds.Num(); ++Index)
		{
			const FName InstanceId(*FString::Printf(TEXT("Selected.%d"), Index));
			OutSelectedInstanceIds.Add(InstanceId);
			Cards.Add(MakeCard(InstanceId, SelectedCardIds[Index], OwnerUnitId, Index));
		}
		const TCHAR* FillerCardIds[] = {
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"),
			TEXT("Hero.Generic.GuanXi")};
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(FillerCardIds)); ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("Selection.Filler.%d"), Index)),
				FName(FillerCardIds[Index]),
				TEXT("Hero"),
				Cards.Num()));
		}

		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 12, 1),
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 30, 2),
			MakeUnit(TEXT("Ally.Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 20, 3),
			MakeUnit(TEXT("Enemy.A"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 10),
			MakeUnit(TEXT("Enemy.B"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 11)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, EGameXXKCardTerrain::Plain, Seed, &Error))
		{
			Test.AddError(FString::Printf(TEXT("three-card selection fixture initialization failed: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.PendingAutomaticHandCards.Reset();
		for (int32 Index = 0; Index < Cards.Num(); ++Index)
		{
			if (Index < SelectedCardIds.Num())
			{
				if (bAllSelectedInHand || Index == PlayedIndex)
				{
					OutRuntime.Deck.Hand.Add(Cards[Index]);
				}
				else if (((Index + PlayedIndex) & 1) == 0)
				{
					OutRuntime.Deck.DrawPile.Add(Cards[Index]);
				}
				else
				{
					OutRuntime.Deck.DiscardPile.Add(Cards[Index]);
				}
			}
			else
			{
				OutRuntime.Deck.DrawPile.Add(Cards[Index]);
			}
		}
		OutRuntime.Deck.SharedEnergy = 50;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("three-card selection fixture validation failed: %s"), *Error));
			return false;
		}
		return true;
	}

	bool ResolveSelectedCard(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		FGameXXKCardPlayResult& OutResult,
		const FString& Context)
	{
		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s preview failed: %s"), *Context, *Error));
			return false;
		}
		FName TargetUnitId = NAME_None;
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
				[](const FGameXXKCardTargetCandidateView& View)
				{
					return View.bCanSelect;
				});
			if (!Candidate)
			{
				Test.AddError(FString::Printf(TEXT("%s exposes no legal manual target"), *Context));
				return false;
			}
			TargetUnitId = Candidate->UnitId;
		}
		if (!GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, TargetUnitId, OutResult, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s resolution failed: %s"), *Context, *Error));
			return false;
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcAllCardsResolveTest,
	"GameXXK.Data.TaskNpcCards.Runtime.All24Cards.PreviewAndBaseResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcAllCardsResolveTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcAllCardsRuntimeTest;
	TArray<const FGameXXKCardDefinition*> TaskNpcCards;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner == EGameXXKCardOwner::QuestNpc)
		{
			TaskNpcCards.Add(&Definition);
		}
	}
	TestEqual(TEXT("runtime matrix covers exactly 24 task-NPC cards"), TaskNpcCards.Num(), 24);

	for (const FGameXXKCardDefinition* Definition : TaskNpcCards)
	{
		if (!Definition)
		{
			AddError(TEXT("task-NPC runtime matrix contains a null definition"));
			continue;
		}
		FGameXXKCardBattleRuntime Runtime;
		FName InstanceId;
		if (!BuildRuntimeForCard(*this, *Definition, Runtime, InstanceId))
		{
			continue;
		}

		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error))
		{
			AddError(FString::Printf(TEXT("%s preview failed: %s"), *Definition->Id.ToString(), *Error));
			continue;
		}
		FName SelectedTargetUnitId = NAME_None;
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
				[](const FGameXXKCardTargetCandidateView& View)
				{
					return View.bCanSelect;
				});
			if (!Candidate)
			{
				AddError(FString::Printf(TEXT("%s manual preview exposed no legal target"), *Definition->Id.ToString()));
				continue;
			}
			SelectedTargetUnitId = Candidate->UnitId;
		}

		FGameXXKCardPlayResult Result;
		Error.Reset();
		if (!GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, SelectedTargetUnitId, Result, &Error))
		{
			AddError(FString::Printf(TEXT("%s base resolution failed: %s"), *Definition->Id.ToString(), *Error));
			continue;
		}
		TestEqual(FString::Printf(TEXT("%s reports its stable CardId"), *Definition->Id.ToString()), Result.CardId, Definition->Id);
		TestEqual(FString::Printf(TEXT("%s counts as one active play"), *Definition->Id.ToString()), Runtime.ActiveCardsPlayedThisRound, 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcAllThreeCardCompositionsTest,
	"GameXXK.Data.TaskNpcCards.Runtime.All24Cards.AllFourThreeCardCompositions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcAllThreeCardCompositionsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTaskNpcAllCardsRuntimeTest;
	int32 CompositionCount = 0;
	int32 ResolutionCount = 0;
	int32 CompletedSpellTaskCount = 0;
	int32 Seed = 59200;

	for (const FGameXXKQuestNpcDefinition& Npc : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		TestEqual(FString::Printf(TEXT("%s retains four candidates"), *Npc.NpcId.ToString()), Npc.FixedCardIds.Num(), 4);
		for (int32 OmittedIndex = 0; OmittedIndex < Npc.FixedCardIds.Num(); ++OmittedIndex)
		{
			const FName OmittedCardId = Npc.FixedCardIds[OmittedIndex];
			TArray<FName> SelectedCardIds = Npc.FixedCardIds;
			SelectedCardIds.RemoveAt(OmittedIndex, 1, EAllowShrinking::No);
			++CompositionCount;

			for (int32 PlayedIndex = 0; PlayedIndex < SelectedCardIds.Num(); ++PlayedIndex)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(SelectedCardIds[PlayedIndex]);
				if (!TestNotNull(FString::Printf(TEXT("selected NPC card exists: %s"), *SelectedCardIds[PlayedIndex].ToString()), Definition))
				{
					continue;
				}
				TestTrue(
					FString::Printf(TEXT("%s carries its own resource producer before Heavy Arrow, Medicine, DOT explosion, or status trigger consumption"), *Definition->Id.ToString()),
					HasSelfContainedNpcPayload(*Definition));

				FGameXXKCardBattleRuntime Runtime;
				TArray<FName> InstanceIds;
				if (!BuildRuntimeForSelection(*this, Npc.NpcId, SelectedCardIds, PlayedIndex, false, Seed++, Runtime, InstanceIds))
				{
					continue;
				}
				const FString Context = FString::Printf(
					TEXT("owner=%s omitted=%s played=%s"),
					*Npc.NpcId.ToString(),
					*OmittedCardId.ToString(),
					*SelectedCardIds[PlayedIndex].ToString());
				FGameXXKCardPlayResult Result;
				if (!ResolveSelectedCard(*this, Runtime, InstanceIds[PlayedIndex], Result, Context))
				{
					continue;
				}
				++ResolutionCount;

				for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
				{
					TestTrue(FString::Printf(TEXT("%s pending choice offers only selected NPC cards or non-NPC fillers"), *Context),
						Candidate.OwnerUnitId != Npc.NpcId || SelectedCardIds.Contains(Candidate.CardId));
				}
				TestFalse(FString::Printf(TEXT("%s never materializes the omitted fourth card"), *Context), RuntimeReferencesCardId(Runtime, OmittedCardId));

				if (UsesThreeCardTask(Npc.NpcId))
				{
					TestEqual(FString::Printf(TEXT("%s starts exactly one carried-three task"), *Context), Runtime.TaskNpcSpellTasks.Num(), 1);
					if (Runtime.TaskNpcSpellTasks.Num() == 1)
					{
						TestEqual(FString::Printf(TEXT("%s task locks the selected trio in stable order"), *Context), Runtime.TaskNpcSpellTasks[0].LockedCardIds, SelectedCardIds);
					}
				}

				FString Error;
				TArray<FGameXXKCardPlayResult> ResumedResults;
				if (Runtime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard
					&& !Runtime.Deck.PendingChoice.Candidates.IsEmpty())
				{
					TestTrue(FString::Printf(TEXT("%s forced discard resumes: %s"), *Context, *Error),
						GameXXKCardRules::SubmitForcedDiscard(Runtime, {Runtime.Deck.PendingChoice.Candidates[0].InstanceId}, &Error, &ResumedResults));
				}
				else if (Runtime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand
					&& !Runtime.Deck.PendingChoice.Candidates.IsEmpty())
				{
					TestTrue(FString::Printf(TEXT("%s task search resumes: %s"), *Context, *Error),
						GameXXKCardRules::SubmitHeroTaskSearchChoice(Runtime, Runtime.Deck.PendingChoice.Candidates[0].InstanceId, ResumedResults, &Error));
				}
				TestFalse(FString::Printf(TEXT("%s omitted card stays absent after choice continuation"), *Context), RuntimeReferencesCardId(Runtime, OmittedCardId));
				TestTrue(FString::Printf(TEXT("%s final runtime validates: %s"), *Context, *Error), GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));
			}

			if (UsesThreeCardTask(Npc.NpcId))
			{
				FGameXXKCardBattleRuntime Runtime;
				TArray<FName> InstanceIds;
				if (!BuildRuntimeForSelection(*this, Npc.NpcId, SelectedCardIds, INDEX_NONE, true, Seed++, Runtime, InstanceIds))
				{
					continue;
				}
				bool bSequenceResolved = true;
				for (int32 Index = 0; Index < InstanceIds.Num(); ++Index)
				{
					FGameXXKCardPlayResult Result;
					const FString Context = FString::Printf(TEXT("%s omitted=%s task step=%d"), *Npc.NpcId.ToString(), *OmittedCardId.ToString(), Index + 1);
					if (!ResolveSelectedCard(*this, Runtime, InstanceIds[Index], Result, Context))
					{
						bSequenceResolved = false;
						break;
					}
					TestFalse(FString::Printf(TEXT("%s never queues the omitted fourth card"), *Context), RuntimeReferencesCardId(Runtime, OmittedCardId));
					if (Index < 2)
					{
						TestEqual(FString::Printf(TEXT("%s keeps one active task before completion"), *Context), Runtime.TaskNpcSpellTasks.Num(), 1);
						if (Runtime.TaskNpcSpellTasks.Num() == 1)
						{
							TestEqual(FString::Printf(TEXT("%s preserves the exact selected lock"), *Context), Runtime.TaskNpcSpellTasks[0].LockedCardIds, SelectedCardIds);
							TestEqual(FString::Printf(TEXT("%s advances one distinct card"), *Context), Runtime.TaskNpcSpellTasks[0].CompletedCardIds.Num(), Index + 1);
						}
					}
					else
					{
						TestEqual(FString::Printf(TEXT("%s completes and resets its three-card task"), *Context), Runtime.TaskNpcSpellTasks.Num(), 0);
						TestEqual(FString::Printf(TEXT("%s replays three bases and only the starter reward"), *Context), Result.AutomaticResolutionCount, 4);
					}
				}
				CompletedSpellTaskCount += bSequenceResolved ? 1 : 0;
			}
		}
	}

	TestEqual(TEXT("six NPCs expose all four omitted-card compositions"), CompositionCount, 6 * 4);
	TestEqual(TEXT("every selected card previews and resolves in every carried-three context"), ResolutionCount, 6 * 4 * 3);
	TestEqual(TEXT("both spell NPCs complete all four carried-three tasks"), CompletedSpellTaskCount, 2 * 4);
	return true;
}

#endif
