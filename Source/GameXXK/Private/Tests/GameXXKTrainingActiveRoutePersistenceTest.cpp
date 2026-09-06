#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardBattleAdapter.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTrainingRouteSaveTest,
	"GameXXK.Training.Settlement.ActiveRouteAndLegacyBossSave", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKTrainingRouteSaveTest::RunTest(const FString& Parameters)
{
	auto* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!Sub->StartGame() || !Sub->StartTrainingChallenge(TEXT("Training.Normal.1-2"))) return false;
	FString Error;
	TestTrue(FString::Printf(TEXT("a real single-map challenge is saveable: %s"),*Error), FGameXXKSaveMigration::ValidateRuntimeState(Sub->GetRuntimeState(),Error));
	FGameXXKSaveState Restored; FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("a route map round trip retains its one-time node chapter"),FGameXXKSaveMigration::MigrateToCurrent(UGameXXKMVPRules::MakeSaveState(Sub->GetRuntimeState()),Restored,Report));
	TestEqual(TEXT("single-map node receipts keep chapter one"), Restored.RuntimeState.CardRun.RouteProgress.CurrentChapter,1);
	TestEqual(TEXT("single-map save never activates three-chapter progression"), Restored.RuntimeState.CardRun.RouteProgress.SchemaVersion,0);
	auto& State=Sub->GetMutableRuntimeState();
	const auto* Boss=State.RouteMapNodes.FindByPredicate([](const auto& N){return N.NodeKind==EGameXXKNodeKind::Boss;});
	if (!Boss) return false;
	const int32 Id=Boss->NodeId; State.ReachableRouteNodeIds={Id};
	if (!Sub->SelectRouteNodeById(Id)) return false;
	State.CardRun.ActiveBattle.Phase=EGameXXKCardBattlePhase::Victory;
	if (!FGameXXKCardBattleAdapter::CreateTieredBattleRewardOffer(State,EGameXXKNodeKind::Boss,Id,773401,&Error)) return false;
	FGameXXKSaveState Old=UGameXXKMVPRules::MakeSaveState(State); Old.SaveVersion=36;
	TestTrue(TEXT("an unclaimed v36 Boss offer migrates"),FGameXXKSaveMigration::MigrateToCurrent(Old,Restored,Report));
	TestTrue(TEXT("old Boss choices are removed before settlement resumes"),Restored.RuntimeState.CardRun.PendingReward.Options.IsEmpty());
	TestTrue(TEXT("the terminal Boss remains retryable after migration"),Restored.RuntimeState.Training.bChallengeActive);
	TestEqual(TEXT("migration grants no currency"),Restored.RuntimeState.PlayerGold,Old.RuntimeState.PlayerGold);
	TestFalse(TEXT("migration does not fabricate an applied receipt"),Restored.RuntimeState.Training.PendingSettlement.ReceiptId.IsValid());
	return true;
}
#endif
