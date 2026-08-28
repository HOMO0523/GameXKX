#pragma once

#include "CoreMinimal.h"

#include "GameXXKInteractionRules.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKInteractionCandidate
{
	GENERATED_BODY()

	FGameXXKInteractionCandidate() = default;
	FGameXXKInteractionCandidate(FName InInteractionId, int32 InPriority, float InDistance)
		: InteractionId(InInteractionId)
		, Priority(InPriority)
		, Distance(InDistance)
	{
	}

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	FName InteractionId;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	int32 Priority = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Interaction")
	float Distance = 0.0f;
};

class GAMEXXK_API FGameXXKInteractionRules final
{
public:
	static TOptional<FGameXXKInteractionCandidate> Choose(
		const TArray<FGameXXKInteractionCandidate>& Candidates);
};
