#include "MVP/GameXXKMVPSubsystem.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEnemyCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteSettlementRules.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameXXKMVPSubsystem, Log, All);

namespace
{
	static constexpr int32 ManualSaveSlotCount = 5;
	static constexpr int32 MaximumMigrationBackupAttempts = 999;
	static const FString ManualSaveSlotPrefix(TEXT("GameXXK_MVP_SaveSlot_"));
	static const FString DefaultSaveSlotName(TEXT("GameXXK_MVP_SaveSlot_1"));
	static const FName DesktopTrainingWorkbenchMapId(TEXT("DesktopTrainingHUD"));

	static EGameXXKNodeKind TrainingNodeKind(const EGameXXKTrainingEncounterKind EncounterKind)
	{
		switch (EncounterKind)
		{
		case EGameXXKTrainingEncounterKind::Elite:
			return EGameXXKNodeKind::Elite;
		case EGameXXKTrainingEncounterKind::Boss:
			return EGameXXKNodeKind::Boss;
		default:
			return EGameXXKNodeKind::Battle;
		}
	}

	static bool ApplyTrainingRewardToRuntime(FGameXXKRuntimeState& State, const FGameXXKTrainingReward& Reward)
	{
		State.PlayerGold = FMath::Max(0, State.PlayerGold + Reward.Gold);
		State.PlayerXP = FMath::Max(0, State.PlayerXP + Reward.Experience);
		if (!Reward.bChestRolled)
		{
			return true;
		}
		return !Reward.ChestItemId.IsNone()
			&& UGameXXKMVPRules::AddItem(State, Reward.ChestItemId, 1);
	}

	static bool BuildTrainingTravelParty(
		const FGameXXKRuntimeState& State,
		TArray<FGameXXKTrainingTravelPartyUnitRuntime>& OutParty,
		FString* OutError = nullptr)
	{
		OutParty.Reset();
		OutParty.Add(FGameXXKTrainingTravelPartyUnitRuntime(
			FGameXXKEquipmentRules::HeroCharacterId(),
			State.PlayerHP,
			State.PlayerMaxHP,
			State.PlayerAttack));

		const FName ActiveCompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		const FGameXXKPermanentCompanion* Companion =
			State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
				[ActiveCompanionId](const FGameXXKPermanentCompanion& Candidate)
				{
					return !ActiveCompanionId.IsNone()
						&& Candidate.InstanceId == ActiveCompanionId
						&& Candidate.bIsActive;
				});
		if (Companion)
		{
			FGameXXKCharacterStats BareStats;
			FGameXXKEquipmentLoadoutSnapshot Snapshot;
			if (!FGameXXKCharacterStatRules::GetBareCompanionStats(
					Companion->Role,
					Companion->Level,
					Companion->Star,
					BareStats,
					OutError)
				|| !FGameXXKEquipmentRules::BuildLoadoutSnapshot(
					State.EquipmentCollection,
					Companion->InstanceId,
					BareStats,
					Snapshot,
					OutError))
			{
				return false;
			}
			OutParty.Add(FGameXXKTrainingTravelPartyUnitRuntime(
				Companion->InstanceId,
				Snapshot.AttributesBeforeRoute.MaxHealth,
				Snapshot.AttributesBeforeRoute.MaxHealth,
				Snapshot.AttributesBeforeRoute.Attack));
		}

