#include "GameXXKMVPRules.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKRelicRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRouteSettlementRules.h"
#include "MVP/GameXXKSaveMigration.h"

namespace GameXXKMVP
{
	static const FName RegionQingshanName(TEXT("Region.Qingshan"));
	static const FName RegionHuangshanName(TEXT("Region.Huangshan"));
	static const FName RegionTanjiangName(TEXT("Region.Tanjiang"));
	static const FName ItemHealingPowderName(TEXT("Item.HealingPowder"));
	static const FName ItemEnhancementStoneName(TEXT("Item.EnhancementStone"));
	static const FName ItemRefinementSandName(TEXT("Item.RefinementSand"));
	static const FName ItemQingshanRouteSealName(TEXT("Item.QingshanRouteSeal"));
	static const FName ItemTrainingNormalChestName(TEXT("Item.TrainingNormalChest"));
	static const FName ItemTrainingAdvancedChestName(TEXT("Item.TrainingAdvancedChest"));
	static const FName ItemLingzhiPowderName(TEXT("Item.LingzhiPowder"));
	static const FName ItemQingxinTeaName(TEXT("Item.QingxinTea"));
	static const FName ItemCraneSachetName(TEXT("Item.CraneSachet"));
	static const FName ItemIronSwordName(TEXT("Item.IronSword"));
	static const FName ItemClothArmorName(TEXT("Item.ClothArmor"));
	static const FName ItemCranePatternTalismanName(TEXT("Item.CranePatternTalisman"));
	static const FName ItemInkstonePendantName(TEXT("Item.InkstonePendant"));
	static const FName ItemWoodenSwordName(TEXT("Item.WoodenSword"));
	static const FName ItemStarterClothArmorName(TEXT("Item.StarterClothArmor"));
	static const FName ItemClothTalismanName(TEXT("Item.ClothTalisman"));
	static const FName TaskQingshanMainName(TEXT("Task.QingshanMain"));
	static const FName QuestNpcTusiChiefName(TEXT("Npc.TusiChief"));
	static const FName CodexGuideName(TEXT("Codex.Guide"));
	static const FName CodexMoneyRatName(TEXT("Codex.Enemy.Ch1.MoneyRat"));
	static const FName CodexBlackBearName(TEXT("Codex.Enemy.Ch2.BlackBear"));
	static const FName CodexTigerName(TEXT("Codex.Enemy.Ch3.Tiger"));
	static const FName PreviousCodexMoneyRatName(TEXT("Codex.MoneyRat"));
	static const FName PreviousCodexBlackBearName(TEXT("Codex.BlackBear"));
	static const FName PreviousCodexTigerName(TEXT("Codex.Tiger"));
	static const FName LegacyCodexBanditName(TEXT("Codex.Bandit"));
	static const FName LegacyCodexWolfName(TEXT("Codex.Wolf"));
	static const FName LegacyCodexEliteBanditName(TEXT("Codex.EliteBandit"));
	static const FName LegacyCodexBossName(TEXT("Codex.Boss"));
	static const FName BattleRuntimeBanditName(TEXT("Bandit"));
	static const FName BattleRuntimeWolfName(TEXT("Wolf"));
	static const FName BattleRuntimeEliteBanditName(TEXT("EliteBandit"));
	static const FName BattleRuntimeBossName(TEXT("Boss"));
	static const FName BattleRuntimeMoneyRatName(TEXT("MoneyRat"));
	static const FName BattleRuntimeBlackBearName(TEXT("BlackBear"));
	static const FName BattleRuntimeTigerName(TEXT("Tiger"));
	static constexpr int32 CurrentSaveVersion = FGameXXKSaveMigration::CurrentSaveVersion;
	static constexpr int32 GuideCodexIntroductionSaveVersion = 5;
	static constexpr int32 CurrentEnemyCodexMigrationSaveVersion = 6;
	static constexpr int32 MaxItemEnhancementLevel = 10;
	static constexpr int32 NormalPermanentCompanionBattleExperience = 25;
	static constexpr int32 ElitePermanentCompanionBattleExperience = 40;
	static constexpr int32 BossPermanentCompanionBattleExperience = 60;

	static FGameXXKItemDef MakeItem(FName Id, const TCHAR* DisplayName, EGameXXKItemKind Kind, int32 Buy, int32 Sell, int32 Heal, int32 MPHeal, int32 Attack, int32 Defense, int32 MaxHP, int32 MaxMP)
	{
		FGameXXKItemDef Def;
		Def.Id = Id;
		Def.DisplayName = FText::FromString(DisplayName);
		Def.Kind = Kind;
		Def.BuyPrice = Buy;
		Def.SellPrice = Sell;
		Def.HealAmount = Heal;
		Def.MPHealAmount = MPHeal;
		Def.AttackBonus = Attack;
		Def.DefenseBonus = Defense;
		Def.MaxHPBonus = MaxHP;
		Def.MaxMPBonus = MaxMP;
		return Def;
	}

	static FGameXXKCodexEntryDef MakeCodexEntry(
		FName Id,
		EGameXXKCodexCategory Category,
		const TCHAR* DisplayName,
		const TCHAR* Description)
	{
		FGameXXKCodexEntryDef Def;
		Def.Id = Id;
		Def.Category = Category;
		Def.DisplayName = FText::FromString(DisplayName);
		Def.Description = FText::FromString(Description);
		return Def;
	}

	static const TArray<FGameXXKCodexEntryDef>& GetCodexEntryDefsInternal()
	{
		static const TArray<FGameXXKCodexEntryDef> EntryDefs = []
		{
			TArray<FGameXXKCodexEntryDef> Definitions;
			const TArray<FGameXXKEnemyDefinition>& EnemyDefinitions = FGameXXKEnemyCatalog::GetAllDefinitions();
			Definitions.Reserve(EnemyDefinitions.Num() + 1);
			Definitions.Add(MakeCodexEntry(
				CodexGuideName,
				EGameXXKCodexCategory::Hero,
				TEXT("引路人"),
				TEXT("在青山镇相遇的同行者。")));
			for (const FGameXXKEnemyDefinition& Enemy : EnemyDefinitions)
			{
				const TCHAR* TierLabel = Enemy.Tier == EGameXXKEnemyTier::Boss
					? TEXT("首领")
					: (Enemy.Tier == EGameXXKEnemyTier::Elite ? TEXT("精英敌人") : TEXT("敌人"));
				FGameXXKCodexEntryDef Definition;
				Definition.Id = Enemy.CodexId;
				Definition.Category = EGameXXKCodexCategory::Monster;
				Definition.DisplayName = Enemy.DisplayName;
				Definition.Description = FText::FromString(FString::Printf(
					TEXT("第%d章遭遇的%s。"),
					Enemy.Chapter,
					TierLabel));
				Definition.IconPath = Enemy.PortraitSoftPath;
				Definitions.Add(MoveTemp(Definition));
			}
			return Definitions;
		}();
		return EntryDefs;
	}

	static const FGameXXKCodexEntryDef* FindCodexEntryDef(FName EntryId)
	{
		return GetCodexEntryDefsInternal().FindByPredicate([EntryId](const FGameXXKCodexEntryDef& EntryDef)
		{
			return EntryDef.Id == EntryId;
		});
	}

	static bool MatchesCodexCategory(const FGameXXKCodexEntryDef& EntryDef, EGameXXKCodexCategory Category)
	{
		return Category == EGameXXKCodexCategory::All || EntryDef.Category == Category;
	}

	static FName GetCodexEntryIdForBattleRuntimeId(FName RuntimeEnemyId)
	{
		if (RuntimeEnemyId == BattleRuntimeMoneyRatName
			|| RuntimeEnemyId == BattleRuntimeBanditName
			|| RuntimeEnemyId == BattleRuntimeWolfName)
		{
			return CodexMoneyRatName;
		}
		if (RuntimeEnemyId == BattleRuntimeBlackBearName
			|| RuntimeEnemyId == BattleRuntimeEliteBanditName)
		{
			return CodexBlackBearName;
		}
		if (RuntimeEnemyId == BattleRuntimeTigerName
			|| RuntimeEnemyId == BattleRuntimeBossName)
		{
			return CodexTigerName;
		}
		return NAME_None;
	}

	static FName GetMigratedCurrentCodexEntryId(FName EntryId)
	{
		if (EntryId == PreviousCodexMoneyRatName
			|| EntryId == LegacyCodexBanditName
			|| EntryId == LegacyCodexWolfName)
		{
			return CodexMoneyRatName;
		}
		if (EntryId == PreviousCodexBlackBearName || EntryId == LegacyCodexEliteBanditName)
		{
			return CodexBlackBearName;
		}
		if (EntryId == PreviousCodexTigerName || EntryId == LegacyCodexBossName)
		{
			return CodexTigerName;
		}
		return NAME_None;
	}

	static void MigrateLegacyCodexEntryIds(TSet<FName>& EntryIds)
	{
		TArray<FName> LegacyEntryIds;
		TArray<FName> CurrentEntryIds;
		for (const FName EntryId : EntryIds)
		{
			const FName CurrentEntryId = GetMigratedCurrentCodexEntryId(EntryId);
			if (!CurrentEntryId.IsNone())
			{
				LegacyEntryIds.Add(EntryId);
				CurrentEntryIds.Add(CurrentEntryId);
			}
		}

		for (const FName LegacyEntryId : LegacyEntryIds)
		{
			EntryIds.Remove(LegacyEntryId);
		}
		for (const FName CurrentEntryId : CurrentEntryIds)
		{
			EntryIds.Add(CurrentEntryId);
		}
	}

	static TArray<FName> GetKnownItemIds()
	{
		return {
			ItemHealingPowderName,
			ItemEnhancementStoneName,
			ItemQingshanRouteSealName,
			ItemTrainingNormalChestName,
			ItemTrainingAdvancedChestName,
			ItemLingzhiPowderName,
			ItemQingxinTeaName,
			ItemCraneSachetName,
			ItemIronSwordName,
			ItemClothArmorName,
			ItemCranePatternTalismanName,
			ItemInkstonePendantName,
			ItemWoodenSwordName,
			ItemStarterClothArmorName,
			ItemClothTalismanName,
		};
	}

	static TArray<FName> GetShopItemIds()
	{
		return {
			ItemHealingPowderName,
			ItemEnhancementStoneName,
			ItemLingzhiPowderName,
			ItemQingxinTeaName,
			ItemCraneSachetName,
			ItemIronSwordName,
			ItemClothArmorName,
			ItemCranePatternTalismanName,
			ItemInkstonePendantName,
		};
	}

