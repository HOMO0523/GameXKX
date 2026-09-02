#pragma once

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKAllCardPlayabilityAuditTest
{
	inline constexpr int32 AuditFillerCount = 18;

	inline const TArray<EGameXXKCardTerrain>& EveryTerrain()
	{
		static const TArray<EGameXXKCardTerrain> Terrains = {
			EGameXXKCardTerrain::Plain,
			EGameXXKCardTerrain::Cliff,
			EGameXXKCardTerrain::Forest,
			EGameXXKCardTerrain::WaterShore,
			EGameXXKCardTerrain::Ferry,
			EGameXXKCardTerrain::Village,
			EGameXXKCardTerrain::Cave};
		return Terrains;
	}

	inline void AddStatus(FGameXXKCardCombatUnit& Unit, const EGameXXKCardStatus Status, const int32 Stacks)
	{
		GameXXKCardRules::AddCombatStatus(Unit, Status, Stacks);
	}

	inline void PrimeRepresentativeStatuses(FGameXXKCardCombatUnit& Unit)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Party)
		{
			AddStatus(Unit, EGameXXKCardStatus::Momentum, 3);
			AddStatus(Unit, EGameXXKCardStatus::Agility, 4);
			AddStatus(Unit, EGameXXKCardStatus::Bleed, 8);
			AddStatus(Unit, EGameXXKCardStatus::Poison, 6);
			AddStatus(Unit, EGameXXKCardStatus::Burn, 4);
			AddStatus(Unit, EGameXXKCardStatus::DamageOverTime, 2);
			AddStatus(Unit, EGameXXKCardStatus::Mark, 3);
			AddStatus(Unit, EGameXXKCardStatus::Medicine, 6);
			AddStatus(Unit, EGameXXKCardStatus::Weak, 1);
			AddStatus(Unit, EGameXXKCardStatus::Prey, 1);
			AddStatus(Unit, EGameXXKCardStatus::Charge, 3);
		}
		else
		{
			AddStatus(Unit, EGameXXKCardStatus::Vulnerability, 3);
			AddStatus(Unit, EGameXXKCardStatus::Bleed, 8);
			AddStatus(Unit, EGameXXKCardStatus::Poison, 6);
			AddStatus(Unit, EGameXXKCardStatus::Burn, 4);
			AddStatus(Unit, EGameXXKCardStatus::DamageOverTime, 2);
			AddStatus(Unit, EGameXXKCardStatus::Mark, 5);
			AddStatus(Unit, EGameXXKCardStatus::Weak, 1);
			AddStatus(Unit, EGameXXKCardStatus::Prey, 1);
		}
	}

	inline FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 HealthPercent)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.MaxHP = Side == EGameXXKCardTargetSide::Enemy ? 1000000 : 5000;
		Unit.HP = FMath::Max(1, Unit.MaxHP * HealthPercent / 100);
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 200 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = Side == EGameXXKCardTargetSide::Enemy ? 10 : 40 - StableSortOrder;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.Armor = Side == EGameXXKCardTargetSide::Party ? 20 : 0;
		Unit.StableSortOrder = StableSortOrder;
		PrimeRepresentativeStatuses(Unit);
		return Unit;
	}

	inline FGameXXKCardInstance MakeCard(
		const FName InstanceId,
		const FGameXXKCardDefinition& Definition,
		const FName OwnerUnitId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = InstanceId;
		Card.CardId = Definition.Id;
		Card.CurrentQuality = Definition.BaseQuality;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("AllCards.Audit.Source.%d"), AcquisitionOrdinal));
		Card.AcquisitionOrdinal = AcquisitionOrdinal;
		return Card;
	}

	inline int32 RequiredOwnerCohortSize(const EGameXXKCardOwner Owner)
	{
		switch (Owner)
		{
		case EGameXXKCardOwner::Hero: return 8;
		case EGameXXKCardOwner::Profession: return 5;
		case EGameXXKCardOwner::QuestNpc: return 3;
		case EGameXXKCardOwner::Route: return 1;
		default: return 1;
		}
	}

	inline bool BuildRuntime(
		FAutomationTestBase& Test,
		const FGameXXKCardDefinition& PlayedDefinition,
		const EGameXXKCardTerrain Terrain,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime,
		FName& OutPlayedInstanceId)
	{
		const FName OwnerUnitId = PlayedDefinition.OwnerId;
		if (OwnerUnitId.IsNone())
		{
			Test.AddError(FString::Printf(TEXT("%s has no stable owner identity"), *PlayedDefinition.Id.ToString()));
			return false;
		}

		TArray<FGameXXKCardInstance> Cards;
		OutPlayedInstanceId = FName(*FString::Printf(TEXT("AllCards.Played.%s"), *PlayedDefinition.Id.ToString()));
		Cards.Add(MakeCard(OutPlayedInstanceId, PlayedDefinition, OwnerUnitId, 0));
		TSet<FName> OwnerCardIds = {PlayedDefinition.Id};
		const int32 DesiredOwnerCards = RequiredOwnerCohortSize(PlayedDefinition.Owner);
		for (const FGameXXKCardDefinition& Candidate : FGameXXKCardCatalog::GetCardDefinitionsForOwner(OwnerUnitId))
		{
			if (Cards.Num() >= DesiredOwnerCards || OwnerCardIds.Contains(Candidate.Id))
			{
				continue;
			}
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("AllCards.Owner.%d"), Cards.Num())),
				Candidate,
				OwnerUnitId,
				Cards.Num()));
			OwnerCardIds.Add(Candidate.Id);
		}
		if (OwnerCardIds.Num() != DesiredOwnerCards)
		{
			Test.AddError(FString::Printf(
				TEXT("%s could only build %d/%d distinct owner cards"),
				*PlayedDefinition.Id.ToString(),
				OwnerCardIds.Num(),
				DesiredOwnerCards));
			return false;
		}

		const FGameXXKCardDefinition* FillerDefinition = FGameXXKCardCatalog::FindCardDefinition(TEXT("Hero.Generic.QingFengYiShi"));
		if (!FillerDefinition)
		{
			Test.AddError(TEXT("all-card audit filler definition is missing"));
			return false;
		}
		for (int32 Index = 0; Index < AuditFillerCount; ++Index)
		{
			Cards.Add(MakeCard(
				FName(*FString::Printf(TEXT("AllCards.Filler.%d"), Index)),
				*FillerDefinition,
				TEXT("Audit.Ally.A"),
				Cards.Num()));
		}

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(OwnerUnitId, EGameXXKCardTargetSide::Party, PlayedDefinition.Role, 0, 80),
			MakeUnit(TEXT("Audit.Ally.A"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1, 50),
			MakeUnit(TEXT("Audit.Ally.B"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 2, 1),
			MakeUnit(TEXT("Audit.Enemy.A"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 100),
			MakeUnit(TEXT("Audit.Enemy.B"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11, 100)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(OutRuntime, Cards, Units, Terrain, Seed, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s runtime initialization failed: %s"), *PlayedDefinition.Id.ToString(), *Error));
			return false;
		}

		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.PendingAutomaticHandCards.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			(Card.InstanceId == OutPlayedInstanceId ? OutRuntime.Deck.Hand : OutRuntime.Deck.DrawPile).Add(Card);
		}
		OutRuntime.Deck.SharedEnergy = 50;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s audit fixture is invalid: %s"), *PlayedDefinition.Id.ToString(), *Error));
			return false;
		}
		return true;
	}

	inline bool DrainChoicesAndAutomaticQueue(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FString& Context)
	{
		for (int32 Guard = 0; Guard < 128; ++Guard)
		{
			FString Error;
			TArray<FGameXXKCardPlayResult> ResumedResults;
			switch (Runtime.Deck.PendingChoice.Kind)
			{
			case EGameXXKCardPendingChoiceKind::ForcedDiscard:
			{
				TArray<FName> DiscardIds;
				for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
				{
					if (DiscardIds.Num() >= Runtime.Deck.PendingChoice.RequiredCount)
					{
						break;
					}
					if (Runtime.Deck.Hand.ContainsByPredicate([&Candidate](const FGameXXKCardInstance& Card)
					{
						return Card.InstanceId == Candidate.InstanceId;
					}))
					{
						DiscardIds.Add(Candidate.InstanceId);
					}
				}
				if (DiscardIds.Num() != Runtime.Deck.PendingChoice.RequiredCount
					|| !GameXXKCardRules::SubmitForcedDiscard(Runtime, DiscardIds, &Error, &ResumedResults))
				{
					Test.AddError(FString::Printf(TEXT("%s could not resolve forced discard: %s"), *Context, *Error));
					return false;
				}
				continue;
			}
			case EGameXXKCardPendingChoiceKind::InsightChooseToHand:
			{
				if (Runtime.Deck.PendingChoice.Candidates.IsEmpty())
				{
					Test.AddError(FString::Printf(TEXT("%s opened insight without candidates"), *Context));
					return false;
				}
				const FName PickedId = Runtime.Deck.PendingChoice.Candidates[0].InstanceId;
				TArray<FName> RemainingIds;
				for (const FName InstanceId : Runtime.Deck.PendingChoice.InsightTopOrder)
				{
					if (InstanceId != PickedId)
					{
						RemainingIds.Add(InstanceId);
					}
				}
				if (!GameXXKCardRules::SubmitInsightChoice(Runtime, PickedId, RemainingIds, &Error, &ResumedResults))
				{
					Test.AddError(FString::Printf(TEXT("%s could not resolve insight: %s"), *Context, *Error));
					return false;
				}
				continue;
			}
			case EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand:
			{
				if (Runtime.Deck.PendingChoice.Candidates.IsEmpty()
					|| !GameXXKCardRules::SubmitHeroTaskSearchChoice(
						Runtime,
						Runtime.Deck.PendingChoice.Candidates[0].InstanceId,
						ResumedResults,
						&Error))
				{
					Test.AddError(FString::Printf(TEXT("%s could not resolve Hero task search: %s"), *Context, *Error));
					return false;
				}
				continue;
			}
			case EGameXXKCardPendingChoiceKind::None:
				if (Runtime.AutomaticResolutionQueue.bActive)
				{
					if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, ResumedResults, &Error))
					{
						Test.AddError(FString::Printf(TEXT("%s could not resume its automatic queue: %s"), *Context, *Error));
						return false;
					}
					continue;
				}
				return true;
			case EGameXXKCardPendingChoiceKind::Invalid:
			default:
				Test.AddError(FString::Printf(TEXT("%s left an invalid pending-choice state"), *Context));
				return false;
			}
		}
		Test.AddError(FString::Printf(TEXT("%s exceeded the bounded choice/queue drain"), *Context));
		return false;
	}

	inline bool BuildRuntimeState(
		FAutomationTestBase& Test,
		const FGameXXKCardDefinition& Definition,
		const EGameXXKCardTerrain Terrain,
		const int32 Seed,
		FGameXXKRuntimeState& OutState,
		FName& OutPlayedInstanceId,
		FString& OutError)
	{
		OutState = FGameXXKRuntimeState();
		OutPlayedInstanceId = NAME_None;
		OutError.Reset();
		const FString Context = FString::Printf(
			TEXT("CardId=%s Terrain=%d"),
			*Definition.Id.ToString(),
			static_cast<int32>(Terrain));
		const auto Fail = [&OutState, &OutPlayedInstanceId, &OutError, &Context](const FString& Reason)
		{
			OutState = FGameXXKRuntimeState();
			OutPlayedInstanceId = NAME_None;
			OutError = FString::Printf(TEXT("%s %s"), *Context, *Reason);
			return false;
		};

		FGameXXKCardBattleRuntime Runtime;
		if (!BuildRuntime(Test, Definition, Terrain, Seed, Runtime, OutPlayedInstanceId))
		{
			return Fail(TEXT("base runtime build failed"));
		}

		static const FName EnemyDefinitionIds[] = {
			TEXT("Enemy.Ch1.Rooster"),
			TEXT("Enemy.Ch1.Goat")};
		int32 NextEnemySlotNumber = 1;
		for (FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				if (NextEnemySlotNumber > static_cast<int32>(UE_ARRAY_COUNT(EnemyDefinitionIds)))
				{
					return Fail(TEXT("base runtime contains more than two enemies"));
				}
				Unit.BattleSlotNumber = NextEnemySlotNumber;
				Unit.EnemyDefinitionId = EnemyDefinitionIds[NextEnemySlotNumber - 1];
				Unit.CombatLevel = 1;
				++NextEnemySlotNumber;
			}
		}
		if (NextEnemySlotNumber != 3)
		{
			return Fail(FString::Printf(
				TEXT("expected the base two-enemy topology but found %d enemies"),
				NextEnemySlotNumber - 1));
		}

		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.Screen = EGameXXKScreen::Battle;
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = 198;
		OutState.CardRun.bHasActiveCardBattle = true;
		OutState.CardRun.ActiveBattleSourceNodeId = 198;
		OutState.CardRun.ActiveBattle = MoveTemp(Runtime);
		OutState.ActiveBattleParty.Reset();
		OutState.ActiveBattleEnemies.Reset();

		for (const FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side != EGameXXKCardTargetSide::Party && Unit.Side != EGameXXKCardTargetSide::Enemy)
			{
				return Fail(FString::Printf(TEXT("unit %s has no stable legacy side"), *Unit.UnitId.ToString()));
			}

			FGameXXKBattleRuntimeUnit LegacyUnit;
			LegacyUnit.Id = Unit.UnitId;
			LegacyUnit.DisplayName = FText::FromName(Unit.UnitId);
			LegacyUnit.HP = Unit.HP;
			LegacyUnit.MaxHP = Unit.MaxHP;
			LegacyUnit.MP = Unit.Mana;
			LegacyUnit.MaxMP = Unit.MaxMana;
			LegacyUnit.Attack = Unit.Attack;
			LegacyUnit.Defense = Unit.Defense;
			LegacyUnit.Speed = Unit.Speed;
			LegacyUnit.Shield = FMath::Max(0, Unit.Armor);
			LegacyUnit.EnemyDefinitionId = Unit.EnemyDefinitionId;
			LegacyUnit.BattleSlotNumber = Unit.BattleSlotNumber;
			LegacyUnit.CombatLevel = Unit.CombatLevel;
			LegacyUnit.bEnemy = Unit.Side == EGameXXKCardTargetSide::Enemy;
			LegacyUnit.bDefeated = !Unit.bLiving;
			(LegacyUnit.bEnemy ? OutState.ActiveBattleEnemies : OutState.ActiveBattleParty).Add(MoveTemp(LegacyUnit));
		}

		FString ProjectionError;
		if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(OutState, &ProjectionError))
		{
			return Fail(FString::Printf(TEXT("legacy projection sync failed: %s"), *ProjectionError));
		}
		return true;
	}
}

#endif
