#pragma once

#include "CoreMinimal.h"

class USaveGame;
class UGameXXKSaveGame;

/** Disk metadata is outside the UE payload, so adding it cannot change legacy object checksums. */
struct GAMEXXK_API FGameXXKSaveCommit
{
    FGuid ProfileId;
    FString OwnerSlot;
    int64 Revision = 0;
    int64 UtcTicks = 0;
    bool bSealed = false;
};

/** Windows desktop save files retain the standard UE SaveGame format. */
class GAMEXXK_API FGameXXKSaveStorage final
{
public:
    static bool Write(USaveGame* Save, const FString& Slot, int32 UserIndex, FString* Error=nullptr,
        FGameXXKSaveCommit* InOutCommit=nullptr);
    static UGameXXKSaveGame* Load(const FString& Slot, int32 UserIndex, bool& bRecovered, FString* Error=nullptr,
        FGameXXKSaveCommit* OutCommit=nullptr);
    static FString SelectResumeSlot(const FString& RegularSlot, const FString& CheckpointSlot);
    static bool Verify(const UGameXXKSaveGame* Save);
    static FString SlotPath(const FString& Slot);
    static constexpr int32 BackupCount=3;
#if WITH_DEV_AUTOMATION_TESTS
    enum class EFault : uint8 { None, BeforeTemporaryWrite, BeforePrimaryReplace };
    static void SetFaultForTest(EFault Fault);
#endif
};