	static bool GetItemDef(FName ItemId, FGameXXKItemDef& OutDef)
	{
		if (ItemId == ItemHealingPowderName)
		{
			OutDef = MakeItem(ItemId, TEXT("金疮药"), EGameXXKItemKind::Consumable, 10, 5, 30, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemEnhancementStoneName)
		{
			OutDef = MakeItem(ItemId, TEXT("强化石"), EGameXXKItemKind::Material, 20, 10, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemRefinementSandName)
		{
			OutDef = MakeItem(ItemId, TEXT("洗炼砂"), EGameXXKItemKind::Material, 20, 10, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemQingshanRouteSealName)
		{
			OutDef = MakeItem(ItemId, TEXT("青山讨伐令"), EGameXXKItemKind::Task, 0, 0, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemTrainingNormalChestName)
		{
			OutDef = MakeItem(ItemId, TEXT("普通历练宝箱"), EGameXXKItemKind::Material, 0, 0, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemTrainingAdvancedChestName)
		{
			OutDef = MakeItem(ItemId, TEXT("高级历练宝箱"), EGameXXKItemKind::Material, 0, 0, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemLingzhiPowderName)
		{
			OutDef = MakeItem(ItemId, TEXT("灵芝散"), EGameXXKItemKind::Consumable, 28, 14, 80, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemQingxinTeaName)
		{
			OutDef = MakeItem(ItemId, TEXT("清心茶"), EGameXXKItemKind::Consumable, 16, 8, 0, 20, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemCraneSachetName)
		{
			OutDef = MakeItem(ItemId, TEXT("鹤羽香囊"), EGameXXKItemKind::Consumable, 22, 11, 0, 0, 0, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemIronSwordName)
		{
			OutDef = MakeItem(ItemId, TEXT("青锋短剑"), EGameXXKItemKind::Weapon, 48, 24, 0, 0, 8, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemClothArmorName)
		{
			OutDef = MakeItem(ItemId, TEXT("竹编轻甲"), EGameXXKItemKind::Armor, 36, 18, 0, 0, 0, 6, 0, 0);
			return true;
		}
		if (ItemId == ItemCranePatternTalismanName)
		{
			OutDef = MakeItem(ItemId, TEXT("鹤纹护符"), EGameXXKItemKind::Accessory, 32, 16, 0, 0, 0, 0, 30, 0);
			return true;
		}
		if (ItemId == ItemInkstonePendantName)
		{
			OutDef = MakeItem(ItemId, TEXT("墨砚坠饰"), EGameXXKItemKind::Accessory, 32, 16, 0, 0, 0, 0, 0, 20);
			return true;
		}
		if (ItemId == ItemWoodenSwordName)
		{
			OutDef = MakeItem(ItemId, TEXT("木剑"), EGameXXKItemKind::Weapon, 18, 9, 0, 0, 3, 0, 0, 0);
			return true;
		}
		if (ItemId == ItemStarterClothArmorName)
		{
			OutDef = MakeItem(ItemId, TEXT("布甲"), EGameXXKItemKind::Armor, 16, 8, 0, 0, 0, 3, 0, 0);
			return true;
		}
		if (ItemId == ItemClothTalismanName)
		{
			OutDef = MakeItem(ItemId, TEXT("布护符"), EGameXXKItemKind::Accessory, 14, 7, 0, 0, 0, 0, 10, 0);
			return true;
		}
		return false;
	}

	static void SynchronizeEnhancementMaterial(FGameXXKRuntimeState& State)
	{
		State.EnhancementMaterial = FMath::Max(0, State.Inventory.FindRef(ItemEnhancementStoneName));
	}

	static void MigrateInventoryCategoryItems(FGameXXKRuntimeState& State)
	{
		if (!State.Inventory.Contains(ItemEnhancementStoneName) && State.EnhancementMaterial > 0)
		{
			State.Inventory.Add(ItemEnhancementStoneName, State.EnhancementMaterial);
		}
		SynchronizeEnhancementMaterial(State);

		if (State.QuestState == EGameXXKQuestState::Accepted)
		{
			State.Inventory.FindOrAdd(ItemQingshanRouteSealName) = FMath::Max(1, State.Inventory.FindRef(ItemQingshanRouteSealName));
		}
		else
		{
			State.Inventory.Remove(ItemQingshanRouteSealName);
		}
	}

	static void MigrateCodexState(FGameXXKRuntimeState& State, int32 SaveVersion)
	{
		if (SaveVersion < GuideCodexIntroductionSaveVersion
			&& (State.QuestState == EGameXXKQuestState::Accepted || State.QuestState == EGameXXKQuestState::Completed))
		{
			UGameXXKMVPRules::DiscoverCodexEntry(State, CodexGuideName);
		}

		if (SaveVersion < CurrentEnemyCodexMigrationSaveVersion)
		{
			MigrateLegacyCodexEntryIds(State.DiscoveredCodexEntryIds);
			MigrateLegacyCodexEntryIds(State.ReadCodexEntryIds);
		}
	}

	static int32 GetClampedItemEnhancementLevel(const FGameXXKRuntimeState& State, FName ItemId)
	{
		return ItemId.IsNone() ? 0 : FMath::Clamp(State.ItemEnhancementLevels.FindRef(ItemId), 0, MaxItemEnhancementLevel);
	}

	static void AddEquipmentBonuses(const FGameXXKRuntimeState& State, FName ItemId, int32& Attack, int32& Defense, int32& MaxHP, int32& MaxMP, int32& Speed)
	{
		if (ItemId.IsNone() || State.Inventory.FindRef(ItemId) <= 0)
		{
			return;
		}
		FGameXXKItemDef Def;
		if (!GetItemDef(ItemId, Def))
		{
			return;
		}
		Attack += Def.AttackBonus;
		Defense += Def.DefenseBonus;
		MaxHP += Def.MaxHPBonus;
		MaxMP += Def.MaxMPBonus;

		const int32 EnhancementLevel = GetClampedItemEnhancementLevel(State, ItemId);
		if (Def.Kind == EGameXXKItemKind::Weapon)
		{
			Attack += EnhancementLevel;
		}
		else if (Def.Kind == EGameXXKItemKind::Armor)
		{
			Defense += EnhancementLevel;
		}
		else if (Def.Kind == EGameXXKItemKind::Accessory)
		{
			Speed += EnhancementLevel;
		}
	}

	static void RecalculatePlayerStats(FGameXXKRuntimeState& State, bool bPreserveMissingResources = true)
	{
		const int32 RouteMaxHP = FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxHealth);
		const int32 RouteMaxMP = FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxMana);
		const int32 OldMaxHP = FMath::Max(1, State.PlayerMaxHP + RouteMaxHP);
		const int32 OldMaxMP = FMath::Max(1, State.PlayerMaxMP + RouteMaxMP);
		const int32 MissingHP = bPreserveMissingResources ? FMath::Max(0, OldMaxHP - State.PlayerHP) : 0;
		const int32 MissingMP = bPreserveMissingResources ? FMath::Max(0, OldMaxMP - State.PlayerMP) : 0;

		State.PlayerLevel = FMath::Clamp(State.PlayerLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
		const FGameXXKCharacterStats BareStats = FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel);
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		if (FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			State.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			BareStats,
			Snapshot))
		{
			State.PlayerMaxHP = Snapshot.AttributesBeforeRoute.MaxHealth;
			State.PlayerMaxMP = Snapshot.AttributesBeforeRoute.MaxMana;
			State.PlayerAttack = Snapshot.AttributesBeforeRoute.Attack;
			State.PlayerDefense = Snapshot.AttributesBeforeRoute.Defense;
			State.PlayerSpeed = Snapshot.AttributesBeforeRoute.Speed;
		}
		else
		{
			State.PlayerMaxHP = BareStats.MaxHealth;
			State.PlayerMaxMP = BareStats.MaxMana;
			State.PlayerAttack = BareStats.Attack;
			State.PlayerDefense = BareStats.Defense;
			State.PlayerSpeed = BareStats.Speed;
		}

		const int32 NewEffectiveMaxHP = FMath::Max(1, State.PlayerMaxHP + RouteMaxHP);
		const int32 NewEffectiveMaxMP = FMath::Max(1, State.PlayerMaxMP + RouteMaxMP);
		State.PlayerHP = FMath::Clamp(NewEffectiveMaxHP - MissingHP, 0, NewEffectiveMaxHP);
		State.PlayerMP = FMath::Clamp(NewEffectiveMaxMP - MissingMP, 0, NewEffectiveMaxMP);
	}

	static void NormalizeLoadedCharacterProgression(FGameXXKRuntimeState& State)
	{
		const int32 OriginalPlayerLevel = State.PlayerLevel;
		State.PlayerLevel = FMath::Clamp(State.PlayerLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
		State.PlayerXP = State.PlayerLevel == FGameXXKCharacterStatRules::MaxCharacterLevel
			? 0
			: FMath::Max(0, State.PlayerXP);
		for (FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			Companion.Level = FMath::Clamp(Companion.Level, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
			Companion.Experience = Companion.Level == FGameXXKCharacterStatRules::MaxCharacterLevel
				? 0
				: FMath::Max(0, Companion.Experience);
		}
		if (State.PlayerLevel != OriginalPlayerLevel)
		{
			RecalculatePlayerStats(State, true);
		}
	}

	static void ApplyXP(FGameXXKRuntimeState& State, int32 XP)
	{
		State.PlayerLevel = FMath::Clamp(State.PlayerLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
		if (State.PlayerLevel == FGameXXKCharacterStatRules::MaxCharacterLevel)
		{
			State.PlayerXP = 0;
			return;
		}

		const int64 AwardedXP = static_cast<int64>(FMath::Max(0, XP));
		State.PlayerXP = static_cast<int32>(FMath::Min<int64>(MAX_int32, static_cast<int64>(FMath::Max(0, State.PlayerXP)) + AwardedXP));
		bool bLeveled = false;
		while (State.PlayerLevel < FGameXXKCharacterStatRules::MaxCharacterLevel
			&& State.PlayerXP >= State.PlayerLevel * 100)
		{
			State.PlayerXP -= State.PlayerLevel * 100;
			State.PlayerLevel += 1;
			bLeveled = true;
		}
		if (State.PlayerLevel == FGameXXKCharacterStatRules::MaxCharacterLevel)
		{
			State.PlayerXP = 0;
		}
		if (bLeveled)
		{
			RecalculatePlayerStats(State, false);
			State.PlayerHP = State.PlayerMaxHP;
			State.PlayerMP = State.PlayerMaxMP;
		}
	}

	/** Only the persistent partner actually selected for this route receives combat progression.
	 *  Task NPCs are deliberately excluded: they are temporary event support and must not acquire
	 *  permanent levels, stars, or roster state. */
	static void AwardActivePermanentCompanionBattleExperience(FGameXXKRuntimeState& State, const int32 ExperienceAmount)
	{
		if (ExperienceAmount <= 0)
		{
			return;
		}

		const FName ActiveInstanceId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		if (ActiveInstanceId.IsNone())
		{
			return;
		}

		FGameXXKPermanentCompanion* ActiveCompanion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[ActiveInstanceId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == ActiveInstanceId && Candidate.bIsActive;
			});
		if (!ActiveCompanion)
		{
			return;
		}

		FString IgnoredError;
		FGameXXKCompanionRules::AwardExperience(*ActiveCompanion, ExperienceAmount, &IgnoredError);
	}

	static const FGameXXKRouteMapNode* FindRouteNode(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	static void AddUniqueInt(TArray<int32>& Values, int32 Value)
	{
		if (!Values.Contains(Value))
		{
			Values.Add(Value);
		}
	}

	static void RemoveInt(TArray<int32>& Values, int32 Value)
	{
		Values.Remove(Value);
	}

	static int32 NormalizeRouteSeed(int32 Seed)
	{
		if (Seed == 0 || Seed == MIN_int32)
		{
			return 1;
		}
		return FMath::Abs(Seed);
	}

	static int32 MakeNewRouteSeed()
	{
		const int32 Seed = FMath::Rand();
		return NormalizeRouteSeed(Seed);
	}

	static void AddRouteNode(FGameXXKRuntimeState& State, int32 NodeId, int32 LayerIndex, int32 ColumnIndex, EGameXXKNodeKind NodeKind, float X, float Y, TArray<int32> OutgoingNodeIds)
	{
		State.RouteMapNodes.Emplace(NodeId, LayerIndex, ColumnIndex, NodeKind, FVector2D(X, Y), OutgoingNodeIds);
		for (int32 ToNodeId : OutgoingNodeIds)
		{
			State.RouteMapEdges.Emplace(NodeId, ToNodeId);
		}
	}

	static FGameXXKRouteMapNode* FindMutableRouteNode(FGameXXKRuntimeState& State, int32 NodeId)
	{
		return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == NodeId;
		});
	}

	static int32 GetGeneratedLayerNodeCount(FRandomStream& Stream, int32 LayerIndex)
	{
		switch (LayerIndex)
		{
		case 1:
			return 2 + Stream.RandRange(0, 1);
		case 2:
		case 3:
		case 4:
			return 3 + Stream.RandRange(0, 1);
		case 5:
			return 2 + Stream.RandRange(0, 1);
		default:
			return 1;
		}
	}

	static EGameXXKNodeKind PickGeneratedRouteNodeKind(FRandomStream& Stream, int32 LayerIndex, int32 ColumnIndex)
	{
		const int32 Roll = Stream.RandRange(0, 99);
		if (LayerIndex == 1)
		{
			return EGameXXKNodeKind::Battle;
		}
		if (LayerIndex == 5)
		{
			if (ColumnIndex == 0)
			{
				return EGameXXKNodeKind::Camp;
			}
			return Roll < 45 ? EGameXXKNodeKind::Battle : (Roll < 72 ? EGameXXKNodeKind::Merchant : EGameXXKNodeKind::Elite);
		}
		if (Roll < 44)
		{
			return EGameXXKNodeKind::Battle;
		}
		if (Roll < 58)
		{
			return EGameXXKNodeKind::Elite;
		}
		if (Roll < 72)
		{
			return EGameXXKNodeKind::Event;
		}
		if (Roll < 84)
		{
			return EGameXXKNodeKind::Chest;
		}
		if (Roll < 93)
		{
			return EGameXXKNodeKind::Merchant;
		}
		return EGameXXKNodeKind::Camp;
	}

	static TArray<int32> GetRouteNodeIdsInLayer(const FGameXXKRuntimeState& State, int32 LayerIndex)
	{
		TArray<int32> NodeIds;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			if (Node.LayerIndex == LayerIndex)
			{
				NodeIds.Add(Node.NodeId);
			}
		}
		NodeIds.Sort([&State](int32 LeftNodeId, int32 RightNodeId)
		{
			const FGameXXKRouteMapNode* LeftNode = FindRouteNode(State, LeftNodeId);
			const FGameXXKRouteMapNode* RightNode = FindRouteNode(State, RightNodeId);
			const int32 LeftColumn = LeftNode ? LeftNode->ColumnIndex : 0;
			const int32 RightColumn = RightNode ? RightNode->ColumnIndex : 0;
			return LeftColumn < RightColumn;
		});
		return NodeIds;
	}

	static int32 CountIncomingRouteEdges(const FGameXXKRuntimeState& State, int32 NodeId)
	{
		int32 Count = 0;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			if (Edge.ToNodeId == NodeId)
			{
				++Count;
			}
		}
		return Count;
	}

	static bool HasRouteEdge(const FGameXXKRuntimeState& State, int32 FromNodeId, int32 ToNodeId)
	{
		return State.RouteMapEdges.ContainsByPredicate([FromNodeId, ToNodeId](const FGameXXKRouteMapEdge& Edge)
		{
			return Edge.FromNodeId == FromNodeId && Edge.ToNodeId == ToNodeId;
		});
	}

	static void AddUniqueRouteEdge(FGameXXKRuntimeState& State, int32 FromNodeId, int32 ToNodeId)
	{
		FGameXXKRouteMapNode* FromNode = FindMutableRouteNode(State, FromNodeId);
		if (!FromNode || !FindRouteNode(State, ToNodeId) || HasRouteEdge(State, FromNodeId, ToNodeId))
		{
			return;
		}
		FromNode->OutgoingNodeIds.Add(ToNodeId);
		State.RouteMapEdges.Emplace(FromNodeId, ToNodeId);
	}

	static int32 PickClosestTargetNode(
		const FGameXXKRuntimeState& State,
		int32 FromNodeId,
		const TArray<int32>& CandidateNodeIds,
		int32 MaxIncomingEdges,
		bool bRespectIncomingLimit)
	{
		const FGameXXKRouteMapNode* FromNode = FindRouteNode(State, FromNodeId);
		if (!FromNode)
		{
			return INDEX_NONE;
		}

		int32 BestNodeId = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 CandidateNodeId : CandidateNodeIds)
		{
			if (HasRouteEdge(State, FromNodeId, CandidateNodeId))
			{
				continue;
			}
			if (bRespectIncomingLimit && CountIncomingRouteEdges(State, CandidateNodeId) >= MaxIncomingEdges)
			{
				continue;
			}
			const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(State, CandidateNodeId);
			if (!CandidateNode)
			{
				continue;
			}
			const float Score = FMath::Abs(FromNode->NormalizedPosition.X - CandidateNode->NormalizedPosition.X);
			if (Score < BestScore)
			{
				BestScore = Score;
				BestNodeId = CandidateNodeId;
			}
		}
		return BestNodeId;
	}

	static int32 PickClosestSourceNode(
		const FGameXXKRuntimeState& State,
		int32 ToNodeId,
		const TArray<int32>& CandidateNodeIds,
		int32 MaxOutgoingEdges)
	{
		const FGameXXKRouteMapNode* ToNode = FindRouteNode(State, ToNodeId);
		if (!ToNode)
		{
			return INDEX_NONE;
		}

		int32 BestNodeId = INDEX_NONE;
		float BestScore = TNumericLimits<float>::Max();
		for (int32 CandidateNodeId : CandidateNodeIds)
		{
			const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(State, CandidateNodeId);
			if (!CandidateNode || CandidateNode->OutgoingNodeIds.Num() >= MaxOutgoingEdges || HasRouteEdge(State, CandidateNodeId, ToNodeId))
			{
				continue;
			}
			const float Score = FMath::Abs(CandidateNode->NormalizedPosition.X - ToNode->NormalizedPosition.X);
			if (Score < BestScore)
			{
				BestScore = Score;
				BestNodeId = CandidateNodeId;
			}
		}
		return BestNodeId;
	}

	static void ConnectGeneratedRouteLayer(FGameXXKRuntimeState& State, FRandomStream& Stream, int32 LayerIndex)
	{
		static constexpr int32 MaxOutgoingEdges = 3;
		static constexpr int32 MaxIncomingEdges = 2;

		const TArray<int32> SourceNodeIds = GetRouteNodeIdsInLayer(State, LayerIndex);
		const TArray<int32> TargetNodeIds = GetRouteNodeIdsInLayer(State, LayerIndex + 1);
		if (SourceNodeIds.IsEmpty() || TargetNodeIds.IsEmpty())
		{
			return;
		}

		const bool bTargetIsBossLayer = TargetNodeIds.Num() == 1;
		for (int32 SourceNodeId : SourceNodeIds)
		{
			int32 TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, !bTargetIsBossLayer);
			if (TargetNodeId == INDEX_NONE)
			{
				TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, false);
			}
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}

		for (int32 TargetNodeId : TargetNodeIds)
		{
			if (CountIncomingRouteEdges(State, TargetNodeId) > 0)
			{
				continue;
			}
			int32 SourceNodeId = PickClosestSourceNode(State, TargetNodeId, SourceNodeIds, MaxOutgoingEdges);
			if (SourceNodeId == INDEX_NONE)
			{
				SourceNodeId = SourceNodeIds[Stream.RandRange(0, SourceNodeIds.Num() - 1)];
			}
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}

		for (int32 SourceNodeId : SourceNodeIds)
		{
			FGameXXKRouteMapNode* SourceNode = FindMutableRouteNode(State, SourceNodeId);
			if (!SourceNode || SourceNode->OutgoingNodeIds.Num() >= MaxOutgoingEdges || Stream.RandRange(0, 99) >= 45)
			{
				continue;
			}
			const bool bRespectIncomingLimit = !bTargetIsBossLayer;
			const int32 TargetNodeId = PickClosestTargetNode(State, SourceNodeId, TargetNodeIds, MaxIncomingEdges, bRespectIncomingLimit);
			AddUniqueRouteEdge(State, SourceNodeId, TargetNodeId);
		}
	}

	static void GenerateRouteMap(FGameXXKRuntimeState& State)
	{
		const int32 Seed = State.RouteSeed != 0 ? State.RouteSeed : MakeNewRouteSeed();
		UGameXXKMVPRules::GenerateRouteMapForSeed(State, Seed);
	}

	static void ClearBattleEntryCheckpoint(FGameXXKRuntimeState& State)
	{
		State.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
	}

	static FGameXXKBattleEntryCheckpoint CaptureBattleEntryCheckpoint(
		const FGameXXKRuntimeState& State,
		const int32 SourceNodeId)
	{
		FGameXXKBattleEntryCheckpoint Checkpoint;
		Checkpoint.bValid = true;
		Checkpoint.SourceNodeId = SourceNodeId;
		Checkpoint.PreviousCurrentRouteNodeId = State.CurrentRouteNodeId;
		Checkpoint.PreviousDungeonNodeIndex = State.DungeonNodeIndex;
		Checkpoint.PreviousPlayerHP = State.PlayerHP;
		Checkpoint.PreviousPlayerMP = State.PlayerMP;
		Checkpoint.PreviousVisitedRouteNodeIds = State.VisitedRouteNodeIds;
		Checkpoint.PreviousReachableRouteNodeIds = State.ReachableRouteNodeIds;
		return Checkpoint;
	}

	static bool CompleteRouteNode(FGameXXKRuntimeState& State, const FGameXXKRouteMapNode& Node)
	{
		AddUniqueInt(State.VisitedRouteNodeIds, Node.NodeId);
		State.ReachableRouteNodeIds.Reset();
		for (int32 OutgoingNodeId : Node.OutgoingNodeIds)
		{
			if (!State.VisitedRouteNodeIds.Contains(OutgoingNodeId))
			{
				AddUniqueInt(State.ReachableRouteNodeIds, OutgoingNodeId);
			}
		}
		State.PendingRouteNodeId = INDEX_NONE;
		State.CurrentRouteNodeId = State.ReachableRouteNodeIds.IsEmpty() ? INDEX_NONE : State.ReachableRouteNodeIds[0];
		State.DungeonNodeIndex = State.VisitedRouteNodeIds.Num();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.TownPanelMode = EGameXXKTownPanelMode::None;
		ClearBattleEntryCheckpoint(State);
		return true;
	}

	static int32 GetBaseRouteNodeTravelMoney(const EGameXXKNodeKind NodeKind)
	{
		switch (NodeKind)
		{
		case EGameXXKNodeKind::Battle: return 20;
		case EGameXXKNodeKind::Elite: return 35;
		case EGameXXKNodeKind::Boss: return 50;
		default: return 0;
		}
	}

	static bool HasRouteNodeReceipt(
		const FGameXXKRuntimeState& State,
		const int32 Chapter,
		const int32 NodeId)
	{
		return State.CardRun.RewardedTravelMoneyNodes.ContainsByPredicate(
			[Chapter, NodeId](const FGameXXKRouteTravelMoneyReceipt& Receipt)
			{
				return Receipt.Chapter == Chapter && Receipt.NodeId == NodeId;
			});
	}

	static bool ApplyRouteNodeReceiptGate(
		FGameXXKRuntimeState& StateWithOneTimeRewards,
		const FGameXXKRuntimeState& StateBeforeOneTimeRewards,
		const int32 Chapter,
		const int32 NodeId,
		const int32 BaseTravelMoney)
	{
		FGameXXKRuntimeState Candidate = StateWithOneTimeRewards;
		int32 RelicTravelMoney = 0;
		FString EconomyError;
		if (!FGameXXKRelicRules::CalculateRouteNodeTravelMoneyBonus(Candidate, RelicTravelMoney, &EconomyError))
		{
			return false;
		}

		const int64 TotalTravelMoney = static_cast<int64>(BaseTravelMoney) + RelicTravelMoney;
		if (BaseTravelMoney < 0 || TotalTravelMoney < 0 || TotalTravelMoney > MAX_int32)
		{
			return false;
		}

		bool bAwarded = false;
		if (!FGameXXKRouteEconomyRules::AwardNodeOnce(
			Candidate.CardRun,
			Chapter,
			NodeId,
			static_cast<int32>(TotalTravelMoney),
			bAwarded,
			&EconomyError))
		{
			return false;
		}

		if (bAwarded)
		{
			FGameXXKRelicRules::ApplyRouteNodeCompletedNonCurrency(Candidate);
		}
		else
		{
			// The receipt is authoritative. Drop every staged reward side effect,
			// while allowing the caller to finish structural node advancement.
			Candidate = StateBeforeOneTimeRewards;
		}

		StateWithOneTimeRewards = MoveTemp(Candidate);
		return true;
	}

	static bool SettleGeneratedRouteNode(
		FGameXXKRuntimeState& StateWithOneTimeRewards,
		const FGameXXKRuntimeState& StateBeforeOneTimeRewards,
		const int32 NodeId,
		const int32 BaseTravelMoney)
	{
		FGameXXKRuntimeState Candidate = StateWithOneTimeRewards;
		const int32 Chapter = StateBeforeOneTimeRewards.CardRun.RouteProgress.CurrentChapter;
		if (!ApplyRouteNodeReceiptGate(
			Candidate,
			StateBeforeOneTimeRewards,
			Chapter,
			NodeId,
			BaseTravelMoney))
		{
			return false;
		}

		const FGameXXKRouteMapNode* CandidateNode = FindRouteNode(Candidate, NodeId);
		if (!CandidateNode || !CompleteRouteNode(Candidate, *CandidateNode))
		{
			return false;
		}
		StateWithOneTimeRewards = MoveTemp(Candidate);
		return true;
	}

	static bool SettleFixedRouteNode(
		FGameXXKRuntimeState& StateWithOneTimeRewards,
		const FGameXXKRuntimeState& StateBeforeOneTimeRewards,
		const int32 BaseTravelMoney)
	{
		const int32 NodeId = StateBeforeOneTimeRewards.DungeonNodeIndex;
		if (NodeId < 0 || NodeId == MAX_int32)
		{
			return false;
		}

		FGameXXKRuntimeState Candidate = StateWithOneTimeRewards;
		if (!ApplyRouteNodeReceiptGate(Candidate, StateBeforeOneTimeRewards, 1, NodeId, BaseTravelMoney))
		{
			return false;
		}
		Candidate.DungeonNodeIndex = NodeId + 1;
		Candidate.Screen = EGameXXKScreen::DungeonMap;
		Candidate.CurrentMapId = TEXT("HuangshanRoute");
		Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
		ClearBattleEntryCheckpoint(Candidate);
		StateWithOneTimeRewards = MoveTemp(Candidate);
		return true;
	}

	static const FGameXXKRouteMapNode* FindFirstReachableRouteNodeOfKind(const FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind)
	{
		for (int32 NodeId : State.ReachableRouteNodeIds)
		{
			const FGameXXKRouteMapNode* Node = FindRouteNode(State, NodeId);
			if (Node && Node->NodeKind == NodeKind)
			{
				return Node;
			}
		}
		return nullptr;
	}

	static const FGameXXKRouteMapNode* FindPendingRouteNode(const FGameXXKRuntimeState& State)
	{
		return FindRouteNode(State, State.PendingRouteNodeId);
	}

	static bool TryBuildRouteRewardChoiceSeed(
		const int32 RouteRandomSeed,
		const int32 SourceNodeId,
		const int32 NextRewardOrdinal,
		int32& OutChoiceSeed)
	{
		OutChoiceSeed = 0;
		if (NextRewardOrdinal < 0 || NextRewardOrdinal == MAX_int32 || SourceNodeId < 0)
		{
			return false;
		}

		// The legacy seed contract combines the low 32 bits of both products. Build those
		// products in int64, then mix as uint32 so wraparound is explicit and deterministic.
		const uint32 MixedBits = static_cast<uint32>(RouteRandomSeed)
			^ static_cast<uint32>(static_cast<int64>(SourceNodeId) * 1103515245LL)
			^ static_cast<uint32>(static_cast<int64>(NextRewardOrdinal) * 12345LL);
		if (MixedBits == 0)
		{
			OutChoiceSeed = 0x3C6EF35F;
			return true;
		}

		// Convert the mixed bit pattern to int32 without relying on an out-of-range unsigned
		// conversion. This preserves the established two's-complement seed sequence.
		constexpr int64 Uint32Range = 0x100000000LL;
		const int64 SignedSeed = MixedBits <= static_cast<uint32>(MAX_int32)
			? static_cast<int64>(MixedBits)
			: static_cast<int64>(MixedBits) - Uint32Range;
		OutChoiceSeed = static_cast<int32>(SignedSeed);
		return true;
	}

	static bool IsDungeonNode(const FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
	{
		if (State.bHasGeneratedRouteMap)
		{
			const FGameXXKRouteMapNode* PendingNode = FindPendingRouteNode(State);
			if (PendingNode)
			{
				return State.bDungeonActive && PendingNode->NodeKind == ExpectedNode;
			}
			return State.bDungeonActive && FindFirstReachableRouteNodeOfKind(State, ExpectedNode) != nullptr;
		}
		const TArray<EGameXXKNodeKind> Nodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		return State.bDungeonActive && Nodes.IsValidIndex(State.DungeonNodeIndex) && Nodes[State.DungeonNodeIndex] == ExpectedNode;
	}

	static FGameXXKBattleUnit MakeBattleUnit(FName Id, int32 HP, int32 Attack, int32 Defense, int32 Speed, FName Weakness, int32 Shield)
	{
		FGameXXKBattleUnit Unit;
		Unit.Id = Id;
		Unit.HP = HP;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.Weakness = Weakness;
		Unit.Shield = Shield;
		return Unit;
	}

	static FGameXXKBattleRuntimeUnit MakeBattleRuntimeUnit(FName Id, const TCHAR* DisplayName, int32 HP, int32 Attack, int32 Defense, int32 Speed, int32 Shield, bool bEnemy, int32 MP = 0, int32 MaxMP = 0)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = Id;
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.MP = FMath::Clamp(MP, 0, FMath::Max(0, MaxMP));
		Unit.MaxMP = FMath::Max(0, MaxMP);
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Speed = Speed;
		Unit.Shield = Shield;
		Unit.bEnemy = bEnemy;
		Unit.bDefeated = HP <= 0;
		return Unit;
	}

	static void ClearActiveBattle(FGameXXKRuntimeState& State)
	{
		State.bHasActiveBattle = false;
		State.ActiveBattleNodeId = INDEX_NONE;
		State.ActiveBattleEnemies.Reset();
		State.ActiveBattleParty.Reset();
		FGameXXKCardBattleAdapter::ClearActiveCardBattle(State);
	}

	static bool SettleTerminalRoute(FGameXXKRuntimeState& State, const EGameXXKRouteTerminalOutcome Outcome)
	{
		FGameXXKRuntimeState Candidate = State;
		FGameXXKRouteSettlementReceipt Receipt;
		FString SettlementError;
		if (!FGameXXKRouteSettlementRules::Preview(Candidate, Outcome, Receipt, &SettlementError))
		{
			return false;
		}
		Candidate.CardRun.PendingSettlement = Receipt;
		if (!FGameXXKRouteSettlementRules::Apply(Candidate, Receipt, &SettlementError))
		{
			return false;
		}
		ClearBattleEntryCheckpoint(Candidate);
		State = MoveTemp(Candidate);
		return true;
	}

	static bool ReturnTerminalRouteToTown(FGameXXKRuntimeState& State, const EGameXXKRouteTerminalOutcome Outcome)
	{
		if (!State.bDungeonActive)
		{
			return false;
		}
		FGameXXKRuntimeState Candidate = State;
		if (!SettleTerminalRoute(Candidate, Outcome))
		{
			return false;
		}
		Candidate.Screen = EGameXXKScreen::Town;
		Candidate.CurrentRegion = UGameXXKMVPRules::RegionQingshan();
		Candidate.CurrentMapId = UGameXXKMVPRules::RegionQingshan();
		Candidate.bDungeonActive = false;
		Candidate.DungeonNodeIndex = 0;
		Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
		Candidate.PlayerHP = Candidate.PlayerMaxHP;
		Candidate.PlayerMP = Candidate.PlayerMaxMP;
		ClearActiveBattle(Candidate);
		State = MoveTemp(Candidate);
		return true;
	}

	static void InitializeThreeChapterRouteProgress(FGameXXKRuntimeState& State)
	{
		FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		Progress.SchemaVersion = 1;
		Progress.RootSeed = State.RouteSeed;
		Progress.ChapterSeeds = {
			Progress.RootSeed,
			NormalizeRouteSeed(FGameXXKEncounterRules::DeriveChapterSeed(Progress.RootSeed, 2)),
			NormalizeRouteSeed(FGameXXKEncounterRules::DeriveChapterSeed(Progress.RootSeed, 3))};
	Progress.CurrentChapter = 1;
	Progress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, 20);
	Progress.ActualRouteCardAcquisitionCount = 0;
	State.CardRun.PendingSettlement = FGameXXKRouteSettlementReceipt();
}

	static bool IsValidThreeChapterRouteProgress(const FGameXXKRouteProgress& Progress)
	{
		if (Progress.SchemaVersion != 1
			|| Progress.RootSeed == 0
			|| Progress.ChapterSeeds.Num() != 3
			|| Progress.ChapterSeeds[0] != Progress.RootSeed
			|| Progress.ChapterSeeds[1] != NormalizeRouteSeed(FGameXXKEncounterRules::DeriveChapterSeed(Progress.RootSeed, 2))
			|| Progress.ChapterSeeds[2] != NormalizeRouteSeed(FGameXXKEncounterRules::DeriveChapterSeed(Progress.RootSeed, 3))
			|| Progress.CurrentChapter < 1
			|| Progress.CurrentChapter > 3
			|| Progress.RouteCombatLevel < 1
			|| Progress.RouteCombatLevel > 20)
		{
			return false;
		}
		return true;
	}

	static bool AdvanceToNextRouteChapter(FGameXXKRuntimeState& State)
	{
		const FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		if (!IsValidThreeChapterRouteProgress(Progress) || Progress.CurrentChapter >= 3)
		{
			return false;
		}

		FGameXXKRuntimeState Candidate = State;
		FGameXXKRouteProgress& CandidateProgress = Candidate.CardRun.RouteProgress;
		const int32 NextChapter = CandidateProgress.CurrentChapter + 1;
		const int32 NextChapterSeed = CandidateProgress.ChapterSeeds[NextChapter - 1];
		if (NextChapterSeed == 0)
		{
			return false;
		}

		// A cleared chapter restores the hero, but intentionally retains every route-local
		// reward, relic, deck addition, and temporary task-NPC choice for the next map.
		Candidate.PlayerHP = Candidate.PlayerMaxHP;
		Candidate.PlayerMP = Candidate.PlayerMaxMP;
		Candidate.Screen = EGameXXKScreen::DungeonMap;
		Candidate.CurrentRegion = RegionHuangshanName;
		Candidate.CurrentMapId = TEXT("HuangshanRoute");
		Candidate.bDungeonActive = true;
		Candidate.DungeonNodeIndex = 0;
		Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
		ClearActiveBattle(Candidate);
		// Merchant identities are chapter-topology local; generated node IDs restart on the next map.
		Candidate.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
		UGameXXKMVPRules::GenerateRouteMapForSeed(Candidate, NextChapterSeed);
		CandidateProgress.CurrentChapter = NextChapter;
		State = MoveTemp(Candidate);
		return true;
	}

	static void BuildPartySnapshot(FGameXXKRuntimeState& State)
	{
		State.ActiveBattleParty.Reset();
		if (State.PlayerMaxMP <= 0)
		{
			RecalculatePlayerStats(State, true);
		}
		if (State.PlayerMP <= 0)
		{
			State.PlayerMP = State.PlayerMaxMP;
		}
		State.PlayerMP = FMath::Clamp(State.PlayerMP, 0, State.PlayerMaxMP);
		State.ActiveBattleParty.Add(MakeBattleRuntimeUnit(TEXT("Player"), TEXT("Hero"), State.PlayerHP, State.PlayerAttack, State.PlayerDefense, State.PlayerSpeed, 1, false, State.PlayerMP, State.PlayerMaxMP));
	}

	static EGameXXKCardTerrain ResolveCardBattleTerrain(const EGameXXKNodeKind NodeKind, const int32 NodeId)
	{
		if (NodeKind == EGameXXKNodeKind::Boss)
		{
			return EGameXXKCardTerrain::Cave;
		}
		if (NodeKind == EGameXXKNodeKind::Elite)
		{
			return EGameXXKCardTerrain::Forest;
		}
		static const EGameXXKCardTerrain RegularTerrains[] = {
			EGameXXKCardTerrain::Plain,
			EGameXXKCardTerrain::Cliff,
			EGameXXKCardTerrain::WaterShore,
			EGameXXKCardTerrain::Ferry,
			EGameXXKCardTerrain::Village};
		const int32 StableIndex = FMath::Abs(NodeId == INDEX_NONE ? 0 : NodeId) % UE_ARRAY_COUNT(RegularTerrains);
		return RegularTerrains[StableIndex];
	}

	static int32 GetRouteEncounterChapter(const FGameXXKRuntimeState& State)
	{
		const FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		return Progress.SchemaVersion == 1 && Progress.CurrentChapter >= 1 && Progress.CurrentChapter <= 3
			? Progress.CurrentChapter
			: 1;
	}

	static int32 GetRouteEncounterCombatLevel(const FGameXXKRuntimeState& State)
	{
		const FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		return Progress.SchemaVersion == 1 && Progress.RouteCombatLevel >= 1 && Progress.RouteCombatLevel <= 20
			? Progress.RouteCombatLevel
			: FMath::Clamp(State.PlayerLevel, 1, 20);
	}

	static int32 GetRouteEncounterChapterSeed(const FGameXXKRuntimeState& State, const int32 Chapter)
	{
		const FGameXXKRouteProgress& Progress = State.CardRun.RouteProgress;
		if (Progress.SchemaVersion == 1 && Progress.ChapterSeeds.IsValidIndex(Chapter - 1) && Progress.ChapterSeeds[Chapter - 1] != 0)
		{
			return Progress.ChapterSeeds[Chapter - 1];
		}
		const int32 RootSeed = Progress.SchemaVersion == 1 && Progress.RootSeed != 0
			? Progress.RootSeed
			: State.RouteSeed;
		return NormalizeRouteSeed(FGameXXKEncounterRules::DeriveChapterSeed(RootSeed, Chapter));
	}

	static FName MakeEncounterRuntimeEnemyId(const FName DefinitionId, const int32 BattleSlotNumber)
	{
		FString Leaf = DefinitionId.ToString();
		int32 SeparatorIndex = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), SeparatorIndex))
		{
			Leaf = Leaf.RightChop(SeparatorIndex + 1);
		}
		return FName(*FString::Printf(TEXT("Enemy.%s.P%d"), *Leaf, BattleSlotNumber));
	}

	static bool BuildEncounterEnemyProjection(
		const FGameXXKRuntimeState& State,
		const EGameXXKNodeKind NodeKind,
		const int32 NodeId,
		TArray<FGameXXKBattleRuntimeUnit>& OutEnemies)
	{
		const int32 Chapter = GetRouteEncounterChapter(State);
		const int32 ChapterSeed = GetRouteEncounterChapterSeed(State, Chapter);
		const int32 RouteCombatLevel = GetRouteEncounterCombatLevel(State);
		TArray<FGameXXKEncounterSlot> Formation;
		FString FormationError;
		if (!FGameXXKEncounterRules::BuildFormation(Chapter, NodeKind, ChapterSeed, NodeId, RouteCombatLevel, Formation, &FormationError))
		{
			return false;
		}
		const FGameXXKEncounterStatScale Scale = FGameXXKEncounterRules::GetAuthoredStatScale(Chapter, NodeKind);

		TArray<FGameXXKBattleRuntimeUnit> NewEnemies;
		NewEnemies.Reserve(Formation.Num());
		for (const FGameXXKEncounterSlot& EncounterSlot : Formation)
		{
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(EncounterSlot.EnemyDefinitionId);
			if (!Definition)
			{
				return false;
			}
			const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(Definition->Id, EncounterSlot.CombatLevel);
			const int32 ScaledMaxHP = FGameXXKEncounterRules::ScaleStat(Stats.MaxHP, Scale.MaxHPPercent, 1);
			const int32 ScaledAttack = FGameXXKEncounterRules::ScaleStat(Stats.Attack, Scale.AttackPercent, 1);
			const int32 ScaledDefense = FGameXXKEncounterRules::ScaleStat(Stats.Defense, Scale.DefensePercent, 0);
			FGameXXKBattleRuntimeUnit Enemy = MakeBattleRuntimeUnit(
				MakeEncounterRuntimeEnemyId(Definition->Id, EncounterSlot.BattleSlotNumber),
				*Definition->DisplayName.ToString(),
				ScaledMaxHP,
				ScaledAttack,
				ScaledDefense,
				Stats.Speed,
				0,
				true);
			Enemy.EnemyDefinitionId = Definition->Id;
			Enemy.BattleSlotNumber = EncounterSlot.BattleSlotNumber;
			Enemy.CombatLevel = EncounterSlot.CombatLevel;
			NewEnemies.Add(MoveTemp(Enemy));
		}

		OutEnemies = MoveTemp(NewEnemies);
		return true;
	}

	static bool BeginBattle(FGameXXKRuntimeState& State, EGameXXKNodeKind NodeKind, int32 NodeId)
	{
		if (NodeKind != EGameXXKNodeKind::Battle && NodeKind != EGameXXKNodeKind::Elite && NodeKind != EGameXXKNodeKind::Boss)
		{
			return false;
		}

		ClearActiveBattle(State);
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = NodeId;
		BuildPartySnapshot(State);

		if (!BuildEncounterEnemyProjection(State, NodeKind, NodeId, State.ActiveBattleEnemies))
		{
			ClearActiveBattle(State);
			return false;
		}

		for (const FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
		{
			const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Enemy.EnemyDefinitionId);
			const FName CodexEntryId = Definition ? Definition->CodexId : GetCodexEntryIdForBattleRuntimeId(Enemy.Id);
			if (!CodexEntryId.IsNone())
			{
				UGameXXKMVPRules::DiscoverCodexEntry(State, CodexEntryId);
			}
		}

		const int32 BattleSeed = NodeId == INDEX_NONE
			? State.RouteSeed ^ 0x51F15EED
			: FGameXXKCardBattleAdapter::MixBattleSeed(State.RouteSeed, NodeId);
		FString CardBattleError;
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &CardBattleError)
			|| !FGameXXKCardBattleAdapter::BeginCardBattle(
				State,
				NodeKind,
				ResolveCardBattleTerrain(NodeKind, NodeId),
				BattleSeed,
				&CardBattleError))
		{
			ClearActiveBattle(State);
			return false;
		}

		State.Screen = EGameXXKScreen::Battle;
		State.CurrentMapId = TEXT("Battle");
		State.TownPanelMode = EGameXXKTownPanelMode::None;
		return true;
	}

	static bool AreAllEnemiesDefeated(const FGameXXKRuntimeState& State)
	{
		return !State.ActiveBattleEnemies.IsEmpty()
			&& !State.ActiveBattleEnemies.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Enemy)
			{
				return Enemy.bEnemy && !Enemy.bDefeated;
			});
	}

	static bool AreAllPartyMembersDefeated(const FGameXXKRuntimeState& State)
	{
		return !State.ActiveBattleParty.IsEmpty()
			&& !State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& PartyMember)
			{
				return !PartyMember.bEnemy && !PartyMember.bDefeated;
			});
	}

	static bool IsHeroDefeated(const FGameXXKRuntimeState& State)
	{
		return State.ActiveBattleParty.IsValidIndex(0)
			&& (State.ActiveBattleParty[0].bDefeated || State.ActiveBattleParty[0].HP <= 0);
	}

	static bool FailBattleToTownWithRestoredHero(FGameXXKRuntimeState& State)
	{
		FGameXXKRuntimeState Candidate = State;
		Candidate.PlayerHP = Candidate.PlayerMaxHP;
		if (Candidate.ActiveBattleParty.IsValidIndex(0))
		{
			Candidate.ActiveBattleParty[0].HP = Candidate.ActiveBattleParty[0].MaxHP;
			Candidate.ActiveBattleParty[0].bDefeated = false;
		}
		if (!UGameXXKMVPRules::FailDungeonToTown(Candidate))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}

	static void SyncPlayerFromBattle(FGameXXKRuntimeState& State)
	{
		if (!State.ActiveBattleParty.IsValidIndex(0))
		{
			return;
		}

		const FGameXXKBattleRuntimeUnit& Hero = State.ActiveBattleParty[0];
		State.PlayerHP = FMath::Clamp(Hero.HP, 0, State.PlayerMaxHP);
		State.PlayerMP = FMath::Clamp(Hero.MP, 0, State.PlayerMaxMP);
	}

	static int32 FindWeakestLivingPartyIndex(const FGameXXKRuntimeState& State)
	{
		int32 BestIndex = INDEX_NONE;
		float BestRatio = TNumericLimits<float>::Max();
		for (int32 PartyIndex = 0; PartyIndex < State.ActiveBattleParty.Num(); ++PartyIndex)
		{
			const FGameXXKBattleRuntimeUnit& PartyMember = State.ActiveBattleParty[PartyIndex];
			if (PartyMember.bEnemy || PartyMember.bDefeated || PartyMember.HP <= 0)
			{
				continue;
			}

			const float Ratio = PartyMember.MaxHP > 0
				? static_cast<float>(PartyMember.HP) / static_cast<float>(PartyMember.MaxHP)
				: 0.0f;
			if (Ratio < BestRatio)
			{
				BestRatio = Ratio;
				BestIndex = PartyIndex;
			}
		}
		return BestIndex;
	}

	static void MarkDefeatedIfNeeded(FGameXXKBattleRuntimeUnit& Unit)
	{
		if (Unit.HP <= 0)
		{
			Unit.HP = 0;
			Unit.bDefeated = true;
			Unit.bDefending = false;
		}
	}

	static void ClearPartyDefense(FGameXXKRuntimeState& State)
	{
		for (FGameXXKBattleRuntimeUnit& PartyMember : State.ActiveBattleParty)
		{
			PartyMember.bDefending = false;
		}
	}

	static void RunEnemyAI(FGameXXKRuntimeState& State)
	{
		for (FGameXXKBattleRuntimeUnit& Enemy : State.ActiveBattleEnemies)
		{
			if (!Enemy.bEnemy || Enemy.bDefeated || Enemy.HP <= 0)
			{
				continue;
			}

			const int32 TargetIndex = FindWeakestLivingPartyIndex(State);
			if (!State.ActiveBattleParty.IsValidIndex(TargetIndex))
			{
				break;
			}

			FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleParty[TargetIndex];
			int32 Damage = FMath::Max(1, Enemy.Attack - Target.Defense);
			if (Target.bDefending)
			{
				Damage = FMath::Max(1, FMath::CeilToInt(static_cast<float>(Damage) * 0.5f));
			}
			Target.HP = FMath::Max(0, Target.HP - Damage);
			MarkDefeatedIfNeeded(Target);
		}

		ClearPartyDefense(State);
		SyncPlayerFromBattle(State);
	}

	static bool FinishPlayerBattleAction(FGameXXKRuntimeState& State)
	{
		SyncPlayerFromBattle(State);
		if (AreAllEnemiesDefeated(State))
		{
			const bool bBossBattle = IsDungeonNode(State, EGameXXKNodeKind::Boss);
			return UGameXXKMVPRules::ResolveBattleVictory(State, bBossBattle);
		}

		RunEnemyAI(State);
		if (IsHeroDefeated(State) || AreAllPartyMembersDefeated(State))
		{
			return FailBattleToTownWithRestoredHero(State);
		}
		return true;
	}

	static bool ValidatePartyAction(FGameXXKRuntimeState& State, int32 PartyIndex)
	{
		if (State.Screen != EGameXXKScreen::Battle || !State.bHasActiveBattle || !State.ActiveBattleParty.IsValidIndex(PartyIndex))
		{
			return false;
		}
		const FGameXXKBattleRuntimeUnit& Actor = State.ActiveBattleParty[PartyIndex];
		return !Actor.bEnemy && !Actor.bDefeated && Actor.HP > 0;
	}

	static bool ValidateEnemyTarget(FGameXXKRuntimeState& State, int32 EnemyIndex)
	{
		if (!State.ActiveBattleEnemies.IsValidIndex(EnemyIndex))
		{
			return false;
		}
		const FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];
		return Target.bEnemy && !Target.bDefeated && Target.HP > 0;
	}
}

