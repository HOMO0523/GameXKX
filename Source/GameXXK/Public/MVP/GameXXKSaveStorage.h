#pragma once

#include "CoreMinimal.h"

class USaveGame;
class UGameXXKSaveGame;

/** Windows desktop save files retain the standard UE SaveGame format. */
class GAMEXXK_API FGameXXKSaveStorage final
{
public:
    static bool Write(USaveGame* Save, const FString& Slot, int32 UserIndex, FString* Error=nullptr);
    static UGameXXKSaveGame* Load(const FString& Slot, int32 UserIndex, bool& bRecovered, FString* Error=nullptr);
    static bool Verify(const UGameXXKSaveGame* Save);
    static FString SlotPath(const FString& Slot);
    static constexpr int32 BackupCount=3;
#if WITH_DEV_AUTOMATION_TESTS
    enum class EFault : uint8 { None, BeforeTemporaryWrite, BeforePrimaryReplace };
    static void SetFaultForTest(EFault Fault);
#endif
};
