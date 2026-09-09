#include "UI/GameXXKLocalization.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    constexpr const TCHAR* SettingsSection = TEXT("GameXXK.Localization");

    struct FEntry
    {
        FString Namespace;
        FString Key;
        FString Chinese;
        FString English;
    };

    struct FCatalog
    {
        TArray<FEntry> Entries;
        TMap<FString, int32> ByKey;
        TMap<FString, int32> BySource;
        TSet<FString> MissingSources;
        FString Language = TEXT("zh-Hans");
        uint64 Revision = 0;
        bool bInitialized = false;
        GameXXKLocalization::FOnLanguageChanged Changed;
    };

    FCatalog& Catalog()
    {
        static FCatalog Value;
        return Value;
    }

    bool Fail(FString* Error, const FString& Message)
    {
        if (Error) *Error = Message;
        return false;
    }

    void ApplyLanguage()
    {
        const FCatalog& Data = Catalog();
        FTextLocalizationResource Resource;
        for (const FEntry& Entry : Data.Entries)
        {
            Resource.AddEntry(FTextKey(Entry.Namespace), FTextKey(Entry.Key), Entry.Chinese,
                Data.Language == TEXT("en") ? Entry.English : Entry.Chinese, 0);
        }
        // A scoped game resource updates existing FText/Format histories in PIE and packaged games.
        // Changing FInternationalization's language here would also change the editor chrome.
        FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    }

    FText EntryText(const FEntry& Entry)
    {
        return FText::AsLocalizable_Advanced(FTextKey(Entry.Namespace), FTextKey(Entry.Key), Entry.Chinese);
    }

    bool HasChinese(const FString& Text)
    {
        for (const TCHAR Char : Text)
            if (Char >= 0x3400 && Char <= 0x9fff) return true;
        return false;
    }
}

bool GameXXKLocalization::Initialize(FString* OutError)
{
    FCatalog& Data = Catalog();
    if (Data.bInitialized) return true;
    FString Json;
    const FString Path = FPaths::Combine(FPaths::ProjectContentDir(), TEXT("Localization/GameXXK/strings.json"));
    if (!FFileHelper::LoadFileToString(Json, *Path))
        return Fail(OutError, TEXT("Could not read the GameXXK language catalogue."));
    TSharedPtr<FJsonObject> Root;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root.IsValid())
        return Fail(OutError, TEXT("The GameXXK language catalogue is not valid JSON."));
    const TArray<TSharedPtr<FJsonValue>>* JsonEntries = nullptr;
    if (!Root->TryGetArrayField(TEXT("entries"), JsonEntries) || JsonEntries->IsEmpty())
        return Fail(OutError, TEXT("The GameXXK language catalogue has no entries."));

    // Validate before replacing a catalogue. A malformed entry must not leave a partially loaded table.
    TArray<FEntry> Entries;
    TMap<FString, int32> ByKey;
    TMap<FString, int32> BySource;
    TSet<FString> Identities;
    for (const TSharedPtr<FJsonValue>& Value : *JsonEntries)
    {
        const TSharedPtr<FJsonObject>* Object = nullptr;
        if (!Value.IsValid() || !Value->TryGetObject(Object) || !Object->IsValid())
            return Fail(OutError, TEXT("A language catalogue entry is not an object."));
        FEntry Entry;
        Entry.Namespace = TEXT("GameXXK");
        (*Object)->TryGetStringField(TEXT("namespace"), Entry.Namespace);
        if (!(*Object)->TryGetStringField(TEXT("key"), Entry.Key) || Entry.Key.IsEmpty()
            || !(*Object)->TryGetStringField(TEXT("zh-Hans"), Entry.Chinese) || Entry.Chinese.IsEmpty()
            || !(*Object)->TryGetStringField(TEXT("en"), Entry.English) || Entry.English.IsEmpty())
            return Fail(OutError, TEXT("A language catalogue entry is missing its key or translation."));
        const FString Identity = Entry.Namespace + TEXT("::") + Entry.Key;
        if (Identities.Contains(Identity)) return Fail(OutError, TEXT("Duplicate language catalogue identity: ") + Identity);
        Identities.Add(Identity);
        const int32 Index = Entries.Add(Entry);
        if (Entry.Namespace == TEXT("GameXXK")) ByKey.Add(Entry.Key, Index);
        if (!BySource.Contains(Entry.Chinese)) BySource.Add(Entry.Chinese, Index);
    }
    Data.Entries = MoveTemp(Entries);
    Data.ByKey = MoveTemp(ByKey);
    Data.BySource = MoveTemp(BySource);
    Data.Language = ReadPreference(GetPreferencePath());
    Data.bInitialized = true;
    ApplyLanguage();
    ++Data.Revision;
    return true;
}

bool GameXXKLocalization::IsSupportedLanguage(const FString& Language)
{
    const FString Code = Language.TrimStartAndEnd().Replace(TEXT("_"), TEXT("-")).ToLower();
    return Code == TEXT("en") || Code.StartsWith(TEXT("en-"))
        || Code == TEXT("zh") || Code == TEXT("zh-hans") || Code.StartsWith(TEXT("zh-hans-")) || Code == TEXT("zh-cn");
}

