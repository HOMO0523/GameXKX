#include "MVP/GameXXKSaveStorage.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "HAL/PlatformFileManager.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

namespace
{
#if WITH_DEV_AUTOMATION_TESTS
    FGameXXKSaveStorage::EFault InjectedFault=FGameXXKSaveStorage::EFault::None;
#endif
    bool Fail(FString* Error,const TCHAR* Message)
    {if(Error)*Error=Message;return false;}

    uint32 PayloadChecksum(const UGameXXKSaveGame* Save)
    {
        auto* Copy=DuplicateObject<UGameXXKSaveGame>(Save,GetTransientPackage());
        Copy->PayloadChecksum=0;
        TArray<uint8> Bytes;
        FMemoryWriter Writer(Bytes,true);
        FObjectAndNameAsStringProxyArchive Archive(Writer,false);
        Copy->Serialize(Archive);
        return FCrc::MemCrc32(Bytes.GetData(),Bytes.Num());
    }

    bool ValidSlot(const FString& Slot)
    {
        if(Slot.IsEmpty()||Slot.Len()>180||Slot==TEXT(".")||Slot==TEXT(".."))return false;
        for(TCHAR C:Slot)if(C<TEXT(' ')||FString(TEXT("/\\:*?\"<>|")).Contains(FString::Chr(C)))return false;
        return !Slot.EndsWith(TEXT("."))&&!Slot.EndsWith(TEXT(" "));
    }

    bool ReplaceAtomically(const FString& From,const FString& To)
    {
#if PLATFORM_WINDOWS
        FString NativeFrom=FPaths::ConvertRelativePathToFull(From),NativeTo=FPaths::ConvertRelativePathToFull(To);
        FPaths::MakePlatformFilename(NativeFrom);FPaths::MakePlatformFilename(NativeTo);
        return ::MoveFileExW(*NativeFrom,*NativeTo,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
#else
        return false;
#endif
    }

    bool WriteBytes(const FString& Path,const TArray<uint8>& Bytes,bool bPrimary,FString* Error)
    {
        IPlatformFile& Files=FPlatformFileManager::Get().GetPlatformFile();
        if(!Files.CreateDirectoryTree(*FPaths::GetPath(Path)))return Fail(Error,TEXT("Could not create the save directory."));
        const FString Temporary=Path+TEXT(".")+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".writing");
        ON_SCOPE_EXIT {Files.DeleteFile(*Temporary);};
#if WITH_DEV_AUTOMATION_TESTS
        if(InjectedFault==FGameXXKSaveStorage::EFault::BeforeTemporaryWrite)return Fail(Error,TEXT("Injected temporary write failure."));
#endif
        TUniquePtr<IFileHandle> Handle(Files.OpenWrite(*Temporary));
        if(!Handle||!Handle->Write(Bytes.GetData(),Bytes.Num())||!Handle->Flush(true))return Fail(Error,TEXT("Could not write and flush the temporary save."));
        Handle.Reset();
        TArray<uint8> ReadBack;
        if(!FFileHelper::LoadFileToArray(ReadBack,*Temporary)||ReadBack!=Bytes)return Fail(Error,TEXT("Temporary save verification failed."));
#if WITH_DEV_AUTOMATION_TESTS
        if(bPrimary&&InjectedFault==FGameXXKSaveStorage::EFault::BeforePrimaryReplace)return Fail(Error,TEXT("Injected primary replacement failure."));
#endif
        if(!ReplaceAtomically(Temporary,Path))return Fail(Error,TEXT("Could not replace the save file; the previous file remains in place."));
        return true;
    }

    bool ValidRuntime(const UGameXXKSaveGame* Save)
    {
        if(!FGameXXKSaveStorage::Verify(Save))return false;
        FGameXXKSaveState Migrated;FGameXXKSaveMigrationReport Report;
        return FGameXXKSaveMigration::MigrateToCurrent(Save->SaveState,Migrated,Report);
    }

    UGameXXKSaveGame* Read(const FString& Path)
    {
        if(Path.IsEmpty()||!FPlatformFileManager::Get().GetPlatformFile().FileExists(*Path))return nullptr;
        TArray<uint8> Bytes;
        if(!FFileHelper::LoadFileToArray(Bytes,*Path)||Bytes.Num()<16||Bytes.Num()>64*1024*1024)return nullptr;
        // Reject obvious truncation/foreign files before handing them to UE's archive reader.
        const uint32 Magic=static_cast<uint32>(Bytes[0])|(static_cast<uint32>(Bytes[1])<<8)|(static_cast<uint32>(Bytes[2])<<16)|(static_cast<uint32>(Bytes[3])<<24);
        if(Magic!=0x53415647)return nullptr;
        return Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
    }
}

