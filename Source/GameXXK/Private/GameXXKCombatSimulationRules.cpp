#include "GameXXKCombatSimulationRules.h"
#include "GameXXKTrainingSettlementRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionRules.h"

namespace
{
	struct FGameXXKScoredSimulationDecision
	{
		FGameXXKSimulationDecision Decision;
		int64 Score = MIN_int64;
		int32 AcquisitionOrdinal = MAX_int32;
	};

	/**
	 * One deterministic policy shared by every simulated profession. These are evaluator weights,
	 * not combat numbers: keeping them together makes benchmark changes reviewable and prevents
	 * individual CardId branches from teaching the bot one hard-coded combo at a time.
	 */
	struct FGameXXKSimulationPolicyWeights
	{
		int64 EnemyHealthDamage = 100;
		int64 EnemyDefeat = 25000;
		int64 PartyHealing = 45;
		int64 PartyHealthLoss = 120;
		int64 PartyDefeat = 50000;
		int64 PartyArmor = 20;
		int64 PartyArmorInDanger = 90;
		int64 SharedEnergy = 400;
		int64 PartyMana = 40;
		int64 NewHandCard = 350;
		int64 TerrainChange = 450;
		int64 FormulaOpened = 2200;
		int64 ReactionTrigger = 500;
		int64 ModifierTrigger = 180;
		int64 GuardLink = 150;
		int64 TaskActivation = 300;
		int64 TaskCardProgress = 180;
		int64 AutomaticQueueEntry = 180;
		int64 PendingDraw = 250;
		int64 RetainedArmorWindow = 250;
		int64 PendingRoundEnergy = 350;
		int64 RevealedIntent = 80;
		int64 Victory = 1000000;
	};

	const FGameXXKSimulationPolicyWeights SimulationPolicy;

	/**
	 * Progress guards for the greedy Skilled policy. A human player can always end the turn, but
	 * the evaluator only stops when nothing scores positively, so zero-cost mana/draw pairs (for
	 * example a mage's mana-gain and draw-one cards) can cycle forever inside one player phase.
	 * After this many consecutive decisions with no living enemy losing health, the simulation
	 * ends the phase exactly like a player would. Separately, when neither side's unit health
	 * changes for this many consecutive round boundaries, the battle resolves as a stalemate
	 * defeat instead of burning the round budget.
	 */
	constexpr int32 MaxDecisionsWithoutEnemyProgress = 64;
	constexpr int32 MaxStagnantRoundBoundaries = 5;

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

	static bool AnyLivingEnemyLostHealth(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After)
	{
		for (const FGameXXKCardCombatUnit& BeforeUnit : Before.Units)
		{
			if (BeforeUnit.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			const FGameXXKCardCombatUnit* AfterUnit = FindUnit(After.Units, BeforeUnit.UnitId);
			if (AfterUnit && AfterUnit->HP < BeforeUnit.HP)
			{
				return true;
			}
		}
		return false;
	}

	static TArray<TPair<FName, int32>> SnapshotUnitHealth(const FGameXXKCardBattleRuntime& Runtime)
	{
		TArray<TPair<FName, int32>> Snapshot;
		Snapshot.Reserve(Runtime.Units.Num());
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Snapshot.Add(MakeTuple(Unit.UnitId, Unit.HP));
		}
		Snapshot.Sort([](const TPair<FName, int32>& Left, const TPair<FName, int32>& Right)
		{
			return Left.Key.LexicalLess(Right.Key);
		});
		return Snapshot;
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

	static int32 AutomaticQueueDepth(const FGameXXKCardBattleRuntime& Runtime)
	{
		if (!Runtime.AutomaticResolutionQueue.bActive)
		{
			return 0;
		}
		return FMath::Max(
			0,
			Runtime.AutomaticResolutionQueue.PendingCards.Num()
				- Runtime.AutomaticResolutionQueue.NextCardIndex
				+ (Runtime.AutomaticResolutionQueue.PendingReward != EGameXXKHeroSpellTaskReward::None ? 1 : 0));
	}

	static void RecordRuntimeHighWaterMarks(
		const FGameXXKCardBattleRuntime& Runtime,
		FGameXXKSimulationMetrics& InOutMetrics,
		const int32 AdditionalQueueDepth = 0)
	{
		InOutMetrics.MaximumHandSize = FMath::Max(InOutMetrics.MaximumHandSize, Runtime.Deck.Hand.Num());
		InOutMetrics.MaximumAutomaticQueueDepth = FMath::Max(
			InOutMetrics.MaximumAutomaticQueueDepth,
			FMath::Max(AutomaticQueueDepth(Runtime), AdditionalQueueDepth));
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

	static int32 SumPartyMana(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Total += Unit.Mana;
			}
		}
		return Total;
	}