		const FName ActiveNpcId = State.CardRun.ActiveTemporaryQuestNpcId.IsNone()
			? FName(TEXT("Npc.TusiChief"))
			: State.CardRun.ActiveTemporaryQuestNpcId;
		FGameXXKCompanionAttributes NpcAttributes;
		if (!FGameXXKCompanionRules::GetQuestNpcAttributes(
				ActiveNpcId,
				State.PlayerLevel,
				NpcAttributes,
				OutError))
		{
			return false;
		}
		FGameXXKCharacterStats NpcBareStats;
		NpcBareStats.MaxHealth = NpcAttributes.Health;
		NpcBareStats.MaxMana = NpcAttributes.Mana;
		NpcBareStats.Attack = NpcAttributes.Attack;
		NpcBareStats.Defense = NpcAttributes.Defense;
		NpcBareStats.Speed = NpcAttributes.Speed;
		FGameXXKEquipmentLoadoutSnapshot NpcSnapshot;
		if (!FGameXXKEquipmentRules::BuildLoadoutSnapshot(
				State.EquipmentCollection,
				ActiveNpcId,
				NpcBareStats,
				NpcSnapshot,
				OutError))
		{
			return false;
		}
		OutParty.Add(FGameXXKTrainingTravelPartyUnitRuntime(
			ActiveNpcId,
			NpcSnapshot.AttributesBeforeRoute.MaxHealth,
			NpcSnapshot.AttributesBeforeRoute.MaxHealth,
			NpcSnapshot.AttributesBeforeRoute.Attack));
		return OutParty.Num() == 3;
	}

	static bool AddTrainingChestCount(FGameXXKRuntimeState& State, const FName ChestItemId, const int32 Count)
	{
		for (int32 Index = 0; Index < FMath::Max(0, Count); ++Index)
		{
			if (!UGameXXKMVPRules::AddItem(State, ChestItemId, 1))
			{
				return false;
			}
		}
		return true;
	}

	static FName MakeTrainingEnemyRuntimeId(const FName DefinitionId, const int32 EncounterIndex)
	{
		FString Leaf = DefinitionId.ToString();
		int32 SeparatorIndex = INDEX_NONE;
		if (Leaf.FindLastChar(TEXT('.'), SeparatorIndex))
		{
			Leaf = Leaf.RightChop(SeparatorIndex + 1);
		}
		return FName(*FString::Printf(TEXT("TrainingEnemy.%s.%d"), *Leaf, EncounterIndex + 1));
	}

	static bool BuildTrainingEnemyProjection(
		const FName EnemyDefinitionId,
		const int32 CombatLevel,
		const int32 FormationSlotIndex,
		FGameXXKBattleRuntimeUnit& OutEnemy,
		FString* OutError)
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(EnemyDefinitionId);
		if (!Definition)
		{
			if (OutError)
			{
				*OutError = FString::Printf(TEXT("Training encounter enemy is not in the catalog: %s"), *EnemyDefinitionId.ToString());
			}
			return false;
		}

		const FGameXXKEnemyComputedStats Stats = FGameXXKEnemyCatalog::ComputeStats(Definition->Id, FMath::Max(1, CombatLevel));
		OutEnemy = FGameXXKBattleRuntimeUnit();
		OutEnemy.Id = MakeTrainingEnemyRuntimeId(Definition->Id, FormationSlotIndex);
		OutEnemy.DisplayName = Definition->DisplayName;
		OutEnemy.HP = FMath::Max(1, Stats.MaxHP);
		OutEnemy.MaxHP = OutEnemy.HP;
		OutEnemy.Attack = FMath::Max(1, Stats.Attack);
		OutEnemy.Defense = FMath::Max(0, Stats.Defense);
		OutEnemy.Speed = FMath::Max(1, Stats.Speed);
		OutEnemy.MP = 0;
		OutEnemy.MaxMP = 0;
		OutEnemy.Shield = 0;
		OutEnemy.bEnemy = true;
		OutEnemy.bDefeated = false;
		OutEnemy.EnemyDefinitionId = Definition->Id;
		OutEnemy.BattleSlotNumber = FormationSlotIndex + 1;
		OutEnemy.CombatLevel = FMath::Max(1, CombatLevel);
		return true;
	}

	static bool BeginTrainingEncounterBattle(
		FGameXXKRuntimeState& InOutState,
		const FName StageId,
		const int32 EncounterIndex,
		FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(StageId, false);
		if (!Encounters.IsValidIndex(EncounterIndex))
		{
			if (OutError)
			{
				*OutError = TEXT("Training challenge encounter index is invalid.");
			}
			return false;
		}
		if (InOutState.CardRun.bHasActiveCardBattle)
		{
			if (OutError)
			{
				*OutError = TEXT("Training challenge cannot replace an active card battle.");
			}
			return false;
		}

		const FGameXXKTrainingEncounterDefinition& Encounter = Encounters[EncounterIndex];
		TArray<FName> Formation = Encounter.EnemyDefinitionIds;
		if (Formation.IsEmpty())
		{
			Formation.Add(Encounter.EnemyDefinitionId);
		}
		if (Formation.IsEmpty() || Formation.Num() > 3)
		{
			if (OutError)
			{
				*OutError = TEXT("Training challenge formation must contain between one and three enemies.");
			}
			return false;
		}
		TArray<FGameXXKBattleRuntimeUnit> ProjectedEnemies;
		ProjectedEnemies.Reserve(Formation.Num());
		for (int32 FormationSlotIndex = 0; FormationSlotIndex < Formation.Num(); ++FormationSlotIndex)
		{
			FGameXXKBattleRuntimeUnit Enemy;
			if (!BuildTrainingEnemyProjection(
				Formation[FormationSlotIndex],
				InOutState.PlayerLevel,
				FormationSlotIndex,
				Enemy,
				OutError))
			{
				return false;
			}
			ProjectedEnemies.Add(MoveTemp(Enemy));
		}

		InOutState.bHasActiveBattle = true;
		InOutState.ActiveBattleNodeId = -100000 - EncounterIndex;
		InOutState.ActiveBattleEnemies = MoveTemp(ProjectedEnemies);
		InOutState.ActiveBattleParty.Reset();
		InOutState.PendingRouteNodeId = INDEX_NONE;
		InOutState.bDungeonActive = false;
		InOutState.Screen = EGameXXKScreen::Battle;
		InOutState.CurrentMapId = TEXT("TrainingBattle");
		InOutState.TownPanelMode = EGameXXKTownPanelMode::None;

		const EGameXXKNodeKind NodeKind = TrainingNodeKind(Encounter.Kind);
		const int32 BattleSeed = FGameXXKCardBattleAdapter::MixBattleSeed(
			InOutState.RouteSeed != 0 ? InOutState.RouteSeed : 0x13579BDF,
			InOutState.ActiveBattleNodeId);
		if (!FGameXXKCardBattleAdapter::BeginCardBattle(
			InOutState,
			NodeKind,
			NodeKind == EGameXXKNodeKind::Boss ? EGameXXKCardTerrain::Cave : EGameXXKCardTerrain::Plain,
			BattleSeed,
			OutError))
		{
			InOutState.bHasActiveBattle = false;
			InOutState.ActiveBattleNodeId = INDEX_NONE;
			InOutState.ActiveBattleEnemies.Reset();
			return false;
		}
		return true;
	}

	static void ClearTrainingBattleProjection(FGameXXKRuntimeState& InOutState)
	{
		FGameXXKCardBattleAdapter::ClearActiveCardBattle(InOutState);
		InOutState.bHasActiveBattle = false;
		InOutState.ActiveBattleNodeId = INDEX_NONE;
		InOutState.ActiveBattleParty.Reset();
		InOutState.ActiveBattleEnemies.Reset();
		InOutState.CardRun.bLoadoutLockedForRoute = false;
	}

	static void ReturnTrainingToWorkbench(FGameXXKRuntimeState& InOutState)
	{
		InOutState.bDungeonActive = false;
		InOutState.PendingRouteNodeId = INDEX_NONE;
		InOutState.BattleEntryCheckpoint = FGameXXKBattleEntryCheckpoint{};
		InOutState.Screen = EGameXXKScreen::Town;
		InOutState.CurrentMapId = DesktopTrainingWorkbenchMapId;
		InOutState.TownPanelMode = EGameXXKTownPanelMode::None;
	}

	static bool ResolveTrainingPendingCardChoice(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		if (!InOutState.CardRun.bHasActiveCardBattle)
		{
			if (OutError)
			{
				*OutError = TEXT("Training challenge has no active card battle for its pending choice.");
			}
			return false;
		}

		FGameXXKCardBattleRuntime& Runtime = InOutState.CardRun.ActiveBattle;
		const FGameXXKPendingCardChoice Pending = Runtime.Deck.PendingChoice;
		TArray<FGameXXKCardPlayResult> ResumedResults;
		switch (Pending.Kind)
		{
		case EGameXXKCardPendingChoiceKind::ForcedDiscard:
		{
			TArray<FName> DiscardedInstanceIds;
			for (const FGameXXKCardInstance& Candidate : Pending.Candidates)
			{
				if (DiscardedInstanceIds.Num() >= Pending.RequiredCount)
				{
					break;
				}
				if (Runtime.Deck.Hand.ContainsByPredicate([&Candidate](const FGameXXKCardInstance& HandCard)
				{
					return HandCard.InstanceId == Candidate.InstanceId;
				}))
				{
					DiscardedInstanceIds.Add(Candidate.InstanceId);
				}
			}
			if (DiscardedInstanceIds.Num() != Pending.RequiredCount)
			{
				if (OutError)
				{
					*OutError = TEXT("Training auto battle could not select the required forced-discard cards.");
				}
				return false;
			}
			return FGameXXKCardBattleAdapter::SubmitForcedDiscard(
				InOutState,
				DiscardedInstanceIds,
				OutError,
				&ResumedResults);
		}

		case EGameXXKCardPendingChoiceKind::InsightChooseToHand:
		{
			if (Pending.InsightTopOrder.IsEmpty())
			{
				if (OutError)
				{
					*OutError = TEXT("Training auto battle opened insight without stable candidates.");
				}
				return false;
			}
			const FName PickedInstanceId = Pending.InsightTopOrder[0];
			TArray<FName> RemainingInstanceIds = Pending.InsightTopOrder;
			RemainingInstanceIds.RemoveAt(0);
			return FGameXXKCardBattleAdapter::SubmitInsightChoice(
				InOutState,
				PickedInstanceId,
				RemainingInstanceIds,
				OutError,
				&ResumedResults);
		}

		case EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand:
		{
			if (Pending.Candidates.IsEmpty())
			{
				if (OutError)
				{
					*OutError = TEXT("Training auto battle opened Hero task search without stable candidates.");
				}
				return false;
			}
			TArray<FGameXXKCardInstance> Candidates = Pending.Candidates;
			Candidates.Sort([](const FGameXXKCardInstance& Left, const FGameXXKCardInstance& Right)
			{
				return Left.AcquisitionOrdinal != Right.AcquisitionOrdinal
					? Left.AcquisitionOrdinal < Right.AcquisitionOrdinal
					: Left.InstanceId.LexicalLess(Right.InstanceId);
			});
			return FGameXXKCardBattleAdapter::SubmitHeroTaskSearchChoice(
				InOutState,
				Candidates[0].InstanceId,
				ResumedResults,
				OutError);
		}

		case EGameXXKCardPendingChoiceKind::None:
			if (!Runtime.AutomaticResolutionQueue.bActive)
			{
				return true;
			}
			return FGameXXKCardBattleAdapter::ResumeAutomaticResolutionQueue(
				InOutState,
				ResumedResults,
				OutError);

		case EGameXXKCardPendingChoiceKind::Invalid:
		default:
			if (OutError)
			{
				*OutError = TEXT("Training auto battle encountered an invalid pending card choice.");
			}
			return false;
		}
	}

	static bool AdvanceTrainingCardBattleStep(FGameXXKRuntimeState& InOutState, FString* OutError)
	{
		if (OutError)
		{
			OutError->Reset();
		}
		if (!InOutState.CardRun.bHasActiveCardBattle)
		{
			if (OutError)
			{
				*OutError = TEXT("Training challenge has no active card battle.");
			}
			return false;
		}

		FGameXXKCardBattleRuntime& Runtime = InOutState.CardRun.ActiveBattle;
		if (Runtime.Phase == EGameXXKCardBattlePhase::Player)
		{
			for (int32 ChoiceGuard = 0;
				ChoiceGuard < 128
				&& (GameXXKCardRules::HasPendingChoice(Runtime.Deck) || Runtime.AutomaticResolutionQueue.bActive);
				++ChoiceGuard)
			{
				if (!ResolveTrainingPendingCardChoice(InOutState, OutError))
				{
					return false;
				}
			}
			if (GameXXKCardRules::HasPendingChoice(Runtime.Deck) || Runtime.AutomaticResolutionQueue.bActive)
			{
				if (OutError)
				{
					*OutError = TEXT("Training auto battle exceeded the pending-choice resolution guard.");
				}
				return false;
			}

			const TArray<FGameXXKCardInstance> HandSnapshot = Runtime.Deck.Hand;
			for (const FGameXXKCardInstance& Card : HandSnapshot)
			{
				FGameXXKCardPlayPreview Preview;
				FString PreviewError;
				if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(InOutState, Card.InstanceId, Preview, &PreviewError)
					|| !Preview.bCanPlay)
				{
					continue;
				}

				FName TargetId = NAME_None;
				if (Preview.TargetRequest.bRequiresManualSelection)
				{
					for (const FGameXXKCardTargetCandidateView& Candidate : Preview.TargetRequest.CandidateViews)
					{
						if (Candidate.bCanSelect && !Candidate.UnitId.IsNone())
						{
							TargetId = Candidate.UnitId;
							break;
						}
					}
					if (TargetId.IsNone())
					{
						continue;
					}
				}

				FGameXXKCardPlayResult Result;
				if (FGameXXKCardBattleAdapter::ResolveCardPlay(InOutState, Card.InstanceId, TargetId, Result, OutError))
				{
					// A successfully played automatic card may itself create a forced
					// discard/search/selection. Drain that continuation in the same
					// logical auto step instead of leaving the UI blocked until the
					// next one-second Travel/Challenge tick.
					for (int32 ChoiceGuard = 0;
						ChoiceGuard < 128
							&& (GameXXKCardRules::HasPendingChoice(InOutState.CardRun.ActiveBattle.Deck)
								|| InOutState.CardRun.ActiveBattle.AutomaticResolutionQueue.bActive);
						++ChoiceGuard)
					{
						if (!ResolveTrainingPendingCardChoice(InOutState, OutError))
						{
							return false;
						}
					}
					if (GameXXKCardRules::HasPendingChoice(InOutState.CardRun.ActiveBattle.Deck)
						|| InOutState.CardRun.ActiveBattle.AutomaticResolutionQueue.bActive)
					{
						if (OutError)
						{
							*OutError = TEXT("Training auto battle exceeded the post-play pending-choice resolution guard.");
						}
						return false;
					}
					return true;
				}
			}

			TArray<FGameXXKCardDamageResult> DamageResults;
			return FGameXXKCardBattleAdapter::EndPlayerCardPhase(InOutState, DamageResults, OutError);
		}

		if (Runtime.Phase == EGameXXKCardBattlePhase::Enemy)
		{
			TArray<FGameXXKCardDamageResult> DamageResults;
			return FGameXXKCardBattleAdapter::ResolveEnemyPhase(InOutState, DamageResults, OutError);
		}

		return true;
	}

	static FString ResolveSaveSlotName(const FString& SlotName)
	{
		return SlotName.IsEmpty() ? UGameXXKMVPSubsystem::GetDefaultSaveSlotName() : SlotName;
	}

	static FString BuildMigrationBackupBaseSlotName(const FString& SlotName)
	{
		return FString::Printf(
			TEXT("%s.PreV%dBackup"),
			*SlotName,
			FGameXXKSaveMigration::CurrentSaveVersion);
	}

	static FString BuildMigrationBackupAttemptSlotName(const FString& BaseSlotName, const int32 AttemptNumber)
	{
		return AttemptNumber == 0
			? BaseSlotName
			: FString::Printf(TEXT("%s.%03d"), *BaseSlotName, AttemptNumber);
	}

	static bool TryGetSaveObjectChecksum(USaveGame* SaveGame, uint32& OutChecksum)
	{
		OutChecksum = 0;
		TArray<uint8> Bytes;
		if (!SaveGame || !UGameplayStatics::SaveGameToMemory(SaveGame, Bytes))
		{
			return false;
		}
		OutChecksum = FCrc::MemCrc32(Bytes.GetData(), Bytes.Num());
		return true;
	}

	static bool AreSaveObjectsSerializationEquivalent(USaveGame* Left, USaveGame* Right)
	{
		if (!Left || !Right || Left->GetClass() != Right->GetClass())
		{
			return false;
		}
		TArray<uint8> LeftBytes;
		TArray<uint8> RightBytes;
		return UGameplayStatics::SaveGameToMemory(Left, LeftBytes)
			&& UGameplayStatics::SaveGameToMemory(Right, RightBytes)
			&& LeftBytes == RightBytes;
	}

	static APawn* GetLivePlayerPawnForSave(const UGameXXKMVPSubsystem* Subsystem)
	{
		UWorld* World = Subsystem ? Subsystem->GetWorld() : nullptr;
		return World && World->IsGameWorld() ? UGameplayStatics::GetPlayerPawn(World, 0) : nullptr;
	}

	static bool IsCompanionConfigurationLocked(const FGameXXKRuntimeState& RuntimeState)
	{
		return RuntimeState.CardRun.bLoadoutLockedForRoute
			|| RuntimeState.CardRun.bHasActiveCardBattle
			|| RuntimeState.bHasActiveBattle
			|| RuntimeState.Screen == EGameXXKScreen::Battle;
	}

	static bool IsTownCompanionConfigurationAvailable(const FGameXXKRuntimeState& RuntimeState)
	{
		return RuntimeState.Screen == EGameXXKScreen::Town && !IsCompanionConfigurationLocked(RuntimeState);
	}

	static bool ResolvePermanentEquipmentOwnerBareStats(
		const FGameXXKRuntimeState& RuntimeState,
		const FName CharacterId,
		FGameXXKCharacterStats& OutBareStats)
	{
		OutBareStats = FGameXXKCharacterStats();
		if (CharacterId == FGameXXKEquipmentRules::HeroCharacterId())
		{
			OutBareStats = FGameXXKCharacterStatRules::GetBareHeroStats(RuntimeState.PlayerLevel);
			return true;
		}
		if (FGameXXKCompanionCatalog::FindQuestNpcDefinition(CharacterId))
		{
			FGameXXKCompanionAttributes Attributes;
			if (!FGameXXKCompanionRules::GetQuestNpcAttributes(
				CharacterId,
				RuntimeState.PlayerLevel,
				Attributes))
			{
				return false;
			}
			OutBareStats.MaxHealth = Attributes.Health;
			OutBareStats.MaxMana = Attributes.Mana;
			OutBareStats.Attack = Attributes.Attack;
			OutBareStats.Defense = Attributes.Defense;
			OutBareStats.Speed = Attributes.Speed;
			return OutBareStats.IsValidProjectionInput();
		}

		const FGameXXKPermanentCompanion* Companion = RuntimeState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[CharacterId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == CharacterId;
			});
		return Companion && FGameXXKCharacterStatRules::GetBareCompanionStats(
			Companion->Role,
			Companion->Level,
			Companion->Star,
			OutBareStats);
	}

	static FGameXXKPermanentCompanion* FindPermanentCompanion(FGameXXKRuntimeState& RuntimeState, const FName InstanceId)
	{
		return RuntimeState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([InstanceId](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == InstanceId;
		});
	}

	static bool RepairFormationAfterCompanionRemoval(
		FGameXXKRuntimeState& State,
		const FName RemovedInstanceId,
		const TArray<FName>& PreferredReplacementIds,
		const FName RequestedFirstCompanionId,
		FString& OutError)
	{
		OutError.Reset();
		FGameXXKOrderedPartyFormation& Formation = State.CardRun.OrderedFormation;
		TArray<int32> RemovedFormationSlots;
		TSet<FName> ReservedFormationMemberIds;
		for (int32 SlotIndex = 0; SlotIndex < Formation.Members.Num(); ++SlotIndex)
		{
			const FGameXXKPartyMemberRef& Ref = Formation.Members[SlotIndex];
			if (Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
				&& Ref.MemberId == RemovedInstanceId)
			{
				RemovedFormationSlots.Add(SlotIndex);
			}
			else if (!Ref.MemberId.IsNone())
			{
				ReservedFormationMemberIds.Add(Ref.MemberId);
			}
		}

		TArray<FName> StableOwnedCompanionIds;
		for (const FGameXXKPermanentCompanion& Companion : State.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (!Companion.InstanceId.IsNone())
			{
				StableOwnedCompanionIds.AddUnique(Companion.InstanceId);
			}
		}
		StableOwnedCompanionIds.Sort(FNameLexicalLess());
		const auto IsAvailableReplacement = [&StableOwnedCompanionIds, &ReservedFormationMemberIds](const FName InstanceId)
		{
			return !InstanceId.IsNone()
				&& StableOwnedCompanionIds.Contains(InstanceId)
				&& !ReservedFormationMemberIds.Contains(InstanceId);
		};
		for (const int32 SlotIndex : RemovedFormationSlots)
		{
			FName ReplacementInstanceId = NAME_None;
			for (const FName PreferredId : PreferredReplacementIds)
			{
				if (IsAvailableReplacement(PreferredId))
				{
					ReplacementInstanceId = PreferredId;
					break;
				}
			}
			if (ReplacementInstanceId.IsNone())
			{
				for (const FName OwnedInstanceId : StableOwnedCompanionIds)
				{
					if (IsAvailableReplacement(OwnedInstanceId))
					{
						ReplacementInstanceId = OwnedInstanceId;
						break;
					}
				}
			}
			if (ReplacementInstanceId.IsNone())
			{
				OutError = TEXT("No unique owned companion can replace the removed formation member.");
				return false;
			}
			Formation.Members[SlotIndex].Kind = EGameXXKPartyMemberKind::PermanentCompanion;
			Formation.Members[SlotIndex].MemberId = ReplacementInstanceId;
			ReservedFormationMemberIds.Add(ReplacementInstanceId);
		}

		if (!RequestedFirstCompanionId.IsNone())
		{
			const int32 FirstCompanionSlot = Formation.Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
			{
				return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
			});
			const int32 RequestedActiveSlot = Formation.Members.IndexOfByPredicate(
				[RequestedFirstCompanionId](const FGameXXKPartyMemberRef& Ref)
				{
					return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
						&& Ref.MemberId == RequestedFirstCompanionId;
				});
			if (FirstCompanionSlot == INDEX_NONE
				|| !StableOwnedCompanionIds.Contains(RequestedFirstCompanionId))
			{
				OutError = TEXT("Requested active companion is not an owned legal post-replacement companion.");
				return false;
			}
			if (RequestedActiveSlot == INDEX_NONE)
			{
				Formation.Members[FirstCompanionSlot].Kind = EGameXXKPartyMemberKind::PermanentCompanion;
				Formation.Members[FirstCompanionSlot].MemberId = RequestedFirstCompanionId;
			}
			else if (FirstCompanionSlot != RequestedActiveSlot)
			{
				Swap(Formation.Members[FirstCompanionSlot], Formation.Members[RequestedActiveSlot]);
			}
		}
		return FGameXXKPartyFormationRules::Validate(State, Formation, &OutError);
	}

	static bool EnsureCompanionCardRun(FGameXXKRuntimeState& RuntimeState)
	{
		FString Error;
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(RuntimeState, &Error);
	}

	static bool HasUnclaimedDismissalRefund(
		const FGameXXKPermanentCompanion& Companion,
		const FGameXXKEquipmentCollectionState& EquipmentCollection)
	{
		// Accumulated experience still has no authoritative material destination. Equipment is claimed
		// only from the v7+ central collection; the deprecated companion array is migration input only.
		return Companion.Level > 1
			|| Companion.Experience > 0
			|| EquipmentCollection.CharacterLoadouts.Contains(Companion.InstanceId);
	}

	static void SetEquipmentTransactionFailure(
		FGameXXKEquipmentTransactionResult& OutResult,
		const EGameXXKEquipmentTransactionError Error)
	{
		OutResult = FGameXXKEquipmentTransactionResult();
		OutResult.Error = Error;
		OutResult.Message = FGameXXKEquipmentRules::GetTransactionErrorMessage(Error);
	}

	static bool BuildToolRefForEquipment(
		const FGameXXKRuntimeState& State,
		const FName InstanceId,
		FGameXXKToolInputRef& OutRef)
	{
		const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId);
		for (const EGameXXKDesktopItemContainer Container : {
			EGameXXKDesktopItemContainer::Backpack,
			EGameXXKDesktopItemContainer::Warehouse})
		{
			const int32 Slot = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Container, Entry);
			if (Slot != INDEX_NONE)
			{
				OutRef.Container = Container;
				OutRef.SlotIndex = Slot;
				OutRef.ExpectedEntry = Entry;
				return true;
			}
		}
		return false;
	}

	static bool IsValidPostReplacementActiveCompanion(
		const FGameXXKCompanionRosterState& Roster,
		const FName DismissedInstanceId,
		const FName RequestedActiveInstanceId)
	{
		if (RequestedActiveInstanceId.IsNone())
		{
			return true;
		}
		if (RequestedActiveInstanceId == DismissedInstanceId)
		{
			return false;
		}
		if (Roster.PendingRecruitment.bHasPendingRecruitment
			&& Roster.PendingRecruitment.Candidate.InstanceId == RequestedActiveInstanceId)
		{
			return true;
		}
		return Roster.PermanentCompanions.ContainsByPredicate(
			[DismissedInstanceId, RequestedActiveInstanceId](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId != DismissedInstanceId
					&& Companion.InstanceId == RequestedActiveInstanceId;
			});
	}

	static bool StarterRecruitSequenceBeginsWithDifferentRoles(const int32 Seed)
	{
		if (Seed == 0 || Seed == MIN_int32)
		{
			return false;
		}

		FGameXXKCompanionRosterState ProbeRoster;
		ProbeRoster.RecruitSequenceSeed = Seed;
		FGameXXKCompanionRecruitResult FirstRecruit;
		FGameXXKCompanionRecruitResult SecondRecruit;
		return FGameXXKCompanionRules::CreateAndResolveNextRecruitment(ProbeRoster, FirstRecruit, nullptr)
			&& FirstRecruit.Outcome == EGameXXKCompanionRecruitOutcome::Recruited
			&& FGameXXKCompanionRules::CreateAndResolveNextRecruitment(ProbeRoster, SecondRecruit, nullptr)
			&& SecondRecruit.Outcome == EGameXXKCompanionRecruitOutcome::Recruited
			&& FirstRecruit.Companion.Role != SecondRecruit.Companion.Role;
	}

	static int32 MakeStarterRecruitSequenceSeed()
	{
		// Existing saves retain their persisted sequence exactly. Only a fresh game's seed is filtered
		// so its two initial tickets cannot land in adjacent templates of the same four-variant role block.
		for (int32 Attempt = 0; Attempt < 64; ++Attempt)
		{
			const int32 Candidate = FMath::Rand();
			if (StarterRecruitSequenceBeginsWithDifferentRoles(Candidate))
			{
				return Candidate;
			}
		}

		// A deterministic fallback keeps new-game creation total even if the process RNG is unavailable.
		for (int32 Candidate = 1; Candidate <= 4096; ++Candidate)
		{
			if (StarterRecruitSequenceBeginsWithDifferentRoles(Candidate))
			{
				return Candidate;
			}
		}
		return 3;
	}

	/** Every facade mutation invalidates development-only overlays before they can become stale. */
	static void BeginRuntimeStateMutation(
		TOptional<FGameXXKRuntimeState>& InOutBattleHudFixtureView,
		TOptional<FGameXXKRuntimeState>* InOutCardTooltipFixtureBackup = nullptr)
	{
		InOutBattleHudFixtureView.Reset();
		if (InOutCardTooltipFixtureBackup)
		{
			InOutCardTooltipFixtureBackup->Reset();
		}
	}

	static FName ResolveBattleHudFixtureCardOwner(
		const FGameXXKCardInstance& CardInstance,
		const FName HeroId,
		const FName CompanionId,
		const FName QuestNpcId)
	{
		const FGameXXKCardDefinition* const Definition = FGameXXKCardCatalog::FindCardDefinition(CardInstance.CardId);
		if (!Definition)
		{
			return HeroId;
		}

		switch (Definition->Owner)
		{
		case EGameXXKCardOwner::Profession:
			return CompanionId;
		case EGameXXKCardOwner::QuestNpc:
			return QuestNpcId;
		case EGameXXKCardOwner::Hero:
		case EGameXXKCardOwner::Route:
		default:
			return HeroId;
		}
	}

	static void RebindBattleHudFixtureDeckOwners(
		FGameXXKBattleDeckState& InOutDeck,
		const FName HeroId,
		const FName CompanionId,
		const FName QuestNpcId)
	{
		const auto RebindPile = [HeroId, CompanionId, QuestNpcId](TArray<FGameXXKCardInstance>& InOutPile)
		{
			for (FGameXXKCardInstance& CardInstance : InOutPile)
			{
				CardInstance.OwnerUnitId = ResolveBattleHudFixtureCardOwner(CardInstance, HeroId, CompanionId, QuestNpcId);
			}
		};

		RebindPile(InOutDeck.DrawPile);
		RebindPile(InOutDeck.Hand);
		RebindPile(InOutDeck.DiscardPile);
		RebindPile(InOutDeck.PendingChoice.Candidates);
	}

	static FGameXXKCardCombatUnit MakeBattleHudFixtureCombatUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 HP,
		const int32 MaxHP,
		const int32 Mana,
		const int32 MaxMana,
		const int32 Attack,
		const int32 Defense,
		const int32 Armor)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.StableSortOrder = StableSortOrder;
		Unit.HP = HP;
		Unit.MaxHP = MaxHP;
		Unit.Mana = Mana;
		Unit.MaxMana = MaxMana;
		Unit.Attack = Attack;
		Unit.Defense = Defense;
		Unit.Armor = Armor;
		Unit.bLiving = HP > 0;
		return Unit;
	}

	static FGameXXKBattleRuntimeUnit MakeBattleHudFixtureLegacyProjection(
		const FName UnitId,
		const FText& DisplayName,
		const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Projection;
		Projection.Id = UnitId;
		Projection.DisplayName = DisplayName;
		Projection.bEnemy = bEnemy;
		// HP/MP/attack/defense/armor stay at placeholders here.  The adapter below is the
		// sole authoritative path that projects those card-runtime values into the legacy facade.
		return Projection;
	}

	static FGameXXKCardEnemyIntent MakeBattleHudFixtureEnemyIntent(
		const FName CardId,
		const FString& CardDisplayName,
		const FName SourceUnitId,
		const int32 SourceSlotNumber,
		const FName SuggestedTargetUnitId,
		const int32 TargetSlotNumber,
		const int32 Damage)
	{
		FGameXXKCardEnemyIntent Intent;
		Intent.CardId = CardId;
		Intent.CardDisplayName = CardDisplayName;
		Intent.SourceUnitId = SourceUnitId;
		Intent.SourceSlotNumber = SourceSlotNumber;
		Intent.SuggestedTargetUnitId = SuggestedTargetUnitId;
		Intent.TargetSlotNumber = TargetSlotNumber;
		Intent.Damage = Damage;
		Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		return Intent;
	}

	static FGameXXKCardCombatUnit MakeTargetOutcomeFixtureUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder,
		const int32 BattleSlotNumber)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 120;
		Unit.MaxHP = 120;
		Unit.Mana = Side == EGameXXKCardTargetSide::Party ? 100 : 0;
		Unit.MaxMana = Unit.Mana;
		Unit.Attack = Side == EGameXXKCardTargetSide::Party ? 20 : 10;
		Unit.Defense = 0;
		Unit.Speed = 1;
		Unit.StableSortOrder = StableSortOrder;
		Unit.BattleSlotNumber = BattleSlotNumber;
		if (Side == EGameXXKCardTargetSide::Enemy)
		{
			static const FName EnemyDefinitions[] = {
				TEXT("Enemy.Ch1.Rooster"),
				TEXT("Enemy.Ch1.Goat"),
				TEXT("Enemy.Ch1.Weasel")};
			Unit.EnemyDefinitionId = EnemyDefinitions[FMath::Clamp(BattleSlotNumber, 1, 3) - 1];
			Unit.CombatLevel = 1;
		}
		return Unit;
	}

	static FGameXXKCardInstance MakeTargetOutcomeFixtureCard(const FName CardId, const FName OwnerUnitId)
	{
		FGameXXKCardInstance Card;
		Card.InstanceId = TEXT("Outcome.Card.Only");
		Card.CardId = CardId;
		Card.CurrentQuality = EGameXXKCardQuality::Common;
		Card.OwnerUnitId = OwnerUnitId;
		Card.SourceEntryId = TEXT("Outcome.Source.Only");
		Card.AcquisitionOrdinal = 0;
		return Card;
	}

	static FGameXXKCardCombatUnit* FindTargetOutcomeFixtureUnit(
		FGameXXKRuntimeState& State,
		const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	static bool BuildTargetOutcomeFixtureState(
		const FName ScenarioId,
		FGameXXKRuntimeState& OutState,
		FString& OutError)
	{
		const FName PartyOneId(TEXT("Outcome.Party.1P"));
		const FName HeroId(TEXT("Player"));
		const FName PartyThreeId(TEXT("Outcome.Party.3P"));
		const FName EnemyOneId(TEXT("Outcome.Enemy.1P"));
		const FName EnemyTwoId(TEXT("Outcome.Enemy.2P"));
		const FName EnemyThreeId(TEXT("Outcome.Enemy.3P"));

		FName CardId;
		EGameXXKCharacterRole OwnerRole = EGameXXKCharacterRole::Guard;
		if (ScenarioId == TEXT("Outcome.Single")
			|| ScenarioId == TEXT("Outcome.AgilityDodge")
			|| ScenarioId == TEXT("Outcome.ArmorBlocked")
			|| ScenarioId == TEXT("Outcome.GuardRedirect")
			|| ScenarioId == TEXT("Outcome.Lethal"))
		{
			CardId = TEXT("Hero.Generic.QingFengYiShi");
		}
		else if (ScenarioId == TEXT("Outcome.HeavyArrow"))
		{
			CardId = TEXT("Hero.Hunter.LieYuLianShi");
			OwnerRole = EGameXXKCharacterRole::Hunter;
		}
		else if (ScenarioId == TEXT("Outcome.GroupThree") || ScenarioId == TEXT("Outcome.GroupMissing2P"))
		{
			CardId = TEXT("Profession.Blade.HengYunKaiFeng");
			OwnerRole = EGameXXKCharacterRole::Blade;
		}
		else if (ScenarioId == TEXT("Outcome.ToxicExplosion"))
		{
			CardId = TEXT("Hero.Healer.DuHuoTongLu");
			OwnerRole = EGameXXKCharacterRole::Healer;
		}
		else if (ScenarioId == TEXT("Outcome.MedicineEnemy"))
		{
			CardId = TEXT("Profession.Healer.CaoMuFuZhi");
			OwnerRole = EGameXXKCharacterRole::Healer;
		}
		else if (ScenarioId == TEXT("Outcome.Healing") || ScenarioId == TEXT("Outcome.Armor"))
		{
			CardId = TEXT("Profession.Healer.WenYangGao");
			OwnerRole = EGameXXKCharacterRole::Healer;
		}
		else
		{
			OutError = FString::Printf(TEXT("Unknown target-outcome fixture scenario: %s"), *ScenarioId.ToString());
			return false;
		}

		if (!FGameXXKCardCatalog::FindCardDefinition(CardId))
		{
			OutError = FString::Printf(TEXT("Target-outcome fixture card is not in the catalog: %s"), *CardId.ToString());
			return false;
		}

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeTargetOutcomeFixtureUnit(PartyOneId, EGameXXKCardTargetSide::Party, OwnerRole, 0, INDEX_NONE),
			MakeTargetOutcomeFixtureUnit(HeroId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1, INDEX_NONE),
			MakeTargetOutcomeFixtureUnit(PartyThreeId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 2, INDEX_NONE),
			MakeTargetOutcomeFixtureUnit(EnemyOneId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 1)};
		if (ScenarioId != TEXT("Outcome.GroupMissing2P"))
		{
			Units.Add(MakeTargetOutcomeFixtureUnit(EnemyTwoId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11, 2));
		}
		Units.Add(MakeTargetOutcomeFixtureUnit(EnemyThreeId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 12, 3));

		const FGameXXKCardInstance Card = MakeTargetOutcomeFixtureCard(CardId, PartyOneId);
		FGameXXKCardBattleRuntime Runtime;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			{Card},
			Units,
			EGameXXKCardTerrain::Plain,
			81208,
			&OutError))
		{
			return false;
		}
		Runtime.Phase = EGameXXKCardBattlePhase::Player;
		Runtime.Deck.Hand = {Card};
		Runtime.Deck.DrawPile.Reset();
		Runtime.Deck.DiscardPile.Reset();
		Runtime.Deck.ExhaustPile.Reset();
		Runtime.Deck.SharedEnergy = 20;
		Runtime.CombatRandomState = 81208;

		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.Screen = EGameXXKScreen::Battle;
		OutState.CardRun.bHasActiveCardBattle = true;
		OutState.CardRun.ActiveBattleSourceNodeId = 1;
		OutState.CardRun.ActiveBattle = MoveTemp(Runtime);

		FGameXXKCardCombatUnit* const Owner = FindTargetOutcomeFixtureUnit(OutState, PartyOneId);
		FGameXXKCardCombatUnit* const PartyOne = FindTargetOutcomeFixtureUnit(OutState, PartyOneId);
		FGameXXKCardCombatUnit* const EnemyOne = FindTargetOutcomeFixtureUnit(OutState, EnemyOneId);
		FGameXXKCardCombatUnit* const EnemyTwo = FindTargetOutcomeFixtureUnit(OutState, EnemyTwoId);
		FGameXXKCardCombatUnit* const EnemyThree = FindTargetOutcomeFixtureUnit(OutState, EnemyThreeId);
		if (!Owner || !PartyOne || !EnemyOne || !EnemyThree)
		{
			OutError = TEXT("Target-outcome fixture lost a required stable unit.");
			return false;
		}

		if (ScenarioId == TEXT("Outcome.HeavyArrow"))
		{
			GameXXKCardRules::AddCombatStatus(*Owner, EGameXXKCardStatus::Charge, 3);
			EnemyOne->HP = EnemyOne->MaxHP = 300;
		}
		else if (ScenarioId == TEXT("Outcome.GroupThree") || ScenarioId == TEXT("Outcome.GroupMissing2P"))
		{
			EnemyOne->Defense = 4;
			if (EnemyTwo)
			{
				EnemyTwo->Defense = 3;
			}
			EnemyThree->Defense = 2;
		}
		else if (ScenarioId == TEXT("Outcome.ToxicExplosion"))
		{
			GameXXKCardRules::AddCombatStatus(*EnemyOne, EGameXXKCardStatus::Bleed, 3);
			GameXXKCardRules::AddCombatStatus(*EnemyOne, EGameXXKCardStatus::Poison, 2);
			GameXXKCardRules::AddCombatStatus(*EnemyOne, EGameXXKCardStatus::Burn, 4);
			EnemyOne->HP = EnemyOne->MaxHP = 300;
		}
		else if (ScenarioId == TEXT("Outcome.MedicineEnemy"))
		{
			GameXXKCardRules::AddCombatStatus(*Owner, EGameXXKCardStatus::Medicine, 5);
		}
		else if (ScenarioId == TEXT("Outcome.Healing"))
		{
			PartyOne->HP = 90;
			PartyOne->Armor = 99;
		}
		else if (ScenarioId == TEXT("Outcome.Armor"))
		{
			PartyOne->HP = PartyOne->MaxHP;
			PartyOne->Armor = 0;
		}
		else if (ScenarioId == TEXT("Outcome.AgilityDodge"))
		{
			GameXXKCardRules::AddCombatStatus(*EnemyOne, EGameXXKCardStatus::Agility, 1);
			OutState.CardRun.ActiveBattle.CombatRandomState = 3;
		}
		else if (ScenarioId == TEXT("Outcome.ArmorBlocked"))
		{
			EnemyOne->Armor = 99;
		}
		else if (ScenarioId == TEXT("Outcome.GuardRedirect"))
		{
			if (!EnemyTwo)
			{
				OutError = TEXT("Guard redirect fixture requires enemy 2P.");
				return false;
			}
			FGameXXKCardGuardLinkRuntime& Link = OutState.CardRun.ActiveBattle.GuardLinks.AddDefaulted_GetRef();
			Link.GuardianUnitId = EnemyTwoId;
			Link.ProtectedUnitId = EnemyOneId;
			Link.Stacks = 1;
			Link.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
		}
		else if (ScenarioId == TEXT("Outcome.Lethal"))
		{
			EnemyOne->HP = 10;
		}

		OutState.ActiveBattleParty = {
			MakeBattleHudFixtureLegacyProjection(PartyOneId, FText::FromString(TEXT("伙伴")), false),
			MakeBattleHudFixtureLegacyProjection(HeroId, FText::FromString(TEXT("主角")), false),
			MakeBattleHudFixtureLegacyProjection(PartyThreeId, FText::FromString(TEXT("任务伙伴")), false)};
		OutState.ActiveBattleEnemies = {
			MakeBattleHudFixtureLegacyProjection(EnemyOneId, FText::FromString(TEXT("敌人一")), true)};
		if (EnemyTwo)
		{
			OutState.ActiveBattleEnemies.Add(
				MakeBattleHudFixtureLegacyProjection(EnemyTwoId, FText::FromString(TEXT("敌人二")), true));
		}
		OutState.ActiveBattleEnemies.Add(
			MakeBattleHudFixtureLegacyProjection(EnemyThreeId, FText::FromString(TEXT("敌人三")), true));

		if (!GameXXKCardRules::ValidateCardBattleRuntime(OutState.CardRun.ActiveBattle, &OutError))
		{
			return false;
		}
		if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(OutState, &OutError))
		{
			return false;
		}
		return true;
	}
}

