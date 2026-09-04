#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKCardRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "GameXXKTalentRules.h"
#include "Misc/Crc.h"

namespace
{
	constexpr int32 HeroSelectedCardCount = 8;
	constexpr int32 PermanentCompanionSelectedCardCount = 5;
	constexpr int32 QuestNpcSelectedCardCount = 3;
	// The shared battle deck is now exclusively the player's configured cards
	// plus boss-card slots; no route cards are materialized (2026-08-14).
	constexpr int32 StartingDeckCardCount = 8;
	constexpr int32 MaximumDeckCardCount = 8 + 5 + 3 + FGameXXKCardRunState::MaxBossCardSlots;

	const FName HeroUnitId(TEXT("Player"));
	const FName SpiralHornDeerSpringHealIntentId(TEXT("SpringHeal"));
	const FName LifeSavingTalismanRelicId(TEXT("Relic.LifeSavingTalisman"));

	EGameXXKCardBattleNodeKind CardBattleNodeKind(
		const EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Battle:
			return EGameXXKCardBattleNodeKind::Battle;
		case EGameXXKNodeKind::Elite:
			return EGameXXKCardBattleNodeKind::Elite;
		case EGameXXKNodeKind::Boss:
			return EGameXXKCardBattleNodeKind::Boss;
		default:
			return EGameXXKCardBattleNodeKind::Invalid;
		}
	}

	bool SetFailure(FString* OutError, const TCHAR* Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool SetFailure(FString* OutError, const FString& Error)
	{
		if (OutError)
		{
			*OutError = Error;
		}
		return false;
	}

	bool FinalizeLifeSavingTalismanConsumption(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		if (!InOutState.CardRun.bHasActiveCardBattle
			|| !InOutState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending)
		{
			return true;
		}

		FGameXXKCardBattleRuntime& Battle = InOutState.CardRun.ActiveBattle;
		if (Battle.bLifeSavingTalismanArmed)
		{
			return SetFailure(OutError, TEXT("A pending life-saving talisman consumption cannot remain armed."));
		}
		if (Battle.LifeSavingTalismanHealingPercent < 1
			|| Battle.LifeSavingTalismanHealingPercent > 100)
		{
			return SetFailure(OutError, TEXT("A pending life-saving talisman consumption lost its catalog healing magnitude."));
		}
		const int32 RelicIndex = InOutState.CardRun.Relics.IndexOfByPredicate([](const FGameXXKRelicInstance& Instance)
		{
			return Instance.RelicId == LifeSavingTalismanRelicId;
		});
		if (RelicIndex == INDEX_NONE)
		{
			return SetFailure(OutError, TEXT("A protected health-loss packet lost its owned life-saving talisman."));
		}

		InOutState.CardRun.Relics.RemoveAt(RelicIndex, 1, EAllowShrinking::No);
		Battle.bLifeSavingTalismanConsumptionPending = false;
		Battle.LifeSavingTalismanHealingPercent = 0;
		return true;
	}

	bool NameLess(const FName Left, const FName Right)
	{
		return Left.ToString() < Right.ToString();
	}

	uint32 NextRandom(uint32& InOutState)
	{
		if (InOutState == 0)
		{
			InOutState = 0x9E3779B9U;
		}
		InOutState ^= InOutState << 13;
		InOutState ^= InOutState >> 17;
		InOutState ^= InOutState << 5;
		return InOutState;
	}

	bool IsHeroCard(const FName CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return Definition && Definition->Owner == EGameXXKCardOwner::Hero;
	}

	bool IsRouteCard(const FName CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return Definition
			&& Definition->Owner == EGameXXKCardOwner::Route
			&& CardId.ToString().StartsWith(TEXT("Route.Boss."));
	}

	bool AreUniqueNonEmptyCardIds(const TArray<FName>& CardIds)
	{
		TSet<FName> Seen;
		for (const FName CardId : CardIds)
		{
			if (CardId.IsNone() || Seen.Contains(CardId))
			{
				return false;
			}
			Seen.Add(CardId);
		}
		return true;
	}

	const FGameXXKPermanentCompanion* FindActiveCompanion(const FGameXXKCompanionRosterState& Roster)
	{
		return Roster.PermanentCompanions.FindByPredicate([](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.bIsActive;
		});
	}

	FGameXXKPermanentCompanion* FindActiveCompanion(FGameXXKCompanionRosterState& Roster)
	{
		return Roster.PermanentCompanions.FindByPredicate([](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.bIsActive;
		});
	}

	bool SynchronizePartySelectionWithRoster(FGameXXKCardRunState& InOutRun, FString* OutError)
	{
		int32 ActiveCount = 0;
		FName ActiveInstanceId = NAME_None;
		for (const FGameXXKPermanentCompanion& Candidate : InOutRun.CompanionRoster.PermanentCompanions)
		{
			if (Candidate.bIsActive)
			{
				++ActiveCount;
				ActiveInstanceId = Candidate.InstanceId;
			}
		}
		if (ActiveCount > 1)
		{
			return SetFailure(OutError, TEXT("The persistent roster contains more than one active permanent companion."));
		}
		InOutRun.PartySelection.ActivePermanentCompanionInstanceId = ActiveInstanceId;
		return true;
	}

	bool ValidateHeroLoadout(const FGameXXKCardRunState& Run, FString* OutError)
	{
		if (Run.HeroUnlockedCardIds.Num() < HeroSelectedCardCount || Run.HeroSelectedCardIds.Num() != HeroSelectedCardCount
			|| !AreUniqueNonEmptyCardIds(Run.HeroUnlockedCardIds) || !AreUniqueNonEmptyCardIds(Run.HeroSelectedCardIds))
		{
			return SetFailure(OutError, TEXT("The hero card collection must have at least eight unlocked cards and exactly eight unique selections."));
		}
		for (const FName CardId : Run.HeroUnlockedCardIds)
		{
			if (!IsHeroCard(CardId))
			{
				return SetFailure(OutError, TEXT("The hero card collection contains a non-hero card."));
			}
		}
		for (const FName CardId : Run.HeroSelectedCardIds)
		{
			if (!Run.HeroUnlockedCardIds.Contains(CardId))
			{
				return SetFailure(OutError, TEXT("The hero card selection contains a locked card."));
			}
		}
		return true;
	}

	FGameXXKBattleRuntimeUnit MakeLegacyProjectionUnit(
		const FName UnitId,
		const FText& DisplayName,
		const FGameXXKCompanionAttributes& Attributes,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = DisplayName;
		Unit.HP = Attributes.Health;
		Unit.MaxHP = Attributes.Health;
		Unit.MP = Attributes.Mana;
		Unit.MaxMP = Attributes.Mana;
		Unit.Attack = Attributes.Attack;
		Unit.Defense = Attributes.Defense;
		Unit.Speed = FMath::Max(1, Attributes.Speed);
		Unit.Shield = 1;
		Unit.bEnemy = bEnemy;
		Unit.bDefeated = Attributes.Health <= 0;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit MakeHeroProjectionUnit(const FGameXXKRuntimeState& State)
	{
		FGameXXKBattleRuntimeUnit Unit;
		if (const FGameXXKBattleRuntimeUnit* Existing = State.ActiveBattleParty.FindByPredicate([](const FGameXXKBattleRuntimeUnit& Candidate)
		{
			return Candidate.Id == HeroUnitId;
		}))
		{
			Unit = *Existing;
		}
		else
		{
			Unit.Id = HeroUnitId;
			Unit.DisplayName = FText::FromString(TEXT("主角"));
			Unit.HP = State.PlayerHP;
			Unit.MaxHP = State.PlayerMaxHP;
			Unit.MP = State.PlayerMP;
			Unit.MaxMP = State.PlayerMaxMP;
			Unit.Attack = State.PlayerAttack;
			Unit.Defense = State.PlayerDefense;
			Unit.Speed = State.PlayerSpeed;
			Unit.Shield = 1;
		}
		Unit.Id = HeroUnitId;
		Unit.bEnemy = false;
		Unit.MaxHP = FMath::Max(1, Unit.MaxHP);
		Unit.HP = FMath::Clamp(Unit.HP, 1, Unit.MaxHP);
		Unit.MaxMP = FMath::Max(0, Unit.MaxMP);
		Unit.MP = FMath::Clamp(Unit.MP, 0, Unit.MaxMP);
		Unit.Attack = FMath::Max(0, Unit.Attack);
		Unit.Defense = FMath::Max(0, Unit.Defense);
		Unit.Speed = FMath::Max(1, Unit.Speed);
		Unit.bDefeated = false;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeCardCombatUnit(
		const FGameXXKBattleRuntimeUnit& LegacyUnit,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = LegacyUnit.Id;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.MaxHP = FMath::Max(1, LegacyUnit.MaxHP);
		Unit.HP = FMath::Clamp(LegacyUnit.HP, 0, Unit.MaxHP);
		Unit.MaxMana = FMath::Max(0, LegacyUnit.MaxMP);
		Unit.Mana = FMath::Clamp(LegacyUnit.MP, 0, Unit.MaxMana);
		Unit.Attack = FMath::Max(0, LegacyUnit.Attack);
		Unit.Defense = FMath::Max(0, LegacyUnit.Defense);
		Unit.Speed = FMath::Max(1, LegacyUnit.Speed);
		Unit.Armor = FMath::Max(0, LegacyUnit.Shield);
		Unit.StableSortOrder = StableSortOrder;
		Unit.EnemyDefinitionId = LegacyUnit.EnemyDefinitionId;
		Unit.BattleSlotNumber = LegacyUnit.BattleSlotNumber;
		Unit.CombatLevel = LegacyUnit.CombatLevel;
		Unit.bLiving = Unit.HP > 0;
		return Unit;
	}

	bool BuildQuestNpcEquipmentSnapshot(
		const FGameXXKRuntimeState& State,
		const FName QuestNpcId,
		const int32 QuestNpcLevel,
		FGameXXKCompanionAttributes& OutBareAttributes,
		FGameXXKEquipmentLoadoutSnapshot& OutSnapshot,
		FString* OutError)
	{
		if (!FGameXXKCompanionRules::GetQuestNpcAttributes(
			QuestNpcId,
			QuestNpcLevel,
			OutBareAttributes,
			OutError))
		{
			return false;
		}

		FGameXXKCharacterStats BareStats;
		BareStats.MaxHealth = OutBareAttributes.Health;
		BareStats.MaxMana = OutBareAttributes.Mana;
		BareStats.Attack = OutBareAttributes.Attack;
		BareStats.Defense = OutBareAttributes.Defense;
		BareStats.Speed = OutBareAttributes.Speed;
		return FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			State.EquipmentCollection,
			QuestNpcId,
			BareStats,
			OutSnapshot,
			OutError);
	}

	bool BuildRoutePartyProjection(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		FGameXXKCardRunState& Run = InOutState.CardRun;
		if (!SynchronizePartySelectionWithRoster(Run, OutError)
			|| !FGameXXKCompanionRules::ValidatePartySelection(Run.CompanionRoster, Run.PartySelection, OutError))
		{
			return false;
		}

		FGameXXKEquipmentLoadoutSnapshot HeroSnapshot;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			InOutState.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			FGameXXKCharacterStatRules::GetBareHeroStats(InOutState.PlayerLevel),
			HeroSnapshot,
			OutError))
		{
			return false;
		}
		FGameXXKTalentProjection TalentProjection;
		if (!FGameXXKTalentRules::BuildProjection(InOutState.Talents, TalentProjection, OutError))
		{
			return false;
		}
		const auto ScaleTalentStat = [](const int32 BaseValue, const int32 Percent)
		{
			return static_cast<int32>(FMath::Clamp<int64>(
				(static_cast<int64>(FMath::Max(0, BaseValue)) * (100 + FMath::Max(0, Percent)) + 50) / 100,
				0,
				MAX_int32));
		};

	TArray<FGameXXKBattleRuntimeUnit> NewParty;
	FGameXXKBattleRuntimeUnit Hero = MakeHeroProjectionUnit(InOutState);
	const FGameXXKRouteAttributeBonuses& RouteBonus = Run.RouteAttributeBonuses;
	// Route bonuses are derived from the persistent hero baseline every battle, including an
	// unequipped hero. ActiveBattleParty is only a presentation projection and may already
	// include these bonuses from the previous room, so incrementing that projection would
	// silently double every event reward.
	const int32 PreviousHeroMaxHP = FMath::Max(1, InOutState.PlayerMaxHP + FMath::Max(0, RouteBonus.MaxHealth));
	const int32 MissingHeroHP = FMath::Max(0, PreviousHeroMaxHP - InOutState.PlayerHP);
	Hero.MaxHP = FMath::Max(1, ScaleTalentStat(
		HeroSnapshot.AttributesBeforeRoute.MaxHealth
			+ FMath::Max(0, RouteBonus.MaxHealth)
			+ TalentProjection.FlatMaxHP,
		TalentProjection.RouteMaxHPPercent));
	Hero.HP = FMath::Clamp(Hero.MaxHP - MissingHeroHP, 1, Hero.MaxHP);
	Hero.MaxMP = FMath::Max(0, HeroSnapshot.AttributesBeforeRoute.MaxMana + FMath::Max(0, RouteBonus.MaxMana));
	Hero.MP = FMath::Clamp(InOutState.PlayerMP, 0, Hero.MaxMP);
	Hero.Attack = ScaleTalentStat(
		HeroSnapshot.AttributesBeforeRoute.Attack
			+ FMath::Max(0, RouteBonus.Attack)
			+ TalentProjection.FlatAttack,
		TalentProjection.RouteAttackPercent);
	Hero.Defense = ScaleTalentStat(
		HeroSnapshot.AttributesBeforeRoute.Defense
			+ FMath::Max(0, RouteBonus.Defense)
			+ TalentProjection.FlatDefense,
		TalentProjection.RouteDefensePercent);
	Hero.Speed = FMath::Max(1, HeroSnapshot.AttributesBeforeRoute.Speed + FMath::Max(0, RouteBonus.Speed));
	Hero.CombatLevel = FMath::Max(1, InOutState.PlayerLevel);
	NewParty.Add(Hero);
		if (const FGameXXKPermanentCompanion* Companion = FindActiveCompanion(Run.CompanionRoster))
		{
			FGameXXKCharacterStats BareStats;
			FGameXXKEquipmentLoadoutSnapshot CompanionSnapshot;
			if (!FGameXXKCharacterStatRules::GetBareCompanionStats(Companion->Role, Companion->Level, Companion->Star, BareStats, OutError)
				|| !FGameXXKEquipmentRules::BuildLoadoutSnapshot(
					InOutState.EquipmentCollection,
					Companion->InstanceId,
					BareStats,
					CompanionSnapshot,
					OutError))
			{
				return false;
			}
			FGameXXKCompanionAttributes Attributes;
			Attributes.Health = CompanionSnapshot.AttributesBeforeRoute.MaxHealth;
			Attributes.Mana = CompanionSnapshot.AttributesBeforeRoute.MaxMana;
			Attributes.Attack = CompanionSnapshot.AttributesBeforeRoute.Attack;
			Attributes.Defense = CompanionSnapshot.AttributesBeforeRoute.Defense;
			Attributes.Speed = CompanionSnapshot.AttributesBeforeRoute.Speed;
			Attributes.Health = FMath::Max(1, ScaleTalentStat(
				Attributes.Health + TalentProjection.FlatMaxHP,
				TalentProjection.RouteMaxHPPercent));
			Attributes.Attack = ScaleTalentStat(
				Attributes.Attack + TalentProjection.FlatAttack,
				TalentProjection.RouteAttackPercent);
			Attributes.Defense = ScaleTalentStat(
				Attributes.Defense + TalentProjection.FlatDefense,
				TalentProjection.RouteDefensePercent);
			FGameXXKBattleRuntimeUnit CompanionUnit = MakeLegacyProjectionUnit(
				Companion->InstanceId,
				FText::FromString(TEXT("伙伴")),
				Attributes,
				false);
			CompanionUnit.CombatLevel = FMath::Max(1, Companion->Level);
			NewParty.Add(MoveTemp(CompanionUnit));
		}

		FName QuestNpcId;
		if (!FGameXXKPartyFormationRules::ResolveQuestNpcId(InOutState, QuestNpcId, OutError))
		{
			return false;
		}
		if (Run.PartySelection.QuestNpc.NpcId != QuestNpcId)
		{
			return SetFailure(OutError, TEXT("The ordered NPC does not match the configured NPC cards."));
		}
		const FGameXXKQuestNpcProgression* QuestNpcProgression =
			Run.PartySelection.QuestNpcProgressions.Find(QuestNpcId);
		const int32 QuestNpcLevel = QuestNpcProgression
			? FMath::Clamp(QuestNpcProgression->Level, 1, FGameXXKCharacterStatRules::MaxCharacterLevel)
			: 1;
		FGameXXKCompanionAttributes QuestNpcAttributes;
		FGameXXKEquipmentLoadoutSnapshot QuestNpcSnapshot;
		if (!BuildQuestNpcEquipmentSnapshot(
			InOutState,
			QuestNpcId,
			QuestNpcLevel,
			QuestNpcAttributes,
			QuestNpcSnapshot,
			OutError))
		{
			return false;
		}
		QuestNpcAttributes.Health = QuestNpcSnapshot.AttributesBeforeRoute.MaxHealth;
		QuestNpcAttributes.Mana = QuestNpcSnapshot.AttributesBeforeRoute.MaxMana;
		QuestNpcAttributes.Attack = QuestNpcSnapshot.AttributesBeforeRoute.Attack;
		QuestNpcAttributes.Defense = QuestNpcSnapshot.AttributesBeforeRoute.Defense;
		QuestNpcAttributes.Speed = QuestNpcSnapshot.AttributesBeforeRoute.Speed;
		FGameXXKBattleRuntimeUnit QuestNpc = MakeLegacyProjectionUnit(
			QuestNpcId,
			FText::FromString(TEXT("NPC")),
			QuestNpcAttributes,
			false);
		QuestNpc.MaxHP = FMath::Max(1, ScaleTalentStat(
			QuestNpc.MaxHP + TalentProjection.FlatMaxHP,
			TalentProjection.RouteMaxHPPercent));
		QuestNpc.HP = QuestNpc.MaxHP;
		QuestNpc.Attack = ScaleTalentStat(
			QuestNpc.Attack + TalentProjection.FlatAttack,
			TalentProjection.RouteAttackPercent);
		QuestNpc.Defense = ScaleTalentStat(
			QuestNpc.Defense + TalentProjection.FlatDefense,
			TalentProjection.RouteDefensePercent);
		QuestNpc.BattleSlotNumber = INDEX_NONE;
		QuestNpc.EnemyDefinitionId = NAME_None;
		QuestNpc.bDefending = false;
		QuestNpc.CombatLevel = QuestNpcLevel;
		NewParty.Add(MoveTemp(QuestNpc));

		if (NewParty.Num() != 3)
		{
			return SetFailure(OutError, TEXT("The card battle party must contain hero, companion, and NPC."));
		}
		InOutState.ActiveBattleParty = MoveTemp(NewParty);
		return true;
	}

	bool MaterializeEquipmentBattleEffects(FGameXXKRuntimeState& InOutState, FGameXXKCardBattleRuntime& InOutRuntime, FString* OutError)
	{
		TArray<FGameXXKEquipmentLoadoutSnapshot> Snapshots;
		FGameXXKEquipmentLoadoutSnapshot HeroSnapshot;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			InOutState.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			FGameXXKCharacterStatRules::GetBareHeroStats(InOutState.PlayerLevel),
			HeroSnapshot,
			OutError))
		{
			return false;
		}
		Snapshots.Add(MoveTemp(HeroSnapshot));

		if (const FGameXXKPermanentCompanion* Companion = FindActiveCompanion(InOutState.CardRun.CompanionRoster))
		{
			FGameXXKCharacterStats CompanionBareStats;
			FGameXXKEquipmentLoadoutSnapshot CompanionSnapshot;
			if (!FGameXXKCharacterStatRules::GetBareCompanionStats(Companion->Role, Companion->Level, Companion->Star, CompanionBareStats, OutError)
				|| !FGameXXKEquipmentRules::BuildLoadoutSnapshot(
					InOutState.EquipmentCollection,
					Companion->InstanceId,
					CompanionBareStats,
					CompanionSnapshot,
					OutError))
			{
				return false;
			}
			Snapshots.Add(MoveTemp(CompanionSnapshot));
		}

		FName QuestNpcId;
		if (!FGameXXKPartyFormationRules::ResolveQuestNpcId(InOutState, QuestNpcId, OutError))
		{
			return false;
		}
		const FGameXXKQuestNpcProgression* QuestNpcProgression =
			InOutState.CardRun.PartySelection.QuestNpcProgressions.Find(QuestNpcId);
		const int32 QuestNpcLevel = QuestNpcProgression
			? FMath::Clamp(QuestNpcProgression->Level, 1, FGameXXKCharacterStatRules::MaxCharacterLevel)
			: 1;
		FGameXXKCompanionAttributes QuestNpcAttributes;
		FGameXXKEquipmentLoadoutSnapshot QuestNpcSnapshot;
		if (!BuildQuestNpcEquipmentSnapshot(
			InOutState,
			QuestNpcId,
			QuestNpcLevel,
			QuestNpcAttributes,
			QuestNpcSnapshot,
			OutError))
		{
			return false;
		}
		Snapshots.Add(MoveTemp(QuestNpcSnapshot));

		InOutRuntime.EquipmentEffects.Reset();
		TSet<FString> EffectKeys;
		const auto AddEffect = [&InOutRuntime, &EffectKeys, OutError](const FGameXXKEquipmentActiveEffect& Effect)
		{
			if (Effect.EffectId.IsNone() || Effect.SourceCharacterId.IsNone())
			{
				return SetFailure(OutError, TEXT("Equipment battle descriptors require stable effect and source IDs."));
			}
			const FString Key = Effect.EffectId.ToString() + TEXT("|") + Effect.SourceCharacterId.ToString();
			if (EffectKeys.Contains(Key))
			{
				return SetFailure(OutError, TEXT("Equipment battle descriptors may not duplicate an effect-source pair."));
			}
			EffectKeys.Add(Key);
			FGameXXKEquipmentBattleEffectRuntime& RuntimeEffect = InOutRuntime.EquipmentEffects.AddDefaulted_GetRef();
			RuntimeEffect.ActiveEffect = Effect;
			RuntimeEffect.SourceCharacterId = Effect.SourceCharacterId;
			return true;
		};
		for (const FGameXXKEquipmentLoadoutSnapshot& Snapshot : Snapshots)
		{
			for (const FGameXXKEquipmentActiveEffect& Effect : Snapshot.ActivePersonalEffects)
			{
				if (!AddEffect(Effect))
				{
					return false;
				}
			}
		}
		for (const FGameXXKEquipmentActiveEffect& Effect : FGameXXKEquipmentRules::ResolveTeamEffects(Snapshots))
		{
			if (!AddEffect(Effect))
			{
				return false;
			}
		}
		return true;
	}

	bool BuildStartingCardInstances(
		const FGameXXKRuntimeState& State,
		const int32 SourceNodeId,
		TArray<FGameXXKCardInstance>& OutInstances,
		FString* OutError)
	{
		OutInstances.Reset();
		const FGameXXKCardRunState& Run = State.CardRun;
		if (!ValidateHeroLoadout(Run, OutError))
		{
			return false;
		}

		int32 InstanceOrdinal = 0;
		const auto AddInstance = [&OutInstances, &InstanceOrdinal, SourceNodeId, OutError](const FName CardId, const FName OwnerUnitId)
		{
			if (CardId.IsNone() || OwnerUnitId.IsNone() || !FGameXXKCardCatalog::FindCardDefinition(CardId))
			{
				return SetFailure(OutError, TEXT("The route deck contains an unknown card or an invalid owner."));
			}
			FGameXXKCardInstance& Instance = OutInstances.AddDefaulted_GetRef();
			const int32 AcquisitionOrdinal = InstanceOrdinal++;
			Instance.InstanceId = FName(*FString::Printf(TEXT("CardRun.%d.%03d"), SourceNodeId, AcquisitionOrdinal));
			Instance.CardId = CardId;
			Instance.OwnerUnitId = OwnerUnitId;
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("RouteEntry.%d.%03d"), SourceNodeId, AcquisitionOrdinal));
			Instance.AcquisitionOrdinal = AcquisitionOrdinal;
			return true;
		};

		for (const FName CardId : Run.HeroSelectedCardIds)
		{
			if (!AddInstance(CardId, HeroUnitId))
			{
				return false;
			}
		}

		int32 MissingFillIndex = 0;
		if (const FGameXXKPermanentCompanion* Companion = FindActiveCompanion(Run.CompanionRoster))
		{
			if (Companion->SelectedCardIds.Num() != PermanentCompanionSelectedCardCount)
			{
				return SetFailure(OutError, TEXT("The active permanent companion does not have five selected cards."));
			}
			for (const FName CardId : Companion->SelectedCardIds)
			{
				if (!AddInstance(CardId, Companion->InstanceId))
				{
					return false;
				}
			}
		}

		FName QuestNpcId;
		if (!FGameXXKPartyFormationRules::ResolveQuestNpcId(State, QuestNpcId, OutError))
		{
			return false;
		}
		if (Run.PartySelection.QuestNpc.NpcId != QuestNpcId
			|| Run.PartySelection.QuestNpc.SelectedCardIds.Num() != QuestNpcSelectedCardCount)
		{
			return SetFailure(OutError, TEXT("The selected NPC does not have a valid three-card loadout."));
		}
		for (const FName CardId : Run.PartySelection.QuestNpc.SelectedCardIds)
		{
			if (!AddInstance(CardId, QuestNpcId))
			{
				return false;
			}
		}

		// Boss cards are the only non-character cards allowed into the player deck.
		for (const FName CardId : Run.BossCardSlots)
		{
			if (!IsRouteCard(CardId) || !AddInstance(CardId, HeroUnitId))
			{
				return SetFailure(OutError, TEXT("The boss-card slot contains an invalid card."));
			}
		}

		if (OutInstances.Num() < StartingDeckCardCount || OutInstances.Num() > MaximumDeckCardCount)
		{
			return SetFailure(OutError, TEXT("The materialized shared deck is outside its 18-30 card contract."));
		}
		TMap<FName, int32> CardCounts;
		for (const FGameXXKCardInstance& Instance : OutInstances)
		{
			const int32 NewCount = ++CardCounts.FindOrAdd(Instance.CardId);
			if (NewCount > 2)
			{
				return SetFailure(OutError, TEXT("The shared route deck contains more than two copies of one CardId."));
			}
		}
		return true;
	}

	bool BuildCardCombatUnits(const FGameXXKRuntimeState& State, TArray<FGameXXKCardCombatUnit>& OutUnits, FString* OutError)
	{
		if (State.ActiveBattleParty.IsEmpty() || State.ActiveBattleEnemies.IsEmpty())
		{
			return SetFailure(OutError, TEXT("A card battle requires non-empty party and enemy projections."));
		}
		TArray<FGameXXKCardCombatUnit> NewUnits;
		NewUnits.Reserve(State.ActiveBattleParty.Num() + State.ActiveBattleEnemies.Num());
		for (int32 PartyIndex = 0; PartyIndex < State.ActiveBattleParty.Num(); ++PartyIndex)
		{
			const FGameXXKBattleRuntimeUnit& LegacyUnit = State.ActiveBattleParty[PartyIndex];
			EGameXXKCharacterRole Role = EGameXXKCharacterRole::QuestNpc;
			if (LegacyUnit.Id == HeroUnitId)
			{
				Role = EGameXXKCharacterRole::Hero;
			}
			else if (const FGameXXKPermanentCompanion* Companion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([&LegacyUnit](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == LegacyUnit.Id;
			}))
			{
				Role = Companion->Role;
			}
			NewUnits.Add(MakeCardCombatUnit(LegacyUnit, EGameXXKCardTargetSide::Party, Role, PartyIndex));
		}
		TSet<int32> ExplicitEnemySlots;
		for (int32 EnemyIndex = 0; EnemyIndex < State.ActiveBattleEnemies.Num(); ++EnemyIndex)
		{
			const FGameXXKBattleRuntimeUnit& LegacyUnit = State.ActiveBattleEnemies[EnemyIndex];
			if (!LegacyUnit.bEnemy || LegacyUnit.Id.IsNone())
			{
				return SetFailure(OutError, TEXT("The battle enemy projection contains an invalid stable enemy unit."));
			}
			if (LegacyUnit.BattleSlotNumber != INDEX_NONE)
			{
				if (LegacyUnit.BattleSlotNumber < 1 || LegacyUnit.BattleSlotNumber > 3)
				{
					return SetFailure(OutError, TEXT("An explicit enemy battle slot must be one of 1P, 2P, or 3P."));
				}
				if (ExplicitEnemySlots.Contains(LegacyUnit.BattleSlotNumber))
				{
					return SetFailure(OutError, TEXT("The battle enemy projection contains duplicate explicit presentation slots."));
				}
				ExplicitEnemySlots.Add(LegacyUnit.BattleSlotNumber);
			}
			NewUnits.Add(MakeCardCombatUnit(LegacyUnit, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, EnemyIndex));
		}
		OutUnits = MoveTemp(NewUnits);
		return true;
	}

	FGameXXKCardCombatUnit* FindCardUnit(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindCardUnit(const TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindLowestLivingPartyUnit(const FGameXXKCardBattleRuntime& Runtime)
	{
		const FGameXXKCardCombatUnit* Result = nullptr;
		for (const FGameXXKCardCombatUnit& Candidate : Runtime.Units)
		{
			if (!Candidate.bLiving || Candidate.Side != EGameXXKCardTargetSide::Party)
			{
				continue;
			}
			if (!Result
				|| static_cast<int64>(Candidate.HP) * Result->MaxHP < static_cast<int64>(Result->HP) * Candidate.MaxHP
				|| (static_cast<int64>(Candidate.HP) * Result->MaxHP == static_cast<int64>(Result->HP) * Candidate.MaxHP
					&& (Candidate.StableSortOrder < Result->StableSortOrder
						|| (Candidate.StableSortOrder == Result->StableSortOrder && NameLess(Candidate.UnitId, Result->UnitId)))))
			{
				Result = &Candidate;
			}
		}
		return Result;
	}

	bool ValidateLivingEnemyIntentPresentation(const FGameXXKCardBattleRuntime& Runtime, FString* OutError)
	{
		int32 LivingEnemyCount = 0;
		TSet<int32> OccupiedEnemySlots;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			if (++LivingEnemyCount > 3)
			{
				return SetFailure(OutError, TEXT("Enemy intent presentation supports at most three living enemies."));
			}
			const int32 SlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, Unit.UnitId);
			if (SlotNumber == INDEX_NONE || OccupiedEnemySlots.Contains(SlotNumber))
			{
				return SetFailure(OutError, TEXT("Living enemy intents require unique supported enemy presentation slots."));
			}
			OccupiedEnemySlots.Add(SlotNumber);
		}
		return true;
	}

	const FGameXXKCardCombatUnit* FindLowestLivingUnitForSide(
		const FGameXXKCardBattleRuntime& Runtime,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCardStatus RequiredStatus = EGameXXKCardStatus::None)
	{
		const FGameXXKCardCombatUnit* Result = nullptr;
		for (const FGameXXKCardCombatUnit& Candidate : Runtime.Units)
		{
			if (!Candidate.bLiving || Candidate.Side != Side
				|| (RequiredStatus != EGameXXKCardStatus::None
					&& GameXXKCardRules::GetCombatStatusStacks(Candidate, RequiredStatus) <= 0))
			{
				continue;
			}
			if (!Result
				|| static_cast<int64>(Candidate.HP) * Result->MaxHP < static_cast<int64>(Result->HP) * Candidate.MaxHP
				|| (static_cast<int64>(Candidate.HP) * Result->MaxHP == static_cast<int64>(Result->HP) * Candidate.MaxHP
					&& (Candidate.StableSortOrder < Result->StableSortOrder
						|| (Candidate.StableSortOrder == Result->StableSortOrder && NameLess(Candidate.UnitId, Result->UnitId)))))
			{
				Result = &Candidate;
			}
		}
		return Result;
	}

	bool IsConcretePersistentTargetStatus(const EGameXXKCardStatus Status)
	{
		const uint8 SerializedStatus = static_cast<uint8>(Status);
		return SerializedStatus > static_cast<uint8>(EGameXXKCardStatus::None)
			&& SerializedStatus <= static_cast<uint8>(EGameXXKCardStatus::Counter);
	}

	const FGameXXKCardCombatUnit* FindLivingPersistentTarget(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKEnemyBattleState& EnemyState)
	{
		const EGameXXKCardStatus TargetStatus = static_cast<EGameXXKCardStatus>(EnemyState.PersistentTargetStatus);
		if (EnemyState.PersistentTargetUnitId.IsNone()
			|| !IsConcretePersistentTargetStatus(TargetStatus))
		{
			return nullptr;
		}
		const FGameXXKCardCombatUnit* Target = FindCardUnit(Runtime.Units, EnemyState.PersistentTargetUnitId);
		return Target
			&& Target->bLiving
			&& Target->Side == EGameXXKCardTargetSide::Party
			&& GameXXKCardRules::GetCombatStatusStacks(*Target, TargetStatus) > 0
			? Target
			: nullptr;
	}

	void ClearPersistentTarget(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKEnemyBattleState& InOutEnemyState)
	{
		const EGameXXKCardStatus TargetStatus = static_cast<EGameXXKCardStatus>(InOutEnemyState.PersistentTargetStatus);
		if (!InOutEnemyState.PersistentTargetUnitId.IsNone()
			&& IsConcretePersistentTargetStatus(TargetStatus))
		{
			if (FGameXXKCardCombatUnit* ExistingTarget = FindCardUnit(
				InOutRuntime.Units,
				InOutEnemyState.PersistentTargetUnitId))
			{
				GameXXKCardRules::ConsumeCombatStatus(
					*ExistingTarget,
					TargetStatus,
					MAX_int32);
			}
		}
		InOutEnemyState.PersistentTargetUnitId = NAME_None;
		InOutEnemyState.PersistentTargetStatus = static_cast<uint8>(EGameXXKCardStatus::None);
	}

	bool AssignPersistentTarget(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName SourceUnitId,
		const FName TargetUnitId,
		const EGameXXKCardStatus Status,
		FString* OutError)
	{
		if (!IsConcretePersistentTargetStatus(Status))
		{
			return SetFailure(OutError, TEXT("A persistent enemy target requires a concrete combat status."));
		}
		FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRuntime.Units, SourceUnitId);
		FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
		FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(SourceUnitId);
		if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy
			|| !Target || !Target->bLiving || Target->Side != EGameXXKCardTargetSide::Party
			|| !EnemyState)
		{
			return SetFailure(OutError, TEXT("A persistent enemy target requires a living catalog source and living party target."));
		}

		ClearPersistentTarget(InOutRuntime, *EnemyState);
		if (GameXXKCardRules::AddCombatStatus(*Target, Status, 1) <= 0)
		{
			return SetFailure(OutError, TEXT("The persistent enemy target status could not be applied."));
		}
		if (!GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(InOutRuntime, *Target, OutError))
		{
			return false;
		}
		EnemyState->PersistentTargetUnitId = TargetUnitId;
		EnemyState->PersistentTargetStatus = static_cast<uint8>(Status);
		return true;
	}

	bool HasPhaseTwoPersistentTargetFallback(const FGameXXKEnemyDefinition& Definition)
	{
		for (const FGameXXKEnemyIntentDefinition& Intent : Definition.Intents)
		{
			if (Intent.Effects.ContainsByPredicate([](const FGameXXKEnemyIntentEffectDefinition& Effect)
			{
				return Effect.bPhaseTwoFallbackToLowestHealth
					&& Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
					&& Effect.Target == EGameXXKEnemyIntentTargetRule::PreyTarget;
			}))
			{
				return true;
			}
		}
		return false;
	}

	bool RetargetDefeatedPersistentTargets(FGameXXKCardBattleRuntime& InOutRuntime, FString* OutError)
	{
		for (const FGameXXKCardCombatUnit& Enemy : InOutRuntime.Units)
		{
			if (!Enemy.bLiving || Enemy.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(Enemy.UnitId);
			if (!EnemyState)
			{
				continue;
			}
			const EGameXXKCardStatus Status = static_cast<EGameXXKCardStatus>(EnemyState->PersistentTargetStatus);
			if (!IsConcretePersistentTargetStatus(Status))
			{
				ClearPersistentTarget(InOutRuntime, *EnemyState);
				continue;
			}
			if (FindLivingPersistentTarget(InOutRuntime, *EnemyState))
			{
				continue;
			}

			const FGameXXKEnemyDefinition* Definition = Enemy.EnemyDefinitionId.IsNone()
				? nullptr
				: FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
			const bool bCanRetarget = Definition
				&& EnemyState->bPhaseTwo
				&& HasPhaseTwoPersistentTargetFallback(*Definition);
			ClearPersistentTarget(InOutRuntime, *EnemyState);
			if (!bCanRetarget)
			{
				continue;
			}
			if (const FGameXXKCardCombatUnit* NextTarget = FindLowestLivingUnitForSide(
				InOutRuntime,
				EGameXXKCardTargetSide::Party))
			{
				if (!AssignPersistentTarget(InOutRuntime, Enemy.UnitId, NextTarget->UnitId, Status, OutError))
				{
					return false;
				}
			}
		}
		return true;
	}

	bool RefreshPersistentTargetReferencesForIntent(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardEnemyIntent& InOutIntent)
	{
		const FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(InOutIntent.SourceUnitId);
		if (!EnemyState)
		{
			return true;
		}
		for (FGameXXKResolvedEnemyIntentEffect& Effect : InOutIntent.Effects)
		{
			if (Effect.TargetRule != EGameXXKEnemyIntentTargetRule::PreyTarget)
			{
				continue;
			}
			Effect.TargetUnitIds.Reset();
			if (const FGameXXKCardCombatUnit* Target = FindLivingPersistentTarget(InOutRuntime, *EnemyState))
			{
				Effect.TargetUnitIds.Add(Target->UnitId);
				InOutIntent.SuggestedTargetUnitId = Target->UnitId;
				InOutIntent.TargetSlotNumber = FGameXXKBattlePresentation::GetSlotNumber(InOutRuntime, Target->UnitId);
			}
			else
			{
				InOutIntent.SuggestedTargetUnitId = NAME_None;
				InOutIntent.TargetSlotNumber = INDEX_NONE;
			}
		}
		return true;
	}

	TArray<FName> ResolveEnemyIntentTargets(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Source,
		const EGameXXKEnemyIntentTargetRule TargetRule)
	{
		TArray<FName> Result;
		const auto AppendLivingSide = [&Runtime, &Result](const EGameXXKCardTargetSide Side)
		{
			TArray<const FGameXXKCardCombatUnit*> Candidates;
			for (const FGameXXKCardCombatUnit& Candidate : Runtime.Units)
			{
				if (Candidate.bLiving && Candidate.Side == Side)
				{
					Candidates.Add(&Candidate);
				}
			}
			Candidates.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
			{
				return Left.StableSortOrder != Right.StableSortOrder
					? Left.StableSortOrder < Right.StableSortOrder
					: NameLess(Left.UnitId, Right.UnitId);
			});
			for (const FGameXXKCardCombatUnit* Candidate : Candidates)
			{
				Result.Add(Candidate->UnitId);
			}
		};

		switch (TargetRule)
		{
		case EGameXXKEnemyIntentTargetRule::None:
			break;
		case EGameXXKEnemyIntentTargetRule::Self:
			Result.Add(Source.UnitId);
			break;
		case EGameXXKEnemyIntentTargetRule::LowestHealthParty:
			if (const FGameXXKCardCombatUnit* Target = FindLowestLivingUnitForSide(Runtime, EGameXXKCardTargetSide::Party)) Result.Add(Target->UnitId);
			break;
		case EGameXXKEnemyIntentTargetRule::MarkedPartyElseRandom:
		{
			// A marked party member is the only thing that overrides an otherwise
			// random pick: monsters chase the mark, otherwise they spread damage
			// across the party with a stable per-round seed.
			if (const FGameXXKCardCombatUnit* MarkedTarget = FindLowestLivingUnitForSide(Runtime, EGameXXKCardTargetSide::Party, EGameXXKCardStatus::Mark))
			{
				Result.Add(MarkedTarget->UnitId);
				break;
			}
			AppendLivingSide(EGameXXKCardTargetSide::Party);
			if (Result.Num() > 1)
			{
				const uint32 Seed = FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed(Source.UnitId, Runtime.RoundNumber);
				const FName Selected = Result[Seed % static_cast<uint32>(Result.Num())];
				Result.Reset();
				Result.Add(Selected);
			}
			break;
		}
		case EGameXXKEnemyIntentTargetRule::RandomLivingParty:
		{
			AppendLivingSide(EGameXXKCardTargetSide::Party);
			if (Result.Num() > 1)
			{
				const uint32 Seed = FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed(Source.UnitId, Runtime.RoundNumber);
				const FName Selected = Result[Seed % static_cast<uint32>(Result.Num())];
				Result.Reset();
				Result.Add(Selected);
			}
			break;
		}
		case EGameXXKEnemyIntentTargetRule::AllLivingParty:
			AppendLivingSide(EGameXXKCardTargetSide::Party);
			break;
		case EGameXXKEnemyIntentTargetRule::AllEnemyAllies:
			AppendLivingSide(EGameXXKCardTargetSide::Enemy);
			break;
		case EGameXXKEnemyIntentTargetRule::LowestHealthEnemyAlly:
			if (const FGameXXKCardCombatUnit* Target = FindLowestLivingUnitForSide(Runtime, EGameXXKCardTargetSide::Enemy)) Result.Add(Target->UnitId);
			break;
		case EGameXXKEnemyIntentTargetRule::MarkedParty:
			if (const FGameXXKCardCombatUnit* MarkedTarget = FindLowestLivingUnitForSide(Runtime, EGameXXKCardTargetSide::Party, EGameXXKCardStatus::Mark)) Result.Add(MarkedTarget->UnitId);
			break;
		case EGameXXKEnemyIntentTargetRule::PreyTarget:
			if (const FGameXXKEnemyBattleState* EnemyState = Runtime.EnemyStates.Find(Source.UnitId))
			{
				if (const FGameXXKCardCombatUnit* PreyTarget = FindLivingPersistentTarget(Runtime, *EnemyState))
				{
					Result.Add(PreyTarget->UnitId);
				}
			}
			break;
		default:
			break;
		}
		return Result;
	}

	bool HasRequiredCatalogIntentTargetStatus(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Source,
		const FGameXXKEnemyIntentDefinition& Candidate)
	{
		if (Candidate.RequiredTargetStatus == EGameXXKCardStatus::None)
		{
			return true;
		}

		for (const FGameXXKEnemyIntentEffectDefinition& Effect : Candidate.Effects)
		{
			for (const FName TargetUnitId : ResolveEnemyIntentTargets(Runtime, Source, Effect.Target))
			{
				const FGameXXKCardCombatUnit* Target = FindCardUnit(Runtime.Units, TargetUnitId);
				if (Target
					&& Target->bLiving
					&& GameXXKCardRules::GetCombatStatusStacks(*Target, Candidate.RequiredTargetStatus) > 0)
				{
					return true;
				}
			}
		}
		return false;
	}

	int32 ComputeEnemyIntentMagnitude(const FGameXXKCardCombatUnit& Source, const FGameXXKEnemyIntentEffectDefinition& Effect)
	{
		const int64 Value = static_cast<int64>(Effect.FlatMagnitude)
			+ static_cast<int64>(FMath::Max(0, Source.Attack)) * Effect.AttackPercent / 100;
		return static_cast<int32>(FMath::Clamp<int64>(Value, 0, MAX_int32));
	}

	int32 RemovePositiveCombatStatusStacks(
		FGameXXKCardBattleRuntime& InOutRuntime,
		FGameXXKCardCombatUnit& InOutUnit,
		const int32 Maximum)
	{
		if (Maximum <= 0 || !InOutUnit.bLiving)
		{
			return 0;
		}

		// Intent resolution must be deterministic across save/reload. The list is deliberately stable
		// and only contains player-beneficial combat statuses; harmful states such as Poison, Bleed,
		// Weak, Mark, and Prey are never eligible for enemy "remove positive status" effects.
		constexpr EGameXXKCardStatus PositiveStatusPriority[] = {
			EGameXXKCardStatus::Momentum,
			EGameXXKCardStatus::Agility,
			EGameXXKCardStatus::CannotReceiveVulnerability,
			EGameXXKCardStatus::NextAttackBonus,
			EGameXXKCardStatus::NextAttackAppliesVulnerability,
			EGameXXKCardStatus::NextHealingBonus,
			EGameXXKCardStatus::TerrainBonusDouble,
			EGameXXKCardStatus::NextTerrainCardFree,
			EGameXXKCardStatus::NextTerrainCardEnergyReduction,
			EGameXXKCardStatus::RedirectSingleTargetEnemyAttack,
			EGameXXKCardStatus::TerrainBonusDoubleThisRound,
			EGameXXKCardStatus::Medicine,
			EGameXXKCardStatus::Wealth,
			EGameXXKCardStatus::Rage,
			EGameXXKCardStatus::Charge,
			EGameXXKCardStatus::Counter,
			EGameXXKCardStatus::Block};

		int32 Removed = 0;
		for (const EGameXXKCardStatus Status : PositiveStatusPriority)
		{
			const int32 RemovedHere = GameXXKCardRules::ConsumeCombatStatus(InOutUnit, Status, Maximum - Removed);
			if (RemovedHere > 0 && InOutUnit.Side == EGameXXKCardTargetSide::Party
				&& (Status == EGameXXKCardStatus::Counter || Status == EGameXXKCardStatus::Block))
			{
				int32 RecordsToRemove = RemovedHere;
				for (int32 ReactionIndex = 0; ReactionIndex < InOutRuntime.Reactions.Num() && RecordsToRemove > 0;)
				{
					const FGameXXKReactionRuntime& Reaction = InOutRuntime.Reactions[ReactionIndex];
					if (Reaction.RecipientUnitId == InOutUnit.UnitId && Reaction.Status == Status)
					{
						InOutRuntime.Reactions.RemoveAt(ReactionIndex, 1, EAllowShrinking::No);
						--RecordsToRemove;
						continue;
					}
					++ReactionIndex;
				}
			}
			Removed += RemovedHere;
			if (Removed >= Maximum)
			{
				break;
			}
		}
		return Removed;
	}

	bool BuildCatalogEnemyIntent(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardCombatUnit& Enemy,
		TMap<FName, FGameXXKEnemyBattleState>& InOutEnemyStates,
		const int32 ResolutionOrder,
		FGameXXKCardEnemyIntent& OutIntent,
		FString* OutError)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
		if (!Definition)
		{
			return false;
		}
		FGameXXKEnemyBattleState& EnemyState = InOutEnemyStates.FindOrAdd(Enemy.UnitId);
		if (EnemyState.DefinitionId.IsNone())
		{
			EnemyState.DefinitionId = Definition->Id;
		}
		if (EnemyState.DefinitionId != Definition->Id || Definition->Intents.IsEmpty())
		{
			return SetFailure(OutError, TEXT("Enemy catalog state does not match a valid intent definition."));
		}

		const FGameXXKEnemyIntentDefinition* Chosen = nullptr;
		bool bExecutingPendingCharge = false;
		bool bContinuingCharge = false;
		if (!EnemyState.PendingChargedIntentId.IsNone())
		{
			Chosen = Definition->Intents.FindByPredicate([&EnemyState](const FGameXXKEnemyIntentDefinition& Candidate)
			{
				return Candidate.Id == EnemyState.PendingChargedIntentId;
			});
			if (!Chosen)
			{
				return SetFailure(OutError, TEXT("A saved enemy charge does not exist in its catalog definition."));
			}
			bContinuingCharge = EnemyState.ChargeRoundsRemaining > 0;
			bExecutingPendingCharge = !bContinuingCharge;
		}
		else for (int32 Offset = 0; Offset < Definition->Intents.Num(); ++Offset)
		{
			const FGameXXKEnemyIntentDefinition& Candidate = Definition->Intents[(EnemyState.IntentCursor + Offset) % Definition->Intents.Num()];
			const bool bBelowHalf = static_cast<int64>(Enemy.HP) * 2 <= Enemy.MaxHP;
			if ((Candidate.bPhaseTwoOnly && !EnemyState.bPhaseTwo)
				|| (Candidate.bRequiresSourceBelowHalf && !bBelowHalf)
				|| (Candidate.CooldownRounds > 0 && EnemyState.HealingCooldownRounds > 0)
				|| !HasRequiredCatalogIntentTargetStatus(Runtime, Enemy, Candidate))
			{
				continue;
			}
			Chosen = &Candidate;
			break;
		}
		if (!Chosen)
		{
			return SetFailure(OutError, TEXT("No catalog enemy intent is currently eligible."));
		}

		OutIntent = FGameXXKCardEnemyIntent();
		OutIntent.CardId = FName(*FString::Printf(TEXT("Monster.Intent.%s.%s"), *Definition->Id.ToString(), *Chosen->Id.ToString()));
		OutIntent.CardDisplayName = Chosen->DisplayName.ToString();
		OutIntent.SourceUnitId = Enemy.UnitId;
		OutIntent.SourceSlotNumber = FGameXXKBattlePresentation::GetSlotNumber(Runtime, Enemy.UnitId);
		OutIntent.IntentDefinitionId = Chosen->Id;
		OutIntent.ResolutionOrder = ResolutionOrder;
		const bool bStartingCharge = !bExecutingPendingCharge && !bContinuingCharge && Chosen->ChargeRounds > 0;
		if (bStartingCharge)
		{
			EnemyState.PendingChargedIntentId = Chosen->Id;
			EnemyState.ChargeRoundsRemaining = Chosen->ChargeRounds;
			EnemyState.PendingChargeTargetUnitIds.Reset();
		}
		OutIntent.bCharging = bStartingCharge || bContinuingCharge;
		OutIntent.ChargeRounds = Chosen->ChargeRounds;
		OutIntent.TooltipLines.Add(OutIntent.CardDisplayName);
		for (const FGameXXKEnemyIntentEffectDefinition& EffectDefinition : Chosen->Effects)
		{
			FGameXXKResolvedEnemyIntentEffect& Effect = OutIntent.Effects.AddDefaulted_GetRef();
			Effect.Type = EffectDefinition.Type;
			Effect.TargetRule = EffectDefinition.Target;
			Effect.bAssignsPersistentTarget = EffectDefinition.bAssignsPersistentTarget;
			Effect.bPhaseTwoFallbackToLowestHealth = EffectDefinition.bPhaseTwoFallbackToLowestHealth;
			Effect.bClearsPersistentTargetAfterResolve = EffectDefinition.bClearsPersistentTargetAfterResolve;
			Effect.TargetUnitIds = ResolveEnemyIntentTargets(Runtime, Enemy, EffectDefinition.Target);
			if (bExecutingPendingCharge
				&& EffectDefinition.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& !EnemyState.PendingChargeTargetUnitIds.IsEmpty())
			{
				Effect.TargetUnitIds = EnemyState.PendingChargeTargetUnitIds;
			}
			else if (bStartingCharge
				&& EffectDefinition.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& EnemyState.PendingChargeTargetUnitIds.IsEmpty())
			{
				EnemyState.PendingChargeTargetUnitIds = Effect.TargetUnitIds;
			}
			Effect.BaseMagnitude = ComputeEnemyIntentMagnitude(Enemy, EffectDefinition);
			Effect.Magnitude = Effect.BaseMagnitude;
			Effect.ConsumedStatus = EffectDefinition.ConsumedStatus;
			Effect.MagnitudePerConsumedStack = FMath::Max(0, EffectDefinition.MagnitudePerConsumedStack);
			Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent = EffectDefinition.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent;
			if (Effect.ConsumedStatus != EGameXXKCardStatus::None
				&& EffectDefinition.MaxConsumedStacks > 0
				&& Effect.MagnitudePerConsumedStack > 0)
			{
				Effect.ConsumedStacks = FMath::Min(
					FMath::Max(0, EffectDefinition.MaxConsumedStacks),
					GameXXKCardRules::GetCombatStatusStacks(Enemy, Effect.ConsumedStatus));
				if (Effect.ConsumedStacks > 0)
				{
					int64 ConsumedMagnitude = static_cast<int64>(Effect.MagnitudePerConsumedStack) * Effect.ConsumedStacks;
					if (Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent && Effect.TargetUnitIds.Num() > 0)
					{
					if (const FGameXXKCardCombatUnit* Target = FindCardUnit(Runtime.Units, Effect.TargetUnitIds[0]))
					{
						const int64 PerStackMagnitude = static_cast<int64>(Target->MaxHP)
							* Effect.MagnitudePerConsumedStack / 100;
						ConsumedMagnitude = PerStackMagnitude * Effect.ConsumedStacks;
					}
					}
					Effect.Magnitude = static_cast<int32>(FMath::Clamp<int64>(
						static_cast<int64>(Effect.BaseMagnitude) + ConsumedMagnitude,
						0,
						MAX_int32));
				}
			}
			if (EffectDefinition.SourceStatusForFlatMagnitude != EGameXXKCardStatus::None
				&& EffectDefinition.FlatMagnitudePerSourceStatusStack > 0)
			{
				const int32 SourceStatusStacks = GameXXKCardRules::GetCombatStatusStacks(
					Enemy,
					EffectDefinition.SourceStatusForFlatMagnitude);
				Effect.Magnitude = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(Effect.Magnitude)
						+ static_cast<int64>(EffectDefinition.FlatMagnitudePerSourceStatusStack) * SourceStatusStacks,
					0,
					MAX_int32));
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& EnemyState.bPhaseTwo
				&& Definition->PhaseTwoDirectDamagePercent != 100)
			{
				Effect.Magnitude = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(Effect.Magnitude) * Definition->PhaseTwoDirectDamagePercent / 100,
					0,
					MAX_int32));
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& EnemyState.bPhaseTwo
				&& Chosen->PhaseTwoDirectDamagePercent != 100)
			{
				Effect.Magnitude = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(Effect.Magnitude) * Chosen->PhaseTwoDirectDamagePercent / 100,
					0,
					MAX_int32));
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& Definition->PassiveId == EGameXXKEnemyPassiveId::RedtuskRage
				&& Chosen->Id == TEXT("RageStrike"))
			{
				const int32 RageStacks = GameXXKCardRules::GetCombatStatusStacks(Enemy, EGameXXKCardStatus::Rage);
				Effect.Magnitude = static_cast<int32>(FMath::Min<int64>(
					MAX_int32,
					static_cast<int64>(Effect.Magnitude) + static_cast<int64>(FMath::Max(0, RageStacks)) * 20));
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& Definition->PassiveId == EGameXXKEnemyPassiveId::GraymaneMarkedHunt
				&& Effect.TargetUnitIds.Num() == 1)
			{
				const FGameXXKCardCombatUnit* Target = FindCardUnit(Runtime.Units, Effect.TargetUnitIds[0]);
				if (Target
					&& Target->bLiving
					&& Target->Side == EGameXXKCardTargetSide::Party
					&& GameXXKCardRules::GetCombatStatusStacks(*Target, EGameXXKCardStatus::Mark) > 0)
				{
					Effect.Magnitude = static_cast<int32>(FMath::Min<int64>(
						MAX_int32,
						static_cast<int64>(Effect.Magnitude) * 120 / 100));
				}
			}
			Effect.HitCount = FMath::Max(1, EffectDefinition.HitCount);
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				&& EnemyState.bPhaseTwo
				&& Definition->PhaseTwoAdditionalHitIntentIds.Contains(Chosen->Id))
			{
				Effect.HitCount = FMath::Min(MAX_int32, Effect.HitCount + 1);
			}
			Effect.Status = EffectDefinition.Status;
			Effect.StatusStacks = FMath::Max(0, EffectDefinition.StatusStacks);
			if (OutIntent.TargetRule == EGameXXKEnemyIntentTargetRule::None)
			{
				OutIntent.TargetRule = EffectDefinition.Target;
				OutIntent.SuggestedTargetUnitId = Effect.TargetUnitIds.IsEmpty() ? NAME_None : Effect.TargetUnitIds[0];
				const FGameXXKCardCombatUnit* Target = FindCardUnit(Runtime.Units, OutIntent.SuggestedTargetUnitId);
				OutIntent.TargetSlotNumber = Target ? FGameXXKBattlePresentation::GetSlotNumber(Runtime, Target->UnitId) : INDEX_NONE;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage && OutIntent.Damage == 0)
			{
				OutIntent.Damage = Effect.Magnitude;
				OutIntent.Kind = Effect.TargetUnitIds.Num() > 1 ? EGameXXKCardDamageKind::GroupAttack : EGameXXKCardDamageKind::SingleTargetAttack;
				if (Effect.Status != EGameXXKCardStatus::None && Effect.StatusStacks > 0)
				{
					OutIntent.OnHitStatuses.Add({Effect.Status, Effect.StatusStacks});
				}
			}
			OutIntent.TooltipLines.Add(FString::Printf(TEXT("效果 %d：%d"), static_cast<int32>(Effect.Type), Effect.Magnitude));
		}
		return OutIntent.SourceSlotNumber != INDEX_NONE;
	}

	bool GetNextCatalogIntentCursor(
		const FGameXXKCardBattleRuntime& Runtime,
		const FGameXXKCardEnemyIntent& Intent,
	int32& OutNextCursor,
	FString* OutError)
	{
		OutNextCursor = INDEX_NONE;
		if (Intent.bCharging)
		{
			return true;
		}
		const FGameXXKCardCombatUnit* Source = FindCardUnit(Runtime.Units, Intent.SourceUnitId);
		if (!Source || Source->EnemyDefinitionId.IsNone() || Intent.IntentDefinitionId.IsNone())
		{
			return true;
		}
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Source->EnemyDefinitionId);
		const FGameXXKEnemyBattleState* EnemyState = Runtime.EnemyStates.Find(Intent.SourceUnitId);
		if (!Definition || !EnemyState || EnemyState->DefinitionId != Definition->Id || Definition->Intents.IsEmpty())
		{
			return SetFailure(OutError, TEXT("A catalog enemy intent cannot advance without matching persisted enemy state."));
		}
		const int32 ResolvedIntentIndex = Definition->Intents.IndexOfByPredicate([&Intent](const FGameXXKEnemyIntentDefinition& Candidate)
		{
			return Candidate.Id == Intent.IntentDefinitionId;
		});
		if (ResolvedIntentIndex == INDEX_NONE)
		{
			return SetFailure(OutError, TEXT("The saved catalog enemy intent does not exist in its source definition."));
		}
		OutNextCursor = (ResolvedIntentIndex + 1) % Definition->Intents.Num();
		return true;
	}

	bool StartSpiralHornDeerHealingCooldown(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardEnemyIntent& Intent,
		FString* OutError)
	{
		if (Intent.IntentDefinitionId != SpiralHornDeerSpringHealIntentId
			|| !Intent.Effects.ContainsByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
			{
				return Effect.Type == EGameXXKEnemyIntentEffectType::Heal;
			}))
		{
			return true;
		}

		const FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRuntime.Units, Intent.SourceUnitId);
		if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy || Source->EnemyDefinitionId.IsNone())
		{
			return true;
		}
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Source->EnemyDefinitionId);
		if (!Definition || Definition->PassiveId != EGameXXKEnemyPassiveId::DeerHealCooldown)
		{
			return true;
		}
		const FGameXXKEnemyIntentDefinition* SpringHealDefinition = Definition->Intents.FindByPredicate([](const FGameXXKEnemyIntentDefinition& Candidate)
		{
			return Candidate.Id == SpiralHornDeerSpringHealIntentId;
		});
		if (!SpringHealDefinition
			|| SpringHealDefinition->CooldownRounds <= 0
			|| !SpringHealDefinition->Effects.ContainsByPredicate([](const FGameXXKEnemyIntentEffectDefinition& Effect)
			{
				return Effect.Type == EGameXXKEnemyIntentEffectType::Heal;
			}))
		{
			return SetFailure(OutError, TEXT("Spiral Horn Deer Spring Heal is missing its catalog cooldown definition."));
		}

		FGameXXKEnemyBattleState NewEnemyState;
		if (const FGameXXKEnemyBattleState* ExistingEnemyState = InOutRuntime.EnemyStates.Find(Source->UnitId))
		{
			NewEnemyState = *ExistingEnemyState;
		}
		if (NewEnemyState.DefinitionId.IsNone())
		{
			NewEnemyState.DefinitionId = Definition->Id;
		}
		if (NewEnemyState.DefinitionId != Definition->Id)
		{
			return SetFailure(OutError, TEXT("Spiral Horn Deer Spring Heal found a mismatched persisted enemy definition."));
		}

		NewEnemyState.HealingCooldownRounds = SpringHealDefinition->CooldownRounds;
		NewEnemyState.bHealingCooldownStartedThisEnemyPhase = true;
		InOutRuntime.EnemyStates.Add(Source->UnitId, MoveTemp(NewEnemyState));
		return true;
	}

	void ApplyEnemyDamagingStatusHealing(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FName SourceUnitId,
		const FGameXXKCardDamageResult& DamageResult)
	{
		if (DamageResult.HealthDamage <= 0)
		{
			return;
		}
		FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRuntime.Units, SourceUnitId);
		const FGameXXKCardCombatUnit* DamagedTarget = FindCardUnit(
			InOutRuntime.Units,
			DamageResult.ResolvedTargetUnitId);
		if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy
			|| !DamagedTarget || DamagedTarget->Side != EGameXXKCardTargetSide::Party
			|| Source->EnemyDefinitionId.IsNone())
		{
			return;
		}
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Source->EnemyDefinitionId);
		if (!Definition
			|| Definition->HealOnDamagingTargetStatus == EGameXXKCardStatus::None
			|| Definition->HealMissingHealthPercentOnDamagingTargetStatus <= 0
			|| GameXXKCardRules::GetCombatStatusStacks(*DamagedTarget, Definition->HealOnDamagingTargetStatus) <= 0)
		{
			return;
		}
		const int32 MissingHealth = FMath::Max(0, Source->MaxHP - Source->HP);
		const int32 Healing = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(MissingHealth) * Definition->HealMissingHealthPercentOnDamagingTargetStatus / 100,
			0,
			MAX_int32));
		GameXXKCardRules::HealCombatUnit(*Source, Healing);
	}

	void ClearExpiredPersistentTargetAfterIntent(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardEnemyIntent& Intent)
	{
		if (!Intent.Effects.ContainsByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.bClearsPersistentTargetAfterResolve;
		}))
		{
			return;
		}
		if (FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(Intent.SourceUnitId);
			EnemyState && !EnemyState->bPhaseTwo)
		{
			ClearPersistentTarget(InOutRuntime, *EnemyState);
		}
	}

	bool ResolveCatalogIntentEffects(
		FGameXXKCardBattleRuntime& InOutRuntime,
		const FGameXXKCardEnemyIntent& Intent,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		FString* OutError)
	{
		for (const FGameXXKResolvedEnemyIntentEffect& Effect : Intent.Effects)
		{
			if (Effect.Type == EGameXXKEnemyIntentEffectType::AddArmor)
			{
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					GameXXKCardRules::AddCombatArmor(*Target, Effect.Magnitude);
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::ApplyStatus)
			{
				if (Effect.bAssignsPersistentTarget)
				{
					TArray<FName> ResolvedTargetUnitIds;
					for (const FName TargetUnitId : Effect.TargetUnitIds)
					{
						const FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
						if (Target && Target->bLiving && Target->Side == EGameXXKCardTargetSide::Party)
						{
							ResolvedTargetUnitIds.Add(TargetUnitId);
						}
					}
					if (ResolvedTargetUnitIds.IsEmpty())
					{
						if (const FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRuntime.Units, Intent.SourceUnitId))
						{
							ResolvedTargetUnitIds = ResolveEnemyIntentTargets(InOutRuntime, *Source, Effect.TargetRule);
						}
					}
					for (const FName TargetUnitId : ResolvedTargetUnitIds)
					{
						if (!AssignPersistentTarget(
							InOutRuntime,
							Intent.SourceUnitId,
							TargetUnitId,
							Effect.Status,
							OutError))
						{
							return false;
						}
					}
					continue;
				}
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					if (GameXXKCardRules::AddCombatStatus(*Target, Effect.Status, Effect.StatusStacks) > 0
						&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(InOutRuntime, *Target, OutError))
					{
						return false;
					}
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::Heal)
			{
				const bool bUsesConsumedStacks = Effect.ConsumedStatus != EGameXXKCardStatus::None
					&& Effect.ConsumedStacks > 0
					&& Effect.MagnitudePerConsumedStack > 0;
				int32 ActuallyConsumedStacks = 0;
				if (bUsesConsumedStacks)
				{
					FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRuntime.Units, Intent.SourceUnitId);
					if (!Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy)
					{
						return SetFailure(OutError, TEXT("A consumed-status heal lost its living enemy source."));
					}
					ActuallyConsumedStacks = GameXXKCardRules::ConsumeCombatStatus(
						*Source,
						Effect.ConsumedStatus,
						Effect.ConsumedStacks);
				}
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					int64 HealingMagnitude = bUsesConsumedStacks ? Effect.BaseMagnitude : Effect.Magnitude;
					if (bUsesConsumedStacks)
					{
						int64 ConsumedMagnitude = static_cast<int64>(Effect.MagnitudePerConsumedStack) * ActuallyConsumedStacks;
						if (Effect.bMagnitudePerConsumedStackUsesTargetMaxHealthPercent)
						{
							const int64 PerStackMagnitude = static_cast<int64>(Target->MaxHP)
								* Effect.MagnitudePerConsumedStack / 100;
							ConsumedMagnitude = PerStackMagnitude * ActuallyConsumedStacks;
						}
						HealingMagnitude += ConsumedMagnitude;
					}
					GameXXKCardRules::HealCombatUnit(*Target, static_cast<int32>(FMath::Clamp<int64>(HealingMagnitude, 0, MAX_int32)));
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::ConsumeSharedQi)
			{
				GameXXKCardRules::ConsumeSharedCombatEnergy(InOutRuntime, Effect.Magnitude);
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy)
			{
				if (!GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(
					InOutRuntime,
					Effect.Magnitude,
					Intent.SourceUnitId,
					OutError))
				{
					return false;
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::ModifyAttack)
			{
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					const int32 OriginalAttack = FMath::Max(0, Target->Attack);
					Target->Attack = static_cast<int32>(FMath::Clamp<int64>(
						static_cast<int64>(Target->Attack) + Effect.Magnitude,
						0,
						MAX_int32));
					if (Target->Side == EGameXXKCardTargetSide::Enemy)
					{
						FGameXXKEnemyBattleState& TargetState = InOutRuntime.EnemyStates.FindOrAdd(TargetUnitId);
						TargetState.TemporaryAttackModifier = static_cast<int32>(FMath::Clamp<int64>(
							static_cast<int64>(TargetState.TemporaryAttackModifier) + (Target->Attack - OriginalAttack),
							MIN_int32,
							MAX_int32));
					}
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::ModifySpeed)
			{
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					const int32 OriginalSpeed = FMath::Max(0, Target->Speed);
					Target->Speed = static_cast<int32>(FMath::Clamp<int64>(
						static_cast<int64>(Target->Speed) + Effect.Magnitude,
						0,
						MAX_int32));
					if (Target->Side == EGameXXKCardTargetSide::Enemy)
					{
						const int32 AppliedSpeed = Target->Speed - OriginalSpeed;
						FGameXXKEnemyBattleState& TargetState = InOutRuntime.EnemyStates.FindOrAdd(TargetUnitId);
						TargetState.TemporarySpeedModifier = static_cast<int32>(FMath::Clamp<int64>(
							static_cast<int64>(TargetState.TemporarySpeedModifier) + AppliedSpeed,
							MIN_int32,
							MAX_int32));
						TargetState.PendingNextEnemyPhaseSpeedModifier = static_cast<int32>(FMath::Clamp<int64>(
							static_cast<int64>(TargetState.PendingNextEnemyPhaseSpeedModifier) + AppliedSpeed,
							MIN_int32,
							MAX_int32));
					}
				}
				continue;
			}
			if (Effect.Type == EGameXXKEnemyIntentEffectType::RemovePositiveStatus)
			{
				for (const FName TargetUnitId : Effect.TargetUnitIds)
				{
					FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!Target || !Target->bLiving)
					{
						continue;
					}
					RemovePositiveCombatStatusStacks(InOutRuntime, *Target, Effect.Magnitude);
				}
				continue;
			}
			if (Effect.Type != EGameXXKEnemyIntentEffectType::DirectDamage)
			{
				continue;
			}
			for (const FName TargetUnitId : Effect.TargetUnitIds)
			{
				const FGameXXKCardCombatUnit* Target = FindCardUnit(InOutRuntime.Units, TargetUnitId);
				if (!Target || !Target->bLiving || Target->Side != EGameXXKCardTargetSide::Party)
				{
					continue;
				}
				FGameXXKCardDamageContext Context;
				Context.SourceUnitId = Intent.SourceUnitId;
				Context.Kind = Effect.TargetUnitIds.Num() > 1
					? EGameXXKCardDamageKind::GroupAttack
					: EGameXXKCardDamageKind::SingleTargetAttack;
				if (Effect.Status != EGameXXKCardStatus::None && Effect.StatusStacks > 0)
				{
					Context.OnHitStatuses.Add({Effect.Status, Effect.StatusStacks});
				}
				for (int32 HitIndex = 0; HitIndex < FMath::Max(1, Effect.HitCount); ++HitIndex)
				{
					FGameXXKCardDamageResult DamageResult;
					TArray<FGameXXKCardDamageResult> ReactiveResults;
					if (!GameXXKCardRules::ResolveEnemyDirectAttack(
						InOutRuntime,
						Context,
						TargetUnitId,
						Effect.Magnitude,
						DamageResult,
						&ReactiveResults,
						OutError,
						true))
					{
						return false;
					}
					ApplyEnemyDamagingStatusHealing(InOutRuntime, Intent.SourceUnitId, DamageResult);
					OutDamageResults.Add(MoveTemp(DamageResult));
					OutDamageResults.Append(MoveTemp(ReactiveResults));
					const FGameXXKCardCombatUnit* UpdatedTarget = FindCardUnit(InOutRuntime.Units, TargetUnitId);
					if (!UpdatedTarget || !UpdatedTarget->bLiving)
					{
						break;
					}
				}
			}
		}
		ClearExpiredPersistentTargetAfterIntent(InOutRuntime, Intent);
		return true;
	}

	void ExpireEnemyPhaseTemporaryModifiers(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(Unit.UnitId);
			if (!EnemyState)
			{
				continue;
			}
			if (EnemyState->TemporaryAttackModifier != 0)
			{
				Unit.Attack = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(Unit.Attack) - EnemyState->TemporaryAttackModifier,
					0,
					MAX_int32));
				EnemyState->TemporaryAttackModifier = 0;
			}
			if (EnemyState->SpeedModifierExpiringAfterCurrentEnemyPhase != 0)
			{
				const int32 ExpiringSpeed = EnemyState->SpeedModifierExpiringAfterCurrentEnemyPhase;
				Unit.Speed = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(Unit.Speed) - ExpiringSpeed,
					0,
					MAX_int32));
				EnemyState->TemporarySpeedModifier = static_cast<int32>(FMath::Clamp<int64>(
					static_cast<int64>(EnemyState->TemporarySpeedModifier) - ExpiringSpeed,
					MIN_int32,
					MAX_int32));
			}
			EnemyState->SpeedModifierExpiringAfterCurrentEnemyPhase = EnemyState->PendingNextEnemyPhaseSpeedModifier;
			EnemyState->PendingNextEnemyPhaseSpeedModifier = 0;
		}
	}

	void AdvancePendingEnemyCharges(FGameXXKCardBattleRuntime& InOutRuntime)
	{
		for (const FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy)
			{
				continue;
			}
			FGameXXKEnemyBattleState* EnemyState = InOutRuntime.EnemyStates.Find(Unit.UnitId);
			if (!EnemyState)
			{
				continue;
			}
			if (!EnemyState->PendingChargedIntentId.IsNone() && EnemyState->ChargeRoundsRemaining > 0)
			{
				--EnemyState->ChargeRoundsRemaining;
			}
		}
	}

	bool AdvanceSpiralHornDeerHealingCooldowns(FGameXXKCardBattleRuntime& InOutRuntime, FString* OutError)
	{
		TMap<FName, FGameXXKEnemyBattleState> NewEnemyStates = InOutRuntime.EnemyStates;
		for (const FGameXXKCardCombatUnit& Unit : InOutRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy || Unit.EnemyDefinitionId.IsNone())
			{
				continue;
			}
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Unit.EnemyDefinitionId);
			if (!Definition || Definition->PassiveId != EGameXXKEnemyPassiveId::DeerHealCooldown)
			{
				continue;
			}

			FGameXXKEnemyBattleState& EnemyState = NewEnemyStates.FindOrAdd(Unit.UnitId);
			if (EnemyState.DefinitionId.IsNone())
			{
				EnemyState.DefinitionId = Definition->Id;
			}
			if (EnemyState.DefinitionId != Definition->Id)
			{
				return SetFailure(OutError, TEXT("Spiral Horn Deer cooldown advance found a mismatched persisted enemy definition."));
			}
			if (EnemyState.bHealingCooldownStartedThisEnemyPhase)
			{
				EnemyState.bHealingCooldownStartedThisEnemyPhase = false;
			}
			else if (EnemyState.HealingCooldownRounds > 0)
			{
				EnemyState.HealingCooldownRounds = FMath::Max(0, EnemyState.HealingCooldownRounds - 1);
			}
		}
		InOutRuntime.EnemyStates = MoveTemp(NewEnemyStates);
		return true;
	}

	bool ApplyCatalogEnemyRoundStartStatuses(FGameXXKCardBattleRuntime& InOutRuntime, FString* OutError)
	{
		FGameXXKCardBattleRuntime NewRuntime = InOutRuntime;
		for (FGameXXKCardCombatUnit& Unit : NewRuntime.Units)
		{
			if (!Unit.bLiving || Unit.Side != EGameXXKCardTargetSide::Enemy || Unit.EnemyDefinitionId.IsNone())
			{
				continue;
			}
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Unit.EnemyDefinitionId);
			if (!Definition || Definition->RoundStartStatus == EGameXXKCardStatus::None)
			{
				continue;
			}

			FGameXXKEnemyBattleState& EnemyState = NewRuntime.EnemyStates.FindOrAdd(Unit.UnitId);
			if (EnemyState.DefinitionId.IsNone())
			{
				EnemyState.DefinitionId = Definition->Id;
			}
			if (EnemyState.DefinitionId != Definition->Id)
			{
				return SetFailure(OutError, TEXT("Enemy round-start status found a mismatched persisted enemy definition."));
			}

			const int32 StatusStacks = EnemyState.bPhaseTwo && Definition->PhaseTwoRoundStartStatusStacks > 0
				? Definition->PhaseTwoRoundStartStatusStacks
				: Definition->RoundStartStatusStacks;
			if (StatusStacks <= 0)
			{
				continue;
			}
			if (GameXXKCardRules::AddCombatStatus(Unit, Definition->RoundStartStatus, StatusStacks) > 0
				&& !GameXXKCardRules::ResolveWhiteApeStatusGuardAfterStatusApplied(NewRuntime, Unit, OutError))
			{
				return false;
			}
		}
		InOutRuntime = MoveTemp(NewRuntime);
		return true;
	}

	bool BuildEnemyIntents(FGameXXKCardRunState& InOutRun, FString* OutError)
	{
		FGameXXKCardBattleRuntime NewRuntime = InOutRun.ActiveBattle;
		if (NewRuntime.Phase != EGameXXKCardBattlePhase::Player
			&& NewRuntime.Phase != EGameXXKCardBattlePhase::Enemy)
		{
			return SetFailure(OutError, TEXT("Enemy intents can only be forecast during a player or enemy card phase."));
		}
		if (!RetargetDefeatedPersistentTargets(NewRuntime, OutError)
			|| !ValidateLivingEnemyIntentPresentation(NewRuntime, OutError))
		{
			return false;
		}

		// Build into a temporary array so a rejected presentation slot never clears a saved forecast.
		TArray<FGameXXKCardEnemyIntent> NewEnemyIntents;
		TArray<const FGameXXKCardCombatUnit*> Enemies;
		for (const FGameXXKCardCombatUnit& Unit : NewRuntime.Units)
		{
			if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Enemies.Add(&Unit);
			}
		}
		Enemies.Sort([&NewRuntime](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			if (Left.Speed != Right.Speed)
			{
				return Left.Speed > Right.Speed;
			}
			const int32 LeftSlot = FGameXXKBattlePresentation::GetSlotNumber(NewRuntime, Left.UnitId);
			const int32 RightSlot = FGameXXKBattlePresentation::GetSlotNumber(NewRuntime, Right.UnitId);
			if (LeftSlot != RightSlot)
			{
				return LeftSlot < RightSlot;
			}
			return NameLess(Left.UnitId, Right.UnitId);
		});
		TMap<FName, FGameXXKEnemyBattleState> NewEnemyStates = NewRuntime.EnemyStates;
		for (const FGameXXKCardCombatUnit* Enemy : Enemies)
		{
			FGameXXKCardEnemyIntent Intent;
			if (!Enemy->EnemyDefinitionId.IsNone())
			{
				if (!BuildCatalogEnemyIntent(
					NewRuntime,
					*Enemy,
					NewEnemyStates,
					NewEnemyIntents.Num(),
					Intent,
					OutError))
				{
					return false;
				}
				NewEnemyIntents.Add(MoveTemp(Intent));
				continue;
			}

			const FGameXXKCardCombatUnit* Target = FindLowestLivingPartyUnit(NewRuntime);
			if (!Target)
			{
				break;
			}
			Intent.CardId = FName(*FString::Printf(TEXT("Monster.Intent.%s"), *Enemy->UnitId.ToString()));
			Intent.CardDisplayName = TEXT("攻击");
			Intent.SourceUnitId = Enemy->UnitId;
			Intent.SuggestedTargetUnitId = Target->UnitId;
			Intent.SourceSlotNumber = FGameXXKBattlePresentation::GetSlotNumber(NewRuntime, Enemy->UnitId);
			Intent.TargetSlotNumber = FGameXXKBattlePresentation::GetSlotNumber(NewRuntime, Target->UnitId);
			if (Intent.SourceSlotNumber == INDEX_NONE)
			{
				return SetFailure(OutError, TEXT("Living enemy intents require a supported enemy presentation slot."));
			}
			Intent.Damage = FMath::Max(1, Enemy->Attack);
			Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
			Intent.ResolutionOrder = NewEnemyIntents.Num();
			NewEnemyIntents.Add(MoveTemp(Intent));
		}
		NewRuntime.EnemyStates = MoveTemp(NewEnemyStates);
		InOutRun.ActiveBattle = MoveTemp(NewRuntime);
		InOutRun.EnemyIntents = MoveTemp(NewEnemyIntents);
		InOutRun.NextEnemyIntentIndex = 0;
		return true;
	}

	void PruneUnexecutableEnemyIntents(FGameXXKCardRunState& InOutRun)
	{
		InOutRun.EnemyIntents.RemoveAll([&InOutRun](const FGameXXKCardEnemyIntent& Intent)
		{
			const FGameXXKCardCombatUnit* Source = FindCardUnit(InOutRun.ActiveBattle.Units, Intent.SourceUnitId);
			return !Source || !Source->bLiving || Source->Side != EGameXXKCardTargetSide::Enemy;
		});
		// Forecasts always begin from the first card when the enemy presentation starts.
		InOutRun.NextEnemyIntentIndex = 0;
	}

	bool IsRewardNodeKind(const EGameXXKNodeKind NodeKind)
	{
		return NodeKind == EGameXXKNodeKind::Battle
			|| NodeKind == EGameXXKNodeKind::Elite
			|| NodeKind == EGameXXKNodeKind::Boss;
	}

	int32 GetActiveRewardSourceNodeId(const FGameXXKRuntimeState& State)
	{
		if (State.CardRun.ActiveBattleSourceNodeId >= 0)
		{
			return State.CardRun.ActiveBattleSourceNodeId;
		}
		return !State.bHasGeneratedRouteMap && State.DungeonNodeIndex >= 0
			? State.DungeonNodeIndex
			: INDEX_NONE;
	}

	bool ValidatePendingRouteRewardGate(
		const FGameXXKRuntimeState& State,
		const bool bRequirePendingReward,
		FString* OutError)
	{
		const FGameXXKCardRunState& Run = State.CardRun;
		if (!Run.bHasActiveCardBattle
			|| Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory
			|| Run.bActiveBattleRewardResolved
			|| GetActiveRewardSourceNodeId(State) < 0)
		{
			return SetFailure(OutError, TEXT("Route rewards require one unresolved active card-battle victory gate."));
		}

		const FGameXXKPendingRouteCardReward& Pending = Run.PendingReward;
		if (Pending.CardIds.IsEmpty() && Pending.Options.IsEmpty())
		{
			if (Pending.SourceNodeId != INDEX_NONE
				|| Pending.ChoiceSeed != 0)
			{
				return SetFailure(OutError, TEXT("An empty pending route reward retains inconsistent metadata."));
			}
			return bRequirePendingReward
				? SetFailure(OutError, TEXT("There is no pending route reward to preview or resolve."))
				: true;
		}

		if (Pending.Options.IsEmpty())
		{
			// Legacy three-route-card offer: card ids must be three distinct catalog route cards.
			if (Pending.SourceNodeId < 0
				|| Pending.SourceNodeId != GetActiveRewardSourceNodeId(State)
				|| Pending.ChoiceSeed == 0
				|| Pending.CardIds.Num() != 3)
			{
				return SetFailure(OutError, TEXT("The pending route reward metadata does not match the active victory gate."));
			}
			TSet<FName> SeenCardIds;
			for (const FName CardId : Pending.CardIds)
			{
				const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
				if (CardId.IsNone()
					|| SeenCardIds.Contains(CardId)
					|| !Definition
					|| !IsRouteCard(CardId))
				{
					return SetFailure(OutError, TEXT("The pending route reward must contain three distinct catalog route cards."));
				}
				SeenCardIds.Add(CardId);
			}
		}
		else
		{
			// Tiered battle reward: three typed options.
			if (Pending.SourceNodeId < 0
				|| Pending.SourceNodeId != GetActiveRewardSourceNodeId(State)
				|| Pending.ChoiceSeed == 0
				|| Pending.Options.Num() != 3)
			{
				return SetFailure(OutError, TEXT("The pending battle reward metadata does not match the active victory gate."));
			}
		}
		return true;
	}

	void AppendEligibleRouteCards(
		const FGameXXKCardRunState& Run,
		const TFunctionRef<bool(const FGameXXKCardDefinition&)>& Predicate,
		TArray<FName>& OutCards)
	{
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (IsRouteCard(Definition.Id)
				&& Predicate(Definition)
				&& !Run.BossCardSlots.Contains(Definition.Id))
			{
				OutCards.Add(Definition.Id);
			}
		}
		OutCards.Sort(NameLess);
	}

	bool AddDeterministicRewardFromPool(
		const TArray<FName>& Pool,
		uint32& InOutRandomState,
		TArray<FName>& InOutPickedIds)
	{
		TArray<FName> Available;
		for (const FName Candidate : Pool)
		{
			if (!InOutPickedIds.Contains(Candidate))
			{
				Available.Add(Candidate);
			}
		}
		if (Available.IsEmpty())
		{
			return false;
		}
		const int32 PickedIndex = static_cast<int32>(NextRandom(InOutRandomState) % static_cast<uint32>(Available.Num()));
		InOutPickedIds.Add(Available[PickedIndex]);
		return true;
	}

	bool DidAnyBossEnterPhaseTwo(
		const FGameXXKCardBattleRuntime& Before,
		const FGameXXKCardBattleRuntime& After)
	{
		for (const TPair<FName, FGameXXKEnemyBattleState>& Pair : After.EnemyStates)
		{
			const FGameXXKEnemyBattleState* BeforeState = Before.EnemyStates.Find(Pair.Key);
			if (Pair.Value.bPhaseTwo && (!BeforeState || !BeforeState->bPhaseTwo))
			{
				return true;
			}
		}
		return false;
	}
}

