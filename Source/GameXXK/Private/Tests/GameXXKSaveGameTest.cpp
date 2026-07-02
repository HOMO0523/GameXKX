#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSaveGameSlotRoundTripTest,
	"GameXXK.MVP.SaveGame.SlotRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSaveGameSlotRoundTripTest::RunTest(const FString& Parameters)
{
	const FString RoundTripSlot = TEXT("GameXXK_MVP_Automation_SaveGame_SlotRoundTrip");
	const FString NewGameSlot = TEXT("GameXXK_MVP_Automation_SaveGame_NewGame");
	const int32 UserIndex = 0;

	UGameplayStatics::DeleteGameInSlot(RoundTripSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(NewGameSlot, UserIndex);

	UGameInstance* SourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SourceSubsystem = NewObject<UGameXXKMVPSubsystem>(SourceGameInstance);
	TestNotNull(TEXT("source subsystem exists"), SourceSubsystem);

	FGameXXKRuntimeState& SourceState = SourceSubsystem->GetMutableRuntimeState();
	SourceState.QuestState = EGameXXKQuestState::Completed;
	SourceState.PlayerLevel = 3;
	SourceState.PlayerXP = 42;
	SourceState.PlayerGold = 123;
	SourceState.UnlockedRegions.Add(UGameXXKMVPRules::RegionTanjiang());
	SourceState.Inventory.Add(UGameXXKMVPRules::ItemIronSword(), 1);
	SourceState.EquippedWeapon = UGameXXKMVPRules::ItemIronSword();

	TestTrue(TEXT("custom slot save succeeds"), SourceSubsystem->SaveCurrentGame(RoundTripSlot, UserIndex));
	TestTrue(TEXT("custom slot exists after save"), UGameplayStatics::DoesSaveGameExist(RoundTripSlot, UserIndex));

	UGameInstance* LoadedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadedSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadedGameInstance);
	TestNotNull(TEXT("loaded subsystem exists"), LoadedSubsystem);
	TestTrue(TEXT("custom slot load succeeds"), LoadedSubsystem->LoadGameFromSlot(RoundTripSlot, UserIndex));

	const FGameXXKRuntimeState& LoadedState = LoadedSubsystem->GetRuntimeState();
	TestEqual(TEXT("load restores to main menu"), LoadedState.Screen, EGameXXKScreen::MainMenu);
	TestEqual(TEXT("quest completion persists"), LoadedState.QuestState, EGameXXKQuestState::Completed);
	TestEqual(TEXT("player level persists"), LoadedState.PlayerLevel, 3);
	TestEqual(TEXT("player XP persists"), LoadedState.PlayerXP, 42);
	TestEqual(TEXT("gold persists"), LoadedState.PlayerGold, 123);
	TestTrue(TEXT("Tanjiang unlock persists"), LoadedState.UnlockedRegions.Contains(UGameXXKMVPRules::RegionTanjiang()));
	TestEqual(TEXT("inventory details do not persist"), LoadedState.Inventory.Num(), 0);
	TestEqual(TEXT("equipment details do not persist"), LoadedState.EquippedWeapon, NAME_None);

	UGameInstance* StartGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* StartGameSubsystem = NewObject<UGameXXKMVPSubsystem>(StartGameInstance);
	TestTrue(TEXT("StartGameFromSlot loads saved slot"), StartGameSubsystem->StartGameFromSlot(RoundTripSlot, UserIndex));
	TestEqual(TEXT("StartGameFromSlot opens world map after load"), StartGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("StartGameFromSlot keeps persisted Tanjiang unlock"), StartGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	UGameInstance* LoadOrCreateGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadOrCreateSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadOrCreateGameInstance);
	TestTrue(TEXT("load-or-create opens saved game world map"), LoadOrCreateSubsystem->LoadOrCreateGame(RoundTripSlot, UserIndex));
	TestEqual(TEXT("load-or-create opens world map after load"), LoadOrCreateSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("load-or-create keeps persisted Tanjiang unlock"), LoadOrCreateSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	UGameInstance* NewGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* NewGameSubsystem = NewObject<UGameXXKMVPSubsystem>(NewGameInstance);
	TestTrue(TEXT("load-or-create creates missing slot state"), NewGameSubsystem->LoadOrCreateGame(NewGameSlot, UserIndex));
	TestEqual(TEXT("new load-or-create opens world map"), NewGameSubsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestTrue(TEXT("new load-or-create unlocks Qingshan"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionQingshan()));
	TestFalse(TEXT("new load-or-create keeps Tanjiang locked"), NewGameSubsystem->IsRegionUnlocked(UGameXXKMVPRules::RegionTanjiang()));

	UGameplayStatics::DeleteGameInSlot(RoundTripSlot, UserIndex);
	UGameplayStatics::DeleteGameInSlot(NewGameSlot, UserIndex);

	return true;
}

#endif