FName UGameXXKMVPRules::RegionQingshan()
{
	return GameXXKMVP::RegionQingshanName;
}

FName UGameXXKMVPRules::RegionHuangshan()
{
	return GameXXKMVP::RegionHuangshanName;
}

FName UGameXXKMVPRules::RegionTanjiang()
{
	return GameXXKMVP::RegionTanjiangName;
}

FName UGameXXKMVPRules::ItemHealingPowder()
{
	return GameXXKMVP::ItemHealingPowderName;
}

FName UGameXXKMVPRules::ItemEnhancementStone()
{
	return GameXXKMVP::ItemEnhancementStoneName;
}

FName UGameXXKMVPRules::ItemRefinementSand()
{
	return GameXXKMVP::ItemRefinementSandName;
}

FName UGameXXKMVPRules::ItemQingshanRouteSeal()
{
	return GameXXKMVP::ItemQingshanRouteSealName;
}

FName UGameXXKMVPRules::ItemTrainingNormalChest()
{
	return GameXXKMVP::ItemTrainingNormalChestName;
}

FName UGameXXKMVPRules::ItemTrainingAdvancedChest()
{
	return GameXXKMVP::ItemTrainingAdvancedChestName;
}

FName UGameXXKMVPRules::ItemIronSword()
{
	return GameXXKMVP::ItemIronSwordName;
}

