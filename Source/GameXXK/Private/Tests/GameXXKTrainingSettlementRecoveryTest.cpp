#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "MVP/GameXXKSaveMigration.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKSettlementRecoveryAtomicTest,
	"GameXXK.Training.Settlement.RecoveryWriteFailureAndRoundTrip", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKSettlementRecoveryAtomicTest::RunTest(const FString& Parameters)
{
	auto* Sub = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("recovery fixture starts"), Sub->StartGame())
		|| !TestTrue(TEXT("recovery fixture enters challenge"), Sub->StartTrainingChallenge(TEXT("Training.Normal.1-2")))) return false;
	auto& State = Sub->GetMutableRuntimeState();
	const auto* Boss = State.RouteMapNodes.FindByPredicate([](const auto& Node){return Node.NodeKind == EGameXXKNodeKind::Boss;});
	if (!TestNotNull(TEXT("recovery fixture has a Boss"), Boss)) return false;
	const int32 BossId = Boss->NodeId; State.ReachableRouteNodeIds = {BossId};
	State.CardRun.RouteAttributeBonuses.MaxHealth = 27;
	State.CardRun.RouteAttributeBonuses.MaxMana = 6;
	State.PlayerHP = State.PlayerMaxHP + 27;
	State.PlayerMP = State.PlayerMaxMP + 6;
	if (!Sub->SelectRouteNodeById(BossId)) return false;
	Sub->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	const auto Before = Sub->GetRuntimeState();
	Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*, const FString&, int32){return false;}));
	bool Complete = false; FGameXXKTrainingReward Reward;
	TestFalse(TEXT("a failed checkpoint write rejects the complete award transaction"), Sub->AdvanceTrainingChallengeEncounter(Complete, Reward));
	TestFalse(TEXT("failed persistence does not report a clear"), Complete);
	TestEqual(TEXT("failed persistence preserves gold"), Sub->GetRuntimeState().PlayerGold, Before.PlayerGold);
	TestEqual(TEXT("failed persistence preserves the reward random seed"), Sub->GetRuntimeState().Training.ChallengeRewardSeed, Before.Training.ChallengeRewardSeed);
	TestFalse(TEXT("failed persistence creates no visible receipt"), Sub->HasPendingTrainingSettlement());
	TestTrue(TEXT("failed persistence keeps the retryable Boss session"), Sub->GetRuntimeState().Training.bChallengeActive);
	TArray<uint8> Bytes; int32 WriteCount = 0;
	const auto SaveToMemory = [&Bytes, &WriteCount](USaveGame* Save, const FString&, int32)
	{
		++WriteCount; Bytes.Reset(); return UGameplayStatics::SaveGameToMemory(Save, Bytes);
	};
	Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(SaveToMemory));
	TestTrue(TEXT("retry atomically writes one complete receipt"), Sub->AdvanceTrainingChallengeEncounter(Complete, Reward));
	TestEqual(TEXT("successful award writes exactly once"), WriteCount, 1);
	const auto Receipt = Sub->GetPendingTrainingSettlementCopy();
	TestTrue(TEXT("clearing temporary maxima keeps permanent health valid"), Sub->GetRuntimeState().PlayerHP <= Sub->GetRuntimeState().PlayerMaxHP);
	TestTrue(TEXT("clearing temporary maxima keeps permanent mana valid"), Sub->GetRuntimeState().PlayerMP <= Sub->GetRuntimeState().PlayerMaxMP);
	auto* Saved = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("the stored UObject round trip is loadable"), Saved)) return false;
	FGameXXKSaveState Restored; FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("stored pending page passes normal migration and validation"), FGameXXKSaveMigration::MigrateToCurrent(Saved->SaveState, Restored, Report));
	TestEqual(TEXT("the round trip keeps the exact pending receipt"), Restored.RuntimeState.Training.PendingSettlement.ReceiptId, Receipt.ReceiptId);
	TestEqual(TEXT("loading does not reaward currency"), Restored.RuntimeState.PlayerGold, Sub->GetRuntimeState().PlayerGold);
	const int32 GoldAfter = Sub->GetRuntimeState().PlayerGold;
	Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*, const FString&, int32){return false;}));
	TestFalse(TEXT("failed acknowledgement persistence keeps the page open"), Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
	TestTrue(TEXT("failed acknowledgement retains the same receipt"), Sub->HasPendingTrainingSettlement());
	TestEqual(TEXT("failed acknowledgement does not change awards"), Sub->GetRuntimeState().PlayerGold, GoldAfter);
	Sub->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda(SaveToMemory));
	TestTrue(TEXT("acknowledgement can retry successfully"), Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
	Saved = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("acknowledged checkpoint loads"), Saved)) return false;
	TestFalse(TEXT("acknowledged checkpoint has no pending page"), Saved->SaveState.RuntimeState.Training.PendingSettlement.ReceiptId.IsValid());
	TestEqual(TEXT("acknowledged checkpoint retains awarded currency once"), Saved->SaveState.RuntimeState.PlayerGold, GoldAfter);
	TestTrue(TEXT("resumed travel has a persisted offline timestamp"), Saved->SaveState.RuntimeState.Training.TravelLastUpdatedUnixSeconds > 0);
	TestFalse(TEXT("duplicate confirmation cannot write or award again"), Sub->ConfirmTrainingSettlement(Receipt.ReceiptId));
	TestEqual(TEXT("only award and acknowledgement checkpoints were written"), WriteCount, 2);
	return true;
}
#endif
