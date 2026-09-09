#include "Audio/GameXXKSfx.h"

#include "Components/AudioComponent.h"
#include "Dom/JsonObject.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "HAL/PlatformTime.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/App.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundBase.h"
#include "Sound/SoundConcurrency.h"

namespace
{
	constexpr const TCHAR* Names[] = {
		TEXT("None"), TEXT("HitLight"), TEXT("HitHeavy"), TEXT("CardPlay"), TEXT("Block"),
		TEXT("Lightning"), TEXT("Fire"), TEXT("Frost"), TEXT("Heal"), TEXT("Down"),
		TEXT("Victory"), TEXT("Defeat"), TEXT("Reward"), TEXT("Button"), TEXT("Tool")
	};
	constexpr int32 Counts[] = { 0, 3, 2, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 1 };

	UGameXXKSfxSubsystem* Resolve(const UObject* Context)
	{
		UWorld* World = GEngine ? GEngine->GetWorldFromContextObject(Context, EGetWorldErrorMode::ReturnNull) : nullptr;
		UGameInstance* Instance = World && World->IsGameWorld() ? World->GetGameInstance() : nullptr;
		return Instance ? Instance->GetSubsystem<UGameXXKSfxSubsystem>() : nullptr;
	}
}

FName FGameXXKSfx::CueName(EGameXXKSfxCue Cue)
{
	const int32 Index = static_cast<int32>(Cue);
	return Index > 0 && Index < UE_ARRAY_COUNT(Names) ? FName(Names[Index]) : NAME_None;
}

EGameXXKSfxCue FGameXXKSfx::FindCue(FName Name)
{
	for (int32 Index = 1; Index < UE_ARRAY_COUNT(Names); ++Index)
	{
		if (Name == FName(Names[Index])) return static_cast<EGameXXKSfxCue>(Index);
	}
	return EGameXXKSfxCue::None;
}

int32 FGameXXKSfx::VariantCount(EGameXXKSfxCue Cue)
{
	const int32 Index = static_cast<int32>(Cue);
	return Index > 0 && Index < UE_ARRAY_COUNT(Counts) ? Counts[Index] : 0;
}

FString FGameXXKSfx::AssetPath(EGameXXKSfxCue Cue, int32 VariantIndex)
{
	const int32 Count = VariantCount(Cue);
	if (Count <= 0) return FString();
	const FString Name = FString::Printf(TEXT("SFX_%s_v%02d"), *CueName(Cue).ToString(), FMath::Clamp(VariantIndex, 0, Count - 1) + 1);
	return FString::Printf(TEXT("/Game/GameXXK/Audio/SFX/Essential/%s.%s"), *Name, *Name);
}

bool FGameXXKSfx::Play(const UObject* WorldContext, EGameXXKSfxCue Cue)
{
	auto* Subsystem = Resolve(WorldContext);
	return Subsystem && Subsystem->Play(Cue);
}

void FGameXXKSfx::SetButtonSound(FButtonStyle& Style)
{
	FSlateSound Sound;
	Sound.SetResourceObject(LoadObject<USoundBase>(nullptr, *AssetPath(EGameXXKSfxCue::Button, 0), nullptr, LOAD_NoWarn));
	Style.SetPressedSound(Sound);
	Style.SetHoveredSound(FSlateSound());
}

bool UGameXXKSfxSubsystem::Play(EGameXXKSfxCue Cue)
{
	UWorld* World = GetWorld();
	const FName Name = FGameXXKSfx::CueName(Cue);
	if (!World || !World->IsGameWorld() || Name.IsNone() || !GEngine || !GEngine->UseSound())
	{
		return false;
	}
	const double Now = FPlatformTime::Seconds();
	if (!Gate.Accept(Cue, Now, FApp::GetVolumeMultiplier() <= 0.0f)) return false;
	const int32 PreviousCount = PlayedCounts.FindRef(Name);
	const FString Path = FGameXXKSfx::AssetPath(Cue, PreviousCount % FGameXXKSfx::VariantCount(Cue));
	const FName CacheKey(*Path);
	USoundBase* Sound = Sounds.FindRef(CacheKey);
	if (!Sound)
	{
		Sound = LoadObject<USoundBase>(nullptr, *Path, nullptr, LOAD_NoWarn);
		if (!Sound)
		{
			UE_LOG(LogTemp, Warning, TEXT("[GameXXKAudio] Missing trial asset: %s"), *Path);
			return false;
		}
		Sounds.Add(CacheKey, Sound);
	}
	if (!Concurrency)
	{
		Concurrency = NewObject<USoundConcurrency>(this);
		Concurrency->Concurrency.MaxCount = 8;
		Concurrency->Concurrency.ResolutionRule = EMaxConcurrentResolutionRule::StopOldest;
	}
	UAudioComponent* Component = UGameplayStatics::SpawnSound2D(World, Sound, 0.8f, 1.0f, 0.0f, Concurrency, false, true);
	if (!Component) return false;
	PlayedCounts.Add(Name, PreviousCount + 1);
	LastAssets.Add(Name, Path);
	LastTimes.Add(Name, Now);
	return true;
}

FString UGameXXKSfxSubsystem::GetDiagnostics() const
{
	const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> CountsObject = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> AssetsObject = MakeShared<FJsonObject>();
	const TSharedRef<FJsonObject> TimesObject = MakeShared<FJsonObject>();
	for (const auto& Pair : PlayedCounts) CountsObject->SetNumberField(Pair.Key.ToString(), Pair.Value);
	for (const auto& Pair : LastAssets) AssetsObject->SetStringField(Pair.Key.ToString(), Pair.Value);
	for (const auto& Pair : LastTimes) TimesObject->SetNumberField(Pair.Key.ToString(), Pair.Value);
	Root->SetObjectField(TEXT("played"), CountsObject);
	Root->SetObjectField(TEXT("last_assets"), AssetsObject);
	Root->SetObjectField(TEXT("last_times"), TimesObject);
	Root->SetNumberField(TEXT("master_volume"), FApp::GetVolumeMultiplier());
	Root->SetNumberField(TEXT("cached_waves"), Sounds.Num());
	FString Json;
	FJsonSerializer::Serialize(Root, TJsonWriterFactory<>::Create(&Json));
	return Json;
}

void UGameXXKSfxSubsystem::ResetDiagnostics()
{
	PlayedCounts.Reset();
	LastAssets.Reset();
	LastTimes.Reset();
	Gate.Reset();
}

bool UGameXXKSfxLibrary::PlayNamed(const UObject* Context, FName Cue)
{
	return FGameXXKSfx::Play(Context, FGameXXKSfx::FindCue(Cue));
}

FString UGameXXKSfxLibrary::GetDiagnostics(const UObject* Context)
{
	auto* Subsystem = Resolve(Context);
	return Subsystem ? Subsystem->GetDiagnostics() : TEXT("{}");
}

void UGameXXKSfxLibrary::ResetDiagnostics(const UObject* Context)
{
	if (auto* Subsystem = Resolve(Context)) Subsystem->ResetDiagnostics();
}
