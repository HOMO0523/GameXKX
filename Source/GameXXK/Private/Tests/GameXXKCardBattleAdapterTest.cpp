#include "Misc/AutomationTest.h"
#include "Misc/Crc.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKRouteCardRecipe.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeLegacyBattleUnit(
		const TCHAR* Id,
		const TCHAR* DisplayName,
		const int32 Health,
		const int32 Mana,
		const int32 Attack,
		const int32 Defense,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.MP = Mana;
		Unit.MaxMP = Mana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.Shield = 1;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeCardCombatUnit(
		const TCHAR* Id,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 Health = 100,
		const int32 Attack = 10)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(Id);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.Attack = Attack;
		Unit.StableSortOrder = StableSortOrder;
		Unit.bLiving = Health > 0;
		return Unit;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleAdapterTest,
	"GameXXK.Integration.CardBattleAdapter.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleAdapterTest::RunTest(const FString& Parameters)
{
	const FName StableEnemyId(TEXT("Balance.Enemy.Ch2.Macaque.P1"));
	const uint32 ExpectedRandomTargetSeed = FCrc::StrCrc32(*StableEnemyId.ToString())
		^ static_cast<uint32>(1 * 2654435761U);
	TestEqual(
		TEXT("random enemy target selection derives from the stable lexical unit ID instead of the process-local FName index"),
		static_cast<int64>(FGameXXKCardBattleAdapter::MakeStableEnemyIntentTargetSeed(StableEnemyId, 1)),
		static_cast<int64>(ExpectedRandomTargetSeed));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(FString::Printf(TEXT("a migrated or new runtime receives the approved card-run defaults: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestEqual(TEXT("the fixed hero starts with the approved eight selected permanent cards"), State.CardRun.HeroSelectedCardIds.Num(), 8);
	TestEqual(TEXT("the fixed hero exposes all twelve approved permanent cards for the eight-slot loadout editor"), State.CardRun.HeroUnlockedCardIds.Num(), 12);
	TArray<FName> EditedHeroLoadout = State.CardRun.HeroUnlockedCardIds;
	EditedHeroLoadout.RemoveAt(0, 4, EAllowShrinking::No);
	TestEqual(TEXT("the edited hero fixture still contains exactly eight selections"), EditedHeroLoadout.Num(), 8);
	TestTrue(TEXT("the fixed hero can swap the first four defaults for the other approved cards"),
		FGameXXKCardBattleAdapter::SetHeroSelectedCards(State, EditedHeroLoadout, &Error));
	TestEqual(TEXT("the edited hero loadout persists the chosen eight cards"), State.CardRun.HeroSelectedCardIds, EditedHeroLoadout);

	FGameXXKRuntimeState MigratedState = State;
	MigratedState.CardRun.HeroUnlockedCardIds.SetNum(8, EAllowShrinking::No);
	MigratedState.CardRun.HeroSelectedCardIds = MigratedState.CardRun.HeroUnlockedCardIds;
	const TArray<FName> PreviouslyEquippedHeroCards = MigratedState.CardRun.HeroSelectedCardIds;
	TestTrue(TEXT("an old eight-card hero save safely migrates to the full editable pool"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(MigratedState, &Error));
	TestEqual(TEXT("migration backfills the four missing approved hero cards"), MigratedState.CardRun.HeroUnlockedCardIds.Num(), 12);
	TestEqual(TEXT("migration keeps the previously equipped eight cards intact"),
		MigratedState.CardRun.HeroSelectedCardIds, PreviouslyEquippedHeroCards);

	TestTrue(TEXT("the accepted route can attach a specific temporary task NPC with three fixed cards"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &Error));
	TestEqual(TEXT("a named temporary task NPC persists exactly three configured cards"), State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("the configured temporary task NPC persists its stable identity"), State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.TusiChief")));

	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 30, 15, 8, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("MoneyRat"), TEXT("钱鼠"), 60, 0, 9, 2, true)};
	State.ActiveBattleEnemies[0].Shield = 7;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 42;
	TestTrue(FString::Printf(TEXT("a route battle builds one serialized shared card runtime from the locked party: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 991, &Error));
	TestTrue(TEXT("the active card battle is explicitly persisted inside runtime state"), State.CardRun.bHasActiveCardBattle);
	TestTrue(TEXT("the active card battle passes its independent persistence validation"), GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	TestEqual(TEXT("hero plus one temporary NPC and deterministic fillers still creates the exact eighteen-card opening deck"), State.CardRun.ActiveBattle.Deck.ActiveInstanceIds.Num(), 18);
	TestEqual(TEXT("the opening card runtime begins with five materialized hand cards"), State.CardRun.ActiveBattle.Deck.Hand.Num(), 5);
	TestEqual(TEXT("the legacy projection contains hero and one task NPC, not the old automatic follower"), State.ActiveBattleParty.Num(), 2);
	TestTrue(TEXT("the temporary task NPC has a stable card-runtime unit"), State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief") && Unit.Side == EGameXXKCardTargetSide::Party;
	}));

	FGameXXKCardCombatUnit* HeroUnit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	FGameXXKCardCombatUnit* EnemyUnit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("MoneyRat");
	});
	TestNotNull(TEXT("the card-runtime projection retains the fixed hero"), HeroUnit);
	TestNotNull(TEXT("the card-runtime projection retains the route enemy by stable id"), EnemyUnit);
	if (HeroUnit && EnemyUnit)
	{
		TestEqual(TEXT("opening legacy shield initializes card runtime armor exactly once"), EnemyUnit->Armor, 7);
		HeroUnit->HP = 71;
		HeroUnit->Mana = 12;
		EnemyUnit->HP = 33;
		EnemyUnit->Armor = 4;
		TestTrue(TEXT("projection sync only reflects serialized card state into legacy battle widgets"), FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error));
		TestEqual(TEXT("legacy hero health projects from the card runtime"), State.ActiveBattleParty[0].HP, 71);
		TestEqual(TEXT("legacy hero mana projects from the card runtime"), State.ActiveBattleParty[0].MP, 12);
		TestEqual(TEXT("legacy enemy health projects from the card runtime"), State.ActiveBattleEnemies[0].HP, 33);
		TestEqual(TEXT("card runtime armor remains authoritative when syncing legacy shield"), State.ActiveBattleEnemies[0].Shield, 4);
	}

	FGameXXKRuntimeState ExplicitSlotState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("explicit-slot fixture initializes the route card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(ExplicitSlotState, &Error));
	ExplicitSlotState.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 30, 15, 8, false)};
	ExplicitSlotState.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.Rooster.P1"), TEXT("公鸡"), 46, 0, 8, 1, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Goat.P3"), TEXT("山羊"), 58, 0, 7, 3, true)};
	ExplicitSlotState.ActiveBattleEnemies[0].EnemyDefinitionId = TEXT("Enemy.Ch1.Rooster");
	ExplicitSlotState.ActiveBattleEnemies[0].BattleSlotNumber = 1;
	ExplicitSlotState.ActiveBattleEnemies[0].CombatLevel = 4;
	ExplicitSlotState.ActiveBattleEnemies[1].EnemyDefinitionId = TEXT("Enemy.Ch1.Goat");
	ExplicitSlotState.ActiveBattleEnemies[1].BattleSlotNumber = 3;
	ExplicitSlotState.ActiveBattleEnemies[1].CombatLevel = 4;
	ExplicitSlotState.bHasActiveBattle = true;
	ExplicitSlotState.ActiveBattleNodeId = 73;
	TestTrue(TEXT("explicit encounter slots build a card runtime"),
		FGameXXKCardBattleAdapter::BeginCardBattle(ExplicitSlotState, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 7373, &Error));
	const FGameXXKCardCombatUnit* ExplicitThreeP = ExplicitSlotState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Goat.P3");
	});
	TestNotNull(TEXT("explicit P3 enemy reaches the card runtime"), ExplicitThreeP);
	if (ExplicitThreeP)
	{
		TestEqual(TEXT("card runtime preserves explicit definition identity"), ExplicitThreeP->EnemyDefinitionId, FName(TEXT("Enemy.Ch1.Goat")));
		TestEqual(TEXT("card runtime preserves explicit P3 slot instead of array-index fallback"), ExplicitThreeP->BattleSlotNumber, 3);
		TestEqual(TEXT("card runtime preserves the route combat-level snapshot"), ExplicitThreeP->CombatLevel, 4);
		TestEqual(TEXT("presentation uses explicit P3 rather than its legacy array index"), FGameXXKBattlePresentation::GetSlotNumber(ExplicitSlotState.CardRun.ActiveBattle, ExplicitThreeP->UnitId), 3);
	}

	FGameXXKRuntimeState DuplicateSlotState = ExplicitSlotState;
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(DuplicateSlotState);
	DuplicateSlotState.ActiveBattleEnemies[1].BattleSlotNumber = 1;
	const FGameXXKRuntimeState DuplicateSlotBefore = DuplicateSlotState;
	Error.Reset();
	TestFalse(TEXT("duplicate explicit enemy presentation slots are rejected before card runtime commit"),
		FGameXXKCardBattleAdapter::BeginCardBattle(DuplicateSlotState, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 7373, &Error));
	TestTrue(TEXT("duplicate explicit slots report a concrete failure"), Error.Contains(TEXT("duplicate explicit presentation slots")));
	TestEqual(TEXT("rejected duplicate explicit slots preserve the source enemy slot"), DuplicateSlotState.ActiveBattleEnemies[1].BattleSlotNumber, DuplicateSlotBefore.ActiveBattleEnemies[1].BattleSlotNumber);
	TestFalse(TEXT("rejected duplicate explicit slots do not create an active card runtime"), DuplicateSlotState.CardRun.bHasActiveCardBattle);

	FGameXXKRuntimeState QuestNpcSnapshotState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("task-NPC snapshot fixture initializes the route card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(QuestNpcSnapshotState, &Error));
	TestTrue(TEXT("task-NPC snapshot fixture configures the route NPC"),
		FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(QuestNpcSnapshotState, TEXT("Npc.TusiChief"), {}, &Error));
	QuestNpcSnapshotState.PlayerLevel = 20;
	QuestNpcSnapshotState.CardRun.RouteProgress.SchemaVersion = 1;
	QuestNpcSnapshotState.CardRun.RouteProgress.RootSeed = 713;
	QuestNpcSnapshotState.CardRun.RouteProgress.ChapterSeeds = {713, 917, 1223};
	QuestNpcSnapshotState.CardRun.RouteProgress.CurrentChapter = 1;
	QuestNpcSnapshotState.CardRun.RouteProgress.RouteCombatLevel = 4;
	QuestNpcSnapshotState.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.Snapshot.P1"), TEXT("测试敌人"), 60, 0, 8, 1, true)};
	QuestNpcSnapshotState.bHasActiveBattle = true;
	QuestNpcSnapshotState.ActiveBattleNodeId = 74;
	TestTrue(TEXT("task-NPC snapshot fixture begins a card battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(QuestNpcSnapshotState, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 7474, &Error));
	const FGameXXKCardCombatUnit* SnapshotNpc = QuestNpcSnapshotState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Npc.TusiChief");
	});
	const FGameXXKCardCombatUnit* SnapshotHero = QuestNpcSnapshotState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Role == EGameXXKCharacterRole::Hero;
	});
	TestNotNull(TEXT("task NPC joins the card runtime during the snapshot fixture"), SnapshotNpc);
	TestNotNull(TEXT("hero joins the card runtime during the snapshot fixture"), SnapshotHero);
	if (SnapshotNpc && SnapshotHero)
	{
		TestEqual(TEXT("task NPC level exactly mirrors the current hero"), SnapshotNpc->CombatLevel, SnapshotHero->CombatLevel);
		TestEqual(TEXT("task NPC health exactly mirrors the current hero"), SnapshotNpc->MaxHP, SnapshotHero->MaxHP);
		TestEqual(TEXT("task NPC mana exactly mirrors the current hero"), SnapshotNpc->MaxMana, SnapshotHero->MaxMana);
		TestEqual(TEXT("task NPC attack exactly mirrors the current hero"), SnapshotNpc->Attack, SnapshotHero->Attack);
		TestEqual(TEXT("task NPC defense exactly mirrors the current hero"), SnapshotNpc->Defense, SnapshotHero->Defense);
		TestEqual(TEXT("task NPC speed exactly mirrors the current hero"), SnapshotNpc->Speed, SnapshotHero->Speed);
	}

	FGameXXKRuntimeState RewardState = UGameXXKMVPRules::CreateNewGame();
	bool bRewardBattleReady = UGameXXKMVPRules::OpenWorldMap(RewardState)
		&& UGameXXKMVPRules::EnterWorldRegion(RewardState, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(RewardState)
		&& UGameXXKMVPRules::EnterDungeon(RewardState);
	RewardState.bHasGeneratedRouteMap = false;
	RewardState.RouteMapNodes.Reset();
	RewardState.RouteMapEdges.Reset();
	RewardState.ReachableRouteNodeIds.Reset();
	RewardState.DungeonNodeIndex = 1;
	bRewardBattleReady = bRewardBattleReady
		&& UGameXXKMVPRules::AdvanceDungeonNode(RewardState, EGameXXKNodeKind::Battle);
	for (FGameXXKCardCombatUnit& Unit : RewardState.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	RewardState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("the route-reward fixture reaches a real locked battle victory"), bRewardBattleReady);

	const int32 RewardEntryOrdinalBefore = RewardState.CardRun.NextRouteCardEntryOrdinal;
	const int32 RewardAcquisitionCountBefore = RewardState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount;
	const int32 RewardSourceNodeId = RewardState.CardRun.ActiveBattleSourceNodeId >= 0
		? RewardState.CardRun.ActiveBattleSourceNodeId
		: RewardState.DungeonNodeIndex;
	TArray<FName> RewardChoiceIds;
	TestTrue(FString::Printf(TEXT("a normal battle produces a deterministic three-card route reward offer: %s"), *Error),
		FGameXXKCardBattleAdapter::CreateRouteRewardOffer(RewardState, EGameXXKNodeKind::Battle, RewardSourceNodeId, 2026, RewardChoiceIds, &Error));
	TestEqual(TEXT("normal battle reward exposes exactly three card choices"), RewardChoiceIds.Num(), 3);
	TSet<FName> UniqueRewardIds(RewardChoiceIds);
	TestEqual(TEXT("normal battle reward never duplicates a card within its visible three choices"), UniqueRewardIds.Num(), RewardChoiceIds.Num());
	FName ChosenRewardCardId = NAME_None;
	for (const FName RewardCardId : RewardChoiceIds)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(RewardCardId);
		const bool bWouldMerge = Definition && RewardState.CardRun.RouteCardEntries.ContainsByPredicate(
			[RewardCardId, Definition](const FGameXXKRouteCardEntry& Entry)
			{
				return Entry.CardId == RewardCardId && Entry.CurrentQuality == Definition->BaseQuality;
			});
		if (Definition && !bWouldMerge)
		{
			ChosenRewardCardId = RewardCardId;
			break;
		}
	}
	TestFalse(TEXT("the deterministic offer contains a direct-acquisition candidate for field-level assertions"), ChosenRewardCardId.IsNone());
	if (!ChosenRewardCardId.IsNone())
	{
		TestTrue(TEXT("the selected reward commits as a route-local card rather than a permanent hero card"),
			FGameXXKCardBattleAdapter::ChoosePendingRouteReward(RewardState, ChosenRewardCardId, NAME_None, &Error));
		const FGameXXKRouteCardEntry* AcquiredEntry = RewardState.CardRun.RouteCardEntries.FindByPredicate(
			[ChosenRewardCardId, RewardEntryOrdinalBefore](const FGameXXKRouteCardEntry& Entry)
			{
				return Entry.CardId == ChosenRewardCardId
					&& Entry.AcquisitionOrdinal == RewardEntryOrdinalBefore;
			});
		const FGameXXKCardDefinition* RewardDefinition = FGameXXKCardCatalog::FindCardDefinition(ChosenRewardCardId);
		TestNotNull(TEXT("a chosen route reward remains in stable route-entry authority"), AcquiredEntry);
		TestNotNull(TEXT("the selected route reward resolves its catalog definition"), RewardDefinition);
		if (AcquiredEntry && RewardDefinition)
		{
			FName ExpectedEntryId;
			TestTrue(TEXT("the chosen reward derives a stable entry ID"),
				FGameXXKRouteCardRecipe::MakeStableEntryId(
					RewardState.CardRun.RouteProgress.RootSeed,
					RewardEntryOrdinalBefore,
					ExpectedEntryId));
			TestEqual(TEXT("the chosen reward keeps catalog base quality"), AcquiredEntry->CurrentQuality, RewardDefinition->BaseQuality);
			TestEqual(TEXT("the chosen reward records route-reward source"), AcquiredEntry->SourceKind, EGameXXKRouteCardSourceKind::RouteReward);
			TestEqual(TEXT("the chosen reward belongs to the player unit"), AcquiredEntry->OwnerUnitId, FName(TEXT("Player")));
			TestEqual(TEXT("the chosen reward records its pre-commit acquisition ordinal"), AcquiredEntry->AcquisitionOrdinal, RewardEntryOrdinalBefore);
			TestEqual(TEXT("the chosen reward stores the derived stable entry ID"), AcquiredEntry->EntryId, ExpectedEntryId);
			TestTrue(TEXT("the chosen reward is temporary route capacity"), AcquiredEntry->bTemporaryRouteCard && AcquiredEntry->bConsumesRouteCapacity);
		}
		TestEqual(TEXT("the selected reward advances the stable-entry sequence exactly once"),
			RewardState.CardRun.NextRouteCardEntryOrdinal,
			RewardEntryOrdinalBefore + 1);
		TestEqual(TEXT("the selected reward advances acquisition history exactly once"),
			RewardState.CardRun.RouteProgress.ActualRouteCardAcquisitionCount,
			RewardAcquisitionCountBefore + 1);
		TestTrue(TEXT("the selected reward leaves legacy RouteCardIds empty"), RewardState.CardRun.RouteCardIds.IsEmpty());
		TestEqual(TEXT("the reward offer clears only after an explicit pick"), RewardState.CardRun.PendingReward.CardIds.Num(), 0);
	}
	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(RewardState);
	TestTrue(TEXT("ending the reward route removes stable route-card entries"), RewardState.CardRun.RouteCardEntries.IsEmpty());
	TestTrue(TEXT("ending the reward route keeps legacy RouteCardIds empty"), RewardState.CardRun.RouteCardIds.IsEmpty());

	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(State);
	TestFalse(TEXT("ending the route clears an in-progress card battle"), State.CardRun.bHasActiveCardBattle);
	TestTrue(TEXT("ending the route removes stable route-card entries"), State.CardRun.RouteCardEntries.IsEmpty());
	TestTrue(TEXT("ending the route removes legacy route-card residue"), State.CardRun.RouteCardIds.IsEmpty());
	TestEqual(TEXT("ending the route removes the temporary task NPC selection"), State.CardRun.PartySelection.QuestNpc.NpcId, NAME_None);
	TestEqual(TEXT("ending the route preserves the hero's permanent configured cards"), State.CardRun.HeroSelectedCardIds.Num(), 8);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattlePresentationAndIntentTest,
	"GameXXK.Integration.CardBattle.Adapter.PresentationAndIntentSteps",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattlePresentationAndIntentTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime PresentationRuntime;
	PresentationRuntime.Units = {
		MakeCardCombatUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0),
		MakeCardCombatUnit(TEXT("Companion.Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 1),
		MakeCardCombatUnit(TEXT("Npc.YueBai"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 2),
		MakeCardCombatUnit(TEXT("Enemy.One"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0),
		MakeCardCombatUnit(TEXT("Enemy.Two"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1),
		MakeCardCombatUnit(TEXT("Enemy.Three"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 2)};
	FGameXXKCardCombatUnit* const PresentationHero = PresentationRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("presentation fixture resolves the authoritative hero unit"), PresentationHero);
	if (!PresentationHero)
	{
		return false;
	}
	PresentationHero->HP = 72;
	PresentationHero->MaxHP = 100;
	PresentationHero->Mana = 18;
	PresentationHero->MaxMana = 30;
	PresentationHero->Armor = 7;
	FGameXXKCardStatusStack HeroPoison;
	HeroPoison.Status = EGameXXKCardStatus::Poison;
	HeroPoison.Stacks = 2;
	PresentationHero->Statuses = {HeroPoison};
	FGameXXKCardCombatUnit* const PresentationEnemy = PresentationRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.One");
	});
	TestNotNull(TEXT("presentation fixture resolves the authoritative enemy unit"), PresentationEnemy);
	if (!PresentationEnemy)
	{
		return false;
	}
	PresentationEnemy->HP = 0;
	PresentationEnemy->MaxHP = 240;
	PresentationEnemy->Mana = 99;
	PresentationEnemy->MaxMana = 100;
	PresentationEnemy->Armor = 4;
	PresentationEnemy->bLiving = false;
	FGameXXKCardStatusStack EnemyBleed;
	EnemyBleed.Status = EGameXXKCardStatus::Bleed;
	EnemyBleed.Stacks = 3;
	PresentationEnemy->Statuses = {EnemyBleed};

	FGameXXKRuntimeState LegacyDisagreementState;
	LegacyDisagreementState.CardRun.ActiveBattle = PresentationRuntime;
	LegacyDisagreementState.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("旧主角投影"), 9, 1, 1, 0, false)};
	LegacyDisagreementState.ActiveBattleParty[0].Shield = 99;
	LegacyDisagreementState.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.One"), TEXT("旧敌人投影"), 240, 0, 1, 0, true)};
	LegacyDisagreementState.ActiveBattleEnemies[0].Shield = 88;
	TestTrue(TEXT("legacy fixture deliberately disagrees with authoritative hero values"),
		LegacyDisagreementState.ActiveBattleParty[0].HP != PresentationHero->HP
		&& LegacyDisagreementState.ActiveBattleParty[0].MP != PresentationHero->Mana
		&& LegacyDisagreementState.ActiveBattleParty[0].Shield != PresentationHero->Armor);
	TestTrue(TEXT("legacy fixture deliberately disagrees with authoritative enemy values"),
		LegacyDisagreementState.ActiveBattleEnemies[0].HP != PresentationEnemy->HP
		&& LegacyDisagreementState.ActiveBattleEnemies[0].MP != PresentationEnemy->Mana
		&& LegacyDisagreementState.ActiveBattleEnemies[0].Shield != PresentationEnemy->Armor);

	FGameXXKBattleUnitHudView HeroView;
	TestTrue(TEXT("HUD view resolves a stable hero UnitId"),
		FGameXXKBattlePresentation::BuildUnitHudView(
			LegacyDisagreementState.CardRun.ActiveBattle, TEXT("Player"), FText::FromString(TEXT("HUD 主角")), HeroView));
	TestEqual(TEXT("HUD view preserves UnitId"), HeroView.UnitId, FName(TEXT("Player")));
	TestEqual(TEXT("HUD view copies authoritative hero side"), HeroView.Side, EGameXXKCardTargetSide::Party);
	TestEqual(TEXT("HUD view copies authoritative hero role"), HeroView.Role, EGameXXKCharacterRole::Hero);
	TestEqual(TEXT("HUD view uses the central fixed hero P slot"), HeroView.SlotNumber, 2);
	TestTrue(TEXT("HUD view copies authoritative hero living state"), HeroView.bLiving);
	TestTrue(TEXT("HUD view enables mana only for party units"), HeroView.bShowMana);
	TestEqual(TEXT("HUD view reads card HP"), HeroView.CurrentHP, 72);
	TestEqual(TEXT("HUD view reads card max HP"), HeroView.MaxHP, 100);
	TestEqual(TEXT("HUD view reads card MP"), HeroView.CurrentMana, 18);
	TestEqual(TEXT("HUD view reads card max MP"), HeroView.MaxMana, 30);
	TestEqual(TEXT("HUD view reads card armor"), HeroView.Armor, 7);
	TestEqual(TEXT("HUD view retains display-only caller text"), HeroView.DisplayName.ToString(), FString(TEXT("HUD 主角")));
	TestEqual(TEXT("HUD display text never replaces authoritative hero HP"), HeroView.CurrentHP, PresentationHero->HP);
	TestEqual(TEXT("HUD view copies the complete hero status count"), HeroView.Statuses.Num(), PresentationHero->Statuses.Num());
	TestTrue(TEXT("HUD view keeps poison stacks"), HeroView.Statuses.ContainsByPredicate([](const FGameXXKCardStatusStack& Stack)
	{
		return Stack.Status == EGameXXKCardStatus::Poison && Stack.Stacks == 2;
	}));

	FGameXXKBattleUnitHudView EnemyView;
	TestTrue(TEXT("HUD view resolves the stable enemy UnitId"),
		FGameXXKBattlePresentation::BuildUnitHudView(
			LegacyDisagreementState.CardRun.ActiveBattle, TEXT("Enemy.One"), FText::FromString(TEXT("HUD 敌人")), EnemyView));
	TestEqual(TEXT("HUD view copies authoritative enemy UnitId"), EnemyView.UnitId, PresentationEnemy->UnitId);
	TestEqual(TEXT("HUD view copies authoritative enemy side"), EnemyView.Side, PresentationEnemy->Side);
	TestEqual(TEXT("HUD view copies authoritative enemy role"), EnemyView.Role, PresentationEnemy->Role);
	TestFalse(TEXT("HUD view copies authoritative enemy living state"), EnemyView.bLiving);
	TestFalse(TEXT("HUD view hides mana for enemy units"), EnemyView.bShowMana);
	TestEqual(TEXT("HUD view copies authoritative enemy HP"), EnemyView.CurrentHP, PresentationEnemy->HP);
	TestEqual(TEXT("HUD view copies authoritative enemy max HP"), EnemyView.MaxHP, PresentationEnemy->MaxHP);
	TestEqual(TEXT("HUD view copies authoritative enemy mana"), EnemyView.CurrentMana, PresentationEnemy->Mana);
	TestEqual(TEXT("HUD view copies authoritative enemy max mana"), EnemyView.MaxMana, PresentationEnemy->MaxMana);
	TestEqual(TEXT("HUD view copies authoritative enemy armor"), EnemyView.Armor, PresentationEnemy->Armor);
	TestEqual(TEXT("HUD view preserves enemy display-only caller text"), EnemyView.DisplayName.ToString(), FString(TEXT("HUD 敌人")));
	TestEqual(TEXT("HUD view copies the complete enemy status count"), EnemyView.Statuses.Num(), PresentationEnemy->Statuses.Num());
	TestTrue(TEXT("HUD view copies enemy bleed stacks"), EnemyView.Statuses.ContainsByPredicate([](const FGameXXKCardStatusStack& Stack)
	{
		return Stack.Status == EGameXXKCardStatus::Bleed && Stack.Stacks == 3;
	}));

	FGameXXKBattleUnitHudView InvalidView;
	InvalidView.UnitId = TEXT("PreviousValue");
	TestFalse(TEXT("HUD view rejects NAME_None identity"),
		FGameXXKBattlePresentation::BuildUnitHudView(
			LegacyDisagreementState.CardRun.ActiveBattle, NAME_None, FText::FromString(TEXT("无效")), InvalidView));
	TestEqual(TEXT("failed NAME_None HUD view resets its stable identity"), InvalidView.UnitId, NAME_None);
	InvalidView.UnitId = TEXT("PreviousValue");
	TestFalse(TEXT("HUD view rejects a missing stable identity"),
		FGameXXKBattlePresentation::BuildUnitHudView(
			LegacyDisagreementState.CardRun.ActiveBattle, TEXT("Missing.Unit"), FText::FromString(TEXT("缺失")), InvalidView));
	TestEqual(TEXT("failed missing HUD view resets its stable identity"), InvalidView.UnitId, NAME_None);
	TestEqual(TEXT("hero is always player 2P"), FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Player")), 2);
	TestEqual(TEXT("permanent companion is always player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Companion.Blade")), 1);
	TestEqual(TEXT("temporary NPC is always player 3P"), FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Npc.YueBai")), 3);
	TestEqual(TEXT("enemy stable order maps outer to inner"), FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Enemy.Three")), 3);
	FGameXXKCardBattleRuntime PermanentRoleRuntime;
	PermanentRoleRuntime.Units = {
		MakeCardCombatUnit(TEXT("Companion.Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 0),
		MakeCardCombatUnit(TEXT("Companion.Guard"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 1),
		MakeCardCombatUnit(TEXT("Companion.Healer"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Healer, 2),
		MakeCardCombatUnit(TEXT("Companion.Hunter"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 3),
		MakeCardCombatUnit(TEXT("Companion.Sorcerer"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 4),
		MakeCardCombatUnit(TEXT("Companion.FormationMaster"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::FormationMaster, 5)};
	TestEqual(TEXT("blade companion maps to player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.Blade")), 1);
	TestEqual(TEXT("guard companion maps to player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.Guard")), 1);
	TestEqual(TEXT("healer companion maps to player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.Healer")), 1);
	TestEqual(TEXT("hunter companion maps to player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.Hunter")), 1);
	TestEqual(TEXT("sorcerer companion maps to player 1P"), FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.Sorcerer")), 1);
	TestEqual(TEXT("formation master companion maps to player 1P"),
		FGameXXKBattlePresentation::GetSlotNumber(PermanentRoleRuntime, TEXT("Companion.FormationMaster")), 1);

	FGameXXKCardBattleRuntime ReorderedPartyRuntime;
	ReorderedPartyRuntime.Units = {
		MakeCardCombatUnit(TEXT("Npc.Reordered"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 0),
		MakeCardCombatUnit(TEXT("Companion.Reordered"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Guard, 1),
		MakeCardCombatUnit(TEXT("Player.Reordered"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2)};
	TestEqual(TEXT("reordered party still maps hero from role rather than array index"),
		FGameXXKBattlePresentation::GetSlotNumber(ReorderedPartyRuntime, TEXT("Player.Reordered")), 2);
	TestEqual(TEXT("reordered party still maps companion from role rather than array index"),
		FGameXXKBattlePresentation::GetSlotNumber(ReorderedPartyRuntime, TEXT("Companion.Reordered")), 1);
	TestEqual(TEXT("reordered party still maps task NPC from role rather than array index"),
		FGameXXKBattlePresentation::GetSlotNumber(ReorderedPartyRuntime, TEXT("Npc.Reordered")), 3);
	if (FGameXXKCardCombatUnit* DefeatedEnemy = PresentationRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Two");
	}))
	{
		DefeatedEnemy->HP = 0;
		DefeatedEnemy->bLiving = false;
	}
	TestEqual(TEXT("a defeated enemy keeps its stable P slot instead of renumbering others"),
		FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Enemy.Three")), 3);
	TestEqual(TEXT("missing units do not receive a presentation slot"),
		FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Enemy.Missing")), INDEX_NONE);
	FGameXXKCardBattleRuntime InvalidPresentationRuntime = PresentationRuntime;
	InvalidPresentationRuntime.Units.Add(MakeCardCombatUnit(TEXT("Enemy.InvalidSlot"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, -2));
	TestEqual(TEXT("enemies without valid stable order do not receive a presentation slot"),
		FGameXXKBattlePresentation::GetSlotNumber(InvalidPresentationRuntime, TEXT("Enemy.InvalidSlot")), INDEX_NONE);
	TestEqual(TEXT("party P slot labels remain role based"),
		FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Party, 2), FString(TEXT("我 2P")));
	TestEqual(TEXT("enemy P slot labels use the enemy side"),
		FGameXXKBattlePresentation::FormatSlotLabel(EGameXXKCardTargetSide::Enemy, 1), FString(TEXT("敌 1P")));
	const TArray<FGameXXKBattlePresentationSlot> PresentationSlots = FGameXXKBattlePresentation::BuildSlots(PresentationRuntime);
	TestEqual(TEXT("the shared presentation builds one fixed display slot per valid unit"), PresentationSlots.Num(), 6);
	TestTrue(TEXT("the shared slot list retains authoritative unit IDs"), PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
	{
		return Slot.UnitId == TEXT("Npc.YueBai") && Slot.Side == EGameXXKCardTargetSide::Party && Slot.SlotNumber == 3;
	}));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("intent fixture initializes the card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 200, 30, 15, 8, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.One"), TEXT("Enemy One"), 60, 0, 9, 2, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Two"), TEXT("Enemy Two"), 60, 0, 9, 2, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Three"), TEXT("Enemy Three"), 60, 0, 9, 2, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 314;
	TestTrue(TEXT("intent fixture starts a card battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260717, &Error));
	TestEqual(TEXT("a player phase begins with every living enemy intent forecast"),
		State.CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("a player phase stores one forecast intent for each living enemy"), State.CardRun.EnemyIntents.Num(), 3);
	if (State.CardRun.EnemyIntents.Num() != 3)
	{
		return false;
	}

	TestEqual(TEXT("the initial forecast uses the outer enemy P slot"), State.CardRun.EnemyIntents[0].SourceSlotNumber, 1);
	TestEqual(TEXT("the initial forecast uses the middle enemy P slot"), State.CardRun.EnemyIntents[1].SourceSlotNumber, 2);
	TestEqual(TEXT("the initial forecast uses the inner enemy P slot"), State.CardRun.EnemyIntents[2].SourceSlotNumber, 3);
	TestEqual(TEXT("the initial forecast target stays at the central hero P slot"), State.CardRun.EnemyIntents[0].TargetSlotNumber, 2);
	TestEqual(TEXT("the initial forecast card ID is stable from its source"),
		State.CardRun.EnemyIntents[0].CardId,
		FName(TEXT("Monster.Intent.Enemy.One")));
	TestEqual(TEXT("the initial forecast uses its fallback card display name"),
		State.CardRun.EnemyIntents[0].CardDisplayName,
		FString(TEXT("攻击")));

	FGameXXKCardStatusStack IntentOnHitStatus;
	IntentOnHitStatus.Status = EGameXXKCardStatus::Poison;
	IntentOnHitStatus.Stacks = 1;
	State.CardRun.EnemyIntents[0].OnHitStatuses = {IntentOnHitStatus};
	const FName ForecastCardId = State.CardRun.EnemyIntents[0].CardId;
	const FName ForecastSourceUnitId = State.CardRun.EnemyIntents[0].SourceUnitId;
	const FName ForecastSuggestedTargetUnitId = State.CardRun.EnemyIntents[0].SuggestedTargetUnitId;
	const EGameXXKCardStatus ForecastStatus = IntentOnHitStatus.Status;
	const int32 ForecastStatusStacks = IntentOnHitStatus.Stacks;

	TArray<FGameXXKCardDamageResult> PlayerEndResults;
	TestTrue(TEXT("ending the player phase preserves the already visible enemy intents"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PlayerEndResults, &Error));
	TestEqual(TEXT("the preserved forecast enters the enemy phase"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestEqual(TEXT("end player phase keeps one intent for each living enemy"), State.CardRun.EnemyIntents.Num(), 3);
	TestEqual(TEXT("end player phase begins at the first already-forecast intent"), State.CardRun.NextEnemyIntentIndex, 0);
	if (State.CardRun.EnemyIntents.Num() == 3)
	{
		TestEqual(TEXT("end player phase keeps the forecast card identity"), State.CardRun.EnemyIntents[0].CardId, ForecastCardId);
		TestEqual(TEXT("end player phase keeps the forecast source"), State.CardRun.EnemyIntents[0].SourceUnitId, ForecastSourceUnitId);
		TestEqual(TEXT("end player phase keeps the forecast target"),
			State.CardRun.EnemyIntents[0].SuggestedTargetUnitId,
			ForecastSuggestedTargetUnitId);
		TestTrue(TEXT("end player phase preserves the forecast on-hit poison"),
			State.CardRun.EnemyIntents[0].OnHitStatuses.ContainsByPredicate([ForecastStatus, ForecastStatusStacks](const FGameXXKCardStatusStack& Status)
			{
				return Status.Status == ForecastStatus && Status.Stacks == ForecastStatusStacks;
			}));
	}

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> Results;
	bool bIntentsFinished = false;
	TestTrue(TEXT("one adapter call resolves exactly one enemy intent"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
	TestEqual(TEXT("only first intent was consumed"), State.CardRun.NextEnemyIntentIndex, 1);
	TestEqual(TEXT("one resolved intent remains in the enemy phase"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestFalse(TEXT("the first of three intents does not complete the phase"), bIntentsFinished);
	TestEqual(TEXT("the resolved intent returns the first stable source"), ResolvedIntent.SourceUnitId, FName(TEXT("Enemy.One")));
	TestEqual(TEXT("the authority-projected hero health remains unchanged when the consumed armor absorbs the full post-defense hit"),
		State.ActiveBattleParty[0].HP,
		100);
	TestEqual(TEXT("legacy hero shield refreshes after card armor is consumed by the first intent"),
		State.ActiveBattleParty[0].Shield,
		0);
	const FGameXXKCardCombatUnit* HeroAfterFirstIntent = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestTrue(TEXT("one resolved intent applies its saved on-hit statuses through the damage context"),
		HeroAfterFirstIntent && HeroAfterFirstIntent->Statuses.ContainsByPredicate([](const FGameXXKCardStatusStack& Status)
		{
			return Status.Status == EGameXXKCardStatus::Poison && Status.Stacks == 1;
		}));

	TestFalse(TEXT("the phase cannot be completed while unresolved intents remain"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, Results, &Error));
	TestEqual(TEXT("an early completion attempt leaves the runtime in enemy phase"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestTrue(TEXT("the second intent resolves separately"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
	TestTrue(TEXT("the third intent resolves separately"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
	TestTrue(TEXT("all intents report finished only after the last individual resolution"), bIntentsFinished);
	TestEqual(TEXT("all three intents have been consumed"), State.CardRun.NextEnemyIntentIndex, 3);
	TestEqual(TEXT("the runtime stays in enemy phase until completion"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestTrue(TEXT("completion alone begins the next player phase"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, Results, &Error));
	TestEqual(TEXT("completion returns to player phase only after all intents"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("completion immediately prepares the next player-phase forecast"), State.CardRun.EnemyIntents.Num(), 3);
	TestEqual(TEXT("completion resets the consumed-intent index"), State.CardRun.NextEnemyIntentIndex, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleFourEnemySafetyTest,
	"GameXXK.Integration.CardBattle.Adapter.FourEnemySafety",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleFourEnemySafetyTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime PresentationRuntime;
	PresentationRuntime.Units = {
		MakeCardCombatUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0),
		MakeCardCombatUnit(TEXT("Enemy.One"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0),
		MakeCardCombatUnit(TEXT("Enemy.Two"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1),
		MakeCardCombatUnit(TEXT("Enemy.Three"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 2),
		MakeCardCombatUnit(TEXT("Enemy.Four"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 3)};
	TestEqual(TEXT("a fourth enemy cannot alias the fixed third enemy presentation slot"),
		FGameXXKBattlePresentation::GetSlotNumber(PresentationRuntime, TEXT("Enemy.Four")), INDEX_NONE);
	const TArray<FGameXXKBattlePresentationSlot> PresentationSlots = FGameXXKBattlePresentation::BuildSlots(PresentationRuntime);
	TestEqual(TEXT("the fixed presentation omits the unsupported fourth enemy"),
		PresentationSlots.FilterByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
		{
			return Slot.Side == EGameXXKCardTargetSide::Enemy;
		}).Num(), 3);
	TestEqual(TEXT("the fixed presentation retains only one enemy 3P slot"),
		PresentationSlots.FilterByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
		{
			return Slot.Side == EGameXXKCardTargetSide::Enemy && Slot.SlotNumber == 3;
		}).Num(), 1);
	TestFalse(TEXT("the unsupported fourth enemy is absent from display slots"),
		PresentationSlots.ContainsByPredicate([](const FGameXXKBattlePresentationSlot& Slot)
		{
			return Slot.UnitId == TEXT("Enemy.Four");
		}));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the four-enemy safety fixture initializes the card run"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 0, 15, 0, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.One"), TEXT("Enemy One"), 60, 0, 9, 0, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Two"), TEXT("Enemy Two"), 60, 0, 9, 0, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Three"), TEXT("Enemy Three"), 60, 0, 9, 0, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Four"), TEXT("Enemy Four"), 60, 0, 9, 0, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 315;
	const auto CountUnitStatusStacks = [](const TArray<FGameXXKCardCombatUnit>& Units)
	{
		int32 Total = 0;
		for (const FGameXXKCardCombatUnit& Unit : Units)
		{
			Total += Unit.Statuses.Num();
		}
		return Total;
	};
	const bool bLoadoutLockedForRouteBeforeRejectedBegin = State.CardRun.bLoadoutLockedForRoute;
	const bool bHadActiveCardBattleBeforeRejectedBegin = State.CardRun.bHasActiveCardBattle;
	const int32 ActiveBattleSourceNodeIdBeforeRejectedBegin = State.CardRun.ActiveBattleSourceNodeId;
	const int32 ActiveBattleUnitCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Units.Num();
	const EGameXXKCardBattlePhase ActiveBattlePhaseBeforeRejectedBegin = State.CardRun.ActiveBattle.Phase;
	const EGameXXKCardTerrain ActiveBattleTerrainBeforeRejectedBegin = State.CardRun.ActiveBattle.Terrain;
	const int32 ActiveBattleRoundNumberBeforeRejectedBegin = State.CardRun.ActiveBattle.RoundNumber;
	const int32 DrawPileCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.DrawPile.Num();
	const int32 HandCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.Hand.Num();
	const int32 DiscardPileCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.DiscardPile.Num();
	const TArray<FName> ActiveInstanceIdsBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.ActiveInstanceIds;
	const int32 InitialRandomSeedBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.InitialRandomSeed;
	const int32 CurrentRandomStateBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.CurrentRandomState;
	const int32 SharedEnergyBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.SharedEnergy;
	const int32 HandLimitBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.HandLimit;
	const EGameXXKCardPendingChoiceKind PendingChoiceKindBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.Kind;
	const int32 PendingChoiceCandidateCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.Candidates.Num();
	const int32 PendingChoiceRequiredCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredCount;
	const int32 PendingChoiceRequiredDiscardCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredDiscardCount;
	const int32 PendingChoiceRequiredHandPickCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredHandPickCount;
	const TArray<FName> PendingChoiceInsightTopOrderBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.InsightTopOrder;
	const FName PendingChoiceInsightPickedInstanceIdBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.InsightPickedInstanceId;
	const TArray<FName> PendingChoiceInsightReorderedInstanceIdsBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.InsightReorderedInstanceIds;
	const bool bPendingChoiceCanCancelBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.bCanCancel;
	const bool bPendingChoiceCancelPreservesDrawTopBeforeRejectedBegin = State.CardRun.ActiveBattle.Deck.PendingChoice.bCancelPreservesDrawTop;
	const int32 ActiveBattleStatusStackCountBeforeRejectedBegin = CountUnitStatusStacks(State.CardRun.ActiveBattle.Units);
	const int32 GuardLinkCountBeforeRejectedBegin = State.CardRun.ActiveBattle.GuardLinks.Num();
	const int32 ModifierCountBeforeRejectedBegin = State.CardRun.ActiveBattle.Modifiers.Num();
	const int32 NextModifierOrdinalBeforeRejectedBegin = State.CardRun.ActiveBattle.NextModifierOrdinal;
	const int32 RevealedEnemyIntentCountBeforeRejectedBegin = State.CardRun.ActiveBattle.RevealedEnemyIntentCount;
	const int32 PendingNextRoundEnergyBonusBeforeRejectedBegin = State.CardRun.ActiveBattle.PendingNextRoundEnergyBonus;
	const int32 EnemyIntentCountBeforeRejectedBegin = State.CardRun.EnemyIntents.Num();
	const int32 NextEnemyIntentIndexBeforeRejectedBegin = State.CardRun.NextEnemyIntentIndex;
	const int32 PendingRewardSourceNodeIdBeforeRejectedBegin = State.CardRun.PendingReward.SourceNodeId;
	const int32 PendingRewardChoiceSeedBeforeRejectedBegin = State.CardRun.PendingReward.ChoiceSeed;
	const TArray<FName> PendingRewardCardIdsBeforeRejectedBegin = State.CardRun.PendingReward.CardIds;
	const bool bPendingRewardRequiresReplacementBeforeRejectedBegin = State.CardRun.PendingReward.bRequiresRouteCardReplacement;
	const bool bActiveBattleRewardResolvedBeforeRejectedBegin = State.CardRun.bActiveBattleRewardResolved;
	Error.Reset();
	TestFalse(TEXT("starting a card battle rejects more living enemies than the presentation supports"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260717, &Error));
	TestTrue(TEXT("the rejected four-enemy battle start explains the fixed three-slot limit"),
		Error.Contains(TEXT("at most three living enemies")));
	TestEqual(TEXT("the rejected four-enemy battle start preserves the route loadout lock"),
		State.CardRun.bLoadoutLockedForRoute,
		bLoadoutLockedForRouteBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the active-battle flag"),
		State.CardRun.bHasActiveCardBattle,
		bHadActiveCardBattleBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its active source node"),
		State.CardRun.ActiveBattleSourceNodeId,
		ActiveBattleSourceNodeIdBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start leaves no partially built combat units"),
		State.CardRun.ActiveBattle.Units.Num(),
		ActiveBattleUnitCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its prior combat phase"),
		State.CardRun.ActiveBattle.Phase,
		ActiveBattlePhaseBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its terrain"),
		State.CardRun.ActiveBattle.Terrain,
		ActiveBattleTerrainBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its round number"),
		State.CardRun.ActiveBattle.RoundNumber,
		ActiveBattleRoundNumberBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start leaves the draw pile unchanged"),
		State.CardRun.ActiveBattle.Deck.DrawPile.Num(),
		DrawPileCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start leaves the hand unchanged"),
		State.CardRun.ActiveBattle.Deck.Hand.Num(),
		HandCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start leaves the discard pile unchanged"),
		State.CardRun.ActiveBattle.Deck.DiscardPile.Num(),
		DiscardPileCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves active card-instance IDs"),
		State.CardRun.ActiveBattle.Deck.ActiveInstanceIds,
		ActiveInstanceIdsBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its deck seed"),
		State.CardRun.ActiveBattle.Deck.InitialRandomSeed,
		InitialRandomSeedBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves its deck random state"),
		State.CardRun.ActiveBattle.Deck.CurrentRandomState,
		CurrentRandomStateBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves shared energy"),
		State.CardRun.ActiveBattle.Deck.SharedEnergy,
		SharedEnergyBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the hand limit"),
		State.CardRun.ActiveBattle.Deck.HandLimit,
		HandLimitBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the pending-choice kind"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind,
		PendingChoiceKindBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice candidates"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Candidates.Num(),
		PendingChoiceCandidateCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice selection count"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredCount,
		PendingChoiceRequiredCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice discard count"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredDiscardCount,
		PendingChoiceRequiredDiscardCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice hand-pick count"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.RequiredHandPickCount,
		PendingChoiceRequiredHandPickCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice insight order"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.InsightTopOrder,
		PendingChoiceInsightTopOrderBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice insight pick"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.InsightPickedInstanceId,
		PendingChoiceInsightPickedInstanceIdBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice reordered insight IDs"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.InsightReorderedInstanceIds,
		PendingChoiceInsightReorderedInstanceIdsBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice cancellation state"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.bCanCancel,
		bPendingChoiceCanCancelBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending-choice cancel draw policy"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.bCancelPreservesDrawTop,
		bPendingChoiceCancelPreservesDrawTopBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves unit status stacks"),
		CountUnitStatusStacks(State.CardRun.ActiveBattle.Units),
		ActiveBattleStatusStackCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves guard links"),
		State.CardRun.ActiveBattle.GuardLinks.Num(),
		GuardLinkCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves modifiers"),
		State.CardRun.ActiveBattle.Modifiers.Num(),
		ModifierCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the modifier source ordinal"),
		State.CardRun.ActiveBattle.NextModifierOrdinal,
		NextModifierOrdinalBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves revealed enemy-intent state"),
		State.CardRun.ActiveBattle.RevealedEnemyIntentCount,
		RevealedEnemyIntentCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves next-round energy state"),
		State.CardRun.ActiveBattle.PendingNextRoundEnergyBonus,
		PendingNextRoundEnergyBonusBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start leaves no hidden intents"),
		State.CardRun.EnemyIntents.Num(),
		EnemyIntentCountBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the intent cursor"),
		State.CardRun.NextEnemyIntentIndex,
		NextEnemyIntentIndexBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the pending reward source"),
		State.CardRun.PendingReward.SourceNodeId,
		PendingRewardSourceNodeIdBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves the pending reward seed"),
		State.CardRun.PendingReward.ChoiceSeed,
		PendingRewardChoiceSeedBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending reward cards"),
		State.CardRun.PendingReward.CardIds,
		PendingRewardCardIdsBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves pending reward replacement state"),
		State.CardRun.PendingReward.bRequiresRouteCardReplacement,
		bPendingRewardRequiresReplacementBeforeRejectedBegin);
	TestEqual(TEXT("the rejected four-enemy battle start preserves reward-resolution state"),
		State.CardRun.bActiveBattleRewardResolved,
		bActiveBattleRewardResolvedBeforeRejectedBegin);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleInitialLivingEnemyForecastTest,
	"GameXXK.Integration.CardBattle.Adapter.InitialLivingEnemyForecast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleInitialLivingEnemyForecastTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the living-enemy forecast fixture initializes the card run"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 0, 15, 0, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.Dead"), TEXT("Dead Enemy"), 0, 0, 9, 0, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Living.One"), TEXT("Living Enemy One"), 60, 0, 9, 0, true),
		MakeLegacyBattleUnit(TEXT("Enemy.Living.Two"), TEXT("Living Enemy Two"), 60, 0, 9, 0, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 316;
	TestTrue(TEXT("the living-enemy forecast fixture begins its card battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260719, &Error));
	TestEqual(TEXT("the initial living-enemy forecast remains in the player phase"),
		State.CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("the initial forecast creates intents only for living enemies"), State.CardRun.EnemyIntents.Num(), 2);
	TestFalse(TEXT("the initial forecast never includes the zero-health enemy"),
		State.CardRun.EnemyIntents.ContainsByPredicate([](const FGameXXKCardEnemyIntent& Intent)
		{
			return Intent.SourceUnitId == TEXT("Enemy.Dead");
		}));
	if (State.CardRun.EnemyIntents.Num() != 2)
	{
		return false;
	}

	TestEqual(TEXT("the initial forecast retains the first living enemy source"),
		State.CardRun.EnemyIntents[0].SourceUnitId,
		FName(TEXT("Enemy.Living.One")));
	TestEqual(TEXT("the initial forecast retains the second living enemy source"),
		State.CardRun.EnemyIntents[1].SourceUnitId,
		FName(TEXT("Enemy.Living.Two")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleEnemyIntentRetryTest,
	"GameXXK.Integration.CardBattle.Adapter.EnemyIntentRetry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleEnemyIntentRetryTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the retry fixture initializes the card run"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 100, 0, 15, 0, false)};
	State.ActiveBattleEnemies = {
		MakeLegacyBattleUnit(TEXT("Enemy.One"), TEXT("Enemy One"), 60, 0, 9, 0, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 316;
	TestTrue(TEXT("the retry fixture starts a card battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260717, &Error));
	TArray<FGameXXKCardDamageResult> PlayerEndResults;
	TestTrue(TEXT("the retry fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PlayerEndResults, &Error));
	TestEqual(TEXT("the retry fixture creates one enemy intent"), State.CardRun.EnemyIntents.Num(), 1);
	if (State.CardRun.EnemyIntents.Num() != 1)
	{
		return false;
	}

	FGameXXKCardStatusStack InvalidOnHitStatus;
	InvalidOnHitStatus.Status = EGameXXKCardStatus::Invalid;
	InvalidOnHitStatus.Stacks = 0;
	State.CardRun.EnemyIntents[0].OnHitStatuses = {InvalidOnHitStatus};
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> Results;
	bool bIntentsFinished = false;
	TestFalse(TEXT("an invalid persisted on-hit status makes the direct intent resolution fail"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
	TestTrue(TEXT("the failed direct intent reports the persisted status validation error"),
		Error.Contains(TEXT("On-hit statuses")));
	TestEqual(TEXT("a failed direct intent remains at the original persisted index"), State.CardRun.NextEnemyIntentIndex, 0);
	TestFalse(TEXT("a failed direct intent cannot report the intent list complete"), bIntentsFinished);

	State.CardRun.EnemyIntents[0].OnHitStatuses.Reset();
	TestTrue(TEXT("correcting the persisted intent lets the same intent resolve once"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, Results, bIntentsFinished, &Error));
	TestEqual(TEXT("the corrected direct intent advances exactly once"), State.CardRun.NextEnemyIntentIndex, 1);
	TestTrue(TEXT("the corrected only intent completes the saved intent list"), bIntentsFinished);
	TestEqual(TEXT("the retried intent returns its original stable source"), ResolvedIntent.SourceUnitId, FName(TEXT("Enemy.One")));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleTerminalEnemyCompletionTest,
	"GameXXK.Integration.CardBattle.Adapter.TerminalEnemyCompletion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleTerminalEnemyCompletionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	// Card-battle startup now derives the hero from the save-authoritative character state and
	// equipped loadout, rather than treating the legacy battle-widget projection as input.
	// Preserve a one-health route snapshot through that authority path.
	State.PlayerHP = 1;
	State.PlayerMP = 0;
	TestTrue(TEXT("the terminal completion fixture initializes the card run"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {
		MakeLegacyBattleUnit(TEXT("Player"), TEXT("Hero"), 1, 0, 15, 0, false)};
	State.ActiveBattleEnemies = {
		// The save-authoritative level-one hero has 8 defense and the persisted projection
		// starts with one armor, so ten attack is the smallest fixture value that can end it.
		MakeLegacyBattleUnit(TEXT("Enemy.One"), TEXT("Enemy One"), 60, 0, 10, 0, true)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 317;
	TestTrue(TEXT("the terminal completion fixture starts a card battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 20260717, &Error));
	TArray<FGameXXKCardDamageResult> PlayerEndResults;
	TestTrue(TEXT("the terminal completion fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PlayerEndResults, &Error));
	const int32 RoundBeforeTerminalIntent = State.CardRun.ActiveBattle.RoundNumber;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the first and final enemy intent resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the final enemy intent reports the saved list complete"), bIntentsFinished);
	TestEqual(TEXT("the final enemy intent defeats the last living party unit"),
		State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Defeat);

	TArray<FGameXXKCardDamageResult> CompletionResults;
	TestTrue(TEXT("terminal enemy completion clears the resolved intent state"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, CompletionResults, &Error));
	TestEqual(TEXT("terminal completion clears all saved intents"), State.CardRun.EnemyIntents.Num(), 0);
	TestEqual(TEXT("terminal completion resets the saved intent index"), State.CardRun.NextEnemyIntentIndex, 0);
	TestEqual(TEXT("terminal completion leaves defeat terminal instead of starting another player round"),
		State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Defeat);
	TestEqual(TEXT("terminal completion does not advance the card battle round"),
		State.CardRun.ActiveBattle.RoundNumber, RoundBeforeTerminalIntent);

	return true;
}

#endif
