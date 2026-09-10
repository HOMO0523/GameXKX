#include "UI/GameXXKLocalization.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Internationalization/TextLocalizationManager.h"
#include "Internationalization/TextLocalizationResource.h"
#include "Internationalization/Regex.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Components/TextBlock.h"
#include "Framework/Application/SlateApplication.h"
#include "Fonts/FontMeasure.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/UObjectIterator.h"

namespace
{
    constexpr const TCHAR* SettingsSection = TEXT("GameXXK.Localization");

    struct FEntry
    {
        FString Namespace;
        FString Key;
        FString Chinese;
        FString English;
        FString NativeSource;
        FString NativePattern;
        TArray<FString> ArgumentTypes;
        TSharedPtr<FRegexPattern> Matcher;
        int32 LiteralLength=0;
        bool bCompact=false;
    };

    struct FCatalog
    {
        TArray<FEntry> Entries;
        TMap<FString, int32> ByKey;
        TMap<FString, int32> BySource;
        TMap<FString, int32> ByCompactSource;
        TSet<FString> Identities;
        TSet<FString> MissingSources;
        TArray<int32> Templates;
        TMap<FString,FText> DisplayCache;
        FString Language = TEXT("zh-Hans");
        uint64 Revision = 0;
        bool bInitialized = false;
        bool bReloading=false;
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
            Resource.AddEntry(FTextKey(Entry.Namespace), FTextKey(Entry.Key), Entry.NativeSource,
                Data.Language == TEXT("en") ? Entry.English : Entry.Chinese, 0);
        }
        // A scoped game resource updates existing FText/Format histories in PIE and packaged games.
        // Changing FInternationalization's language here would also change the editor chrome.
        FTextLocalizationManager::Get().UpdateFromLocalizationResource(Resource);
    }

    FText EntryText(const FEntry& Entry)
    {
        return FText::AsLocalizable_Advanced(FTextKey(Entry.Namespace), FTextKey(Entry.Key), Entry.NativeSource);
    }

    bool HasChinese(const FString& Text)
    {
        for (const TCHAR Char : Text)
            if (Char >= 0x3400 && Char <= 0x9fff) return true;
        return false;
    }

    FString EscapePattern(const FString& Value)
    {
        FString Escaped;
        const FString Special(TEXT("\\.^$|()[]{}*+?"));
        for(TCHAR C:Value)
        {
            if(Special.Contains(FString::Chr(C)))Escaped.AppendChar(TEXT('\\'));
            Escaped.AppendChar(C);
        }
        return Escaped;
    }

    bool PrepareTemplate(FEntry& Entry)
    {
        const FRegexPattern Printf(TEXT("%(?:%|[-+ #0]*[0-9]*(?:\\.[0-9]+)?(?:ll|l|h|z)?[diuoxXfFeEgGcs])"));
        FRegexMatcher Tokens(Printf,Entry.NativePattern);
        FString Pattern(TEXT("\\A"));
        FString NativeFormat;
        int32 End=0;
        while(Tokens.FindNext())
        {
            const FString Literal=Entry.NativePattern.Mid(End,Tokens.GetMatchBeginning()-End);
            Pattern+=EscapePattern(Literal);NativeFormat+=Literal;
            Entry.LiteralLength+=Literal.Len();
            const FString Spec=Tokens.GetCaptureGroup(0);
            End=Tokens.GetMatchEnding();
            if(Spec==TEXT("%%")){Pattern+=TEXT("%");NativeFormat+=TEXT("%");++Entry.LiteralLength;continue;}
            const TCHAR Type=Spec[Spec.Len()-1];
            NativeFormat+=FString::Printf(TEXT("{%d}"),Entry.ArgumentTypes.Num());
            Entry.ArgumentTypes.Add(Spec);
            if(Type==TEXT('s'))Pattern+=Entry.NativePattern.Contains(TEXT("\n"))?TEXT("([\\s\\S]*?)"):TEXT("([^\\r\\n；;]*?)");
            else if(Type==TEXT('c'))Pattern+=TEXT("([\\s\\S])");
            else if(Type==TEXT('d')||Type==TEXT('i'))Pattern+=Spec.Contains(TEXT("+"))?TEXT("([+-][0-9]+)"):TEXT("([+-]?[0-9]+)");
            else if(Type==TEXT('u'))Pattern+=TEXT("([0-9]+)");
            else if(Type==TEXT('x')||Type==TEXT('X'))Pattern+=TEXT("([0-9a-fA-F]+)");
            else if(Type==TEXT('o'))Pattern+=TEXT("([0-7]+)");
            else Pattern+=TEXT("([+-]?[0-9]+(?:\\.[0-9]+)?(?:[eE][+-]?[0-9]+)?)");
        }
        const FString Tail=Entry.NativePattern.Mid(End);
        // Compact card copy removes a final full stop before joining clauses.
        // Accept that presentation-only variant while preserving all typed arguments.
        Pattern+=(Tail.EndsWith(TEXT("。")) ? EscapePattern(Tail.LeftChop(1))+TEXT("(?:。)?") : EscapePattern(Tail))+TEXT("\\z");
        NativeFormat+=Tail;Entry.LiteralLength+=Tail.Len();
        if(Entry.ArgumentTypes.IsEmpty()||Entry.LiteralLength==0||NativeFormat!=Entry.NativeSource)return false;
        Entry.Matcher=MakeShared<FRegexPattern>(Pattern);
        return true;
    }

    FText ResolveDisplay(const FString& Source,int32 Depth)
    {
        FCatalog& Data=Catalog();
        if(const int32* Index=Data.BySource.Find(Source))return EntryText(Data.Entries[*Index]);
        if(!Source.EndsWith(TEXT("。")))
            if(const int32* Index=Data.BySource.Find(Source+TEXT("。")))return EntryText(Data.Entries[*Index]);
        if(const FText* Cached=Data.DisplayCache.Find(Source))return *Cached;
        if(Depth<6&&Source.Len()<=16384&&HasChinese(Source))
        {
            for(int32 Index:Data.Templates)
            {
                const FEntry& Entry=Data.Entries[Index];
                FRegexMatcher Matcher(*Entry.Matcher,Source);
                if(!Matcher.FindNext())continue;
                FFormatOrderedArguments Arguments;
                for(int32 Argument=0;Argument<Entry.ArgumentTypes.Num();++Argument)
                {
                    const FString Capture=Matcher.GetCaptureGroup(Argument+1);
                    const FString& Spec=Entry.ArgumentTypes[Argument];
                    Arguments.Add(Spec.EndsWith(TEXT("s"))?ResolveDisplay(Capture,Depth+1):FText::FromString(Capture));
                }
                const FText Result=FText::Format(EntryText(Entry),Arguments);
                if(Data.DisplayCache.Num()>=2048)Data.DisplayCache.Reset();
                Data.DisplayCache.Add(Source,Result);
                return Result;
            }
            if(Source.Contains(TEXT("\n")))
            {
                TArray<FString> Lines;Source.ParseIntoArray(Lines,TEXT("\n"),false);
                FString Pattern;FFormatOrderedArguments Arguments;
                for(int32 Index=0;Index<Lines.Num();++Index)
                {
                    if(Index>0)Pattern+=TEXT("\n");
                    Pattern+=FString::Printf(TEXT("{%d}"),Index);
                    Arguments.Add(Lines[Index].IsEmpty()?FText::GetEmpty():ResolveDisplay(Lines[Index],Depth+1));
                }
                const FText Result=FText::Format(FText::FromString(Pattern),Arguments);
                if(Data.DisplayCache.Num()>=2048)Data.DisplayCache.Reset();
                Data.DisplayCache.Add(Source,Result);
                return Result;
            }
            // Split only sentence-level compact separators after complete templates
            // have had a chance to match. Never split inside a condition or Pill name.
            if(Source.Contains(TEXT("；")))
            {
                TArray<FString> Clauses;Source.ParseIntoArray(Clauses,TEXT("；"),false);
                FText Result=ResolveDisplay(Clauses[0],Depth+1);
                for(int32 Index=1;Index<Clauses.Num();++Index)
                    Result=FText::Format(GameXXKLocalization::Text(TEXT("Localization.CompactClauseJoin")),
                        Result,Clauses[Index].IsEmpty()?FText::GetEmpty():ResolveDisplay(Clauses[Index],Depth+1));
                if(Data.DisplayCache.Num()>=2048)Data.DisplayCache.Reset();
                Data.DisplayCache.Add(Source,Result);
                return Result;
            }
        }
        if(Data.Language==TEXT("en")&&HasChinese(Source)&&Data.MissingSources.Num()<512)Data.MissingSources.Add(Source);
        return FText::FromString(Source);
    }

    void RefreshGameTextMaterialAspects()
    {
        if(!FSlateApplication::IsInitialized())return;
        const auto Measure=FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
        for(TObjectIterator<UTextBlock> It;It;++It)
        {
            const FSlateFontInfo Font=It->GetFont();
            if(!Font.FontObject||!Font.FontObject->GetPathName().StartsWith(TEXT("/Game/GameXXK/UI/Fonts/")))continue;
            It->SetText(GameXXKLocalization::Localize(It->GetText()));
            const float Aspect=FMath::Max(1.0f,static_cast<float>(Measure->Measure(It->GetText(),Font).X)/FMath::Max(1.0f,static_cast<float>(Measure->GetMaxCharacterHeight(Font))));
            for(UObject* Resource:{Font.FontMaterial.Get(),Font.OutlineSettings.OutlineMaterial.Get()})
            {
                auto* Material=Cast<UMaterialInstanceDynamic>(Resource);float Existing=0;
                if(Material&&Material->GetScalarParameterValue(FMaterialParameterInfo(TEXT("TextAspect")),Existing))
                    Material->SetScalarParameterValue(TEXT("TextAspect"),Aspect);
            }
        }
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
    TMap<FString, int32> ByCompactSource;
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
        Entry.NativeSource=Entry.Chinese;
        (*Object)->TryGetStringField(TEXT("nativeSource"),Entry.NativeSource);
        if(Entry.NativeSource.IsEmpty())return Fail(OutError,TEXT("A language entry has an empty native source."));
        if((*Object)->TryGetStringField(TEXT("nativePattern"),Entry.NativePattern)&&!PrepareTemplate(Entry))
            return Fail(OutError,TEXT("Invalid complete-sentence template: ")+Entry.Key);
        FString Usage;(*Object)->TryGetStringField(TEXT("usage"),Usage);Entry.bCompact=Usage==TEXT("compact");
        const FString Identity = Entry.Namespace + TEXT("::") + Entry.Key;
        if (Identities.Contains(Identity)) return Fail(OutError, TEXT("Duplicate language catalogue identity: ") + Identity);
        Identities.Add(Identity);
        const int32 Index = Entries.Add(Entry);
        if (Entry.Namespace == TEXT("GameXXK")) ByKey.Add(Entry.Key, Index);
        if(Entry.bCompact)
        {
            ByCompactSource.Add(Entry.NativeSource,Index);
            if(!ByCompactSource.Contains(Entry.Chinese))ByCompactSource.Add(Entry.Chinese,Index);
        }
        else
        {
            if(!BySource.Contains(Entry.Chinese))BySource.Add(Entry.Chinese,Index);
            if(!BySource.Contains(Entry.NativeSource))BySource.Add(Entry.NativeSource,Index);
        }
    }
    Data.Entries = MoveTemp(Entries);
    Data.ByKey = MoveTemp(ByKey);
    Data.BySource = MoveTemp(BySource);
    Data.ByCompactSource=MoveTemp(ByCompactSource);
    Data.Identities = MoveTemp(Identities);
    Data.Templates.Reset();Data.DisplayCache.Reset();Data.MissingSources.Reset();
    for(int32 Index=0;Index<Data.Entries.Num();++Index)if(Data.Entries[Index].Matcher.IsValid())Data.Templates.Add(Index);
    Data.Templates.StableSort([&Data](int32 A,int32 B){return Data.Entries[A].LiteralLength>Data.Entries[B].LiteralLength;});
    if(!Data.bReloading)Data.Language = ReadPreference(GetPreferencePath());
    Data.bInitialized = true;
    ApplyLanguage();
    ++Data.Revision;
    return true;
}