int32 FGameXXKCardBattleAdapter::MixBattleSeed(const int32 BaseSeed, const int32 NodeId)
{
	constexpr int64 BattleNodeSeedMultiplier = 486187739;
	return BaseSeed ^ static_cast<int32>(static_cast<int64>(NodeId) * BattleNodeSeedMultiplier);
}

uint32 FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed(const FName SourceUnitId, const int32 RoundNumber)
{
	// Stable lexical source + round mix, then a Murmur-style finalizer so the
	// low bits (used for modulo target picks) look random across rounds instead
	// of alternating mechanically when exactly two party members are alive.
	uint32 Seed = FCrc::StrCrc32(*SourceUnitId.ToString())
		^ (static_cast<uint32>(RoundNumber) * 2654435761U);
	Seed ^= Seed >> 16;
	Seed *= 0x85ebca6bU;
	Seed ^= Seed >> 13;
	Seed *= 0xc2b2ae35U;
	Seed ^= Seed >> 16;
	return Seed;
}

bool FGameXXKCardBattleAdapter::EnsureCardRunInitialized(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	const TArray<FName> AllowedHeroCardIds = FGameXXKCardCatalog::GetHeroCardIdsUnlockedAtLevel(InOutState.PlayerLevel);
	if (AllowedHeroCardIds.Num() < HeroSelectedCardCount)
	{
		return SetFailure(OutError, TEXT("The protagonist card catalog does not expose eight cards at the current level."));
	}

	// Unlock authority is derived only from hero level and canonical catalog order. This also
	// removes unknown, duplicated, or now-level-gated IDs from older and manually edited saves.
	Run.HeroUnlockedCardIds = AllowedHeroCardIds;
	TArray<FName> RepairedSelection;
	RepairedSelection.Reserve(HeroSelectedCardCount);
	for (const FName SavedCardId : Run.HeroSelectedCardIds)
	{
		if (AllowedHeroCardIds.Contains(SavedCardId) && !RepairedSelection.Contains(SavedCardId))
		{
			RepairedSelection.Add(SavedCardId);
			if (RepairedSelection.Num() == HeroSelectedCardCount)
			{
				break;
			}
		}
	}
	for (const FName AllowedCardId : AllowedHeroCardIds)
	{
		if (RepairedSelection.Num() == HeroSelectedCardCount)
		{
			break;
		}
		if (!RepairedSelection.Contains(AllowedCardId))
		{
			RepairedSelection.Add(AllowedCardId);
		}
	}
	if (RepairedSelection.Num() != HeroSelectedCardCount)
	{
		return SetFailure(OutError, TEXT("The protagonist card selection could not be repaired to eight unique unlocked cards."));
	}
	Run.HeroSelectedCardIds = MoveTemp(RepairedSelection);
	if (Run.RouteRandomSeed == 0)
	{
		Run.RouteRandomSeed = InOutState.RouteSeed != 0 ? InOutState.RouteSeed : 0x13579BDF;
	}
	if (!FGameXXKCompanionRules::NormalizeOwnedQuestNpcCardLoadouts(
			Run.PartySelection,
			Run.RouteRandomSeed,
			OutError)
		|| !SynchronizePartySelectionWithRoster(Run, OutError)
		|| !ValidateHeroLoadout(Run, OutError))
	{
		return false;
	}
	if (!Run.CompanionRoster.PermanentCompanions.IsEmpty()
		&& !FGameXXKPartyFormationRules::Normalize(InOutState, OutError))
	{
		return false;
	}
	return true;
}

