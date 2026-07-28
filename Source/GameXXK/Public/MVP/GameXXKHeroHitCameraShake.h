#pragma once

#include "CoreMinimal.h"
#include "Camera/CameraShakeBase.h"
#include "GameXXKHeroHitCameraShake.generated.h"

/** A tiny, fixed-duration local recoil used only after the main Hero loses HP. */
UCLASS()
class GAMEXXK_API UGameXXKHeroHitCameraShakePattern : public UCameraShakePattern
{
	GENERATED_BODY()

protected:
	virtual void GetShakePatternInfoImpl(FCameraShakeInfo& OutInfo) const override;
	virtual void StartShakePatternImpl(const FCameraShakePatternStartParams& Params) override;
	virtual void UpdateShakePatternImpl(const FCameraShakePatternUpdateParams& Params, FCameraShakePatternUpdateResult& OutResult) override;
	virtual bool IsFinishedImpl() const override;

private:
	static constexpr float DurationSeconds = 0.12f;
	float ElapsedSeconds = 0.0f;
};

/** Runtime-only camera shake; it applies an additive CameraLocal offset and never edits camera actors. */
UCLASS()
class GAMEXXK_API UGameXXKHeroHitCameraShake : public UCameraShakeBase
{
	GENERATED_BODY()

public:
	UGameXXKHeroHitCameraShake(const FObjectInitializer& ObjectInitializer = FObjectInitializer::Get());
};
