#include "MVP/GameXXKSaveMigration.h"

#include "GameXXKCardRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKTalentRules.h"
#include "GameXXKTrainingRules.h"
#include "Guide/GameXXKGuideTargetRegistry.h"
#include "Misc/Crc.h"
#include "Narrative/GameXXKStoryCatalog.h"

namespace
{
	const FName GuideId(TEXT("Codex.Guide"));
	const FName MoneyRatId(TEXT("Codex.Enemy.Ch1.MoneyRat"));
	const FName BlackBearId(TEXT("Codex.Enemy.Ch2.BlackBear"));
	const FName TigerId(TEXT("Codex.Enemy.Ch3.Tiger"));
	const FName PreviousMoneyRatId(TEXT("Codex.MoneyRat"));
	const FName PreviousBlackBearId(TEXT("Codex.BlackBear"));
	const FName PreviousTigerId(TEXT("Codex.Tiger"));
	const FName LegacyBanditId(TEXT("Codex.Bandit"));
	const FName LegacyWolfId(TEXT("Codex.Wolf"));
	const FName LegacyEliteBanditId(TEXT("Codex.EliteBandit"));
	const FName LegacyBossId(TEXT("Codex.Boss"));
	constexpr int32 GuideIntroductionVersion = 5;
	constexpr int32 StableMigrationCollectionSeed = 0x4758584B;
	constexpr uint32 HeroCombatRandomSalt = 0xA341316CU;

	const TArray<TPair<FName, FName>>& LegacyHeroCardPairs()
	{
		static const TArray<TPair<FName, FName>> Pairs = {
			{TEXT("Hero.QingFengYiShi"), TEXT("Hero.Generic.QingFengYiShi")},
			{TEXT("Hero.HeYuZhan"), TEXT("Hero.Generic.HeYuZhan")},
			{TEXT("Hero.FengShenBu"), TEXT("Hero.Generic.FengShenBu")},
			{TEXT("Hero.SuiYanJi"), TEXT("Hero.Generic.SuiYanJi")},
			{TEXT("Hero.GuiYuanShu"), TEXT("Hero.Generic.GuiYuanShu")},
			{TEXT("Hero.HengJianShouShi"), TEXT("Hero.Generic.HengJianShouShi")},
			{TEXT("Hero.NingShenTuNa"), TEXT("Hero.Generic.NingShenTuNa")},
			{TEXT("Hero.GuanXi"), TEXT("Hero.Generic.GuanXi")},
			{TEXT("Hero.PoYunYiShan"), TEXT("Hero.Generic.PoYunYiShan")},
			{TEXT("Hero.HuiFengZhuiJian"), TEXT("Hero.Generic.XingQiHuiHuan")},
			{TEXT("Hero.JianYiGuanHong"), TEXT("Hero.Generic.JianYiGuanHong")},
			{TEXT("Hero.GuiYuanFanZhao"), TEXT("Hero.Generic.GuiYuanFanZhao")}
		};
		return Pairs;
	}

	const TArray<TPair<FName, FName>>& LegacyBladePartnerCardPairs()
	{
		static const TArray<TPair<FName, FName>> Pairs = {
			{TEXT("Profession.Blade.YiShangHuanShi"), TEXT("Profession.Blade.JingHongChuQiao")},
			{TEXT("Profession.Blade.DaoYiShouShu"), TEXT("Profession.Blade.HengYunKaiFeng")},
			{TEXT("Profession.Blade.XiaoJiaLianJi"), TEXT("Profession.Blade.LianXiGuiQiao")},
			{TEXT("Profession.Blade.CanYueSanDie"), TEXT("Profession.Blade.BaoDaoShouYe")}
		};
		return Pairs;
	}

	bool MigrateHeroCardId(FName& InOutCardId)
	{
		for (const TPair<FName, FName>& Pair : LegacyHeroCardPairs())
		{
			if (InOutCardId == Pair.Key)
			{
				InOutCardId = Pair.Value;
				return true;
			}
		}
		return false;
	}

	void MigrateHeroCardIds(TArray<FName>& InOutCardIds)
	{
		for (FName& CardId : InOutCardIds)
		{
			MigrateHeroCardId(CardId);
		}
	}

	void MigrateHeroCardInstance(FGameXXKCardInstance& InOutInstance)
	{
		if (MigrateHeroCardId(InOutInstance.CardId))
		{
			InOutInstance.CurrentQuality = EGameXXKCardQuality::Common;
		}
	}

	void MigrateHeroCardInstances(TArray<FGameXXKCardInstance>& InOutInstances)
	{
		for (FGameXXKCardInstance& Instance : InOutInstances)
		{
			MigrateHeroCardInstance(Instance);
		}
	}

	void MigrateHeroCardSnapshot(FGameXXKResolvedCardSnapshot& InOutSnapshot)
	{
		if (MigrateHeroCardId(InOutSnapshot.CardId))
		{
			InOutSnapshot.Quality = EGameXXKCardQuality::Common;
		}
	}

	bool MigrateBladePartnerCardId(FName& InOutCardId)
	{
		for (const TPair<FName, FName>& Pair : LegacyBladePartnerCardPairs())
		{
			if (InOutCardId == Pair.Key)
			{
				InOutCardId = Pair.Value;
				return true;
			}
		}
		return false;
	}

	void MigrateBladePartnerCardIds(TArray<FName>& InOutCardIds, int32& InOutMigratedReferenceCount)
	{
		for (FName& CardId : InOutCardIds)
		{
			InOutMigratedReferenceCount += MigrateBladePartnerCardId(CardId) ? 1 : 0;
		}
	}

	void MigrateBladePartnerCardInstances(
		TArray<FGameXXKCardInstance>& InOutInstances,
		int32& InOutMigratedReferenceCount)
	{
		for (FGameXXKCardInstance& Instance : InOutInstances)
		{
			InOutMigratedReferenceCount += MigrateBladePartnerCardId(Instance.CardId) ? 1 : 0;
		}
	}

	void MigrateBladePartnerCardSnapshot(
		FGameXXKResolvedCardSnapshot& InOutSnapshot,
		int32& InOutMigratedReferenceCount)
	{
		InOutMigratedReferenceCount += MigrateBladePartnerCardId(InOutSnapshot.CardId) ? 1 : 0;
	}

	void MigrateBladePartnerProfile(
		FGameXXKPermanentCompanion& InOutProfile,
		int32& InOutMigratedReferenceCount)
	{
		MigrateBladePartnerCardIds(InOutProfile.PersonalCardIds, InOutMigratedReferenceCount);
		MigrateBladePartnerCardIds(InOutProfile.UnlockedPersonalCardIds, InOutMigratedReferenceCount);
		MigrateBladePartnerCardIds(InOutProfile.SelectedCardIds, InOutMigratedReferenceCount);
	}

	void MigrateBladePartnerBattle(
		FGameXXKCardBattleRuntime& InOutBattle,
		int32& InOutMigratedReferenceCount)
	{
		MigrateBladePartnerCardInstances(InOutBattle.Deck.DrawPile, InOutMigratedReferenceCount);
		MigrateBladePartnerCardInstances(InOutBattle.Deck.Hand, InOutMigratedReferenceCount);
		MigrateBladePartnerCardInstances(InOutBattle.Deck.DiscardPile, InOutMigratedReferenceCount);
		MigrateBladePartnerCardInstances(InOutBattle.Deck.ExhaustPile, InOutMigratedReferenceCount);
		MigrateBladePartnerCardInstances(InOutBattle.Deck.PendingChoice.Candidates, InOutMigratedReferenceCount);
		MigrateBladePartnerCardSnapshot(InOutBattle.LastActiveCard, InOutMigratedReferenceCount);
		for (FGameXXKCardBattleModifierRuntime& Modifier : InOutBattle.Modifiers)
		{
			MigrateBladePartnerCardSnapshot(Modifier.SourceCardSnapshot, InOutMigratedReferenceCount);
		}
		for (FGameXXKResolvedCardSnapshot& Snapshot : InOutBattle.AutomaticResolutionQueue.PendingCards)
		{
			MigrateBladePartnerCardSnapshot(Snapshot, InOutMigratedReferenceCount);
		}
	}

	void MigrateBladePartnerCards(
		FGameXXKRuntimeState& InOutState,
		FGameXXKSaveMigrationReport& Report)
	{
		int32 MigratedReferenceCount = 0;
		FGameXXKCardRunState& Run = InOutState.CardRun;
		for (FGameXXKPermanentCompanion& Companion : Run.CompanionRoster.PermanentCompanions)
		{
			MigrateBladePartnerProfile(Companion, MigratedReferenceCount);
		}
		if (Run.CompanionRoster.PendingRecruitment.bHasPendingRecruitment)
		{
			MigrateBladePartnerProfile(Run.CompanionRoster.PendingRecruitment.Candidate, MigratedReferenceCount);
		}
		MigrateBladePartnerCardIds(Run.PendingReward.CardIds, MigratedReferenceCount);
		for (FGameXXKCardEnemyIntent& Intent : Run.EnemyIntents)
		{
			MigratedReferenceCount += MigrateBladePartnerCardId(Intent.CardId) ? 1 : 0;
		}
		for (FGameXXKRouteMerchantOffer& Offer : Run.RouteMerchant.Offers)
		{
			if (Offer.Kind == EGameXXKRouteMerchantOfferKind::Card)
			{
				MigratedReferenceCount += MigrateBladePartnerCardId(Offer.ContentId) ? 1 : 0;
			}
		}
		if (Run.RouteMerchant.PendingPurchase.bActive)
		{
			MigratedReferenceCount += MigrateBladePartnerCardId(Run.RouteMerchant.PendingPurchase.CardId) ? 1 : 0;
		}
		MigrateBladePartnerBattle(Run.ActiveBattle, MigratedReferenceCount);

		if (MigratedReferenceCount > 0)
		{
			Report.Warnings.Add(FString::Printf(
				TEXT("Migrated %d retired Blade partner card references to their replacement IDs."),
				MigratedReferenceCount));
		}
	}

	void ConvertLegacyMedicine(FGameXXKCardBattleRuntime& InOutBattle)
	{
		for (FGameXXKCardCombatUnit& Unit : InOutBattle.Units)
		{
			int64 LegacyMedicine = 0;
			int64 ExistingHealingBonus = 0;
			for (const FGameXXKCardStatusStack& Stack : Unit.Statuses)
			{
				if (Stack.Status == EGameXXKCardStatus::Medicine && Stack.Stacks > 0)
				{
					LegacyMedicine = FMath::Min<int64>(MAX_int32, LegacyMedicine + Stack.Stacks);
				}
				else if (Stack.Status == EGameXXKCardStatus::NextHealingBonus && Stack.Stacks > 0)
				{
					ExistingHealingBonus = FMath::Min<int64>(99, ExistingHealingBonus + Stack.Stacks);
				}
			}
			Unit.Statuses.RemoveAll([](const FGameXXKCardStatusStack& Stack)
			{
				return Stack.Status == EGameXXKCardStatus::Medicine
					|| Stack.Status == EGameXXKCardStatus::NextHealingBonus;
			});
			const int32 MigratedHealingBonus = static_cast<int32>(FMath::Min<int64>(
				99,
				ExistingHealingBonus + LegacyMedicine * 6));
			if (MigratedHealingBonus > 0)
			{
				FGameXXKCardStatusStack& Stack = Unit.Statuses.AddDefaulted_GetRef();
				Stack.Status = EGameXXKCardStatus::NextHealingBonus;
				Stack.Stacks = MigratedHealingBonus;
			}
		}
	}

	void MigrateHeroCardBattle(FGameXXKCardBattleRuntime& InOutBattle, const bool bConvertLegacyMedicine)
	{
		MigrateHeroCardIds(InOutBattle.EquippedHeroCardIds);
		MigrateHeroCardInstances(InOutBattle.Deck.DrawPile);
		MigrateHeroCardInstances(InOutBattle.Deck.Hand);
		MigrateHeroCardInstances(InOutBattle.Deck.DiscardPile);
		MigrateHeroCardInstances(InOutBattle.Deck.ExhaustPile);
		MigrateHeroCardInstances(InOutBattle.Deck.PendingChoice.Candidates);
		MigrateHeroCardSnapshot(InOutBattle.LastActiveCard);
		for (FGameXXKCardBattleModifierRuntime& Modifier : InOutBattle.Modifiers)
		{
			MigrateHeroCardSnapshot(Modifier.SourceCardSnapshot);
		}
		for (FGameXXKResolvedCardSnapshot& Snapshot : InOutBattle.AutomaticResolutionQueue.PendingCards)
		{
			MigrateHeroCardSnapshot(Snapshot);
		}
		if (InOutBattle.HeroSpellTask.bActive)
		{
			MigrateHeroCardIds(InOutBattle.HeroSpellTask.LockedHeroCardIds);
			MigrateHeroCardIds(InOutBattle.HeroSpellTask.CompletedHeroCardIds);
			for (FGameXXKResolvedCardSnapshot& Snapshot : InOutBattle.HeroSpellTask.FirstPlayOrder)
			{
				MigrateHeroCardSnapshot(Snapshot);
			}
		}
		else
		{
			// Spell-task progress did not exist before v12. Discard any inactive stale
			// payload instead of turning it into an invalid current-version runtime.
			InOutBattle.HeroSpellTask = FGameXXKHeroSpellTaskRuntime{};
		}
		if (bConvertLegacyMedicine)
		{
			ConvertLegacyMedicine(InOutBattle);
			uint32 CombatSeed = static_cast<uint32>(InOutBattle.Deck.CurrentRandomState) ^ HeroCombatRandomSalt;
			if (CombatSeed == 0)
			{
				CombatSeed = HeroCombatRandomSalt;
			}
			InOutBattle.CombatRandomState = static_cast<int32>(CombatSeed);
		}
	}

	bool MigrateHeroCardPool(FGameXXKRuntimeState& InOutState, FString& OutError)
	{
		FGameXXKCardRunState& Run = InOutState.CardRun;
		MigrateHeroCardIds(Run.HeroUnlockedCardIds);
		MigrateHeroCardIds(Run.HeroSelectedCardIds);
		MigrateHeroCardIds(Run.PendingReward.CardIds);
		for (FGameXXKCardEnemyIntent& Intent : Run.EnemyIntents)
		{
			MigrateHeroCardId(Intent.CardId);
		}
		MigrateHeroCardBattle(Run.ActiveBattle, Run.bHasActiveCardBattle);
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(InOutState, &OutError);
	}