bool FGameXXKCardBattleAdapter::SetHeroSelectedCards(
	FGameXXKRuntimeState& InOutState,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	if (!EnsureCardRunInitialized(InOutState, OutError))
	{
		return false;
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (Run.bLoadoutLockedForRoute || Run.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("Hero cards cannot change after entering the route."));
	}
	if (SelectedCardIds.Num() != HeroSelectedCardCount || !AreUniqueNonEmptyCardIds(SelectedCardIds))
	{
		return SetFailure(OutError, TEXT("Hero configuration requires exactly eight distinct cards."));
	}
	for (const FName CardId : SelectedCardIds)
	{
		if (!Run.HeroUnlockedCardIds.Contains(CardId) || !IsHeroCard(CardId))
		{
			return SetFailure(OutError, TEXT("Hero configuration contains a locked or non-hero card."));
		}
	}
	Run.HeroSelectedCardIds = SelectedCardIds;
	return true;
}

bool FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
	FGameXXKRuntimeState& InOutState,
	const FName QuestNpcId,
	const TArray<FName>& SelectedCardIds,
	FString* OutError)
{
	FGameXXKRuntimeState Candidate = InOutState;
	if (!EnsureCardRunInitialized(Candidate, OutError))
	{
		return false;
	}
	FGameXXKCardRunState& Run = Candidate.CardRun;
	if (Run.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("Task NPC cards cannot change during an active card battle."));
	}
	if (QuestNpcId.IsNone())
	{
		return SetFailure(OutError, TEXT("The permanent NPC formation slot cannot be empty."));
	}
	const FGameXXKQuestNpcDefinition* Definition =
		FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
	if (!Definition)
	{
		return SetFailure(OutError, TEXT("The selected NPC is not one of the six owned definitions."));
	}
	TArray<FName> EffectiveSelection = SelectedCardIds;
	if (EffectiveSelection.IsEmpty())
	{
		if (const FGameXXKQuestNpcOwnedCardLoadout* SavedLoadout =
				Run.PartySelection.QuestNpcCardLoadouts.Find(QuestNpcId);
			SavedLoadout && FGameXXKCompanionRules::ValidateQuestNpcCardSelection(
				QuestNpcId,
				SavedLoadout->SelectedCardIds))
		{
			EffectiveSelection = SavedLoadout->SelectedCardIds;
		}
		else
		{
			const int32 SelectionSeed = Run.RouteProgress.RootSeed != 0
				? Run.RouteProgress.RootSeed
				: (Candidate.RouteSeed != 0 ? Candidate.RouteSeed : Run.RouteRandomSeed);
			if (!FGameXXKCompanionRules::BuildQuestNpcRouteCardSelection(
				QuestNpcId,
				SelectionSeed,
				EffectiveSelection,
				OutError))
			{
				return false;
			}
		}
	}
	if (!FGameXXKCompanionRules::ValidateQuestNpcCardSelection(
			QuestNpcId,
			EffectiveSelection,
			OutError))
	{
		return false;
	}
	Run.PartySelection.QuestNpcCardLoadouts.FindOrAdd(QuestNpcId).SelectedCardIds =
		EffectiveSelection;
	if (!FGameXXKPartyFormationRules::SetQuestNpc(Candidate, QuestNpcId, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKCardBattleAdapter::BeginCardBattle(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKNodeKind NodeKind,
	const EGameXXKCardTerrain Terrain,
	const int32 InitialRandomSeed,
	FString* OutError,
	const int32 EnemyDifficultyDamagePercent)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("An existing active card battle must be resumed or cleared before another battle begins."));
	}
	if ((NodeKind != EGameXXKNodeKind::Battle && NodeKind != EGameXXKNodeKind::Elite && NodeKind != EGameXXKNodeKind::Boss)
		|| Terrain == EGameXXKCardTerrain::Invalid || !InOutState.bHasActiveBattle || InOutState.ActiveBattleEnemies.IsEmpty())
	{
		return SetFailure(OutError, TEXT("Card battle initialization requires an active battle node, a concrete terrain, and enemies."));
	}

	// Build and project a complete candidate state first. A rejected setup must not leave a partial
	// card battle, shuffled deck, or forecast in the save-authoritative runtime state.
	FGameXXKRuntimeState NewState = InOutState;
	if (!EnsureCardRunInitialized(NewState, OutError) || !BuildRoutePartyProjection(NewState, OutError))
	{
		return false;
	}

	FGameXXKCardRunState& Run = NewState.CardRun;
	TArray<FGameXXKCardCombatUnit> Units;
	TArray<FGameXXKCardInstance> Instances;
	if (!BuildCardCombatUnits(NewState, Units, OutError)
		|| !BuildStartingCardInstances(NewState, NewState.ActiveBattleNodeId, Instances, OutError))
	{
		return false;
	}
	// Permanent deck-card quality upgrades (tiered battle rewards) override base quality.
	for (FGameXXKCardInstance& Instance : Instances)
	{
		if (const EGameXXKCardQuality* Upgraded = Run.UpgradedCardQualities.Find(Instance.CardId))
		{
			Instance.CurrentQuality = *Upgraded;
		}
	}
	FGameXXKCardBattleRuntime NewRuntime;
	const int32 EffectiveSeed = InitialRandomSeed != 0
		? InitialRandomSeed
		: FGameXXKCardBattleAdapter::MixBattleSeed(Run.RouteRandomSeed, NewState.ActiveBattleNodeId);
	if (!GameXXKCardRules::InitializeCardBattleRuntime(
		NewRuntime,
		Instances,
		Units,
		Terrain,
		EffectiveSeed,
		OutError,
		EnemyDifficultyDamagePercent))
	{
		return false;
	}
	NewRuntime.SourceNodeKind = CardBattleNodeKind(NodeKind);
	FGameXXKTalentProjection TalentProjection;
	if (!FGameXXKTalentRules::BuildProjection(NewState.Talents, TalentProjection, OutError))
	{
		return false;
	}
	NewRuntime.TalentFinalDamagePercent = TalentProjection.RouteFinalDamagePercent;
	NewRuntime.TalentCriticalChancePercent = TalentProjection.CriticalChancePercent;
	NewRuntime.TalentCriticalDamagePercent = TalentProjection.CriticalDamagePercent;
	const bool bOwnsLifeSavingTalisman = Run.Relics.ContainsByPredicate([](const FGameXXKRelicInstance& Instance)
	{
		return Instance.RelicId == LifeSavingTalismanRelicId;
	});
	NewRuntime.bLifeSavingTalismanArmed = bOwnsLifeSavingTalisman;
	NewRuntime.bLifeSavingTalismanConsumptionPending = false;
	NewRuntime.LifeSavingTalismanHealingPercent = 0;
	if (bOwnsLifeSavingTalisman)
	{
		const FGameXXKRelicDefinition* Definition = FGameXXKRelicCatalog::FindDefinition(LifeSavingTalismanRelicId);
		if (!Definition
			|| Definition->Trigger != EGameXXKRelicTrigger::DamageTaken
			|| Definition->EffectKind != EGameXXKRelicEffectKind::EmergencyHealPartyPercent
			|| Definition->Magnitude < 1
			|| Definition->Magnitude > 100)
		{
			return SetFailure(OutError, TEXT("The owned life-saving talisman has an invalid catalog battle-healing contract."));
		}
		NewRuntime.LifeSavingTalismanHealingPercent = Definition->Magnitude;
	}
	NewRuntime.EquippedHeroCardIds = Run.HeroSelectedCardIds;
	NewRuntime.BonusSharedEnergyCap = Run.BonusSharedEnergyCap;
	NewRuntime.BonusRoundDrawCount = Run.BonusRoundDrawCount;
	if (NewRuntime.BonusRoundDrawCount > 0)
	{
		FString DrawError;
		if (!GameXXKCardRules::DrawCards(NewRuntime.Deck, NewRuntime.BonusRoundDrawCount, 0, &DrawError))
		{
			return SetFailure(OutError, FString::Printf(TEXT("Opening-hand bonus draw failed: %s"), *DrawError));
		}
	}
	if (!MaterializeEquipmentBattleEffects(NewState, NewRuntime, OutError))
	{
		return false;
	}
	FGameXXKCardPlayResult OpeningTerrainResult;
	if (!GameXXKCardRules::ResolveRoundStartTerrainBenefits(
		NewRuntime,
		OpeningTerrainResult,
		OutError))
	{
		return false;
	}
	if (!ValidateLivingEnemyIntentPresentation(NewRuntime, OutError))
	{
		return false;
	}
	Run.bLoadoutLockedForRoute = true;
	Run.bHasActiveCardBattle = true;
	Run.ActiveBattleSourceNodeId = NewState.ActiveBattleNodeId;
	Run.ActiveBattle = MoveTemp(NewRuntime);
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	Run.PendingReward = FGameXXKPendingRouteCardReward();
	Run.bActiveBattleRewardResolved = false;
	FGameXXKRelicRules::ApplyBattleStart(NewState);
	if (!ApplyCatalogEnemyRoundStartStatuses(Run.ActiveBattle, OutError)
		|| !BuildEnemyIntents(Run, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	return true;
}

bool FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (!Run.bHasActiveCardBattle)
	{
		return false;
	}
	if (Run.ActiveBattle.bLifeSavingTalismanConsumptionPending)
	{
		return SetFailure(OutError, TEXT("A life-saving talisman consumption must finalize before legacy projection."));
	}
	if (!GameXXKCardRules::ValidateCardBattleRuntime(Run.ActiveBattle, OutError))
	{
		return false;
	}
	const auto SyncArray = [&Run, OutError](TArray<FGameXXKBattleRuntimeUnit>& InOutLegacyUnits, const EGameXXKCardTargetSide ExpectedSide)
	{
		for (FGameXXKBattleRuntimeUnit& LegacyUnit : InOutLegacyUnits)
		{
			const FGameXXKCardCombatUnit* CardUnit = FindCardUnit(Run.ActiveBattle.Units, LegacyUnit.Id);
			if (!CardUnit || CardUnit->Side != ExpectedSide)
			{
				return SetFailure(OutError, TEXT("A legacy battle projection lost a stable card-runtime unit."));
			}
			LegacyUnit.HP = CardUnit->HP;
			LegacyUnit.MaxHP = CardUnit->MaxHP;
			LegacyUnit.MP = CardUnit->Mana;
			LegacyUnit.MaxMP = CardUnit->MaxMana;
			LegacyUnit.Attack = CardUnit->Attack;
			LegacyUnit.Defense = CardUnit->Defense;
			LegacyUnit.Speed = CardUnit->Speed;
			// Card runtime owns armor.  Legacy scenes still render Shield, so keep it
			// as a one-way compatibility projection rather than a second mitigation pool.
			LegacyUnit.Shield = FMath::Max(0, CardUnit->Armor);
			LegacyUnit.bDefeated = !CardUnit->bLiving;
			LegacyUnit.bDefending = false;
		}
		return true;
	};
	if (!SyncArray(InOutState.ActiveBattleParty, EGameXXKCardTargetSide::Party)
		|| !SyncArray(InOutState.ActiveBattleEnemies, EGameXXKCardTargetSide::Enemy))
	{
		return false;
	}
	if (const FGameXXKCardCombatUnit* Hero = FindCardUnit(Run.ActiveBattle.Units, HeroUnitId))
	{
		// Keep the current route value, including any temporary event-created capacity.  The permanent
		// maxima remain unchanged and route cleanup restores the town baseline.
		InOutState.PlayerHP = FMath::Clamp(Hero->HP, 0, Hero->MaxHP);
		// The permanent mirror stays within base + route capacity. Explicit battle-local
		// capacity and its current Mana remain on the saved combat unit until battle ends.
		const int32 SurfaceManaCap = static_cast<int32>(FMath::Clamp<int64>(
			static_cast<int64>(InOutState.PlayerMaxMP) + FMath::Max(0, Run.RouteAttributeBonuses.MaxMana),
			0, Hero->MaxMana));
		InOutState.PlayerMP = FMath::Clamp(Hero->Mana, 0, SurfaceManaCap);
	}
	return true;
}

