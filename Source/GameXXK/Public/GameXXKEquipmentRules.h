#pragma once

#include "CoreMinimal.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKEquipmentSetCatalog.h"
#include "GameXXKEquipmentTypes.h"

#include "GameXXKEquipmentRules.generated.h"

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentCreateRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 ItemLevel = 1;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bForceSlot = false;

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	EGameXXKEquipmentSlot ForcedSlot = EGameXXKEquipmentSlot::Invalid;
};

UENUM(BlueprintType)
enum class EGameXXKEquipmentTransactionError : uint8
{
	None = 0,
	InvalidRequest,
	InstanceMissing,
	DefinitionMissing,
	CollectionInvalid,
	WarehouseFull,
	InvalidOwner,
	SlotMismatch,
	ItemNotInWarehouse,
	ConfirmationRequired,
	InsufficientEnhancementStones,
	MaxEnhancementReached,
	InsufficientRefinementSand,
	PendingReforgeExists,
	NoPendingReforge,
	PendingReforgeStale,
	RouteLocked,
	SaveMigrationFailed
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentTransactionResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bSucceeded = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentTransactionError Error = EGameXXKEquipmentTransactionError::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Message;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FName> AffectedInstanceIds;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bConfirmationRequired = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 EnhancementStoneDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 RefinementSandDelta = 0;
};

/** Complete equipment projection before route/relic/terrain/status modifiers. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentLoadoutSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName CharacterId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats BareStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats EnhancedEquipmentBaseStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats AttributesBeforeRoute;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<EGameXXKEquipmentModifierKind, int32> UniversalModifiers;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<EGameXXKEquipmentModifierKind, int32> SetModifiers;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<EGameXXKEquipmentSet, int32> SetPieceCounts;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKEquipmentActiveEffect> ActivePersonalEffects;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKEquipmentActiveEffect> CandidateTeamEffects;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 TeamEffectSourceScore = 0;
};

/** One immutable warehouse/equipped item view compared through two complete character loadouts. */
USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKEquipmentTooltipSnapshot
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName InstanceId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName BaseEquipmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentSlot Slot = EGameXXKEquipmentSlot::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 ItemLevel = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 EnhancementLevel = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats ItemBaseStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats ItemCurrentStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TArray<FGameXXKEquipmentAffixRoll> Affixes;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<EGameXXKEquipmentSet, int32> CurrentSetPieceCounts;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	TMap<EGameXXKEquipmentSet, int32> CandidateSetPieceCounts;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats CurrentCharacterStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats CandidateCharacterStats;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCharacterStats CharacterStatDeltas;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentTransactionError EquipError = EGameXXKEquipmentTransactionError::None;
};

/** Pure deterministic collection rules. Cross-resource transactions belong to the economy layer. */
class GAMEXXK_API FGameXXKEquipmentRules final
{
public:
	static constexpr int32 WarehouseCapacity = 200;
	static constexpr int32 MaxEnhancementLevel = 10;
	static constexpr int32 MaxItemLevel = 20;

	static FName HeroCharacterId();
	static FName GetLoadoutSlotInstanceId(
		const FGameXXKEquipmentLoadout& Loadout,
		EGameXXKEquipmentSlot Slot);
	static const FGameXXKEquipmentInstance* FindInstance(
		const FGameXXKEquipmentCollectionState& Collection,
		FName InstanceId);
	static int32 CountWarehouseItems(const FGameXXKEquipmentCollectionState& Collection);
	/** Deterministically orders warehouse instances for the single UI sort action. */
	static bool SortWarehouseInstanceIds(
		FGameXXKEquipmentCollectionState& InOutCollection,
		FString* OutError = nullptr);
	static bool HasWarehouseCapacity(
		const FGameXXKEquipmentCollectionState& Collection,
		int32 RequiredSlots = 1);
	static bool ValidateCollectionState(
		const FGameXXKEquipmentCollectionState& Collection,
		FString* OutError = nullptr);
	static bool ValidateCollectionAgainstRoster(
		const FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKCompanionRosterState& Roster,
		FString* OutError = nullptr);
	static bool CreateRolledInstance(
		FGameXXKEquipmentCollectionState& InOutCollection,
		const FGameXXKEquipmentCreateRequest& Request,
		FName& OutInstanceId,
		FString* OutError = nullptr);
	static FGameXXKEquipmentTransactionResult EquipInstance(
		FGameXXKEquipmentCollectionState& InOutCollection,
		const FGameXXKCompanionRosterState& Roster,
		FName CharacterId,
		EGameXXKEquipmentSlot Slot,
		FName InstanceId);
	static FGameXXKEquipmentTransactionResult UnequipInstance(
		FGameXXKEquipmentCollectionState& InOutCollection,
		FName CharacterId,
		EGameXXKEquipmentSlot Slot);
	static FGameXXKEquipmentTransactionResult ReturnAllEquipmentToWarehouse(
		FGameXXKEquipmentCollectionState& InOutCollection,
		FName CharacterId);
	static bool BuildLoadoutSnapshot(
		const FGameXXKEquipmentCollectionState& Collection,
		FName CharacterId,
		const FGameXXKCharacterStats& BareStats,
		FGameXXKEquipmentLoadoutSnapshot& OutSnapshot,
		FString* OutError = nullptr);
	static bool BuildTooltipSnapshot(
		const FGameXXKEquipmentCollectionState& Collection,
		FName InstanceId,
		FName CompareCharacterId,
		const FGameXXKCharacterStats& CompareBareStats,
		FGameXXKEquipmentTooltipSnapshot& OutSnapshot,
		FString* OutError = nullptr);
	static TArray<FGameXXKEquipmentActiveEffect> ResolveTeamEffects(
		const TArray<FGameXXKEquipmentLoadoutSnapshot>& Snapshots);
	/** Verifies that a persisted battle descriptor can only originate from the immutable set/affix catalogs. */
	static bool IsKnownActiveEffect(const FGameXXKEquipmentActiveEffect& Effect);
	static FGameXXKCharacterStats ApplyPostEquipmentModifiers(
		const FGameXXKCharacterStats& AttributesBeforeRoute,
		const TMap<EGameXXKEquipmentModifierKind, int32>& BasisPointModifiers,
		const TMap<EGameXXKEquipmentModifierKind, int32>& FlatCountModifiers);
	static FText GetTransactionErrorMessage(EGameXXKEquipmentTransactionError Error);
};
