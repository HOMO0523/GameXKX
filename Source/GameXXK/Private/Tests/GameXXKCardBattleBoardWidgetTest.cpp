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
#include "UI/GameXXKBattleUnitVisualWidget.h"

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

	template <typename TBoard, typename = void>
	struct THasPartyQiResponsiveRefresh : std::false_type
	{
	};

	template <typename TBoard>
	struct THasPartyQiResponsiveRefresh<TBoard, std::void_t<decltype(
		std::declval<TBoard&>().RefreshPartyQiForCanvasSizeForTest(std::declval<FVector2D>()))>> : std::true_type
	{
	};

	template <typename TBoard>
	bool RefreshPartyQiForResponsiveLayoutTest(
		FAutomationTestBase& Test,
		TBoard* const Board,
		const FVector2D CanvasSize)
	{
		if constexpr (!THasPartyQiResponsiveRefresh<TBoard>::value)
		{
			Test.AddError(TEXT("The Board-owned Party Qi responsive-refresh test seam has not been implemented."));
			return false;
		}
		else
		{
			Board->RefreshPartyQiForCanvasSizeForTest(CanvasSize);
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
		FString& OutError,
		const bool bRequirePureDamageCard = false)
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
		FGameXXKCardCombatUnit* const Hero = State.CardRun.ActiveBattle.Units.FindByPredicate(
			[](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.Side == EGameXXKCardTargetSide::Party
					&& Unit.Role == EGameXXKCharacterRole::Hero
					&& Unit.bLiving;
			});
		if (!Hero || State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			OutError = TEXT("The route fixture has no living hero or hand card.");
			return false;
		}
		// Route generation shuffles the opening hand. Pin one affordable manual
		// enemy-target card so reward and lethal-presentation fixtures never flake.
		FGameXXKCardInstance& FixtureCard = State.CardRun.ActiveBattle.Deck.Hand[0];
		FixtureCard.CardId = TEXT("Hero.QingFengYiShi");
		FixtureCard.OwnerUnitId = Hero->UnitId;
		State.CardRun.ActiveBattle.Deck.SharedEnergy = FMath::Max(
			State.CardRun.ActiveBattle.Deck.SharedEnergy,
			1);
		for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
		{
			if (bRequirePureDamageCard)
			{
				const FGameXXKCardDefinition* const Definition =
					FGameXXKCardCatalog::FindCardDefinition(CardInstance.CardId);
				const bool bHasDamage = Definition && Definition->Effects.ContainsByPredicate(
					[](const FGameXXKCardEffect& Effect)
					{
						return Effect.Type == EGameXXKCardEffectType::DamagePercentAttack
							|| Effect.Type == EGameXXKCardEffectType::DamageFlat
							|| Effect.Type == EGameXXKCardEffectType::LoseHealth;
					});
				const bool bHasStatusMutation = Definition && Definition->Effects.ContainsByPredicate(
					[](const FGameXXKCardEffect& Effect)
					{
						return Effect.Type == EGameXXKCardEffectType::ApplyStatus
							|| Effect.Type == EGameXXKCardEffectType::RemoveStatus
							|| Effect.Type == EGameXXKCardEffectType::RemoveAnyDamageOverTime;
					});
				if (!bHasDamage || bHasStatusMutation)
				{
					continue;
				}
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
			Enemy->HP = 1;
			if (bRequirePureDamageCard)
			{
				for (FGameXXKCardCombatUnit& OtherUnit : State.CardRun.ActiveBattle.Units)
				{
					if (OtherUnit.Side == EGameXXKCardTargetSide::Enemy
						&& OtherUnit.UnitId != Enemy->UnitId)
					{
						OtherUnit.HP = 0;
						OtherUnit.bLiving = false;
					}
				}
			}
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

	FGameXXKCardCombatUnit MakeBoardPresentationUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 Attack,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Mana = 20;
		Unit.MaxMana = 20;
		Unit.Attack = Attack;
		Unit.Speed = 8;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKBattleRuntimeUnit MakeBoardPresentationLegacyUnit(
		const TCHAR* UnitId,
		const TCHAR* DisplayName,
		const bool bEnemy,
		const int32 Attack)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(UnitId);
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.MP = 20;
		Unit.MaxMP = 20;
		Unit.Attack = Attack;
		Unit.Speed = 8;
		Unit.Shield = 0;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeBoardPresentationCards(
		const FName CardId,
		const FName OwnerUnitId)
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 6; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("Board.Presentation.Card.%d"), Index));
			Card.CardId = CardId;
			Card.OwnerUnitId = OwnerUnitId;
			Card.SourceEntryId = FName(*FString::Printf(TEXT("Board.Presentation.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	bool BuildBoardPresentationGateFixture(
		UGameXXKMVPSubsystem* const Subsystem,
		FName& OutCardInstanceId,
		FString& OutError)
	{
		OutCardInstanceId = NAME_None;
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The Board presentation subsystem is missing.");
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 73;
		State.ActiveBattleParty = {
			MakeBoardPresentationLegacyUnit(TEXT("Blade"), TEXT("刀客"), false, 20)};
		State.ActiveBattleEnemies = {
			MakeBoardPresentationLegacyUnit(TEXT("Enemy"), TEXT("反击敌人"), true, 10)};

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeBoardPresentationUnit(TEXT("Blade"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 20, 0),
			MakeBoardPresentationUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeBoardPresentationCards(TEXT("Profession.Blade.JiYuLianZhan"), TEXT("Blade")),
			Units,
			EGameXXKCardTerrain::Plain,
			8801,
			&OutError))
		{
			return false;
		}

		FGameXXKCardBattleModifierRuntime& Reflect = Runtime.Modifiers.AddDefaulted_GetRef();
		Reflect.ModifierId = TEXT("Board.Presentation.Reflect");
		Reflect.SourceCardInstanceId = Runtime.Deck.ActiveInstanceIds[0];
		Reflect.SourceUnitId = TEXT("Enemy");
		Reflect.RecipientUnitIds = {TEXT("Enemy")};
		Reflect.Definition.Trigger = EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound;
		Reflect.Definition.EffectType = EGameXXKCardEffectType::DamagePercentAttack;
		Reflect.Definition.Target = EGameXXKCardEffectTarget::Attacker;
		Reflect.Definition.Magnitude = 50;
		Reflect.Definition.RemainingTriggers = 1;
		Reflect.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
		Reflect.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
		Reflect.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;

		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattleSourceNodeId = State.ActiveBattleNodeId;
		State.CardRun.ActiveBattle = MoveTemp(Runtime);
		if (!FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &OutError))
		{
			return false;
		}
		if (State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			OutError = TEXT("The Board presentation fixture did not draw a hand card.");
			return false;
		}
		OutCardInstanceId = State.CardRun.ActiveBattle.Deck.Hand[0].InstanceId;
		return true;
	}

	template <typename TBoard, typename = void>
	struct TBoardPresentationGateApi
	{
		static constexpr bool bAvailable = false;
		static bool IsLocked(const TBoard*) { return false; }
		static FName Attacker(const TBoard*) { return NAME_None; }
		static FName Target(const TBoard*) { return NAME_None; }
		static int32 Continuations(const TBoard*) { return INDEX_NONE; }
	};

	template <typename TBoard>
	struct TBoardPresentationGateApi<TBoard, std::void_t<
		decltype(std::declval<const TBoard&>().IsBattlePresentationLockedForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationAttackerUnitIdForTest()),
		decltype(std::declval<const TBoard&>().GetActiveBattlePresentationTargetUnitIdForTest()),
		decltype(std::declval<const TBoard&>().GetExecutedBattlePresentationContinuationCountForTest())>>
	{
		static constexpr bool bAvailable = true;
		static bool IsLocked(const TBoard* Board) { return Board->IsBattlePresentationLockedForTest(); }
		static FName Attacker(const TBoard* Board) { return Board->GetActiveBattlePresentationAttackerUnitIdForTest(); }
		static FName Target(const TBoard* Board) { return Board->GetActiveBattlePresentationTargetUnitIdForTest(); }
		static int32 Continuations(const TBoard* Board) { return Board->GetExecutedBattlePresentationContinuationCountForTest(); }
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardPresentationGateTest,
	"GameXXK.Integration.CardBattle.BoardPresentationGate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardPresentationGateTest::RunTest(const FString& Parameters)
{
	using FGateApi = TBoardPresentationGateApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("Board exposes an occupancy-based presentation lock and typed-continuation diagnostics"), FGateApi::bAvailable);
	if (!FGateApi::bAvailable)
	{
		return false;
	}

	UGameInstance* const GateGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const GateSubsystem = NewObject<UGameXXKMVPSubsystem>(GateGameInstance);
	FName GateCardInstanceId;
	FString Error;
	if (!TestTrue(TEXT("presentation-lock fixture builds a real reflected card battle"),
		BuildBoardPresentationGateFixture(GateSubsystem, GateCardInstanceId, Error)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const GateBoard = NewObject<UGameXXKBattleBoardWidget>();
	GateBoard->SetMVPSubsystem(GateSubsystem);
	TestTrue(TEXT("presentation-lock Board initializes"), GateBoard->Initialize());
	GateBoard->NativeConstruct();
	TestTrue(TEXT("presentation-lock Board begins a visual session"), GateBoard->BeginBattleVisualSession(8101));
	TestTrue(TEXT("the real hand card enters manual targeting before the lock"), GateBoard->ClickCardInHand(GateCardInstanceId));

	FGameXXKBattlePresentationEvent BlockingEvent;
	BlockingEvent.EventId = 9001;
	BlockingEvent.AttackerUnitId = TEXT("Blade");
	BlockingEvent.TargetUnitId = TEXT("Enemy");
	BlockingEvent.TargetHealthBefore = 100;
	BlockingEvent.TargetHealthAfter = 90;
	BlockingEvent.HealthDamage = 10;
	BlockingEvent.bTargetEnemy = true;
	GateBoard->QueuePresentation(BlockingEvent);
	const FGameXXKRuntimeState GateStateBeforeRejectedInput = GateSubsystem->GetRuntimeState();
	TestTrue(TEXT("queue occupancy locks input before the first timeline sample"), FGateApi::IsLocked(GateBoard));
	TestFalse(TEXT("a queued presentation rejects a second card click"), GateBoard->ClickCardInHand(GateCardInstanceId));
	TestFalse(TEXT("a queued presentation rejects a valid target confirmation"), GateBoard->ConfirmTargetingUnit(TEXT("Enemy")));
	GateBoard->HandleUnitTargetProxyClicked(TEXT("Enemy"));
	TestFalse(TEXT("a queued presentation rejects end turn"), GateBoard->EndCardPlayerPhase());
	TestTrue(TEXT("every locked card/target/proxy/end-turn path preserves runtime state"),
		RuntimeStatesEqual(GateSubsystem->GetRuntimeState(), GateStateBeforeRejectedInput));
	UButton* const LockedHandCard = GateBoard->WidgetTree
		? Cast<UButton>(GateBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00")))
		: nullptr;
	UButton* const LockedEndTurn = GateBoard->GetEndTurnButtonForTest();
	UButton* const LockedTargetProxy = GateBoard->GetUnitTargetProxyForTest(TEXT("Enemy"));
	TestFalse(TEXT("the visible hand control is disabled for the whole pending queue"), LockedHandCard && LockedHandCard->GetIsEnabled());
	TestFalse(TEXT("the visible end-turn control is disabled for the whole pending queue"), LockedEndTurn && LockedEndTurn->GetIsEnabled());
	TestTrue(TEXT("the target proxy is hidden or disabled for the whole pending queue"),
		LockedTargetProxy
		&& (!LockedTargetProxy->GetIsEnabled() || LockedTargetProxy->GetVisibility() != ESlateVisibility::Visible));
	GateBoard->AdvanceVisualsAtRealTime(0.0);
	GateBoard->AdvanceVisualsAtRealTime(1.1);
	TestEqual(TEXT("the marker exposes the packet-local intermediate health"), GateBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 90);
	GateBoard->AdvanceVisualsAtRealTime(2.5);
	TestFalse(TEXT("input unlocks only after the full presentation queue drains"), FGateApi::IsLocked(GateBoard));
	TestEqual(TEXT("full-drain reconciliation restores authoritative target health"), GateBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 100);
	TestTrue(TEXT("the target confirmation can mutate again after full drain"), GateBoard->ConfirmTargetingUnit(TEXT("Enemy")));

	UGameInstance* const OrderedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const OrderedSubsystem = NewObject<UGameXXKMVPSubsystem>(OrderedGameInstance);
	FName OrderedCardInstanceId;
	if (!TestTrue(TEXT("ordered-packet fixture builds a real reflected card battle"),
		BuildBoardPresentationGateFixture(OrderedSubsystem, OrderedCardInstanceId, Error)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const OrderedBoard = NewObject<UGameXXKBattleBoardWidget>();
	OrderedBoard->SetMVPSubsystem(OrderedSubsystem);
	TestTrue(TEXT("ordered-packet Board initializes"), OrderedBoard->Initialize());
	OrderedBoard->NativeConstruct();
	TestTrue(TEXT("ordered-packet Board begins a visual session"), OrderedBoard->BeginBattleVisualSession(8102));
	UGameXXKBattlePartyQiWidget* const OrderedPartyQiWidget = OrderedBoard->GetPartyQiWidgetForTest();
	TestNotNull(TEXT("ordered-packet Board owns the Party Qi projection"), OrderedPartyQiWidget);
	const int32 OrderedQiBeforeCommit = OrderedSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestTrue(TEXT("two-hit reflected card enters targeting"), OrderedBoard->ClickCardInHand(OrderedCardInstanceId));
	TestTrue(TEXT("two-hit reflected card commits through the Board"), OrderedBoard->ConfirmTargetingUnit(TEXT("Enemy")));
	const int32 OrderedQiAfterCommit = OrderedSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestTrue(TEXT("the reflected presentation fixture spends positive authoritative Qi"),
		OrderedQiAfterCommit < OrderedQiBeforeCommit);
	TestTrue(TEXT("a successful Board mutation is locked until its presentation drains"), FGateApi::IsLocked(OrderedBoard));
	TestEqual(TEXT("Party Qi starts the locked batch on its pre-mutation baseline"),
		OrderedPartyQiWidget ? OrderedPartyQiWidget->GetSharedQiForTest() : INDEX_NONE,
		OrderedQiBeforeCommit);
	TestTrue(TEXT("the responsive Party Qi path accepts the first settled canvas geometry while locked"),
		RefreshPartyQiForResponsiveLayoutTest(*this, OrderedBoard, FVector2D(1280.0f, 722.0f)));
	const FGameXXKBattlePartyQiLayout LockedCompactQiLayout =
		OrderedBoard->ResolvePartyQiLayoutForTest(FVector2D(1280.0f, 722.0f));
	const UCanvasPanelSlot* const OrderedPartyQiSlot = OrderedPartyQiWidget
		? Cast<UCanvasPanelSlot>(OrderedPartyQiWidget->Slot)
		: nullptr;
	TestTrue(TEXT("the locked responsive refresh still applies the settled canvas layout"),
		OrderedPartyQiSlot
		&& OrderedPartyQiSlot->GetOffsets() == LockedCompactQiLayout.SlotOffsets);
	TestEqual(TEXT("a locked responsive-layout refresh cannot expose post-mutation Qi"),
		OrderedPartyQiWidget ? OrderedPartyQiWidget->GetSharedQiForTest() : INDEX_NONE,
		OrderedQiBeforeCommit);
	TestEqual(TEXT("the Board enqueues the two primary packets and reflected packet before refresh"),
		OrderedBoard->GetBattlePresentationQueueCountForTest(),
		3);
	OrderedBoard->AdvanceVisualsAtRealTime(0.0);
	TestEqual(TEXT("packet one retains the primary attacker"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Blade")));
	TestEqual(TEXT("packet one retains the primary target"), FGateApi::Target(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("the first target baseline is seeded from packet one"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 100);
	OrderedBoard->AdvanceVisualsAtRealTime(1.1);
	TestEqual(TEXT("packet one marker applies only packet one's health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 85);
	OrderedBoard->AdvanceVisualsAtRealTime(2.5);
	TestEqual(TEXT("packet two reverses the reflected source"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("packet two reverses the reflected target"), FGateApi::Target(OrderedBoard), FName(TEXT("Blade")));
	TestEqual(TEXT("packet one's target override survives the reflected intermediate entry"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 85);
	OrderedBoard->AdvanceVisualsAtRealTime(3.6);
	TestEqual(TEXT("the reflection marker applies its own target health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Blade")), 95);
	OrderedBoard->AdvanceVisualsAtRealTime(5.0);
	TestEqual(TEXT("packet three returns to the primary attacker"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Blade")));
	TestEqual(TEXT("packet three returns to the primary target"), FGateApi::Target(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("packet three begins at packet one's committed target health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 85);
	OrderedBoard->AdvanceVisualsAtRealTime(6.1);
	TestEqual(TEXT("packet three marker reaches the final target health without early reconciliation"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 70);
	OrderedBoard->AdvanceVisualsAtRealTime(7.5);
	TestFalse(TEXT("the ordered batch unlocks after all three packets"), FGateApi::IsLocked(OrderedBoard));
	TestEqual(TEXT("ordered target HUD reconciles to authoritative final health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 70);
	TestEqual(TEXT("reflected target HUD reconciles to authoritative final health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Blade")), 95);
	TestEqual(TEXT("Party Qi reconciles to authoritative energy only after the complete batch drains"),
		OrderedPartyQiWidget ? OrderedPartyQiWidget->GetSharedQiForTest() : INDEX_NONE,
		OrderedQiAfterCommit);
	TestEqual(TEXT("the card finalization continuation executes exactly once"), FGateApi::Continuations(OrderedBoard), 1);

	UGameInstance* const LethalGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const LethalSubsystem = NewObject<UGameXXKMVPSubsystem>(LethalGameInstance);
	FName LethalCardInstanceId;
	FName LethalTargetUnitId;
	FName LethalOwnerUnitId;
	if (!TestTrue(TEXT("lethal route fixture enters a one-health reward battle"),
		BuildRouteRewardFixture(LethalSubsystem, LethalCardInstanceId, LethalTargetUnitId, LethalOwnerUnitId, Error, true)))
	{
		AddError(Error);
		return false;
	}
	FGameXXKCardCombatUnit* const LethalTarget =
		LethalSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate(
			[LethalTargetUnitId](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == LethalTargetUnitId;
			});
	TestNotNull(TEXT("lethal status-order fixture keeps the target unit"), LethalTarget);
	TestTrue(TEXT("lethal status-order fixture adds one consumable vulnerability stack"),
		LethalTarget
		&& GameXXKCardRules::AddCombatStatus(
			*LethalTarget,
			EGameXXKCardStatus::Vulnerability,
			1) == 1);
	UGameXXKBattleBoardWidget* const LethalBoard = NewObject<UGameXXKBattleBoardWidget>();
	LethalBoard->SetMVPSubsystem(LethalSubsystem);
	TestTrue(TEXT("lethal Board initializes"), LethalBoard->Initialize());
	LethalBoard->NativeConstruct();
	TestTrue(TEXT("lethal Board begins a visual session"), LethalBoard->BeginBattleVisualSession(8103));
	TestTrue(TEXT("lethal route card enters targeting"), LethalBoard->ClickCardInHand(LethalCardInstanceId));
	TestTrue(TEXT("lethal route card commits"), LethalBoard->ConfirmTargetingUnit(LethalTargetUnitId));
	TestEqual(TEXT("the adapter commits terminal phase before presentation"),
		LethalSubsystem->GetRuntimeState().CardRun.ActiveBattle.Phase,
		EGameXXKCardBattlePhase::Victory);
	TestFalse(TEXT("terminal reward handling is deferred until after Death"), LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(0.0);
	LethalBoard->AdvanceVisualsAtRealTime(2.5);
	TestTrue(TEXT("lethal Hit transitions to Death before removal"), LethalBoard->IsBattleDeathPresentationActiveForTest());
	TestFalse(TEXT("reward remains deferred throughout Death"), LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(7.5);
	TestTrue(TEXT("the consumed status delta begins only after lethal Hit and Death"),
		LethalBoard->IsBattleStatusPresentationActiveForTest());
	TestEqual(TEXT("post-Death status presentation retains the defeated affected unit"),
		FGateApi::Target(LethalBoard), LethalTargetUnitId);
	TestNotNull(TEXT("post-Death status presentation retains the affected unit visual"),
		LethalBoard->GetUnitVisualForTest(LethalTargetUnitId));
	TestEqual(TEXT("consumed vulnerability uses a signed negative status readout"),
		LethalBoard->GetActiveBattleStatusDeltaForTest(), -1);
	TestFalse(TEXT("reward remains deferred throughout the post-Death status delta"),
		LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(100.0);
	TestTrue(TEXT("the reward gate opens only after Hit, Death, and status all drain"),
		LethalBoard->HasPendingRouteReward());
	const TArray<FName> RewardIdsAfterDrain = LethalBoard->GetPendingRouteRewardCardIds();
	TestEqual(TEXT("lethal finalization continuation executes exactly once"), FGateApi::Continuations(LethalBoard), 1);
	LethalBoard->AdvanceVisualsAtRealTime(200.0);
	TestEqual(TEXT("a large later delta never repeats terminal reward generation"),
		LethalBoard->GetPendingRouteRewardCardIds(),
		RewardIdsAfterDrain);
	TestEqual(TEXT("a large later delta never re-enters the lethal continuation"), FGateApi::Continuations(LethalBoard), 1);

	UGameInstance* const CancelGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const CancelSubsystem = NewObject<UGameXXKMVPSubsystem>(CancelGameInstance);
	FName CancelCardInstanceId;
	FName CancelTargetUnitId;
	FName CancelOwnerUnitId;
	if (!TestTrue(TEXT("cancellation fixture enters a one-health reward battle"),
		BuildRouteRewardFixture(CancelSubsystem, CancelCardInstanceId, CancelTargetUnitId, CancelOwnerUnitId, Error, true)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const CancelBoard = NewObject<UGameXXKBattleBoardWidget>();
	CancelBoard->SetMVPSubsystem(CancelSubsystem);
	TestTrue(TEXT("cancellation Board initializes"), CancelBoard->Initialize());
	CancelBoard->NativeConstruct();
	TestTrue(TEXT("cancellation Board begins a visual session"), CancelBoard->BeginBattleVisualSession(8104));
	UGameXXKBattlePartyQiWidget* const CancelPartyQiWidget = CancelBoard->GetPartyQiWidgetForTest();
	TestNotNull(TEXT("cancellation Board owns the Party Qi projection"), CancelPartyQiWidget);
	const int32 CancelQiBeforeCommit = CancelSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestTrue(TEXT("cancellation route card enters targeting"), CancelBoard->ClickCardInHand(CancelCardInstanceId));
	TestTrue(TEXT("cancellation route card commits"), CancelBoard->ConfirmTargetingUnit(CancelTargetUnitId));
	const int32 CancelQiAfterCommit = CancelSubsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy;
	TestTrue(TEXT("cancellation fixture commits a positive Qi cost"), CancelQiAfterCommit < CancelQiBeforeCommit);
	TestEqual(TEXT("cancellation fixture retains pre-mutation Qi while its continuation is pending"),
		CancelPartyQiWidget ? CancelPartyQiWidget->GetSharedQiForTest() : INDEX_NONE,
		CancelQiBeforeCommit);
	TestTrue(TEXT("cancellation fixture owns a pending typed continuation"), FGateApi::IsLocked(CancelBoard));
	CancelBoard->CancelBattleVisualSession(8104);
	TestTrue(TEXT("responsive Party Qi can refresh after visual-session cancellation"),
		RefreshPartyQiForResponsiveLayoutTest(*this, CancelBoard, FVector2D(1920.0f, 1080.0f)));
	TestEqual(TEXT("visual-session cancellation discards the retained Qi snapshot"),
		CancelPartyQiWidget ? CancelPartyQiWidget->GetSharedQiForTest() : INDEX_NONE,
		CancelQiAfterCommit);
	CancelBoard->AdvanceVisualsAtRealTime(100.0);
	TestFalse(TEXT("session cancellation discards terminal continuation without generating a reward"), CancelBoard->HasPendingRouteReward());
	TestEqual(TEXT("session cancellation never executes a discarded continuation"), FGateApi::Continuations(CancelBoard), 0);
	return true;
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
	TestTrue(TEXT("card-targeting fixture begins the Board-owned visual session"),
		Board->BeginBattleVisualSession(8201));
	double PresentationClock = 0.0;
	const auto DrainBoardPresentation = [&Board, &PresentationClock]()
	{
		Board->AdvanceVisualsAtRealTime(PresentationClock);
		PresentationClock += 100.0;
		Board->AdvanceVisualsAtRealTime(PresentationClock);
	};
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
	const UGameXXKBattleUnitVisualWidget* const OwnerVisual = Board->GetUnitVisualForTest(OwnerUnitId);
	TestNotNull(TEXT("the card owner keeps its persistent fixed-slot visual"), OwnerVisual);
	TestEqual(TEXT("the arrow begins at the owner's fixed stage center, never legacy actor projection"),
		Board->GetTargetingSourcePositionForTest(),
		OwnerVisual ? OwnerVisual->GetStageCenter() : FVector2D::ZeroVector);
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
	TestTrue(TEXT("the committed card locks later mutations until its presentation drains"),
		Board->IsBattlePresentationLockedForTest());
	TestEqual(TEXT("Party Qi retains its pre-commit baseline throughout the presentation"),
		PartyQiWidget->GetSharedQiForTest(), SharedQiBeforeCommit);
	TestEqual(TEXT("Party Qi retains its pre-commit number throughout the presentation"),
		PartyQiWidget->GetDisplayTextForTest(), FString::FromInt(SharedQiBeforeCommit));
	TestFalse(TEXT("end turn is rejected while the committed card presentation is locked"),
		Board->EndCardPlayerPhase());
	DrainBoardPresentation();
	TestFalse(TEXT("the committed card unlocks only after the full presentation drains"),
		Board->IsBattlePresentationLockedForTest());
	TestEqual(TEXT("Party Qi reconciles to authoritative shared energy after full drain"),
		PartyQiWidget->GetSharedQiForTest(), SharedQiAfterCommit);
	TestEqual(TEXT("Party Qi overlays authoritative shared energy after full drain"),
		PartyQiWidget->GetDisplayTextForTest(), FString::FromInt(SharedQiAfterCommit));
	TestFalse(TEXT("a committed card exits targeting state"), Board->IsCardTargetingForTest());
	TestFalse(TEXT("a committed card leaves the hand zone"), Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([CardInstanceId](const FGameXXKCardInstance& Card)
	{
		return Card.InstanceId == CardInstanceId;
	}));
	TestTrue(TEXT("a committed enemy-target card updates the scene-facing health projection"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP < EnemyHealthBeforePreview);
	const int32 RoundBeforeEndTurn = Subsystem->GetRuntimeState().CardRun.ActiveBattle.RoundNumber;
	TestTrue(TEXT("end turn only starts the saved enemy-intent presentation"), Board->EndCardPlayerPhase());
	if (Board->IsBattlePresentationLockedForTest())
	{
		DrainBoardPresentation();
	}
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
	TestTrue(TEXT("enemy intent damage locks its settle transition until presentation drain"),
		Board->IsBattlePresentationLockedForTest());
	DrainBoardPresentation();
	Board->AdvanceEnemyIntentPresentationForTest(0.32f);
	if (Board->IsBattlePresentationLockedForTest())
	{
		DrainBoardPresentation();
	}
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
	Board->CancelBattleVisualSession(8201);

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