FName UGameXXKMVPRules::ItemClothArmor()
{
	return GameXXKMVP::ItemClothArmorName;
}

FName UGameXXKMVPRules::ItemWoodenSword()
{
	return GameXXKMVP::ItemWoodenSwordName;
}

FName UGameXXKMVPRules::ItemStarterClothArmor()
{
	return GameXXKMVP::ItemStarterClothArmorName;
}

FName UGameXXKMVPRules::ItemClothTalisman()
{
	return GameXXKMVP::ItemClothTalismanName;
}

FGameXXKItemDef UGameXXKMVPRules::GetItemDef(FName ItemId, bool& bFound)
{
	FGameXXKItemDef Def;
	bFound = GameXXKMVP::GetItemDef(ItemId, Def);
	return Def;
}

TArray<FName> UGameXXKMVPRules::GetKnownItemIds()
{
	return GameXXKMVP::GetKnownItemIds();
}

TArray<FName> UGameXXKMVPRules::GetShopItemIds()
{
	return GameXXKMVP::GetShopItemIds();
}

TArray<FGameXXKCodexEntryDef> UGameXXKMVPRules::GetCodexEntryDefs()
{
	return GameXXKMVP::GetCodexEntryDefsInternal();
}

FGameXXKCodexEntryDef UGameXXKMVPRules::GetCodexEntryDef(FName EntryId, bool& bFound)
{
	bFound = false;
	if (const FGameXXKCodexEntryDef* EntryDef = GameXXKMVP::FindCodexEntryDef(EntryId))
	{
		bFound = true;
		return *EntryDef;
	}
	return FGameXXKCodexEntryDef();
}

