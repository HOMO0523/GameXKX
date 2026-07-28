#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "Blueprint/WidgetTree.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleAnimationLayerWidget.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	/**
	 * Preserve the actual route node entered by this test, but choose a deterministic opening hand
	 * which contains an affordable manually-targeted enemy card.  The card runtime remains the
	 * source of truth; the legacy projection is only synchronized after the fixture is configured.
	 */
	bool BuildCardVictoryFixture(
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
			OutError = TEXT("The test subsystem is missing.");
			return false;
		}

		for (int32 Seed = 1; Seed <= 256; ++Seed)
		{
			FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
			FString Error;
			if (!FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, Seed, &Error))
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
				if (!EnemyCandidate)
				{
					continue;
				}

				FGameXXKCardCombatUnit* Enemy = State.CardRun.ActiveBattle.Units.FindByPredicate([EnemyCandidate](const FGameXXKCardCombatUnit& Unit)
				{
					return Unit.UnitId == EnemyCandidate->UnitId;
				});
				if (!Enemy)
				{
					continue;
				}

				Enemy->HP = 1;
				if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &Error))
				{
					OutError = Error;
					return false;
				}
				OutCardInstanceId = CardInstance.InstanceId;
				OutTargetUnitId = EnemyCandidate->UnitId;
				OutOwnerUnitId = Preview.OwnerUnitId;
				return true;
			}
		}

		OutError = TEXT("No affordable manual enemy-target card was found in the route fixture.");
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleBoardWidgetTest,
	"GameXXK.MVP.Battle.BoardWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleBoardWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("new game starts"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan can be selected"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("quest can be accepted"), Subsystem->AcceptQuest());
	TestTrue(TEXT("accepted quest enters route map"), Subsystem->OpenDungeonFromTownExit());
	TestTrue(TEXT("start node advances to battle route node"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start));
	TestTrue(TEXT("battle node opens battle screen"), Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle));
	TestEqual(TEXT("battle screen is active"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);

	AGameXXKMVPHUD* HUD = NewObject<AGameXXKMVPHUD>();
	HUD->SetMVPSubsystemForTest(Subsystem);
	UGameXXKBattleBoardWidget* BattleWidget = HUD->CreateBattleBoardWidgetForTest();
	TestNotNull(TEXT("HUD creates battle board widget"), BattleWidget);
	TestTrue(TEXT("HUD retains battle board widget"), HUD->HasBattleBoardWidget());
	TestEqual(TEXT("battle widget receives same MVP subsystem"), BattleWidget ? BattleWidget->GetMVPSubsystem() : nullptr, Subsystem);

	TestTrue(TEXT("battle board initializes widget tree"), BattleWidget->Initialize());
	BattleWidget->NativeConstruct();
	BattleWidget->RefreshFromState();
	UGameXXKBattlePartyQiWidget* PartyQiWidget = BattleWidget->WidgetTree
		? Cast<UGameXXKBattlePartyQiWidget>(BattleWidget->WidgetTree->FindWidget(TEXT("BattlePartyQiWidget")))
		: nullptr;
	TestNotNull(TEXT("battle board owns its shared Party Qi widget"), PartyQiWidget);
	TestEqual(TEXT("battle board exposes its shared Party Qi through the probe seam"), BattleWidget->GetPartyQiWidgetForTest(), PartyQiWidget);
	TestNotNull(TEXT("battle board owns its projected unit HUD layer"), BattleWidget->GetBattleProjectedUnitHudLayerForTest());
	TestTrue(TEXT("battle board projects at least the living card-runtime units"), BattleWidget->GetProjectedUnitHudCountForTest() > 0);
	TestNotNull(TEXT("battle board exposes its active hand container through the probe seam"), BattleWidget->GetHandCardBoxForTest());
	TestNotNull(TEXT("battle board exposes its end-turn control through the probe seam"), BattleWidget->GetEndTurnButtonForTest());
	if (PartyQiWidget)
	{
		TestEqual(TEXT("shared Party Qi reads the live opening card-runtime value"), PartyQiWidget->GetSharedQiForTest(), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy);
		TestEqual(TEXT("shared Party Qi is visible for an active card battle"), PartyQiWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	}
	TestTrue(TEXT("battle board remains active as a battle input/status layer"), BattleWidget->IsBattleBoardVisible());
	TestEqual(TEXT("battle board leaves enemies to scene actors instead of UMG cards"), BattleWidget->GetEnemySlotCount(), 0);
	TestEqual(TEXT("battle board leaves party members to scene actors instead of UMG cards"), BattleWidget->GetPartySlotCount(), 0);
	TestFalse(TEXT("battle board command menu starts hidden"), BattleWidget->IsCommandMenuVisibleForTest());
	TestFalse(TEXT("battle board starts outside targeting mode"), BattleWidget->IsTargetingBattleActionForTest());
	TestFalse(TEXT("active card battles do not reopen legacy action buttons from a party click"), BattleWidget->OpenCommandMenuForPartyUnit(0, FVector2D(1120.0f, 360.0f), FVector2D(1120.0f, 360.0f)));
	const FVector2D EmbeddedPieCanvasSize(1540.0f, 720.0f);
	const FVector2D BrokenEmbeddedProjection(660.0f, 420.0f);
	const FVector2D CorrectedCommandSource = BattleWidget->ResolveCommandSourcePositionForTest(1, BrokenEmbeddedProjection, BrokenEmbeddedProjection, EmbeddedPieCanvasSize);
	TestTrue(TEXT("battle board corrects embedded PIE party command source back to the right-side party lane"), CorrectedCommandSource.X >= EmbeddedPieCanvasSize.X * 0.72f);
	TestTrue(TEXT("battle board keeps corrected party command source inside the canvas"), CorrectedCommandSource.X <= EmbeddedPieCanvasSize.X - 12.0f);
	const FVector2D EmbeddedPieWidgetOrigin(96.0f, 90.0f);
	const FVector2D SlateCursorPosition(602.0f, 236.0f);
	const FVector2D LocalCursorPosition = BattleWidget->ResolveSlateAbsolutePositionToLocalForTest(SlateCursorPosition, EmbeddedPieWidgetOrigin, EmbeddedPieCanvasSize);
	TestEqual(TEXT("battle board converts Slate absolute cursor to widget-local targeting coordinates"), LocalCursorPosition, FVector2D(506.0f, 146.0f));
	const FVector2D ScaledWidgetAbsoluteOrigin(100.0f, 80.0f);
	const FVector2D ScaledWidgetAbsoluteSize(1600.0f, 800.0f);
	const FVector2D ScaledWidgetLocalSize(800.0f, 400.0f);
	const FVector2D ScaledSlateCursorPosition(900.0f, 480.0f);
	const FVector2D ScaledLocalCursorPosition = BattleWidget->ResolveSlateAbsolutePositionToLocalForTest(
		ScaledSlateCursorPosition,
		ScaledWidgetAbsoluteOrigin,
		ScaledWidgetAbsoluteSize,
		ScaledWidgetLocalSize);
	TestEqual(TEXT("battle board converts scaled Slate absolute cursor to widget-local targeting coordinates"), ScaledLocalCursorPosition, FVector2D(400.0f, 200.0f));
	TestTrue(TEXT("battle targeting arrow head asset is loaded"), BattleWidget->GetTargetingArrowHeadResourcePathForTest().Contains(TEXT("T_BattleTargetArrowHead")));
	TestEqual(TEXT("battle targeting uses all generated ink dab pieces"), BattleWidget->GetTargetingInkDabTextureCountForTest(), 12);
	TestEqual(TEXT("the active battle hand uses the readable enlarged PSD card size"), BattleWidget->GetCardFrameRuntimeSizeForTest(), FVector2D(225.0f, 257.0f));
	TestEqual(TEXT("PSD card frame stays un-tinted while ownership is carried by its strip"), BattleWidget->GetCardFrameTintForTest(), FLinearColor::White);
	TestEqual(TEXT("hero cards use the locked original-hero portrait asset"), BattleWidget->GetCardPortraitResourcePathForTest(TEXT("Hero.QingFengYiShi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero")));
	TestEqual(TEXT("NPC cards use their named original-art portrait asset"), BattleWidget->GetCardPortraitResourcePathForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief")));
	TestTrue(TEXT("card UI exposes at least one card from the active hand"), BattleWidget->GetVisibleHandCardCountForTest() > 0);

	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString CardFixtureError;
	TestTrue(FString::Printf(TEXT("battle board finds a deterministic manual card victory fixture: %s"), *CardFixtureError),
		BuildCardVictoryFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, CardFixtureError));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	const FVector2D OwnerScreenPosition(940.0f, 420.0f);
	BattleWidget->RefreshFromState();
	BattleWidget->RegisterBattleUnitScreenPosition(OwnerUnitId, OwnerScreenPosition);
	TestTrue(TEXT("clicking a hand card uses its CardCheck preview"), BattleWidget->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("manual card enters target-selection mode"), BattleWidget->IsCardTargetingForTest());
	TestEqual(TEXT("pending targeting retains the stable card instance id"), BattleWidget->GetPendingCardInstanceIdForTest(), CardInstanceId);
	TestEqual(TEXT("card targeting arrow begins at the registered owner actor"), BattleWidget->GetTargetingSourcePositionForTest(), OwnerScreenPosition);
	TestTrue(TEXT("only legal card candidate units are highlighted"), BattleWidget->IsTargetUnitHighlighted(TargetUnitId));
	TestFalse(TEXT("the card owner is not spuriously highlighted as an enemy target"), BattleWidget->IsTargetUnitHighlighted(OwnerUnitId));
	BattleWidget->UpdateTargetingPointer(FVector2D(520.0f, 360.0f));
	TestEqual(TEXT("the card targeting arrow endpoint follows the cursor"), BattleWidget->GetTargetingPointerPositionForTest(), FVector2D(520.0f, 360.0f));
	TestTrue(TEXT("committing a legal stable target resolves through the card adapter"), BattleWidget->ConfirmTargetingUnit(TargetUnitId));
	TestEqual(TEXT("victory waits on the battle screen for a reward decision"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("victory exposes the saved route reward offer"), BattleWidget->HasPendingRouteReward());
	TestEqual(TEXT("victory exposes exactly three reward cards"), BattleWidget->GetPendingRouteRewardCardIds().Num(), 3);
	TestEqual(TEXT("reward choice hides the spent battle hand"), BattleWidget->GetVisibleHandCardCountForTest(), 0);
	if (PartyQiWidget)
	{
		TestEqual(TEXT("pending route rewards hide the shared Party Qi widget"), PartyQiWidget->GetVisibility(), ESlateVisibility::Collapsed);
	}
	BattleWidget->QueueCombatAnimation(TEXT("Player"), false, TargetUnitId, true, false);
	TestTrue(TEXT("battle exit fixture owns an active transient animation"),
		BattleWidget->GetBattleAnimationLayerForTest()
		&& BattleWidget->GetBattleAnimationLayerForTest()->IsPresentationActiveForTest());
	TestTrue(TEXT("skipping the reward resolves the route victory gate"), BattleWidget->SkipPendingRouteReward());
	TestEqual(TEXT("reward resolution returns to dungeon route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("reward resolution advances route index"), Subsystem->GetRuntimeState().DungeonNodeIndex, 2);

	BattleWidget->RefreshFromState();
	TestFalse(TEXT("battle board hides outside battle screen"), BattleWidget->IsBattleBoardVisible());
	TestFalse(TEXT("leaving battle resets the transient animation layer"),
		BattleWidget->GetBattleAnimationLayerForTest()
		&& BattleWidget->GetBattleAnimationLayerForTest()->IsPresentationActiveForTest());
	TestEqual(TEXT("leaving battle discards queued transient cinematics"),
		BattleWidget->GetBattleAnimationLayerForTest()
			? BattleWidget->GetBattleAnimationLayerForTest()->GetQueuedSequenceCountForTest()
			: -1,
		0);
	if (PartyQiWidget)
	{
		TestEqual(TEXT("leaving battle hides the shared Party Qi widget"), PartyQiWidget->GetVisibility(), ESlateVisibility::Collapsed);
	}
	BattleWidget->SetMVPSubsystem(nullptr);
	BattleWidget->RefreshFromState();
	if (PartyQiWidget)
	{
		TestEqual(TEXT("missing card runtime hides the shared Party Qi widget"), PartyQiWidget->GetVisibility(), ESlateVisibility::Collapsed);
	}
	return true;
}

#endif
