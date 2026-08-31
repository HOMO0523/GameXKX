#pragma once

#include "CoreMinimal.h"
#include "Guide/GameXXKGuideAsset.h"

#include "GameXXKGuideRules.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKGuideOutput
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	bool bActive = false;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	bool bCompleted = false;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	FName GuideId;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	FName StepId;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	TArray<FName> TargetIds;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	FName BubbleAnchorTargetId;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	EGameXXKGuideInputPolicy InputPolicy = EGameXXKGuideInputPolicy::Soft;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	FText Text;

	UPROPERTY(BlueprintReadOnly, Category = "Guide")
	TSet<FName> AllowedActionIds;
};

class GAMEXXK_API FGameXXKGuideRules final
{
public:
	static bool TryStart(
		const UGameXXKGuideAsset& Asset,
		FName TriggerEventId,
		FGameXXKGuideProgress& InOutProgress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError = nullptr);

	static bool HandleEvent(
		const UGameXXKGuideAsset& Asset,
		FName EventId,
		FGameXXKGuideProgress& InOutProgress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError = nullptr);

	static bool Resume(
		const UGameXXKGuideAsset& Asset,
		FGameXXKGuideProgress& InOutProgress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError = nullptr);

	static bool HandleTargetUnavailable(
		const UGameXXKGuideAsset& Asset,
		FName TargetId,
		FGameXXKGuideProgress& InOutProgress,
		FGameXXKGuideOutput& OutOutput,
		FString* OutError = nullptr);

	static bool CanExecuteAction(
		const UGameXXKGuideAsset& Asset,
		const FGameXXKGuideProgress& Progress,
		FName ActionId);

	static void Cancel(FGameXXKGuideProgress& InOutProgress, const FString& Diagnostic = FString());
	static void ResetCombatGuide(FGameXXKGuideProgress& InOutProgress);
};
