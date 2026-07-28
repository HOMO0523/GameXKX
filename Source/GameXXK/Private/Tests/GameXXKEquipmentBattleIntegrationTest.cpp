#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	constexpr EGameXXKEquipmentSlot OrderedSlots[] = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	FGameXXKBattleRuntimeUnit MakeEnemy()
	{
		FGameXXKBattleRuntimeUnit Enemy;
		Enemy.Id = TEXT("EquipmentIntegration.Enemy");
		Enemy.DisplayName = FText::FromString(TEXT("测试敌人"));
		Enemy.HP = 240;
		Enemy.MaxHP = 240;
		Enemy.Attack = 18;
		Enemy.Defense = 8;
		Enemy.Speed = 9;
		Enemy.bEnemy = true;
		return Enemy;
	}

	bool AddAndEquipFullSet(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName CharacterId,
		const EGameXXKEquipmentSet Set)
	{
		for (const EGameXXKEquipmentSlot Slot : OrderedSlots)
		{
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = Set;
			Request.Quality = EGameXXKEquipmentQuality::Rare;
			Request.ItemLevel = 6;
			Request.bForceSlot = true;
			Request.ForcedSlot = Slot;
			FName InstanceId;
			FString Error;
			if (!Test.TestTrue(
				FString::Printf(TEXT("creates %s slot %d"), *CharacterId.ToString(), static_cast<int32>(Slot)),
				FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
			{
				Test.AddError(Error);
				return false;
			}

			FGameXXKEquipmentTransactionResult EquipResult;
			if (!Test.TestTrue(
				FString::Printf(TEXT("equips %s slot %d through the central state transaction"), *CharacterId.ToString(), static_cast<int32>(Slot)),
				FGameXXKEquipmentEconomyRules::Equip(State, CharacterId, Slot, InstanceId, EquipResult)))
			{
				Test.AddError(EquipResult.Message.ToString());
				return false;
			}
		}
		return true;
	}

	const FGameXXKCardCombatUnit* FindUnit(const FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Candidate)
		{
			return Candidate.UnitId == UnitId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentBattleIntegrationTest,
	"GameXXK.Equipment.BattleIntegration.ProjectionAndDescriptors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentBattleIntegrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	FString Error;
	if (!TestTrue(TEXT("new state initializes the shared card-run data"), FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)))
	{
		AddError(Error);
		return false;
	}

	State.CardRun.CompanionRoster.RecruitSequenceSeed = 713;
	FGameXXKCompanionRecruitResult RecruitResult;
	if (!TestTrue(TEXT("fixture recruits one permanent companion"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(State.CardRun.CompanionRoster, RecruitResult, &Error))
		|| !TestTrue(TEXT("fixture has an immediately recruited companion"), RecruitResult.Outcome == EGameXXKCompanionRecruitOutcome::Recruited)
		|| !TestTrue(TEXT("fixture selects the permanent companion"),
			FGameXXKCompanionRules::SetActivePermanentCompanion(State.CardRun.CompanionRoster, RecruitResult.Companion.InstanceId, &Error))
		|| !TestTrue(TEXT("fixture configures the route-local task NPC"),
			FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, TEXT("Npc.TusiChief"), {}, &Error)))
	{
		AddError(Error);
		return false;
	}

	if (!AddAndEquipFullSet(*this, State, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSet::ShanHe)
		|| !AddAndEquipFullSet(*this, State, RecruitResult.Companion.InstanceId, EGameXXKEquipmentSet::ShanHe))
	{
		return false;
	}

	FGameXXKEquipmentLoadoutSnapshot HeroSnapshot;
	FGameXXKEquipmentLoadoutSnapshot CompanionSnapshot;
	FGameXXKCharacterStats CompanionBareStats;
	if (!TestTrue(TEXT("builds the authoritative hero loadout snapshot"),
		FGameXXKEquipmentRules::BuildLoadoutSnapshot(
			State.EquipmentCollection,
			FGameXXKEquipmentRules::HeroCharacterId(),
			FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel),
			HeroSnapshot,
			&Error))
		|| !TestTrue(TEXT("builds the authoritative permanent-companion loadout snapshot"),
			FGameXXKCharacterStatRules::GetBareCompanionStats(
				RecruitResult.Companion.Role,
				RecruitResult.Companion.Level,
				RecruitResult.Companion.Star,
				CompanionBareStats,
				&Error))
		|| !TestTrue(TEXT("builds the equipped permanent-companion projection"),
			FGameXXKEquipmentRules::BuildLoadoutSnapshot(
				State.EquipmentCollection,
				RecruitResult.Companion.InstanceId,
				CompanionBareStats,
				CompanionSnapshot,
				&Error)))
	{
		AddError(Error);
		return false;
	}

	FGameXXKCompanionAttributes TaskNpcAttributes;
	if (!TestTrue(TEXT("resolves the task NPC from its non-equipment snapshot"),
		FGameXXKCompanionRules::GetQuestNpcAttributes(TEXT("Npc.TusiChief"), State.PlayerLevel, TaskNpcAttributes, &Error)))
	{
		AddError(Error);
		return false;
	}

	State.ActiveBattleParty.Reset();
	State.ActiveBattleEnemies = {MakeEnemy()};
	State.bHasActiveBattle = true;
	// This fixture deliberately exercises the legacy/no-route-map battle save
	// shape.  A concrete source node is only valid when a generated route owns
	// that node, so keep the battle source unset rather than creating a save that
	// the migration validator must reject.
	State.ActiveBattleNodeId = INDEX_NONE;
	if (!TestTrue(TEXT("battle initialization projects both equipped permanent characters"),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 5566, &Error)))
	{
		AddError(Error);
		return false;
	}

	const FGameXXKCardCombatUnit* Hero = FindUnit(State.CardRun.ActiveBattle, FGameXXKEquipmentRules::HeroCharacterId());
	const FGameXXKCardCombatUnit* Companion = FindUnit(State.CardRun.ActiveBattle, RecruitResult.Companion.InstanceId);
	const FGameXXKCardCombatUnit* TaskNpc = FindUnit(State.CardRun.ActiveBattle, TEXT("Npc.TusiChief"));
	TestNotNull(TEXT("card battle materializes the equipped hero"), Hero);
	TestNotNull(TEXT("card battle materializes the equipped permanent companion"), Companion);
	TestNotNull(TEXT("card battle materializes the task NPC"), TaskNpc);
	if (!Hero || !Companion || !TaskNpc)
	{
		return false;
	}

	TestEqual(TEXT("hero max HP is projected from exactly one loadout snapshot"), Hero->MaxHP, HeroSnapshot.AttributesBeforeRoute.MaxHealth);
	TestEqual(TEXT("hero max MP is projected from exactly one loadout snapshot"), Hero->MaxMana, HeroSnapshot.AttributesBeforeRoute.MaxMana);
	TestEqual(TEXT("hero attack is projected from exactly one loadout snapshot"), Hero->Attack, HeroSnapshot.AttributesBeforeRoute.Attack);
	TestEqual(TEXT("hero defense is projected from exactly one loadout snapshot"), Hero->Defense, HeroSnapshot.AttributesBeforeRoute.Defense);
	TestEqual(TEXT("hero speed is present in the card runtime and projected from its loadout"), Hero->Speed, HeroSnapshot.AttributesBeforeRoute.Speed);
	TestEqual(TEXT("companion max HP is projected from its independent loadout snapshot"), Companion->MaxHP, CompanionSnapshot.AttributesBeforeRoute.MaxHealth);
	TestEqual(TEXT("companion max MP is projected from its independent loadout snapshot"), Companion->MaxMana, CompanionSnapshot.AttributesBeforeRoute.MaxMana);
	TestEqual(TEXT("companion attack is projected from its independent loadout snapshot"), Companion->Attack, CompanionSnapshot.AttributesBeforeRoute.Attack);
	TestEqual(TEXT("companion defense is projected from its independent loadout snapshot"), Companion->Defense, CompanionSnapshot.AttributesBeforeRoute.Defense);
	TestEqual(TEXT("companion speed is present in the card runtime and projected from its loadout"), Companion->Speed, CompanionSnapshot.AttributesBeforeRoute.Speed);
	TestEqual(TEXT("task NPC max HP ignores player equipment"), TaskNpc->MaxHP, TaskNpcAttributes.Health);
	TestEqual(TEXT("task NPC max MP ignores player equipment"), TaskNpc->MaxMana, TaskNpcAttributes.Mana);
	TestEqual(TEXT("task NPC attack ignores player equipment"), TaskNpc->Attack, TaskNpcAttributes.Attack);
	TestEqual(TEXT("task NPC defense ignores player equipment"), TaskNpc->Defense, TaskNpcAttributes.Defense);
	TestEqual(TEXT("task NPC speed ignores player equipment"), TaskNpc->Speed, TaskNpcAttributes.Speed);

	const TArray<FGameXXKEquipmentActiveEffect> TeamEffects = FGameXXKEquipmentRules::ResolveTeamEffects({HeroSnapshot, CompanionSnapshot});
	const int32 ExpectedEffectCount = HeroSnapshot.ActivePersonalEffects.Num() + CompanionSnapshot.ActivePersonalEffects.Num() + TeamEffects.Num();
	TestEqual(TEXT("battle stores each permanent equipment descriptor exactly once"), State.CardRun.ActiveBattle.EquipmentEffects.Num(), ExpectedEffectCount);
	TSet<FString> EffectKeys;
	for (const FGameXXKEquipmentBattleEffectRuntime& EffectRuntime : State.CardRun.ActiveBattle.EquipmentEffects)
	{
		TestTrue(TEXT("equipment effect has a stable source unit"), !EffectRuntime.SourceCharacterId.IsNone());
		TestTrue(TEXT("equipment effect never uses the task NPC as an equipment owner"), EffectRuntime.SourceCharacterId != TaskNpc->UnitId);
		TestEqual(TEXT("new effects begin with no current-round triggers"), EffectRuntime.CurrentRoundTriggerCount, 0);
		TestEqual(TEXT("new effects have not fired in a battle round"), EffectRuntime.LastTriggerRound, 0);
		EffectKeys.Add(EffectRuntime.ActiveEffect.EffectId.ToString() + TEXT("|") + EffectRuntime.SourceCharacterId.ToString());
	}
	TestEqual(TEXT("battle effects have no duplicate effect-source pair"), EffectKeys.Num(), State.CardRun.ActiveBattle.EquipmentEffects.Num());
	TestTrue(TEXT("equipment-enriched card runtime passes central validation"), GameXXKCardRules::ValidateCardBattleRuntime(State.CardRun.ActiveBattle, &Error));
	if (State.CardRun.ActiveBattle.EquipmentEffects.IsEmpty())
	{
		AddError(TEXT("fixture must materialize at least one equipment effect before validating effect IDs."));
		return false;
	}
	FGameXXKCardBattleRuntime UnknownEffectRuntime = State.CardRun.ActiveBattle;
	UnknownEffectRuntime.EquipmentEffects[0].ActiveEffect.EffectId = TEXT("Set.Unknown.99");
	TestFalse(TEXT("battle validation rejects an unknown equipment effect ID"),
		GameXXKCardRules::ValidateCardBattleRuntime(UnknownEffectRuntime, &Error));

	FGameXXKCardBattleRuntime StaleSourceRuntime = State.CardRun.ActiveBattle;
	StaleSourceRuntime.EquipmentEffects[0].SourceCharacterId = TEXT("Missing.Equipment.Owner");
	StaleSourceRuntime.EquipmentEffects[0].ActiveEffect.SourceCharacterId = TEXT("Missing.Equipment.Owner");
	TestFalse(TEXT("battle validation rejects a stale equipment-effect source"),
		GameXXKCardRules::ValidateCardBattleRuntime(StaleSourceRuntime, &Error));

	FGameXXKCardBattleRuntime NegativeCounterRuntime = State.CardRun.ActiveBattle;
	NegativeCounterRuntime.EquipmentEffects[0].CurrentRoundTriggerCount = -1;
	TestFalse(TEXT("battle validation rejects a negative equipment trigger counter"),
		GameXXKCardRules::ValidateCardBattleRuntime(NegativeCounterRuntime, &Error));

	FGameXXKCardBattleRuntime DuplicateEffectRuntime = State.CardRun.ActiveBattle;
	const FGameXXKEquipmentBattleEffectRuntime DuplicateEffect = DuplicateEffectRuntime.EquipmentEffects[0];
	DuplicateEffectRuntime.EquipmentEffects.Add(DuplicateEffect);
	TestFalse(TEXT("battle validation rejects a duplicated equipment effect source pair"),
		GameXXKCardRules::ValidateCardBattleRuntime(DuplicateEffectRuntime, &Error));

	FGameXXKCardBattleRuntime FutureTriggerRuntime = State.CardRun.ActiveBattle;
	FutureTriggerRuntime.EquipmentEffects[0].LastTriggerRound = FutureTriggerRuntime.RoundNumber + 1;
	TestFalse(TEXT("battle validation rejects an equipment trigger marked beyond the current round"),
		GameXXKCardRules::ValidateCardBattleRuntime(FutureTriggerRuntime, &Error));

	const FString SaveSlot = TEXT("Automation.GameXXK.Equipment.BattleIntegration");
	constexpr int32 SaveUserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(SaveSlot, SaveUserIndex);
	UGameXXKMVPSubsystem* SavingSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	SavingSubsystem->GetMutableRuntimeState() = State;
	TestTrue(TEXT("equipment-enriched active battle saves through the project save path"),
		SavingSubsystem->SaveCurrentGame(SaveSlot, SaveUserIndex));
	UGameXXKMVPSubsystem* LoadedSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("equipment-enriched active battle reloads through the project save path"),
		LoadedSubsystem->LoadGameFromSlot(SaveSlot, SaveUserIndex));
	const FGameXXKCardBattleRuntime& ReloadedBattle = LoadedSubsystem->GetRuntimeState().CardRun.ActiveBattle;
	TestEqual(TEXT("save reload preserves equipment battle-effect count"),
		ReloadedBattle.EquipmentEffects.Num(), State.CardRun.ActiveBattle.EquipmentEffects.Num());
	for (int32 EffectIndex = 0; EffectIndex < ReloadedBattle.EquipmentEffects.Num()
		&& EffectIndex < State.CardRun.ActiveBattle.EquipmentEffects.Num(); ++EffectIndex)
	{
		const FGameXXKEquipmentBattleEffectRuntime& SavedEffect = State.CardRun.ActiveBattle.EquipmentEffects[EffectIndex];
		const FGameXXKEquipmentBattleEffectRuntime& ReloadedEffect = ReloadedBattle.EquipmentEffects[EffectIndex];
		TestEqual(TEXT("save reload preserves equipment effect ID"), ReloadedEffect.ActiveEffect.EffectId, SavedEffect.ActiveEffect.EffectId);
		TestEqual(TEXT("save reload preserves equipment effect source"), ReloadedEffect.SourceCharacterId, SavedEffect.SourceCharacterId);
		TestEqual(TEXT("save reload preserves equipment effect trigger count"), ReloadedEffect.CurrentRoundTriggerCount, SavedEffect.CurrentRoundTriggerCount);
		TestEqual(TEXT("save reload preserves equipment effect last-trigger round"), ReloadedEffect.LastTriggerRound, SavedEffect.LastTriggerRound);
	}
	TestTrue(TEXT("save reload preserves a centrally valid equipment-enriched battle"),
		GameXXKCardRules::ValidateCardBattleRuntime(ReloadedBattle, &Error));
	UGameplayStatics::DeleteGameInSlot(SaveSlot, SaveUserIndex);

	return true;
}

#endif