	static int32 CountNewHandCards(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After)
	{
		TSet<FName> ExistingInstanceIds;
		for (const FGameXXKCardInstance& Card : Before.Deck.Hand)
		{
			ExistingInstanceIds.Add(Card.InstanceId);
		}

		int32 Count = 0;
		for (const FGameXXKCardInstance& Card : After.Deck.Hand)
		{
			Count += ExistingInstanceIds.Contains(Card.InstanceId) ? 0 : 1;
		}
		return Count;
	}

	static int32 SumRemainingReactionTriggers(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKReactionRuntime& Reaction : Runtime.Reactions)
		{
			Total += FMath::Max(0, Reaction.RemainingTriggers);
		}
		return Total;
	}

	static int32 SumRemainingModifierTriggers(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardBattleModifierRuntime& Modifier : Runtime.Modifiers)
		{
			Total += Modifier.Definition.bPersistent
				? 1
				: FMath::Max(0, Modifier.Definition.RemainingTriggers);
		}
		return Total;
	}

	static int32 CountActiveTasks(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Count = Runtime.HeroSpellTask.bActive ? 1 : 0;
		for (const FGameXXKSorcererPartnerTaskRuntime& Task : Runtime.SorcererPartnerTasks)
		{
			Count += Task.bActive ? 1 : 0;
		}
		for (const FGameXXKTaskNpcSpellTaskRuntime& Task : Runtime.TaskNpcSpellTasks)
		{
			Count += Task.bActive ? 1 : 0;
		}
		return Count;
	}

	static int32 CountCompletedTaskCards(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Count = Runtime.HeroSpellTask.CompletedHeroCardIds.Num();
		for (const FGameXXKSorcererPartnerTaskRuntime& Task : Runtime.SorcererPartnerTasks)
		{
			Count += Task.CompletedCardIds.Num();
		}
		for (const FGameXXKTaskNpcSpellTaskRuntime& Task : Runtime.TaskNpcSpellTasks)
		{
			Count += Task.CompletedCardIds.Num();
		}
		return Count;
	}

	static int32 StatusStackScore(
		const EGameXXKCardStatus Status,
		const EGameXXKCardTargetSide Side)
	{
		int32 Magnitude = 0;
		bool bHarmful = false;
		switch (Status)
		{
		case EGameXXKCardStatus::Momentum: Magnitude = 140; break;
		case EGameXXKCardStatus::Agility: Magnitude = 250; break;
		case EGameXXKCardStatus::Vulnerability: Magnitude = 140; bHarmful = true; break;
		case EGameXXKCardStatus::Bleed: Magnitude = 50; bHarmful = true; break;
		case EGameXXKCardStatus::Poison: Magnitude = 60; bHarmful = true; break;
		case EGameXXKCardStatus::Burn: Magnitude = 65; bHarmful = true; break;
		case EGameXXKCardStatus::Mark: Magnitude = 90; bHarmful = true; break;
		case EGameXXKCardStatus::Guard: Magnitude = 100; break;
		case EGameXXKCardStatus::DamageOverTime: Magnitude = 55; bHarmful = true; break;
		case EGameXXKCardStatus::CannotReceiveVulnerability: Magnitude = 160; break;
		case EGameXXKCardStatus::NextAttackBonus: Magnitude = 140; break;
		case EGameXXKCardStatus::NextAttackAppliesVulnerability: Magnitude = 120; break;
		case EGameXXKCardStatus::NextHealingBonus: Magnitude = 100; break;
		case EGameXXKCardStatus::TerrainBonusDouble: Magnitude = 140; break;
		case EGameXXKCardStatus::NextTerrainCardFree: Magnitude = 180; break;
		case EGameXXKCardStatus::NextTerrainCardEnergyReduction: Magnitude = 160; break;
		case EGameXXKCardStatus::RedirectSingleTargetEnemyAttack: Magnitude = 160; break;
		case EGameXXKCardStatus::TerrainBonusDoubleThisRound: Magnitude = 140; break;
		case EGameXXKCardStatus::Medicine: Magnitude = 90; break;
		case EGameXXKCardStatus::Weak: Magnitude = 120; bHarmful = true; break;
		case EGameXXKCardStatus::Charge: Magnitude = 180; break;
		case EGameXXKCardStatus::Counter: Magnitude = 260; break;
		case EGameXXKCardStatus::Block: Magnitude = 300; break;
		default: return 0;
		}

		const bool bParty = Side == EGameXXKCardTargetSide::Party;
		return (bParty != bHarmful) ? Magnitude : -Magnitude;
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

	static void RecordCurrentHandAsSeen(
		const FGameXXKCardBattleRuntime& Runtime,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		for (const FGameXXKCardInstance& Card : Runtime.Deck.Hand)
		{
			AddMetric(InOutMetrics.CardsSeenById, Card.CardId, 1);
		}
	}

	static void RecordNewHandCardsAsSeen(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		TSet<FName> ExistingInstanceIds;
		for (const FGameXXKCardInstance& Card : Before.Deck.Hand)
		{
			ExistingInstanceIds.Add(Card.InstanceId);
		}
		for (const FGameXXKCardInstance& Card : After.Deck.Hand)
		{
			if (!ExistingInstanceIds.Contains(Card.InstanceId))
			{
				AddMetric(InOutMetrics.CardsSeenById, Card.CardId, 1);
			}
		}
	}

	static int32 SumLivingPartyMana(const FGameXXKCardBattleRuntime& Runtime)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party && Unit.bLiving)
			{
				Total += FMath::Max(0, Unit.Mana);
			}
		}
		return Total;
	}

	static int32 CountPositivePartyArmorDelta(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& BeforeUnit : Before.Units)
		{
			if (BeforeUnit.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			if (const FGameXXKCardCombatUnit* AfterUnit = FindUnit(After.Units, BeforeUnit.UnitId))
			{
				Total += FMath::Max(0, AfterUnit->Armor - BeforeUnit.Armor);
			}
		}
		return Total;
	}

	static void RecordActiveCardMetrics(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After,
		const FGameXXKCardPlayResult& PlayResult,
		FGameXXKSimulationMetrics& InOutMetrics)
	{
		AddMetric(InOutMetrics.CardsPlayedById, PlayResult.CardId, 1);
		for (const FGameXXKCardDamageResult& Damage : PlayResult.DamageResults)
		{
			const FGameXXKCardCombatUnit* Target = FindUnit(Before.Units, Damage.ResolvedTargetUnitId);
			if (!Target)
			{
				Target = FindUnit(After.Units, Damage.ResolvedTargetUnitId);
			}
			if (!Target || Target->Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			AddMetric(InOutMetrics.DamageByCardId, PlayResult.CardId, FMath::Max(0, Damage.HealthDamage));
			if (!Damage.bAvoidedByAgility)
			{
				const int32 DamagePastArmor = FMath::Max(
					0,
					Damage.DamageAfterLevelDifference - Damage.ArmorAbsorbed);
				InOutMetrics.OverkillDamage += FMath::Max(0, DamagePastArmor - Damage.HealthDamage);
			}
		}

		for (const FGameXXKCardHealingResult& Healing : PlayResult.HealingResults)
		{
			AddMetric(InOutMetrics.HealingByCardId, PlayResult.CardId, FMath::Max(0, Healing.EffectiveHealing));
			InOutMetrics.Overhealing += FMath::Max(0, Healing.RequestedHealing - Healing.EffectiveHealing);
		}

		for(const auto& Armor:PlayResult.ArmorResults)
		{const auto* Target=FindUnit(After.Units,Armor.TargetUnitId);if(Target&&Target->Side==EGameXXKCardTargetSide::Party)AddMetric(InOutMetrics.ArmorByCardId,PlayResult.CardId,Armor.EffectiveArmor);}
	}

	static bool InitializeScenarioIdentity(
		const FGameXXKRuntimeState& State,
		const EGameXXKCardTerrain Terrain,
		FGameXXKSimulationMetrics& InOutMetrics,
		FString* OutError)
	{
		InOutMetrics.Terrain = Terrain;
		const FGameXXKPermanentCompanion* Companion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.bIsActive;
			});
		if (!Companion)
		{
			return true;
		}

		TArray<FName> RebuiltBirthCards;
		TArray<FName> RebuiltFullCards;
		FName PrimaryArchetypeId;
		FString RebuildError;
		if (!FGameXXKCompanionRules::BuildFullProfessionCardPool(
			Companion->Role,
			Companion->CardSeed,
			RebuiltFullCards,
			&RebuildError)
			|| RebuiltFullCards != Companion->PersonalCardIds
			|| !FGameXXKCompanionRules::BuildPersonalCardPool(
			Companion->Role,
			Companion->CardSeed,
			RebuiltBirthCards,
			&RebuildError,
			&PrimaryArchetypeId))
		{
			if (OutError)
			{
				*OutError = RebuildError.IsEmpty()
					? TEXT("The active companion full card-pool identity does not match Role + CardSeed.")
					: RebuildError;
			}
			return false;
		}

		InOutMetrics.CompanionTemplateId = Companion->RecruitTemplateId;
		InOutMetrics.CompanionRole = Companion->Role;
		InOutMetrics.CompanionPrimaryArchetypeId = PrimaryArchetypeId;
		InOutMetrics.CompanionBirthCardIds = MoveTemp(RebuiltBirthCards);
		InOutMetrics.CompanionSelectedCardIds = Companion->SelectedCardIds;
		return true;
	}

	static FName DamageOriginMetricKey(const FGameXXKCardDamageResult& Damage)
	{
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::Reaction)
		{
			return TEXT("Reaction");
		}
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::TaskReward)
		{
			return TEXT("TaskReward");
		}
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::TerrainListener)
		{
			return TEXT("TerrainListener");
		}
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::HeavyArrow)
		{
			return TEXT("HeavyArrow");
		}
		if (Damage.Cause == EGameXXKCardDamageCause::Bleed
			|| Damage.Cause == EGameXXKCardDamageCause::Poison
			|| Damage.Cause == EGameXXKCardDamageCause::Burn
			|| Damage.Cause == EGameXXKCardDamageCause::Rot
			|| Damage.Cause == EGameXXKCardDamageCause::ToxicExplosionBleed
			|| Damage.Cause == EGameXXKCardDamageCause::ToxicExplosionPoison
			|| Damage.Cause == EGameXXKCardDamageCause::ToxicExplosionBurn)
		{
			return TEXT("DamageOverTime");
		}
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::MageTaskReplay)
		{
			return TEXT("MageReplay");
		}
		if (Damage.ResolutionOrigin == EGameXXKCardResolutionOrigin::AutomaticReplay)
		{
			return TEXT("AutomaticReplay");
		}
		return TEXT("Direct");
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

	static void RecordDamageMetrics(const FGameXXKCardBattleRuntime& Before,const FGameXXKCardBattleRuntime& After,
		const TArray<FGameXXKCardDamageResult>& DamageResults,FGameXXKSimulationMetrics& M)
	{
		for(const auto& D:DamageResults)
		{
			const auto* Target=FindUnit(After.Units,D.ResolvedTargetUnitId);if(!Target)Target=FindUnit(Before.Units,D.ResolvedTargetUnitId);
			if(!Target||Target->Side!=EGameXXKCardTargetSide::Enemy)continue;
			AddMetric(M.DamageBySource,D.SourceUnitId,D.HealthDamage);AddMetric(M.DamageByOrigin,DamageOriginMetricKey(D),D.HealthDamage);
			M.DamageLedgerDifference-=D.HealthDamage;
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
		TArray<FGameXXKSimulationTraceEntry>& OutTrace,
		const int32 ExplicitEnergySpent = 0,
		const int32 ExplicitManaSpent = 0,
		const int32 AutomaticResolutionCount = 0,
		const int32 AdditionalQueueDepth = 0,
		const FGameXXKCardPlayResult* ActivePlayResult = nullptr)
	{
		const int32 PartyHealthDelta = SumPartyHealth(After) - SumPartyHealth(Before);
		const int32 TotalHealthDelta = SumAllHealth(After) - SumAllHealth(Before);
		const int32 ManaDelta = SumMana(After) - SumMana(Before);
		const int32 ArmorDelta = SumArmor(After) - SumArmor(Before);
		const int32 EnergyDelta = After.Deck.SharedEnergy - Before.Deck.SharedEnergy;
		const auto LedgerBefore=FGameXXKTrainingSettlementRules::CaptureBattleStats(Before);
		const auto LedgerAfter=FGameXXKTrainingSettlementRules::CaptureBattleStats(After);
		const int64 Dealt=LedgerAfter.PartyDamageDealt-LedgerBefore.PartyDamageDealt;
		const int64 Taken=LedgerAfter.PartyDamageTaken-LedgerBefore.PartyDamageTaken;
		const int64 Healing=LedgerAfter.HealingDone-LedgerBefore.HealingDone;
		const int64 GeneratedArmor=LedgerAfter.ArmorGenerated-LedgerBefore.ArmorGenerated;
		InOutMetrics.DamageDealt+=Dealt;InOutMetrics.DamageTaken+=Taken;InOutMetrics.DamageLedgerDifference+=Dealt;
		InOutMetrics.HealingGenerated+=Healing;InOutMetrics.ArmorGenerated+=GeneratedArmor;
		int64 AttributedHealing=0,AttributedArmor=0;
		if(ActivePlayResult)
		{
			for(const auto& H:ActivePlayResult->HealingResults)
			{const auto* T=FindUnit(After.Units,H.TargetUnitId);if(T&&T->Side==EGameXXKCardTargetSide::Party){AddMetric(InOutMetrics.HealingBySource,H.SourceUnitId,H.EffectiveHealing);AttributedHealing+=H.EffectiveHealing;}}
			for(const auto& A:ActivePlayResult->ArmorResults)
			{const auto* T=FindUnit(After.Units,A.TargetUnitId);if(T&&T->Side==EGameXXKCardTargetSide::Party){AddMetric(InOutMetrics.ArmorBySource,A.SourceUnitId,A.EffectiveArmor);AttributedArmor+=A.EffectiveArmor;}}
		}
		// Ledger-only effects keep an explicit unattributed bucket; never invent an owner.
		AddMetric(InOutMetrics.HealingBySource,TEXT("Unattributed"),FMath::Max<int64>(0,Healing-AttributedHealing));
		AddMetric(InOutMetrics.ArmorBySource,TEXT("Unattributed"),FMath::Max<int64>(0,GeneratedArmor-AttributedArmor));
		const int32 ResidualEnergyDelta = EnergyDelta + ExplicitEnergySpent;
		const int32 ResidualManaDelta = ManaDelta + ExplicitManaSpent;
		InOutMetrics.EnergySpent += ExplicitEnergySpent + FMath::Max(0, -ResidualEnergyDelta);
		InOutMetrics.EnergyGained += FMath::Max(0, ResidualEnergyDelta);
		InOutMetrics.ManaSpent += ExplicitManaSpent + FMath::Max(0, -ResidualManaDelta);
		InOutMetrics.ManaGained += FMath::Max(0, ResidualManaDelta);
		InOutMetrics.AutomaticResolutionCount += FMath::Max(0, AutomaticResolutionCount);
		RecordDamageMetrics(Before, After, DamageResults, InOutMetrics);
		RecordStatusDeltas(Before, After, InOutMetrics);
		RecordNewHandCardsAsSeen(Before, After, InOutMetrics);
		if (ActivePlayResult)
		{
			RecordActiveCardMetrics(Before, After, *ActivePlayResult, InOutMetrics);
		}
		RecordRuntimeHighWaterMarks(Before, InOutMetrics, AdditionalQueueDepth);
		RecordRuntimeHighWaterMarks(After, InOutMetrics, AdditionalQueueDepth);

		FGameXXKSimulationTraceEntry Trace;
		Trace.Round = Before.RoundNumber;
		Trace.EnergyBefore=Before.Deck.SharedEnergy;Trace.EnergyAfter=After.Deck.SharedEnergy;Trace.EnergyPaid=ExplicitEnergySpent;Trace.ManaPaid=ExplicitManaSpent;
		Trace.EffectiveDamage=Dealt;Trace.DamageTaken=Taken;Trace.EffectiveHealing=Healing;Trace.GeneratedArmor=GeneratedArmor;Trace.DamagePackets=DamageResults;
		if(ActivePlayResult){Trace.HealingPackets=ActivePlayResult->HealingResults;Trace.ArmorPackets=ActivePlayResult->ArmorResults;Trace.StatusChanges=ActivePlayResult->StatusChanges;}
		auto Capture=[](const FGameXXKCardBattleRuntime& R,TArray<FGameXXKSimulationUnitSnapshot>& Out)
		{for(const auto& U:R.Units){auto& V=Out.AddDefaulted_GetRef();V.UnitId=U.UnitId;V.Side=U.Side;V.HP=U.HP;V.Armor=U.Armor;V.Mana=U.Mana;V.Statuses=U.Statuses;}};
		Capture(Before,Trace.UnitsBefore);Capture(After,Trace.UnitsAfter);
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
			if (BeforeUnit.Side == EGameXXKCardTargetSide::Party && BeforeUnit.bLiving && AfterUnit && !AfterUnit->bLiving && !InOutCountedDefeats.Contains(BeforeUnit.UnitId))
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
		const FGameXXKCardBattleRuntime& After)
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
				Score += static_cast<int64>(BeforeUnit.HP - AfterUnit->HP)
					* SimulationPolicy.EnemyHealthDamage;
				if (BeforeUnit.bLiving && !AfterUnit->bLiving)
				{
					Score += SimulationPolicy.EnemyDefeat;
				}
			}
			else
			{
				const int32 HealthDelta = AfterUnit->HP - BeforeUnit.HP;
				Score += static_cast<int64>(FMath::Max(0, HealthDelta)) * SimulationPolicy.PartyHealing;
				Score -= static_cast<int64>(FMath::Max(0, -HealthDelta)) * SimulationPolicy.PartyHealthLoss;
				const bool bInDanger = BeforeUnit.MaxHP > 0 && BeforeUnit.HP * 4 <= BeforeUnit.MaxHP;
				Score += static_cast<int64>(AfterUnit->Armor - BeforeUnit.Armor)
					* (bInDanger ? SimulationPolicy.PartyArmorInDanger : SimulationPolicy.PartyArmor);
				if (BeforeUnit.bLiving && !AfterUnit->bLiving)
				{
					Score -= SimulationPolicy.PartyDefeat;
				}
			}

			for (int32 RawStatus = static_cast<int32>(EGameXXKCardStatus::Momentum);
				RawStatus <= static_cast<int32>(EGameXXKCardStatus::Block);
				++RawStatus)
			{
				const EGameXXKCardStatus Status = static_cast<EGameXXKCardStatus>(RawStatus);
				const int32 StackDelta = GameXXKCardRules::GetCombatStatusStacks(*AfterUnit, Status)
					- GameXXKCardRules::GetCombatStatusStacks(BeforeUnit, Status);
				Score += static_cast<int64>(StackDelta) * StatusStackScore(Status, BeforeUnit.Side);
			}
		}

		Score += static_cast<int64>(After.Deck.SharedEnergy - Before.Deck.SharedEnergy)
			* SimulationPolicy.SharedEnergy;
		Score += static_cast<int64>(SumPartyMana(After) - SumPartyMana(Before))
			* SimulationPolicy.PartyMana;
		Score += static_cast<int64>(CountNewHandCards(Before, After)) * SimulationPolicy.NewHandCard;
		Score += static_cast<int64>(After.HealerFormulas.Num() - Before.HealerFormulas.Num())
			* SimulationPolicy.FormulaOpened;
		Score += static_cast<int64>(SumRemainingReactionTriggers(After) - SumRemainingReactionTriggers(Before))
			* SimulationPolicy.ReactionTrigger;
		Score += static_cast<int64>(SumRemainingModifierTriggers(After) - SumRemainingModifierTriggers(Before))
			* SimulationPolicy.ModifierTrigger;
		Score += static_cast<int64>(After.GuardLinks.Num() - Before.GuardLinks.Num())
			* SimulationPolicy.GuardLink;
		Score += static_cast<int64>(CountActiveTasks(After) - CountActiveTasks(Before))
			* SimulationPolicy.TaskActivation;
		Score += static_cast<int64>(CountCompletedTaskCards(After) - CountCompletedTaskCards(Before))
			* SimulationPolicy.TaskCardProgress;
		Score += static_cast<int64>(AutomaticQueueDepth(After) - AutomaticQueueDepth(Before))
			* SimulationPolicy.AutomaticQueueEntry;
		Score += static_cast<int64>(After.PendingTriggeredDrawCount - Before.PendingTriggeredDrawCount)
			* SimulationPolicy.PendingDraw;
		Score += static_cast<int64>(After.RetainArmorAtNextPartyPhaseUnitIds.Num()
			- Before.RetainArmorAtNextPartyPhaseUnitIds.Num()) * SimulationPolicy.RetainedArmorWindow;
		Score += static_cast<int64>(After.PendingNextRoundEnergyBonus - Before.PendingNextRoundEnergyBonus)
			* SimulationPolicy.PendingRoundEnergy;
		Score += static_cast<int64>(After.RevealedEnemyIntentCount - Before.RevealedEnemyIntentCount)
			* SimulationPolicy.RevealedIntent;
		if (After.Terrain != Before.Terrain)
		{
			Score += SimulationPolicy.TerrainChange;
		}
		if (SumLivingEnemyHealth(After) == 0)
		{
			Score += SimulationPolicy.Victory;
		}
		return Score;
	}

	static int32 CountStrandedTargetFailures(const FGameXXKRuntimeState& State)
	{
		const FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
		int32 Count = 0;
		for (const FGameXXKCardInstance& Card : Runtime.Deck.Hand)
		{
			FGameXXKCardPlayPreview Preview;
			FString PreviewError;
			if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, Card.InstanceId, Preview, &PreviewError))
			{
				Count += PreviewError == TEXT("No legal target is available for this card.") ? 1 : 0;
				continue;
			}
			if (Preview.bCanPlay
				|| !Preview.TargetRequest.bRequiresManualSelection
				|| Preview.EffectiveEnergyCost > Runtime.Deck.SharedEnergy)
			{
				continue;
			}
			const FGameXXKCardCombatUnit* Owner = FindUnit(Runtime.Units, Preview.OwnerUnitId);
			if (!Owner || Preview.EffectiveManaCost > Owner->Mana)
			{
				continue;
			}
			const bool bHasLegalTarget = Preview.TargetRequest.CandidateViews.ContainsByPredicate([](const FGameXXKCardTargetCandidateView& Candidate)
			{
				return Candidate.bCanSelect;
			});
			Count += bHasLegalTarget ? 0 : 1;
		}
		return Count;
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
				const int64 Score = ScoreCandidateResolution(Runtime, CandidateState.CardRun.ActiveBattle);
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
		const int32 QueueDepthBefore = AutomaticQueueDepth(Before);
		const auto FlattenDamageResults = [](const TArray<FGameXXKCardPlayResult>& Results)
		{
			TArray<FGameXXKCardDamageResult> DamageResults;
			for (const FGameXXKCardPlayResult& Result : Results)
			{
				DamageResults.Append(Result.DamageResults);
			}
			return DamageResults;
		};
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
			TArray<FGameXXKCardPlayResult> ResumedResults;
			if (Discarded.Num() != Pending.RequiredDiscardCount
				|| !FGameXXKCardBattleAdapter::SubmitForcedDiscard(InOutState, Discarded, OutError, &ResumedResults))
			{
				return false;
			}
			RecordAction(
				Before,
				InOutState.CardRun.ActiveBattle,
				TEXT("ForcedDiscard"),
				NAME_None,
				NAME_None,
				NAME_None,
				FlattenDamageResults(ResumedResults),
				InOutMetrics,
				OutTrace,
				0,
				0,
				ResumedResults.Num(),
				QueueDepthBefore);
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
			TArray<FGameXXKCardPlayResult> ResumedResults;
			if (!FGameXXKCardBattleAdapter::SubmitInsightChoice(InOutState, PickedId, Remaining, OutError, &ResumedResults))
			{
				return false;
			}
			RecordAction(
				Before,
				InOutState.CardRun.ActiveBattle,
				TEXT("InsightChoice"),
				NAME_None,
				PickedId,
				NAME_None,
				FlattenDamageResults(ResumedResults),
				InOutMetrics,
				OutTrace,
				0,
				0,
				ResumedResults.Num(),
				QueueDepthBefore);
			return true;
		}
		if (Pending.Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand)
		{
			if (Pending.Candidates.IsEmpty())
			{
				if (OutError)
				{
					*OutError = TEXT("Hero task search had no stable candidates.");
				}
				return false;
			}
			TArray<FGameXXKCardInstance> Candidates = Pending.Candidates;
			Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
			{
				return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
					? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
					: Left.InstanceId.LexicalLess(Right.InstanceId);
			});
			const FName PickedId = Candidates[0].InstanceId;
			TArray<FGameXXKCardPlayResult> ResumedResults;
			if (!FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(InOutState, PickedId, ResumedResults, OutError))
			{
				return false;
			}
			RecordAction(
				Before,
				InOutState.CardRun.ActiveBattle,
				TEXT("HeroTaskSearchChoice"),
				NAME_None,
				PickedId,
				NAME_None,
				FlattenDamageResults(ResumedResults),
				InOutMetrics,
				OutTrace,
				0,
				0,
				ResumedResults.Num(),
				QueueDepthBefore);
			return true;
		}
		if (OutError)
		{
			*OutError = TEXT("Simulation encountered an unsupported pending card choice.");
		}
		return false;
	}
}

