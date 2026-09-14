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
#include "Serialization/MemoryReader.h"
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

    constexpr uint64 EnvelopeMagic = 0x32455641534B5858ull;
    constexpr int32 FooterSize = 20;
    constexpr int32 MaximumSaveBytes = 64 * 1024 * 1024;
    int64 LastCommitTicks = 0; // All storage entry points run on the game thread.

    void Seal(TArray<uint8>& Bytes, FGameXXKSaveCommit& Commit)
    {
        const uint32 PayloadSize = Bytes.Num();
        FMemoryWriter Writer(Bytes, true);
        Writer.Seek(Bytes.Num());
        Writer << Commit.ProfileId << Commit.OwnerSlot << Commit.Revision << Commit.UtcTicks;
        uint32 MetadataSize = Bytes.Num() - PayloadSize;
        uint32 Size = PayloadSize;
        Writer << Size << MetadataSize;
        uint32 Checksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
        uint64 Magic = EnvelopeMagic;
        Writer << Checksum << Magic;
    }

    UGameXXKSaveGame* Decode(const TArray<uint8>& Bytes, FGameXXKSaveCommit* OutCommit)
    {
        if (OutCommit) *OutCommit = {};
        if (Bytes.Num() < 16 || Bytes.Num() > MaximumSaveBytes) return nullptr;
        FMemoryReader Reader(Bytes, true);
        uint32 GvasMagic = 0; Reader << GvasMagic;
        if (GvasMagic != 0x53415647) return nullptr;
        uint64 Magic = 0;
        Reader.Seek(Bytes.Num() - sizeof(Magic)); Reader << Magic;
        FGameXXKSaveCommit Commit;
        if (Magic == EnvelopeMagic)
        {
            if (Bytes.Num() < FooterSize) return nullptr;
            uint32 PayloadSize = 0, MetadataSize = 0, Checksum = 0;
            Reader.Seek(Bytes.Num() - FooterSize);
            Reader << PayloadSize << MetadataSize << Checksum;
            if (PayloadSize < 16 || MetadataSize > 2048 ||
                uint64(PayloadSize) + MetadataSize + FooterSize != uint64(Bytes.Num()) ||
                FCrc::MemCrc32(Bytes.GetData(), Bytes.Num() - 12) != Checksum) return nullptr;
            TArray<uint8> Metadata;
            Metadata.Append(Bytes.GetData() + PayloadSize, MetadataSize);
            FMemoryReader MetadataReader(Metadata, true);
            MetadataReader.ArMaxSerializeSize = 2048;
            MetadataReader << Commit.ProfileId << Commit.OwnerSlot << Commit.Revision << Commit.UtcTicks;
            if (MetadataReader.IsError() || MetadataReader.Tell() != Metadata.Num() ||
                !Commit.ProfileId.IsValid() || !ValidSlot(Commit.OwnerSlot) ||
                Commit.Revision <= 0 || Commit.UtcTicks <= 0) return nullptr;
            Commit.bSealed = true;
        }
        auto* Save = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
        if (!Save) return nullptr;
        if (Commit.bSealed)
        {
            if (Save->IntegritySchema != 2) return nullptr;
            // The raw bytes were checked BEFORE deserialization. Seal only this in-memory
            // representation for callers' mutation checks; never compare new class layouts
            // with the old program's serialized checksum.
            Save->IntegritySchema = 1;
            Save->PayloadChecksum = PayloadChecksum(Save);
        }
        else if (Save->IntegritySchema == 2) return nullptr; // Missing/damaged footer, never a legacy save.
        if (OutCommit) *OutCommit = Commit;
        return Save;
    }

    UGameXXKSaveGame* Read(const FString& Path, FGameXXKSaveCommit* OutCommit=nullptr)
    {
        if (OutCommit) *OutCommit = {};
        auto& Files = FPlatformFileManager::Get().GetPlatformFile();
        const int64 Size = Path.IsEmpty() ? -1 : Files.FileSize(*Path);
        if (Size < 16 || Size > MaximumSaveBytes) return nullptr;
        TArray<uint8> Bytes;
        if (!FFileHelper::LoadFileToArray(Bytes, *Path)) return nullptr;
        auto* Save = Decode(Bytes, OutCommit);
        if (Save && OutCommit && !OutCommit->bSealed)
            OutCommit->UtcTicks = Files.GetTimeStamp(*Path).GetTicks();
        return Save;
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

bool FGameXXKSaveStorage::Write(USaveGame* Save,const FString& Slot,int32 UserIndex,FString* Error,FGameXXKSaveCommit* InOutCommit)
{
    if(Error)Error->Reset();
    const FString Path=SlotPath(Slot);
    if(Path.IsEmpty()||UserIndex<0||!Save)return Fail(Error,TEXT("Invalid save slot."));
    UGameXXKSaveGame* Typed=Cast<UGameXXKSaveGame>(Save);
    if(!Typed)return Fail(Error,TEXT("Unexpected save object type."));
    FGameXXKSaveCommit PreviousCommit;
    const auto* Existing = Read(Path, &PreviousCommit);
    if (Existing && Existing->SaveState.SaveVersion > FGameXXKSaveMigration::CurrentSaveVersion)
        return Fail(Error,TEXT("A newer game version owns this save; refusing to overwrite it."));
    FGameXXKSaveCommit Commit = InOutCommit ? *InOutCommit : PreviousCommit;
    if (!Commit.ProfileId.IsValid()) Commit.ProfileId = FGuid::NewGuid();
    if (Commit.OwnerSlot.IsEmpty()) Commit.OwnerSlot = Slot;
    if (!ValidSlot(Commit.OwnerSlot)) return Fail(Error,TEXT("Invalid owner slot."));
    const int64 LastRevision = FMath::Max(Commit.Revision, PreviousCommit.Revision);
    const int64 LastTicks = FMath::Max3(Commit.UtcTicks, PreviousCommit.UtcTicks, LastCommitTicks);
    if (LastRevision == MAX_int64 || LastTicks == MAX_int64) return Fail(Error,TEXT("Invalid commit sequence."));
    Commit.Revision = LastRevision + 1;
    Commit.UtcTicks = FMath::Max(FDateTime::UtcNow().GetTicks(), LastTicks + 1);
    Commit.bSealed = true;
    UGameXXKSaveGame* Copy = DuplicateObject<UGameXXKSaveGame>(Typed,GetTransientPackage());
    Copy->IntegritySchema = 2;
    Copy->PayloadChecksum = 0;
    TArray<uint8> Bytes;
    if (!UGameplayStatics::SaveGameToMemory(Copy, Bytes)) return Fail(Error,TEXT("Save serialization failed."));
    Seal(Bytes, Commit);
    if (!Verify(Decode(Bytes, nullptr))) return Fail(Error,TEXT("Serialized save did not pass integrity verification."));

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
    if (!WriteBytes(Path,Bytes,true,Error)) return false;
    LastCommitTicks = Commit.UtcTicks;
    if (InOutCommit) *InOutCommit = Commit;
    return true;
}

UGameXXKSaveGame* FGameXXKSaveStorage::Load(const FString& Slot,int32 UserIndex,bool& bRecovered,FString* Error,FGameXXKSaveCommit* OutCommit)
{
    bRecovered=false;if(Error)Error->Reset();if(OutCommit)*OutCommit={};
    const FString Path=SlotPath(Slot);
    if(Path.IsEmpty()||UserIndex<0){Fail(Error,TEXT("Invalid save slot."));return nullptr;}
    UGameXXKSaveGame* Primary=Read(Path,OutCommit);
    // A future version is not corruption and must never be replaced by an older backup.
    if(Primary&&Primary->SaveState.SaveVersion>FGameXXKSaveMigration::CurrentSaveVersion)return Primary;
    // The version dispatcher must preserve and verify a migration backup before it
    // rejects a readable legacy payload. Storage must not pre-empt that transaction.
    if(Primary&&Verify(Primary)&&Primary->SaveState.SaveVersion<FGameXXKSaveMigration::CurrentSaveVersion)return Primary;
    if(Primary&&ValidRuntime(Primary))return Primary;
    for(int32 Index=1;Index<=BackupCount;++Index)
    {
        FGameXXKSaveCommit BackupCommit;
        UGameXXKSaveGame* Backup=Read(SlotPath(Slot+FString::Printf(TEXT(".Previous%d"),Index)),&BackupCommit);
        if(Backup&&ValidRuntime(Backup)){bRecovered=true;if(OutCommit)*OutCommit=BackupCommit;return Backup;}
    }
    if(Primary&&Verify(Primary))return Primary; // Let semantic validation provide its precise error.
    Fail(Error,TEXT("No valid save or backup was found."));return nullptr;
}

#if WITH_DEV_AUTOMATION_TESTS
void FGameXXKSaveStorage::SetFaultForTest(EFault Fault){InjectedFault=Fault;}
#endif

FString FGameXXKSaveStorage::SelectResumeSlot(const FString& RegularSlot,const FString& CheckpointSlot)
{
    bool Recovered=false;
    FGameXXKSaveCommit RegularCommit, CheckpointCommit;
    auto* Regular=Load(RegularSlot,0,Recovered,nullptr,&RegularCommit);
    auto* Checkpoint=Load(CheckpointSlot,0,Recovered,nullptr,&CheckpointCommit);
    if (Regular && Regular->SaveState.SaveVersion > FGameXXKSaveMigration::CurrentSaveVersion) return RegularSlot;
    const bool HasRegular=Regular && ValidRuntime(Regular);
    const bool HasCheckpoint=Checkpoint && ValidRuntime(Checkpoint);
    if (CheckpointCommit.bSealed && CheckpointCommit.OwnerSlot != RegularSlot) return RegularSlot;
    // Once a main slot has an explicit generation, an unowned legacy checkpoint
    // must not resurrect another character, even if its filesystem time is ahead.
    if (HasRegular && RegularCommit.bSealed && !CheckpointCommit.bSealed) return RegularSlot;
    if (HasRegular && RegularCommit.bSealed && CheckpointCommit.bSealed &&
        RegularCommit.ProfileId != CheckpointCommit.ProfileId) return RegularSlot;
    if (Checkpoint && Checkpoint->SaveState.SaveVersion > FGameXXKSaveMigration::CurrentSaveVersion) return CheckpointSlot;
    if (!HasCheckpoint) return RegularSlot;
    if (!HasRegular) return CheckpointSlot;
    if (RegularCommit.bSealed && CheckpointCommit.bSealed)
        return CheckpointCommit.Revision > RegularCommit.Revision ? CheckpointSlot : RegularSlot;
    // Legacy files have no generation counter. Use the selected copy's file time,
    // including when Load recovered a backup, instead of blindly preferring checkpoints.
    return CheckpointCommit.UtcTicks > RegularCommit.UtcTicks ? CheckpointSlot : RegularSlot;
}
