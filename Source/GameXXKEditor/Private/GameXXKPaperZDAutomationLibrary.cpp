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
constexpr TCHAR TownIdleSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_Idle");
constexpr TCHAR TownWalkStartSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_WalkStart");
constexpr TCHAR TownWalkLoopSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_WalkLoop");
constexpr TCHAR TownWalkStopSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_WalkStop");
constexpr TCHAR TownDeepBreathSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_DeepBreath");
constexpr TCHAR TownAdjustBackpackSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_AdjustBackpack");
constexpr TCHAR TownCollectItemSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_CollectItem");
constexpr TCHAR TownCombatIdleSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_CombatIdle");
constexpr TCHAR TownPunchSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_Punch");
constexpr TCHAR TownKickSequencePackagePath[] = TEXT("/Game/GameXXK/Characters/Hero/PaperZD/PZD_Hero_Town_Kick");
constexpr TCHAR SourceAssetName[] = TEXT("AS_Hero_Flipbook");
constexpr TCHAR AnimBPAssetName[] = TEXT("ABP_Hero_PaperZD");
constexpr TCHAR TownIdleSequenceAssetName[] = TEXT("PZD_Hero_Town_Idle");
constexpr TCHAR TownWalkStartSequenceAssetName[] = TEXT("PZD_Hero_Town_WalkStart");
constexpr TCHAR TownWalkLoopSequenceAssetName[] = TEXT("PZD_Hero_Town_WalkLoop");
constexpr TCHAR TownWalkStopSequenceAssetName[] = TEXT("PZD_Hero_Town_WalkStop");
constexpr TCHAR TownDeepBreathSequenceAssetName[] = TEXT("PZD_Hero_Town_DeepBreath");
constexpr TCHAR TownAdjustBackpackSequenceAssetName[] = TEXT("PZD_Hero_Town_AdjustBackpack");
constexpr TCHAR TownCollectItemSequenceAssetName[] = TEXT("PZD_Hero_Town_CollectItem");
constexpr TCHAR TownCombatIdleSequenceAssetName[] = TEXT("PZD_Hero_Town_CombatIdle");
constexpr TCHAR TownPunchSequenceAssetName[] = TEXT("PZD_Hero_Town_Punch");
constexpr TCHAR TownKickSequenceAssetName[] = TEXT("PZD_Hero_Town_Kick");
constexpr TCHAR TownIdleFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Idle_Left.FB_Hero_Town_Idle_Left");
constexpr TCHAR TownWalkStartFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStart_Left.FB_Hero_Town_WalkStart_Left");
constexpr TCHAR TownWalkLoopFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkLoop_Left.FB_Hero_Town_WalkLoop_Left");
constexpr TCHAR TownWalkStopFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_WalkStop_Left.FB_Hero_Town_WalkStop_Left");
constexpr TCHAR TownDeepBreathFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_DeepBreath_Left.FB_Hero_Town_DeepBreath_Left");
constexpr TCHAR TownAdjustBackpackFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_AdjustBackpack_Left.FB_Hero_Town_AdjustBackpack_Left");
constexpr TCHAR TownCollectItemFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CollectItem_Left.FB_Hero_Town_CollectItem_Left");
constexpr TCHAR TownCombatIdleFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_CombatIdle_Left.FB_Hero_Town_CombatIdle_Left");
constexpr TCHAR TownPunchFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Punch_Left.FB_Hero_Town_Punch_Left");
constexpr TCHAR TownKickFlipbookPath[] = TEXT("/Game/GameXXK/Characters/Hero/TownHorizontal/Flipbooks/FB_Hero_Town_Kick_Left.FB_Hero_Town_Kick_Left");

struct FTownPaperZDSequenceSpec
{
	const TCHAR* PackagePath;
	const TCHAR* AssetName;
	const TCHAR* FlipbookPath;
};

constexpr FTownPaperZDSequenceSpec TownPaperZDSequenceSpecs[] = {
	{TownIdleSequencePackagePath, TownIdleSequenceAssetName, TownIdleFlipbookPath},
	{TownWalkStartSequencePackagePath, TownWalkStartSequenceAssetName, TownWalkStartFlipbookPath},
	{TownWalkLoopSequencePackagePath, TownWalkLoopSequenceAssetName, TownWalkLoopFlipbookPath},
	{TownWalkStopSequencePackagePath, TownWalkStopSequenceAssetName, TownWalkStopFlipbookPath},
	{TownDeepBreathSequencePackagePath, TownDeepBreathSequenceAssetName, TownDeepBreathFlipbookPath},
	{TownAdjustBackpackSequencePackagePath, TownAdjustBackpackSequenceAssetName, TownAdjustBackpackFlipbookPath},
	{TownCollectItemSequencePackagePath, TownCollectItemSequenceAssetName, TownCollectItemFlipbookPath},
	{TownCombatIdleSequencePackagePath, TownCombatIdleSequenceAssetName, TownCombatIdleFlipbookPath},
	{TownPunchSequencePackagePath, TownPunchSequenceAssetName, TownPunchFlipbookPath},
	{TownKickSequencePackagePath, TownKickSequenceAssetName, TownKickFlipbookPath},
};

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

