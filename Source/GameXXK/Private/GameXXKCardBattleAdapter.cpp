#include "GameXXKCardBattleAdapter.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"

namespace
{
	constexpr int32 HeroSelectedCardCount = 8;
	constexpr int32 PermanentCompanionSelectedCardCount = 5;
	constexpr int32 QuestNpcSelectedCardCount = 3;
	constexpr int32 BaseRouteCardCount = 2;
	constexpr int32 StartingDeckCardCount = 18;
	constexpr int32 MaximumDeckCardCount = 30;
	constexpr int32 MaximumRouteRewardCardCount = MaximumDeckCardCount - StartingDeckCardCount;

	const FName HeroUnitId(TEXT("Player"));
	const TArray<FName> BaseRouteCards = {
		FName(TEXT("Route.General.PoJiaTuCi")),
		FName(TEXT("Route.General.ShouShiHuiYuan"))};
	const TArray<FName> MissingPartyFillCards = {
		FName(TEXT("Route.General.QingShenQuShi")),
		FName(TEXT("Route.General.TuNaJue")),
		FName(TEXT("Route.General.ZhiXueSan")),
		FName(TEXT("Route.General.FeiZhen")),
		FName(TEXT("Route.General.YanDun")),
		FName(TEXT("Route.General.TieJiLi")),
		FName(TEXT("Route.General.LinZhenMoRen")),
		FName(TEXT("Route.Terrain.XingJunBuZhen"))};

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
		return Definition && Definition->Owner == EGameXXKCardOwner::Route;
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

