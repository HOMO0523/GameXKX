#include "GameXXKRelicRules.h"

#include "GameXXKCardRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKMVPRules.h"

namespace
{
	bool Fail(FString* OutError, const TCHAR* Message)
	{
		if (OutError)
		{
			*OutError = Message;
		}
		return false;
	}

	uint32 NextRandom(uint32& State)
	{
		if (State == 0) State = 0x9E3779B9U;
		State ^= State << 13;
		State ^= State >> 17;
		State ^= State << 5;
		return State;
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKRuntimeState& State, FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	void AddStatus(FGameXXKCardCombatUnit& Unit, EGameXXKCardStatus Status, int32 Stacks)
	{
		if (Stacks <= 0 || !Unit.bLiving) return;
		if (FGameXXKCardStatusStack* Existing = Unit.Statuses.FindByPredicate([Status](const FGameXXKCardStatusStack& Stack)
		{
			return Stack.Status == Status;
		}))
		{
			Existing->Stacks = FMath::Clamp(Existing->Stacks + Stacks, 0, 99);
		}
		else
		{
			FGameXXKCardStatusStack NewStack;
			NewStack.Status = Status;
			NewStack.Stacks = FMath::Clamp(Stacks, 0, 99);
			Unit.Statuses.Add(NewStack);
		}
	}

	int32 EffectiveMagnitude(const FGameXXKRelicDefinition& Definition, const FGameXXKRelicInstance& Instance)
	{
		return Definition.Magnitude * (Definition.bStackable ? FMath::Max(1, Instance.Stacks) : 1);
	}

	void ApplyCombatEffect(
		FGameXXKRuntimeState& State,
		const FGameXXKRelicDefinition& Definition,
		const FGameXXKRelicInstance& Instance,
		FName OwnerUnitId,
		const TArray<FGameXXKCardDamageResult>* DamageResults)
	{
		FGameXXKCardBattleRuntime& Battle = State.CardRun.ActiveBattle;
		const int32 Magnitude = EffectiveMagnitude(Definition, Instance);
		auto ForLivingParty = [&Battle](auto&& Callback)
		{
			for (FGameXXKCardCombatUnit& Unit : Battle.Units)
			{
				if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party) Callback(Unit);
			}
		};
		auto ForLivingEnemies = [&Battle](auto&& Callback)
		{
			for (FGameXXKCardCombatUnit& Unit : Battle.Units)
			{
				if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy) Callback(Unit);
			}
		};

