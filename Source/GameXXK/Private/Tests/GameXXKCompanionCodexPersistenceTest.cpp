#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionCodexPersistenceTest,
	"GameXXK.MVP.Codex.SaveMigration.Persistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexPersistenceTest::RunTest(const FString& Parameters)
{
	const FName GuideId(TEXT("Codex.Guide"));
	const FName MoneyRatId(TEXT("Codex.Enemy.Ch1.MoneyRat"));
	const FName BlackBearId(TEXT("Codex.Enemy.Ch2.BlackBear"));
	const FName TigerId(TEXT("Codex.Enemy.Ch3.Tiger"));
	const FName PreviousMoneyRatId(TEXT("Codex.MoneyRat"));
	const FName PreviousBlackBearId(TEXT("Codex.BlackBear"));
	const FName PreviousTigerId(TEXT("Codex.Tiger"));
	const FName LegacyBanditId(TEXT("Codex.Bandit"));
	const FName LegacyWolfId(TEXT("Codex.Wolf"));
	const FName LegacyEliteBanditId(TEXT("Codex.EliteBandit"));
	const FName LegacyBossId(TEXT("Codex.Boss"));

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("guide discovery succeeds"), UGameXXKMVPRules::DiscoverCodexEntry(State, GuideId));
	TestTrue(TEXT("Money Rat discovery succeeds"), UGameXXKMVPRules::DiscoverCodexEntry(State, MoneyRatId));
	TestTrue(TEXT("guide can be marked read"), UGameXXKMVPRules::MarkCodexEntryRead(State, GuideId));

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(State);
	TestEqual(TEXT("codex save uses the current version"), SaveState.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	FGameXXKRuntimeState DirectRestoredState;
	FGameXXKSaveMigrationReport DirectReport;
	TestTrue(TEXT("current codex save restores through typed dispatcher"), FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, DirectRestoredState, DirectReport));
	TestTrue(TEXT("direct restore preserves guide discovery"), DirectRestoredState.DiscoveredCodexEntryIds.Contains(GuideId));
	TestTrue(TEXT("direct restore preserves Money Rat discovery"), DirectRestoredState.DiscoveredCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("direct restore preserves guide read state"), DirectRestoredState.ReadCodexEntryIds.Contains(GuideId));
	TestFalse(TEXT("direct restore keeps Money Rat unread"), DirectRestoredState.ReadCodexEntryIds.Contains(MoneyRatId));

	const FString SaveSlot(TEXT("GameXXK_MVP_Automation_CompanionCodex"));
	const FString BackupSlot = SaveSlot + TEXT(".PreV7Backup");
	const int32 SaveUserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(SaveSlot, SaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(BackupSlot, SaveUserIndex);

	FGameXXKSaveState InvalidCurrentFollowerSave = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	InvalidCurrentFollowerSave.RuntimeState.QuestState = EGameXXKQuestState::Accepted;
	InvalidCurrentFollowerSave.RuntimeState.bFollowerJoined = false;
	FGameXXKRuntimeState InvalidCurrentFollowerState;
	FGameXXKSaveMigrationReport InvalidCurrentFollowerReport;
	// New semantics: accepting the quest keeps the guide in town. An accepted
	// quest without a recruited follower is a legal current save.
	TestTrue(
		TEXT("current saves accept an accepted quest without its follower until 入队 recruits it"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(
			InvalidCurrentFollowerSave,
			InvalidCurrentFollowerState,
			InvalidCurrentFollowerReport));
	TestFalse(
		TEXT("current-save restore keeps the guide unrecruited until 入队"),
		InvalidCurrentFollowerState.bFollowerJoined);

	FGameXXKSaveState VersionFourteenSave = UGameXXKMVPRules::MakeSaveState(UGameXXKMVPRules::CreateNewGame());
	VersionFourteenSave.SaveVersion = 14;
	VersionFourteenSave.RuntimeState.QuestState = EGameXXKQuestState::Accepted;
	VersionFourteenSave.RuntimeState.bFollowerJoined = false;
	VersionFourteenSave.RuntimeState.bHasQuestNpcLocation = false;
	VersionFourteenSave.RuntimeState.DiscoveredCodexEntryIds.Reset();
	VersionFourteenSave.RuntimeState.ReadCodexEntryIds.Reset();
	VersionFourteenSave.RuntimeState.DiscoveredCodexEntryIds.Add(PreviousMoneyRatId);
	VersionFourteenSave.RuntimeState.DiscoveredCodexEntryIds.Add(PreviousBlackBearId);
	VersionFourteenSave.RuntimeState.DiscoveredCodexEntryIds.Add(PreviousTigerId);
	VersionFourteenSave.RuntimeState.ReadCodexEntryIds.Add(PreviousMoneyRatId);
	VersionFourteenSave.RuntimeState.ReadCodexEntryIds.Add(PreviousTigerId);
	FGameXXKRuntimeState MigratedVersionFourteenState;
	FGameXXKSaveMigrationReport VersionFourteenReport;
	if (!TestTrue(
		TEXT("version fourteen save migrates follower and current enemy codex contracts"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(
			VersionFourteenSave,
			MigratedVersionFourteenState,
			VersionFourteenReport)))
	{
		return false;
	}
	TestEqual(TEXT("version fourteen report keeps its source version"), VersionFourteenReport.SourceVersion, 14);
	TestEqual(TEXT("version fourteen report targets the current version"), VersionFourteenReport.TargetVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	TestEqual(TEXT("version fourteen accepted quest remains accepted"), MigratedVersionFourteenState.QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("version fourteen accepted quest restores the required follower"), MigratedVersionFourteenState.bFollowerJoined);
	TestFalse(TEXT("version fourteen migration does not invent an NPC location"), MigratedVersionFourteenState.bHasQuestNpcLocation);
	TestTrue(TEXT("version fourteen Money Rat discovery migrates to the current enemy id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("version fourteen Black Bear discovery migrates to the current enemy id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(BlackBearId));
	TestTrue(TEXT("version fourteen Tiger discovery migrates to the current enemy id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(TigerId));
	TestTrue(TEXT("version fourteen Money Rat read history migrates to the current enemy id"), MigratedVersionFourteenState.ReadCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("version fourteen Tiger read history migrates to the current enemy id"), MigratedVersionFourteenState.ReadCodexEntryIds.Contains(TigerId));
	TestFalse(TEXT("version fourteen migration removes the previous Money Rat id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(PreviousMoneyRatId));
	TestFalse(TEXT("version fourteen migration removes the previous Black Bear id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(PreviousBlackBearId));
	TestFalse(TEXT("version fourteen migration removes the previous Tiger id"), MigratedVersionFourteenState.DiscoveredCodexEntryIds.Contains(PreviousTigerId));
	UGameInstance* SourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SourceSubsystem = NewObject<UGameXXKMVPSubsystem>(SourceGameInstance);
	FGameXXKRuntimeState& SourceState = SourceSubsystem->GetMutableRuntimeState();
	TestTrue(TEXT("save source discovers guide"), UGameXXKMVPRules::DiscoverCodexEntry(SourceState, GuideId));
	TestTrue(TEXT("save source discovers Money Rat"), UGameXXKMVPRules::DiscoverCodexEntry(SourceState, MoneyRatId));
	TestTrue(TEXT("save source marks guide read"), UGameXXKMVPRules::MarkCodexEntryRead(SourceState, GuideId));
	TestTrue(TEXT("real save game writes codex state"), SourceSubsystem->SaveCurrentGame(SaveSlot, SaveUserIndex));
	TestFalse(TEXT("current codex save creates no migration backup"), UGameplayStatics::DoesSaveGameExist(BackupSlot, SaveUserIndex));

	UGameInstance* LoadGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* LoadSubsystem = NewObject<UGameXXKMVPSubsystem>(LoadGameInstance);
	TestTrue(TEXT("real save game loads codex state"), LoadSubsystem->LoadGameFromSlot(SaveSlot, SaveUserIndex));
	const FGameXXKRuntimeState& LoadedState = LoadSubsystem->GetRuntimeState();
	TestTrue(TEXT("real save game preserves guide discovery"), LoadedState.DiscoveredCodexEntryIds.Contains(GuideId));
	TestTrue(TEXT("real save game preserves Money Rat discovery"), LoadedState.DiscoveredCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("real save game preserves guide read state"), LoadedState.ReadCodexEntryIds.Contains(GuideId));
	UGameplayStatics::DeleteGameInSlot(SaveSlot, SaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(BackupSlot, SaveUserIndex);

	FGameXXKSaveState VersionFourSave;
	VersionFourSave.SaveVersion = 4;
	VersionFourSave.RuntimeState = FGameXXKRuntimeState();
	VersionFourSave.RuntimeState.Inventory.Reset();
	VersionFourSave.RuntimeState.EquipmentCollection = FGameXXKEquipmentCollectionState();
	VersionFourSave.RuntimeState.QuestState = EGameXXKQuestState::Accepted;
	VersionFourSave.RuntimeState.DiscoveredCodexEntryIds.Reset();
	VersionFourSave.RuntimeState.ReadCodexEntryIds.Reset();
	FGameXXKRuntimeState MigratedVersionFourState;
	FGameXXKSaveMigrationReport VersionFourReport;
	TestTrue(TEXT("version four codex migration succeeds"), FGameXXKSaveMigration::TryRestoreRuntimeState(VersionFourSave, MigratedVersionFourState, VersionFourReport));
	TestTrue(TEXT("version four accepted quest discovers the guide"), MigratedVersionFourState.DiscoveredCodexEntryIds.Contains(GuideId));
	TestFalse(TEXT("version four migration does not invent Money Rat history"), MigratedVersionFourState.DiscoveredCodexEntryIds.Contains(MoneyRatId));
	TestFalse(TEXT("version four guide migration leaves the guide unread"), MigratedVersionFourState.ReadCodexEntryIds.Contains(GuideId));

	FGameXXKSaveState VersionFiveSave = VersionFourSave;
	VersionFiveSave.SaveVersion = 5;
	FGameXXKRuntimeState RestoredVersionFiveState;
	FGameXXKSaveMigrationReport EmptyVersionFiveReport;
	TestTrue(TEXT("empty version five codex migration succeeds"), FGameXXKSaveMigration::TryRestoreRuntimeState(VersionFiveSave, RestoredVersionFiveState, EmptyVersionFiveReport));
	TestFalse(TEXT("version five empty codex does not infer the guide"), RestoredVersionFiveState.DiscoveredCodexEntryIds.Contains(GuideId));

	VersionFiveSave.RuntimeState.DiscoveredCodexEntryIds.Reset();
	VersionFiveSave.RuntimeState.ReadCodexEntryIds.Reset();
	VersionFiveSave.RuntimeState.DiscoveredCodexEntryIds.Add(LegacyBanditId);
	VersionFiveSave.RuntimeState.DiscoveredCodexEntryIds.Add(LegacyWolfId);
	VersionFiveSave.RuntimeState.DiscoveredCodexEntryIds.Add(LegacyEliteBanditId);
	VersionFiveSave.RuntimeState.DiscoveredCodexEntryIds.Add(LegacyBossId);
	VersionFiveSave.RuntimeState.ReadCodexEntryIds.Add(LegacyWolfId);
	VersionFiveSave.RuntimeState.ReadCodexEntryIds.Add(LegacyEliteBanditId);
	VersionFiveSave.RuntimeState.ReadCodexEntryIds.Add(LegacyBossId);
	FGameXXKRuntimeState MigratedLegacyVersionFiveState;
	FGameXXKSaveMigrationReport LegacyVersionFiveReport;
	TestTrue(TEXT("legacy version five codex migration succeeds"), FGameXXKSaveMigration::TryRestoreRuntimeState(VersionFiveSave, MigratedLegacyVersionFiveState, LegacyVersionFiveReport));
	TestTrue(TEXT("version five Bandit or Wolf discovery migrates to Money Rat"), MigratedLegacyVersionFiveState.DiscoveredCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("version five EliteBandit discovery migrates to Black Bear"), MigratedLegacyVersionFiveState.DiscoveredCodexEntryIds.Contains(BlackBearId));
	TestTrue(TEXT("version five Boss discovery migrates to Tiger"), MigratedLegacyVersionFiveState.DiscoveredCodexEntryIds.Contains(TigerId));
	TestTrue(TEXT("version five Wolf read history migrates to Money Rat"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(MoneyRatId));
	TestTrue(TEXT("version five EliteBandit read history migrates to Black Bear"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(BlackBearId));
	TestTrue(TEXT("version five Boss read history migrates to Tiger"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(TigerId));
	TestFalse(TEXT("version five migration removes legacy Bandit discovery id"), MigratedLegacyVersionFiveState.DiscoveredCodexEntryIds.Contains(LegacyBanditId));
	TestFalse(TEXT("version five migration removes legacy Wolf read id"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(LegacyWolfId));
	TestFalse(TEXT("version five migration removes legacy EliteBandit read id"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(LegacyEliteBanditId));
	TestFalse(TEXT("version five migration removes legacy Boss read id"), MigratedLegacyVersionFiveState.ReadCodexEntryIds.Contains(LegacyBossId));

	return true;
}

#endif
