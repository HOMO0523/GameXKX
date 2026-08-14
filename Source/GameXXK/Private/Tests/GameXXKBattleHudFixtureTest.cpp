#include "GameXXKCardTypes.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKMVPRules.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
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

	FString TargetOutcomeFixtureFingerprint(const FGameXXKRuntimeState& State)
	{
		FString Result = FString::Printf(
			TEXT("screen=%d|active=%d|party=%d|enemy=%d|phase=%d|energy=%d|random=%d"),
			static_cast<int32>(State.Screen),
			State.CardRun.bHasActiveCardBattle ? 1 : 0,
			State.ActiveBattleParty.Num(),
			State.ActiveBattleEnemies.Num(),
			static_cast<int32>(State.CardRun.ActiveBattle.Phase),
			State.CardRun.ActiveBattle.Deck.SharedEnergy,
			State.CardRun.ActiveBattle.CombatRandomState);
		for (const FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			Result += FString::Printf(
				TEXT("|unit=%s,%d,%d,%d,%d,%d,%d"),
				*Unit.UnitId.ToString(),
				static_cast<int32>(Unit.Side),
				Unit.StableSortOrder,
				Unit.HP,
				Unit.MaxHP,
				Unit.Armor,
				Unit.bLiving ? 1 : 0);
		}
		for (const FGameXXKCardInstance& Card : State.CardRun.ActiveBattle.Deck.Hand)
		{
			Result += FString::Printf(
				TEXT("|hand=%s,%s,%s"),
				*Card.InstanceId.ToString(),
				*Card.CardId.ToString(),
				*Card.OwnerUnitId.ToString());
		}
		return Result;
	}

	FBox2D TargetOutcomeProxyRect(const UButton* const Proxy)
	{
		const UCanvasPanelSlot* const Slot = Proxy ? Cast<UCanvasPanelSlot>(Proxy->Slot) : nullptr;
		if (!Slot)
		{
			return FBox2D(EForceInit::ForceInit);
		}
		const FVector2D StageSize(1920.0f, 1080.0f);
		const FVector2D Size = Slot->GetSize();
		const FVector2D Anchor(
			Slot->GetAnchors().Minimum.X * StageSize.X,
			Slot->GetAnchors().Minimum.Y * StageSize.Y);
		const FVector2D Minimum = Anchor + Slot->GetPosition() - Slot->GetAlignment() * Size;
		return FBox2D(Minimum, Minimum + Size);
	}

	bool TargetOutcomeRectsOverlap(const FBox2D& A, const FBox2D& B)
	{
		return A.bIsValid && B.bIsValid
			&& A.Min.X < B.Max.X && B.Min.X < A.Max.X
			&& A.Min.Y < B.Max.Y && B.Min.Y < A.Max.Y;
	}

	bool RunTargetOutcomeFixtureContract(FAutomationTestBase& Test)
	{
		const TArray<FName> ScenarioIds = {
			TEXT("Outcome.Single"),
			TEXT("Outcome.HeavyArrow"),
			TEXT("Outcome.GroupThree"),
			TEXT("Outcome.GroupMissing2P"),
			TEXT("Outcome.ToxicExplosion"),
			TEXT("Outcome.MedicineEnemy"),
			TEXT("Outcome.Healing"),
			TEXT("Outcome.Armor"),
			TEXT("Outcome.AgilityDodge"),
			TEXT("Outcome.ArmorBlocked"),
			TEXT("Outcome.GuardRedirect"),
			TEXT("Outcome.Lethal")};

		UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
		if (!BuildActiveCardBattle(Subsystem, Test))
		{
			return false;
		}

		const FString OriginalFingerprint = TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy());
		FString Error;
		Test.TestFalse(
			TEXT("unknown target-outcome scenario rejects atomically"),
			Subsystem->ApplyTargetOutcomeFixtureForTest(TEXT("Outcome.Unknown"), Error));
		Test.TestFalse(TEXT("unknown scenario leaves no target-outcome fixture active"), Subsystem->IsTargetOutcomeFixtureActiveForTest());
		Test.TestFalse(TEXT("unknown scenario reports a concrete reason"), Error.IsEmpty());
		Test.TestEqual(
			TEXT("unknown scenario leaves the complete fixture fingerprint unchanged"),
			TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy()),
			OriginalFingerprint);

		Error.Reset();
		Test.TestTrue(
			TEXT("first target-outcome fixture applies"),
			Subsystem->ApplyTargetOutcomeFixtureForTest(TEXT("Outcome.Single"), Error));
		const FString FirstFixtureFingerprint = TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy());
		Test.TestFalse(
			TEXT("a second target-outcome fixture apply rejects atomically"),
			Subsystem->ApplyTargetOutcomeFixtureForTest(TEXT("Outcome.HeavyArrow"), Error));
		Test.TestTrue(TEXT("failed repeated apply preserves the active fixture"), Subsystem->IsTargetOutcomeFixtureActiveForTest());
		Test.TestEqual(
			TEXT("failed repeated apply leaves the first fixture unchanged"),
			TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy()),
			FirstFixtureFingerprint);
		Test.TestTrue(TEXT("clear restores the source runtime"), Subsystem->ClearTargetOutcomeFixtureForTest(Error));
		Test.TestFalse(TEXT("clear deactivates the target-outcome fixture"), Subsystem->IsTargetOutcomeFixtureActiveForTest());
		Test.TestEqual(
			TEXT("clear restores the original runtime fingerprint"),
			TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy()),
			OriginalFingerprint);
		Test.TestTrue(TEXT("clear without a backup is a successful no-op"), Subsystem->ClearTargetOutcomeFixtureForTest(Error));
		Test.TestEqual(
			TEXT("no-op clear preserves the original runtime fingerprint"),
			TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy()),
			OriginalFingerprint);

		for (const FName ScenarioId : ScenarioIds)
		{
			Error.Reset();
			if (!Test.TestTrue(
				*FString::Printf(TEXT("%s fixture applies: %s"), *ScenarioId.ToString(), *Error),
				Subsystem->ApplyTargetOutcomeFixtureForTest(ScenarioId, Error)))
			{
				return false;
			}

			const FGameXXKRuntimeState Fixture = Subsystem->GetRuntimeStateCopy();
			const FGameXXKBattleDeckState& Deck = Fixture.CardRun.ActiveBattle.Deck;
			Test.TestTrue(*FString::Printf(TEXT("%s is active"), *ScenarioId.ToString()), Subsystem->IsTargetOutcomeFixtureActiveForTest());
			Test.TestEqual(*FString::Printf(TEXT("%s has exactly one test card"), *ScenarioId.ToString()), Deck.Hand.Num(), 1);
			if (Deck.Hand.Num() == 1)
			{
				Test.TestNotNull(
					*FString::Printf(TEXT("%s hand card is from the real catalog"), *ScenarioId.ToString()),
					FGameXXKCardCatalog::FindCardDefinition(Deck.Hand[0].CardId));
				FGameXXKCardPlayPreview Preview;
				FString PreviewError;
				Test.TestTrue(
					*FString::Printf(TEXT("%s unique hand card has a real play preview: %s"), *ScenarioId.ToString(), *PreviewError),
					FGameXXKCardBattleAdapter::BuildCardPlayPreview(Fixture, Deck.Hand[0].InstanceId, Preview, &PreviewError));
			}

			FString ValidationError;
			Test.TestTrue(
				*FString::Printf(TEXT("%s validates as a real card runtime: %s"), *ScenarioId.ToString(), *ValidationError),
				GameXXKCardRules::ValidateCardBattleRuntime(Fixture.CardRun.ActiveBattle, &ValidationError));
			FGameXXKRuntimeState ProjectionCopy = Fixture;
			FString ProjectionError;
			Test.TestTrue(
				*FString::Printf(TEXT("%s synchronizes through the legacy projection: %s"), *ScenarioId.ToString(), *ProjectionError),
				FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(ProjectionCopy, &ProjectionError));
			Test.TestEqual(
				*FString::Printf(TEXT("%s legacy party count matches"), *ScenarioId.ToString()),
				ProjectionCopy.ActiveBattleParty.Num(),
				CountLivingUnits(ProjectionCopy, EGameXXKCardTargetSide::Party));
			Test.TestEqual(
				*FString::Printf(TEXT("%s legacy enemy count matches"), *ScenarioId.ToString()),
				ProjectionCopy.ActiveBattleEnemies.Num(),
				CountLivingUnits(ProjectionCopy, EGameXXKCardTargetSide::Enemy));

			const FName PartyOneId(TEXT("Outcome.Party.1P"));
			const FName PartyTwoId(TEXT("Player"));
			const FName PartyThreeId(TEXT("Outcome.Party.3P"));
			const FName EnemyOneId(TEXT("Outcome.Enemy.1P"));
			const FName EnemyTwoId(TEXT("Outcome.Enemy.2P"));
			const FName EnemyThreeId(TEXT("Outcome.Enemy.3P"));
			Test.TestEqual(TEXT("stable party 1P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, PartyOneId), 1);
			Test.TestEqual(TEXT("stable party 2P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, PartyTwoId), 2);
			Test.TestEqual(TEXT("stable party 3P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, PartyThreeId), 3);
			Test.TestEqual(TEXT("stable enemy 1P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, EnemyOneId), 1);
			if (ScenarioId == TEXT("Outcome.GroupMissing2P"))
			{
				Test.TestNull(TEXT("missing-2P fixture has no enemy in 2P"), FindUnitById(Fixture, EnemyTwoId));
			}
			else
			{
				Test.TestEqual(TEXT("stable enemy 2P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, EnemyTwoId), 2);
			}
			Test.TestEqual(TEXT("stable enemy 3P slot"), FGameXXKBattlePresentation::GetSlotNumber(Fixture.CardRun.ActiveBattle, EnemyThreeId), 3);

			UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
			Board->SetMVPSubsystem(Subsystem);
			Test.TestTrue(TEXT("target-outcome fixture board initializes"), Board->Initialize());
			Board->NativeConstruct();
			Board->RefreshFromState();
			if (ScenarioId == TEXT("Outcome.Healing"))
			{
				Test.TestTrue(TEXT("party target-proxy overlap fixture starts its real visual session"),
					Board->BeginBattleVisualSession(8813));
				UButton* const PartyOneProxy = Board->GetUnitTargetProxyForTest(PartyOneId);
				UButton* const PartyTwoProxy = Board->GetUnitTargetProxyForTest(PartyTwoId);
				UButton* const PartyThreeProxy = Board->GetUnitTargetProxyForTest(PartyThreeId);
				Test.TestNotNull(TEXT("party 1P has a real target proxy"), PartyOneProxy);
				Test.TestNotNull(TEXT("party 2P has a real target proxy"), PartyTwoProxy);
				Test.TestNotNull(TEXT("party 3P has a real target proxy"), PartyThreeProxy);
				const FBox2D PartyOneRect = TargetOutcomeProxyRect(PartyOneProxy);
				const FBox2D PartyTwoRect = TargetOutcomeProxyRect(PartyTwoProxy);
				const FBox2D PartyThreeRect = TargetOutcomeProxyRect(PartyThreeProxy);
				for (const UButton* const Proxy : {PartyOneProxy, PartyTwoProxy, PartyThreeProxy})
				{
					const UCanvasPanelSlot* const ProxySlot = Proxy ? Cast<UCanvasPanelSlot>(Proxy->Slot) : nullptr;
					Test.TestEqual(TEXT("party target proxy keeps the non-overlapping low-resolution hit size"),
						ProxySlot ? ProxySlot->GetSize() : FVector2D::ZeroVector,
						FVector2D(180.0f, 320.0f));
				}
				Test.TestFalse(TEXT("party 1P target center cannot be intercepted by party 2P"),
					TargetOutcomeRectsOverlap(PartyOneRect, PartyTwoRect));
				Test.TestFalse(TEXT("party 2P target center cannot be intercepted by party 3P"),
					TargetOutcomeRectsOverlap(PartyTwoRect, PartyThreeRect));
				for (const FName UnitId : {PartyOneId, PartyTwoId, PartyThreeId})
				{
					const UGameXXKBattleUnitVisualWidget* const Visual = Board->GetUnitVisualForTest(UnitId);
					const UCanvasPanelSlot* const VisualSlot = Visual ? Cast<UCanvasPanelSlot>(Visual->Slot) : nullptr;
					Test.TestEqual(*FString::Printf(TEXT("%s keeps the confirmed 410x410 visual layout"), *UnitId.ToString()),
						VisualSlot ? VisualSlot->GetSize() : FVector2D::ZeroVector,
						FVector2D(410.0f, 410.0f));
				}
			}
			Test.TestTrue(TEXT("target-outcome fixture hand is really clickable"), Board->IsHandCardSlotEnabledForTest(0));
			if (Deck.Hand.Num() == 1)
			{
				Test.TestTrue(TEXT("target-outcome fixture click enters targeting or commits"), Board->ClickCardInHand(Deck.Hand[0].InstanceId));
			}

			Test.TestTrue(TEXT("fixture clears after click"), Subsystem->ClearTargetOutcomeFixtureForTest(Error));
			Test.TestFalse(TEXT("fixture is inactive after clear"), Subsystem->IsTargetOutcomeFixtureActiveForTest());
			Test.TestEqual(
				TEXT("every scenario clear restores the original runtime"),
				TargetOutcomeFixtureFingerprint(Subsystem->GetRuntimeStateCopy()),
				OriginalFingerprint);
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
	return RunTargetOutcomeFixtureContract(*this)
		&& RunBattleHudFixtureContract<UGameXXKMVPSubsystem>(*this, 0);
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPilotComparisonFixtureTest,
	"GameXXK.MVP.Battle.PilotComparisonFixture",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPilotComparisonFixtureTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	if (!BuildActiveCardBattle(Subsystem, *this))
	{
		return false;
	}
	const FGameXXKRuntimeState RawBefore = Subsystem->GetRuntimeStateCopy();

	FString ApplyError;
	TestTrue(TEXT("pilot comparison fixture applies over the active card battle"),
		Subsystem->ApplyPilotComparisonFixtureForTest(ApplyError));
	const FGameXXKRuntimeState FixtureView = Subsystem->GetRuntimeStateCopy();
	TestTrue(TEXT("pilot comparison fixture reports as the active fixture view"),
		Subsystem->IsBattleHudFixtureActiveForTest());

	TestEqual(TEXT("pilot fixture presents three party units"),
		CountLivingUnits(FixtureView, EGameXXKCardTargetSide::Party), 3);
	TestEqual(TEXT("pilot fixture presents three enemies"),
		CountLivingUnits(FixtureView, EGameXXKCardTargetSide::Enemy), 3);
	int32 HeroRoleCount = 0;
	for (const FGameXXKCardCombatUnit& Unit : FixtureView.CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party && Unit.Role == EGameXXKCharacterRole::Hero)
		{
			++HeroRoleCount;
		}
	}
	TestEqual(TEXT("pilot fixture marks every party unit with the hero role"), HeroRoleCount, 3);
	TestTrue(TEXT("pilot fixture ids carry the resolution tokens for side-by-side comparison"),
		FixtureView.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Pilot.Hero.Two.2K");
		})
		&& FixtureView.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Pilot.Hero.Three.1K");
		})
		&& FixtureView.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Pilot.Rooster.Two.2K");
		})
		&& FixtureView.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Pilot.Rooster.Three.1K");
		}));
	for (const FName RoosterId : {FName(TEXT("Pilot.Rooster.One")), FName(TEXT("Pilot.Rooster.Two.2K")), FName(TEXT("Pilot.Rooster.Three.1K"))})
	{
		const FGameXXKCardCombatUnit* const Rooster = FindUnitById(FixtureView, RoosterId);
		TestTrue(FString::Printf(TEXT("pilot fixture rooster %s leaves its definition empty for suffix resolution"), *RoosterId.ToString()),
			Rooster && Rooster->EnemyDefinitionId.IsNone());
	}
	TestEqual(TEXT("pilot fixture intent rail carries three rooster intents"), FixtureView.CardRun.EnemyIntents.Num(), 3);

	// The scene-unit actor must resolve the hero/rooster idle flipbooks for this fixture.
	const FGameXXKBattleRuntimeUnit* const HeroLegacy = FindLegacyUnitById(FixtureView.ActiveBattleParty, TEXT("Pilot.Hero.One"));
	const FGameXXKBattleRuntimeUnit* const RoosterLegacy = FindLegacyUnitById(FixtureView.ActiveBattleEnemies, TEXT("Pilot.Rooster.One"));
	TestNotNull(TEXT("pilot fixture keeps the first hero in the legacy party projection"), HeroLegacy);
	TestNotNull(TEXT("pilot fixture keeps the first rooster in the legacy enemy projection"), RoosterLegacy);
	if (HeroLegacy && RoosterLegacy)
	{
		AGameXXKBattleSceneUnitActor* const HeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
		HeroActor->SetMVPSubsystemForTest(Subsystem);
		HeroActor->ConfigureFromRuntimeUnit(
			false,
			0,
			*HeroLegacy,
			FGameXXKBattlePresentation::GetSlotNumber(Subsystem->GetRuntimeState().CardRun.ActiveBattle, HeroLegacy->Id));
		TestNotNull(TEXT("pilot fixture hero resolves an approved battle flipbook"), HeroActor->GetCurrentBattleFlipbook());
		if (const UPaperFlipbook* const HeroFlipbook = HeroActor->GetCurrentBattleFlipbook())
		{
			TestTrue(TEXT("pilot fixture hero flipbook resolves to the hero asset"),
				HeroFlipbook->GetName().Contains(TEXT("hero")));
		}
		AGameXXKBattleSceneUnitActor* const RoosterActor = NewObject<AGameXXKBattleSceneUnitActor>();
		RoosterActor->SetMVPSubsystemForTest(Subsystem);
		RoosterActor->ConfigureFromRuntimeUnit(
			true,
			0,
			*RoosterLegacy,
			FGameXXKBattlePresentation::GetSlotNumber(Subsystem->GetRuntimeState().CardRun.ActiveBattle, RoosterLegacy->Id));
		TestNotNull(TEXT("pilot fixture rooster resolves an approved battle flipbook"), RoosterActor->GetCurrentBattleFlipbook());
		if (const UPaperFlipbook* const RoosterFlipbook = RoosterActor->GetCurrentBattleFlipbook())
		{
			TestTrue(TEXT("pilot fixture rooster flipbook resolves to the rooster asset"),
				RoosterFlipbook->GetName().Contains(TEXT("rooster")));
		}
	}

	// The overlay is non-saving and clears back to the raw battle.
	Subsystem->ClearBattleHudFixtureForTest();
	const FGameXXKRuntimeState RawAfterClear = Subsystem->GetRuntimeStateCopy();
	TestEqual(TEXT("pilot fixture clear restores raw party membership"), RawAfterClear.ActiveBattleParty.Num(), RawBefore.ActiveBattleParty.Num());
	TestEqual(TEXT("pilot fixture clear restores raw enemy membership"), RawAfterClear.ActiveBattleEnemies.Num(), RawBefore.ActiveBattleEnemies.Num());
	TestEqual(TEXT("pilot fixture clear restores raw combat-unit membership"), RawAfterClear.CardRun.ActiveBattle.Units.Num(), RawBefore.CardRun.ActiveBattle.Units.Num());
	TestEqual(TEXT("pilot fixture clear never creates permanent companions"), RawAfterClear.CardRun.CompanionRoster.PermanentCompanions.Num(), RawBefore.CardRun.CompanionRoster.PermanentCompanions.Num());
	return true;
}

#endif