		switch (Definition.EffectKind)
		{
		case EGameXXKRelicEffectKind::GainPartyArmor:
			ForLivingParty([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.Armor = FMath::Clamp(Unit.Armor + Magnitude, 0, 99); });
			break;
		case EGameXXKRelicEffectKind::GainHeroArmor:
			if (FGameXXKCardCombatUnit* Unit = FindUnit(State, OwnerUnitId.IsNone() ? FName(TEXT("Player")) : OwnerUnitId))
				Unit->Armor = FMath::Clamp(Unit->Armor + Magnitude, 0, 99);
			break;
		case EGameXXKRelicEffectKind::HealParty:
			ForLivingParty([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.HP = FMath::Min(Unit.MaxHP, Unit.HP + Magnitude); });
			break;
		case EGameXXKRelicEffectKind::RestorePartyMana:
			ForLivingParty([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.Mana = FMath::Min(Unit.MaxMana, Unit.Mana + Magnitude); });
			break;
		case EGameXXKRelicEffectKind::GainSharedEnergy:
			Battle.Deck.SharedEnergy = FMath::Max(0, Battle.Deck.SharedEnergy + Magnitude);
			break;
		case EGameXXKRelicEffectKind::IncreasePartyAttack:
			ForLivingParty([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.Attack += Magnitude; });
			break;
		case EGameXXKRelicEffectKind::IncreasePartyDefense:
			ForLivingParty([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.Defense += Magnitude; });
			break;
		case EGameXXKRelicEffectKind::DamageAllEnemies:
			ForLivingEnemies([Magnitude](FGameXXKCardCombatUnit& Unit){ Unit.HP = FMath::Max(0, Unit.HP - Magnitude); Unit.bLiving = Unit.HP > 0; });
			break;
		case EGameXXKRelicEffectKind::PoisonAllEnemies:
			ForLivingEnemies([Magnitude](FGameXXKCardCombatUnit& Unit){ AddStatus(Unit, EGameXXKCardStatus::Poison, Magnitude); });
			break;
		case EGameXXKRelicEffectKind::BleedAllEnemies:
			ForLivingEnemies([Magnitude](FGameXXKCardCombatUnit& Unit){ AddStatus(Unit, EGameXXKCardStatus::Bleed, Magnitude); });
			break;
		case EGameXXKRelicEffectKind::DrawCards:
			GameXXKCardRules::DrawCards(Battle.Deck, Magnitude, 0);
			break;
		case EGameXXKRelicEffectKind::RevealEnemyIntent:
			Battle.RevealedEnemyIntentCount += Magnitude;
			break;
		case EGameXXKRelicEffectKind::RestoreHeroMana:
			if (FGameXXKCardCombatUnit* Unit = FindUnit(State, FName(TEXT("Player")))) Unit->Mana = FMath::Min(Unit->MaxMana, Unit->Mana + Magnitude);
			break;
		case EGameXXKRelicEffectKind::HealDamagedUnit:
		case EGameXXKRelicEffectKind::ArmorDamagedUnit:
			if (DamageResults)
			{
				TSet<FName> Processed;
				for (const FGameXXKCardDamageResult& Result : *DamageResults)
				{
					if (Result.HealthDamage <= 0 || Processed.Contains(Result.ResolvedTargetUnitId)) continue;
					Processed.Add(Result.ResolvedTargetUnitId);
					if (FGameXXKCardCombatUnit* Unit = FindUnit(State, Result.ResolvedTargetUnitId))
					{
						// These are player-owned relics.  Card plays also report damage dealt to
						// enemies, so never turn an offensive hit into healing or armor for the foe.
						if (Unit->Side != EGameXXKCardTargetSide::Party || !Unit->bLiving) continue;
						if (Definition.EffectKind == EGameXXKRelicEffectKind::HealDamagedUnit) Unit->HP = FMath::Min(Unit->MaxHP, Unit->HP + Magnitude);
						else Unit->Armor = FMath::Clamp(Unit->Armor + Magnitude, 0, 99);
					}
				}
			}
			break;
		default:
			break;
		}
	}

	void ApplyTrigger(
		FGameXXKRuntimeState& State,
		EGameXXKRelicTrigger Trigger,
		FName OwnerUnitId = NAME_None,
		const TArray<FGameXXKCardDamageResult>* DamageResults = nullptr)
	{
		if (!State.CardRun.bHasActiveCardBattle) return;
		for (const FGameXXKRelicInstance& Instance : State.CardRun.Relics)
		{
			const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Instance.RelicId);
			if (Definition && Definition->Trigger == Trigger)
			{
				ApplyCombatEffect(State, *Definition, Instance, OwnerUnitId, DamageResults);
			}
		}
	}
}

bool FGameXXKRelicRules::AcquireRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError)
{
	const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(RelicId);
	if (!Definition) return Fail(OutError, TEXT("The selected relic does not exist in the approved catalog."));
	FGameXXKRelicInstance* Existing = InOutState.CardRun.Relics.FindByPredicate([RelicId](const FGameXXKRelicInstance& Instance){ return Instance.RelicId == RelicId; });
	if (Existing)
	{
		if (!Definition->bStackable) return Fail(OutError, TEXT("This relic is unique and is already owned."));
		FGameXXKRelicInstance Updated = *Existing;
		Updated.Stacks = FMath::Clamp(Updated.Stacks + 1, 1, 99);
		Updated.AcquisitionOrdinal = ++InOutState.CardRun.NextRelicAcquisitionOrdinal;
		InOutState.CardRun.Relics.RemoveAll([RelicId](const FGameXXKRelicInstance& Instance){ return Instance.RelicId == RelicId; });
		InOutState.CardRun.Relics.Insert(Updated, 0);
		return true;
	}
	FGameXXKRelicInstance Instance;
	Instance.RelicId = RelicId;
	Instance.Stacks = 1;
	Instance.AcquisitionOrdinal = ++InOutState.CardRun.NextRelicAcquisitionOrdinal;
	InOutState.CardRun.Relics.Insert(Instance, 0);
	return true;
}

