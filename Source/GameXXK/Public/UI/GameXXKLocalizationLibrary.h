#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "GameXXKLocalizationLibrary.generated.h"

/** Game-only language access for native/Blueprint UI and isolated presentation probes. */
UCLASS()
class GAMEXXK_API UGameXXKLocalizationLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintPure,Category="GameXXK|Localization")
    static FString GetLanguage();
    UFUNCTION(BlueprintCallable,Category="GameXXK|Localization")
    static bool SetLanguage(const FString& Language,bool bPersist=true);
    UFUNCTION(BlueprintCallable,Category="GameXXK|Localization",meta=(DevelopmentOnly))
    static bool ReloadTextCatalog();
    UFUNCTION(BlueprintPure,Category="GameXXK|Localization",meta=(DevelopmentOnly))
    static TArray<FString> GetMissingSources();
    /** Read the mounted UMG tree; report untranslated text and potential overflow for visual review. */
    UFUNCTION(BlueprintCallable,Category="GameXXK|Localization",meta=(DevelopmentOnly))
    static FString AuditTextLayout(class UUserWidget* Root);
};
