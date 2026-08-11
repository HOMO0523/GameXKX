#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionCatalog.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKAllCardPlayabilityAuditTest
{
	constexpr int32 AuditFillerCount = 18;

	const TArray<EGameXXKCardTerrain>& EveryTerrain()
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

	void AddStatus(FGameXXKCardCombatUnit& Unit, const EGameXXKCardStatus Status, const int32 Stacks)
	{
		GameXXKCardRules::AddCombatStatus(Unit, Status, Stacks);
	}

	void PrimeRepresentativeStatuses(FGameXXKCardCombatUnit& Unit)
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

	FGameXXKCardCombatUnit MakeUnit(
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

	FGameXXKCardInstance MakeCard(
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

	int32 RequiredOwnerCohortSize(const EGameXXKCardOwner Owner)
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

	bool BuildRuntime(
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

	bool DrainChoicesAndAutomaticQueue(
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

	bool HasMissableGateAndBaseEffect(const FGameXXKCardDefinition& Definition)
	{
		bool bHasMissableGate = false;
		bool bHasUnconditionalBase = false;
		for (const FGameXXKCardEffect& Effect : Definition.Effects)
		{
			bHasUnconditionalBase |= Effect.Condition.Type == EGameXXKCardEffectConditionType::None;
			switch (Effect.Condition.Type)
			{
			case EGameXXKCardEffectConditionType::TargetHasStatus:
			case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:
			case EGameXXKCardEffectConditionType::OwnerHasStatus:
			case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:
			case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:
			case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:
			case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:
				bHasMissableGate = true;
				break;
			default:
				break;
			}
		}
		return bHasMissableGate && bHasUnconditionalBase;
	}

	void ClearMissableGates(FGameXXKCardBattleRuntime& Runtime)
	{
		for (FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Unit.Statuses.Reset();
			Unit.Armor = 0;
			Unit.HP = Unit.MaxHP;
		}
	}

	bool ResolveWithFirstLegalTarget(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName PlayedInstanceId,
		const FString& Context)
	{
		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, PlayedInstanceId, Preview, &Error) || !Preview.bCanPlay)
		{
			Test.AddError(FString::Printf(TEXT("%s condition-miss preview failed: %s"), *Context, *Error));
			return false;
		}
		FName SelectedTargetId = NAME_None;
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
				[](const FGameXXKCardTargetCandidateView& View)
				{
					return View.bCanSelect;
				});
			if (!Candidate)
			{
				Test.AddError(FString::Printf(TEXT("%s condition-miss preview has no legal target"), *Context));
				return false;
			}
			SelectedTargetId = Candidate->UnitId;
		}
		FGameXXKCardPlayResult Result;
		if (!GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, SelectedTargetId, Result, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s condition-miss resolution failed: %s"), *Context, *Error));
			return false;
		}
		return DrainChoicesAndAutomaticQueue(Test, Runtime, Context);
	}

	FGameXXKCardCombatUnit* FindUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	struct FMixedLoadoutFixture
	{
		FGameXXKCardBattleRuntime Runtime;
		FName HeroPlayedInstanceId = NAME_None;
		FName PartnerPlayedInstanceId = NAME_None;
		FName NpcPlayedInstanceId = NAME_None;
		FName PartnerUnitId = NAME_None;
		FName NpcUnitId = NAME_None;
	};

	bool BuildMixedLoadout(
		FAutomationTestBase& Test,
		const EGameXXKCharacterRole PartnerRole,
		const FGameXXKQuestNpcDefinition& Npc,
		const bool bUseHeroMageStarter,
		const int32 Seed,
		FMixedLoadoutFixture& OutFixture)
	{
		const TCHAR* DefaultHeroCardIds[] = {
			TEXT("Hero.Generic.HeYuZhan"),
			TEXT("Hero.Generic.QingFengYiShi"),
			TEXT("Hero.Generic.FengShenBu"),
			TEXT("Hero.Generic.SuiYanJi"),
			TEXT("Hero.Generic.GuiYuanShu"),
			TEXT("Hero.Generic.HengJianShouShi"),
			TEXT("Hero.Generic.NingShenTuNa"),
			TEXT("Hero.Generic.GuanXi")};
		TArray<const FGameXXKCardDefinition*> HeroDefinitions;
		for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(DefaultHeroCardIds)); ++Index)
		{
			const FName CardId = bUseHeroMageStarter && Index == 0
				? FName(TEXT("Hero.Mage.YanXuLiaoYuan"))
				: FName(DefaultHeroCardIds[Index]);
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (!Definition)
			{
				Test.AddError(FString::Printf(TEXT("mixed loadout is missing Hero card %s"), *CardId.ToString()));
				return false;
			}
			HeroDefinitions.Add(Definition);
		}

		TArray<const FGameXXKCardDefinition*> PartnerDefinitions;
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (Definition.Owner == EGameXXKCardOwner::Profession && Definition.Role == PartnerRole)
			{
				PartnerDefinitions.Add(&Definition);
				if (PartnerDefinitions.Num() == 5)
				{
					break;
				}
			}
		}
		if (PartnerDefinitions.Num() != 5 || Npc.FixedCardIds.Num() != 4)
		{
			Test.AddError(FString::Printf(
				TEXT("mixed loadout cannot build role %d five-card or %s four-card source pool"),
				static_cast<int32>(PartnerRole),
				*Npc.NpcId.ToString()));
			return false;
		}

		TArray<const FGameXXKCardDefinition*> NpcDefinitions;
		for (int32 Index = 0; Index < 3; ++Index)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(Npc.FixedCardIds[Index]);
			if (!Definition)
			{
				Test.AddError(FString::Printf(TEXT("mixed loadout is missing NPC card %s"), *Npc.FixedCardIds[Index].ToString()));
				return false;
			}
			NpcDefinitions.Add(Definition);
		}

		OutFixture.PartnerUnitId = FName(*FString::Printf(TEXT("Audit.Partner.%d"), static_cast<int32>(PartnerRole)));
		OutFixture.NpcUnitId = Npc.NpcId;
		TArray<FGameXXKCardInstance> Cards;
		int32 Ordinal = 0;
		for (int32 Index = 0; Index < HeroDefinitions.Num(); ++Index)
		{
			const FName InstanceId(*FString::Printf(TEXT("Mixed.Hero.%d"), Index));
			Cards.Add(MakeCard(InstanceId, *HeroDefinitions[Index], TEXT("Hero"), Ordinal++));
			if (Index == 0)
			{
				OutFixture.HeroPlayedInstanceId = InstanceId;
			}
		}
		for (int32 Index = 0; Index < PartnerDefinitions.Num(); ++Index)
		{
			const FName InstanceId(*FString::Printf(TEXT("Mixed.Partner.%d"), Index));
			Cards.Add(MakeCard(InstanceId, *PartnerDefinitions[Index], OutFixture.PartnerUnitId, Ordinal++));
			if (Index == 0)
			{
				OutFixture.PartnerPlayedInstanceId = InstanceId;
			}
		}
		for (int32 Index = 0; Index < NpcDefinitions.Num(); ++Index)
		{
			const FName InstanceId(*FString::Printf(TEXT("Mixed.Npc.%d"), Index));
			Cards.Add(MakeCard(InstanceId, *NpcDefinitions[Index], OutFixture.NpcUnitId, Ordinal++));
			if (Index == 0)
			{
				OutFixture.NpcPlayedInstanceId = InstanceId;
			}
		}

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0, 80),
			MakeUnit(OutFixture.PartnerUnitId, EGameXXKCardTargetSide::Party, PartnerRole, 1, 70),
			MakeUnit(OutFixture.NpcUnitId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 2, 60),
			MakeUnit(TEXT("Mixed.Enemy.A"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 100),
			MakeUnit(TEXT("Mixed.Enemy.B"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11, 100)};
		FString Error;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutFixture.Runtime,
			Cards,
			Units,
			EGameXXKCardTerrain::Plain,
			Seed,
			&Error))
		{
			Test.AddError(FString::Printf(TEXT("mixed loadout initialization failed: %s"), *Error));
			return false;
		}
		OutFixture.Runtime.Deck.Hand.Reset();
		OutFixture.Runtime.Deck.DrawPile.Reset();
		OutFixture.Runtime.Deck.DiscardPile.Reset();
		OutFixture.Runtime.Deck.ExhaustPile.Reset();
		OutFixture.Runtime.Deck.PendingAutomaticHandCards.Reset();
		for (const FGameXXKCardInstance& Card : Cards)
		{
			const bool bPlayedCard = Card.InstanceId == OutFixture.HeroPlayedInstanceId
				|| Card.InstanceId == OutFixture.PartnerPlayedInstanceId
				|| Card.InstanceId == OutFixture.NpcPlayedInstanceId;
			(bPlayedCard ? OutFixture.Runtime.Deck.Hand : OutFixture.Runtime.Deck.DrawPile).Add(Card);
		}
		OutFixture.Runtime.EquippedHeroCardIds.Reset();
		for (const FGameXXKCardDefinition* HeroDefinition : HeroDefinitions)
		{
			OutFixture.Runtime.EquippedHeroCardIds.Add(HeroDefinition->Id);
		}
		OutFixture.Runtime.Deck.SharedEnergy = 50;
		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutFixture.Runtime, &Error))
		{
			Test.AddError(FString::Printf(TEXT("mixed loadout fixture is invalid: %s"), *Error));
			return false;
		}
		return true;
	}

	bool PlayMixedCard(
		FAutomationTestBase& Test,
		FGameXXKCardBattleRuntime& Runtime,
		const FName InstanceId,
		const FString& Context,
		const bool bAssertExactPayment)
	{
		FGameXXKCardPlayPreview Preview;
		FString Error;
		if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error) || !Preview.bCanPlay)
		{
			Test.AddError(FString::Printf(TEXT("%s mixed preview failed: %s"), *Context, *Error));
			return false;
		}
		FName SelectedTargetId = NAME_None;
		if (Preview.TargetRequest.bRequiresManualSelection)
		{
			const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
				[](const FGameXXKCardTargetCandidateView& View)
				{
					return View.bCanSelect;
				});
			if (!Candidate)
			{
				Test.AddError(FString::Printf(TEXT("%s mixed preview has no legal target"), *Context));
				return false;
			}
			SelectedTargetId = Candidate->UnitId;
		}
		const int32 EnergyBefore = Runtime.Deck.SharedEnergy;
		FGameXXKCardCombatUnit* OwnerBefore = FindUnit(Runtime, Preview.OwnerUnitId);
		const int32 ManaBefore = OwnerBefore ? OwnerBefore->Mana : 0;
		const int32 ActiveBefore = Runtime.ActiveCardsPlayedThisRound;
		FGameXXKCardPlayResult Result;
		if (!GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, SelectedTargetId, Result, &Error))
		{
			Test.AddError(FString::Printf(TEXT("%s mixed resolution failed: %s"), *Context, *Error));
			return false;
		}
		if (Runtime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand)
		{
			for (const FGameXXKCardInstance& Candidate : Runtime.Deck.PendingChoice.Candidates)
			{
				Test.TestEqual(
					FString::Printf(TEXT("%s search candidate stays with its owner"), *Context),
					Candidate.OwnerUnitId,
					Result.OwnerUnitId);
			}
		}
		if (bAssertExactPayment)
		{
			const FGameXXKCardCombatUnit* OwnerAfter = FindUnit(Runtime, Preview.OwnerUnitId);
			Test.TestEqual(
				FString::Printf(TEXT("%s pays shared Energy exactly once"), *Context),
				Runtime.Deck.SharedEnergy,
				EnergyBefore - Preview.EffectiveEnergyCost);
			if (OwnerAfter)
			{
				Test.TestEqual(
					FString::Printf(TEXT("%s pays owner Mana exactly once"), *Context),
					OwnerAfter->Mana,
					ManaBefore - Preview.EffectiveManaCost);
			}
		}
		if (!DrainChoicesAndAutomaticQueue(Test, Runtime, Context))
		{
			return false;
		}
		Test.TestTrue(
			FString::Printf(TEXT("%s advances active count only for the real play"), *Context),
			Runtime.ActiveCardsPlayedThisRound > ActiveBefore);
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKAllCardPlayabilityAuditTest,
	"GameXXK.Data.AllCards.Playability.All198EveryTerrain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKAllCardPlayabilityAuditTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKAllCardPlayabilityAuditTest;
	const TArray<FGameXXKCardDefinition>& Definitions = FGameXXKCardCatalog::GetAllCardDefinitions();
	TestEqual(TEXT("all-card audit sees exactly 198 definitions"), Definitions.Num(), 198);

	TMap<FName, int32> SuccessfulTerrainsByCard;
	int32 SuccessfulResolutionCount = 0;
	int32 ConditionMissCandidateCount = 0;
	int32 SuccessfulConditionMissCount = 0;
	for (int32 DefinitionIndex = 0; DefinitionIndex < Definitions.Num(); ++DefinitionIndex)
	{
		const FGameXXKCardDefinition& Definition = Definitions[DefinitionIndex];
		for (int32 TerrainIndex = 0; TerrainIndex < EveryTerrain().Num(); ++TerrainIndex)
		{
			const EGameXXKCardTerrain Terrain = EveryTerrain()[TerrainIndex];
			const FString Context = FString::Printf(
				TEXT("%s terrain=%d"),
				*Definition.Id.ToString(),
				static_cast<int32>(Terrain));
			FGameXXKCardBattleRuntime Runtime;
			FName PlayedInstanceId;
			if (!BuildRuntime(*this, Definition, Terrain, 73000 + DefinitionIndex * 10 + TerrainIndex, Runtime, PlayedInstanceId))
			{
				continue;
			}

			FGameXXKCardPlayPreview Preview;
			FString Error;
			if (!GameXXKCardRules::BuildCardPlayPreview(Runtime, PlayedInstanceId, Preview, &Error))
			{
				AddError(FString::Printf(TEXT("%s preview failed: %s"), *Context, *Error));
				continue;
			}
			if (!TestTrue(FString::Printf(TEXT("%s preview is playable"), *Context), Preview.bCanPlay))
			{
				continue;
			}

			FName SelectedTargetId = NAME_None;
			if (Preview.TargetRequest.bRequiresManualSelection)
			{
				const FGameXXKCardBattleRuntime BeforeRejectedTarget = Runtime;
				FGameXXKCardPlayResult RejectedResult;
				FString RejectedError;
				TestFalse(
					FString::Printf(TEXT("%s rejects a missing selected target"), *Context),
					GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, NAME_None, RejectedResult, &RejectedError));
				TestFalse(FString::Printf(TEXT("%s explains the missing target"), *Context), RejectedError.IsEmpty());
				TestTrue(
					FString::Printf(TEXT("%s missing-target rejection is atomic"), *Context),
					FGameXXKCardBattleRuntime::StaticStruct()->CompareScriptStruct(
						&Runtime,
						&BeforeRejectedTarget,
						PPF_None));

				const FGameXXKCardTargetCandidateView* Candidate = Preview.TargetRequest.CandidateViews.FindByPredicate(
					[](const FGameXXKCardTargetCandidateView& View)
					{
						return View.bCanSelect;
					});
				if (!Candidate)
				{
					AddError(FString::Printf(TEXT("%s has no legal manual target"), *Context));
					continue;
				}
				SelectedTargetId = Candidate->UnitId;
			}

			FGameXXKCardPlayResult Result;
			Error.Reset();
			if (!GameXXKCardRules::ResolveCardPlay(Runtime, PlayedInstanceId, SelectedTargetId, Result, &Error))
			{
				AddError(FString::Printf(TEXT("%s active resolution failed: %s"), *Context, *Error));
				continue;
			}
			if (!DrainChoicesAndAutomaticQueue(*this, Runtime, Context))
			{
				continue;
			}
			if (!GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error))
			{
				AddError(FString::Printf(TEXT("%s leaves an invalid runtime: %s"), *Context, *Error));
				continue;
			}

			TestEqual(FString::Printf(TEXT("%s result keeps stable CardId"), *Context), Result.CardId, Definition.Id);
			TestEqual(FString::Printf(TEXT("%s result keeps active origin"), *Context), Result.ResolutionOrigin, EGameXXKCardResolutionOrigin::ActivePlay);
			TestTrue(FString::Printf(TEXT("%s counts at least one active play"), *Context), Runtime.ActiveCardsPlayedThisRound >= 1);
			TestFalse(
				FString::Printf(TEXT("%s never leaves candidates under Invalid choice"), *Context),
				Runtime.Deck.PendingChoice.Kind == EGameXXKCardPendingChoiceKind::Invalid
					&& !Runtime.Deck.PendingChoice.Candidates.IsEmpty());
			SuccessfulTerrainsByCard.FindOrAdd(Definition.Id) += 1;
			++SuccessfulResolutionCount;
		}

		if (HasMissableGateAndBaseEffect(Definition))
		{
			++ConditionMissCandidateCount;
			FGameXXKCardBattleRuntime MissRuntime;
			FName MissPlayedInstanceId;
			if (BuildRuntime(
					*this,
					Definition,
					EGameXXKCardTerrain::Plain,
					76000 + DefinitionIndex,
					MissRuntime,
					MissPlayedInstanceId))
			{
				ClearMissableGates(MissRuntime);
				const FString MissContext = FString::Printf(TEXT("%s condition-miss"), *Definition.Id.ToString());
				if (ResolveWithFirstLegalTarget(*this, MissRuntime, MissPlayedInstanceId, MissContext))
				{
					FString MissValidationError;
					if (GameXXKCardRules::ValidateCardBattleRuntime(MissRuntime, &MissValidationError))
					{
						++SuccessfulConditionMissCount;
					}
					else
					{
						AddError(FString::Printf(TEXT("%s leaves invalid condition-miss runtime: %s"), *Definition.Id.ToString(), *MissValidationError));
					}
				}
			}
		}
	}

	TSet<FName> FullyExecutedIds;
	for (const TPair<FName, int32>& Pair : SuccessfulTerrainsByCard)
	{
		if (Pair.Value == EveryTerrain().Num())
		{
			FullyExecutedIds.Add(Pair.Key);
		}
	}
	TestEqual(TEXT("all 198 stable CardIds execute on every terrain"), FullyExecutedIds.Num(), 198);
	TestEqual(TEXT("198 cards times seven terrains resolve"), SuccessfulResolutionCount, 198 * EveryTerrain().Num());
	TestTrue(TEXT("the catalog contains cards with a missable condition and an unconditional base"), ConditionMissCandidateCount > 0);
	TestEqual(TEXT("every missable-condition card still resolves its base"), SuccessfulConditionMissCount, ConditionMissCandidateCount);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMixedEightFiveThreeAuditTest,
	"GameXXK.Data.AllCards.Playability.MixedEightFiveThreeMatrix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMixedEightFiveThreeAuditTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKAllCardPlayabilityAuditTest;
	const EGameXXKCharacterRole Roles[] = {
		EGameXXKCharacterRole::Blade,
		EGameXXKCharacterRole::Guard,
		EGameXXKCharacterRole::Healer,
		EGameXXKCharacterRole::Hunter,
		EGameXXKCharacterRole::Sorcerer,
		EGameXXKCharacterRole::FormationMaster};
	const FName NpcIds[] = {
		TEXT("Npc.TusiChief"),
		TEXT("Npc.SongJinBao"),
		TEXT("Npc.ZhouGuangZu"),
		TEXT("Npc.JinGui"),
		TEXT("Npc.YueBai"),
		TEXT("Npc.QiongMeiEr")};
	TSet<EGameXXKCharacterRole> CoveredRoles;
	TSet<FName> CoveredNpcIds;
	for (int32 Index = 0; Index < static_cast<int32>(UE_ARRAY_COUNT(Roles)); ++Index)
	{
		const FGameXXKQuestNpcDefinition* Npc = FGameXXKCompanionCatalog::FindQuestNpcDefinition(NpcIds[Index]);
		if (!TestNotNull(FString::Printf(TEXT("mixed NPC %s resolves"), *NpcIds[Index].ToString()), Npc))
		{
			continue;
		}
		const bool bTaskOverlapCase = Roles[Index] == EGameXXKCharacterRole::Sorcerer;
		FMixedLoadoutFixture Fixture;
		if (!BuildMixedLoadout(*this, Roles[Index], *Npc, bTaskOverlapCase, 78000 + Index, Fixture))
		{
			continue;
		}
		TestEqual(TEXT("mixed loadout contains exactly sixteen 8+5+3 instances"), Fixture.Runtime.Deck.ActiveInstanceIds.Num(), 16);
		TestEqual(TEXT("mixed loadout locks exactly eight Hero cards"), Fixture.Runtime.EquippedHeroCardIds.Num(), 8);
		if (!PlayMixedCard(*this, Fixture.Runtime, Fixture.HeroPlayedInstanceId, FString::Printf(TEXT("mixed[%d] Hero"), Index), true)
			|| !PlayMixedCard(*this, Fixture.Runtime, Fixture.NpcPlayedInstanceId, FString::Printf(TEXT("mixed[%d] NPC"), Index), false)
			|| !PlayMixedCard(*this, Fixture.Runtime, Fixture.PartnerPlayedInstanceId, FString::Printf(TEXT("mixed[%d] partner"), Index), false))
		{
			continue;
		}
		if (bTaskOverlapCase)
		{
			TestTrue(TEXT("task-overlap loadout keeps the Hero eight-card task active"), Fixture.Runtime.HeroSpellTask.bActive);
			TestTrue(TEXT("task-overlap loadout keeps the Sorcerer five-card task active"), Fixture.Runtime.SorcererPartnerTasks.ContainsByPredicate(
				[&Fixture](const FGameXXKSorcererPartnerTaskRuntime& Task)
				{
					return Task.bActive && Task.OwnerUnitId == Fixture.PartnerUnitId;
				}));
			TestTrue(TEXT("task-overlap loadout keeps the NPC three-card task active"), Fixture.Runtime.TaskNpcSpellTasks.ContainsByPredicate(
				[&Fixture](const FGameXXKTaskNpcSpellTaskRuntime& Task)
				{
					return Task.bActive && Task.OwnerUnitId == Fixture.NpcUnitId;
				}));
		}
		FString Error;
		TestTrue(
			FString::Printf(TEXT("mixed[%d] runtime remains valid: %s"), Index, *Error),
			GameXXKCardRules::ValidateCardBattleRuntime(Fixture.Runtime, &Error));
		CoveredRoles.Add(Roles[Index]);
		CoveredNpcIds.Add(Npc->NpcId);
	}
	TestEqual(TEXT("all six permanent professions have an 8+5+3 mixed loadout"), CoveredRoles.Num(), 6);
	TestEqual(TEXT("all six NPCs have an 8+5+3 mixed loadout"), CoveredNpcIds.Num(), 6);
	return true;
}

#endif