bool FGameXXKRelicRules::CreateRelicOffer(FGameXXKRuntimeState& InOutState, int32 SourceNodeId, int32 ChoiceSeed, TArray<FName>& OutRelicIds, FString* OutError)
{
	OutRelicIds.Reset();
	if (SourceNodeId == INDEX_NONE || ChoiceSeed == 0) return Fail(OutError, TEXT("Relic offers require a stable node and non-zero seed."));
	FGameXXKPendingRelicOffer& Pending = InOutState.CardRun.PendingRelicOffer;
	if (!Pending.RelicIds.IsEmpty())
	{
		if (Pending.SourceNodeId != SourceNodeId) return Fail(OutError, TEXT("A different relic offer is still unresolved."));
		OutRelicIds = Pending.RelicIds;
		return true;
	}
	TArray<FName> Pool;
	for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
	{
		const bool bOwned = InOutState.CardRun.Relics.ContainsByPredicate([&Definition](const FGameXXKRelicInstance& Instance){ return Instance.RelicId == Definition.Id; });
		if (!bOwned || Definition.bStackable) Pool.Add(Definition.Id);
	}
	if (Pool.Num() < 3) return Fail(OutError, TEXT("The relic catalog cannot provide three legal choices."));
	uint32 RandomState = static_cast<uint32>(ChoiceSeed);
	for (int32 Index = 0; Index < 3; ++Index)
	{
		const int32 Pick = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(Pool.Num()));
		OutRelicIds.Add(Pool[Pick]);
		Pool.RemoveAt(Pick);
	}
	Pending.SourceNodeId = SourceNodeId;
	Pending.ChoiceSeed = ChoiceSeed;
	Pending.RelicIds = OutRelicIds;
	return true;
}

bool FGameXXKRelicRules::ChoosePendingRelic(FGameXXKRuntimeState& InOutState, FName RelicId, FString* OutError)
{
	if (!InOutState.CardRun.PendingRelicOffer.RelicIds.Contains(RelicId)) return Fail(OutError, TEXT("The selected relic is not in the saved three-choice offer."));
	if (!AcquireRelic(InOutState, RelicId, OutError)) return false;
	InOutState.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
	return true;
}

void FGameXXKRelicRules::ClearRouteRelics(FGameXXKRuntimeState& InOutState)
{
	InOutState.CardRun.Relics.Reset();
	InOutState.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
	InOutState.CardRun.NextRelicAcquisitionOrdinal = 0;
	InOutState.CardRun.RouteAttributeBonuses = FGameXXKRouteAttributeBonuses();
}

void FGameXXKRelicRules::ApplyBattleStart(FGameXXKRuntimeState& InOutState) { ApplyTrigger(InOutState, EGameXXKRelicTrigger::BattleStart); }
void FGameXXKRelicRules::ApplyPlayerRoundStart(FGameXXKRuntimeState& InOutState) { ApplyTrigger(InOutState, EGameXXKRelicTrigger::PlayerRoundStart); }
void FGameXXKRelicRules::ApplyPlayerRoundEnd(FGameXXKRuntimeState& InOutState) { ApplyTrigger(InOutState, EGameXXKRelicTrigger::PlayerRoundEnd); }

