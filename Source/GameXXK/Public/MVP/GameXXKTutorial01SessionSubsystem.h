#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"
#include "Guide/GameXXKGuideAsset.h"
#include "MVP/GameXXKTutorial01RouteRules.h"
#include "Subsystems/GameInstanceSubsystem.h"

#include "GameXXKTutorial01SessionSubsystem.generated.h"

UENUM()
enum class EGameXXKTutorial01ReturnReason : uint8
{
	None,
	Victory,
	Defeat,
};

USTRUCT()
struct GAMEXXK_API FGameXXKTutorial01ReturnContext
{
	GENERATED_BODY()

	UPROPERTY()
	FGameXXKRuntimeState RuntimeBeforeTutorial;

	UPROPERTY()
	FTransform StatueReturnTransform = FTransform::Identity;

	UPROPERTY()
	EGameXXKGuidePreference GuidePreference = EGameXXKGuidePreference::Unset;

	UPROPERTY()
	EGameXXKTutorial01ReturnReason ReturnReason =
		EGameXXKTutorial01ReturnReason::None;

	UPROPERTY()
	bool bActive = false;

	UPROPERTY(Transient)
	FGameXXKTutorial01RouteState RouteState;

	UPROPERTY(Transient)
	FGameXXKGuideProgress TutorialGuideProgress;
};

UCLASS()
class GAMEXXK_API UGameXXKTutorial01SessionSubsystem
	: public UGameInstanceSubsystem
{
	GENERATED_BODY()

public:
	bool BeginFromTown(
		const FGameXXKRuntimeState& RuntimeBeforeTutorial,
		const FTransform& StatueReturnTransform,
		EGameXXKGuidePreference GuidePreference);
	bool BuildRouteRuntime(FGameXXKRuntimeState& OutRuntimeState) const;
	bool BuildBattleSeedRuntime(
		FGameXXKRuntimeState& OutRuntimeState,
		FString* OutError = nullptr);
	bool ArrangeDeterministicOpeningHand(
		FGameXXKRuntimeState& InOutRuntimeState,
		FString* OutError = nullptr) const;
	bool MarkBattleVictory(FGameXXKRuntimeState& OutRouteRuntime);
	bool MarkBattleDefeat();
	bool RequestRouteNode(
		int32 NodeId,
		EGameXXKTutorial01RouteAction& OutAction);
	TArray<FGameXXKRouteMapNode> BuildRouteNodes() const;
	TArray<FGameXXKRouteMapEdge> BuildRouteEdges() const;
	TMap<int32, FText> BuildRouteLabels() const;
	FText BuildRouteCompletionNotice() const;
	const FGameXXKTutorial01RouteState& GetRouteState() const
	{
		return Context.RouteState;
	}
	FGameXXKGuideProgress& GetMutableGuideProgress();
	void ResetGuideForRetry();
	bool PrepareRetry(FGameXXKRuntimeState& OutRuntimeState);
	bool RestoreForTownReturn(
		EGameXXKTutorial01ReturnReason ReturnReason,
		FGameXXKRuntimeState& OutRuntimeState);
	bool ConsumeTownReturn(FGameXXKTutorial01ReturnContext& OutContext);
	void CancelSession();

	bool HasActiveSession() const { return Context.bActive; }
	EGameXXKGuidePreference GetGuidePreference() const;
	const FGameXXKTutorial01ReturnContext& GetContextForTest() const
	{
		return Context;
	}

private:
	UPROPERTY(Transient)
	FGameXXKTutorial01ReturnContext Context;
};
