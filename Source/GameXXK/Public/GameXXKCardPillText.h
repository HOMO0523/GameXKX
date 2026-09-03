#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"

/** Card keywords only. Resource-bar and target explanations do not belong here. */
namespace GameXXKCardPillText
{
	GAMEXXK_API const TArray<FString>& InlineNames();
	GAMEXXK_API bool IsKeyword(const FString& Name);
	GAMEXXK_API FString DescribeHelp(const FString& CardText, EGameXXKCardQuality Quality, int32 TaskCardCount);
}
