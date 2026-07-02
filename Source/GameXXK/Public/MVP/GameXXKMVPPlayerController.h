#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "GameXXKMVPPlayerController.generated.h"

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;
	virtual void FlushPressedKeys() override;
};
