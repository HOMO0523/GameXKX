#pragma once

#include "CoreMinimal.h"

class UGameXXKMVPSubsystem;
enum class EGameXXKScreen : uint8;
struct FGameXXKRuntimeState;

namespace GameXXKLevelFlow
{
	GAMEXXK_API FName MapForScreen(EGameXXKScreen Screen);
	GAMEXXK_API FName MapForRuntimeState(const FGameXXKRuntimeState& State);
	GAMEXXK_API bool RequiresMapLoadForRuntimeState(
		const FString& CurrentPackageName,
		const FGameXXKRuntimeState& State);
	GAMEXXK_API bool MapPackageMatches(const FString& CurrentPackageName, FName TargetMap);
	GAMEXXK_API bool IsTownGameplayMapPackage(const FString& CurrentPackageName);
	/** True for the isolated pure-HUD town replacement map. */
	GAMEXXK_API bool IsDesktopTrainingHUDMapPackage(const FString& CurrentPackageName);
	/** Stable playable 3D Qingshan target used only by explicit scene requests. */
	GAMEXXK_API FName QingshanTownGameplayMap();
	/** Transient URL options for the repeatable carriage preview. */
	GAMEXXK_API FString CarriagePreviewTravelOptions();
	/** True only when the travelled URL explicitly requests the carriage preview. */
	GAMEXXK_API bool HasCarriagePreviewTravelOption(const FString& Options);
	/** Isolated pure-2D tutorial battle map; ordinary map routing never resolves here. */
	GAMEXXK_API FName Tutorial01Map();
	/** Transient URL option required by the isolated tutorial map. */
	GAMEXXK_API FString Tutorial01TravelOptions();
	/** True only for an explicit 0-1 tutorial travel request. */
	GAMEXXK_API bool HasTutorial01TravelOption(const FString& Options);
	/** True only for the isolated tutorial map package, including PIE prefixes. */
	GAMEXXK_API bool IsTutorial01MapPackage(const FString& CurrentPackageName);
	/** Story travel closes the expanded backpack; ordinary town travel keeps its session policy. */
	GAMEXXK_API bool ShouldCollapseBackpackForTravelOptions(const FString& Options);
	GAMEXXK_API FName TownToggleTargetForMapPackage(
		const FString& CurrentPackageName);
	/** Zero keeps legacy maps uncapped; the HUD-only desktop surface is fixed at 30 FPS. */
	GAMEXXK_API float FrameRateLimitForMapPackage(const FString& CurrentPackageName);
	GAMEXXK_API bool OpenMapForRuntimeState(UGameXXKMVPSubsystem* Subsystem);
}
