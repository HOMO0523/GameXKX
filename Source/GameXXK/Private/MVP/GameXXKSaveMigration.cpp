#include "MVP/GameXXKSaveMigration.h"

#include "GameXXKCardRules.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEncounterRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKRelicCatalog.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "GameXXKRouteMerchantRules.h"
#include "GameXXKRunDeckRules.h"
#include "Misc/Crc.h"

namespace
{
	const FName GuideId(TEXT("Codex.Guide"));
	const FName MoneyRatId(TEXT("Codex.MoneyRat"));
	const FName BlackBearId(TEXT("Codex.BlackBear"));
	const FName TigerId(TEXT("Codex.Tiger"));
	const FName LegacyBanditId(TEXT("Codex.Bandit"));
	const FName LegacyWolfId(TEXT("Codex.Wolf"));
	const FName LegacyEliteBanditId(TEXT("Codex.EliteBandit"));
	const FName LegacyBossId(TEXT("Codex.Boss"));
	constexpr int32 GuideIntroductionVersion = 5;
	constexpr int32 EnemyCodexVersion = 6;
	constexpr int32 StableMigrationCollectionSeed = 0x4758584B;
	constexpr int32 RouteEntrySeedFallback = 0x13579BDF;
	constexpr uint32 HeroCombatRandomSalt = 0xA341316CU;
	const FName RouteEntryOwnerUnitId(TEXT("Player"));

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
		MigrateHeroCardIds(Run.RouteCardIds);
		MigrateHeroCardIds(Run.PendingReward.CardIds);
		for (FGameXXKRouteCardEntry& Entry : Run.RouteCardEntries)
		{
			if (MigrateHeroCardId(Entry.CardId))
			{
				Entry.CurrentQuality = EGameXXKCardQuality::Common;
			}
		}
		for (FGameXXKCardEnemyIntent& Intent : Run.EnemyIntents)
		{
			MigrateHeroCardId(Intent.CardId);
		}
		MigrateHeroCardBattle(Run.ActiveBattle, Run.bHasActiveCardBattle);
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(InOutState, &OutError);
	}

	void Fail(FGameXXKSaveMigrationReport& Report, const FString& Error)
	{
		Report.bSucceeded = false;
		Report.Error = Error;
	}

	void MigrateLegacyCodexIds(TSet<FName>& EntryIds)
	{
		if (EntryIds.Contains(LegacyBanditId) || EntryIds.Contains(LegacyWolfId))
		{
			EntryIds.Add(MoneyRatId);
		}
		if (EntryIds.Contains(LegacyEliteBanditId))
		{
			EntryIds.Add(BlackBearId);
		}
		if (EntryIds.Contains(LegacyBossId))
		{
			EntryIds.Add(TigerId);
		}
		EntryIds.Remove(LegacyBanditId);
		EntryIds.Remove(LegacyWolfId);
		EntryIds.Remove(LegacyEliteBanditId);
		EntryIds.Remove(LegacyBossId);
	}

	void MigrateInventoryCategories(FGameXXKRuntimeState& State)
	{
		const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
		if (!State.Inventory.Contains(StoneId) && State.EnhancementMaterial > 0)
		{
			State.Inventory.Add(StoneId, State.EnhancementMaterial);
		}
		State.EnhancementMaterial = FMath::Max(0, State.Inventory.FindRef(StoneId));
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
		if (SourceVersion < EnemyCodexVersion)
		{
			MigrateLegacyCodexIds(State.DiscoveredCodexEntryIds);
			MigrateLegacyCodexIds(State.ReadCodexEntryIds);
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

	int32 ResolveRouteEntryMigrationSeed(const FGameXXKRuntimeState& State)
	{
		if (State.CardRun.RouteProgress.RootSeed != 0)
		{
			return State.CardRun.RouteProgress.RootSeed;
		}
		if (State.RouteSeed != 0)
		{
			return State.RouteSeed;
		}
		if (State.CardRun.RouteRandomSeed != 0)
		{
			return State.CardRun.RouteRandomSeed;
		}
		return RouteEntrySeedFallback;
	}

	bool MigrateRouteCardEntries(
		FGameXXKRuntimeState& State,
		FGameXXKSaveMigrationReport& Report,
		FString& OutError)
	{
		OutError.Reset();
		FGameXXKRuntimeState Candidate = State;
		FGameXXKCardRunState& Run = Candidate.CardRun;
		const TArray<FName> LegacyRouteCardIds = Run.RouteCardIds;
		if (!Run.RouteCardEntries.IsEmpty())
		{
			Report.Warnings.Add(TEXT("Discarded nonempty prerelease RouteCardEntries while migrating a pre-v9 save."));
		}
		Run.RouteCardEntries.Reset();

		if (!Candidate.bDungeonActive)
		{
			Run.RouteCardIds.Reset();
			Run.NextRouteCardEntryOrdinal = 0;
			Run.PendingReward.bRequiresRouteCardReplacement = false;
			State = MoveTemp(Candidate);
			return true;
		}

		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &OutError))
		{
			return false;
		}

		FGameXXKCardRunState& InitializedRun = Candidate.CardRun;
		const int32 RouteEntrySeed = ResolveRouteEntryMigrationSeed(Candidate);
		TArray<FGameXXKRouteCardEntry> MigratedEntries;
		if (!FGameXXKRouteCardRecipe::BuildBaseEntries(
			Candidate,
			RouteEntrySeed,
			MigratedEntries,
			&OutError))
		{
			return false;
		}
		if (MigratedEntries.Num() != FGameXXKRouteCardRecipe::BaseEntryCount)
		{
			OutError = TEXT("Route-card entry migration did not produce the canonical eighteen-entry base recipe.");
			return false;
		}

		const int32 MigratedLegacySlotCount = FMath::Min(
			LegacyRouteCardIds.Num(),
			FGameXXKRunDeckRules::MaxRouteCardCapacity);
		if (LegacyRouteCardIds.Num() > FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			Report.Warnings.Add(FString::Printf(
				TEXT("Ignored %d legacy route-card IDs beyond the first 12 migration slots."),
				LegacyRouteCardIds.Num() - FGameXXKRunDeckRules::MaxRouteCardCapacity));
		}

		for (int32 LegacyIndex = 0; LegacyIndex < MigratedLegacySlotCount; ++LegacyIndex)
		{
			const FName LegacyCardId = LegacyRouteCardIds[LegacyIndex];
			if (LegacyCardId.IsNone())
			{
				Report.Warnings.Add(FString::Printf(
					TEXT("Skipped empty legacy RouteCardIds entry at source index %d."),
					LegacyIndex));
				continue;
			}

			const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(LegacyCardId);
			if (!Definition)
			{
				Report.Warnings.Add(FString::Printf(
					TEXT("Skipped unknown legacy route-card ID '%s' at source index %d."),
					*LegacyCardId.ToString(),
					LegacyIndex));
				continue;
			}
			if (Definition->Owner != EGameXXKCardOwner::Route)
			{
				Report.Warnings.Add(FString::Printf(
					TEXT("Skipped non-route legacy card '%s' at source index %d."),
					*LegacyCardId.ToString(),
					LegacyIndex));
				continue;
			}

			FGameXXKRouteCardEntry Entry;
			Entry.CardId = Definition->Id;
			Entry.CurrentQuality = Definition->BaseQuality;
			Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
			Entry.OwnerUnitId = RouteEntryOwnerUnitId;
			Entry.bTemporaryRouteCard = true;
			Entry.bConsumesRouteCapacity = true;
			Entry.AcquisitionOrdinal = FGameXXKRouteCardRecipe::BaseEntryCount + LegacyIndex;
			if (!FGameXXKRouteCardRecipe::MakeStableEntryId(
				RouteEntrySeed,
				Entry.AcquisitionOrdinal,
				Entry.EntryId,
				&OutError))
			{
				return false;
			}

			FGameXXKCardMergePreview AppliedMerge;
			if (!FGameXXKRunDeckRules::AddAndMerge(
				MigratedEntries,
				Entry,
				AppliedMerge,
				&OutError))
			{
				return false;
			}
		}

		const int64 NextFromLegacySlots = static_cast<int64>(FGameXXKRouteCardRecipe::BaseEntryCount)
			+ static_cast<int64>(MigratedLegacySlotCount);
		const int64 NextFromAcquisitionHistory = static_cast<int64>(FGameXXKRouteCardRecipe::BaseEntryCount)
			+ static_cast<int64>(InitializedRun.RouteProgress.ActualRouteCardAcquisitionCount);
		const int64 NextEntryOrdinal = FMath::Max(NextFromLegacySlots, NextFromAcquisitionHistory);
		if (NextEntryOrdinal < 0 || NextEntryOrdinal > MAX_int32)
		{
			OutError = TEXT("Migrated route-card next entry ordinal exceeds its persisted int32 range.");
			return false;
		}

		InitializedRun.RouteCardEntries = MoveTemp(MigratedEntries);
		InitializedRun.NextRouteCardEntryOrdinal = static_cast<int32>(NextEntryOrdinal);
		InitializedRun.RouteCardIds.Reset();
		InitializedRun.PendingReward.bRequiresRouteCardReplacement = false;
		State = MoveTemp(Candidate);
		return true;
	}

	bool ValidateRouteCardEntryState(const FGameXXKRuntimeState& State, FString& OutError)
	{
		const FGameXXKCardRunState& Run = State.CardRun;
		if (!State.bDungeonActive)
		{
			if (!Run.RouteCardEntries.IsEmpty() || Run.NextRouteCardEntryOrdinal != 0)
			{
				OutError = TEXT("Saved inactive route retains stable route-card entry state.");
				return false;
			}
			return true;
		}
		if (Run.RouteCardEntries.IsEmpty())
		{
			return true;
		}

		int32 CapacityUsed = 0;
		if (!FGameXXKRunDeckRules::GetCapacityUsed(Run.RouteCardEntries, CapacityUsed, &OutError))
		{
			return false;
		}
		if (CapacityUsed > FGameXXKRunDeckRules::MaxRouteCardCapacity)
		{
			OutError = TEXT("Saved stable route-card entries exceed route capacity.");
			return false;
		}

		int32 MaximumAcquisitionOrdinal = INDEX_NONE;
		for (const FGameXXKRouteCardEntry& Entry : Run.RouteCardEntries)
		{
			if (!FGameXXKCardCatalog::FindCardDefinition(Entry.CardId))
			{
				OutError = TEXT("Saved stable route-card entry references an unknown catalog card.");
				return false;
			}
			if (Entry.OwnerUnitId.IsNone())
			{
				OutError = TEXT("Saved stable route-card entry has an empty owner.");
				return false;
			}
			MaximumAcquisitionOrdinal = FMath::Max(MaximumAcquisitionOrdinal, Entry.AcquisitionOrdinal);
		}
		if (Run.NextRouteCardEntryOrdinal <= MaximumAcquisitionOrdinal)
		{
			OutError = TEXT("Saved stable route-card next ordinal does not follow every persisted entry.");
			return false;
		}
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
		RouteProgress.RouteCombatLevel = FMath::Clamp(State.PlayerLevel, 1, 20);
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
		NormalizeProgression(State);
		return State;
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
		FString ValidationError;
		if (!ValidateRuntimeState(Source.RuntimeState, ValidationError))
		{
			Fail(OutReport, ValidationError);
			return false;
		}
		OutMigrated = Source;
		OutReport.bSucceeded = true;
		return true;
	}

	FGameXXKSaveState Candidate = Source;
	Candidate.RuntimeState = RestoreOldChain(Source);
	FString MigrationError;
	if (Source.SaveVersion < HeroCardPoolIntroducedSaveVersion
		&& !MigrateHeroCardPool(Candidate.RuntimeState, MigrationError))
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
	if (Source.SaveVersion < RouteCardEntriesIntroducedSaveVersion
		&& !MigrateRouteCardEntries(Candidate.RuntimeState, OutReport, MigrationError))
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
	Candidate.SaveVersion = CurrentSaveVersion;
	if (!ValidateRuntimeState(Candidate.RuntimeState, MigrationError))
	{
		Fail(OutReport, MigrationError);
		return false;
	}
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
		|| State.EnhancementMaterial != FMath::Max(0, State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone())))
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
	const FGameXXKCompanionRosterState& Roster = State.CardRun.CompanionRoster;
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
	if (State.CardRun.ActiveTemporaryQuestNpcId != State.CardRun.PartySelection.QuestNpc.NpcId)
	{
		OutError = TEXT("Saved task-NPC route provenance is inconsistent.");
		return false;
	}
	if (!ValidateRouteEconomyState(State, OutError))
	{
		return false;
	}
	if (!ValidateRouteCardEntryState(State, OutError))
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
		|| RouteProgress.RouteCombatLevel > 20)
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
	const bool bHasPendingReward = !PendingReward.CardIds.IsEmpty();
	if (!bHasPendingReward)
	{
		if (PendingReward.SourceNodeId != INDEX_NONE
			|| PendingReward.ChoiceSeed != 0)
		{
			OutError = TEXT("Saved pending reward has incomplete metadata.");
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
	for (const FName RouteCardId : State.CardRun.RouteCardIds)
	{
		if (RouteCardId.IsNone() || !FGameXXKCardCatalog::FindCardDefinition(RouteCardId))
		{
			OutError = TEXT("Saved route card collection contains an unknown card.");
			return false;
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
