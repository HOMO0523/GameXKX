#include "MVP/GameXXKMVPSubsystem.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMetaShopRules.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Crc.h"

namespace
{
	static constexpr int32 ManualSaveSlotCount = 5;
	static constexpr int32 MaximumMigrationBackupAttempts = 999;
	static const FString ManualSaveSlotPrefix(TEXT("GameXXK_MVP_SaveSlot_"));
	static const FString DefaultSaveSlotName(TEXT("GameXXK_MVP_SaveSlot_1"));

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
		return RuntimeState.CardRun.bLoadoutLockedForRoute || RuntimeState.CardRun.bHasActiveCardBattle;
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

	/** Every facade mutation invalidates the copied development-only HUD view before it can become stale. */
	static void BeginRuntimeStateMutation(TOptional<FGameXXKRuntimeState>& InOutBattleHudFixtureView)
	{
		InOutBattleHudFixtureView.Reset();
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
}

const FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetRuntimeState() const
{
	return BattleHudFixtureView.IsSet() ? BattleHudFixtureView.GetValue() : RuntimeState;
}

FGameXXKRuntimeState& UGameXXKMVPSubsystem::GetMutableRuntimeState()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return RuntimeState;
}

FGameXXKRuntimeState UGameXXKMVPSubsystem::GetRuntimeStateCopy() const
{
	return GetRuntimeState();
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
}

bool UGameXXKMVPSubsystem::IsBattleHudFixtureActiveForTest() const
{
	return BattleHudFixtureView.IsSet();
}

bool UGameXXKMVPSubsystem::ApplyTargetOutcomeFixtureForTest(const FName ScenarioId, FString& OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
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

bool UGameXXKMVPSubsystem::StartGame()
{
	return StartNewGame();
}

bool UGameXXKMVPSubsystem::StartNewGame()
{
	LastSaveLoadError = FText::GetEmpty();
	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = UGameXXKMVPRules::CreateNewGame();
	FString Error;
	if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(RuntimeState, &Error))
	{
		return false;
	}

	FGameXXKCompanionRosterState& StarterRoster = RuntimeState.CardRun.CompanionRoster;
	StarterRoster.RecruitSequenceSeed = MakeStarterRecruitSequenceSeed();
	FGameXXKCompanionRecruitResult StarterRecruit;
	FGameXXKCompanionRecruitResult SecondStarterRecruit;
	// Two deterministic starters let the player switch partners from the first
	// town visit; the sequence yields two different roles before duplicates.
	if (!FGameXXKCompanionRules::CreateAndResolveNextRecruitment(StarterRoster, StarterRecruit, &Error)
		|| StarterRecruit.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
		|| !FGameXXKCompanionRules::SetActivePermanentCompanion(StarterRoster, StarterRecruit.Companion.InstanceId, &Error)
		|| !FGameXXKCompanionRules::CreateAndResolveNextRecruitment(StarterRoster, SecondStarterRecruit, &Error)
		|| SecondStarterRecruit.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
		|| !FGameXXKCardBattleAdapter::EnsureCardRunInitialized(RuntimeState, &Error))
	{
		return false;
	}

	// The player lands directly in the Qingshan town map; the world map stays
	// one click away from the town HUD instead of blocking the first arrival.
	return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, UGameXXKMVPRules::RegionQingshan());
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
	if (APawn* PlayerPawn = GetLivePlayerPawnForSave(this))
	{
		RecordPlayerLocation(PlayerPawn->GetActorLocation());
	}

	UGameXXKSaveGame* SaveGame = Cast<UGameXXKSaveGame>(UGameplayStatics::CreateSaveGameObject(UGameXXKSaveGame::StaticClass()));
	if (!SaveGame)
	{
		return false;
	}

	SaveGame->SaveState = UGameXXKMVPRules::MakeSaveState(RuntimeState);
	return WriteSaveGameToSlot(SaveGame, ResolveSaveSlotName(SlotName), UserIndex);
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
		BeginRuntimeStateMutation(BattleHudFixtureView);
		RuntimeState = MoveTemp(MigratedSaveState.RuntimeState);
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

	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = MoveTemp(MigratedSaveState.RuntimeState);
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

	BeginRuntimeStateMutation(BattleHudFixtureView);
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

	BeginRuntimeStateMutation(BattleHudFixtureView);
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

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::EnhanceInstance(Candidate, InstanceId, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = MoveTemp(Candidate);
	return true;
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

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::BeginReforge(Candidate, InstanceId, AffixIndex, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::ResolveEquipmentReforge(const bool bAccept, FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		SetEquipmentTransactionFailure(OutResult, EGameXXKEquipmentTransactionError::RouteLocked);
		return false;
	}

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Candidate, bAccept, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = MoveTemp(Candidate);
	return true;
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

	FGameXXKRuntimeState Candidate = RuntimeState;
	if (!FGameXXKEquipmentEconomyRules::DismantleBatch(Candidate, InstanceIds, bConfirmedProtected, OutResult))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::OpenWorldMap(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectWorldRegion(FName RegionId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::EnterWorldRegion(RuntimeState, RegionId);
}

bool UGameXXKMVPSubsystem::EnsureQingshanTownRuntimeForDirectMap()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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

bool UGameXXKMVPSubsystem::IsRegionUnlocked(FName RegionId) const
{
	return RuntimeState.UnlockedRegions.Contains(RegionId);
}

bool UGameXXKMVPSubsystem::AcceptQuest()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::AcceptTownQuest(RuntimeState);
}

void UGameXXKMVPSubsystem::RecordQuestNpcLocation(FVector Location)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState.bHasQuestNpcLocation = true;
	RuntimeState.QuestNpcLocation = Location;
}

void UGameXXKMVPSubsystem::RecordPlayerLocation(FVector Location)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState.bHasPlayerLocation = true;
	RuntimeState.PlayerLocation = Location;
}