bool FGameXXKCardBattleAdapter::BuildCardPlayPreview(
	const FGameXXKRuntimeState& State,
	const FName CardInstanceId,
	FGameXXKCardPlayPreview& OutPreview,
	FString* OutError)
{
	if (!State.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}
	return GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, CardInstanceId, OutPreview, OutError);
}

bool FGameXXKCardBattleAdapter::ResolveCardPlay(
	FGameXXKRuntimeState& InOutState,
	const FName CardInstanceId,
	const FName SelectedTargetUnitId,
	FGameXXKCardPlayResult& OutResult,
	FString* OutError)
{
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}

	FGameXXKRuntimeState NewState = InOutState;
	FGameXXKCardPlayResult NewResult;
	const FGameXXKCardBattleRuntime BeforeCardPlay = NewState.CardRun.ActiveBattle;
	if (!GameXXKCardRules::ResolveCardPlay(NewState.CardRun.ActiveBattle, CardInstanceId, SelectedTargetUnitId, NewResult, OutError))
	{
		return false;
	}

	// The saved forecast represents the next enemy phase. Rebuild it exactly when a completed
	// player packet entered a new boss phase, so phase-only actions and tooltip data never lag a turn.
	if (DidAnyBossEnterPhaseTwo(BeforeCardPlay, NewState.CardRun.ActiveBattle)
		&& NewState.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player)
	{
		NewState.CardRun.EnemyIntents.Reset();
		NewState.CardRun.NextEnemyIntentIndex = 0;
		if (!BuildEnemyIntents(NewState.CardRun, OutError))
		{
			return false;
		}
	}

	const TArray<FGameXXKCardDamageResult> PrimaryDamageResults = NewResult.DamageResults;
	if (!FGameXXKRelicRules::ApplyCardPlayed(
		NewState,
		NewResult.OwnerUnitId,
		PrimaryDamageResults,
		NewResult,
		OutError))
	{
		return false;
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(NewState);
	OutResult = MoveTemp(NewResult);
	return true;
}

