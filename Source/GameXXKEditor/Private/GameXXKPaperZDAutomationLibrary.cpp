#include "GameXXKPaperZDAutomationLibrary.h"

#include "AnimSequences/PaperZDAnimSequence_Flipbook.h"
#include "AnimSequences/PaperZDFlipbookAnimDataSource.h"
#include "AnimSequences/Sources/PaperZDAnimationSource_Flipbook.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "FileHelpers.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "PaperFlipbook.h"
#include "PaperZDAnimBP.h"
#include "PaperZDAnimBPGeneratedClass.h"
#include "PaperZDAnimInstance.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameXXKPaperZDAutomation, Log, All);

namespace
{
constexpr TCHAR PaperZDDir[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD");
constexpr TCHAR SourcePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/AS_Hero_Flipbook");
constexpr TCHAR AnimBPPackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/ABP_Hero_PaperZD");
constexpr TCHAR WalkSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Walk_8Dir");
constexpr TCHAR SourceAssetName[] = TEXT("AS_Hero_Flipbook");
constexpr TCHAR AnimBPAssetName[] = TEXT("ABP_Hero_PaperZD");
constexpr TCHAR WalkSequenceAssetName[] = TEXT("PZD_Hero_Walk_8Dir");

const TCHAR* DirectionNames[] = {
	TEXT("South"),
	TEXT("SouthWest"),
	TEXT("West"),
	TEXT("NorthWest"),
	TEXT("North"),
	TEXT("NorthEast"),
	TEXT("East"),
	TEXT("SouthEast"),
};

FString JsonBool(const bool bValue)
{
	return bValue ? TEXT("true") : TEXT("false");
}

FString ObjectPathForPackage(const TCHAR* LongPackageName)
{
	return FString::Printf(TEXT("%s.%s"), LongPackageName, *FPackageName::GetShortName(LongPackageName));
}

template <typename AssetType>
AssetType* LoadAssetByPackagePath(const TCHAR* LongPackageName)
{
	return LoadObject<AssetType>(nullptr, *ObjectPathForPackage(LongPackageName));
}

UPackage* CreateOrLoadPackageForAsset(const TCHAR* LongPackageName)
{
	UPackage* Package = CreatePackage(LongPackageName);
	if (Package)
	{
		Package->FullyLoad();
	}
	return Package;
}

template <typename AssetType>
AssetType* CreateAsset(const TCHAR* LongPackageName, const TCHAR* AssetName)
{
	UPackage* Package = CreateOrLoadPackageForAsset(LongPackageName);
	if (!Package)
	{
		return nullptr;
	}

	AssetType* Asset = NewObject<AssetType>(Package, AssetName, RF_Public | RF_Standalone | RF_Transactional);
	if (!Asset)
	{
		return nullptr;
	}

	FAssetRegistryModule::AssetCreated(Asset);
	Asset->MarkPackageDirty();
	return Asset;
}

UPaperZDAnimationSource_Flipbook* EnsureSource(bool& bCreated)
{
	bCreated = false;
	if (UPaperZDAnimationSource_Flipbook* Existing = LoadAssetByPackagePath<UPaperZDAnimationSource_Flipbook>(SourcePackagePath))
	{
		return Existing;
	}

	bCreated = true;
	return CreateAsset<UPaperZDAnimationSource_Flipbook>(SourcePackagePath, SourceAssetName);
}

UPaperZDAnimBP* EnsureAnimBP(UPaperZDAnimationSource_Flipbook* Source, bool& bCreated)
{
	bCreated = false;
	if (UPaperZDAnimBP* Existing = LoadAssetByPackagePath<UPaperZDAnimBP>(AnimBPPackagePath))
	{
		Existing->Modify();
		Existing->SupportedAnimationSource = Source;
		Existing->MarkPackageDirty();
		return Existing;
	}

	UPackage* Package = CreateOrLoadPackageForAsset(AnimBPPackagePath);
	if (!Package)
	{
		return nullptr;
	}

	bCreated = true;
	UPaperZDAnimBP* AnimBP = Cast<UPaperZDAnimBP>(FKismetEditorUtilities::CreateBlueprint(
		UPaperZDAnimInstance::StaticClass(),
		Package,
		AnimBPAssetName,
		BPTYPE_Normal,
		UPaperZDAnimBP::StaticClass(),
		UPaperZDAnimBPGeneratedClass::StaticClass()));
	if (!AnimBP)
	{
		return nullptr;
	}

	AnimBP->SupportedAnimationSource = Source;
	FAssetRegistryModule::AssetCreated(AnimBP);
	AnimBP->MarkPackageDirty();
	return AnimBP;
}

UPaperZDAnimSequence_Flipbook* EnsureWalkSequence(UPaperZDAnimationSource_Flipbook* Source, bool& bCreated)
{
	bCreated = false;
	if (UPaperZDAnimSequence_Flipbook* Existing = LoadAssetByPackagePath<UPaperZDAnimSequence_Flipbook>(WalkSequencePackagePath))
	{
		Existing->Modify();
		Existing->SetAnimSource(Source);
		Existing->MarkPackageDirty();
		return Existing;
	}

	bCreated = true;
	UPaperZDAnimSequence_Flipbook* Sequence = CreateAsset<UPaperZDAnimSequence_Flipbook>(
		WalkSequencePackagePath,
		WalkSequenceAssetName);
	if (Sequence)
	{
		Sequence->SetAnimSource(Source);
	}
	return Sequence;
}

bool LoadWalkFlipbooks(TArray<UPaperFlipbook*>& OutFlipbooks, FString& OutError)
{
	OutFlipbooks.Reset();
	for (const TCHAR* DirectionName : DirectionNames)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_%s.FB_Hero_Walk_%s"),
			DirectionName,
			DirectionName);
		UPaperFlipbook* Flipbook = LoadObject<UPaperFlipbook>(nullptr, *ObjectPath);
		if (!Flipbook)
		{
			OutError = FString::Printf(TEXT("Missing Paper2D flipbook %s"), *ObjectPath);
			return false;
		}
		OutFlipbooks.Add(Flipbook);
	}
	return true;
}