UGameXXKMVPSubsystem::UGameXXKMVPSubsystem()
{
	RuntimeState = UGameXXKMVPRules::CreateNewGame();
	RebuildTrainingTravelRuntime();
}

bool UGameXXKMVPSubsystem::RebuildTrainingTravelRuntime()
{
	FGameXXKTrainingTravelRuntime CandidateRuntime;
	if (!BuildTrainingTravelRuntimeForState(RuntimeState, CandidateRuntime))
	{
		return false;
	}
	TrainingTravelRuntime = MoveTemp(CandidateRuntime);
	return true;
}

bool UGameXXKMVPSubsystem::BuildTrainingTravelRuntimeForState(
	const FGameXXKRuntimeState& State,
	FGameXXKTrainingTravelRuntime& OutRuntime) const
{
	OutRuntime = FGameXXKTrainingTravelRuntime();
	if (!State.Training.bTravelActive)
	{
		return true;
	}
	TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
	if (!BuildTrainingTravelParty(State, Party))
	{
		return false;
	}
	return FGameXXKTrainingRules::InitializeTravelRunner(
		State.Training,
		OutRuntime,
		Party);
}

int64 UGameXXKMVPSubsystem::GetCurrentTravelUnixSeconds() const
{
	return FDateTime::UtcNow().ToUnixTimestamp();
}

bool UGameXXKMVPSubsystem::ApplyOfflineTravelSinceLastUpdate(
	FGameXXKRuntimeState& State,
	FGameXXKTrainingTravelRuntime& OutRuntime,
	const int64 NowUnixSeconds) const
{
	OutRuntime = FGameXXKTrainingTravelRuntime();
	if (!State.Training.bTravelActive)
	{
		return true;
	}

	TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
	if (!BuildTrainingTravelParty(State, Party)
		|| !FGameXXKTrainingRules::InitializeTravelRunner(
			State.Training,
			OutRuntime,
			Party))
	{
		return false;
	}

	const int64 SafeNow = FMath::Max<int64>(1, NowUnixSeconds);
	const int64 LastUpdated = State.Training.TravelLastUpdatedUnixSeconds;
	if (LastUpdated <= 0 || SafeNow <= LastUpdated || State.Training.bTravelPausedAtDefeat)
	{
		State.Training.TravelLastUpdatedUnixSeconds = SafeNow;
		return true;
	}

	const int64 Elapsed = SafeNow - LastUpdated;
	const int32 SimulatedSeconds = static_cast<int32>(FMath::Min<int64>(
		Elapsed,
		FGameXXKTrainingRules::MaxTravelOfflineSimulationSeconds));
	if (SimulatedSeconds <= 0)
	{
		State.Training.TravelLastUpdatedUnixSeconds = SafeNow;
		return true;
	}

	FGameXXKTrainingOfflineReward SimulatedReward;
	if (!FGameXXKTrainingRules::AdvanceTravelOffline(
		State.Training,
		OutRuntime,
		SimulatedSeconds,
		SimulatedReward)
		|| !FGameXXKTrainingRules::AccumulatePendingTravelReward(State.Training, SimulatedReward))
	{
		return false;
	}
	State.PlayerHP = OutRuntime.PlayerHP;
	State.Training.TravelLastUpdatedUnixSeconds = LastUpdated + SimulatedReward.SimulatedSeconds;
	if (SimulatedReward.bStoppedAtDefeat)
	{
		State.Training.bTravelPausedAtDefeat = true;
	}
	return true;
}

const FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetRuntimeState() const
{
	return BattleHudFixtureView.IsSet() ? BattleHudFixtureView.GetValue() : RuntimeState;
}

FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetMutableRuntimeState()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return RuntimeState;
}

FGameXXKRuntimeState UGameXXKMVPSubsystem::GetRuntimeStateCopy() const
{
	return GetRuntimeState();
}

FGameXXKTrainingProgress UGameXXKMVPSubsystem::GetTrainingProgressCopy() const
{
	return GetRuntimeState().Training;
}

