#include "MVP/GameXXKOneGameIslandRouteMapBridge.h"

#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKOneGameIslandRouteMapBridgeTest,
	"GameXXK.MVP.OneGameIslandRouteMapBridge",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKOneGameIslandRouteMapBridgeTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("target offset keeps a low node near the top padding"),
		AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(3277.0f, 280.0f),
		2997.0f);
	TestEqual(
		TEXT("target offset clamps before the top of the scroll content"),
		AGameXXKOneGameIslandRouteMapBridge::CalculateTargetScrollOffset(180.0f, 280.0f),
		0.0f);

	TestFalse(
		TEXT("original 1Game route does not open battle layout before any click"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForOriginalLevelAdvance(INDEX_NONE, 0, 1));
	TestTrue(
		TEXT("original 1Game route battle click opens battle layout"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForOriginalLevelAdvance(0, 1, 1));
	TestFalse(
		TEXT("already-open battle layout does not reopen on later level changes"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForOriginalLevelAdvance(1, 2, 1));
	TestTrue(
		TEXT("GameXXK battle runtime opens battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::Battle));
	TestFalse(
		TEXT("GameXXK route-map runtime does not open battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::DungeonMap));
	TestFalse(
		TEXT("GameXXK town runtime does not open battle layout directly"),
		AGameXXKOneGameIslandRouteMapBridge::ShouldOpenBattleLayoutForGameXXKRuntimeScreen(EGameXXKScreen::Town));

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(
		TEXT("bridge can prime a real battle state for the 1Game route map"),
		AGameXXKOneGameIslandRouteMapBridge::PrimeBattleSubsystemForIslandRoute(*Subsystem));
	TestEqual(TEXT("bridge battle state is the real battle screen"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("bridge battle state includes the quest follower"), Subsystem->GetRuntimeState().bFollowerJoined);

	return true;
}

#endif
