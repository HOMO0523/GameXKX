#pragma once

#include "CoreMinimal.h"

/** Game-only language preference and keyed text. Never changes the editor/OS culture. */
namespace GameXXKLocalization
{
    DECLARE_MULTICAST_DELEGATE(FOnLanguageChanged);

    GAMEXXK_API bool Initialize(FString* OutError = nullptr);
    GAMEXXK_API FString GetLanguage();
    GAMEXXK_API bool IsEnglish();
    GAMEXXK_API bool IsSupportedLanguage(const FString& Language);
    GAMEXXK_API FString NormalizeLanguage(const FString& Language);
    GAMEXXK_API bool SetLanguage(const FString& Language, bool bPersist = true, FString* OutError = nullptr);
    GAMEXXK_API uint64 GetRevision();
    GAMEXXK_API FOnLanguageChanged& OnLanguageChanged();

    GAMEXXK_API FText Text(const TCHAR* Key);
    GAMEXXK_API FText Format(const TCHAR* Key, const FFormatNamedArguments& Arguments);
    /** Exact, complete source strings only. Call at a presentation boundary, after rule parsing. */
    GAMEXXK_API FText Source(const FString& NativeText);
    GAMEXXK_API FText Localize(const FText& NativeText);
    GAMEXXK_API TArray<FString> GetMissingSources();

    GAMEXXK_API FString GetPreferencePath();
    /** An explicit path allows tests to exercise persistence without touching a player preference. */
    GAMEXXK_API FString ReadPreference(const FString& Path);
    GAMEXXK_API bool WritePreference(const FString& Language, const FString& Path, FString* OutError = nullptr);
}
