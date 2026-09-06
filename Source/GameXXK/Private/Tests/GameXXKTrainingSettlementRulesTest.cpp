#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardRules.h"
#include "GameXXKTrainingSettlementRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "UI/GameXXKTrainingSettlementWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSettlementCountersTest,
	"GameXXK.Training.Settlement.Counters", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKSettlementCountersTest::RunTest(const FString& Parameters)
{
	FGameXXKCardCombatUnit Unit;
	Unit.UnitId = TEXT("Player"); Unit.Side = EGameXXKCardTargetSide::Party;
	Unit.bLiving = true; Unit.HP = 90; Unit.MaxHP = 100;
	TestEqual(TEXT("overheal returns only the effective healing"), GameXXKCardRules::HealCombatUnit(Unit, 25), 10);
	TestEqual(TEXT("settlement records effective healing"), Unit.SettlementHealingReceived, int64(10));
	TestEqual(TEXT("armor is granted"), GameXXKCardRules::AddCombatArmor(Unit, 12), 12);
	TestEqual(TEXT("settlement records generated armor"), Unit.SettlementArmorGenerated, int64(12));
	FGameXXKCardBattleRuntime Battle;
	Battle.RoundNumber = 4; Battle.SessionStats.ActiveCardsPlayed = 7; Battle.Units.Add(Unit);
	const auto Stats = FGameXXKTrainingSettlementRules::CaptureBattleStats(Battle);
	TestEqual(TEXT("snapshot records rounds"), Stats.Rounds, 4);
	TestEqual(TEXT("snapshot records active cards"), Stats.ActiveCardsPlayed, 7);
	TestEqual(TEXT("snapshot records living party HP"), Stats.PartyEndingHealth, int64(100));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSettlementBossFlowTest,
	"GameXXK.Training.Settlement.BossReceiptAndAcknowledge", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKSettlementBossFlowTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts a valid permanent party"), Sub->StartGame())) return false;
	const FName StageId(TEXT("Training.Normal.1-2"));
	if (!TestTrue(TEXT("fixture starts an unlocked challenge"), Sub->StartTrainingChallenge(StageId))) return false;
	FGameXXKRuntimeState& State = Sub->GetMutableRuntimeState();
	const FGameXXKRouteMapNode* Boss = State.RouteMapNodes.FindByPredicate([](const FGameXXKRouteMapNode& Node){return Node.NodeKind == EGameXXKNodeKind::Boss;});
	if (!TestNotNull(TEXT("generated route contains its Boss"), Boss)) return false;
	const int32 BossId = Boss->NodeId;
	State.ReachableRouteNodeIds = {BossId};
	if (!TestTrue(TEXT("the Boss encounter starts"), Sub->SelectRouteNodeById(BossId))) return false;
	Sub->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	Sub->GetMutableRuntimeState().CardRun.ActiveBattle.RoundNumber = 5;
	const int32 GoldBefore = Sub->GetRuntimeState().PlayerGold;
	bool Completed = false; FGameXXKTrainingReward Reward;
	if (!TestTrue(TEXT("terminal Boss reward transaction completes"), Sub->AdvanceTrainingChallengeEncounter(Completed, Reward))) return false;
	if (!TestTrue(TEXT("a Boss victory retains a pending settlement page"), Sub->HasPendingTrainingSettlement())) return false;
	const auto Receipt = Sub->GetPendingTrainingSettlementCopy();
	TestEqual(TEXT("receipt owns the actual stage"), Receipt.StageId, StageId);
	TestEqual(TEXT("receipt shows actual gold already awarded"), Receipt.Gold, Sub->GetRuntimeState().PlayerGold - GoldBefore);
	TestEqual(TEXT("receipt freezes all three deployed member outcomes"), Receipt.Members.Num(), 3);
	TestEqual(TEXT("receipt statistics identify the final Boss battle"), Receipt.Stats.Rounds, 5);
	TestTrue(TEXT("Boss does not create ordinary reward choices"), Sub->GetRuntimeState().CardRun.PendingReward.Options.IsEmpty());
	TestFalse(TEXT("idle waits for confirmation"), Sub->GetRuntimeState().Training.bTravelActive);
	TestFalse(TEXT("a pending receipt prevents another challenge"), Sub->StartTrainingChallenge(StageId));
	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	Controller->SetMVPSubsystemForTest(Sub); Controller->SetDesktopTrainingBootProfileForTest(true);
	Controller->EnsureDesktopTrainingWidgetsForTest(); Controller->RefreshPlayerFlowWidgetsForTest();
	auto* Widget = Controller->GetTrainingSettlementWidgetForTest();
	if (TestNotNull(TEXT("the desktop flow opens the actual settlement widget"), Widget))
	{
		TestEqual(TEXT("widget displays the same immutable receipt"), Widget->GetReceiptIdForTest(), Receipt.ReceiptId);
		TestEqual(TEXT("widget displays the applied gold"), Widget->GetDisplayedGoldForTest(), Receipt.Gold);
	}
	FGameXXKSaveState Saved = UGameXXKMVPRules::MakeSaveState(Sub->GetRuntimeState()), Reloaded;
	FGameXXKSaveMigrationReport Migration;
	TestTrue(TEXT("pending receipt survives save normalization"), FGameXXKSaveMigration::MigrateToCurrent(Saved, Reloaded, Migration));
	TestEqual(TEXT("load retains the same receipt identity"), Reloaded.RuntimeState.Training.PendingSettlement.ReceiptId, Receipt.ReceiptId);
	TestEqual(TEXT("load never grants gold twice"), Reloaded.RuntimeState.PlayerGold, Saved.RuntimeState.PlayerGold);
	TestFalse(TEXT("wrong confirmation id is rejected"), Sub->ConfirmTrainingSettlement(FGuid::NewGuid()));
	const int32 GoldApplied = Sub->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("matching confirmation returns to the workbench"), Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
	TestFalse(TEXT("confirmation dismisses the pending receipt"), Sub->HasPendingTrainingSettlement());
	TestTrue(TEXT("confirmation resumes idle travel"), Sub->GetRuntimeState().Training.bTravelActive);
	TestEqual(TEXT("confirmation grants no additional gold"), Sub->GetRuntimeState().PlayerGold, GoldApplied);
	TestFalse(TEXT("duplicate confirmation is rejected without changes"), Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
	Controller->RefreshPlayerFlowWidgetsForTest();
	if (Widget) TestEqual(TEXT("confirmed settlement is hidden"), Widget->GetVisibility(), ESlateVisibility::Collapsed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSettlementMigrationTest,
	"GameXXK.Training.Settlement.Version36Migration", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKSettlementMigrationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!Sub->StartGame()) return false;
	FGameXXKSaveState Old = UGameXXKMVPRules::MakeSaveState(Sub->GetRuntimeState());
	Old.SaveVersion = 36;
	const int32 Gold = Old.RuntimeState.PlayerGold;
	FGameXXKSaveState Migrated; FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("v36 upgrades without fabricating a past receipt"), FGameXXKSaveMigration::MigrateToCurrent(Old, Migrated, Report));
	TestEqual(TEXT("schema uses the actual next version"), Migrated.SaveVersion, 37);
	TestFalse(TEXT("old clear flags do not create a pending receipt"), Migrated.RuntimeState.Training.PendingSettlement.ReceiptId.IsValid());
	TestEqual(TEXT("migration grants no gold"), Migrated.RuntimeState.PlayerGold, Gold);
	return true;
}
#endif
