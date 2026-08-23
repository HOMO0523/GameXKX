#pragma once

#include "CoreMinimal.h"
#include "GameXXKPartyFormationTypes.generated.h"

/** Append-only serialized identity for a deployed party member. */
UENUM(BlueprintType)
enum class EGameXXKPartyMemberKind : uint8
{
	Invalid = 0 UMETA(Hidden),
	Hero = 1,
	PermanentCompanion = 2,
	QuestNpc = 3
};

/** Stable save reference; runtime attributes and cards remain owned by their existing authoritative records. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPartyMemberRef
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKPartyMemberKind Kind = EGameXXKPartyMemberKind::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName MemberId = NAME_None;

	bool IsValid() const
	{
		return Kind != EGameXXKPartyMemberKind::Invalid && !MemberId.IsNone();
	}

	bool operator==(const FGameXXKPartyMemberRef& Other) const
	{
		return Kind == Other.Kind && MemberId == Other.MemberId;
	}
};

/** Authoritative ordered 1P / 2P / 3P deployment. Rules enforce exactly three entries. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKOrderedPartyFormation
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKPartyMemberRef> Members;
};
