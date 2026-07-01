#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "GameXXKMVPHUD.generated.h"

class UGameXXKMVPSubsystem;

USTRUCT(BlueprintType)
struct FGameXXKMVPCommandDescriptor
{
	GENERATED_BODY()

	FGameXXKMVPCommandDescriptor() = default;
	FGameXXKMVPCommandDescriptor(FName InCommandName, const FText& InLabel, bool bInEnabled);

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FName CommandName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	bool bEnabled = true;
};

UCLASS(Blueprintable)
class GAMEXXK_API AGameXXKMVPHUD : public AHUD
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	TArray<FGameXXKMVPCommandDescriptor> BuildVisibleCommands() const;

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Playable")
	bool HandleDemoCommand(FName CommandName);

	UFUNCTION(BlueprintPure, Category = "GameXXK|Playable")
	FText BuildStatusText() const;

	void SetMVPSubsystemForTest(UGameXXKMVPSubsystem* InSubsystem);
	void SetStartGameSlotForTest(const FString& SlotName, int32 UserIndex);

	virtual void DrawHUD() override;
	virtual void NotifyHitBoxClick(FName BoxName) override;

protected:
	UGameXXKMVPSubsystem* ResolveMVPSubsystem() const;

private:
	UPROPERTY(Transient)
	TObjectPtr<UGameXXKMVPSubsystem> OverrideSubsystem;

	UPROPERTY(Transient)
	FString StartGameSlotNameOverride;

	UPROPERTY(Transient)
	int32 StartGameUserIndexOverride = 0;
};
