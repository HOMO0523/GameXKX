#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	// Enters a generated-route battle victory at SourceNodeId of the requested kind.
	// When bAddFollowupBattle is set, a second battle node is attached so the route
	// can keep fighting after the first reward resolves.
	bool StartTieredBattleVictory(
		FGameXXKRuntimeState& OutState,
		const EGameXXKNodeKind NodeKind,
		const int32 SourceNodeId,
		const int32 RouteRandomSeed,
		const int32 NextRewardOrdinal = 0,
		const bool bAddFollowupBattle = false)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(OutState)
			|| !UGameXXKMVPRules::EnterWorldRegion(OutState, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(OutState)
			|| !UGameXXKMVPRules::EnterDungeon(OutState))
		{
			return false;
		}

		OutState.Screen = EGameXXKScreen::DungeonMap;
		OutState.CurrentMapId = TEXT("HuangshanRoute");
		OutState.CurrentRouteNodeId = SourceNodeId;
		OutState.PendingRouteNodeId = INDEX_NONE;
		OutState.RouteMapNodes.Reset();
		OutState.RouteMapEdges.Reset();
		OutState.VisitedRouteNodeIds.Reset();
		OutState.ReachableRouteNodeIds.Reset();
		OutState.ReachableRouteNodeIds.Add(SourceNodeId);
		OutState.RouteMapNodes.Add(FGameXXKRouteMapNode(
			SourceNodeId,
			1,
			0,
			NodeKind,
			FVector2D(0.5f, 0.5f),
			bAddFollowupBattle ? TArray<int32>{SourceNodeId + 1} : TArray<int32>{}));
		if (bAddFollowupBattle)
		{
			OutState.RouteMapNodes.Add(FGameXXKRouteMapNode(
				SourceNodeId + 1,
				2,
				0,
				EGameXXKNodeKind::Battle,
				FVector2D(0.5f, 0.9f),
				TArray<int32>{}));
			OutState.RouteMapEdges.Add(FGameXXKRouteMapEdge(SourceNodeId, SourceNodeId + 1));
		}
		if (!UGameXXKMVPRules::SelectRouteNodeById(OutState, SourceNodeId))
		{
			return false;
		}
		OutState.CardRun.RouteRandomSeed = RouteRandomSeed;
		OutState.CardRun.NextRewardOrdinal = NextRewardOrdinal;
		for (FGameXXKCardCombatUnit& Unit : OutState.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		OutState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		return true;
	}

	void MarkEveryConfiguredCardEpicExcept(FGameXXKRuntimeState& State, const FName ExemptCardId)
	{
		FGameXXKCardRunState& Run = State.CardRun;
		const auto MarkEpic = [&Run](const TArray<FName>& CardIds, const FName Exempt)
		{
			for (const FName CardId : CardIds)
			{
				if (CardId != Exempt)
				{
					Run.UpgradedCardQualities.Add(CardId, EGameXXKCardQuality::Epic);
				}
			}
		};
		MarkEpic(Run.HeroSelectedCardIds, ExemptCardId);
		for (const FGameXXKPermanentCompanion& Companion : Run.CompanionRoster.PermanentCompanions)
		{
			if (Companion.bIsActive)
			{
				MarkEpic(Companion.SelectedCardIds, ExemptCardId);
			}
		}
	}

	bool RuntimeStatesEqual(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRewardTieringTierShapeTest,
	"GameXXK.Integration.CardRoute.BattleRewardTiering.TierShape",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRewardTieringTierShapeTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x7E57A11D;

	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the normal battle tier enters a generated battle victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Battle, 2, RouteRandomSeed)))
		{
			return false;
		}
		TestTrue(TEXT("the normal battle victory creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		const TArray<FGameXXKBattleRewardOption>& Options = State.CardRun.PendingReward.Options;
		TestEqual(TEXT("the normal battle tier offers exactly three options"), Options.Num(), 3);
		TestEqual(TEXT("the normal battle first option is a relic"), Options[0].Kind, EGameXXKBattleRewardKind::Relic);
		TestEqual(TEXT("the normal battle second option is a relic"), Options[1].Kind, EGameXXKBattleRewardKind::Relic);
		TestEqual(TEXT("the normal battle third option upgrades a deck card"), Options[2].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
		TestFalse(TEXT("the normal battle relics never duplicate"), Options[0].RelicId == Options[1].RelicId);
		TestFalse(TEXT("the upgrade option always names a configured card"), Options[2].CardId.IsNone());
		TestTrue(TEXT("the tiered offer keeps the legacy CardIds payload empty"), State.CardRun.PendingReward.CardIds.IsEmpty());
		TestNotEqual(TEXT("the tiered offer persists a non-zero choice seed"), State.CardRun.PendingReward.ChoiceSeed, 0);
	}

	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the elite battle tier enters a generated elite victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Elite, 3, RouteRandomSeed)))
		{
			return false;
		}
		TestTrue(TEXT("the elite battle victory creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		const TArray<FGameXXKBattleRewardOption>& Options = State.CardRun.PendingReward.Options;
		TestEqual(TEXT("the elite battle tier offers exactly three options"), Options.Num(), 3);
		TestTrue(TEXT("the elite first option is an energy-cap or draw attribute bonus"),
			Options[0].Kind == EGameXXKBattleRewardKind::EnergyCapBonus
				|| Options[0].Kind == EGameXXKBattleRewardKind::DrawBonus);
		TestTrue(TEXT("the elite attribute option carries no card payload"), Options[0].CardId.IsNone());
		TestTrue(TEXT("the elite attribute option carries no relic payload"), Options[0].RelicId.IsNone());
		TestEqual(TEXT("the elite second option upgrades a deck card"), Options[1].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
		TestEqual(TEXT("the elite third option is a relic"), Options[2].Kind, EGameXXKBattleRewardKind::Relic);
	}

	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the boss battle tier enters a generated boss victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Boss, 4, RouteRandomSeed)))
		{
			return false;
		}
		TestTrue(TEXT("the boss battle victory creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, true));
		const TArray<FGameXXKBattleRewardOption>& Options = State.CardRun.PendingReward.Options;
		TestEqual(TEXT("the boss battle tier offers exactly three options"), Options.Num(), 3);
		TestEqual(TEXT("the boss first option is a boss card"), Options[0].Kind, EGameXXKBattleRewardKind::BossCard);
		TestEqual(TEXT("the boss second option upgrades a deck card"), Options[1].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
		TestEqual(TEXT("the boss third option is a relic"), Options[2].Kind, EGameXXKBattleRewardKind::Relic);
		TestTrue(TEXT("the boss-card option names a route boss pool card"),
			Options[0].CardId.ToString().StartsWith(TEXT("Route.Boss.")));
		TestTrue(TEXT("the boss offer never duplicates its visible card options"), Options[0].CardId != Options[1].CardId);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRewardTieringDeterminismTest,
	"GameXXK.Integration.CardRoute.BattleRewardTiering.ChoiceSeedDeterminism",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRewardTieringDeterminismTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x13579BDF;
	FGameXXKRuntimeState FirstState;
	FGameXXKRuntimeState RepeatedState;
	if (!TestTrue(TEXT("the determinism fixture enters its first generated battle victory"),
		StartTieredBattleVictory(FirstState, EGameXXKNodeKind::Battle, 2, RouteRandomSeed, 7))
		|| !TestTrue(TEXT("the determinism fixture enters its second identical battle victory"),
			StartTieredBattleVictory(RepeatedState, EGameXXKNodeKind::Battle, 2, RouteRandomSeed, 7)))
	{
		return false;
	}
	TestTrue(TEXT("the first facade call creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(FirstState, false));
	TestTrue(TEXT("the identical facade call creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(RepeatedState, false));
	TestEqual(TEXT("identical inputs persist the same choice seed"),
		RepeatedState.CardRun.PendingReward.ChoiceSeed,
		FirstState.CardRun.PendingReward.ChoiceSeed);
	TestEqual(TEXT("identical inputs persist the same option count"),
		RepeatedState.CardRun.PendingReward.Options.Num(),
		FirstState.CardRun.PendingReward.Options.Num());
	const TArray<FGameXXKBattleRewardOption>& FirstOptions = FirstState.CardRun.PendingReward.Options;
	const TArray<FGameXXKBattleRewardOption>& RepeatedOptions = RepeatedState.CardRun.PendingReward.Options;
	for (int32 OptionIndex = 0; OptionIndex < FirstOptions.Num(); ++OptionIndex)
	{
		TestEqual(FString::Printf(TEXT("option %d keeps the same kind"), OptionIndex),
			RepeatedOptions[OptionIndex].Kind,
			FirstOptions[OptionIndex].Kind);
		TestEqual(FString::Printf(TEXT("option %d keeps the same card payload"), OptionIndex),
			RepeatedOptions[OptionIndex].CardId,
			FirstOptions[OptionIndex].CardId);
		TestEqual(FString::Printf(TEXT("option %d keeps the same relic payload"), OptionIndex),
			RepeatedOptions[OptionIndex].RelicId,
			FirstOptions[OptionIndex].RelicId);
	}
	TestTrue(TEXT("identical inputs persist byte-identical pending offers"),
		FGameXXKPendingRouteCardReward::StaticStruct()->CompareScriptStruct(
			&RepeatedState.CardRun.PendingReward,
			&FirstState.CardRun.PendingReward,
			PPF_None));

	FGameXXKRuntimeState DifferentSeedState;
	if (TestTrue(TEXT("the comparison fixture enters a different-seed battle victory"),
		StartTieredBattleVictory(DifferentSeedState, EGameXXKNodeKind::Battle, 2, RouteRandomSeed, 8))
		&& TestTrue(TEXT("the different-seed facade call creates its tiered offer"),
			UGameXXKMVPRules::ResolveBattleVictory(DifferentSeedState, false)))
	{
		TestNotEqual(TEXT("different reward ordinals produce distinct choice seeds"),
			DifferentSeedState.CardRun.PendingReward.ChoiceSeed,
			FirstState.CardRun.PendingReward.ChoiceSeed);
		TestFalse(TEXT("different choice seeds produce different tiered offers"),
			FGameXXKPendingRouteCardReward::StaticStruct()->CompareScriptStruct(
				&DifferentSeedState.CardRun.PendingReward,
				&FirstState.CardRun.PendingReward,
				PPF_None));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRewardTieringUpgradeTest,
	"GameXXK.Integration.CardRoute.BattleRewardTiering.UpgradeAndEpicGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRewardTieringUpgradeTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x6D2B79F5;

	{
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the upgrade fixture enters a generated battle victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Battle, 2, RouteRandomSeed)))
		{
			return false;
		}
		TestTrue(TEXT("the upgrade fixture creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		constexpr int32 UpgradeOptionIndex = 2;
		const TArray<FGameXXKBattleRewardOption>& Options = State.CardRun.PendingReward.Options;
		TestEqual(TEXT("the upgrade fixture offers a deck-card upgrade last"), Options[UpgradeOptionIndex].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
		const FName UpgradedCardId = Options[UpgradeOptionIndex].CardId;
		const EGameXXKCardQuality QualityBefore = FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, UpgradedCardId);
		FString Error;
		TestTrue(TEXT("choosing the upgrade option commits and finishes the victory"),
			UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(State, UpgradeOptionIndex, NAME_None, &Error));
		TestEqual(TEXT("the chosen card persists its one-step quality upgrade"),
			State.CardRun.UpgradedCardQualities.FindRef(UpgradedCardId),
			FGameXXKCardBattleAdapter::GetNextCardQuality(QualityBefore));
		TestEqual(TEXT("the configured-card quality reads the persisted upgrade"),
			FGameXXKCardBattleAdapter::GetConfiguredCardQuality(State.CardRun, UpgradedCardId),
			FGameXXKCardBattleAdapter::GetNextCardQuality(QualityBefore));

		// The upgrade survives a current-version save round trip.
		const FGameXXKSaveState Saved = UGameXXKMVPRules::MakeSaveState(State);
		FGameXXKRuntimeState Restored;
		FGameXXKSaveMigrationReport Report;
		TestTrue(TEXT("the upgraded state restores through the save migration chain"),
			FGameXXKSaveMigration::TryRestoreRuntimeState(Saved, Restored, Report));
		TestEqual(TEXT("the restored save keeps the upgraded quality"),
			Restored.CardRun.UpgradedCardQualities.FindRef(UpgradedCardId),
			FGameXXKCardBattleAdapter::GetNextCardQuality(QualityBefore));
	}

	{
		// Epic-locked cards never appear as upgrade options; the offer picks the one
		// remaining non-Epic configured card.
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the epic-gate fixture enters a generated battle victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Battle, 2, RouteRandomSeed)))
		{
			return false;
		}
		const FName ReservedCardId = State.CardRun.HeroSelectedCardIds[0];
		MarkEveryConfiguredCardEpicExcept(State, ReservedCardId);
		TestTrue(TEXT("the epic-gate fixture creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		constexpr int32 UpgradeOptionIndex = 2;
		TestEqual(TEXT("the epic-gate offer still grants a deck-card upgrade"),
			State.CardRun.PendingReward.Options[UpgradeOptionIndex].Kind,
			EGameXXKBattleRewardKind::DeckCardUpgrade);
		TestEqual(TEXT("the only non-Epic configured card is the upgrade option"),
			State.CardRun.PendingReward.Options[UpgradeOptionIndex].CardId,
			ReservedCardId);
	}

	{
		// When every configured card is Epic the upgrade slot falls back to a relic.
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the fallback fixture enters a generated battle victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Battle, 2, RouteRandomSeed)))
		{
			return false;
		}
		MarkEveryConfiguredCardEpicExcept(State, NAME_None);
		TestTrue(TEXT("the fallback fixture creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		const TArray<FGameXXKBattleRewardOption>& Options = State.CardRun.PendingReward.Options;
		TestEqual(TEXT("the exhausted upgrade slot falls back to a relic"), Options[2].Kind, EGameXXKBattleRewardKind::Relic);
		TestEqual(TEXT("the all-Epic offer still has exactly three options"), Options.Num(), 3);
	}

	{
		// Choosing an upgrade for an already-Epic card is rejected atomically.
		FGameXXKRuntimeState State;
		if (!TestTrue(TEXT("the max-quality fixture enters a generated battle victory"),
			StartTieredBattleVictory(State, EGameXXKNodeKind::Battle, 2, RouteRandomSeed)))
		{
			return false;
		}
		TestTrue(TEXT("the max-quality fixture creates its tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
		const FName EpicLockedCardId = State.CardRun.HeroSelectedCardIds[0];
		State.CardRun.PendingReward.Options[2].Kind = EGameXXKBattleRewardKind::DeckCardUpgrade;
		State.CardRun.PendingReward.Options[2].CardId = EpicLockedCardId;
		State.CardRun.PendingReward.Options[2].RelicId = NAME_None;
		State.CardRun.UpgradedCardQualities.Add(EpicLockedCardId, EGameXXKCardQuality::Epic);
		const FGameXXKRuntimeState MaxQualityBefore = State;
		FString Error;
		TestFalse(TEXT("an Epic-locked upgrade option is rejected"),
			UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(State, 2, NAME_None, &Error));
		TestTrue(TEXT("the Epic-locked rejection reports a concrete error"), !Error.IsEmpty());
		TestTrue(TEXT("the Epic-locked rejection rolls back the complete runtime"),
			RuntimeStatesEqual(State, MaxQualityBefore));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleRewardTieringAttributeBonusTest,
	"GameXXK.Integration.CardRoute.BattleRewardTiering.AttributeBonusNextBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRewardTieringAttributeBonusTest::RunTest(const FString& Parameters)
{
	constexpr int32 RouteRandomSeed = 0x4A39B70D;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the attribute fixture enters a generated elite victory with a followup battle"),
		StartTieredBattleVictory(State, EGameXXKNodeKind::Elite, 3, RouteRandomSeed, 0, true)))
	{
		return false;
	}
	TestTrue(TEXT("the attribute fixture creates its elite tiered offer"), UGameXXKMVPRules::ResolveBattleVictory(State, false));
	const EGameXXKBattleRewardKind AttributeKind = State.CardRun.PendingReward.Options[0].Kind;
	if (!TestTrue(TEXT("the elite attribute option is an energy-cap or draw bonus"),
		AttributeKind == EGameXXKBattleRewardKind::EnergyCapBonus
			|| AttributeKind == EGameXXKBattleRewardKind::DrawBonus))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("choosing the elite attribute option commits and finishes the victory"),
		UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(State, 0, NAME_None, &Error));
	if (AttributeKind == EGameXXKBattleRewardKind::EnergyCapBonus)
	{
		TestEqual(TEXT("the energy-cap option grants exactly one shared energy cap"), State.CardRun.BonusSharedEnergyCap, 1);
		TestEqual(TEXT("the energy-cap option never touches the draw bonus"), State.CardRun.BonusRoundDrawCount, 0);
	}
	else
	{
		TestEqual(TEXT("the draw-bonus option grants exactly one extra draw"), State.CardRun.BonusRoundDrawCount, 1);
		TestEqual(TEXT("the draw-bonus option never touches the shared energy cap"), State.CardRun.BonusSharedEnergyCap, 0);
	}

	TestEqual(TEXT("the elite settle returns to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the followup battle node becomes selectable"), State.ReachableRouteNodeIds.Contains(4));
	TestTrue(TEXT("the followup battle begins"), UGameXXKMVPRules::SelectRouteNodeById(State, 4));
	TestTrue(TEXT("the followup battle carries the permanent bonuses into its runtime"),
		State.CardRun.ActiveBattle.BonusSharedEnergyCap == State.CardRun.BonusSharedEnergyCap
			&& State.CardRun.ActiveBattle.BonusRoundDrawCount == State.CardRun.BonusRoundDrawCount);
	TestEqual(TEXT("the opening hand draws the hand limit plus the permanent draw bonus"),
		State.CardRun.ActiveBattle.Deck.Hand.Num(),
		State.CardRun.ActiveBattle.Deck.HandLimit + State.CardRun.BonusRoundDrawCount);

	TArray<FGameXXKCardDamageResult> DamageResults;
	TestTrue(TEXT("the followup battle ends its opening player phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, DamageResults, &Error));
	FGameXXKCardEnemyIntent ResolvedIntent;
	bool bIntentsFinished = false;
	for (int32 IntentGuard = 0; IntentGuard < 8 && !bIntentsFinished; ++IntentGuard)
	{
		if (!FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, DamageResults, bIntentsFinished, &Error))
		{
			break;
		}
	}
	TestTrue(TEXT("the followup enemy phase resolves every intent"), bIntentsFinished);
	TestTrue(TEXT("the followup battle completes its enemy phase"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, DamageResults, &Error));
	TestEqual(TEXT("the new round energy is three plus the permanent shared energy cap"),
		State.CardRun.ActiveBattle.Deck.SharedEnergy,
		3 + State.CardRun.BonusSharedEnergyCap);
	TestEqual(TEXT("the new round hand refills to the hand limit plus the permanent draw bonus"),
		State.CardRun.ActiveBattle.Deck.Hand.Num(),
		State.CardRun.ActiveBattle.Deck.HandLimit + State.CardRun.BonusRoundDrawCount);
	return true;
}

#endif