TArray<FGameXXKCodexEntryView> UGameXXKMVPRules::BuildCodexEntryViews(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category)
{
	TArray<FGameXXKCodexEntryView> EntryViews;
	for (const FGameXXKCodexEntryDef& EntryDef : GameXXKMVP::GetCodexEntryDefsInternal())
	{
		if (!GameXXKMVP::MatchesCodexCategory(EntryDef, Category))
		{
			continue;
		}

		FGameXXKCodexEntryView EntryView;
		EntryView.Id = EntryDef.Id;
		EntryView.Category = EntryDef.Category;
		EntryView.bIsDiscovered = State.DiscoveredCodexEntryIds.Contains(EntryDef.Id);
		if (EntryView.bIsDiscovered)
		{
			EntryView.DisplayName = EntryDef.DisplayName;
			EntryView.Description = EntryDef.Description;
			EntryView.IconPath = EntryDef.IconPath;
			EntryView.bIsRead = State.ReadCodexEntryIds.Contains(EntryDef.Id);
		}
		else
		{
			EntryView.DisplayName = FText::FromString(TEXT("????"));
			EntryView.Description = FText::FromString(TEXT("未遇见"));
			EntryView.IconPath = FSoftObjectPath();
			EntryView.bIsRead = false;
		}
		EntryViews.Add(MoveTemp(EntryView));
	}
	return EntryViews;
}

int32 UGameXXKMVPRules::GetCodexEntryCount(EGameXXKCodexCategory Category)
{
	int32 EntryCount = 0;
	for (const FGameXXKCodexEntryDef& EntryDef : GameXXKMVP::GetCodexEntryDefsInternal())
	{
		if (GameXXKMVP::MatchesCodexCategory(EntryDef, Category))
		{
			++EntryCount;
		}
	}
	return EntryCount;
}

int32 UGameXXKMVPRules::GetDiscoveredCodexEntryCount(const FGameXXKRuntimeState& State, EGameXXKCodexCategory Category)
{
	int32 DiscoveredEntryCount = 0;
	for (const FGameXXKCodexEntryDef& EntryDef : GameXXKMVP::GetCodexEntryDefsInternal())
	{
		if (GameXXKMVP::MatchesCodexCategory(EntryDef, Category) && State.DiscoveredCodexEntryIds.Contains(EntryDef.Id))
		{
			++DiscoveredEntryCount;
		}
	}
	return DiscoveredEntryCount;
}

bool UGameXXKMVPRules::HasUnreadCodexEntries(const FGameXXKRuntimeState& State)
{
	for (const FGameXXKCodexEntryDef& EntryDef : GameXXKMVP::GetCodexEntryDefsInternal())
	{
		if (State.DiscoveredCodexEntryIds.Contains(EntryDef.Id) && !State.ReadCodexEntryIds.Contains(EntryDef.Id))
		{
			return true;
		}
	}
	return false;
}

bool UGameXXKMVPRules::DiscoverCodexEntry(FGameXXKRuntimeState& State, FName EntryId)
{
	if (!GameXXKMVP::FindCodexEntryDef(EntryId) || State.DiscoveredCodexEntryIds.Contains(EntryId))
	{
		return false;
	}
	State.DiscoveredCodexEntryIds.Add(EntryId);
	return true;
}

bool UGameXXKMVPRules::MarkCodexEntryRead(FGameXXKRuntimeState& State, FName EntryId)
{
	if (!GameXXKMVP::FindCodexEntryDef(EntryId)
		|| !State.DiscoveredCodexEntryIds.Contains(EntryId)
		|| State.ReadCodexEntryIds.Contains(EntryId))
	{
		return false;
	}
	State.ReadCodexEntryIds.Add(EntryId);
	return true;
}

FGameXXKRuntimeState UGameXXKMVPRules::CreateNewGame()
{
	FGameXXKRuntimeState State;
	State.Screen = EGameXXKScreen::MainMenu;
	State.CurrentRegion = NAME_None;
	State.CurrentMapId = TEXT("MainMenu");
	State.EnhancementMaterial = 0;
	State.EquipmentCollection = FGameXXKEquipmentCollectionState();
	State.EquipmentCollection.EquipmentSchemaVersion = 1;
	State.EquipmentCollection.CollectionSeed = 0x4758584B;
	State.MetaShop.Seed = FGameXXKMetaShopRules::DeriveSeed(State);
	State.MetaShop.NextPurchaseOrdinal = 0;
	FGameXXKTrainingRules::InitializeNewGame(State.Training);
	GameXXKMVP::RecalculatePlayerStats(State, false);
	State.UnlockedRegions.Add(RegionQingshan());
	AddItem(State, ItemEnhancementStone(), 10);
	// UI V2 starter set: six ordinary starter equipment pieces (no legacy items).
	static const EGameXXKEquipmentSlot StarterSlots[] = {
		EGameXXKEquipmentSlot::Weapon, EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor, EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes, EGameXXKEquipmentSlot::Accessory};
	for (const EGameXXKEquipmentSlot Slot : StarterSlots)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::Starter;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;
		FName InstanceId;
		FString Error;
		FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error);
	}
	FGameXXKDesktopInventoryRules::Normalize(State);
	return State;
}

int32 UGameXXKMVPRules::GetCurrentSaveVersion()
{
	return GameXXKMVP::CurrentSaveVersion;
}

FName UGameXXKMVPRules::TaskQingshanMain()
{
	return GameXXKMVP::TaskQingshanMainName;
}

TArray<FGameXXKTaskView> UGameXXKMVPRules::BuildAvailableTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category)
{
	TArray<FGameXXKTaskView> Tasks;
	if (Category != EGameXXKTaskCategory::Main || State.QuestState != EGameXXKQuestState::NotAccepted)
	{
		return Tasks;
	}

	FGameXXKTaskView Task;
	Task.Id = TaskQingshanMain();
	Task.Category = EGameXXKTaskCategory::Main;
	Task.Title = NSLOCTEXT("GameXXKTaskPanel", "QingshanMainTitle", "初入江湖");
	Task.ProgressTarget = 1;
	Task.Reward.Gold = 500;
	Task.Reward.Experience = 1200;
	Task.Reward.Token = 10;

	Task.Description = NSLOCTEXT("GameXXKTaskPanel", "QingshanMainNotAcceptedDescription", "前往青山镇寻找引路人");

	Tasks.Add(MoveTemp(Task));
	return Tasks;
}

TArray<FGameXXKTaskView> UGameXXKMVPRules::BuildAcceptedTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category)
{
	TArray<FGameXXKTaskView> Tasks;
	if (Category != EGameXXKTaskCategory::Main || State.QuestState != EGameXXKQuestState::Accepted)
	{
		return Tasks;
	}

	FGameXXKTaskView Task;
	Task.Id = TaskQingshanMain();
	Task.Category = EGameXXKTaskCategory::Main;
	Task.Title = NSLOCTEXT("GameXXKTaskPanel", "QingshanMainTitle", "初入江湖");
	Task.Description = NSLOCTEXT("GameXXKTaskPanel", "QingshanMainAcceptedDescription", "与引路人同行，前往北门出口");
	Task.ProgressCurrent = 1;
	Task.ProgressTarget = 1;
	Task.Reward.Gold = 500;
	Task.Reward.Experience = 1200;
	Task.Reward.Token = 10;
	Tasks.Add(MoveTemp(Task));
	return Tasks;
}

TArray<FGameXXKTaskView> UGameXXKMVPRules::BuildTaskViews(const FGameXXKRuntimeState& State, EGameXXKTaskCategory Category)
{
	return State.QuestState == EGameXXKQuestState::NotAccepted
		? BuildAvailableTaskViews(State, Category)
		: BuildAcceptedTaskViews(State, Category);
}