bool UGameXXKMVPSubsystem::CanEnterDungeon() const
{
	return UGameXXKMVPRules::CanEnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::OpenDungeonFromTownExit()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::EnterDungeon(RuntimeState);
}

bool UGameXXKMVPSubsystem::SelectDungeonNode(EGameXXKNodeKind ExpectedNode)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::AdvanceDungeonNode(RuntimeState, ExpectedNode);
}

bool UGameXXKMVPSubsystem::SelectRouteNodeById(int32 NodeId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::SelectRouteNodeById(RuntimeState, NodeId);
}

bool UGameXXKMVPSubsystem::ResolveBattleVictory(bool bBossBattle)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolveBattleVictory(RuntimeState, bBossBattle);
}

bool UGameXXKMVPSubsystem::ResolvePendingRouteRewardChoiceAndFinish(
	const FName RewardCardId,
	const FName ReplacementEntryId,
	FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolvePendingRouteRewardChoiceAndFinish(
		RuntimeState,
		RewardCardId,
		ReplacementEntryId,
		OutError);
}

bool UGameXXKMVPSubsystem::ResolvePendingBattleRewardChoiceAndFinish(
	const int32 OptionIndex,
	const FName ReplacementEntryId,
	FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
		RuntimeState,
		OptionIndex,
		ReplacementEntryId,
		OutError);
}

