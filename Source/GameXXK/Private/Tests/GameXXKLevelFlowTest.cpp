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
		FName(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
	TestEqual(
		TEXT("route map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::DungeonMap),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestEqual(
		TEXT("1Game battle copy map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Battle),
		FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game")));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	TestEqual(
		TEXT("runtime battle state maps to 1Game battle copy"),
		GameXXKLevelFlow::MapForRuntimeState(State),
		FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game")));
	TestTrue(
		TEXT("PIE route map package matches route target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_RouteMap"))));
	TestTrue(
		TEXT("PIE battle map package matches battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_Battle_1Game"), FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game"))));
	TestFalse(
		TEXT("route map package does not match battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_Battle_1Game"))));

	return true;
}

#endif
