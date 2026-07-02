#pragma once

#include "CoreMinimal.h"
#include "UI/GameXXKMVPWidgetBase.h"
#include "GameXXKCharacterPanelWidget.generated.h"

USTRUCT(BlueprintType)
struct FGameXXKCharacterSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	FName DisplayName;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	int32 HP = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	int32 Attack = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, Category = "GameXXK|Character")
	int32 Speed = 0;
};

UCLASS(Blueprintable)
class GAMEXXK_API UGameXXKCharacterPanelWidget : public UGameXXKMVPWidgetBase
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, Category = "GameXXK|Character")
	void SetCharacterSummary(const FGameXXKCharacterSummary& InSummary);

	UFUNCTION(BlueprintCallable, Category = "GameXXK|Character")
	bool RefreshPlayerSummary();

	UFUNCTION(BlueprintPure, Category = "GameXXK|Character")
	const FGameXXKCharacterSummary& GetCharacterSummary() const;

	UFUNCTION(BlueprintImplementableEvent, Category = "GameXXK|Character")
	void OnCharacterSummaryChanged();

private:
	UPROPERTY(Transient)
	FGameXXKCharacterSummary Summary;
};