bool FGameXXKCardBattleAdapter::SubmitInsightChoice(
	FGameXXKRuntimeState& InOutState,
	const FName PickedInstanceId,
	const TArray<FName>& ReorderedRemainingInstanceIds,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}
	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::SubmitInsightChoice(
		NewState.CardRun.ActiveBattle,
		PickedInstanceId,
		ReorderedRemainingInstanceIds,
		OutError,
		&ResumedResults))
	{
		return false;
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(
	FGameXXKRuntimeState& InOutState,
	const FName PickedInstanceId,
	TArray<FGameXXKCardPlayResult>& OutResumedResults,
	FString* OutError)
{
	OutResumedResults.Reset();
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}
	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::SubmitHeroTaskSearchChoice(
		NewState.CardRun.ActiveBattle,
		PickedInstanceId,
		ResumedResults,
		OutError))
	{
		return false;
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	OutResumedResults = MoveTemp(ResumedResults);
	return true;
}

bool FGameXXKCardBattleAdapter::SubmitForcedDiscard(
	FGameXXKRuntimeState& InOutState,
	const TArray<FName>& DiscardedInstanceIds,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}
	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::SubmitForcedDiscard(
		NewState.CardRun.ActiveBattle,
		DiscardedInstanceIds,
		OutError,
		&ResumedResults))
	{
		return false;
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool FGameXXKCardBattleAdapter::CancelInsight(
	FGameXXKRuntimeState& InOutState,
	FString* OutError,
	TArray<FGameXXKCardPlayResult>* OutResumedResults)
{
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}
	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardPlayResult> ResumedResults;
	if (!GameXXKCardRules::CancelInsight(NewState.CardRun.ActiveBattle, OutError, &ResumedResults))
	{
		return false;
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	if (OutResumedResults)
	{
		*OutResumedResults = MoveTemp(ResumedResults);
	}
	return true;
}

bool FGameXXKCardBattleAdapter::ResumeAutomaticResolutionQueue(
	FGameXXKRuntimeState& InOutState,
	TArray<FGameXXKCardPlayResult>& OutResumedResults,
	FString* OutError)
{
	OutResumedResults.Reset();
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}

	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardPlayResult> NewResults;
	if (!GameXXKCardRules::ResumeAutomaticResolutionQueue(
			NewState.CardRun.ActiveBattle,
			NewResults,
			OutError)
		|| !FinalizeLifeSavingTalismanConsumption(NewState, OutError)
		|| !SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(NewState);
	OutResumedResults = MoveTemp(NewResults);
	return true;
}

bool FGameXXKCardBattleAdapter::EndPlayerCardPhase(
	FGameXXKRuntimeState& InOutState,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	FString* OutError)
{
	OutDamageResults.Reset();
	if (!InOutState.CardRun.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("There is no active card battle session."));
	}

	FGameXXKRuntimeState NewState = InOutState;
	TArray<FGameXXKCardDamageResult> NewDamageResults;
	if (!ValidateLivingEnemyIntentPresentation(NewState.CardRun.ActiveBattle, OutError))
	{
		return false;
	}
	if (!GameXXKCardRules::EndPlayerCardPhase(NewState.CardRun.ActiveBattle, NewDamageResults, OutError))
	{
		return false;
	}
	FGameXXKRelicRules::ApplyPlayerRoundEnd(NewState);
	FGameXXKRelicRules::ApplyDamageTaken(NewState, NewDamageResults);
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError))
	{
		return false;
	}

	FGameXXKCardRunState& Run = NewState.CardRun;
	if (Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		// Player-side end-phase effects can finish the battle. Terminal states never retain a forecast.
		Run.EnemyIntents.Reset();
		Run.NextEnemyIntentIndex = 0;
	}
	else
	{
		// Reuse the forecast created at player-phase start. Only a legacy/recovery state without one
		// may create a new list here. Defeated sources are removed before presentation can show them.
		PruneUnexecutableEnemyIntents(Run);
		if (Run.EnemyIntents.IsEmpty() && !BuildEnemyIntents(Run, OutError))
		{
			return false;
		}
	}
	if (!SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(NewState);
	OutDamageResults = MoveTemp(NewDamageResults);
	return true;
}