	bool ValidateLegacyCompanionCardState(
		const FGameXXKPermanentCompanion& Companion,
		FString& OutError)
	{
		if (Companion.PersonalCardIds.Num() != 6
			&& Companion.PersonalCardIds.Num() != 12
			&& Companion.PersonalCardIds.Num() != 18)
		{
			OutError = TEXT("A companion migration source must retain six, twelve, or eighteen profession cards.");
			return false;
		}

		TSet<FName> SeenPersonalCardIds;
		for (const FName CardId : Companion.PersonalCardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (CardId.IsNone()
				|| SeenPersonalCardIds.Contains(CardId)
				|| !Definition
				|| Definition->Owner != EGameXXKCardOwner::Profession
				|| Definition->Role != Companion.Role)
			{
				OutError = TEXT("A pre-v13 companion personal pool contains an unknown, duplicate, or wrong-role card.");
				return false;
			}
			SeenPersonalCardIds.Add(CardId);
		}

		if (Companion.UnlockedPersonalCardIds.Num() < 5
			|| Companion.UnlockedPersonalCardIds.Num() > Companion.PersonalCardIds.Num())
		{
			OutError = TEXT("A pre-v13 companion unlock frontier has an invalid size.");
			return false;
		}
		TSet<FName> SeenUnlockedCardIds;
		for (const FName CardId : Companion.UnlockedPersonalCardIds)
		{
			if (!SeenPersonalCardIds.Contains(CardId) || SeenUnlockedCardIds.Contains(CardId))
			{
				OutError = TEXT("A pre-v13 companion unlock frontier is not a unique subset of its personal pool.");
				return false;
			}
			SeenUnlockedCardIds.Add(CardId);
		}

		if (Companion.SelectedCardIds.Num() != 5)
		{
			OutError = TEXT("A pre-v13 companion must retain exactly five selected cards.");
			return false;
		}
		TSet<FName> SeenSelectedCardIds;
		for (const FName CardId : Companion.SelectedCardIds)
		{
			if (!SeenUnlockedCardIds.Contains(CardId) || SeenSelectedCardIds.Contains(CardId))
			{
				OutError = TEXT("A pre-v13 companion selection is not a unique subset of its unlocked cards.");
				return false;
			}
			SeenSelectedCardIds.Add(CardId);
		}
		return true;
	}

	bool MigrateCompanionBirthPoolProfile(
		FGameXXKPermanentCompanion& InOutCompanion,
		FGameXXKSaveMigrationReport& Report,
		FString& OutError)
	{
		if (!ValidateLegacyCompanionCardState(InOutCompanion, OutError))
		{
			return false;
		}

		TArray<FName> NewFullPool;
		if (!FGameXXKCompanionRules::BuildFullProfessionCardPool(
			InOutCompanion.Role,
			InOutCompanion.CardSeed,
			NewFullPool,
			&OutError))
		{
			return false;
		}

		const TArray<FName> PreviousPersonalCards = InOutCompanion.PersonalCardIds;
		const TArray<FName> PreviousUnlockedCards = InOutCompanion.UnlockedPersonalCardIds;
		const TArray<FName> PreviousSelection = InOutCompanion.SelectedCardIds;
		InOutCompanion.PersonalCardIds = NewFullPool;
		if (!FGameXXKCompanionRules::RefreshUnlockedPersonalCards(InOutCompanion, &OutError))
		{
			return false;
		}

		// Old twelve-card saves may have selected profession cards that now sit
		// beyond the level-gated frontier. Preserve every still-unlocked choice in
		// player order, then fill only from the current unlocked prefix. Current
		// valid saves therefore remain byte-stable while legacy saves cannot retain
		// a selected card that the UI correctly presents as locked.
		TArray<FName> NewSelection;
		for (const FName CardId : PreviousSelection)
		{
			if (InOutCompanion.UnlockedPersonalCardIds.Contains(CardId)
				&& !NewSelection.Contains(CardId))
			{
				NewSelection.Add(CardId);
			}
		}
		for (const FName CardId : InOutCompanion.UnlockedPersonalCardIds)
		{
			if (NewSelection.Num() >= 5)
			{
				break;
			}
			if (!NewSelection.Contains(CardId))
			{
				NewSelection.Add(CardId);
			}
		}
		InOutCompanion.SelectedCardIds = MoveTemp(NewSelection);
		const bool bChanged = PreviousPersonalCards != InOutCompanion.PersonalCardIds
			|| PreviousUnlockedCards != InOutCompanion.UnlockedPersonalCardIds
			|| PreviousSelection != InOutCompanion.SelectedCardIds;
		if (!FGameXXKCompanionRules::ValidatePermanentCompanionProfile(InOutCompanion, &OutError))
		{
			return false;
		}
		if (bChanged)
		{
			Report.Warnings.Add(FString::Printf(
				TEXT("Companion %s was migrated to its deterministic eighteen-card profession pool."),
				*InOutCompanion.InstanceId.ToString()));
		}
		return true;
	}

	bool MigrateCompanionBirthPools(
		FGameXXKRuntimeState& InOutState,
		FGameXXKSaveMigrationReport& Report,
		FString& OutError)
	{
		FGameXXKCompanionRosterState& Roster = InOutState.CardRun.CompanionRoster;
		for (FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
		{
			if (!MigrateCompanionBirthPoolProfile(Companion, Report, OutError))
			{
				return false;
			}
		}
		if (Roster.PendingRecruitment.bHasPendingRecruitment
			&& !MigrateCompanionBirthPoolProfile(Roster.PendingRecruitment.Candidate, Report, OutError))
		{
			return false;
		}
		return true;
	}

	void Fail(FGameXXKSaveMigrationReport& Report, const FString& Error)
	{
		Report.bSucceeded = false;
		Report.Error = Error;
	}

	void MigrateLegacyCodexIds(TSet<FName>& EntryIds)
	{
		if (EntryIds.Contains(PreviousMoneyRatId)
			|| EntryIds.Contains(LegacyBanditId)
			|| EntryIds.Contains(LegacyWolfId))
		{
			EntryIds.Add(MoneyRatId);
		}
		if (EntryIds.Contains(PreviousBlackBearId) || EntryIds.Contains(LegacyEliteBanditId))
		{
			EntryIds.Add(BlackBearId);
		}
		if (EntryIds.Contains(PreviousTigerId) || EntryIds.Contains(LegacyBossId))
		{
			EntryIds.Add(TigerId);
		}
		EntryIds.Remove(PreviousMoneyRatId);
		EntryIds.Remove(PreviousBlackBearId);
		EntryIds.Remove(PreviousTigerId);
		EntryIds.Remove(LegacyBanditId);
		EntryIds.Remove(LegacyWolfId);
		EntryIds.Remove(LegacyEliteBanditId);
		EntryIds.Remove(LegacyBossId);
	}

	void MigrateRefinementSandMirror(FGameXXKRuntimeState& State)
	{
		const FName SandId = UGameXXKMVPRules::ItemRefinementSand();
		if (!State.Inventory.Contains(SandId) && State.EquipmentCollection.RefinementSand > 0)
		{
			State.Inventory.Add(SandId, State.EquipmentCollection.RefinementSand);
		}
		State.EquipmentCollection.RefinementSand = FMath::Max(0, State.Inventory.FindRef(SandId));
	}

	bool MigrateLegacyTrainingChestStacks(FGameXXKRuntimeState& State, FString& OutError)
	{
		const FName NormalId = UGameXXKMVPRules::ItemTrainingNormalChest();
		const FName AdvancedId = UGameXXKMVPRules::ItemTrainingAdvancedChest();
		const int64 NormalCount = static_cast<int64>(State.Inventory.FindRef(NormalId))
			+ State.DesktopInventory.WarehouseItems.FindRef(NormalId);
		const int64 AdvancedCount = static_cast<int64>(State.Inventory.FindRef(AdvancedId))
			+ State.DesktopInventory.WarehouseItems.FindRef(AdvancedId);
		if (NormalCount < 0 || AdvancedCount < 0 || NormalCount > MAX_int32 || AdvancedCount > MAX_int32)
		{
			OutError = TEXT("Legacy Training chest count is invalid.");
			return false;
		}
		FName StageId = State.Training.CurrentTravelStageId;
		if (StageId.IsNone()) StageId = State.Training.SelectedStageId;
		if (StageId.IsNone()) StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
		for (int32 Index = 0; Index < static_cast<int32>(NormalCount); ++Index)
			if (!FGameXXKTrainingRules::AppendChestToken(State.Training, EGameXXKTrainingRewardTier::NormalChest, StageId, State.PlayerLevel, &OutError)) return false;
		for (int32 Index = 0; Index < static_cast<int32>(AdvancedCount); ++Index)
			if (!FGameXXKTrainingRules::AppendChestToken(State.Training, EGameXXKTrainingRewardTier::AdvancedChest, StageId, State.PlayerLevel, &OutError)) return false;
		State.Inventory.Remove(NormalId);
		State.Inventory.Remove(AdvancedId);
		State.DesktopInventory.WarehouseItems.Remove(NormalId);
		State.DesktopInventory.WarehouseItems.Remove(AdvancedId);
		State.DesktopInventory.LockedItemIds.Remove(NormalId);
		State.DesktopInventory.LockedItemIds.Remove(AdvancedId);
		return FGameXXKDesktopInventoryRules::Normalize(State, &OutError);
	}

	bool MigratePermanentNpcFormation(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report,
		FString& OutError)
	{
		OutError.Reset();
		FName OrderedNpcId;
		const bool bHasOrderedNpc =
			FGameXXKPartyFormationRules::ResolveQuestNpcId(State, OrderedNpcId);
		const FName SelectionNpcId = State.CardRun.PartySelection.QuestNpc.NpcId;
		const FName TemporaryNpcId = State.CardRun.ActiveTemporaryQuestNpcId;
		FName RecoveredNpcId = bHasOrderedNpc ? OrderedNpcId : NAME_None;
		if (RecoveredNpcId.IsNone()
			&& FGameXXKCompanionCatalog::FindQuestNpcDefinition(SelectionNpcId))
		{
			RecoveredNpcId = SelectionNpcId;
		}
		if (RecoveredNpcId.IsNone()
			&& FGameXXKCompanionCatalog::FindQuestNpcDefinition(TemporaryNpcId))
		{
			RecoveredNpcId = TemporaryNpcId;
		}
		if (RecoveredNpcId.IsNone())
		{
			RecoveredNpcId = TEXT("Npc.TusiChief");
			Report.Warnings.Add(TEXT("Missing legacy NPC formation repaired to Tusi Chief."));
		}

		State.CardRun.ActiveTemporaryQuestNpcId = NAME_None;
		const FGameXXKQuestNpcOwnedCardLoadout* RecoveredLoadout =
			State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(RecoveredNpcId);
		if (!RecoveredLoadout)
		{
			OutError = TEXT("Recovered NPC has no persisted owned loadout.");
			return false;
		}
		State.CardRun.PartySelection.QuestNpc.NpcId = RecoveredNpcId;
		State.CardRun.PartySelection.QuestNpc.SelectedCardIds = RecoveredLoadout->SelectedCardIds;
		if (!FGameXXKPartyFormationRules::Normalize(State, &OutError))
		{
			return false;
		}

		FGameXXKPartyMemberRef* NpcSlot =
			State.CardRun.OrderedFormation.Members.FindByPredicate(
				[](const FGameXXKPartyMemberRef& Ref)
				{
					return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
				});
		if (!NpcSlot)
		{
			OutError = TEXT("Normalized migrated formation has no NPC slot.");
			return false;
		}
		NpcSlot->MemberId = RecoveredNpcId;
		FGameXXKPartyFormationRules::ProjectCompatibility(State);
		if (!FGameXXKPartyFormationRules::Validate(
				State,
				State.CardRun.OrderedFormation,
				&OutError)
			|| !FGameXXKPartyFormationRules::ValidateCompatibilityProjection(State, &OutError))
		{
			return false;
		}
		return true;
	}

	bool MigrateRetiredNpcEncounter(FGameXXKRuntimeState& State, FString& OutError)
	{
		OutError.Reset();
		FGameXXKPendingRouteEvent& Pending = State.CardRun.PendingEvent;
		const bool bRetiredCatalogId =
			FGameXXKRouteEncounterCatalog::IsRetiredNpcEncounterId(Pending.EncounterId);
		const bool bLegacyRosterNpc =
			FGameXXKCompanionCatalog::FindQuestNpcDefinition(Pending.EventNpcId) != nullptr;
		const bool bLegacyNiuHuan = Pending.EventNpcId == TEXT("Npc.Event.NiuHuan");
		if (!bRetiredCatalogId && !bLegacyRosterNpc && !bLegacyNiuHuan)
		{
			return true;
		}

		const FGameXXKRouteEncounterDefinition* Replacement =
			FGameXXKRouteEncounterCatalog::ChooseDeterministic(
				EGameXXKRouteEncounterKind::Event,
				Pending.ChoiceSeed);
		if (!Replacement)
		{
			Pending = FGameXXKPendingRouteEvent();
			State.Screen = EGameXXKScreen::DungeonMap;
			State.CurrentMapId = TEXT("HuangshanRoute");
			return true;
		}

		Pending.EncounterId = Replacement->Id;
		Pending.EventNpcId = Replacement->EventNpcId;
		Pending.bCanRecruitPermanentCompanion = false;
		return true;
	}

	void MigrateInventoryCategories(FGameXXKRuntimeState& State)
	{
		const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
		if (!State.Inventory.Contains(StoneId) && State.EnhancementMaterial > 0)
		{
			State.Inventory.Add(StoneId, State.EnhancementMaterial);
		}
		State.EnhancementMaterial = FMath::Max(0, State.Inventory.FindRef(StoneId));
		MigrateRefinementSandMirror(State);
		const FName SealId = UGameXXKMVPRules::ItemQingshanRouteSeal();
		if (State.QuestState == EGameXXKQuestState::Accepted)
		{
			State.Inventory.FindOrAdd(SealId) = FMath::Max(1, State.Inventory.FindRef(SealId));
		}
		else
		{
			State.Inventory.Remove(SealId);
		}
	}

	void MigrateCodex(FGameXXKRuntimeState& State, const int32 SourceVersion)
	{
		if (SourceVersion < GuideIntroductionVersion
			&& (State.QuestState == EGameXXKQuestState::Accepted || State.QuestState == EGameXXKQuestState::Completed))
		{
			State.DiscoveredCodexEntryIds.Add(GuideId);
		}
		if (SourceVersion < FGameXXKSaveMigration::QuestFollowerAndCurrentEnemyCodexIntroducedSaveVersion)
		{
			MigrateLegacyCodexIds(State.DiscoveredCodexEntryIds);
			MigrateLegacyCodexIds(State.ReadCodexEntryIds);
		}
	}

	void MigrateQuestFollowerContract(FGameXXKRuntimeState& State, const int32 SourceVersion)
	{
		if (SourceVersion < FGameXXKSaveMigration::QuestFollowerAndCurrentEnemyCodexIntroducedSaveVersion
			&& State.QuestState == EGameXXKQuestState::Accepted)
		{
			State.bFollowerJoined = true;
		}
	}

	void NormalizeProgression(FGameXXKRuntimeState& State)
	{
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
	}

	void NormalizeTrainingProgress(FGameXXKTrainingProgress& Progress)
	{
		if (!Progress.UnlockedDifficultyIds.Contains(FGameXXKTrainingRules::DifficultyId(EGameXXKTrainingDifficulty::Normal))
			|| Progress.ClearedStageIds.IsEmpty()
			|| Progress.CurrentTravelStageId.IsNone())
		{
			FGameXXKTrainingRules::InitializeNewGame(Progress);
			return;
		}
		FGameXXKTrainingStageDefinition Definition;
		if (!FGameXXKTrainingRules::TryGetStageDefinition(Progress.CurrentTravelStageId, Definition))
		{
			Progress.CurrentTravelStageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
		}
		if (!FGameXXKTrainingRules::TryGetStageDefinition(Progress.SelectedStageId, Definition))
		{
			Progress.SelectedStageId = Progress.CurrentTravelStageId;
		}
		if (Progress.bChallengeActive && Progress.ActiveChallengeStageId.IsNone())
		{
			Progress.bChallengeActive = false;
			Progress.ActiveChallengeEncounterIndex = INDEX_NONE;
		}
		if (Progress.bTravelActive && Progress.bChallengeActive)
		{
			Progress.bTravelActive = false;
		}
		if (Progress.bTravelActive)
		{
			Progress.ActiveTravelEncounterIndex = FMath::Max(0, Progress.ActiveTravelEncounterIndex);
		}
		else
		{
			Progress.ActiveTravelEncounterIndex = INDEX_NONE;
		}
		Progress.TravelVictories = FMath::Max(0, Progress.TravelVictories);
		Progress.TravelFailures = FMath::Max(0, Progress.TravelFailures);
		Progress.TravelNormalChestCooldownRemainingSeconds = FMath::Max(0, Progress.TravelNormalChestCooldownRemainingSeconds);
		Progress.TravelAdvancedChestCooldownRemainingSeconds = FMath::Max(0, Progress.TravelAdvancedChestCooldownRemainingSeconds);
		Progress.PendingTravelGold = FMath::Max(0, Progress.PendingTravelGold);
		Progress.PendingTravelExperience = FMath::Max(0, Progress.PendingTravelExperience);
		Progress.PendingTravelNormalChestCount = FMath::Max(0, Progress.PendingTravelNormalChestCount);
		Progress.PendingTravelAdvancedChestCount = FMath::Max(0, Progress.PendingTravelAdvancedChestCount);
		Progress.PendingTravelCompletedEncounters = FMath::Max(0, Progress.PendingTravelCompletedEncounters);
		Progress.PendingTravelCompletedStages = FMath::Max(0, Progress.PendingTravelCompletedStages);
		Progress.PendingTravelSimulatedSeconds = FMath::Max(0, Progress.PendingTravelSimulatedSeconds);
		Progress.TravelLastUpdatedUnixSeconds = FMath::Max<int64>(0, Progress.TravelLastUpdatedUnixSeconds);
		if (!Progress.bTravelActive)
		{
			Progress.bTravelPausedAtDefeat = false;
		}
		if (Progress.ChallengeRewardSeed == 0)
		{
			Progress.ChallengeRewardSeed = FGameXXKTrainingRules::DefaultChallengeRewardSeed();
		}
	}

	int32 NormalizeRouteSeedForMigration(const int32 Seed)
	{
		return Seed == 0 || Seed == MIN_int32 ? 1 : FMath::Abs(Seed);
	}

	bool IsEmptySettlementReceipt(const FGameXXKRouteSettlementReceipt& Receipt)
	{
		const FGameXXKRouteSettlementReceipt EmptyReceipt;
		return Receipt.SettlementId == EmptyReceipt.SettlementId
			&& Receipt.Outcome == EmptyReceipt.Outcome
			&& Receipt.SourceTravelMoney == EmptyReceipt.SourceTravelMoney
			&& Receipt.SourceCardAcquisitionCount == EmptyReceipt.SourceCardAcquisitionCount
			&& Receipt.PermanentGoldAward == EmptyReceipt.PermanentGoldAward
			&& Receipt.EnhancementStoneAward == EmptyReceipt.EnhancementStoneAward;
	}

	bool IsValidSettlementOutcome(const EGameXXKRouteTerminalOutcome Outcome)
	{
		switch (Outcome)
		{
		case EGameXXKRouteTerminalOutcome::Cleared:
		case EGameXXKRouteTerminalOutcome::Defeated:
		case EGameXXKRouteTerminalOutcome::Abandoned:
			return true;
		default:
			return false;
		}
	}

	bool IsValidSettlementReceipt(const FGameXXKRouteSettlementReceipt& Receipt)
	{
		if (!Receipt.SettlementId.IsValid()
			|| !IsValidSettlementOutcome(Receipt.Outcome)
			|| Receipt.SourceTravelMoney < 0
			|| Receipt.SourceCardAcquisitionCount < 0
			|| Receipt.PermanentGoldAward < 0
			|| Receipt.EnhancementStoneAward < 0)
		{
			return false;
		}
		const int32 MoneyDivisor = Receipt.Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 10 : 20;
		const int32 CardDivisor = Receipt.Outcome == EGameXXKRouteTerminalOutcome::Cleared ? 5 : 10;
		return Receipt.PermanentGoldAward == Receipt.SourceTravelMoney / MoneyDivisor
			&& Receipt.EnhancementStoneAward == Receipt.SourceCardAcquisitionCount / CardDivisor;
	}

	uint64 MakeRouteEconomyReceiptKey(const int32 Chapter, const int32 NodeId)
	{
		return (static_cast<uint64>(static_cast<uint32>(Chapter)) << 32U)
			| static_cast<uint32>(NodeId);
	}

	bool ValidateRouteEconomyState(const FGameXXKRuntimeState& State, FString& OutError)
	{
		const FGameXXKCardRunState& CardRun = State.CardRun;
		if (!State.bDungeonActive)
		{
			if (CardRun.RouteTravelMoney != 0
				|| CardRun.bRouteEconomyInitialized
				|| !CardRun.RewardedTravelMoneyNodes.IsEmpty())
			{
				OutError = TEXT("Saved inactive route retains route-economy state.");
				return false;
			}
			return true;
		}

		if (!CardRun.bRouteEconomyInitialized || CardRun.RouteTravelMoney < 0)
		{
			OutError = TEXT("Saved active route has an uninitialized or negative route economy.");
			return false;
		}

		TSet<uint64> SeenReceiptKeys;
		SeenReceiptKeys.Reserve(CardRun.RewardedTravelMoneyNodes.Num());
		for (const FGameXXKRouteTravelMoneyReceipt& Receipt : CardRun.RewardedTravelMoneyNodes)
		{
			if (Receipt.Chapter < 1
				|| Receipt.Chapter > 3
				|| Receipt.NodeId < 0
				|| Receipt.Amount < 0)
			{
				OutError = TEXT("Saved route travel-money receipt is invalid.");
				return false;
			}

			const uint64 Key = MakeRouteEconomyReceiptKey(Receipt.Chapter, Receipt.NodeId);
			if (SeenReceiptKeys.Contains(Key))
			{
				OutError = TEXT("Saved route travel-money receipts contain a duplicate chapter-node key.");
				return false;
			}
			SeenReceiptKeys.Add(Key);
		}
		return true;
	}

	bool MigrateRouteEconomy(FGameXXKRuntimeState& State, FString& OutError)
	{
		FGameXXKCardRunState& CardRun = State.CardRun;
		if (!State.bDungeonActive)
		{
			FGameXXKRouteEconomyRules::ClearRouteEconomy(CardRun);
			return true;
		}

		if (CardRun.RouteTravelMoney < 0)
		{
			OutError = TEXT("Legacy active route has a negative route travel-money balance.");
			return false;
		}

		const int32 PreservedBalance = CardRun.RouteTravelMoney;
		FGameXXKCardRunState Candidate = CardRun;
		Candidate.RouteTravelMoney = 0;
		Candidate.bRouteEconomyInitialized = false;
		Candidate.RewardedTravelMoneyNodes.Reset();
		if (!FGameXXKRouteEconomyRules::InitializeRoute(Candidate, PreservedBalance, &OutError))
		{
			return false;
		}
		CardRun = MoveTemp(Candidate);
		return true;
	}

	void MigrateThreeChapterRouteProgress(FGameXXKRuntimeState& State)
	{
		if (!State.bDungeonActive)
		{
			State.CardRun.RouteProgress = FGameXXKRouteProgress();
			return;
		}

		FGameXXKRouteProgress& RouteProgress = State.CardRun.RouteProgress;
		RouteProgress.SchemaVersion = 1;
		RouteProgress.RootSeed = State.RouteSeed;
		RouteProgress.ChapterSeeds = {
			RouteProgress.RootSeed,
			NormalizeRouteSeedForMigration(FGameXXKEncounterRules::DeriveChapterSeed(RouteProgress.RootSeed, 2)),
			NormalizeRouteSeedForMigration(FGameXXKEncounterRules::DeriveChapterSeed(RouteProgress.RootSeed, 3))};
		RouteProgress.CurrentChapter = 1;
		RouteProgress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel);
	}

