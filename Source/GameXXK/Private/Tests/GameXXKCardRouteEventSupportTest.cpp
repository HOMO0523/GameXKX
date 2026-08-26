#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool StartNextRouteWithoutQuestNpc(FGameXXKRuntimeState& OutState)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutState = Subsystem->GetRuntimeState();
		return UGameXXKMVPRules::AcceptTownQuest(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState)
			&& UGameXXKMVPRules::AbandonDungeonToTown(OutState)
			&& UGameXXKMVPRules::EnterDungeon(OutState);
	}

	void ConfigurePendingYueBaiEvent(FGameXXKRuntimeState& State, const int32 NodeId)
	{
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.Screen = EGameXXKScreen::RouteEvent;
		State.RouteSeed = 24681357;
		State.RouteMapNodes = {FGameXXKRouteMapNode(NodeId, 2, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.4f), {})};
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.PendingRouteNodeId = NodeId;
		State.CardRun.RouteProgress.CurrentChapter = 1;
		State.CardRun.PendingEvent.SourceNodeId = NodeId;
		State.CardRun.PendingEvent.ChoiceSeed = 7654321 + NodeId;
		State.CardRun.PendingEvent.EncounterId = TEXT("Encounter.Event.YueBai");
		State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.YueBai");
	}

	int32 FindLastPermanentCompanionSlot(const FGameXXKOrderedPartyFormation& Formation)
	{
		for (int32 SlotIndex = Formation.Members.Num() - 1; SlotIndex >= 0; --SlotIndex)
		{
			if (Formation.Members[SlotIndex].Kind == EGameXXKPartyMemberKind::PermanentCompanion)
			{
				return SlotIndex;
			}
		}
		return INDEX_NONE;
	}

	bool FormationRoundTrips(const FGameXXKRuntimeState& State)
	{
		const FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(State);
		FGameXXKSaveState Reloaded;
		FGameXXKSaveMigrationReport Report;
		return FGameXXKSaveMigration::MigrateToCurrent(Source, Reloaded, Report)
			&& FGameXXKOrderedPartyFormation::StaticStruct()->CompareScriptStruct(
				&Reloaded.RuntimeState.CardRun.OrderedFormation,
				&State.CardRun.OrderedFormation,
				PPF_None);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardRouteEventSupportTest,
	"GameXXK.Integration.CardRoute.EventSupport.WrapperFormationAndSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardRouteEventSupportTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("event support fixture starts the next route after task-NPC retirement"),
		StartNextRouteWithoutQuestNpc(State)))
	{
		return false;
	}
	const FGameXXKOrderedPartyFormation FormationBeforeSupport = State.CardRun.OrderedFormation;
	const int32 ReplacedCompanionSlot = FindLastPermanentCompanionSlot(FormationBeforeSupport);
	if (!TestTrue(TEXT("settled formation exposes a deterministic companion slot for support"),
		ReplacedCompanionSlot != INDEX_NONE))
	{
		return false;
	}
	ConfigurePendingYueBaiEvent(State, 23);

	TestTrue(TEXT("accepting a named route event NPC adds only the temporary task-NPC slot and completes that event node"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	TestEqual(TEXT("the accepted named NPC is available to the later route battles"), State.CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("the accepted named NPC uses its fixed three-card route selection"), State.CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestEqual(TEXT("accepting temporary support returns to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the accepted event node completes exactly once"), State.VisitedRouteNodeIds.Contains(23));
	TestEqual(TEXT("accepted support occupies the prior highest companion slot"),
		State.CardRun.OrderedFormation.Members[ReplacedCompanionSlot].MemberId,
		FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("accepted support changes only that slot kind"),
		State.CardRun.OrderedFormation.Members[ReplacedCompanionSlot].Kind,
		EGameXXKPartyMemberKind::QuestNpc);
	for (int32 SlotIndex = 0; SlotIndex < FormationBeforeSupport.Members.Num(); ++SlotIndex)
	{
		if (SlotIndex != ReplacedCompanionSlot)
		{
			TestEqual(TEXT("support insertion preserves every other member kind"),
				State.CardRun.OrderedFormation.Members[SlotIndex].Kind,
				FormationBeforeSupport.Members[SlotIndex].Kind);
			TestEqual(TEXT("support insertion preserves every other member identity"),
				State.CardRun.OrderedFormation.Members[SlotIndex].MemberId,
				FormationBeforeSupport.Members[SlotIndex].MemberId);
		}
	}
	FString ValidationError;
	TestTrue(TEXT("accepted wrapper support is immediately save-valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError));
	TestTrue(TEXT("accepted wrapper support preserves exact formation through v24 roundtrip"),
		FormationRoundTrips(State));
	FGameXXKRuntimeState SettledAcceptedSupport = State;
	TestTrue(TEXT("accepted wrapper support can settle continuously"),
		UGameXXKMVPRules::AbandonDungeonToTown(SettledAcceptedSupport));
	ValidationError.Reset();
	TestTrue(TEXT("post-support settlement remains immediately save-valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(SettledAcceptedSupport, ValidationError));

	// Reuse the stable linear battle entry to prove that the accepted event identity is
	// projected into the next battle and then given a concrete scene placement.
	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.DungeonNodeIndex = 1;
	TestTrue(TEXT("the battle after accepting temporary support opens normally"),
		UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	TestTrue(TEXT("the accepted event NPC joins the next battle party"),
		State.ActiveBattleParty.ContainsByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == TEXT("Npc.YueBai");
		}));
	TestTrue(TEXT("the accepted event NPC is represented as a task-NPC combat unit"),
		State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == TEXT("Npc.YueBai") && Unit.Role == EGameXXKCharacterRole::QuestNpc;
		}));
	const FGameXXKBattleSceneUnitPlacement* QuestNpcPlacement = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(State).FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
		{
			return !Placement.bEnemy && Placement.UnitId == TEXT("Npc.YueBai");
		});
	TestNotNull(TEXT("the battle scene presenter creates a party placement for the accepted event NPC"), QuestNpcPlacement);
	if (QuestNpcPlacement)
	{
		TestEqual(TEXT("the task NPC retains fixed 我 3P after accepting its route event"), QuestNpcPlacement->SlotNumber, 3);
		const FGameXXKBattleRuntimeUnit* QuestNpcUnit = State.ActiveBattleParty.FindByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
		{
			return Unit.Id == TEXT("Npc.YueBai");
		});
		TestNotNull(TEXT("the accepted task NPC has a projected legacy scene unit"), QuestNpcUnit);
		if (QuestNpcUnit)
		{
			AGameXXKBattleSceneUnitActor* QuestNpcActor = NewObject<AGameXXKBattleSceneUnitActor>();
			QuestNpcActor->ConfigureFromRuntimeUnit(false, QuestNpcPlacement->UnitIndex, *QuestNpcUnit, QuestNpcPlacement->SlotNumber);
			UPaperFlipbook* QuestNpcFlipbook = QuestNpcActor->GetCurrentBattleFlipbook();
			TestNotNull(TEXT("the accepted named task NPC resolves its named battle flipbook"), QuestNpcFlipbook);
			if (QuestNpcFlipbook)
			{
				TestEqual(
					TEXT("the accepted Yue Bai NPC uses its dedicated 2K battle flipbook"),
					QuestNpcFlipbook->GetPathName(),
					FString(TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_character_09_yue_bai_2k_idle.FB_character_09_yue_bai_2k_idle")));
			}
		}
	}
	const TArray<FGameXXKBattleSceneUnitPlacement> AcceptedNpcPlacements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(State);
	const bool bHasPersistentPartner = State.CardRun.ActiveBattle.Units.ContainsByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Party
			&& Unit.Role != EGameXXKCharacterRole::Hero
			&& Unit.Role != EGameXXKCharacterRole::QuestNpc;
	});
	if (!bHasPersistentPartner)
	{
		TestFalse(TEXT("event NPC support never invents a permanent-partner 1P placement"),
			AcceptedNpcPlacements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
			{
				return !Placement.bEnemy && Placement.UnitId != TEXT("Player") && Placement.SlotNumber == 1;
			}));
	}

	State.Screen = EGameXXKScreen::RouteEvent;
	State.RouteMapNodes.Add(FGameXXKRouteMapNode(24, 3, 0, EGameXXKNodeKind::Event, FVector2D(0.5f, 0.55f), {}));
	State.PendingRouteNodeId = 24;
	State.CardRun.PendingEvent.SourceNodeId = 24;
	State.CardRun.PendingEvent.ChoiceSeed = 7654322;
	State.CardRun.PendingEvent.EventNpcId = TEXT("Npc.TusiChief");
	TestFalse(TEXT("a second task NPC cannot silently replace the route's existing temporary support"),
		UGameXXKMVPRules::AcceptRouteEventNpcSupport(State));
	TestEqual(TEXT("rejecting a second support offer preserves the original temporary NPC"),
		State.CardRun.PartySelection.QuestNpc.NpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("rejecting a second support offer leaves the event choice available"),
		State.CardRun.PendingEvent.EventNpcId, FName(TEXT("Npc.TusiChief")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteEncounterChoiceNpcFormationTest,
	"GameXXK.Integration.CardRoute.EventSupport.ChoiceFormationAndSettlement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteEncounterChoiceNpcFormationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State;
	if (!TestTrue(TEXT("choice support fixture starts after the prior task NPC retires"),
		StartNextRouteWithoutQuestNpc(State)))
	{
		return false;
	}
	ConfigurePendingYueBaiEvent(State, 31);
	const FGameXXKRouteEncounterDefinition* Encounter =
		FGameXXKRouteEncounterCatalog::FindDefinition(TEXT("Encounter.Event.YueBai"));
	if (!TestNotNull(TEXT("choice support fixture resolves the YueBai event definition"), Encounter))
	{
		return false;
	}
	const int32 SupportChoiceIndex = Encounter->Choices.IndexOfByPredicate(
		[](const FGameXXKRouteEncounterChoiceDefinition& Choice)
		{
			return Choice.RewardKind == EGameXXKRouteEncounterRewardKind::TemporaryNpcSupport
				&& Choice.QuestNpcId == TEXT("Npc.YueBai");
		});
	if (!TestTrue(TEXT("authored YueBai event exposes the production temporary-support choice"),
		SupportChoiceIndex != INDEX_NONE))
	{
		return false;
	}

	const FGameXXKRuntimeState BeforeChoice = State;
	const int32 ReplacedCompanionSlot = FindLastPermanentCompanionSlot(BeforeChoice.CardRun.OrderedFormation);
	if (!TestTrue(TEXT("choice support finds the deterministic last companion slot"),
		ReplacedCompanionSlot != INDEX_NONE))
	{
		return false;
	}
	TestTrue(TEXT("temporary-support encounter choice resolves"),
		UGameXXKMVPRules::ResolveRouteEncounterChoice(State, SupportChoiceIndex));
	TestEqual(TEXT("choice support occupies the previous last companion slot"),
		State.CardRun.OrderedFormation.Members[ReplacedCompanionSlot].MemberId,
		FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("choice support writes the quest-NPC member kind"),
		State.CardRun.OrderedFormation.Members[ReplacedCompanionSlot].Kind,
		EGameXXKPartyMemberKind::QuestNpc);
	for (int32 SlotIndex = 0; SlotIndex < BeforeChoice.CardRun.OrderedFormation.Members.Num(); ++SlotIndex)
	{
		if (SlotIndex != ReplacedCompanionSlot)
		{
			TestEqual(TEXT("choice support preserves every other member kind"),
				State.CardRun.OrderedFormation.Members[SlotIndex].Kind,
				BeforeChoice.CardRun.OrderedFormation.Members[SlotIndex].Kind);
			TestEqual(TEXT("choice support preserves every other member identity"),
				State.CardRun.OrderedFormation.Members[SlotIndex].MemberId,
				BeforeChoice.CardRun.OrderedFormation.Members[SlotIndex].MemberId);
		}
	}
	TestTrue(TEXT("choice support preserves the owned companion roster and decks bit-identically"),
		FGameXXKCompanionRosterState::StaticStruct()->CompareScriptStruct(
			&State.CardRun.CompanionRoster,
			&BeforeChoice.CardRun.CompanionRoster,
			PPF_None));
	FString ValidationError;
	TestTrue(TEXT("choice support is immediately save-valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError));
	TestTrue(TEXT("choice support survives an exact v24 roundtrip"), FormationRoundTrips(State));
	FGameXXKRuntimeState Settled = State;
	TestTrue(TEXT("choice support can settle without an invalid formation gap"),
		UGameXXKMVPRules::AbandonDungeonToTown(Settled));
	ValidationError.Reset();
	TestTrue(TEXT("choice support settlement remains save-valid"),
		FGameXXKSaveMigration::ValidateRuntimeState(Settled, ValidationError));

	FGameXXKRuntimeState NoLegalSlot = BeforeChoice;
	for (FGameXXKPartyMemberRef& Ref : NoLegalSlot.CardRun.OrderedFormation.Members)
	{
		Ref.Kind = EGameXXKPartyMemberKind::Hero;
		Ref.MemberId = TEXT("Player");
	}
	const FGameXXKRuntimeState NoLegalSlotBefore = NoLegalSlot;
	TestFalse(TEXT("temporary-support choice rejects a formation with no legal insertion slot"),
		UGameXXKMVPRules::ResolveRouteEncounterChoice(NoLegalSlot, SupportChoiceIndex));
	TestTrue(TEXT("failed support insertion leaves the entire candidate source bit-identical"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&NoLegalSlot,
			&NoLegalSlotBefore,
			PPF_None));

	FGameXXKRuntimeState StaleNpcSlot = BeforeChoice;
	const int32 StaleNpcSlotIndex = FindLastPermanentCompanionSlot(StaleNpcSlot.CardRun.OrderedFormation);
	StaleNpcSlot.CardRun.OrderedFormation.Members[StaleNpcSlotIndex].Kind = EGameXXKPartyMemberKind::QuestNpc;
	StaleNpcSlot.CardRun.OrderedFormation.Members[StaleNpcSlotIndex].MemberId = TEXT("Npc.TusiChief");
	const FGameXXKRuntimeState StaleNpcSlotBefore = StaleNpcSlot;
	TestFalse(TEXT("temporary-support choice rejects a stale pre-existing NPC slot with cleared mirrors"),
		UGameXXKMVPRules::ResolveRouteEncounterChoice(StaleNpcSlot, SupportChoiceIndex));
	TestTrue(TEXT("stale NPC source rejection leaves the complete runtime bit-identical"),
		FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(
			&StaleNpcSlot,
			&StaleNpcSlotBefore,
			PPF_None));
	return true;
}

#endif