static bool ResolveNextEnemyIntentImpl(
	FGameXXKRuntimeState& InOutState,
	FGameXXKCardEnemyIntent& OutResolvedIntent,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	bool& bOutIntentsFinished,
	FString* OutError)
{
	OutResolvedIntent = FGameXXKCardEnemyIntent();
	OutDamageResults.Reset();
	bOutIntentsFinished = false;
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (!Run.bHasActiveCardBattle || Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only resolve during an active enemy card phase."));
	}
	if (Run.EnemyIntents.IsEmpty())
	{
		if (!BuildEnemyIntents(Run, OutError))
		{
			return false;
		}
	}
	if (Run.NextEnemyIntentIndex >= Run.EnemyIntents.Num())
	{
		bOutIntentsFinished = true;
		return FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(InOutState, OutError);
	}

	FGameXXKCardEnemyIntent Intent = Run.EnemyIntents[Run.NextEnemyIntentIndex];
	if (!RetargetDefeatedPersistentTargets(Run.ActiveBattle, OutError)
		|| !RefreshPersistentTargetReferencesForIntent(Run.ActiveBattle, Intent))
	{
		return false;
	}
	OutResolvedIntent = Intent;
	bool bIntentConsumed = false;
	int32 NextCatalogIntentCursor = INDEX_NONE;
	if (!GetNextCatalogIntentCursor(Run.ActiveBattle, Intent, NextCatalogIntentCursor, OutError))
	{
		return false;
	}
	const FGameXXKCardCombatUnit* Enemy = FindCardUnit(Run.ActiveBattle.Units, Intent.SourceUnitId);
	if (Enemy && Enemy->bLiving && Enemy->Side == EGameXXKCardTargetSide::Enemy)
	{
		const bool bHasCatalogResolvedEffect = Intent.Effects.ContainsByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage
				|| Effect.Type == EGameXXKEnemyIntentEffectType::AddArmor
				|| Effect.Type == EGameXXKEnemyIntentEffectType::ApplyStatus
				|| Effect.Type == EGameXXKEnemyIntentEffectType::Heal
				|| Effect.Type == EGameXXKEnemyIntentEffectType::ConsumeSharedQi
				|| Effect.Type == EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy
				|| Effect.Type == EGameXXKEnemyIntentEffectType::ModifyAttack
				|| Effect.Type == EGameXXKEnemyIntentEffectType::ModifySpeed
				|| Effect.Type == EGameXXKEnemyIntentEffectType::RemovePositiveStatus;
		});
		if (bHasCatalogResolvedEffect)
		{
			if (!Intent.bCharging && !ResolveCatalogIntentEffects(Run.ActiveBattle, Intent, OutDamageResults, OutError))
			{
				return false;
			}
			bIntentConsumed = true;
		}
		else
		{
			const FGameXXKCardCombatUnit* Target = FindCardUnit(Run.ActiveBattle.Units, Intent.SuggestedTargetUnitId);
			if (!Target || !Target->bLiving || Target->Side != EGameXXKCardTargetSide::Party)
			{
				Target = FindLowestLivingPartyUnit(Run.ActiveBattle);
			}
			if (Target)
			{
				FGameXXKCardDamageContext Context;
				Context.SourceUnitId = Intent.SourceUnitId;
				Context.Kind = Intent.Kind;
				Context.OnHitStatuses = Intent.OnHitStatuses;
				FGameXXKCardDamageResult DamageResult;
				TArray<FGameXXKCardDamageResult> ReactiveResults;
				if (!GameXXKCardRules::ResolveEnemyDirectAttack(
					Run.ActiveBattle,
					Context,
					Target->UnitId,
					Intent.Damage,
					DamageResult,
					&ReactiveResults,
					OutError,
					true))
				{
					return false;
				}
				OutDamageResults.Add(MoveTemp(DamageResult));
				OutDamageResults.Append(MoveTemp(ReactiveResults));
			}
			// There is no living party target for this saved action, so deliberately skip it.
			bIntentConsumed = true;
		}
	}
	else
	{
		// A defeated or invalid persisted source cannot execute its saved action, so deliberately skip it.
		bIntentConsumed = true;
	}
	if (bIntentConsumed && !Intent.bCharging)
	{
		FName FinalRecipientUnitId = NAME_None;
		for (const FGameXXKCardDamageResult& DamageResult : OutDamageResults)
		{
			if (DamageResult.Cause == EGameXXKCardDamageCause::DirectAttack
				&& DamageResult.SourceUnitId == Intent.SourceUnitId)
			{
				FinalRecipientUnitId = DamageResult.ResolvedTargetUnitId;
				break;
			}
		}
		TArray<FGameXXKCardDamageResult> ReactionDamageResults;
		if (!GameXXKCardRules::ResolvePartyReactionsAfterEnemyCard(
			Run.ActiveBattle,
			Intent.SourceUnitId,
			Intent.Kind,
			FinalRecipientUnitId,
			ReactionDamageResults,
			OutError))
		{
			return false;
		}
		OutDamageResults.Append(MoveTemp(ReactionDamageResults));
	}
	if (bIntentConsumed)
	{
		if (NextCatalogIntentCursor != INDEX_NONE)
		{
			FGameXXKEnemyBattleState* EnemyState = Run.ActiveBattle.EnemyStates.Find(Intent.SourceUnitId);
			if (!EnemyState)
			{
				return SetFailure(OutError, TEXT("A resolved catalog enemy intent lost its persisted enemy state."));
			}
			EnemyState->IntentCursor = NextCatalogIntentCursor;
		}
		if (!Intent.bCharging)
		{
			if (FGameXXKEnemyBattleState* EnemyState = Run.ActiveBattle.EnemyStates.Find(Intent.SourceUnitId);
				EnemyState && EnemyState->PendingChargedIntentId == Intent.IntentDefinitionId)
			{
				EnemyState->PendingChargedIntentId = NAME_None;
				EnemyState->ChargeRoundsRemaining = 0;
				EnemyState->PendingChargeTargetUnitIds.Reset();
			}
		}
		if (!StartSpiralHornDeerHealingCooldown(Run.ActiveBattle, Intent, OutError))
		{
			return false;
		}
		if (!RetargetDefeatedPersistentTargets(Run.ActiveBattle, OutError))
		{
			return false;
		}
		++Run.NextEnemyIntentIndex;
	}
	FGameXXKRelicRules::ApplyDamageTaken(InOutState, OutDamageResults);
	if (!FinalizeLifeSavingTalismanConsumption(InOutState, OutError))
	{
		return false;
	}
	GameXXKCardRules::RefreshCombatTerminalPhase(Run.ActiveBattle);

	bOutIntentsFinished = Run.NextEnemyIntentIndex >= Run.EnemyIntents.Num();
	return FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(InOutState, OutError);
}

