#include "MVP/GameXXKHeroHitCameraShake.h"

void UGameXXKHeroHitCameraShakePattern::GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const
{
	OutInfo.Duration = FCameraShakeDuration(DurationSeconds);
	OutInfo.BlendIn = 0.0f;
	OutInfo.BlendOut = 0.0f;
}

void UGameXXKHeroHitCameraShakePattern::StartShakePatternImpl(const FCameraShakePatternStartParams& Params)
{
	(void)Params;
	ElapsedSeconds = 0.0f;
}

void UGameXXKHeroHitCameraShakePattern::UpdateShakePatternImpl(
	const FCameraShakePatternUpdateParams& Params,
	FCameraShakePatternUpdateResult& OutResult)
{
	ElapsedSeconds = FMath::Min(DurationSeconds, ElapsedSeconds + FMath::Max(0.0f, Params.DeltaTime));
	const float Progress = FMath::Clamp(ElapsedSeconds / DurationSeconds, 0.0f, 1.0f);
	const float Envelope = FMath::Sin(PI * Progress);
	const float Direction = Progress < 0.5f ? 1.0f : -1.0f;

	// Default flags keep these values additive and let UCameraShakeBase apply the
	// CameraLocal play-space scaling, without changing the authored camera actor.
	OutResult.Location += FVector(0.0f, 1.25f * Envelope * Direction, 0.35f * Envelope);
	OutResult.Rotation += FRotator(0.0f, 0.20f * Envelope * Direction, 0.0f);
}

bool UGameXXKHeroHitCameraShakePattern::IsFinishedImpl() const
{
	return ElapsedSeconds >= DurationSeconds;
}

UGameXXKHeroHitCameraShake::UGameXXKHeroHitCameraShake(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bSingleInstance = true;
	// ChangeRootShakePattern uses NewObject, which is invalid while a UObject
	// default object is being constructed. Create a named default subobject via
	// the constructor's ObjectInitializer instead.
	SetRootShakePattern(ObjectInitializer.CreateDefaultSubobject<UGameXXKHeroHitCameraShakePattern>(
		this,
		TEXT("HeroHitShakePattern")));
}
