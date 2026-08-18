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
		FName(TEXT("/Game/GameXXK/Maps/L_DesktopTrainingHUD")));
	TestEqual(
		TEXT("route map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::DungeonMap),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestEqual(
		TEXT("route event stays on the route map beneath its HUD modal"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteEvent),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestEqual(
		TEXT("route camp map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteCamp),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteCamp")));
	TestEqual(
		TEXT("route merchant stays on the route map beneath its HUD modal"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::RouteMerchant),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestEqual(
		TEXT("battle stays on route map"),
		GameXXKLevelFlow::MapForScreen(EGameXXKScreen::Battle),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));

	FGameXXKRuntimeState BattleState = UGameXXKMVPRules::CreateNewGame();
	BattleState.Screen = EGameXXKScreen::Battle;
	TestEqual(
		TEXT("runtime battle state maps to route map"),
		GameXXKLevelFlow::MapForRuntimeState(BattleState),
		FName(TEXT("/Game/GameXXK/Maps/L_RouteMap")));
	TestFalse(
		TEXT("battle overlay does not request route-map reload"),
		GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
			TEXT("/Game/GameXXK/Maps/L_RouteMap"), BattleState));
	TestTrue(
		TEXT("loading a battle save from town still opens route map"),
		GameXXKLevelFlow::RequiresMapLoadForRuntimeState(
			TEXT("/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo"), BattleState));
	TestTrue(
		TEXT("PIE route map package matches route target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_RouteMap"))));
	TestTrue(
		TEXT("PIE battle map package matches battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_BattleTown"), FName(TEXT("/Game/GameXXK/Maps/L_BattleTown"))));
	TestTrue(
		TEXT("playable Asian Village demo is a Town gameplay map"),
		GameXXKLevelFlow::IsTownGameplayMapPackage(TEXT("/Game/GameXXK/Maps/Prototype/UEDPIE_0_L_Qingshan_AsianVillage_Demo")));
	TestTrue(
		TEXT("legacy QingshanInn remains a Town gameplay map"),
		GameXXKLevelFlow::IsTownGameplayMapPackage(TEXT("/Game/GameXXK/Maps/L_QingshanInn")));
	TestTrue(
		TEXT("desktop training map is recognized as the HUD-only town entry"),
		GameXXKLevelFlow::IsDesktopTrainingHUDMapPackage(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_DesktopTrainingHUD")));
	TestFalse(
		TEXT("desktop training map does not opt into 3D town actor spawning"),
		GameXXKLevelFlow::IsTownGameplayMapPackage(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_DesktopTrainingHUD")));
	TestFalse(
		TEXT("route map is not a Town gameplay map"),
		GameXXKLevelFlow::IsTownGameplayMapPackage(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap")));
	TestFalse(
		TEXT("route map package does not match battle target"),
		GameXXKLevelFlow::MapPackageMatches(TEXT("/Game/GameXXK/Maps/UEDPIE_0_L_RouteMap"), FName(TEXT("/Game/GameXXK/Maps/L_BattleTown"))));

	return true;
}

#endif
