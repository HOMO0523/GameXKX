#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMetaShopTypes.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRunDeckRules.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKSaveGame.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const FGameXXKEquipmentInstance* FindByBaseAndOwner(
		const FGameXXKEquipmentCollectionState& Collection,
		const FName BaseId,
		const EGameXXKEquipmentOwnerKind OwnerKind,
		const FName OwnerId)
	{
		return Collection.EquipmentInstances.FindByPredicate(
			[BaseId, OwnerKind, OwnerId](const FGameXXKEquipmentInstance& Instance)
			{
				return Instance.BaseEquipmentId == BaseId
					&& Instance.OwnerKind == OwnerKind
					&& Instance.OwnerCharacterId == OwnerId;
			});
	}

	void AddSimpleCompanions(FGameXXKRuntimeState& State)
	{
		const TArray<FGameXXKCompanionTemplateDefinition>& Templates = FGameXXKCompanionCatalog::GetRecruitTemplates();
		for (int32 Index = 0; Index < 12; ++Index)
		{
			if (!Templates.IsValidIndex(Index))
			{
				return;
			}
			FGameXXKCompanionRecruitResult RecruitResult;
			FString Error;
			if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				State.CardRun.CompanionRoster,
				Templates[Index].TemplateId,
				1000 + Index,
				RecruitResult,
				&Error)
				|| RecruitResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
			{
				return;
			}
			FGameXXKPermanentCompanion& Companion = State.CardRun.CompanionRoster.PermanentCompanions.Last();
			Companion.EquippedItemIds = Index == 0
				? TArray<FName>{TEXT("Item.IronSword"), TEXT("Item.WoodenSword")}
				: TArray<FName>{TEXT("Item.ClothTalisman")};
		}
		if (Templates.IsValidIndex(12))
		{
			FGameXXKCompanionRosterState CandidateRoster;
			FGameXXKCompanionRecruitResult CandidateResult;
			FString Error;
			if (FGameXXKCompanionRules::RecruitPermanentCompanion(
				CandidateRoster,
				Templates[12].TemplateId,
				9012,
				CandidateResult,
				&Error)
				&& CandidateResult.Outcome == EGameXXKCompanionRecruitOutcome::Recruited)
			{
				State.CardRun.CompanionRoster.PendingRecruitment.bHasPendingRecruitment = true;
				State.CardRun.CompanionRoster.PendingRecruitment.Candidate = CandidateResult.Companion;
			}
		}
	}

	FGameXXKBattleRuntimeUnit MakeSavedBattleUnit(
		FName Id,
		int32 Health,
		int32 Mana,
		int32 Attack,
		int32 Defense,
		bool bEnemy);

	FGameXXKSaveState MakeDetailedVersionSixFixture()
	{
		FGameXXKSaveState Source;
		Source.SaveVersion = 6;
		FGameXXKRuntimeState& State = Source.RuntimeState;
		State.PlayerLevel = 3;
		State.PlayerXP = 47;
		State.PlayerGold = 321;
		State.PlayerMaxHP = 130;
		State.PlayerHP = 145;
		State.PlayerMaxMP = 40;
		State.PlayerMP = 47;
		State.CardRun.RouteAttributeBonuses.MaxHealth = 25;
		State.CardRun.RouteAttributeBonuses.MaxMana = 12;
		State.Inventory.Add(TEXT("Item.WoodenSword"), 3);
		State.Inventory.Add(TEXT("Item.HealingPowder"), 7);
		State.Inventory.Add(TEXT("Item.ClothTalisman"), 0);
		State.Inventory.Add(TEXT("Item.EnhancementStone"), 9);
		State.Inventory.Add(TEXT("Item.StarterClothArmor"), 2);
		State.Inventory.Add(TEXT("Item.QingshanRouteSeal"), 1);
		State.EnhancementMaterial = 9;
		State.ItemEnhancementLevels.Add(TEXT("Item.WoodenSword"), 2);
		State.ItemEnhancementLevels.Add(TEXT("Item.StarterClothArmor"), 4);
		State.ItemEnhancementLevels.Add(TEXT("Item.ClothTalisman"), 6);
		State.EquippedWeapon = TEXT("Item.WoodenSword");
		State.EquippedArmor = TEXT("Item.StarterClothArmor");
		State.EquippedAccessory = TEXT("Item.ClothTalisman");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bDungeonActive = true;
		State.RouteSeed = 0x4242;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{10, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.2f, 0.5f), TArray<int32>{11}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{11, 2, 1, EGameXXKNodeKind::Battle, FVector2D(0.4f, 0.6f), TArray<int32>{12}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{12, 3, 1, EGameXXKNodeKind::Event, FVector2D(0.6f, 0.6f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{10, 11});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{11, 12});
		State.CurrentRouteNodeId = 11;
		State.PendingRouteNodeId = 11;
		State.VisitedRouteNodeIds = {10, 11};
		State.ReachableRouteNodeIds = {12};
		State.DiscoveredCodexEntryIds.Add(TEXT("Codex.MoneyRat"));
		State.ReadCodexEntryIds.Add(TEXT("Codex.MoneyRat"));
		State.CardRun.RouteRandomSeed = 0x1234567;
		State.CardRun.RouteCardIds.Add(TEXT("Route.General.PoJiaTuCi"));
		AddSimpleCompanions(State);
		if (!State.CardRun.CompanionRoster.PermanentCompanions.IsEmpty())
		{
			FGameXXKCompanionRules::SetActivePermanentCompanion(
				State.CardRun.CompanionRoster,
				State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId,
				nullptr);
		}
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, nullptr);
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, nullptr);
		FGameXXKRelicInstance Relic;
		Relic.RelicId = TEXT("Relic.AncientCoin");
		Relic.Stacks = 1;
		Relic.AcquisitionOrdinal = 1;
		State.CardRun.Relics.Add(Relic);
		State.CardRun.NextRelicAcquisitionOrdinal = 1;
		State.ActiveBattleParty = {
			MakeSavedBattleUnit(TEXT("Player"), 145, 47, 21, 11, false)};
		State.ActiveBattleEnemies = {
			MakeSavedBattleUnit(TEXT("MoneyRat"), 60, 0, 9, 2, true)};
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 11;
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			0x5151,
			nullptr);
		State.CardRun.CompanionRoster.RecruitSequenceSeed = 8731;
		State.CardRun.CompanionRoster.RecruitSequenceOrdinal = 19;
		State.EquipmentCollection.CollectionSeed = 0x77A11;
		return Source;
	}

	bool StatsEqual(const FGameXXKCharacterStats& A, const FGameXXKCharacterStats& B)
	{
		return A.MaxHealth == B.MaxHealth
			&& A.MaxMana == B.MaxMana
			&& A.Attack == B.Attack
			&& A.Defense == B.Defense
			&& A.Speed == B.Speed;
	}

	FGameXXKBattleRuntimeUnit MakeSavedBattleUnit(
		const FName Id,
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = Id;
		Unit.DisplayName = FText::FromName(Id);
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.MP = Mana;
		Unit.MaxMP = Mana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.Shield = 0;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	FGameXXKSaveState MakeCurrentPendingEventFixture(const bool bChest)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.bDungeonActive = true;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60);
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{
			10,
			1,
			0,
			bChest ? EGameXXKNodeKind::Chest : EGameXXKNodeKind::Event,
			FVector2D(0.5f, 0.5f),
			TArray<int32>{}});
		State.CurrentRouteNodeId = 10;
		State.PendingRouteNodeId = 10;
		State.CardRun.PendingEvent.SourceNodeId = 10;
		State.CardRun.PendingEvent.ChoiceSeed = bChest ? 0x7102 : 0x7101;
		State.CardRun.PendingEvent.EncounterId = bChest
			? FName(TEXT("Encounter.Chest.Bamboo"))
			: FName(TEXT("Encounter.Event.YueBai"));
		State.CardRun.PendingEvent.EventNpcId = bChest ? NAME_None : FName(TEXT("Npc.YueBai"));
		if (bChest)
		{
			State.CardRun.PendingRelicOffer.SourceNodeId = 10;
			State.CardRun.PendingRelicOffer.ChoiceSeed = 0x7102;
			State.CardRun.PendingRelicOffer.RelicIds = {
				TEXT("Relic.AncientCoin"), TEXT("Relic.JadeBell"), TEXT("Relic.BambooTally")};
		}
		return UGameXXKMVPRules::MakeSaveState(State);
	}

	FGameXXKSaveState MakeCurrentPendingRewardFixture()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.bDungeonActive = true;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60);
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{
			10, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.5f, 0.5f), TArray<int32>{}});
		State.CurrentRouteNodeId = 10;
		State.PendingRouteNodeId = 10;
		State.CardRun.RouteRandomSeed = 0x7103;
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, nullptr);
		State.ActiveBattleParty = {
			MakeSavedBattleUnit(
				TEXT("Player"),
				State.PlayerHP,
				State.PlayerMP,
				State.PlayerAttack,
				State.PlayerDefense,
				false)};
		State.ActiveBattleEnemies = {
			MakeSavedBattleUnit(TEXT("MoneyRat"), 60, 0, 9, 2, true)};
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 10;
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			0x7103,
			nullptr);
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Enemy)
			{
				Unit.HP = 0;
				Unit.bLiving = false;
			}
		}
		State.CardRun.EnemyIntents.Reset();
		State.CardRun.NextEnemyIntentIndex = 0;
		FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, nullptr);
		State.CardRun.PendingReward.SourceNodeId = 10;
		State.CardRun.PendingReward.ChoiceSeed = 0x7104;
		FGameXXKBattleRewardOption FixtureUpgradeOption;
		FixtureUpgradeOption.Kind = EGameXXKBattleRewardKind::DeckCardUpgrade;
		FixtureUpgradeOption.CardId = TEXT("Hero.Generic.QingFengYiShi");
		State.CardRun.PendingReward.Options.Add(FixtureUpgradeOption);
		FGameXXKBattleRewardOption FixtureBossOption;
		FixtureBossOption.Kind = EGameXXKBattleRewardKind::BossCard;
		FixtureBossOption.CardId = TEXT("Route.Boss.HuPoZhenDan");
		State.CardRun.PendingReward.Options.Add(FixtureBossOption);
		FGameXXKBattleRewardOption FixtureRelicOption;
		FixtureRelicOption.Kind = EGameXXKBattleRewardKind::Relic;
		FixtureRelicOption.RelicId = TEXT("Relic.PaperCrane");
		State.CardRun.PendingReward.Options.Add(FixtureRelicOption);
		return UGameXXKMVPRules::MakeSaveState(State);
	}

	FGameXXKSaveState MakeCurrentMerchantFixture()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::RouteMerchant;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 0x6137;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{9, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.25f, 0.5f), TArray<int32>{10}},
			FGameXXKRouteMapNode{10, 2, 1, EGameXXKNodeKind::Merchant, FVector2D(0.55f, 0.5f), TArray<int32>{}}};
		State.RouteMapEdges = {FGameXXKRouteMapEdge{9, 10}};
		State.VisitedRouteNodeIds = {9};
		State.ReachableRouteNodeIds = {10};
		State.CurrentRouteNodeId = 9;
		State.PendingRouteNodeId = 10;
		State.CardRun.RouteProgress.SchemaVersion = 1;
		State.CardRun.RouteProgress.RootSeed = State.RouteSeed;
		State.CardRun.RouteProgress.ChapterSeeds = {
			State.RouteSeed,
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 2)),
			FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(State.RouteSeed, 3))};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.RouteProgress.RouteCombatLevel = State.PlayerLevel;
		State.CardRun.bLoadoutLockedForRoute = true;
		FString Error;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60, &Error);
		FGameXXKRouteMerchantRules::EnsureStock(State, &Error);
		return UGameXXKMVPRules::MakeSaveState(State);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSaveMigrationVersionContractTest,
	"GameXXK.Equipment.SaveMigration.VersionContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSaveMigrationVersionContractTest::RunTest(const FString& Parameters)
{
	const UGameXXKSaveGame* NeutralObject = NewObject<UGameXXKSaveGame>();
	TestEqual(TEXT("save object constructor is serialization-neutral"), NeutralObject->SaveState.SaveVersion, 0);
	TestEqual(TEXT("neutral save object inherits no starter inventory"), NeutralObject->SaveState.RuntimeState.Inventory.Num(), 0);
	TestEqual(TEXT("neutral save object inherits no starter equipment"), NeutralObject->SaveState.RuntimeState.EquipmentCollection.EquipmentInstances.Num(), 0);

	FGameXXKSaveState Source;
	Source.SaveVersion = FGameXXKSaveMigration::CurrentSaveVersion + 1;
	FGameXXKSaveState Migrated;
	Migrated.SaveVersion = 1234;
	FGameXXKSaveMigrationReport Report;

	TestFalse(TEXT("future version is rejected"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	TestFalse(TEXT("future rejection reports failure"), Report.bSucceeded);
	TestEqual(TEXT("future rejection reports its source"), Report.SourceVersion, FGameXXKSaveMigration::CurrentSaveVersion + 1);
	TestEqual(TEXT("future rejection targets the dynamic current version"), Report.TargetVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	TestTrue(TEXT("future rejection has a stable diagnostic"), !Report.Error.IsEmpty());
	TestEqual(TEXT("future rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);
	FGameXXKRuntimeState FutureRuntime;
	TestFalse(TEXT("typed restore also rejects a future version"), FGameXXKSaveMigration::TryRestoreRuntimeState(Source, FutureRuntime, Report));

	FGameXXKSaveState NegativeInventory = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	NegativeInventory.RuntimeState.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), -1);
	TestFalse(
		TEXT("current saves with a negative non-stone inventory quantity are rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(NegativeInventory, Migrated, Report));
	TestEqual(TEXT("negative-inventory rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);

	FGameXXKSaveState StaleHeroStats = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	++StaleHeroStats.RuntimeState.PlayerAttack;
	TestFalse(
		TEXT("current saves with combat stats that disagree with the authoritative equipment snapshot are rejected"),
		FGameXXKSaveMigration::MigrateToCurrent(StaleHeroStats, Migrated, Report));
	TestEqual(TEXT("stale-stat rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);

	FGameXXKSaveState StaleRouteReferences = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	StaleRouteReferences.RuntimeState.RouteMapNodes.Add(
		FGameXXKRouteMapNode{10, 1, 0, EGameXXKNodeKind::Start, FVector2D(0.2f, 0.5f), TArray<int32>{}});
	TestFalse(
		TEXT("a current non-generated route cannot retain generated-route nodes"),
		FGameXXKSaveMigration::MigrateToCurrent(StaleRouteReferences, Migrated, Report));
	TestEqual(TEXT("stale-route rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);

	FGameXXKSaveState StaleBattleSource = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	StaleBattleSource.RuntimeState.CardRun.ActiveBattleSourceNodeId = 10;
	TestFalse(
		TEXT("a current non-generated route cannot retain an active-battle source node"),
		FGameXXKSaveMigration::MigrateToCurrent(StaleBattleSource, Migrated, Report));
	TestEqual(TEXT("stale-battle-source rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);

	FGameXXKSaveState EmptyGeneratedRoute = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	EmptyGeneratedRoute.RuntimeState.bHasGeneratedRouteMap = true;
	TestFalse(
		TEXT("a current save cannot claim a generated route without any nodes"),
		FGameXXKSaveMigration::MigrateToCurrent(EmptyGeneratedRoute, Migrated, Report));
	TestEqual(TEXT("empty-generated-route rejection exposes no partial migrated save"), Migrated.SaveVersion, 0);

	FGameXXKSaveState ValidPendingReward = MakeCurrentPendingRewardFixture();
	TestTrue(
		TEXT("the pending-reward mutation starts from a valid current save"),
		FGameXXKSaveMigration::MigrateToCurrent(ValidPendingReward, Migrated, Report));
	FGameXXKSaveState MalformedPendingReward = ValidPendingReward;
	MalformedPendingReward.RuntimeState.CardRun.PendingReward.Options.SetNum(2);
	TestFalse(
		TEXT("a current save cannot retain a malformed tiered pending reward"),
		FGameXXKSaveMigration::MigrateToCurrent(MalformedPendingReward, Migrated, Report));
	TestTrue(TEXT("malformed tiered pending reward reports its contract"), Report.Error.Contains(TEXT("reward")));

	FGameXXKSaveState UnknownLegacyPendingReward = ValidPendingReward;
	UnknownLegacyPendingReward.RuntimeState.CardRun.PendingReward.Options.Reset();
	UnknownLegacyPendingReward.RuntimeState.CardRun.PendingReward.CardIds = {
		TEXT("Route.General.PoJiaTuCi"),
		TEXT("Route.General.ShouShiHuiYuan"),
		TEXT("Route.Unknown")};
	TestFalse(
		TEXT("a current save still rejects an unknown legacy pending reward card"),
		FGameXXKSaveMigration::MigrateToCurrent(UnknownLegacyPendingReward, Migrated, Report));
	TestTrue(TEXT("unknown legacy pending reward reports its contract"), Report.Error.Contains(TEXT("reward")));

	FGameXXKSaveState ValidPendingEvent = MakeCurrentPendingEventFixture(false);
	TestTrue(
		TEXT("the pending-event mutation starts from a valid current save"),
		FGameXXKSaveMigration::MigrateToCurrent(ValidPendingEvent, Migrated, Report));
	FGameXXKSaveState UnknownPendingEvent = ValidPendingEvent;
	UnknownPendingEvent.RuntimeState.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Unknown");
	TestFalse(
		TEXT("a current save cannot retain an unknown pending event or NPC"),
		FGameXXKSaveMigration::MigrateToCurrent(UnknownPendingEvent, Migrated, Report));
	TestTrue(TEXT("unknown pending event reports its contract"), Report.Error.Contains(TEXT("event")));

	FGameXXKSaveState ValidPendingRelic = MakeCurrentPendingEventFixture(true);
	TestTrue(
		TEXT("the pending-relic mutation starts from a valid current save"),
		FGameXXKSaveMigration::MigrateToCurrent(ValidPendingRelic, Migrated, Report));
	FGameXXKSaveState UnknownPendingRelic = ValidPendingRelic;
	UnknownPendingRelic.RuntimeState.CardRun.PendingRelicOffer.RelicIds[2] = TEXT("Relic.Unknown");
	TestFalse(
		TEXT("a current save cannot retain an unknown pending relic"),
		FGameXXKSaveMigration::MigrateToCurrent(UnknownPendingRelic, Migrated, Report));
	TestTrue(TEXT("unknown pending relic reports its contract"), Report.Error.Contains(TEXT("relic")));

	FGameXXKSaveState UnknownPendingSource = ValidPendingEvent;
	UnknownPendingSource.RuntimeState.CardRun.PendingEvent.SourceNodeId = 999;
	TestFalse(
		TEXT("a pending event cannot reference a node outside the generated route"),
		FGameXXKSaveMigration::MigrateToCurrent(UnknownPendingSource, Migrated, Report));
	TestTrue(TEXT("unknown pending source reports its contract"), Report.Error.Contains(TEXT("node")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopSaveMigrationTest,
	"GameXXK.MetaShop.SaveMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopSaveMigrationTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("current save schema is version sixteen"), FGameXXKSaveMigration::CurrentSaveVersion, 16);
	TestEqual(TEXT("meta shop has an explicit schema gate"), FGameXXKSaveMigration::MetaShopIntroducedSaveVersion, 11);

	const FGameXXKSaveState NewGame = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	TestTrue(TEXT("new game receives a positive meta shop seed"), NewGame.RuntimeState.MetaShop.Seed > 0);
	TestEqual(TEXT("new game starts at purchase ordinal zero"), NewGame.RuntimeState.MetaShop.NextPurchaseOrdinal, 0);

	FGameXXKSaveState VersionTen = NewGame;
	VersionTen.SaveVersion = 10;
	VersionTen.RuntimeState.MetaShop = FGameXXKMetaShopState();
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v10 migrates"), FGameXXKSaveMigration::MigrateToCurrent(VersionTen, Migrated, Report));
	TestEqual(TEXT("v10 targets v16"), Migrated.SaveVersion, 16);
	TestTrue(TEXT("v10 migration initializes a positive seed"), Migrated.RuntimeState.MetaShop.Seed > 0);
	TestEqual(TEXT("v10 migration starts at ordinal zero"), Migrated.RuntimeState.MetaShop.NextPurchaseOrdinal, 0);

	FGameXXKSaveState MigratedAgain;
	FGameXXKSaveMigrationReport SecondReport;
	TestTrue(TEXT("the same v10 source migrates twice"), FGameXXKSaveMigration::MigrateToCurrent(VersionTen, MigratedAgain, SecondReport));
	TestEqual(TEXT("v10 seed derivation is deterministic"), MigratedAgain.RuntimeState.MetaShop.Seed, Migrated.RuntimeState.MetaShop.Seed);

	FGameXXKSaveState CurrentRoundTrip;
	FGameXXKSaveMigrationReport CurrentReport;
	TestTrue(TEXT("valid v12 save roundtrips"), FGameXXKSaveMigration::MigrateToCurrent(Migrated, CurrentRoundTrip, CurrentReport));
	TestTrue(
		TEXT("v12 meta shop payload roundtrips exactly"),
		FGameXXKMetaShopState::StaticStruct()->CompareScriptStruct(
			&CurrentRoundTrip.RuntimeState.MetaShop,
			&Migrated.RuntimeState.MetaShop,
			PPF_None));

	FGameXXKSaveState NegativeOrdinal = Migrated;
	NegativeOrdinal.RuntimeState.MetaShop.NextPurchaseOrdinal = -1;
	TestFalse(TEXT("negative purchase ordinal is rejected"), FGameXXKSaveMigration::MigrateToCurrent(NegativeOrdinal, CurrentRoundTrip, CurrentReport));
	TestTrue(TEXT("negative ordinal reports meta shop contract"), CurrentReport.Error.Contains(TEXT("meta shop")));

	FGameXXKSaveState ExhaustedOrdinal = Migrated;
	ExhaustedOrdinal.RuntimeState.MetaShop.NextPurchaseOrdinal = MAX_int32;
	TestFalse(TEXT("exhausted purchase ordinal is rejected"), FGameXXKSaveMigration::MigrateToCurrent(ExhaustedOrdinal, CurrentRoundTrip, CurrentReport));
	TestTrue(TEXT("exhausted ordinal reports meta shop contract"), CurrentReport.Error.Contains(TEXT("meta shop")));

	FGameXXKSaveState ZeroSeed = Migrated;
	ZeroSeed.RuntimeState.MetaShop.Seed = 0;
	TestFalse(TEXT("current save with zero meta shop seed is rejected"), FGameXXKSaveMigration::MigrateToCurrent(ZeroSeed, CurrentRoundTrip, CurrentReport));
	TestTrue(TEXT("zero seed reports meta shop contract"), CurrentReport.Error.Contains(TEXT("meta shop")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteMerchantSaveMigrationRoundTripTest,
	"GameXXK.Equipment.SaveMigration.RouteMerchantRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteMerchantSaveMigrationRoundTripTest::RunTest(const FString& Parameters)
{
	const FGameXXKSaveState Source = MakeCurrentMerchantFixture();
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	const bool bMigrated = FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report);
	TestTrue(TEXT("a valid populated merchant snapshot passes through migration"), bMigrated);
	if (!bMigrated)
	{
		AddError(FString::Printf(TEXT("route merchant fixture migration error: %s"), *Report.Error));
	}
	TestTrue(TEXT("merchant pass-through reports success"), Report.bSucceeded);
	TestTrue(
		TEXT("merchant snapshot roundtrips every persisted property exactly"),
		FGameXXKRouteMerchantState::StaticStruct()->CompareScriptStruct(
			&Migrated.RuntimeState.CardRun.RouteMerchant,
			&Source.RuntimeState.CardRun.RouteMerchant,
			PPF_None));

	FGameXXKSaveState NegativePrice = Source;
	NegativePrice.RuntimeState.CardRun.RouteMerchant.Offers[0].Price = -1;
	TestFalse(TEXT("a merchant offer cannot retain a negative price"), FGameXXKSaveMigration::MigrateToCurrent(NegativePrice, Migrated, Report));
	TestTrue(TEXT("negative merchant price reports its contract"), Report.Error.Contains(TEXT("merchant")));

	FGameXXKSaveState ZeroPrice = Source;
	ZeroPrice.RuntimeState.CardRun.RouteMerchant.Offers[0].Price = 0;
	TestFalse(TEXT("a merchant offer must have a positive price"), FGameXXKSaveMigration::MigrateToCurrent(ZeroPrice, Migrated, Report));
	TestTrue(TEXT("zero merchant price reports its contract"), Report.Error.Contains(TEXT("merchant")));

	FGameXXKSaveState DuplicateOfferId = Source;
	DuplicateOfferId.RuntimeState.CardRun.RouteMerchant.Offers[1].OfferId = DuplicateOfferId.RuntimeState.CardRun.RouteMerchant.Offers[0].OfferId;
	TestFalse(TEXT("a merchant snapshot cannot retain duplicate offer IDs"), FGameXXKSaveMigration::MigrateToCurrent(DuplicateOfferId, Migrated, Report));
	TestTrue(TEXT("duplicate merchant offer ID reports its contract"), Report.Error.Contains(TEXT("merchant")));

	FGameXXKSaveState InvalidOfferKind = Source;
	InvalidOfferKind.RuntimeState.CardRun.RouteMerchant.Offers[0].Kind = EGameXXKRouteMerchantOfferKind::Invalid;
	TestFalse(TEXT("a merchant snapshot cannot retain an invalid offer kind"), FGameXXKSaveMigration::MigrateToCurrent(InvalidOfferKind, Migrated, Report));
	TestTrue(TEXT("invalid merchant offer kind reports its contract"), Report.Error.Contains(TEXT("merchant")));

	FGameXXKSaveState UnknownMerchantSource = Source;
	UnknownMerchantSource.RuntimeState.CardRun.RouteMerchant.SourceNodeId = 999;
	TestFalse(TEXT("a merchant snapshot cannot reference a node outside the generated route"), FGameXXKSaveMigration::MigrateToCurrent(UnknownMerchantSource, Migrated, Report));
	TestTrue(TEXT("unknown merchant source reports its contract"), Report.Error.Contains(TEXT("merchant")));

	FGameXXKSaveState NonMerchantSource = Source;
	NonMerchantSource.RuntimeState.RouteMapNodes[1].NodeKind = EGameXXKNodeKind::Battle;
	TestFalse(TEXT("a merchant snapshot cannot reference a non-merchant route node"), FGameXXKSaveMigration::MigrateToCurrent(NonMerchantSource, Migrated, Report));
	TestTrue(TEXT("non-merchant source reports its contract"), Report.Error.Contains(TEXT("merchant")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSaveMigrationLegacyChainTest,
	"GameXXK.Equipment.SaveMigration.LegacyVersionsZeroThroughFive",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSaveMigrationLegacyChainTest::RunTest(const FString& Parameters)
{
	for (int32 Version : {0, 1})
	{
		FGameXXKSaveState Source;
		Source.SaveVersion = Version;
		Source.QuestState = EGameXXKQuestState::Accepted;
		Source.PlayerLevel = 4;
		Source.PlayerXP = 33;
		Source.PlayerGold = 456;
		Source.bFollowerJoined = true;
		Source.bHasPlayerLocation = true;
		Source.PlayerLocation = FVector(7.0f, 8.0f, 9.0f);
		Source.UnlockedRegions.Add(UGameXXKMVPRules::RegionQingshan());
		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		TestTrue(FString::Printf(TEXT("version %d migrates"), Version), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
		TestEqual(TEXT("legacy facade becomes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
		TestEqual(TEXT("legacy facade preserves level"), Migrated.RuntimeState.PlayerLevel, 4);
		TestEqual(TEXT("legacy facade preserves gold"), Migrated.RuntimeState.PlayerGold, 456);
		TestEqual(TEXT("legacy facade preserves player location"), Migrated.RuntimeState.PlayerLocation, Source.PlayerLocation);
		TestEqual(TEXT("legacy facade does not inherit starter equipment"), Migrated.RuntimeState.EquipmentCollection.EquipmentInstances.Num(), 0);
		TestEqual(TEXT("legacy facade retains the old new-game map baseline"), Migrated.RuntimeState.CurrentMapId, FName(TEXT("MainMenu")));
	}

	for (int32 Version : {2, 3, 4, 5})
	{
		FGameXXKSaveState Source;
		Source.SaveVersion = Version;
		Source.RuntimeState.PlayerLevel = 2;
		Source.RuntimeState.PlayerXP = 12;
		Source.RuntimeState.PlayerGold = 99;
		Source.RuntimeState.EnhancementMaterial = Version == 2 ? 0 : 6;
		Source.RuntimeState.Inventory.Add(TEXT("Item.HealingPowder"), 3);
		if (Version >= 3)
		{
			Source.RuntimeState.Inventory.Add(TEXT("Item.EnhancementStone"), 6);
			Source.RuntimeState.ItemEnhancementLevels.Add(TEXT("Item.WoodenSword"), 4);
		}
		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		TestTrue(FString::Printf(TEXT("version %d migrates"), Version), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
		TestEqual(TEXT("old runtime save becomes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
		TestEqual(TEXT("old runtime save preserves consumables"), Migrated.RuntimeState.Inventory.FindRef(TEXT("Item.HealingPowder")), 3);
		TestEqual(TEXT("version two retains its historic ten-stone default"),
			Migrated.RuntimeState.Inventory.FindRef(TEXT("Item.EnhancementStone")), Version == 2 ? 10 : 6);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSaveMigrationDeterminismTest,
	"GameXXK.Equipment.SaveMigration.VersionSixDeterministicConversion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSaveMigrationDeterminismTest::RunTest(const FString& Parameters)
{
	FGameXXKSaveState Source = MakeDetailedVersionSixFixture();
	const FGameXXKCardCombatUnit* LegacyIntentSource = Source.RuntimeState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Enemy;
	});
	const FGameXXKCardCombatUnit* LegacyIntentTarget = Source.RuntimeState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Party;
	});
	TestNotNull(TEXT("legacy fixture has an enemy source for damage-only intent migration"), LegacyIntentSource);
	TestNotNull(TEXT("legacy fixture has a party target for damage-only intent migration"), LegacyIntentTarget);
	if (!LegacyIntentSource || !LegacyIntentTarget)
	{
		return false;
	}
	FGameXXKCardEnemyIntent LegacyDamageIntent;
	LegacyDamageIntent.CardId = TEXT("Legacy.Intent.DamageOnly");
	LegacyDamageIntent.CardDisplayName = TEXT("旧式攻击");
	LegacyDamageIntent.SourceUnitId = LegacyIntentSource->UnitId;
	LegacyDamageIntent.SuggestedTargetUnitId = LegacyIntentTarget->UnitId;
	LegacyDamageIntent.SourceSlotNumber = LegacyIntentSource->BattleSlotNumber;
	LegacyDamageIntent.TargetSlotNumber = LegacyIntentTarget->BattleSlotNumber;
	LegacyDamageIntent.Damage = 17;
	Source.RuntimeState.CardRun.EnemyIntents = { LegacyDamageIntent };
	Source.RuntimeState.CardRun.NextEnemyIntentIndex = 0;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("detailed version six save migrates"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	TestTrue(TEXT("migration reports success"), Report.bSucceeded);
	TestEqual(TEXT("migration reports source six"), Report.SourceVersion, 6);
	TestEqual(TEXT("migration writes the current version"), Migrated.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);

	const FGameXXKRuntimeState& SourceState = Source.RuntimeState;
	const FGameXXKRuntimeState& State = Migrated.RuntimeState;
	TestEqual(TEXT("sentinel gold survives"), State.PlayerGold, 321);
	TestTrue(TEXT("source fixture contains a real generated route"), SourceState.bHasGeneratedRouteMap);
	TestEqual(TEXT("all generated route nodes survive"), State.RouteMapNodes.Num(), 3);
	TestEqual(TEXT("all generated route edges survive"), State.RouteMapEdges.Num(), 2);
	TestEqual(TEXT("current route node survives"), State.CurrentRouteNodeId, 11);
	TestEqual(TEXT("pending route node survives"), State.PendingRouteNodeId, 11);
	TestEqual(TEXT("visited route projection survives"), State.VisitedRouteNodeIds, SourceState.VisitedRouteNodeIds);
	TestEqual(TEXT("reachable route projection survives"), State.ReachableRouteNodeIds, SourceState.ReachableRouteNodeIds);
	TestEqual(TEXT("route random seed survives"), State.CardRun.RouteRandomSeed, 0x1234567);
	TestTrue(TEXT("source fixture contains a validated active card battle"), SourceState.CardRun.bHasActiveCardBattle);
	FGameXXKCardRunState ExpectedCardRun = SourceState.CardRun;
	ExpectedCardRun.RouteProgress.SchemaVersion = 1;
	ExpectedCardRun.RouteProgress.RootSeed = SourceState.RouteSeed;
	ExpectedCardRun.RouteProgress.ChapterSeeds = {
		SourceState.RouteSeed,
		FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(SourceState.RouteSeed, 2)),
		FMath::Abs(FGameXXKEncounterRules::DeriveChapterSeed(SourceState.RouteSeed, 3))};
	ExpectedCardRun.RouteProgress.CurrentChapter = 1;
	ExpectedCardRun.RouteProgress.RouteCombatLevel = FMath::Clamp(SourceState.PlayerLevel, 1, 20);
	ExpectedCardRun.RouteTravelMoney = SourceState.CardRun.RouteTravelMoney;
	ExpectedCardRun.bRouteEconomyInitialized = true;
	ExpectedCardRun.RewardedTravelMoneyNodes.Reset();
	TArray<FGameXXKRouteCardEntry> ExpectedRouteCardEntries;
	FString ExpectedRouteCardError;
	if (!TestTrue(
		TEXT("the version-six fixture can build its independent canonical route-card recipe"),
		FGameXXKRouteCardRecipe::BuildBaseEntries(
			ExpectedCardRun,
			ExpectedCardRun.RouteProgress.RootSeed,
			ExpectedRouteCardEntries,
			&ExpectedRouteCardError)))
	{
		return false;
	}
	FGameXXKRouteCardEntry ExpectedLegacyRouteCard;
	ExpectedLegacyRouteCard.CardId = TEXT("Route.General.PoJiaTuCi");
	ExpectedLegacyRouteCard.CurrentQuality = EGameXXKCardQuality::Common;
	ExpectedLegacyRouteCard.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
	ExpectedLegacyRouteCard.OwnerUnitId = TEXT("Player");
	ExpectedLegacyRouteCard.bTemporaryRouteCard = true;
	ExpectedLegacyRouteCard.bConsumesRouteCapacity = true;
	ExpectedLegacyRouteCard.AcquisitionOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount;
	if (!TestTrue(
		TEXT("the expected legacy route card receives its independent stable id"),
		FGameXXKRouteCardRecipe::MakeStableEntryId(
			ExpectedCardRun.RouteProgress.RootSeed,
			ExpectedLegacyRouteCard.AcquisitionOrdinal,
			ExpectedLegacyRouteCard.EntryId,
			&ExpectedRouteCardError)))
	{
		return false;
	}
	FGameXXKCardMergePreview ExpectedLegacyMerge;
	if (!TestTrue(
		TEXT("the expected legacy route card independently merges into the canonical base survivor"),
		FGameXXKRunDeckRules::AddAndMerge(
			ExpectedRouteCardEntries,
			ExpectedLegacyRouteCard,
			ExpectedLegacyMerge,
			&ExpectedRouteCardError)))
	{
		return false;
	}
	ExpectedCardRun.RouteCardIds.Reset();
	ExpectedCardRun.RouteCardEntries = MoveTemp(ExpectedRouteCardEntries);
	ExpectedCardRun.NextRouteCardEntryOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount + 1;
	ExpectedCardRun.PendingReward.bRequiresRouteCardReplacement = false;
	FGameXXKResolvedEnemyIntentEffect ExpectedLegacyDamageEffect;
	ExpectedLegacyDamageEffect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
	ExpectedLegacyDamageEffect.TargetUnitIds = { LegacyDamageIntent.SuggestedTargetUnitId };
	ExpectedLegacyDamageEffect.Magnitude = LegacyDamageIntent.Damage;
	ExpectedLegacyDamageEffect.HitCount = 1;
	ExpectedCardRun.EnemyIntents[0].Effects = { ExpectedLegacyDamageEffect };
	TestTrue(
		TEXT("card-run payload survives except for explicit three-chapter and stable route-card migration state"),
		FGameXXKCardRunState::StaticStruct()->CompareScriptStruct(&State.CardRun, &ExpectedCardRun, PPF_None));
	TestTrue(TEXT("version-six legacy route-card IDs are cleared"), State.CardRun.RouteCardIds.IsEmpty());
	TestFalse(TEXT("version-six legacy replacement flag is cleared"),
		State.CardRun.PendingReward.bRequiresRouteCardReplacement);
	TestEqual(
		TEXT("version-six route cards become the canonical eighteen-entry recipe after deterministic merging"),
		State.CardRun.RouteCardEntries.Num(),
		FGameXXKRouteCardRecipe::BaseEntryCount);
	TestEqual(TEXT("one legacy source slot advances next to nineteen"), State.CardRun.NextRouteCardEntryOrdinal, 19);
	const FGameXXKRouteCardEntry* MigratedBaseSurvivor = State.CardRun.RouteCardEntries.FindByPredicate(
		[](const FGameXXKRouteCardEntry& Entry)
		{
			return Entry.AcquisitionOrdinal == 16;
		});
	TestNotNull(TEXT("the canonical PoJiaTuCi base entry survives the legacy duplicate merge"), MigratedBaseSurvivor);
	if (MigratedBaseSurvivor)
	{
		TestEqual(TEXT("the surviving base entry is upgraded to Rare"),
			MigratedBaseSurvivor->CurrentQuality, EGameXXKCardQuality::Rare);
	}
	TestEqual(TEXT("damage-only legacy intent remains one intent"), State.CardRun.EnemyIntents.Num(), 1);
	if (!State.CardRun.EnemyIntents.IsEmpty())
	{
		const FGameXXKCardEnemyIntent& MigratedIntent = State.CardRun.EnemyIntents[0];
		TestEqual(TEXT("damage-only intent preserves source unit"), MigratedIntent.SourceUnitId, LegacyDamageIntent.SourceUnitId);
		TestEqual(TEXT("damage-only intent preserves target unit"), MigratedIntent.SuggestedTargetUnitId, LegacyDamageIntent.SuggestedTargetUnitId);
		TestEqual(TEXT("damage-only intent produces one resolved effect"), MigratedIntent.Effects.Num(), 1);
		if (!MigratedIntent.Effects.IsEmpty())
		{
			const FGameXXKResolvedEnemyIntentEffect& Effect = MigratedIntent.Effects[0];
			TestEqual(TEXT("legacy effect type is direct damage"), Effect.Type, EGameXXKEnemyIntentEffectType::DirectDamage);
			TestEqual(TEXT("legacy effect preserves damage magnitude"), Effect.Magnitude, 17);
			TestEqual(TEXT("legacy effect keeps one hit"), Effect.HitCount, 1);
			TestEqual(TEXT("legacy effect copies its target"), Effect.TargetUnitIds, TArray<FName>{LegacyDamageIntent.SuggestedTargetUnitId});
		}
	}
	TestEqual(TEXT("legacy active battle source node survives"), State.ActiveBattleNodeId, 11);
	TestEqual(TEXT("legacy enemy projection survives"), State.ActiveBattleEnemies.Num(), SourceState.ActiveBattleEnemies.Num());
	TestEqual(TEXT("temporary task NPC provenance survives"), State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.TusiChief")));
	TestEqual(TEXT("route relic survives"), State.CardRun.Relics.Num(), 1);
	TestTrue(TEXT("codex discovery migrates to the current enemy id"), State.DiscoveredCodexEntryIds.Contains(TEXT("Codex.Enemy.Ch1.MoneyRat")));
	TestEqual(TEXT("twelve companions survive in order"), State.CardRun.CompanionRoster.PermanentCompanions.Num(), 12);
	TestTrue(TEXT("pending full-roster replacement survives"), State.CardRun.CompanionRoster.PendingRecruitment.bHasPendingRecruitment);
	TestEqual(
		TEXT("pending replacement identity survives"),
		State.CardRun.CompanionRoster.PendingRecruitment.Candidate.InstanceId,
		Source.RuntimeState.CardRun.CompanionRoster.PendingRecruitment.Candidate.InstanceId);
	if (!State.CardRun.CompanionRoster.PermanentCompanions.IsEmpty())
	{
		TestEqual(TEXT("first companion identity survives"), State.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId, Source.RuntimeState.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId);
	}

	const FGameXXKEquipmentLoadout* Hero = State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	TestNotNull(TEXT("hero loadout is created"), Hero);
	const FGameXXKEquipmentInstance* HeroWeapon = Hero ? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Hero->WeaponInstanceId) : nullptr;
	const FGameXXKEquipmentInstance* HeroArmor = Hero ? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Hero->ArmorInstanceId) : nullptr;
	const FGameXXKEquipmentInstance* HeroAccessory = Hero ? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Hero->AccessoryInstanceId) : nullptr;
	TestTrue(TEXT("hero weapon consumes one wooden-sword copy"), HeroWeapon && HeroWeapon->BaseEquipmentId == TEXT("Item.WoodenSword"));
	TestTrue(TEXT("hero armor consumes one starter-armor copy"), HeroArmor && HeroArmor->BaseEquipmentId == TEXT("Item.StarterClothArmor"));
	TestTrue(TEXT("zero-count equipped accessory synthesizes exactly one copy"), HeroAccessory && HeroAccessory->BaseEquipmentId == TEXT("Item.ClothTalisman"));
	TestEqual(TEXT("wooden-sword enhancement is exact"), HeroWeapon ? HeroWeapon->EnhancementLevel : INDEX_NONE, 2);
	TestEqual(TEXT("starter-armor enhancement is exact"), HeroArmor ? HeroArmor->EnhancementLevel : INDEX_NONE, 4);
	TestEqual(TEXT("cloth-talisman enhancement is exact"), HeroAccessory ? HeroAccessory->EnhancementLevel : INDEX_NONE, 6);

	for (const FGameXXKEquipmentInstance& Instance : State.EquipmentCollection.EquipmentInstances)
	{
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
		TestNotNull(TEXT("every migrated legacy copy is queryable"), Definition);
		TestEqual(TEXT("every migrated legacy copy is Common"), Instance.Quality, EGameXXKEquipmentQuality::Common);
		TestEqual(TEXT("every migrated legacy copy has no random affix"), Instance.RolledAffixes.Num(), 0);
		TestEqual(TEXT("every migrated legacy copy uses old scaling"), Instance.ScalingRule, EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement);
		if (Definition)
		{
			TestTrue(TEXT("every migrated legacy copy preserves exact base snapshot"), StatsEqual(Instance.LegacyBaseStatSnapshot, Definition->LegacyBaseStatSnapshot));
		}
	}

	TestEqual(TEXT("derived wooden-sword count includes hero and warehouse"), State.Inventory.FindRef(TEXT("Item.WoodenSword")), 4);
	TestEqual(TEXT("derived armor count includes hero and warehouse"), State.Inventory.FindRef(TEXT("Item.StarterClothArmor")), 2);
	TestEqual(TEXT("derived accessory count includes hero and companion ownership"), State.Inventory.FindRef(TEXT("Item.ClothTalisman")), 12);
	TestEqual(TEXT("non-equipment consumable is untouched"), State.Inventory.FindRef(TEXT("Item.HealingPowder")), 7);
	TestEqual(TEXT("material remains authoritative"), State.Inventory.FindRef(TEXT("Item.EnhancementStone")), 9);
	TestEqual(TEXT("hero enhancement compatibility mirror derives from equipped instance"), State.ItemEnhancementLevels.FindRef(TEXT("Item.WoodenSword")), 2);
	TestEqual(TEXT("active-route HP preserves exact ten missing health"), State.PlayerHP, State.PlayerMaxHP + 25 - 10);
	TestEqual(TEXT("active-route MP preserves exact five missing mana"), State.PlayerMP, State.PlayerMaxMP + 12 - 5);

	const FName FirstCompanionId = Source.RuntimeState.CardRun.CompanionRoster.PermanentCompanions[0].InstanceId;
	const FGameXXKEquipmentLoadout* FirstCompanionLoadout = State.EquipmentCollection.CharacterLoadouts.Find(FirstCompanionId);
	TestNotNull(TEXT("first companion receives an authoritative loadout"), FirstCompanionLoadout);
	const FGameXXKEquipmentInstance* FirstCompanionWeapon = FirstCompanionLoadout
		? FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, FirstCompanionLoadout->WeaponInstanceId)
		: nullptr;
	TestTrue(TEXT("first same-slot companion item wins"), FirstCompanionWeapon && FirstCompanionWeapon->BaseEquipmentId == TEXT("Item.IronSword"));
	TestNotNull(TEXT("later duplicate-slot item is retained in warehouse"),
		FindByBaseAndOwner(State.EquipmentCollection, TEXT("Item.WoodenSword"), EGameXXKEquipmentOwnerKind::Warehouse, NAME_None));
	TestTrue(TEXT("duplicate companion slot emits a warning"), Report.Warnings.Num() > 0);
	TestTrue(TEXT("converted collection validates against the preserved roster"),
		FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(State.EquipmentCollection, State.CardRun.CompanionRoster));

	FGameXXKSaveState SecondPass;
	FGameXXKSaveMigrationReport SecondReport;
	TestTrue(TEXT("version nine pass-through succeeds"), FGameXXKSaveMigration::MigrateToCurrent(Migrated, SecondPass, SecondReport));
	TestTrue(
		TEXT("version nine pass-through preserves every reflected property"),
		FGameXXKSaveState::StaticStruct()->CompareScriptStruct(&SecondPass, &Migrated, PPF_None));

	FGameXXKSaveState EventSource = MakeDetailedVersionSixFixture();
	FGameXXKRuntimeState& EventRuntime = EventSource.RuntimeState;
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(EventRuntime);
	EventRuntime.bHasActiveBattle = false;
	EventRuntime.ActiveBattleNodeId = INDEX_NONE;
	EventRuntime.ActiveBattleParty.Reset();
	EventRuntime.ActiveBattleEnemies.Reset();
	EventRuntime.CurrentRouteNodeId = 12;
	EventRuntime.PendingRouteNodeId = 12;
	EventRuntime.CardRun.PendingEvent.SourceNodeId = 12;
	EventRuntime.CardRun.PendingEvent.ChoiceSeed = 0x6001;
	EventRuntime.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.YueBai");
	EventRuntime.CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	FGameXXKSaveState MigratedEvent;
	FGameXXKSaveMigrationReport EventReport;
	TestTrue(TEXT("version six pending event migrates"), FGameXXKSaveMigration::MigrateToCurrent(EventSource, MigratedEvent, EventReport));
	TestTrue(
		TEXT("version six pending event survives exactly"),
		FGameXXKPendingRouteEvent::StaticStruct()->CompareScriptStruct(
			&MigratedEvent.RuntimeState.CardRun.PendingEvent,
			&EventRuntime.CardRun.PendingEvent,
			PPF_None));

	FGameXXKSaveState RelicOfferSource = EventSource;
	FGameXXKRuntimeState& RelicOfferRuntime = RelicOfferSource.RuntimeState;
	RelicOfferRuntime.RouteMapNodes[2].NodeKind = EGameXXKNodeKind::Chest;
	RelicOfferRuntime.CardRun.PendingEvent.ChoiceSeed = 0x6002;
	RelicOfferRuntime.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Chest.Bamboo");
	RelicOfferRuntime.CardRun.PendingEvent.EventNpcId = NAME_None;
	RelicOfferRuntime.CardRun.PendingRelicOffer.SourceNodeId = 12;
	RelicOfferRuntime.CardRun.PendingRelicOffer.ChoiceSeed = 0x6002;
	RelicOfferRuntime.CardRun.PendingRelicOffer.RelicIds = {
		TEXT("Relic.AncientCoin"), TEXT("Relic.JadeBell"), TEXT("Relic.BambooTally")};
	FGameXXKSaveState MigratedRelicOffer;
	FGameXXKSaveMigrationReport RelicOfferReport;
	TestTrue(TEXT("version six pending relic offer migrates"), FGameXXKSaveMigration::MigrateToCurrent(RelicOfferSource, MigratedRelicOffer, RelicOfferReport));
	TestTrue(
		TEXT("version six pending relic offer survives exactly"),
		FGameXXKPendingRelicOffer::StaticStruct()->CompareScriptStruct(
			&MigratedRelicOffer.RuntimeState.CardRun.PendingRelicOffer,
			&RelicOfferRuntime.CardRun.PendingRelicOffer,
			PPF_None));

	FGameXXKSaveState RewardSource = MakeDetailedVersionSixFixture();
	FGameXXKRuntimeState& RewardRuntime = RewardSource.RuntimeState;
	RewardRuntime.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	for (FGameXXKCardCombatUnit& Unit : RewardRuntime.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	RewardRuntime.CardRun.EnemyIntents.Reset();
	RewardRuntime.CardRun.NextEnemyIntentIndex = 0;
	RewardRuntime.CardRun.PendingReward.SourceNodeId = 11;
	RewardRuntime.CardRun.PendingReward.ChoiceSeed = 0x6003;
	RewardRuntime.CardRun.PendingReward.CardIds = {
		TEXT("Route.General.PoJiaTuCi"), TEXT("Route.General.ShouShiHuiYuan"), TEXT("Route.General.QingShenQuShi")};
	RewardRuntime.CardRun.PendingReward.bRequiresRouteCardReplacement = false;
	FGameXXKSaveState MigratedReward;
	FGameXXKSaveMigrationReport RewardReport;
	TestTrue(TEXT("version six pending card reward migrates"), FGameXXKSaveMigration::MigrateToCurrent(RewardSource, MigratedReward, RewardReport));
	TestEqual(
		TEXT("the pre-tiering version-six reward offer is cleared"),
		MigratedReward.RuntimeState.CardRun.PendingReward.Options.Num(), 0);
	TestEqual(
		TEXT("the pre-tiering version-six reward cards are cleared"),
		MigratedReward.RuntimeState.CardRun.PendingReward.CardIds.Num(), 0);
	TestEqual(
		TEXT("the pre-tiering version-six reward metadata is cleared"),
		MigratedReward.RuntimeState.CardRun.PendingReward.SourceNodeId, INDEX_NONE);
	TestFalse(
		TEXT("the version-six reward gate re-arms for the next victory"),
		MigratedReward.RuntimeState.CardRun.bActiveBattleRewardResolved);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSaveMigrationOverflowTest,
	"GameXXK.Equipment.SaveMigration.LegacyWarehouseOverflow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSaveMigrationOverflowTest::RunTest(const FString& Parameters)
{
	FGameXXKSaveState Source;
	Source.SaveVersion = 6;
	Source.RuntimeState.PlayerLevel = 1;
	Source.RuntimeState.Inventory.Add(TEXT("Item.WoodenSword"), 201);
	Source.RuntimeState.EquipmentCollection.CollectionSeed = 0x201;
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("201-copy legacy save remains loadable"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	TestEqual(TEXT("all 201 copies survive in warehouse"), Migrated.RuntimeState.EquipmentCollection.WarehouseInstanceIds.Num(), 201);
	TestEqual(TEXT("all 201 instances survive"), Migrated.RuntimeState.EquipmentCollection.EquipmentInstances.Num(), 201);
	TestTrue(TEXT("overflow state is explicitly flagged"), Migrated.RuntimeState.EquipmentCollection.bLegacyWarehouseOverflow);
	TestTrue(TEXT("report exposes migrated overflow"), Report.bCreatedLegacyOverflow);
	TestTrue(TEXT("flagged legacy overflow validates"), FGameXXKEquipmentRules::ValidateCollectionState(Migrated.RuntimeState.EquipmentCollection));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentSaveMigrationRefinementSandMirrorTest,
	"GameXXK.Equipment.SaveMigration.RefinementSandMirrorCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentSaveMigrationRefinementSandMirrorTest::RunTest(const FString& Parameters)
{
	const FName SandId = UGameXXKMVPRules::ItemRefinementSand();

	FGameXXKSaveState LegacyMirrorOnly;
	LegacyMirrorOnly.SaveVersion = FGameXXKSaveMigration::CurrentSaveVersion;
	LegacyMirrorOnly.RuntimeState = UGameXXKMVPRules::CreateNewGame();
	LegacyMirrorOnly.RuntimeState.Inventory.Remove(SandId);
	LegacyMirrorOnly.RuntimeState.EquipmentCollection.RefinementSand = 7;
	FGameXXKSaveState MigratedLegacyMirrorOnly;
	FGameXXKSaveMigrationReport LegacyMirrorOnlyReport;
	TestTrue(TEXT("a current-version save written before backpack sand remains loadable"),
		FGameXXKSaveMigration::MigrateToCurrent(
			LegacyMirrorOnly,
			MigratedLegacyMirrorOnly,
			LegacyMirrorOnlyReport));
	TestEqual(TEXT("legacy collection-only sand is preserved in the authoritative backpack item"),
		MigratedLegacyMirrorOnly.RuntimeState.Inventory.FindRef(SandId), 7);
	TestEqual(TEXT("legacy collection-only sand keeps its compatibility mirror"),
		MigratedLegacyMirrorOnly.RuntimeState.EquipmentCollection.RefinementSand, 7);

	FGameXXKSaveState MismatchedCurrent = LegacyMirrorOnly;
	MismatchedCurrent.RuntimeState.Inventory.FindOrAdd(SandId) = 3;
	MismatchedCurrent.RuntimeState.EquipmentCollection.RefinementSand = 7;
	FGameXXKSaveState MigratedMismatch;
	FGameXXKSaveMigrationReport MismatchReport;
	TestTrue(TEXT("an explicit backpack sand balance repairs a stale collection mirror"),
		FGameXXKSaveMigration::MigrateToCurrent(MismatchedCurrent, MigratedMismatch, MismatchReport));
	TestEqual(TEXT("the explicit backpack sand balance remains authoritative"),
		MigratedMismatch.RuntimeState.Inventory.FindRef(SandId), 3);
	TestEqual(TEXT("migration rewrites the stale collection sand mirror"),
		MigratedMismatch.RuntimeState.EquipmentCollection.RefinementSand, 3);
	return true;
}

#endif