bool UGameXXKMVPSubsystem::SkipPendingRouteRewardAndFinish(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::ExecuteBattleBasicAttack(int32 PartyIndex, int32 EnemyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ExecuteBattleBasicAttack(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleCraneWingSlash(int32 PartyIndex, int32 EnemyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ExecuteBattleCraneWingSlash(RuntimeState, PartyIndex, EnemyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleGuiyuanArt(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ExecuteBattleGuiyuanArt(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleDefend(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ExecuteBattleDefend(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ExecuteBattleHealingPowder(int32 PartyIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ExecuteBattleHealingPowder(RuntimeState, PartyIndex);
}

bool UGameXXKMVPSubsystem::ResolveEventReward(bool bTakeGold)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolveEventReward(RuntimeState, bTakeGold);
}

bool UGameXXKMVPSubsystem::ResolveRouteEncounterChoice(const int32 ChoiceIndex)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolveRouteEncounterChoice(RuntimeState, ChoiceIndex);
}

bool UGameXXKMVPSubsystem::AcceptRouteEventNpcSupport()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::AcceptRouteEventNpcSupport(RuntimeState);
}

bool UGameXXKMVPSubsystem::ResolveCampReward(bool bHealNow)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolveCampReward(RuntimeState, bHealNow);
}

bool UGameXXKMVPSubsystem::EnsureRouteMerchantStock(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::EnsureRouteMerchantStock(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::GetRouteMerchantView(
	FGameXXKRouteMerchantView& OutView,
	FString* OutError) const
{
	return UGameXXKMVPRules::GetRouteMerchantView(RuntimeState, OutView, OutError);
}

bool UGameXXKMVPSubsystem::RefreshRouteMerchant(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::PurchaseRouteMerchant(
		RuntimeState,
		OfferId,
		ReplacementEntryId,
		OutResult);
}

bool UGameXXKMVPSubsystem::CancelPendingRouteMerchantPurchase(FString* OutError)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::CancelPendingRouteMerchantPurchase(RuntimeState, OutError);
}

bool UGameXXKMVPSubsystem::ResolveMerchantRouteNode()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::ResolveMerchantRouteNode(RuntimeState);
}

bool UGameXXKMVPSubsystem::FailDungeonToTown()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::FailDungeonToTown(RuntimeState);
}

bool UGameXXKMVPSubsystem::BuyItem(FName ItemId, int32 Quantity)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::BuyItem(RuntimeState, ItemId, Quantity);
}

bool UGameXXKMVPSubsystem::SellItem(FName ItemId, int32 Quantity)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::EnhanceItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::CanDecomposeItem(FName ItemId) const
{
	return UGameXXKMVPRules::CanDecomposeItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::DecomposeItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::DecomposeItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::UseHealingItem()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::UseHealingItem(RuntimeState);
}

bool UGameXXKMVPSubsystem::UseItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::UseItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::EquipItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::EquipItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::UnequipItem(FName ItemId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::UnequipItem(RuntimeState, ItemId);
}

bool UGameXXKMVPSubsystem::OpenTownPanel(EGameXXKTownPanelMode PanelMode)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return UGameXXKMVPRules::OpenTownPanel(RuntimeState, PanelMode);
}

bool UGameXXKMVPSubsystem::CloseTownPanel()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
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

bool UGameXXKMVPSubsystem::PrepareCompanionRosterForTown()
{
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		// Hidden town widgets are refreshed alongside the battle board.  A rejected
		// town-only request must be observational, otherwise it discards the
		// development-only battle HUD overlay before the scene presenter reads it.
		return false;
	}
	BeginRuntimeStateMutation(BattleHudFixtureView);
	return EnsureCompanionCardRun(RuntimeState);
}

bool UGameXXKMVPSubsystem::RecruitPermanentCompanionFromSeed(const int32 RecruitOrderSeed, FGameXXKCompanionRecruitResult& OutResult)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
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

	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	// The player must always keep at least one permanent companion.
	if (Candidate.CardRun.CompanionRoster.PermanentCompanions.Num() <= 1)
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
		|| !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)
		|| !FGameXXKSaveMigration::ValidateRuntimeState(Candidate, Error))
	{
		return false;
	}

	BeginRuntimeStateMutation(BattleHudFixtureView);
	RuntimeState = MoveTemp(Candidate);
	return true;
}

bool UGameXXKMVPSubsystem::DiscardPendingPermanentCompanionRecruitment()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FString Error;
	return FGameXXKCompanionRules::SetActivePermanentCompanion(RuntimeState.CardRun.CompanionRoster, InstanceId, &Error)
		&& EnsureCompanionCardRun(RuntimeState);
}

bool UGameXXKMVPSubsystem::ClearActivePermanentCompanion()
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || !EnsureCompanionCardRun(RuntimeState))
	{
		return false;
	}

	FString Error;
	return FGameXXKCompanionRules::SetActivePermanentCompanion(RuntimeState.CardRun.CompanionRoster, NAME_None, &Error)
		&& EnsureCompanionCardRun(RuntimeState);
}

bool UGameXXKMVPSubsystem::SetPermanentCompanionCardLoadout(const FName InstanceId, const TArray<FName>& SelectedCardIds)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState))
	{
		return false;
	}
	FString Error;
	return FGameXXKCardBattleAdapter::SetHeroSelectedCards(RuntimeState, SelectedCardIds, &Error);
}

bool UGameXXKMVPSubsystem::SelectTownQuestNpcForParty(const FName QuestNpcId)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
	if (!IsTownCompanionConfigurationAvailable(RuntimeState) || QuestNpcId.IsNone())
	{
		return false;
	}

	FString Error;
	if (!FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(RuntimeState, QuestNpcId, {}, &Error))
	{
		return false;
	}

	// Route support selection is independent from the accepted story NPC that
	// follows the player in town. Never discard that follower's saved state here.
	return true;
}

bool UGameXXKMVPSubsystem::SetTemporaryQuestNpcCardLoadout(const FName QuestNpcId, const TArray<FName>& SelectedCardIds)
{
	// This public Blueprint-facing entry point remains for source/save compatibility only.
	// Route/event resolution owns temporary task NPC selection and assigns the canonical three
	// fixed cards internally, so a player-facing caller must never be able to persist an edit.
	(void)QuestNpcId;
	(void)SelectedCardIds;
	return false;
}

bool UGameXXKMVPSubsystem::AwardPermanentCompanionExperience(const FName InstanceId, const int32 ExperienceAmount)
{
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
	BeginRuntimeStateMutation(BattleHudFixtureView);
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
