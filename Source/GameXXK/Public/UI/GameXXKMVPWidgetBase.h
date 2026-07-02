#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameXXKMVPWidgetBase.generated.h"

class UGameXXKMVPSubsystem;

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKMVPWidgetBase : public UUserWidget
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|MVP")
	void SetMVPSubsystem(UGameXXKMVPSubsystem* InSubsystem);

	UFUNCTION(BlueprintPure, Category = "GameXXK|MVP")
	UGameXXKMVPSubsystem* GetMVPSubsystem() const;

protected:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;
};
