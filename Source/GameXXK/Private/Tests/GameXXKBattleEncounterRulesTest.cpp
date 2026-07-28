#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKRouteEconomyRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static FGameXXKRuntimeState BuildRouteBattleState(EGameXXKNodeKind NodeKind)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bFollowerJoined = true;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 707;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, NodeKind, FVector2D(0.5f, 0.5f), TArray<int32>{2}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		State.CardRun.RouteProgress.CurrentChapter = 1;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun);
		return State;
	}

	static const FGameXXKBattleRuntimeUnit* FindEnemySlot(const FGameXXKRuntimeState& State, const int32 SlotNumber)
	{
		return State.ActiveBattleEnemies.FindByPredicate([SlotNumber](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.BattleSlotNumber == SlotNumber;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleEncounterRulesTest,
	"GameXXK.MVP.Battle.EncounterRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleEncounterRulesTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState BattleState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("battle route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(BattleState, 1));
	TestEqual(TEXT("battle route node opens battle screen"), BattleState.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle route node creates active battle state"), BattleState.bHasActiveBattle);
	TestEqual(TEXT("active battle remembers pending node"), BattleState.ActiveBattleNodeId, 1);
	TestEqual(TEXT("normal battle creates exactly two flanking enemies"), BattleState.ActiveBattleEnemies.Num(), 2);
	TestNotNull(TEXT("normal battle reserves an enemy at 1P"), FindEnemySlot(BattleState, 1));
	TestNull(TEXT("normal battle keeps central enemy 2P empty"), FindEnemySlot(BattleState, 2));
	TestNotNull(TEXT("normal battle reserves an enemy at 3P"), FindEnemySlot(BattleState, 3));
	for (const FGameXXKBattleRuntimeUnit& Enemy : BattleState.ActiveBattleEnemies)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
		TestNotNull(TEXT("normal battle preserves a catalog definition identity"), Definition);
		TestTrue(TEXT("normal battle uses chapter one normal definitions"), Definition && Definition->Chapter == 1 && Definition->Tier == EGameXXKEnemyTier::Normal);
		TestTrue(TEXT("normal battle runtime IDs retain explicit P slot names"), Enemy.Id.ToString().EndsWith(FString::Printf(TEXT(".P%d"), Enemy.BattleSlotNumber)));
		if (Definition)
		{
			const FGameXXKEnemyComputedStats RawStats = FGameXXKEnemyCatalog::ComputeStats(Definition->Id, Enemy.CombatLevel);
			const FGameXXKEncounterStatScale Scale = FGameXXKEncounterRules::GetAuthoredStatScale(1, EGameXXKNodeKind::Battle);
			TestEqual(TEXT("formal battle projection applies the authored normal HP scale"), Enemy.MaxHP,
				FGameXXKEncounterRules::ScaleStat(RawStats.MaxHP, Scale.MaxHPPercent, 1));
			TestEqual(TEXT("formal battle projection applies the authored normal attack scale"), Enemy.Attack,
				FGameXXKEncounterRules::ScaleStat(RawStats.Attack, Scale.AttackPercent, 1));
			TestEqual(TEXT("formal battle projection applies the authored normal defense scale"), Enemy.Defense,
				FGameXXKEncounterRules::ScaleStat(RawStats.Defense, Scale.DefensePercent, 0));
		}
	}
	// bFollowerJoined is retained for the town quest narrative only.  It must not
	// create the old implicit fourth/ghost combat unit; real combat support is
	// supplied explicitly by the permanent-companion and task-NPC selections.
	TestEqual(TEXT("party snapshot includes only the hero without an explicit combat companion"), BattleState.ActiveBattleParty.Num(), 1);
	TestEqual(TEXT("hero snapshot is first party member"), BattleState.ActiveBattleParty[0].Id, FName(TEXT("Player")));
	TestFalse(TEXT("legacy narrative follower never becomes a ghost combat member"), BattleState.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == FName(TEXT("Follower"));
	}));

	FGameXXKRuntimeState EliteState = BuildRouteBattleState(EGameXXKNodeKind::Elite);
	TestTrue(TEXT("elite route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(EliteState, 1));
	TestTrue(TEXT("elite creates active battle state"), EliteState.bHasActiveBattle);
	TestEqual(TEXT("elite battle creates two normals plus one central elite"), EliteState.ActiveBattleEnemies.Num(), 3);
	const FGameXXKBattleRuntimeUnit* EliteCenter = FindEnemySlot(EliteState, 2);
	const FGameXXKBattleRuntimeUnit* EliteOneP = FindEnemySlot(EliteState, 1);
	const FGameXXKBattleRuntimeUnit* EliteThreeP = FindEnemySlot(EliteState, 3);
	const FGameXXKEnemyDefinition* EliteCenterDefinition = EliteCenter ? FGameXXKEnemyCatalog::Find(EliteCenter->EnemyDefinitionId) : nullptr;
	TestTrue(TEXT("elite central 2P is an elite definition"), EliteCenterDefinition && EliteCenterDefinition->Tier == EGameXXKEnemyTier::Elite);
	TestTrue(TEXT("elite center carries more sustained health pressure than either normal flank"), EliteCenter
		&& EliteOneP
		&& EliteThreeP
		&& EliteCenter->MaxHP > EliteOneP->MaxHP
		&& EliteCenter->MaxHP > EliteThreeP->MaxHP);

	FGameXXKRuntimeState BossState = BuildRouteBattleState(EGameXXKNodeKind::Boss);
	TestTrue(TEXT("boss route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(BossState, 1));
	TestTrue(TEXT("boss creates active battle state"), BossState.bHasActiveBattle);
	TestEqual(TEXT("boss battle creates two elite flanks and one central boss"), BossState.ActiveBattleEnemies.Num(), 3);
	const FGameXXKBattleRuntimeUnit* BossCenter = FindEnemySlot(BossState, 2);
	const FGameXXKEnemyDefinition* BossCenterDefinition = BossCenter ? FGameXXKEnemyCatalog::Find(BossCenter->EnemyDefinitionId) : nullptr;
	TestTrue(TEXT("boss central 2P uses the chapter one Money Rat definition"), BossCenterDefinition && BossCenterDefinition->Id == FName(TEXT("Enemy.Ch1.MoneyRat")));
	TestTrue(TEXT("boss central 2P is structurally stronger than both elite flanks"), BossCenter
		&& BossCenter->Attack > FindEnemySlot(BossState, 1)->Attack
		&& BossCenter->Attack > FindEnemySlot(BossState, 3)->Attack);

	FGameXXKRuntimeState AttackState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("attack test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(AttackState, 1));
	TestTrue(TEXT("battle state gives hero MP"), AttackState.PlayerMaxMP > 0);
	TestEqual(TEXT("hero battle snapshot copies player MP"), AttackState.ActiveBattleParty[0].MP, AttackState.PlayerMP);
	TestEqual(TEXT("hero battle snapshot copies max MP"), AttackState.ActiveBattleParty[0].MaxMP, AttackState.PlayerMaxMP);
	for (FGameXXKBattleRuntimeUnit& Enemy : AttackState.ActiveBattleEnemies)
	{
		// This test isolates the basic-attack contract. Encounter pressure is covered above.
		Enemy.Attack = 1;
	}
	const int32 EnemyHPBefore = AttackState.ActiveBattleEnemies[0].HP;
	const int32 ExpectedDamage = FMath::Max(1, AttackState.ActiveBattleParty[0].Attack - AttackState.ActiveBattleEnemies[0].Defense);
	TestTrue(TEXT("basic attack succeeds"), UGameXXKMVPRules::ExecuteBattleBasicAttack(AttackState, 0, 0));
	TestEqual(TEXT("basic attack reduces enemy HP"), AttackState.ActiveBattleEnemies[0].HP, FMath::Max(0, EnemyHPBefore - ExpectedDamage));
	TestTrue(TEXT("enemy AI acts after a non-lethal player attack"), AttackState.PlayerHP < AttackState.PlayerMaxHP);

	FGameXXKRuntimeState SkillState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("skill test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(SkillState, 1));
	for (int32 EnemyIndex = 1; EnemyIndex < SkillState.ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		SkillState.ActiveBattleEnemies[EnemyIndex].HP = 0;
		SkillState.ActiveBattleEnemies[EnemyIndex].bDefeated = true;
	}
	SkillState.ActiveBattleEnemies[0].HP = 200;
	SkillState.ActiveBattleEnemies[0].MaxHP = 200;
	SkillState.ActiveBattleEnemies[0].Attack = 1;
	const int32 SkillEnemyHPBefore = SkillState.ActiveBattleEnemies[0].HP;
	const int32 SkillMPBefore = SkillState.PlayerMP;
	TestTrue(TEXT("Crane Wing Slash succeeds with enough MP"), UGameXXKMVPRules::ExecuteBattleCraneWingSlash(SkillState, 0, 0));
	TestTrue(TEXT("Crane Wing Slash spends MP"), SkillState.PlayerMP < SkillMPBefore);
	TestTrue(TEXT("Crane Wing Slash deals more than a basic hit"), SkillState.ActiveBattleEnemies[0].HP <= SkillEnemyHPBefore - ExpectedDamage - 1);

	FGameXXKRuntimeState HealState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("healing art test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(HealState, 1));
	for (FGameXXKBattleRuntimeUnit& Enemy : HealState.ActiveBattleEnemies)
	{
		Enemy.Attack = 1;
	}
	HealState.ActiveBattleParty[0].HP = 40;
	HealState.PlayerHP = 40;
	const int32 HealMPBefore = HealState.PlayerMP;
	TestTrue(TEXT("Guiyuan art succeeds"), UGameXXKMVPRules::ExecuteBattleGuiyuanArt(HealState, 0));
	TestTrue(TEXT("Guiyuan art restores HP even after enemy response"), HealState.PlayerHP > 40);
	TestTrue(TEXT("Guiyuan art spends MP"), HealState.PlayerMP < HealMPBefore);

	FGameXXKRuntimeState DefendState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("defend test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(DefendState, 1));
	for (FGameXXKBattleRuntimeUnit& Enemy : DefendState.ActiveBattleEnemies)
	{
		Enemy.Attack = 25;
	}
	DefendState.ActiveBattleParty[0].Defense = 0;
	DefendState.PlayerDefense = 0;
	const int32 DefendHPBefore = DefendState.PlayerHP;
	TestTrue(TEXT("defend action succeeds"), UGameXXKMVPRules::ExecuteBattleDefend(DefendState, 0));
	TestTrue(TEXT("defend reduces incoming enemy damage"), DefendState.PlayerHP >= DefendHPBefore - 26);
	TestFalse(TEXT("defending flag clears after enemy response"), DefendState.ActiveBattleParty[0].bDefending);

	FGameXXKRuntimeState PowderState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("powder test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(PowderState, 1));
	for (FGameXXKBattleRuntimeUnit& Enemy : PowderState.ActiveBattleEnemies)
	{
		Enemy.Attack = 1;
	}
	PowderState.ActiveBattleParty[0].HP = 30;
	PowderState.PlayerHP = 30;
	const int32 PowderBefore = UGameXXKMVPRules::GetItemCount(PowderState, UGameXXKMVPRules::ItemHealingPowder());
	TestTrue(TEXT("battle healing powder succeeds"), UGameXXKMVPRules::ExecuteBattleHealingPowder(PowderState, 0));
	TestEqual(TEXT("battle healing powder consumes one item"), UGameXXKMVPRules::GetItemCount(PowderState, UGameXXKMVPRules::ItemHealingPowder()), PowderBefore - 1);
	TestTrue(TEXT("battle healing powder restores HP even after enemy response"), PowderState.PlayerHP > 30);

	FGameXXKRuntimeState HeroDefeatState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("hero defeat test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(HeroDefeatState, 1));
	HeroDefeatState.ActiveBattleParty[0].HP = 1;
	HeroDefeatState.PlayerHP = 1;
	if (HeroDefeatState.ActiveBattleParty.IsValidIndex(1))
	{
		HeroDefeatState.ActiveBattleParty[1].HP = HeroDefeatState.ActiveBattleParty[1].MaxHP;
		HeroDefeatState.ActiveBattleParty[1].bDefeated = false;
	}
	for (int32 EnemyIndex = 0; EnemyIndex < HeroDefeatState.ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		FGameXXKBattleRuntimeUnit& Enemy = HeroDefeatState.ActiveBattleEnemies[EnemyIndex];
		Enemy.Attack = 100;
		if (EnemyIndex > 0)
		{
			Enemy.HP = 0;
			Enemy.bDefeated = true;
		}
	}
	TestTrue(TEXT("enemy AI defeats the hero when no explicit combat companion is selected"), UGameXXKMVPRules::ExecuteBattleDefend(HeroDefeatState, 0));
	TestEqual(TEXT("hero defeat returns to town"), HeroDefeatState.Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("hero defeat clears active battle state"), HeroDefeatState.bHasActiveBattle);
	TestEqual(TEXT("hero defeat restores full town HP"), HeroDefeatState.PlayerHP, HeroDefeatState.PlayerMaxHP);

	FGameXXKRuntimeState DefeatState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("defeat test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(DefeatState, 1));
	DefeatState.ActiveBattleParty[0].HP = 1;
	DefeatState.PlayerHP = 1;
	if (DefeatState.ActiveBattleParty.IsValidIndex(1))
	{
		DefeatState.ActiveBattleParty[1].HP = 0;
		DefeatState.ActiveBattleParty[1].bDefeated = true;
	}
	for (FGameXXKBattleRuntimeUnit& Enemy : DefeatState.ActiveBattleEnemies)
	{
		Enemy.Attack = 100;
	}
	TestTrue(TEXT("enemy AI can defeat the party after player action"), UGameXXKMVPRules::ExecuteBattleDefend(DefeatState, 0));
	TestEqual(TEXT("defeat returns to town"), DefeatState.Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("defeat clears active battle state"), DefeatState.bHasActiveBattle);

	FGameXXKRuntimeState VictoryState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("victory test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(VictoryState, 1));
	for (FGameXXKCardCombatUnit& Unit : VictoryState.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	VictoryState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("card-runtime victory opens the saved three-card reward offer"), UGameXXKMVPRules::ResolveBattleVictory(VictoryState, false));
	TestEqual(TEXT("battle victory remains on the board while card reward is pending"), VictoryState.Screen, EGameXXKScreen::Battle);
	TestEqual(TEXT("battle victory exposes three route reward choices"), VictoryState.CardRun.PendingReward.CardIds.Num(), 3);
	FString RewardError;
	TestTrue(FString::Printf(TEXT("skipping the saved reward resolves the route victory gate: %s"), *RewardError),
		FGameXXKCardBattleAdapter::SkipPendingRouteReward(VictoryState, &RewardError));
	TestTrue(TEXT("resolved reward completes route battle victory"), UGameXXKMVPRules::ResolveBattleVictory(VictoryState, false));
	TestEqual(TEXT("resolved battle victory returns to route map"), VictoryState.Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("resolved battle victory clears active battle state"), VictoryState.bHasActiveBattle);
	TestTrue(TEXT("battle victory marks pending node visited"), VictoryState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("battle victory clears pending node"), VictoryState.PendingRouteNodeId, INDEX_NONE);

	return true;
}

#endif
