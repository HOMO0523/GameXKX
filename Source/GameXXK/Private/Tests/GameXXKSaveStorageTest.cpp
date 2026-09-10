#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKSaveStorage.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "GameXXKPermanentPartyTestFixtures.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
    UGameXXKSaveGame* MakeStorageSave(int32 Gold)
    {
        auto* Save=NewObject<UGameXXKSaveGame>();
        auto State=GameXXKPermanentPartyTestFixtures::MakeStartedState();
        State.PlayerGold=Gold;
        Save->SaveState=UGameXXKMVPRules::MakeSaveState(State);
        return Save;
    }
    void CleanStorageSlots(const FString& Slot)
    {
        UGameplayStatics::DeleteGameInSlot(Slot,0);
        for(int32 Index=1;Index<=FGameXXKSaveStorage::BackupCount;++Index)
            UGameplayStatics::DeleteGameInSlot(Slot+FString::Printf(TEXT(".Previous%d"),Index),0);
        FGameXXKSaveStorage::SetFaultForTest(FGameXXKSaveStorage::EFault::None);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSaveStorageRoundTripTest,
    "GameXXK.SaveIntegrity.Storage.SealedRoundTripAndBackup",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKSaveStorageRoundTripTest::RunTest(const FString&)
{
    const FString Slot=TEXT("GameXXK_Automation_Storage_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT{CleanStorageSlots(Slot);};
    FString Error;bool Recovered=false;
    auto* First=MakeStorageSave(271);
    if(!TestTrue(FString::Printf(TEXT("Write a verified save: %s"),*Error),FGameXXKSaveStorage::Write(First,Slot,0,&Error)))return false;
    TestEqual(TEXT("Writing never mutates the caller's snapshot"),First->IntegritySchema,0);
    auto* Loaded=FGameXXKSaveStorage::Load(Slot,0,Recovered,&Error);
    if(!TestNotNull(TEXT("Sealed save reads successfully"),Loaded))return false;
    TestFalse(TEXT("Primary read needs no recovery"),Recovered);
    TestEqual(TEXT("Wallet survived the actual file round trip"),Loaded->SaveState.RuntimeState.PlayerGold,271);
    TestTrue(TEXT("Integrity survives serialization"),FGameXXKSaveStorage::Verify(Loaded));
    Loaded->SaveState.RuntimeState.PlayerGold=272;
    TestFalse(TEXT("Changed payload cannot pass its old checksum"),FGameXXKSaveStorage::Verify(Loaded));
    if(!TestTrue(TEXT("Second commit rotates a verified previous save"),FGameXXKSaveStorage::Write(MakeStorageSave(391),Slot,0,&Error)))return false;
    TArray<uint8> Broken{0,1,2,3,4,5,6,7};
    TestTrue(TEXT("Inject a truncated primary only in the unique fixture"),FFileHelper::SaveArrayToFile(Broken,*FGameXXKSaveStorage::SlotPath(Slot)));
    Loaded=FGameXXKSaveStorage::Load(Slot,0,Recovered,&Error);
    if(!TestNotNull(TEXT("Corrupt primary has a verified fallback"),Loaded))return false;
    TestTrue(TEXT("Recovery is explicit"),Recovered);
    TestEqual(TEXT("Fallback is the preceding valid commit"),Loaded->SaveState.RuntimeState.PlayerGold,271);
    TArray<uint8> Unmodified;
    FFileHelper::LoadFileToArray(Unmodified,*FGameXXKSaveStorage::SlotPath(Slot));
    TestTrue(TEXT("Read-only recovery keeps the damaged primary for review"),Unmodified==Broken);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSaveStorageFaultTest,
    "GameXXK.SaveIntegrity.Storage.InterruptedWriteKeepsPrimary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKSaveStorageFaultTest::RunTest(const FString&)
{
    const FString Slot=TEXT("GameXXK_Automation_StorageFault_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
    ON_SCOPE_EXIT{CleanStorageSlots(Slot);};
    FString Error;
    if(!TestTrue(TEXT("Create original fixture"),FGameXXKSaveStorage::Write(MakeStorageSave(631),Slot,0,&Error)))return false;
    TArray<uint8> Original;FFileHelper::LoadFileToArray(Original,*FGameXXKSaveStorage::SlotPath(Slot));
    for(auto Fault:{FGameXXKSaveStorage::EFault::BeforeTemporaryWrite,FGameXXKSaveStorage::EFault::BeforePrimaryReplace})
    {
        FGameXXKSaveStorage::SetFaultForTest(Fault);
        TestFalse(TEXT("Injected write failure is reported"),FGameXXKSaveStorage::Write(MakeStorageSave(990),Slot,0,&Error));
        TArray<uint8> After;FFileHelper::LoadFileToArray(After,*FGameXXKSaveStorage::SlotPath(Slot));
        TestTrue(TEXT("Original primary bytes stay intact on failure"),After==Original);
        FGameXXKSaveStorage::SetFaultForTest(FGameXXKSaveStorage::EFault::None);
    }
    TestTrue(TEXT("Slot path traversal is rejected"),FGameXXKSaveStorage::SlotPath(TEXT("../outside")).IsEmpty());
    return true;
}
#endif
