#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPartyFormationRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FRecoveryCase
	{
		FName OrderedNpcId;
		FName SelectionNpcId;
		FName TemporaryNpcId;
		FName ExpectedNpcId;
	};

	struct FLegacyEncounterCase
	{
		FName EncounterId;
		FName EventNpcId;
	};

	bool BuildStartedSave(FGameXXKSaveState& OutSave)
	{
		UGameInstance* GameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
		if (!Subsystem || !Subsystem->StartGame())
		{
			return false;
		}
		OutSave = UGameXXKMVPRules::MakeSaveState(Subsystem->GetRuntimeState());
		OutSave.SaveVersion = 29;
		return true;
	}

	void SetOrderedNpcOrRemove(FGameXXKRuntimeState& State, const FName NpcId)
	{
		const int32 NpcIndex = State.CardRun.OrderedFormation.Members.IndexOfByPredicate(
			[](const FGameXXKPartyMemberRef& Member)
			{
				return Member.Kind == EGameXXKPartyMemberKind::QuestNpc;
			});
		if (NpcIndex == INDEX_NONE)
		{
			return;
		}
		if (NpcId.IsNone())
		{
			State.CardRun.OrderedFormation.Members.RemoveAt(NpcIndex);
			return;
		}
		State.CardRun.OrderedFormation.Members[NpcIndex].MemberId = NpcId;
	}

	void SetLegacySelectionOrClear(FGameXXKRuntimeState& State, const FName NpcId)
	{
		State.CardRun.PartySelection.QuestNpc = FGameXXKQuestNpcCardSelection();
		if (NpcId.IsNone())
		{
			return;
		}
		const FGameXXKQuestNpcOwnedCardLoadout* Loadout =
			State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(NpcId);
		if (Loadout)
		{
			State.CardRun.PartySelection.QuestNpc.NpcId = NpcId;
			State.CardRun.PartySelection.QuestNpc.SelectedCardIds = Loadout->SelectedCardIds;
		}
	}

	void ConfigureLegacyPendingEvent(
		FGameXXKRuntimeState& State,
		const FName EncounterId,
		const FName EventNpcId)
	{
		constexpr int32 SourceNodeId = 71;
		constexpr int32 ChoiceSeed = 0x7135;
		State.Screen = EGameXXKScreen::RouteEvent;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.bDungeonActive = true;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun, 60);
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode(
				SourceNodeId,
				1,
				0,
				EGameXXKNodeKind::Event,
				FVector2D(0.5f, 0.5f),
				{})};
		State.RouteMapEdges.Reset();
		State.VisitedRouteNodeIds.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.CurrentRouteNodeId = SourceNodeId;
		State.PendingRouteNodeId = SourceNodeId;
		State.CardRun.PendingEvent.SourceNodeId = SourceNodeId;
		State.CardRun.PendingEvent.ChoiceSeed = ChoiceSeed;
		State.CardRun.PendingEvent.EncounterId = EncounterId;
		State.CardRun.PendingEvent.EventNpcId = EventNpcId;
		State.CardRun.PendingEvent.bCanRecruitPermanentCompanion = false;
		State.CardRun.PendingRelicOffer = FGameXXKPendingRelicOffer();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPermanentNpcV30SaveMigrationTest,
	"GameXXK.MVP.SaveGame.PermanentNpcV30Migration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPermanentNpcV30SaveMigrationTest::RunTest(const FString& Parameters)
{
	const FRecoveryCase RecoveryCases[] = {
		{TEXT("Npc.YueBai"), TEXT("Npc.JinGui"), TEXT("Npc.QiongMeiEr"), TEXT("Npc.YueBai")},
		{NAME_None, TEXT("Npc.JinGui"), TEXT("Npc.QiongMeiEr"), TEXT("Npc.JinGui")},
		{NAME_None, NAME_None, TEXT("Npc.QiongMeiEr"), TEXT("Npc.QiongMeiEr")},
		{NAME_None, NAME_None, NAME_None, TEXT("Npc.TusiChief")}};

	for (const FRecoveryCase& Recovery : RecoveryCases)
	{
		FGameXXKSaveState Source;
		if (!TestTrue(TEXT("recovery fixture starts"), BuildStartedSave(Source)))
		{
			return false;
		}
		SetOrderedNpcOrRemove(Source.RuntimeState, Recovery.OrderedNpcId);
		SetLegacySelectionOrClear(Source.RuntimeState, Recovery.SelectionNpcId);
		// Explicit legacy-v29 migration input; current fixtures never write this tombstone.
		Source.RuntimeState.CardRun.ActiveTemporaryQuestNpcId = Recovery.TemporaryNpcId;

		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		if (!TestTrue(TEXT("v29 NPC source migrates"),
			FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
		{
			AddError(Report.Error);
			continue;
		}
		FName ResolvedNpcId;
		TestTrue(TEXT("migrated ordered NPC resolves"),
			FGameXXKPartyFormationRules::ResolveQuestNpcId(Migrated.RuntimeState, ResolvedNpcId));
		TestEqual(TEXT("recovery order selects the approved identity"), ResolvedNpcId, Recovery.ExpectedNpcId);
		TestTrue(TEXT("legacy temporary field is cleared"),
			Migrated.RuntimeState.CardRun.ActiveTemporaryQuestNpcId.IsNone());
		TestEqual(TEXT("migration targets current v34"), Migrated.SaveVersion, 34);
	}

	const FLegacyEncounterCase EncounterCases[] = {
		{TEXT("Encounter.Event.TusiChief"), TEXT("Npc.TusiChief")},
		{TEXT("Encounter.Event.SongJinBao"), TEXT("Npc.SongJinBao")},
		{TEXT("Encounter.Event.YueBai"), TEXT("Npc.YueBai")},
		{TEXT("Encounter.Event.ZhouGuangZu"), TEXT("Npc.ZhouGuangZu")},
		{TEXT("Encounter.Event.JinGui"), TEXT("Npc.JinGui")},
		{TEXT("Encounter.Event.QiongMeiEr"), TEXT("Npc.QiongMeiEr")},
		{TEXT("Encounter.Event.NiuHuan"), TEXT("Npc.Event.NiuHuan")},
		{NAME_None, TEXT("Npc.YueBai")},
		{NAME_None, TEXT("Npc.Event.NiuHuan")}};

	for (const FLegacyEncounterCase& Encounter : EncounterCases)
	{
		FGameXXKSaveState Source;
		if (!TestTrue(TEXT("pending-event fixture starts"), BuildStartedSave(Source)))
		{
			return false;
		}
		FString SelectionError;
		TestTrue(TEXT("fixture selects Yue Bai before route entry"),
			FGameXXKPartyFormationRules::SetQuestNpc(
				Source.RuntimeState,
				TEXT("Npc.YueBai"),
				&SelectionError));
		ConfigureLegacyPendingEvent(
			Source.RuntimeState,
			Encounter.EncounterId,
			Encounter.EventNpcId);

		const int32 GoldBefore = Source.RuntimeState.PlayerGold;
		const FGameXXKRouteAttributeBonuses AttributesBefore =
			Source.RuntimeState.CardRun.RouteAttributeBonuses;
		const TMap<FName, int32> InventoryBefore = Source.RuntimeState.Inventory;
		const FGameXXKRouteSettlementReceipt SettlementBefore =
			Source.RuntimeState.CardRun.PendingSettlement;
		const int32 PendingNodeBefore = Source.RuntimeState.PendingRouteNodeId;

		FGameXXKSaveState Migrated;
		FGameXXKSaveMigrationReport Report;
		if (!TestTrue(TEXT("v29 pending NPC event migrates"),
			FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report)))
		{
			AddError(Report.Error);
			continue;
		}
		TestEqual(TEXT("pending node is preserved"),
			Migrated.RuntimeState.CardRun.PendingEvent.SourceNodeId, 71);
		TestEqual(TEXT("choice seed is preserved"),
			Migrated.RuntimeState.CardRun.PendingEvent.ChoiceSeed, 0x7135);
		TestEqual(TEXT("removed event remaps to Mountain Spring"),
			Migrated.RuntimeState.CardRun.PendingEvent.EncounterId,
			FName(TEXT("Encounter.Event.MountainSpring")));
		TestEqual(TEXT("environment presentation identity is installed"),
			Migrated.RuntimeState.CardRun.PendingEvent.EventNpcId,
			FName(TEXT("Event.Attribute.MountainSpring")));
		TestEqual(TEXT("screen remains an unresolved event"),
			Migrated.RuntimeState.Screen,
			EGameXXKScreen::RouteEvent);
		TestEqual(TEXT("no reward is granted"), Migrated.RuntimeState.PlayerGold, GoldBefore);
		TestTrue(TEXT("route attributes are unchanged"),
			FGameXXKRouteAttributeBonuses::StaticStruct()->CompareScriptStruct(
				&Migrated.RuntimeState.CardRun.RouteAttributeBonuses,
				&AttributesBefore,
				PPF_None));
		TestTrue(TEXT("inventory is unchanged"),
			Migrated.RuntimeState.Inventory.OrderIndependentCompareEqual(InventoryBefore));
		TestTrue(TEXT("pending settlement is unchanged"),
			FGameXXKRouteSettlementReceipt::StaticStruct()->CompareScriptStruct(
				&Migrated.RuntimeState.CardRun.PendingSettlement,
				&SettlementBefore,
				PPF_None));
		TestEqual(TEXT("route node remains unresolved"),
			Migrated.RuntimeState.PendingRouteNodeId,
			PendingNodeBefore);
		FName MigratedNpcId;
		TestTrue(TEXT("event remap keeps the selected party NPC"),
			FGameXXKPartyFormationRules::ResolveQuestNpcId(Migrated.RuntimeState, MigratedNpcId));
		TestEqual(TEXT("event remap leaves Yue Bai selected"), MigratedNpcId, FName(TEXT("Npc.YueBai")));

		FGameXXKSaveState SecondPass;
		FGameXXKSaveMigrationReport SecondReport;
		TestTrue(TEXT("v30 result migrates idempotently"),
			FGameXXKSaveMigration::MigrateToCurrent(Migrated, SecondPass, SecondReport));
		TestTrue(TEXT("v30 second pass preserves every reflected property"),
			FGameXXKSaveState::StaticStruct()->CompareScriptStruct(
				&SecondPass,
				&Migrated,
				PPF_None));
	}

	return true;
}

#endif
