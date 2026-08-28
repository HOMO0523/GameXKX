#pragma once

#include "CoreMinimal.h"

#include "GameXXKBattleProfile.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleAnchor
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	FName AnchorId;

	/** Viewport-relative placement. Battle profiles deliberately own no world transforms. */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	FVector2D NormalizedPosition = FVector2D::ZeroVector;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKBattleProfileDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	FName BattleProfileId;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	TArray<FGameXXKBattleAnchor> PartyAnchors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	TArray<FGameXXKBattleAnchor> EnemyAnchors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	TArray<FGameXXKBattleAnchor> CameraAnchors;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Narrative|Battle")
	TArray<FGameXXKBattleAnchor> VfxAnchors;
};

class GAMEXXK_API FGameXXKBattleProfileCatalog
{
public:
	static const FGameXXKBattleProfileDefinition* Find(FName BattleProfileId);
	static bool Validate(const FGameXXKBattleProfileDefinition& Profile, FString* OutError = nullptr);
};
