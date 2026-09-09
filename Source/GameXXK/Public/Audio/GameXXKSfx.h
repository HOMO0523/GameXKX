#pragma once

#include "CoreMinimal.h"
#include "Audio/GameXXKSfxPolicy.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "Styling/SlateTypes.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "GameXXKSfx.generated.h"

class USoundBase;
class USoundConcurrency;

UCLASS()
class GAMEXXK_API UGameXXKSfxSubsystem : public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool Play(EGameXXKSfxCue Cue);
	FString GetDiagnostics() const;
	void ResetDiagnostics();

private:
	FGameXXKSfxPolicy Gate;
	UPROPERTY(Transient) TMap<FName, TObjectPtr<USoundBase>> Sounds;
	UPROPERTY(Transient) TObjectPtr<USoundConcurrency> Concurrency;
	TMap<FName, int32> PlayedCounts;
	TMap<FName, FString> LastAssets;
	TMap<FName, double> LastTimes;
};

class GAMEXXK_API FGameXXKSfx
{
public:
	static FName CueName(EGameXXKSfxCue Cue);
	static EGameXXKSfxCue FindCue(FName Name);
	static int32 VariantCount(EGameXXKSfxCue Cue);
	static FString AssetPath(EGameXXKSfxCue Cue, int32 VariantIndex);
	static bool Play(const UObject* WorldContext, EGameXXKSfxCue Cue);
	static void SetButtonSound(FButtonStyle& Style);
};

/** Readable trial hooks; calls use the same service as gameplay and never alter game state. */
UCLASS()
class GAMEXXK_API UGameXXKSfxLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()
public:
	UFUNCTION(BlueprintCallable, Category="GameXXK|Audio", meta=(WorldContext="WorldContext"))
	static bool PlayNamed(const UObject* WorldContext, FName Cue);
	UFUNCTION(BlueprintPure, Category="GameXXK|Audio", meta=(WorldContext="WorldContext"))
	static FString GetDiagnostics(const UObject* WorldContext);
	UFUNCTION(BlueprintCallable, Category="GameXXK|Audio", meta=(WorldContext="WorldContext"))
	static void ResetDiagnostics(const UObject* WorldContext);
};
