#pragma once

#include "CoreMinimal.h"

class UGameXXKMVPSubsystem;
enum class EGameXXKScreen : uint8;
struct FGameXXKRuntimeState;

namespace GameXXKLevelFlow
{
	GAMEXXK_API FName MapForScreen(EGameXXKScreen Screen);
	GAMEXXK_API FName MapForRuntimeState(const FGameXXKRuntimeState& State);
	GAMEXXK_API bool MapPackageMatches(const FString& CurrentPackageName, FName TargetMap);
	GAMEXXK_API bool OpenMapForRuntimeState(UGameXXKMVPSubsystem* Subsystem);
}
