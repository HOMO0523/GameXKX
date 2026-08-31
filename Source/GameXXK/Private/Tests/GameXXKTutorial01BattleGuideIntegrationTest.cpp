#include "Misc/AutomationTest.h"

#include "Guide/GameXXKGuideTargetRegistry.h"
#include "Guide/GameXXKTutorial01GuideHost.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKTutorial01SessionSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"

#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKTutorial01BattleGuideIntegrationTestPrivate
{
	struct FFixture
	{
		TObjectPtr<UGameInstance> GameInstance;
		TObjectPtr<UGameXXKMVPSubsystem> Runtime;
		TObjectPtr<UGameXXKTutorial01SessionSubsystem> Session;
		TObjectPtr<AGameXXKMVPPlayerController> Controller;
		TObjectPtr<UGameXXKBattleBoardWidget> Board;
		TObjectPtr<UGameXXKGuideAsset> Asset;
	};

	bool BuildFixture(FFixture& Out, FString& OutError)
	{
		Out.GameInstance = NewObject<UGameInstance>();
		Out.Runtime = NewObject<UGameXXKMVPSubsystem>(Out.GameInstance);
		Out.Session = NewObject<UGameXXKTutorial01SessionSubsystem>(Out.GameInstance);
		if (!Out.Runtime || !Out.Runtime->EnsureQingshanTownRuntimeForDirectMap())
		{
			OutError = TEXT("Could not create Qingshan runtime.");
			return false;
		}
		FGameXXKRuntimeState Before = Out.Runtime->GetRuntimeStateCopy();
		Before.Screen = EGameXXKScreen::Town;
		Out.Runtime->GetMutableRuntimeState() = Before;
		if (!Out.Session->BeginFromTown(
				Before,
				FTransform::Identity,
				EGameXXKGuidePreference::NewPlayer))
		{
			OutError = TEXT("Could not begin tutorial session.");
			return false;
		}
		EGameXXKTutorial01RouteAction Action = EGameXXKTutorial01RouteAction::None;
		if (!Out.Session->RequestRouteNode(
				FGameXXKTutorial01RouteRules::BattleNodeId,
				Action))
		{
			OutError = TEXT("Could not select tutorial battle node.");
			return false;
		}
		Out.Controller = NewObject<AGameXXKMVPPlayerController>();
		if (!Out.Controller->StartTutorial01BattleRuntimeForTest(
				Out.Runtime,
				Out.Session))
		{
			OutError = TEXT("Could not build tutorial battle runtime.");
			return false;
		}
		Out.Board = NewObject<UGameXXKBattleBoardWidget>();
		Out.Board->SetMVPSubsystem(Out.Runtime);
		Out.Board->TakeWidget();
		Out.Board->RefreshFromState();
		if (!Out.Board->BeginBattleVisualSession(701))
		{
			OutError = TEXT("Could not start Board visual session.");
			return false;
		}
		Out.Board->RefreshFromState();
		Out.Asset = LoadObject<UGameXXKGuideAsset>(
			nullptr,
			TEXT("/Game/GameXXK/Narrative/Guides/DA_Guide_Battle_Tutorial01_NewPlayer.DA_Guide_Battle_Tutorial01_NewPlayer"));
		if (!Out.Asset)
		{
			OutError = TEXT("Tutorial Guide asset is missing.");
			return false;
		}
		return true;
	}

	FName FindHandInstanceByCardId(
		const UGameXXKMVPSubsystem& Runtime,
		const FName CardId)
	{
		const FGameXXKBattleDeckState& Deck =
			Runtime.GetRuntimeState().CardRun.ActiveBattle.Deck;
		const FGameXXKCardInstance* Card = Deck.Hand.FindByPredicate(
			[CardId](const FGameXXKCardInstance& Candidate)
			{
				return Candidate.CardId == CardId;
			});
		return Card ? Card->InstanceId : NAME_None;
	}

	const FGameXXKCardCombatUnit* FindUnit(
		const UGameXXKMVPSubsystem& Runtime,
		const FName UnitId)
	{
		return Runtime.GetRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate(
			[UnitId](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == UnitId;
			});
	}

	void DrainBoard(UGameXXKBattleBoardWidget& Board, double& InOutClock)
	{
		for (int32 Step = 0; Step < 8; ++Step)
		{
			InOutClock += 10.0;
			Board.AdvanceVisualsAtRealTime(InOutClock);
			Board.AdvanceEnemyIntentPresentationForTest(10.0f);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01BattleGuideTargetsTest,
	"GameXXK.Tutorial01.BattleGuide.RealTargets",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01BattleGuideTargetsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTutorial01BattleGuideIntegrationTestPrivate;
	UGameXXKBattleUnitResourceWidget* Resource =
		NewObject<UGameXXKBattleUnitResourceWidget>();
	TestTrue(TEXT("resource fixture prepares"), Resource->PrepareForScreenSpaceEmbedding());
	TestNotNull(TEXT("health row exposes the real Guide target"),
		Resource->GetHealthRowForGuide());
	TestNotNull(TEXT("mana row exposes the real Guide target"),
		Resource->GetManaRowForGuide());

	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	Registry.Reset();
	FFixture Fixture;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("tutorial Board fixture builds: %s"), *Error),
		BuildFixture(Fixture, Error)))
	{
		return false;
	}
	int32 FailureCount = 0;
	TestTrue(TEXT("Board attaches new-player tutorial Guide"),
		Fixture.Board->StartTutorial01Guide(
			Fixture.Session->GetMutableGuideProgress(),
			*Fixture.Asset,
			FGameXXKTutorial01GuideFailed::CreateLambda(
				[&FailureCount](const FString& Diagnostic) { ++FailureCount; })));
	const TArray<FName> InitialTargets = {
		TEXT("Battle.Hud.PartyQi"),
		TEXT("Battle.Unit.Hero.Health"),
		TEXT("Battle.Unit.Hero.Mana"),
		TEXT("Battle.Enemy.Intent"),
		TEXT("Battle.Hand.HengJianShouShi"),
		TEXT("Battle.Hand.SuiYanJi"),
		TEXT("Battle.Hand.FengShenBu"),
		TEXT("Battle.Unit.Hero.Target"),
		TEXT("Battle.Unit.Enemy.Target"),
		TEXT("Battle.Unit.YueBai.Visual"),
		TEXT("Battle.EndTurn"),
		TEXT("Battle.AutoBattle")};
	for (const FName TargetId : InitialTargets)
	{
		TestTrue(
			FString::Printf(TEXT("Board registers %s"), *TargetId.ToString()),
			Registry.IsTargetRegistered(TargetId));
	}
	TestFalse(TEXT("forced-discard target is dynamic before FengShen"),
		Registry.IsTargetRegistered(TEXT("Battle.Pending.ForcedDiscard")));
	TestEqual(TEXT("target setup emits no Guide failure"), FailureCount, 0);
	Fixture.Board->CancelTutorial01Guide();
	Fixture.Board->CancelBattleVisualSession(701);
	Registry.Reset();
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTutorial01BattleGuideActionChainTest,
	"GameXXK.Tutorial01.BattleGuide.AuthoritativeActionChain",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTutorial01BattleGuideActionChainTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKTutorial01BattleGuideIntegrationTestPrivate;
	FGameXXKGuideTargetRegistry& Registry = FGameXXKGuideTargetRegistry::Get();
	Registry.Reset();
	FFixture Fixture;
	FString Error;
	if (!TestTrue(FString::Printf(TEXT("action-chain fixture builds: %s"), *Error),
		BuildFixture(Fixture, Error)))
	{
		return false;
	}
	int32 FailureCount = 0;
	if (!TestTrue(TEXT("action-chain Guide starts"),
		Fixture.Board->StartTutorial01Guide(
			Fixture.Session->GetMutableGuideProgress(),
			*Fixture.Asset,
			FGameXXKTutorial01GuideFailed::CreateLambda(
				[&FailureCount](const FString& Diagnostic) { ++FailureCount; }))))
	{
		return false;
	}
	TestTrue(TEXT("Qi Space continues"), Fixture.Board->HandleTutorial01GuideContinue());
	TestTrue(TEXT("vitals Space continues"), Fixture.Board->HandleTutorial01GuideContinue());
	TestTrue(TEXT("intent Space continues"), Fixture.Board->HandleTutorial01GuideContinue());

	const FName HengJian = FindHandInstanceByCardId(
		*Fixture.Runtime, TEXT("Hero.Generic.HengJianShouShi"));
	const FName SuiYan = FindHandInstanceByCardId(
		*Fixture.Runtime, TEXT("Hero.Generic.SuiYanJi"));
	const FName FengShen = FindHandInstanceByCardId(
		*Fixture.Runtime, TEXT("Hero.Generic.FengShenBu"));
	TestFalse(TEXT("fixed three tutorial cards resolve to instances"),
		HengJian.IsNone() || SuiYan.IsNone() || FengShen.IsNone());
	TestFalse(TEXT("wrong SuiYan click is blocked during HengJian step"),
		Fixture.Board->ClickCardInHand(SuiYan));

	const FGameXXKCardCombatUnit* HeroBefore = FindUnit(*Fixture.Runtime, TEXT("Player"));
	const int32 HeroArmorBefore = HeroBefore ? HeroBefore->Armor : 0;
	TestTrue(TEXT("HengJian card can be selected"), Fixture.Board->ClickCardInHand(HengJian));
	TestTrue(TEXT("HengJian commits only on hero"), Fixture.Board->ConfirmTargetingUnit(TEXT("Player")));
	double PresentationClock = 0.0;
	DrainBoard(*Fixture.Board, PresentationClock);
	const FGameXXKCardCombatUnit* HeroAfter = FindUnit(*Fixture.Runtime, TEXT("Player"));
	TestTrue(TEXT("HengJian increases hero armor"),
		HeroAfter && HeroAfter->Armor > HeroArmorBefore);
	TestEqual(TEXT("HengJian event advances to SuiYan"),
		Fixture.Session->GetMutableGuideProgress().ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.SuiYan")));

	const FGameXXKCardCombatUnit* EnemyBefore =
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate(
			[](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Enemy;
			});
	const FName EnemyId = EnemyBefore ? EnemyBefore->UnitId : NAME_None;
	const int32 EnemyHpBefore = EnemyBefore ? EnemyBefore->HP : 0;
	TestTrue(TEXT("SuiYan card can be selected"), Fixture.Board->ClickCardInHand(SuiYan));
	TestTrue(TEXT("SuiYan commits only on enemy"), Fixture.Board->ConfirmTargetingUnit(EnemyId));
	DrainBoard(*Fixture.Board, PresentationClock);
	const FGameXXKCardCombatUnit* EnemyAfter = FindUnit(*Fixture.Runtime, EnemyId);
	TestTrue(TEXT("SuiYan reduces enemy HP"), EnemyAfter && EnemyAfter->HP < EnemyHpBefore);
	TestEqual(TEXT("SuiYan event advances to FengShen"),
		Fixture.Session->GetMutableGuideProgress().ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.FengShen")));

	TestTrue(TEXT("FengShen card can be selected"), Fixture.Board->ClickCardInHand(FengShen));
	TestTrue(TEXT("FengShen commits on hero"), Fixture.Board->ConfirmTargetingUnit(TEXT("Player")));
	DrainBoard(*Fixture.Board, PresentationClock);
	const FGameXXKPendingCardChoice& Pending =
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Deck.PendingChoice;
	TestEqual(TEXT("FengShen opens forced discard"),
		Pending.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestTrue(TEXT("forced-discard target registers only when opened"),
		Registry.IsTargetRegistered(TEXT("Battle.Pending.ForcedDiscard")));
	if (!TestFalse(TEXT("forced discard exposes a candidate"), Pending.Candidates.IsEmpty()))
	{
		return false;
	}
	const FName DiscardedId = Pending.Candidates[0].InstanceId;
	TestTrue(TEXT("player can submit one forced discard"),
		Fixture.Board->SubmitPendingForcedDiscard(DiscardedId));
	DrainBoard(*Fixture.Board, PresentationClock);
	TestFalse(TEXT("chosen card leaves the hand"),
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate(
			[DiscardedId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == DiscardedId;
			}));
	TestEqual(TEXT("discard event advances to EndTurn"),
		Fixture.Session->GetMutableGuideProgress().ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.EndTurn")));

	TestTrue(TEXT("guided EndTurn succeeds"), Fixture.Board->EndCardPlayerPhase());
	TMap<FName, int32> ManaAtEnemyStart;
	for (const FGameXXKCardCombatUnit& Unit :
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party)
		{
			ManaAtEnemyStart.Add(Unit.UnitId, Unit.Mana);
		}
	}
	for (int32 Cycle = 0; Cycle < 64
		&& Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Phase
			!= EGameXXKCardBattlePhase::Player; ++Cycle)
	{
		DrainBoard(*Fixture.Board, PresentationClock);
	}
	TestEqual(TEXT("enemy phase returns to player"),
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Player);
	TestEqual(TEXT("next player turn advances to AutoBattle"),
		Fixture.Session->GetMutableGuideProgress().ActiveGuideStepId,
		FName(TEXT("Guide.Battle.Tutorial01.AutoBattle")));
	for (const FGameXXKCardCombatUnit& Unit :
		Fixture.Runtime->GetRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party
			&& ManaAtEnemyStart.Contains(Unit.UnitId))
		{
			TestEqual(TEXT("living party unit restores exactly two clamped mana"),
				Unit.Mana,
				FMath::Min(Unit.MaxMana, ManaAtEnemyStart.FindChecked(Unit.UnitId) + 2));
		}
	}
	TestTrue(TEXT("guided AutoBattle toggle succeeds"),
		Fixture.Board->SetAutoBattleEnabled(true));
	TestTrue(TEXT("AutoBattle is enabled authoritatively"), Fixture.Board->IsAutoBattleEnabled());
	TestTrue(TEXT("AutoBattle event completes the Guide"),
		Fixture.Session->GetMutableGuideProgress().ActiveGuideId.IsNone());
	TestFalse(TEXT("completed Guide releases action gate"), Registry.HasActionGate());
	TestEqual(TEXT("authoritative chain emits no Guide failure"), FailureCount, 0);

	Fixture.Board->SetAutoBattleEnabled(false);
	Fixture.Board->CancelTutorial01Guide();
	Fixture.Board->CancelBattleVisualSession(701);
	Registry.Reset();
	return true;
}

#endif