TArray<FGameXXKTrainingStageDefinition> UGameXXKMVPSubsystem::GetTrainingStageDefinitions() const
{
	return FGameXXKTrainingRules::GetStageDefinitions();
}

TArray<FGameXXKTrainingEncounterDefinition> UGameXXKMVPSubsystem::GetTrainingEncounterSequence(const FName StageId, const bool bTravelMode) const
{
	return FGameXXKTrainingRules::BuildEncounterSequence(StageId, bTravelMode);
}

FText UGameXXKMVPSubsystem::BuildTrainingStageTooltip(const FName StageId) const
{
	return FGameXXKTrainingRules::BuildStageTooltip(GetRuntimeState().Training, StageId);
}

bool UGameXXKMVPSubsystem::SelectTrainingStage(const FName StageId)
{
	FGameXXKTrainingStageDefinition Definition;
	if (!FGameXXKTrainingRules::TryGetStageDefinition(StageId, Definition))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.Training.SelectedStageId = StageId;
	return true;
}

bool UGameXXKMVPSubsystem::StartTrainingChallenge(const FName StageId)
{
	if (RuntimeState.CardRun.bHasActiveCardBattle)
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKTrainingRules::StartChallenge(Candidate.Training, StageId))
	{
		return false;
	}

	FString Error;
	if (!BeginTrainingEncounterBattle(Candidate, StageId, Candidate.Training.ActiveChallengeEncounterIndex, &Error))
	{
		UE_LOG(LogTemp, Error, TEXT("[Training] BeginTrainingEncounterBattle failed for %s: %s"), *StageId.ToString(), *Error);
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::IsTrainingChallengeBattleActive() const
{
	return RuntimeState.Training.bChallengeActive
		&& RuntimeState.CardRun.bHasActiveCardBattle
		&& RuntimeState.Screen == EGameXXKScreen::Battle;
}

bool UGameXXKMVPSubsystem::CancelTrainingChallengeToWorkbench()
{
	if (!RuntimeState.Training.bChallengeActive)
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	ClearTrainingBattleProjection(Candidate);
	Candidate.Training.bChallengeActive = false;
	Candidate.Training.ActiveChallengeStageId = NAME_None;
	Candidate.Training.ActiveChallengeEncounterIndex = INDEX_NONE;
	Candidate.Training.bChallengeAutoBattle = false;
	ReturnTrainingToWorkbench(Candidate);

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::AdvanceTrainingChallengeEncounter(bool& bOutStageCompleted, FGameXXKTrainingReward& OutReward)
{
	bOutStageCompleted = false;
	OutReward = FGameXXKTrainingReward();
	if (!RuntimeState.Training.bChallengeActive)
	{
		return false;
	}

	if (RuntimeState.CardRun.bHasActiveCardBattle)
	{
		FGameXXKRuntimeState Candidate = RuntimeState;
		FString Error;
		if (!AdvanceTrainingCardBattleStep(Candidate, &Error))
		{
			return false;
		}

		const FGameXXKTrainingProgress ActiveProgress = Candidate.Training;
		if (!FGameXXKCardBattleAdapter::IsCardBattleTerminal(Candidate))
		{
			BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
			RuntimeState = MoveTemp(Candidate);
			return true;
		}

		const FName StageId = ActiveProgress.ActiveChallengeStageId;
		const int32 EncounterIndex = ActiveProgress.ActiveChallengeEncounterIndex;
		const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(StageId, false);
		if (!Encounters.IsValidIndex(EncounterIndex))
		{
			return false;
		}

		if (Candidate.CardRun.ActiveBattle.Phase == EGameXXKCardBattlePhase::Defeat)
		{
			// Challenge failure is a local retry: it does not alter the cleared-stage graph
			// and it never falls through the town failure settlement used by route runs.
			ClearTrainingBattleProjection(Candidate);
			Candidate.PlayerHP = Candidate.PlayerMaxHP;
			Candidate.PlayerMP = Candidate.PlayerMaxMP;
			if (!BeginTrainingEncounterBattle(Candidate, StageId, EncounterIndex, &Error))
			{
				return false;
			}
			BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
			RuntimeState = MoveTemp(Candidate);
			return true;
		}

		OutReward = FGameXXKTrainingRules::ResolveChallengeReward(
			StageId,
			Encounters[EncounterIndex].Kind,
			Candidate.Training.ChallengeRewardSeed,
			0.0f);
		Candidate.Training.ChallengeRewardSeed = FGameXXKTrainingRules::NextChallengeRewardSeed(
			Candidate.Training.ChallengeRewardSeed);
		if (!ApplyTrainingRewardToRuntime(Candidate, OutReward))
		{
			return false;
		}
		ClearTrainingBattleProjection(Candidate);
		const bool bLastEncounter = EncounterIndex == Encounters.Num() - 1;
		if (bLastEncounter)
		{
			bOutStageCompleted = FGameXXKTrainingRules::CompleteChallenge(Candidate.Training, StageId);
			ReturnTrainingToWorkbench(Candidate);
		}
		else
		{
			++Candidate.Training.ActiveChallengeEncounterIndex;
			if (!BeginTrainingEncounterBattle(Candidate, StageId, Candidate.Training.ActiveChallengeEncounterIndex, &Error))
			{
				return false;
			}
		}

		BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
		RuntimeState = MoveTemp(Candidate);
		return true;
	}

	// Compatibility path for pure widget fixtures created before the real battle
	// bridge was installed. Live challenge sessions always take the branch above.
	const FGameXXKTrainingProgress Progress = RuntimeState.Training;
	const TArray<FGameXXKTrainingEncounterDefinition> Encounters = FGameXXKTrainingRules::BuildEncounterSequence(Progress.ActiveChallengeStageId);
	if (!Encounters.IsValidIndex(Progress.ActiveChallengeEncounterIndex))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	const EGameXXKTrainingEncounterKind EncounterKind = Encounters[Progress.ActiveChallengeEncounterIndex].Kind;
	const bool bLastEncounter = Progress.ActiveChallengeEncounterIndex == Encounters.Num() - 1;
	OutReward = FGameXXKTrainingRules::ResolveChallengeReward(
		Progress.ActiveChallengeStageId,
		EncounterKind,
		RuntimeState.Training.ChallengeRewardSeed,
		0.0f);
	RuntimeState.Training.ChallengeRewardSeed = FGameXXKTrainingRules::NextChallengeRewardSeed(
		RuntimeState.Training.ChallengeRewardSeed);
	if (!ApplyTrainingRewardToRuntime(RuntimeState, OutReward))
	{
		return false;
	}
	if (bLastEncounter)
	{
		bOutStageCompleted = FGameXXKTrainingRules::CompleteChallenge(RuntimeState.Training, Progress.ActiveChallengeStageId);
		ReturnTrainingToWorkbench(RuntimeState);
		return bOutStageCompleted;
	}
	++RuntimeState.Training.ActiveChallengeEncounterIndex;
	return true;
}

bool UGameXXKMVPSubsystem::SetTrainingChallengeAutoBattle(const bool bEnabled)
{
	if (!RuntimeState.Training.bChallengeActive)
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.Training.bChallengeAutoBattle = bEnabled;
	return true;
}

bool UGameXXKMVPSubsystem::StartTrainingTravel(const FName StageId)
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKTrainingRules::StartTravel(Candidate.Training, StageId))
	{
		return false;
	}

	FGameXXKTrainingTravelRuntime CandidateRunner;
	TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
	if (!BuildTrainingTravelParty(Candidate, Party)
		|| !FGameXXKTrainingRules::InitializeTravelRunner(
			Candidate.Training,
			CandidateRunner,
			Party))
	{
		return false;
	}
	Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

FGameXXKTrainingTravelRuntime UGameXXKMVPSubsystem::GetTrainingTravelRuntimeCopy() const
{
	return TrainingTravelRuntime;
}

bool UGameXXKMVPSubsystem::AdvanceTrainingTravelStep(
	bool& bOutEncounterCompleted,
	bool& bOutStageCompleted,
	bool& bOutDefeated,
	FGameXXKTrainingReward& OutReward,
	const int32 ElapsedSeconds)
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	FGameXXKTrainingTravelRuntime CandidateRunner = TrainingTravelRuntime;
	if (!FGameXXKTrainingRules::AdvanceTravelRunner(
		Candidate.Training,
		CandidateRunner,
		bOutEncounterCompleted,
		bOutStageCompleted,
		bOutDefeated,
		OutReward,
		ElapsedSeconds))
	{
		return false;
	}

	if (bOutEncounterCompleted && !ApplyTrainingRewardToRuntime(Candidate, OutReward))
	{
		return false;
	}
	Candidate.PlayerHP = CandidateRunner.PlayerHP;
	Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

bool UGameXXKMVPSubsystem::AdvanceTrainingTravelEncounter(bool& bOutStageCompleted, FGameXXKTrainingReward& OutReward)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	const bool bAdvanced = FGameXXKTrainingRules::AdvanceTravelEncounter(RuntimeState.Training, bOutStageCompleted, OutReward);
	if (bAdvanced && !ApplyTrainingRewardToRuntime(RuntimeState, OutReward))
	{
		return false;
	}
	return bAdvanced;
}

bool UGameXXKMVPSubsystem::SetTrainingRetryOnFailure(const bool bEnabled)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.Training.bRetryOnFailure = bEnabled;
	return true;
}

bool UGameXXKMVPSubsystem::ResolveTrainingTravelFailure()
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKTrainingRules::ResolveTravelFailure(Candidate.Training))
	{
		return false;
	}

	// A retry is a fresh low-cost attempt, not a continuation from the death
	// frame.  The durable stage/index policy remains owned by TrainingRules.
	Candidate.PlayerHP = Candidate.PlayerMaxHP;
	Candidate.PlayerMP = Candidate.PlayerMaxMP;
	FGameXXKTrainingTravelRuntime CandidateRunner;
	TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
	if (Candidate.Training.bTravelActive
		&& (!BuildTrainingTravelParty(Candidate, Party)
			|| !FGameXXKTrainingRules::InitializeTravelRunner(
				Candidate.Training,
				CandidateRunner,
				Party)))
	{
		return false;
	}
	Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

