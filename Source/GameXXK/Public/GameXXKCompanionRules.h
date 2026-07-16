#pragma once

#include "CoreMinimal.h"
#include "GameXXKCompanionTypes.h"

/** Pure deterministic rules for permanent companion progression and deck ownership. */
class GAMEXXK_API FGameXXKCompanionRules final
{
public:
	static constexpr int32 MaxPermanentCompanions = 12;
	static constexpr int32 MaxCompanionLevel = 20;

	/** Builds the immutable twelve-card pool: four role cores plus eight seeded, distinct choices. */
	static bool BuildPersonalCardPool(
		EGameXXKCharacterRole Role,
		int32 CardSeed,
		TArray<FName>& OutCardIds,
		FString* OutError = nullptr);

	/** Validates immutable identity, deterministic pool, unlock frontier, and selected five-card configuration. */
	static bool ValidatePermanentCompanionProfile(
		const FGameXXKPermanentCompanion& Companion,
		FString* OutError = nullptr);

	/** Creates one persistent recruit ticket from an externally persisted order seed. */
	static bool CreateRecruitOrder(
		FGameXXKCompanionRosterState& InOutRoster,
		int32 RecruitOrderSeed,
		FGameXXKCompanionRecruitOrder& OutOrder,
		FString* OutError = nullptr);

	/** Claims the single stored order; full-roster outcomes retain the same order and candidate until explicitly resolved. */
	static bool ResolvePendingRecruitOrder(
		FGameXXKCompanionRosterState& InOutRoster,
		FGameXXKCompanionRecruitResult& OutResult,
		FString* OutError = nullptr);

	/** Creates a permanent profile or records a duplicate/full-roster outcome without rerolling its seed. */
	static bool RecruitPermanentCompanion(
		FGameXXKCompanionRosterState& InOutRoster,
		FName RecruitTemplateId,
		int32 RecruitSeed,
		FGameXXKCompanionRecruitResult& OutResult,
		FString* OutError = nullptr);

	/** Rebuilds the six-to-twelve deterministic unlock frontier from level and star. */
	static bool RefreshUnlockedPersonalCards(FGameXXKPermanentCompanion& InOutCompanion, FString* OutError = nullptr);

	/** A route configuration must contain exactly five distinct cards from this companion's unlocked pool. */
	static bool ValidateSelectedPersonalCards(
		const FGameXXKPermanentCompanion& Companion,
		const TArray<FName>& SelectedCardIds,
		FString* OutError = nullptr);
	static bool SetSelectedPersonalCards(
		FGameXXKPermanentCompanion& InOutCompanion,
		const TArray<FName>& SelectedCardIds,
		FString* OutError = nullptr);

	/** A task NPC always exposes four fixed cards and the player configures exactly three. */
	static bool ValidateQuestNpcCardSelection(
		FName QuestNpcId,
		const TArray<FName>& SelectedCardIds,
		FString* OutError = nullptr);
	static bool SetQuestNpcCardSelection(
		FGameXXKQuestNpcCardSelection& InOutSelection,
		FName QuestNpcId,
		const TArray<FName>& SelectedCardIds,
		FString* OutError = nullptr);

	/** Marks exactly one persisted companion as active, or clears the optional permanent-partner slot. */
	static bool SetActivePermanentCompanion(
		FGameXXKCompanionRosterState& InOutRoster,
		FName InstanceId,
		FString* OutError = nullptr);

	/**
	 * Commits a full-roster pending candidate after one existing companion is explicitly dismissed.
	 * When dismissing the active companion, the caller must explicitly choose the new active instance
	 * (including the pending candidate) or NAME_None to confirm no permanent partner.
	 */
	static bool ResolvePendingRecruitment(
		FGameXXKCompanionRosterState& InOutRoster,
		FName DismissedInstanceId,
		FName ActivePermanentCompanionInstanceIdAfterReplacement,
		FGameXXKCompanionDismissalRefund& OutRefund,
		FString* OutError = nullptr);

	static int32 GetExperienceRequiredForNextLevel(int32 CurrentLevel);
	static bool AwardExperience(FGameXXKPermanentCompanion& InOutCompanion, int32 ExperienceAmount, FString* OutError = nullptr);
	static bool PromoteCompanionStar(FGameXXKPermanentCompanion& InOutCompanion, int32& InOutSigilCount, FString* OutError = nullptr);
	static bool GetCompanionAttributes(
		EGameXXKCharacterRole Role,
		int32 Level,
		int32 Star,
		const FGameXXKCompanionAttributes& EquipmentBonus,
		FGameXXKCompanionAttributes& OutAttributes,
		FString* OutError = nullptr);
	static bool GetQuestNpcAttributes(
		FName QuestNpcId,
		int32 HeroLevel,
		FGameXXKCompanionAttributes& OutAttributes,
		FString* OutError = nullptr);

	/** Validates the fixed hero + optional one permanent partner + optional one temporary task NPC contract. */
	static bool ValidatePartySelection(
		const FGameXXKCompanionRosterState& Roster,
		const FGameXXKCompanionPartySelection& Selection,
		FString* OutError = nullptr);
};
