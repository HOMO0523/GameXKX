#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPCommandDescriptor.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMVPCommandDescriptor
{
	GENERATED_BODY()

	FGameXXKMVPCommandDescriptor() = default;

	FGameXXKMVPCommandDescriptor(
		FName InCommandName,
		const FText& InLabel,
		bool bInEnabled,
		const FText& InDisabledReason = FText::GetEmpty())
		: CommandName(InCommandName)
		, Label(InLabel)
		, bEnabled(bInEnabled)
		, DisabledReason(InDisabledReason)
	{
	}

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FName CommandName;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FText Label;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	bool bEnabled = true;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "GameXXK|Playable")
	FText DisabledReason;
};