UPaperZDAnimSequence_Flipbook* EnsureSequence(UPaperZDAnimationSource_Flipbook* Source, const TCHAR* SequencePackagePath, const TCHAR* SequenceAssetName, bool& bCreated)
{
	bCreated = false;
	if (UPaperZDAnimSequence_Flipbook* Existing = LoadAssetByPackagePath<UPaperZDAnimSequence_Flipbook>(SequencePackagePath))
	{
		Existing->Modify();
		Existing->SetAnimSource(Source);
		Existing->MarkPackageDirty();
		return Existing;
	}

	bCreated = true;
	UPaperZDAnimSequence_Flipbook* Sequence = CreateAsset<UPaperZDAnimSequence_Flipbook>(
		SequencePackagePath,
		SequenceAssetName);
	if (Sequence)
	{
		Sequence->SetAnimSource(Source);
	}
	return Sequence;
}

UPaperZDAnimSequence_Flipbook* EnsureTownIdleSequence(UPaperZDAnimationSource_Flipbook* Source, bool& bCreated)
{
	return EnsureSequence(Source, TownIdleSequencePackagePath, TownIdleSequenceAssetName, bCreated);
}

UPaperZDAnimSequence_Flipbook* EnsureTownWalkStartSequence(UPaperZDAnimationSource_Flipbook* Source, bool& bCreated)
{
	return EnsureSequence(Source, TownWalkStartSequencePackagePath, TownWalkStartSequenceAssetName, bCreated);
}

UPaperZDAnimSequence_Flipbook* EnsureTownWalkLoopSequence(UPaperZDAnimationSource_Flipbook* Source, bool& bCreated)
{
	return EnsureSequence(Source, TownWalkLoopSequencePackagePath, TownWalkLoopSequenceAssetName, bCreated);
}