bool UGameXXKMVPRules::OpenWorldMap(FGameXXKRuntimeState& State)
{
	State.Screen = EGameXXKScreen::WorldMap;
	State.CurrentRegion = NAME_None;
	State.CurrentMapId = TEXT("WorldMap");
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

bool UGameXXKMVPRules::EnterWorldRegion(FGameXXKRuntimeState& State, FName RegionId)
{
	// Qingshan is the only world-map region with a dedicated playable town level.
	// Keep future-region unlocks in save data, but never route them into Qingshan by fallback.
	if (RegionId != RegionQingshan() || !State.UnlockedRegions.Contains(RegionId))
	{
		return false;
	}
	State.CurrentRegion = RegionId;
	State.CurrentMapId = RegionId;
	State.Screen = EGameXXKScreen::Town;
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

bool UGameXXKMVPRules::AcceptTownQuest(FGameXXKRuntimeState& State)
{
	if (State.Screen != EGameXXKScreen::Town || State.QuestState != EGameXXKQuestState::NotAccepted)
	{
		return false;
	}
	if (!AddItem(State, ItemQingshanRouteSeal(), 1))
	{
		return false;
	}
	// Accepting the quest keeps the guide NPC in town. The player recruits the
	// narrative follower explicitly through the NPC dialog's 入队 action.
	State.QuestState = EGameXXKQuestState::Accepted;
	State.TrackedTaskId = NAME_None;
	DiscoverCodexEntry(State, GameXXKMVP::CodexGuideName);
	return true;
}

bool UGameXXKMVPRules::CanEnterDungeon(const FGameXXKRuntimeState& State)
{
	return State.QuestState == EGameXXKQuestState::Accepted;
}

void UGameXXKMVPRules::GenerateRouteMapForSeed(FGameXXKRuntimeState& State, int32 Seed)
{
	const int32 NormalizedSeed = GameXXKMVP::NormalizeRouteSeed(Seed);
	FRandomStream Stream(NormalizedSeed);
	static constexpr int32 FinalLayerIndex = 6;

	State.bHasGeneratedRouteMap = true;
	State.RouteSeed = NormalizedSeed;
	State.CurrentRouteNodeId = 0;
	State.PendingRouteNodeId = INDEX_NONE;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.VisitedRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.ReachableRouteNodeIds.Add(0);
	GameXXKMVP::ClearBattleEntryCheckpoint(State);

	int32 NextNodeId = 0;
	GameXXKMVP::AddRouteNode(State, NextNodeId++, 0, 0, EGameXXKNodeKind::Start, 0.50f, 0.00f, {});
	for (int32 LayerIndex = 1; LayerIndex < FinalLayerIndex; ++LayerIndex)
	{
		const int32 LayerNodeCount = GameXXKMVP::GetGeneratedLayerNodeCount(Stream, LayerIndex);
		for (int32 ColumnIndex = 0; ColumnIndex < LayerNodeCount; ++ColumnIndex)
		{
			const float BaseX = static_cast<float>(ColumnIndex + 1) / static_cast<float>(LayerNodeCount + 1);
			const float Jitter = LayerNodeCount > 1 ? Stream.FRandRange(-0.035f, 0.035f) : 0.0f;
			const float X = FMath::Clamp(BaseX + Jitter, 0.12f, 0.88f);
			const float Y = static_cast<float>(LayerIndex) / static_cast<float>(FinalLayerIndex);
			const EGameXXKNodeKind NodeKind = GameXXKMVP::PickGeneratedRouteNodeKind(Stream, LayerIndex, ColumnIndex);
			GameXXKMVP::AddRouteNode(State, NextNodeId++, LayerIndex, ColumnIndex, NodeKind, X, Y, {});
		}
	}
	GameXXKMVP::AddRouteNode(State, NextNodeId++, FinalLayerIndex, 0, EGameXXKNodeKind::Boss, 0.50f, 1.00f, {});

	for (int32 LayerIndex = 0; LayerIndex < FinalLayerIndex; ++LayerIndex)
	{
		GameXXKMVP::ConnectGeneratedRouteLayer(State, Stream, LayerIndex);
	}
}

bool UGameXXKMVPRules::EnterDungeon(FGameXXKRuntimeState& State)
{
	if (State.Screen != EGameXXKScreen::Town || State.CurrentRegion != RegionQingshan() || !CanEnterDungeon(State))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	// A route is a self-contained card run. Keep the explicit town NPC party choice,
	// while clearing prior route rewards, battles, and pending events.
	FString CardRunError;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &CardRunError))
	{
		return false;
	}
	const FName SelectedTownNpcId = Candidate.CardRun.ActiveTemporaryQuestNpcId;
	FGameXXKCardBattleAdapter::ClearRouteLocalCardState(Candidate);
	GameXXKMVP::ClearBattleEntryCheckpoint(Candidate);
	// InitializeRoute is intentionally idempotent, so a genuinely new route must
	// first discard a prior valid balance and its chapter-scoped receipts.
	FGameXXKRouteEconomyRules::ClearRouteEconomy(Candidate.CardRun);
	Candidate.CardRun.bLoadoutLockedForRoute = true;
	Candidate.Screen = EGameXXKScreen::DungeonMap;
	Candidate.CurrentRegion = RegionHuangshan();
	Candidate.CurrentMapId = TEXT("HuangshanRoute");
	Candidate.bDungeonActive = true;
	Candidate.DungeonNodeIndex = 0;
	Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
	GameXXKMVP::GenerateRouteMap(Candidate);
	GameXXKMVP::InitializeThreeChapterRouteProgress(Candidate);
	if (!SelectedTownNpcId.IsNone()
		&& !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Candidate, SelectedTownNpcId, {}, &CardRunError))
	{
		return false;
	}
	if (!FGameXXKRouteEconomyRules::InitializeRoute(Candidate.CardRun, 60, &CardRunError))
	{
		return false;
	}
	const int32 RootSeed = Candidate.CardRun.RouteProgress.RootSeed;
	Candidate.CardRun.RouteRandomSeed = RootSeed;
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::AdvanceDungeonNode(FGameXXKRuntimeState& State, EGameXXKNodeKind ExpectedNode)
{
	if (State.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* Node = GameXXKMVP::FindFirstReachableRouteNodeOfKind(State, ExpectedNode);
		return Node ? SelectRouteNodeById(State, Node->NodeId) : false;
	}
	if (!GameXXKMVP::IsDungeonNode(State, ExpectedNode))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	if (ExpectedNode == EGameXXKNodeKind::Battle || ExpectedNode == EGameXXKNodeKind::Boss)
	{
		Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
		if (!GameXXKMVP::BeginBattle(Candidate, ExpectedNode, INDEX_NONE))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	if (!GameXXKMVP::SettleFixedRouteNode(
		Candidate,
		BeforeOneTimeRewards,
		GameXXKMVP::GetBaseRouteNodeTravelMoney(ExpectedNode)))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::SelectRouteNodeById(FGameXXKRuntimeState& State, int32 NodeId)
{
	if (!State.bDungeonActive || !State.bHasGeneratedRouteMap || State.Screen != EGameXXKScreen::DungeonMap || !State.ReachableRouteNodeIds.Contains(NodeId))
	{
		return false;
	}

	const FGameXXKRouteMapNode* Node = GameXXKMVP::FindRouteNode(State, NodeId);
	if (!Node)
	{
		return false;
	}

	const EGameXXKNodeKind NodeKind = Node->NodeKind;
	FGameXXKRuntimeState Candidate = State;
	Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
	if (Node->NodeKind == EGameXXKNodeKind::Battle || Node->NodeKind == EGameXXKNodeKind::Elite || Node->NodeKind == EGameXXKNodeKind::Boss)
	{
		Candidate.BattleEntryCheckpoint = GameXXKMVP::CaptureBattleEntryCheckpoint(State, NodeId);
		Candidate.CurrentRouteNodeId = NodeId;
		Candidate.PendingRouteNodeId = NodeId;
		if (!GameXXKMVP::BeginBattle(Candidate, NodeKind, NodeId))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	Candidate.CurrentRouteNodeId = NodeId;
	if (Node->NodeKind == EGameXXKNodeKind::Event || Node->NodeKind == EGameXXKNodeKind::Chest)
	{
		Candidate.PendingRouteNodeId = NodeId;
		if (Node->NodeKind == EGameXXKNodeKind::Event)
		{
			int32 EventChoiceSeed = Candidate.RouteSeed ^ NodeId ^ 0x5F3759DF;
			if (EventChoiceSeed == 0)
			{
				EventChoiceSeed = 0x6D2B79F5;
			}
			FName IgnoredEventNpcId;
			if (!FGameXXKCardBattleAdapter::CreateRouteEventOffer(Candidate, NodeId, EventChoiceSeed, IgnoredEventNpcId))
			{
				return false;
			}
		}
		else
		{
			int32 RewardSeed = Candidate.RouteSeed ^ NodeId ^ 0x4A39B70D;
			if (RewardSeed == 0) RewardSeed = 0x13572468;
			const FGameXXKRouteEncounterDefinition* Encounter = FGameXXKRouteEncounterCatalog::ChooseDeterministic(EGameXXKRouteEncounterKind::Chest, RewardSeed);
			TArray<FName> RelicIds;
			if (!Encounter || !FGameXXKRelicRules::CreateRelicOffer(Candidate, NodeId, RewardSeed, RelicIds))
			{
				return false;
			}
			Candidate.CardRun.PendingEvent.SourceNodeId = NodeId;
			Candidate.CardRun.PendingEvent.ChoiceSeed = RewardSeed;
			Candidate.CardRun.PendingEvent.EncounterId = Encounter->Id;
			Candidate.CardRun.PendingEvent.EventNpcId = NAME_None;
		}
		Candidate.Screen = EGameXXKScreen::RouteEvent;
		Candidate.CurrentMapId = TEXT("RouteEvent");
		State = MoveTemp(Candidate);
		return true;
	}
	if (Node->NodeKind == EGameXXKNodeKind::Camp)
	{
		Candidate.PendingRouteNodeId = NodeId;
		Candidate.Screen = EGameXXKScreen::RouteCamp;
		Candidate.CurrentMapId = TEXT("RouteCamp");
		State = MoveTemp(Candidate);
		return true;
	}
	if (Node->NodeKind == EGameXXKNodeKind::Merchant)
	{
		Candidate.PendingRouteNodeId = NodeId;
		Candidate.Screen = EGameXXKScreen::RouteMerchant;
		Candidate.CurrentMapId = TEXT("RouteMerchant");
		if (!FGameXXKRouteMerchantRules::EnsureStock(Candidate))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}

	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	if (!GameXXKMVP::SettleGeneratedRouteNode(
		Candidate,
		BeforeOneTimeRewards,
		NodeId,
		GameXXKMVP::GetBaseRouteNodeTravelMoney(NodeKind)))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::RetreatCurrentBattleToRoute(FGameXXKRuntimeState& State)
{
	const FGameXXKBattleEntryCheckpoint& Checkpoint = State.BattleEntryCheckpoint;
	if (!Checkpoint.bValid
		|| !State.bDungeonActive
		|| !State.bHasGeneratedRouteMap
		|| State.Screen != EGameXXKScreen::Battle
		|| !State.bHasActiveBattle
		|| !State.CardRun.bHasActiveCardBattle
		|| Checkpoint.SourceNodeId != State.CurrentRouteNodeId
		|| Checkpoint.SourceNodeId != State.PendingRouteNodeId
		|| Checkpoint.SourceNodeId != State.ActiveBattleNodeId
		|| Checkpoint.SourceNodeId != State.CardRun.ActiveBattleSourceNodeId)
	{
		return false;
	}

	const FGameXXKRouteMapNode* SourceNode = GameXXKMVP::FindRouteNode(State, Checkpoint.SourceNodeId);
	if (!SourceNode
		|| (SourceNode->NodeKind != EGameXXKNodeKind::Battle
			&& SourceNode->NodeKind != EGameXXKNodeKind::Elite
			&& SourceNode->NodeKind != EGameXXKNodeKind::Boss))
	{
		return false;
	}

	FString ValidationError;
	if (!FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = State;
	const FGameXXKBattleEntryCheckpoint SavedCheckpoint = Candidate.BattleEntryCheckpoint;
	Candidate.CurrentRouteNodeId = SavedCheckpoint.PreviousCurrentRouteNodeId;
	Candidate.PendingRouteNodeId = INDEX_NONE;
	Candidate.DungeonNodeIndex = SavedCheckpoint.PreviousDungeonNodeIndex;
	Candidate.PlayerHP = SavedCheckpoint.PreviousPlayerHP;
	Candidate.PlayerMP = SavedCheckpoint.PreviousPlayerMP;
	Candidate.VisitedRouteNodeIds = SavedCheckpoint.PreviousVisitedRouteNodeIds;
	Candidate.ReachableRouteNodeIds = SavedCheckpoint.PreviousReachableRouteNodeIds;
	Candidate.Screen = EGameXXKScreen::DungeonMap;
	Candidate.CurrentMapId = TEXT("HuangshanRoute");
	Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
	GameXXKMVP::ClearActiveBattle(Candidate);
	GameXXKMVP::ClearBattleEntryCheckpoint(Candidate);

	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, ValidationError))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::ResolveBattleVictory(FGameXXKRuntimeState& State, bool bBossBattle)
{
	if (State.Screen != EGameXXKScreen::Battle)
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = State;
	if (!Candidate.CardRun.bHasActiveCardBattle
		|| Candidate.CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory)
	{
		return false;
	}
	const FGameXXKRouteMapNode* RewardPendingNode = Candidate.bHasGeneratedRouteMap
		? GameXXKMVP::FindPendingRouteNode(Candidate)
		: nullptr;
	const EGameXXKNodeKind RewardNodeKind = RewardPendingNode
		? RewardPendingNode->NodeKind
		: (bBossBattle ? EGameXXKNodeKind::Boss : EGameXXKNodeKind::Battle);
	const int32 StableRewardSourceNodeId = RewardPendingNode
		? RewardPendingNode->NodeId
		: Candidate.DungeonNodeIndex;
	if (!Candidate.CardRun.bActiveBattleRewardResolved)
	{
		int32 ChoiceSeed = Candidate.CardRun.PendingReward.ChoiceSeed;
		if (Candidate.CardRun.PendingReward.Options.IsEmpty()
			&& Candidate.CardRun.PendingReward.CardIds.IsEmpty()
			&& !GameXXKMVP::TryBuildRouteRewardChoiceSeed(
				Candidate.CardRun.RouteRandomSeed,
				StableRewardSourceNodeId,
				Candidate.CardRun.NextRewardOrdinal,
				ChoiceSeed))
		{
			return false;
		}
		if (!FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(
			Candidate,
			RewardNodeKind,
			StableRewardSourceNodeId,
			ChoiceSeed))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	if (Candidate.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(Candidate);
		if (!PendingNode || (PendingNode->NodeKind != EGameXXKNodeKind::Battle && PendingNode->NodeKind != EGameXXKNodeKind::Elite && PendingNode->NodeKind != EGameXXKNodeKind::Boss))
		{
			return false;
		}
		const int32 NodeId = PendingNode->NodeId;
		const EGameXXKNodeKind NodeKind = PendingNode->NodeKind;
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (NodeKind == EGameXXKNodeKind::Boss)
		{
			GameXXKMVP::ApplyXP(Candidate, 150);
			GameXXKMVP::AwardActivePermanentCompanionBattleExperience(Candidate, GameXXKMVP::BossPermanentCompanionBattleExperience);
			FGameXXKEquipmentTransactionResult EquipmentReward;
			FGameXXKEquipmentEconomyRules::GrantLegacyEquipmentForCompatibility(Candidate, ItemClothArmor(), 1, EquipmentReward);
			if (!GameXXKMVP::SettleGeneratedRouteNode(
				Candidate,
				BeforeOneTimeRewards,
				NodeId,
				GameXXKMVP::GetBaseRouteNodeTravelMoney(NodeKind)))
			{
				return false;
			}
			GameXXKMVP::ClearActiveBattle(Candidate);
			if (!ResolveBossClear(Candidate))
			{
				return false;
			}
			State = MoveTemp(Candidate);
			return true;
		}
		GameXXKMVP::ApplyXP(Candidate, NodeKind == EGameXXKNodeKind::Elite ? 110 : 80);
		GameXXKMVP::AwardActivePermanentCompanionBattleExperience(
			Candidate,
			NodeKind == EGameXXKNodeKind::Elite
				? GameXXKMVP::ElitePermanentCompanionBattleExperience
				: GameXXKMVP::NormalPermanentCompanionBattleExperience);
		AddItem(Candidate, ItemHealingPowder(), 1);
		if (!GameXXKMVP::SettleGeneratedRouteNode(
			Candidate,
			BeforeOneTimeRewards,
			NodeId,
			GameXXKMVP::GetBaseRouteNodeTravelMoney(NodeKind)))
		{
			return false;
		}
		GameXXKMVP::ClearActiveBattle(Candidate);
		State = MoveTemp(Candidate);
		return true;
	}
	if (bBossBattle)
	{
		if (!GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Boss))
		{
			return false;
		}
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		GameXXKMVP::ApplyXP(Candidate, 150);
		GameXXKMVP::AwardActivePermanentCompanionBattleExperience(Candidate, GameXXKMVP::BossPermanentCompanionBattleExperience);
		FGameXXKEquipmentTransactionResult EquipmentReward;
		FGameXXKEquipmentEconomyRules::GrantLegacyEquipmentForCompatibility(Candidate, ItemClothArmor(), 1, EquipmentReward);
		if (!GameXXKMVP::SettleFixedRouteNode(
			Candidate,
			BeforeOneTimeRewards,
			GameXXKMVP::GetBaseRouteNodeTravelMoney(EGameXXKNodeKind::Boss)))
		{
			return false;
		}
		GameXXKMVP::ClearActiveBattle(Candidate);
		if (!ResolveBossClear(Candidate))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	if (!GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Battle))
	{
		return false;
	}
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	GameXXKMVP::ApplyXP(Candidate, 80);
	GameXXKMVP::AwardActivePermanentCompanionBattleExperience(Candidate, GameXXKMVP::NormalPermanentCompanionBattleExperience);
	AddItem(Candidate, ItemHealingPowder(), 1);
	if (!GameXXKMVP::SettleFixedRouteNode(
		Candidate,
		BeforeOneTimeRewards,
		GameXXKMVP::GetBaseRouteNodeTravelMoney(EGameXXKNodeKind::Battle)))
	{
		return false;
	}
	GameXXKMVP::ClearActiveBattle(Candidate);
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
	FGameXXKRuntimeState& State,
	const int32 OptionIndex,
	const FName ReplacementEntryId,
	FString* OutError)
{
	FGameXXKRuntimeState Candidate = State;

	bool bBossBattle = false;
	if (Candidate.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(Candidate);
		if (!PendingNode
			|| (PendingNode->NodeKind != EGameXXKNodeKind::Battle
				&& PendingNode->NodeKind != EGameXXKNodeKind::Elite
				&& PendingNode->NodeKind != EGameXXKNodeKind::Boss))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending battle reward has no resolvable battle node.");
			}
			return false;
		}
		bBossBattle = PendingNode->NodeKind == EGameXXKNodeKind::Boss;
	}
	else
	{
		bBossBattle = GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Boss);
		if (!bBossBattle && !GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Battle))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending battle reward has no resolvable fixed battle node.");
			}
			return false;
		}
	}

	if (!Candidate.CardRun.PendingReward.Options.IsValidIndex(OptionIndex))
	{
		if (OutError)
		{
			*OutError = TEXT("The chosen battle reward option is not part of the saved offer.");
		}
		return false;
	}

	const FGameXXKBattleRewardOption Option = Candidate.CardRun.PendingReward.Options[OptionIndex];
	switch (Option.Kind)
	{
	case EGameXXKBattleRewardKind::DeckCardUpgrade:
		{
			const EGameXXKCardQuality CurrentQuality =
				FGameXXKCardBattleAdapter::GetConfiguredCardQuality(Candidate.CardRun, Option.CardId);
			if (CurrentQuality >= EGameXXKCardQuality::Epic)
			{
				if (OutError)
				{
					*OutError = TEXT("The chosen deck card is already at maximum quality.");
				}
				return false;
			}
			Candidate.CardRun.UpgradedCardQualities.Add(
				Option.CardId,
				FGameXXKCardBattleAdapter::GetNextCardQuality(CurrentQuality));
			break;
		}
	case EGameXXKBattleRewardKind::BossCard:
		if (!FGameXXKCardBattleAdapter::CommitBossCardReward(
			Candidate,
			Option.CardId,
			OutError))
		{
			return false;
		}
		break;
	case EGameXXKBattleRewardKind::Relic:
		if (!FGameXXKRelicRules::AcquireRelic(Candidate, Option.RelicId, OutError))
		{
			return false;
		}
		break;
	case EGameXXKBattleRewardKind::EnergyCapBonus:
		++Candidate.CardRun.BonusSharedEnergyCap;
		break;
	case EGameXXKBattleRewardKind::DrawBonus:
		++Candidate.CardRun.BonusRoundDrawCount;
		break;
	default:
		if (OutError)
		{
			*OutError = TEXT("The chosen battle reward option has an unknown kind.");
		}
		return false;
	}

	Candidate.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
	Candidate.CardRun.bActiveBattleRewardResolved = true;

	if (!ResolveBattleVictory(Candidate, bBossBattle))
	{
		if (OutError)
		{
			*OutError = TEXT("Reward choice succeeded but battle victory settlement failed.");
		}
		return false;
	}

	State = MoveTemp(Candidate);
	return true;
}


