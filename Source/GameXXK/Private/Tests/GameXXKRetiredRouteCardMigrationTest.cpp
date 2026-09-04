#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FName HeroId(TEXT("Hero"));
	const FName EnemyId(TEXT("Enemy"));
	const FName BossCardId(TEXT("Route.Boss.XiongPiPiJia"));

	FGameXXKCardCombatUnit MakeUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 500;
		Unit.MaxHP = 500;
		Unit.Attack = 30;
		Unit.Defense = 10;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 30 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Speed = 10;
		Unit.StableSortOrder = StableSortOrder;
		Unit.CombatLevel = 50;
		return Unit;
	}

	FGameXXKCardInstance MakeInstance(const int32 Ordinal, const TCHAR* CardId)
	{
		FGameXXKCardInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(TEXT("RetiredRoute.Instance.%d"), Ordinal));
		Instance.CardId = FName(CardId);
		Instance.CurrentQuality = EGameXXKCardQuality::Common;
		Instance.OwnerUnitId = HeroId;
		Instance.SourceEntryId = FName(*FString::Printf(TEXT("RetiredRoute.Entry.%d"), Ordinal));
		Instance.AcquisitionOrdinal = Ordinal;
		return Instance;
	}

	bool IsRetiredCardId(const FName CardId)
	{
		const FString Id = CardId.ToString();
		return Id.StartsWith(TEXT("Route.General."))
			|| Id.StartsWith(TEXT("Route.Terrain."))
			|| Id.StartsWith(TEXT("Route.Rare."));
	}

	bool BuildLegacyBattle(FGameXXKCardBattleRuntime& OutBattle)
	{
		const TArray<FGameXXKCardInstance> Instances = {
			MakeInstance(0, TEXT("Route.General.PoJiaTuCi")),
			MakeInstance(1, TEXT("Route.Terrain.DuanYaLuoShi")),
			MakeInstance(2, TEXT("Route.Rare.GuJuanCanZhang")),
			MakeInstance(3, TEXT("Route.General.ShouShiHuiYuan")),
			MakeInstance(4, TEXT("Route.Terrain.DiMaiHuiXiang")),
			MakeInstance(5, TEXT("Route.Boss.XiongPiPiJia"))};
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			OutBattle,
			Instances,
			{MakeUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1),
			 MakeUnit(EnemyId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)},
			EGameXXKCardTerrain::Plain,
			3401))
		{
			return false;
		}

		OutBattle.Deck.Hand = {Instances[0], Instances[5]};
		OutBattle.Deck.DrawPile = {Instances[1]};
		OutBattle.Deck.DiscardPile = {Instances[2]};
		OutBattle.Deck.ExhaustPile = {Instances[3]};
		OutBattle.Deck.PendingAutomaticHandCards = {Instances[4]};
		OutBattle.Deck.ActiveInstanceIds.Reset();
		for (const FGameXXKCardInstance& Instance : Instances)
		{
			OutBattle.Deck.ActiveInstanceIds.Add(Instance.InstanceId);
		}
		OutBattle.Deck.PendingChoice = FGameXXKPendingCardChoice();
		OutBattle.Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::ForcedDiscard;
		OutBattle.Deck.PendingChoice.Candidates = OutBattle.Deck.Hand;
		OutBattle.Deck.PendingChoice.RequiredCount = 1;
		OutBattle.Deck.PendingChoice.RequiredDiscardCount = 1;

		FGameXXKCardBattleModifierRuntime& Modifier = OutBattle.Modifiers.AddDefaulted_GetRef();
		Modifier.ModifierId = TEXT("Modifier.0");
		Modifier.SourceCardInstanceId = Instances[5].InstanceId;
		Modifier.SourceUnitId = HeroId;
		Modifier.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
		Modifier.Definition.EffectType = EGameXXKCardEffectType::GainMana;
		Modifier.Definition.Target = EGameXXKCardEffectTarget::CardOwner;
		Modifier.Definition.Magnitude = 123;
		Modifier.Definition.MagnitudePolicy = EGameXXKCardMagnitudePolicy::ContinuousQuality;
		Modifier.Definition.RareMagnitude = 456;
		Modifier.Definition.EpicMagnitude = 789;
		Modifier.Definition.RemainingTriggers = 1;
		Modifier.Definition.bPersistent = true;
		Modifier.SourceCardSnapshot.CardId = BossCardId;
		Modifier.SourceCardSnapshot.Quality = EGameXXKCardQuality::Epic;
		Modifier.SourceCardSnapshot.OwnerUnitId = HeroId;
		OutBattle.NextModifierOrdinal = 1;
		return true;
	}

	FGameXXKSaveState BuildV33Source()
	{
		FGameXXKRuntimeState Runtime = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		FString Error;
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Runtime, &Error);
		Runtime.CardRun.bLoadoutLockedForRoute = true;
		Runtime.CardRun.bHasActiveCardBattle = BuildLegacyBattle(Runtime.CardRun.ActiveBattle);
		Runtime.CardRun.ActiveBattleSourceNodeId = INDEX_NONE;
		Runtime.CardRun.BossCardSlots = {BossCardId, TEXT("Route.General.PoJiaTuCi")};
		Runtime.CardRun.UpgradedCardQualities.Add(TEXT("Route.Rare.GuJuanCanZhang"), EGameXXKCardQuality::Rare);
		Runtime.CardRun.UpgradedCardQualities.Add(BossCardId, EGameXXKCardQuality::Epic);

		Runtime.CardRun.PendingReward.SourceNodeId = 77;
		Runtime.CardRun.PendingReward.ChoiceSeed = 7701;
		Runtime.CardRun.PendingReward.CardIds = {
			TEXT("Route.General.PoJiaTuCi"),
			TEXT("Route.Terrain.DuanYaLuoShi"),
			TEXT("Route.Rare.GuJuanCanZhang")};
		FGameXXKBattleRewardOption RetiredOption;
		RetiredOption.Kind = EGameXXKBattleRewardKind::BossCard;
		RetiredOption.CardId = TEXT("Route.General.ShouShiHuiYuan");
		Runtime.CardRun.PendingReward.Options = {RetiredOption};

		Runtime.CardRun.RouteMerchant.SourceNodeId = 88;
		Runtime.CardRun.RouteMerchant.OfferSeed = 8801;
		FGameXXKRouteMerchantOffer RetiredOffer;
		RetiredOffer.OfferId = TEXT("RetiredRoute.Offer");
		RetiredOffer.Kind = EGameXXKRouteMerchantOfferKind::Card;
		RetiredOffer.ContentId = TEXT("Route.Terrain.DiMaiHuiXiang");
		RetiredOffer.OwnerMemberId = HeroId;
		RetiredOffer.Quality = EGameXXKCardQuality::Common;
		RetiredOffer.NextQuality = EGameXXKCardQuality::Rare;
		RetiredOffer.Price = 40;
		Runtime.CardRun.RouteMerchant.Offers = {RetiredOffer};
		Runtime.CardRun.RouteMerchant.PendingPurchase.bActive = true;
		Runtime.CardRun.RouteMerchant.PendingPurchase.OfferId = RetiredOffer.OfferId;
		Runtime.CardRun.RouteMerchant.PendingPurchase.CardId = TEXT("Route.Rare.GuJuanCanZhang");
		Runtime.CardRun.RouteMerchant.PendingPurchase.Price = 40;

		FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(Runtime);
		Source.SaveVersion = 33;
		return Source;
	}

	bool DeckContainsRetiredCard(const FGameXXKBattleDeckState& Deck)
	{
		const TArray<const TArray<FGameXXKCardInstance>*> Zones = {
			&Deck.DrawPile,
			&Deck.Hand,
			&Deck.DiscardPile,
			&Deck.ExhaustPile,
			&Deck.PendingAutomaticHandCards,
			&Deck.PendingChoice.Candidates};
		for (const TArray<FGameXXKCardInstance>* Zone : Zones)
		{
			if (Zone->ContainsByPredicate([](const FGameXXKCardInstance& Instance)
			{
				return IsRetiredCardId(Instance.CardId);
			}))
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRetiredRouteCardMigrationTest,
	"GameXXK.SaveMigration.RetiredRouteCardsV34",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRetiredRouteCardMigrationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("active 173-card pool owns save version 34"),
		FGameXXKSaveMigration::ActiveCardPool173IntroducedSaveVersion, 34);
	TestEqual(TEXT("active 173-card pool is current schema v35"), FGameXXKSaveMigration::CurrentSaveVersion, 35);

	const FGameXXKSaveState Source = BuildV33Source();
	const FGameXXKSaveState SourceBefore = Source;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	if (!TestTrue(TEXT("v33 retired route-card save migrates"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
	{
		AddError(Report.Error);
		return false;
	}
	TestTrue(TEXT("retirement migration does not mutate its source"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&Source, &SourceBefore, PPF_None));
	TestEqual(TEXT("retirement migration reports source v33"), Report.SourceVersion, 33);
	TestEqual(TEXT("retirement migration writes v35"), Migrated.SaveVersion, 35);
	TestTrue(TEXT("battle remains active while its Boss compatibility card survives"), Migrated.RuntimeState.CardRun.bHasActiveCardBattle);
	const FGameXXKBattleDeckState& Deck = Migrated.RuntimeState.CardRun.ActiveBattle.Deck;
	TestFalse(TEXT("every retired card is removed from owning and choice zones"), DeckContainsRetiredCard(Deck));
	TestEqual(TEXT("only the Boss compatibility instance remains in the ledger"), Deck.ActiveInstanceIds.Num(), 1);
	TestEqual(TEXT("the surviving Boss compatibility instance remains in hand"), Deck.Hand.Num(), 1);
	if (Deck.Hand.Num() == 1)
	{
		TestEqual(TEXT("熊罴皮甲 survives migration"), Deck.Hand[0].CardId, BossCardId);
	}
	TestEqual(TEXT("retired pending choice is cleared"), Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestEqual(TEXT("one surviving v33 modifier remains"), Migrated.RuntimeState.CardRun.ActiveBattle.Modifiers.Num(), 1);
	if (Migrated.RuntimeState.CardRun.ActiveBattle.Modifiers.Num() == 1)
	{
		const FGameXXKCardBattleModifier& Modifier = Migrated.RuntimeState.CardRun.ActiveBattle.Modifiers[0].Definition;
		TestEqual(TEXT("surviving v33 modifier keeps its resolved magnitude"), Modifier.Magnitude, 123);
		TestEqual(TEXT("surviving v33 modifier becomes explicitly unscaled"), Modifier.MagnitudePolicy, EGameXXKCardMagnitudePolicy::Unscaled);
		TestEqual(TEXT("surviving v33 modifier clears an absent Rare override"), Modifier.RareMagnitude, INDEX_NONE);
		TestEqual(TEXT("surviving v33 modifier clears an absent Epic override"), Modifier.EpicMagnitude, INDEX_NONE);
	}
	TestEqual(TEXT("only Boss compatibility slots survive"), Migrated.RuntimeState.CardRun.BossCardSlots, TArray<FName>{BossCardId});
	TestFalse(TEXT("retired quality overrides are removed"), Migrated.RuntimeState.CardRun.UpgradedCardQualities.Contains(TEXT("Route.Rare.GuJuanCanZhang")));
	TestEqual(TEXT("surviving Boss quality override is preserved"),
		Migrated.RuntimeState.CardRun.UpgradedCardQualities.FindRef(BossCardId), EGameXXKCardQuality::Epic);
	TestTrue(TEXT("retired pending rewards are cleared"), Migrated.RuntimeState.CardRun.PendingReward.Options.IsEmpty()
		&& Migrated.RuntimeState.CardRun.PendingReward.CardIds.IsEmpty()
		&& Migrated.RuntimeState.CardRun.PendingReward.SourceNodeId == INDEX_NONE);
	TestTrue(TEXT("retired merchant state is cleared"), Migrated.RuntimeState.CardRun.RouteMerchant.Offers.IsEmpty()
		&& !Migrated.RuntimeState.CardRun.RouteMerchant.PendingPurchase.bActive
		&& Migrated.RuntimeState.CardRun.RouteMerchant.SourceNodeId == INDEX_NONE);
	FString Error;
	TestTrue(TEXT("migrated surviving battle validates"),
		FGameXXKSaveMigration::ValidateRuntimeState(Migrated.RuntimeState, Error));

	FGameXXKSaveState AllRetiredSource = BuildV33Source();
	for (FGameXXKCardInstance& Instance : AllRetiredSource.RuntimeState.CardRun.ActiveBattle.Deck.Hand)
	{
		if (Instance.CardId == BossCardId)
		{
			Instance.CardId = TEXT("Route.General.HeJiLing");
		}
	}
	const int32 GoldBefore = AllRetiredSource.RuntimeState.PlayerGold;
	AllRetiredSource.RuntimeState.Screen = EGameXXKScreen::Battle;
	FGameXXKSaveState Recovered;
	FGameXXKSaveMigrationReport RecoveryReport;
	if (TestTrue(TEXT("an all-retired active deck recovers without migration failure"),
		FGameXXKSaveMigration::MigrateToCurrent(AllRetiredSource, Recovered, RecoveryReport)))
	{
		TestFalse(TEXT("an all-retired deck clears only the active card battle"), Recovered.RuntimeState.CardRun.bHasActiveCardBattle);
		TestEqual(TEXT("standalone battle recovery returns to the 2D workbench surface"), Recovered.RuntimeState.Screen, EGameXXKScreen::Town);
		TestEqual(TEXT("retirement recovery grants no currency"), Recovered.RuntimeState.PlayerGold, GoldBefore);
		TestTrue(TEXT("retirement recovery creates no victory reward"), Recovered.RuntimeState.CardRun.PendingReward.Options.IsEmpty()
			&& Recovered.RuntimeState.CardRun.PendingReward.CardIds.IsEmpty()
			&& !Recovered.RuntimeState.CardRun.bActiveBattleRewardResolved);
	}
	return true;
}

#endif
