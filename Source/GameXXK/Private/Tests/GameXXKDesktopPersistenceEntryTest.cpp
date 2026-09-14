#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveStorage.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Engine/GameInstance.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    // Tests may run beside other test data. Preserve every default-slot byte.
    struct FDefaultSlotBackup
    {
        struct FFile { FString Path; TArray<uint8> Bytes; bool bExisted = false; };
        TArray<FFile> Files;
        FDefaultSlotBackup()
        {
            const FString Slot = UGameXXKMVPSubsystem::GetDefaultSaveSlotName();
            for (int32 Index = 0; Index <= FGameXXKSaveStorage::BackupCount; ++Index)
            {
                FFile& File = Files.AddDefaulted_GetRef();
                File.Path = FGameXXKSaveStorage::SlotPath(Index == 0 ? Slot : Slot + FString::Printf(TEXT(".Previous%d"), Index));
                IFileManager::Get().MakeDirectory(*FPaths::GetPath(File.Path), true);
                File.bExisted = FFileHelper::LoadFileToArray(File.Bytes, *File.Path);
            }
        }
        ~FDefaultSlotBackup()
        {
            for (const FFile& File : Files)
            {
                if (File.bExisted) FFileHelper::SaveArrayToFile(File.Bytes, *File.Path);
                else IFileManager::Get().Delete(*File.Path, false, true);
            }
        }
    };
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopLoadsRegularSaveTest,
    "GameXXK.Persistence.DesktopEntryLoadsRegularSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopLoadsRegularSaveTest::RunTest(const FString&)
{
    FDefaultSlotBackup Backup;
    auto* Writer = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if (!TestTrue(TEXT("new game fixture initializes"), Writer->StartNewGame())) return false;
    Writer->GetMutableRuntimeState().PlayerGold = 732145;
    const auto CompanionId = Writer->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId;
    if (!TestTrue(TEXT("regular slot is saved to disk"), Writer->SaveCurrentGame())) return false;
    auto* Reader = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    TestTrue(TEXT("direct desktop launch resumes the regular save"), Reader->EnsureDesktopTrainingRuntimeForDirectMap());
    TestEqual(TEXT("saved gold survives instead of being reset by new-game initialization"), Reader->GetRuntimeState().PlayerGold, 732145);
    TestEqual(TEXT("the same owned companion resumes"), Reader->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId, CompanionId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopRejectsCorruptSaveTest,
    "GameXXK.Persistence.DesktopEntryDoesNotReplaceUnreadableSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopRejectsCorruptSaveTest::RunTest(const FString&)
{
    FDefaultSlotBackup Backup;
    const TArray<uint8> Corrupt = {1, 2, 3, 4};
    for (const auto& File : Backup.Files)
    {
        if (!TestTrue(TEXT("unreadable-slot fixture is written"), FFileHelper::SaveArrayToFile(Corrupt, *File.Path))) return false;
    }
    auto* Reader = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    TestFalse(TEXT("an unreadable existing save must not silently start a new game"), Reader->EnsureDesktopTrainingRuntimeForDirectMap());
    TestFalse(TEXT("the existing save failure is reported"), Reader->GetLastSaveLoadError().IsEmpty());
    TArray<uint8> Actual;
    FFileHelper::LoadFileToArray(Actual, *Backup.Files[0].Path);
    TestTrue(TEXT("failed startup preserves the existing file"), Actual == Corrupt);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopExitSaveGateTest,
    "GameXXK.Persistence.DesktopExitSavesBeforeQuit", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopExitSaveGateTest::RunTest(const FString&)
{
    auto* MVP = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if (!TestTrue(TEXT("exit fixture initializes"), MVP->StartNewGame())) return false;
    auto* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
    Widget->SetMVPSubsystem(MVP);
    int32 Writes = 0;
    MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([&Writes](USaveGame*, const FString&, int32) { ++Writes; return false; }));
    Widget->HandleDesktopActionForTest(15);
    Widget->HandleDesktopActionForTest(54);
    TestTrue(TEXT("save failure keeps the exit confirmation open"), Widget->IsExitConfirmationOpenForTest());
    TestEqual(TEXT("exit attempts to persist exactly once"), Writes, 1);
    MVP->ResetSaveSlotWriteDelegateForTest();
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKPeriodicPersistenceTest,
    "GameXXK.Persistence.PeriodicSaveRetriesAndSkipsUnchanged",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKPeriodicPersistenceTest::RunTest(const FString&)
{
    auto* MVP=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    int32 Writes=0;bool CanWrite=false;
    MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(
        [&Writes,&CanWrite](USaveGame*,const FString&,int32){++Writes;return CanWrite;}));
    TestFalse(TEXT("Never save before player state is ready"),MVP->TickPersistence(30));
    if(!MVP->StartNewGame())return false;
    TestTrue(TEXT("Wait for interval"),MVP->TickPersistence(29));
    TestEqual(TEXT("No early disk writes"),Writes,0);
    TestFalse(TEXT("Write failure is reported"),MVP->TickPersistence(1));
    TestEqual(TEXT("One failed attempt"),Writes,1);
    CanWrite=true;
    TestTrue(TEXT("Pending changes retry on next interval"),MVP->TickPersistence(30));
    TestEqual(TEXT("Retry reached writer"),Writes,2);
    TestTrue(TEXT("Unchanged state is a no-op"),MVP->TickPersistence(30));
    TestEqual(TEXT("No redundant save"),Writes,2);
#if GAMEXXK_WITH_DEV_TOOLS
    MVP->SetDevelopmentWritesSuppressed(true);
    MVP->GetMutableRuntimeState().PlayerGold+=1;
    TestFalse(TEXT("Dev state cannot autosave"),MVP->TickPersistence(30,true));
    TestEqual(TEXT("Dev guard prevents even attempted writes"),Writes,2);
    MVP->SetDevelopmentWritesSuppressed(false);
#endif
    MVP->GetMutableRuntimeState().PlayerGold+=1;
    TestTrue(TEXT("Orderly exit flushes dirty state immediately"),MVP->TickPersistence(0,true));
    TestEqual(TEXT("Exit writes one final snapshot"),Writes,3);
    MVP->ResetSaveSlotWriteDelegateForTest();
    return true;
}
#endif