bool UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(
	FGameXXKRuntimeState& State,
	FString* OutError)
{
	FGameXXKRuntimeState Candidate = State;
	if (!FGameXXKCardBattleAdapter::SkipPendingRouteReward(Candidate, OutError))
	{
		return false;
	}

	bool bBossBattle = false;
	if (Candidate.bHasGeneratedRouteMap)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(Candidate);
		if (!PendingNode
			|| (PendingNode->NodeKind != EGameXXKNodeKind::Battle
				&& PendingNode->NodeKind != EGameXXKNodeKind::Elite
				&& PendingNode->NodeKind != EGameXXKNodeKind::Boss))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending route reward has no resolvable battle node.");
			}
			return false;
		}
		bBossBattle = PendingNode->NodeKind == EGameXXKNodeKind::Boss;
	}
	else
	{
		bBossBattle = GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Boss);
		if (!bBossBattle && !GameXXKMVP::IsDungeonNode(Candidate, EGameXXKNodeKind::Battle))
		{
			if (OutError)
			{
				*OutError = TEXT("Pending route reward has no resolvable fixed battle node.");
			}
			return false;
		}
	}

	if (!ResolveBattleVictory(Candidate, bBossBattle))
	{
		if (OutError)
		{
			*OutError = TEXT("Reward skip succeeded but battle victory settlement failed.");
		}
		return false;
	}

	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::ExecuteBattleBasicAttack(FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex) || !GameXXKMVP::ValidateEnemyTarget(State, EnemyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Attacker = State.ActiveBattleParty[PartyIndex];
	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];

	const int32 Damage = FMath::Max(1, Attacker.Attack - Target.Defense);
	Target.HP = FMath::Max(0, Target.HP - Damage);
	GameXXKMVP::MarkDefeatedIfNeeded(Target);
	Attacker.MP = FMath::Min(Attacker.MaxMP, Attacker.MP + 2);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleCraneWingSlash(FGameXXKRuntimeState& State, int32 PartyIndex, int32 EnemyIndex)
{
	static constexpr int32 MPCost = 8;
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex) || !GameXXKMVP::ValidateEnemyTarget(State, EnemyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Attacker = State.ActiveBattleParty[PartyIndex];
	if (Attacker.MP < MPCost)
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleEnemies[EnemyIndex];
	Attacker.MP -= MPCost;
	const int32 Damage = FMath::Max(1, FMath::RoundToInt(static_cast<float>(Attacker.Attack) * 1.6f) + 6 - Target.Defense);
	Target.HP = FMath::Max(0, Target.HP - Damage);
	GameXXKMVP::MarkDefeatedIfNeeded(Target);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleGuiyuanArt(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	static constexpr int32 MPCost = 10;
	static constexpr int32 HealAmount = 36;
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Caster = State.ActiveBattleParty[PartyIndex];
	if (Caster.MP < MPCost || Caster.HP >= Caster.MaxHP)
	{
		return false;
	}

	Caster.MP -= MPCost;
	Caster.HP = FMath::Min(Caster.MaxHP, Caster.HP + HealAmount);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleDefend(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Defender = State.ActiveBattleParty[PartyIndex];
	Defender.bDefending = true;
	Defender.MP = FMath::Min(Defender.MaxMP, Defender.MP + 6);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ExecuteBattleHealingPowder(FGameXXKRuntimeState& State, int32 PartyIndex)
{
	if (!GameXXKMVP::ValidatePartyAction(State, PartyIndex))
	{
		return false;
	}

	FGameXXKBattleRuntimeUnit& Target = State.ActiveBattleParty[PartyIndex];
	if (Target.HP >= Target.MaxHP || !RemoveItem(State, ItemHealingPowder(), 1))
	{
		return false;
	}

	FGameXXKItemDef HealingPowderDef;
	const int32 HealAmount = GameXXKMVP::GetItemDef(ItemHealingPowder(), HealingPowderDef)
		? HealingPowderDef.HealAmount
		: 35;
	Target.HP = FMath::Min(Target.MaxHP, Target.HP + HealAmount);
	return GameXXKMVP::FinishPlayerBattleAction(State);
}

bool UGameXXKMVPRules::ResolveRouteEncounterChoice(FGameXXKRuntimeState& State, const int32 ChoiceIndex)
{
	if (!State.bHasGeneratedRouteMap || State.Screen != EGameXXKScreen::RouteEvent || ChoiceIndex < 0)
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(Candidate);
	if (!PendingNode || (PendingNode->NodeKind != EGameXXKNodeKind::Event && PendingNode->NodeKind != EGameXXKNodeKind::Chest))
	{
		return false;
	}
	const int32 NodeId = PendingNode->NodeId;
	const EGameXXKNodeKind NodeKind = PendingNode->NodeKind;
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	const int32 Chapter = Candidate.CardRun.RouteProgress.CurrentChapter;
	if (GameXXKMVP::HasRouteNodeReceipt(Candidate, Chapter, NodeId))
	{
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		if (NodeKind == EGameXXKNodeKind::Chest)
		{
			Candidate.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
		}
		Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
		State = MoveTemp(Candidate);
		return true;
	}
	const FGameXXKRouteEncounterDefinition* Encounter = FGameXXKRouteEncounterCatalog::FindDefinition(Candidate.CardRun.PendingEvent.EncounterId);
	if (!Encounter || Encounter->Choices.Num() <= ChoiceIndex)
	{
		return false;
	}

	if (NodeKind == EGameXXKNodeKind::Chest)
	{
		if (Encounter->Kind != EGameXXKRouteEncounterKind::Chest
			|| !Candidate.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(ChoiceIndex))
		{
			return false;
		}
		const FName ChosenRelicId = Candidate.CardRun.PendingRelicOffer.RelicIds[ChoiceIndex];
		if (!FGameXXKRelicRules::ChoosePendingRelic(Candidate, ChosenRelicId)
			|| !GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		Candidate.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
		Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
		State = MoveTemp(Candidate);
		return true;
	}

	if (Encounter->Kind != EGameXXKRouteEncounterKind::Event)
	{
		return false;
	}
	const FGameXXKRouteEncounterChoiceDefinition& Choice = Encounter->Choices[ChoiceIndex];
	if (Choice.RewardKind == EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport)
	{
		if (!Candidate.CardRun.PartySelection.QuestNpc.NpcId.IsNone()
			|| Choice.QuestNpcId.IsNone()
			|| !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Candidate, Choice.QuestNpcId, {}))
		{
			return false;
		}
	}
	else if (Choice.RewardKind != EGameXXKRouteEncounterRewardKind::RouteAttribute)
	{
		return false;
	}

	const int32 Magnitude = FMath::Max(0, Choice.Magnitude);
	switch (Choice.AttributeKind)
	{
	case EGameXXKRouteAttributeKind::MaxHealth: Candidate.CardRun.RouteAttributeBonuses.MaxHealth += Magnitude; break;
	case EGameXXKRouteAttributeKind::MaxMana: Candidate.CardRun.RouteAttributeBonuses.MaxMana += Magnitude; break;
	case EGameXXKRouteAttributeKind::Attack: Candidate.CardRun.RouteAttributeBonuses.Attack += Magnitude; break;
	case EGameXXKRouteAttributeKind::Defense: Candidate.CardRun.RouteAttributeBonuses.Defense += Magnitude; break;
	case EGameXXKRouteAttributeKind::Speed: Candidate.CardRun.RouteAttributeBonuses.Speed += Magnitude; break;
	default: return false;
	}
	if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
	{
		return false;
	}
	Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::ResolveEventReward(FGameXXKRuntimeState& State, bool bTakeGold)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteEvent)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		// Chest nodes own a pending three-relic choice. They must only complete
		// through ResolveRouteEncounterChoice; otherwise the unresolved offer can
		// survive into a later chapter and block every subsequent chest.
		if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Event)
		{
			return false;
		}
		// A saved catalog encounter owns explicit choices and must resolve only
		// through ResolveRouteEncounterChoice. This API is legacy compatibility
		// for pending event states that predate the catalog encounter identity.
		if (!State.CardRun.PendingEvent.EncounterId.IsNone())
		{
			return false;
		}
		const int32 NodeId = PendingNode->NodeId;
		FGameXXKRuntimeState Candidate = State;
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (!bTakeGold)
		{
			AddItem(Candidate, ItemHealingPowder(), 1);
		}
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, bTakeGold ? 20 : 0))
		{
			return false;
		}
		if (Candidate.CardRun.PendingEvent.SourceNodeId == NodeId)
		{
			Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
		}
		State = MoveTemp(Candidate);
		return true;
	}
	if (State.bHasGeneratedRouteMap)
	{
		return false;
	}
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Event))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	if (!bTakeGold)
	{
		AddItem(Candidate, ItemHealingPowder(), 1);
	}
	if (!GameXXKMVP::SettleFixedRouteNode(Candidate, BeforeOneTimeRewards, bTakeGold ? 20 : 0))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::AcceptRouteEventNpcSupport(FGameXXKRuntimeState& State)
{
	if (!State.bHasGeneratedRouteMap || State.Screen != EGameXXKScreen::RouteEvent)
	{
		return false;
	}
	const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
	if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Event)
	{
		return false;
	}
	const int32 NodeId = PendingNode->NodeId;
	if (GameXXKMVP::HasRouteNodeReceipt(State, State.CardRun.RouteProgress.CurrentChapter, NodeId))
	{
		FGameXXKRuntimeState Candidate = State;
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
		State = MoveTemp(Candidate);
		return true;
	}
	const FGameXXKPendingRouteEvent PendingEvent = State.CardRun.PendingEvent;
	if (PendingEvent.SourceNodeId != PendingNode->NodeId
		|| PendingEvent.EventNpcId.IsNone()
		|| !FGameXXKCompanionCatalog::FindQuestNpcDefinition(PendingEvent.EventNpcId))
	{
		return false;
	}
	// A route intentionally has one temporary support slot.  Never replace a
	// previously accepted task NPC without a separate player-confirmed flow.
	if (!State.CardRun.PartySelection.QuestNpc.NpcId.IsNone())
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	if (!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Candidate, PendingEvent.EventNpcId, {}))
	{
		return false;
	}
	if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
	{
		return false;
	}
	Candidate.CardRun.PendingEvent = FGameXXKPendingRouteEvent();
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::ResolveCampReward(FGameXXKRuntimeState& State, bool bHealNow)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteCamp)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Camp)
		{
			return false;
		}
		const int32 NodeId = PendingNode->NodeId;
		FGameXXKRuntimeState Candidate = State;
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (bHealNow)
		{
			Candidate.PlayerHP = Candidate.PlayerMaxHP;
		}
		else
		{
			AddItem(Candidate, ItemHealingPowder(), 1);
		}
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::DungeonMap)
	{
		const FGameXXKRouteMapNode* Node = GameXXKMVP::FindFirstReachableRouteNodeOfKind(State, EGameXXKNodeKind::Camp);
		if (!Node)
		{
			return false;
		}
		const int32 NodeId = Node->NodeId;
		FGameXXKRuntimeState Candidate = State;
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (bHealNow)
		{
			Candidate.PlayerHP = Candidate.PlayerMaxHP;
		}
		else
		{
			AddItem(Candidate, ItemHealingPowder(), 1);
		}
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	if (!GameXXKMVP::IsDungeonNode(State, EGameXXKNodeKind::Camp))
	{
		return false;
	}
	FGameXXKRuntimeState Candidate = State;
	const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
	if (bHealNow)
	{
		Candidate.PlayerHP = Candidate.PlayerMaxHP;
	}
	else
	{
		AddItem(Candidate, ItemHealingPowder(), 1);
	}
	if (!GameXXKMVP::SettleFixedRouteNode(Candidate, BeforeOneTimeRewards, 0))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::EnsureRouteMerchantStock(FGameXXKRuntimeState& State, FString* OutError)
{
	return FGameXXKRouteMerchantRules::EnsureStock(State, OutError);
}

bool UGameXXKMVPRules::GetRouteMerchantView(
	FGameXXKRuntimeState& State,
	FGameXXKRouteMerchantView& OutView,
	FString* OutError)
{
	return FGameXXKRouteMerchantRules::GetView(State, OutView, OutError);
}

bool UGameXXKMVPRules::RefreshRouteMerchant(FGameXXKRuntimeState& State, FString* OutError)
{
	return FGameXXKRouteMerchantRules::Refresh(State, OutError);
}

bool UGameXXKMVPRules::PreviewRouteMerchantPurchase(
	const FGameXXKRuntimeState& State,
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchasePreview& OutPreview,
	FString* OutError)
{
	return FGameXXKRouteMerchantRules::PreviewPurchase(
		State,
		OfferId,
		ReplacementEntryId,
		OutPreview,
		OutError);
}

bool UGameXXKMVPRules::PurchaseRouteMerchant(
	FGameXXKRuntimeState& State,
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchaseResult& OutResult)
{
	return FGameXXKRouteMerchantRules::Purchase(State, OfferId, ReplacementEntryId, OutResult);
}

bool UGameXXKMVPRules::CancelPendingRouteMerchantPurchase(FGameXXKRuntimeState& State, FString* OutError)
{
	return FGameXXKRouteMerchantRules::CancelPendingPurchase(State, OutError);
}

bool UGameXXKMVPRules::ResolveMerchantRouteNode(FGameXXKRuntimeState& State)
{
	if (State.bHasGeneratedRouteMap && State.Screen == EGameXXKScreen::RouteMerchant)
	{
		const FGameXXKRouteMapNode* PendingNode = GameXXKMVP::FindPendingRouteNode(State);
		if (!PendingNode || PendingNode->NodeKind != EGameXXKNodeKind::Merchant)
		{
			return false;
		}
		const int32 NodeId = PendingNode->NodeId;
		FGameXXKRuntimeState Candidate = State;
		if (Candidate.CardRun.RouteMerchant.PendingPurchase.bActive
			&& !FGameXXKRouteMerchantRules::CancelPendingPurchase(Candidate))
		{
			return false;
		}
		const FGameXXKRuntimeState BeforeOneTimeRewards = Candidate;
		if (!GameXXKMVP::SettleGeneratedRouteNode(Candidate, BeforeOneTimeRewards, NodeId, 0))
		{
			return false;
		}
		State = MoveTemp(Candidate);
		return true;
	}
	return false;
}

bool UGameXXKMVPRules::FailDungeonToTown(FGameXXKRuntimeState& State)
{
	return GameXXKMVP::ReturnTerminalRouteToTown(State, EGameXXKRouteTerminalOutcome::Defeated);
}

bool UGameXXKMVPRules::AbandonDungeonToTown(FGameXXKRuntimeState& State)
{
	return GameXXKMVP::ReturnTerminalRouteToTown(State, EGameXXKRouteTerminalOutcome::Abandoned);
}

bool UGameXXKMVPRules::ResolveBossClear(FGameXXKRuntimeState& State)
{
	if (!State.bDungeonActive || State.QuestState != EGameXXKQuestState::Accepted)
	{
		return false;
	}
	if (State.CardRun.RouteProgress.SchemaVersion == 1)
	{
		if (!GameXXKMVP::IsValidThreeChapterRouteProgress(State.CardRun.RouteProgress))
		{
			return false;
		}
		if (State.CardRun.RouteProgress.CurrentChapter < 3)
		{
			return GameXXKMVP::AdvanceToNextRouteChapter(State);
		}
	}

	FGameXXKRuntimeState Candidate = State;
	if (!GameXXKMVP::SettleTerminalRoute(Candidate, EGameXXKRouteTerminalOutcome::Cleared))
	{
		return false;
	}
	Candidate.QuestState = EGameXXKQuestState::Completed;
	Candidate.bFollowerJoined = false;
	Candidate.bDungeonActive = false;
	Candidate.DungeonNodeIndex = 0;
	Candidate.Screen = EGameXXKScreen::WorldMap;
	Candidate.CurrentRegion = NAME_None;
	Candidate.CurrentMapId = TEXT("WorldMap");
	Candidate.TownPanelMode = EGameXXKTownPanelMode::None;
	Candidate.UnlockedRegions.Add(RegionTanjiang());
	if (GetItemCount(Candidate, ItemQingshanRouteSeal()) > 0)
	{
		RemoveItem(Candidate, ItemQingshanRouteSeal(), GetItemCount(Candidate, ItemQingshanRouteSeal()));
	}
	GameXXKMVP::ClearActiveBattle(Candidate);
	State = MoveTemp(Candidate);
	return true;
}

TArray<EGameXXKNodeKind> UGameXXKMVPRules::GetFixedDungeonNodes(int32 Seed)
{
	const bool bEventBeforeCamp = (Seed % 2) == 0;
	return bEventBeforeCamp
		? TArray<EGameXXKNodeKind>{EGameXXKNodeKind::Start, EGameXXKNodeKind::Battle, EGameXXKNodeKind::Event, EGameXXKNodeKind::Camp, EGameXXKNodeKind::Boss}
		: TArray<EGameXXKNodeKind>{EGameXXKNodeKind::Start, EGameXXKNodeKind::Battle, EGameXXKNodeKind::Camp, EGameXXKNodeKind::Event, EGameXXKNodeKind::Boss};
}

bool UGameXXKMVPRules::AddItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}
	if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		return false;
	}
	const int64 NewCount = static_cast<int64>(State.Inventory.FindRef(ItemId)) + Quantity;
	if (NewCount > MAX_int32)
	{
		return false;
	}
	State.Inventory.FindOrAdd(ItemId) = static_cast<int32>(NewCount);
	if (ItemId == ItemEnhancementStone())
	{
		GameXXKMVP::SynchronizeEnhancementMaterial(State);
	}
	return true;
}

