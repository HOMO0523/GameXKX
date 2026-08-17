#pragma once

#include "CoreMinimal.h"
#include "GameXXKMVPRules.h"

/** Typed, side-effect-free result for save migration and preview. */
struct GAMEXXK_API FGameXXKSaveMigrationReport
{
	bool bSucceeded = false;
	int32 SourceVersion = 0;
	int32 TargetVersion = 0;
	uint32 SourceChecksum = 0;
	uint32 BackupChecksum = 0;
	FString BackupSlotName;
	bool bCreatedLegacyOverflow = false;
	TArray<FString> Warnings;
	FString Error;
};

/** Pure version dispatcher. Disk transactions are owned by UGameXXKMVPSubsystem. */
class GAMEXXK_API FGameXXKSaveMigration final
{
public:
	static constexpr int32 ThreeChapterRouteIntroducedSaveVersion = 8;
	static constexpr int32 RouteMerchantSnapshotIntroducedSaveVersion = 8;
	static constexpr int32 RouteEconomyIntroducedSaveVersion = 9;
	static constexpr int32 RouteMerchantStockSchemaIntroducedSaveVersion = 10;
	static constexpr int32 MetaShopIntroducedSaveVersion = 11;
	static constexpr int32 HeroCardPoolIntroducedSaveVersion = 12;
	static constexpr int32 CompanionBirthPoolIntroducedSaveVersion = 13;
	static constexpr int32 BladePartnerCardsIntroducedSaveVersion = 14;
	static constexpr int32 QuestFollowerAndCurrentEnemyCodexIntroducedSaveVersion = 15;
	static constexpr int32 BattleRewardTieringIntroducedSaveVersion = 16;
	/** v17: route-card entries (RouteCardIds/RouteCardEntries) were removed; BossCardSlots were added (empty default). */
	static constexpr int32 BossCardSlotsIntroducedSaveVersion = 17;
	/** v18: pure-2D desktop Training progress and challenge/travel state. */
	static constexpr int32 DesktopTrainingWorkbenchIntroducedSaveVersion = 18;
	/** v19: deterministic Training reward seed and Travel chest cooldown state. */
	static constexpr int32 TrainingRewardCooldownsIntroducedSaveVersion = 19;
	/** v20: closed-window Travel pending reward ledger and offline timestamp. */
	static constexpr int32 TrainingOfflineCollectionIntroducedSaveVersion = 20;
	static constexpr int32 CurrentSaveVersion = 20;

	static bool MigrateToCurrent(
		const FGameXXKSaveState& Source,
		FGameXXKSaveState& OutMigrated,
		FGameXXKSaveMigrationReport& OutReport);

	static bool TryRestoreRuntimeState(
		const FGameXXKSaveState& Source,
		FGameXXKRuntimeState& OutRuntimeState,
		FGameXXKSaveMigrationReport& OutReport);

	/** Pure compatibility-aware validation. It never initializes or repairs card/route state. */
	static bool ValidateRuntimeState(const FGameXXKRuntimeState& State, FString& OutError);
};