#if WITH_DEV_AUTOMATION_TESTS
bool FGameXXKCombatSimulationRules::ChooseSkilledDecisionForTest(
	const FGameXXKRuntimeState& State,
	FGameXXKSimulationDecision& OutDecision,
	FString* OutError)
{
	return ChooseSkilledDecision(State, OutDecision, OutError);
}
#endif

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
	if (!Scenario.bResumeActiveBattle && !FGameXXKCardBattleAdapter::BeginCardBattle(
		State,
		Scenario.NodeKind,
		Scenario.Terrain,
		Scenario.Seed,
		&AdapterError))
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.BeginCardBattleFailed"), AdapterError);
	}
	if (Scenario.bResumeActiveBattle && !State.CardRun.bHasActiveCardBattle)
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.NoActiveBattle"), TEXT("There is no active battle to continue."));
	if (!ValidateRuntimeBounds(State, &AdapterError))
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.InvalidInitialRuntime"), AdapterError);
	}
	if (!InitializeScenarioIdentity(State, Scenario.Terrain, OutMetrics, &AdapterError))
	{
		return SetFailure(OutMetrics, OutError, TEXT("Simulation.InvalidCompanionIdentity"), AdapterError);
	}
	RecordCurrentHandAsSeen(State.CardRun.ActiveBattle, OutMetrics);
	RecordRuntimeHighWaterMarks(State.CardRun.ActiveBattle, OutMetrics);

	int32 DecisionCount = 0;
	int32 DecisionsWithoutEnemyProgress = 0;
	int32 StagnantRoundBoundaries = 0;
	int32 LastBoundaryRound = State.CardRun.ActiveBattle.RoundNumber;
	TArray<TPair<FName, int32>> LastRoundHealthSnapshot = SnapshotUnitHealth(State.CardRun.ActiveBattle);
	TSet<FName> FirstRoundDefeats;
	while (!FGameXXKCardBattleAdapter::IsCardBattleTerminal(State))
	{
		const FGameXXKCardBattleRuntime& CurrentRuntime = State.CardRun.ActiveBattle;
		OutMetrics.Rounds=CurrentRuntime.RoundNumber;OutMetrics.RemainingPartyHealth=SumPartyHealth(CurrentRuntime);
		if (CurrentRuntime.RoundNumber > Scenario.MaxRounds)
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.MaxRounds"), TEXT("Simulation reached MaxRounds before a terminal result."));
		}
		if (DecisionCount >= Scenario.MaxDecisions)
		{
			return SetFailure(OutMetrics, OutError, TEXT("Simulation.MaxDecisions"), TEXT("Simulation reached MaxDecisions before a terminal result."));
		}

		if (CurrentRuntime.RoundNumber != LastBoundaryRound)
		{
			const TArray<TPair<FName, int32>> NewSnapshot = SnapshotUnitHealth(CurrentRuntime);
			StagnantRoundBoundaries = (NewSnapshot == LastRoundHealthSnapshot)
				? StagnantRoundBoundaries + 1
				: 0;
			LastRoundHealthSnapshot = NewSnapshot;
			LastBoundaryRound = CurrentRuntime.RoundNumber;
			DecisionsWithoutEnemyProgress = 0;
			if (StagnantRoundBoundaries >= MaxStagnantRoundBoundaries)
			{
				OutMetrics.bVictory = false;
				OutMetrics.bStalemateResolved = true;
				OutMetrics.Rounds = CurrentRuntime.RoundNumber;
				OutMetrics.RemainingPartyHealth = SumPartyHealth(CurrentRuntime);
				OutMetrics.FailureReason = TEXT("Simulation.Defeat");
				RecordAction(
					CurrentRuntime,
					CurrentRuntime,
					TEXT("StalemateDefeat"),
					NAME_None,
					NAME_None,
					NAME_None,
					TArray<FGameXXKCardDamageResult>(),
					OutMetrics,
					OutTrace);
				return true;
			}
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
				if (DecisionsWithoutEnemyProgress >= MaxDecisionsWithoutEnemyProgress)
				{
					// A human player would end the turn here; the greedy evaluator never would.
					Decision.bEndPlayerPhase = true;
				}
				else if (!ChooseSkilledDecision(State, Decision, &AdapterError))
				{
					return SetFailure(OutMetrics, OutError, TEXT("Simulation.PolicyFailed"), AdapterError);
				}
				const FGameXXKCardBattleRuntime Before = CurrentRuntime;
				if (Decision.bEndPlayerPhase)
				{
					OutMetrics.StrandedTargetFailures += CountStrandedTargetFailures(State);
					OutMetrics.EnergyUnspentAtPhaseEnd += FMath::Max(0, Before.Deck.SharedEnergy);
					OutMetrics.ManaUnspentAtPhaseEnd += SumLivingPartyMana(Before);
					TArray<FGameXXKCardDamageResult> DamageResults;
					if (!FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, DamageResults, &AdapterError))
					{
						return SetFailure(OutMetrics, OutError, TEXT("Simulation.EndPlayerPhaseFailed"), AdapterError);
					}
					RecordAction(Before, State.CardRun.ActiveBattle, TEXT("EndPlayerPhase"), NAME_None, NAME_None, NAME_None, DamageResults, OutMetrics, OutTrace);
				}
				else
				{
					EGameXXKCardZone DecisionZone = EGameXXKCardZone::Invalid;
					const FGameXXKCardInstance* DecisionInstance = GameXXKCardRules::FindInstance(
						CurrentRuntime.Deck,
						Decision.CardInstanceId,
						DecisionZone);
					FGameXXKCardPlayPreview CommittedPreview;
					if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(
						State,
						Decision.CardInstanceId,
						CommittedPreview,
						&AdapterError)
						|| !CommittedPreview.bCanPlay)
					{
						return SetFailure(OutMetrics, OutError, TEXT("Simulation.PreviewCommitMismatch"), AdapterError);
					}
					FGameXXKCardPlayResult PlayResult;
					if (!FGameXXKCardBattleAdapter::ResolveCardPlay(
						State,
						Decision.CardInstanceId,
						Decision.TargetUnitId,
						PlayResult,
						&AdapterError))
					{
						const FString DetailedError = FString::Printf(
							TEXT("card=%s instance=%s target=%s zone=%d: %s"),
							DecisionInstance ? *DecisionInstance->CardId.ToString() : TEXT("None"),
							*Decision.CardInstanceId.ToString(),
							*Decision.TargetUnitId.ToString(),
							static_cast<int32>(DecisionZone),
							*AdapterError);
						return SetFailure(OutMetrics, OutError, TEXT("Simulation.ResolveCardFailed"), DetailedError);
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
						OutTrace,
						CommittedPreview.EffectiveEnergyCost,
						CommittedPreview.EffectiveManaCost,
						PlayResult.AutomaticResolutionCount,
						PlayResult.MaximumAutomaticQueueDepth,
						&PlayResult);
					DecisionsWithoutEnemyProgress = AnyLivingEnemyLostHealth(Before, State.CardRun.ActiveBattle)
						? 0
						: DecisionsWithoutEnemyProgress + 1;
					++OutMetrics.ActivelyPlayedCards;
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
