#include "GameXXKMVPRules.h"
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
		return State;
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
	TestTrue(TEXT("normal battle creates one to three enemies"), BattleState.ActiveBattleEnemies.Num() >= 1 && BattleState.ActiveBattleEnemies.Num() <= 3);
	TestEqual(TEXT("party snapshot includes hero and follower"), BattleState.ActiveBattleParty.Num(), 2);
	TestEqual(TEXT("hero snapshot is first party member"), BattleState.ActiveBattleParty[0].Id, FName(TEXT("Player")));
	TestEqual(TEXT("follower snapshot is second party member"), BattleState.ActiveBattleParty[1].Id, FName(TEXT("Follower")));

	FGameXXKRuntimeState EliteState = BuildRouteBattleState(EGameXXKNodeKind::Elite);
	TestTrue(TEXT("elite route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(EliteState, 1));
	TestTrue(TEXT("elite creates active battle state"), EliteState.bHasActiveBattle);
	TestTrue(TEXT("elite enemy is stronger than normal enemy"), EliteState.ActiveBattleEnemies[0].Attack > BattleState.ActiveBattleEnemies[0].Attack);

	FGameXXKRuntimeState BossState = BuildRouteBattleState(EGameXXKNodeKind::Boss);
	TestTrue(TEXT("boss route node selection succeeds"), UGameXXKMVPRules::SelectRouteNodeById(BossState, 1));
	TestTrue(TEXT("boss creates active battle state"), BossState.bHasActiveBattle);
	TestEqual(TEXT("boss battle creates one enemy"), BossState.ActiveBattleEnemies.Num(), 1);
	TestEqual(TEXT("boss enemy is named Boss"), BossState.ActiveBattleEnemies[0].Id, FName(TEXT("Boss")));

	FGameXXKRuntimeState AttackState = BuildRouteBattleState(EGameXXKNodeKind::Battle);
	TestTrue(TEXT("attack test selects battle node"), UGameXXKMVPRules::SelectRouteNodeById(AttackState, 1));
	TestTrue(TEXT("battle state gives hero MP"), AttackState.PlayerMaxMP > 0);
	TestEqual(TEXT("hero battle snapshot copies player MP"), AttackState.ActiveBattleParty[0].MP, AttackState.PlayerMP);
	TestEqual(TEXT("hero battle snapshot copies max MP"), AttackState.ActiveBattleParty[0].MaxMP, AttackState.PlayerMaxMP);
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
	TestTrue(TEXT("enemy AI defeats hero even while follower survives"), UGameXXKMVPRules::ExecuteBattleDefend(HeroDefeatState, 0));
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
	for (int32 EnemyIndex = 1; EnemyIndex < VictoryState.ActiveBattleEnemies.Num(); ++EnemyIndex)
	{
		VictoryState.ActiveBattleEnemies[EnemyIndex].HP = 0;
		VictoryState.ActiveBattleEnemies[EnemyIndex].bDefeated = true;
	}
	VictoryState.ActiveBattleEnemies[0].HP = 1;
	VictoryState.ActiveBattleEnemies[0].bDefeated = false;
	TestTrue(TEXT("final basic attack succeeds"), UGameXXKMVPRules::ExecuteBattleBasicAttack(VictoryState, 0, 0));
	TestEqual(TEXT("battle victory returns to route map"), VictoryState.Screen, EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("battle victory clears active battle state"), VictoryState.bHasActiveBattle);
	TestTrue(TEXT("battle victory marks pending node visited"), VictoryState.VisitedRouteNodeIds.Contains(1));
	TestEqual(TEXT("battle victory clears pending node"), VictoryState.PendingRouteNodeId, INDEX_NONE);

	return true;
}

#endif