FString GameXXKLocalization::NormalizeLanguage(const FString& Language)
{
    const FString Code = Language.TrimStartAndEnd().Replace(TEXT("_"), TEXT("-")).ToLower();
    return Code == TEXT("en") || Code.StartsWith(TEXT("en-")) ? TEXT("en") : TEXT("zh-Hans");
}

FString GameXXKLocalization::GetLanguage() { Initialize(); return Catalog().Language; }
bool GameXXKLocalization::IsEnglish() { return GetLanguage() == TEXT("en"); }
uint64 GameXXKLocalization::GetRevision() { Initialize(); return Catalog().Revision; }
GameXXKLocalization::FOnLanguageChanged& GameXXKLocalization::OnLanguageChanged() { return Catalog().Changed; }

bool GameXXKLocalization::SetLanguage(const FString& Language, bool bPersist, FString* OutError)
{
    if (OutError) OutError->Reset();
    if (!IsSupportedLanguage(Language)) return Fail(OutError, TEXT("Only Simplified Chinese and English are supported."));
    if (!Initialize(OutError)) return false;
    const FString Normalized = NormalizeLanguage(Language);
    if (bPersist && !WritePreference(Normalized, GetPreferencePath(), OutError)) return false;
    FCatalog& Data = Catalog();
    if (Data.Language == Normalized) return true;
    Data.Language = Normalized;
    ApplyLanguage();
    ++Data.Revision;
    Data.Changed.Broadcast();
    return true;
}

FText GameXXKLocalization::Text(const TCHAR* Key)
{
    if (Initialize())
    {
        const FCatalog& Data = Catalog();
        if (const int32* Index = Data.ByKey.Find(Key)) return EntryText(Data.Entries[*Index]);
    }
    // Missing keys are explicit in diagnostics and must fail the catalogue coverage check.
    Catalog().MissingSources.Add(FString(TEXT("Key:")) + Key);
    return FText::FromString(Key);
}

FText GameXXKLocalization::Format(const TCHAR* Key, const FFormatNamedArguments& Arguments)
{
    return FText::Format(Text(Key), Arguments);
}

FText GameXXKLocalization::Source(const FString& NativeText)
{
    if (Initialize())
    {
        FCatalog& Data = Catalog();
        if (const int32* Index = Data.BySource.Find(NativeText)) return EntryText(Data.Entries[*Index]);
        if (Data.Language == TEXT("en") && HasChinese(NativeText) && Data.MissingSources.Num() < 512) Data.MissingSources.Add(NativeText);
    }
    return FText::FromString(NativeText);
}

FText GameXXKLocalization::Localize(const FText& NativeText)
{
    Initialize();
    const FString SourceString = NativeText.BuildSourceString();
    if (const int32* Index = Catalog().BySource.Find(SourceString)) return EntryText(Catalog().Entries[*Index]);
    if (IsEnglish() && HasChinese(NativeText.ToString()) && Catalog().MissingSources.Num() < 512) Catalog().MissingSources.Add(SourceString);
    return NativeText;
}

TArray<FString> GameXXKLocalization::GetMissingSources()
{
    TArray<FString> Result = Catalog().MissingSources.Array();
    Result.Sort();
    return Result;
}

FString GameXXKLocalization::GetPreferencePath()
{
#if WITH_EDITOR
    // The editor's -UserDir is an automation detail, not a second set of player preferences.
    return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Saved/Config/GameXXKLanguageSettings.ini")));
#else
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("Config/GameXXKLanguageSettings.ini"));
#endif
}

FString GameXXKLocalization::ReadPreference(const FString& Path)
{
    FConfigFile Config;
    Config.Read(Path);
    FString Language;
    Config.GetString(SettingsSection, TEXT("Language"), Language);
    return NormalizeLanguage(Language);
}

bool GameXXKLocalization::WritePreference(const FString& Language, const FString& Path, FString* OutError)
{
    if (OutError) OutError->Reset();
    if (!IsSupportedLanguage(Language)) return Fail(OutError, TEXT("Unsupported language preference."));
    if (Path.IsEmpty()) return Fail(OutError, TEXT("The language preference path is empty."));
    if (!IFileManager::Get().MakeDirectory(*FPaths::GetPath(Path), true))
        return Fail(OutError, TEXT("Could not create the language preference directory."));
    FConfigFile Config;
    Config.Read(Path);
    Config.SetString(SettingsSection, TEXT("Language"), *NormalizeLanguage(Language));
    Config.Dirty = true;
    const FString TemporaryPath = Path + TEXT(".") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".tmp");
    const bool bWritten = Config.Write(TemporaryPath);
    const bool bReplaced = bWritten && IFileManager::Get().Move(*Path, *TemporaryPath, true, false, false, true);
    if (!bReplaced)
    {
        IFileManager::Get().Delete(*TemporaryPath);
        return Fail(OutError, TEXT("Could not save the language preference."));
    }
    return true;
}