bool UGameXXKMVPSubsystem::ApplyTrainingOfflineRewardToRuntime(
	FGameXXKRuntimeState& State,
	const FGameXXKTrainingOfflineReward& Reward) const
{
	if (Reward.Gold < 0
		|| Reward.Experience < 0
		|| Reward.NormalChestCount < 0
		|| Reward.AdvancedChestCount < 0)
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = State;
	Candidate.PlayerGold = FMath::Max(0, Candidate.PlayerGold + Reward.Gold);
	Candidate.PlayerXP = FMath::Max(0, Candidate.PlayerXP + Reward.Experience);
	if (!AddTrainingChestCount(
		Candidate,
		UGameXXKMVPRules::ItemTrainingNormalChest(),
		Reward.NormalChestCount)
		|| !AddTrainingChestCount(
			Candidate,
			UGameXXKMVPRules::ItemTrainingAdvancedChest(),
			Reward.AdvancedChestCount))
	{
		return false;
	}
	State = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::SimulateTrainingTravelOffline(
	const int32 ElapsedSeconds,
	FGameXXKTrainingOfflineReward& OutReward)
{
	OutReward = FGameXXKTrainingOfflineReward();
	if (!RuntimeState.Training.bTravelActive || RuntimeState.Training.bChallengeActive)
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FGameXXKTrainingTravelRuntime CandidateRunner = TrainingTravelRuntime;
	if (!FGameXXKTrainingRules::AdvanceTravelOffline(
		Candidate.Training,
		CandidateRunner,
		ElapsedSeconds,
		OutReward)
		|| !FGameXXKTrainingRules::AccumulatePendingTravelReward(Candidate.Training, OutReward))
	{
		return false;
	}
	Candidate.PlayerHP = CandidateRunner.PlayerHP;
	if (Candidate.Training.TravelLastUpdatedUnixSeconds > 0)
	{
		Candidate.Training.TravelLastUpdatedUnixSeconds += OutReward.SimulatedSeconds;
	}
	else
	{
		Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

FGameXXKTrainingOfflineReward UGameXXKMVPSubsystem::GetPendingTrainingTravelRewardCopy() const
{
	FGameXXKTrainingOfflineReward PendingReward;
	FGameXXKTrainingRules::GetPendingTravelReward(GetRuntimeState().Training, PendingReward);
	return PendingReward;
}

bool UGameXXKMVPSubsystem::CollectTrainingTravelRewards(FGameXXKTrainingOfflineReward& OutReward)
{
	OutReward = FGameXXKTrainingOfflineReward();
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKTrainingRules::ConsumePendingTravelReward(Candidate.Training, OutReward)
		|| !ApplyTrainingOfflineRewardToRuntime(Candidate, OutReward))
	{
		return false;
	}
	if (Candidate.Training.bTravelActive)
	{
		Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

TArray<FGameXXKMetaShopProductDefinition> UGameXXKMVPSubsystem::GetMetaShopProducts() const
{
	return FGameXXKMetaShopRules::GetProducts();
}

bool UGameXXKMVPSubsystem::PreviewMetaShopPurchase(
	const EGameXXKMetaShopProductId ProductId,
	FGameXXKMetaShopPurchasePreview& OutPreview) const
{
	return FGameXXKMetaShopRules::PreviewPurchase(RuntimeState, ProductId, OutPreview);
}

bool UGameXXKMVPSubsystem::PurchaseMetaShopProduct(
	const EGameXXKMetaShopProductId ProductId,
	FGameXXKMetaShopPurchaseResult& OutResult)
{
	return FGameXXKMetaShopRules::Purchase(RuntimeState, ProductId, OutResult);
}

bool UGameXXKMVPSubsystem::ApplyBattleHudFixtureForTest(FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (RuntimeState.Screen != EGameXXKScreen::Battle)
	{
		OutError = TEXT("Battle HUD fixture requires the Battle screen.");
		return false;
	}
	if (!RuntimeState.CardRun.bHasActiveCardBattle)
	{
		OutError = TEXT("Battle HUD fixture requires an active card battle.");
		return false;
	}

	FGameXXKRuntimeState FixtureState = RuntimeState;
	// This is a static read-only visual board, not a second live turn.  If the
	// raw save happens to be in Enemy phase, BattleBoardWidget would otherwise
	// begin its tick-driven intent resolver and mutate the raw state, which
	// intentionally clears this non-persistent overlay.  Keep the forecast rail
	// visible while presenting the fixture as a safe player-phase snapshot.
	FixtureState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	const FName HeroId(TEXT("Player"));
	const FName CompanionId(TEXT("CompanionInstance.Companion_Blade_01.HudFixture"));
	const FName QuestNpcId(TEXT("Npc.TusiChief"));
	const FName MoneyRatId(TEXT("MoneyRat"));
	const FName BlackBearId(TEXT("BlackBear"));
	const FName TigerId(TEXT("Tiger"));

	// This is intentionally a copied render/verification state.  It does not create a roster
	// companion, alter the route-local NPC provenance, or invoke any battle lifecycle mutation.
	FGameXXKCardCombatUnit Companion = MakeBattleHudFixtureCombatUnit(
		CompanionId,
		EGameXXKCardTargetSide::Party,
		EGameXXKCharacterRole::Blade,
		0,
		61,
		92,
		12,
		22,
		17,
		6,
		3);
	FGameXXKCardCombatUnit Hero = MakeBattleHudFixtureCombatUnit(
		HeroId,
		EGameXXKCardTargetSide::Party,
		EGameXXKCharacterRole::Hero,
		1,
		72,
		100,
		18,
		30,
		20,
		8,
		7);
	FGameXXKCardCombatUnit QuestNpc = MakeBattleHudFixtureCombatUnit(
		QuestNpcId,
		EGameXXKCardTargetSide::Party,
		EGameXXKCharacterRole::QuestNpc,
		2,
		86,
		115,
		14,
		24,
		15,
		10,
		1);
	FGameXXKCardCombatUnit MoneyRat = MakeBattleHudFixtureCombatUnit(
		MoneyRatId,
		EGameXXKCardTargetSide::Enemy,
		EGameXXKCharacterRole::Invalid,
		0,
		42,
		60,
		0,
		0,
		10,
		3,
		0);
	FGameXXKCardCombatUnit BlackBear = MakeBattleHudFixtureCombatUnit(
		BlackBearId,
		EGameXXKCardTargetSide::Enemy,
		EGameXXKCharacterRole::Invalid,
		1,
		84,
		110,
		0,
		0,
		15,
		5,
		0);
	FGameXXKCardCombatUnit Tiger = MakeBattleHudFixtureCombatUnit(
		TigerId,
		EGameXXKCardTargetSide::Enemy,
		EGameXXKCharacterRole::Invalid,
		2,
		152,
		180,
		0,
		0,
		21,
		8,
		0);
	FGameXXKCardStatusStack& Poison = MoneyRat.Statuses.AddDefaulted_GetRef();
	Poison.Status = EGameXXKCardStatus::Poison;
	Poison.Stacks = 2;
	FGameXXKCardStatusStack& Bleed = MoneyRat.Statuses.AddDefaulted_GetRef();
	Bleed.Status = EGameXXKCardStatus::Bleed;
	Bleed.Stacks = 3;

	FixtureState.CardRun.ActiveBattle.Units = {
		MoveTemp(Companion),
		MoveTemp(Hero),
		MoveTemp(QuestNpc),
		MoveTemp(MoneyRat),
		MoveTemp(BlackBear),
		MoveTemp(Tiger)};
	RebindBattleHudFixtureDeckOwners(FixtureState.CardRun.ActiveBattle.Deck, HeroId, CompanionId, QuestNpcId);
	FixtureState.ActiveBattleParty = {
		MakeBattleHudFixtureLegacyProjection(CompanionId, FText::FromString(TEXT("伙伴")), false),
		MakeBattleHudFixtureLegacyProjection(HeroId, FText::FromString(TEXT("主角")), false),
		MakeBattleHudFixtureLegacyProjection(QuestNpcId, FText::FromString(TEXT("土司首领")), false)};
	FixtureState.ActiveBattleEnemies = {
		MakeBattleHudFixtureLegacyProjection(MoneyRatId, FText::FromString(TEXT("金钱鼠")), true),
		MakeBattleHudFixtureLegacyProjection(BlackBearId, FText::FromString(TEXT("黑熊")), true),
		MakeBattleHudFixtureLegacyProjection(TigerId, FText::FromString(TEXT("虎王")), true)};
	FixtureState.CardRun.ActiveBattle.GuardLinks.Reset();
	FixtureState.CardRun.ActiveBattle.Modifiers.Reset();
	FixtureState.CardRun.ActiveBattle.NextModifierOrdinal = 0;
	FixtureState.CardRun.ActiveBattle.Deck.SharedEnergy = 2;

	// Intent cards are read-only disclosure for the fixture's three living enemies; no enemy
	// action, damage resolution, draw, or other gameplay mutation is performed to create them.
	FixtureState.CardRun.EnemyIntents = {
		MakeBattleHudFixtureEnemyIntent(TEXT("Fixture.Intent.MoneyRat.Bite"), TEXT("撕咬"), MoneyRatId, 1, HeroId, 2, 10),
		MakeBattleHudFixtureEnemyIntent(TEXT("Fixture.Intent.BlackBear.Swipe"), TEXT("扑击"), BlackBearId, 2, HeroId, 2, 15),
		MakeBattleHudFixtureEnemyIntent(TEXT("Fixture.Intent.Tiger.Pounce"), TEXT("虎扑"), TigerId, 3, HeroId, 2, 22)};
	FixtureState.CardRun.NextEnemyIntentIndex = 0;

	if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(FixtureState, &OutError))
	{
		return false;
	}

	BattleHudFixtureView.Emplace(MoveTemp(FixtureState));
	return true;
}

void UGameXXKMVPSubsystem::ClearBattleHudFixtureForTest()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
}

bool UGameXXKMVPSubsystem::IsBattleHudFixtureActiveForTest() const
{
	return BattleHudFixtureView.IsSet();
}

bool UGameXXKMVPSubsystem::ApplyCardTooltipFixtureForTest(const FName CardId, FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (CardTooltipFixtureBackup.IsSet())
	{
		OutError = TEXT("A card-tooltip fixture is already active.");
		return false;
	}
	if (RuntimeState.Screen != EGameXXKScreen::Battle)
	{
		OutError = TEXT("Card tooltip fixture requires the Battle screen.");
		return false;
	}
	if (!RuntimeState.CardRun.bHasActiveCardBattle)
	{
		OutError = TEXT("Card tooltip fixture requires an active card battle.");
		return false;
	}
	const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
	if (!Definition)
	{
		OutError = FString::Printf(TEXT("Card tooltip fixture rejected an unknown card id: %s."), *CardId.ToString());
		return false;
	}

	CardTooltipFixtureBackup.Emplace(RuntimeState);
	FGameXXKRuntimeState FixtureState = RuntimeState;
	FixtureState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	FGameXXKBattleDeckState& FixtureDeck = FixtureState.CardRun.ActiveBattle.Deck;
	if (FixtureDeck.Hand.IsEmpty())
	{
		CardTooltipFixtureBackup.Reset();
		OutError = TEXT("Card tooltip fixture requires at least one visible hand card.");
		return false;
	}
	// If the requested card is already visible elsewhere in the hand, move it
	// to the first slot instead of overwriting another card and creating a
	// duplicate. Otherwise replace the first slot.
	int32 ExistingTargetIndex = INDEX_NONE;
	for (int32 HandIndex = 0; HandIndex < FixtureDeck.Hand.Num(); ++HandIndex)
	{
		if (FixtureDeck.Hand[HandIndex].CardId == CardId)
		{
			ExistingTargetIndex = HandIndex;
			break;
		}
	}
	if (ExistingTargetIndex != INDEX_NONE)
	{
		FixtureDeck.Hand.Swap(0, ExistingTargetIndex);
	}
	else
	{
		FGameXXKCardInstance& FixtureCard = FixtureDeck.Hand[0];
		FixtureCard.CardId = CardId;
		FixtureCard.CurrentQuality = Definition->BaseQuality;
	}
	// Keep the visible card playable so hover/tooltip acceptance never sees a
	// disabled-slot fallback just because the copied battle happened to be low
	// on shared Energy.
	FixtureDeck.SharedEnergy = FMath::Max(FixtureDeck.SharedEnergy, 5);

	RuntimeState = MoveTemp(FixtureState);
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (UGameXXKBattleBoardWidget* const Board = PlayerController->GetBattleBoardWidgetForTest())
		{
			Board->RefreshFromState();
		}
	}
	return true;
}

void UGameXXKMVPSubsystem::ClearCardTooltipFixtureForTest()
{
	if (CardTooltipFixtureBackup.IsSet())
	{
		RuntimeState = MoveTemp(CardTooltipFixtureBackup.GetValue());
		CardTooltipFixtureBackup.Reset();
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (UGameXXKBattleBoardWidget* const Board = PlayerController->GetBattleBoardWidgetForTest())
		{
			Board->RefreshFromState();
		}
	}
}

bool UGameXXKMVPSubsystem::IsCardTooltipFixtureActiveForTest() const
{
	return CardTooltipFixtureBackup.IsSet();
}


bool UGameXXKMVPSubsystem::ApplyPilotComparisonFixtureForTest(FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (RuntimeState.Screen != EGameXXKScreen::Battle)
	{
		OutError = TEXT("Pilot comparison fixture requires the Battle screen.");
		return false;
	}
	if (!RuntimeState.CardRun.bHasActiveCardBattle)
	{
		OutError = TEXT("Pilot comparison fixture requires an active card battle.");
		return false;
	}

	FGameXXKRuntimeState FixtureState = RuntimeState;
	FixtureState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Player;
	// Three party units with deliberately unmapped runtime ids.  The ".4K" token selects the
	// untouched 4K masters, the plain id selects the default 2K production siblings, and the
	// ".1K" token selects the quarter-resolution siblings, so one scene compares all three.
	const FName HeroOneId(TEXT("Pilot.Hero.One.4K"));
	const FName HeroTwoId(TEXT("Pilot.Hero.Two"));
	const FName HeroThreeId(TEXT("Pilot.Hero.Three.1K"));
	const FName RoosterOneId(TEXT("Pilot.Rooster.One.4K"));
	const FName RoosterTwoId(TEXT("Pilot.Rooster.Two"));
	const FName RoosterThreeId(TEXT("Pilot.Rooster.Three.1K"));

	FGameXXKCardCombatUnit HeroOne = MakeBattleHudFixtureCombatUnit(
		HeroOneId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0, 72, 100, 18, 30, 20, 8, 7);
	FGameXXKCardCombatUnit HeroTwo = MakeBattleHudFixtureCombatUnit(
		HeroTwoId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 1, 66, 92, 15, 26, 18, 7, 5);
	FGameXXKCardCombatUnit HeroThree = MakeBattleHudFixtureCombatUnit(
		HeroThreeId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2, 78, 108, 20, 32, 22, 9, 8);
	FGameXXKCardCombatUnit RoosterOne = MakeBattleHudFixtureCombatUnit(
		RoosterOneId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0, 46, 60, 0, 0, 11, 3, 0);
	FGameXXKCardCombatUnit RoosterTwo = MakeBattleHudFixtureCombatUnit(
		RoosterTwoId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1, 50, 64, 0, 0, 12, 3, 0);
	FGameXXKCardCombatUnit RoosterThree = MakeBattleHudFixtureCombatUnit(
		RoosterThreeId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 2, 44, 58, 0, 0, 10, 3, 0);

	FixtureState.CardRun.ActiveBattle.Units = {
		MoveTemp(HeroOne),
		MoveTemp(HeroTwo),
		MoveTemp(HeroThree),
		MoveTemp(RoosterOne),
		MoveTemp(RoosterTwo),
		MoveTemp(RoosterThree)};
	RebindBattleHudFixtureDeckOwners(FixtureState.CardRun.ActiveBattle.Deck, HeroOneId, HeroTwoId, HeroThreeId);
	FixtureState.ActiveBattleParty = {
		MakeBattleHudFixtureLegacyProjection(HeroOneId, FText::FromString(TEXT("主角一")), false),
		MakeBattleHudFixtureLegacyProjection(HeroTwoId, FText::FromString(TEXT("主角二")), false),
		MakeBattleHudFixtureLegacyProjection(HeroThreeId, FText::FromString(TEXT("主角三")), false)};
	FixtureState.ActiveBattleEnemies = {
		MakeBattleHudFixtureLegacyProjection(RoosterOneId, FText::FromString(TEXT("公鸡一")), true),
		MakeBattleHudFixtureLegacyProjection(RoosterTwoId, FText::FromString(TEXT("公鸡二")), true),
		MakeBattleHudFixtureLegacyProjection(RoosterThreeId, FText::FromString(TEXT("公鸡三")), true)};
	FixtureState.CardRun.ActiveBattle.GuardLinks.Reset();
	FixtureState.CardRun.ActiveBattle.Modifiers.Reset();
	FixtureState.CardRun.ActiveBattle.NextModifierOrdinal = 0;
	FixtureState.CardRun.ActiveBattle.Deck.SharedEnergy = 2;

	FixtureState.CardRun.EnemyIntents = {
		MakeBattleHudFixtureEnemyIntent(TEXT("Pilot.Intent.Rooster.Peck"), TEXT("啄击"), RoosterOneId, 1, HeroOneId, 1, 8),
		MakeBattleHudFixtureEnemyIntent(TEXT("Pilot.Intent.Rooster.Peck"), TEXT("啄击"), RoosterTwoId, 2, HeroTwoId, 2, 8),
		MakeBattleHudFixtureEnemyIntent(TEXT("Pilot.Intent.Rooster.Peck"), TEXT("啄击"), RoosterThreeId, 3, HeroThreeId, 3, 8)};
	FixtureState.CardRun.NextEnemyIntentIndex = 0;

	if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(FixtureState, &OutError))
	{
		return false;
	}

	BattleHudFixtureView.Emplace(MoveTemp(FixtureState));
	return true;
}

bool UGameXXKMVPSubsystem::ApplyTargetOutcomeFixtureForTest(const FName ScenarioId, FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (TargetOutcomeFixtureBackup.IsSet())
	{
		OutError = TEXT("A target-outcome fixture is already active.");
		return false;
	}

	TargetOutcomeFixtureBackup.Emplace(RuntimeState);
	FGameXXKRuntimeState FixtureState;
	if (!BuildTargetOutcomeFixtureState(ScenarioId, FixtureState, OutError))
	{
		TargetOutcomeFixtureBackup.Reset();
		return false;
	}

	RuntimeState = MoveTemp(FixtureState);
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (UGameXXKBattleBoardWidget* const Board = PlayerController->GetBattleBoardWidgetForTest())
		{
			Board->RefreshFromState();
		}
	}
	return true;
}

bool UGameXXKMVPSubsystem::ClearTargetOutcomeFixtureForTest(FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (!TargetOutcomeFixtureBackup.IsSet())
	{
		return true;
	}

	RuntimeState = MoveTemp(TargetOutcomeFixtureBackup.GetValue());
	TargetOutcomeFixtureBackup.Reset();
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		if (UGameXXKBattleBoardWidget* const Board = PlayerController->GetBattleBoardWidgetForTest())
		{
			Board->RefreshFromState();
		}
	}
	return true;
}

bool UGameXXKMVPSubsystem::IsTargetOutcomeFixtureActiveForTest() const
{
	return TargetOutcomeFixtureBackup.IsSet();
}

bool UGameXXKMVPSubsystem::ApplyRouteExitAcceptanceFixtureForTest(FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (RouteExitAcceptanceFixtureBackup.IsSet())
	{
		OutError = TEXT("A route-exit acceptance fixture is already active.");
		return false;
	}
	if (RuntimeState.Screen != EGameXXKScreen::DungeonMap
		|| !RuntimeState.bDungeonActive
		|| !RuntimeState.bHasGeneratedRouteMap
		|| RuntimeState.CardRun.bHasActiveCardBattle)
	{
		OutError = TEXT("Route-exit acceptance fixture requires an idle generated route map.");
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FGameXXKRouteMapNode* ReachableBattle = Candidate.RouteMapNodes.FindByPredicate([&Candidate](const FGameXXKRouteMapNode& Node)
	{
		return Candidate.ReachableRouteNodeIds.Contains(Node.NodeId)
			&& Node.NodeKind == EGameXXKNodeKind::Battle;
	});
	if (!ReachableBattle)
	{
		OutError = TEXT("Route-exit acceptance fixture found no reachable Battle node to convert.");
		return false;
	}

	ReachableBattle->NodeKind = EGameXXKNodeKind::Elite;
	Candidate.PlayerHP = Candidate.PlayerMaxHP;
	Candidate.PlayerMP = Candidate.PlayerMaxMP;
	Candidate.CardRun.RouteTravelMoney = 99;
	Candidate.CardRun.RouteProgress.ActualRouteCardAcquisitionCount = 29;
	FString ValidationError;
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, ValidationError))
	{
		OutError = FString::Printf(TEXT("Route-exit acceptance fixture is invalid: %s"), *ValidationError);
		return false;
	}

	RouteExitAcceptanceFixtureBackup.Emplace(RuntimeState);
	RuntimeState = MoveTemp(Candidate);
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		PlayerController->RefreshPlayerFlowWidgetsForTest();
	}
	return true;
}

bool UGameXXKMVPSubsystem::ClearRouteExitAcceptanceFixtureForTest(FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutError.Reset();
	if (!RouteExitAcceptanceFixtureBackup.IsSet())
	{
		return true;
	}
	RuntimeState = MoveTemp(RouteExitAcceptanceFixtureBackup.GetValue());
	RouteExitAcceptanceFixtureBackup.Reset();
	if (AGameXXKMVPPlayerController* const PlayerController =
		Cast<AGameXXKMVPPlayerController>(UGameplayStatics::GetPlayerController(GetWorld(), 0)))
	{
		PlayerController->RefreshPlayerFlowWidgetsForTest();
	}
	return true;
}

bool UGameXXKMVPSubsystem::IsRouteExitAcceptanceFixtureActiveForTest() const
{
	return RouteExitAcceptanceFixtureBackup.IsSet();
}

bool UGameXXKMVPSubsystem::StartGame()
{
	return StartNewGame();
}