void FGameXXKRelicRules::ApplyCardPlayed(FGameXXKRuntimeState& InOutState, FName OwnerUnitId, const TArray<FGameXXKCardDamageResult>& DamageResults)
{
	ApplyTrigger(InOutState, EGameXXKRelicTrigger::CardPlayed, OwnerUnitId, &DamageResults);
	ApplyDamageTaken(InOutState, DamageResults);
	TSet<FName> DefeatedEnemies;
	for (const FGameXXKCardDamageResult& Result : DamageResults)
	{
		if (FGameXXKCardCombatUnit* Unit = FindUnit(InOutState, Result.ResolvedTargetUnitId); Unit && Unit->Side == EGameXXKCardTargetSide::Enemy && !Unit->bLiving)
			DefeatedEnemies.Add(Unit->UnitId);
	}
	if (!DefeatedEnemies.IsEmpty()) ApplyTrigger(InOutState, EGameXXKRelicTrigger::EnemyDefeated, OwnerUnitId, &DamageResults);
}

void FGameXXKRelicRules::ApplyDamageTaken(FGameXXKRuntimeState& InOutState, const TArray<FGameXXKCardDamageResult>& DamageResults)
{
	ApplyTrigger(InOutState, EGameXXKRelicTrigger::DamageTaken, NAME_None, &DamageResults);
}

bool FGameXXKRelicRules::CalculateRouteNodeTravelMoneyBonus(
	const FGameXXKRuntimeState& State,
	int32& OutBonus,
	FString* OutError)
{
	OutBonus = 0;
	if (OutError)
	{
		OutError->Reset();
	}

	int64 TotalBonus = 0;
	for (const FGameXXKRelicInstance& Instance : State.CardRun.Relics)
	{
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Instance.RelicId);
		if (!Definition
			|| Definition->Trigger != EGameXXKRelicTrigger::RouteNodeCompleted
			|| Definition->EffectKind != EGameXXKRelicEffectKind::GainRouteTravelMoney)
		{
			continue;
		}
		if (Definition->Magnitude < 0 || Instance.Stacks < 1)
		{
			return Fail(OutError, TEXT("The route-travel-money relic state is invalid."));
		}

		const int64 StackMultiplier = Definition->bStackable ? static_cast<int64>(Instance.Stacks) : 1;
		const int64 Contribution = static_cast<int64>(Definition->Magnitude) * StackMultiplier;
		if (Contribution > MAX_int32 || TotalBonus > static_cast<int64>(MAX_int32) - Contribution)
		{
			return Fail(OutError, TEXT("The route-travel-money relic bonus would overflow."));
		}
		TotalBonus += Contribution;
	}

	OutBonus = static_cast<int32>(TotalBonus);
	return true;
}

void FGameXXKRelicRules::ApplyRouteNodeCompletedNonCurrency(FGameXXKRuntimeState& InOutState)
{
	for (const FGameXXKRelicInstance& Instance : InOutState.CardRun.Relics)
	{
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(Instance.RelicId);
		if (!Definition || Definition->Trigger != EGameXXKRelicTrigger::RouteNodeCompleted) continue;
		const int32 Magnitude = EffectiveMagnitude(*Definition, Instance);
		switch (Definition->EffectKind)
		{
		case EGameXXKRelicEffectKind::GainRouteTravelMoney:
			break;
		case EGameXXKRelicEffectKind::HealPlayer:
			InOutState.PlayerHP = FMath::Min(
				InOutState.PlayerMaxHP + FMath::Max(0, InOutState.CardRun.RouteAttributeBonuses.MaxHealth),
				InOutState.PlayerHP + Magnitude);
			break;
		case EGameXXKRelicEffectKind::GainRouteMaxHealth: InOutState.CardRun.RouteAttributeBonuses.MaxHealth += Magnitude; break;
		case EGameXXKRelicEffectKind::GainRouteMaxMana: InOutState.CardRun.RouteAttributeBonuses.MaxMana += Magnitude; break;
		case EGameXXKRelicEffectKind::GainRouteAttack: InOutState.CardRun.RouteAttributeBonuses.Attack += Magnitude; break;
		case EGameXXKRelicEffectKind::GainRouteDefense: InOutState.CardRun.RouteAttributeBonuses.Defense += Magnitude; break;
		case EGameXXKRelicEffectKind::GainRouteSpeed: InOutState.CardRun.RouteAttributeBonuses.Speed += Magnitude; break;
		default: break;
		}
	}
}