bool FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
	FGameXXKRuntimeState& InOutState,
	FGameXXKCardEnemyIntent& OutResolvedIntent,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	bool& bOutIntentsFinished,
	FString* OutError)
{
	OutResolvedIntent = FGameXXKCardEnemyIntent();
	OutDamageResults.Reset();
	bOutIntentsFinished = false;

	FGameXXKRuntimeState NewState = InOutState;
	FGameXXKCardEnemyIntent NewResolvedIntent;
	TArray<FGameXXKCardDamageResult> NewDamageResults;
	bool bNewIntentsFinished = false;
	if (!ResolveNextEnemyIntentImpl(NewState, NewResolvedIntent, NewDamageResults, bNewIntentsFinished, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(NewState);
	OutResolvedIntent = MoveTemp(NewResolvedIntent);
	OutDamageResults = MoveTemp(NewDamageResults);
	bOutIntentsFinished = bNewIntentsFinished;
	return true;
}

bool FGameXXKCardBattleAdapter::SkipCurrentEnemyIntent(
	FGameXXKRuntimeState& InOutState,
	FString* OutError)
{
	FGameXXKRuntimeState NewState = InOutState;
	FGameXXKCardRunState& Run = NewState.CardRun;
	if (!Run.bHasActiveCardBattle || Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only be skipped during an active enemy card phase."));
	}
	if (Run.EnemyIntents.IsEmpty() || !Run.EnemyIntents.IsValidIndex(Run.NextEnemyIntentIndex))
	{
		return SetFailure(OutError, TEXT("There is no pending enemy intent to skip."));
	}

	++Run.NextEnemyIntentIndex;
	if (!SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}
	InOutState = MoveTemp(NewState);
	return true;
}

bool FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(
	FGameXXKRuntimeState& InOutState,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}

	FGameXXKRuntimeState NewState = InOutState;
	FGameXXKCardRunState& Run = NewState.CardRun;
	if (!Run.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only complete during an active card battle."));
	}
	if (Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy
		&& Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory
		&& Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Defeat)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only complete during an enemy or terminal card phase."));
	}
	if (Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy
		&& Run.NextEnemyIntentIndex < Run.EnemyIntents.Num())
	{
		return SetFailure(OutError, TEXT("All pending enemy intents must resolve before completing the enemy phase."));
	}
	TArray<FGameXXKCardDamageResult> NewDamageResults;
	if (Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy
		&& !GameXXKCardRules::BeginNextPlayerCardRound(Run.ActiveBattle, NewDamageResults, OutError))
	{
		return false;
	}
	if (Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player)
	{
		if (!GameXXKCardRules::ResetWhiteApeStatusGuardsForPlayerRound(Run.ActiveBattle, OutError))
		{
			return false;
		}
		if (!AdvanceSpiralHornDeerHealingCooldowns(Run.ActiveBattle, OutError))
		{
			return false;
		}
		// Enemy-end DOT and terminal cleanup have completed.  Only living enemies that
		// survived the boundary may advance charges or expire one-phase modifiers.
		AdvancePendingEnemyCharges(Run.ActiveBattle);
		ExpireEnemyPhaseTemporaryModifiers(Run.ActiveBattle);
		FGameXXKRelicRules::ApplyPlayerRoundStart(NewState);
		FGameXXKRelicRules::ApplyDamageTaken(NewState, NewDamageResults);
		if (!ApplyCatalogEnemyRoundStartStatuses(Run.ActiveBattle, OutError))
		{
			return false;
		}
	}
	if (!FinalizeLifeSavingTalismanConsumption(NewState, OutError))
	{
		return false;
	}
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	if (Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Player
		&& !BuildEnemyIntents(Run, OutError))
	{
		return false;
	}
	if (!SyncCardBattleToLegacyProjection(NewState, OutError))
	{
		return false;
	}

	InOutState = MoveTemp(NewState);
	OutDamageResults = MoveTemp(NewDamageResults);
	return true;
}

