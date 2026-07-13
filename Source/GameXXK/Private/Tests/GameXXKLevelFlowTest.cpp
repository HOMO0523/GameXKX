#include "MVP/GameXXKLevelFlow.h"

#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLevelFlowTest,
	"GameXXK.MVP.LevelFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLevelFlowTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("town map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Town),
		FName(TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo")));
	TestEqual(
		TEXT("route map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::DungeonMap),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestEqual(
		TEXT("route event map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteEvent),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteEvent")));
	TestEqual(
		TEXT("route camp map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteCamp),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteCamp")));
	TestEqual(
		TEXT("route merchant map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteMerchant),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMerchant")));
	TestEqual(
		TEXT("GameXXK battle scene map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Battle),
		FName(TEXT("/Game/GameXXK/Maps/L_BattleScene")));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	TestEqual(
		TEXT("runtime battle state maps to GameXXK battle scene"),
		GameXXKLevelFlow::MapForRuntimeState(State),
		FName(TEXT("/Game/GameXXK/Maps/L_BattleScene")));
	TestTrue(
		TEXT("PIE route map package matches route target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_RouteMap"))));
	TestTrue(
		TEXT("PIE battle map package matches battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_BattleScene"), FName(TEXT("/Game/GameXXK/Maps/L_BattleScene"))));
	TestFalse(
		TEXT("route map package does not match battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_BattleScene"))));

	return true;
}

#endif
