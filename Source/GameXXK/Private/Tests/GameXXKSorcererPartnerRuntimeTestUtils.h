#pragma once

#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerRuntimeTestUtils
{
	inline const FName SorcererId(TEXT("Partner.Sorcerer"));
	inline const FName AllyId(TEXT("Partner.Ally"));
	inline const FName EnemyAId(TEXT("Enemy.A"));
	inline const FName EnemyBId(TEXT("Enemy.B"));

	inline FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 Attack = 20)
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

	inline FGameXXKCardInstance MakeCard(
		const FName CardId,
		const int32 Ordinal,
		const FName OwnerUnitId = SorcererId)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = FName(*FString::Printf(TEXT("Sorcerer.Runtime.%d"), Ordinal));
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = FName(*FString::Printf(TEXT("Sorcerer.Runtime.Source.%d"), Ordinal));
		Card.AcquisitionOrdinal = Ordinal;
		return Card;
	}

	inline bool BuildRuntime(
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
			Test.AddError(FString::Printf(TEXT("Sorcerer runtime fixture failed to initialize: %s"), *Error));
			return false;
		}
		OutRuntime.Deck.SharedEnergy = 20;
		return true;
	}

	inline bool InstallAllCardsInHand(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const TArray<FGameXXKCardInstance>& Cards)
	{
		Runtime.Deck.Hand = Cards;
		Runtime.Deck.DrawPile.Reset();
		Runtime.Deck.DiscardPile.Reset();
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.PendingAutomaticHandCards.Reset();
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer all-hand fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	inline bool ResolveActive(
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

	inline bool ResolveAutomaticSnapshot(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName CardId,
		const int32 Position,
		const EGameXXKSorcererCardFamily PreviousFamily,
		const EGameXXKSorcererTaskBranch Branch,
		FGameXXKCardPlayResult& OutResult,
		const int32 PaidMana = 0)
	{
		FGameXXKResolvedCardSnapshot Snapshot;
		Snapshot.CardId = CardId;
		Snapshot.Quality = EGameXXKCardQuality::Common;
		Snapshot.OwnerUnitId = SorcererId;
		Snapshot.PaidManaCost = PaidMana;
		Snapshot.SorcererSequencePosition = Position;
		Snapshot.PreviousSorcererFamily = PreviousFamily;
		Snapshot.SorcererTaskBranch = Branch;
		Runtime.AutomaticResolutionQueue.bActive = true;
		Runtime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
		Runtime.AutomaticResolutionQueue.PendingCards = {Snapshot};
		Runtime.AutomaticResolutionQueue.NextCardIndex = 0;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer automatic snapshot fixture is invalid: %s"), *Error));
			return false;
		}
		TArray<FGameXXKCardPlayResult> Results;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Sorcerer automatic snapshot failed: %s"), *Error));
			return false;
		}
		if (!Test.TestEqual(TEXT("one automatic Sorcerer snapshot produces one result"), Results.Num(), 1))
		{
			return false;
		}
		OutResult = MoveTemp(Results[0]);
		return true;
	}

	inline FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	inline EGameXXKSorcererTaskBranch BranchForFamily(const EGameXXKSorcererCardFamily Family)
	{
		switch (Family)
		{
		case EGameXXKSorcererCardFamily::Fire: return EGameXXKSorcererTaskBranch::Fire;
		case EGameXXKSorcererCardFamily::Ice: return EGameXXKSorcererTaskBranch::Ice;
		case EGameXXKSorcererCardFamily::Lightning: return EGameXXKSorcererTaskBranch::Lightning;
		case EGameXXKSorcererCardFamily::Core:
		case EGameXXKSorcererCardFamily::Universal:
			return EGameXXKSorcererTaskBranch::Normal;
		case EGameXXKSorcererCardFamily::None:
		default:
			return EGameXXKSorcererTaskBranch::None;
		}
	}

	inline bool BuildCompletedRewardRuntime(
		FAutomationTestBase& Test,
		const FName StarterCardId,
		const EGameXXKSorcererTaskBranch RequestedBranch,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		const FGameXXKCardDefinition* StarterDefinition = FGameXXKCardCatalog::FindCardDefinition(StarterCardId);
		if (!StarterDefinition
			|| StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::None
			|| StarterDefinition->SorcererRule.RewardRule == EGameXXKSorcererRewardRule::None)
		{
			Test.AddError(TEXT("Completed Sorcerer reward fixture requires a catalog starter rule."));
			return false;
		}

		TArray<FName> CardIds = {StarterCardId};
		const auto AddUnique = [&CardIds](const FName CardId)
		{
			if (!CardIds.Contains(CardId) && CardIds.Num() < 5)
			{
				CardIds.Add(CardId);
			}
		};
		if (StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal)
		{
			switch (RequestedBranch)
			{
			case EGameXXKSorcererTaskBranch::Fire: AddUnique(TEXT("Profession.Sorcerer.LiHuoYin")); break;
			case EGameXXKSorcererTaskBranch::Ice: AddUnique(TEXT("Profession.Sorcerer.FenMaiFu")); break;
			case EGameXXKSorcererTaskBranch::Lightning: AddUnique(TEXT("Profession.Sorcerer.ChiXiaoFenXing")); break;
			case EGameXXKSorcererTaskBranch::Normal: AddUnique(TEXT("Profession.Sorcerer.JuLing")); break;
			case EGameXXKSorcererTaskBranch::None:
			default:
				Test.AddError(TEXT("A Universal reward fixture requires one concrete branch."));
				return false;
			}
		}
		for (const FName Candidate : {
			FName(TEXT("Profession.Sorcerer.JuLing")),
			FName(TEXT("Profession.Sorcerer.LiHuoYin")),
			FName(TEXT("Profession.Sorcerer.FenMaiFu")),
			FName(TEXT("Profession.Sorcerer.ChiXiaoFenXing")),
			FName(TEXT("Profession.Sorcerer.YanQiang")),
			FName(TEXT("Profession.Sorcerer.SheLingHuo")),
			FName(TEXT("Profession.Sorcerer.NingYanChengRen"))})
		{
			AddUnique(Candidate);
		}
		if (CardIds.Num() != 5)
		{
			Test.AddError(TEXT("Completed Sorcerer reward fixture could not build five unique cards."));
			return false;
		}

		TArray<FGameXXKCardInstance> AllInstances;
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			AllInstances.Add(MakeCard(CardIds[Index], Index));
		}
		for (int32 Index = 0; Index < 12; ++Index)
		{
			AllInstances.Add(MakeCard(TEXT("Hero.Generic.QingFengYiShi"), 100 + Index, AllyId));
		}
		if (!BuildRuntime(
			Test,
			AllInstances,
			{
				MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
				MakeUnit(AllyId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
				MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
				MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)
			},
			Seed,
			OutRuntime))
		{
			return false;
		}

		OutRuntime.Deck.Hand.Reset();
		OutRuntime.Deck.DrawPile.Reset();
		OutRuntime.Deck.DiscardPile.Reset();
		OutRuntime.Deck.ExhaustPile.Reset();
		OutRuntime.Deck.PendingAutomaticHandCards.Reset();
		for (int32 Index = 0; Index < AllInstances.Num(); ++Index)
		{
			if (Index < 5)
			{
				OutRuntime.Deck.DiscardPile.Add(AllInstances[Index]);
			}
			else
			{
				OutRuntime.Deck.DrawPile.Add(AllInstances[Index]);
			}
		}

		FGameXXKSorcererPartnerTaskRuntime& Task = OutRuntime.SorcererPartnerTasks.AddDefaulted_GetRef();
		Task.bActive = true;
		Task.OwnerUnitId = SorcererId;
		Task.LockedCardIds = CardIds;
		Task.CompletedCardIds = CardIds;
		Task.StarterReward = StarterDefinition->SorcererRule.RewardRule;
		Task.LockedBranch = StarterDefinition->SorcererRule.Family == EGameXXKSorcererCardFamily::Universal
			? RequestedBranch
			: BranchForFamily(StarterDefinition->SorcererRule.Family);
		for (int32 Index = 0; Index < CardIds.Num(); ++Index)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardIds[Index]);
			FGameXXKResolvedCardSnapshot& Snapshot = Task.FirstPlayOrder.AddDefaulted_GetRef();
			Snapshot.CardId = CardIds[Index];
			Snapshot.Quality = EGameXXKCardQuality::Common;
			Snapshot.OwnerUnitId = SorcererId;
			Snapshot.SorcererSequencePosition = Index + 1;
			Snapshot.PreviousSorcererFamily = Index == 0
				? EGameXXKSorcererCardFamily::None
				: FGameXXKCardCatalog::FindCardDefinition(CardIds[Index - 1])->SorcererRule.Family;
			Snapshot.SorcererTaskBranch = Task.LockedBranch;
			if (!Definition)
			{
				Test.AddError(TEXT("Completed Sorcerer reward fixture lost one card definition."));
				return false;
			}
		}

		OutRuntime.AutomaticResolutionQueue.bActive = true;
		OutRuntime.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::PartnerSorcererTaskReplay;
		OutRuntime.AutomaticResolutionQueue.PendingCards = Task.FirstPlayOrder;
		OutRuntime.AutomaticResolutionQueue.NextCardIndex = Task.FirstPlayOrder.Num();
		OutRuntime.AutomaticResolutionQueue.PendingSorcererReward = Task.StarterReward;
		OutRuntime.AutomaticResolutionQueue.RewardOwnerUnitId = SorcererId;
		FString Error;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutRuntime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Completed Sorcerer reward fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	inline bool ResolveCompletedReward(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		FGameXXKCardPlayResult& OutRewardResult)
	{
		TArray<FGameXXKCardPlayResult> Results;
		FString Error;
		if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(Runtime, Results, &Error))
		{
			Test.AddError(FString::Printf(TEXT("Completed Sorcerer reward failed: %s"), *Error));
			return false;
		}
		if (!Test.TestEqual(TEXT("one completed task emits one aggregated reward result"), Results.Num(), 1))
		{
			return false;
		}
		OutRewardResult = MoveTemp(Results[0]);
		return true;
	}
}

#endif