	TArray<FName> GetOrderedHeroCatalogIds()
	{
		TArray<FName> Result;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Hero)
			{
				Result.Add(Definition.Id);
			}
		}
		return Result;
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
		Unit.Speed = bEnemy ? 8 : 10;
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
		Unit.Armor = 0;
		Unit.StableSortOrder = StableSortOrder;
		Unit.bLiving = Unit.HP > 0;
		return Unit;
	}

	bool BuildRoutePartyProjection(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		FGameXXKCardRunState& Run = InOutState.CardRun;
		if (!SynchronizePartySelectionWithRoster(Run, OutError)
			|| !FGameXXKCompanionRules::ValidatePartySelection(Run.CompanionRoster, Run.PartySelection, OutError))
		{
			return false;
		}

		TArray<FGameXXKBattleRuntimeUnit> NewParty;
		NewParty.Add(MakeHeroProjectionUnit(InOutState));
		if (const FGameXXKPermanentCompanion* Companion = FindActiveCompanion(Run.CompanionRoster))
		{
			FGameXXKCompanionAttributes Attributes;
			if (!FGameXXKCompanionRules::GetCompanionAttributes(Companion->Role, Companion->Level, Companion->Star, FGameXXKCompanionAttributes(), Attributes, OutError))
			{
				return false;
			}
			NewParty.Add(MakeLegacyProjectionUnit(
				Companion->InstanceId,
				FText::FromString(TEXT("伙伴")),
				Attributes,
				false));
		}

		if (Run.ActiveTemporaryQuestNpcId != NAME_None)
		{
			if (Run.ActiveTemporaryQuestNpcId != Run.PartySelection.QuestNpc.NpcId)
			{
				return SetFailure(OutError, TEXT("The route-local task NPC provenance does not match the configured NPC cards."));
			}
			FGameXXKCompanionAttributes Attributes;
			if (!FGameXXKCompanionRules::GetQuestNpcAttributes(Run.ActiveTemporaryQuestNpcId, FMath::Max(1, InOutState.PlayerLevel), Attributes, OutError))
			{
				return false;
			}
			NewParty.Add(MakeLegacyProjectionUnit(
				Run.ActiveTemporaryQuestNpcId,
				FText::FromString(TEXT("任务同伴")),
				Attributes,
				false));
		}

		if (NewParty.Num() > 3)
		{
			return SetFailure(OutError, TEXT("The card battle party exceeds the fixed hero plus one companion plus one task-NPC limit."));
		}
		InOutState.ActiveBattleParty = MoveTemp(NewParty);
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
		else
		{
			for (int32 Index = 0; Index < PermanentCompanionSelectedCardCount; ++Index)
			{
				if (!MissingPartyFillCards.IsValidIndex(MissingFillIndex) || !AddInstance(MissingPartyFillCards[MissingFillIndex++], HeroUnitId))
				{
					return SetFailure(OutError, TEXT("The deterministic missing-companion fill sequence is incomplete."));
				}
			}
		}

		if (Run.ActiveTemporaryQuestNpcId != NAME_None)
		{
			if (Run.PartySelection.QuestNpc.NpcId != Run.ActiveTemporaryQuestNpcId
				|| Run.PartySelection.QuestNpc.SelectedCardIds.Num() != QuestNpcSelectedCardCount)
			{
				return SetFailure(OutError, TEXT("The active task NPC does not have a valid three-card route selection."));
			}
			for (const FName CardId : Run.PartySelection.QuestNpc.SelectedCardIds)
			{
				if (!AddInstance(CardId, Run.ActiveTemporaryQuestNpcId))
				{
					return false;
				}
			}
		}
		else
		{
			for (int32 Index = 0; Index < QuestNpcSelectedCardCount; ++Index)
			{
				if (!MissingPartyFillCards.IsValidIndex(MissingFillIndex) || !AddInstance(MissingPartyFillCards[MissingFillIndex++], HeroUnitId))
				{
					return SetFailure(OutError, TEXT("The deterministic missing-NPC fill sequence is incomplete."));
				}
			}
		}

		for (const FName CardId : BaseRouteCards)
		{
			if (!AddInstance(CardId, HeroUnitId))
			{
				return false;
			}
		}
		for (const FName CardId : Run.RouteCardIds)
		{
			if (!IsRouteCard(CardId) || !AddInstance(CardId, HeroUnitId))
			{
				return SetFailure(OutError, TEXT("The route-local deck contains an invalid reward card."));
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
		OutUnits.Reset();
		if (State.ActiveBattleParty.IsEmpty() || State.ActiveBattleEnemies.IsEmpty())
		{
			return SetFailure(OutError, TEXT("A card battle requires non-empty party and enemy projections."));
		}
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
			OutUnits.Add(MakeCardCombatUnit(LegacyUnit, EGameXXKCardTargetSide::Party, Role, PartyIndex));
		}
		for (int32 EnemyIndex = 0; EnemyIndex < State.ActiveBattleEnemies.Num(); ++EnemyIndex)
		{
			const FGameXXKBattleRuntimeUnit& LegacyUnit = State.ActiveBattleEnemies[EnemyIndex];
			if (!LegacyUnit.bEnemy || LegacyUnit.Id.IsNone())
			{
				return SetFailure(OutError, TEXT("The battle enemy projection contains an invalid stable enemy unit."));
			}
			OutUnits.Add(MakeCardCombatUnit(LegacyUnit, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 100 + EnemyIndex));
		}
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

	void BuildEnemyIntents(FGameXXKCardRunState& InOutRun)
	{
		InOutRun.EnemyIntents.Reset();
		InOutRun.NextEnemyIntentIndex = 0;
		if (InOutRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
		{
			return;
		}
		TArray<const FGameXXKCardCombatUnit*> Enemies;
		for (const FGameXXKCardCombatUnit& Unit : InOutRun.ActiveBattle.Units)
		{
			if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Enemies.Add(&Unit);
			}
		}
		Enemies.Sort([](const FGameXXKCardCombatUnit& Left, const FGameXXKCardCombatUnit& Right)
		{
			return Left.StableSortOrder != Right.StableSortOrder
				? Left.StableSortOrder < Right.StableSortOrder
				: NameLess(Left.UnitId, Right.UnitId);
		});
		for (const FGameXXKCardCombatUnit* Enemy : Enemies)
		{
			const FGameXXKCardCombatUnit* Target = FindLowestLivingPartyUnit(InOutRun.ActiveBattle);
			if (!Target)
			{
				break;
			}
			FGameXXKCardEnemyIntent& Intent = InOutRun.EnemyIntents.AddDefaulted_GetRef();
			Intent.SourceUnitId = Enemy->UnitId;
			Intent.SuggestedTargetUnitId = Target->UnitId;
			Intent.Damage = FMath::Max(1, Enemy->Attack);
			Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		}
	}

	TArray<FName> BuildBaseDeckCardIds(const FGameXXKCardRunState& Run)
	{
		TArray<FName> Result = Run.HeroSelectedCardIds;
		int32 MissingFillIndex = 0;
		if (const FGameXXKPermanentCompanion* Companion = FindActiveCompanion(Run.CompanionRoster))
		{
			Result.Append(Companion->SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < PermanentCompanionSelectedCardCount; ++Index)
			{
				Result.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}
		if (Run.ActiveTemporaryQuestNpcId != NAME_None)
		{
			Result.Append(Run.PartySelection.QuestNpc.SelectedCardIds);
		}
		else
		{
			for (int32 Index = 0; Index < QuestNpcSelectedCardCount; ++Index)
			{
				Result.Add(MissingPartyFillCards[MissingFillIndex++]);
			}
		}
		Result.Append(BaseRouteCards);
		return Result;
	}

	int32 GetCurrentCardCopyCount(const FGameXXKCardRunState& Run, const FName CardId, const FName ExcludedRouteCardId = NAME_None)
	{
		int32 Result = 0;
		for (const FName ExistingCardId : BuildBaseDeckCardIds(Run))
		{
			Result += ExistingCardId == CardId;
		}
		bool bExcluded = false;
		for (const FName ExistingCardId : Run.RouteCardIds)
		{
			if (!bExcluded && ExistingCardId == ExcludedRouteCardId)
			{
				bExcluded = true;
				continue;
			}
			Result += ExistingCardId == CardId;
		}
		return Result;
	}

	void AppendEligibleRouteCards(
		const FGameXXKCardRunState& Run,
		const TFunctionRef<bool(const FGameXXKCardDefinition&)>& Predicate,
		TArray<FName>& OutCards)
	{
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Route
				&& Predicate(Definition)
				&& GetCurrentCardCopyCount(Run, Definition.Id) < 2)
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
}

bool FGameXXKCardBattleAdapter::EnsureCardRunInitialized(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	const TArray<FName> HeroCatalogIds = GetOrderedHeroCatalogIds();
	if (HeroCatalogIds.Num() != 12)
	{
		return SetFailure(OutError, TEXT("The card catalog must expose exactly twelve fixed hero cards."));
	}
	if (Run.HeroUnlockedCardIds.IsEmpty())
	{
		Run.HeroUnlockedCardIds.Append(HeroCatalogIds.GetData(), HeroSelectedCardCount);
	}
	if (Run.HeroSelectedCardIds.IsEmpty())
	{
		if (Run.HeroUnlockedCardIds.Num() < HeroSelectedCardCount)
		{
			return SetFailure(OutError, TEXT("A migrated hero does not have enough unlocked cards for the default eight-slot loadout."));
		}
		Run.HeroSelectedCardIds.Append(Run.HeroUnlockedCardIds.GetData(), HeroSelectedCardCount);
	}
	if (Run.RouteRandomSeed == 0)
	{
		Run.RouteRandomSeed = InOutState.RouteSeed != 0 ? InOutState.RouteSeed : 0x13579BDF;
	}
	if (!SynchronizePartySelectionWithRoster(Run, OutError) || !ValidateHeroLoadout(Run, OutError))
	{
		return false;
	}
	if (Run.ActiveTemporaryQuestNpcId == NAME_None && !Run.PartySelection.QuestNpc.NpcId.IsNone())
	{
		// A legacy save can contain no NPC selection only.  Do not silently make a task NPC active.
		Run.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
	}
	if (Run.ActiveTemporaryQuestNpcId != NAME_None
		&& Run.ActiveTemporaryQuestNpcId != Run.PartySelection.QuestNpc.NpcId)
	{
		return SetFailure(OutError, TEXT("The saved task NPC provenance and card selection disagree."));
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
	if (!EnsureCardRunInitialized(InOutState, OutError))
	{
		return false;
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (Run.bLoadoutLockedForRoute || Run.bHasActiveCardBattle)
	{
		return SetFailure(OutError, TEXT("Task NPC cards cannot change after entering the route."));
	}
	if (QuestNpcId.IsNone())
	{
		Run.ActiveTemporaryQuestNpcId = NAME_None;
		Run.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
		return true;
	}
	const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId);
	if (!Definition)
	{
		return SetFailure(OutError, TEXT("The route task NPC is not one of the approved named task NPCs."));
	}
	TArray<FName> EffectiveSelection = SelectedCardIds;
	if (EffectiveSelection.IsEmpty())
	{
		EffectiveSelection.Append(Definition->FixedCardIds.GetData(), QuestNpcSelectedCardCount);
	}
	if (!FGameXXKCompanionRules::SetQuestNpcCardSelection(Run.PartySelection.QuestNpc, QuestNpcId, EffectiveSelection, OutError))
	{
		return false;
	}
	Run.ActiveTemporaryQuestNpcId = QuestNpcId;
	return true;
}

bool FGameXXKCardBattleAdapter::BeginCardBattle(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKNodeKind NodeKind,
	const EGameXXKCardTerrain Terrain,
	const int32 InitialRandomSeed,
	FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	if ((NodeKind != EGameXXKNodeKind::Battle && NodeKind != EGameXXKNodeKind::Elite && NodeKind != EGameXXKNodeKind::Boss)
		|| Terrain == EGameXXKCardTerrain::Invalid || !InOutState.bHasActiveBattle || InOutState.ActiveBattleEnemies.IsEmpty())
	{
		return SetFailure(OutError, TEXT("Card battle initialization requires an active battle node, a concrete terrain, and enemies."));
	}
	if (!EnsureCardRunInitialized(InOutState, OutError) || !BuildRoutePartyProjection(InOutState, OutError))
	{
		return false;
	}

	FGameXXKCardRunState& Run = InOutState.CardRun;
	TArray<FGameXXKCardCombatUnit> Units;
	TArray<FGameXXKCardInstance> Instances;
	if (!BuildCardCombatUnits(InOutState, Units, OutError)
		|| !BuildStartingCardInstances(InOutState, InOutState.ActiveBattleNodeId, Instances, OutError))
	{
		return false;
	}
	FGameXXKCardBattleRuntime NewRuntime;
	const int32 EffectiveSeed = InitialRandomSeed != 0
		? InitialRandomSeed
		: Run.RouteRandomSeed ^ (InOutState.ActiveBattleNodeId * 486187739);
	if (!GameXXKCardRules::InitializeCardBattleRuntime(NewRuntime, Instances, Units, Terrain, EffectiveSeed, OutError))
	{
		return false;
	}
	Run.bLoadoutLockedForRoute = true;
	Run.bHasActiveCardBattle = true;
	Run.ActiveBattleSourceNodeId = InOutState.ActiveBattleNodeId;
	Run.ActiveBattle = MoveTemp(NewRuntime);
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	return SyncCardBattleToLegacyProjection(InOutState, OutError);
}

bool FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (OutError)
	{
		OutError->Reset();
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (!Run.bHasActiveCardBattle || !GameXXKCardRules::ValidateCardBattleRuntime(Run.ActiveBattle, OutError))
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
		InOutState.PlayerHP = FMath::Clamp(Hero->HP, 0, InOutState.PlayerMaxHP);
		InOutState.PlayerMP = FMath::Clamp(Hero->Mana, 0, InOutState.PlayerMaxMP);
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
	if (!GameXXKCardRules::ResolveCardPlay(InOutState.CardRun.ActiveBattle, CardInstanceId, SelectedTargetUnitId, OutResult, OutError))
	{
		return false;
	}
	return SyncCardBattleToLegacyProjection(InOutState, OutError);
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
	if (!GameXXKCardRules::EndPlayerCardPhase(InOutState.CardRun.ActiveBattle, OutDamageResults, OutError))
	{
		return false;
	}
	BuildEnemyIntents(InOutState.CardRun);
	return SyncCardBattleToLegacyProjection(InOutState, OutError);
}

bool FGameXXKCardBattleAdapter::ResolveEnemyPhase(
	FGameXXKRuntimeState& InOutState,
	TArray<FGameXXKCardDamageResult>& OutDamageResults,
	FString* OutError)
{
	OutDamageResults.Reset();
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (!Run.bHasActiveCardBattle || Run.ActiveBattle.Phase != EGameXXKCardBattlePhase::Enemy)
	{
		return SetFailure(OutError, TEXT("Enemy intents can only resolve during an active enemy card phase."));
	}
	if (Run.EnemyIntents.IsEmpty())
	{
		BuildEnemyIntents(Run);
	}
	while (Run.NextEnemyIntentIndex < Run.EnemyIntents.Num()
		&& Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		const FGameXXKCardEnemyIntent Intent = Run.EnemyIntents[Run.NextEnemyIntentIndex++];
		const FGameXXKCardCombatUnit* Enemy = FindCardUnit(Run.ActiveBattle.Units, Intent.SourceUnitId);
		if (!Enemy || !Enemy->bLiving || Enemy->Side != EGameXXKCardTargetSide::Enemy)
		{
			continue;
		}
		const FGameXXKCardCombatUnit* Target = FindCardUnit(Run.ActiveBattle.Units, Intent.SuggestedTargetUnitId);
		if (!Target || !Target->bLiving || Target->Side != EGameXXKCardTargetSide::Party)
		{
			Target = FindLowestLivingPartyUnit(Run.ActiveBattle);
		}
		if (!Target)
		{
			break;
		}
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = Intent.SourceUnitId;
		Context.Kind = Intent.Kind;
		FGameXXKCardDamageResult DamageResult;
		TArray<FGameXXKCardDamageResult> ReactiveResults;
		if (!GameXXKCardRules::ResolveEnemyDirectAttack(Run.ActiveBattle, Context, Target->UnitId, Intent.Damage, DamageResult, &ReactiveResults, OutError))
		{
			return false;
		}
		OutDamageResults.Add(MoveTemp(DamageResult));
		OutDamageResults.Append(MoveTemp(ReactiveResults));
	}
	if (Run.ActiveBattle.Phase == EGameXXKCardBattlePhase::Enemy)
	{
		TArray<FGameXXKCardDamageResult> EnemyDotResults;
		if (!GameXXKCardRules::BeginNextPlayerCardRound(Run.ActiveBattle, EnemyDotResults, OutError))
		{
			return false;
		}
		OutDamageResults.Append(MoveTemp(EnemyDotResults));
	}
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	return SyncCardBattleToLegacyProjection(InOutState, OutError);
}

bool FGameXXKCardBattleAdapter::CreateRouteRewardOffer(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKNodeKind NodeKind,
	const int32 SourceNodeId,
	const int32 ChoiceSeed,
	TArray<FName>& OutCardIds,
	FString* OutError)
{
	OutCardIds.Reset();
	if (!EnsureCardRunInitialized(InOutState, OutError) || SourceNodeId == INDEX_NONE || ChoiceSeed == 0)
	{
		return SetFailure(OutError, TEXT("Route rewards require initialized card state, a stable source node, and a non-zero choice seed."));
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (!Run.PendingReward.CardIds.IsEmpty())
	{
		if (Run.PendingReward.SourceNodeId != SourceNodeId)
		{
			return SetFailure(OutError, TEXT("A different route reward must be resolved before opening another offer."));
		}
		OutCardIds = Run.PendingReward.CardIds;
		return true;
	}
	if (Run.RouteCardIds.Num() > MaximumRouteRewardCardCount)
	{
		return SetFailure(OutError, TEXT("The route reward list already exceeds the twelve-card temporary cap."));
	}

	TArray<FName> AllEligible;
	AppendEligibleRouteCards(Run, [](const FGameXXKCardDefinition&)
	{
		return true;
	}, AllEligible);
	TArray<FName> GeneralOrTerrain;
	AppendEligibleRouteCards(Run, [](const FGameXXKCardDefinition& Definition)
	{
		return Definition.AcquisitionKey == TEXT("Route.General") || Definition.AcquisitionKey == TEXT("Route.Terrain");
	}, GeneralOrTerrain);
	TArray<FName> TerrainCards;
	AppendEligibleRouteCards(Run, [](const FGameXXKCardDefinition& Definition)
	{
		return Definition.AcquisitionKey == TEXT("Route.Terrain");
	}, TerrainCards);
	TArray<FName> RareCards;
	AppendEligibleRouteCards(Run, [](const FGameXXKCardDefinition& Definition)
	{
		return Definition.Rarity == EGameXXKCardRarity::Rare;
	}, RareCards);

	uint32 RandomState = static_cast<uint32>(ChoiceSeed);
	TArray<FName> Picks;
	if (NodeKind == EGameXXKNodeKind::Boss)
	{
		const bool bTiger = InOutState.ActiveBattleEnemies.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Enemy)
		{
			return Enemy.Id == TEXT("Tiger");
		});
		const FName BossAcquisitionKey = bTiger ? FName(TEXT("Route.Boss.Tiger")) : FName(TEXT("Route.Boss.BlackBear"));
		TArray<FName> BossCards;
		AppendEligibleRouteCards(Run, [BossAcquisitionKey](const FGameXXKCardDefinition& Definition)
		{
			return Definition.AcquisitionKey == BossAcquisitionKey;
		}, BossCards);
		if (!AddDeterministicRewardFromPool(BossCards, RandomState, Picks))
		{
			AddDeterministicRewardFromPool(RareCards, RandomState, Picks);
		}
	}
	else if (NodeKind == EGameXXKNodeKind::Elite)
	{
		if (!AddDeterministicRewardFromPool(RareCards, RandomState, Picks))
		{
			AddDeterministicRewardFromPool(AllEligible, RandomState, Picks);
		}
	}
	else
	{
		AddDeterministicRewardFromPool(GeneralOrTerrain, RandomState, Picks);
		AddDeterministicRewardFromPool(GeneralOrTerrain, RandomState, Picks);
		AddDeterministicRewardFromPool(TerrainCards, RandomState, Picks);
	}
	while (Picks.Num() < 3 && AddDeterministicRewardFromPool(AllEligible, RandomState, Picks))
	{
	}
	if (Picks.Num() != 3)
	{
		return SetFailure(OutError, TEXT("The route reward catalog cannot supply three distinct legal cards."));
	}
	Run.PendingReward.SourceNodeId = SourceNodeId;
	Run.PendingReward.ChoiceSeed = ChoiceSeed;
	Run.PendingReward.CardIds = Picks;
	Run.PendingReward.bRequiresRouteCardReplacement = Run.RouteCardIds.Num() >= MaximumRouteRewardCardCount;
	++Run.NextRewardOrdinal;
	OutCardIds = MoveTemp(Picks);
	return true;
}

bool FGameXXKCardBattleAdapter::ChoosePendingRouteReward(
	FGameXXKRuntimeState& InOutState,
	const FName RewardCardId,
	const FName ReplacedRouteCardId,
	FString* OutError)
{
	if (!EnsureCardRunInitialized(InOutState, OutError))
	{
		return false;
	}
	FGameXXKCardRunState& Run = InOutState.CardRun;
	if (RewardCardId.IsNone() || !Run.PendingReward.CardIds.Contains(RewardCardId) || !IsRouteCard(RewardCardId))
	{
		return SetFailure(OutError, TEXT("The selected route reward is not in the saved pending offer."));
	}
	const bool bAtCapacity = Run.RouteCardIds.Num() >= MaximumRouteRewardCardCount;
	if (Run.PendingReward.bRequiresRouteCardReplacement != bAtCapacity)
	{
		return SetFailure(OutError, TEXT("The saved route reward replacement requirement no longer matches the temporary deck size."));
	}
	if (bAtCapacity)
	{
		if (ReplacedRouteCardId.IsNone() || !Run.RouteCardIds.Contains(ReplacedRouteCardId))
		{
			return SetFailure(OutError, TEXT("A full route deck may replace only one existing temporary route reward card."));
		}
	}
	else if (!ReplacedRouteCardId.IsNone())
	{
		return SetFailure(OutError, TEXT("A route-card replacement is only valid at the thirty-card deck cap."));
	}
	if (GetCurrentCardCopyCount(Run, RewardCardId, bAtCapacity ? ReplacedRouteCardId : NAME_None) >= 2)
	{
		return SetFailure(OutError, TEXT("The chosen reward would create more than two copies of one CardId in the shared deck."));
	}
	if (bAtCapacity)
	{
		const int32 RemoveIndex = Run.RouteCardIds.IndexOfByKey(ReplacedRouteCardId);
		Run.RouteCardIds.RemoveAt(RemoveIndex);
	}
	Run.RouteCardIds.Add(RewardCardId);
	Run.PendingReward = FGameXXKPendingRouteCardReward();
	return true;
}

bool FGameXXKCardBattleAdapter::SkipPendingRouteReward(FGameXXKRuntimeState& InOutState, FString* OutError)
{
	if (!EnsureCardRunInitialized(InOutState, OutError))
	{
		return false;
	}
	if (InOutState.CardRun.PendingReward.CardIds.IsEmpty())
	{
		return SetFailure(OutError, TEXT("There is no pending route reward to skip."));
	}
	InOutState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
	return true;
}

void FGameXXKCardBattleAdapter::ClearRouteLocalCardState(FGameXXKRuntimeState& InOutState)
{
	FGameXXKCardRunState& Run = InOutState.CardRun;
	Run.bLoadoutLockedForRoute = false;
	Run.ActiveTemporaryQuestNpcId = NAME_None;
	Run.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
	Run.RouteCardIds.Reset();
	Run.bHasActiveCardBattle = false;
	Run.ActiveBattleSourceNodeId = INDEX_NONE;
	Run.ActiveBattle = FGameXXKCardBattleRuntime();
	Run.EnemyIntents.Reset();
	Run.NextEnemyIntentIndex = 0;
	Run.PendingReward = FGameXXKPendingRouteCardReward();
	Run.PendingEvent = FGameXXKPendingRouteEvent();
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