bool UGameXXKMVPSubsystem::StartNewGame()
{
	PersistenceBoundaryDelegate.Broadcast();
	FGameXXKRuntimeState Candidate = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &Error))
	{
		return false;
	}

	FGameXXKCompanionRosterState& StarterRoster = Candidate.CardRun.CompanionRoster;
	const int32 StarterSeed = MakeStarterRecruitSequenceSeed();
	StarterRoster.RecruitSequenceSeed = StarterSeed;
	static const FName StarterTemplates[] = {
		FName(TEXT("Companion.Blade.01")),
		FName(TEXT("Companion.Guard.01")),
		FName(TEXT("Companion.Healer.01")),
		FName(TEXT("Companion.Hunter.01")),
		FName(TEXT("Companion.Sorcerer.01")),
		FName(TEXT("Companion.FormationMaster.01"))};
	FName StarterBladeId = NAME_None;
	for (int32 StarterIndex = 0; StarterIndex < UE_ARRAY_COUNT(StarterTemplates); ++StarterIndex)
	{
		FGameXXKCompanionRecruitResult RecruitResult;
		if (!FGameXXKCompanionRules::RecruitPermanentCompanion(
				StarterRoster,
				StarterTemplates[StarterIndex],
				StarterSeed + StarterIndex + 1,
				RecruitResult,
				&Error)
			|| RecruitResult.Outcome != EGameXXKCompanionRecruitOutcome::Recruited)
		{
			return false;
		}
		if (RecruitResult.Companion.Role == EGameXXKCharacterRole::Blade)
		{
			StarterBladeId = RecruitResult.Companion.InstanceId;
		}
	}

	// Every new game owns one representative of all six partner roles and all
	// six named NPC definitions. The initial fixed three-person party is hero +
	// Blade + Tusi Chief; clicking another portrait can replace either side slot.
	if (StarterBladeId.IsNone()
		|| !FGameXXKCompanionRules::SetActivePermanentCompanion(StarterRoster, StarterBladeId, &Error)
		|| !FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &Error)
		|| !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(
			Candidate,
			FName(TEXT("Npc.TusiChief")),
			{},
			&Error)
		|| !FGameXXKPartyFormationRules::Normalize(Candidate, &Error)
		|| !UGameXXKMVPRules::EnterWorldRegion(Candidate, UGameXXKMVPRules::RegionQingshan()))
	{
		return false;
	}
	FGameXXKTrainingTravelRuntime CandidateTravelRuntime;
	if (!BuildTrainingTravelRuntimeForState(Candidate, CandidateTravelRuntime))
	{
		return false;
	}

#if WITH_DEV_AUTOMATION_TESTS
	if (StartNewGameCommitGateForTest && !StartNewGameCommitGateForTest())
	{
		return false;
	}
#endif
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	LastSaveLoadError = FText::GetEmpty();
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateTravelRuntime);
	return true;
}

bool UGameXXKMVPSubsystem::StartGameFromSlot(FString SlotName, int32 UserIndex)
{
	return ContinueGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::ContinueGameFromSlot(FString SlotName, int32 UserIndex)
{
	return LoadGameFromSlot(SlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::SaveCurrentGame(FString SlotName, int32 UserIndex)
{
	PersistenceBoundaryDelegate.Broadcast();
	LastSaveLoadError = FText::GetEmpty();
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (APawn* PlayerPawn = GetLivePlayerPawnForSave(this))
	{
		Candidate.bHasPlayerLocation = true;
		Candidate.PlayerLocation = PlayerPawn->GetActorLocation();
	}
	if (Candidate.Training.bTravelActive && !Candidate.Training.bChallengeActive)
	{
		Candidate.Training.TravelLastUpdatedUnixSeconds = GetCurrentTravelUnixSeconds();
	}
	FString ValidationError;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &ValidationError)
		|| !FGameXXKSaveMigration::ValidateRuntimeState(Candidate, ValidationError))
	{
		LastSaveLoadError = FText::FromString(ValidationError);
		return false;
	}

	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	if (!SaveGame)
	{
		SetSaveMigrationFailure();
		return false;
	}

	SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(Candidate);
	if (!WriteSaveGameToSlot(SaveGame, ResolveSaveSlotName(SlotName), UserIndex))
	{
		return false;
	}
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::DoesSaveGameExist(FString SlotName, int32 UserIndex) const
{
	return UGameplayStatics::DoesSaveGameExist(ResolveSaveSlotName(SlotName), UserIndex);
}

bool UGameXXKMVPSubsystem::DeleteSaveGame(FString SlotName, int32 UserIndex)
{
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	return UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex)
		&& UGameplayStatics::DeleteGameInSlot(ResolvedSlotName, UserIndex);
}

bool UGameXXKMVPSubsystem::LoadGameFromSlot(FString SlotName, int32 UserIndex)
{
	PersistenceBoundaryDelegate.Broadcast();
	LastSaveLoadError = FText::GetEmpty();
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	if (!UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex))
	{
		return false;
	}

	UGameXXKSaveGame* OriginalSaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromSlot(ResolvedSlotName, UserIndex));
	if (!OriginalSaveGame)
	{
		SetSaveMigrationFailure();
		return false;
	}

	FGameXXKSaveState MigratedSaveState;
	FGameXXKSaveMigrationReport MigrationReport;
	if (OriginalSaveGame->SaveState.SaveVersion == FGameXXKSaveMigration::CurrentSaveVersion)
	{
		if (!FGameXXKSaveMigration::MigrateToCurrent(OriginalSaveGame->SaveState, MigratedSaveState, MigrationReport))
		{
			SetSaveMigrationFailure();
			return false;
		}
		FGameXXKTrainingTravelRuntime LoadedTravelRuntime;
		if (!ApplyOfflineTravelSinceLastUpdate(
			MigratedSaveState.RuntimeState,
			LoadedTravelRuntime,
			GetCurrentTravelUnixSeconds()))
		{
			SetSaveMigrationFailure();
			return false;
		}
		BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
		RuntimeState = MoveTemp(MigratedSaveState.RuntimeState);
		TrainingTravelRuntime = MoveTemp(LoadedTravelRuntime);
		return true;
	}

	// Invalid/future versions are rejected before any backup or main-slot write.
	if (OriginalSaveGame->SaveState.SaveVersion < 0
		|| OriginalSaveGame->SaveState.SaveVersion > FGameXXKSaveMigration::CurrentSaveVersion)
	{
		SetSaveMigrationFailure();
		return false;
	}

	uint32 OriginalChecksum = 0;
	if (!TryGetSaveObjectChecksum(OriginalSaveGame, OriginalChecksum))
	{
		SetSaveMigrationFailure();
		return false;
	}

	const FString BackupBaseSlotName = BuildMigrationBackupBaseSlotName(ResolvedSlotName);
	FString BackupSlotName;
	UGameXXKSaveGame* VerifiedBackup = nullptr;
	for (int32 AttemptNumber = 0; AttemptNumber <= MaximumMigrationBackupAttempts; ++AttemptNumber)
	{
		const FString CandidateBackupSlotName = BuildMigrationBackupAttemptSlotName(BackupBaseSlotName, AttemptNumber);
		if (UGameplayStatics::DoesSaveGameExist(CandidateBackupSlotName, UserIndex))
		{
			UGameXXKSaveGame* CandidateBackup = Cast<UGameXXKSaveGame>(
				UGameplayStatics::LoadGameFromSlot(CandidateBackupSlotName, UserIndex));
			uint32 CandidateChecksum = 0;
			if (CandidateBackup
				&& TryGetSaveObjectChecksum(CandidateBackup, CandidateChecksum)
				&& CandidateChecksum == OriginalChecksum
				&& AreSaveObjectsSerializationEquivalent(OriginalSaveGame, CandidateBackup))
			{
				VerifiedBackup = CandidateBackup;
				BackupSlotName = CandidateBackupSlotName;
				break;
			}
			continue;
		}

		if (!WriteSaveGameToSlot(OriginalSaveGame, CandidateBackupSlotName, UserIndex))
		{
			SetSaveMigrationFailure();
			return false;
		}
		UGameXXKSaveGame* CandidateBackup = Cast<UGameXXKSaveGame>(
			UGameplayStatics::LoadGameFromSlot(CandidateBackupSlotName, UserIndex));
		uint32 CandidateChecksum = 0;
		if (!CandidateBackup
			|| !TryGetSaveObjectChecksum(CandidateBackup, CandidateChecksum)
			|| CandidateChecksum != OriginalChecksum
			|| !AreSaveObjectsSerializationEquivalent(OriginalSaveGame, CandidateBackup))
		{
			SetSaveMigrationFailure();
			return false;
		}
		VerifiedBackup = CandidateBackup;
		BackupSlotName = CandidateBackupSlotName;
		break;
	}
	if (!VerifiedBackup || BackupSlotName.IsEmpty())
	{
		SetSaveMigrationFailure();
		return false;
	}

	if (!FGameXXKSaveMigration::MigrateToCurrent(OriginalSaveGame->SaveState, MigratedSaveState, MigrationReport))
	{
		SetSaveMigrationFailure();
		return false;
	}
	FGameXXKTrainingTravelRuntime MigratedTravelRuntime;
	if (!ApplyOfflineTravelSinceLastUpdate(
		MigratedSaveState.RuntimeState,
		MigratedTravelRuntime,
		GetCurrentTravelUnixSeconds()))
	{
		SetSaveMigrationFailure();
		return false;
	}
	MigrationReport.SourceChecksum = OriginalChecksum;
	MigrationReport.BackupChecksum = OriginalChecksum;
	MigrationReport.BackupSlotName = BackupSlotName;

	UGameXXKSaveGame* MigratedSaveGame = Cast<UGameXXKSaveGame>(
		UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	if (!MigratedSaveGame)
	{
		SetSaveMigrationFailure();
		return false;
	}
	MigratedSaveGame->SaveState = MigratedSaveState;
	const auto RestoreOriginalMain = [this, VerifiedBackup, OriginalSaveGame, OriginalChecksum, &ResolvedSlotName, UserIndex]()
	{
		if (!WriteSaveGameToSlot(VerifiedBackup, ResolvedSlotName, UserIndex))
		{
			return false;
		}
		UGameXXKSaveGame* RestoredSaveGame = Cast<UGameXXKSaveGame>(
			UGameplayStatics::LoadGameFromSlot(ResolvedSlotName, UserIndex));
		uint32 RestoredChecksum = 0;
		return TryGetSaveObjectChecksum(RestoredSaveGame, RestoredChecksum)
			&& RestoredChecksum == OriginalChecksum
			&& AreSaveObjectsSerializationEquivalent(OriginalSaveGame, RestoredSaveGame);
	};

	if (!WriteSaveGameToSlot(MigratedSaveGame, ResolvedSlotName, UserIndex))
	{
		const bool bRollbackVerified = RestoreOriginalMain();
		if (bRollbackVerified)
		{
			SetSaveMigrationFailure();
		}
		else
		{
			SetSaveRollbackFailure();
		}
		return false;
	}

	UGameXXKSaveGame* VerifiedMigratedSave = Cast<UGameXXKSaveGame>(
		UGameplayStatics::LoadGameFromSlot(ResolvedSlotName, UserIndex));
	uint32 MigratedChecksum = 0;
	uint32 VerifiedMigratedChecksum = 0;
	if (!TryGetSaveObjectChecksum(MigratedSaveGame, MigratedChecksum)
		|| !TryGetSaveObjectChecksum(VerifiedMigratedSave, VerifiedMigratedChecksum)
		|| MigratedChecksum != VerifiedMigratedChecksum
		|| !AreSaveObjectsSerializationEquivalent(MigratedSaveGame, VerifiedMigratedSave))
	{
		const bool bRollbackVerified = RestoreOriginalMain();
		if (bRollbackVerified)
		{
			SetSaveMigrationFailure();
		}
		else
		{
			SetSaveRollbackFailure();
		}
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(MigratedSaveState.RuntimeState);
	TrainingTravelRuntime = MoveTemp(MigratedTravelRuntime);
	return true;
}

bool UGameXXKMVPSubsystem::LoadOrCreateGame(FString SlotName, int32 UserIndex)
{
	const FString ResolvedSlotName = ResolveSaveSlotName(SlotName);
	if (UGameplayStatics::DoesSaveGameExist(ResolvedSlotName, UserIndex))
	{
		return ContinueGameFromSlot(ResolvedSlotName, UserIndex);
	}

	return StartNewGame();
}

FText UGameXXKMVPSubsystem::GetLastSaveLoadError() const
{
	return LastSaveLoadError;
}

bool UGameXXKMVPSubsystem::GetEquipmentWarehouseSnapshot(TArray<FName>& OutOrderedInstanceIds) const
{
	OutOrderedInstanceIds.Reset();
	FString Error;
	if (!FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
		RuntimeState.EquipmentCollection,
		RuntimeState.CardRun.CompanionRoster,
		&Error))
	{
		return false;
	}
	OutOrderedInstanceIds = RuntimeState.EquipmentCollection.WarehouseInstanceIds;
	return true;
}

bool UGameXXKMVPSubsystem::GetEquipmentLoadoutSnapshot(
	const FName CharacterId,
	FGameXXKEquipmentLoadoutSnapshot& OutSnapshot) const
{
	OutSnapshot = FGameXXKEquipmentLoadoutSnapshot();
	FGameXXKCharacterStats BareStats;
	FString Error;
	return ResolvePermanentEquipmentOwnerBareStats(RuntimeState, CharacterId, BareStats)
		&& FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			RuntimeState.EquipmentCollection,
			CharacterId,
			BareStats,
			OutSnapshot,
			&Error);
}

bool UGameXXKMVPSubsystem::GetEquipmentTooltipSnapshot(
	const FName InstanceId,
	const FName CompareCharacterId,
	FGameXXKEquipmentTooltipSnapshot& OutSnapshot) const
{
	OutSnapshot = FGameXXKEquipmentTooltipSnapshot();
	FGameXXKCharacterStats CompareBareStats;
	FString Error;
	return ResolvePermanentEquipmentOwnerBareStats(RuntimeState, CompareCharacterId, CompareBareStats)
		&& FGameXXKEquipmentRules::BuildTooltipSnapshot(
			RuntimeState.EquipmentCollection,
			InstanceId,
			CompareCharacterId,
			CompareBareStats,
			OutSnapshot,
			&Error);
}

bool UGameXXKMVPSubsystem::NormalizeDesktopInventoryState()
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::MoveDesktopInventoryEntry(
	const EGameXXKDesktopItemContainer FromContainer,
	const int32 FromSlotIndex,
	const EGameXXKDesktopItemContainer ToContainer,
	const int32 ToSlotIndex,
	FString* OutError)
{
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKDesktopInventoryRules::MoveEntry(
		Candidate,
		FromContainer,
		FromSlotIndex,
		ToContainer,
		ToSlotIndex,
		OutError))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::SortEquipmentWarehouse()
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState)
		|| !FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
			RuntimeState.EquipmentCollection,
			RuntimeState.CardRun.CompanionRoster))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!FGameXXKEquipmentRules::SortWarehouseInstanceIds(Candidate.EquipmentCollection, &Error)
		|| !FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
			Candidate.EquipmentCollection,
			Candidate.CardRun.CompanionRoster,
			&Error))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::EquipEquipmentInstance(
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	const FName InstanceId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::Equip(Candidate, CharacterId, Slot, InstanceId, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::EquipEquipmentFromDesktopCell(
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	const EGameXXKDesktopItemContainer SourceContainer,
	const int32 SourceSlotIndex,
	const FName ExpectedInstanceId,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}

	TArray<FGameXXKDesktopInventoryEntryKey>* SourceSlots = nullptr;
	if (SourceContainer == EGameXXKDesktopItemContainer::Backpack)
	{
		SourceSlots = &Candidate.DesktopInventory.BackpackSlots;
	}
	else if (SourceContainer == EGameXXKDesktopItemContainer::Warehouse)
	{
		SourceSlots = &Candidate.DesktopInventory.WarehouseSlots;
	}
	if (!SourceSlots || !SourceSlots->IsValidIndex(SourceSlotIndex))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}

	const FGameXXKDesktopInventoryEntryKey IncomingEntry = (*SourceSlots)[SourceSlotIndex];
	if (ExpectedInstanceId.IsNone() || IncomingEntry.EntryId != ExpectedInstanceId)
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest);
		return false;
	}
	if (!IncomingEntry.IsValid() || !IncomingEntry.bEquipmentInstance)
	{
		SetEquipmentTransactionFailure(
			OutResult,
			IncomingEntry.IsValid()
				? EGameXXKEquipmentTransactionError::InvalidRequest
				: EGameXXKEquipmentTransactionError::InstanceMissing);
		return false;
	}
	const FName IncomingInstanceId = IncomingEntry.EntryId;
	if (!FGameXXKEquipmentRules::FindInstance(Candidate.EquipmentCollection, IncomingInstanceId))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InstanceMissing);
		return false;
	}
	const bool bSourcePartitionedToWarehouse =
		Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(IncomingInstanceId);
	if (!Candidate.EquipmentCollection.WarehouseInstanceIds.Contains(IncomingInstanceId)
		|| bSourcePartitionedToWarehouse
			!= (SourceContainer == EGameXXKDesktopItemContainer::Warehouse))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
		return false;
	}

	const FGameXXKEquipmentLoadout* DestinationLoadout =
		Candidate.EquipmentCollection.CharacterLoadouts.Find(CharacterId);
	const FName DisplacedInstanceId = DestinationLoadout
		? FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*DestinationLoadout, Slot)
		: NAME_None;
	(*SourceSlots)[SourceSlotIndex] = FGameXXKDesktopInventoryEntryKey();
	if (SourceContainer == EGameXXKDesktopItemContainer::Warehouse)
	{
		Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveSingle(IncomingInstanceId);
	}

	if (!FGameXXKEquipmentEconomyRules::Equip(
		Candidate,
		CharacterId,
		Slot,
		IncomingInstanceId,
		OutResult))
	{
		return false;
	}

	if (!DisplacedInstanceId.IsNone())
	{
		(*SourceSlots)[SourceSlotIndex] =
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(DisplacedInstanceId);
		if (SourceContainer == EGameXXKDesktopItemContainer::Warehouse)
		{
			Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.AddUnique(DisplacedInstanceId);
		}
		else
		{
			Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveSingle(DisplacedInstanceId);
		}
	}

	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error)
		|| !FGameXXKDesktopInventoryRules::Validate(Candidate, &Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	const FGameXXKDesktopInventoryEntryKey ProjectedSourceEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(Candidate, SourceContainer, SourceSlotIndex);
	if (!DisplacedInstanceId.IsNone()
		&& ProjectedSourceEntry
			!= FGameXXKDesktopInventoryRules::MakeEquipmentEntry(DisplacedInstanceId))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}
	if (DisplacedInstanceId.IsNone() && ProjectedSourceEntry == IncomingEntry)
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid);
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::UnequipEquipmentSlot(
	const FName CharacterId,
	const EGameXXKEquipmentSlot Slot,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::Unequip(Candidate, CharacterId, Slot, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::EnhanceEquipmentInstance(const FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKToolInputRef Input;
	if (!BuildToolRefForEquipment(RuntimeState, InstanceId, Input))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
		return false;
	}
	return ExecuteToolEnhance(Input, OutResult);
}

bool UGameXXKMVPSubsystem::BeginEquipmentReforge(
	const FName InstanceId,
	const int32 AffixIndex,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKToolInputRef Input;
	if (!BuildToolRefForEquipment(RuntimeState, InstanceId, Input))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
		return false;
	}
	return ExecuteToolBeginReforge(Input, AffixIndex, OutResult);
}

