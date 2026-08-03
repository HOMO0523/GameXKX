#include "GameXXKMVPRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKBattlePresentation.h"
#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/Image.h"
#include "Components/HorizontalBox.h"
#include "Components/ScaleBox.h"
#include "Components/ScaleBoxSlot.h"
#include "Components/SizeBox.h"
#include "Engine/Texture2D.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleAnimationLayerWidget.h"
#include "UI/GameXXKBattleAtlasCache.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UI/GameXXKMVPHUD.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	class FBoardAtlasLoadHandle final : public IGameXXKBattleAtlasLoadHandle
	{
	public:
		virtual void Cancel() override { bCancelled = true; }
		bool bCancelled = false;
	};

	class FBoardAtlasLoader final : public IGameXXKBattleAtlasLoader
	{
	public:
		virtual TSharedPtr<IGameXXKBattleAtlasLoadHandle> RequestAsyncLoad(
			const FSoftObjectPath& Path,
			FGameXXKAtlasLoaderCompletion Completion) override
		{
			RequestedPaths.Add(Path);
			Completions.Add(Path, MoveTemp(Completion));
			TSharedRef<FBoardAtlasLoadHandle> Handle = MakeShared<FBoardAtlasLoadHandle>();
			Handles.Add(Path, Handle);
			return Handle;
		}

		void CompleteMissing(const FSoftObjectPath& Path)
		{
			FGameXXKAtlasLoaderCompletion Completion;
			if (FGameXXKAtlasLoaderCompletion* Found = Completions.Find(Path))
			{
				Completion = MoveTemp(*Found);
				Completions.Remove(Path);
			}
			if (Completion)
			{
				Completion(nullptr, 0);
			}
		}

		void CompleteLoaded(const FSoftObjectPath& Path, UTexture2D* Texture, const int64 ResidentBytes = 4096)
		{
			FGameXXKAtlasLoaderCompletion Completion;
			if (FGameXXKAtlasLoaderCompletion* Found = Completions.Find(Path))
			{
				Completion = MoveTemp(*Found);
				Completions.Remove(Path);
			}
			if (Completion)
			{
				Completion(Texture, ResidentBytes);
			}
		}

		int32 GetRequestCount() const { return RequestedPaths.Num(); }
		const TArray<FSoftObjectPath>& GetRequestedPaths() const { return RequestedPaths; }

	private:
		TArray<FSoftObjectPath> RequestedPaths;
		TMap<FSoftObjectPath, FGameXXKAtlasLoaderCompletion> Completions;
		TMap<FSoftObjectPath, TSharedRef<FBoardAtlasLoadHandle>> Handles;
	};

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
			// SelectDungeonNode now begins the authoritative card battle itself. This
			// helper deliberately replaces that active runtime with a deterministic
			// seed fixture while preserving the entered route node and legacy units.
			State.CardRun.bHasActiveCardBattle = false;
			FString Error;
			if (!FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, Seed, &Error))
			{
				OutError = Error;
				return false;
			}

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				// Use a known damaging, one-Qi, manually targeted attack. Selecting any
				// manual enemy card can pick a status-only action and is not a victory fixture.
				if (CardInstance.CardId != FName(TEXT("Hero.QingFengYiShi")))
				{
					continue;
				}
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

				for (FGameXXKCardCombatUnit& CandidateUnit : State.CardRun.ActiveBattle.Units)
				{
					if (CandidateUnit.Side == EGameXXKCardTargetSide::Enemy
						&& CandidateUnit.UnitId != EnemyCandidate->UnitId)
					{
						CandidateUnit.HP = 0;
						CandidateUnit.bLiving = false;
					}
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

	double SlateNow = 100.0;
	const TSharedRef<FBoardAtlasLoader> AtlasLoader = MakeShared<FBoardAtlasLoader>();
	BattleWidget->SetAtlasCacheForTest(MakeUnique<FGameXXKBattleAtlasCache>(
		AtlasLoader,
		[&SlateNow]() { return SlateNow; }));
	TestTrue(TEXT("battle board initializes widget tree"), BattleWidget->Initialize());
	BattleWidget->NativeConstruct();
	BattleWidget->RefreshFromState();
	TestTrue(TEXT("battle visual session accepts the real nonzero overlay token"), BattleWidget->BeginBattleVisualSession(7001));
	TestEqual(TEXT("battle visual session exposes the exact controller token"), BattleWidget->GetActiveBattleVisualSessionTokenForTest(), uint64(7001));
	TestEqual(TEXT("backdrop occupies canonical z zero"), BattleWidget->GetLayerZ(EGameXXKBattleHudLayer::Backdrop), 0);
	TestEqual(TEXT("formation occupies canonical z ten"), BattleWidget->GetLayerZ(EGameXXKBattleHudLayer::Formation), 10);
	TestEqual(TEXT("controls occupy canonical z twenty"), BattleWidget->GetLayerZ(EGameXXKBattleHudLayer::Controls), 20);

	UCanvasPanel* const ViewportRoot = BattleWidget->GetBattleViewportRootForTest();
	UCanvasPanel* const DesignStage = BattleWidget->GetBattleDesignStageForTest();
	UCanvasPanel* const ControlsLayer = BattleWidget->GetBattleControlsLayerForTest();
	TestNotNull(TEXT("battle board has one viewport root"), ViewportRoot);
	TestNotNull(TEXT("battle board has one common 1920 by 1080 design stage"), DesignStage);
	TestNotNull(TEXT("battle board has one controls container in the common stage"), ControlsLayer);
	TestTrue(TEXT("widget tree root is the viewport root"),
		BattleWidget->WidgetTree && BattleWidget->WidgetTree->RootWidget == ViewportRoot);
	TestTrue(TEXT("existing hand controls are owned by the controls layer"),
		BattleWidget->GetHandCardBoxForTest() && BattleWidget->GetHandCardBoxForTest()->GetParent() == ControlsLayer);
	TestTrue(TEXT("legacy animation layer remains in the common stage during migration"),
		BattleWidget->GetBattleAnimationLayerForTest()
		&& BattleWidget->GetBattleAnimationLayerForTest()->GetParent() == DesignStage);

	UScaleBox* const BackdropScale = BattleWidget->GetBattleBackdropScaleBoxForTest();
	UImage* const BackdropImage = BattleWidget->GetBattleBackdropImageForTest();
	TestNotNull(TEXT("viewport root owns an opaque battle backdrop"), BackdropImage);
	TestTrue(TEXT("backdrop is a full-viewport sibling behind the 16:9 safe stage"),
		BackdropScale
		&& BackdropScale->GetParent() == ViewportRoot
		&& Cast<UCanvasPanelSlot>(BackdropScale->Slot));
	const UWidget* const SafeStageWidget = DesignStage
		&& DesignStage->GetParent()
		? DesignStage->GetParent()->GetParent()
		: nullptr;
	const UCanvasPanelSlot* const BackdropRootSlot = BackdropScale
		? Cast<UCanvasPanelSlot>(BackdropScale->Slot)
		: nullptr;
	const UCanvasPanelSlot* const SafeStageRootSlot = SafeStageWidget
		? Cast<UCanvasPanelSlot>(SafeStageWidget->Slot)
		: nullptr;
	TestTrue(TEXT("backdrop fills the viewport root below the centered safe stage"),
		BackdropRootSlot
		&& SafeStageWidget
		&& SafeStageWidget->GetParent() == ViewportRoot
		&& SafeStageRootSlot
		&& BackdropRootSlot->GetAnchors().Minimum == FVector2D::ZeroVector
		&& BackdropRootSlot->GetAnchors().Maximum == FVector2D::UnitVector
		&& BackdropRootSlot->GetOffsets() == FMargin(0.0f)
		&& BackdropRootSlot->GetZOrder() < SafeStageRootSlot->GetZOrder());
	TestEqual(TEXT("backdrop uses centered scale-to-fill cropping"),
		BackdropScale ? BackdropScale->GetStretch() : EStretch::None,
		EStretch::ScaleToFill);
	TestEqual(TEXT("backdrop clips scaled overflow to the viewport root"),
		BackdropScale ? BackdropScale->GetClipping() : EWidgetClipping::Inherit,
		EWidgetClipping::ClipToBounds);
	const UScaleBoxSlot* const BackdropContentSlot = BackdropImage ? Cast<UScaleBoxSlot>(BackdropImage->Slot) : nullptr;
	TestTrue(TEXT("backdrop crop remains centered in both axes"),
		BackdropContentSlot
		&& BackdropContentSlot->GetHorizontalAlignment() == HAlign_Center
		&& BackdropContentSlot->GetVerticalAlignment() == VAlign_Center);
	TestTrue(TEXT("battle backdrop uses the approved generated riverside asset"),
		BattleWidget->GetBattleBackdropResourcePathForTest().Contains(TEXT("T_BattleArena_Riverside_GeneratedV1")));
	const UTexture2D* const BackdropTexture = BackdropImage
		? Cast<UTexture2D>(BackdropImage->GetBrush().GetResourceObject())
		: nullptr;
	TestEqual(TEXT("backdrop brush preserves the authored texture aspect for scale-to-fill"),
		BackdropImage ? BackdropImage->GetBrush().GetImageSize() : FVector2D::ZeroVector,
		BackdropTexture
			? FVector2D(static_cast<float>(BackdropTexture->GetSizeX()), static_cast<float>(BackdropTexture->GetSizeY()))
			: FVector2D::ZeroVector);

	TArray<FName> LivingDisplayUnitIds;
	for (const FGameXXKCardCombatUnit& Unit : Subsystem->GetRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && FGameXXKBattlePresentation::GetSlotNumber(
			Subsystem->GetRuntimeState().CardRun.ActiveBattle, Unit.UnitId) != INDEX_NONE)
		{
			LivingDisplayUnitIds.Add(Unit.UnitId);
		}
	}
	const int32 LivingDisplayUnitCount = LivingDisplayUnitIds.Num();
	TestEqual(TEXT("one persistent unit visual exists for every living display slot"),
		BattleWidget->GetUnitVisualCountForTest(), LivingDisplayUnitCount);
	TestEqual(TEXT("idle atlases are requested asynchronously only for living display slots"),
		AtlasLoader->GetRequestCount(), LivingDisplayUnitCount);
	TestEqual(TEXT("pin-before-acquire records one visible pin per requested idle"),
		BattleWidget->GetPinnedBattleAtlasCountForTest(), LivingDisplayUnitCount);
	if (LivingDisplayUnitCount > 0 && AtlasLoader->GetRequestedPaths().Num() > 0)
	{
		const FName FirstUnitId = LivingDisplayUnitIds[0];
		UGameXXKBattleUnitVisualWidget* const Visual = BattleWidget->GetUnitVisualForTest(FirstUnitId);
		TestNotNull(TEXT("first active unit has a persistent visual"), Visual);
		TestNotNull(TEXT("first active unit has a stable target proxy while its atlas is pending"),
			BattleWidget->GetUnitTargetProxyForTest(FirstUnitId));
		TestTrue(TEXT("pending idle shows a selectable placeholder"),
			BattleWidget->IsUnitTargetPlaceholderVisibleForTest(FirstUnitId));
		UTexture2D* const LoadedIdleAtlas = NewObject<UTexture2D>(BattleWidget);
		AtlasLoader->CompleteLoaded(AtlasLoader->GetRequestedPaths()[0], LoadedIdleAtlas);
		TestEqual(TEXT("a successful asynchronous idle request binds the canonical atlas to the persistent visual"),
			Visual ? Visual->GetAtlasForTest() : nullptr,
			LoadedIdleAtlas);
		TestFalse(TEXT("a loaded idle hides its loading placeholder"),
			BattleWidget->IsUnitTargetPlaceholderVisibleForTest(FirstUnitId));
		BattleWidget->AdvanceVisualsAtRealTime(SlateNow);
		TestEqual(TEXT("paused-world idle begins from atlas frame zero on the first Slate clock sample"),
			Visual ? Visual->GetCurrentFrameForTest() : INDEX_NONE,
			0);
		SlateNow += 7.0 / 12.0;
		BattleWidget->AdvanceVisualsAtRealTime(SlateNow);
		TestEqual(TEXT("paused-world idle advances from Slate absolute time without world delta"),
			Visual ? Visual->GetCurrentFrameForTest() : INDEX_NONE,
			7);
		BattleWidget->RefreshFromState();
		SlateNow += 1.0 / 12.0;
		BattleWidget->AdvanceVisualsAtRealTime(SlateNow);
		TestEqual(TEXT("an ordinary HUD refresh preserves the persistent idle epoch and advances to the next frame"),
			Visual ? Visual->GetCurrentFrameForTest() : INDEX_NONE,
			8);

		if (LivingDisplayUnitCount > 1 && AtlasLoader->GetRequestedPaths().Num() > 1)
		{
			const FName MissingUnitId = LivingDisplayUnitIds[1];
			AtlasLoader->CompleteMissing(AtlasLoader->GetRequestedPaths()[1]);
			TestTrue(TEXT("missing idle keeps the stable placeholder selectable"),
				BattleWidget->IsUnitTargetPlaceholderVisibleForTest(MissingUnitId)
				&& BattleWidget->GetUnitTargetProxyForTest(MissingUnitId)->GetIsEnabled());
			TestEqual(TEXT("missing idle releases only its own visibility pin"),
				BattleWidget->GetPinnedBattleAtlasCountForTest(), LivingDisplayUnitCount - 1);
		}
	}
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
	UGameXXKBattleUnitVisualWidget* const OwnerVisual = BattleWidget->GetUnitVisualForTest(OwnerUnitId);
	TestNotNull(TEXT("card owner keeps its persistent formation visual"), OwnerVisual);
	const FVector2D FixedOwnerStageCenter = OwnerVisual
		? OwnerVisual->GetStageCenter()
		: FVector2D(0.755f * 1920.0f, 0.52f * 1080.0f);
	BattleWidget->CancelBattleVisualSession(7001);
	TestNull(TEXT("card-targeting fallback fixture removes the persistent visual"),
		BattleWidget->GetUnitVisualForTest(OwnerUnitId));
	BattleWidget->RegisterBattleUnitScreenPosition(OwnerUnitId, OwnerScreenPosition);
	TestTrue(TEXT("clicking a hand card uses its CardCheck preview"), BattleWidget->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("manual card enters target-selection mode"), BattleWidget->IsCardTargetingForTest());
	TestEqual(TEXT("pending targeting retains the stable card instance id"), BattleWidget->GetPendingCardInstanceIdForTest(), CardInstanceId);
	TestEqual(TEXT("card targeting without a visual begins at the fixed card-runtime stage center, never actor projection"),
		BattleWidget->GetTargetingSourcePositionForTest(),
		FixedOwnerStageCenter);
	BattleWidget->RegisterBattleUnitScreenPosition(OwnerUnitId, FVector2D(14.0f, 17.0f));
	TestEqual(TEXT("legacy scene projection cannot overwrite active card targeting after its visual is removed"),
		BattleWidget->GetTargetingSourcePositionForTest(),
		FixedOwnerStageCenter);
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
	BattleWidget->CancelBattleVisualSession(7001);
	TestEqual(TEXT("battle visual cancellation invalidates the board token first"),
		BattleWidget->GetActiveBattleVisualSessionTokenForTest(), uint64(0));
	TestEqual(TEXT("battle visual cancellation drops every Board-held atlas pin"),
		BattleWidget->GetPinnedBattleAtlasCountForTest(), 0);
	TestEqual(TEXT("battle visual cancellation clears the active visual registry"),
		BattleWidget->GetUnitVisualCountForTest(), 0);
	return true;
}

#endif
