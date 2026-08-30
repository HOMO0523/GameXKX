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
	GAMEXXK_API FName TownToggleTargetForMapPackage(
		const FString& CurrentPackageName);
	/** Zero keeps legacy maps uncapped; the HUD-only desktop surface is fixed at 30 FPS. */
	GAMEXXK_API float FrameRateLimitForMapPackage(const FString& CurrentPackageName);
	GAMEXXK_API bool OpenMapForRuntimeState(UGameXXKMVPSubsystem* Subsystem);
}