bool FGameXXKCardBattleAdapter::ResolveEnemyPhase(
	FGameXXKRuntimeState& InOutState,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	FString* OutError)
{
	FGameXXKRuntimeState NewState = InOutState;
	if (!NewState.CardRun.bHasActiveCardBattle || NewState.CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only resolve during an active enemy card phase."));
	}
	if (NewState.CardRun.EnemyIntents.IsEmpty())
	{
		if (!BuildEnemyIntents(NewState.CardRun, OutError))
		{
			return false;
		}
	}
	TArray<FGameXXKCardDamageResult> NewDamageResults;
	while (NewState.CardRun.NextEnemyIntentIndex < NewState.CardRun.EnemyIntents.Num()
		&& NewState.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		FGameXXKCardEnemyIntent ResolvedIntent;
		TArray<FGameXXKCardDamageResult> IntentDamageResults;
		bool bIntentsFinished = false;
		if (!ResolveNextEnemyIntent(NewState, ResolvedIntent, IntentDamageResults, bIntentsFinished, OutError))
		{
			return false;
		}
		NewDamageResults.Append(MoveTemp(IntentDamageResults));
	}

	TArray<FGameXXKCardDamageResult> CompletionDamageResults;
	if (!CompleteEnemyCardPhase(NewState, CompletionDamageResults, OutError))
	{
		return false;
	}
	NewDamageResults.Append(MoveTemp(CompletionDamageResults));
	InOutState = MoveTemp(NewState);
	OutDamageResults = MoveTemp(NewDamageResults);
	return true;
}

EGameXXKCardQuality FGameXXKCardBattleAdapter::GetConfiguredCardQuality(
	const FGameXXKCardRunState& CardRun,
	const FName CardId)
{
	if (const EGameXXKCardQuality* Upgraded = CardRun.UpgradedCardQualities.Find(CardId))
	{
		return *Upgraded;
	}
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Id == CardId)
		{
			return Definition.BaseQuality;
		}
	}
	return EGameXXKCardQuality::Common;
}

EGameXXKCardQuality FGameXXKCardBattleAdapter::GetNextCardQuality(const EGameXXKCardQuality Quality)
{
	switch (Quality)
	{
	case EGameXXKCardQuality::Common:
		return EGameXXKCardQuality::Rare;
	case EGameXXKCardQuality::Rare:
		return EGameXXKCardQuality::Epic;
	default:
		return Quality;
	}
}

bool FGameXXKCardBattleAdapter::CommitBossCardReward(
	FGameXXKRuntimeState& InOutState,
	const FName RewardCardId,
	FString* OutError)
{
	FGameXXKRuntimeState CandidateState = InOutState;
	if (!ValidatePendingRouteRewardGate(CandidateState, true, OutError))
	{
		return false;
	}

	const FGameXXKCardRunState& Run = CandidateState.CardRun;
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RewardCardId);
	const bool bInTieredOffer = Run.PendingReward.Options.ContainsByPredicate(
		[RewardCardId](const FGameXXKBattleRewardOption& Option)
		{
			return Option.CardId == RewardCardId && Option.Kind == EGameXXKBattleRewardKind::BossCard;
		});
	if (!CandidateState.bHasGeneratedRouteMap
		|| RewardCardId.IsNone()
		|| !bInTieredOffer
		|| !Definition
		|| !IsRouteCard(RewardCardId))
	{
		return SetFailure(OutError, TEXT("The selected boss card is not part of the saved boss reward offer."));
	}
	if (Run.BossCardSlots.Contains(RewardCardId))
	{
		return SetFailure(OutError, TEXT("The selected boss card already occupies a boss card slot."));
	}
	if (Run.BossCardSlots.Num() >= FGameXXKCardRunState::MaxBossCardSlots)
	{
		return SetFailure(OutError, TEXT("All boss card slots are full; no further boss card can be acquired."));
	}

	// Boss cards enter the player deck through one of the three dedicated
	// slots, and the chosen card is granted straight into the current hand.
	CandidateState.CardRun.BossCardSlots.Add(RewardCardId);
	if (CandidateState.CardRun.bHasActiveCardBattle)
	{
		FGameXXKCardInstance BossInstance;
		BossInstance.InstanceId = FName(*FString::Printf(
			TEXT("BossSlot.%d.%s"),
			CandidateState.CardRun.BossCardSlots.Num(),
			*RewardCardId.ToString()));
		BossInstance.CardId = RewardCardId;
		BossInstance.CurrentQuality = Definition->BaseQuality;
		BossInstance.OwnerUnitId = HeroUnitId;
		BossInstance.SourceEntryId = BossInstance.InstanceId;
		BossInstance.AcquisitionOrdinal = CandidateState.CardRun.NextRewardOrdinal;
		CandidateState.CardRun.ActiveBattle.Deck.Hand.Add(MoveTemp(BossInstance));
	}

	InOutState = MoveTemp(CandidateState);
	return true;
}

bool FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKNodeKind NodeKind,
	const int32 SourceNodeId,
	const int32 ChoiceSeed,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if (SourceNodeId < 0)
	{
		return SetFailure(OutError, TEXT("Tiered battle rewards require a non-negative source node."));
	}

	FGameXXKRuntimeState CandidateState = InOutState;
	if (!EnsureCardRunInitialized(CandidateState, OutError))
	{
		return false;
	}
	if (!CandidateState.bDungeonActive || !CandidateState.CardRun.bLoadoutLockedForRoute)
	{
		return SetFailure(OutError, TEXT("Battle rewards require an active route with its loadout locked."));
	}
	if (!ValidatePendingRouteRewardGate(CandidateState, false, OutError))
	{
		return false;
	}

	FGameXXKCardRunState& Run = CandidateState.CardRun;
	if (!Run.PendingReward.Options.IsEmpty())
	{
		if (Run.PendingReward.SourceNodeId != SourceNodeId)
		{
			return SetFailure(OutError, TEXT("A different battle reward source cannot replace the saved pending offer."));
		}
		return true;
	}
	if (!IsRewardNodeKind(NodeKind) || ChoiceSeed == 0)
	{
		return SetFailure(OutError, TEXT("A new tiered battle reward requires a battle, elite, or boss node and a non-zero choice seed."));
	}
	if (SourceNodeId != GetActiveRewardSourceNodeId(CandidateState))
	{
		return SetFailure(OutError, TEXT("The route reward source does not match the active card-battle victory."));
	}
	if (Run.NextRewardOrdinal < 0 || Run.NextRewardOrdinal == MAX_int32)
	{
		return SetFailure(OutError, TEXT("The next route reward ordinal must be non-negative and safely incrementable."));
	}

	uint32 RandomState = static_cast<uint32>(ChoiceSeed);
	TArray<FGameXXKBattleRewardOption> Options;
	Options.Reserve(3);
	auto AddOption = [&Options](const EGameXXKBattleRewardKind Kind, const FName CardId, const FName RelicId)
	{
		FGameXXKBattleRewardOption Option;
		Option.Kind = Kind;
		Option.CardId = CardId;
		Option.RelicId = RelicId;
		Options.Add(MoveTemp(Option));
	};

	// Deck-card upgrade candidates: hero + active-companion configured cards below Epic.
	TArray<FName> DeckCandidates;
	TSet<FName> SeenDeckCards;
	auto AppendDeckCards = [&](const TArray<FName>& CardIds)
	{
		for (const FName CardId : CardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (!CardId.IsNone() && Definition && Definition->Owner != EGameXXKCardOwner::Route
				&& !SeenDeckCards.Contains(CardId)
				&& GetConfiguredCardQuality(Run, CardId) < EGameXXKCardQuality::Epic)
			{
				SeenDeckCards.Add(CardId);
				DeckCandidates.Add(CardId);
			}
		}
	};
	AppendDeckCards(Run.HeroSelectedCardIds);
	for (const FGameXXKPermanentCompanion& Companion : Run.CompanionRoster.PermanentCompanions)
	{
		if (Companion.bIsActive)
		{
			AppendDeckCards(Companion.SelectedCardIds);
		}
	}

	TArray<FName> RelicPool;
	for (const FGameXXKRelicDefinition& Definition : FGameXXKRelicCatalog::GetAllDefinitions())
	{
		const bool bOwned = Run.Relics.ContainsByPredicate(
			[&Definition](const FGameXXKRelicInstance& Instance) { return Instance.RelicId == Definition.Id; });
		if (Definition.bOfferEligible && (!bOwned || Definition.bStackable))
		{
			RelicPool.Add(Definition.Id);
		}
	}

	auto PickDeckCard = [&]() -> bool
	{
		if (DeckCandidates.IsEmpty())
		{
			return false;
		}
		const int32 Index = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(DeckCandidates.Num()));
		AddOption(EGameXXKBattleRewardKind::DeckCardUpgrade, DeckCandidates[Index], NAME_None);
		DeckCandidates.RemoveAt(Index);
		return true;
	};
	auto PickRelic = [&]() -> bool
	{
		if (RelicPool.IsEmpty())
		{
			return false;
		}
		const int32 Index = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(RelicPool.Num()));
		AddOption(EGameXXKBattleRewardKind::Relic, NAME_None, RelicPool[Index]);
		RelicPool.RemoveAt(Index);
		return true;
	};

	if (NodeKind == EGameXXKNodeKind::Boss && CandidateState.bHasGeneratedRouteMap)
	{
		const bool bTiger = CandidateState.ActiveBattleEnemies.ContainsByPredicate(
			[](const FGameXXKBattleRuntimeUnit& Enemy) { return Enemy.Id == TEXT("Tiger"); });
		const FName BossAcquisitionKey = bTiger ? FName(TEXT("Route.Boss.Tiger")) : FName(TEXT("Route.Boss.BlackBear"));
		TArray<FName> BossCards;
		AppendEligibleRouteCards(Run, [BossAcquisitionKey](const FGameXXKCardDefinition& Definition)
		{
			return Definition.AcquisitionKey == BossAcquisitionKey;
		}, BossCards);
		if (BossCards.IsEmpty())
		{
			// Every boss card already occupies a boss slot; fall back to a relic.
			if (!PickRelic())
			{
				return SetFailure(OutError, TEXT("The boss reward pool is empty and no relic fallback is available."));
			}
		}
		else
		{
			const int32 BossPick = static_cast<int32>(NextRandom(RandomState) % static_cast<uint32>(BossCards.Num()));
			AddOption(EGameXXKBattleRewardKind::BossCard, BossCards[BossPick], NAME_None);
		}
		if (!PickDeckCard())
		{
			PickRelic();
		}
		PickRelic();
	}
	else if (NodeKind == EGameXXKNodeKind::Elite)
	{
		const EGameXXKBattleRewardKind AttributeKind = (NextRandom(RandomState) % 2U) == 0U
			? EGameXXKBattleRewardKind::EnergyCapBonus
			: EGameXXKBattleRewardKind::DrawBonus;
		AddOption(AttributeKind, NAME_None, NAME_None);
		if (!PickDeckCard())
		{
			PickRelic();
		}
		PickRelic();
	}
	else
	{
		PickRelic();
		PickRelic();
		if (!PickDeckCard())
		{
			PickRelic();
		}
	}
	while (Options.Num() < 3 && PickRelic())
	{
	}

	if (Options.Num() != 3)
	{
		return SetFailure(OutError, TEXT("The tiered battle reward pools cannot supply three distinct legal options."));
	}
	Run.PendingReward.SourceNodeId = SourceNodeId;
	Run.PendingReward.ChoiceSeed = ChoiceSeed;
	Run.PendingReward.Options = MoveTemp(Options);
	Run.PendingReward.bRequiresRouteCardReplacement = false;
	++Run.NextRewardOrdinal;
	InOutState = MoveTemp(CandidateState);
	return true;
}


bool FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
	const FGameXXKRuntimeState& State,
	const FName RewardCardId,
	const FName ReplacementEntryId,
	FGameXXKRouteCardAcquisitionPreview& OutPreview,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	OutPreview = FGameXXKRouteCardAcquisitionPreview();
	if (!ValidatePendingRouteRewardGate(State, true, OutError))
	{
		return false;
	}

	const FGameXXKCardRunState& Run = State.CardRun;
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RewardCardId);
	const bool bInTieredOffer = Run.PendingReward.Options.ContainsByPredicate(
		[RewardCardId](const FGameXXKBattleRewardOption& Option)
		{
			return Option.CardId == RewardCardId && Option.Kind == EGameXXKBattleRewardKind::BossCard;
		});
	if (!State.bHasGeneratedRouteMap
		|| RewardCardId.IsNone()
		|| !bInTieredOffer
		|| !Definition
		|| !IsRouteCard(RewardCardId))
	{
		return SetFailure(OutError, TEXT("The selected boss card is not part of the saved boss reward offer."));
	}
	if (Run.BossCardSlots.Contains(RewardCardId))
	{
		return SetFailure(OutError, TEXT("The selected boss card already occupies a boss card slot."));
	}
	if (Run.BossCardSlots.Num() >= FGameXXKCardRunState::MaxBossCardSlots)
	{
		return SetFailure(OutError, TEXT("All boss card slots are full; no further boss card can be acquired."));
	}

	// Boss cards never replace another card: an available slot means the
	// reward commits directly into the player deck and the current hand.
	OutPreview.Decision = EGameXXKRouteCardAcquisitionDecision::CanCommit;
	OutPreview.CardId = RewardCardId;
	return true;
}

bool FGameXXKCardBattleAdapter::SkipPendingRouteReward(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKRuntimeState CandidateState = InOutState;
	if (!EnsureCardRunInitialized(CandidateState, OutError))
	{
		return false;
	}
	if (!ValidatePendingRouteRewardGate(CandidateState, true, OutError))
	{
		return false;
	}
	CandidateState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
	CandidateState.CardRun.bActiveBattleRewardResolved = true;
	InOutState = MoveTemp(CandidateState);
	return true;
}

bool FGameXXKCardBattleAdapter::CreateRouteEventOffer(
	FGameXXKRuntimeState& InOutState,
	const int32 SourceNodeId,
	const int32 ChoiceSeed,
	FName& OutEventNpcId,
	FString* OutError)
{
	OutEventNpcId = NAME_None;
	if (!EnsureCardRunInitialized(InOutState, OutError) || SourceNodeId == INDEX_NONE || ChoiceSeed == 0)
	{
		return SetFailure(OutError, TEXT("Route events require initialized card state, a stable source node, and a non-zero choice seed."));
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (Run.PendingEvent.SourceNodeId != INDEX_NONE)
	{
		if (Run.PendingEvent.SourceNodeId != SourceNodeId)
		{
			return SetFailure(OutError, TEXT("A different route event must be resolved before opening another event offer."));
		}
		OutEventNpcId = Run.PendingEvent.EventNpcId;
		return !OutEventNpcId.IsNone();
	}

	const FGameXXKRouteEncounterDefinition* Encounter = FGameXXKRouteEncounterCatalog::ChooseDeterministic(
		EGameXXKRouteEncounterKind::Event,
		ChoiceSeed);
	if (!Encounter)
	{
		return SetFailure(OutError, TEXT("The route event catalog does not contain any approved event identity."));
	}
	Run.PendingEvent.SourceNodeId = SourceNodeId;
	Run.PendingEvent.ChoiceSeed = ChoiceSeed;
	Run.PendingEvent.EncounterId = Encounter->Id;
	Run.PendingEvent.EventNpcId = Encounter->EventNpcId;
	Run.PendingEvent.bCanRecruitPermanentCompanion = false;
	OutEventNpcId = Run.PendingEvent.EventNpcId;
	return true;
}

void FGameXXKCardBattleAdapter::ClearActiveCardBattle(FGameXXKRuntimeState& InOutState)
{
	FGameXXKCardRunState& Run = InOutState.CardRun;
	Run.bHasActiveCardBattle = false;
	Run.ActiveBattleSourceNodeId = INDEX_NONE;
	Run.ActiveBattle = FGameXXKCardBattleRuntime();
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	Run.PendingReward = FGameXXKPendingRouteCardReward();
	Run.bActiveBattleRewardResolved = false;
}

void FGameXXKCardBattleAdapter::ClearRouteLocalCardState(FGameXXKRuntimeState& InOutState)
{
	FGameXXKCardRunState& Run = InOutState.CardRun;
	Run.bLoadoutLockedForRoute = false;
	ClearActiveCardBattle(InOutState);
	Run.PendingEvent = FGameXXKPendingRouteEvent();
	Run.RouteMerchant = FGameXXKRouteMerchantState();
	FGameXXKRelicRules::ClearRouteRelics(InOutState);
}

bool FGameXXKCardBattleAdapter::IsCardBattleTerminal(const FGameXXKRuntimeState& State)
{
	if (!State.CardRun.bHasActiveCardBattle)
	{
		return false;
	}
	return State.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Victory
		|| State.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Defeat;
}