bool UGameXXKMVPRules::RemoveItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	if (Quantity <= 0 || FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		return false;
	}
	int32* Count = State.Inventory.Find(ItemId);
	if (!Count || *Count < Quantity)
	{
		return false;
	}
	*Count -= Quantity;
	if (*Count <= 0)
	{
		State.Inventory.Remove(ItemId);
	}
	if (ItemId == ItemEnhancementStone())
	{
		GameXXKMVP::SynchronizeEnhancementMaterial(State);
	}
	return true;
}

int32 UGameXXKMVPRules::GetItemCount(const FGameXXKRuntimeState& State, FName ItemId)
{
	if (const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		return Definition->Set == EGameXXKEquipmentSet::Legacy
			? FGameXXKEquipmentEconomyRules::CountLegacyEquipmentInstances(State, ItemId)
			: 0;
	}
	return State.Inventory.FindRef(ItemId);
}

bool UGameXXKMVPRules::BuyItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !GameXXKMVP::GetShopItemIds().Contains(ItemId) || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}
	const int64 Total = static_cast<int64>(Def.BuyPrice) * Quantity;
	if (Total < 0 || Total > State.PlayerGold)
	{
		return false;
	}
	if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		FGameXXKRuntimeState Candidate = State;
		FGameXXKEquipmentTransactionResult Result;
		for (int32 CopyIndex = 0; CopyIndex < Quantity; ++CopyIndex)
		{
			if (!FGameXXKEquipmentEconomyRules::PurchaseLegacyEquipmentForCompatibility(Candidate, ItemId, Result))
			{
				return false;
			}
		}
		State = MoveTemp(Candidate);
		return true;
	}
	FGameXXKRuntimeState Candidate = State;
	Candidate.PlayerGold -= static_cast<int32>(Total);
	if (!AddItem(Candidate, ItemId, Quantity))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPRules::SellItem(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity)
{
	FGameXXKItemDef Def;
	if (Quantity <= 0 || !CanSellItem(State, ItemId) || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}
	if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		FGameXXKEquipmentTransactionResult Result;
		return FGameXXKEquipmentEconomyRules::SellLegacyEquipmentForCompatibility(State, ItemId, Quantity, Result);
	}
	const int64 GoldDelta = static_cast<int64>(Def.SellPrice) * Quantity;
	if (GoldDelta < 0 || static_cast<int64>(State.PlayerGold) + GoldDelta > MAX_int32
		|| !RemoveItem(State, ItemId, Quantity))
	{
		return false;
	}
	State.PlayerGold += static_cast<int32>(GoldDelta);
	return true;
}

bool UGameXXKMVPRules::CanSellItem(const FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		return !FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, false).IsNone();
	}
	return !ItemId.IsNone()
		&& GetItemCount(State, ItemId) > 0
		&& GameXXKMVP::GetItemDef(ItemId, Def)
		&& Def.Kind != EGameXXKItemKind::Task;
}

int32 UGameXXKMVPRules::GetMaxItemEnhancementLevel()
{
	return GameXXKMVP::MaxItemEnhancementLevel;
}

int32 UGameXXKMVPRules::GetItemEnhancementLevel(const FGameXXKRuntimeState& State, FName ItemId)
{
	if (FGameXXKEquipmentCatalog::FindDefinition(ItemId))
	{
		const FName InstanceId = FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, true);
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, InstanceId);
		return Instance ? Instance->EnhancementLevel : 0;
	}
	return GameXXKMVP::GetClampedItemEnhancementLevel(State, ItemId);
}

bool UGameXXKMVPRules::CanEnhanceItem(const FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (ItemId.IsNone()
		|| GetItemCount(State, ItemEnhancementStone()) <= 0
		|| !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}

	const bool bIsEquipment = Def.Kind == EGameXXKItemKind::Weapon
		|| Def.Kind == EGameXXKItemKind::Armor
		|| Def.Kind == EGameXXKItemKind::Accessory;
	return bIsEquipment
		&& !FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, true).IsNone()
		&& GetItemEnhancementLevel(State, ItemId) < GetMaxItemEnhancementLevel();
}

bool UGameXXKMVPRules::EnhanceItem(FGameXXKRuntimeState& State, FName ItemId)
{
	if (!CanEnhanceItem(State, ItemId))
	{
		return false;
	}

	const FName InstanceId = FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, true);
	FGameXXKEquipmentTransactionResult Result;
	return FGameXXKEquipmentEconomyRules::EnhanceInstance(State, InstanceId, Result);
}

bool UGameXXKMVPRules::CanDecomposeItem(const FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (ItemId.IsNone() || !GameXXKMVP::GetItemDef(ItemId, Def))
	{
		return false;
	}

	return !FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, true).IsNone()
		&& (Def.Kind == EGameXXKItemKind::Weapon
		|| Def.Kind == EGameXXKItemKind::Armor
		|| Def.Kind == EGameXXKItemKind::Accessory);
}

bool UGameXXKMVPRules::DecomposeItem(FGameXXKRuntimeState& State, FName ItemId)
{
	if (!CanDecomposeItem(State, ItemId))
	{
		return false;
	}
	const FName InstanceId = FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, true);
	FGameXXKEquipmentTransactionResult Result;
	return FGameXXKEquipmentEconomyRules::DismantleBatch(State, {InstanceId}, true, Result);
}

bool UGameXXKMVPRules::EquipItem(FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	const FGameXXKEquipmentDefinition* EquipmentDefinition = FGameXXKEquipmentCatalog::FindDefinition(ItemId);
	if (!GameXXKMVP::GetItemDef(ItemId, Def) || !EquipmentDefinition)
	{
		return false;
	}

	if (const FGameXXKEquipmentLoadout* HeroLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId()))
	{
		FName EquippedInstanceId = NAME_None;
		switch (EquipmentDefinition->Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: EquippedInstanceId = HeroLoadout->WeaponInstanceId; break;
		case EGameXXKEquipmentSlot::Head: EquippedInstanceId = HeroLoadout->HeadInstanceId; break;
		case EGameXXKEquipmentSlot::Armor: EquippedInstanceId = HeroLoadout->ArmorInstanceId; break;
		case EGameXXKEquipmentSlot::Belt: EquippedInstanceId = HeroLoadout->BeltInstanceId; break;
		case EGameXXKEquipmentSlot::Shoes: EquippedInstanceId = HeroLoadout->ShoesInstanceId; break;
		case EGameXXKEquipmentSlot::Accessory: EquippedInstanceId = HeroLoadout->AccessoryInstanceId; break;
		default: break;
		}

		const FGameXXKEquipmentInstance* EquippedInstance =
			FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, EquippedInstanceId);
		if (EquippedInstance && EquippedInstance->BaseEquipmentId == ItemId)
		{
			// The successful equip already synchronized every compatibility mirror.
			// Repeating the same legacy facade request is a strict no-op, but it must
			// still reject collection corruption that the core equip path would reject.
			return FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				State.EquipmentCollection,
				State.CardRun.CompanionRoster);
		}
	}

	const FName InstanceId = FGameXXKEquipmentEconomyRules::FindLegacyInstanceForCompatibility(State, ItemId, false);
	if (InstanceId.IsNone())
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult Result;
	return FGameXXKEquipmentEconomyRules::Equip(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EquipmentDefinition->Slot,
		InstanceId,
		Result);
}

bool UGameXXKMVPRules::UnequipItem(FGameXXKRuntimeState& State, FName ItemId)
{
	const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(ItemId);
	const FGameXXKEquipmentLoadout* HeroLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(FGameXXKEquipmentRules::HeroCharacterId());
	if (!Definition || !HeroLoadout)
	{
		return false;
	}
	FName EquippedInstanceId = NAME_None;
	switch (Definition->Slot)
	{
	case EGameXXKEquipmentSlot::Weapon: EquippedInstanceId = HeroLoadout->WeaponInstanceId; break;
	case EGameXXKEquipmentSlot::Armor: EquippedInstanceId = HeroLoadout->ArmorInstanceId; break;
	case EGameXXKEquipmentSlot::Accessory: EquippedInstanceId = HeroLoadout->AccessoryInstanceId; break;
	default: return false;
	}
	const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, EquippedInstanceId);
	if (!Instance || Instance->BaseEquipmentId != ItemId)
	{
		return false;
	}
	FGameXXKEquipmentTransactionResult Result;
	return FGameXXKEquipmentEconomyRules::Unequip(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		Definition->Slot,
		Result);
}

bool UGameXXKMVPRules::UseHealingItem(FGameXXKRuntimeState& State)
{
	return UseItem(State, ItemHealingPowder());
}

bool UGameXXKMVPRules::UseItem(FGameXXKRuntimeState& State, FName ItemId)
{
	FGameXXKItemDef Def;
	if (!GameXXKMVP::GetItemDef(ItemId, Def) || Def.Kind != EGameXXKItemKind::Consumable)
	{
		return false;
	}
	const bool bCanHealHP = Def.HealAmount > 0 && State.PlayerHP < State.PlayerMaxHP;
	const bool bCanHealMP = Def.MPHealAmount > 0 && State.PlayerMP < State.PlayerMaxMP;
	if (!bCanHealHP && !bCanHealMP)
	{
		return false;
	}
	if (!RemoveItem(State, ItemId, 1))
	{
		return false;
	}
	State.PlayerHP = FMath::Min(State.PlayerMaxHP, State.PlayerHP + Def.HealAmount);
	State.PlayerMP = FMath::Min(State.PlayerMaxMP, State.PlayerMP + Def.MPHealAmount);
	return true;
}

bool UGameXXKMVPRules::OpenTownPanel(FGameXXKRuntimeState& State, EGameXXKTownPanelMode PanelMode)
{
	if (State.Screen != EGameXXKScreen::Town)
	{
		return false;
	}
	State.TownPanelMode = PanelMode;
	return true;
}

bool UGameXXKMVPRules::CloseTownPanel(FGameXXKRuntimeState& State)
{
	State.TownPanelMode = EGameXXKTownPanelMode::None;
	return true;
}

void UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(FGameXXKRuntimeState& State)
{
	FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State);
}

TArray<FName> UGameXXKMVPRules::BuildTurnOrder(const FGameXXKRuntimeState& State, bool bBossBattle)
{
	if (State.bHasActiveBattle)
	{
		TArray<FGameXXKBattleRuntimeUnit> Units = State.ActiveBattleParty;
		Units.Append(State.ActiveBattleEnemies);
		Units.Sort([](const FGameXXKBattleRuntimeUnit& A, const FGameXXKBattleRuntimeUnit& B)
		{
			if (A.Speed == B.Speed)
			{
				return A.Id.LexicalLess(B.Id);
			}
			return A.Speed > B.Speed;
		});

		TArray<FName> Order;
		for (const FGameXXKBattleRuntimeUnit& Unit : Units)
		{
			if (!Unit.bDefeated)
			{
				Order.Add(Unit.Id);
			}
		}
		return Order;
	}

	TArray<FGameXXKBattleUnit> Units;
	Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Player"), State.PlayerHP, State.PlayerAttack, State.PlayerDefense, State.PlayerSpeed, TEXT("Sword"), 1));
	if (!State.CardRun.ActiveTemporaryQuestNpcId.IsNone())
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(
			State.CardRun.ActiveTemporaryQuestNpcId,
			State.PlayerHP,
			State.PlayerAttack,
			State.PlayerDefense,
			State.PlayerSpeed,
			TEXT("Sword"),
			1));
	}
	if (bBossBattle)
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Boss"), 180, 18, 8, 9, TEXT("Sword"), 3));
	}
	else
	{
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Bandit"), 60, 10, 3, 7, TEXT("Sword"), 1));
		Units.Add(GameXXKMVP::MakeBattleUnit(TEXT("Wolf"), 45, 8, 2, 12, TEXT("Bow"), 1));
	}

	Units.Sort([](const FGameXXKBattleUnit& A, const FGameXXKBattleUnit& B)
	{
		if (A.Speed == B.Speed)
		{
			return A.Id.LexicalLess(B.Id);
		}
		return A.Speed > B.Speed;
	});

	TArray<FName> Order;
	for (const FGameXXKBattleUnit& Unit : Units)
	{
		Order.Add(Unit.Id);
	}
	return Order;
}

FGameXXKSaveState UGameXXKMVPRules::MakeSaveState(const FGameXXKRuntimeState& State)
{
	// Save snapshots are a persistence boundary. Runtime mutations (tests,
	// equipment transactions, or UI fixtures) may have changed authoritative
	// inventory maps without rebuilding their physical desktop cells yet. Take
	// a normalized copy so a current-version save can pass the same validation
	// that a load will apply, without mutating the live runtime state.
	FGameXXKRuntimeState Snapshot = State;
	FString DesktopInventoryError;
	FGameXXKDesktopInventoryRules::Normalize(Snapshot, &DesktopInventoryError);

	FGameXXKSaveState SaveState;
	SaveState.SaveVersion = GameXXKMVP::CurrentSaveVersion;
	SaveState.RuntimeState = Snapshot;
	SaveState.bHasPlayerLocation = Snapshot.bHasPlayerLocation;
	SaveState.PlayerLocation = Snapshot.PlayerLocation;
	SaveState.QuestState = Snapshot.QuestState;
	SaveState.bFollowerJoined = Snapshot.bFollowerJoined;
	SaveState.bHasQuestNpcLocation = Snapshot.bHasQuestNpcLocation;
	SaveState.QuestNpcLocation = Snapshot.QuestNpcLocation;
	SaveState.PlayerLevel = Snapshot.PlayerLevel;
	SaveState.PlayerXP = Snapshot.PlayerXP;
	SaveState.PlayerGold = Snapshot.PlayerGold;
	SaveState.UnlockedRegions = Snapshot.UnlockedRegions;
	return SaveState;
}

FGameXXKRuntimeState UGameXXKMVPRules::RestoreFromSaveState(const FGameXXKSaveState& SaveState)
{
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	return FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, Report)
		? Restored
		: FGameXXKRuntimeState();
}
