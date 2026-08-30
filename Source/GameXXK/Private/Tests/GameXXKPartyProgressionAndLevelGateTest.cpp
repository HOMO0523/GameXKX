#include "Misc/AutomationTest.h"

#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Serialization/MemoryWriter.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int64 TotalUnifiedExperience(const int32 Level, const int32 Experience)
	{
		const int64 CompletedLevels = FMath::Max(0, Level - 1);
		return CompletedLevels * (CompletedLevels + 1) * 50
			+ FMath::Max(0, Experience);
	}

	FName ResolveDeployedNpcId(const FGameXXKRuntimeState& State)
	{
		FName NpcId;
		FGameXXKPartyFormationRules::ResolveQuestNpcId(State, NpcId);
		return NpcId;
	}

	TArray<uint8> SerializeEquipmentCollection(const FGameXXKEquipmentCollectionState& Collection)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FGameXXKEquipmentCollectionState Copy = Collection;
		FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		return Bytes;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcProgressionStorageSchemaTest,
	"GameXXK.Training.PartyProgression.NpcStorageSchema",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcProgressionStorageSchemaTest::RunTest(const FString& Parameters)
{
	const FMapProperty* ProgressionMap = FindFProperty<FMapProperty>(
		FGameXXKCompanionPartySelection::StaticStruct(),
		TEXT("QuestNpcProgressions"));
	TestNotNull(TEXT("party save state owns a per-NPC progression map"), ProgressionMap);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcProgressionNormalizationAndStatsTest,
	"GameXXK.Training.PartyProgression.NpcNormalizationAndStats",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcProgressionNormalizationAndStatsTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("NPC progression fixture starts a new game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}

	FGameXXKCompanionPartySelection& Party =
		Subsystem->GetMutableRuntimeState().CardRun.PartySelection;
	TestEqual(TEXT("new game owns one progression entry per approved NPC"),
		Party.QuestNpcProgressions.Num(),
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions().Num());
	for (const FGameXXKQuestNpcDefinition& Definition : FGameXXKCompanionCatalog::GetQuestNpcDefinitions())
	{
		const FGameXXKQuestNpcProgression* Progression = Party.QuestNpcProgressions.Find(Definition.NpcId);
		TestNotNull(*FString::Printf(TEXT("%s owns persistent progression"), *Definition.NpcId.ToString()), Progression);
		if (Progression)
		{
			TestEqual(TEXT("new NPC progression starts at level one"), Progression->Level, 1);
			TestEqual(TEXT("new NPC progression starts at zero XP"), Progression->Experience, 0);
		}
	}

	const FName TusiId(TEXT("Npc.TusiChief"));
	FGameXXKQuestNpcProgression& TusiProgression = Party.QuestNpcProgressions.FindOrAdd(TusiId);
	TusiProgression.Level = 7;
	TusiProgression.Experience = 321;
	Subsystem->GetMutableRuntimeState().PlayerLevel = 1;
	FGameXXKEquipmentLoadoutSnapshot Snapshot;
	TestTrue(TEXT("NPC equipment snapshot resolves from independent progression"),
		Subsystem->GetEquipmentLoadoutSnapshot(TusiId, Snapshot));
	FGameXXKCompanionAttributes ExpectedAttributes;
	TestTrue(TEXT("level-seven NPC reference attributes resolve"),
		FGameXXKCompanionRules::GetQuestNpcAttributes(TusiId, 7, ExpectedAttributes, nullptr));
	TestEqual(TEXT("NPC snapshot health uses NPC level instead of hero level"),
		Snapshot.BareStats.MaxHealth, ExpectedAttributes.Health);
	TestEqual(TEXT("NPC snapshot attack uses NPC level instead of hero level"),
		Snapshot.BareStats.Attack, ExpectedAttributes.Attack);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("NPC progression survives the current save round-trip"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, Report));
	const FGameXXKQuestNpcProgression* RestoredTusi =
		Restored.CardRun.PartySelection.QuestNpcProgressions.Find(TusiId);
	TestNotNull(TEXT("restored save retains Tusi progression"), RestoredTusi);
	if (RestoredTusi)
	{
		TestEqual(TEXT("restored Tusi level is independent"), RestoredTusi->Level, 7);
		TestEqual(TEXT("restored Tusi XP is independent"), RestoredTusi->Experience, 321);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcProgressionCurrentSaveBackfillTest,
	"GameXXK.Training.PartyProgression.NpcCurrentSaveBackfill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcProgressionCurrentSaveBackfillTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("current-save backfill fixture starts a valid game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	SaveState.RuntimeState.CardRun.PartySelection.QuestNpcProgressions.Reset();
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("a current-version save without the newly added NPC map restores"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, Report));
	TestEqual(TEXT("current-version save restore backfills all NPC progression entries"),
		Restored.CardRun.PartySelection.QuestNpcProgressions.Num(),
		FGameXXKCompanionCatalog::GetQuestNpcDefinitions().Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingDeployedPartyExperienceTest,
	"GameXXK.Training.PartyProgression.DeployedTrioExperience",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingDeployedPartyExperienceTest::RunTest(const FString& Parameters)
{
	auto VerifyAward = [this](UGameXXKMVPSubsystem* Subsystem, const int32 ExpectedExperience) -> bool
	{
		const FGameXXKRuntimeState& After = Subsystem->GetRuntimeState();
		const FName ActiveCompanionId = After.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		const FGameXXKPermanentCompanion* ActiveCompanion =
			After.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
				[ActiveCompanionId](const FGameXXKPermanentCompanion& Companion)
				{
					return Companion.InstanceId == ActiveCompanionId;
				});
		const FName ActiveNpcId = ResolveDeployedNpcId(After);
		const FGameXXKQuestNpcProgression* ActiveNpc =
			After.CardRun.PartySelection.QuestNpcProgressions.Find(ActiveNpcId);
		TestNotNull(TEXT("deployed companion remains resolvable"), ActiveCompanion);
		TestNotNull(TEXT("deployed NPC remains resolvable"), ActiveNpc);
		if (!ActiveCompanion || !ActiveNpc)
		{
			return false;
		}
		TestEqual(TEXT("hero receives the complete Training experience"),
			TotalUnifiedExperience(After.PlayerLevel, After.PlayerXP),
			static_cast<int64>(ExpectedExperience));
		TestEqual(TEXT("deployed companion receives the complete Training experience"),
			TotalUnifiedExperience(ActiveCompanion->Level, ActiveCompanion->Experience),
			static_cast<int64>(ExpectedExperience));
		TestEqual(TEXT("deployed NPC receives the complete Training experience"),
			TotalUnifiedExperience(ActiveNpc->Level, ActiveNpc->Experience),
			static_cast<int64>(ExpectedExperience));
		for (const FGameXXKPermanentCompanion& Companion : After.CardRun.CompanionRoster.PermanentCompanions)
		{
			if (Companion.InstanceId != ActiveCompanionId)
			{
				TestEqual(TEXT("inactive companion receives no Training experience"),
					TotalUnifiedExperience(Companion.Level, Companion.Experience), int64(0));
			}
		}
		for (const TPair<FName, FGameXXKQuestNpcProgression>& Pair : After.CardRun.PartySelection.QuestNpcProgressions)
		{
			if (Pair.Key != ActiveNpcId)
			{
				TestEqual(TEXT("inactive NPC receives no Training experience"),
					TotalUnifiedExperience(Pair.Value.Level, Pair.Value.Experience), int64(0));
			}
		}
		return true;
	};

	UGameXXKMVPSubsystem* Online = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("online trio XP fixture starts"), Online && Online->StartGame()))
	{
		return false;
	}
	TestTrue(TEXT("online XP fixture selects Yue Bai"),
		Online->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	bool bStageCompleted = false;
	FGameXXKTrainingReward OnlineReward;
	TestTrue(TEXT("one online Training encounter settles"),
		Online->AdvanceTrainingTravelEncounter(bStageCompleted, OnlineReward));
	TestTrue(TEXT("online Training encounter grants positive XP"), OnlineReward.Experience > 0);
	VerifyAward(Online, OnlineReward.Experience);

	UGameXXKMVPSubsystem* Offline = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("offline trio XP fixture starts"), Offline && Offline->StartGame()))
	{
		return false;
	}
	TestTrue(TEXT("offline XP fixture selects Yue Bai"),
		Offline->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	constexpr int32 PendingOfflineExperience = 150;
	Offline->GetMutableRuntimeState().Training.PendingTravelExperience =
		PendingOfflineExperience;
	Offline->GetMutableRuntimeState().Training.PendingTravelSimulatedSeconds = 1;
	FGameXXKTrainingOfflineReward Collected;
	TestTrue(TEXT("offline Training reward can be collected"),
		Offline->CollectTrainingTravelRewards(Collected));
	TestEqual(TEXT("collected offline XP matches the pending ledger"),
		Collected.Experience, PendingOfflineExperience);
	VerifyAward(Offline, PendingOfflineExperience);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingTravelPartyLevelUpSynchronizationTest,
	"GameXXK.Training.PartyProgression.TravelRuntimeLevelUpSync",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingTravelPartyLevelUpSynchronizationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("Travel level-up sync fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.PlayerXP = 99;
	const FName CompanionId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	FGameXXKPermanentCompanion* Companion = State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[CompanionId](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == CompanionId;
		});
	const FName NpcId = ResolveDeployedNpcId(State);
	FGameXXKQuestNpcProgression* Npc = State.CardRun.PartySelection.QuestNpcProgressions.Find(NpcId);
	if (!TestNotNull(TEXT("Travel level-up sync fixture has a companion"), Companion)
		|| !TestNotNull(TEXT("Travel level-up sync fixture has an NPC"), Npc))
	{
		return false;
	}
	Companion->Experience = 99;
	Npc->Experience = 99;

	bool bEncounterCompleted = false;
	bool bStageCompleted = false;
	bool bDefeated = false;
	FGameXXKTrainingReward Reward;
	for (int32 Guard = 0; Guard < 512 && !bEncounterCompleted; ++Guard)
	{
		if (!Subsystem->AdvanceTrainingTravelStep(
			bEncounterCompleted,
			bStageCompleted,
			bDefeated,
			Reward,
			1))
		{
			AddError(TEXT("Travel level-up sync fixture could not advance"));
			return false;
		}
	}
	TestTrue(TEXT("Travel level-up sync fixture completes an encounter"), bEncounterCompleted);
	const FGameXXKRuntimeState& After = Subsystem->GetRuntimeState();
	const FGameXXKPermanentCompanion* AfterCompanion = After.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
		[CompanionId](const FGameXXKPermanentCompanion& Candidate)
		{
			return Candidate.InstanceId == CompanionId;
		});
	const FGameXXKQuestNpcProgression* AfterNpc = After.CardRun.PartySelection.QuestNpcProgressions.Find(NpcId);
	TestEqual(TEXT("hero levels at the encounter reward boundary"), After.PlayerLevel, 2);
	TestEqual(TEXT("companion levels at the encounter reward boundary"), AfterCompanion ? AfterCompanion->Level : 0, 2);
	TestEqual(TEXT("NPC levels at the encounter reward boundary"), AfterNpc ? AfterNpc->Level : 0, 2);

	const FGameXXKTrainingTravelRuntime Runtime = Subsystem->GetTrainingTravelRuntimeCopy();
	for (const FGameXXKTrainingTravelPartyUnitRuntime& Unit : Runtime.PartyUnits)
	{
		FGameXXKEquipmentLoadoutSnapshot Expected;
		if (Subsystem->GetEquipmentLoadoutSnapshot(Unit.UnitId, Expected))
		{
			TestEqual(TEXT("level-up sync refreshes party max health"), Unit.MaxHP, Expected.AttributesBeforeRoute.MaxHealth);
			TestEqual(TEXT("level-up sync refreshes party attack"), Unit.Attack, Expected.AttributesBeforeRoute.Attack);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionFullCardPoolUnlockFrontierTest,
	"GameXXK.Training.PartyProgression.CompanionFullCardPool",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionFullCardPoolUnlockFrontierTest::RunTest(const FString& Parameters)
{
	FGameXXKCompanionRosterState Roster;
	FGameXXKCompanionRecruitResult Result;
	if (!TestTrue(TEXT("full-pool fixture recruits a Blade companion"),
		FGameXXKCompanionRules::RecruitPermanentCompanion(
			Roster,
			TEXT("Companion.Blade.01"),
			7331,
			Result,
			nullptr))
		|| !TestEqual(TEXT("full-pool fixture owns one companion"), Roster.PermanentCompanions.Num(), 1))
	{
		return false;
	}
	FGameXXKPermanentCompanion& Companion = Roster.PermanentCompanions[0];
	TArray<FName> BirthCards;
	TestTrue(TEXT("the original deterministic six-card birth prefix resolves"),
		FGameXXKCompanionRules::BuildPersonalCardPool(
			Companion.Role,
			Companion.CardSeed,
			BirthCards,
			nullptr));
	TestEqual(TEXT("companion persists the full eighteen-card profession pool"),
		Companion.PersonalCardIds.Num(), 18);
	TestEqual(TEXT("level-one companion starts with six unlocked cards"),
		Companion.UnlockedPersonalCardIds.Num(), 6);
	for (int32 Index = 0; Index < BirthCards.Num(); ++Index)
	{
		TestEqual(TEXT("the migrated full pool preserves every birth-card prefix position"),
			Companion.PersonalCardIds.IsValidIndex(Index) ? Companion.PersonalCardIds[Index] : NAME_None,
			BirthCards[Index]);
	}
	const TArray<FName> SelectedBefore = Companion.SelectedCardIds;
	const TPair<int32, int32> UnlockCases[] = {{4, 6}, {5, 10}, {10, 14}, {15, 18}};
	for (const TPair<int32, int32>& UnlockCase : UnlockCases)
	{
		Companion.Level = UnlockCase.Key;
		Companion.Experience = 0;
		TestTrue(TEXT("level frontier refresh succeeds"),
			FGameXXKCompanionRules::RefreshUnlockedPersonalCards(Companion, nullptr));
		TestEqual(TEXT("level frontier unlock count is deterministic"),
			Companion.UnlockedPersonalCardIds.Num(), UnlockCase.Value);
		TestEqual(TEXT("unlocking cards never rewrites the selected five"),
			Companion.SelectedCardIds, SelectedBefore);
	}
	TSet<FName> UniqueCards(Companion.PersonalCardIds);
	TestEqual(TEXT("the full profession pool contains no duplicate cards"), UniqueCards.Num(), 18);
	for (const FName CardId : Companion.PersonalCardIds)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		TestTrue(TEXT("every full-pool card belongs to the companion profession"),
			Definition
				&& Definition->Owner == EGameXXKCardOwner::Profession
				&& Definition->Role == Companion.Role);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionCurrentSaveFullPoolBackfillTest,
	"GameXXK.Training.PartyProgression.CompanionCurrentSaveBackfill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCurrentSaveFullPoolBackfillTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("companion backfill fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKPermanentCompanion& SourceCompanion =
		Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions[0];
	const FName CompanionId = SourceCompanion.InstanceId;
	const TArray<FName> SelectedBefore = SourceCompanion.SelectedCardIds;
	SourceCompanion.PersonalCardIds.SetNum(6, EAllowShrinking::No);
	SourceCompanion.UnlockedPersonalCardIds = SourceCompanion.PersonalCardIds;
	const TArray<FName> BirthPrefix = SourceCompanion.PersonalCardIds;

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("current save with legacy six-card companion restores"),
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, Report));
	const FGameXXKPermanentCompanion* RestoredCompanion =
		Restored.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[CompanionId](const FGameXXKPermanentCompanion& Companion)
			{
				return Companion.InstanceId == CompanionId;
			});
	TestNotNull(TEXT("restored save retains the same companion"), RestoredCompanion);
	if (RestoredCompanion)
	{
		TestEqual(TEXT("current-save backfill expands the pool to eighteen"),
			RestoredCompanion->PersonalCardIds.Num(), 18);
		TestEqual(TEXT("current-save backfill preserves selected five"),
			RestoredCompanion->SelectedCardIds, SelectedBefore);
		for (int32 Index = 0; Index < BirthPrefix.Num(); ++Index)
		{
			TestEqual(TEXT("current-save backfill preserves birth prefix order"),
				RestoredCompanion->PersonalCardIds.IsValidIndex(Index)
					? RestoredCompanion->PersonalCardIds[Index]
					: NAME_None,
				BirthPrefix[Index]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionLockedCardInventoryPresentationTest,
	"GameXXK.Training.PartyProgression.CompanionLockedCardPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionLockedCardInventoryPresentationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("locked-card UI fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FName CompanionId =
		Subsystem->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(CompanionId);
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("companion inventory opens"), Inventory->OpenFreeInventoryForTest())
		|| !TestTrue(TEXT("companion deck tab opens"),
			Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck)))
	{
		return false;
	}
	TestEqual(TEXT("companion deck renders all eighteen profession cards"),
		Inventory->GetHeroCardBackpackIdsForTest().Num(), 18);
	UButton* SeventhButton = Inventory->WidgetTree
		? Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckCard_06")))
		: nullptr;
	UImage* SeventhLock = Inventory->WidgetTree
		? Cast<UImage>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckLockedIcon_06")))
		: nullptr;
	UTextBlock* SeventhUnlockText = Inventory->WidgetTree
		? Cast<UTextBlock>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckUnlockText_06")))
		: nullptr;
	TestFalse(TEXT("seventh card cannot be selected below level five"),
		SeventhButton && SeventhButton->GetIsEnabled());
	TestEqual(TEXT("seventh card keeps the existing lock overlay"),
		SeventhLock ? SeventhLock->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("seventh card labels its level-five requirement"),
		SeventhUnlockText
			&& SeventhUnlockText->GetVisibility() == ESlateVisibility::HitTestInvisible
			&& SeventhUnlockText->GetText().ToString().Contains(TEXT("5级解锁")));

	FGameXXKPermanentCompanion* Companion =
		Subsystem->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[CompanionId](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId == CompanionId;
			});
	if (!TestNotNull(TEXT("locked-card UI fixture resolves mutable companion"), Companion))
	{
		return false;
	}
	Companion->Level = 5;
	Companion->Experience = 0;
	TestTrue(TEXT("level-five card frontier refreshes"),
		FGameXXKCompanionRules::RefreshUnlockedPersonalCards(*Companion, nullptr));
	Inventory->RefreshVisibleRuntimeValues();
	TestTrue(TEXT("seventh card becomes selectable at level five"), SeventhButton->GetIsEnabled());
	TestEqual(TEXT("unlocked seventh card removes the lock overlay"),
		SeventhLock->GetVisibility(), ESlateVisibility::Collapsed);
	TestEqual(TEXT("unlocked seventh card removes its level text"),
		SeventhUnlockText ? SeventhUnlockText->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::Collapsed);
	TestTrue(TEXT("unlocked seventh card clears stale unlock text"),
		SeventhUnlockText && SeventhUnlockText->GetText().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentCharacterLevelGateTest,
	"GameXXK.Training.PartyProgression.EquipmentCharacterLevelGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentCharacterLevelGateTest::RunTest(const FString& Parameters)
{
	for (int32 TargetKind = 0; TargetKind < 3; ++TargetKind)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!TestTrue(TEXT("equipment level-gate fixture starts"), Subsystem && Subsystem->StartGame()))
		{
			return false;
		}
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		FName CharacterId = FGameXXKEquipmentRules::HeroCharacterId();
		if (TargetKind == 1)
		{
			CharacterId = State.CardRun.PartySelection.ActivePermanentCompanionInstanceId;
		}
		else if (TargetKind == 2)
		{
			CharacterId = ResolveDeployedNpcId(State);
		}

		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::XuanJia;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 2;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		FString Error;
		if (!TestTrue(TEXT("equipment level-gate fixture creates a level-two weapon"),
			FGameXXKEquipmentRules::CreateRolledInstance(
				State.EquipmentCollection,
				Request,
				InstanceId,
				&Error)))
		{
			AddError(Error);
			return false;
		}
		const TArray<uint8> Before = SerializeEquipmentCollection(State.EquipmentCollection);
		FGameXXKEquipmentTransactionResult Result;
		TestFalse(TEXT("level-two equipment is rejected for a level-one character"),
			Subsystem->EquipEquipmentInstance(
				CharacterId,
				EGameXXKEquipmentSlot::Weapon,
				InstanceId,
				Result));
		TestTrue(TEXT("level-gate rejection reports the required character level"),
			Result.Message.ToString().Contains(TEXT("需要角色达到 2 级")));
		TestEqual(TEXT("level-gate rejection leaves equipment state byte-identical"),
			SerializeEquipmentCollection(Subsystem->GetRuntimeState().EquipmentCollection),
			Before);

		if (!TestTrue(TEXT("level-gate fixture normalizes the physical item cell"),
			Subsystem->NormalizeDesktopInventoryState()))
		{
			return false;
		}
		EGameXXKDesktopItemContainer SourceContainer = EGameXXKDesktopItemContainer::Backpack;
		int32 SourceSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
			Subsystem->GetRuntimeState(),
			SourceContainer,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
		if (SourceSlot == INDEX_NONE)
		{
			SourceContainer = EGameXXKDesktopItemContainer::Warehouse;
			SourceSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
				Subsystem->GetRuntimeState(),
				SourceContainer,
				FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
		}
		if (!TestTrue(TEXT("level-gate fixture resolves the physical source cell"), SourceSlot != INDEX_NONE))
		{
			return false;
		}
		const TArray<uint8> BeforePhysicalAttempt =
			SerializeEquipmentCollection(Subsystem->GetRuntimeState().EquipmentCollection);
		TestFalse(TEXT("physical-cell equip obeys the same character level gate"),
			Subsystem->EquipEquipmentFromDesktopCell(
				CharacterId,
				EGameXXKEquipmentSlot::Weapon,
				SourceContainer,
				SourceSlot,
				InstanceId,
				Result));
		TestTrue(TEXT("physical-cell rejection reports the required character level"),
			Result.Message.ToString().Contains(TEXT("需要角色达到 2 级")));
		TestEqual(TEXT("physical-cell level rejection leaves equipment state byte-identical"),
			SerializeEquipmentCollection(Subsystem->GetRuntimeState().EquipmentCollection),
			BeforePhysicalAttempt);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentGrandfatheredOverLevelLoadoutTest,
	"GameXXK.Training.PartyProgression.EquipmentGrandfatheredOverLevelLoadout",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentGrandfatheredOverLevelLoadoutTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("grandfathered equipment fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	State.PlayerLevel = 2;
	State.PlayerXP = 0;

	FGameXXKEquipmentCreateRequest Request;
	Request.Set = EGameXXKEquipmentSet::XuanJia;
	Request.Quality = EGameXXKEquipmentQuality::Rare;
	Request.ItemLevel = 2;
	Request.bForceSlot = true;
	Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
	FName InstanceId;
	FString Error;
	if (!TestTrue(TEXT("grandfathered equipment fixture creates a level-two weapon"),
		FGameXXKEquipmentRules::CreateRolledInstance(
			State.EquipmentCollection,
			Request,
			InstanceId,
			&Error)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKEquipmentTransactionResult Result;
	if (!TestTrue(TEXT("level-two hero equips the level-two weapon"),
		Subsystem->EquipEquipmentInstance(
			HeroId,
			EGameXXKEquipmentSlot::Weapon,
			InstanceId,
			Result)))
	{
		return false;
	}

	State.PlayerLevel = 1;
	UGameXXKMVPRules::RecalculatePlayerStatsFromEquipment(State);
	const FGameXXKEquipmentLoadout* LoweredLoadout =
		State.EquipmentCollection.CharacterLoadouts.Find(HeroId);
	TestEqual(TEXT("lowering character level does not strip an already equipped item"),
		LoweredLoadout
			? FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(
				*LoweredLoadout,
				EGameXXKEquipmentSlot::Weapon)
			: NAME_None,
		InstanceId);
	TestTrue(TEXT("grandfathered fixture reconciles the physical inventory projection"),
		Subsystem->NormalizeDesktopInventoryState());
	FString ValidationError;
	const bool bRuntimeValid =
		FGameXXKSaveMigration::ValidateRuntimeState(Subsystem->GetRuntimeState(), ValidationError);
	TestTrue(
		FString::Printf(TEXT("grandfathered runtime remains save-valid before serialization: %s"), *ValidationError),
		bRuntimeValid);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport MigrationReport;
	const bool bRestored =
		FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, MigrationReport);
	TestTrue(
		FString::Printf(TEXT("save restore accepts grandfathered over-level equipment: %s"), *MigrationReport.Error),
		bRestored);
	const FGameXXKEquipmentLoadout* RestoredLoadout =
		Restored.EquipmentCollection.CharacterLoadouts.Find(HeroId);
	TestEqual(TEXT("save restore preserves grandfathered over-level equipment"),
		RestoredLoadout
			? FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(
				*RestoredLoadout,
				EGameXXKEquipmentSlot::Weapon)
			: NAME_None,
		InstanceId);

	TestTrue(TEXT("the grandfathered item can still be removed"),
		Subsystem->UnequipEquipmentSlot(
			HeroId,
			EGameXXKEquipmentSlot::Weapon,
			Result));
	TestFalse(TEXT("the removed over-level item cannot be re-equipped"),
		Subsystem->EquipEquipmentInstance(
			HeroId,
			EGameXXKEquipmentSlot::Weapon,
			InstanceId,
			Result));
	TestTrue(TEXT("the failed re-equip reports the required level"),
		Result.Message.ToString().Contains(TEXT("需要角色达到 2 级")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcExperienceUiRefreshTest,
	"GameXXK.Training.PartyProgression.NpcExperienceUiRefresh",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcExperienceUiRefreshTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("NPC experience UI fixture starts"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FName NpcId(TEXT("Npc.TusiChief"));
	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(NpcId);
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("NPC inventory opens"), Inventory->OpenFreeInventoryForTest())
		|| !TestTrue(TEXT("NPC attributes tab opens"),
			Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes)))
	{
		return false;
	}
	UTextBlock* ExperienceText = Inventory->WidgetTree
		? Cast<UTextBlock>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceText")))
		: nullptr;
	UProgressBar* ExperienceBar = Inventory->WidgetTree
		? Cast<UProgressBar>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceBar")))
		: nullptr;
	TestTrue(TEXT("NPC experience text is visible"),
		ExperienceText && ExperienceText->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("NPC experience bar is visible"),
		ExperienceBar && ExperienceBar->GetVisibility() == ESlateVisibility::HitTestInvisible);
	TestTrue(TEXT("level-one NPC starts at zero of one hundred XP"),
		ExperienceText && ExperienceText->GetText().ToString().Contains(TEXT("0 / 100")));

	FGameXXKQuestNpcProgression* Progression =
		Subsystem->GetMutableRuntimeState().CardRun.PartySelection.QuestNpcProgressions.Find(NpcId);
	if (!TestNotNull(TEXT("NPC experience UI fixture resolves progression"), Progression))
	{
		return false;
	}
	Progression->Experience = 50;
	Inventory->RefreshVisibleRuntimeValues();
	TestTrue(TEXT("visible NPC experience text refreshes without reopening"),
		ExperienceText->GetText().ToString().Contains(TEXT("50 / 100")));
	TestTrue(TEXT("visible NPC experience bar refreshes without reopening"),
		FMath::IsNearlyEqual(ExperienceBar->GetPercent(), 0.5f));
	return true;
}

#endif