	void MigrateLegacyEnemyDamageIntents(FGameXXKRuntimeState& State)
	{
		for (FGameXXKCardEnemyIntent& Intent : State.CardRun.EnemyIntents)
		{
			if (!Intent.Effects.IsEmpty() || Intent.Damage <= 0)
			{
				continue;
			}

			FGameXXKResolvedEnemyIntentEffect Effect;
			Effect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
			Effect.Magnitude = Intent.Damage;
			Effect.HitCount = 1;
			if (!Intent.SuggestedTargetUnitId.IsNone())
			{
				Effect.TargetUnitIds.Add(Intent.SuggestedTargetUnitId);
			}
			Intent.Effects = { MoveTemp(Effect) };
		}
	}

	FName* SlotPtr(FGameXXKEquipmentLoadout& Loadout, const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return &Loadout.WeaponInstanceId;
		case EGameXXKEquipmentSlot::Head: return &Loadout.HeadInstanceId;
		case EGameXXKEquipmentSlot::Armor: return &Loadout.ArmorInstanceId;
		case EGameXXKEquipmentSlot::Belt: return &Loadout.BeltInstanceId;
		case EGameXXKEquipmentSlot::Shoes: return &Loadout.ShoesInstanceId;
		case EGameXXKEquipmentSlot::Accessory: return &Loadout.AccessoryInstanceId;
		default: return nullptr;
		}
	}

	bool AddLegacyInstance(
		FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentDefinition& Definition,
		const int32 EnhancementLevel,
		const EGameXXKEquipmentOwnerKind OwnerKind,
		const FName OwnerId,
		FName& OutInstanceId,
		FString& OutError)
	{
		OutInstanceId = NAME_None;
		if (Collection.NextInstanceOrdinal == MAX_int32)
		{
			OutError = TEXT("Legacy equipment instance ordinal is exhausted.");
			return false;
		}
		const FName InstanceId(*FString::Printf(
			TEXT("EquipmentInstance.%08X.%d"),
			static_cast<uint32>(Collection.CollectionSeed),
			Collection.NextInstanceOrdinal));
		if (FGameXXKEquipmentRules::FindInstance(Collection, InstanceId))
		{
			OutError = TEXT("Legacy equipment migration generated a duplicate instance ID.");
			return false;
		}
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = InstanceId;
		Instance.BaseEquipmentId = Definition.Id;
		Instance.ItemLevel = 1;
		Instance.Quality = EGameXXKEquipmentQuality::Common;
		Instance.EnhancementLevel = FMath::Clamp(EnhancementLevel, 0, FGameXXKEquipmentRules::MaxEnhancementLevel);
		Instance.AcquisitionSeed = static_cast<int32>(FCrc::StrCrc32(*InstanceId.ToString()));
		Instance.ScalingRule = EGameXXKEquipmentScalingRule::LegacyFlatPerEnhancement;
		Instance.LegacyBaseStatSnapshot = Definition.LegacyBaseStatSnapshot;
		Instance.OwnerKind = OwnerKind;
		Instance.OwnerCharacterId = OwnerId;
		Collection.EquipmentInstances.Add(MoveTemp(Instance));
		if (OwnerKind == EGameXXKEquipmentOwnerKind::Warehouse)
		{
			Collection.WarehouseInstanceIds.Add(InstanceId);
		}
		else
		{
			FGameXXKEquipmentLoadout& Loadout = Collection.CharacterLoadouts.FindOrAdd(OwnerId);
			FName* SlotId = SlotPtr(Loadout, Definition.Slot);
			if (!SlotId || !SlotId->IsNone())
			{
				OutError = TEXT("Legacy equipment migration attempted to overwrite an occupied slot.");
				return false;
			}
			*SlotId = InstanceId;
		}
		Collection.NextInstanceOrdinal += 1;
		OutInstanceId = InstanceId;
		return true;
	}

	bool ValidateHeroMirror(
		const FName BaseId,
		const EGameXXKEquipmentSlot ExpectedSlot,
		FString& OutError)
	{
		if (BaseId.IsNone())
		{
			return true;
		}
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseId);
		if (!Definition || Definition->Set != EGameXXKEquipmentSet::Legacy || Definition->Slot != ExpectedSlot)
		{
			OutError = FString::Printf(TEXT("Legacy hero mirror %s does not match its slot."), *BaseId.ToString());
			return false;
		}
		return true;
	}

	bool ConvertLegacyEquipment(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report,
		FString& OutError)
	{
		FGameXXKEquipmentCollectionState& Collection = State.EquipmentCollection;
		if (Collection.CollectionSeed == 0)
		{
			Collection.CollectionSeed = StableMigrationCollectionSeed;
		}
		Collection.EquipmentSchemaVersion = 1;
		if (!Collection.EquipmentInstances.IsEmpty())
		{
			if (!FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(Collection, State.CardRun.CompanionRoster, &OutError))
			{
				return false;
			}
			// Task 5 could write instance-backed v6 saves. Their Inventory equipment entries are already mirrors.
			return FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State);
		}

		if (!ValidateHeroMirror(State.EquippedWeapon, EGameXXKEquipmentSlot::Weapon, OutError)
			|| !ValidateHeroMirror(State.EquippedArmor, EGameXXKEquipmentSlot::Armor, OutError)
			|| !ValidateHeroMirror(State.EquippedAccessory, EGameXXKEquipmentSlot::Accessory, OutError))
		{
			return false;
		}

		TSet<FName> EquipmentKeySet;
		for (const TPair<FName, int32>& Pair : State.Inventory)
		{
			if (FGameXXKEquipmentCatalog::FindDefinition(Pair.Key))
			{
				if (Pair.Value < 0)
				{
					OutError = TEXT("Legacy equipment quantity cannot be negative.");
					return false;
				}
				EquipmentKeySet.Add(Pair.Key);
			}
		}
		for (const FName Mirror : {State.EquippedWeapon, State.EquippedArmor, State.EquippedAccessory})
		{
			if (!Mirror.IsNone())
			{
				EquipmentKeySet.Add(Mirror);
			}
		}
		TArray<FName> EquipmentKeys = EquipmentKeySet.Array();
		EquipmentKeys.Sort(FNameLexicalLess());

		for (const FName BaseId : EquipmentKeys)
		{
			const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseId);
			if (!Definition || Definition->Set != EGameXXKEquipmentSet::Legacy)
			{
				OutError = TEXT("Only legacy catalog equipment can be converted from Inventory.");
				return false;
			}
			const int32 SavedQuantity = State.Inventory.FindRef(BaseId);
			const int32 EnhancementLevel = State.ItemEnhancementLevels.FindRef(BaseId);
			const bool bHeroEquipped = State.EquippedWeapon == BaseId
				|| State.EquippedArmor == BaseId
				|| State.EquippedAccessory == BaseId;
			int32 WarehouseCopies = SavedQuantity;
			if (bHeroEquipped)
			{
				FName InstanceId;
				if (!AddLegacyInstance(
					Collection,
					*Definition,
					EnhancementLevel,
					EGameXXKEquipmentOwnerKind::Hero,
					FGameXXKEquipmentRules::HeroCharacterId(),
					InstanceId,
					OutError))
				{
					return false;
				}
				WarehouseCopies = FMath::Max(0, SavedQuantity - 1);
			}
			for (int32 CopyIndex = 0; CopyIndex < WarehouseCopies; ++CopyIndex)
			{
				FName InstanceId;
				if (!AddLegacyInstance(
					Collection,
					*Definition,
					EnhancementLevel,
					EGameXXKEquipmentOwnerKind::Warehouse,
					NAME_None,
					InstanceId,
					OutError))
				{
					return false;
				}
			}
		}

		for (FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (Companion.InstanceId.IsNone())
			{
				OutError = TEXT("A legacy companion equipment owner has no stable instance ID.");
				return false;
			}
			TSet<EGameXXKEquipmentSlot> OccupiedSlots;
			for (const FName BaseId : Companion.EquippedItemIds)
			{
				const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(BaseId);
				if (!Definition || Definition->Set != EGameXXKEquipmentSet::Legacy)
				{
					OutError = FString::Printf(TEXT("Companion legacy equipment %s has no supported definition."), *BaseId.ToString());
					return false;
				}
				const bool bDuplicateSlot = OccupiedSlots.Contains(Definition->Slot);
				FName InstanceId;
				if (!AddLegacyInstance(
					Collection,
					*Definition,
					State.ItemEnhancementLevels.FindRef(BaseId),
					bDuplicateSlot ? EGameXXKEquipmentOwnerKind::Warehouse : EGameXXKEquipmentOwnerKind::PermanentCompanion,
					bDuplicateSlot ? NAME_None : Companion.InstanceId,
					InstanceId,
					OutError))
				{
					return false;
				}
				if (bDuplicateSlot)
				{
					Report.Warnings.Add(FString::Printf(
						TEXT("Companion %s had duplicate legacy slot data; %s was moved to the warehouse."),
						*Companion.InstanceId.ToString(),
						*BaseId.ToString()));
				}
				else
				{
					OccupiedSlots.Add(Definition->Slot);
				}
			}
		}

		Collection.bLegacyWarehouseOverflow =
			Collection.WarehouseInstanceIds.Num() > FGameXXKEquipmentRules::WarehouseCapacity;
		Report.bCreatedLegacyOverflow = Collection.bLegacyWarehouseOverflow;
		if (!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(State))
		{
			OutError = TEXT("Migrated equipment mirrors or runtime stats failed validation.");
			return false;
		}
		return true;
	}

	FGameXXKRuntimeState RestoreOldChain(const FGameXXKSaveState& Source)
	{
		FGameXXKRuntimeState State;
		if (Source.SaveVersion >= 3)
		{
			State = Source.RuntimeState;
			if (Source.bHasPlayerLocation)
			{
				State.bHasPlayerLocation = true;
				State.PlayerLocation = Source.PlayerLocation;
			}
		}
		else if (Source.SaveVersion == 2)
		{
			State = Source.RuntimeState;
			State.EnhancementMaterial = 10;
			State.ItemEnhancementLevels.Reset();
			if (Source.bHasPlayerLocation)
			{
				State.bHasPlayerLocation = true;
				State.PlayerLocation = Source.PlayerLocation;
			}
		}
		else
		{
			// Preserve the historic v0/v1 baseline (including CurrentMapId=MainMenu),
			// then explicitly remove fields that did not exist before instance equipment.
			State = UGameXXKMVPRules::CreateNewGame();
			State.Screen = EGameXXKScreen::MainMenu;
			State.QuestState = Source.QuestState;
			State.PlayerLevel = Source.PlayerLevel;
			State.PlayerXP = Source.PlayerXP;
			State.PlayerGold = Source.PlayerGold;
			State.UnlockedRegions = Source.UnlockedRegions;
			State.Inventory.Reset();
			State.ItemEnhancementLevels.Reset();
			State.EquipmentCollection = FGameXXKEquipmentCollectionState();
			State.EquippedWeapon = NAME_None;
			State.EquippedArmor = NAME_None;
			State.EquippedAccessory = NAME_None;
			State.TownPanelMode = EGameXXKTownPanelMode::None;
			State.bHasPlayerLocation = Source.bHasPlayerLocation;
			State.PlayerLocation = Source.PlayerLocation;
			State.bFollowerJoined = Source.bFollowerJoined;
			State.bHasQuestNpcLocation = Source.bHasQuestNpcLocation;
			State.QuestNpcLocation = Source.QuestNpcLocation;
		}
		MigrateInventoryCategories(State);
		MigrateCodex(State, Source.SaveVersion);
		MigrateQuestFollowerContract(State, Source.SaveVersion);
		NormalizeProgression(State);
		return State;
	}

	bool IsBattleRetreatEncounterKind(const EGameXXKNodeKind Kind)
	{
		return Kind == EGameXXKNodeKind::Battle
			|| Kind == EGameXXKNodeKind::Elite
			|| Kind == EGameXXKNodeKind::Boss;
	}

	void MigrateBattleRetreatCheckpoint(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report)
	{
		State.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
		if (!State.bDungeonActive
			|| !State.bHasGeneratedRouteMap
			|| State.Screen != EGameXXKScreen::Battle
			|| !State.CardRun.bHasActiveCardBattle
			|| State.PendingRouteNodeId == INDEX_NONE)
		{
			return;
		}

		const FGameXXKRouteMapNode* SourceNode = State.RouteMapNodes.FindByPredicate([&State](const FGameXXKRouteMapNode& Node)
		{
			return Node.NodeId == State.PendingRouteNodeId;
		});
		if (!SourceNode || !IsBattleRetreatEncounterKind(SourceNode->NodeKind))
		{
			Report.Warnings.Add(TEXT("Legacy active battle has no recoverable battle retreat source node; retreat is disabled for this battle."));
			return;
		}

		TSet<int32> UniqueVisitedParents;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			if (Edge.ToNodeId == State.PendingRouteNodeId
				&& State.VisitedRouteNodeIds.Contains(Edge.FromNodeId))
			{
				UniqueVisitedParents.Add(Edge.FromNodeId);
			}
		}
		if (UniqueVisitedParents.Num() != 1 || !State.ReachableRouteNodeIds.Contains(State.PendingRouteNodeId))
		{
			Report.Warnings.Add(TEXT("Legacy active battle has an ambiguous battle retreat parent; retreat is disabled for this battle."));
			return;
		}

		FGameXXKBattleEntryCheckpoint& Checkpoint = State.BattleEntryCheckpoint;
		Checkpoint.bValid = true;
		Checkpoint.SourceNodeId = State.PendingRouteNodeId;
		Checkpoint.PreviousCurrentRouteNodeId = UniqueVisitedParents.Array()[0];
		Checkpoint.PreviousDungeonNodeIndex = State.DungeonNodeIndex;
		Checkpoint.PreviousPlayerHP = State.PlayerHP;
		Checkpoint.PreviousPlayerMP = State.PlayerMP;
		Checkpoint.PreviousVisitedRouteNodeIds = State.VisitedRouteNodeIds;
		Checkpoint.PreviousReachableRouteNodeIds = State.ReachableRouteNodeIds;
	}

	bool AddMinimumLegacyPartyCompanions(
		FGameXXKRuntimeState& State,
		int32& OutAddedCount,
		FString& OutError)
	{
		OutAddedCount = 0;
		FGameXXKOrderedPartyFormation Probe;
		if (State.CardRun.CompanionRoster.PermanentCompanions.Num()
				>= FGameXXKPartyFormationRules::MinimumOwnedPermanentCompanions
			&& FGameXXKPartyFormationRules::BuildLegacyProjection(State, Probe))
		{
			return true;
		}

		static const FName StarterTemplates[] = {
			TEXT("Companion.Blade.01"),
			TEXT("Companion.Guard.01"),
			TEXT("Companion.Healer.01"),
			TEXT("Companion.Hunter.01"),
			TEXT("Companion.Sorcerer.01"),
			TEXT("Companion.FormationMaster.01")};
		constexpr int32 StableRepairSeedBase = 0x24680000;
		for (int32 TemplateIndex = 0; TemplateIndex < UE_ARRAY_COUNT(StarterTemplates); ++TemplateIndex)
		{
			const FName TemplateId = StarterTemplates[TemplateIndex];
			if (State.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate(
				[TemplateId](const FGameXXKPermanentCompanion& Companion)
				{
					return Companion.RecruitTemplateId == TemplateId;
				}))
			{
				continue;
			}

			FGameXXKCompanionRecruitResult RecruitResult;
			if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				State.CardRun.CompanionRoster,
				TemplateId,
				StableRepairSeedBase + TemplateIndex,
				RecruitResult,
				&OutError)
				|| RecruitResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
			{
				if (OutError.IsEmpty())
				{
					OutError = TEXT("Legacy ordered-party repair could not append an approved starter companion.");
				}
				return false;
			}
			++OutAddedCount;
			if (State.CardRun.CompanionRoster.PermanentCompanions.Num()
					>= FGameXXKPartyFormationRules::MinimumOwnedPermanentCompanions
				&& FGameXXKPartyFormationRules::BuildLegacyProjection(State, Probe))
			{
				return true;
			}
		}

		OutError = TEXT("Legacy ordered-party repair could not provide three legal party entities.");
		return false;
	}

	void MigrateLegacyTalentProgress(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report)
	{
		State.Talents.NodeRanks.Reset();
		const int32 LastBackpackSlot = FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
			State,
			EGameXXKDesktopItemContainer::Backpack);
		const int32 LastWarehouseSlot = FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
			State,
			EGameXXKDesktopItemContainer::Warehouse);
		State.Talents.MinimumBackpackCapacity = FMath::Clamp(
			FMath::Max(20, LastBackpackSlot + 1),
			20,
			FGameXXKDesktopInventoryRules::BackpackCapacity);
		State.Talents.MinimumWarehousePages = FMath::Clamp(
			FMath::Max(1, FMath::DivideAndRoundUp(LastWarehouseSlot + 1, 36)),
			1,
			6);
		Report.Warnings.Add(FString::Printf(
			TEXT("Legacy talent migration preserved capacity floors at Backpack %d and Warehouse %d page(s)."),
			State.Talents.MinimumBackpackCapacity,
			State.Talents.MinimumWarehousePages));
	}

	const FGameXXKTaskStepDefinition* FindNarrativeTaskStep(
		const FGameXXKTaskDefinition& Task,
		const FName StepId)
	{
		return Task.Steps.FindByPredicate([StepId](const FGameXXKTaskStepDefinition& Step)
		{
			return Step.StepId == StepId;
		});
	}

	void MigrateLegacyTutorialNarrative(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report)
	{
		State.NarrativeSequenceSession = FGameXXKNarrativeSequenceSessionState();
		State.NarrativeProgress = FGameXXKNarrativeProgress();
		State.GuideProgress = FGameXXKGuideProgress();
		Report.Warnings.Add(TEXT("Pre-v29 narrative state initialized without the retired Xu Xiake StoryTask."));
	}

	void RetireLegacyTutorialNarrative(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report)
	{
		const FName StoryId(TEXT("Story.Main.XuXiakeTreasure"));
		const FName TaskId(TEXT("Task.Main.XuXiake.Prologue"));
		const FName SequenceId(TEXT("Sequence.Main.XuXiake.CarriageArrival"));
		bool bChanged = false;

		if (State.NarrativeSequenceSession.StoryId == StoryId
			|| State.NarrativeSequenceSession.TaskId == TaskId
			|| State.NarrativeSequenceSession.SequenceId == SequenceId)
		{
			State.NarrativeSequenceSession = FGameXXKNarrativeSequenceSessionState();
			bChanged = true;
		}
		if (State.DialogueSession.StoryId == StoryId
			|| State.DialogueSession.TaskId == TaskId)
		{
			State.DialogueSession = FGameXXKDialogueSessionState();
			bChanged = true;
		}
		if (State.NarrativeProgress.TrackedTaskId == TaskId)
		{
			State.NarrativeProgress.TrackedTaskId = NAME_None;
			bChanged = true;
		}
		bChanged |= State.NarrativeProgress.TaskProgressById.Remove(TaskId) > 0;
		bChanged |= State.NarrativeProgress.StoryProgressById.Remove(StoryId) > 0;
		for (TPair<FName, FGameXXKStoryProgress>& Pair :
			State.NarrativeProgress.StoryProgressById)
		{
			bChanged |= Pair.Value.ActiveTaskIds.Remove(TaskId) > 0;
			bChanged |= Pair.Value.CompletedTaskIds.Remove(TaskId) > 0;
		}
		if (bChanged)
		{
			Report.Warnings.Add(TEXT("Retired legacy Xu Xiake StoryTask progress and sessions were removed."));
		}
	}

	bool ValidateNarrativeStageGuideState(const FGameXXKRuntimeState& State, FString& OutError)
	{
		const FGameXXKNarrativeProgress& Narrative = State.NarrativeProgress;
		for (const TPair<FName, FGameXXKStoryProgress>& Pair : Narrative.StoryProgressById)
		{
			const FGameXXKStoryDefinition* Definition = FGameXXKStoryCatalog::FindStory(Pair.Key);
			const FGameXXKStoryProgress& Progress = Pair.Value;
			if (!Definition
				|| Progress.Version != Definition->Version
				|| Progress.ActiveTaskIds.Contains(NAME_None)
				|| Progress.CompletedTaskIds.Contains(NAME_None))
			{
				OutError = TEXT("Saved narrative story progress is invalid.");
				return false;
			}
			for (const FName ActiveTaskId : Progress.ActiveTaskIds)
			{
				const FGameXXKTaskDefinition* Task = FGameXXKStoryCatalog::FindTask(ActiveTaskId);
				if (!Task || Task->StoryId != Pair.Key || Progress.CompletedTaskIds.Contains(ActiveTaskId))
				{
					OutError = TEXT("Saved story has invalid active-task membership.");
					return false;
				}
			}
			for (const FName CompletedTaskId : Progress.CompletedTaskIds)
			{
				const FGameXXKTaskDefinition* Task = FGameXXKStoryCatalog::FindTask(CompletedTaskId);
				if (!Task || Task->StoryId != Pair.Key)
				{
					OutError = TEXT("Saved story has invalid completed-task membership.");
					return false;
				}
			}
			if ((Progress.State == EGameXXKStoryState::Inactive
					&& (!Progress.ActiveTaskIds.IsEmpty() || !Progress.CompletedTaskIds.IsEmpty()))
				|| (Progress.State == EGameXXKStoryState::Completed && !Progress.ActiveTaskIds.IsEmpty()))
			{
				OutError = TEXT("Saved story state disagrees with its task membership.");
				return false;
			}
		}

		for (const TPair<FName, FGameXXKTaskProgress>& Pair : Narrative.TaskProgressById)
		{
			const FGameXXKTaskDefinition* Definition = FGameXXKStoryCatalog::FindTask(Pair.Key);
			const FGameXXKTaskProgress& Progress = Pair.Value;
			const FGameXXKStoryProgress* Story = Definition
				? Narrative.StoryProgressById.Find(Definition->StoryId)
				: nullptr;
			const FGameXXKTaskStepDefinition* Step = Definition
				? FindNarrativeTaskStep(*Definition, Progress.CurrentStepId)
				: nullptr;
			if (!Definition || !Story)
			{
				OutError = TEXT("Saved narrative task has no catalog story.");
				return false;
			}
			if (Progress.ObjectiveCounts.Contains(NAME_None))
			{
				OutError = TEXT("Saved narrative task has an empty objective ID.");
				return false;
			}
			for (const TPair<FName, int32>& Objective : Progress.ObjectiveCounts)
			{
				if (Objective.Value < 0)
				{
					OutError = TEXT("Saved narrative task has a negative objective count.");
					return false;
				}
			}
			const bool bStarted = Progress.State == EGameXXKTaskState::Active
				|| Progress.State == EGameXXKTaskState::Completed
				|| Progress.State == EGameXXKTaskState::Rewarded;
			if ((bStarted && !Step)
				|| (!bStarted && !Progress.CurrentStepId.IsNone())
				|| (Progress.State == EGameXXKTaskState::Rewarded && !Progress.bRewardCommitted)
				|| (Progress.bRewardCommitted && Progress.State != EGameXXKTaskState::Rewarded))
			{
				OutError = TEXT("Saved narrative task state or step is invalid.");
				return false;
			}
			if (Progress.State == EGameXXKTaskState::Active
				&& (!Story->ActiveTaskIds.Contains(Pair.Key) || Story->State != EGameXXKStoryState::Active))
			{
				OutError = TEXT("Saved active narrative task is detached from its story.");
				return false;
			}
			if ((Progress.State == EGameXXKTaskState::Completed || Progress.State == EGameXXKTaskState::Rewarded)
				&& !Story->CompletedTaskIds.Contains(Pair.Key))
			{
				OutError = TEXT("Saved completed narrative task is detached from its story receipt.");
				return false;
			}
		}

		if (!Narrative.TrackedTaskId.IsNone())
		{
			const FGameXXKTaskProgress* Tracked = Narrative.TaskProgressById.Find(Narrative.TrackedTaskId);
			if (!Tracked
				|| (Tracked->State != EGameXXKTaskState::Active
					&& Tracked->State != EGameXXKTaskState::Completed))
			{
				OutError = TEXT("Saved tracked narrative task is invalid.");
				return false;
			}
		}

		const FGameXXKNarrativeSequenceSessionState& Sequence = State.NarrativeSequenceSession;
		const bool bHasSequenceContext = !Sequence.StoryId.IsNone()
			|| Sequence.StoryVersion != 0
			|| !Sequence.TaskId.IsNone()
			|| !Sequence.StepId.IsNone()
			|| !Sequence.SequenceId.IsNone()
			|| Sequence.SequenceVersion != 0
			|| !Sequence.StageContractId.IsNone()
			|| !Sequence.CurrentSequenceStepId.IsNone()
			|| !Sequence.AwaitedDialogueId.IsNone()
			|| !Sequence.LastOutcomeId.IsNone()
			|| !Sequence.CharacterIdByRole.IsEmpty()
			|| !Sequence.PauseReason.IsEmpty();
		if (!Sequence.bActive && bHasSequenceContext)
		{
			OutError = TEXT("Saved inactive narrative sequence retains active context.");
			return false;
		}
		if (Sequence.bActive)
		{
			const FGameXXKStoryDefinition* StoryDefinition = FGameXXKStoryCatalog::FindStory(Sequence.StoryId);
			const FGameXXKTaskDefinition* TaskDefinition = FGameXXKStoryCatalog::FindTask(Sequence.TaskId);
			const FGameXXKTaskStepDefinition* StepDefinition = TaskDefinition
				? FindNarrativeTaskStep(*TaskDefinition, Sequence.StepId)
				: nullptr;
			const FGameXXKTaskProgress* TaskProgress = Narrative.TaskProgressById.Find(Sequence.TaskId);
			if (!StoryDefinition
				|| StoryDefinition->Version != Sequence.StoryVersion
				|| !TaskDefinition
				|| TaskDefinition->StoryId != Sequence.StoryId
				|| !StepDefinition
				|| StepDefinition->SequenceId != Sequence.SequenceId
				|| StepDefinition->StageContractId != Sequence.StageContractId
				|| Sequence.SequenceVersion <= 0
				|| Sequence.CurrentSequenceStepId.IsNone()
				|| !TaskProgress
				|| TaskProgress->State != EGameXXKTaskState::Active
				|| TaskProgress->CurrentStepId != Sequence.StepId)
			{
				OutError = TEXT("Saved active narrative sequence is detached from its task or catalog contract.");
				return false;
			}
			for (const TPair<FName, FName>& Role : Sequence.CharacterIdByRole)
			{
				if (Role.Key.IsNone() || Role.Value.IsNone())
				{
					OutError = TEXT("Saved narrative sequence has an invalid role binding.");
					return false;
				}
			}
		}
		for (const FName CommandKey : Sequence.ExecutedCommandKeys)
		{
			TArray<FString> Parts;
			CommandKey.ToString().ParseIntoArray(Parts, TEXT("/"), false);
			if (CommandKey.IsNone()
				|| Parts.Num() != 4
				|| Parts.ContainsByPredicate([](const FString& Part) { return Part.IsEmpty(); }))
			{
				OutError = TEXT("Saved narrative sequence has an invalid executed-command key.");
				return false;
			}
		}

		const FGameXXKGuideProgress& Guide = State.GuideProgress;
		const bool bHasActiveGuide = !Guide.ActiveGuideId.IsNone() || !Guide.ActiveGuideStepId.IsNone();
		if (Guide.CompletedGuideStepIds.Contains(NAME_None)
			|| (Guide.ActiveGuideId.IsNone() != Guide.ActiveGuideStepId.IsNone()))
		{
			OutError = TEXT("Saved guide progress has invalid active or completed IDs.");
			return false;
		}
		if (bHasActiveGuide)
		{
			const FGameXXKTaskProgress* Tracked = Narrative.TaskProgressById.Find(Narrative.TrackedTaskId);
			const FGameXXKTaskDefinition* Task = FGameXXKStoryCatalog::FindTask(Narrative.TrackedTaskId);
			const FGameXXKTaskStepDefinition* Step = Task && Tracked
				? FindNarrativeTaskStep(*Task, Tracked->CurrentStepId)
				: nullptr;
			const FString ExpectedStepPrefix = Guide.ActiveGuideId.ToString() + TEXT(".");
			if (Guide.Preference != EGameXXKGuidePreference::NewPlayer
				|| !FGameXXKGuideTargetRegistry::IsKnownGuideId(Guide.ActiveGuideId)
				|| !Tracked
				|| Tracked->State != EGameXXKTaskState::Active
				|| !Step
				|| Step->RouteId.IsNone()
				|| !Guide.ActiveGuideStepId.ToString().StartsWith(ExpectedStepPrefix))
			{
				OutError = TEXT("Saved active guide is detached from an active narrative route task.");
				return false;
			}
		}
		return true;
	}

	void MigrateCombatScalingFoundation(FGameXXKRuntimeState& InOutState)
	{
		if (!InOutState.CardRun.bHasActiveCardBattle)
		{
			return;
		}

		FGameXXKCardBattleRuntime& Battle = InOutState.CardRun.ActiveBattle;
		int32 HighestPartyLevel = 1;
		for (const FGameXXKCardCombatUnit& Unit : Battle.Units)
		{
			if (Unit.Side == EGameXXKCardTargetSide::Party)
			{
				HighestPartyLevel = FMath::Max(
					HighestPartyLevel,
					FMath::Clamp(Unit.CombatLevel, 1, FGameXXKCharacterStatRules::MaxCharacterLevel));
			}
		}
		Battle.TeamMaxLevelSnapshot = HighestPartyLevel;
		Battle.EnemyDifficultyDamagePercent = 100;
		Battle.PendingNextRoundEnergyPenalty = 0;
	}
}

