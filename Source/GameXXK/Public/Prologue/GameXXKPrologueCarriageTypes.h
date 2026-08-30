#pragma once

#include "CoreMinimal.h"
#include "GameXXKPrologueCarriageTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKPrologueCarriagePhase : uint8
{
	Dormant,
	Arriving,
	Parked,
	HeroRevealed,
	HoldTwoSeconds,
	Departing,
	Handoff,
	Finished,
	Cancelled,
};

UENUM(BlueprintType)
enum class EGameXXKPrologueCarriageAtlas : uint8
{
	None,
	RunStop,
	PostStopIdle,
};

UENUM(BlueprintType)
enum class EGameXXKPrologueCarriageFailure : uint8
{
	MissingArrivalTexture,
	MissingIdleTexture,
	MissingHero,
	PlaybackTimeout,
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPrologueCarriageConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	float ArrivalSeconds = 4.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	float HoldSeconds = 2.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	float DepartureSeconds = 4.04f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	float FramesPerSecond = 14.851485f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	int32 FullFrameCount = 60;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	int32 DepartureFirstFrame = 0;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "GameXXK|Prologue|Carriage")
	int32 DepartureLastFrame = 35;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPrologueCarriageState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	EGameXXKPrologueCarriagePhase Phase = EGameXXKPrologueCarriagePhase::Dormant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	float PhaseElapsedSeconds = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bPaused = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bFinishBroadcastConsumed = false;

	bool operator==(const FGameXXKPrologueCarriageState& Other) const
	{
		return Phase == Other.Phase
			&& FMath::IsNearlyEqual(PhaseElapsedSeconds, Other.PhaseElapsedSeconds)
			&& bPaused == Other.bPaused
			&& bFinishBroadcastConsumed == Other.bFinishBroadcastConsumed;
	}
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPrologueCarriageStepOutput
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	EGameXXKPrologueCarriagePhase PreviousPhase = EGameXXKPrologueCarriagePhase::Dormant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	EGameXXKPrologueCarriagePhase CurrentPhase = EGameXXKPrologueCarriagePhase::Dormant;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	EGameXXKPrologueCarriageAtlas Atlas = EGameXXKPrologueCarriageAtlas::None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	int32 AtlasFrameIndex = 0;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	float MotionAlpha = 0.0f;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bSwitchToIdle = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bRevealHero = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bBeginDeparture = false;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "GameXXK|Prologue|Carriage")
	bool bBeginHandoff = false;
};
