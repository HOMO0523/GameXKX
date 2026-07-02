#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "UI/GameXXKMVPCommandDescriptor.h"
#include "GameXXKMVPHUD.generated.h"

class UGameXXKMVPSubsystem;
class UGameXXKPlayableRootWidget;

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPHUD : public AHUD
{
	GENERATED_BODY()

public:
	AGameXXKMVPHUD();

	virtual void BeginPlay() override;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Playable")
	bool HandleDemoCommand(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	FText BuildStatusText() const;

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);
	void SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex);
	UGameXXKPlayableRootWidget* CreatePlayableRootWidgetForTest();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	bool HasPlayableRootWidget() const;

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	UGameXXKPlayableRootWidget* GetPlayableRootWidget() const;

	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

protected:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;

private:
	UGameXXKPlayableRootWidget* CreatePlayableRootWidget();

	UPROPERTY(EditDefaultsOnly, Category = "GameXXK|Playable")
	TSubclassOf<UGameXXKPlayableRootWidget> PlayableRootWidgetClass;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKPlayableRootWidget> PlayableRootWidget;

	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	FString StartGameSlotNameOverride;

	UPROPERTY(Transient)
	int32 StartGameUserIndexOverride = 0;
};