bool ConfigureWalkAnimData(UPaperZDAnimSequence_Flipbook* Sequence, const TArray<UPaperFlipbook*>& Flipbooks, FString& OutError)
{
	if (!Sequence)
	{
		OutError = TEXT("Walk sequence is null");
		return false;
	}

	FArrayProperty* AnimDataProperty = FindFProperty<FArrayProperty>(
		UPaperZDAnimSequence_Flipbook::StaticClass(),
		TEXT("AnimData"));
	if (!AnimDataProperty)
	{
		OutError = TEXT("Could not find UPaperZDAnimSequence_Flipbook AnimData property");
		return false;
	}

	Sequence->Modify();
	Sequence->bDirectionalSequence = true;
	Sequence->DirectionalAngleOffset = 0.0f;
	Sequence->Category = FName(TEXT("Locomotion"));

	FScriptArrayHelper Helper(AnimDataProperty, AnimDataProperty->ContainerPtrToValuePtr<void>(Sequence));
	Helper.EmptyValues();
	for (UPaperFlipbook* Flipbook : Flipbooks)
	{
		const int32 Index = Helper.AddValue();
		FPaperZDFlipbookAnimDataSource* Entry = reinterpret_cast<FPaperZDFlipbookAnimDataSource*>(Helper.GetRawPtr(Index));
		Entry->Animation = Flipbook;
		Entry->CompositeLayerAnimations.Reset();
		Entry->MirrorMode = EPaperZDFlipbookMirrorMode::None;
		Entry->MirroredKeyFrames.Reset();
		Entry->VerticalMirroredKeyFrames.Reset();
	}

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}
}

FString UGameXXKPaperZDAutomationLibrary::EnsureHeroPaperZDAssets()
{
	TArray<UPaperFlipbook*> Flipbooks;
	FString Error;
	if (!LoadWalkFlipbooks(Flipbooks, Error))
	{
		return FString::Printf(TEXT("{\"ok\":false,\"error\":\"%s\"}"), *Error.ReplaceCharWithEscapedChar());
	}

	bool bCreatedSource = false;
	bool bCreatedAnimBP = false;
	bool bCreatedSequence = false;
	UPaperZDAnimationSource_Flipbook* Source = EnsureSource(bCreatedSource);
	UPaperZDAnimBP* AnimBP = Source ? EnsureAnimBP(Source, bCreatedAnimBP) : nullptr;
	UPaperZDAnimSequence_Flipbook* Sequence = Source ? EnsureWalkSequence(Source, bCreatedSequence) : nullptr;

	bool bConfigured = false;
	if (Source && AnimBP && Sequence)
	{
		bConfigured = ConfigureWalkAnimData(Sequence, Flipbooks, Error);
	}
	else if (!Source)
	{
		Error = TEXT("Could not create or load PaperZD source");
	}
	else if (!AnimBP)
	{
		Error = TEXT("Could not create or load PaperZD AnimBP");
	}
	else
	{
		Error = TEXT("Could not create or load PaperZD walk sequence");
	}

	const bool bOk = Source && AnimBP && Sequence && bConfigured;
	if (bOk)
	{
		UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true);
	}

	UE_LOG(LogGameXXKPaperZDAutomation, Log, TEXT("EnsureHeroPaperZDAssets ok=%s source=%s animBP=%s sequence=%s"),
		bOk ? TEXT("true") : TEXT("false"),
		Source ? *Source->GetPathName() : TEXT(""),
		AnimBP ? *AnimBP->GetPathName() : TEXT(""),
		Sequence ? *Sequence->GetPathName() : TEXT(""));

	return FString::Printf(
		TEXT("{")
		TEXT("\"ok\":%s,")
		TEXT("\"paperzd_dir\":\"%s\",")
		TEXT("\"created_source\":%s,")
		TEXT("\"created_anim_bp\":%s,")
		TEXT("\"created_sequence\":%s,")
		TEXT("\"source\":\"%s\",")
		TEXT("\"anim_bp\":\"%s\",")
		TEXT("\"walk_sequence\":\"%s\",")
		TEXT("\"flipbook_count\":%d,")
		TEXT("\"error\":\"%s\"")
		TEXT("}"),
		*JsonBool(bOk),
		PaperZDDir,
		*JsonBool(bCreatedSource),
		*JsonBool(bCreatedAnimBP),
		*JsonBool(bCreatedSequence),
		Source ? *Source->GetPathName() : TEXT(""),
		AnimBP ? *AnimBP->GetPathName() : TEXT(""),
		Sequence ? *Sequence->GetPathName() : TEXT(""),
		Flipbooks.Num(),
		*Error.ReplaceCharWithEscapedChar());
}