bool UGameXXKMVPSubsystem::ResolveEquipmentReforge(const bool bAccept, FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	return ExecuteToolResolveReforge(bAccept, OutResult);
}

bool UGameXXKMVPSubsystem::DismantleEquipmentInstances(
	const TArray<FName>& InstanceIds,
	const bool bConfirmedProtected,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	TArray<FGameXXKToolInputRef> Inputs;
	for (const FName InstanceId : InstanceIds)
	{
		FGameXXKToolInputRef Input;
		if (!BuildToolRefForEquipment(RuntimeState, InstanceId, Input))
		{
			SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
			return false;
		}
		Inputs.Add(Input);
	}
	return ExecuteToolDismantle(Inputs, bConfirmedProtected, OutResult);
}

bool UGameXXKMVPSubsystem::SetToolSelectedCraftingLevel(const int32 Level)
{
	if (Level < FGameXXKEquipmentToolRules::MinimumLevel || Level > RuntimeState.ToolProgress.Level)
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.ToolProgress.SelectedCraftingLevel = Level;
	return true;
}

bool UGameXXKMVPSubsystem::SetToolAutoFillIncludesWarehouse(const bool bIncludeWarehouse)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.DesktopInventory.bToolAutoFillIncludesWarehouse = bIncludeWarehouse;
	return true;
}

bool UGameXXKMVPSubsystem::ExecuteToolDismantle(
	const TArray<FGameXXKToolInputRef>& Inputs,
	const bool bConfirmed,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentToolRules::Dismantle(Candidate, Inputs, bConfirmed, OutResult)) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::ExecuteToolCombine(
	const EGameXXKToolCombineKind Kind,
	const TArray<FGameXXKToolInputRef>& Inputs,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	const bool bSucceeded = Kind == EGameXXKToolCombineKind::Equipment
		? FGameXXKEquipmentToolRules::CombineEquipment(Candidate, Inputs, OutResult)
		: (Inputs.Num() == 1 && FGameXXKEquipmentToolRules::CombineGem(Candidate, Inputs[0], OutResult));
	if (!bSucceeded) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::BuildToolCombineAutoFill(
	const EGameXXKToolCombineKind Kind,
	const bool bIncludeWarehouse,
	TArray<FGameXXKToolInputRef>& OutInputs,
	FString* OutError) const
{
	return FGameXXKEquipmentToolRules::BuildCombineAutoFill(RuntimeState, Kind, bIncludeWarehouse, OutInputs, OutError);
}

bool UGameXXKMVPSubsystem::ExecuteToolEnhance(
	const FGameXXKToolInputRef& Input,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentToolRules::Enhance(Candidate, Input, OutResult)) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::ExecuteToolBeginReforge(
	const FGameXXKToolInputRef& Input,
	const int32 AffixIndex,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentToolRules::BeginReforge(Candidate, Input, AffixIndex, OutResult)) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::ExecuteToolResolveReforge(
	const bool bAccept,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentToolRules::ResolveReforge(Candidate, bAccept, OutResult)) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::ExecuteToolSocket(
	const FGameXXKSocketGemRequest& Request,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}
	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentToolRules::SocketGem(Candidate, Request, OutResult)) return false;
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::WriteSaveGameToSlot(USaveGame* SaveGame, const FString& SlotName, const int32 UserIndex)
{
#if WITH_DEV_AUTOMATION_TESTS
	if (SaveSlotWriteDelegateForTest.IsBound())
	{
		return SaveSlotWriteDelegateForTest.Execute(SaveGame, SlotName, UserIndex);
	}
#endif
	return UGameplayStatics::SaveGameToSlot(SaveGame, SlotName, UserIndex);
}

void UGameXXKMVPSubsystem::SetSaveMigrationFailure()
{
	LastSaveLoadError = FGameXXKEquipmentRules::GetTransactionErrorMessage(
		EGameXXKEquipmentTransactionError::SaveMigrationFailed);
}

void UGameXXKMVPSubsystem::SetSaveRollbackFailure()
{
	// The fixed-name backup was already written and verified before touching the main
	// slot. If automatic main-slot rollback fails, never claim the main was preserved.
	LastSaveLoadError = NSLOCTEXT(
		"GameXXKSaveMigration",
		"RollbackFailed",
		"存档迁移失败，原存档仍保存在迁移备份中，请勿覆盖当前存档。");
}

#if WITH_DEV_AUTOMATION_TESTS
void UGameXXKMVPSubsystem::SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate InDelegate)
{
	SaveSlotWriteDelegateForTest = MoveTemp(InDelegate);
}

void UGameXXKMVPSubsystem::ResetSaveSlotWriteDelegateForTest()
{
	SaveSlotWriteDelegateForTest.Unbind();
}

void UGameXXKMVPSubsystem::SetStartNewGameCommitGateForTest(TFunction<bool()> InGate)
{
	StartNewGameCommitGateForTest = MoveTemp(InGate);
}

void UGameXXKMVPSubsystem::ResetStartNewGameCommitGateForTest()
{
	StartNewGameCommitGateForTest = nullptr;
}
#endif

FString UGameXXKMVPSubsystem::GetDefaultSaveSlotName()
{
	return DefaultSaveSlotName;
}

int32 UGameXXKMVPSubsystem::GetManualSaveSlotCount()
{
	return ManualSaveSlotCount;
}

FString UGameXXKMVPSubsystem::GetManualSaveSlotName(int32 SlotIndex)
{
	const int32 ClampedSlotIndex = FMath::Clamp(SlotIndex, 0, ManualSaveSlotCount - 1);
	return FString::Printf(TEXT("%s%d"), *ManualSaveSlotPrefix, ClampedSlotIndex + 1);
}

bool UGameXXKMVPSubsystem::OpenWorldMap()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectWorldRegion(FName RegionId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, RegionId);
}

bool UGameXXKMVPSubsystem::EnsureQingshanTownRuntimeForDirectMap()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (RuntimeState.Screen == EGameXXKScreen::Town && RuntimeState.CurrentRegion == UGameXXKMVPRules::RegionQingshan())
	{
		return true;
	}
	if (RuntimeState.Screen == EGameXXKScreen::MainMenu)
	{
		RuntimeState = UGameXXKMVPRules::CreateNewGame();
		UGameXXKMVPRules::OpenWorldMap(RuntimeState);
		return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, UGameXXKMVPRules::RegionQingshan());
	}
	if (RuntimeState.Screen == EGameXXKScreen::WorldMap)
	{
		return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, UGameXXKMVPRules::RegionQingshan());
	}
	return false;
}

bool UGameXXKMVPSubsystem::EnsureDesktopTrainingRuntimeForDirectMap()
{
	if (RuntimeState.Screen == EGameXXKScreen::MainMenu)
	{
		// The isolated HUD map is itself a playable entry surface. A fresh direct
		// launch therefore needs the full new-game initialization (starter roster,
		// active party projection, card run, and Training runtime), not the lighter
		// 3D-town editor normalization used by EnsureQingshanTownRuntimeForDirectMap.
		return StartNewGame();
	}
	return EnsureQingshanTownRuntimeForDirectMap();
}

bool UGameXXKMVPSubsystem::IsRegionUnlocked(FName RegionId) const
{
	return RuntimeState.UnlockedRegions.Contains(RegionId);
}

bool UGameXXKMVPSubsystem::AcceptQuest()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::AcceptTownQuest(RuntimeState);
}

void UGameXXKMVPSubsystem::RecordQuestNpcLocation(FVector Location)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.bHasQuestNpcLocation = true;
	RuntimeState.QuestNpcLocation = Location;
}

void UGameXXKMVPSubsystem::RecordPlayerLocation(FVector Location)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState.bHasPlayerLocation = true;
	RuntimeState.PlayerLocation = Location;
}

bool UGameXXKMVPSubsystem::CanEnterDungeon() const
{
	return UGameXXKMVPRules::CanEnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::OpenDungeonFromTownExit()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::EnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectDungeonNode(EGameXXKNodeKind ExpectedNode)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::AdvanceDungeonNode(RuntimeState, ExpectedNode);
}

bool UGameXXKMVPSubsystem::SelectRouteNodeById(int32 NodeId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::SelectRouteNodeById(RuntimeState, NodeId);
}

bool UGameXXKMVPSubsystem::RetreatCurrentBattleToRoute()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::RetreatCurrentBattleToRoute(RuntimeState);
}

bool UGameXXKMVPSubsystem::IsBattleAutoPlayEnabled() const
{
	return bBattleAutoPlayEnabled;
}

bool UGameXXKMVPSubsystem::SetBattleAutoPlayEnabled(const bool bEnabled)
{
	bBattleAutoPlayEnabled = bEnabled;
	return true;
}

bool UGameXXKMVPSubsystem::ResolveBattleVictory(bool bBossBattle)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolveBattleVictory(RuntimeState, bBossBattle);
}

bool UGameXXKMVPSubsystem::ResolvePendingBattleRewardChoiceAndFinish(
	const int32 OptionIndex,
	const FName ReplacementEntryId,
	FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
		RuntimeState,
		OptionIndex,
		ReplacementEntryId,
		OutError);
}

