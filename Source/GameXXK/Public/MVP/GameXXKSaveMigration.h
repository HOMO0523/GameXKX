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
	/** v21: physical desktop backpack/warehouse cells and persistent warehouse partition. */
	static constexpr int32 DesktopInventoryStorageIntroducedSaveVersion = 21;
	/** v22: approved named NPCs can own central equipment loadouts via the QuestNpc owner kind. */
	static constexpr int32 QuestNpcEquipmentOwnerIntroducedSaveVersion = 22;
	/** v23: generated-route combat saves retain the exact pre-encounter rollback checkpoint. */
	static constexpr int32 BattleRetreatCheckpointIntroducedSaveVersion = 23;
	/** v24: ordered 1P / 2P / 3P party references become save-authoritative. */
	static constexpr int32 OrderedPartyFormationIntroducedSaveVersion = 24;
	/** v25 begins the equipment-tools/chest-wallet schema with persistent entry locks and the Tool Auto Fill preference. */
	static constexpr int32 EquipmentToolsAndChestWalletIntroducedSaveVersion = 25;
	/** v26 adds permanent shared talent ranks and compatibility capacity floors. */
	static constexpr int32 PermanentTalentGraphIntroducedSaveVersion = 26;
	/** v27 adds the independent new-player story/tutorial quest state. */
	static constexpr int32 TutorialQuestIntroducedSaveVersion = 27;
	/** v28 adds node-boundary dialogue session persistence. */
	static constexpr int32 DialogueRuntimeIntroducedSaveVersion = 28;
	/** v29 adds multi-story/task, sequence-session and semantic combat-guide persistence. */
	static constexpr int32 NarrativeStageGuideIntroducedSaveVersion = 29;
	/** v30: ordered formation always owns one permanent NPC; NPC route encounters are retired. */
	static constexpr int32 PermanentNpcFormationIntroducedSaveVersion = 30;
	/** v31: the unique tutorial map and pending full-inventory delivery become persistent. */
	static constexpr int32 TutorialMapItemIntroducedSaveVersion = 31;
	/** v32: retire the disconnected Xu Xiake StoryTask and town-NPC option sessions. */
	static constexpr int32 RetiredLegacyTutorialNarrativeSaveVersion = 32;
	/** v33: persist battle level/difficulty scaling and deferred shared-energy denial. */
	static constexpr int32 CombatScalingFoundationIntroducedSaveVersion = 33;
	/** v34: retire 25 run-local route cards while retaining five Boss compatibility IDs. */
	static constexpr int32 ActiveCardPool173IntroducedSaveVersion = 34;
	static constexpr int32 CurrentSaveVersion = 34;

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
