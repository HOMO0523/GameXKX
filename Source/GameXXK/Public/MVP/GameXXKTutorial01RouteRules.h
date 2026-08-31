#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

#include "GameXXKTutorial01RouteRules.generated.h"

UENUM()
enum class EGameXXKTutorial01RouteAction : uint8
{
	None,
	StartBattle,
	ReturnTown,
};

USTRUCT()
struct GAMEXXK_API FGameXXKTutorial01RouteState
{
	GENERATED_BODY()

	UPROPERTY(Transient)
	TSet<int32> VisitedNodeIds;

	UPROPERTY(Transient)
	TSet<int32> ReachableNodeIds;

	UPROPERTY(Transient)
	bool bBattleInProgress = false;

	UPROPERTY(Transient)
	bool bBattleWon = false;
};

class GAMEXXK_API FGameXXKTutorial01RouteRules final
{
public:
	static constexpr int32 StartNodeId = 100;
	static constexpr int32 BattleNodeId = 101;
	static constexpr int32 ReturnTownNodeId = 102;

	static void Initialize(FGameXXKTutorial01RouteState& OutState);
	static TArray<FGameXXKRouteMapNode> BuildNodes(
		const FGameXXKTutorial01RouteState& State);
	static TArray<FGameXXKRouteMapEdge> BuildEdges();
	static TMap<int32, FText> BuildLabels();
	static FText BuildCompletionNotice(
		const FGameXXKTutorial01RouteState& State);
	static bool RequestNode(
		FGameXXKTutorial01RouteState& InOutState,
		int32 NodeId,
		EGameXXKTutorial01RouteAction& OutAction);
	static bool MarkVictory(FGameXXKTutorial01RouteState& InOutState);
	static void MarkBattleAborted(FGameXXKTutorial01RouteState& InOutState);
};