bool GameXXKLocalization::ReloadTextCatalog(FString* OutError)
{
    FCatalog& Data=Catalog();
    if(!Data.bInitialized)return Initialize(OutError);
    Data.bInitialized=false;Data.bReloading=true;
    const bool bLoaded=Initialize(OutError);
    Data.bReloading=false;Data.bInitialized=true;
    if(!bLoaded)return false; // Parsing validates the replacement before mutating the catalogue.
    Data.Changed.Broadcast();RefreshGameTextMaterialAspects();
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
    RefreshGameTextMaterialAspects();
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
    if (Initialize())return ResolveDisplay(NativeText,0);
    return FText::FromString(NativeText);
}

FText GameXXKLocalization::Localize(const FText& NativeText)
{
    Initialize();
    const FTextId Identity = FTextInspector::GetTextId(NativeText);
    if (!Identity.IsEmpty())
    {
        const FString Namespace = Identity.GetNamespace().ToString();
        const FString Key = Identity.GetKey().ToString();
        if (Catalog().Identities.Contains(Namespace + TEXT("::") + Key))
            return NativeText;
    }
    // Keep the explicit identity of a formatted UI label. Flattening it to its
    // Chinese source lets a legacy whole-string entry replace Ch. 2 with Chapter 2,
    // or loses the already translated argument in the difficulty selector.
    TArray<FHistoricTextFormatData> Formats;
    FTextInspector::GetHistoricFormatData(NativeText,Formats);
    if(!Formats.IsEmpty())
    {
        auto& Format=Formats.Last();
        const FText Pattern=Format.SourceFmt.GetSourceText();
        const FTextId PatternId=FTextInspector::GetTextId(Pattern);
        if(!PatternId.IsEmpty() && Catalog().Identities.Contains(
            PatternId.GetNamespace().ToString()+TEXT("::")+PatternId.GetKey().ToString()))
        {
            for(auto& Argument:Format.Arguments)
                if(Argument.Value.GetType()==EFormatArgumentType::Text)
                    Argument.Value=FFormatArgumentValue(Localize(Argument.Value.GetTextValue()));
            return FText::Format(Format.SourceFmt,MoveTemp(Format.Arguments));
        }
    }
    const FString SourceString = NativeText.BuildSourceString();
    if(const int32* Index=Catalog().BySource.Find(SourceString))return EntryText(Catalog().Entries[*Index]);
    if(HasChinese(SourceString))return ResolveDisplay(SourceString,0);
    return NativeText;
}

TArray<FString> GameXXKLocalization::GetMissingSources()
{
    TArray<FString> Result = Catalog().MissingSources.Array();
    Result.Sort();
    return Result;
}

FText GameXXKLocalization::Compact(const FText& NativeText)
{
    Initialize();
    if(const int32* Index=Catalog().ByCompactSource.Find(NativeText.BuildSourceString()))return EntryText(Catalog().Entries[*Index]);
    return Localize(NativeText);
}

FString GameXXKLocalization::EnglishText(const TCHAR* Key)
{
    Initialize();
    if(const int32* Index=Catalog().ByKey.Find(Key))return Catalog().Entries[*Index].English;
    Catalog().MissingSources.Add(FString(TEXT("Key:"))+Key);
    return FString();
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
