#include "GameXXKCombatSimulationRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"

namespace
{
	struct FGameXXKScoredSimulationDecision
	{
		FGameXXKSimulationDecision Decision;
		int64 Score = MIN_int64;
		int32 AcquisitionOrdinal = MAX_int32;
	};

	static bool SetFailure(
		FGameXXKSimulationMetrics& OutMetrics,
		FString* OutError,
		const FName Reason,
		const FString& Message)
	{
		OutMetrics.FailureReason = Reason;
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	static const FGameXXKCardCombatUnit* FindUnit(const TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	static int32 SumPartyHealth(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Total += FMath::Max(0, Unit.HP);
			}
		}
		return Total;
	}

	static int32 SumAllHealth(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Total += FMath::Max(0, Unit.HP);
		}
		return Total;
	}

	static int32 SumMana(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Total += Unit.Mana;
		}
		return Total;
	}

	static int32 SumArmor(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Total += Unit.Armor;
		}
		return Total;
	}

	static int32 SumLivingEnemyHealth(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.bLiving)
			{
				Total += FMath::Max(0, Unit.HP);
			}
		}
		return Total;
	}

	static int32 GetTotalStatusStacks(const FGameXXKCardBattleRuntime& Runtime, const EGameXXKCardStatus Status)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Total += GameXXKCardRules::GetCombatStatusStacks(Unit, Status);
		}
		return Total;
	}

	static FName MakeStatusMetricKey(const EGameXXKCardStatus Status)
	{
		return FName(*FString::Printf(TEXT("Status.%d"), static_cast<int32>(Status)));
	}

	static void AddMetric(TMap<FName, int64>& InOutMetrics, const FName Key, const int64 Value)
	{
		if (!Key.IsNone() && Value != 0)
		{
			InOutMetrics.FindOrAdd(Key) += Value;
		}
	}

	static void RecordStatusDeltas(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		for (int32 RawStatus = static_cast<int32>(EGameXXKCardStatus::Momentum);
			RawStatus <= static_cast<int32>(EGameXXKCardStatus::TerrainBonusDoubleThisRound);
			++RawStatus)
		{
			const EGameXXKCardStatus Status = static_cast<EGameXXKCardStatus>(RawStatus);
			const int32 Delta = GetTotalStatusStacks(After, Status) - GetTotalStatusStacks(Before, Status);
			if (Delta > 0)
			{
				AddMetric(InOutMetrics.StatusProduced, MakeStatusMetricKey(Status), Delta);
			}
			else if (Delta < 0)
			{
				AddMetric(InOutMetrics.StatusConsumed, MakeStatusMetricKey(Status), -Delta);
			}
		}
	}

	static void RecordDamageMetrics(
		const TArray<FGameXXKCardDamageResult>& DamageResults,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		for (const FGameXXKCardDamageResult& Damage : DamageResults)
		{
			AddMetric(InOutMetrics.DamageBySource, Damage.SourceUnitId, Damage.HealthDamage);
		}
	}

	static void RecordAction(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		const FName Action,
		const FName SourceUnitId,
		const FName CardOrIntentId,
		const FName TargetUnitId,
		const TArray<FGameXXKCardDamageResult>& DamageResults,
		FGameXXKSimulationMetrics& InOutMetrics,
		TArray<FGameXXKSimulationTraceEntry>& OutTrace)
	{
		const int32 PartyHealthDelta = SumPartyHealth(After) - SumPartyHealth(Before);
		const int32 TotalHealthDelta = SumAllHealth(After) - SumAllHealth(Before);
		const int32 ManaDelta = SumMana(After) - SumMana(Before);
		const int32 ArmorDelta = SumArmor(After) - SumArmor(Before);
		if (PartyHealthDelta > 0)
		{
			AddMetric(InOutMetrics.HealingBySource, SourceUnitId, PartyHealthDelta);
		}
		if (ArmorDelta > 0)
		{
			AddMetric(InOutMetrics.ArmorBySource, SourceUnitId, ArmorDelta);
		}
		RecordDamageMetrics(DamageResults, InOutMetrics);
		RecordStatusDeltas(Before, After, InOutMetrics);

		FGameXXKSimulationTraceEntry Trace;
		Trace.Round = After.RoundNumber;
		Trace.Action = Action;
		Trace.SourceUnitId = SourceUnitId;
		Trace.CardOrIntentId = CardOrIntentId;
		Trace.TargetUnitId = TargetUnitId;
		Trace.HealthDelta = TotalHealthDelta;
		Trace.ManaDelta = ManaDelta;
		Trace.ArmorDelta = ArmorDelta;
		OutTrace.Add(MoveTemp(Trace));
	}

	static void RecordFirstRoundDefeats(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		TSet<FName>& InOutCountedDefeats,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		if (Before.RoundNumber != 1)
		{
			return;
		}
		for (const FGameXXKCardCombatUnit& BeforeUnit : Before.Units)
		{
			const FGameXXKCardCombatUnit* AfterUnit = FindUnit(After.Units, BeforeUnit.UnitId);
			if (BeforeUnit.bLiving && AfterUnit && !AfterUnit->bLiving && !InOutCountedDefeats.Contains(BeforeUnit.UnitId))
			{
				InOutCountedDefeats.Add(BeforeUnit.UnitId);
				++InOutMetrics.FirstRoundDeaths;
			}
		}
	}

	static bool ValidateRuntimeBounds(const FGameXXKRuntimeState& State, FString* OutError)
	{
		if (!State.CardRun.bHasActiveCardBattle
			|| !GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, OutError))
		{
			return false;
		}
		for (const FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.HP < 0 || Unit.Mana < 0 || Unit.Armor < 0)
			{
				if (OutError)
				{
					*OutError = TEXT("Card battle produced a negative combat resource.");
				}
				return false;
			}
		}
		return true;
	}

	static int64 ScoreCandidateResolution(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		const FGameXXKCardPlayPreview& Preview)
	{
		int64 Score = 0;
		for (const FGameXXKCardCombatUnit& BeforeUnit : Before.Units)
		{
			const FGameXXKCardCombatUnit* AfterUnit = FindUnit(After.Units, BeforeUnit.UnitId);
			if (!AfterUnit)
			{
				continue;
			}
			if (BeforeUnit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Score += static_cast<int64>(FMath::Max(0, BeforeUnit.HP - AfterUnit->HP)) * 100;
				if (BeforeUnit.bLiving && !AfterUnit->bLiving)
				{
					Score += 25000;
				}
			}
			else
			{
				Score += static_cast<int64>(FMath::Max(0, AfterUnit->HP - BeforeUnit.HP)) * 35;
				Score += static_cast<int64>(FMath::Max(0, AfterUnit->Armor - BeforeUnit.Armor)) * 8;
				if (BeforeUnit.bLiving && !AfterUnit->bLiving)
				{
					Score -= 50000;
				}
			}
		}
		if (SumLivingEnemyHealth(After) == 0)
		{
			Score += 1000000;
		}
		return Score - Preview.EffectiveEnergyCost * 4 - Preview.EffectiveManaCost * 2;
	}

	static bool ChooseSkilledDecision(
		const FGameXXKRuntimeState& State,
		FGameXXKSimulationDecision& OutDecision,
		FString* OutError)
	{
		OutDecision = FGameXXKSimulationDecision();
		const FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
		FGameXXKScoredSimulationDecision Best;
		TArray<FGameXXKCardInstance> Hand = Runtime.Deck.Hand;
		Hand.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
		{
			return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
				? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
				: Left.InstanceId.LexicalLess(Right.InstanceId);
		});

		for (const FGameXXKCardInstance& Card : Hand)
		{
			FGameXXKCardPlayPreview Preview;
			FString PreviewError;
			if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, Card.InstanceId, Preview, &PreviewError)
				|| !Preview.bCanPlay)
			{
				continue;
			}

			TArray<FName> Targets;
			if (Preview.TargetRequest.bRequiresManualSelection)
			{
				for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
				{
					if (Candidate.bCanSelect)
					{
						Targets.Add(Candidate.UnitId);
					}
				}
				Targets.Sort([](const FName Left, const FName Right) { return Left.LexicalLess(Right); });
			}
			else
			{
				Targets.Add(NAME_None);
			}

			for (const FName Target : Targets)
			{
				FGameXXKRuntimeState CandidateState = State;
				FGameXXKCardPlayResult Result;
				FString ResolveError;
				if (!FGameXXKCardBattleAdapter::ResolveCardPlay(CandidateState, Card.InstanceId, Target, Result, &ResolveError))
				{
					continue;
				}
				const int64 Score = ScoreCandidateResolution(Runtime, CandidateState.CardRun.ActiveBattle, Preview);
				if (Score <= 0)
				{
					continue;
				}
				const bool bBetter = Score > Best.Score
					|| (Score == Best.Score && Card.AcquisitionOrdinal < Best.AcquisitionOrdinal)
					|| (Score == Best.Score && Card.AcquisitionOrdinal == Best.AcquisitionOrdinal
						&& (Best.Decision.TargetUnitId.IsNone() || Target.LexicalLess(Best.Decision.TargetUnitId)));
				if (bBetter)
				{
					Best.Score = Score;
					Best.AcquisitionOrdinal = Card.AcquisitionOrdinal;
					Best.Decision.CardInstanceId = Card.InstanceId;
					Best.Decision.TargetUnitId = Target;
				}
			}
		}

		if (Best.Score == MIN_int64)
		{
			OutDecision.bEndPlayerPhase = true;
			return true;
		}
		OutDecision = Best.Decision;
		return true;
	}

	static bool ResolvePendingChoice(
		FGameXXKRuntimeState& InOutState,
		FGameXXKSimulationMetrics& InOutMetrics,
		TArray<FGameXXKSimulationTraceEntry>& OutTrace,
		FString* OutError)
	{
		const FGameXXKPendingCardChoice& Pending = InOutState.CardRun.ActiveBattle.Deck.PendingChoice;
		const FGameXXKCardBattleRuntime Before = InOutState.CardRun.ActiveBattle;
		if (Pending.Kind == EGameXXKCardPendingChoiceKind::ForcedDiscard)
		{
			TArray<FGameXXKCardInstance> Hand = Before.Deck.Hand;
			Hand.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
			{
				return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
					? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
					: Left.InstanceId.LexicalLess(Right.InstanceId);
			});
			TArray<FName> Discarded;
			for (int32 Index = 0; Index < Pending.RequiredDiscardCount && Hand.IsValidIndex(Index); ++Index)
			{
				Discarded.Add(Hand[Index].InstanceId);
			}
			if (Discarded.Num() != Pending.RequiredDiscardCount
				|| !FGameXXKCardBattleAdapter::SubmitForcedDiscard(InOutState, Discarded, OutError))
			{
				return false;
			}
			RecordAction(Before, InOutState.CardRun.ActiveBattle, TEXT("ForcedDiscard"), NAME_None, NAME_None, NAME_None, {}, InOutMetrics, OutTrace);
			return true;
		}
		if (Pending.Kind == EGameXXKCardPendingChoiceKind::InsightChooseToHand)
		{
			if (Pending.InsightTopOrder.IsEmpty())
			{
				if (OutError)
				{
					*OutError = TEXT("Insight choice had no stable top-order candidates.");
				}
				return false;
			}
			const FName PickedId = Pending.InsightTopOrder[0];
			TArray<FName> Remaining = Pending.InsightTopOrder;
			Remaining.RemoveAt(0);
			if (!FGameXXKCardBattleAdapter::SubmitInsightChoice(InOutState, PickedId, Remaining, OutError))
			{
				return false;
			}
			RecordAction(Before, InOutState.CardRun.ActiveBattle, TEXT("InsightChoice"), NAME_None, PickedId, NAME_None, {}, InOutMetrics, OutTrace);
			return true;
		}
		if (OutError)
		{
			*OutError = TEXT("Simulation encountered an unsupported pending card choice.");
		}
		return false;
	}
}