bool UGameXXKMVPSubsystem::SkipPendingRouteRewardAndFinish(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::ExecuteBattleBasicAttack(int32 PartyIndex, int32 EnemyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ExecuteBattleBasicAttack(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleCraneWingSlash(int32 PartyIndex, int32 EnemyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ExecuteBattleCraneWingSlash(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleGuiyuanArt(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ExecuteBattleGuiyuanArt(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleDefend(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ExecuteBattleDefend(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleHealingPowder(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ExecuteBattleHealingPowder(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ResolveEventReward(bool bTakeGold)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolveEventReward(RuntimeState, bTakeGold);
}

bool UGameXXKMVPSubsystem::ResolveRouteEncounterChoice(const int32 ChoiceIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolveRouteEncounterChoice(RuntimeState, ChoiceIndex);
}

bool UGameXXKMVPSubsystem::AcceptRouteEventNpcSupport()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::AcceptRouteEventNpcSupport(RuntimeState);
}

bool UGameXXKMVPSubsystem::ResolveCampReward(const bool bHealNow)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolveCampReward(RuntimeState, bHealNow);
}

bool UGameXXKMVPSubsystem::EnsureRouteMerchantStock(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::EnsureRouteMerchantStock(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::GetRouteMerchantView(
	FGameXXKRouteMerchantView& OutView,
	FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::GetRouteMerchantView(RuntimeState, OutView, OutError);
}

bool UGameXXKMVPSubsystem::RefreshRouteMerchant(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::RefreshRouteMerchant(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::PreviewRouteMerchantPurchase(
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchasePreview& OutPreview,
	FString* OutError) const
{
	return UGameXXKMVPRules::PreviewRouteMerchantPurchase(
		RuntimeState,
		OfferId,
		ReplacementEntryId,
		OutPreview,
		OutError);
}

bool UGameXXKMVPSubsystem::PurchaseRouteMerchant(
	const FName OfferId,
	const FName ReplacementEntryId,
	FGameXXKRouteMerchantPurchaseResult& OutResult)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::PurchaseRouteMerchant(
		RuntimeState,
		OfferId,
		ReplacementEntryId,
		OutResult);
}

bool UGameXXKMVPSubsystem::CancelPendingRouteMerchantPurchase(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::CancelPendingRouteMerchantPurchase(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::ResolveMerchantRouteNode()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::ResolveMerchantRouteNode(RuntimeState);
}

bool UGameXXKMVPSubsystem::FailDungeonToTown()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::FailDungeonToTown(RuntimeState);
}

bool UGameXXKMVPSubsystem::PreviewAbandonedRouteSettlement(
	FGameXXKRouteSettlementReceipt& OutReceipt,
	FString* OutError) const
{
	return FGameXXKRouteSettlementRules::Preview(
		RuntimeState,
		EGameXXKRouteTerminalOutcome::Abandoned,
		OutReceipt,
		OutError);
}

bool UGameXXKMVPSubsystem::AbandonDungeonToTown()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::AbandonDungeonToTown(RuntimeState);
}

bool UGameXXKMVPSubsystem::BuyItem(FName ItemId, int32 Quantity)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::BuyItem(RuntimeState, ItemId, Quantity);
}

bool UGameXXKMVPSubsystem::SellItem(FName ItemId, int32 Quantity)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::SellItem(RuntimeState, ItemId, Quantity);
}

bool UGameXXKMVPSubsystem::CanSellItem(FName ItemId) const
{
	return UGameXXKMVPRules::CanSellItem(RuntimeState, ItemId);
}

int32 UGameXXKMVPSubsystem::GetItemEnhancementLevel(FName ItemId) const
{
	return UGameXXKMVPRules::GetItemEnhancementLevel(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::CanEnhanceItem(FName ItemId) const
{
	return UGameXXKMVPRules::CanEnhanceItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::EnhanceItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::EnhanceItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::CanDecomposeItem(FName ItemId) const
{
	return UGameXXKMVPRules::CanDecomposeItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::DecomposeItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::DecomposeItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::UseHealingItem()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::UseHealingItem(RuntimeState);
}

bool UGameXXKMVPSubsystem::UseItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::UseItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::EquipItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::EquipItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::UnequipItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::UnequipItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::OpenTownPanel(EGameXXKTownPanelMode PanelMode)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::OpenTownPanel(RuntimeState, PanelMode);
}

bool UGameXXKMVPSubsystem::CloseTownPanel()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::CloseTownPanel(RuntimeState);
}

int32 UGameXXKMVPSubsystem::GetItemCount(FName ItemId) const
{
	return UGameXXKMVPRules::GetItemCount(RuntimeState, ItemId);
}

TArray<FGameXXKCodexEntryView> UGameXXKMVPSubsystem::GetCodexEntryViews(EGameXXKCodexCategory Category) const
{
	return UGameXXKMVPRules::BuildCodexEntryViews(RuntimeState, Category);
}

int32 UGameXXKMVPSubsystem::GetCodexEntryCount(EGameXXKCodexCategory Category) const
{
	return UGameXXKMVPRules::GetCodexEntryCount(Category);
}

int32 UGameXXKMVPSubsystem::GetDiscoveredCodexEntryCount(EGameXXKCodexCategory Category) const
{
	return UGameXXKMVPRules::GetDiscoveredCodexEntryCount(RuntimeState, Category);
}

bool UGameXXKMVPSubsystem::HasUnreadCodexEntries() const
{
	return UGameXXKMVPRules::HasUnreadCodexEntries(RuntimeState);
}

bool UGameXXKMVPSubsystem::MarkCodexEntryRead(FName EntryId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return UGameXXKMVPRules::MarkCodexEntryRead(RuntimeState, EntryId);
}

TArray<FGameXXKPermanentCompanion> UGameXXKMVPSubsystem::GetPermanentCompanionViews() const
{
	return RuntimeState.CardRun.CompanionRoster.PermanentCompanions;
}

bool UGameXXKMVPSubsystem::TryGetPermanentCompanionView(const FName InstanceId, FGameXXKPermanentCompanion& OutCompanion) const
{
	OutCompanion = FGameXXKPermanentCompanion();
	const FGameXXKPermanentCompanion* Companion = RuntimeState.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate([InstanceId](const FGameXXKPermanentCompanion& Candidate)
	{
		return Candidate.InstanceId == InstanceId;
	});
	if (!Companion)
	{
		return false;
	}
	OutCompanion = *Companion;
	return true;
}

int32 UGameXXKMVPSubsystem::GetPermanentCompanionRosterCapacity() const
{
	return FGameXXKCompanionRules::MaxPermanentCompanions;
}

bool UGameXXKMVPSubsystem::IsCompanionLoadoutMutationLocked() const
{
	return IsCompanionConfigurationLocked(RuntimeState);
}

TArray<FName> UGameXXKMVPSubsystem::GetHeroCardLoadout() const
{
	return RuntimeState.CardRun.HeroSelectedCardIds;
}

FGameXXKQuestNpcCardSelection UGameXXKMVPSubsystem::GetQuestNpcCardLoadout() const
{
	return RuntimeState.CardRun.PartySelection.QuestNpc;
}

FGameXXKOrderedPartyFormation UGameXXKMVPSubsystem::GetOrderedPartyFormation() const
{
	FString Error;
	if (!FGameXXKPartyFormationRules::Validate(
		RuntimeState,
		RuntimeState.CardRun.OrderedFormation,
		&Error))
	{
		UE_LOG(
			LogGameXXKMVPSubsystem,
			Error,
			TEXT("GetOrderedPartyFormation rejected invalid raw formation: %s"),
			*Error);
	}
	return RuntimeState.CardRun.OrderedFormation;
}

bool UGameXXKMVPSubsystem::SetOrderedPartyFormation(
	const FGameXXKOrderedPartyFormation& Formation,
	FString& OutError)
{
	OutError.Reset();
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		OutError = TEXT("Party formation can change only at the unlocked town workbench.");
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKPartyFormationRules::Validate(Candidate, Formation, &OutError))
	{
		return false;
	}
	Candidate.CardRun.OrderedFormation = Formation;
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, OutError))
	{
		return false;
	}
	if (FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
		&RuntimeState,
		&Candidate,
		PPF_None))
	{
		return true;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::PrepareCompanionRosterForTown()
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		// Hidden town widgets are refreshed alongside the battle board.  A rejected
		// town-only request must be observational, otherwise it discards the
		// development-only battle HUD overlay before the scene presenter reads it.
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	return EnsureCompanionCardRun(RuntimeState);
}

bool UGameXXKMVPSubsystem::RecruitPermanentCompanionFromSeed(const int32 RecruitOrderSeed, FGameXXKCompanionRecruitResult& OutResult)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutResult = FGameXXKCompanionRecruitResult();
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FGameXXKCompanionRecruitOrder Order;
	FString Error;
	return FGameXXKCompanionRules::CreateRecruitOrder(RuntimeState.CardRun.CompanionRoster, RecruitOrderSeed, Order, &Error)
		&& FGameXXKCompanionRules::ResolvePendingRecruitOrder(RuntimeState.CardRun.CompanionRoster, OutResult, &Error);
}

bool UGameXXKMVPSubsystem::StartRandomPermanentCompanionRecruitment(FGameXXKCompanionRecruitResult& OutResult)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	OutResult = FGameXXKCompanionRecruitResult();
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FString Error;
	return FGameXXKCompanionRules::CreateAndResolveNextRecruitment(RuntimeState.CardRun.CompanionRoster, OutResult, &Error);
}

bool UGameXXKMVPSubsystem::TryGetPendingPermanentCompanionRecruitment(FGameXXKPermanentCompanion& OutCandidate) const
{
	OutCandidate = FGameXXKPermanentCompanion();
	const FGameXXKPendingCompanionRecruitment& Pending = RuntimeState.CardRun.CompanionRoster.PendingRecruitment;
	if (!Pending.bHasPendingRecruitment)
	{
		return false;
	}

	OutCandidate = Pending.Candidate;
	return true;
}

bool UGameXXKMVPSubsystem::ResolvePendingPermanentCompanionReplacement(
	const FName DismissedInstanceId,
	const FName ActivePermanentCompanionInstanceIdAfterReplacement)
{
	FGameXXKEquipmentTransactionResult Result;
	return ResolvePendingPermanentCompanionReplacement(
		DismissedInstanceId,
		ActivePermanentCompanionInstanceIdAfterReplacement,
		Result);
}

bool UGameXXKMVPSubsystem::ResolvePendingPermanentCompanionReplacement(
	const FName DismissedInstanceId,
	const FName ActivePermanentCompanionInstanceIdAfterReplacement,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return OutResult.bSucceeded;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FGameXXKPermanentCompanion* DismissedCompanion = FindPermanentCompanion(Candidate, DismissedInstanceId);
	if (!DismissedCompanion
		|| !IsValidPostReplacementActiveCompanion(
			Candidate.CardRun.CompanionRoster,
			DismissedInstanceId,
			ActivePermanentCompanionInstanceIdAfterReplacement))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidOwner);
		return OutResult.bSucceeded;
	}

	FGameXXKEquipmentTransactionResult EquipmentResult = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(
		Candidate.EquipmentCollection,
		DismissedInstanceId);
	if (!EquipmentResult.bSucceeded)
	{
		OutResult = MoveTemp(EquipmentResult);
		return OutResult.bSucceeded;
	}

	if (HasUnclaimedDismissalRefund(*DismissedCompanion, Candidate.EquipmentCollection))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest);
		return OutResult.bSucceeded;
	}

	const FName PendingCandidateInstanceId =
		Candidate.CardRun.CompanionRoster.PendingRecruitment.Candidate.InstanceId;
	FGameXXKCompanionDismissalRefund LegacyRefund;
	FString Error;
	if (!FGameXXKCompanionRules::ResolvePendingRecruitment(
		Candidate.CardRun.CompanionRoster,
		DismissedInstanceId,
		ActivePermanentCompanionInstanceIdAfterReplacement,
		LegacyRefund,
		&Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest);
		return OutResult.bSucceeded;
	}
	if (!FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
		Candidate.EquipmentCollection,
		Candidate.CardRun.CompanionRoster,
		&Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid);
		return OutResult.bSucceeded;
	}
	if (!EnsureCompanionCardRun(Candidate))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest);
		return OutResult.bSucceeded;
	}

	const TArray<FName> PreferredReplacementIds = {
		ActivePermanentCompanionInstanceIdAfterReplacement,
		PendingCandidateInstanceId};
	if (!RepairFormationAfterCompanionRemoval(
		Candidate,
		DismissedInstanceId,
		PreferredReplacementIds,
		ActivePermanentCompanionInstanceIdAfterReplacement,
		Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::InvalidOwner);
		return OutResult.bSucceeded;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	if (!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)
		|| !FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::SaveMigrationFailed);
		return OutResult.bSucceeded;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	OutResult = MoveTemp(EquipmentResult);
	return OutResult.bSucceeded;
}

bool UGameXXKMVPSubsystem::DismissPermanentCompanion(const FName InstanceId)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	const FGameXXKPermanentCompanion* Companion = FindPermanentCompanion(Candidate, InstanceId);
	if (!Companion)
	{
		return false;
	}
	// A temporary NPC can retire at route settlement, so a current roster must
	// always retain a second permanent companion for exact-slot replacement.
	if (Candidate.CardRun.CompanionRoster.PermanentCompanions.Num()
		<= FGameXXKPartyFormationRules::MinimumOwnedPermanentCompanions)
	{
		return false;
	}

	FGameXXKEquipmentTransactionResult EquipmentResult = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(
		Candidate.EquipmentCollection,
		InstanceId);
	if (!EquipmentResult.bSucceeded)
	{
		return false;
	}

	if (Candidate.CardRun.PartySelection.ActivePermanentCompanionInstanceId == InstanceId)
	{
		Candidate.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
	}
	Candidate.CardRun.CompanionRoster.PermanentCompanions.RemoveAll([InstanceId](const FGameXXKPermanentCompanion& Entry)
	{
		return Entry.InstanceId == InstanceId;
	});

	FString Error;
	if (!EnsureCompanionCardRun(Candidate)
		|| !RepairFormationAfterCompanionRemoval(
			Candidate,
			InstanceId,
			TArray<FName>(),
			NAME_None,
			Error))
	{
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	if (!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)
		|| !FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::DiscardPendingPermanentCompanionRecruitment()
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FString Error;
	return FGameXXKCompanionRules::DiscardPendingRecruitment(RuntimeState.CardRun.CompanionRoster, &Error);
}

int32 UGameXXKMVPSubsystem::GetPermanentCompanionSigilCount() const
{
	return FMath::Max(0, RuntimeState.CardRun.CompanionRoster.SigilCount);
}

bool UGameXXKMVPSubsystem::SetActivePermanentCompanion(const FName InstanceId)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || InstanceId.IsNone())
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!EnsureCompanionCardRun(Candidate)
		|| !Candidate.CardRun.CompanionRoster.PermanentCompanions.ContainsByPredicate([InstanceId](const FGameXXKPermanentCompanion& Companion)
		{
			return Companion.InstanceId == InstanceId;
		}))
	{
		return false;
	}
	TArray<FGameXXKPartyMemberRef>& Members = Candidate.CardRun.OrderedFormation.Members;
	const int32 FirstCompanionSlot = Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
	});
	if (FirstCompanionSlot == INDEX_NONE)
	{
		return false;
	}
	const int32 SelectedCompanionSlot = Members.IndexOfByPredicate([InstanceId](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion
			&& Ref.MemberId == InstanceId;
	});
	if (SelectedCompanionSlot != INDEX_NONE && SelectedCompanionSlot != FirstCompanionSlot)
	{
		Swap(Members[FirstCompanionSlot], Members[SelectedCompanionSlot]);
	}
	else
	{
		Members[FirstCompanionSlot].Kind = EGameXXKPartyMemberKind::PermanentCompanion;
		Members[FirstCompanionSlot].MemberId = InstanceId;
	}
	if (!FGameXXKPartyFormationRules::Validate(Candidate, Candidate.CardRun.OrderedFormation, &Error))
	{
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		return false;
	}
	FGameXXKTrainingTravelRuntime CandidateRunner = TrainingTravelRuntime;
	if (Candidate.Training.bTravelActive)
	{
		TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
		if (!BuildTrainingTravelParty(Candidate, Party)
			|| !FGameXXKTrainingRules::InitializeTravelRunner(Candidate.Training, CandidateRunner, Party))
		{
			return false;
		}
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

bool UGameXXKMVPSubsystem::ClearActivePermanentCompanion()
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState)
		|| RuntimeState.CardRun.OrderedFormation.Members.ContainsByPredicate([](const FGameXXKPartyMemberRef& Ref)
		{
			return Ref.Kind == EGameXXKPartyMemberKind::PermanentCompanion;
		}))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!EnsureCompanionCardRun(Candidate)
		|| !FGameXXKCompanionRules::SetActivePermanentCompanion(Candidate.CardRun.CompanionRoster, NAME_None, &Error))
	{
		return false;
	}
	Candidate.CardRun.PartySelection.ActivePermanentCompanionInstanceId = NAME_None;
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::SetPermanentCompanionCardLoadout(const FName InstanceId, const TArray<FName>& SelectedCardIds)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FGameXXKPermanentCompanion* Companion = FindPermanentCompanion(RuntimeState, InstanceId);
	FString Error;
	return Companion && FGameXXKCompanionRules::SetSelectedPersonalCards(*Companion, SelectedCardIds, &Error);
}

bool UGameXXKMVPSubsystem::SetHeroCardLoadout(const TArray<FName>& SelectedCardIds)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		return false;
	}
	FString Error;
	return FGameXXKCardBattleAdapter::SetHeroSelectedCards(RuntimeState, SelectedCardIds, &Error);
}

bool UGameXXKMVPSubsystem::SelectTownQuestNpcForParty(const FName QuestNpcId)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || QuestNpcId.IsNone())
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!EnsureCompanionCardRun(Candidate))
	{
		return false;
	}
	TArray<FGameXXKPartyMemberRef>& Members = Candidate.CardRun.OrderedFormation.Members;
	const int32 QuestNpcSlot = Members.IndexOfByPredicate([](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.Kind == EGameXXKPartyMemberKind::QuestNpc;
	});
	const int32 ExistingMemberSlot = Members.IndexOfByPredicate([QuestNpcId](const FGameXXKPartyMemberRef& Ref)
	{
		return Ref.MemberId == QuestNpcId;
	});
	if (QuestNpcSlot == INDEX_NONE
		|| (ExistingMemberSlot != INDEX_NONE && ExistingMemberSlot != QuestNpcSlot)
		|| !FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(Candidate, QuestNpcId, {}, &Error))
	{
		return false;
	}
	Members[QuestNpcSlot].Kind = EGameXXKPartyMemberKind::QuestNpc;
	Members[QuestNpcSlot].MemberId = QuestNpcId;
	if (!FGameXXKPartyFormationRules::Validate(Candidate, Candidate.CardRun.OrderedFormation, &Error))
	{
		return false;
	}
	FGameXXKPartyFormationRules::ProjectCompatibility(Candidate);
	if (!FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		return false;
	}
	FGameXXKTrainingTravelRuntime CandidateRunner = TrainingTravelRuntime;
	if (Candidate.Training.bTravelActive)
	{
		TArray<FGameXXKTrainingTravelPartyUnitRuntime> Party;
		if (!BuildTrainingTravelParty(Candidate, Party)
			|| !FGameXXKTrainingRules::InitializeTravelRunner(Candidate.Training, CandidateRunner, Party))
		{
			return false;
		}
	}

	// Route support selection is independent from the accepted story NPC that
	// follows the player in town. Never discard that follower's saved state here.
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	TrainingTravelRuntime = MoveTemp(CandidateRunner);
	return true;
}

bool UGameXXKMVPSubsystem::SetTemporaryQuestNpcCardLoadout(const FName QuestNpcId, const TArray<FName>& SelectedCardIds)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState)
		|| QuestNpcId.IsNone()
		|| !FGameXXKCompanionCatalog::FindQuestNpcDefinition(QuestNpcId))
	{
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	FString Error;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &Error)
		|| !FGameXXKCompanionRules::ValidateQuestNpcCardSelection(QuestNpcId, SelectedCardIds, &Error))
	{
		return false;
	}
	Candidate.CardRun.PartySelection.QuestNpcCardLoadouts.FindOrAdd(QuestNpcId).SelectedCardIds = SelectedCardIds;
	if (Candidate.CardRun.ActiveTemporaryQuestNpcId == QuestNpcId
		&& !FGameXXKCompanionRules::SetQuestNpcCardSelection(
			Candidate.CardRun.PartySelection.QuestNpc,
			QuestNpcId,
			SelectedCardIds,
			&Error))
	{
		return false;
	}
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(Candidate, &Error))
	{
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::AwardPermanentCompanionExperience(const FName InstanceId, const int32 ExperienceAmount)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (IsCompanionConfigurationLocked(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}
	FGameXXKPermanentCompanion* Companion = FindPermanentCompanion(RuntimeState, InstanceId);
	FString Error;
	return Companion && FGameXXKCompanionRules::AwardExperience(*Companion, ExperienceAmount, &Error);
}

bool UGameXXKMVPSubsystem::PromotePermanentCompanionStar(const FName InstanceId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView, &CardTooltipFixtureBackup);
	if (IsCompanionConfigurationLocked(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}
	FGameXXKPermanentCompanion* Companion = FindPermanentCompanion(RuntimeState, InstanceId);
	FString Error;
	return Companion && FGameXXKCompanionRules::PromoteCompanionStar(*Companion, RuntimeState.CardRun.CompanionRoster.SigilCount, &Error);
}

TArray<FName> UGameXXKMVPSubsystem::BuildTurnOrder(bool bBossBattle) const
{
	return UGameXXKMVPRules::BuildTurnOrder(RuntimeState, bBossBattle);
}
