#include "Misc/AutomationTest.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEnemyCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeEnemyIntentFixtureUnit(
		const FName Id,
		const FName DefinitionId,
		const int32 SlotNumber,
		const int32 Health,
		const int32 Attack)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = Id;
		Unit.DisplayName = FText::FromName(DefinitionId);
		Unit.HP = Health;
		Unit.MaxHP = Health;
		Unit.Attack = Attack;
		Unit.Defense = 2;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		Unit.EnemyDefinitionId = DefinitionId;
		Unit.BattleSlotNumber = SlotNumber;
		Unit.CombatLevel = 4;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit MakeEnemyIntentFixtureHero()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("Player");
		Unit.DisplayName = FText::FromString(TEXT("主角"));
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.MP = 30;
		Unit.MaxMP = 30;
		Unit.Attack = 16;
		Unit.Defense = 6;
		Unit.Speed = 10;
		return Unit;
	}

	FGameXXKCardInstance MakeEnemyIntentFixtureCard(
		const FName InstanceId,
		const FName CardId,
		const int32 AcquisitionOrdinal)
	{
		FGameXXKCardInstance Instance;
		Instance.InstanceId = InstanceId;
		Instance.CardId = CardId;
		Instance.OwnerUnitId = TEXT("Player");
		Instance.SourceEntryId = FName(*FString::Printf(TEXT("Fixture.Source.%d"), AcquisitionOrdinal));
		Instance.AcquisitionOrdinal = AcquisitionOrdinal;
		return Instance;
	}

	bool InitializeBluehornIntentFixture(
		FGameXXKRuntimeState& OutState,
		FString& OutError,
		const int32 NodeId)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}
		OutState.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
		OutState.ActiveBattleEnemies = {
			MakeEnemyIntentFixtureUnit(TEXT("Enemy.Bluehorn.P2"), TEXT("Enemy.Ch1.BluehornGoatKing"), 2, 138, 17)};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = NodeId;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			NodeId,
			&OutError);
	}

	bool InitializeRedtuskIntentFixture(
		FGameXXKRuntimeState& OutState,
		FString& OutError,
		const int32 NodeId)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}
		OutState.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
		OutState.ActiveBattleEnemies = {
			MakeEnemyIntentFixtureUnit(TEXT("Enemy.Redtusk.P2"), TEXT("Enemy.Ch2.RedtuskBoarKing"), 2, 188, 17)};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = NodeId;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			NodeId,
			&OutError);
	}

	TArray<uint8> SerializeEnemyIntentStateForTest(const FGameXXKRuntimeState& State)
	{
		FGameXXKRuntimeState Copy = State;
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool DeserializeEnemyIntentStateForTest(const TArray<uint8>& Bytes, FGameXXKRuntimeState& OutState)
	{
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &OutState, nullptr);
		return !Archive.IsError();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentCatalogForecastTest,
	"GameXXK.Battle.EnemyIntentRules.ForecastUsesCatalogDefinition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentCatalogForecastTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the catalog intent fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8),
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P3"), TEXT("Enemy.Ch1.Goat"), 3, 58, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 901;
	TestTrue(TEXT("the catalog intent fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 901, &Error));

	const FGameXXKEnemyDefinition* Rooster = FGameXXKEnemyCatalog::Find(TEXT("Enemy.Ch1.Rooster"));
	TestNotNull(TEXT("the rooster definition exists in the authoritative catalog"), Rooster);
	const FGameXXKCardEnemyIntent* RoosterIntent = State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Candidate)
	{
		return Candidate.SourceUnitId == TEXT("Enemy.Rooster.P1");
	});
	TestNotNull(TEXT("the live forecast retains the rooster as its source"), RoosterIntent);
	if (!Rooster || !RoosterIntent || Rooster->Intents.IsEmpty())
	{
		return false;
	}

	const FGameXXKEnemyIntentDefinition& ExpectedIntent = Rooster->Intents[0];
	TestEqual(TEXT("the forecast stores the catalog skill identity rather than a generic attack id"),
		RoosterIntent->IntentDefinitionId, ExpectedIntent.Id);
	TestEqual(TEXT("the forecast card face names the catalog skill"),
		RoosterIntent->CardDisplayName, ExpectedIntent.DisplayName.ToString());
	TestEqual(TEXT("the forecast preserves every catalog effect packet"),
		RoosterIntent->Effects.Num(), ExpectedIntent.Effects.Num());
	TestTrue(TEXT("the forecast has a concrete locked target for its first effect"),
		RoosterIntent->Effects.IsValidIndex(0) && RoosterIntent->Effects[0].TargetUnitIds.Contains(TEXT("Player")));
	TestTrue(TEXT("the forecast computes a positive, inspectable effect magnitude"),
		RoosterIntent->Effects.IsValidIndex(0) && RoosterIntent->Effects[0].Magnitude > 0);
	TestFalse(TEXT("the forecast supplies visible tooltip detail from the catalog action"), RoosterIntent->TooltipLines.IsEmpty());
	const FGameXXKEnemyBattleState* RoosterState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Rooster.P1"));
	TestNotNull(TEXT("the battle initializes serializable enemy state for every catalog source"), RoosterState);
	if (RoosterState)
	{
		TestEqual(TEXT("the serializable enemy state retains its catalog definition id"), RoosterState->DefinitionId, Rooster->Id);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentCursorAdvanceTest,
	"GameXXK.Battle.EnemyIntentRules.ResolvedCatalogIntentAdvancesCursor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentCursorAdvanceTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the cursor fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8),
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P3"), TEXT("Enemy.Ch1.Goat"), 3, 58, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 902;
	TestTrue(TEXT("the cursor fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 902, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	TestTrue(TEXT("the cursor fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the rooster's first catalog action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the first resolved catalog action is rooster peck"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Peck")));
	TestTrue(FString::Printf(TEXT("the goat's first catalog action resolves: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("both saved actions finish before the next forecast"), bIntentsFinished);
	TestTrue(FString::Printf(TEXT("the completed enemy phase starts the next player phase: %s"), *Error),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));

	const FGameXXKEnemyDefinition* Rooster = FGameXXKEnemyCatalog::Find(TEXT("Enemy.Ch1.Rooster"));
	const FGameXXKCardEnemyIntent* NextRoosterIntent = State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Candidate)
	{
		return Candidate.SourceUnitId == TEXT("Enemy.Rooster.P1");
	});
	const FGameXXKEnemyBattleState* RoosterState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Rooster.P1"));
	TestNotNull(TEXT("the next player forecast keeps the rooster intent visible"), NextRoosterIntent);
	TestNotNull(TEXT("the rooster keeps its persisted running state after resolving"), RoosterState);
	if (!Rooster || Rooster->Intents.Num() < 2 || !NextRoosterIntent || !RoosterState)
	{
		return false;
	}
	TestEqual(TEXT("the next rooster intent advances to double peck rather than repeating peck"),
		NextRoosterIntent->IntentDefinitionId, Rooster->Intents[1].Id);
	TestEqual(TEXT("the serialized cursor points at the next chosen catalog action"), RoosterState->IntentCursor, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentMultiHitResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogMultiHitResolvesEveryHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentMultiHitResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the multi-hit fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 903;
	TestTrue(TEXT("the multi-hit fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 903, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the first rooster round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the opening peck resolves before advancing the rooster cursor"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the opening one-enemy phase can complete"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the second rooster round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));

	TestTrue(TEXT("the catalog double peck resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is double peck"), ResolvedIntent.IntentDefinitionId, FName(TEXT("DoublePeck")));
	TestEqual(TEXT("the two-hit catalog action creates one independently audited result per hit"), IntentResults.Num(), 2);
	TestTrue(TEXT("each multi-hit result keeps the stable rooster source"), IntentResults.Num() == 2
		&& IntentResults[0].SourceUnitId == TEXT("Enemy.Rooster.P1")
		&& IntentResults[1].SourceUnitId == TEXT("Enemy.Rooster.P1"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentAttackModifierResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogModifierAppliesToLockedTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentAttackModifierResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the modifier fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 904;
	TestTrue(TEXT("the modifier fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 904, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 CompletedIntentRounds = 0; CompletedIntentRounds < 2; ++CompletedIntentRounds)
	{
		TestTrue(FString::Printf(TEXT("catalog setup round %d enters enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("catalog setup round %d resolves its current rooster intent: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("catalog setup round %d completes the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	TestTrue(TEXT("the third rooster round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardCombatUnit* RoosterBefore = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Rooster.P1");
	});
	TestNotNull(TEXT("the locked rooster source exists before its modifier intent"), RoosterBefore);
	const int32 AttackBefore = RoosterBefore ? RoosterBefore->Attack : 0;
	TestTrue(TEXT("the third catalog intent is the non-damage battle cry"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is battle cry"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Crow")));
	const FGameXXKCardCombatUnit* RoosterAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Rooster.P1");
	});
	TestNotNull(TEXT("the rooster remains available after its modifier intent"), RoosterAfter);
	TestEqual(TEXT("the catalog modifier applies its declared two attack to the locked enemy target"),
		RoosterAfter ? RoosterAfter->Attack : 0, AttackBefore + 2);
	TestTrue(TEXT("the modifier round completes before the next player forecast"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardCombatUnit* RoosterAfterExpiry = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Rooster.P1");
	});
	TestNotNull(TEXT("the rooster remains available after the temporary modifier expires"), RoosterAfterExpiry);
	TestEqual(TEXT("the temporary catalog attack modifier expires at the end of its enemy phase"),
		RoosterAfterExpiry ? RoosterAfterExpiry->Attack : 0, AttackBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentArmorResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogArmorAppliesToLockedTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentArmorResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the armor fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P1"), TEXT("Enemy.Ch1.Goat"), 1, 58, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 905;
	TestTrue(TEXT("the armor fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 905, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the goat horn round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the goat horn resolves before the armor action"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the goat horn round completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the goat armor round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));

	const FGameXXKCardCombatUnit* GoatBefore = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Goat.P1");
	});
	TestNotNull(TEXT("the locked goat source exists before stomp"), GoatBefore);
	const int32 ArmorBefore = GoatBefore ? GoatBefore->Armor : 0;
	TestTrue(TEXT("the goat stomp resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is stomp"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Stomp")));
	const FGameXXKCardCombatUnit* GoatAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Goat.P1");
	});
	TestNotNull(TEXT("the goat remains available after stomp"), GoatAfter);
	TestEqual(TEXT("the catalog armor applies its declared ten armor to the locked goat target"),
		GoatAfter ? GoatAfter->Armor : 0, ArmorBefore + 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBluehornGuardHerdRetentionIntegrationTest,
	"GameXXK.Battle.EnemyIntentRules.BluehornGuardHerdArmorRetentionBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBluehornGuardHerdRetentionIntegrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Bluehorn GuardHerd fixture begins a catalog battle"), InitializeBluehornIntentFixture(State, Error, 913)))
	{
		return false;
	}

	const TArray<FName> ExpectedIntentIds = {
		TEXT("Pierce"),
		TEXT("HerdStomp"),
		TEXT("GuardHerd")};
	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 IntentIndex = 0; IntentIndex < ExpectedIntentIds.Num(); ++IntentIndex)
	{
		TestTrue(FString::Printf(TEXT("Bluehorn setup round %d enters the enemy phase"), IntentIndex + 1),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("Bluehorn setup round %d resolves its catalog intent"), IntentIndex + 1),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestEqual(FString::Printf(TEXT("Bluehorn setup round %d follows the real catalog order"), IntentIndex + 1),
			ResolvedIntent.IntentDefinitionId,
			ExpectedIntentIds[IntentIndex]);
		TestTrue(FString::Printf(TEXT("Bluehorn setup round %d completes its enemy phase"), IntentIndex + 1),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	FGameXXKCardCombatUnit* Bluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	if (!TestNotNull(TEXT("the Bluehorn remains available after its real GuardHerd intent"), Bluehorn))
	{
		return false;
	}
	TestEqual(TEXT("GuardHerd grants its full eight armor during the following player phase"), Bluehorn->Armor, 8);
	TestEqual(TEXT("GuardHerd resolves into a player phase before the armor-retention boundary"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Player);
	TestTrue(TEXT("the next player end reaches Bluehorn's existing enemy-phase-start armor boundary"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardCombatUnit* BluehornAfterRetention = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	TestEqual(TEXT("the real GuardHerd armor is retained as floor eight-halves at the next enemy phase"), BluehornAfterRetention ? BluehornAfterRetention->Armor : -1, 4);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBluehornEnemyPhaseAtomicityTest,
	"GameXXK.Battle.EnemyIntentRules.BluehornCompletionFailureIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBluehornEnemyPhaseAtomicityTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Bluehorn completion-atomicity fixture begins a catalog battle"), InitializeBluehornIntentFixture(State, Error, 914)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the atomicity fixture enters the Bluehorn enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the atomicity fixture resolves Bluehorn's current intent before completion"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the atomicity fixture consumes every current enemy intent"), bIntentsFinished);

	FGameXXKCardCombatUnit* Bluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	FGameXXKEnemyBattleState* BluehornState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Bluehorn.P2"));
	if (!TestNotNull(TEXT("the atomicity fixture exposes the live Bluehorn"), Bluehorn)
		|| !TestNotNull(TEXT("the atomicity fixture exposes Bluehorn's persisted state"), BluehornState))
	{
		return false;
	}
	Bluehorn->Attack += 3;
	BluehornState->TemporaryAttackModifier = 3;
	BluehornState->PendingChargedIntentId = TEXT("AtomicityProbeCharge");
	BluehornState->ChargeRoundsRemaining = 2;
	BluehornState->DefinitionId = TEXT("Enemy.Ch1.Goat");

	const TArray<uint8> BeforeRejectedCompletion = SerializeEnemyIntentStateForTest(State);
	TArray<FGameXXKCardDamageResult> PreservedResults;
	FGameXXKCardDamageResult& PreservedResult = PreservedResults.AddDefaulted_GetRef();
	PreservedResult.SourceUnitId = TEXT("Completion.OutputMustRemainUnchanged");
	PreservedResult.OriginalTargetUnitId = TEXT("Completion.OutputTargetMustRemainUnchanged");
	PreservedResult.RequestedDamage = 91;
	Error.Reset();
	TestFalse(TEXT("a mismatched Bluehorn persisted definition rejects phase completion during next-intent rebuild"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PreservedResults, &Error));
	TestTrue(TEXT("the rejected completion reports persisted enemy-state mismatch"), Error.Contains(TEXT("Enemy catalog state does not match a valid intent definition.")));
	TestEqual(TEXT("a rejected completion preserves the whole runtime byte-for-byte"),
		SerializeEnemyIntentStateForTest(State),
		BeforeRejectedCompletion);
	if (!TestEqual(TEXT("a rejected completion preserves output count"), PreservedResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("a rejected completion preserves output source"), PreservedResults[0].SourceUnitId, FName(TEXT("Completion.OutputMustRemainUnchanged")));
	TestEqual(TEXT("a rejected completion preserves output target"), PreservedResults[0].OriginalTargetUnitId, FName(TEXT("Completion.OutputTargetMustRemainUnchanged")));
	TestEqual(TEXT("a rejected completion preserves output payload"), PreservedResults[0].RequestedDamage, 91);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBluehornDotDefeatDoesNotForecastTest,
	"GameXXK.Battle.EnemyIntentRules.BluehornDotDefeatDoesNotForecast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBluehornDotDefeatDoesNotForecastTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Bluehorn DOT-defeat fixture begins a catalog battle"), InitializeBluehornIntentFixture(State, Error, 915)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	TestTrue(TEXT("the DOT-defeat fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	FGameXXKCardCombatUnit* Bluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	if (!TestNotNull(TEXT("the DOT-defeat fixture exposes the living Bluehorn"), Bluehorn))
	{
		return false;
	}
	Bluehorn->HP = 2;
	Bluehorn->MaxHP = 138;
	Bluehorn->Armor = 9;
	TestEqual(TEXT("the DOT-defeat fixture applies one poison stack"),
		GameXXKCardRules::AddCombatStatus(*Bluehorn, EGameXXKCardStatus::Poison, 1), 1);
	TestTrue(TEXT("the DOT-defeat fixture skips the only pending intent before end-phase DOT"),
		FGameXXKCardBattleAdapter::SkipCurrentEnemyIntent(State, &Error));
	TestTrue(TEXT("enemy-side DOT can complete the phase into a terminal defeat for Bluehorn"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardCombatUnit* DefeatedBluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	TestNotNull(TEXT("the DOT-killed Bluehorn remains available for terminal-state audit"), DefeatedBluehorn);
	TestFalse(TEXT("a Bluehorn killed by enemy-side DOT never reaches a later armor-retention boundary"), DefeatedBluehorn && DefeatedBluehorn->bLiving);
	TestEqual(TEXT("the DOT-killed Bluehorn reaches zero health"), DefeatedBluehorn ? DefeatedBluehorn->HP : -1, 0);
	TestEqual(TEXT("the DOT-killed Bluehorn produces no future enemy forecast"), State.CardRun.EnemyIntents.Num(), 0);
	TestEqual(TEXT("the DOT-killed Bluehorn ends the battle instead of entering a player forecast phase"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Victory);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyEndDotTerminalPrecedesPhaseMaintenanceTest,
	"GameXXK.Battle.EnemyIntentRules.EnemyEndDotTerminalPrecedesChargeAndTemporaryExpiry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyEndDotTerminalPrecedesPhaseMaintenanceTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the terminal-order fixture begins a Bluehorn catalog battle"), InitializeBluehornIntentFixture(State, Error, 916)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	TestTrue(TEXT("the terminal-order fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	FGameXXKCardCombatUnit* Bluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	FGameXXKEnemyBattleState* BluehornState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Bluehorn.P2"));
	if (!TestNotNull(TEXT("the terminal-order fixture exposes the live Bluehorn"), Bluehorn)
		|| !TestNotNull(TEXT("the terminal-order fixture exposes Bluehorn's persisted state"), BluehornState))
	{
		return false;
	}

	Bluehorn->HP = 2;
	Bluehorn->MaxHP = 138;
	Bluehorn->Attack += 3;
	BluehornState->TemporaryAttackModifier = 3;
	BluehornState->PendingChargedIntentId = TEXT("RageCharge");
	BluehornState->ChargeRoundsRemaining = 2;
	TestEqual(TEXT("the terminal-order fixture applies one lethal enemy-end poison stack"),
		GameXXKCardRules::AddCombatStatus(*Bluehorn, EGameXXKCardStatus::Poison, 1), 1);
	TestTrue(TEXT("the terminal-order fixture consumes the current forecast before enemy-end DOT"),
		FGameXXKCardBattleAdapter::SkipCurrentEnemyIntent(State, &Error));
	TestTrue(TEXT("enemy-end DOT completes a terminal Bluehorn phase"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));

	const FGameXXKCardCombatUnit* DefeatedBluehorn = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Bluehorn.P2");
	});
	const FGameXXKEnemyBattleState* DefeatedBluehornState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Bluehorn.P2"));
	TestNotNull(TEXT("the terminal Bluehorn remains available for state-order audit"), DefeatedBluehorn);
	TestNotNull(TEXT("the terminal Bluehorn keeps persisted state for state-order audit"), DefeatedBluehornState);
	TestFalse(TEXT("enemy-end poison defeats Bluehorn before phase maintenance"), DefeatedBluehorn && DefeatedBluehorn->bLiving);
	TestEqual(TEXT("a terminal DOT defeat does not decrement an unresolved charge"),
		DefeatedBluehornState ? DefeatedBluehornState->ChargeRoundsRemaining : -1,
		2);
	TestEqual(TEXT("a terminal DOT defeat preserves the pending charge identity"),
		DefeatedBluehornState ? DefeatedBluehornState->PendingChargedIntentId : NAME_None,
		FName(TEXT("RageCharge")));
	TestEqual(TEXT("a terminal DOT defeat does not expire the temporary attack modifier"),
		DefeatedBluehornState ? DefeatedBluehornState->TemporaryAttackModifier : -1,
		3);
	TestEqual(TEXT("a terminal DOT defeat does not subtract the temporary attack from the defeated unit"),
		DefeatedBluehorn ? DefeatedBluehorn->Attack : -1,
		20);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKResolveEnemyPhaseFailureIsOuterAtomicTest,
	"GameXXK.Battle.EnemyIntentRules.ResolveEnemyPhaseCompletionFailureIsOuterAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKResolveEnemyPhaseFailureIsOuterAtomicTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("the outer-atomicity fixture initializes its route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		return false;
	}
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8),
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P2"), TEXT("Enemy.Ch1.Goat"), 2, 58, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 917;
	if (!TestTrue(TEXT("the outer-atomicity fixture begins a two-enemy catalog battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			917,
			&Error)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the outer-atomicity fixture enters its enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestEqual(TEXT("the first saved action belongs to Rooster and can resolve before the induced later failure"),
		State.CardRun.EnemyIntents.IsValidIndex(0) ? State.CardRun.EnemyIntents[0].SourceUnitId : NAME_None,
		FName(TEXT("Enemy.Rooster.P1"))))
	{
		return false;
	}

	// Keep one real current intent so ResolveEnemyPhase performs a genuine first resolution.
	// The intentionally mismatched Goat state remains invisible until completion rebuilds
	// the following player-phase forecast for every living enemy.
	State.CardRun.EnemyIntents.SetNum(1);
	State.CardRun.NextEnemyIntentIndex = 0;
	FGameXXKEnemyBattleState* GoatState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Goat.P2"));
	if (!TestNotNull(TEXT("the outer-atomicity fixture exposes Goat's persisted state"), GoatState))
	{
		return false;
	}
	GoatState->DefinitionId = TEXT("Enemy.Ch1.Rooster");

	const TArray<uint8> BeforeRejectedResolve = SerializeEnemyIntentStateForTest(State);
	TArray<FGameXXKCardDamageResult> PreservedResults;
	FGameXXKCardDamageResult& PreservedResult = PreservedResults.AddDefaulted_GetRef();
	PreservedResult.SourceUnitId = TEXT("ResolvePhase.OutputMustRemainUnchanged");
	PreservedResult.OriginalTargetUnitId = TEXT("ResolvePhase.OutputTargetMustRemainUnchanged");
	PreservedResult.RequestedDamage = 73;
	Error.Reset();
	TestFalse(TEXT("a completion reforecast failure after the first enemy intent rejects the aggregate enemy phase"),
		FGameXXKCardBattleAdapter::ResolveEnemyPhase(State, PreservedResults, &Error));
	TestTrue(TEXT("the rejected aggregate phase reports the later persisted enemy-state mismatch"),
		Error.Contains(TEXT("Enemy catalog state does not match a valid intent definition.")));
	TestEqual(TEXT("a rejected aggregate enemy phase preserves the whole runtime byte-for-byte"),
		SerializeEnemyIntentStateForTest(State),
		BeforeRejectedResolve);
	if (!TestEqual(TEXT("a rejected aggregate enemy phase preserves output count"), PreservedResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("a rejected aggregate enemy phase preserves output source"),
		PreservedResults[0].SourceUnitId,
		FName(TEXT("ResolvePhase.OutputMustRemainUnchanged")));
	TestEqual(TEXT("a rejected aggregate enemy phase preserves output target"),
		PreservedResults[0].OriginalTargetUnitId,
		FName(TEXT("ResolvePhase.OutputTargetMustRemainUnchanged")));
	TestEqual(TEXT("a rejected aggregate enemy phase preserves output payload"),
		PreservedResults[0].RequestedDamage,
		73);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentStatusResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogStatusAppliesToEveryLockedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentStatusResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the status fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	FGameXXKCompanionRecruitResult RecruitedPartner;
	TestTrue(TEXT("the status fixture recruits a save-authoritative permanent partner"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			State.CardRun.CompanionRoster,
			TEXT("Companion.Blade.01"),
			906,
			RecruitedPartner,
			&Error));
	TestEqual(TEXT("the status fixture obtains a permanent partner"), RecruitedPartner.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
	TestTrue(TEXT("the recruited partner becomes the route-authoritative carried partner"),
		FGameXXKCompanionRules::SetActivePermanentCompanion(
			State.CardRun.CompanionRoster,
			RecruitedPartner.Companion.InstanceId,
			&Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Weasel.P1"), TEXT("Enemy.Ch1.Weasel"), 1, 42, 6)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 906;
	TestTrue(TEXT("the status fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 906, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the weasel harass round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the weasel harass resolves before stink fog"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the weasel harass round completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the weasel stink fog round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));

	TestTrue(TEXT("the status-only stink fog resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is stink fog"), ResolvedIntent.IntentDefinitionId, FName(TEXT("StinkFog")));
	const FGameXXKCardCombatUnit* HeroAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	const FName PartnerId = RecruitedPartner.Companion.InstanceId;
	const FGameXXKCardCombatUnit* PartnerAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([PartnerId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == PartnerId;
	});
	TestNotNull(TEXT("the hero remains available for the locked all-party status"), HeroAfter);
	TestNotNull(TEXT("the partner remains available for the locked all-party status"), PartnerAfter);
	TestEqual(TEXT("stink fog applies its declared weak stack to the locked hero"),
		HeroAfter ? GameXXKCardRules::GetCombatStatusStacks(*HeroAfter, EGameXXKCardStatus::Weak) : 0, 1);
	TestEqual(TEXT("stink fog applies its declared weak stack to the locked partner"),
		PartnerAfter ? GameXXKCardRules::GetCombatStatusStacks(*PartnerAfter, EGameXXKCardStatus::Weak) : 0, 1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentHealResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.MoneyRat.BreakWealthConsumesStacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentHealResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the healing fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.MoneyRat.P1"), TEXT("Enemy.Ch1.MoneyRat"), 1, 220, 24)};
	State.ActiveBattleEnemies[0].MaxHP = 240;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 907;
	TestTrue(TEXT("the healing fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 907, &Error));

	FGameXXKCardCombatUnit* MoneyRat = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.MoneyRat.P1");
	});
	TestNotNull(TEXT("the wounded money rat is present in the card runtime"), MoneyRat);
	if (!MoneyRat)
	{
		return false;
	}
	MoneyRat->HP = 120;

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 CompletedIntentRounds = 0; CompletedIntentRounds < 4; ++CompletedIntentRounds)
	{
		TestTrue(FString::Printf(TEXT("healing setup round %d enters the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("healing setup round %d resolves its catalog action: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("healing setup round %d completes the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	FGameXXKCardCombatUnit* MoneyRatBeforeForecast = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.MoneyRat.P1");
	});
	if (!TestNotNull(TEXT("the money rat remains available before its controlled wealth conversion"), MoneyRatBeforeForecast))
	{
		return false;
	}
	GameXXKCardRules::ConsumeCombatStatus(*MoneyRatBeforeForecast, EGameXXKCardStatus::Wealth, MAX_int32);
	TestEqual(TEXT("the controlled money rat fixture starts Break Wealth with exactly three stacks"),
		GameXXKCardRules::AddCombatStatus(*MoneyRatBeforeForecast, EGameXXKCardStatus::Wealth, 3), 3);
	// The saved forecast belongs to the prior player round. Rebuild this controlled intent only
	// after the test has established the source stacks that it is expected to consume.
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;

	TestTrue(TEXT("the money rat healing round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardEnemyIntent* ForecastBreakWealth = State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Intent)
	{
		return Intent.IntentDefinitionId == TEXT("BreakWealth");
	});
	TestNotNull(TEXT("Break Wealth is saved as the controlled enemy forecast"), ForecastBreakWealth);
	const FGameXXKResolvedEnemyIntentEffect* ForecastHeal = ForecastBreakWealth
		? ForecastBreakWealth->Effects.FindByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Type == EGameXXKEnemyIntentEffectType::Heal;
		})
		: nullptr;
	TestNotNull(TEXT("Break Wealth forecast exposes its consumed-stack heal"), ForecastHeal);
	const int32 ExpectedBreakWealthHealing = (MoneyRatBeforeForecast ? MoneyRatBeforeForecast->MaxHP : 0) * 6 / 100 * 3;
	TestEqual(TEXT("Break Wealth forecast rounds each six-percent stack before totaling"),
		ForecastHeal ? ForecastHeal->Magnitude : INDEX_NONE,
		ExpectedBreakWealthHealing);
	const FGameXXKCardCombatUnit* MoneyRatBefore = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.MoneyRat.P1");
	});
	TestNotNull(TEXT("the wounded money rat remains a valid locked heal target"), MoneyRatBefore);
	const int32 HPBefore = MoneyRatBefore ? MoneyRatBefore->HP : 0;
	TestTrue(TEXT("the money rat break wealth healing intent resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is break wealth"), ResolvedIntent.IntentDefinitionId, FName(TEXT("BreakWealth")));
	const FGameXXKCardCombatUnit* MoneyRatAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.MoneyRat.P1");
	});
	TestNotNull(TEXT("the money rat remains available after its self heal"), MoneyRatAfter);
	TestEqual(TEXT("Break Wealth consumes up to three Wealth stacks and heals six percent max health for each"),
		MoneyRatAfter ? MoneyRatAfter->HP : 0,
		FMath::Min(MoneyRatAfter ? MoneyRatAfter->MaxHP : 0, HPBefore + ExpectedBreakWealthHealing));
	TestEqual(TEXT("Break Wealth removes the exact Wealth stacks that funded its healing"),
		MoneyRatAfter ? GameXXKCardRules::GetCombatStatusStacks(*MoneyRatAfter, EGameXXKCardStatus::Wealth) : INDEX_NONE,
		0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentSharedQiResolutionTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogQiReductionClampsAtZero",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentSharedQiResolutionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the qi fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Civet.P1"), TEXT("Enemy.Ch1.Civet"), 1, 48, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 908;
	TestTrue(TEXT("the qi fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 908, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 CompletedIntentRounds = 0; CompletedIntentRounds < 2; ++CompletedIntentRounds)
	{
		TestTrue(FString::Printf(TEXT("qi setup round %d enters the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("qi setup round %d resolves its catalog action: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("qi setup round %d completes the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	TestTrue(TEXT("the civet pickpocket round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	State.CardRun.ActiveBattle.Deck.SharedEnergy = 1;
	TestTrue(TEXT("the civet pickpocket resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is pickpocket"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Pickpocket")));
	TestEqual(TEXT("the locked one-point shared qi loss cannot underflow the party resource"),
		State.CardRun.ActiveBattle.Deck.SharedEnergy, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentRemovePositiveStatusTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogRemovePositiveStatusLeavesNegativeStatuses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentRemovePositiveStatusTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the positive-status fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Macaque.P1"), TEXT("Enemy.Ch2.Macaque"), 1, 58, 8)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 909;
	TestTrue(TEXT("the positive-status fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 909, &Error));

	FGameXXKCardCombatUnit* Hero = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("the hero exists for the locked snatch target"), Hero);
	if (!Hero)
	{
		return false;
	}
	TestEqual(TEXT("the fixture adds the removable momentum status"),
		GameXXKCardRules::AddCombatStatus(*Hero, EGameXXKCardStatus::Momentum, 1), 1);
	TestEqual(TEXT("the fixture adds a second positive status"),
		GameXXKCardRules::AddCombatStatus(*Hero, EGameXXKCardStatus::Medicine, 1), 1);
	TestEqual(TEXT("the fixture adds a negative poison status that snatch must not remove"),
		GameXXKCardRules::AddCombatStatus(*Hero, EGameXXKCardStatus::Poison, 2), 2);

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the macaque throw-stone round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the macaque opening action resolves before snatch"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the macaque opening action completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the macaque snatch round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the macaque snatch resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is snatch"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Snatch")));

	const FGameXXKCardCombatUnit* HeroAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("the locked hero target remains available after snatch"), HeroAfter);
	TestEqual(TEXT("snatch removes the deterministic first positive status"),
		HeroAfter ? GameXXKCardRules::GetCombatStatusStacks(*HeroAfter, EGameXXKCardStatus::Momentum) : 0, 0);
	TestEqual(TEXT("snatch removes only one positive stack, leaving later positive statuses intact"),
		HeroAfter ? GameXXKCardRules::GetCombatStatusStacks(*HeroAfter, EGameXXKCardStatus::Medicine) : 0, 1);
	TestEqual(TEXT("snatch never treats poison as a removable positive status"),
		HeroAfter ? GameXXKCardRules::GetCombatStatusStacks(*HeroAfter, EGameXXKCardStatus::Poison) : 0, 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentSpeedModifierTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogSpeedModifierAffectsNextForecastThenExpires",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentSpeedModifierTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the speed-modifier fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P1"), TEXT("Enemy.Ch1.Goat"), 1, 58, 7),
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Weasel.P2"), TEXT("Enemy.Ch1.Weasel"), 2, 42, 6)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 910;
	TestTrue(TEXT("the speed-modifier fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 910, &Error));

	const FGameXXKCardCombatUnit* WeaselAtStart = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Weasel.P2");
	});
	TestNotNull(TEXT("the weasel exists for its locked speed modifier"), WeaselAtStart);
	const int32 BaseWeaselSpeed = WeaselAtStart ? WeaselAtStart->Speed : 0;

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 CompletedRounds = 0; CompletedRounds < 2; ++CompletedRounds)
	{
		TestTrue(FString::Printf(TEXT("speed setup round %d enters the enemy phase: %s"), CompletedRounds, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("speed setup round %d resolves goat: %s"), CompletedRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("speed setup round %d resolves weasel: %s"), CompletedRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("speed setup round %d completes: %s"), CompletedRounds, *Error),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	TestTrue(TEXT("the escape round enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestEqual(TEXT("the equal-speed initial forecast resolves the lower slot first"),
		State.CardRun.EnemyIntents.IsValidIndex(0) ? State.CardRun.EnemyIntents[0].SourceUnitId : NAME_None,
		FName(TEXT("Enemy.Goat.P1")));
	TestTrue(TEXT("the goat resolves before the weasel escape"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the weasel escape resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolved catalog identity is escape"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Escape")));

	const FGameXXKCardCombatUnit* WeaselAfterEscape = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Weasel.P2");
	});
	TestEqual(TEXT("escape applies its one-point speed modifier before the next forecast"),
		WeaselAfterEscape ? WeaselAfterEscape->Speed : 0, BaseWeaselSpeed + 1);
	TestTrue(TEXT("the speed modifier round completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestEqual(TEXT("the following saved forecast uses the boosted weasel initiative"),
		State.CardRun.EnemyIntents.IsValidIndex(0) ? State.CardRun.EnemyIntents[0].SourceUnitId : NAME_None,
		FName(TEXT("Enemy.Weasel.P2")));

	TestTrue(TEXT("the boosted-speed forecast enters its enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the boosted weasel action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the remaining goat action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestTrue(TEXT("the boosted-speed enemy phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardCombatUnit* WeaselAfterExpiry = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Weasel.P2");
	});
	TestEqual(TEXT("the one-round speed modifier expires after its affected enemy phase"),
		WeaselAfterExpiry ? WeaselAfterExpiry->Speed : 0, BaseWeaselSpeed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentChargeTest,
	"GameXXK.Battle.EnemyIntentRules.CatalogChargeLocksTargetAndResolvesFollowingPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentChargeTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the charge fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Goat.P1"), TEXT("Enemy.Ch1.Goat"), 1, 58, 7)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 911;
	TestTrue(TEXT("the charge fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 911, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	for (int32 CompletedIntentRounds = 0; CompletedIntentRounds < 2; ++CompletedIntentRounds)
	{
		TestTrue(FString::Printf(TEXT("charge setup round %d enters the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
		TestTrue(FString::Printf(TEXT("charge setup round %d resolves its existing goat intent: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
		TestTrue(FString::Printf(TEXT("charge setup round %d completes the enemy phase: %s"), CompletedIntentRounds, *Error),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	}

	const FGameXXKCardCombatUnit* HeroBeforeCharge = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	const int32 HPBeforeCharge = HeroBeforeCharge ? HeroBeforeCharge->HP : 0;
	TestTrue(TEXT("the goat charge warning enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the saved goat charge forecast exposes its charging delay"),
		State.CardRun.EnemyIntents.IsValidIndex(0) && State.CardRun.EnemyIntents[0].bCharging && State.CardRun.EnemyIntents[0].ChargeRounds == 1);
	TestTrue(TEXT("the goat begins charging without applying its delayed hit"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the charging catalog identity is preserved"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Charge")));
	TestTrue(TEXT("a charging card creates no immediate damage record"), IntentResults.IsEmpty());
	const FGameXXKCardCombatUnit* HeroAfterChargeWarning = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestEqual(TEXT("a charging card leaves its locked hero target unharmed this phase"),
		HeroAfterChargeWarning ? HeroAfterChargeWarning->HP : 0, HPBeforeCharge);
	TestTrue(TEXT("the goat charge warning phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the following saved forecast preserves the same locked goat charge"),
		State.CardRun.EnemyIntents.IsValidIndex(0)
			&& State.CardRun.EnemyIntents[0].IntentDefinitionId == FName(TEXT("Charge"))
			&& !State.CardRun.EnemyIntents[0].bCharging
			&& State.CardRun.EnemyIntents[0].SuggestedTargetUnitId == TEXT("Player"));

	TestTrue(TEXT("the delayed charge enters its following enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the delayed goat charge resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestFalse(TEXT("the second charge card is now an executable hit rather than another warning"), ResolvedIntent.bCharging);
	TestEqual(TEXT("the delayed charge produces one audited direct-damage result"), IntentResults.Num(), 1);
	const FGameXXKCardCombatUnit* HeroAfterDelayedCharge = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestTrue(TEXT("the delayed charge damages the target locked by its warning card"),
		HeroAfterDelayedCharge && HeroAfterDelayedCharge->HP < HPBeforeCharge);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyIntentResolutionAtomicityTest,
	"GameXXK.Battle.EnemyIntentRules.ResolveFailureIsAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyIntentResolutionAtomicityTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the atomicity fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Rooster.P1"), TEXT("Enemy.Ch1.Rooster"), 1, 46, 8)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 913;
	TestTrue(TEXT("the atomicity fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 913, &Error));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	TestTrue(TEXT("the atomicity fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the atomicity fixture has one saved enemy intent"),
		State.CardRun.EnemyIntents.IsValidIndex(State.CardRun.NextEnemyIntentIndex));
	if (!State.CardRun.EnemyIntents.IsValidIndex(State.CardRun.NextEnemyIntentIndex))
	{
		return false;
	}

	FGameXXKCardEnemyIntent& SavedIntent = State.CardRun.EnemyIntents[State.CardRun.NextEnemyIntentIndex];
	FGameXXKResolvedEnemyIntentEffect AddArmor;
	AddArmor.Type = EGameXXKEnemyIntentEffectType::AddArmor;
	AddArmor.TargetUnitIds = {SavedIntent.SourceUnitId};
	AddArmor.Magnitude = 5;
	FGameXXKResolvedEnemyIntentEffect InvalidSurcharge;
	InvalidSurcharge.Type = EGameXXKEnemyIntentEffectType::IncreaseNextCardEnergy;
	InvalidSurcharge.Magnitude = 2;
	SavedIntent.Effects = {AddArmor, InvalidSurcharge};

	const FGameXXKRuntimeState StateBefore = State;
	const int32 IntentIndexBefore = State.CardRun.NextEnemyIntentIndex;
	const FGameXXKEnemyBattleState* EnemyStateBefore = StateBefore.CardRun.ActiveBattle.EnemyStates.Find(SavedIntent.SourceUnitId);
	TestNotNull(TEXT("the atomicity fixture retains the source enemy state"), EnemyStateBefore);
	if (!EnemyStateBefore)
	{
		return false;
	}
	const auto ExportState = [](const FGameXXKRuntimeState& RuntimeState)
	{
		FString Exported;
		FGameXXKRuntimeState::StaticStruct()->ExportText(Exported, &RuntimeState, nullptr, nullptr, PPF_None, nullptr);
		return Exported;
	};
	const FString StateBeforeText = ExportState(StateBefore);
	FGameXXKCardEnemyIntent ResolvedIntent;
	ResolvedIntent.CardId = TEXT("Sentinel.Intent");
	TArray<FGameXXKCardDamageResult> IntentResults;
	FGameXXKCardDamageResult SentinelDamageResult;
	SentinelDamageResult.SourceUnitId = TEXT("Sentinel.Source");
	IntentResults.Add(SentinelDamageResult);
	bool bIntentsFinished = true;

	TestFalse(TEXT("a malformed later catalog effect rejects the saved enemy intent"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the resolver retains the expected surcharge validation error"), Error,
		FString(TEXT("A next-player-hand energy surcharge requires one positive point from a stable enemy during the enemy phase.")));
	TestEqual(TEXT("a failed intent resolution leaves the complete runtime state unchanged"),
		ExportState(State), StateBeforeText);
	TestEqual(TEXT("a failed intent resolution leaves the next saved intent index unchanged"),
		State.CardRun.NextEnemyIntentIndex, IntentIndexBefore);
	const FGameXXKEnemyBattleState* EnemyStateAfter = State.CardRun.ActiveBattle.EnemyStates.Find(SavedIntent.SourceUnitId);
	TestNotNull(TEXT("the source enemy state remains after the rejected intent"), EnemyStateAfter);
	if (EnemyStateAfter)
	{
		TestEqual(TEXT("a failed intent resolution leaves the source cursor unchanged"), EnemyStateAfter->IntentCursor, EnemyStateBefore->IntentCursor);
		TestEqual(TEXT("a failed intent resolution leaves the pending charge id unchanged"), EnemyStateAfter->PendingChargedIntentId, EnemyStateBefore->PendingChargedIntentId);
		TestEqual(TEXT("a failed intent resolution leaves remaining charge rounds unchanged"), EnemyStateAfter->ChargeRoundsRemaining, EnemyStateBefore->ChargeRoundsRemaining);
		TestEqual(TEXT("a failed intent resolution leaves locked charge targets unchanged"), EnemyStateAfter->PendingChargeTargetUnitIds.Num(), EnemyStateBefore->PendingChargeTargetUnitIds.Num());
	}
	FGameXXKCardEnemyIntent DefaultResolvedIntent;
	FString DefaultResolvedIntentText;
	FGameXXKCardEnemyIntent::StaticStruct()->ExportText(DefaultResolvedIntentText, &DefaultResolvedIntent, nullptr, nullptr, PPF_None, nullptr);
	FString ResolvedIntentText;
	FGameXXKCardEnemyIntent::StaticStruct()->ExportText(ResolvedIntentText, &ResolvedIntent, nullptr, nullptr, PPF_None, nullptr);
	TestEqual(TEXT("a failed intent resolution resets the resolved-intent output to its default"), ResolvedIntentText, DefaultResolvedIntentText);
	TestTrue(TEXT("a failed intent resolution emits no partial damage output"), IntentResults.IsEmpty());
	TestFalse(TEXT("a failed intent resolution resets the caller completion output"), bIntentsFinished);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeDisturbIntentTest,
	"GameXXK.Battle.EnemyIntentRules.WhiteApeDisturbBindsOneNextHandCardEnergySurcharge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeDisturbIntentTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	TestTrue(TEXT("the White Ape fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.WhiteApe.P1"), TEXT("Enemy.Ch3.WhiteApe"), 1, 198, 21)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 912;
	TestTrue(TEXT("the White Ape fixture begins a normal route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 912, &Error));

	const TArray<FGameXXKCardInstance> FillerCards = {
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Filler.1"), TEXT("Hero.QingFengYiShi"), 1),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Filler.2"), TEXT("Hero.QingFengYiShi"), 2),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Filler.3"), TEXT("Hero.QingFengYiShi"), 3),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Filler.4"), TEXT("Hero.QingFengYiShi"), 4),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Filler.5"), TEXT("Hero.QingFengYiShi"), 5)};
	const TArray<FGameXXKCardInstance> NextHandCards = {
		MakeEnemyIntentFixtureCard(TEXT("Disturb.HighLater"), TEXT("Hero.JianYiGuanHong"), 40),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.HighHigherOrdinal"), TEXT("Hero.JianYiGuanHong"), 41),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.Low"), TEXT("Hero.QingFengYiShi"), 43),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.HighEarlier"), TEXT("Hero.JianYiGuanHong"), 40),
		MakeEnemyIntentFixtureCard(TEXT("Disturb.LowLater"), TEXT("Hero.QingFengYiShi"), 44)};
	TArray<FGameXXKCardInstance> AllFixtureCards = FillerCards;
	AllFixtureCards.Append(NextHandCards);
	TestTrue(TEXT("the deterministic disturbance fixture initializes its card ledger"),
		GameXXKCardRules::InitializeBattleDeck(State.CardRun.ActiveBattle.Deck, AllFixtureCards, 912, &Error));
	State.CardRun.ActiveBattle.Deck.Hand = FillerCards;
	State.CardRun.ActiveBattle.Deck.DrawPile = NextHandCards;
	State.CardRun.ActiveBattle.Deck.DiscardPile.Reset();
	TestTrue(TEXT("the deterministic disturbance fixture keeps a valid initial battle runtime"),
		GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	FGameXXKCardBattleRuntime StalePlayerPendingRuntime = State.CardRun.ActiveBattle;
	StalePlayerPendingRuntime.PendingNextPlayerHandEnergySurcharge = 1;
	StalePlayerPendingRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId = TEXT("Enemy.WhiteApe.P1");
	FString StalePlayerPendingError;
	TestFalse(TEXT("a saved next-hand surcharge is malformed outside its enemy phase"),
		GameXXKCardRules::ValidateCardBattleRuntime(StalePlayerPendingRuntime, &StalePlayerPendingError));

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the White Ape opening turn enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	TestTrue(TEXT("the White Ape opening throw rock resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	TestEqual(TEXT("the first White Ape catalog intent is throw rock"), ResolvedIntent.IntentDefinitionId, FName(TEXT("ThrowRock")));
	TestTrue(TEXT("the White Ape opening enemy phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));
	TestEqual(TEXT("the next forecast exposes White Ape disturb"),
		State.CardRun.EnemyIntents.IsValidIndex(0) ? State.CardRun.EnemyIntents[0].IntentDefinitionId : NAME_None,
		FName(TEXT("Disturb")));

	TestTrue(TEXT("the White Ape disturbance turn enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	State.CardRun.ActiveBattle.Deck.Hand.Reset();
	State.CardRun.ActiveBattle.Deck.DrawPile = NextHandCards;
	State.CardRun.ActiveBattle.Deck.DiscardPile = FillerCards;
	TestTrue(TEXT("the disturbance hand refresh setup keeps a valid battle runtime"),
		GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	FGameXXKCardBattleRuntime DuplicateQueueRuntime = State.CardRun.ActiveBattle;
	FString DuplicateQueueError;
	const int32 ModifierCountBeforeDuplicateQueue = DuplicateQueueRuntime.Modifiers.Num();
	TestTrue(TEXT("the first same-phase White Ape surcharge packet queues"),
		GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(DuplicateQueueRuntime, 1, TEXT("Enemy.WhiteApe.P1"), &DuplicateQueueError));
	TestTrue(TEXT("a duplicate same-phase White Ape surcharge packet collapses instead of stacking"),
		GameXXKCardRules::QueueNextPlayerHandEnergySurcharge(DuplicateQueueRuntime, 1, TEXT("Enemy.WhiteApe.P1"), &DuplicateQueueError));
	TestEqual(TEXT("duplicate same-phase packets retain exactly one pending surcharge"),
		DuplicateQueueRuntime.PendingNextPlayerHandEnergySurcharge, 1);
	TestEqual(TEXT("duplicate same-phase packets keep the first saved enemy source"),
		DuplicateQueueRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId, FName(TEXT("Enemy.WhiteApe.P1")));
	TestEqual(TEXT("queued duplicate packets do not materialize or stack card modifiers early"),
		DuplicateQueueRuntime.Modifiers.Num(), ModifierCountBeforeDuplicateQueue);
	const bool bResolvedDisturb = FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error);
	TestTrue(FString::Printf(TEXT("White Ape disturb resolves as a non-damage intent: %s"), *Error), bResolvedDisturb);
	if (!bResolvedDisturb)
	{
		return false;
	}
	TestEqual(TEXT("the resolved catalog identity remains disturb"), ResolvedIntent.IntentDefinitionId, FName(TEXT("Disturb")));
	TestTrue(TEXT("disturb produces no direct-damage record"), IntentResults.IsEmpty());
	FGameXXKCardBattleRuntime VictoryPendingRuntime = State.CardRun.ActiveBattle;
	FGameXXKCardCombatUnit* VictoryWhiteApe = VictoryPendingRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.WhiteApe.P1");
	});
	TestNotNull(TEXT("the queued-disturb victory fixture keeps its White Ape source"), VictoryWhiteApe);
	if (!VictoryWhiteApe)
	{
		return false;
	}
	VictoryWhiteApe->HP = 1;
	VictoryWhiteApe->bLiving = true;
	TestEqual(TEXT("enemy-side poison can set up a terminal end-phase after disturb queues"),
		GameXXKCardRules::AddCombatStatus(*VictoryWhiteApe, EGameXXKCardStatus::Poison, 1), 1);
	TArray<FGameXXKCardDamageResult> VictoryTerminalResults;
	FString VictoryTerminalError;
	const bool bVictoryTerminalTransition = GameXXKCardRules::BeginNextPlayerCardRound(VictoryPendingRuntime, VictoryTerminalResults, &VictoryTerminalError);
	TestTrue(FString::Printf(TEXT("an enemy-end DoT terminal transition clears queued disturb state: %s"), *VictoryTerminalError), bVictoryTerminalTransition);
	if (bVictoryTerminalTransition)
	{
		TestEqual(TEXT("the queued-disturb end-phase fixture reaches victory"), VictoryPendingRuntime.Phase, EGameXXKCardBattlePhase::Victory);
		TestEqual(TEXT("victory clears a queued next-hand surcharge amount"), VictoryPendingRuntime.PendingNextPlayerHandEnergySurcharge, 0);
		TestTrue(TEXT("victory clears a queued next-hand surcharge source"), VictoryPendingRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone());
		TestTrue(TEXT("victory after queued disturb remains a valid persisted runtime"),
			GameXXKCardRules::ValidateCardBattleRuntime(VictoryPendingRuntime, &VictoryTerminalError));
	}

	FGameXXKCardBattleRuntime DefeatPendingRuntime = State.CardRun.ActiveBattle;
	FGameXXKCardCombatUnit* DefeatHero = DefeatPendingRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("the queued-disturb defeat fixture keeps its player target"), DefeatHero);
	if (!DefeatHero)
	{
		return false;
	}
	DefeatHero->HP = 1;
	DefeatHero->Defense = 0;
	DefeatHero->Armor = 0;
	DefeatHero->bLiving = true;
	FGameXXKCardDamageContext DefeatContext;
	DefeatContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	DefeatContext.SourceUnitId = TEXT("Enemy.WhiteApe.P1");
	FGameXXKCardDamageResult DefeatTerminalResult;
	FString DefeatTerminalError;
	const bool bDefeatTerminalTransition = GameXXKCardRules::ResolveEnemyDirectAttack(
		DefeatPendingRuntime,
		DefeatContext,
		TEXT("Player"),
		1,
		DefeatTerminalResult,
		nullptr,
		&DefeatTerminalError);
	TestTrue(FString::Printf(TEXT("an enemy direct-hit terminal transition clears queued disturb state: %s"), *DefeatTerminalError), bDefeatTerminalTransition);
	if (bDefeatTerminalTransition)
	{
		TestEqual(TEXT("the queued-disturb direct-hit fixture reaches defeat"), DefeatPendingRuntime.Phase, EGameXXKCardBattlePhase::Defeat);
		TestEqual(TEXT("defeat clears a queued next-hand surcharge amount"), DefeatPendingRuntime.PendingNextPlayerHandEnergySurcharge, 0);
		TestTrue(TEXT("defeat clears a queued next-hand surcharge source"), DefeatPendingRuntime.PendingNextPlayerHandEnergySurchargeSourceUnitId.IsNone());
		TestTrue(TEXT("defeat after queued disturb remains a valid persisted runtime"),
			GameXXKCardRules::ValidateCardBattleRuntime(DefeatPendingRuntime, &DefeatTerminalError));
	}
	TestTrue(TEXT("the disturbance enemy phase completes into a refreshed player hand"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error));

	State.CardRun.ActiveBattle.Deck.SharedEnergy = 4;
	FGameXXKCardPlayPreview EarlierHighPreview;
	FGameXXKCardPlayPreview LaterHighPreview;
	FGameXXKCardPlayPreview HigherOrdinalHighPreview;
	TestTrue(TEXT("equal-cost and equal-acquisition candidates select the lexical instance ID for the White Ape surcharge"),
		GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, TEXT("Disturb.HighEarlier"), EarlierHighPreview, &Error));
	TestEqual(TEXT("the exact selected high-cost card pays one additional shared energy"), EarlierHighPreview.EffectiveEnergyCost, 4);
	TestTrue(TEXT("the competing high-cost card remains normally priced"),
		GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, TEXT("Disturb.HighLater"), LaterHighPreview, &Error));
	TestEqual(TEXT("the lexically later equal-acquisition instance remains normally priced"), LaterHighPreview.EffectiveEnergyCost, 3);
	TestTrue(TEXT("a higher-acquisition high-cost card remains normally priced"),
		GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, TEXT("Disturb.HighHigherOrdinal"), HigherOrdinalHighPreview, &Error));
	TestEqual(TEXT("acquisition order wins before lexical order when selecting the exact surcharge target"), HigherOrdinalHighPreview.EffectiveEnergyCost, 3);

	State.CardRun.ActiveBattle.Deck.SharedEnergy = 3;
	FGameXXKCardPlayPreview RejectedPreview;
	TestFalse(TEXT("insufficient shared energy rejects the surcharged card without consuming its one-shot effect"),
		GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, TEXT("Disturb.HighEarlier"), RejectedPreview, &Error));
	TestEqual(TEXT("a failed preview retains the pending one-shot cost modifier"), State.CardRun.ActiveBattle.Modifiers.Num(), 1);

	State.CardRun.ActiveBattle.Deck.SharedEnergy = 4;
	FGameXXKCardPlayResult SuccessfulPlay;
	TestTrue(TEXT("the selected card commits at the same surcharged cost shown by preview"),
		GameXXKCardRules::ResolveCardPlay(State.CardRun.ActiveBattle, TEXT("Disturb.HighEarlier"), TEXT("Enemy.WhiteApe.P1"), SuccessfulPlay, &Error));
	TestEqual(TEXT("the successful surcharged play spends the full previewed shared energy"), State.CardRun.ActiveBattle.Deck.SharedEnergy, 0);
	TestTrue(TEXT("the successful selected play consumes the one-shot White Ape modifier"), State.CardRun.ActiveBattle.Modifiers.IsEmpty());

	const FName ForcedDiscardTargetId(TEXT("Disturb.HighLater"));
	const bool bHasForcedDiscardTargetInHand = State.CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([ForcedDiscardTargetId](const FGameXXKCardInstance& Instance)
	{
		return Instance.InstanceId == ForcedDiscardTargetId;
	});
	TestTrue(TEXT("the unplayed comparison card remains available for the forced-discard lifecycle"), bHasForcedDiscardTargetInHand);
	if (!bHasForcedDiscardTargetInHand)
	{
		return false;
	}

	// This mirrors the exact per-instance runtime state materialized by Disturb, then sends that
	// instance through the normal adapter-owned forced-discard flow.
	FGameXXKCardBattleModifierRuntime& ForcedDiscardSurcharge = State.CardRun.ActiveBattle.Modifiers.AddDefaulted_GetRef();
	ForcedDiscardSurcharge.ModifierId = TEXT("Disturb.ForcedDiscardSurcharge");
	ForcedDiscardSurcharge.RequiredPlayedCardInstanceId = ForcedDiscardTargetId;
	ForcedDiscardSurcharge.SourceCardInstanceId = ForcedDiscardTargetId;
	ForcedDiscardSurcharge.SourceUnitId = TEXT("Enemy.WhiteApe.P1");
	ForcedDiscardSurcharge.Definition.Trigger = EGameXXKCardBattleModifierTrigger::OnCardPlayed;
	ForcedDiscardSurcharge.Definition.EffectType = EGameXXKCardEffectType::ModifyEnergyCost;
	ForcedDiscardSurcharge.Definition.Target = EGameXXKCardEffectTarget::PlayedCard;
	ForcedDiscardSurcharge.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::SharedDeck;
	ForcedDiscardSurcharge.Definition.RecipientTarget = EGameXXKCardEffectTarget::PlayedCard;
	ForcedDiscardSurcharge.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	ForcedDiscardSurcharge.Definition.Magnitude = 1;
	ForcedDiscardSurcharge.Definition.RemainingTriggers = 1;
	ForcedDiscardSurcharge.Definition.bPersistent = true;
	TestTrue(TEXT("the forced-discard lifecycle fixture retains a valid exact surcharge runtime"),
		GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	const auto IsExactSurchargeRuntimeValidAfter = [&State](auto&& Mutate)
	{
		FGameXXKCardBattleRuntime CorruptedRuntime = State.CardRun.ActiveBattle;
		Mutate(CorruptedRuntime.Modifiers[0]);
		FString CorruptionError;
		return GameXXKCardRules::ValidateCardBattleRuntime(CorruptedRuntime, &CorruptionError);
	};
	TestFalse(TEXT("a saved exact surcharge cannot add a triggered-role gate"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.RequiredTriggeredRole = EGameXXKCharacterRole::Hero;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot add a triggered-owner gate"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.RequiredTriggeredOwnerId = TEXT("Player");
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain an original selected target"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.OriginalSelectedTargetUnitId = TEXT("Player");
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain explicit recipients"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.RecipientUnitIds.Add(TEXT("Player"));
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a triggered attack target scope"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.TriggeredAttackTargetScope = EGameXXKCardTriggeredAttackTargetScope::AnyTarget;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain an unrelated status payload"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Status = EGameXXKCardStatus::Poison;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a minimum-result threshold"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.MinimumResult = 1;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot add a conditional type"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.Type = EGameXXKCardEffectConditionType::OwnerHasStatus;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a conditional status"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.Status = EGameXXKCardStatus::Poison;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a minimum-status-stack condition"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.MinimumStatusStacks = 1;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a minimum-armor condition"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.MinimumArmor = 1;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a health-percent condition"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.HealthPercentThreshold = 0.5f;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a terrain condition"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.Terrain = EGameXXKCardTerrain::Plain;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain an alternate-terrain condition"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.AlternateTerrain = EGameXXKCardTerrain::Plain;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot consume a condition status"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.bConsumeStatus = true;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a consumed-status cap"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.MaxConsumedStatusStacks = 1;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot scale from consumed stacks"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.bScaleMagnitudeByConsumedStacks = true;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot consume owner armor"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.bConsumeOwnerArmor = true;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot retain a consumed-armor cap"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.MaxConsumedArmor = 1;
		}));
	TestFalse(TEXT("a saved exact surcharge cannot negate its otherwise unconditional application"),
		IsExactSurchargeRuntimeValidAfter([](FGameXXKCardBattleModifierRuntime& Modifier)
		{
			Modifier.Definition.Condition.bNegate = true;
		}));
	FGameXXKCardBattleRuntime UnboundModifierRuntime = State.CardRun.ActiveBattle;
	UnboundModifierRuntime.Modifiers[0].RequiredPlayedCardInstanceId = NAME_None;
	FString UnboundModifierError;
	TestTrue(TEXT("ordinary unbound card modifiers remain outside the White Ape exact-binding invariant"),
		GameXXKCardRules::ValidateCardBattleRuntime(UnboundModifierRuntime, &UnboundModifierError));
	FGameXXKCardBattleRuntime PartySourceBoundRuntime = State.CardRun.ActiveBattle;
	PartySourceBoundRuntime.Modifiers[0].SourceUnitId = TEXT("Player");
	FString PartySourceBoundError;
	TestFalse(TEXT("a saved exact surcharge is malformed when its source is not an enemy"),
		GameXXKCardRules::ValidateCardBattleRuntime(PartySourceBoundRuntime, &PartySourceBoundError));
	FGameXXKCardBattleRuntime MultipleBoundRuntime = State.CardRun.ActiveBattle;
	FGameXXKCardBattleModifierRuntime SecondBoundSurcharge = MultipleBoundRuntime.Modifiers[0];
	SecondBoundSurcharge.ModifierId = TEXT("Disturb.SecondBoundSurcharge");
	SecondBoundSurcharge.RequiredPlayedCardInstanceId = TEXT("Disturb.Low");
	SecondBoundSurcharge.SourceCardInstanceId = TEXT("Disturb.Low");
	MultipleBoundRuntime.Modifiers.Add(MoveTemp(SecondBoundSurcharge));
	FString MultipleBoundError;
	TestFalse(TEXT("a saved runtime cannot stack more than one exact White Ape surcharge"),
		GameXXKCardRules::ValidateCardBattleRuntime(MultipleBoundRuntime, &MultipleBoundError));
	FGameXXKCardBattleRuntime DefeatedEnemySourceBoundRuntime = State.CardRun.ActiveBattle;
	DefeatedEnemySourceBoundRuntime.Phase = EGameXXKCardBattlePhase::Victory;
	FGameXXKCardCombatUnit* DefeatedEnemySource = DefeatedEnemySourceBoundRuntime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.WhiteApe.P1");
	});
	TestNotNull(TEXT("the defeated-enemy exact-binding fixture retains its White Ape source"), DefeatedEnemySource);
	if (!DefeatedEnemySource)
	{
		return false;
	}
	DefeatedEnemySource->HP = 0;
	DefeatedEnemySource->bLiving = false;
	FString DefeatedEnemySourceBoundError;
	TestTrue(TEXT("an already materialized exact surcharge remains valid after its enemy source is defeated"),
		GameXXKCardRules::ValidateCardBattleRuntime(DefeatedEnemySourceBoundRuntime, &DefeatedEnemySourceBoundError));
	FGameXXKCardBattleRuntime VictoryBoundRuntime = State.CardRun.ActiveBattle;
	VictoryBoundRuntime.Phase = EGameXXKCardBattlePhase::Victory;
	FString VictoryBoundError;
	TestTrue(TEXT("a hand-bound surcharge remains valid after a different player card has won the battle"),
		GameXXKCardRules::ValidateCardBattleRuntime(VictoryBoundRuntime, &VictoryBoundError));
	FGameXXKCardBattleRuntime StaleEnemyBoundRuntime = State.CardRun.ActiveBattle;
	StaleEnemyBoundRuntime.Phase = EGameXXKCardBattlePhase::Enemy;
	FString StaleEnemyBoundError;
	TestFalse(TEXT("a saved hand-bound surcharge is malformed during the enemy phase"),
		GameXXKCardRules::ValidateCardBattleRuntime(StaleEnemyBoundRuntime, &StaleEnemyBoundError));
	FGameXXKCardBattleRuntime StaleDefeatBoundRuntime = State.CardRun.ActiveBattle;
	StaleDefeatBoundRuntime.Phase = EGameXXKCardBattlePhase::Defeat;
	FString StaleDefeatBoundError;
	TestFalse(TEXT("a saved hand-bound surcharge is malformed after defeat"),
		GameXXKCardRules::ValidateCardBattleRuntime(StaleDefeatBoundRuntime, &StaleDefeatBoundError));
	FGameXXKCardBattleRuntime StaleSavedRuntime = State.CardRun.ActiveBattle;
	FString StaleSaveError;
	TestTrue(TEXT("the stale-save fixture can model a raw deck move after a bound card leaves hand"),
		GameXXKCardRules::MoveHandCardToDiscard(StaleSavedRuntime.Deck, ForcedDiscardTargetId, &StaleSaveError));
	TestFalse(TEXT("a saved exact surcharge whose target no longer occupies hand is rejected as malformed"),
		GameXXKCardRules::ValidateCardBattleRuntime(StaleSavedRuntime, &StaleSaveError));

	TestTrue(TEXT("a draw with one declared discard opens the forced-discard choice around the marked instance"),
		GameXXKCardRules::DrawCards(State.CardRun.ActiveBattle.Deck, 2, 1, &Error));
	TestEqual(TEXT("the mid-phase declared discard exposes a real forced-discard choice"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestTrue(TEXT("the adapter accepts discarding the marked instance through that choice"),
		FGameXXKCardBattleAdapter::SubmitForcedDiscard(State, {ForcedDiscardTargetId}, &Error));
	TestTrue(TEXT("a marked card leaving hand through forced discard immediately clears its surcharge"),
		State.CardRun.ActiveBattle.Modifiers.IsEmpty());

	FGameXXKBattleDeckState& DeckAfterForcedDiscard = State.CardRun.ActiveBattle.Deck;
	TestTrue(TEXT("the marked instance reaches discard before a same-phase redraw"),
		DeckAfterForcedDiscard.DiscardPile.ContainsByPredicate([ForcedDiscardTargetId](const FGameXXKCardInstance& Instance)
		{
			return Instance.InstanceId == ForcedDiscardTargetId;
		}));
	TestTrue(TEXT("a different hand card can leave to make room for the redraw"),
		GameXXKCardRules::MoveHandCardToDiscard(DeckAfterForcedDiscard, DeckAfterForcedDiscard.Hand.Last().InstanceId, &Error));

	// Stage the discarded marked instance as the next real draw without changing its stable identity.
	DeckAfterForcedDiscard.DiscardPile.Append(DeckAfterForcedDiscard.DrawPile);
	DeckAfterForcedDiscard.DrawPile.Reset();
	const int32 ForcedDiscardTargetIndex = DeckAfterForcedDiscard.DiscardPile.IndexOfByPredicate([ForcedDiscardTargetId](const FGameXXKCardInstance& Instance)
	{
		return Instance.InstanceId == ForcedDiscardTargetId;
	});
	TestTrue(TEXT("the marked discard remains addressable for the controlled redraw"), ForcedDiscardTargetIndex != INDEX_NONE);
	if (ForcedDiscardTargetIndex == INDEX_NONE)
	{
		return false;
	}
	DeckAfterForcedDiscard.DrawPile.Add(DeckAfterForcedDiscard.DiscardPile[ForcedDiscardTargetIndex]);
	DeckAfterForcedDiscard.DiscardPile.RemoveAt(ForcedDiscardTargetIndex, 1, EAllowShrinking::No);
	TestTrue(TEXT("the controlled redraw setup preserves the persisted runtime invariants"),
		GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	TestTrue(TEXT("the discarded marked instance can re-enter hand before the player phase ends"),
		GameXXKCardRules::DrawCards(DeckAfterForcedDiscard, 1, 0, &Error));

	State.CardRun.ActiveBattle.Deck.SharedEnergy = 4;
	FGameXXKCardPlayPreview RedrawnTargetPreview;
	TestTrue(TEXT("the re-drawn instance remains playable after its forced-discard cleanup"),
		GameXXKCardRules::BuildCardPlayPreview(State.CardRun.ActiveBattle, ForcedDiscardTargetId, RedrawnTargetPreview, &Error));
	TestEqual(TEXT("a same-phase re-draw never revives the discarded instance's old White Ape surcharge"),
		RedrawnTargetPreview.EffectiveEnergyCost, 3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGraymaneMarkedHuntCatalogPassiveTest,
	"GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntOnlyAmplifiesMarkedCatalogDirectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGraymaneMarkedHuntCatalogPassiveTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("the Graymane fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		return false;
	}
	FGameXXKBattleRuntimeUnit Hero = MakeEnemyIntentFixtureHero();
	State.ActiveBattleParty = {Hero};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Graymane.P2"), TEXT("Enemy.Ch2.GraymaneWolfKing"), 2, 158, 20)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 924;
	if (!TestTrue(TEXT("the Graymane fixture begins an elite route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Elite, EGameXXKCardTerrain::Plain, 924, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* HeroBeforeOpeningAttack = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the Graymane fixture has its runtime hero before enemy damage"), HeroBeforeOpeningAttack))
	{
		return false;
	}
	HeroBeforeOpeningAttack->Defense = 0;
	if (!TestTrue(TEXT("Graymane's opening catalog forecast exists"), State.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent& OpeningForecast = State.CardRun.EnemyIntents[0];
	TestEqual(TEXT("the unmarked opening attack remains the catalog base action"), OpeningForecast.IntentDefinitionId, FName(TEXT("HuntMark")));
	TestEqual(TEXT("the unmarked opening forecast keeps the base direct-damage magnitude"), OpeningForecast.Damage, 18);

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the opening Graymane turn enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error))
		|| !TestTrue(TEXT("the opening Graymane catalog action resolves"),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the resolved opening action remains hunt mark"), ResolvedIntent.IntentDefinitionId, FName(TEXT("HuntMark")));
	if (!TestEqual(TEXT("the unmarked opening action emits exactly one hit"), IntentResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the unmarked opening action executes its base requested damage"), IntentResults[0].RequestedDamage, 18);
	FGameXXKCardCombatUnit* MarkedHero = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the Graymane fixture keeps its hero target"), MarkedHero))
	{
		return false;
	}
	TestEqual(TEXT("the opening action applies the catalog mark before the follow-up forecast"),
		GameXXKCardRules::GetCombatStatusStacks(*MarkedHero, EGameXXKCardStatus::Mark), 2);
	if (!TestTrue(TEXT("the single Graymane action finishes its enemy phase"), bIntentsFinished)
		|| !TestTrue(TEXT("the marked Graymane enemy phase completes"),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the marked follow-up Graymane forecast exists"), State.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent& MarkedForecast = State.CardRun.EnemyIntents[0];
	TestEqual(TEXT("the marked follow-up forecast selects continuous hunt"), MarkedForecast.IntentDefinitionId, FName(TEXT("ContinuousHunt")));
	TestEqual(TEXT("the marked follow-up forecast exposes the 20 percent amplified direct damage"), MarkedForecast.Damage, 16);
	if (!TestEqual(TEXT("the marked follow-up forecast has one resolved direct effect"), MarkedForecast.Effects.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the marked resolved effect matches the amplified card-face magnitude"), MarkedForecast.Effects[0].Magnitude, 16);

	if (!TestTrue(TEXT("the marked continuous-hunt turn enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error))
		|| !TestTrue(TEXT("the marked continuous-hunt action resolves"),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the marked action resolves the same card shown by the forecast"), ResolvedIntent.IntentDefinitionId, FName(TEXT("ContinuousHunt")));
	if (!TestEqual(TEXT("continuous hunt preserves its catalog three-hit count"), IntentResults.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("the first continuous-hunt hit consumes Mark for the fixed bonus"), IntentResults[0].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("the second continuous-hunt hit consumes Mark for the fixed bonus"), IntentResults[1].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("the third continuous-hunt hit has no Mark bonus after both stacks are consumed"), IntentResults[2].MarkDamageBonusPercent, 0);
	TestEqual(TEXT("the first continuous-hunt hit floor-amplifies sixteen damage to eighteen"), IntentResults[0].DamageAfterVulnerability, 18);
	TestEqual(TEXT("the second continuous-hunt hit floor-amplifies sixteen damage to eighteen"), IntentResults[1].DamageAfterVulnerability, 18);
	TestEqual(TEXT("the third continuous-hunt hit remains at sixteen after Mark is exhausted"), IntentResults[2].DamageAfterVulnerability, 16);
	for (const FGameXXKCardDamageResult& DamageResult : IntentResults)
	{
		TestEqual(TEXT("each marked continuous-hunt hit keeps the Graymane source"), DamageResult.SourceUnitId, FName(TEXT("Enemy.Graymane.P2")));
		TestEqual(TEXT("each marked continuous-hunt hit executes the forecasted amplified requested damage"), DamageResult.RequestedDamage, 16);
	}
	FGameXXKCardCombatUnit* HeroAfterContinuousHunt = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the Graymane fixture keeps its hero after continuous hunt"), HeroAfterContinuousHunt))
	{
		return false;
	}
	TestEqual(TEXT("continuous hunt consumes both starting Mark stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*HeroAfterContinuousHunt, EGameXXKCardStatus::Mark), 0);

	FGameXXKCardDamageContext GenericContext;
	GenericContext.SourceUnitId = TEXT("Enemy.Graymane.P2");
	GenericContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult GenericDamageResult;
	if (!TestTrue(TEXT("a post-hunt generic direct-damage packet can target the unmarked hero"),
		GameXXKCardRules::ResolveEnemyDirectAttack(
			State.CardRun.ActiveBattle,
			GenericContext,
			TEXT("Player"),
			10,
			GenericDamageResult,
			nullptr,
			&Error)))
	{
		return false;
	}
	TestEqual(TEXT("the post-hunt generic packet preserves its requested damage"), GenericDamageResult.RequestedDamage, 10);
	TestEqual(TEXT("the post-hunt generic packet keeps ten damage after zero defense"), GenericDamageResult.DamageAfterDefense, 10);
	TestEqual(TEXT("the post-hunt unmarked generic packet audits no Mark bonus"), GenericDamageResult.MarkDamageBonusPercent, 0);
	TestEqual(TEXT("the post-hunt unmarked generic packet remains at ten after status amplification"), GenericDamageResult.DamageAfterVulnerability, 10);

	FGameXXKCardCombatUnit* HeroBeforeRemarkedGenericHit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the post-hunt generic packet keeps its hero target"), HeroBeforeRemarkedGenericHit))
	{
		return false;
	}
	TestEqual(TEXT("the generic damage fixture can reapply one Mark stack"),
		GameXXKCardRules::AddCombatStatus(*HeroBeforeRemarkedGenericHit, EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardDamageResult RemarkedGenericDamageResult;
	if (!TestTrue(TEXT("an independent generic packet can hit the re-marked hero"),
		GameXXKCardRules::ResolveEnemyDirectAttack(
			State.CardRun.ActiveBattle,
			GenericContext,
			TEXT("Player"),
			10,
			RemarkedGenericDamageResult,
			nullptr,
			&Error)))
	{
		return false;
	}
	TestEqual(TEXT("the re-marked generic packet preserves its requested damage"), RemarkedGenericDamageResult.RequestedDamage, 10);
	TestEqual(TEXT("the re-marked generic packet keeps ten damage after zero defense"), RemarkedGenericDamageResult.DamageAfterDefense, 10);
	TestEqual(TEXT("the re-marked generic packet uses only the global fifteen-percent Mark bonus"), RemarkedGenericDamageResult.MarkDamageBonusPercent, 15);
	TestEqual(TEXT("the re-marked generic packet floor-amplifies ten damage to eleven"), RemarkedGenericDamageResult.DamageAfterVulnerability, 11);
	TestEqual(TEXT("the re-marked generic packet consumes exactly one Mark stack"), RemarkedGenericDamageResult.MarkStacksConsumed, 1);
	FGameXXKCardCombatUnit* HeroAfterRemarkedGenericHit = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the re-marked generic packet keeps its hero after damage"), HeroAfterRemarkedGenericHit))
	{
		return false;
	}
	TestEqual(TEXT("the re-marked generic packet consumes the reapplied Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*HeroAfterRemarkedGenericHit, EGameXXKCardStatus::Mark), 0);

	FGameXXKRuntimeState OtherEnemyState = UGameXXKMVPRules::CreateNewGame();
	if (!TestTrue(TEXT("the non-Graymane fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OtherEnemyState, &Error)))
	{
		return false;
	}
	FGameXXKBattleRuntimeUnit OtherHero = MakeEnemyIntentFixtureHero();
	OtherHero.Defense = 0;
	OtherEnemyState.ActiveBattleParty = {OtherHero};
	OtherEnemyState.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.GrayWolf.P2"), TEXT("Enemy.Ch2.GrayWolf"), 2, 62, 20)};
	OtherEnemyState.bHasActiveBattle = true;
	OtherEnemyState.ActiveBattleNodeId = 925;
	if (!TestTrue(TEXT("the non-Graymane fixture begins a route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(OtherEnemyState, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 925, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* OtherMarkedHero = OtherEnemyState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the non-Graymane fixture keeps its hero target"), OtherMarkedHero))
	{
		return false;
	}
	TestEqual(TEXT("the test can mark the non-Graymane target before reforecasting"),
		GameXXKCardRules::AddCombatStatus(*OtherMarkedHero, EGameXXKCardStatus::Mark, 1), 1);
	OtherEnemyState.CardRun.EnemyIntents.Reset();
	if (!TestTrue(TEXT("the marked non-Graymane target enters an enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(OtherEnemyState, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the marked non-Graymane forecast exists"), OtherEnemyState.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	TestEqual(TEXT("a marked target does not amplify a non-Graymane catalog enemy"), OtherEnemyState.CardRun.EnemyIntents[0].Damage, 20);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGraymaneMarkedHuntRequiresMarkedTargetTest,
	"GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntRequiresMarkedTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGraymaneMarkedHuntRequiresMarkedTargetTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("the no-mark Graymane fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		return false;
	}
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Graymane.P2"), TEXT("Enemy.Ch2.GraymaneWolfKing"), 2, 158, 20)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 926;
	if (!TestTrue(TEXT("the no-mark Graymane fixture begins an elite route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Elite, EGameXXKCardTerrain::Plain, 926, &Error)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* GraymaneState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Graymane.P2"));
	if (!TestNotNull(TEXT("the no-mark fixture has persisted Graymane state"), GraymaneState))
	{
		return false;
	}
	GraymaneState->IntentCursor = 1;
	State.CardRun.EnemyIntents.Reset();
	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the no-mark continuous-hunt fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the no-mark fixture has a replacement forecast"), State.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	TestEqual(TEXT("continuous hunt without a real marked target is skipped instead of falling back"),
		State.CardRun.EnemyIntents[0].IntentDefinitionId,
		FName(TEXT("PackOrder")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGraymaneMarkedHuntFloorRoundingTest,
	"GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntUsesFloorRounding",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGraymaneMarkedHuntFloorRoundingTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("the rounding Graymane fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		return false;
	}
	State.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Graymane.P2"), TEXT("Enemy.Ch2.GraymaneWolfKing"), 2, 158, 19)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 927;
	if (!TestTrue(TEXT("the rounding Graymane fixture begins an elite route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Elite, EGameXXKCardTerrain::Plain, 927, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* MarkedHero = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	FGameXXKEnemyBattleState* GraymaneState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Graymane.P2"));
	if (!TestNotNull(TEXT("the rounding fixture retains its marked target"), MarkedHero)
		|| !TestNotNull(TEXT("the rounding fixture has persisted Graymane state"), GraymaneState))
	{
		return false;
	}
	TestEqual(TEXT("the rounding fixture can add one mark to the locked target"),
		GameXXKCardRules::AddCombatStatus(*MarkedHero, EGameXXKCardStatus::Mark, 1), 1);
	GraymaneState->IntentCursor = 1;
	State.CardRun.EnemyIntents.Reset();
	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the rounding fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the rounding fixture builds its continuous-hunt forecast"), State.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent& Forecast = State.CardRun.EnemyIntents[0];
	TestEqual(TEXT("the rounding fixture selects continuous hunt"), Forecast.IntentDefinitionId, FName(TEXT("ContinuousHunt")));
	TestEqual(TEXT("base magnitude thirteen is floor-amplified to fifteen"), Forecast.Damage, 15);
	if (!TestEqual(TEXT("the floor-rounding forecast contains one direct effect"), Forecast.Effects.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the stored direct effect uses the same floor-rounded magnitude"), Forecast.Effects[0].Magnitude, 15);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGraymaneMarkedHuntActualTargetOnlyTest,
	"GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntChecksActualLockedTargetOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGraymaneMarkedHuntActualTargetOnlyTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!TestTrue(TEXT("the target-specific Graymane fixture initializes the route deck"),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		return false;
	}
	FGameXXKBattleRuntimeUnit UnmarkedHero = MakeEnemyIntentFixtureHero();
	UnmarkedHero.Defense = 0;
	UnmarkedHero.HP = 40;
	FGameXXKBattleRuntimeUnit MarkedPartner = MakeEnemyIntentFixtureHero();
	MarkedPartner.Id = TEXT("Partner");
	MarkedPartner.DisplayName = FText::FromString(TEXT("伙伴"));
	MarkedPartner.Defense = 0;
	State.ActiveBattleParty = {UnmarkedHero};
	State.ActiveBattleEnemies = {
		MakeEnemyIntentFixtureUnit(TEXT("Enemy.Graymane.P2"), TEXT("Enemy.Ch2.GraymaneWolfKing"), 2, 158, 20)};
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 928;
	if (!TestTrue(TEXT("the target-specific Graymane fixture begins an elite route battle"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Elite, EGameXXKCardTerrain::Plain, 928, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* RuntimeUnmarkedHero = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	if (!TestNotNull(TEXT("the target-specific fixture retains its lower-health hero"), RuntimeUnmarkedHero))
	{
		return false;
	}
	RuntimeUnmarkedHero->HP = 40;
	RuntimeUnmarkedHero->Defense = 0;
	FGameXXKCardCombatUnit RuntimePartner = *RuntimeUnmarkedHero;
	RuntimePartner.UnitId = TEXT("Partner");
	RuntimePartner.HP = 100;
	RuntimePartner.MaxHP = 100;
	RuntimePartner.StableSortOrder = RuntimeUnmarkedHero->StableSortOrder + 1;
	RuntimePartner.Statuses.Reset();
	State.CardRun.ActiveBattle.Units.Add(RuntimePartner);
	State.ActiveBattleParty.Add(MarkedPartner);
	FGameXXKCardCombatUnit* RuntimeMarkedPartner = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Partner");
	});
	if (!TestNotNull(TEXT("the target-specific fixture retains its non-target marked partner"), RuntimeMarkedPartner))
	{
		return false;
	}
	TestEqual(TEXT("the target-specific fixture can mark the higher-health partner"),
		GameXXKCardRules::AddCombatStatus(*RuntimeMarkedPartner, EGameXXKCardStatus::Mark, 1), 1);
	State.CardRun.EnemyIntents.Reset();
	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the target-specific fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the target-specific fixture has a hunt-mark forecast"), State.CardRun.EnemyIntents.IsValidIndex(0)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent& Forecast = State.CardRun.EnemyIntents[0];
	TestEqual(TEXT("the target-specific forecast remains hunt mark"), Forecast.IntentDefinitionId, FName(TEXT("HuntMark")));
	TestEqual(TEXT("a marked non-target does not amplify the unmarked low-health actual target"), Forecast.Damage, 18);
	TestEqual(TEXT("the unmarked lower-health hero remains the locked target"), Forecast.SuggestedTargetUnitId, FName(TEXT("Player")));
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the target-specific hunt-mark action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	if (!TestEqual(TEXT("the target-specific hunt-mark action emits one direct hit"), IntentResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the unmarked actual target receives the unamplified requested damage"), IntentResults[0].RequestedDamage, 18);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRedtuskRageStrikeIntentTest,
	"GameXXK.Battle.EnemyIntentRules.RedtuskRageStrikeUsesSavedRage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRedtuskRageStrikeIntentTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the Redtusk RageStrike fixture begins an elite route battle"),
		InitializeRedtuskIntentFixture(State, Error, 929)))
	{
		return false;
	}

	FGameXXKCardCombatUnit* Redtusk = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Redtusk.P2");
	});
	FGameXXKEnemyBattleState* RedtuskState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.Redtusk.P2"));
	const FGameXXKEnemyDefinition* RedtuskDefinition = FGameXXKEnemyCatalog::Find(TEXT("Enemy.Ch2.RedtuskBoarKing"));
	if (!TestNotNull(TEXT("the Redtusk RageStrike fixture retains its catalog enemy"), Redtusk)
		|| !TestNotNull(TEXT("the Redtusk RageStrike fixture retains serializable enemy state"), RedtuskState)
		|| !TestNotNull(TEXT("the Redtusk RageStrike catalog definition exists"), RedtuskDefinition))
	{
		return false;
	}
	const FGameXXKEnemyIntentDefinition* RageStrikeDefinition = RedtuskDefinition->Intents.FindByPredicate([](const FGameXXKEnemyIntentDefinition& Candidate)
	{
		return Candidate.Id == TEXT("RageStrike");
	});
	if (!TestNotNull(TEXT("the Redtusk catalog contains RageStrike"), RageStrikeDefinition)
		|| !TestTrue(TEXT("RageStrike contains one direct-damage catalog effect"), RageStrikeDefinition->Effects.Num() == 1))
	{
		return false;
	}

	const FGameXXKEnemyIntentEffectDefinition& RageStrikeEffectDefinition = RageStrikeDefinition->Effects[0];
	const int32 BaseMagnitude = static_cast<int32>(FMath::Clamp<int64>(
		static_cast<int64>(RageStrikeEffectDefinition.FlatMagnitude)
			+ static_cast<int64>(Redtusk->Attack) * RageStrikeEffectDefinition.AttackPercent / 100,
		0,
		MAX_int32));
	TestEqual(TEXT("the Redtusk RageStrike fixture clamps six injected Rage stacks to five"),
		GameXXKCardRules::AddCombatStatus(*Redtusk, EGameXXKCardStatus::Rage, 6), 5);
	RedtuskState->IntentCursor = 2;
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;

	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the RageStrike fixture enters the enemy phase and rebuilds its forecast"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* Forecast = State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Candidate)
	{
		return Candidate.SourceUnitId == TEXT("Enemy.Redtusk.P2");
	});
	if (!TestNotNull(TEXT("the Redtusk RageStrike forecast remains visible"), Forecast)
		|| !TestTrue(TEXT("the Redtusk RageStrike forecast has one direct-damage packet"), Forecast->Effects.Num() == 1))
	{
		return false;
	}
	const int32 ExpectedMagnitude = BaseMagnitude + 5 * 20;
	TestEqual(TEXT("the saved rage stacks select RageStrike rather than a different catalog action"),
		Forecast->IntentDefinitionId, FName(TEXT("RageStrike")));
	TestEqual(TEXT("the RageStrike forecast adds exactly twenty per current rage to its catalog base"),
		Forecast->Effects[0].Magnitude, ExpectedMagnitude);
	TestEqual(TEXT("the RageStrike card face reports the same rage-amplified direct damage"), Forecast->Damage, ExpectedMagnitude);

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> DamageResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the rage-amplified RageStrike resolves from its saved forecast"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, DamageResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	if (!TestEqual(TEXT("the rage-amplified RageStrike resolves one direct damage packet"), DamageResults.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the resolved RageStrike retains its stable catalog identity"), ResolvedIntent.IntentDefinitionId, FName(TEXT("RageStrike")));
	TestEqual(TEXT("the resolved RageStrike uses the exact rage-amplified forecast magnitude"), DamageResults[0].RequestedDamage, ExpectedMagnitude);
	const FGameXXKCardCombatUnit* ResolvedRedtusk = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Redtusk.P2");
	});
	if (!TestNotNull(TEXT("the resolved Redtusk remains addressable after RageStrike"), ResolvedRedtusk))
	{
		return false;
	}
	TestEqual(TEXT("RageStrike does not consume the five-stack capped Rage combat status"),
		GameXXKCardRules::GetCombatStatusStacks(*ResolvedRedtusk, EGameXXKCardStatus::Rage), 5);

	return true;
}

namespace
{
	bool InitializeWhiteApeStatusGuardIntentFixture(
		FGameXXKRuntimeState& OutState,
		FString& OutError,
		const int32 NodeId)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}
		OutState.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
		OutState.ActiveBattleEnemies = {
			MakeEnemyIntentFixtureUnit(TEXT("Enemy.WhiteApe.StatusGuard"), TEXT("Enemy.Ch3.WhiteApe"), 1, 198, 21)};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = NodeId;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			NodeId,
			&OutError);
	}

	FGameXXKCardCombatUnit* FindWhiteApeStatusGuardIntentUnit(FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Enemy.WhiteApe.StatusGuard");
		});
	}

	void ReplaceWhiteApeStatusGuardIntentWithApplyStatus(
		FGameXXKCardEnemyIntent& InOutIntent,
		const EGameXXKCardStatus Status,
		const int32 StatusStacks)
	{
		InOutIntent.Effects.Reset();
		FGameXXKResolvedEnemyIntentEffect& StatusEffect = InOutIntent.Effects.AddDefaulted_GetRef();
		StatusEffect.Type = EGameXXKEnemyIntentEffectType::ApplyStatus;
		StatusEffect.TargetUnitIds = {TEXT("Enemy.WhiteApe.StatusGuard")};
		StatusEffect.Status = Status;
		StatusEffect.StatusStacks = StatusStacks;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardEnemyIntentRoundResetTest,
	"GameXXK.Battle.EnemyIntentRules.WhiteApeStatusGuardEnemyIntentAndCompletedPlayerTransitionReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardEnemyIntentRoundResetTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the White Ape status-guard intent fixture initializes"),
		InitializeWhiteApeStatusGuardIntentFixture(State, Error, 927)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the White Ape intent fixture initializes serializable catalog state"), WhiteApeState))
	{
		return false;
	}
	TestTrue(TEXT("the White Ape intent fixture begins with its status guard available"), WhiteApeState->bFirstStatusPassiveAvailable);

	FGameXXKCardDamageContext InitialPlayerStatusContext;
	InitialPlayerStatusContext.SourceUnitId = TEXT("Player");
	InitialPlayerStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& InitialPlayerStatus = InitialPlayerStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	InitialPlayerStatus.Status = EGameXXKCardStatus::Poison;
	InitialPlayerStatus.Stacks = 1;
	FGameXXKCardDamageResult InitialPlayerStatusResult;
	if (!TestTrue(TEXT("the player round can consume White Ape guard before its enemy phase"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			State.CardRun.ActiveBattle,
			InitialPlayerStatusContext,
			TEXT("Enemy.WhiteApe.StatusGuard"),
			1,
			InitialPlayerStatusResult,
			&Error)))
	{
		return false;
	}
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	TestFalse(TEXT("the first player-round status consumes White Ape guard before the enemy phase"),
		WhiteApeState && WhiteApeState->bFirstStatusPassiveAvailable);

	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the consumed White Ape guard fixture enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPlayerResults, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("ending the player phase enters the enemy phase"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	TestFalse(TEXT("ending the player phase does not refresh a consumed White Ape guard"),
		WhiteApeState && WhiteApeState->bFirstStatusPassiveAvailable);

	const TArray<uint8> BeforeRejectedCompletion = SerializeEnemyIntentStateForTest(State);
	TArray<FGameXXKCardDamageResult> RejectedCompletionResults;
	FGameXXKCardDamageResult& PreservedCompletionResult = RejectedCompletionResults.AddDefaulted_GetRef();
	PreservedCompletionResult.SourceUnitId = TEXT("WhiteApe.StatusGuard.CompletionResultMustRemainUnchanged");
	TestFalse(TEXT("an enemy phase with unresolved White Ape intent cannot complete"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, RejectedCompletionResults, &Error));
	TestEqual(TEXT("a rejected White Ape enemy-phase completion leaks no partial runtime reset"),
		SerializeEnemyIntentStateForTest(State), BeforeRejectedCompletion);
	TestEqual(TEXT("a rejected White Ape enemy-phase completion preserves caller output"),
		RejectedCompletionResults[0].SourceUnitId, FName(TEXT("WhiteApe.StatusGuard.CompletionResultMustRemainUnchanged")));

	if (!TestTrue(TEXT("the same player round retains one White Ape catalog intent to resolve"),
		State.CardRun.EnemyIntents.IsValidIndex(State.CardRun.NextEnemyIntentIndex)))
	{
		return false;
	}
	// Retain the valid catalog identity/cursor, but drive the generic resolved ApplyStatus packet path.
	ReplaceWhiteApeStatusGuardIntentWithApplyStatus(
		State.CardRun.EnemyIntents[State.CardRun.NextEnemyIntentIndex],
		EGameXXKCardStatus::Mark,
		1);
	FGameXXKCardEnemyIntent SameRoundResolvedIntent;
	TArray<FGameXXKCardDamageResult> SameRoundIntentResults;
	bool bSameRoundIntentsFinished = false;
	if (!TestTrue(TEXT("the same-round White Ape ApplyStatus intent resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			SameRoundResolvedIntent,
			SameRoundIntentResults,
			bSameRoundIntentsFinished,
			&Error)))
	{
		return false;
	}
	TestTrue(TEXT("the one-enemy same-round status intent finishes the enemy action list"), bSameRoundIntentsFinished);
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardIntentUnit(State.CardRun.ActiveBattle);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the same-round status intent retains White Ape"), WhiteApe)
		|| !TestNotNull(TEXT("the same-round status intent retains White Ape state"), WhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("the same-round enemy intent actually applies its Mark status"),
		GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Mark), 1);
	TestEqual(TEXT("an enemy intent in the already-consumed player round does not add White Ape armor"), WhiteApe->Armor, 0);
	TestFalse(TEXT("an enemy intent in the already-consumed player round keeps White Ape guard consumed"),
		WhiteApeState->bFirstStatusPassiveAvailable);

	TArray<FGameXXKCardDamageResult> CompletedEnemyPhaseResults;
	if (!TestTrue(TEXT("the fully resolved White Ape enemy phase completes into a player phase"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, CompletedEnemyPhaseResults, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("a successful White Ape enemy-phase completion transitions to player"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Player);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the completed White Ape enemy phase retains state"), WhiteApeState))
	{
		return false;
	}
	TestTrue(TEXT("only the successful completion that transitions to player refreshes White Ape guard"),
		WhiteApeState->bFirstStatusPassiveAvailable);

	TArray<FGameXXKCardDamageResult> NextEndPlayerResults;
	if (!TestTrue(TEXT("the refreshed White Ape guard fixture enters its next enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, NextEndPlayerResults, &Error)))
	{
		return false;
	}
	if (!TestTrue(TEXT("the refreshed White Ape guard fixture has a next catalog intent"),
		State.CardRun.EnemyIntents.IsValidIndex(State.CardRun.NextEnemyIntentIndex)))
	{
		return false;
	}
	ReplaceWhiteApeStatusGuardIntentWithApplyStatus(
		State.CardRun.EnemyIntents[State.CardRun.NextEnemyIntentIndex],
		EGameXXKCardStatus::Burn,
		1);
	FGameXXKCardEnemyIntent NextRoundResolvedIntent;
	TArray<FGameXXKCardDamageResult> NextRoundIntentResults;
	bool bNextRoundIntentsFinished = false;
	if (!TestTrue(TEXT("the refreshed-round White Ape ApplyStatus intent resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			NextRoundResolvedIntent,
			NextRoundIntentResults,
			bNextRoundIntentsFinished,
			&Error)))
	{
		return false;
	}
	WhiteApe = FindWhiteApeStatusGuardIntentUnit(State.CardRun.ActiveBattle);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the refreshed-round status intent retains White Ape"), WhiteApe)
		|| !TestNotNull(TEXT("the refreshed-round status intent retains White Ape state"), WhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("the refreshed-round enemy intent actually applies Burn to White Ape"),
		GameXXKCardRules::GetCombatStatusStacks(*WhiteApe, EGameXXKCardStatus::Burn), 1);
	TestEqual(TEXT("the first enemy-intent status after a completed player transition grants White Ape armor"), WhiteApe->Armor, 8);
	TestFalse(TEXT("the first enemy-intent status after a completed player transition consumes White Ape guard"),
		WhiteApeState->bFirstStatusPassiveAvailable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKWhiteApeStatusGuardPlayerStatusAfterCompletionTest,
	"GameXXK.Battle.EnemyIntentRules.WhiteApeStatusGuardPlayerStatusAfterCompletedEnemyPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKWhiteApeStatusGuardPlayerStatusAfterCompletionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the White Ape post-completion player-status fixture initializes"),
		InitializeWhiteApeStatusGuardIntentFixture(State, Error, 928)))
	{
		return false;
	}

	FGameXXKCardDamageContext FirstRoundStatusContext;
	FirstRoundStatusContext.SourceUnitId = TEXT("Player");
	FirstRoundStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& FirstRoundStatus = FirstRoundStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	FirstRoundStatus.Status = EGameXXKCardStatus::Poison;
	FirstRoundStatus.Stacks = 1;
	FGameXXKCardDamageResult FirstRoundStatusResult;
	if (!TestTrue(TEXT("the first player round applies White Ape's initial status"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			State.CardRun.ActiveBattle,
			FirstRoundStatusContext,
			TEXT("Enemy.WhiteApe.StatusGuard"),
			1,
			FirstRoundStatusResult,
			&Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* WhiteApe = FindWhiteApeStatusGuardIntentUnit(State.CardRun.ActiveBattle);
	FGameXXKEnemyBattleState* WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the first-round player status retains White Ape"), WhiteApe)
		|| !TestNotNull(TEXT("the first-round player status retains White Ape state"), WhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("the first player-round status grants White Ape armor before its enemy phase"), WhiteApe->Armor, 8);
	TestFalse(TEXT("the first player-round status consumes White Ape guard"), WhiteApeState->bFirstStatusPassiveAvailable);

	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the first player round reaches the White Ape enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPlayerResults, &Error)))
	{
		return false;
	}
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the normal White Ape intent resolves before its phase completes"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the one-enemy White Ape phase has no remaining intents"), bIntentsFinished))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> CompletedEnemyPhaseResults;
	if (!TestTrue(TEXT("the resolved White Ape enemy phase completes into the next player round"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, CompletedEnemyPhaseResults, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the completed White Ape enemy phase reaches player"), State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Player);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the completed White Ape phase retains state for the next player round"), WhiteApeState))
	{
		return false;
	}
	TestTrue(TEXT("the completed player transition refreshes White Ape guard for a direct player status"),
		WhiteApeState->bFirstStatusPassiveAvailable);

	FGameXXKCardDamageContext NextRoundStatusContext;
	NextRoundStatusContext.SourceUnitId = TEXT("Player");
	NextRoundStatusContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardStatusStack& NextRoundStatus = NextRoundStatusContext.OnHitStatuses.AddDefaulted_GetRef();
	NextRoundStatus.Status = EGameXXKCardStatus::Mark;
	NextRoundStatus.Stacks = 1;
	FGameXXKCardDamageResult NextRoundStatusResult;
	if (!TestTrue(TEXT("the next player round applies a second direct White Ape status"),
		GameXXKCardRules::ApplyPlayerCardDirectDamage(
			State.CardRun.ActiveBattle,
			NextRoundStatusContext,
			TEXT("Enemy.WhiteApe.StatusGuard"),
			1,
			NextRoundStatusResult,
			&Error)))
	{
		return false;
	}
	WhiteApe = FindWhiteApeStatusGuardIntentUnit(State.CardRun.ActiveBattle);
	WhiteApeState = State.CardRun.ActiveBattle.EnemyStates.Find(TEXT("Enemy.WhiteApe.StatusGuard"));
	if (!TestNotNull(TEXT("the next player-round status retains White Ape"), WhiteApe)
		|| !TestNotNull(TEXT("the next player-round status retains White Ape state"), WhiteApeState))
	{
		return false;
	}
	TestEqual(TEXT("the first direct status in the new player round grants fresh White Ape armor"), WhiteApe->Armor, 8);
	TestFalse(TEXT("the first direct status in the new player round consumes fresh White Ape guard"),
		WhiteApeState->bFirstStatusPassiveAvailable);
	return true;
}

namespace
{
	bool InitializeSpiralHornDeerCooldownFixture(
		FGameXXKRuntimeState& OutState,
		FString& OutError,
		const int32 NodeId,
		const bool bIncludeTiedAllies)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}
		OutState.ActiveBattleParty = {MakeEnemyIntentFixtureHero()};
		OutState.ActiveBattleEnemies = {
			MakeEnemyIntentFixtureUnit(TEXT("Enemy.SpiralHornDeer.P1"), TEXT("Enemy.Ch3.SpiralHornDeer"), 1, 210, 20)};
		if (bIncludeTiedAllies)
		{
			FGameXXKBattleRuntimeUnit EarlyTieAlly = MakeEnemyIntentFixtureUnit(TEXT("Enemy.DeerHeal.Early.P2"), TEXT("Enemy.Ch1.Rooster"), 2, 100, 8);
			EarlyTieAlly.HP = 40;
			FGameXXKBattleRuntimeUnit LateTieAlly = MakeEnemyIntentFixtureUnit(TEXT("Enemy.DeerHeal.Late.P3"), TEXT("Enemy.Ch1.Goat"), 3, 50, 7);
			LateTieAlly.HP = 20;
			OutState.ActiveBattleEnemies.Add(EarlyTieAlly);
			OutState.ActiveBattleEnemies.Add(LateTieAlly);
		}
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = NodeId;
		return FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Elite,
			EGameXXKCardTerrain::Plain,
			NodeId,
			&OutError);
	}

	FGameXXKCardCombatUnit* FindSpiralHornDeerUnit(FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Enemy.SpiralHornDeer.P1");
		});
	}

	FGameXXKEnemyBattleState* FindSpiralHornDeerState(FGameXXKCardBattleRuntime& Runtime)
	{
		return Runtime.EnemyStates.Find(TEXT("Enemy.SpiralHornDeer.P1"));
	}

	const FGameXXKCardEnemyIntent* FindSpiralHornDeerIntent(const FGameXXKRuntimeState& State)
	{
		return State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Candidate)
		{
			return Candidate.SourceUnitId == TEXT("Enemy.SpiralHornDeer.P1");
		});
	}

	void ForceSpiralHornDeerSpringHealCursor(FGameXXKRuntimeState& InOutState)
	{
		if (FGameXXKEnemyBattleState* DeerState = FindSpiralHornDeerState(InOutState.CardRun.ActiveBattle))
		{
			DeerState->IntentCursor = 3;
		}
		InOutState.CardRun.EnemyIntents.Reset();
		InOutState.CardRun.NextEnemyIntentIndex = 0;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSpiralHornDeerSpringHealForecastLockTest,
	"GameXXK.Battle.EnemyIntentRules.SpiralHornDeerSpringHealLocksLowestLivingEnemy",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSpiralHornDeerSpringHealForecastLockTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the tied-target Spiral Horn Deer fixture initializes"),
		InitializeSpiralHornDeerCooldownFixture(State, Error, 963, true)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* DeerState = FindSpiralHornDeerState(State.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the tied-target fixture retains Deer persisted state"), DeerState))
	{
		return false;
	}
	ForceSpiralHornDeerSpringHealCursor(State);

	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the tied-target fixture enters the Spring Heal enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPlayerResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* Forecast = FindSpiralHornDeerIntent(State);
	if (!TestNotNull(TEXT("the tied-target fixture exposes Deer forecast"), Forecast)
		|| !TestEqual(TEXT("the forced Deer cursor forecasts Spring Heal"), Forecast->IntentDefinitionId, FName(TEXT("SpringHeal")))
		|| !TestEqual(TEXT("the equal-health percentage tie selects the earlier stable enemy"), Forecast->SuggestedTargetUnitId, FName(TEXT("Enemy.DeerHeal.Early.P2")))
		|| !TestEqual(TEXT("the Spring Heal forecast carries exactly one locked effect"), Forecast->Effects.Num(), 1)
		|| !TestEqual(TEXT("the locked Spring Heal effect carries one target ID"), Forecast->Effects[0].TargetUnitIds.Num(), 1))
	{
		return false;
	}
	TestEqual(TEXT("the locked Spring Heal effect retains the earlier stable target"),
		Forecast->Effects[0].TargetUnitIds[0], FName(TEXT("Enemy.DeerHeal.Early.P2")));

	FGameXXKCardCombatUnit* EarlyTieAlly = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.DeerHeal.Early.P2");
	});
	FGameXXKCardCombatUnit* LateTieAlly = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.DeerHeal.Late.P3");
	});
	if (!TestNotNull(TEXT("the locked early ally remains addressable"), EarlyTieAlly)
		|| !TestNotNull(TEXT("the later tied ally remains addressable"), LateTieAlly))
	{
		return false;
	}
	EarlyTieAlly->HP = 60;
	LateTieAlly->HP = 10;

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the locked Spring Heal resolves against its forecast target"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the resolved Deer action preserves Spring Heal identity"), ResolvedIntent.IntentDefinitionId, FName(TEXT("SpringHeal")));
	EarlyTieAlly = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.DeerHeal.Early.P2");
	});
	LateTieAlly = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.DeerHeal.Late.P3");
	});
	TestEqual(TEXT("the locked earlier target receives the catalog heal after health ordering changes"), EarlyTieAlly ? EarlyTieAlly->HP : 0, 72);
	TestEqual(TEXT("the newly lower later ally does not replace the locked Spring Heal target"), LateTieAlly ? LateTieAlly->HP : 0, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSpiralHornDeerSpringHealCooldownLifecycleTest,
	"GameXXK.Battle.EnemyIntentRules.SpiralHornDeerSpringHealCooldownLifecyclePersists",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSpiralHornDeerSpringHealCooldownLifecycleTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the full-health Spiral Horn Deer fixture initializes"),
		InitializeSpiralHornDeerCooldownFixture(State, Error, 964, false)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Deer = FindSpiralHornDeerUnit(State.CardRun.ActiveBattle);
	FGameXXKEnemyBattleState* DeerState = FindSpiralHornDeerState(State.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the full-health fixture retains Deer"), Deer)
		|| !TestNotNull(TEXT("the full-health fixture retains Deer state"), DeerState))
	{
		return false;
	}
	ForceSpiralHornDeerSpringHealCursor(State);

	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the full-health Deer fixture enters the Spring Heal enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPlayerResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* SpringHealForecast = FindSpiralHornDeerIntent(State);
	if (!TestNotNull(TEXT("the full-health fixture exposes Spring Heal"), SpringHealForecast)
		|| !TestEqual(TEXT("the full-health fixture selects Spring Heal"), SpringHealForecast->IntentDefinitionId, FName(TEXT("SpringHeal")))
		|| !TestEqual(TEXT("a full-health Deer still locks itself as the valid heal target"), SpringHealForecast->SuggestedTargetUnitId, FName(TEXT("Enemy.SpiralHornDeer.P1"))))
	{
		return false;
	}
	const int32 FullHealthBefore = Deer->HP;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the full-health Spring Heal card still resolves successfully"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the one-Deer Spring Heal exhausts its enemy action list"), bIntentsFinished))
	{
		return false;
	}
	Deer = FindSpiralHornDeerUnit(State.CardRun.ActiveBattle);
	DeerState = FindSpiralHornDeerState(State.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the resolved full-health Spring Heal retains Deer"), Deer)
		|| !TestNotNull(TEXT("the resolved full-health Spring Heal retains Deer state"), DeerState))
	{
		return false;
	}
	TestEqual(TEXT("a full-health Spring Heal remains a no-op heal rather than a rejected action"), Deer->HP, FullHealthBefore);
	TestEqual(TEXT("a successfully resolved full-health Spring Heal starts its two-phase cooldown"), DeerState->HealingCooldownRounds, 2);
	TestEqual(TEXT("a resolved Spring Heal preserves the fixed rotation's next cursor"), DeerState->IntentCursor, 0);

	const TArray<uint8> SavedStateBytes = SerializeEnemyIntentStateForTest(State);
	FGameXXKRuntimeState ReloadedState;
	if (!TestTrue(TEXT("the Spring Heal cooldown state survives SaveGame reload"),
		DeserializeEnemyIntentStateForTest(SavedStateBytes, ReloadedState)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the reloaded Spring Heal state retains Deer"), DeerState))
	{
		return false;
	}
	TestEqual(TEXT("the reloaded Spring Heal state retains its two completed-phase cooldown"), DeerState->HealingCooldownRounds, 2);

	// The phase which resolved Spring Heal establishes the cooldown; it is not one
	// of its two future enemy phases. Force the next forecast onto Spring Heal's
	// catalog slot so Horn proves the positive-cooldown skip rather than rotation.
	ForceSpiralHornDeerSpringHealCursor(ReloadedState);
	TArray<FGameXXKCardDamageResult> FirstCompletionResults;
	if (!TestTrue(TEXT("the Spring Heal originating enemy phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(ReloadedState, FirstCompletionResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	const FGameXXKCardEnemyIntent* FixedRotationForecast = FindSpiralHornDeerIntent(ReloadedState);
	if (!TestNotNull(TEXT("the originating phase retains Deer cooldown state"), DeerState)
		|| !TestNotNull(TEXT("the originating phase rebuilds Deer forecast"), FixedRotationForecast))
	{
		return false;
	}
	TestEqual(TEXT("completing Spring Heal's originating enemy phase does not consume its new cooldown"), DeerState->HealingCooldownRounds, 2);
	TestEqual(TEXT("the immediate forecast skips Spring Heal while its new cooldown is positive"), FixedRotationForecast->IntentDefinitionId, FName(TEXT("Horn")));

	TArray<FGameXXKCardDamageResult> CooldownEndPlayerResults;
	if (!TestTrue(TEXT("the first future positive-cooldown Deer phase begins"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(ReloadedState, CooldownEndPlayerResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	const FGameXXKCardEnemyIntent* PositiveCooldownForecast = FindSpiralHornDeerIntent(ReloadedState);
	if (!TestNotNull(TEXT("ending the player phase retains positive Deer cooldown state"), DeerState)
		|| !TestNotNull(TEXT("positive Deer cooldown keeps a forecast visible"), PositiveCooldownForecast))
	{
		return false;
	}
	TestEqual(TEXT("ending the player phase does not decrement Spring Heal cooldown"), DeerState->HealingCooldownRounds, 2);
	TestEqual(TEXT("Spring Heal is ineligible while its cooldown remains positive"), PositiveCooldownForecast->IntentDefinitionId, FName(TEXT("Horn")));

	FGameXXKCardEnemyIntent CooldownResolvedIntent;
	TArray<FGameXXKCardDamageResult> CooldownIntentResults;
	bIntentsFinished = false;
	if (!TestTrue(TEXT("the positive-cooldown replacement catalog action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(ReloadedState, CooldownResolvedIntent, CooldownIntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the replacement one-Deer action exhausts the phase"), bIntentsFinished))
	{
		return false;
	}
	TestEqual(TEXT("the positive-cooldown Deer resolves Horn instead of Spring Heal"), CooldownResolvedIntent.IntentDefinitionId, FName(TEXT("Horn")));
	TArray<FGameXXKCardDamageResult> FirstFutureCompletionResults;
	if (!TestTrue(TEXT("the first future post-Spring-Heal enemy phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(ReloadedState, FirstFutureCompletionResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the first future completed phase retains Deer cooldown state"), DeerState))
	{
		return false;
	}
	TestEqual(TEXT("the first future completed enemy phase decrements Spring Heal cooldown from two to one"), DeerState->HealingCooldownRounds, 1);

	ForceSpiralHornDeerSpringHealCursor(ReloadedState);
	TArray<FGameXXKCardDamageResult> SecondCooldownEndPlayerResults;
	if (!TestTrue(TEXT("the second future positive-cooldown Deer phase begins"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(ReloadedState, SecondCooldownEndPlayerResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	const FGameXXKCardEnemyIntent* SecondPositiveCooldownForecast = FindSpiralHornDeerIntent(ReloadedState);
	if (!TestNotNull(TEXT("the second future phase retains Deer cooldown state"), DeerState)
		|| !TestNotNull(TEXT("the second future phase keeps a forecast visible"), SecondPositiveCooldownForecast))
	{
		return false;
	}
	TestEqual(TEXT("the second future player-phase end retains the one-phase Spring Heal cooldown"), DeerState->HealingCooldownRounds, 1);
	TestEqual(TEXT("Spring Heal remains ineligible before its second future phase completes"), SecondPositiveCooldownForecast->IntentDefinitionId, FName(TEXT("Horn")));

	FGameXXKCardEnemyIntent SecondCooldownResolvedIntent;
	TArray<FGameXXKCardDamageResult> SecondCooldownIntentResults;
	bIntentsFinished = false;
	if (!TestTrue(TEXT("the second positive-cooldown replacement catalog action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(ReloadedState, SecondCooldownResolvedIntent, SecondCooldownIntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the second replacement one-Deer action exhausts the phase"), bIntentsFinished))
	{
		return false;
	}
	TestEqual(TEXT("the second positive-cooldown Deer also resolves Horn instead of Spring Heal"), SecondCooldownResolvedIntent.IntentDefinitionId, FName(TEXT("Horn")));
	TArray<FGameXXKCardDamageResult> SecondFutureCompletionResults;
	if (!TestTrue(TEXT("the second future post-Spring-Heal enemy phase completes"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(ReloadedState, SecondFutureCompletionResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(ReloadedState.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the second future completed phase retains Deer cooldown state"), DeerState))
	{
		return false;
	}
	TestEqual(TEXT("the second future completed enemy phase decrements Spring Heal cooldown from one to zero"), DeerState->HealingCooldownRounds, 0);

	ForceSpiralHornDeerSpringHealCursor(ReloadedState);
	TArray<FGameXXKCardDamageResult> ReadyEndPlayerResults;
	if (!TestTrue(TEXT("the exhausted-cooldown Deer fixture enters a new enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(ReloadedState, ReadyEndPlayerResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* ReadyForecast = FindSpiralHornDeerIntent(ReloadedState);
	if (!TestNotNull(TEXT("the exhausted-cooldown Deer forecast remains visible"), ReadyForecast))
	{
		return false;
	}
	TestEqual(TEXT("Spring Heal becomes eligible again only after both completed enemy phases"), ReadyForecast->IntentDefinitionId, FName(TEXT("SpringHeal")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSpiralHornDeerSpringHealAtomicityTest,
	"GameXXK.Battle.EnemyIntentRules.SpiralHornDeerSpringHealCooldownFailuresAreAtomic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSpiralHornDeerSpringHealAtomicityTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState CompletionFailureState;
	if (!TestTrue(TEXT("the failed-completion Deer fixture initializes"),
		InitializeSpiralHornDeerCooldownFixture(CompletionFailureState, Error, 965, false)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* DeerState = FindSpiralHornDeerState(CompletionFailureState.CardRun.ActiveBattle);
	if (!TestNotNull(TEXT("the failed-completion fixture retains Deer state"), DeerState))
	{
		return false;
	}
	DeerState->HealingCooldownRounds = 2;
	ForceSpiralHornDeerSpringHealCursor(CompletionFailureState);
	TArray<FGameXXKCardDamageResult> EndPlayerResults;
	if (!TestTrue(TEXT("the failed-completion Deer fixture enters an enemy phase with a pending action"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(CompletionFailureState, EndPlayerResults, &Error)))
	{
		return false;
	}
	const TArray<uint8> BeforeRejectedCompletion = SerializeEnemyIntentStateForTest(CompletionFailureState);
	TArray<FGameXXKCardDamageResult> RejectedCompletionResults;
	FGameXXKCardDamageResult& PreservedCompletionResult = RejectedCompletionResults.AddDefaulted_GetRef();
	PreservedCompletionResult.SourceUnitId = TEXT("SpiralHornDeer.Cooldown.CompletionOutputMustRemainUnchanged");
	TestFalse(TEXT("a Deer phase with a pending intent cannot complete and decrement cooldown"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(CompletionFailureState, RejectedCompletionResults, &Error));
	TestEqual(TEXT("a rejected Deer completion leaks no cooldown, cursor, or target mutation"),
		SerializeEnemyIntentStateForTest(CompletionFailureState), BeforeRejectedCompletion);
	TestEqual(TEXT("a rejected Deer completion preserves caller output"),
		RejectedCompletionResults[0].SourceUnitId, FName(TEXT("SpiralHornDeer.Cooldown.CompletionOutputMustRemainUnchanged")));

	FGameXXKRuntimeState DefinitionMismatchState;
	if (!TestTrue(TEXT("the definition-mismatch Deer fixture initializes"),
		InitializeSpiralHornDeerCooldownFixture(DefinitionMismatchState, Error, 966, false)))
	{
		return false;
	}
	ForceSpiralHornDeerSpringHealCursor(DefinitionMismatchState);
	if (!TestTrue(TEXT("the definition-mismatch fixture enters its Spring Heal enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(DefinitionMismatchState, EndPlayerResults, &Error)))
	{
		return false;
	}
	DeerState = FindSpiralHornDeerState(DefinitionMismatchState.CardRun.ActiveBattle);
	const FGameXXKCardEnemyIntent* BeforeMismatchIntent = FindSpiralHornDeerIntent(DefinitionMismatchState);
	if (!TestNotNull(TEXT("the definition-mismatch fixture retains Deer state"), DeerState)
		|| !TestNotNull(TEXT("the definition-mismatch fixture retains locked Spring Heal forecast"), BeforeMismatchIntent)
		|| !TestEqual(TEXT("the definition-mismatch fixture builds Spring Heal before its state is corrupted"), BeforeMismatchIntent->IntentDefinitionId, FName(TEXT("SpringHeal"))))
	{
		return false;
	}
	const FName LockedTargetBeforeMismatch = BeforeMismatchIntent->SuggestedTargetUnitId;
	const int32 CursorBeforeMismatch = DeerState->IntentCursor;
	const int32 CooldownBeforeMismatch = DeerState->HealingCooldownRounds;
	DeerState->DefinitionId = TEXT("Enemy.Ch1.Goat");
	const TArray<uint8> BeforeRejectedMismatch = SerializeEnemyIntentStateForTest(DefinitionMismatchState);
	FGameXXKCardEnemyIntent RejectedResolvedIntent;
	TArray<FGameXXKCardDamageResult> RejectedIntentResults;
	bool bRejectedIntentsFinished = true;
	TestFalse(TEXT("a mismatched Deer definition rejects the saved Spring Heal action atomically"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			DefinitionMismatchState,
			RejectedResolvedIntent,
			RejectedIntentResults,
			bRejectedIntentsFinished,
			&Error));
	TestEqual(TEXT("a rejected Deer definition leaks no persisted cooldown, cursor, or target mutation"),
		SerializeEnemyIntentStateForTest(DefinitionMismatchState), BeforeRejectedMismatch);
	DeerState = FindSpiralHornDeerState(DefinitionMismatchState.CardRun.ActiveBattle);
	const FGameXXKCardEnemyIntent* AfterMismatchIntent = FindSpiralHornDeerIntent(DefinitionMismatchState);
	if (!TestNotNull(TEXT("the rejected definition leaves Deer state addressable"), DeerState)
		|| !TestNotNull(TEXT("the rejected definition leaves its saved forecast addressable"), AfterMismatchIntent))
	{
		return false;
	}
	TestEqual(TEXT("the rejected definition leaves Deer cooldown unchanged"), DeerState->HealingCooldownRounds, CooldownBeforeMismatch);
	TestEqual(TEXT("the rejected definition leaves Deer cursor unchanged"), DeerState->IntentCursor, CursorBeforeMismatch);
	TestEqual(TEXT("the rejected definition leaves the locked Spring Heal target unchanged"), AfterMismatchIntent->SuggestedTargetUnitId, LockedTargetBeforeMismatch);
	TestTrue(TEXT("a rejected Deer definition emits no partial intent damage output"), RejectedIntentResults.IsEmpty());
	TestFalse(TEXT("a rejected Deer definition resets the completion flag"), bRejectedIntentsFinished);
	return true;
}

namespace
{
	struct FBossPhaseFixtureSpec
	{
		FName UnitId;
		FName DefinitionId;
		const TCHAR* Label = TEXT("");
	};

	const TArray<FBossPhaseFixtureSpec>& GetBossPhaseFixtureSpecs()
	{
		static const TArray<FBossPhaseFixtureSpec> Specs = {
			{TEXT("Enemy.MoneyRat.P2"), TEXT("Enemy.Ch1.MoneyRat"), TEXT("Money Rat")},
			{TEXT("Enemy.BlackBear.P2"), TEXT("Enemy.Ch2.BlackBear"), TEXT("Black Bear")},
			{TEXT("Enemy.Tiger.P2"), TEXT("Enemy.Ch3.Tiger"), TEXT("Tiger")}};
		return Specs;
	}

	bool InitializeBossPhaseFixture(
		FGameXXKRuntimeState& OutState,
		const FBossPhaseFixtureSpec& Spec,
		const int32 NodeId,
		FString& OutError)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			return false;
		}

		FGameXXKBattleRuntimeUnit Hero = MakeEnemyIntentFixtureHero();
		Hero.Attack = 70;
		OutState.ActiveBattleParty = {Hero};
		OutState.ActiveBattleEnemies = {
			MakeEnemyIntentFixtureUnit(Spec.UnitId, Spec.DefinitionId, 2, 100, 10)};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = NodeId;
		if (!FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			NodeId,
			&OutError))
		{
			return false;
		}

		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 5; ++Index)
		{
			Cards.Add(MakeEnemyIntentFixtureCard(
				FName(*FString::Printf(TEXT("BossPhase.%s.Card.%d"), *Spec.UnitId.ToString(), Index)),
				TEXT("Hero.QingFengYiShi"),
				Index + 1));
		}
		if (!GameXXKCardRules::InitializeBattleDeck(OutState.CardRun.ActiveBattle.Deck, Cards, NodeId, &OutError))
		{
			return false;
		}
		OutState.CardRun.ActiveBattle.Deck.Hand = Cards;
		OutState.CardRun.ActiveBattle.Deck.DrawPile.Reset();
		OutState.CardRun.ActiveBattle.Deck.DiscardPile.Reset();
		OutState.CardRun.ActiveBattle.Deck.SharedEnergy = 3;
		return GameXXKCardRules::ValidateCardBattleRuntime(OutState.CardRun.ActiveBattle, &OutError);
	}

	FGameXXKCardCombatUnit* FindBossPhaseUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	FGameXXKEnemyBattleState* FindBossPhaseState(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.EnemyStates.Find(UnitId);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBossPhaseCardPacketTransitionTest,
	"GameXXK.Battle.EnemyIntentRules.BossPhase.CardPacketCrossesHalf",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBossPhaseCardPacketTransitionTest::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < GetBossPhaseFixtureSpecs().Num(); ++Index)
	{
		const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[Index];
		FGameXXKRuntimeState State;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("%s card-packet fixture initializes: %s"), Spec.Label, *Error),
			InitializeBossPhaseFixture(State, Spec, 970 + Index, Error)))
		{
			continue;
		}

		FGameXXKEnemyBattleState* EnemyState = FindBossPhaseState(State, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("%s fixture creates saved enemy phase state"), Spec.Label), EnemyState))
		{
			continue;
		}
		TestFalse(FString::Printf(TEXT("%s starts before phase two"), Spec.Label), EnemyState->bPhaseTwo);
		FGameXXKCardCombatUnit* PrePacketBoss = FindBossPhaseUnit(State, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("%s card-packet fixture exposes its boss before the threshold crossing"), Spec.Label), PrePacketBoss))
		{
			continue;
		}
		// The normal fixture's player attack intentionally stays representative.  Set only the
		// current HP so this one card is guaranteed to cross the inclusive 50% phase boundary
		// without defeating the boss, regardless of its catalog defense/passive reduction.
		PrePacketBoss->HP = 51;

		const FName CardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		FGameXXKCardPlayResult CardResult;
		if (!TestTrue(FString::Printf(TEXT("%s direct player card resolves as one complete packet: %s"), Spec.Label, *Error),
			FGameXXKCardBattleAdapter::ResolveCardPlay(State, CardInstanceId, Spec.UnitId, CardResult, &Error)))
		{
			continue;
		}

		const FGameXXKCardCombatUnit* Boss = FindBossPhaseUnit(State, Spec.UnitId);
		TestTrue(FString::Printf(TEXT("%s remains living at or below the fifty-percent threshold after the card packet"), Spec.Label),
			Boss && Boss->bLiving && static_cast<int64>(Boss->HP) * 2 <= Boss->MaxHP);
		EnemyState = FindBossPhaseState(State, Spec.UnitId);
		TestTrue(FString::Printf(TEXT("%s enters phase two only after the complete player damage packet"), Spec.Label),
			EnemyState && EnemyState->bPhaseTwo);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMoneyRatRoundStartWealthTest,
	"GameXXK.Battle.EnemyIntentRules.MoneyRat.RoundStartWealthTracksPhase",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMoneyRatRoundStartWealthTest::RunTest(const FString& Parameters)
{
	const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[0];
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Money Rat round-start fixture initializes"),
		InitializeBossPhaseFixture(State, Spec, 973, Error)))
	{
		return false;
	}

	FGameXXKCardCombatUnit* MoneyRat = FindBossPhaseUnit(State, Spec.UnitId);
	if (!TestNotNull(TEXT("the Money Rat exists for round-start wealth"), MoneyRat))
	{
		return false;
	}
	TestEqual(TEXT("Money Rat receives one Wealth before its first forecast"),
		GameXXKCardRules::GetCombatStatusStacks(*MoneyRat, EGameXXKCardStatus::Wealth),
		1);

	MoneyRat->HP = 51;
	const FName CardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult CardResult;
	if (!TestTrue(TEXT("the Money Rat crosses into phase two through a real player card"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(State, CardInstanceId, Spec.UnitId, CardResult, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the real player card enters Money Rat phase two"),
		FindBossPhaseState(State, Spec.UnitId) && FindBossPhaseState(State, Spec.UnitId)->bPhaseTwo);

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the phase-two Money Rat player phase ends"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error))
		|| !TestTrue(TEXT("the phase-two Money Rat resolves its locked first intent"),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the phase-two Money Rat finishes its enemy phase"),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}

	MoneyRat = FindBossPhaseUnit(State, Spec.UnitId);
	TestEqual(TEXT("phase two changes the next round-start Wealth gain from one to two"),
		MoneyRat ? GameXXKCardRules::GetCombatStatusStacks(*MoneyRat, EGameXXKCardStatus::Wealth) : INDEX_NONE,
		3);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMoneyRatCoinCrashScalingTest,
	"GameXXK.Battle.EnemyIntentRules.MoneyRat.CoinCrashLocksWealthAndPhaseDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMoneyRatCoinCrashScalingTest::RunTest(const FString& Parameters)
{
	const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[0];
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Money Rat coin-crash fixture initializes"),
		InitializeBossPhaseFixture(State, Spec, 974, Error)))
	{
		return false;
	}

	FGameXXKCardCombatUnit* MoneyRat = FindBossPhaseUnit(State, Spec.UnitId);
	if (!TestNotNull(TEXT("the Money Rat exists for coin-crash scaling"), MoneyRat))
	{
		return false;
	}
	MoneyRat->HP = 51;
	const FName CardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult CardResult;
	if (!TestTrue(TEXT("a real player card enters Money Rat phase two before forecasting Coin Crash"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(State, CardInstanceId, Spec.UnitId, CardResult, &Error)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* EnemyState = FindBossPhaseState(State, Spec.UnitId);
	if (!TestTrue(TEXT("the Money Rat is saved in phase two before forecasting Coin Crash"), EnemyState && EnemyState->bPhaseTwo))
	{
		return false;
	}

	MoneyRat = FindBossPhaseUnit(State, Spec.UnitId);
	GameXXKCardRules::ConsumeCombatStatus(*MoneyRat, EGameXXKCardStatus::Wealth, MAX_int32);
	TestEqual(TEXT("the controlled coin-crash fixture receives exactly three Wealth stacks"),
		GameXXKCardRules::AddCombatStatus(*MoneyRat, EGameXXKCardStatus::Wealth, 3),
		3);
	EnemyState->IntentCursor = 5;
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;

	TArray<FGameXXKCardDamageResult> PhaseResults;
	TestTrue(TEXT("the controlled Coin Crash forecast enters the enemy phase"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error));
	const FGameXXKCardEnemyIntent* CoinCrashForecast = State.CardRun.EnemyIntents.FindByPredicate([](const FGameXXKCardEnemyIntent& Intent)
	{
		return Intent.IntentDefinitionId == TEXT("CoinCrash");
	});
	TestNotNull(TEXT("the controlled enemy forecast is Coin Crash"), CoinCrashForecast);
	const FGameXXKResolvedEnemyIntentEffect* ForecastDamage = CoinCrashForecast
		? CoinCrashForecast->Effects.FindByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
		})
		: nullptr;
	TestNotNull(TEXT("Coin Crash forecast exposes its direct-damage packet"), ForecastDamage);
	MoneyRat = FindBossPhaseUnit(State, Spec.UnitId);
	const int32 ExpectedCoinCrashDamage = static_cast<int32>(
		(static_cast<int64>(MoneyRat ? MoneyRat->Attack : 0) + 15 * 3) * 125 / 100);
	TestEqual(TEXT("Coin Crash locks its Wealth bonus and phase-two direct-damage multiplier in the forecast"),
		ForecastDamage ? ForecastDamage->Magnitude : INDEX_NONE,
		ExpectedCoinCrashDamage);

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	TestTrue(TEXT("the locked Coin Crash resolves through the shared enemy packet path"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error));
	const FGameXXKCardDamageResult* CoinCrashDamageResult = IntentResults.FindByPredicate([&Spec](const FGameXXKCardDamageResult& Result)
	{
		return Result.SourceUnitId == Spec.UnitId;
	});
	TestNotNull(TEXT("Coin Crash produces one shared-rule damage record"), CoinCrashDamageResult);
	TestEqual(TEXT("Coin Crash resolution uses the exact saved forecast damage"),
		CoinCrashDamageResult ? CoinCrashDamageResult->RequestedDamage : INDEX_NONE,
		ExpectedCoinCrashDamage);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBlackBearPhaseStatsTest,
	"GameXXK.Battle.EnemyIntentRules.BlackBear.PhaseStatsApplyOnce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBlackBearPhaseStatsTest::RunTest(const FString& Parameters)
{
	const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[1];
	FGameXXKRuntimeState State;
	FString Error;
	if (!TestTrue(TEXT("the Black Bear phase-stat fixture initializes"),
		InitializeBossPhaseFixture(State, Spec, 975, Error)))
	{
		return false;
	}

	FGameXXKCardCombatUnit* BlackBear = FindBossPhaseUnit(State, Spec.UnitId);
	if (!TestNotNull(TEXT("the Black Bear exists for phase-stat testing"), BlackBear))
	{
		return false;
	}
	BlackBear->Attack = 20;
	BlackBear->Defense = 12;
	BlackBear->HP = 51;
	const FName CardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult CardResult;
	if (!TestTrue(TEXT("a real player card crosses the Black Bear phase threshold"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(State, CardInstanceId, Spec.UnitId, CardResult, &Error)))
	{
		return false;
	}

	BlackBear = FindBossPhaseUnit(State, Spec.UnitId);
	TestTrue(TEXT("the Black Bear enters its saved second phase"),
		FindBossPhaseState(State, Spec.UnitId) && FindBossPhaseState(State, Spec.UnitId)->bPhaseTwo);
	TestEqual(TEXT("Black Bear phase two raises its base attack by thirty percent exactly once"),
		BlackBear ? BlackBear->Attack : INDEX_NONE,
		26);
	TestEqual(TEXT("Black Bear phase two lowers its base defense by twenty-five percent exactly once"),
		BlackBear ? BlackBear->Defense : INDEX_NONE,
		9);

	const int32 AttackAfterFirstPhaseEntry = BlackBear ? BlackBear->Attack : INDEX_NONE;
	const int32 DefenseAfterFirstPhaseEntry = BlackBear ? BlackBear->Defense : INDEX_NONE;
	FGameXXKCardPlayResult FollowUpResult;
	if (!State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
	{
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			State,
			State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId,
			Spec.UnitId,
			FollowUpResult,
			&Error);
	}
	BlackBear = FindBossPhaseUnit(State, Spec.UnitId);
	TestEqual(TEXT("a later player packet cannot stack the Black Bear phase attack bonus"),
		BlackBear ? BlackBear->Attack : INDEX_NONE,
		AttackAfterFirstPhaseEntry);
	TestEqual(TEXT("a later player packet cannot stack the Black Bear phase defense reduction"),
		BlackBear ? BlackBear->Defense : INDEX_NONE,
		DefenseAfterFirstPhaseEntry);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBlackBearPhaseExtraHitTest,
	"GameXXK.Battle.EnemyIntentRules.BlackBear.PhasePounceAndRendGainOneHit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBlackBearPhaseExtraHitTest::RunTest(const FString& Parameters)
{
	struct FBlackBearIntentExpectation
	{
		int32 Cursor = 0;
		FName IntentId = NAME_None;
		int32 ExpectedHitCount = 1;
	};
	const FBlackBearIntentExpectation Expectations[] = {
		{0, TEXT("Sweep"), 1},
		{1, TEXT("Pounce"), 2},
		{3, TEXT("Rend"), 2}};
	const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[1];

	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expectations); ++Index)
	{
		const FBlackBearIntentExpectation& Expectation = Expectations[Index];
		FGameXXKRuntimeState State;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("Black Bear %s phase-hit fixture initializes: %s"), *Expectation.IntentId.ToString(), *Error),
			InitializeBossPhaseFixture(State, Spec, 976 + Index, Error)))
		{
			continue;
		}

		FGameXXKCardCombatUnit* BlackBear = FindBossPhaseUnit(State, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("Black Bear %s fixture exposes its boss"), *Expectation.IntentId.ToString()), BlackBear))
		{
			continue;
		}
		BlackBear->HP = 51;
		FGameXXKCardPlayResult CardResult;
		if (!TestTrue(FString::Printf(TEXT("Black Bear %s fixture crosses phase two through a card packet: %s"), *Expectation.IntentId.ToString(), *Error),
			FGameXXKCardBattleAdapter::ResolveCardPlay(
				State,
				State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId,
				Spec.UnitId,
				CardResult,
				&Error)))
		{
			continue;
		}

		FGameXXKEnemyBattleState* EnemyState = FindBossPhaseState(State, Spec.UnitId);
		BlackBear = FindBossPhaseUnit(State, Spec.UnitId);
		if (!TestTrue(FString::Printf(TEXT("Black Bear %s fixture enters phase two"), *Expectation.IntentId.ToString()),
			EnemyState && EnemyState->bPhaseTwo)
			|| !TestNotNull(FString::Printf(TEXT("Black Bear %s survives for its forecast"), *Expectation.IntentId.ToString()), BlackBear))
		{
			continue;
		}

		// Keep the real resolver path safe for each isolated expectation; the forecast's hit count
		// must come from the catalog intent, not from this test-only damage magnitude.
		BlackBear->Attack = 10;
		for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				Unit.MaxHP = 1000;
				Unit.HP = 1000;
			}
		}
		EnemyState->IntentCursor = Expectation.Cursor;
		// ResolveCardPlay refreshes the now-phase-two forecast immediately.  This fixture intentionally
		// selects a later catalog action, so discard that old forecast and let EndPlayer rebuild the
		// exact cursor-selected card through the normal recovery path.
		State.CardRun.EnemyIntents.Reset();
		State.CardRun.NextEnemyIntentIndex = 0;

		TArray<FGameXXKCardDamageResult> EndPlayerResults;
		if (!TestTrue(FString::Printf(TEXT("Black Bear %s forecast enters the real enemy phase: %s"), *Expectation.IntentId.ToString(), *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, EndPlayerResults, &Error)))
		{
			continue;
		}
		const FGameXXKCardEnemyIntent* Forecast = State.CardRun.EnemyIntents.FindByPredicate([&Spec](const FGameXXKCardEnemyIntent& Candidate)
		{
			return Candidate.SourceUnitId == Spec.UnitId;
		});
		if (!TestNotNull(FString::Printf(TEXT("Black Bear %s forecast remains visible"), *Expectation.IntentId.ToString()), Forecast))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("Black Bear phase forecast keeps the %s intent identity"), *Expectation.IntentId.ToString()),
			Forecast->IntentDefinitionId,
			Expectation.IntentId);
		const FGameXXKResolvedEnemyIntentEffect* DirectEffect = Forecast->Effects.FindByPredicate([](const FGameXXKResolvedEnemyIntentEffect& Effect)
		{
			return Effect.Type == EGameXXKEnemyIntentEffectType::DirectDamage;
		});
		if (!TestNotNull(FString::Printf(TEXT("Black Bear %s forecast contains direct damage"), *Expectation.IntentId.ToString()), DirectEffect))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("Black Bear phase %s forecast locks the expected hit count"), *Expectation.IntentId.ToString()),
			DirectEffect->HitCount,
			Expectation.ExpectedHitCount);

		FGameXXKCardEnemyIntent ResolvedIntent;
		TArray<FGameXXKCardDamageResult> IntentResults;
		bool bIntentsFinished = false;
		if (!TestTrue(FString::Printf(TEXT("Black Bear phase %s resolves through the shared enemy packet path: %s"), *Expectation.IntentId.ToString(), *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
		{
			continue;
		}
		TestEqual(FString::Printf(TEXT("Black Bear phase %s produces one audited result for each locked hit"), *Expectation.IntentId.ToString()),
			IntentResults.Num(),
			Expectation.ExpectedHitCount);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBossPhaseEndRoundDotTransitionTest,
	"GameXXK.Battle.EnemyIntentRules.BossPhase.EndRoundDotBeforeForecast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBossPhaseEndRoundDotTransitionTest::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < GetBossPhaseFixtureSpecs().Num(); ++Index)
	{
		const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[Index];
		FGameXXKRuntimeState State;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("%s end-round DoT fixture initializes: %s"), Spec.Label, *Error),
			InitializeBossPhaseFixture(State, Spec, 980 + Index, Error)))
		{
			continue;
		}

		FGameXXKCardCombatUnit* Boss = FindBossPhaseUnit(State, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("%s end-round DoT fixture exposes its boss"), Spec.Label), Boss))
		{
			continue;
		}
		Boss->HP = 52;
		TestEqual(FString::Printf(TEXT("%s receives one poison stack for the end-round threshold crossing"), Spec.Label),
			GameXXKCardRules::AddCombatStatus(*Boss, EGameXXKCardStatus::Poison, 1), 1);

		TArray<FGameXXKCardDamageResult> PhaseResults;
		if (!TestTrue(FString::Printf(TEXT("%s enters the enemy phase before enemy-side DoT: %s"), Spec.Label, *Error),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
		{
			continue;
		}
		FGameXXKCardEnemyIntent ResolvedIntent;
		TArray<FGameXXKCardDamageResult> IntentResults;
		bool bIntentsFinished = false;
		if (!TestTrue(FString::Printf(TEXT("%s resolves its saved enemy intent before the boundary: %s"), Spec.Label, *Error),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
			|| !TestTrue(FString::Printf(TEXT("%s has no unresolved enemy intents before completion"), Spec.Label), bIntentsFinished)
			|| !TestTrue(FString::Printf(TEXT("%s completes into the next player round: %s"), Spec.Label, *Error),
				FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error)))
		{
			continue;
		}

		Boss = FindBossPhaseUnit(State, Spec.UnitId);
		TestTrue(FString::Printf(TEXT("%s enemy-side poison reaches the inclusive fifty-percent threshold"), Spec.Label),
			Boss && Boss->bLiving && static_cast<int64>(Boss->HP) * 2 <= Boss->MaxHP);
		const FGameXXKEnemyBattleState* EnemyState = FindBossPhaseState(State, Spec.UnitId);
		TestTrue(FString::Printf(TEXT("%s enters phase two from end-round DoT before its new forecast is retained"), Spec.Label),
			EnemyState && EnemyState->bPhaseTwo);
		TestTrue(FString::Printf(TEXT("%s retains a next enemy forecast after the DoT phase transition"), Spec.Label),
			State.CardRun.EnemyIntents.ContainsByPredicate([&Spec](const FGameXXKCardEnemyIntent& Intent)
			{
				return Intent.SourceUnitId == Spec.UnitId;
			}));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBossPhaseSaveReloadOneTimeTest,
	"GameXXK.Battle.EnemyIntentRules.BossPhase.SaveReloadPersistsAfterHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBossPhaseSaveReloadOneTimeTest::RunTest(const FString& Parameters)
{
	for (int32 Index = 0; Index < GetBossPhaseFixtureSpecs().Num(); ++Index)
	{
		const FBossPhaseFixtureSpec& Spec = GetBossPhaseFixtureSpecs()[Index];
		FGameXXKRuntimeState State;
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("%s save/reload fixture initializes: %s"), Spec.Label, *Error),
			InitializeBossPhaseFixture(State, Spec, 990 + Index, Error)))
		{
			continue;
		}
		FGameXXKCardCombatUnit* PreSaveBoss = FindBossPhaseUnit(State, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("%s save/reload fixture exposes its boss before the first threshold crossing"), Spec.Label), PreSaveBoss))
		{
			continue;
		}
		PreSaveBoss->HP = 51;

		const FName FirstCardId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		FGameXXKCardPlayResult FirstResult;
		if (!TestTrue(FString::Printf(TEXT("%s crosses the threshold before save: %s"), Spec.Label, *Error),
			FGameXXKCardBattleAdapter::ResolveCardPlay(State, FirstCardId, Spec.UnitId, FirstResult, &Error)))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s records its one-time phase entry before save"), Spec.Label),
			FindBossPhaseState(State, Spec.UnitId) && FindBossPhaseState(State, Spec.UnitId)->bPhaseTwo);

		FGameXXKRuntimeState ReloadedState;
		const TArray<uint8> SavedBytes = SerializeEnemyIntentStateForTest(State);
		if (!TestTrue(FString::Printf(TEXT("%s phase state serializes and reloads"), Spec.Label),
			DeserializeEnemyIntentStateForTest(SavedBytes, ReloadedState)))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s keeps phase two after save/reload"), Spec.Label),
			FindBossPhaseState(ReloadedState, Spec.UnitId) && FindBossPhaseState(ReloadedState, Spec.UnitId)->bPhaseTwo);

		FGameXXKCardCombatUnit* ReloadedBoss = FindBossPhaseUnit(ReloadedState, Spec.UnitId);
		if (!TestNotNull(FString::Printf(TEXT("%s remains addressable after reload"), Spec.Label), ReloadedBoss))
		{
			continue;
		}
		GameXXKCardRules::HealCombatUnit(*ReloadedBoss, ReloadedBoss->MaxHP);
		TestTrue(FString::Printf(TEXT("%s stays in phase two after healing above half"), Spec.Label),
			FindBossPhaseState(ReloadedState, Spec.UnitId) && FindBossPhaseState(ReloadedState, Spec.UnitId)->bPhaseTwo);

		const FName SecondCardId = ReloadedState.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		FGameXXKCardPlayResult SecondResult;
		if (!TestTrue(FString::Printf(TEXT("%s can cross the threshold again after healing: %s"), Spec.Label, *Error),
			FGameXXKCardBattleAdapter::ResolveCardPlay(ReloadedState, SecondCardId, Spec.UnitId, SecondResult, &Error)))
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("%s retains its prior phase entry instead of resetting after a second crossing"), Spec.Label),
			FindBossPhaseState(ReloadedState, Spec.UnitId) && FindBossPhaseState(ReloadedState, Spec.UnitId)->bPhaseTwo);
	}
	return true;
}

namespace
{
	bool InitializeTigerPreyFixture(
		FGameXXKRuntimeState& OutState,
		FString& OutError,
		const int32 NodeId)
	{
		const FBossPhaseFixtureSpec& TigerSpec = GetBossPhaseFixtureSpecs()[2];
		if (!InitializeBossPhaseFixture(OutState, TigerSpec, NodeId, OutError))
		{
			return false;
		}

		auto AddPartyUnit = [&OutState](
			const FName UnitId,
			const EGameXXKCharacterRole Role,
			const int32 Health,
			const int32 StableSortOrder)
		{
			FGameXXKCardCombatUnit Unit;
			Unit.UnitId = UnitId;
			Unit.Side = EGameXXKCardTargetSide::Party;
			Unit.Role = Role;
			Unit.MaxHP = 100;
			Unit.HP = Health;
			Unit.MaxMana = 20;
			Unit.Mana = 20;
			Unit.Attack = 10;
			Unit.Defense = 0;
			Unit.Speed = 8;
			Unit.Armor = 0;
			Unit.StableSortOrder = StableSortOrder;
			Unit.bLiving = true;
			OutState.CardRun.ActiveBattle.Units.Add(MoveTemp(Unit));
		};

		// This is a card-runtime-only three-person fixture.  The normal route projection
		// still owns the production hero + permanent-partner + task-NPC limit.
		AddPartyUnit(TEXT("Fixture.Tiger.Prey"), EGameXXKCharacterRole::Blade, 30, 1);
		AddPartyUnit(TEXT("Fixture.Tiger.NextPrey"), EGameXXKCharacterRole::QuestNpc, 60, 2);
		OutState.CardRun.EnemyIntents.Reset();
		OutState.CardRun.NextEnemyIntentIndex = 0;
		return GameXXKCardRules::ValidateCardBattleRuntime(OutState.CardRun.ActiveBattle, &OutError);
	}

	FGameXXKCardCombatUnit* FindTigerPreyFixtureUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardEnemyIntent* FindTigerPreyFixtureIntent(const FGameXXKRuntimeState& State, const FName IntentId)
	{
		return State.CardRun.EnemyIntents.FindByPredicate([IntentId](const FGameXXKCardEnemyIntent& Intent)
		{
			return Intent.SourceUnitId == TEXT("Enemy.Tiger.P2") && Intent.IntentDefinitionId == IntentId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTigerPreyPhaseRetargetTest,
	"GameXXK.Battle.EnemyIntentRules.Tiger.PreyLocksPhasePounceAndRetargetsAfterDeath",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTigerPreyPhaseRetargetTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the three-party Tiger fixture initializes"),
		InitializeTigerPreyFixture(State, Error, 994)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("the Tiger starts by forecasting Mark Prey against the lowest-health living party member"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* MarkPreyForecast = FindTigerPreyFixtureIntent(State, TEXT("MarkPrey"));
	if (!TestNotNull(TEXT("the Tiger Mark Prey forecast remains visible"), MarkPreyForecast)
		|| !TestEqual(TEXT("Mark Prey locks the current lowest-health party member"),
			MarkPreyForecast->SuggestedTargetUnitId, FName(TEXT("Fixture.Tiger.Prey"))))
	{
		return false;
	}

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the locked Mark Prey action resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the one-Tiger Mark Prey phase has no further enemy cards"), bIntentsFinished))
	{
		return false;
	}
	FGameXXKEnemyBattleState* TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	FGameXXKCardCombatUnit* FirstPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	if (!TestNotNull(TEXT("the Tiger retains serializable state after marking prey"), TigerState)
		|| !TestNotNull(TEXT("the marked party member remains addressable"), FirstPrey))
	{
		return false;
	}
	TestEqual(TEXT("Mark Prey records a stable target identity rather than relying on live health ordering"),
		TigerState->PersistentTargetUnitId, FName(TEXT("Fixture.Tiger.Prey")));
	TestEqual(TEXT("the recorded target status is the catalog Prey status"),
		TigerState->PersistentTargetStatus, static_cast<uint8>(EGameXXKCardStatus::Prey));
	TestEqual(TEXT("the recorded target receives the visible Prey status"),
		GameXXKCardRules::GetCombatStatusStacks(*FirstPrey, EGameXXKCardStatus::Prey), 1);

	FGameXXKRuntimeState ReloadedState;
	if (!TestTrue(TEXT("the saved Prey target survives a SaveGame round-trip"),
		DeserializeEnemyIntentStateForTest(SerializeEnemyIntentStateForTest(State), ReloadedState)))
	{
		return false;
	}
	State = MoveTemp(ReloadedState);
	TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	if (!TestNotNull(TEXT("the reloaded Tiger still retains serialized prey state"), TigerState))
	{
		return false;
	}
	TestEqual(TEXT("reload preserves the locked Prey target"),
		TigerState->PersistentTargetUnitId, FName(TEXT("Fixture.Tiger.Prey")));

	if (!TestTrue(TEXT("the Mark Prey phase completes into a player turn"),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Tiger = FindBossPhaseUnit(State, TEXT("Enemy.Tiger.P2"));
	if (!TestNotNull(TEXT("the Tiger remains addressable before phase-two entry"), Tiger)
		|| !TestTrue(TEXT("the player has a card to cross Tiger's phase threshold"), !State.CardRun.ActiveBattle.Deck.Hand.IsEmpty()))
	{
		return false;
	}
	Tiger->HP = 51;
	const FName PhaseCardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult PhaseCardResult;
	if (!TestTrue(TEXT("a real player card enters Tiger phase two"),
		FGameXXKCardBattleAdapter::ResolveCardPlay(State, PhaseCardInstanceId, Tiger->UnitId, PhaseCardResult, &Error)))
	{
		return false;
	}
	TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	if (!TestTrue(TEXT("Tiger phase two is stored before its next intent is forecast"), TigerState && TigerState->bPhaseTwo))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* PounceForecast = FindTigerPreyFixtureIntent(State, TEXT("TigerPounce"));
	if (!TestNotNull(TEXT("phase-two Tiger forecasts Tiger Pounce"), PounceForecast)
		|| !TestTrue(TEXT("phase-two Tiger Pounce has one direct-damage effect"), PounceForecast->Effects.Num() == 1))
	{
		return false;
	}
	const FGameXXKResolvedEnemyIntentEffect& PounceEffect = PounceForecast->Effects[0];
	TestEqual(TEXT("a lower-health non-Prey party member cannot steal the locked Tiger Pounce forecast"),
		PounceEffect.TargetUnitIds.IsEmpty() ? NAME_None : PounceEffect.TargetUnitIds[0], FName(TEXT("Fixture.Tiger.Prey")));
	TestEqual(TEXT("Tiger phase two raises only Tiger Pounce to one-hundred-fifty percent damage"), PounceEffect.Magnitude, 24);
	TestEqual(TEXT("Tiger phase two changes Tiger Pounce from one hit to two hits"), PounceEffect.HitCount, 2);
	if (!TestTrue(TEXT("the player phase ends before the saved Tiger Pounce resolves"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}

	FirstPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	FGameXXKCardCombatUnit* NextPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.NextPrey"));
	if (!TestNotNull(TEXT("the locked first Prey remains mutable for the execution test"), FirstPrey)
		|| !TestNotNull(TEXT("the eventual second Prey remains mutable for the execution test"), NextPrey))
	{
		return false;
	}
	FirstPrey->HP = 1;
	FirstPrey->bLiving = true;
	NextPrey->HP = 10;
	NextPrey->bLiving = true;

	if (!TestTrue(TEXT("the two-hit phase-two Tiger Pounce resolves its locked first target"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	FirstPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	NextPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.NextPrey"));
	if (!TestNotNull(TEXT("the Tiger retains state after a defeated Prey"), TigerState)
		|| !TestNotNull(TEXT("the next lowest living party member remains addressable"), NextPrey))
	{
		return false;
	}
	TestTrue(TEXT("the locked Prey is defeated by the phase-two Pounce"), FirstPrey && !FirstPrey->bLiving);
	TestEqual(TEXT("Tiger retargets only after its stored Prey dies"),
		TigerState->PersistentTargetUnitId, FName(TEXT("Fixture.Tiger.NextPrey")));
	TestEqual(TEXT("the newly selected target receives the visible Prey status"),
		GameXXKCardRules::GetCombatStatusStacks(*NextPrey, EGameXXKCardStatus::Prey), 1);
	TestEqual(TEXT("the defeated former target no longer retains the visible Prey status"),
		FirstPrey ? GameXXKCardRules::GetCombatStatusStacks(*FirstPrey, EGameXXKCardStatus::Prey) : INDEX_NONE, 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTigerPreyPhaseOneExpiryTest,
	"GameXXK.Battle.EnemyIntentRules.Tiger.PreyExpiresAfterPhaseOnePounce",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTigerPreyPhaseOneExpiryTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the phase-one Tiger prey-expiry fixture initializes"),
		InitializeTigerPreyFixture(State, Error, 995)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("the phase-one Tiger marks its first prey"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error))
		|| !TestTrue(TEXT("the phase-one Mark Prey intent resolves"),
			FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error))
		|| !TestTrue(TEXT("the Mark Prey phase completes"),
			FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(State, PhaseResults, &Error))
		|| !TestTrue(TEXT("the subsequent player phase ends into Tiger Pounce"),
			FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* PounceForecast = FindTigerPreyFixtureIntent(State, TEXT("TigerPounce"));
	if (!TestNotNull(TEXT("phase-one Tiger forecasts a single-hit Pounce"), PounceForecast)
		|| !TestEqual(TEXT("phase-one Tiger Pounce is still one hit"), PounceForecast->Effects[0].HitCount, 1))
	{
		return false;
	}
	if (!TestTrue(TEXT("the phase-one Tiger Pounce resolves"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	FGameXXKEnemyBattleState* TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	FGameXXKCardCombatUnit* FirstPrey = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	if (!TestNotNull(TEXT("the phase-one Tiger persists its state after Pounce"), TigerState)
		|| !TestNotNull(TEXT("the living phase-one prey remains addressable"), FirstPrey))
	{
		return false;
	}
	TestFalse(TEXT("Tiger remains outside phase two for the normal Prey expiry case"), TigerState->bPhaseTwo);
	TestTrue(TEXT("a phase-one Tiger Pounce clears the saved Prey target"), TigerState->PersistentTargetUnitId.IsNone());
	TestEqual(TEXT("a phase-one Tiger Pounce clears the saved Prey status key"),
		TigerState->PersistentTargetStatus, static_cast<uint8>(EGameXXKCardStatus::None));
	TestEqual(TEXT("a phase-one Tiger Pounce removes the visible Prey status"),
		GameXXKCardRules::GetCombatStatusStacks(*FirstPrey, EGameXXKCardStatus::Prey), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTigerMarkPreyStaleForecastTest,
	"GameXXK.Battle.EnemyIntentRules.Tiger.MarkPreyRetargetsStaleForecast",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTigerMarkPreyStaleForecastTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the stale-Mark-Prey fixture initializes"), InitializeTigerPreyFixture(State, Error, 997)))
	{
		return false;
	}

	TArray<FGameXXKCardDamageResult> PhaseResults;
	if (!TestTrue(TEXT("Tiger forecasts Mark Prey before the forecast target expires"),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ExpiredForecastTarget = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	FGameXXKCardCombatUnit* ExpectedReplacement = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.NextPrey"));
	if (!TestNotNull(TEXT("the forecast target remains addressable before expiry"), ExpiredForecastTarget)
		|| !TestNotNull(TEXT("the living replacement prey remains addressable"), ExpectedReplacement))
	{
		return false;
	}
	ExpiredForecastTarget->HP = 0;
	ExpiredForecastTarget->bLiving = false;

	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(FString::Printf(TEXT("a stale Mark Prey forecast retargets instead of failing: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	const FGameXXKEnemyBattleState* TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	TestNotNull(TEXT("Tiger keeps serializable state after stale-forecast retargeting"), TigerState);
	if (TigerState)
	{
		TestEqual(TEXT("Tiger records the current lowest living party member as Prey"),
			TigerState->PersistentTargetUnitId, FName(TEXT("Fixture.Tiger.NextPrey")));
	}
	ExpectedReplacement = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.NextPrey"));
	TestNotNull(TEXT("the replacement target remains addressable after transactional intent resolution"), ExpectedReplacement);
	if (ExpectedReplacement)
	{
		TestEqual(TEXT("the replacement target receives the visible Prey status"),
			GameXXKCardRules::GetCombatStatusStacks(*ExpectedReplacement, EGameXXKCardStatus::Prey), 1);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTigerPredatorBleedHealingTest,
	"GameXXK.Battle.EnemyIntentRules.Tiger.PredatorHealsAfterDamagingBleedingTarget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTigerPredatorBleedHealingTest::RunTest(const FString& Parameters)
{
	FString Error;
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("the Tiger predator-heal fixture initializes"),
		InitializeTigerPreyFixture(State, Error, 996)))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Tiger = FindBossPhaseUnit(State, TEXT("Enemy.Tiger.P2"));
	FGameXXKCardCombatUnit* BleedingTarget = FindTigerPreyFixtureUnit(State, TEXT("Fixture.Tiger.Prey"));
	FGameXXKEnemyBattleState* TigerState = FindBossPhaseState(State, TEXT("Enemy.Tiger.P2"));
	if (!TestNotNull(TEXT("the Tiger predator-heal fixture exposes the Tiger"), Tiger)
		|| !TestNotNull(TEXT("the Tiger predator-heal fixture exposes a party target"), BleedingTarget)
		|| !TestNotNull(TEXT("the Tiger predator-heal fixture exposes saved Tiger state"), TigerState))
	{
		return false;
	}
	Tiger->HP = 40;
	TigerState->IntentCursor = 3; // Bleeding Rend.
	// Older saves predate the persistent-target field and therefore deserialize its byte as
	// the enum's Invalid value.  A non-Prey Tiger must normalize that legacy value instead
	// of trying to retarget an invalid status while forecasting Bleeding Rend.
	TigerState->PersistentTargetStatus = static_cast<uint8>(EGameXXKCardStatus::Invalid);
	TestEqual(TEXT("the target receives the required Bleed status before Tiger damages it"),
		GameXXKCardRules::AddCombatStatus(*BleedingTarget, EGameXXKCardStatus::Bleed, 1), 1);
	State.CardRun.EnemyIntents.Reset();
	State.CardRun.NextEnemyIntentIndex = 0;

	TArray<FGameXXKCardDamageResult> PhaseResults;
	const bool bEnteredRendEnemyPhase = FGameXXKCardBattleAdapter::EndPlayerCardPhase(State, PhaseResults, &Error);
	if (!TestTrue(FString::Printf(TEXT("the controlled Tiger forecast enters the Bleeding Rend enemy phase: %s"), *Error),
		bEnteredRendEnemyPhase))
	{
		return false;
	}
	const FGameXXKCardEnemyIntent* RendForecast = FindTigerPreyFixtureIntent(State, TEXT("BleedingRend"));
	if (!TestNotNull(TEXT("the controlled Tiger forecast is Bleeding Rend"), RendForecast))
	{
		return false;
	}
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> IntentResults;
	bool bIntentsFinished = false;
	if (!TestTrue(TEXT("Tiger damages the already-Bleeding party target"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(State, ResolvedIntent, IntentResults, bIntentsFinished, &Error)))
	{
		return false;
	}
	Tiger = FindBossPhaseUnit(State, TEXT("Enemy.Tiger.P2"));
	TestEqual(TEXT("Tiger heals eight percent of its current missing health after a successful Bleed-target hit"),
		Tiger ? Tiger->HP : INDEX_NONE, 44);
	return true;
}

#endif
