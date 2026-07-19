#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleBoardWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeLegacyFooterTestEnemy()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("LegacyFooter.Enemy");
		Unit.DisplayName = FText::FromString(TEXT("旧页脚敌人"));
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Defense = 0;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildLegacyFooterRetirementFixture(
		UGameXXKMVPSubsystem* Subsystem,
		FName& OutCardInstanceId,
		FName& OutTargetUnitId,
		FName& OutOwnerUnitId,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		OutTargetUnitId = NAME_None;
		OutOwnerUnitId = NAME_None;
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The retirement test subsystem is missing.");
			return false;
		}

		for (int32 Seed = 1; Seed <= 256; ++Seed)
		{
			FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
			State = UGameXXKMVPRules::CreateNewGame();
			State.Screen = EGameXXKScreen::Battle;
			State.bHasActiveBattle = true;
			State.ActiveBattleNodeId = 47;
			State.ActiveBattleEnemies = {MakeLegacyFooterTestEnemy()};

			FString Error;
			if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
				|| !FGameXXKCardBattleAdapter::BeginCardBattle(
					State,
					EGameXXKNodeKind::Battle,
					EGameXXKCardTerrain::Plain,
					Seed,
					&Error))
			{
				OutError = Error;
				return false;
			}

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				FGameXXKCardPlayPreview Preview;
				if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, CardInstance.InstanceId, Preview, &Error)
					|| !Preview.bCanPlay
					|| !Preview.TargetRequest.bRequiresManualSelection)
				{
					continue;
				}

				const FGameXXKCardTargetCandidateView* EnemyCandidate = Preview.TargetRequest.CandidateViews.FindByPredicate([](const FGameXXKCardTargetCandidateView& Candidate)
				{
					return Candidate.bCanSelect && Candidate.Side == EGameXXKCardTargetSide::Enemy;
				});
				if (EnemyCandidate)
				{
					OutCardInstanceId = CardInstance.InstanceId;
					OutTargetUnitId = EnemyCandidate->UnitId;
					OutOwnerUnitId = Preview.OwnerUnitId;
					return true;
				}
			}
		}

		OutError = TEXT("No affordable manual enemy-target card was found in the retirement fixtures.");
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleLegacyFooterRetirementTest,
	"GameXXK.UI.Battle.LegacyFooterRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleLegacyFooterRetirementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("retirement fixture enters a deterministic card battle: %s"), *Error),
		BuildLegacyFooterRetirementFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Error));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("retirement board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestNull(
		TEXT("actor-owned HUD leaves no legacy BattleUnitFooter widget on the board"),
		Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleUnitFooter_00")) : nullptr);

	const FVector2D OwnerProjection(786.0f, 406.0f);
	const FVector2D PointerProjection(442.0f, 283.0f);
	Board->RegisterBattleUnitScreenPosition(OwnerUnitId, OwnerProjection);
	TestTrue(TEXT("manual card still enters arrow targeting after footer retirement"), Board->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("manual card remains in targeting state after footer retirement"), Board->IsCardTargetingActive());
	TestEqual(TEXT("registered actor projection remains the card-arrow source"), Board->GetTargetingSourcePositionForTest(), OwnerProjection);
	TestTrue(TEXT("legal enemy remains highlighted after footer retirement"), Board->IsTargetUnitHighlighted(TargetUnitId));
	Board->UpdateTargetingPointer(PointerProjection);
	TestEqual(TEXT("mouse pointer still drives the targeting arrow endpoint"), Board->GetTargetingPointerPositionForTest(), PointerProjection);
	Board->ClearBattleUnitScreenPositions();
	TestTrue(TEXT("clearing actor projections does not cancel the active card targeting state"), Board->IsCardTargetingActive());

	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	TestNotNull(TEXT("retirement controller can be constructed"), Controller);
	if (Controller)
	{
		TArray<UWidgetInteractionComponent*> HoverBridgeComponents;
		Controller->GetComponents(HoverBridgeComponents);
		TestEqual(TEXT("actor-owned screen HUD requires no world-widget interaction bridge"), HoverBridgeComponents.Num(), 0);
	}

	return true;
}

#endif
