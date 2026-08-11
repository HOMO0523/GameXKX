#include "GameXXKCardTypes.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKMVPRules.h"
#include "Components/Button.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UObject/UnrealType.h"

#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool BuildActiveCardBattle(UGameXXKMVPSubsystem* const Subsystem, FAutomationTestBase& Test)
	{
		return Test.TestTrue(TEXT("fixture test starts a new game"), Subsystem && Subsystem->StartGame())
			&& Test.TestTrue(TEXT("fixture test selects Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()))
			&& Test.TestTrue(TEXT("fixture test accepts the town quest"), Subsystem->AcceptQuest())
			&& Test.TestTrue(TEXT("fixture test enters the route"), Subsystem->OpenDungeonFromTownExit())
			&& Test.TestTrue(TEXT("fixture test advances the route start"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start))
			&& Test.TestTrue(TEXT("fixture test opens the card battle"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
	}

	const FGameXXKCardCombatUnit* FindUnit(
		const FGameXXKRuntimeState& State,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole PreferredRole = EGameXXKCharacterRole::Invalid)
	{
		const FGameXXKCardCombatUnit* Fallback = nullptr;
		for (const FGameXXKCardCombatUnit& Candidate : State.CardRun.ActiveBattle.Units)
		{
			if (!Candidate.bLiving || Candidate.Side != Side)
			{
				continue;
			}
			if (PreferredRole == EGameXXKCharacterRole::Invalid || Candidate.Role == PreferredRole)
			{
				return &Candidate;
			}
			Fallback = Fallback ? Fallback : &Candidate;
		}
		return Fallback;
	}

	const FGameXXKCardCombatUnit* FindUnitById(const FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}

	const FGameXXKBattleRuntimeUnit* FindLegacyUnitById(const TArray<FGameXXKBattleRuntimeUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKBattleRuntimeUnit& Candidate)
		{
			return Candidate.Id == UnitId;
		});
	}

	int32 CountLivingUnits(const FGameXXKRuntimeState& State, const EGameXXKCardTargetSide Side)
	{
		int32 Count = 0;
		for (const FGameXXKCardCombatUnit& Candidate : State.CardRun.ActiveBattle.Units)
		{
			if (Candidate.bLiving && Candidate.Side == Side)
			{
				++Count;
			}
		}

		return Count;
	}

	bool TestFixtureFlipbook(
		FAutomationTestBase& Test,
		UGameXXKMVPSubsystem* const Subsystem,
		const FGameXXKBattleRuntimeUnit& LegacyUnit,
		const bool bEnemy,
		const int32 UnitIndex)
	{
		AGameXXKBattleSceneUnitActor* const Actor = NewObject<AGameXXKBattleSceneUnitActor>();
		if (!Test.TestNotNull(*FString::Printf(TEXT("fixture creates a battle actor for %s"), *LegacyUnit.Id.ToString()), Actor))
		{
			return false;
		}
		Actor->SetMVPSubsystemForTest(Subsystem);
		Actor->ConfigureFromRuntimeUnit(
			bEnemy,
			UnitIndex,
			LegacyUnit,
			FGameXXKBattlePresentation::GetSlotNumber(Subsystem->GetRuntimeState().CardRun.ActiveBattle, LegacyUnit.Id));
		return Test.TestNotNull(
			*FString::Printf(TEXT("fixture identity resolves an approved battle flipbook for %s"), *LegacyUnit.Id.ToString()),
			Actor->GetCurrentBattleFlipbook());
	}

	int32 GetStatusStacks(const FGameXXKCardCombatUnit* const Unit, const EGameXXKCardStatus Status)
	{
		if (!Unit)
		{
			return INDEX_NONE;
		}
		if (const FGameXXKCardStatusStack* const Stack = Unit->Statuses.FindByPredicate([Status](const FGameXXKCardStatusStack& Candidate)
		{
			return Candidate.Status == Status;
		}))
		{
			return Stack->Stacks;
		}
		return 0;
	}

	bool HasOnlyLivingFixtureOwners(const FGameXXKRuntimeState& FixtureView, const TArray<FGameXXKCardInstance>& CardInstances)
	{
		for (const FGameXXKCardInstance& CardInstance : CardInstances)
		{
			const FGameXXKCardCombatUnit* const Owner = FindUnitById(FixtureView, CardInstance.OwnerUnitId);
			if (!Owner || !Owner->bLiving)
			{
				return false;
			}
		}
		return true;
	}

	template <typename TSubsystem>
	auto RunBattleHudFixtureContract(FAutomationTestBase& Test, int) -> decltype(
		std::declval<TSubsystem&>().ApplyBattleHudFixtureForTest(std::declval<FString&>()),
		std::declval<TSubsystem&>().ClearBattleHudFixtureForTest(),
		bool())
	{
		Test.TestNotNull(TEXT("fixture apply seam is reflected"), TSubsystem::StaticClass()->FindFunctionByName(TEXT("ApplyBattleHudFixtureForTest")));
		Test.TestNotNull(TEXT("fixture clear seam is reflected"), TSubsystem::StaticClass()->FindFunctionByName(TEXT("ClearBattleHudFixtureForTest")));

		UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
		TSubsystem* const NoBattleSubsystem = NewObject<TSubsystem>(TestGameInstance);
		FString NoBattleError;
		Test.TestFalse(TEXT("fixture rejects a state without an active card battle"), NoBattleSubsystem->ApplyBattleHudFixtureForTest(NoBattleError));
		Test.TestFalse(TEXT("fixture rejection reports a concrete reason"), NoBattleError.IsEmpty());
		NoBattleSubsystem->ClearBattleHudFixtureForTest();

		TSubsystem* const Subsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(Subsystem, Test))
		{
			return false;
		}
		Test.TestEqual(TEXT("fixture source is the active battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
		const FGameXXKRuntimeState RawBefore = Subsystem->GetRuntimeStateCopy();
		const FGameXXKCardCombatUnit* const RawHero = FindUnit(RawBefore, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero);
		const FGameXXKCardCombatUnit* const RawEnemy = FindUnit(RawBefore, EGameXXKCardTargetSide::Enemy);
		Test.TestNotNull(TEXT("fixture source has a living party hero"), RawHero);
		Test.TestNotNull(TEXT("fixture source has a living enemy"), RawEnemy);
		FString ApplyError;

		// A visual fixture is a read-only board preview, never a real enemy turn.  In
		// particular, applying it while the saved battle is between player turns must
		// not let the board's tick-driven enemy-intent presentation consume the raw run
		// and discard the fixture overlay.
		TSubsystem* const EnemyPhaseFixtureSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(EnemyPhaseFixtureSubsystem, Test))
		{
			return false;
		}
		TArray<FGameXXKCardDamageResult> EnemyPhaseStartDamageResults;
		FString EnemyPhaseStartError;
		Test.TestTrue(
			TEXT("fixture regression source can enter the saved enemy phase"),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(
				EnemyPhaseFixtureSubsystem->GetMutableRuntimeState(),
				EnemyPhaseStartDamageResults,
				&EnemyPhaseStartError));
		Test.TestEqual(
			TEXT("fixture regression source is now an enemy-phase runtime"),
			EnemyPhaseFixtureSubsystem->GetRuntimeState().CardRun.ActiveBattle.Phase,
			EGameXXKCardBattlePhase::Enemy);
		Test.TestTrue(
			TEXT("fixture still applies over an enemy-phase source"),
			EnemyPhaseFixtureSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		const FGameXXKRuntimeState EnemyPhaseFixtureView = EnemyPhaseFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestEqual(
			TEXT("fixture locks its preview to player phase instead of presenting live enemy resolution"),
			EnemyPhaseFixtureView.CardRun.ActiveBattle.Phase,
			EGameXXKCardBattlePhase::Player);
		Test.TestEqual(
			TEXT("fixture keeps its three read-only enemy intent cards after phase normalization"),
			EnemyPhaseFixtureView.CardRun.EnemyIntents.Num(),
			3);
		Test.TestEqual(
			TEXT("fixture restarts the read-only intent rail at its first card"),
			EnemyPhaseFixtureView.CardRun.NextEnemyIntentIndex,
			0);

		// Player-flow refresh touches the hidden town roster widget even while the
		// battle board is visible.  Its town-only preparation call must be a strict
		// no-op outside Town, otherwise it clears the copied fixture between the
		// board/Qi refresh and battle-scene refresh.
		TSubsystem* const BattleRefreshFixtureSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(BattleRefreshFixtureSubsystem, Test))
		{
			return false;
		}
		Test.TestTrue(
			TEXT("fixture applies before a non-town roster refresh"),
			BattleRefreshFixtureSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		Test.TestFalse(
			TEXT("town roster preparation correctly declines during battle"),
			BattleRefreshFixtureSubsystem->PrepareCompanionRosterForTown());
		const FGameXXKRuntimeState BattleRefreshFixtureView = BattleRefreshFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestEqual(
			TEXT("non-town roster preparation preserves the fixture Party Qi"),
			BattleRefreshFixtureView.CardRun.ActiveBattle.Deck.SharedEnergy,
			2);
		Test.TestEqual(
			TEXT("non-town roster preparation preserves all six fixture units"),
			BattleRefreshFixtureView.CardRun.ActiveBattle.Units.Num(),
			6);

		// The fixture is a copied visual inspection board. Its player-phase hand and
		// forecast rail must never become an alternate way to mutate the raw battle.
		TSubsystem* const ReadOnlyFixtureSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(ReadOnlyFixtureSubsystem, Test))
		{
			return false;
		}
		const FGameXXKRuntimeState ReadOnlyRawBefore = ReadOnlyFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestTrue(
			TEXT("fixture applies before board input is exercised"),
			ReadOnlyFixtureSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		UGameXXKBattleBoardWidget* const ReadOnlyFixtureBoard = NewObject<UGameXXKBattleBoardWidget>();
		ReadOnlyFixtureBoard->SetMVPSubsystem(ReadOnlyFixtureSubsystem);
		Test.TestTrue(TEXT("read-only fixture board initializes its widget tree"), ReadOnlyFixtureBoard->Initialize());
		ReadOnlyFixtureBoard->NativeConstruct();
		ReadOnlyFixtureBoard->RefreshFromState();
		const FGameXXKRuntimeState& ReadOnlyFixtureView = ReadOnlyFixtureSubsystem->GetRuntimeState();
		Test.TestEqual(TEXT("read-only fixture board initially exposes six preview units"), ReadOnlyFixtureView.CardRun.ActiveBattle.Units.Num(), 6);
		Test.TestTrue(TEXT("read-only fixture has a hand card to reject"), !ReadOnlyFixtureView.CardRun.ActiveBattle.Deck.Hand.IsEmpty());
		if (!ReadOnlyFixtureView.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			Test.TestFalse(
				TEXT("fixture hand input never starts or resolves a card play"),
				ReadOnlyFixtureBoard->ClickCardInHand(ReadOnlyFixtureView.CardRun.ActiveBattle.Deck.Hand[0].InstanceId));
		}
		Test.TestFalse(TEXT("fixture end turn never begins a real enemy phase"), ReadOnlyFixtureBoard->EndCardPlayerPhase());
		Test.TestFalse(TEXT("fixture hand buttons are visually non-interactive"), ReadOnlyFixtureBoard->IsHandCardSlotEnabledForTest(0));
		Test.TestFalse(
			TEXT("fixture end-turn button is visually non-interactive"),
			ReadOnlyFixtureBoard->GetEndTurnButtonForTest() && ReadOnlyFixtureBoard->GetEndTurnButtonForTest()->GetIsEnabled());
		Test.TestEqual(
			TEXT("fixture input preserves all six copied preview units"),
			ReadOnlyFixtureSubsystem->GetRuntimeState().CardRun.ActiveBattle.Units.Num(),
			6);
		Test.TestEqual(
			TEXT("fixture input preserves copied Party Qi"),
			ReadOnlyFixtureSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy,
			2);
		ReadOnlyFixtureSubsystem->ClearBattleHudFixtureForTest();
		const FGameXXKRuntimeState ReadOnlyRawAfter = ReadOnlyFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestEqual(
			TEXT("fixture card input leaves the raw turn phase unchanged"),
			ReadOnlyRawAfter.CardRun.ActiveBattle.Phase,
			ReadOnlyRawBefore.CardRun.ActiveBattle.Phase);
		Test.TestEqual(
			TEXT("fixture card input leaves the raw Party Qi unchanged"),
			ReadOnlyRawAfter.CardRun.ActiveBattle.Deck.SharedEnergy,
			ReadOnlyRawBefore.CardRun.ActiveBattle.Deck.SharedEnergy);
		Test.TestEqual(
			TEXT("fixture card input leaves the raw hand size unchanged"),
			ReadOnlyRawAfter.CardRun.ActiveBattle.Deck.Hand.Num(),
			ReadOnlyRawBefore.CardRun.ActiveBattle.Deck.Hand.Num());

		// Applying the fixture can happen after a real board has already started its
		// local enemy-intent animation. Advancing that stale animation must discard
		// only local presentation state; it must not resolve the raw intent.
		TSubsystem* const StaleIntentFixtureSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(StaleIntentFixtureSubsystem, Test))
		{
			return false;
		}
		UGameXXKBattleBoardWidget* const StaleIntentFixtureBoard = NewObject<UGameXXKBattleBoardWidget>();
		StaleIntentFixtureBoard->SetMVPSubsystem(StaleIntentFixtureSubsystem);
		Test.TestTrue(TEXT("stale-intent fixture board initializes its widget tree"), StaleIntentFixtureBoard->Initialize());
		StaleIntentFixtureBoard->NativeConstruct();
		Test.TestTrue(TEXT("stale-intent source starts an enemy presentation"), StaleIntentFixtureBoard->EndCardPlayerPhase());
		Test.TestEqual(
			TEXT("stale-intent source begins with the first enemy showcase active"),
			StaleIntentFixtureBoard->GetActiveEnemyIntentPresentationIndexForTest(),
			0);
		const FGameXXKRuntimeState StaleIntentRawBefore = StaleIntentFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestTrue(
			TEXT("fixture applies over the stale local enemy presentation"),
			StaleIntentFixtureSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		StaleIntentFixtureBoard->AdvanceEnemyIntentPresentationForTest(1.0f);
		Test.TestEqual(
			TEXT("fixture advance clears the stale local enemy showcase"),
			StaleIntentFixtureBoard->GetActiveEnemyIntentPresentationIndexForTest(),
			INDEX_NONE);
		Test.TestEqual(
			TEXT("fixture advance preserves its copied six-unit board"),
			StaleIntentFixtureSubsystem->GetRuntimeState().CardRun.ActiveBattle.Units.Num(),
			6);
		Test.TestEqual(
			TEXT("fixture advance keeps the copied forecast at its first intent"),
			StaleIntentFixtureSubsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex,
			0);
		StaleIntentFixtureSubsystem->ClearBattleHudFixtureForTest();
		const FGameXXKRuntimeState StaleIntentRawAfter = StaleIntentFixtureSubsystem->GetRuntimeStateCopy();
		Test.TestEqual(
			TEXT("fixture advance leaves the raw enemy intent index unchanged"),
			StaleIntentRawAfter.CardRun.NextEnemyIntentIndex,
			StaleIntentRawBefore.CardRun.NextEnemyIntentIndex);
		Test.TestEqual(
			TEXT("fixture advance leaves the raw enemy phase unchanged"),
			StaleIntentRawAfter.CardRun.ActiveBattle.Phase,
			StaleIntentRawBefore.CardRun.ActiveBattle.Phase);

		if (!Test.TestTrue(TEXT("fixture applies to a copied active card battle"), Subsystem->ApplyBattleHudFixtureForTest(ApplyError)))
		{
			Test.AddError(FString::Printf(TEXT("fixture apply error: %s"), *ApplyError));
			return false;
		}
		const FGameXXKRuntimeState FixtureView = Subsystem->GetRuntimeStateCopy();
		const FGameXXKCardCombatUnit* const FixtureHero = FindUnit(FixtureView, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero);
		const FGameXXKCardCombatUnit* const FixtureEnemy = FindUnit(FixtureView, EGameXXKCardTargetSide::Enemy);
		Test.TestEqual(TEXT("fixture exposes the desired shared Party Qi"), FixtureView.CardRun.ActiveBattle.Deck.SharedEnergy, 2);
		Test.TestNotNull(TEXT("fixture view retains a party hero"), FixtureHero);
		Test.TestNotNull(TEXT("fixture view retains an enemy"), FixtureEnemy);
		if (RawHero && FixtureHero)
		{
			Test.TestTrue(TEXT("fixture damages the hero only in the view"), FixtureHero->HP < RawHero->HP);
			Test.TestTrue(TEXT("fixture lowers hero mana only in the view"), FixtureHero->Mana < RawHero->Mana);
			Test.TestEqual(TEXT("fixture exposes armor through card runtime"), FixtureHero->Armor, 7);
		}
		if (RawEnemy && FixtureEnemy)
		{
			Test.TestTrue(TEXT("fixture damages the enemy only in the view"), FixtureEnemy->HP < RawEnemy->HP);
			Test.TestEqual(TEXT("fixture exposes poison status stacks"), GetStatusStacks(FixtureEnemy, EGameXXKCardStatus::Poison), 2);
			Test.TestEqual(TEXT("fixture exposes bleed status stacks"), GetStatusStacks(FixtureEnemy, EGameXXKCardStatus::Bleed), 3);
		}
		const FGameXXKBattleDeckState& FixtureDeck = FixtureView.CardRun.ActiveBattle.Deck;
		Test.TestTrue(TEXT("fixture draw-pile cards keep living fixture owners"), HasOnlyLivingFixtureOwners(FixtureView, FixtureDeck.DrawPile));
		Test.TestTrue(TEXT("fixture hand cards keep living fixture owners"), HasOnlyLivingFixtureOwners(FixtureView, FixtureDeck.Hand));
		Test.TestTrue(TEXT("fixture discard cards keep living fixture owners"), HasOnlyLivingFixtureOwners(FixtureView, FixtureDeck.DiscardPile));
		Test.TestTrue(TEXT("fixture pending-choice card views keep living fixture owners"), HasOnlyLivingFixtureOwners(FixtureView, FixtureDeck.PendingChoice.Candidates));
		bool bFixtureHandPreviewSucceeded = false;
		FString FixturePreviewError;
		for (const FGameXXKCardInstance& HandCard : FixtureDeck.Hand)
		{
			FGameXXKCardPlayPreview Preview;
			if (FGameXXKCardBattleAdapter::BuildCardPlayPreview(FixtureView, HandCard.InstanceId, Preview, &FixturePreviewError))
			{
				bFixtureHandPreviewSucceeded = true;
				break;
			}
		}
		Test.TestTrue(FString::Printf(TEXT("fixture keeps at least one hand card previewable by a living owner: %s"), *FixturePreviewError), bFixtureHandPreviewSucceeded);

		const FName FixtureCompanionId(TEXT("CompanionInstance.Companion_Blade_01.HudFixture"));
		const FName FixtureQuestNpcId(TEXT("Npc.TusiChief"));
		const FName FixtureMoneyRatId(TEXT("MoneyRat"));
		const FName FixtureBlackBearId(TEXT("BlackBear"));
		const FName FixtureTigerId(TEXT("Tiger"));
		const FGameXXKCardCombatUnit* const FixtureBlade = FindUnitById(FixtureView, FixtureCompanionId);
		const FGameXXKCardCombatUnit* const FixtureQuestNpc = FindUnitById(FixtureView, FixtureQuestNpcId);
		const FGameXXKCardCombatUnit* const FixtureMoneyRat = FindUnitById(FixtureView, FixtureMoneyRatId);
		const FGameXXKCardCombatUnit* const FixtureBlackBear = FindUnitById(FixtureView, FixtureBlackBearId);
		const FGameXXKCardCombatUnit* const FixtureTiger = FindUnitById(FixtureView, FixtureTigerId);
		Test.TestEqual(TEXT("fixture exposes exactly three living party previews"), CountLivingUnits(FixtureView, EGameXXKCardTargetSide::Party), 3);
		Test.TestEqual(TEXT("fixture exposes exactly three living enemy previews"), CountLivingUnits(FixtureView, EGameXXKCardTargetSide::Enemy), 3);
		Test.TestEqual(TEXT("fixture projects three party runtime units"), FixtureView.ActiveBattleParty.Num(), 3);
		Test.TestEqual(TEXT("fixture projects three enemy runtime units"), FixtureView.ActiveBattleEnemies.Num(), 3);
		Test.TestNotNull(TEXT("fixture includes the stable Blade companion instance"), FixtureBlade);
		Test.TestNotNull(TEXT("fixture includes the named Tusi Chief task NPC"), FixtureQuestNpc);
		Test.TestNotNull(TEXT("fixture includes MoneyRat as enemy 1P"), FixtureMoneyRat);
		Test.TestNotNull(TEXT("fixture includes BlackBear as enemy 2P"), FixtureBlackBear);
		Test.TestNotNull(TEXT("fixture includes Tiger as enemy 3P"), FixtureTiger);
		if (FixtureHero && FixtureBlade && FixtureQuestNpc && FixtureMoneyRat && FixtureBlackBear && FixtureTiger)
		{
			Test.TestEqual(TEXT("fixture retains the hero role"), FixtureHero->Role, EGameXXKCharacterRole::Hero);
			Test.TestEqual(TEXT("fixture assigns the companion role"), FixtureBlade->Role, EGameXXKCharacterRole::Blade);
			Test.TestEqual(TEXT("fixture assigns the task NPC role"), FixtureQuestNpc->Role, EGameXXKCharacterRole::QuestNpc);
			Test.TestEqual(TEXT("fixture gives the companion a real mana pool"), FixtureBlade->MaxMana, 22);
			Test.TestEqual(TEXT("fixture gives the task NPC a real mana pool"), FixtureQuestNpc->MaxMana, 24);
			Test.TestTrue(TEXT("fixture starts the companion with usable mana"), FixtureBlade->Mana > 0);
			Test.TestTrue(TEXT("fixture starts the task NPC with usable mana"), FixtureQuestNpc->Mana > 0);
			Test.TestEqual(TEXT("fixture keeps monster mana disabled for MoneyRat"), FixtureMoneyRat->MaxMana, 0);
			Test.TestEqual(TEXT("fixture keeps monster mana disabled for BlackBear"), FixtureBlackBear->MaxMana, 0);
			Test.TestEqual(TEXT("fixture keeps monster mana disabled for Tiger"), FixtureTiger->MaxMana, 0);
			Test.TestEqual(TEXT("fixture keeps hero armor in the authoritative runtime"), FixtureHero->Armor, 7);
			Test.TestEqual(TEXT("fixture assigns the companion to 我 1P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureBlade->UnitId), 1);
			Test.TestEqual(TEXT("fixture assigns the hero to the central 我 2P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureHero->UnitId), 2);
			Test.TestEqual(TEXT("fixture assigns the task NPC to 我 3P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureQuestNpc->UnitId), 3);
			Test.TestEqual(TEXT("fixture assigns MoneyRat to 敌 1P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureMoneyRat->UnitId), 1);
			Test.TestEqual(TEXT("fixture assigns BlackBear to 敌 2P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureBlackBear->UnitId), 2);
			Test.TestEqual(TEXT("fixture assigns Tiger to 敌 3P"), FGameXXKBattlePresentation::GetSlotNumber(FixtureView.CardRun.ActiveBattle, FixtureTiger->UnitId), 3);
			Test.TestEqual(TEXT("fixture keeps MoneyRat at stable enemy order zero"), FixtureMoneyRat->StableSortOrder, 0);
			Test.TestEqual(TEXT("fixture keeps BlackBear at stable enemy order one"), FixtureBlackBear->StableSortOrder, 1);
			Test.TestEqual(TEXT("fixture keeps Tiger at stable enemy order two"), FixtureTiger->StableSortOrder, 2);
			Test.TestEqual(TEXT("fixture gives MoneyRat poison stacks"), GetStatusStacks(FixtureMoneyRat, EGameXXKCardStatus::Poison), 2);
			Test.TestEqual(TEXT("fixture gives MoneyRat bleed stacks"), GetStatusStacks(FixtureMoneyRat, EGameXXKCardStatus::Bleed), 3);
		}

		const FGameXXKBattleRuntimeUnit* const LegacyBlade = FindLegacyUnitById(FixtureView.ActiveBattleParty, FixtureCompanionId);
		const FGameXXKBattleRuntimeUnit* const LegacyHero = FindLegacyUnitById(FixtureView.ActiveBattleParty, TEXT("Player"));
		const FGameXXKBattleRuntimeUnit* const LegacyQuestNpc = FindLegacyUnitById(FixtureView.ActiveBattleParty, FixtureQuestNpcId);
		const FGameXXKBattleRuntimeUnit* const LegacyMoneyRat = FindLegacyUnitById(FixtureView.ActiveBattleEnemies, FixtureMoneyRatId);
		const FGameXXKBattleRuntimeUnit* const LegacyBlackBear = FindLegacyUnitById(FixtureView.ActiveBattleEnemies, FixtureBlackBearId);
		const FGameXXKBattleRuntimeUnit* const LegacyTiger = FindLegacyUnitById(FixtureView.ActiveBattleEnemies, FixtureTigerId);
		Test.TestNotNull(TEXT("fixture legacy projection includes Blade"), LegacyBlade);
		Test.TestNotNull(TEXT("fixture legacy projection includes hero"), LegacyHero);
		Test.TestNotNull(TEXT("fixture legacy projection includes Tusi Chief"), LegacyQuestNpc);
		Test.TestNotNull(TEXT("fixture legacy projection includes MoneyRat"), LegacyMoneyRat);
		Test.TestNotNull(TEXT("fixture legacy projection includes BlackBear"), LegacyBlackBear);
		Test.TestNotNull(TEXT("fixture legacy projection includes Tiger"), LegacyTiger);
		if (LegacyBlade && LegacyHero && LegacyQuestNpc && LegacyMoneyRat && LegacyBlackBear && LegacyTiger)
		{
			TestFixtureFlipbook(Test, Subsystem, *LegacyBlade, false, 0);
			TestFixtureFlipbook(Test, Subsystem, *LegacyHero, false, 1);
			TestFixtureFlipbook(Test, Subsystem, *LegacyQuestNpc, false, 2);
			TestFixtureFlipbook(Test, Subsystem, *LegacyMoneyRat, true, 0);
			TestFixtureFlipbook(Test, Subsystem, *LegacyBlackBear, true, 1);
			TestFixtureFlipbook(Test, Subsystem, *LegacyTiger, true, 2);
		}

		Test.TestEqual(TEXT("fixture prebuilds one read-only intent per living enemy"), FixtureView.CardRun.EnemyIntents.Num(), 3);
		const auto HasFixtureIntent = [&FixtureView, FixtureHero](const FName SourceUnitId, const int32 SourceSlotNumber)
		{
			return FixtureView.CardRun.EnemyIntents.ContainsByPredicate([SourceUnitId, SourceSlotNumber, FixtureHero](const FGameXXKCardEnemyIntent& Intent)
			{
				return Intent.SourceUnitId == SourceUnitId
					&& Intent.SourceSlotNumber == SourceSlotNumber
					&& Intent.SuggestedTargetUnitId == (FixtureHero ? FixtureHero->UnitId : NAME_None)
					&& Intent.TargetSlotNumber == 2
					&& Intent.Damage > 0;
			});
		};
		Test.TestTrue(TEXT("fixture intent rail includes MoneyRat as 敌 1P"), HasFixtureIntent(FixtureMoneyRatId, 1));
		Test.TestTrue(TEXT("fixture intent rail includes BlackBear as 敌 2P"), HasFixtureIntent(FixtureBlackBearId, 2));
		Test.TestTrue(TEXT("fixture intent rail includes Tiger as 敌 3P"), HasFixtureIntent(FixtureTigerId, 3));

		Subsystem->ClearBattleHudFixtureForTest();
		const FGameXXKRuntimeState RawAfterExplicitClear = Subsystem->GetRuntimeStateCopy();
		Test.TestEqual(TEXT("explicit fixture clear restores raw shared Party Qi"), RawAfterExplicitClear.CardRun.ActiveBattle.Deck.SharedEnergy, RawBefore.CardRun.ActiveBattle.Deck.SharedEnergy);
		Test.TestEqual(TEXT("explicit fixture clear restores raw party membership"), RawAfterExplicitClear.ActiveBattleParty.Num(), RawBefore.ActiveBattleParty.Num());
		Test.TestEqual(TEXT("explicit fixture clear restores raw enemy membership"), RawAfterExplicitClear.ActiveBattleEnemies.Num(), RawBefore.ActiveBattleEnemies.Num());
		Test.TestEqual(TEXT("explicit fixture clear restores raw combat-unit membership"), RawAfterExplicitClear.CardRun.ActiveBattle.Units.Num(), RawBefore.CardRun.ActiveBattle.Units.Num());
		Test.TestEqual(TEXT("explicit fixture clear never creates permanent companions"), RawAfterExplicitClear.CardRun.CompanionRoster.PermanentCompanions.Num(), RawBefore.CardRun.CompanionRoster.PermanentCompanions.Num());
		Test.TestEqual(TEXT("explicit fixture clear never changes the route task-NPC provenance"), RawAfterExplicitClear.CardRun.ActiveTemporaryQuestNpcId, RawBefore.CardRun.ActiveTemporaryQuestNpcId);
		const FGameXXKCardCombatUnit* const HeroAfterExplicitClear = FindUnit(RawAfterExplicitClear, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero);
		const FGameXXKCardCombatUnit* const EnemyAfterExplicitClear = FindUnit(RawAfterExplicitClear, EGameXXKCardTargetSide::Enemy);
		if (RawHero && HeroAfterExplicitClear)
		{
			Test.TestEqual(TEXT("explicit clear leaves raw hero health unchanged"), HeroAfterExplicitClear->HP, RawHero->HP);
			Test.TestEqual(TEXT("explicit clear leaves raw hero mana unchanged"), HeroAfterExplicitClear->Mana, RawHero->Mana);
			Test.TestEqual(TEXT("explicit clear leaves raw hero armor unchanged"), HeroAfterExplicitClear->Armor, RawHero->Armor);
		}
		if (RawEnemy && EnemyAfterExplicitClear)
		{
			Test.TestEqual(TEXT("explicit clear leaves raw enemy health unchanged"), EnemyAfterExplicitClear->HP, RawEnemy->HP);
			Test.TestEqual(TEXT("explicit clear leaves raw enemy poison unchanged"), GetStatusStacks(EnemyAfterExplicitClear, EGameXXKCardStatus::Poison), GetStatusStacks(RawEnemy, EGameXXKCardStatus::Poison));
		}

		Test.TestTrue(TEXT("fixture reapplies before a mutable-state access"), Subsystem->ApplyBattleHudFixtureForTest(ApplyError));
		FGameXXKRuntimeState& RawAfterMutableAccess = Subsystem->GetMutableRuntimeState();
		Test.TestEqual(TEXT("mutable state access clears the fixture view"), RawAfterMutableAccess.CardRun.ActiveBattle.Deck.SharedEnergy, RawBefore.CardRun.ActiveBattle.Deck.SharedEnergy);
		Test.TestEqual(TEXT("mutable state access never persists fixture party members"), RawAfterMutableAccess.ActiveBattleParty.Num(), RawBefore.ActiveBattleParty.Num());
		Test.TestEqual(TEXT("mutable state access never persists fixture enemies"), RawAfterMutableAccess.ActiveBattleEnemies.Num(), RawBefore.ActiveBattleEnemies.Num());
		Test.TestEqual(TEXT("mutable state access never persists fixture combat units"), RawAfterMutableAccess.CardRun.ActiveBattle.Units.Num(), RawBefore.CardRun.ActiveBattle.Units.Num());

		Test.TestTrue(TEXT("fixture reapplies before a direct lifecycle exit"), Subsystem->ApplyBattleHudFixtureForTest(ApplyError));
		Test.TestTrue(TEXT("world-map exit succeeds from the raw battle state"), Subsystem->OpenWorldMap());
		Test.TestEqual(TEXT("direct lifecycle exit clears the fixture view"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);

		TSubsystem* const FailureExitSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(FailureExitSubsystem, Test))
		{
			return false;
		}
		Test.TestTrue(TEXT("fixture applies before the failed-dungeon facade exit"), FailureExitSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		Test.TestTrue(TEXT("FailDungeonToTown succeeds from the raw active battle"), FailureExitSubsystem->FailDungeonToTown());
		Test.TestEqual(TEXT("FailDungeonToTown clears the fixture overlay instead of exposing its stale battle screen"), FailureExitSubsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
		Test.TestFalse(TEXT("FailDungeonToTown clears the raw active battle after removing the overlay"), FailureExitSubsystem->GetRuntimeState().CardRun.bHasActiveCardBattle);

		TSubsystem* const HealingMutationSubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(HealingMutationSubsystem, Test))
		{
			return false;
		}
		FGameXXKRuntimeState& HealingRawState = HealingMutationSubsystem->GetMutableRuntimeState();
		HealingRawState.PlayerHP = FMath::Max(1, HealingRawState.PlayerMaxHP - 10);
		Test.TestTrue(TEXT("healing facade fixture explicitly grants one healing powder"),
			UGameXXKMVPRules::AddItem(HealingRawState, UGameXXKMVPRules::ItemHealingPowder(), 1));
		const int32 RawHealingUnitCount = HealingRawState.CardRun.ActiveBattle.Units.Num();
		const int32 RawHealingEnemyCount = HealingRawState.ActiveBattleEnemies.Num();
		Test.TestTrue(TEXT("fixture applies before the direct healing facade mutation"), HealingMutationSubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		Test.TestTrue(TEXT("UseHealingItem succeeds against the raw damaged player state"), HealingMutationSubsystem->UseHealingItem());
		const FGameXXKRuntimeState& HealedRawView = HealingMutationSubsystem->GetRuntimeState();
		Test.TestEqual(TEXT("UseHealingItem clears the fixture overlay and exposes raw player health"), HealedRawView.PlayerHP, HealedRawView.PlayerMaxHP);
		Test.TestEqual(TEXT("UseHealingItem clears the fixture overlay and restores raw battle-unit count"), HealedRawView.CardRun.ActiveBattle.Units.Num(), RawHealingUnitCount);
		Test.TestEqual(TEXT("UseHealingItem clears the fixture overlay and restores raw enemy count"), HealedRawView.ActiveBattleEnemies.Num(), RawHealingEnemyCount);

		TSubsystem* const FailedReapplySubsystem = NewObject<TSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(FailedReapplySubsystem, Test))
		{
			return false;
		}
		Test.TestTrue(TEXT("fixture applies before the failed re-apply regression"), FailedReapplySubsystem->ApplyBattleHudFixtureForTest(ApplyError));
		FStructProperty* const RuntimeStateProperty = FindFProperty<FStructProperty>(TSubsystem::StaticClass(), TEXT("RuntimeState"));
		FGameXXKRuntimeState* const RawStateForFailedReapply = RuntimeStateProperty
			? RuntimeStateProperty->ContainerPtrToValuePtr<FGameXXKRuntimeState>(FailedReapplySubsystem)
			: nullptr;
		Test.TestNotNull(TEXT("fixture regression locates raw runtime state for a no-clear mutation"), RawStateForFailedReapply);
		if (RawStateForFailedReapply)
		{
			RawStateForFailedReapply->Screen = EGameXXKScreen::WorldMap;
			FString FailedReapplyError;
			Test.TestFalse(TEXT("fixture re-apply rejects the deliberately invalid raw screen"), FailedReapplySubsystem->ApplyBattleHudFixtureForTest(FailedReapplyError));
			Test.TestFalse(TEXT("failed re-apply reports a concrete reason"), FailedReapplyError.IsEmpty());
			Test.TestEqual(TEXT("failed re-apply clears the prior overlay instead of leaving it stale"), FailedReapplySubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
		}
		return true;
	}

	template <typename TSubsystem>
	bool RunBattleHudFixtureContract(FAutomationTestBase& Test, long)
	{
		Test.AddError(TEXT("Battle HUD fixture seam is not implemented."));
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleHudFixtureTest,
	"GameXXK.MVP.Battle.HudFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleHudFixtureTest::RunTest(const FString& Parameters)
{
	TestNotNull(TEXT("Board Party Qi accessor is reflected"), UGameXXKBattleBoardWidget::StaticClass()->FindFunctionByName(TEXT("GetPartyQiWidgetForTest")));
	TestNotNull(TEXT("Board hand accessor is reflected"), UGameXXKBattleBoardWidget::StaticClass()->FindFunctionByName(TEXT("GetHandCardBoxForTest")));
	TestNotNull(TEXT("Board end-turn accessor is reflected"), UGameXXKBattleBoardWidget::StaticClass()->FindFunctionByName(TEXT("GetEndTurnButtonForTest")));
	TestNotNull(TEXT("Party Qi value accessor is reflected"), UGameXXKBattlePartyQiWidget::StaticClass()->FindFunctionByName(TEXT("GetSharedQiForTest")));
	return RunBattleHudFixtureContract<UGameXXKMVPSubsystem>(*this, 0);
}

#endif
