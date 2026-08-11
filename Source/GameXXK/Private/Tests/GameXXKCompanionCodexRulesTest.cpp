#include "GameXXKMVPRules.h"
#include "GameXXKEnemyCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static const FGameXXKCodexEntryView* FindCodexEntryView(const TArray<FGameXXKCodexEntryView>& EntryViews, FName EntryId)
	{
		return EntryViews.FindByPredicate([EntryId](const FGameXXKCodexEntryView& EntryView)
		{
			return EntryView.Id == EntryId;
		});
	}

	static FGameXXKRuntimeState BuildReachableCombatRouteState(EGameXXKNodeKind NodeKind)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		if (!UGameXXKMVPRules::OpenWorldMap(State)
			|| !UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
			|| !UGameXXKMVPRules::AcceptTownQuest(State))
		{
			return FGameXXKRuntimeState();
		}
		State.RouteSeed = NodeKind == EGameXXKNodeKind::Boss ? 909 : 808;
		if (!UGameXXKMVPRules::EnterDungeon(State))
		{
			return FGameXXKRuntimeState();
		}

		// Preserve the canonical route-card state and replace only the focused node topology.
		State.bHasGeneratedRouteMap = true;
		State.CurrentRouteNodeId = 0;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, NodeKind, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		return State;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionCodexRulesTest,
	"GameXXK.MVP.Codex.RulesDiscovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexRulesTest::RunTest(const FString& Parameters)
{
	const FName GuideId(TEXT("Codex.Guide"));
	const FName RetiredMoneyRatAlias(TEXT("Codex.MoneyRat"));
	const FName RetiredBlackBearAlias(TEXT("Codex.BlackBear"));
	const FName RetiredTigerAlias(TEXT("Codex.Tiger"));
	const FName LegacyBanditId(TEXT("Codex.Bandit"));
	const FName LegacyWolfId(TEXT("Codex.Wolf"));
	const FName LegacyEliteBanditId(TEXT("Codex.EliteBandit"));
	const FName LegacyBossId(TEXT("Codex.Boss"));
	const FName UnknownId(TEXT("Codex.Unknown"));

	const TArray<FGameXXKEnemyDefinition>& EnemyDefinitions = FGameXXKEnemyCatalog::GetAllDefinitions();
	TestEqual(TEXT("enemy catalog exposes the approved twenty-one current definitions"), EnemyDefinitions.Num(), 21);
	TestEqual(TEXT("codex contains the Guide plus every current enemy definition"),
		UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::All), EnemyDefinitions.Num() + 1);
	TestEqual(TEXT("monster codex contains every current enemy definition"),
		UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::Monster), EnemyDefinitions.Num());
	TestEqual(TEXT("new game has no spirit codex entries"), UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory::Spirit), 0);

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestEqual(TEXT("new game has no discovered codex entries"), UGameXXKMVPRules::GetDiscoveredCodexEntryCount(State, EGameXXKCodexCategory::All), 0);
	TestFalse(TEXT("new game has no unread codex entries"), UGameXXKMVPRules::HasUnreadCodexEntries(State));

	const TArray<FGameXXKCodexEntryView> AllEntryViews = UGameXXKMVPRules::BuildCodexEntryViews(State, EGameXXKCodexCategory::All);
	const FGameXXKCodexEntryView* GuideView = FindCodexEntryView(AllEntryViews, GuideId);
	TestNotNull(TEXT("all codex view contains the guide"), GuideView);
	if (!GuideView)
	{
		return false;
	}
	TestEqual(TEXT("undiscovered guide uses hidden display name"), GuideView->DisplayName.ToString(), FString(TEXT("????")));
	TestFalse(TEXT("undiscovered guide is not marked discovered"), GuideView->bIsDiscovered);
	TestFalse(TEXT("undiscovered guide is not marked read"), GuideView->bIsRead);
	TSet<FName> EnemyCodexIds;
	for (const FGameXXKEnemyDefinition& Definition : EnemyDefinitions)
	{
		TestFalse(FString::Printf(TEXT("%s has a stable codex id"), *Definition.Id.ToString()), Definition.CodexId.IsNone());
		TestFalse(FString::Printf(TEXT("%s codex id is unique"), *Definition.Id.ToString()), EnemyCodexIds.Contains(Definition.CodexId));
		EnemyCodexIds.Add(Definition.CodexId);
		const FGameXXKCodexEntryView* EnemyView = FindCodexEntryView(AllEntryViews, Definition.CodexId);
		TestNotNull(FString::Printf(TEXT("codex view contains %s"), *Definition.Id.ToString()), EnemyView);
		if (EnemyView)
		{
			TestEqual(FString::Printf(TEXT("undiscovered %s remains hidden"), *Definition.Id.ToString()),
				EnemyView->DisplayName.ToString(), FString(TEXT("????")));
			TestTrue(FString::Printf(TEXT("undiscovered %s does not leak its portrait"), *Definition.Id.ToString()),
				EnemyView->IconPath.IsNull());
		}
		bool bFoundCodexDefinition = false;
		const FGameXXKCodexEntryDef EnemyCodexDefinition = UGameXXKMVPRules::GetCodexEntryDef(
			Definition.CodexId,
			bFoundCodexDefinition);
		TestTrue(FString::Printf(TEXT("codex definition exists for %s"), *Definition.Id.ToString()), bFoundCodexDefinition);
		if (bFoundCodexDefinition)
		{
			TestEqual(FString::Printf(TEXT("%s codex name follows the enemy catalog"), *Definition.Id.ToString()),
				EnemyCodexDefinition.DisplayName.ToString(), Definition.DisplayName.ToString());
			TestEqual(FString::Printf(TEXT("%s codex portrait follows the enemy catalog"), *Definition.Id.ToString()),
				EnemyCodexDefinition.IconPath, Definition.PortraitSoftPath);
		}
	}

	FGameXXKRuntimeState RegistryProbe = UGameXXKMVPRules::CreateNewGame();
	for (const FGameXXKEnemyDefinition& Definition : EnemyDefinitions)
	{
		TestTrue(FString::Printf(TEXT("current enemy codex id %s is discoverable"), *Definition.CodexId.ToString()),
			UGameXXKMVPRules::DiscoverCodexEntry(RegistryProbe, Definition.CodexId));
	}
	TestEqual(TEXT("all current enemy codex ids contribute to discovered monster count"),
		UGameXXKMVPRules::GetDiscoveredCodexEntryCount(RegistryProbe, EGameXXKCodexCategory::Monster), EnemyDefinitions.Num());

	TestTrue(TEXT("new game opens the world map"), UGameXXKMVPRules::OpenWorldMap(State));
	TestTrue(TEXT("world map enters Qingshan"), UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("town quest is accepted"), UGameXXKMVPRules::AcceptTownQuest(State));
	TestTrue(TEXT("accepting town quest discovers the guide"), State.DiscoveredCodexEntryIds.Contains(GuideId));
	TestTrue(TEXT("newly discovered guide is unread"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
	TestTrue(TEXT("first guide read mark succeeds"), UGameXXKMVPRules::MarkCodexEntryRead(State, GuideId));
	TestFalse(TEXT("reading the only discovered guide clears unread state"), UGameXXKMVPRules::HasUnreadCodexEntries(State));
	TestFalse(TEXT("repeated guide discovery reports no new discovery"), UGameXXKMVPRules::DiscoverCodexEntry(State, GuideId));
	TestTrue(TEXT("repeated guide discovery keeps the entry read"), State.ReadCodexEntryIds.Contains(GuideId));
	TestFalse(TEXT("unknown codex discovery is rejected"), UGameXXKMVPRules::DiscoverCodexEntry(State, UnknownId));
	TestFalse(TEXT("unknown codex read mark is rejected"), UGameXXKMVPRules::MarkCodexEntryRead(State, UnknownId));
	TestFalse(TEXT("retired Money Rat alias is no longer a duplicate visible entry"), UGameXXKMVPRules::DiscoverCodexEntry(State, RetiredMoneyRatAlias));
	TestFalse(TEXT("retired Black Bear alias is no longer a duplicate visible entry"), UGameXXKMVPRules::DiscoverCodexEntry(State, RetiredBlackBearAlias));
	TestFalse(TEXT("retired Tiger alias is no longer a duplicate visible entry"), UGameXXKMVPRules::DiscoverCodexEntry(State, RetiredTigerAlias));
	TestFalse(TEXT("legacy Bandit codex entry is no longer visible"), UGameXXKMVPRules::DiscoverCodexEntry(State, LegacyBanditId));
	TestFalse(TEXT("legacy Wolf codex entry is no longer visible"), UGameXXKMVPRules::DiscoverCodexEntry(State, LegacyWolfId));
	TestFalse(TEXT("legacy EliteBandit codex entry is no longer visible"), UGameXXKMVPRules::DiscoverCodexEntry(State, LegacyEliteBanditId));
	TestFalse(TEXT("legacy Boss codex entry is no longer visible"), UGameXXKMVPRules::DiscoverCodexEntry(State, LegacyBossId));

	TestTrue(TEXT("accepted quest enters the dungeon"), UGameXXKMVPRules::EnterDungeon(State));
	TestTrue(TEXT("route start node advances"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Start));
	TestTrue(TEXT("route battle node begins"), UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestTrue(TEXT("normal battle creates at least one current enemy"), !State.ActiveBattleEnemies.IsEmpty());
	for (const FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
		TestNotNull(TEXT("normal battle enemy resolves through the current catalog"), Definition);
		TestTrue(TEXT("normal battle discovers each encountered enemy's canonical codex id"),
			Definition && State.DiscoveredCodexEntryIds.Contains(Definition->CodexId));
	}
	TestTrue(TEXT("battle discoveries are unread"), UGameXXKMVPRules::HasUnreadCodexEntries(State));

	FGameXXKRuntimeState EliteState = BuildReachableCombatRouteState(EGameXXKNodeKind::Elite);
	TestTrue(TEXT("elite route node selection begins battle"), UGameXXKMVPRules::SelectRouteNodeById(EliteState, 1));
	TestEqual(TEXT("elite route node opens battle screen"), EliteState.Screen, EGameXXKScreen::Battle);
	for (const FGameXXKBattleRuntimeUnit& Enemy : EliteState.ActiveBattleEnemies)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
		TestTrue(TEXT("elite encounter discovers each current catalog enemy"),
			Definition && EliteState.DiscoveredCodexEntryIds.Contains(Definition->CodexId));
	}

	FGameXXKRuntimeState BossState = BuildReachableCombatRouteState(EGameXXKNodeKind::Boss);
	TestTrue(TEXT("boss route node selection begins battle"), UGameXXKMVPRules::SelectRouteNodeById(BossState, 1));
	TestEqual(TEXT("boss route node opens battle screen"), BossState.Screen, EGameXXKScreen::Battle);
	for (const FGameXXKBattleRuntimeUnit& Enemy : BossState.ActiveBattleEnemies)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
		TestTrue(TEXT("boss encounter discovers each current catalog enemy"),
			Definition && BossState.DiscoveredCodexEntryIds.Contains(Definition->CodexId));
	}

	return true;
}

#endif