FString FGameXXKSaveStorage::SlotPath(const FString& Slot)
{return ValidSlot(Slot)?FPaths::Combine(FPaths::ProjectSavedDir(),TEXT("SaveGames"),Slot+TEXT(".sav")):FString();}

bool FGameXXKSaveStorage::Verify(const UGameXXKSaveGame* Save)
{
    if(!Save)return false;
    if(Save->IntegritySchema==0)return true;
    if(Save->IntegritySchema!=1)return false;
    return PayloadChecksum(Save)==Save->PayloadChecksum;
}

bool FGameXXKSaveStorage::Write(USaveGame* Save,const FString& Slot,int32 UserIndex,FString* Error)
{
    if(Error)Error->Reset();
    const FString Path=SlotPath(Slot);
    if(Path.IsEmpty()||UserIndex<0||!Save)return Fail(Error,TEXT("Invalid save slot."));
    UGameXXKSaveGame* Typed=Cast<UGameXXKSaveGame>(Save);
    if(!Typed)return Fail(Error,TEXT("Unexpected save object type."));
    // Preserve a legacy migration backup byte-for-byte. New runtime snapshots use the
    // current schema and receive an integrity seal without mutating the caller's object.
    UGameXXKSaveGame* Copy=DuplicateObject<UGameXXKSaveGame>(Typed,GetTransientPackage());
    TArray<uint8> Bytes;
    if(Copy->SaveState.SaveVersion==FGameXXKSaveMigration::CurrentSaveVersion)
    {
        Copy->IntegritySchema=1;Copy->PayloadChecksum=0;
        // The first persistent serialization can assign identities to freshly
        // created FText (for example encounter unit names). Seal the round-trip
        // representation, otherwise those generated keys invalidate our own
        // checksum when the save is read back. The caller and legacy verifier
        // remain unchanged.
        TArray<uint8> CanonicalBytes;
        if(!UGameplayStatics::SaveGameToMemory(Copy,CanonicalBytes))return Fail(Error,TEXT("Save canonicalization failed."));
        Copy=Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(CanonicalBytes));
        if(!Copy)return Fail(Error,TEXT("Canonical save could not be read back."));
        Copy->PayloadChecksum=PayloadChecksum(Copy);
    }
    if(!UGameplayStatics::SaveGameToMemory(Copy,Bytes))return Fail(Error,TEXT("Save serialization failed."));
    if(!Verify(Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes))))return Fail(Error,TEXT("Serialized save did not pass integrity verification."));

    if(const UGameXXKSaveGame* Previous=Read(Path);Previous&&ValidRuntime(Previous))
    {
        for(int32 Index=BackupCount;Index>=1;--Index)
        {
            const FString Source=Index==1?Path:SlotPath(Slot+FString::Printf(TEXT(".Previous%d"),Index-1));
            const auto* SourceSave=Read(Source);
            if(!SourceSave||!ValidRuntime(SourceSave))continue;
            TArray<uint8> PreviousBytes;
            if(!FFileHelper::LoadFileToArray(PreviousBytes,*Source)||!WriteBytes(SlotPath(Slot+FString::Printf(TEXT(".Previous%d"),Index)),PreviousBytes,false,Error))return false;
        }
    }
    return WriteBytes(Path,Bytes,true,Error);
}

UGameXXKSaveGame* FGameXXKSaveStorage::Load(const FString& Slot,int32 UserIndex,bool& bRecovered,FString* Error)
{
    bRecovered=false;if(Error)Error->Reset();
    const FString Path=SlotPath(Slot);
    if(Path.IsEmpty()||UserIndex<0){Fail(Error,TEXT("Invalid save slot."));return nullptr;}
    UGameXXKSaveGame* Primary=Read(Path);
    // A future version is not corruption and must never be replaced by an older backup.
    if(Primary&&Primary->SaveState.SaveVersion>FGameXXKSaveMigration::CurrentSaveVersion)return Primary;
    // The version dispatcher must preserve and verify a migration backup before it
    // rejects a readable legacy payload. Storage must not pre-empt that transaction.
    if(Primary&&Verify(Primary)&&Primary->SaveState.SaveVersion<FGameXXKSaveMigration::CurrentSaveVersion)return Primary;
    if(Primary&&ValidRuntime(Primary))return Primary;
    for(int32 Index=1;Index<=BackupCount;++Index)
    {
        UGameXXKSaveGame* Backup=Read(SlotPath(Slot+FString::Printf(TEXT(".Previous%d"),Index)));
        if(Backup&&ValidRuntime(Backup)){bRecovered=true;return Backup;}
    }
    if(Primary&&Verify(Primary))return Primary; // Let semantic validation provide its precise error.
    Fail(Error,TEXT("No valid save or backup was found."));return nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void FGameXXKSaveStorage::SetFaultForTest(EFault Fault){InjectedFault=Fault;}
#endif