bool FGameXXKSaveMigration::MigrateToCurrent(
	const FGameXXKSaveState& Source,
	FGameXXKSaveState& OutMigrated,
	FGameXXKSaveMigrationReport& OutReport)
{
	OutMigrated = FGameXXKSaveState();
	OutReport = FGameXXKSaveMigrationReport();
	OutReport.SourceVersion = Source.SaveVersion;
	OutReport.TargetVersion = CurrentSaveVersion;
	if (Source.SaveVersion < 0 || Source.SaveVersion > CurrentSaveVersion)
	{
		Fail(OutReport, TEXT("Unsupported save version."));
		return false;
	}
	if (Source.SaveVersion == CurrentSaveVersion)
	{
		FGameXXKSaveState Candidate = Source;
		MigrateRefinementSandMirror(Candidate.RuntimeState);
		FString ValidationError;
		const int32 QuestNpcProgressionSeed = Candidate.RuntimeState.CardRun.RouteRandomSeed != 0
			? Candidate.RuntimeState.CardRun.RouteRandomSeed
			: FGameXXKTrainingRules::DefaultChallengeRewardSeed();
		if (!MigrateCompanionBirthPools(Candidate.RuntimeState, OutReport, ValidationError)
			|| !FGameXXKCompanionRules::NormalizeOwnedQuestNpcCardLoadouts(
				Candidate.RuntimeState.CardRun.PartySelection,
				QuestNpcProgressionSeed,
				&ValidationError)
			|| !FGameXXKEquipmentRules::NormalizeSocketArrays(Candidate.RuntimeState.EquipmentCollection, &ValidationError)
			|| !FGameXXKEquipmentToolRules::NormalizeProgress(Candidate.RuntimeState.ToolProgress)
			|| !MigrateLegacyTrainingChestStacks(Candidate.RuntimeState, ValidationError)
			|| !MigratePermanentNpcFormation(Candidate.RuntimeState, OutReport, ValidationError)
			|| !MigrateRetiredNpcEncounter(Candidate.RuntimeState, ValidationError))
		{
			Fail(OutReport, ValidationError);
			return false;
		}
		if (!ValidateRuntimeState(Candidate.RuntimeState, ValidationError))
		{
			Fail(OutReport, ValidationError);
			return false;
		}
		OutMigrated = MoveTemp(Candidate);
		OutReport.bSucceeded = true;
		return true;
	}
	if (Source.SaveVersion == OrderedPartyFormationIntroducedSaveVersion)
	{
		// v24 already satisfies every pre-lock current invariant. Upgrade it
		// surgically so legacy category/progression/Training normalizers cannot
		// rewrite valid-but-noncanonical player state.
		FGameXXKSaveState Candidate = Source;
		MigrateRefinementSandMirror(Candidate.RuntimeState);
		Candidate.RuntimeState.DesktopInventory.LockedEquipmentInstanceIds.Reset();
		Candidate.RuntimeState.DesktopInventory.LockedItemIds.Reset();
		Candidate.RuntimeState.DesktopInventory.bToolAutoFillIncludesWarehouse = true;
		MigrateLegacyTalentProgress(Candidate.RuntimeState, OutReport);
		MigrateLegacyTutorialNarrative(Candidate.RuntimeState, OutReport);
		MigrateCombatScalingFoundation(Candidate.RuntimeState);
		Candidate.SaveVersion = CurrentSaveVersion;
		FString ValidationError;
		const int32 QuestNpcProgressionSeed = Candidate.RuntimeState.CardRun.RouteRandomSeed != 0
			? Candidate.RuntimeState.CardRun.RouteRandomSeed
			: FGameXXKTrainingRules::DefaultChallengeRewardSeed();
		if (!MigrateCompanionBirthPools(Candidate.RuntimeState, OutReport, ValidationError)
			|| !FGameXXKCompanionRules::NormalizeOwnedQuestNpcCardLoadouts(
				Candidate.RuntimeState.CardRun.PartySelection,
				QuestNpcProgressionSeed,
				&ValidationError)
			|| !FGameXXKEquipmentRules::NormalizeSocketArrays(Candidate.RuntimeState.EquipmentCollection, &ValidationError)
			|| !FGameXXKEquipmentToolRules::NormalizeProgress(Candidate.RuntimeState.ToolProgress)
			|| !MigrateLegacyTrainingChestStacks(Candidate.RuntimeState, ValidationError)
			|| !MigratePermanentNpcFormation(Candidate.RuntimeState, OutReport, ValidationError)
			|| !MigrateRetiredNpcEncounter(Candidate.RuntimeState, ValidationError))
		{
			Fail(OutReport, ValidationError);
			return false;
		}
		if (!ValidateRuntimeState(Candidate.RuntimeState, ValidationError))
		{
			Fail(OutReport, ValidationError);
			return false;
		}
		OutMigrated = MoveTemp(Candidate);
		OutReport.bSucceeded = true;
		return true;
	}

	FGameXXKSaveState Candidate = Source;
	Candidate.RuntimeState = RestoreOldChain(Source);
	FString MigrationError;
	if (Source.SaveVersion < BladePartnerCardsIntroducedSaveVersion)
	{
		// Map retired Blade IDs before the v13 birth-pool migration validates any
		// legacy six- or twelve-card companion profile against the live catalog.
		MigrateBladePartnerCards(Candidate.RuntimeState, OutReport);
	}
	if (Source.SaveVersion < HeroCardPoolIntroducedSaveVersion
		&& !MigrateHeroCardPool(Candidate.RuntimeState, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
	if (!MigrateCompanionBirthPools(Candidate.RuntimeState, OutReport, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
	if (Source.SaveVersion < MetaShopIntroducedSaveVersion)
	{
		Candidate.RuntimeState.MetaShop.Seed = FGameXXKMetaShopRules::DeriveSeed(Candidate.RuntimeState);
		Candidate.RuntimeState.MetaShop.NextPurchaseOrdinal = 0;
	}
	if (Source.SaveVersion < RouteMerchantStockSchemaIntroducedSaveVersion)
	{
		// Pre-v10 snapshots did not carry the canonical six-slot identities and ownership contract.
		// Discard only merchant stock; route topology, pending rewards, v9 entries, and economy stay exact.
		Candidate.RuntimeState.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
	}
	if (Source.SaveVersion < ThreeChapterRouteIntroducedSaveVersion)
	{
		MigrateThreeChapterRouteProgress(Candidate.RuntimeState);
		MigrateLegacyEnemyDamageIntents(Candidate.RuntimeState);
	}
	if (!ConvertLegacyEquipment(Candidate.RuntimeState, OutReport, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
	if (Source.SaveVersion < RouteEconomyIntroducedSaveVersion
		&& !MigrateRouteEconomy(Candidate.RuntimeState, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
	if (Source.SaveVersion < BattleRewardTieringIntroducedSaveVersion)
	{
		// v16 reward tiering: typed Options replace the legacy three-route-card offer.
		// An in-flight pre-v16 reward cannot map to the new tiering, so it is cleared
		// and the next victory re-rolls the offer.  New permanent bonus fields keep
		// their zero defaults.
		Candidate.RuntimeState.CardRun.PendingReward = FGameXXKPendingRouteCardReward();
		Candidate.RuntimeState.CardRun.bActiveBattleRewardResolved = false;
	}
	if (Source.SaveVersion < BossCardSlotsIntroducedSaveVersion)
	{
		// v17 removed the route-card system (2026-08-14). Discard any persisted merchant
		// card stock and pending card-replacement purchase; BossCardSlots keeps its empty
		// default and removed RouteCardIds/RouteCardEntries simply no longer deserialize.
		Candidate.RuntimeState.CardRun.RouteMerchant = FGameXXKRouteMerchantState();
	}
	if (Source.SaveVersion < DesktopTrainingWorkbenchIntroducedSaveVersion)
	{
		// v18 introduces a separate pure-2D workbench progression namespace;
		// old saves start at the explicitly cleared Normal 1-1 tutorial stage.
		FGameXXKTrainingRules::InitializeNewGame(Candidate.RuntimeState.Training);
	}
	if (Source.SaveVersion < TrainingRewardCooldownsIntroducedSaveVersion
		&& Candidate.RuntimeState.Training.ChallengeRewardSeed == 0)
	{
		// v19 adds only deterministic reward/cooldown state; old Training progress
		// remains authoritative and receives a safe sequence starting point.
		Candidate.RuntimeState.Training.ChallengeRewardSeed = FGameXXKTrainingRules::DefaultChallengeRewardSeed();
	}
	if (Source.SaveVersion < TrainingOfflineCollectionIntroducedSaveVersion)
	{
		// v20 introduces only the closed-window Travel ledger and timestamp. Existing
		// v19 saves have no pending offline rewards and start with a fresh baseline.
		Candidate.RuntimeState.Training.PendingTravelGold = 0;
		Candidate.RuntimeState.Training.PendingTravelExperience = 0;
		Candidate.RuntimeState.Training.PendingTravelNormalChestCount = 0;
		Candidate.RuntimeState.Training.PendingTravelAdvancedChestCount = 0;
		Candidate.RuntimeState.Training.PendingTravelCompletedEncounters = 0;
		Candidate.RuntimeState.Training.PendingTravelCompletedStages = 0;
		Candidate.RuntimeState.Training.PendingTravelSimulatedSeconds = 0;
		Candidate.RuntimeState.Training.bTravelPausedAtDefeat = false;
		Candidate.RuntimeState.Training.TravelLastUpdatedUnixSeconds = 0;
	}
	if (Source.SaveVersion < DesktopInventoryStorageIntroducedSaveVersion)
	{
		// v21 splits the visual left warehouse from the character backpack.
		// Existing unequipped equipment and item stacks remain in the backpack;
		// storage begins empty and deterministic physical cells are generated.
		int32 RequiredLegacyBackpackSlots =
			Candidate.RuntimeState.EquipmentCollection.WarehouseInstanceIds.Num();
		for (const TPair<FName, int32>& Pair : Candidate.RuntimeState.Inventory)
		{
			if (!Pair.Key.IsNone() && Pair.Value > 0
				&& !FGameXXKEquipmentCatalog::FindDefinition(Pair.Key))
			{
				++RequiredLegacyBackpackSlots;
			}
		}
		Candidate.RuntimeState.Talents.MinimumBackpackCapacity = FMath::Clamp(
			FMath::Max(20, RequiredLegacyBackpackSlots),
			20,
			FGameXXKDesktopInventoryRules::BackpackCapacity);
		Candidate.RuntimeState.DesktopInventory = FGameXXKDesktopInventoryState();
		if (!FGameXXKDesktopInventoryRules::Normalize(Candidate.RuntimeState, &MigrationError))
		{
			Fail(OutReport, MigrationError);
			return false;
		}
	}
	if (Source.SaveVersion < QuestNpcEquipmentOwnerIntroducedSaveVersion)
	{
		// v22 makes all six named NPCs permanent account-owned configuration
		// targets. Preserve a valid active v21 selection and deterministically
		// seed the five missing three-card loadouts.
		const int32 SelectionSeed = Candidate.RuntimeState.CardRun.RouteRandomSeed != 0
			? Candidate.RuntimeState.CardRun.RouteRandomSeed
			: FGameXXKTrainingRules::DefaultChallengeRewardSeed();
		if (!FGameXXKCompanionRules::NormalizeOwnedQuestNpcCardLoadouts(
			Candidate.RuntimeState.CardRun.PartySelection,
			SelectionSeed,
			&MigrationError))
		{
			Fail(OutReport, MigrationError);
			return false;
		}
	}
	if (Source.SaveVersion < BattleRetreatCheckpointIntroducedSaveVersion)
	{
		MigrateBattleRetreatCheckpoint(Candidate.RuntimeState, OutReport);
	}
	if (Source.SaveVersion < OrderedPartyFormationIntroducedSaveVersion)
	{
		// Only pre-v24 saves lack authoritative ordered membership. Leave the
		// formation empty until the v30 helper can apply the approved recovery
		// priority after all six persisted NPC loadouts exist.
		Candidate.RuntimeState.CardRun.OrderedFormation = FGameXXKOrderedPartyFormation();
		int32 AddedLegacyPartyCompanions = 0;
		if (!AddMinimumLegacyPartyCompanions(
			Candidate.RuntimeState,
			AddedLegacyPartyCompanions,
			MigrationError))
		{
			Fail(OutReport, MigrationError);
			return false;
		}
		if (AddedLegacyPartyCompanions > 0)
		{
			OutReport.Warnings.Add(FString::Printf(
				TEXT("Legacy ordered-party migration appended %d approved starter companion profile(s)."),
				AddedLegacyPartyCompanions));
		}
	}
	if (Source.SaveVersion < EquipmentToolsAndChestWalletIntroducedSaveVersion)
	{
		// v25 introduces only persistent entry locks and the Tool Auto Fill
		// Warehouse preference in this slice. Every pre-v25 physical cell,
		// equipment/loadout, formation, and runtime field remains authoritative.
		Candidate.RuntimeState.DesktopInventory.LockedEquipmentInstanceIds.Reset();
		Candidate.RuntimeState.DesktopInventory.LockedItemIds.Reset();
		Candidate.RuntimeState.DesktopInventory.bToolAutoFillIncludesWarehouse = true;
	}
	if (Source.SaveVersion < PermanentTalentGraphIntroducedSaveVersion)
	{
		MigrateLegacyTalentProgress(Candidate.RuntimeState, OutReport);
	}
	if (Source.SaveVersion < TutorialQuestIntroducedSaveVersion)
	{
		// Old saves have no onboarding quest. Do not infer it from the legacy
		// Qingshan main quest or reset any existing task/party state.
		Candidate.RuntimeState.TutorialQuest = FGameXXKTutorialQuestProgress();
	}
	if (Source.SaveVersion < DialogueRuntimeIntroducedSaveVersion)
	{
		Candidate.RuntimeState.DialogueSession = FGameXXKDialogueSessionState();
	}
	if (Source.SaveVersion < NarrativeStageGuideIntroducedSaveVersion)
	{
		MigrateLegacyTutorialNarrative(Candidate.RuntimeState, OutReport);
	}
	if (Source.SaveVersion < TutorialMapItemIntroducedSaveVersion)
	{
		Candidate.RuntimeState.DesktopInventory.PendingTaskItemIds.Reset();
	}
	if (Source.SaveVersion < RetiredLegacyTutorialNarrativeSaveVersion)
	{
		RetireLegacyTutorialNarrative(Candidate.RuntimeState, OutReport);
	}
	if (Source.SaveVersion < CombatScalingFoundationIntroducedSaveVersion)
	{
		MigrateCombatScalingFoundation(Candidate.RuntimeState);
	}
	NormalizeTrainingProgress(Candidate.RuntimeState.Training);
	const int32 QuestNpcProgressionSeed = Candidate.RuntimeState.CardRun.RouteRandomSeed != 0
		? Candidate.RuntimeState.CardRun.RouteRandomSeed
		: FGameXXKTrainingRules::DefaultChallengeRewardSeed();
	if (!FGameXXKCompanionRules::NormalizeOwnedQuestNpcCardLoadouts(
			Candidate.RuntimeState.CardRun.PartySelection,
			QuestNpcProgressionSeed,
			&MigrationError)
		|| !FGameXXKEquipmentRules::NormalizeSocketArrays(Candidate.RuntimeState.EquipmentCollection, &MigrationError)
		|| !FGameXXKEquipmentToolRules::NormalizeProgress(Candidate.RuntimeState.ToolProgress)
		|| !MigrateLegacyTrainingChestStacks(Candidate.RuntimeState, MigrationError)
		|| !MigratePermanentNpcFormation(Candidate.RuntimeState, OutReport, MigrationError)
		|| !MigrateRetiredNpcEncounter(Candidate.RuntimeState, MigrationError)
		|| !ValidateRuntimeState(Candidate.RuntimeState, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
	Candidate.SaveVersion = CurrentSaveVersion;
	OutMigrated = MoveTemp(Candidate);
	OutReport.bSucceeded = true;
	return true;
}

bool FGameXXKSaveMigration::TryRestoreRuntimeState(
	const FGameXXKSaveState& Source,
	FGameXXKRuntimeState& OutRuntimeState,
	FGameXXKSaveMigrationReport& OutReport)
{
	OutRuntimeState = FGameXXKRuntimeState();
	FGameXXKSaveState Migrated;
	if (!MigrateToCurrent(Source, Migrated, OutReport))
	{
		return false;
	}
	OutRuntimeState = MoveTemp(Migrated.RuntimeState);
	return true;
}

bool FGameXXKSaveMigration::ValidateRuntimeState(const FGameXXKRuntimeState& State, FString& OutError)
{
	OutError.Reset();
	if (!ValidateNarrativeStageGuideState(State, OutError))
	{
		return false;
	}
	if ((State.TutorialQuest.State == EGameXXKTutorialQuestState::NotStarted
			&& !State.TutorialQuest.CurrentStepId.IsNone())
		|| (State.TutorialQuest.State == EGameXXKTutorialQuestState::Active
			&& State.TutorialQuest.CurrentStepId.IsNone()))
	{
		OutError = TEXT("Saved tutorial quest progress is invalid.");
		return false;
	}
	const FGameXXKDialogueSessionState& Dialogue = State.DialogueSession;
	const bool bHasDialogueContext = !Dialogue.StoryId.IsNone()
		|| Dialogue.StoryVersion != 0
		|| !Dialogue.TaskId.IsNone()
		|| !Dialogue.StepId.IsNone()
		|| !Dialogue.SequenceId.IsNone()
		|| !Dialogue.StageContractId.IsNone()
		|| !Dialogue.DialogueId.IsNone()
		|| Dialogue.DialogueVersion != 0
		|| !Dialogue.CurrentNodeId.IsNone()
		|| !Dialogue.PauseReason.IsEmpty();
	if ((!Dialogue.bActive && bHasDialogueContext)
		|| (Dialogue.bActive
			&& (Dialogue.StoryId.IsNone()
				|| Dialogue.StoryVersion <= 0
				|| Dialogue.TaskId.IsNone()
				|| Dialogue.StepId.IsNone()
				|| Dialogue.SequenceId.IsNone()
				|| Dialogue.StageContractId.IsNone()
				|| Dialogue.DialogueId.IsNone()
				|| Dialogue.DialogueVersion <= 0
				|| Dialogue.CurrentNodeId.IsNone()))
		|| Dialogue.History.Num() > 100
		|| Dialogue.SelectedOptionIds.Contains(NAME_None)
		|| Dialogue.SeenNodeIds.Contains(NAME_None))
	{
		OutError = TEXT("Saved dialogue session is invalid.");
		return false;
	}
	// Accepted-without-follower is a legal current save: the guide NPC stays in
	// town until the player recruits it through the dialog's 入队 action. Legacy
	// saves are still upgraded to a joined follower by the version migration path.
	const int32 EffectiveMaxHP = FMath::Max(
		1,
		State.PlayerMaxHP + FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxHealth));
	const int32 EffectiveMaxMP = FMath::Max(
		1,
		State.PlayerMaxMP + FMath::Max(0, State.CardRun.RouteAttributeBonuses.MaxMana));
	if (State.PlayerLevel < 1 || State.PlayerLevel > FGameXXKCharacterStatRules::MaxCharacterLevel
		|| State.PlayerXP < 0 || State.PlayerGold < 0
		|| State.PlayerMaxHP <= 0 || State.PlayerMaxMP <= 0
		|| State.PlayerHP < 0 || State.PlayerHP > EffectiveMaxHP
		|| State.PlayerMP < 0 || State.PlayerMP > EffectiveMaxMP
		|| State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()) < 0
		|| State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()) < 0
		|| State.EnhancementMaterial != FMath::Max(0, State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()))
		|| State.EquipmentCollection.RefinementSand != FMath::Max(0, State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand())))
	{
		OutError = TEXT("Persistent player resources are invalid.");
		return false;
	}
	for (const TPair<FName, int32>& Pair : State.Inventory)
	{
		if (Pair.Key.IsNone() || Pair.Value < 0)
		{
			OutError = TEXT("Saved inventory contains an invalid item quantity.");
			return false;
		}
	}
	if (!FGameXXKDesktopInventoryRules::Validate(State, &OutError))
	{
		return false;
	}
	if (!FGameXXKTalentRules::ValidateProgress(State.Talents, &OutError))
	{
		return false;
	}
	const int32 LastBackpackSlot = FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
		State,
		EGameXXKDesktopItemContainer::Backpack);
	const int32 LastWarehouseSlot = FGameXXKDesktopInventoryRules::GetLastOccupiedSlotIndex(
		State,
		EGameXXKDesktopItemContainer::Warehouse);
	if (LastBackpackSlot >= FGameXXKTalentRules::GetUnlockedBackpackCapacity(State)
		|| LastWarehouseSlot >= FGameXXKTalentRules::GetUnlockedWarehouseCapacity(State))
	{
		OutError = TEXT("Saved physical inventory exceeds unlocked talent capacity.");
		return false;
	}
	if (!FGameXXKEquipmentToolRules::ValidateProgress(State.ToolProgress, &OutError))
	{
		return false;
	}
	if (!FGameXXKTrainingRules::ValidateChestTokens(State.Training, &OutError))
	{
		return false;
	}
	if (!FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
		State.EquipmentCollection,
		State.CardRun.CompanionRoster,
		&OutError))
	{
		return false;
	}
	if (!FGameXXKMetaShopRules::ValidateState(State, &OutError))
	{
		return false;
	}

	if (!FGameXXKCompanionRules::ValidatePartySelection(
		State.CardRun.CompanionRoster,
		State.CardRun.PartySelection,
		&OutError))
	{
		return false;
	}
	if (!FGameXXKPartyFormationRules::Validate(
		State,
		State.CardRun.OrderedFormation,
		&OutError))
	{
		return false;
	}
	if (!FGameXXKPartyFormationRules::ValidateCompatibilityProjection(State, &OutError))
	{
		return false;
	}
	const FGameXXKCompanionRosterState& Roster = State.CardRun.CompanionRoster;
	if (Roster.PermanentCompanions.Num()
		< FGameXXKPartyFormationRules::MinimumOwnedPermanentCompanions)
	{
		OutError = TEXT("Saved current state must own at least two permanent companions.");
		return false;
	}
	if (Roster.SigilCount < 0 || Roster.RecruitSequenceOrdinal < 0
		|| State.CardRun.NextRewardOrdinal < 0
		|| State.CardRun.NextRelicAcquisitionOrdinal < 0
		|| State.CardRun.RouteAttributeBonuses.MaxHealth < 0
		|| State.CardRun.RouteAttributeBonuses.MaxMana < 0
		|| State.CardRun.RouteAttributeBonuses.Attack < 0
		|| State.CardRun.RouteAttributeBonuses.Defense < 0
		|| State.CardRun.RouteAttributeBonuses.Speed < 0)
	{
		OutError = TEXT("Saved route or companion resources are invalid.");
		return false;
	}
	if (!State.CardRun.ActiveTemporaryQuestNpcId.IsNone())
	{
		OutError = TEXT("Current saves cannot retain retired temporary NPC provenance.");
		return false;
	}
	if (!ValidateRouteEconomyState(State, OutError))
	{
		return false;
	}
	const FGameXXKRouteSettlementReceipt& PendingSettlement = State.CardRun.PendingSettlement;
	if (!IsEmptySettlementReceipt(PendingSettlement)
		&& (!IsValidSettlementReceipt(PendingSettlement)
			|| !State.bDungeonActive
			|| PendingSettlement.SourceTravelMoney != State.CardRun.RouteTravelMoney
			|| PendingSettlement.SourceCardAcquisitionCount
				!= State.CardRun.RouteProgress.ActualRouteCardAcquisitionCount))
	{
		OutError = TEXT("Saved pending route settlement is invalid or detached from its source state.");
		return false;
	}
	if (!FGameXXKRouteMerchantRules::ValidateSavedStock(State, &OutError))
	{
		return false;
	}
	FGameXXKTrainingStageDefinition TrainingDefinition;
	if (!State.Training.UnlockedDifficultyIds.Contains(FGameXXKTrainingRules::DifficultyId(EGameXXKTrainingDifficulty::Normal))
		|| !FGameXXKTrainingRules::TryGetStageDefinition(State.Training.CurrentTravelStageId, TrainingDefinition)
		|| (!State.Training.SelectedStageId.IsNone()
			&& !FGameXXKTrainingRules::TryGetStageDefinition(State.Training.SelectedStageId, TrainingDefinition)))
	{
		OutError = TEXT("Saved Training progress is invalid.");
		return false;
	}
	if (State.Training.bChallengeActive
		&& (!FGameXXKTrainingRules::TryGetStageDefinition(State.Training.ActiveChallengeStageId, TrainingDefinition)
			|| !FGameXXKTrainingRules::BuildEncounterSequence(State.Training.ActiveChallengeStageId).IsValidIndex(State.Training.ActiveChallengeEncounterIndex)))
	{
		OutError = TEXT("Saved Training challenge session is invalid.");
		return false;
	}
	if (State.Training.bTravelActive)
	{
		const TArray<FGameXXKTrainingEncounterDefinition> TravelEncounters =
			FGameXXKTrainingRules::BuildEncounterSequence(State.Training.CurrentTravelStageId, true);
		if (!TravelEncounters.IsValidIndex(State.Training.ActiveTravelEncounterIndex))
		{
			OutError = TEXT("Saved Training travel session is invalid.");
			return false;
		}
	}
	else if (State.Training.ActiveTravelEncounterIndex != INDEX_NONE)
	{
		OutError = TEXT("Saved inactive Training travel session retains an encounter index.");
		return false;
	}
	if (State.Training.bTravelActive && State.Training.bChallengeActive)
	{
		OutError = TEXT("Saved Training challenge and travel sessions cannot be active together.");
		return false;
	}
	if (State.Training.ChallengeRewardSeed == 0
		|| State.Training.TravelNormalChestCooldownRemainingSeconds < 0
		|| State.Training.TravelAdvancedChestCooldownRemainingSeconds < 0
		|| State.Training.PendingTravelGold < 0
		|| State.Training.PendingTravelExperience < 0
		|| State.Training.PendingTravelNormalChestCount < 0
		|| State.Training.PendingTravelAdvancedChestCount < 0
		|| State.Training.PendingTravelCompletedEncounters < 0
		|| State.Training.PendingTravelCompletedStages < 0
		|| State.Training.PendingTravelSimulatedSeconds < 0
		|| State.Training.TravelLastUpdatedUnixSeconds < 0
		|| (State.Training.bTravelPausedAtDefeat && !State.Training.bTravelActive))
	{
		OutError = TEXT("Saved Training reward seed or chest cooldown state is invalid.");
		return false;
	}
	const FGameXXKRouteProgress& RouteProgress = State.CardRun.RouteProgress;
	if (RouteProgress.SchemaVersion < 0
		|| RouteProgress.SchemaVersion > 1
		|| RouteProgress.ActualRouteCardAcquisitionCount < 0)
	{
		OutError = TEXT("Saved three-chapter route progress has an invalid schema or acquisition count.");
		return false;
	}
	if (RouteProgress.SchemaVersion == 0)
	{
		if (RouteProgress.RootSeed != 0
			|| !RouteProgress.ChapterSeeds.IsEmpty()
			|| RouteProgress.CurrentChapter != 0
			|| RouteProgress.RouteCombatLevel != 0
			|| RouteProgress.ActualRouteCardAcquisitionCount != 0)
		{
			OutError = TEXT("Saved inactive three-chapter route progress retains active data.");
			return false;
		}
	}
	else if (!State.bDungeonActive
		|| RouteProgress.RootSeed == 0
		|| RouteProgress.ChapterSeeds.Num() != 3
		|| RouteProgress.ChapterSeeds[0] != RouteProgress.RootSeed
		|| RouteProgress.ChapterSeeds[1] != NormalizeRouteSeedForMigration(FGameXXKEncounterRules::DeriveChapterSeed(RouteProgress.RootSeed, 2))
		|| RouteProgress.ChapterSeeds[2] != NormalizeRouteSeedForMigration(FGameXXKEncounterRules::DeriveChapterSeed(RouteProgress.RootSeed, 3))
		|| RouteProgress.CurrentChapter < 1
		|| RouteProgress.CurrentChapter > 3
		|| RouteProgress.RouteCombatLevel < 1
		|| RouteProgress.RouteCombatLevel > FGameXXKCharacterStatRules::MaxCharacterLevel)
	{
		OutError = TEXT("Saved active three-chapter route progress is incomplete.");
		return false;
	}
	for (const TPair<FName, FGameXXKEnemyBattleState>& Pair : State.CardRun.ActiveBattle.EnemyStates)
	{
		const FGameXXKEnemyBattleState& EnemyState = Pair.Value;
		if (Pair.Key.IsNone()
			|| EnemyState.DefinitionId.IsNone()
			|| EnemyState.IntentCursor < 0
			|| EnemyState.ChargeRoundsRemaining < 0
			|| EnemyState.HealingCooldownRounds < 0
			|| EnemyState.InitiativeBonus < 0)
		{
			OutError = TEXT("Saved enemy battle state is invalid.");
			return false;
		}
	}
	if (!State.CardRun.bHasActiveCardBattle && !State.CardRun.ActiveBattle.EnemyStates.IsEmpty())
	{
		OutError = TEXT("Inactive card battle retains enemy battle state.");
		return false;
	}
	TSet<FName> SeenRelicIds;
	TSet<int32> SeenRelicOrdinals;
	for (const FGameXXKRelicInstance& Relic : State.CardRun.Relics)
	{
		if (Relic.RelicId.IsNone() || !FGameXXKRelicCatalog::FindDefinition(Relic.RelicId)
			|| Relic.Stacks < 1 || Relic.Stacks > 99
			|| Relic.AcquisitionOrdinal < 1
			|| Relic.AcquisitionOrdinal > State.CardRun.NextRelicAcquisitionOrdinal
			|| SeenRelicIds.Contains(Relic.RelicId)
			|| SeenRelicOrdinals.Contains(Relic.AcquisitionOrdinal))
		{
			OutError = TEXT("Saved route relic collection is invalid.");
			return false;
		}
		SeenRelicIds.Add(Relic.RelicId);
		SeenRelicOrdinals.Add(Relic.AcquisitionOrdinal);
	}

	const FGameXXKPendingRouteCardReward& PendingReward = State.CardRun.PendingReward;
	const bool bHasPendingReward = !PendingReward.CardIds.IsEmpty() || !PendingReward.Options.IsEmpty();
	if (!bHasPendingReward)
	{
		if (PendingReward.SourceNodeId != INDEX_NONE
			|| PendingReward.ChoiceSeed != 0)
		{
			OutError = TEXT("Saved pending reward has incomplete metadata.");
			return false;
		}
	}
	else if (!PendingReward.Options.IsEmpty())
	{
		// Tiered battle reward: three typed options on a live victory gate.
		if (PendingReward.SourceNodeId < 0
			|| PendingReward.ChoiceSeed == 0
			|| PendingReward.Options.Num() != 3
			|| !State.CardRun.bHasActiveCardBattle
			|| State.CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory
			|| State.CardRun.bActiveBattleRewardResolved)
		{
			OutError = TEXT("Saved pending battle reward metadata is invalid.");
			return false;
		}
	}
	else
	{
		if (PendingReward.SourceNodeId < 0
			|| PendingReward.ChoiceSeed == 0
			|| PendingReward.CardIds.Num() != 3
			|| !State.CardRun.bHasActiveCardBattle
			|| State.CardRun.ActiveBattle.Phase != EGameXXKCardBattlePhase::Victory
			|| State.CardRun.bActiveBattleRewardResolved)
		{
			OutError = TEXT("Saved pending reward metadata is invalid.");
			return false;
		}
		TSet<FName> SeenRewardCardIds;
		for (const FName CardId : PendingReward.CardIds)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (CardId.IsNone() || SeenRewardCardIds.Contains(CardId)
				|| !Definition || Definition->Owner != EGameXXKCardOwner::Route)
			{
				OutError = TEXT("Saved pending reward contains an unknown, duplicate, or non-route card.");
				return false;
			}
			SeenRewardCardIds.Add(CardId);
		}
	}
	if (!State.CardRun.bHasActiveCardBattle && State.CardRun.bActiveBattleRewardResolved)
	{
		OutError = TEXT("Inactive card battle retains resolved-reward state.");
		return false;
	}

	const FGameXXKPendingRouteEvent& PendingEvent = State.CardRun.PendingEvent;
	const bool bHasPendingEvent = PendingEvent.SourceNodeId != INDEX_NONE;
	const FGameXXKRouteEncounterDefinition* PendingEncounter = nullptr;
	if (!bHasPendingEvent)
	{
		if (PendingEvent.ChoiceSeed != 0
			|| !PendingEvent.EventNpcId.IsNone()
			|| !PendingEvent.EncounterId.IsNone()
			|| PendingEvent.bCanRecruitPermanentCompanion)
		{
			OutError = TEXT("Saved pending event has incomplete metadata.");
			return false;
		}
	}
	else
	{
		PendingEncounter = FGameXXKRouteEncounterCatalog::FindDefinition(PendingEvent.EncounterId);
		if (PendingEvent.SourceNodeId < 0
			|| PendingEvent.ChoiceSeed == 0
			|| !PendingEncounter
			|| PendingEvent.EventNpcId != PendingEncounter->EventNpcId
			|| PendingEvent.bCanRecruitPermanentCompanion)
		{
			OutError = TEXT("Saved pending event contains an unknown encounter or mismatched NPC.");
			return false;
		}
	}

	const FGameXXKPendingRelicOffer& PendingRelicOffer = State.CardRun.PendingRelicOffer;
	const bool bHasPendingRelicOffer = !PendingRelicOffer.RelicIds.IsEmpty();
	if (!bHasPendingRelicOffer)
	{
		if (PendingRelicOffer.SourceNodeId != INDEX_NONE || PendingRelicOffer.ChoiceSeed != 0)
		{
			OutError = TEXT("Saved pending relic offer has incomplete metadata.");
			return false;
		}
	}
	else
	{
		if (PendingRelicOffer.SourceNodeId < 0
			|| PendingRelicOffer.ChoiceSeed == 0
			|| PendingRelicOffer.RelicIds.Num() != 3)
		{
			OutError = TEXT("Saved pending relic offer metadata is invalid.");
			return false;
		}
		TSet<FName> SeenOfferedRelicIds;
		for (const FName RelicId : PendingRelicOffer.RelicIds)
		{
			if (RelicId.IsNone() || SeenOfferedRelicIds.Contains(RelicId)
				|| !FGameXXKRelicCatalog::FindDefinition(RelicId))
			{
				OutError = TEXT("Saved pending relic offer contains an unknown or duplicate relic.");
				return false;
			}
			SeenOfferedRelicIds.Add(RelicId);
		}
	}
	if (bHasPendingReward && bHasPendingEvent)
	{
		OutError = TEXT("Saved route cannot retain a pending reward and event simultaneously.");
		return false;
	}
	if (bHasPendingEvent && State.CardRun.bHasActiveCardBattle)
	{
		OutError = TEXT("Saved route event cannot overlap an active card battle.");
		return false;
	}
	if (bHasPendingRelicOffer)
	{
		if (!PendingEncounter
			|| PendingEncounter->Kind != EGameXXKRouteEncounterKind::Chest
			|| PendingRelicOffer.SourceNodeId != PendingEvent.SourceNodeId
			|| PendingRelicOffer.ChoiceSeed != PendingEvent.ChoiceSeed)
		{
			OutError = TEXT("Saved pending relic offer is detached from its chest event.");
			return false;
		}
	}
	else if (PendingEncounter && PendingEncounter->Kind == EGameXXKRouteEncounterKind::Chest)
	{
		OutError = TEXT("Saved chest event is missing its pending relic offer.");
		return false;
	}
	if (Roster.PendingRecruitment.bHasPendingRecruitment)
	{
		if (!FGameXXKCompanionRules::ValidatePermanentCompanionProfile(
			Roster.PendingRecruitment.Candidate,
			&OutError))
		{
			return false;
		}
		if (Roster.PermanentCompanions.ContainsByPredicate([&Roster](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId == Roster.PendingRecruitment.Candidate.InstanceId;
		}))
		{
			OutError = TEXT("Pending companion recruitment duplicates a roster instance ID.");
			return false;
		}
	}
	if (Roster.PendingRecruitOrder.bHasPendingOrder
		&& Roster.PendingRecruitOrder.ResolvedTemplateId.IsNone())
	{
		OutError = TEXT("Pending companion recruit order is incomplete.");
		return false;
	}

	const TArray<FName>& HeroUnlocked = State.CardRun.HeroUnlockedCardIds;
	const TArray<FName>& HeroSelected = State.CardRun.HeroSelectedCardIds;
	if (!HeroUnlocked.IsEmpty() || !HeroSelected.IsEmpty())
	{
		if (HeroUnlocked.Num() < 8 || HeroSelected.Num() != 8)
		{
			OutError = TEXT("Saved hero card configuration has an invalid size.");
			return false;
		}
		TSet<FName> SeenUnlocked;
		for (const FName CardId : HeroUnlocked)
		{
			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
			if (CardId.IsNone() || SeenUnlocked.Contains(CardId) || !Definition
				|| Definition->Owner != EGameXXKCardOwner::Hero)
			{
				OutError = TEXT("Saved hero card unlocks are invalid.");
				return false;
			}
			SeenUnlocked.Add(CardId);
		}
		TSet<FName> SeenSelected;
		for (const FName CardId : HeroSelected)
		{
			if (!SeenUnlocked.Contains(CardId) || SeenSelected.Contains(CardId))
			{
				OutError = TEXT("Saved hero selected cards are invalid.");
				return false;
			}
			SeenSelected.Add(CardId);
		}
	}

	if (State.CardRun.bHasActiveCardBattle
		&& (!State.CardRun.bLoadoutLockedForRoute
			|| !GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &OutError)))
	{
		if (OutError.IsEmpty())
		{
			OutError = TEXT("An active card battle must retain its locked route loadout.");
		}
		return false;
	}
	if (State.CardRun.NextEnemyIntentIndex < 0
		|| State.CardRun.NextEnemyIntentIndex > State.CardRun.EnemyIntents.Num())
	{
		OutError = TEXT("Saved enemy-intent progress is invalid.");
		return false;
	}
	if (!State.CardRun.bHasActiveCardBattle
		&& (!State.CardRun.EnemyIntents.IsEmpty() || State.CardRun.NextEnemyIntentIndex != 0))
	{
		OutError = TEXT("Inactive card battle retains enemy intent progress.");
		return false;
	}
	if (State.CardRun.bHasActiveCardBattle)
	{
		for (const FGameXXKCardEnemyIntent& Intent : State.CardRun.EnemyIntents)
		{
			const FGameXXKCardCombatUnit* SourceUnit = State.CardRun.ActiveBattle.Units.FindByPredicate([&Intent](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == Intent.SourceUnitId;
			});
			const FGameXXKCardCombatUnit* TargetUnit = Intent.SuggestedTargetUnitId.IsNone()
				? nullptr
				: State.CardRun.ActiveBattle.Units.FindByPredicate([&Intent](const FGameXXKCardCombatUnit& Unit)
				{
					return Unit.UnitId == Intent.SuggestedTargetUnitId;
				});
			if (!SourceUnit || SourceUnit->Side != EGameXXKCardTargetSide::Enemy
				|| (!Intent.SuggestedTargetUnitId.IsNone() && !TargetUnit))
			{
				OutError = TEXT("Saved enemy intent references an invalid combat unit.");
				return false;
			}
		}
	}

	if (!State.bHasGeneratedRouteMap
		&& (!State.RouteMapNodes.IsEmpty()
			|| !State.RouteMapEdges.IsEmpty()
			|| !State.VisitedRouteNodeIds.IsEmpty()
			|| !State.ReachableRouteNodeIds.IsEmpty()
			|| State.BattleEntryCheckpoint.bValid
			|| State.CurrentRouteNodeId != INDEX_NONE
			|| State.PendingRouteNodeId != INDEX_NONE
			|| State.CardRun.ActiveBattleSourceNodeId != INDEX_NONE
			|| PendingReward.SourceNodeId != INDEX_NONE
			|| PendingEvent.SourceNodeId != INDEX_NONE
			|| PendingRelicOffer.SourceNodeId != INDEX_NONE
			|| State.CardRun.RouteMerchant.SourceNodeId != INDEX_NONE))
	{
		OutError = TEXT("A non-generated route retains generated-route references.");
		return false;
	}

	if (State.bHasGeneratedRouteMap)
	{
		if (State.RouteMapNodes.IsEmpty())
		{
			OutError = TEXT("Generated route must contain at least one node.");
			return false;
		}
		TSet<int32> NodeIds;
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			if (Node.NodeId < 0 || NodeIds.Contains(Node.NodeId))
			{
				OutError = TEXT("Generated route contains an invalid or duplicate node ID.");
				return false;
			}
			NodeIds.Add(Node.NodeId);
		}
		TSet<uint64> EdgeKeys;
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			const uint64 EdgeKey = (static_cast<uint64>(static_cast<uint32>(Edge.FromNodeId)) << 32U)
				| static_cast<uint32>(Edge.ToNodeId);
			if (!NodeIds.Contains(Edge.FromNodeId) || !NodeIds.Contains(Edge.ToNodeId) || EdgeKeys.Contains(EdgeKey))
			{
				OutError = TEXT("Generated route contains an invalid or duplicate edge.");
				return false;
			}
			EdgeKeys.Add(EdgeKey);
		}
		for (const FGameXXKRouteMapNode& Node : State.RouteMapNodes)
		{
			TSet<int32> SeenOutgoing;
			for (const int32 ToNodeId : Node.OutgoingNodeIds)
			{
				const uint64 EdgeKey = (static_cast<uint64>(static_cast<uint32>(Node.NodeId)) << 32U)
					| static_cast<uint32>(ToNodeId);
				if (!NodeIds.Contains(ToNodeId) || SeenOutgoing.Contains(ToNodeId) || !EdgeKeys.Contains(EdgeKey))
				{
					OutError = TEXT("Generated route outgoing-node projection is invalid.");
					return false;
				}
				SeenOutgoing.Add(ToNodeId);
			}
		}
		for (const FGameXXKRouteMapEdge& Edge : State.RouteMapEdges)
		{
			const FGameXXKRouteMapNode* FromNode = State.RouteMapNodes.FindByPredicate([&Edge](const FGameXXKRouteMapNode& Node)
			{
				return Node.NodeId == Edge.FromNodeId;
			});
			if (!FromNode || !FromNode->OutgoingNodeIds.Contains(Edge.ToNodeId))
			{
				OutError = TEXT("Generated route edge is absent from its outgoing-node projection.");
				return false;
			}
		}
		const auto ValidateNodeList = [&NodeIds, &OutError](const TArray<int32>& Values, const TCHAR* ErrorText)
		{
			TSet<int32> Seen;
			for (const int32 NodeId : Values)
			{
				if (!NodeIds.Contains(NodeId) || Seen.Contains(NodeId))
				{
					OutError = ErrorText;
					return false;
				}
				Seen.Add(NodeId);
			}
			return true;
		};
		if (!ValidateNodeList(State.VisitedRouteNodeIds, TEXT("Generated route visited-node list is invalid."))
			|| !ValidateNodeList(State.ReachableRouteNodeIds, TEXT("Generated route reachable-node list is invalid."))
			|| (State.CurrentRouteNodeId != INDEX_NONE && !NodeIds.Contains(State.CurrentRouteNodeId))
			|| (State.PendingRouteNodeId != INDEX_NONE && !NodeIds.Contains(State.PendingRouteNodeId))
			|| (State.CardRun.ActiveBattleSourceNodeId != INDEX_NONE && !NodeIds.Contains(State.CardRun.ActiveBattleSourceNodeId)))
		{
			if (OutError.IsEmpty())
			{
				OutError = TEXT("Generated route progress references an unknown node.");
			}
			return false;
		}

		const auto FindRouteNode = [&State](const int32 NodeId)
		{
			return State.RouteMapNodes.FindByPredicate([NodeId](const FGameXXKRouteMapNode& Node)
			{
				return Node.NodeId == NodeId;
			});
		};
		const FGameXXKBattleEntryCheckpoint& Checkpoint = State.BattleEntryCheckpoint;
		if (Checkpoint.bValid)
		{
			const FGameXXKRouteMapNode* SourceNode = FindRouteNode(Checkpoint.SourceNodeId);
			const FGameXXKRouteMapNode* PreviousCurrentNode = FindRouteNode(Checkpoint.PreviousCurrentRouteNodeId);
			const auto ValidateCheckpointNodeList = [&NodeIds](const TArray<int32>& Values)
			{
				TSet<int32> Seen;
				for (const int32 NodeId : Values)
				{
					if (!NodeIds.Contains(NodeId) || Seen.Contains(NodeId))
					{
						return false;
					}
					Seen.Add(NodeId);
				}
				return true;
			};
			if (!State.bDungeonActive
				|| State.Screen != EGameXXKScreen::Battle
				|| !State.CardRun.bHasActiveCardBattle
				|| !SourceNode
				|| !IsBattleRetreatEncounterKind(SourceNode->NodeKind)
				|| Checkpoint.SourceNodeId != State.PendingRouteNodeId
				|| Checkpoint.SourceNodeId != State.CurrentRouteNodeId
				|| Checkpoint.SourceNodeId != State.CardRun.ActiveBattleSourceNodeId
				|| !PreviousCurrentNode
				|| Checkpoint.PreviousDungeonNodeIndex < 0
				|| Checkpoint.PreviousPlayerHP < 0
				|| Checkpoint.PreviousPlayerHP > EffectiveMaxHP
				|| Checkpoint.PreviousPlayerMP < 0
				|| Checkpoint.PreviousPlayerMP > EffectiveMaxMP
				|| !ValidateCheckpointNodeList(Checkpoint.PreviousVisitedRouteNodeIds)
				|| !ValidateCheckpointNodeList(Checkpoint.PreviousReachableRouteNodeIds)
				|| Checkpoint.PreviousVisitedRouteNodeIds.Contains(Checkpoint.SourceNodeId)
				|| !Checkpoint.PreviousReachableRouteNodeIds.Contains(Checkpoint.SourceNodeId))
			{
				OutError = TEXT("Saved battle-entry retreat checkpoint is invalid or detached from its encounter.");
				return false;
			}
		}
		if (State.CardRun.RouteMerchant.SourceNodeId != INDEX_NONE)
		{
			const FGameXXKRouteMapNode* MerchantNode = FindRouteNode(State.CardRun.RouteMerchant.SourceNodeId);
			if (!MerchantNode || MerchantNode->NodeKind != EGameXXKNodeKind::Merchant)
			{
				OutError = TEXT("Saved route merchant references an invalid route node.");
				return false;
			}
		}
		if (bHasPendingReward)
		{
			const FGameXXKRouteMapNode* RewardNode = FindRouteNode(PendingReward.SourceNodeId);
			if (!RewardNode
				|| (RewardNode->NodeKind != EGameXXKNodeKind::Battle
					&& RewardNode->NodeKind != EGameXXKNodeKind::Elite
					&& RewardNode->NodeKind != EGameXXKNodeKind::Boss)
				|| State.CardRun.ActiveBattleSourceNodeId != PendingReward.SourceNodeId
				|| State.PendingRouteNodeId != PendingReward.SourceNodeId)
			{
				OutError = TEXT("Saved pending reward references an invalid route node.");
				return false;
			}
		}
		if (bHasPendingEvent)
		{
			const FGameXXKRouteMapNode* EventNode = FindRouteNode(PendingEvent.SourceNodeId);
			const bool bExpectedChest = PendingEncounter && PendingEncounter->Kind == EGameXXKRouteEncounterKind::Chest;
			if (!EventNode
				|| EventNode->NodeKind != (bExpectedChest ? EGameXXKNodeKind::Chest : EGameXXKNodeKind::Event)
				|| State.PendingRouteNodeId != PendingEvent.SourceNodeId)
			{
				OutError = TEXT("Saved pending event references an invalid route node.");
				return false;
			}
		}
	}

	FGameXXKRuntimeState DerivedMirrors = State;
	if (!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(DerivedMirrors)
		|| DerivedMirrors.EquippedWeapon != State.EquippedWeapon
		|| DerivedMirrors.EquippedArmor != State.EquippedArmor
		|| DerivedMirrors.EquippedAccessory != State.EquippedAccessory
		|| DerivedMirrors.PlayerHP != State.PlayerHP
		|| DerivedMirrors.PlayerMaxHP != State.PlayerMaxHP
		|| DerivedMirrors.PlayerMP != State.PlayerMP
		|| DerivedMirrors.PlayerMaxMP != State.PlayerMaxMP
		|| DerivedMirrors.PlayerAttack != State.PlayerAttack
		|| DerivedMirrors.PlayerDefense != State.PlayerDefense
		|| DerivedMirrors.PlayerSpeed != State.PlayerSpeed
		|| DerivedMirrors.ItemEnhancementLevels.Num() != State.ItemEnhancementLevels.Num())
	{
		OutError = TEXT("Saved legacy equipment compatibility mirrors are invalid.");
		return false;
	}
	for (const TPair<FName, int32>& Pair : DerivedMirrors.ItemEnhancementLevels)
	{
		if (State.ItemEnhancementLevels.FindRef(Pair.Key) != Pair.Value)
		{
			OutError = TEXT("Saved legacy enhancement mirror is invalid.");
			return false;
		}
	}
	for (const FGameXXKEquipmentDefinition& Definition : FGameXXKEquipmentCatalog::GetPackageDefinitions())
	{
		if (State.Inventory.FindRef(Definition.Id) != DerivedMirrors.Inventory.FindRef(Definition.Id))
		{
			OutError = TEXT("Saved legacy equipment count mirror is invalid.");
			return false;
		}
	}

	for (const FName DiscoveredId : State.DiscoveredCodexEntryIds)
	{
		bool bFound = false;
		UGameXXKMVPRules::GetCodexEntryDef(DiscoveredId, bFound);
		if (!bFound)
		{
			OutError = TEXT("Saved codex discovery contains an unknown entry.");
			return false;
		}
	}
	for (const FName ReadId : State.ReadCodexEntryIds)
	{
		if (!State.DiscoveredCodexEntryIds.Contains(ReadId))
		{
			OutError = TEXT("A read codex entry is not discovered.");
			return false;
		}
	}
	return true;
}