bool FGameXXKCombatSimulationRules::RunScenario(
	const FGameXXKSimulationScenario& Scenario,
	FGameXXKSimulationMetrics& OutMetrics,
	TArray<FGameXXKSimulationTraceEntry>& OutTrace,
	FString* OutError)
{
	OutMetrics = FGameXXKSimulationMetrics();
	OutTrace.Reset();
	if (OutError)
	{
		OutError->Reset();
	}
	if (Scenario.Policy != EGameXXKSimulationPolicy::Skilled
		|| Scenario.MaxRounds <= 0
		|| Scenario.MaxDecisions <= 0
		|| Scenario.Terrain == EGameXXKCardTerrain::Invalid)
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.InvalidScenario"), TEXT("Simulation scenario has invalid policy or bounds."));
	}

	FGameXXKRuntimeState State = Scenario.InitialRuntimeState;
	FString AdapterError;
	if (!FGameXXKCardBattleAdapter::BeginCardBattle(
		State,
		Scenario.NodeKind,
		Scenario.Terrain,
		Scenario.Seed,
		&AdapterError))
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.BeginCardBattleFailed"), AdapterError);
	}
	if (!ValidateRuntimeBounds(State, &AdapterError))
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.InvalidInitialRuntime"), AdapterError);
	}

	int32 DecisionCount = 0;
	TSet<FName> FirstRoundDefeats;
	while (!FGameXXKCardBattleAdapter::IsCardBattleTerminal(State))
	{
		const FGameXXKCardBattleRuntime& CurrentRuntime = State.CardRun.ActiveBattle;
		if (CurrentRuntime.RoundNumber > Scenario.MaxRounds)
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.MaxRounds"), TEXT("Simulation reached MaxRounds before a terminal result."));
		}
		if (DecisionCount >= Scenario.MaxDecisions)
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.MaxDecisions"), TEXT("Simulation reached MaxDecisions before a terminal result."));
		}

		if (CurrentRuntime.Phase == EGameXXKCardBattlePhase::Player)
		{
			if (GameXXKCardRules::HasPendingChoice(CurrentRuntime.Deck))
			{
				const FGameXXKCardBattleRuntime Before = CurrentRuntime;
				if (!ResolvePendingChoice(State, OutMetrics, OutTrace, &AdapterError))
				{
					return SetFailure(OutMetrics, OutError, TEXT("Simulation.PendingChoiceFailed"), AdapterError);
				}
				RecordFirstRoundDefeats(Before, State.CardRun.ActiveBattle, FirstRoundDefeats, OutMetrics);
				++DecisionCount;
			}
			else
			{
				FGameXXKSimulationDecision Decision;
				if (!ChooseSkilledDecision(State, Decision, &AdapterError))
				{
					return SetFailure(OutMetrics, OutError, TEXT("Simulation.PolicyFailed"), AdapterError);
				}
				const FGameXXKCardBattleRuntime Before = CurrentRuntime;
				if (Decision.bEndPlayerPhase)
				{
					TArray<FGameXXKCardDamageResult> DamageResults;
					if (!FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, DamageResults, &AdapterError))
					{
						return SetFailure(OutMetrics, OutError, TEXT("Simulation.EndPlayerPhaseFailed"), AdapterError);
					}
					RecordAction(Before, State.CardRun.ActiveBattle, TEXT("EndPlayerPhase"), NAME_None, NAME_None, NAME_None, DamageResults, OutMetrics, OutTrace);
				}
				else
				{
					FGameXXKCardPlayResult PlayResult;
					if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
						State,
						Decision.CardInstanceId,
						Decision.TargetUnitId,
						PlayResult,
						&AdapterError))
					{
						return SetFailure(OutMetrics, OutError, TEXT("Simulation.ResolveCardFailed"), AdapterError);
					}
					RecordAction(
						Before,
						State.CardRun.ActiveBattle,
						TEXT("PlayCard"),
						PlayResult.OwnerUnitId,
						PlayResult.CardId,
						Decision.TargetUnitId,
						PlayResult.DamageResults,
						OutMetrics,
						OutTrace);
				}
				RecordFirstRoundDefeats(Before, State.CardRun.ActiveBattle, FirstRoundDefeats, OutMetrics);
				++DecisionCount;
			}
		}
		else if (CurrentRuntime.Phase == EGameXXKCardBattlePhase::Enemy)
		{
			const FGameXXKCardBattleRuntime Before = CurrentRuntime;
			FGameXXKCardEnemyIntent Intent;
			TArray<FGameXXKCardDamageResult> DamageResults;
			bool bIntentsFinished = false;
			if (!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, Intent, DamageResults, bIntentsFinished, &AdapterError))
			{
				return SetFailure(OutMetrics, OutError, TEXT("Simulation.ResolveEnemyIntentFailed"), AdapterError);
			}
			RecordAction(Before, State.CardRun.ActiveBattle, TEXT("EnemyIntent"), Intent.SourceUnitId, Intent.CardId, Intent.SuggestedTargetUnitId, DamageResults, OutMetrics, OutTrace);
			RecordFirstRoundDefeats(Before, State.CardRun.ActiveBattle, FirstRoundDefeats, OutMetrics);
			++DecisionCount;
			if (!FGameXXKCardBattleAdapter::IsCardBattleTerminal(State) && bIntentsFinished)
			{
				const FGameXXKCardBattleRuntime BeforeCompletion = State.CardRun.ActiveBattle;
				TArray<FGameXXKCardDamageResult> CompletionDamageResults;
				if (!FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, CompletionDamageResults, &AdapterError))
				{
					return SetFailure(OutMetrics, OutError, TEXT("Simulation.CompleteEnemyPhaseFailed"), AdapterError);
				}
				RecordAction(BeforeCompletion, State.CardRun.ActiveBattle, TEXT("CompleteEnemyPhase"), NAME_None, NAME_None, NAME_None, CompletionDamageResults, OutMetrics, OutTrace);
				RecordFirstRoundDefeats(BeforeCompletion, State.CardRun.ActiveBattle, FirstRoundDefeats, OutMetrics);
				++DecisionCount;
			}
		}
		else
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.InvalidPhase"), TEXT("Simulation reached a non-terminal invalid card battle phase."));
		}

		if (!ValidateRuntimeBounds(State, &AdapterError))
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.RuntimeInvalid"), AdapterError);
		}
	}

	const FGameXXKCardBattleRuntime& TerminalRuntime = State.CardRun.ActiveBattle;
	OutMetrics.bVictory = TerminalRuntime.Phase == EGameXXKCardBattlePhase::Victory;
	OutMetrics.Rounds = TerminalRuntime.RoundNumber;
	OutMetrics.RemainingPartyHealth = SumPartyHealth(TerminalRuntime);
	if (!OutMetrics.bVictory)
	{
		OutMetrics.FailureReason = TEXT("Simulation.Defeat");
	}
	return true;
}
