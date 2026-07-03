#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleBoardWidgetTest,
	"GameXXK.MVP.Battle.BoardWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleBoardWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("new game starts"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan can be selected"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("quest can be accepted"), Subsystem->AcceptQuest());
	TestTrue(TEXT("accepted quest enters route map"), Subsystem->OpenDungeonFromTownExit());
	TestTrue(TEXT("start node advances to battle route node"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("battle node opens battle screen"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle screen is active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	UGameXXKBattleBoardWidget* BattleWidget = HUD->CreateBattleBoardWidgetForTest();
	TestNotNull(TEXT("HUD creates battle board widget"), BattleWidget);
	TestTrue(TEXT("HUD retains battle board widget"), HUD->HasBattleBoardWidget());
	TestEqual(TEXT("battle widget receives same MVP subsystem"), BattleWidget ? BattleWidget->GetMVPSubsystem() : nullptr, Subsystem);

	TestTrue(TEXT("battle board initializes widget tree"), BattleWidget->Initialize());
	BattleWidget->NativeConstruct();
	BattleWidget->RefreshFromState();
	TestTrue(TEXT("battle board visible on battle screen"), BattleWidget->IsBattleBoardVisible());
	TestEqual(TEXT("battle board creates one enemy slot"), BattleWidget->GetEnemySlotCount(), 1);
	TestEqual(TEXT("battle board creates hero and follower slots"), BattleWidget->GetPartySlotCount(), 2);
	TestEqual(TEXT("first enemy appears on the left side"), BattleWidget->GetEnemySlotSide(), FString(TEXT("Left")));
	TestEqual(TEXT("party appears on the right side"), BattleWidget->GetPartySlotSide(), FString(TEXT("Right")));
	const FVector2D EnemySlotPosition = BattleWidget->GetEnemySlotPositionForTest(0);
	const FVector2D HeroSlotPosition = BattleWidget->GetPartySlotPositionForTest(0);
	const FVector2D FollowerSlotPosition = BattleWidget->GetPartySlotPositionForTest(1);
	TestTrue(TEXT("enemy battle slot stays clear of left HUD command panel"), EnemySlotPosition.X >= 540.0f);
	TestTrue(TEXT("hero battle slot stays to the right of the enemy lane"), HeroSlotPosition.X >= EnemySlotPosition.X + 640.0f);
	TestEqual(TEXT("follower battle slot aligns under hero lane"), FollowerSlotPosition.X, HeroSlotPosition.X);

	TestTrue(TEXT("right-click enemy resolves battle victory"), BattleWidget->ExecutePrimaryEnemyAction());
	TestEqual(TEXT("battle victory returns to dungeon route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("battle victory advances route index"), Subsystem->GetRuntimeState().DungeonNodeIndex, 2);

	BattleWidget->RefreshFromState();
	TestFalse(TEXT("battle board hides outside battle screen"), BattleWidget->IsBattleBoardVisible());
	return true;
}

#endif
