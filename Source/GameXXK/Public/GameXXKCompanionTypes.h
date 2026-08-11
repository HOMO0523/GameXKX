#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.h"
#include "GameXXKCompanionTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKCompanionRecruitOutcome : uint8
{
	Invalid = 0 UMETA(Hidden),
	Recruited = 1,
	DuplicateSigil = 2,
	PendingReplacement = 3
};

/** Immutable visual and role identity for one possible permanent companion recruit. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionTemplateDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName TemplateId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName PortraitVariantKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName NamePoolKey = NAME_None;
};

/** Save-compatible, permanent progression and personal-card state for one recruited companion. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPermanentCompanion
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName RecruitTemplateId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName PortraitVariantId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NameSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Level = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Experience = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Star = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CardSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> PersonalCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> UnlockedPersonalCardIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> SelectedCardIds;

	/**
	 * Deprecated pre-v7 migration source. EquipmentCollection is authoritative after migration;
	 * current recruitment, dismissal, replacement, and equipment gameplay must never write this array.
	 */
	UPROPERTY(BlueprintReadOnly, VisibleAnywhere, SaveGame, meta = (DeprecatedProperty, DeprecationMessage = "Pre-v7 migration source only; EquipmentCollection is authoritative."))
	TArray<FName> EquippedItemIds;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bIsActive = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bIsNew = true;
};

/** A fixed full-roster recruit result that cannot be rerolled while replacement is pending. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKPendingCompanionRecruitment
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bHasPendingRecruitment = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPermanentCompanion Candidate;
};

/**
 * A deterministic recruit ticket.  The order is saved before it is claimed so
 * reopening a panel, reloading, or cancelling a full roster replacement cannot
 * change the result.
 */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionRecruitOrder
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	bool bHasPendingOrder = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RecruitOrderSeed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ResolvedTemplateId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 CardSeed = 0;
};

/** All resources that must be transferred back to the shared inventory when an old companion is dismissed. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionDismissalRefund
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName DismissedInstanceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ReturnedExperienceMaterials = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> ReturnedEquippedItemIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bDismissedWasActive = false;
};

/** Persistent companion-owned state, intentionally isolated from legacy MVP runtime state. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionRosterState
{
	GENERATED_BODY()

	/**
	 * Immutable key for this save's recruit sequence. Zero is deliberately reserved so old saves can
	 * lazily receive the safe default key without a save-version migration.
	 */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RecruitSequenceSeed = 0;

	/** Next deterministic ticket ordinal; it advances only after a new ticket is persisted. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 RecruitSequenceOrdinal = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FGameXXKPermanentCompanion> PermanentCompanions;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 SigilCount = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKPendingCompanionRecruitment PendingRecruitment;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKCompanionRecruitOrder PendingRecruitOrder;
};

/** Result payload for a deterministic recruit attempt; duplicate and full states do not silently reroll. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionRecruitResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKCompanionRecruitOutcome Outcome = EGameXXKCompanionRecruitOutcome::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKPermanentCompanion Companion;
};

/** Integer combat values after applying level, star, and later equipment progression. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionAttributes
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Health = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Attack = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Defense = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Mana = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 Speed = 0;
};

/** Per-level attribute increments; fractional attack/defense values are intentional. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionAttributeGrowth
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Health = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Attack = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Defense = 0.0f;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float Mana = 0.0f;
};

/** Immutable definition of a named, temporary task NPC. It never represents a recruitable companion. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKQuestNpcDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName NpcId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName PassiveId = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FName PortraitKey = NAME_None;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKCompanionAttributes BaseAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	FGameXXKCompanionAttributeGrowth GrowthPerLevel;

	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> FixedCardIds;

	/** Deprecated authored fallback retained for data compatibility; runtime routes use seeded four-choose-three selection. */
	UPROPERTY(BlueprintReadOnly, EditAnywhere)
	TArray<FName> DefaultRouteCardIds;
};

/** Persisted route-local three-card result for one temporary task NPC. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKQuestNpcCardSelection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName NpcId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	TArray<FName> SelectedCardIds;
};

/** Route-ready party selection; the fixed hero is implicit, so this can never encode a fourth combat member. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKCompanionPartySelection
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FName ActivePermanentCompanionInstanceId = NAME_None;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	FGameXXKQuestNpcCardSelection QuestNpc;
};
