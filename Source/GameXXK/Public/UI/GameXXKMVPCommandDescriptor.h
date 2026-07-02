#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPCommandDescriptor.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMVPCommandDescriptor
{
	GENERATED_BODY()

	FGameXXKMVPCommandDescriptor() = default;

	FGameXXKMVPCommandDescriptor(FName InCommandName, const FText& InLabel, bool bInEnabled)
		: CommandName(InCommandName)
		, Label(InLabel)
		, bEnabled(bInEnabled)
	{
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FName CommandName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	bool bEnabled = true;
};
