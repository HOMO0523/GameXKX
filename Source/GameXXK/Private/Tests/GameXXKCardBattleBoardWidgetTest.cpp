#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteCardRecipe.h"
#include "GameXXKRunDeckRules.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "Math/Box2D.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKSaveMigration.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattlePartyQiWidget.h"

#include <type_traits>
#include <utility>

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool RectanglesOverlap(const FBox2D& First, const FBox2D& Second)
	{
		return First.bIsValid
			&& Second.bIsValid
			&& First.Min.X < Second.Max.X
			&& First.Max.X > Second.Min.X
			&& First.Min.Y < Second.Max.Y
			&& First.Max.Y > Second.Min.Y;
	}

	template <typename TBoard, typename = void>
	struct THasPartyQiLayoutResolver : std::false_type
	{
	};

	template <typename TBoard>
	struct THasPartyQiLayoutResolver<TBoard, std::void_t<decltype(std::declval<const TBoard&>().ResolvePartyQiLayoutForTest(std::declval<FVector2D>()))>> : std::true_type
	{
	};

	template <typename TBoard>
	bool AssertPartyQiResponsiveLayout(FAutomationTestBase& Test, const TBoard* Board)
	{
		if (!Board)
		{
			Test.AddError(TEXT("The battle board is missing while validating Party Qi layout."));
			return false;
		}

		if constexpr (!THasPartyQiLayoutResolver<TBoard>::value)
		{
			Test.AddError(TEXT("The Board-owned Party Qi responsive-layout test seam has not been implemented."));
			return false;
		}
		else
		{
			const auto CompactLayout = Board->ResolvePartyQiLayoutForTest(FVector2D(1280.0f, 722.0f));
			Test.TestTrue(TEXT("compact Party Qi layout has a valid Qi rectangle"), CompactLayout.QiRect.bIsValid);
			Test.TestTrue(TEXT("compact Party Qi layout has a valid expanded-hand safety rectangle"), CompactLayout.ExpandedHandRect.bIsValid);
			Test.TestTrue(TEXT("compact Party Qi layout has a valid end-turn rectangle"), CompactLayout.EndTurnRect.bIsValid);
			Test.TestTrue(TEXT("compact Party Qi layout moves above the expanded hand safety envelope"), CompactLayout.bUsesHandSafeFallback);
			Test.TestFalse(TEXT("compact Party Qi never overlaps the five-card expanded hand envelope"), RectanglesOverlap(CompactLayout.QiRect, CompactLayout.ExpandedHandRect));
			Test.TestFalse(TEXT("compact Party Qi never overlaps end turn"), RectanglesOverlap(CompactLayout.QiRect, CompactLayout.EndTurnRect));
			Test.TestTrue(TEXT("compact Party Qi keeps the required 12-unit clearance above the expanded hand envelope"), CompactLayout.ExpandedHandRect.Min.Y - CompactLayout.QiRect.Max.Y >= 12.0f);

			const auto WideLayout = Board->ResolvePartyQiLayoutForTest(FVector2D(1920.0f, 1080.0f));
			Test.TestTrue(TEXT("wide Party Qi layout has a valid Qi rectangle"), WideLayout.QiRect.bIsValid);
			Test.TestFalse(TEXT("wide Party Qi retains the right action rail when it is clear of the hand"), WideLayout.bUsesHandSafeFallback);
			Test.TestFalse(TEXT("wide Party Qi never overlaps the five-card expanded hand envelope"), RectanglesOverlap(WideLayout.QiRect, WideLayout.ExpandedHandRect));
			Test.TestFalse(TEXT("wide Party Qi never overlaps end turn"), RectanglesOverlap(WideLayout.QiRect, WideLayout.EndTurnRect));

			const auto InitialGeometryLayout = Board->ResolvePartyQiLayoutForTest(FVector2D::ZeroVector);
			Test.TestTrue(TEXT("zero-geometry Party Qi layout uses the conservative hand-safe fallback"), InitialGeometryLayout.bUsesHandSafeFallback);
			Test.TestFalse(TEXT("zero-geometry Party Qi layout defers a canvas rectangle until geometry exists"), InitialGeometryLayout.QiRect.bIsValid);
			Test.TestTrue(TEXT("zero-geometry Party Qi layout keeps the rail above the expanded-hand fallback plane"), InitialGeometryLayout.SlotOffsets.Top <= -444.0f);
			return true;
		}
	}

	FGameXXKBattleRuntimeUnit MakeEnemy(const TCHAR* Id, const TCHAR* DisplayName)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(Id);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Defense = 0;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildManualTargetCardFixture(
		UGameXXKMVPSubsystem* Subsystem,
		FName& OutCardInstanceId,
		FName& OutTargetUnitId,
		FName& OutOwnerUnitId,
		FGameXXKCardPlayPreview& OutPreview,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		OutTargetUnitId = NAME_None;
		OutOwnerUnitId = NAME_None;
		OutPreview = FGameXXKCardPlayPreview();
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The test subsystem is missing.");
			return false;
		}

		for (int32 Seed = 1; Seed <= 256; ++Seed)
		{
			FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
			State = UGameXXKMVPRules::CreateNewGame();
			State.Screen = EGameXXKScreen::Battle;
			State.bHasActiveBattle = true;
			State.ActiveBattleNodeId = 17;
			State.ActiveBattleEnemies = {MakeEnemy(TEXT("MoneyRat"), TEXT("钱鼠"))};

			FString Error;
			if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
				|| !FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, Seed, &Error))
			{
				OutError = Error;
				return false;
			}

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				if (CardInstance.CardId != FName(TEXT("Hero.QingFengYiShi")))
				{
					continue;
				}

				FGameXXKCardPlayPreview Preview;
				if (!FGameXXKCardBattleAdapter::BuildCardPlayPreview(State, CardInstance.InstanceId, Preview, &Error)
					|| !Preview.bCanPlay
					|| Preview.EffectiveEnergyCost <= 0
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
					OutPreview = Preview;
					return true;
				}
			}
		}

		OutError = TEXT("No affordable manual enemy-target card was found in the deterministic opening-hand fixtures.");
		return false;
	}

	bool BuildRouteRewardFixture(
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
		if (!Subsystem
			|| !Subsystem->StartGame()
			|| !Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan())
			|| !Subsystem->AcceptQuest()
			|| !Subsystem->OpenDungeonFromTownExit()
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Start)
			|| !Subsystem->SelectDungeonNode(EGameXXKNodeKind::Battle))
		{
			OutError = TEXT("The route fixture could not enter its first card battle.");
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		FString Error;
		if (!State.CardRun.bHasActiveCardBattle)
		{
			OutError = TEXT("The selected route node did not create an active card battle.");
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

		OutError = Error.IsEmpty()
			? TEXT("No manual enemy-target card was available in the active route battle.")
			: Error;
		return false;
	}

	const FName PlayerUnitId(TEXT("Player"));
	const FName MergeRewardCardId(TEXT("Route.General.PoJiaTuCi"));
	const FName ReplacementRewardCardId(TEXT("Route.Rare.GuJuanCanZhang"));
	const FName AlternateReplacementRewardCardId(TEXT("Route.Rare.TieYiYiJue"));
	const FName DuplicateReplacementCardId(TEXT("Route.General.TuNaJue"));

	bool AddCapacityEntry(
		FGameXXKRuntimeState& State,
		const FName CardId,
		const EGameXXKCardQuality Quality)
	{
		FGameXXKRouteCardEntry Entry;
		Entry.CardId = CardId;
		Entry.CurrentQuality = Quality;
		Entry.SourceKind = EGameXXKRouteCardSourceKind::RouteReward;
		Entry.OwnerUnitId = PlayerUnitId;
		Entry.bTemporaryRouteCard = true;
		Entry.bConsumesRouteCapacity = true;
		Entry.AcquisitionOrdinal = State.CardRun.NextRouteCardEntryOrdinal;
		if (!FGameXXKRouteCardRecipe::MakeStableEntryId(
			State.CardRun.RouteProgress.RootSeed,
			Entry.AcquisitionOrdinal,
			Entry.EntryId))
		{
			return false;
		}
		State.CardRun.RouteCardEntries.Add(MoveTemp(Entry));
		++State.CardRun.NextRouteCardEntryOrdinal;
		return true;
	}

	int32 CountCapacityEntries(const FGameXXKRuntimeState& State)
	{
		int32 Count = 0;
		for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
		{
			Count += Entry.bConsumesRouteCapacity ? 1 : 0;
		}
		return Count;
	}

	bool FillRewardEntryCapacity(FGameXXKRuntimeState& State)
	{
		if (!AddCapacityEntry(State, MergeRewardCardId, EGameXXKCardQuality::Common)
			|| !AddCapacityEntry(State, DuplicateReplacementCardId, EGameXXKCardQuality::Epic)
			|| !AddCapacityEntry(State, DuplicateReplacementCardId, EGameXXKCardQuality::Epic))
		{
			return false;
		}

		const TSet<FName> ExcludedCardIds = {
			MergeRewardCardId,
			ReplacementRewardCardId,
			AlternateReplacementRewardCardId,
			DuplicateReplacementCardId};
		for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
		{
			if (CountCapacityEntries(State) >= FGameXXKRunDeckRules::MaxRouteCardCapacity)
			{
				break;
			}
			if (Definition.Owner == EGameXXKCardOwner::Route && !ExcludedCardIds.Contains(Definition.Id))
			{
				if (!AddCapacityEntry(State, Definition.Id, EGameXXKCardQuality::Epic))
				{
					return false;
				}
			}
		}
		return CountCapacityEntries(State) == FGameXXKRunDeckRules::MaxRouteCardCapacity;
	}

	void SetMixedFullCapacityOffer(FGameXXKRuntimeState& State, const bool bLegacyReplacementFlag)
	{
		State.CardRun.PendingReward.CardIds = {
			MergeRewardCardId,
			ReplacementRewardCardId,
			AlternateReplacementRewardCardId};
		State.CardRun.PendingReward.bRequiresRouteCardReplacement = bLegacyReplacementFlag;
	}

	bool RuntimeStatesEqual(const FGameXXKRuntimeState& Left, const FGameXXKRuntimeState& Right)
	{
		return FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Left, &Right, PPF_None);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardWidgetTest,
	"GameXXK.Integration.CardBattle.BoardTargeting",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FGameXXKCardPlayPreview CardPreview;
	FString Error;
	TestTrue(FString::Printf(TEXT("card board fixture opens a deterministic card battle: %s"), *Error),
		BuildManualTargetCardFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, CardPreview, Error));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("card battle board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	UGameXXKBattlePartyQiWidget* PartyQiWidget = Board->WidgetTree
		? Cast<UGameXXKBattlePartyQiWidget>(Board->WidgetTree->FindWidget(TEXT("BattlePartyQiWidget")))
		: nullptr;
	TestNotNull(TEXT("card battle board owns one named Party Qi widget"), PartyQiWidget);
	TestTrue(TEXT("card battle board exposes the Party Qi responsive-layout seam"), AssertPartyQiResponsiveLayout(*this, Board));
	if (!PartyQiWidget)
	{
		return false;
	}
	const int32 InitialSharedQi = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestEqual(TEXT("Party Qi reads the initial authoritative card-runtime shared energy"), PartyQiWidget->GetSharedQiForTest(), InitialSharedQi);
	TestEqual(TEXT("Party Qi overlays the initial authoritative card-runtime shared energy number"), PartyQiWidget->GetDisplayTextForTest(), FString::FromInt(InitialSharedQi));
	TestEqual(TEXT("Party Qi stays visible during the player card phase"), PartyQiWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("Board-embedded Party Qi remains input-transparent"), PartyQiWidget->AreContentWidgetsHitTestTransparentForTest());
	UCanvasPanelSlot* PartyQiCanvasSlot = Cast<UCanvasPanelSlot>(PartyQiWidget->Slot);
	TestNotNull(TEXT("Party Qi is attached to the Board root canvas"), PartyQiCanvasSlot);
	if (PartyQiCanvasSlot)
	{
		const FAnchors PartyQiAnchors = PartyQiCanvasSlot->GetAnchors();
		TestEqual(TEXT("Party Qi keeps a right/bottom anchor minimum"), PartyQiAnchors.Minimum, FVector2D(1.0f, 1.0f));
		TestEqual(TEXT("Party Qi keeps a right/bottom anchor maximum"), PartyQiAnchors.Maximum, FVector2D(1.0f, 1.0f));
		TestEqual(TEXT("Party Qi renders above the Board action rail"), PartyQiCanvasSlot->GetZOrder(), 35);
	}
	TestEqual(TEXT("player hand cards retain a readable 1.5x PSD frame at runtime"), Board->GetCardFrameRuntimeSizeForTest(), FVector2D(225.0f, 257.0f));
	TestEqual(TEXT("approved PSD card frame remains un-tinted"), Board->GetCardFrameTintForTest(), FLinearColor::White);
	TestEqual(TEXT("hero cards resolve the original hero card portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Hero.QingFengYiShi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero")));
	TestEqual(TEXT("task NPC cards resolve their locked named-NPC portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief")));
	TestEqual(TEXT("profession cards resolve the shared role portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Profession.Blade.LieFengZhan")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade.T_CardPortrait_Role_Blade")));
	TestEqual(TEXT("general route cards resolve their shared ink-command crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.General.PoJiaTuCi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_General.T_CardPortrait_Route_General")));
	TestEqual(TEXT("terrain route cards resolve their shared landscape crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Terrain.DuanYaLuoShi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Terrain.T_CardPortrait_Route_Terrain")));
	TestEqual(TEXT("rare route cards resolve their shared relic crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Rare.TieYiYiJue")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Rare.T_CardPortrait_Route_Rare")));
	TestEqual(TEXT("boss route cards resolve their shared battle-gong crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Boss.FuHuDuanJiang")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Boss.T_CardPortrait_Route_Boss")));
	int32 RouteDefinitionCount = 0;
	for (const FGameXXKCardDefinition& Definition : FGameXXKCardCatalog::GetAllCardDefinitions())
	{
		if (Definition.Owner != EGameXXKCardOwner::Route)
		{
			continue;
		}
		++RouteDefinitionCount;
		const FString RoutePortraitPath = Board->GetCardPortraitResourcePathForTest(Definition.Id);
		TestFalse(FString::Printf(TEXT("route card %s has category portrait art"), *Definition.Id.ToString()), RoutePortraitPath.IsEmpty());
		TestTrue(FString::Printf(TEXT("route card %s remains in the approved PartyDeck card-art root"), *Definition.Id.ToString()), RoutePortraitPath.StartsWith(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_")));
	}
	TestEqual(TEXT("all thirty route-card definitions resolve a non-empty category portrait"), RouteDefinitionCount, 30);
	TestEqual(TEXT("hero lower information strip uses pale parchment"), Board->GetCardInfoStripTintForTest(TEXT("Hero.QingFengYiShi")), FLinearColor(0.945f, 0.894f, 0.800f, 1.0f));
	TestEqual(TEXT("blade lower information strip uses cinnabar only"), Board->GetCardInfoStripTintForTest(TEXT("Profession.Blade.LieFengZhan")), FLinearColor(0.714f, 0.282f, 0.247f, 1.0f));
	TestEqual(TEXT("task NPC lower information strip uses near-black"), Board->GetCardInfoStripTintForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FLinearColor(0.145f, 0.137f, 0.129f, 1.0f));
	TestTrue(TEXT("card battle board exposes the active five-card hand"), Board->GetVisibleHandCardCountForTest() > 0);
	USizeBox* FirstHandCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleHandCardSize_00"))) : nullptr;
	USizeBox* FirstIntentCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCardSize_00"))) : nullptr;
	TestNotNull(TEXT("player hand keeps a named PSD card-size box"), FirstHandCardSize);
	TestNotNull(TEXT("enemy intent keeps its independent compact card-size box"), FirstIntentCardSize);
	TestEqual(TEXT("player hand width is large enough after runtime DPI scaling"), FirstHandCardSize ? FirstHandCardSize->GetWidthOverride() : 0.0f, 225.0f);
	TestEqual(TEXT("player hand height preserves the PSD card ratio"), FirstHandCardSize ? FirstHandCardSize->GetHeightOverride() : 0.0f, 257.0f);
	TestEqual(TEXT("enemy intent width remains compact instead of inheriting the player hand size"), FirstIntentCardSize ? FirstIntentCardSize->GetWidthOverride() : 0.0f, 150.0f);
	TestEqual(TEXT("enemy intent height remains compact instead of inheriting the player hand size"), FirstIntentCardSize ? FirstIntentCardSize->GetHeightOverride() : 0.0f, 171.0f);

	UButton* FirstPlayerHandCard = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleHandCard_00"))) : nullptr;
	TestNotNull(TEXT("the first visible player hand card keeps its real hover target"), FirstPlayerHandCard);
	if (FirstPlayerHandCard)
	{
		FirstPlayerHandCard->OnHovered.Broadcast();
		TestTrue(TEXT("hovering a visible player hand card displays its shared effect tooltip"), Board->IsCardTooltipVisibleForTest());
		FirstPlayerHandCard->OnUnhovered.Broadcast();
		TestFalse(TEXT("leaving a visible player hand card hides the shared effect tooltip"), Board->IsCardTooltipVisibleForTest());
	}

	const FVector2D OwnerScreenPosition(940.0f, 420.0f);
	Board->RegisterBattleUnitScreenPosition(OwnerUnitId, OwnerScreenPosition);
	TestEqual(TEXT("shared-energy fixture uses the stable one-Qi manual hero card"), CardPreview.CardId, FName(TEXT("Hero.QingFengYiShi")));
	TestTrue(TEXT("shared-energy fixture records a positive authoritative effective energy cost"), CardPreview.EffectiveEnergyCost > 0);
	const int32 SharedQiBeforeCommit = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	const int32 EnemyHealthBeforePreview = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
	TestTrue(TEXT("clicking a playable hand card performs a non-mutating card preview"), Board->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("a manual card enters card-targeting mode"), Board->IsCardTargetingForTest());
	TestEqual(TEXT("the pending card keeps the stable instance id"), Board->GetPendingCardInstanceIdForTest(), CardInstanceId);
	TestEqual(TEXT("the arrow begins at the registered card owner position"), Board->GetTargetingSourcePositionForTest(), OwnerScreenPosition);
	TestTrue(TEXT("the preview marks its stable legal enemy target for highlight"), Board->IsTargetUnitHighlighted(TargetUnitId));
	TestFalse(TEXT("the preview does not make the owner a legal enemy-card target"), Board->IsTargetUnitHighlighted(OwnerUnitId));
	TestEqual(TEXT("previewing a card does not deal damage"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHealthBeforePreview);

	Board->UpdateTargetingPointer(FVector2D(520.0f, 360.0f));
	TestEqual(TEXT("the card targeting arrow endpoint follows the cursor"), Board->GetTargetingPointerPositionForTest(), FVector2D(520.0f, 360.0f));
	TestFalse(TEXT("a stable but non-highlighted unit cannot commit the card"), Board->ConfirmTargetingUnit(OwnerUnitId));
	TestTrue(TEXT("an invalid target keeps the card selection active"), Board->IsCardTargetingForTest());
	TestTrue(TEXT("right-click or Escape cancellation clears card-targeting state"), Board->CancelBattleTargeting());
	TestFalse(TEXT("cancel removes the card target highlights"), Board->IsTargetUnitHighlighted(TargetUnitId));
	TestTrue(TEXT("cancelling preserves the selected hand card"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == CardInstanceId;
	}));

	TestTrue(TEXT("the same stable card can be previewed again after cancellation"), Board->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("clicking a highlighted stable target commits through the card adapter"), Board->ConfirmTargetingUnit(TargetUnitId));
	const int32 SharedQiAfterCommit = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestEqual(TEXT("a successful card play subtracts exactly its authoritative effective shared-energy cost once"), SharedQiAfterCommit, SharedQiBeforeCommit - CardPreview.EffectiveEnergyCost);
	TestEqual(TEXT("Party Qi immediately projects the post-commit authoritative shared energy"), PartyQiWidget->GetSharedQiForTest(), SharedQiAfterCommit);
	TestEqual(TEXT("Party Qi immediately overlays the post-commit authoritative shared energy number"), PartyQiWidget->GetDisplayTextForTest(), FString::FromInt(SharedQiAfterCommit));
	TestFalse(TEXT("a committed card exits targeting state"), Board->IsCardTargetingForTest());
	TestFalse(TEXT("a committed card leaves the hand zone"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == CardInstanceId;
	}));
	TestTrue(TEXT("a committed enemy-target card updates the scene-facing health projection"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP < EnemyHealthBeforePreview);
	const int32 RoundBeforeEndTurn = Subsystem->GetRuntimeState().CardRun.ActiveBattle.RoundNumber;
	TestTrue(TEXT("end turn only starts the saved enemy-intent presentation"), Board->EndCardPlayerPhase());
	TestEqual(TEXT("end turn leaves control in the enemy phase until its presentation finishes"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestEqual(TEXT("Party Qi remains visible during the enemy intent phase"), PartyQiWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("Party Qi continues to read the runtime shared energy during the enemy intent phase"), PartyQiWidget->GetSharedQiForTest(), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy);
	TestEqual(TEXT("the saved enemy phase exposes its single top intent card"), Board->GetVisibleEnemyIntentCardCountForTest(), 1);
	TestEqual(TEXT("the first saved intent is the active showcase index"), Board->GetActiveEnemyIntentPresentationIndexForTest(), 0);
	TestEqual(TEXT("the first enemy intent keeps its separate fixed P-slot label"), Board->GetEnemyIntentSlotLabelForTest(0), FString(TEXT("敌 1P")));
	const FString FirstIntentTooltip = Board->GetEnemyIntentTooltipForTest(0);
	TestTrue(TEXT("the saved enemy intent tooltip identifies the direct central hero target"), FirstIntentTooltip.Contains(TEXT("我 2P")));
	TestTrue(TEXT("the saved enemy intent tooltip identifies the persisted base damage"), FirstIntentTooltip.Contains(TEXT("基础伤害")));
	UButton* FirstIntentCard = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCard_00"))) : nullptr;
	UBorder* EnemyIntentDetailPanel = Board->WidgetTree ? Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentDetailPanel"))) : nullptr;
	TestNotNull(TEXT("the existing enemy-intent card remains hoverable"), FirstIntentCard);
	TestNotNull(TEXT("the existing enemy-intent hover tooltip panel remains available"), EnemyIntentDetailPanel);
	if (FirstIntentCard && EnemyIntentDetailPanel)
	{
		FirstIntentCard->OnHovered.Broadcast();
		TestEqual(TEXT("enemy-intent hover keeps its custom input-transparent tooltip"), EnemyIntentDetailPanel->GetVisibility(), ESlateVisibility::HitTestInvisible);
		FirstIntentCard->OnUnhovered.Broadcast();
		TestEqual(TEXT("enemy-intent mouse leave still hides its custom tooltip"), EnemyIntentDetailPanel->GetVisibility(), ESlateVisibility::Collapsed);
	}
	UButton* EndTurnButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEndTurnButton"))) : nullptr;
	// End-player-phase moves the hand to discard before enemy intents are shown.
	// Therefore enemy intent cards are the only visible card type in this phase.
	TestFalse(TEXT("enemy intent display has no stale player hand card left interactive"), Board->IsHandCardSlotEnabledForTest(0));
	TestFalse(TEXT("enemy intent display locks end turn"), EndTurnButton && EndTurnButton->GetIsEnabled());
	Board->AdvanceEnemyIntentPresentationForTest(0.55f);
	Board->AdvanceEnemyIntentPresentationForTest(0.18f);
	TestEqual(TEXT("the only displayed intent resolves exactly once"), Subsystem->GetRuntimeState().CardRun.NextEnemyIntentIndex, 1);
	Board->AdvanceEnemyIntentPresentationForTest(0.32f);
	TestEqual(TEXT("enemy presentation starts the next player phase only after settling"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Player);
	const int32 SharedQiAtNextPlayerPhase = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestTrue(TEXT("the new player phase resets the shared current-turn energy in the authoritative runtime"), SharedQiAtNextPlayerPhase > SharedQiAfterCommit);
	TestEqual(TEXT("Party Qi reads the authoritative reset at the new player phase"), PartyQiWidget->GetSharedQiForTest(), SharedQiAtNextPlayerPhase);
	TestEqual(TEXT("Party Qi overlays the authoritative reset at the new player phase"), PartyQiWidget->GetDisplayTextForTest(), FString::FromInt(SharedQiAtNextPlayerPhase));
	TestTrue(TEXT("enemy presentation advances the persisted card round"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.RoundNumber > RoundBeforeEndTurn);
	TestTrue(TEXT("enemy presentation refreshes to the configured hand limit"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.Num() <= Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.HandLimit);

	FGameXXKBattleRuntimeUnit Enemy = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
	AGameXXKBattleSceneUnitActor* SceneUnit = NewObject<AGameXXKBattleSceneUnitActor>();
	SceneUnit->ConfigureFromRuntimeUnit(true, 0, Enemy);
	SceneUnit->SetCardTargetHighlight(true);
	TestTrue(TEXT("scene unit exposes the legal-target highlight state for the controller bridge"), SceneUnit->IsCardTargetHighlighted());
	TestTrue(TEXT("highlighted scene unit enables its visible outline channel"), SceneUnit->IsCardTargetOutlineEnabled());
	SceneUnit->SetCardTargetHighlight(false);
	TestFalse(TEXT("clearing a legal target disables the scene highlight"), SceneUnit->IsCardTargetHighlighted());

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardPendingInsightChoiceTest,
	"GameXXK.Integration.CardBattle.BoardPendingInsightChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardPendingInsightChoiceTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 23;
	State.ActiveBattleEnemies = {MakeEnemy(TEXT("MoneyRat"), TEXT("钱鼠"))};

	FString Error;
	TestTrue(FString::Printf(TEXT("pending-insight fixture initializes card run: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestTrue(FString::Printf(TEXT("pending-insight fixture begins card battle: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 317, &Error));
	FGameXXKBattleDeckState& Deck = State.CardRun.ActiveBattle.Deck;
	if (Deck.Hand.Num() >= Deck.HandLimit)
	{
		TestTrue(TEXT("pending-insight fixture frees one hand slot"),
			GameXXKCardRules::MoveHandCardToDiscard(Deck, Deck.Hand.Last().InstanceId, &Error));
	}
	TestTrue(FString::Printf(TEXT("pending-insight fixture opens a real insight choice: %s"), *Error),
		GameXXKCardRules::BeginInsight(Deck, 2, &Error));
	TestEqual(TEXT("fixture reaches the live blocking insight choice kind"),
		Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::InsightChooseToHand);
	TestEqual(TEXT("fixture exposes two visible candidate cards"), Deck.PendingChoice.Candidates.Num(), 2);

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("pending-insight board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	UButton* EndTurnButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEndTurnButton"))) : nullptr;
	TestNotNull(TEXT("pending-insight board retains the end-turn control"), EndTurnButton);
	TestFalse(TEXT("end turn is disabled while a mandatory insight choice is unresolved"), EndTurnButton && EndTurnButton->GetIsEnabled());

	UWidget* PendingPanel = Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePanel")) : nullptr;
	TestNotNull(TEXT("a blocking insight always exposes a named visible choice panel"), PendingPanel);
	UButton* FirstChoiceButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCard_00"))) : nullptr;
	TestNotNull(TEXT("each insight candidate is a clickable PSD-framed card"), FirstChoiceButton);
	if (!FirstChoiceButton)
	{
		return false;
	}
	const int32 InsightHandCountBeforeHover = Deck.Hand.Num();
	const int32 InsightCandidateCountBeforeHover = Deck.PendingChoice.Candidates.Num();
	FirstChoiceButton->OnHovered.Broadcast();
	TestTrue(TEXT("hovering an insight candidate reveals the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the shared battle card tooltip never intercepts input"), Board->IsCardTooltipHitTestInvisibleForTest());
	TestTrue(TEXT("the insight tooltip states the actual click result"), Board->GetCardTooltipTextForTest().Contains(TEXT("点击后加入手牌。")));
	TestEqual(TEXT("insight hover preserves the pending choice kind"), Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::InsightChooseToHand);
	TestEqual(TEXT("insight hover preserves candidate cards"), Deck.PendingChoice.Candidates.Num(), InsightCandidateCountBeforeHover);
	TestEqual(TEXT("insight hover preserves the hand"), Deck.Hand.Num(), InsightHandCountBeforeHover);
	FirstChoiceButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving an insight candidate immediately hides the shared card tooltip"), Board->IsCardTooltipVisibleForTest());

	FirstChoiceButton->OnClicked.Broadcast();
	TestEqual(TEXT("selecting an insight candidate clears the blocking runtime choice"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestTrue(TEXT("resolving insight restores the end-turn path"), Board->EndCardPlayerPhase());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardPendingInsightCancelTest,
	"GameXXK.Integration.CardBattle.BoardPendingInsightCancel",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardPendingInsightCancelTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 27;
	State.ActiveBattleEnemies = {MakeEnemy(TEXT("MoneyRat"), TEXT("钱鼠"))};

	FString Error;
	TestTrue(FString::Printf(TEXT("pending-insight-cancel fixture initializes card run: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestTrue(FString::Printf(TEXT("pending-insight-cancel fixture begins card battle: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 323, &Error));
	FGameXXKBattleDeckState& Deck = State.CardRun.ActiveBattle.Deck;
	TestTrue(TEXT("pending-insight-cancel fixture frees one hand slot"),
		GameXXKCardRules::MoveHandCardToDiscard(Deck, Deck.Hand.Last().InstanceId, &Error));
	TestTrue(FString::Printf(TEXT("pending-insight-cancel fixture opens a cancellable insight: %s"), *Error),
		GameXXKCardRules::BeginInsight(Deck, 2, &Error));
	TestTrue(TEXT("fixture marks insight as cancellable"), Deck.PendingChoice.bCanCancel);

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("pending-insight-cancel board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	UButton* CancelChoiceButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCancelButton"))) : nullptr;
	TestNotNull(TEXT("a cancellable insight exposes its explicit cancel control"), CancelChoiceButton);
	TestTrue(TEXT("the insight cancel control is visible"), CancelChoiceButton && CancelChoiceButton->GetVisibility() == ESlateVisibility::Visible);
	if (!CancelChoiceButton)
	{
		return false;
	}

	CancelChoiceButton->OnClicked.Broadcast();
	TestEqual(TEXT("cancelling insight clears the blocking runtime choice"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestEqual(TEXT("cancelling insight does not add an offered card to hand"), State.CardRun.ActiveBattle.Deck.Hand.Num(), 4);
	TestTrue(TEXT("cancelling insight restores the end-turn path"), Board->EndCardPlayerPhase());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardPendingForcedDiscardTest,
	"GameXXK.Integration.CardBattle.BoardPendingForcedDiscard",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardPendingForcedDiscardTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 29;
	State.ActiveBattleEnemies = {MakeEnemy(TEXT("MoneyRat"), TEXT("钱鼠"))};

	FString Error;
	TestTrue(FString::Printf(TEXT("pending-discard fixture initializes card run: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestTrue(FString::Printf(TEXT("pending-discard fixture begins card battle: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 331, &Error));
	FGameXXKBattleDeckState& Deck = State.CardRun.ActiveBattle.Deck;
	TestTrue(TEXT("pending-discard fixture frees one hand slot"),
		GameXXKCardRules::MoveHandCardToDiscard(Deck, Deck.Hand.Last().InstanceId, &Error));
	TestTrue(FString::Printf(TEXT("pending-discard fixture opens a real forced-discard choice: %s"), *Error),
		GameXXKCardRules::DrawCards(Deck, 2, 1, &Error));
	TestEqual(TEXT("fixture reaches the live blocking forced-discard choice kind"),
		Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("fixture requires exactly one selected discard"), Deck.PendingChoice.RequiredCount, 1);

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("pending-discard board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	UButton* EndTurnButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleEndTurnButton"))) : nullptr;
	TestNotNull(TEXT("pending-discard board retains the end-turn control"), EndTurnButton);
	TestFalse(TEXT("end turn is disabled while a mandatory forced discard is unresolved"), EndTurnButton && EndTurnButton->GetIsEnabled());

	UWidget* PendingPanel = Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePanel")) : nullptr;
	TestNotNull(TEXT("a forced discard exposes the same named blocking choice panel"), PendingPanel);
	TestTrue(TEXT("the forced-discard panel is actually visible to the player"),
		PendingPanel && PendingPanel->GetVisibility() == ESlateVisibility::Visible);
	UTextBlock* PendingPrompt = Board->WidgetTree
		? Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePrompt")))
		: nullptr;
	TestNotNull(TEXT("forced discard exposes a concrete instruction prompt"), PendingPrompt);
	TestTrue(TEXT("forced-discard prompt describes the declared card cost instead of a hand overflow"),
		PendingPrompt && PendingPrompt->GetText().ToString().Contains(TEXT("此牌要求弃置 1 张手牌")));
	TestFalse(TEXT("forced-discard prompt does not misreport the 20-card capacity as exceeded"),
		PendingPrompt && PendingPrompt->GetText().ToString().Contains(TEXT("超出上限")));
	UButton* FirstChoiceButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCard_00"))) : nullptr;
	TestNotNull(TEXT("each discardable hand card remains clickable in the pending-choice panel"), FirstChoiceButton);
	if (!FirstChoiceButton)
	{
		return false;
	}
	const int32 ForcedDiscardHandCountBeforeHover = Deck.Hand.Num();
	const int32 ForcedDiscardCandidateCountBeforeHover = Deck.PendingChoice.Candidates.Num();
	FirstChoiceButton->OnHovered.Broadcast();
	TestTrue(TEXT("hovering a forced-discard candidate reveals the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the forced-discard tooltip states the actual click result"), Board->GetCardTooltipTextForTest().Contains(TEXT("点击后弃置此牌。")));
	TestEqual(TEXT("forced-discard hover preserves the pending choice kind"), Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::ForcedDiscard);
	TestEqual(TEXT("forced-discard hover preserves candidate cards"), Deck.PendingChoice.Candidates.Num(), ForcedDiscardCandidateCountBeforeHover);
	TestEqual(TEXT("forced-discard hover preserves the hand"), Deck.Hand.Num(), ForcedDiscardHandCountBeforeHover);
	FirstChoiceButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a forced-discard candidate immediately hides the shared card tooltip"), Board->IsCardTooltipVisibleForTest());

	FirstChoiceButton->OnClicked.Broadcast();
	TestEqual(TEXT("selecting the required discard clears the blocking runtime choice"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind, EGameXXKCardPendingChoiceKind::None);
	TestEqual(TEXT("resolving the declared discard restores the normal round-refill target"), State.CardRun.ActiveBattle.Deck.Hand.Num(), State.CardRun.ActiveBattle.Deck.HandLimit);
	TestTrue(TEXT("resolving forced discard restores the end-turn path"), Board->EndCardPlayerPhase());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardHandCardHoverStyleTest,
	"GameXXK.Integration.CardBattle.BoardHandCardHoverStyle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardHandCardHoverStyleTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FGameXXKCardPlayPreview CardPreview;
	FString Error;
	TestTrue(FString::Printf(TEXT("hover fixture enters a playable manual-target card battle: %s"), *Error),
		BuildManualTargetCardFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, CardPreview, Error));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("hover board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	const TArray<FGameXXKCardInstance>& Hand = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand;
	const int32 SlotIndex = Hand.IndexOfByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == CardInstanceId;
	});
	TestTrue(TEXT("manual-target fixture card appears in the visible hand"), SlotIndex >= 0 && SlotIndex < 5);
	UButton* CardButton = Board->WidgetTree && SlotIndex >= 0
		? Cast<UButton>(Board->WidgetTree->FindWidget(*FString::Printf(TEXT("BattleHandCard_%02d"), SlotIndex)))
		: nullptr;
	TestNotNull(TEXT("hover fixture resolves the playable hand-card widget"), CardButton);
	if (!CardButton)
	{
		return false;
	}

	const FLinearColor NormalTint = CardButton->GetStyle().Normal.TintColor.GetSpecifiedColor();
	const FLinearColor HoveredTint = CardButton->GetStyle().Hovered.TintColor.GetSpecifiedColor();
	TestFalse(TEXT("a PSD-framed hand card has a visible non-normal hover treatment"),
		NormalTint.Equals(HoveredTint));

	CardButton->OnHovered.Broadcast();
	Board->AdvanceHandCardHoverMotionForTest(1.0f);
	const FWidgetTransform HoverTransform = CardButton->GetRenderTransform();
	TestTrue(TEXT("hover clearly lifts the readable card above its hand-row baseline"), HoverTransform.Translation.Y <= -25.0f);
	TestTrue(TEXT("hover clearly enlarges the card without changing its PSD frame asset"), HoverTransform.Scale.X >= 1.15f);
	UBorder* DetailPanel = Board->WidgetTree
		? Cast<UBorder>(Board->WidgetTree->FindWidget(TEXT("BattleHandCardDetailPanel")))
		: nullptr;
	UTextBlock* DetailBody = Board->WidgetTree
		? Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattleHandCardDetailBody")))
		: nullptr;
	TestNotNull(TEXT("hover creates a parchment card-detail panel"), DetailPanel);
	TestNotNull(TEXT("hover creates a readable card-detail body"), DetailBody);
	TestEqual(TEXT("hover reveals the card-detail panel"), DetailPanel ? DetailPanel->GetVisibility() : ESlateVisibility::Collapsed, ESlateVisibility::HitTestInvisible);
	const FString DetailText = DetailBody ? DetailBody->GetText().ToString() : FString();
	TestTrue(TEXT("hover detail explains the target instruction"), DetailText.Contains(TEXT("目标：")) && DetailText.Contains(TEXT("单体敌方")));
	TestTrue(TEXT("hover detail explains the card effect"), DetailText.Contains(TEXT("攻击伤害")));
	TestTrue(TEXT("hand hover exposes the actual preview interaction"), DetailText.Contains(TEXT("点击后选择高亮合法目标。")));
	TestTrue(TEXT("hand hover keeps the reusable tooltip input-transparent"), Board->IsCardTooltipHitTestInvisibleForTest());

	CardButton->OnUnhovered.Broadcast();
	Board->AdvanceHandCardHoverMotionForTest(1.0f);
	const FWidgetTransform ResetTransform = CardButton->GetRenderTransform();
	TestTrue(TEXT("unhover returns the hand card to its baseline"), FMath::IsNearlyZero(ResetTransform.Translation.Y));
	TestTrue(TEXT("unhover returns the hand card to its base scale"), FMath::IsNearlyEqual(ResetTransform.Scale.X, 1.0f));

	TestTrue(TEXT("manual target card enters the existing arrow-targeting state"), Board->ClickCardInHand(CardInstanceId));
	Board->AdvanceHandCardHoverMotionForTest(1.0f);
	const FWidgetTransform TargetingTransform = CardButton->GetRenderTransform();
	TestTrue(TEXT("the selected manual-target card remains visibly lifted while the arrow follows the cursor"),
		TargetingTransform.Translation.Y < HoverTransform.Translation.Y);
	TestTrue(TEXT("the selected manual-target card remains visibly emphasized"),
		TargetingTransform.Scale.X > HoverTransform.Scale.X);
	TestFalse(TEXT("targeting never leaves a persistent card tooltip after mouse leave"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("right-click cancellation clears the selected-card emphasis state"), Board->CancelBattleTargeting());
	Board->AdvanceHandCardHoverMotionForTest(1.0f);
	const FWidgetTransform CancelledTransform = CardButton->GetRenderTransform();
	TestTrue(TEXT("cancelling target selection restores the card baseline"), FMath::IsNearlyZero(CancelledTransform.Translation.Y));

	CardButton->OnHovered.Broadcast();
	Board->AdvanceHandCardHoverMotionForTest(1.0f);
	TestTrue(TEXT("the stale-hover fixture starts with a visible hand tooltip"), Board->IsCardTooltipVisibleForTest());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;
	Board->RefreshFromState();
	TestFalse(TEXT("leaving battle clears the shared hand tooltip immediately"), Board->IsCardTooltipVisibleForTest());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
	Board->RefreshFromState();
	TestFalse(TEXT("repopulating the same hand slot never resurrects a stale tooltip"), Board->IsCardTooltipVisibleForTest());
	const FWidgetTransform ReenteredTransform = CardButton->GetRenderTransform();
	TestTrue(TEXT("repopulating the same hand slot keeps its transform at identity until a new hover"),
		FMath::IsNearlyZero(ReenteredTransform.Translation.Y)
		&& FMath::IsNearlyEqual(ReenteredTransform.Scale.X, 1.0f)
		&& FMath::IsNearlyEqual(ReenteredTransform.Scale.Y, 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardRewardTest,
	"GameXXK.Integration.CardBattle.BoardRewards",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardRewardTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString Error;
	TestTrue(FString::Printf(TEXT("route reward fixture enters a playable card battle: %s"), *Error),
		BuildRouteRewardFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Error));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("reward board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("the saved victory gate creates a pending route reward"), Subsystem->ResolveBattleVictory(false));
	Board->RefreshFromState();
	TestEqual(TEXT("victory stays on the battle board while the saved reward gate is unresolved"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("battle board exposes the pending three-card reward state"), Board->HasPendingRouteReward());
	TestEqual(TEXT("battle board exposes exactly three saved reward ids"), Board->GetPendingRouteRewardCardIds().Num(), 3);
	TestEqual(TEXT("reward overlay hides the spent battle hand"), Board->GetVisibleHandCardCountForTest(), 0);
	UButton* FirstRewardButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleRewardCard_00"))) : nullptr;
	TestNotNull(TEXT("the first saved route reward remains a hoverable card"), FirstRewardButton);
	if (!FirstRewardButton)
	{
		return false;
	}
	const TArray<FName> RewardIdsBeforeHover = Board->GetPendingRouteRewardCardIds();
	const FGameXXKRuntimeState StateBeforeRewardHover = Subsystem->GetRuntimeState();
	FirstRewardButton->OnHovered.Broadcast();
	TestTrue(TEXT("hovering a reward reveals the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the reward tooltip states the actual route-deck action"), Board->GetCardTooltipTextForTest().Contains(TEXT("点击后加入临时路线卡组；满位时选择要替换的路线牌。")));
	TestEqual(TEXT("reward hover preserves every saved reward id"), Board->GetPendingRouteRewardCardIds(), RewardIdsBeforeHover);
	TestTrue(TEXT("reward hover preserves the complete runtime state"),
		RuntimeStatesEqual(Subsystem->GetRuntimeState(), StateBeforeRewardHover));
	FirstRewardButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a reward immediately hides the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("skip reward resolves through adapter then Rules victory gate"), Board->SkipPendingRouteReward());
	TestEqual(TEXT("skipping a reward advances the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardRewardReplacementTest,
	"GameXXK.Integration.CardBattle.BoardRewardReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardRewardReplacementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString Error;
	TestTrue(FString::Printf(TEXT("full-route reward fixture enters a playable card battle: %s"), *Error),
		BuildRouteRewardFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Error));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	TestTrue(TEXT("fixture fills exactly twelve canonical temporary route-card slots"), FillRewardEntryCapacity(State));
	TestEqual(TEXT("legacy route-card projection remains empty"), State.CardRun.RouteCardIds.Num(), 0);

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("replacement board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("the saved victory gate creates a pending full-capacity reward"), Subsystem->ResolveBattleVictory(false));
	SetMixedFullCapacityOffer(State, true);
	Board->RefreshFromState();

	FGameXXKRouteCardAcquisitionPreview ReplacementPreview;
	TestTrue(FString::Printf(TEXT("full non-merge offer previews through the canonical acquisition rules: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			ReplacementRewardCardId,
			NAME_None,
			ReplacementPreview,
			&Error));
	if (ReplacementPreview.Decision != EGameXXKRouteCardAcquisitionDecision::RequiresReplacement)
	{
		AddError(FString::Printf(TEXT("Replacement preview failed before Board assertions: %s"), *Error));
		return false;
	}
	TestEqual(TEXT("the non-merge candidate requires replacement"),
		ReplacementPreview.Decision,
		EGameXXKRouteCardAcquisitionDecision::RequiresReplacement);

	UGameInstance* DirectGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* DirectSubsystem = NewObject<UGameXXKMVPSubsystem>(DirectGameInstance);
	DirectSubsystem->GetMutableRuntimeState() = State;
	UGameXXKBattleBoardWidget* DirectBoard = NewObject<UGameXXKBattleBoardWidget>();
	DirectBoard->SetMVPSubsystem(DirectSubsystem);
	TestTrue(TEXT("direct-candidate board initializes its widget tree"), DirectBoard->Initialize());
	DirectBoard->NativeConstruct();
	DirectBoard->RefreshFromState();
	TestTrue(TEXT("a CanCommit candidate enters the route map in one Board click"),
		DirectBoard->ChoosePendingRouteReward(MergeRewardCardId, NAME_None));
	TestEqual(TEXT("direct Board click finishes the victory gate"),
		DirectSubsystem->GetRuntimeState().Screen,
		EGameXXKScreen::DungeonMap);

	TestFalse(TEXT("first replacement-required click only enters that candidate's replacement state"),
		Board->ChoosePendingRouteReward(ReplacementRewardCardId, NAME_None));
	TestEqual(TEXT("board records the exact reward candidate awaiting replacement"),
		Board->GetRouteRewardCardIdAwaitingReplacementForTest(),
		ReplacementRewardCardId);
	TestEqual(TEXT("board exposes exactly the preview's eligible stable EntryIds"),
		Board->GetRouteRewardReplacementEntryIds(),
		ReplacementPreview.EligibleReplacementEntryIds);
	const TArray<FName> LegacyTrueEntryIds = Board->GetRouteRewardReplacementEntryIds();
	State.CardRun.PendingReward.bRequiresRouteCardReplacement = false;
	TestEqual(TEXT("legacy bool false exposes the same EntryId replacement UI"),
		Board->GetRouteRewardReplacementEntryIds(),
		LegacyTrueEntryIds);
	State.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	TestEqual(TEXT("entering replacement state clears stale selection"),
		Board->GetSelectedRouteRewardReplacementEntryIdForTest(),
		NAME_None);
	TestFalse(TEXT("a CardId cannot masquerade as a replacement EntryId"),
		Board->SelectRouteRewardReplacementEntry(DuplicateReplacementCardId));

	TArray<FName> DuplicateEntryIds;
	for (const FGameXXKRouteCardEntry& Entry : State.CardRun.RouteCardEntries)
	{
		if (Entry.CardId == DuplicateReplacementCardId && Entry.bConsumesRouteCapacity)
		{
			DuplicateEntryIds.Add(Entry.EntryId);
		}
	}
	TestEqual(TEXT("same CardId remains two independent stable replacement entries"), DuplicateEntryIds.Num(), 2);
	TestTrue(TEXT("first duplicate instance is independently eligible"),
		DuplicateEntryIds.IsValidIndex(0) && Board->GetRouteRewardReplacementEntryIds().Contains(DuplicateEntryIds[0]));
	TestTrue(TEXT("second duplicate instance is independently eligible"),
		DuplicateEntryIds.IsValidIndex(1) && Board->GetRouteRewardReplacementEntryIds().Contains(DuplicateEntryIds[1]));
	TestEqual(TEXT("board renders one replacement button per eligible EntryId"),
		Board->GetRouteRewardReplacementEntryIds().Num(),
		12);
	UButton* DuplicateReplacementButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleRouteReplaceCard_01"))) : nullptr;
	TestNotNull(TEXT("each duplicate-card entry remains independently hoverable"), DuplicateReplacementButton);
	if (!DuplicateReplacementButton)
	{
		return false;
	}
	const FGameXXKRuntimeState StateBeforeHover = State;
	const TArray<FName> ReplaceableIdsBeforeHover = Board->GetRouteRewardReplacementEntryIds();
	DuplicateReplacementButton->OnHovered.Broadcast();
	TestTrue(TEXT("hovering a route replacement reveals the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the replacement tooltip states the actual replacement action"), Board->GetCardTooltipTextForTest().Contains(TEXT("点击后作为被替换的临时路线牌。")));
	TestTrue(TEXT("replacement tooltip uses the duplicate entry's Epic CurrentQuality"), Board->GetCardTooltipTextForTest().Contains(TEXT("品质：珍稀")));
	TestEqual(TEXT("replacement hover keeps the replacement selection unchanged"), Board->GetSelectedRouteRewardReplacementEntryIdForTest(), NAME_None);
	TestEqual(TEXT("replacement hover preserves the eligible EntryIds"), Board->GetRouteRewardReplacementEntryIds(), ReplaceableIdsBeforeHover);
	TestTrue(TEXT("replacement hover preserves the complete runtime state"), RuntimeStatesEqual(State, StateBeforeHover));
	DuplicateReplacementButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a route replacement immediately hides the shared card tooltip"), Board->IsCardTooltipVisibleForTest());

	const FName FirstSelectedEntryId = ReplaceableIdsBeforeHover[0];
	TestTrue(TEXT("board accepts an eligible stable EntryId as the replacement choice"), Board->SelectRouteRewardReplacementEntry(FirstSelectedEntryId));
	TestEqual(TEXT("board keeps the stable EntryId selection"), Board->GetSelectedRouteRewardReplacementEntryIdForTest(), FirstSelectedEntryId);
	const FGameXXKRuntimeState StateBeforeCancel = State;
	TestTrue(TEXT("explicit replacement cancel clears the transient chooser"), Board->CancelRouteRewardReplacement());
	TestEqual(TEXT("cancel clears the awaiting reward CardId"), Board->GetRouteRewardCardIdAwaitingReplacementForTest(), NAME_None);
	TestEqual(TEXT("cancel clears the selected EntryId"), Board->GetSelectedRouteRewardReplacementEntryIdForTest(), NAME_None);
	TestTrue(TEXT("cancel preserves the complete runtime state byte-for-byte"), RuntimeStatesEqual(State, StateBeforeCancel));
	TestFalse(TEXT("re-entering the same replacement candidate remains a pure first click"),
		Board->ChoosePendingRouteReward(ReplacementRewardCardId, NAME_None));
	TestTrue(TEXT("board accepts a replacement selection after cancel"), Board->SelectRouteRewardReplacementEntry(FirstSelectedEntryId));
	TestFalse(TEXT("switching to another replacement-required reward enters its own replacement state"),
		Board->ChoosePendingRouteReward(AlternateReplacementRewardCardId, NAME_None));
	TestEqual(TEXT("switching reward candidates records the new awaiting CardId"),
		Board->GetRouteRewardCardIdAwaitingReplacementForTest(),
		AlternateReplacementRewardCardId);
	TestEqual(TEXT("switching reward candidates clears the previous EntryId selection"),
		Board->GetSelectedRouteRewardReplacementEntryIdForTest(),
		NAME_None);
	TestTrue(TEXT("board accepts a fresh eligible EntryId for the switched candidate"), Board->SelectRouteRewardReplacementEntry(FirstSelectedEntryId));
	TestTrue(TEXT("one subsystem facade call chooses, settles, and returns to the route map"),
		Board->ChoosePendingRouteReward(AlternateReplacementRewardCardId, Board->GetSelectedRouteRewardReplacementEntryIdForTest()));
	TestEqual(TEXT("reward replacement returns the player to the route map"), State.Screen, EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("replacement preserves the twelve-entry temporary route cap"), CountCapacityEntries(State), 12);
	TestFalse(TEXT("chosen stable EntryId was removed"), State.CardRun.RouteCardEntries.ContainsByPredicate([FirstSelectedEntryId](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.EntryId == FirstSelectedEntryId;
	}));
	TestTrue(TEXT("new reward card occupies one canonical temporary entry"), State.CardRun.RouteCardEntries.ContainsByPredicate([](const FGameXXKRouteCardEntry& Entry)
	{
		return Entry.CardId == AlternateReplacementRewardCardId && Entry.bConsumesRouteCapacity;
	}));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardRewardAtomicFacadeTest,
	"GameXXK.Integration.CardBattle.BoardRewardAtomicFacade",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardRewardAtomicFacadeTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString Error;
	if (!TestTrue(TEXT("atomic-facade fixture enters a playable card battle"),
		BuildRouteRewardFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Error)))
	{
		return false;
	}

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	for (FGameXXKCardCombatUnit& Unit : State.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.HP = 0;
			Unit.bLiving = false;
		}
	}
	State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Victory;
	TestTrue(TEXT("victory gate creates a pending route reward"), Subsystem->ResolveBattleVictory(false));
	SetMixedFullCapacityOffer(State, true);

	FGameXXKRuntimeState LegacyFalse = State;
	FGameXXKRuntimeState LegacyTrue = State;
	LegacyFalse.CardRun.PendingReward.bRequiresRouteCardReplacement = false;
	LegacyTrue.CardRun.PendingReward.bRequiresRouteCardReplacement = true;
	FString ValidationError;
	TestTrue(TEXT("validator accepts a nonempty pending offer with the legacy bool false"),
		FGameXXKSaveMigration::ValidateRuntimeState(LegacyFalse, ValidationError));
	TestTrue(TEXT("validator accepts the same nonempty pending offer with the legacy bool true"),
		FGameXXKSaveMigration::ValidateRuntimeState(LegacyTrue, ValidationError));

	FGameXXKRuntimeState RollbackState = LegacyFalse;
	RollbackState.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState RollbackBefore = RollbackState;
	TestFalse(TEXT("choose-and-finish reports a deliberately failed victory settlement"),
		UGameXXKMVPRules::ResolvePendingRouteRewardChoiceAndFinish(
			RollbackState,
			MergeRewardCardId,
			NAME_None,
			&Error));
	TestTrue(TEXT("failed post-choice victory settlement rolls back the complete runtime"),
		RuntimeStatesEqual(RollbackState, RollbackBefore));

	FGameXXKRuntimeState SkipRollbackState = LegacyTrue;
	SkipRollbackState.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState SkipRollbackBefore = SkipRollbackState;
	TestFalse(TEXT("skip-and-finish reports a deliberately failed victory settlement"),
		UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(SkipRollbackState, &Error));
	TestTrue(TEXT("failed post-skip victory settlement rolls back the complete runtime"),
		RuntimeStatesEqual(SkipRollbackState, SkipRollbackBefore));

	FGameXXKRuntimeState DirectState = LegacyFalse;
	TestTrue(TEXT("one rules facade call commits a merge-direct reward and finishes victory"),
		UGameXXKMVPRules::ResolvePendingRouteRewardChoiceAndFinish(
			DirectState,
			MergeRewardCardId,
			NAME_None,
			&Error));
	TestEqual(TEXT("direct facade returns to the route map"), DirectState.Screen, EGameXXKScreen::DungeonMap);

	FGameXXKRuntimeState SkipState = LegacyTrue;
	TestTrue(TEXT("one rules facade call skips and finishes victory"),
		UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(SkipState, &Error));
	TestEqual(TEXT("skip facade returns to the route map"), SkipState.Screen, EGameXXKScreen::DungeonMap);
	return true;
}

#endif
