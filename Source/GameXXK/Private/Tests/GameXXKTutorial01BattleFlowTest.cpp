#include "MVP/GameXXKMVPPlayerController.h"

#include "GameXXKCardBattleAdapter.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01SessionSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKTutorial01ResultWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01BattleFlowTest,
	"GameXXK.Tutorial01.BattleFlow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01BattleFlowTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Runtime =
		NewObject<UGameXXKMVPSubsystem>(GameInstance);
	UGameXXKTutorial01SessionSubsystem* Session =
		NewObject<UGameXXKTutorial01SessionSubsystem>(GameInstance);
	if (!TestTrue(TEXT("battle-flow fixture starts Qingshan"),
		Runtime && Runtime->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}
	FGameXXKRuntimeState Before = Runtime->GetRuntimeStateCopy();
	Before.Screen = EGameXXKScreen::Town;
	Before.PlayerGold = 9876;
	Before.PlayerXP = 123;
	Runtime->GetMutableRuntimeState() = Before;
	TestTrue(TEXT("battle-flow session begins"),
		Session->BeginFromTown(
			Before,
			FTransform::Identity,
			EGameXXKGuidePreference::ExperiencedPlayer));
	EGameXXKTutorial01RouteAction Action = EGameXXKTutorial01RouteAction::None;
	TestTrue(TEXT("battle node is selected"),
		Session->RequestRouteNode(
			FGameXXKTutorial01RouteRules::BattleNodeId,
			Action));

	AGameXXKMVPPlayerController* Controller =
		NewObject<AGameXXKMVPPlayerController>();
	TestTrue(TEXT("controller starts tutorial battle runtime"),
		Controller->StartTutorial01BattleRuntimeForTest(Runtime, Session));
	Runtime->GetMutableRuntimeState().CardRun.ActiveBattle.Phase =
		EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("tutorial victory is intercepted"),
		Controller->HandleTutorial01BattleTerminalForTest(
			Runtime,
			Session,
			EGameXXKCardBattlePhase::Victory));
	TestEqual(TEXT("tutorial victory returns to fixed route"),
		Runtime->GetRuntimeState().Screen,
		EGameXXKScreen::DungeonMap);
	TestFalse(TEXT("tutorial victory clears active card battle"),
		Runtime->GetRuntimeState().CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("tutorial victory grants no gold"),
		Runtime->GetRuntimeState().PlayerGold,
		9876);
	TestEqual(TEXT("tutorial victory grants no XP"),
		Runtime->GetRuntimeState().PlayerXP,
		123);
	TestTrue(TEXT("tutorial victory creates no reward offer"),
		Runtime->GetRuntimeState().CardRun.PendingReward.SourceNodeId == INDEX_NONE
		&& Runtime->GetRuntimeState().CardRun.PendingReward.Options.IsEmpty()
		&& Runtime->GetRuntimeState().CardRun.PendingReward.CardIds.IsEmpty());
	TestTrue(TEXT("tutorial battle node is marked complete"),
		Session->GetRouteState().VisitedNodeIds.Contains(
			FGameXXKTutorial01RouteRules::BattleNodeId));
	TestTrue(TEXT("tutorial return node unlocks"),
		Session->GetRouteState().ReachableNodeIds.Contains(
			FGameXXKTutorial01RouteRules::ReturnTownNodeId));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Runtime);
	Board->TakeWidget();
	int32 TerminalInterceptCount = 0;
	Board->SetBattleTerminalInterceptor(
		FGameXXKBattleTerminalInterceptor::CreateLambda(
			[&TerminalInterceptCount](const EGameXXKCardBattlePhase Phase)
			{
				++TerminalInterceptCount;
				return Phase == EGameXXKCardBattlePhase::Victory;
			}));
	Runtime->GetMutableRuntimeState() = Before;
	Runtime->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	Runtime->GetMutableRuntimeState().CardRun.bHasActiveCardBattle = true;
	Runtime->GetMutableRuntimeState().CardRun.ActiveBattle.Phase =
		EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("board terminal interceptor handles victory"),
		Board->ResolveCardBattleTerminalStateForTest());
	TestEqual(TEXT("terminal interceptor executes once"), TerminalInterceptCount, 1);
	TestEqual(TEXT("board interceptor bypasses ordinary gold mutation"),
		Runtime->GetRuntimeState().PlayerGold,
		9876);
	TestTrue(TEXT("board interceptor creates no ordinary reward"),
		Runtime->GetRuntimeState().CardRun.PendingReward.SourceNodeId == INDEX_NONE
		&& Runtime->GetRuntimeState().CardRun.PendingReward.Options.IsEmpty()
		&& Runtime->GetRuntimeState().CardRun.PendingReward.CardIds.IsEmpty());

	int32 ExitInterceptCount = 0;
	Board->SetBattleExitInterceptor(
		FGameXXKBattleExitInterceptor::CreateLambda(
			[&ExitInterceptCount]()
			{
				++ExitInterceptCount;
				return true;
			}));
	TestTrue(TEXT("tutorial exit request is handled"),
		Board->RequestBattleExitForTest());
	TestEqual(TEXT("exit interceptor executes once"), ExitInterceptCount, 1);
	TestFalse(TEXT("tutorial exit bypasses ordinary retreat modal"),
		Board->IsBattleRetreatConfirmationOpenForTest());

	UGameXXKTutorial01ResultWidget* ResultWidget =
		NewObject<UGameXXKTutorial01ResultWidget>();
	ResultWidget->TakeWidget();
	int32 RetryCount = 0;
	int32 ReturnCount = 0;
	ResultWidget->SetRetryRequested(
		FGameXXKTutorial01RetryRequested::CreateLambda(
			[&RetryCount]() { ++RetryCount; }));
	ResultWidget->SetReturnTownRequested(
		FGameXXKTutorial01ReturnTownRequested::CreateLambda(
			[&ReturnCount]() { ++ReturnCount; }));
	ResultWidget->PresentFailure(FText::FromString(TEXT("战斗失败")));
	TestTrue(TEXT("failure surface becomes visible"), ResultWidget->IsVisibleForTest());
	TestTrue(TEXT("failure surface uses approved paper"),
		ResultWidget->GetPaperTexturePathForTest().Contains(
			TEXT("T_MasterV2_PanelLarge")));
	ResultWidget->ChooseRetryForTest();
	TestEqual(TEXT("retry emits once"), RetryCount, 1);
	ResultWidget->PresentFailure(FText::FromString(TEXT("战斗失败")));
	ResultWidget->ChooseReturnTownForTest();
	TestEqual(TEXT("return emits once"), ReturnCount, 1);

	return true;
}

#endif