bool LoadHeroFlipbooks(const TCHAR* StateName, TArray<UPaperFlipbook*>& OutFlipbooks, FString& OutError)
{
	OutFlipbooks.Reset();
	for (const TCHAR* DirectionName : DirectionNames)
	{
		const FString ObjectPath = FString::Printf(
			TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_%s_%s.FB_Hero_%s_%s"),
			StateName,
			DirectionName,
			StateName,
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

bool LoadWalkFlipbooks(TArray<UPaperFlipbook*>& OutFlipbooks, FString& OutError)
{
	return LoadHeroFlipbooks(TEXT("Walk"), OutFlipbooks, OutError);
}

bool LoadIdleFlipbooks(TArray<UPaperFlipbook*>& OutFlipbooks, FString& OutError)
{
	return LoadHeroFlipbooks(TEXT("Idle"), OutFlipbooks, OutError);
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

bool ConfigureTownHorizontalAnimData(
	UPaperZDAnimSequence_Flipbook* Sequence,
	UPaperFlipbook* Flipbook,
	FString& OutError)
{
	if (!Sequence || !Flipbook)
	{
		OutError = TEXT("Town horizontal PaperZD sequence or flipbook is null");
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
	Sequence->bDirectionalSequence = false;
	Sequence->DirectionalAngleOffset = 0.0f;
	Sequence->Category = FName(TEXT("Locomotion"));

	FScriptArrayHelper Helper(AnimDataProperty, AnimDataProperty->ContainerPtrToValuePtr<void>(Sequence));
	Helper.EmptyValues();
	const int32 Index = Helper.AddValue();
	FPaperZDFlipbookAnimDataSource* Entry = reinterpret_cast<FPaperZDFlipbookAnimDataSource*>(Helper.GetRawPtr(Index));
	Entry->Animation = Flipbook;
	Entry->CompositeLayerAnimations.Reset();
	Entry->MirrorMode = EPaperZDFlipbookMirrorMode::None;
	Entry->MirroredKeyFrames.Reset();
	Entry->VerticalMirroredKeyFrames.Reset();

	Sequence->PostEditChange();
	Sequence->MarkPackageDirty();
	return true;
}
}

FString UGameXXKPaperZDAutomationLibrary::EnsureHeroPaperZDAssets()
{
	FString Error;
	TArray<UPaperFlipbook*> TownFlipbooks;
	TownFlipbooks.Reserve(UE_ARRAY_COUNT(TownPaperZDSequenceSpecs));
	for (const FTownPaperZDSequenceSpec& Spec : TownPaperZDSequenceSpecs)
	{
		UPaperFlipbook* Flipbook = LoadObject<UPaperFlipbook>(nullptr, Spec.FlipbookPath);
		if (!Flipbook)
		{
			Error = FString::Printf(TEXT("Missing horizontal town hero flipbook %s"), Spec.FlipbookPath);
			return FString::Printf(TEXT("{\"ok\":false,\"error\":\"%s\"}"), *Error.ReplaceCharWithEscapedChar());
		}
		TownFlipbooks.Add(Flipbook);
	}

	bool bCreatedSource = false;
	bool bCreatedAnimBP = false;
	UPaperZDAnimationSource_Flipbook* Source = EnsureSource(bCreatedSource);
	UPaperZDAnimBP* AnimBP = Source ? EnsureAnimBP(Source, bCreatedAnimBP) : nullptr;
	TArray<UPaperZDAnimSequence_Flipbook*> TownSequences;
	TownSequences.Reserve(UE_ARRAY_COUNT(TownPaperZDSequenceSpecs));
	int32 CreatedSequenceCount = 0;
	bool bConfiguredAll = Source && AnimBP;
	if (!Source)
	{
		Error = TEXT("Could not create or load PaperZD source");
	}
	else if (!AnimBP)
	{
		Error = TEXT("Could not create or load PaperZD AnimBP");
	}
	else
	{
		for (int32 Index = 0; Index < UE_ARRAY_COUNT(TownPaperZDSequenceSpecs); ++Index)
		{
			const FTownPaperZDSequenceSpec& Spec = TownPaperZDSequenceSpecs[Index];
			bool bCreatedSequence = false;
			UPaperZDAnimSequence_Flipbook* Sequence = EnsureSequence(
				Source,
				Spec.PackagePath,
				Spec.AssetName,
				bCreatedSequence);
			if (!Sequence
				|| !ConfigureTownHorizontalAnimData(Sequence, TownFlipbooks[Index], Error))
			{
				bConfiguredAll = false;
				if (Error.IsEmpty())
				{
					Error = FString::Printf(TEXT("Could not configure PaperZD sequence %s"), Spec.AssetName);
				}
				break;
			}
			CreatedSequenceCount += bCreatedSequence ? 1 : 0;
			TownSequences.Add(Sequence);
		}
	}

	const bool bOk = Source
		&& AnimBP
		&& bConfiguredAll
		&& TownSequences.Num() == UE_ARRAY_COUNT(TownPaperZDSequenceSpecs);
	if (bOk)
	{
		UEditorLoadingAndSavingUtils::SaveDirtyPackages(true, true);
	}

	UE_LOG(LogGameXXKPaperZDAutomation, Log, TEXT("EnsureHeroPaperZDAssets ok=%s source=%s animBP=%s sequenceCount=%d"),
		bOk ? TEXT("true") : TEXT("false"),
		Source ? *Source->GetPathName() : TEXT(""),
		AnimBP ? *AnimBP->GetPathName() : TEXT(""),
		TownSequences.Num());

	TArray<FString> SequenceJsonEntries;
	for (UPaperZDAnimSequence_Flipbook* Sequence : TownSequences)
	{
		SequenceJsonEntries.Add(FString::Printf(
			TEXT("\"%s\""),
			Sequence ? *Sequence->GetPathName().ReplaceCharWithEscapedChar() : TEXT("")));
	}

	return FString::Printf(
		TEXT("{")
		TEXT("\"ok\":%s,")
		TEXT("\"paperzd_dir\":\"%s\",")
		TEXT("\"created_source\":%s,")
		TEXT("\"created_anim_bp\":%s,")
		TEXT("\"created_sequence_count\":%d,")
		TEXT("\"source\":\"%s\",")
		TEXT("\"anim_bp\":\"%s\",")
		TEXT("\"sequences\":[%s],")
		TEXT("\"town_flipbook_count\":%d,")
		TEXT("\"direction_policy\":\"left_source_with_runtime_horizontal_mirror\",")
		TEXT("\"error\":\"%s\"")
		TEXT("}"),
		*JsonBool(bOk),
		PaperZDDir,
		*JsonBool(bCreatedSource),
		*JsonBool(bCreatedAnimBP),
		CreatedSequenceCount,
		Source ? *Source->GetPathName() : TEXT(""),
		AnimBP ? *AnimBP->GetPathName() : TEXT(""),
		*FString::Join(SequenceJsonEntries, TEXT(",")),
		TownFlipbooks.Num(),
		*Error.ReplaceCharWithEscapedChar());
}
