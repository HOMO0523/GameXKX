#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRelicCatalog.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
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
#include "UI/GameXXKCardOutcomePreviewWidget.h"
#include "UI/GameXXKBattleUnitVisualWidget.h"
#include "UObject/UObjectGlobals.h"

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
			Test.TestTrue(TEXT("compact Party Qi keeps the required 12-unit clearance above the expanded hand envelope within float tolerance"), CompactLayout.ExpandedHandRect.Min.Y - CompactLayout.QiRect.Max.Y >= 11.99f);

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
		FString& OutError,
		const int32 EnemyCount = 1)
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
			State.ActiveBattleEnemies.Reset();
			for (int32 EnemyIndex = 0; EnemyIndex < FMath::Clamp(EnemyCount, 1, 3); ++EnemyIndex)
			{
				State.ActiveBattleEnemies.Add(MakeEnemy(
					*FString::Printf(TEXT("PreviewEnemy%d"), EnemyIndex + 1),
					*FString::Printf(TEXT("预演敌人%d"), EnemyIndex + 1)));
			}

			FString Error;
			if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error)
				|| !FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, Seed, &Error))
			{
				OutError = Error;
				return false;
			}
			if (!State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
			{
				State.CardRun.ActiveBattle.Deck.Hand[0].CardId = TEXT("Route.General.PoJiaTuCi");
			}

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				if (CardInstance.CardId != FName(TEXT("Route.General.PoJiaTuCi")))
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
		FixtureCard.CardId = TEXT("Route.Boss.DuKouLieFeng");
		FixtureCard.OwnerUnitId = Hero->UnitId;
		State.CardRun.ActiveBattle.Deck.SharedEnergy = FMath::Max(
			State.CardRun.ActiveBattle.Deck.SharedEnergy,
			2);
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

	bool BuildBossRewardFixture(
		UGameXXKMVPSubsystem* Subsystem,
		FString& OutError)
	{
		OutError.Reset();
		if (!Subsystem
			|| !Subsystem->StartGame()
			|| !Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan())
			|| !Subsystem->AcceptQuest()
			|| !Subsystem->OpenDungeonFromTownExit())
		{
			OutError = TEXT("The boss fixture could not reach the dungeon.");
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		// The subsystem chain opens a generated route; collapse it to the fixed
		// Qingshan dungeon so the final boss node can begin directly.
		State.bHasGeneratedRouteMap = false;
		State.RouteMapNodes.Reset();
		State.RouteMapEdges.Reset();
		State.ReachableRouteNodeIds.Reset();
		State.VisitedRouteNodeIds.Reset();
		const TArray<EGameXXKNodeKind> FixedNodes = UGameXXKMVPRules::GetFixedDungeonNodes(0);
		const int32 BossIndex = FixedNodes.IndexOfByKey(EGameXXKNodeKind::Boss);
		if (BossIndex == INDEX_NONE)
		{
			OutError = TEXT("The fixed dungeon has no boss node.");
			return false;
		}
		State.DungeonNodeIndex = BossIndex;
		if (!Subsystem->SelectDungeonNode(EGameXXKNodeKind::Boss)
			|| !State.CardRun.bHasActiveCardBattle)
		{
			OutError = TEXT("The boss node did not create an active card battle.");
			return false;
		}
		return true;
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
			Card.bTemporary = true;
			Card.EnergyCostOverride = 1;
			Card.ManaCostOverride = 3;
			Card.ExpireAfterPlayerRound = 1;
		}
		return Cards;
	}

	bool BuildBoardPresentationGateFixture(
		UGameXXKMVPSubsystem* const Subsystem,
		FName& OutCardInstanceId,
		FString& OutError,
		const FName CardId = TEXT("Npc.JinGui.ShiJingErMu"),
		const bool bAddReflection = true)
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
			MakeBoardPresentationLegacyUnit(TEXT("Npc.JinGui"), TEXT("金桂"), false, 20)};
		State.ActiveBattleEnemies = {
			MakeBoardPresentationLegacyUnit(TEXT("Enemy"), TEXT("反击敌人"), true, 10)};

		TArray<FGameXXKCardCombatUnit> Units = {
			MakeBoardPresentationUnit(TEXT("Npc.JinGui"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Invalid, 20, 0),
			MakeBoardPresentationUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10, 0)};
		FGameXXKCardBattleRuntime Runtime;
		if (!GameXXKCardRules::InitializeCardBattleRuntime(
			Runtime,
			MakeBoardPresentationCards(CardId, TEXT("Npc.JinGui")),
			Units,
			EGameXXKCardTerrain::Plain,
			8801,
			&OutError))
		{
			return false;
		}
		if (bAddReflection)
		{
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
		}

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

	template <typename TBoard, typename = void>
	struct TPlayedCardCommitApi
	{
		static constexpr bool bAvailable = false;
		static bool IsActive(const TBoard*) { return false; }
		static FName InstanceId(const TBoard*) { return NAME_None; }
		static double Elapsed(const TBoard*) { return 0.0; }
		static FVector2D Translation(const TBoard*) { return FVector2D::ZeroVector; }
		static FVector2D Scale(const TBoard*) { return FVector2D(1.0f, 1.0f); }
		static float Opacity(const TBoard*) { return 1.0f; }
		static int32 CompletionCount(const TBoard*) { return 0; }
	};

	template <typename TBoard>
	struct TPlayedCardCommitApi<TBoard, std::void_t<
		decltype(std::declval<const TBoard&>().IsPlayedCardCommitActiveForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitInstanceIdForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitElapsedForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitTranslationForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitScaleForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitOpacityForTest()),
		decltype(std::declval<const TBoard&>().GetPlayedCardCommitCompletionCountForTest())>>
	{
		static constexpr bool bAvailable = true;
		static bool IsActive(const TBoard* Board) { return Board->IsPlayedCardCommitActiveForTest(); }
		static FName InstanceId(const TBoard* Board) { return Board->GetPlayedCardCommitInstanceIdForTest(); }
		static double Elapsed(const TBoard* Board) { return Board->GetPlayedCardCommitElapsedForTest(); }
		static FVector2D Translation(const TBoard* Board) { return Board->GetPlayedCardCommitTranslationForTest(); }
		static FVector2D Scale(const TBoard* Board) { return Board->GetPlayedCardCommitScaleForTest(); }
		static float Opacity(const TBoard* Board) { return Board->GetPlayedCardCommitOpacityForTest(); }
		static int32 CompletionCount(const TBoard* Board) { return Board->GetPlayedCardCommitCompletionCountForTest(); }
	};

	bool BuildPureEnemyGroupCardFixture(
		UGameXXKMVPSubsystem* const Subsystem,
		FName& OutCardInstanceId,
		FGameXXKCardPlayPreview& OutPlayability,
		FGameXXKCardOutcomePreview& OutOutcome,
		FString& OutError,
		const int32 EnemyCount = 3)
	{
		OutCardInstanceId = NAME_None;
		OutPlayability = FGameXXKCardPlayPreview();
		OutOutcome = FGameXXKCardOutcomePreview();
		OutError.Reset();
		if (!Subsystem)
		{
			OutError = TEXT("The pure-group test subsystem is missing.");
			return false;
		}

		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 23;
		State.ActiveBattleEnemies.Reset();
		for (int32 EnemyIndex = 0; EnemyIndex < FMath::Clamp(EnemyCount, 1, 3); ++EnemyIndex)
		{
			State.ActiveBattleEnemies.Add(MakeEnemy(
				*FString::Printf(TEXT("GroupPreviewEnemy%d"), EnemyIndex + 1),
				*FString::Printf(TEXT("群攻敌人%d"), EnemyIndex + 1)));
		}

		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &OutError)
			|| !FGameXXKCardBattleAdapter::BeginCardBattle(
				State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 37, &OutError)
			|| State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			return false;
		}

		FGameXXKCardInstance& Card = State.CardRun.ActiveBattle.Deck.Hand[0];
		const TArray<FName> SorcererTaskCardIds = {
			TEXT("Profession.Sorcerer.BaoYanShu"),
			TEXT("Profession.Sorcerer.LiHuoYin"),
			TEXT("Profession.Sorcerer.YanQiang"),
			TEXT("Profession.Sorcerer.XingHuoLiaoYuan"),
			TEXT("Profession.Sorcerer.ChiXiaoFenXing")};
		TArray<FGameXXKCardInstance*> CarriedInstances;
		for (FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.Hand)
		{
			CarriedInstances.Add(&Instance);
		}
		for (FGameXXKCardInstance& Instance : State.CardRun.ActiveBattle.Deck.DrawPile)
		{
			CarriedInstances.Add(&Instance);
		}
		if (CarriedInstances.Num() < SorcererTaskCardIds.Num())
		{
			OutError = TEXT("The pure-group fixture has fewer than five carried instances.");
			return false;
		}
		for (int32 CardIndex = 0; CardIndex < SorcererTaskCardIds.Num(); ++CardIndex)
		{
			CarriedInstances[CardIndex]->CardId = SorcererTaskCardIds[CardIndex];
			CarriedInstances[CardIndex]->OwnerUnitId = Card.OwnerUnitId;
			CarriedInstances[CardIndex]->bTemporary = false;
		}
		OutCardInstanceId = Card.InstanceId;
		if (FGameXXKCardCombatUnit* const Owner = State.CardRun.ActiveBattle.Units.FindByPredicate(
			[&Card](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == Card.OwnerUnitId; }))
		{
			Owner->Role = EGameXXKCharacterRole::Sorcerer;
			Owner->Mana = 99;
			Owner->MaxMana = 99;
			Owner->Attack = FMath::Max(Owner->Attack, 30);
		}
		State.CardRun.ActiveBattle.Deck.SharedEnergy = 99;

		return FGameXXKCardBattleAdapter::BuildCardPlayPreview(
				State, OutCardInstanceId, OutPlayability, &OutError)
			&& OutPlayability.bCanPlay
			&& !OutPlayability.TargetRequest.bRequiresManualSelection
			&& OutPlayability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies
			&& FGameXXKCardOutcomePreviewRules::Build(
				State, OutCardInstanceId, NAME_None, OutOutcome, &OutError)
			&& OutOutcome.Classification == EGameXXKCardOutcomePreviewClass::PureEnemyGroup
			&& OutOutcome.EnemyPositionLines.Num() == FMath::Clamp(EnemyCount, 1, 3);
	}

	struct FCanvasLayoutSnapshot
	{
		FVector2D AnchorMinimum = FVector2D::ZeroVector;
		FVector2D AnchorMaximum = FVector2D::ZeroVector;
		FVector2D Alignment = FVector2D::ZeroVector;
		FMargin Offsets;
		FVector2D Size = FVector2D::ZeroVector;
		int32 ZOrder = INDEX_NONE;
	};

	TMap<FName, FCanvasLayoutSnapshot> CaptureNonOutcomeCanvasLayout(const UGameXXKBattleBoardWidget* const Board)
	{
		TMap<FName, FCanvasLayoutSnapshot> Result;
		if (!Board || !Board->WidgetTree)
		{
			return Result;
		}
		TArray<UWidget*> Widgets;
		Board->WidgetTree->GetAllWidgets(Widgets);
		for (UWidget* const Widget : Widgets)
		{
			if (!Widget
				|| Widget->GetName().Contains(TEXT("OutcomePreview"))
				|| Widget->GetName().Contains(TEXT("HandCardDetailPanel")))
			{
				continue;
			}
			const UCanvasPanelSlot* const CanvasSlot = Cast<UCanvasPanelSlot>(Widget->Slot);
			if (!CanvasSlot)
			{
				continue;
			}
			FCanvasLayoutSnapshot& Snapshot = Result.Add(Widget->GetFName());
			Snapshot.AnchorMinimum = CanvasSlot->GetAnchors().Minimum;
			Snapshot.AnchorMaximum = CanvasSlot->GetAnchors().Maximum;
			Snapshot.Alignment = CanvasSlot->GetAlignment();
			Snapshot.Offsets = CanvasSlot->GetOffsets();
			Snapshot.Size = CanvasSlot->GetSize();
			Snapshot.ZOrder = CanvasSlot->GetZOrder();
		}
		return Result;
	}

	bool AssertCanvasLayoutUnchanged(
		FAutomationTestBase& Test,
		const TMap<FName, FCanvasLayoutSnapshot>& Before,
		const TMap<FName, FCanvasLayoutSnapshot>& After)
	{
		bool bEqual = Test.TestEqual(TEXT("functional hover preserves the number of pre-existing Canvas slots"), After.Num(), Before.Num());
		for (const TPair<FName, FCanvasLayoutSnapshot>& Pair : Before)
		{
			const FCanvasLayoutSnapshot* const Actual = After.Find(Pair.Key);
			bEqual &= Test.TestNotNull(*FString::Printf(TEXT("Canvas slot %s remains present"), *Pair.Key.ToString()), Actual);
			if (!Actual)
			{
				continue;
			}
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s anchor minimum is invariant"), *Pair.Key.ToString()), Actual->AnchorMinimum, Pair.Value.AnchorMinimum);
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s anchor maximum is invariant"), *Pair.Key.ToString()), Actual->AnchorMaximum, Pair.Value.AnchorMaximum);
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s alignment is invariant"), *Pair.Key.ToString()), Actual->Alignment, Pair.Value.Alignment);
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s offsets are invariant"), *Pair.Key.ToString()), Actual->Offsets, Pair.Value.Offsets);
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s size is invariant"), *Pair.Key.ToString()), Actual->Size, Pair.Value.Size);
			bEqual &= Test.TestEqual(*FString::Printf(TEXT("%s z-order is invariant"), *Pair.Key.ToString()), Actual->ZOrder, Pair.Value.ZOrder);
		}
		return bEqual;
	}

	bool AssertOutcomeCleared(FAutomationTestBase& Test, const UGameXXKBattleBoardWidget* const Board, const TCHAR* Context)
	{
		if (!Board)
		{
			Test.AddError(FString::Printf(TEXT("%s: Board is missing"), Context));
			return false;
		}
		bool bClear = Test.TestFalse(*FString::Printf(TEXT("%s hides both preview widgets"), Context), Board->IsCardOutcomePreviewVisibleForTest());
		bClear &= Test.TestEqual(*FString::Printf(TEXT("%s clears cached card"), Context), Board->GetCardOutcomePreviewCardInstanceIdForTest(), NAME_None);
		bClear &= Test.TestEqual(*FString::Printf(TEXT("%s clears cached target"), Context), Board->GetCardOutcomePreviewTargetUnitIdForTest(), NAME_None);
		bClear &= Test.TestEqual(*FString::Printf(TEXT("%s clears visible lines"), Context), Board->GetCardOutcomePreviewLinesForTest().Num(), 0);
		return bClear;
	}

	TArray<FString> FlattenOutcomeLines(const TArray<FGameXXKCardOutcomeTextLine>& Lines)
	{
		TArray<FString> Result;
		for (const FGameXXKCardOutcomeTextLine& Line : Lines)
		{
			FString& PlainLine = Result.AddDefaulted_GetRef();
			for (const FGameXXKCardOutcomeTextSegment& Segment : Line.Segments)
			{
				PlainLine += Segment.Text.ToString();
			}
		}
		return Result;
	}

	int32 GetPreviewHealthDamage(const FGameXXKCardOutcomeTarget& Target)
	{
		return Target.DirectDamage
			+ Target.GroupDamage
			+ Target.BleedDamage
			+ Target.PoisonDamage
			+ Target.BurnDamage
			+ Target.ToxicExplosionDamage
			+ Target.MedicineDamage
			+ Target.LinkedDamage;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPlayedCardCommitPresentationTest,
	"GameXXK.Integration.CardBattle.PlayedCardCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPlayedCardCommitPresentationTest::RunTest(const FString& Parameters)
{
	using FCommitApi = TPlayedCardCommitApi<UGameXXKBattleBoardWidget>;
	using FGateApi = TBoardPresentationGateApi<UGameXXKBattleBoardWidget>;
	TestTrue(TEXT("Board exposes deterministic played-card commit diagnostics"), FCommitApi::bAvailable);
	if (!FCommitApi::bAvailable || !FGateApi::bAvailable)
	{
		return false;
	}

	FString Error;
	UGameInstance* const ManualGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const ManualSubsystem = NewObject<UGameXXKMVPSubsystem>(ManualGameInstance);
	FName ManualCardInstanceId;
	if (!TestTrue(TEXT("manual commit fixture builds the reflected multi-packet card battle"),
		BuildBoardPresentationGateFixture(ManualSubsystem, ManualCardInstanceId, Error)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const ManualBoard = NewObject<UGameXXKBattleBoardWidget>();
	ManualBoard->SetMVPSubsystem(ManualSubsystem);
	TestTrue(TEXT("manual commit Board initializes"), ManualBoard->Initialize());
	ManualBoard->NativeConstruct();
	TestTrue(TEXT("manual commit Board begins its visual session"), ManualBoard->BeginBattleVisualSession(8301));
	UButton* const ManualSourceButton = ManualBoard->WidgetTree
		? Cast<UButton>(ManualBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00")))
		: nullptr;
	USizeBox* const ManualSourceSize = ManualBoard->WidgetTree
		? Cast<USizeBox>(ManualBoard->WidgetTree->FindWidget(TEXT("BattleHandCardSize_00")))
		: nullptr;
	TestNotNull(TEXT("manual commit keeps the real source hand button"), ManualSourceButton);
	TestNotNull(TEXT("manual commit keeps the approved source size box"), ManualSourceSize);
	const FWidgetTransform InitialTransform = ManualSourceButton
		? ManualSourceButton->GetRenderTransform()
		: FWidgetTransform();
	const float InitialOpacity = ManualSourceButton ? ManualSourceButton->GetRenderOpacity() : 1.0f;
	const int32 InitialHandChildCount = ManualBoard->GetHandCardBoxForTest()
		? ManualBoard->GetHandCardBoxForTest()->GetChildrenCount()
		: INDEX_NONE;

	TestTrue(TEXT("manual source card enters targeting without starting commit"),
		ManualBoard->ClickCardInHand(ManualCardInstanceId));
	TestFalse(TEXT("an illegal target cannot start played-card commit"),
		ManualBoard->ConfirmTargetingUnit(TEXT("Missing.Target")));
	TestFalse(TEXT("failed target confirmation leaves commit inactive"), FCommitApi::IsActive(ManualBoard));
	TestEqual(TEXT("failed target confirmation does not increment completion"), FCommitApi::CompletionCount(ManualBoard), 0);
	ManualBoard->AdvanceHandCardHoverMotionForTest(1.0f);
	TestTrue(TEXT("targeting presents the selected card at its approved one-point-two scale before release"),
		ManualSourceButton
		&& ManualSourceButton->GetRenderTransform().Scale.Equals(FVector2D(1.20f, 1.20f), 0.001f));
	TestTrue(TEXT("valid manual target commits the card"), ManualBoard->ConfirmTargetingUnit(TEXT("Enemy")));
	TestTrue(TEXT("successful manual target starts commit immediately"), FCommitApi::IsActive(ManualBoard));
	TestEqual(TEXT("manual commit retains the resolved source instance"), FCommitApi::InstanceId(ManualBoard), ManualCardInstanceId);
	TestTrue(TEXT("commit preserves the already-selected card scale instead of shrinking on release"),
		FCommitApi::Scale(ManualBoard).X >= 1.20f - KINDA_SMALL_NUMBER);
	TestEqual(TEXT("multi-packet base, reflection, and follow-up queue before commit finishes"),
		ManualBoard->GetBattlePresentationQueueCountForTest(), 3);
	TestEqual(TEXT("damage presentation cannot start before the commit timeline"),
		ManualBoard->GetActiveBattlePresentationEventIdForTest(), static_cast<uint64>(0));
	TestTrue(TEXT("commit locks the whole Board"), FGateApi::IsLocked(ManualBoard));
	TestEqual(TEXT("commit never changes the approved hand width"),
		ManualSourceSize ? ManualSourceSize->GetWidthOverride() : 0.0f, 206.0f);
	TestEqual(TEXT("commit never changes the approved hand height"),
		ManualSourceSize ? ManualSourceSize->GetHeightOverride() : 0.0f, 285.0f);
	TestEqual(TEXT("commit never inserts or removes layout slots"),
		ManualBoard->GetHandCardBoxForTest() ? ManualBoard->GetHandCardBoxForTest()->GetChildrenCount() : INDEX_NONE,
		InitialHandChildCount);
	TestTrue(TEXT("commit retains the original source button in its slot"),
		ManualBoard->WidgetTree
		&& ManualBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00")) == ManualSourceButton);

	ManualBoard->AdvanceVisualsAtRealTime(0.0);
	TestEqual(TEXT("the first absolute sample starts commit at zero elapsed"), FCommitApi::Elapsed(ManualBoard), 0.0);
	ManualBoard->AdvanceVisualsAtRealTime(0.09);
	TestTrue(TEXT("ease-out commit lifts the card during the first half"),
		FCommitApi::Translation(ManualBoard).Y < InitialTransform.Translation.Y - 1.0f);
	TestTrue(TEXT("ease-out commit keeps scaling outward from the selected-card pose"),
		FCommitApi::Scale(ManualBoard).X > 1.20f);
	const FVector2D MidCommitTranslation = FCommitApi::Translation(ManualBoard);
	const FVector2D MidCommitScale = FCommitApi::Scale(ManualBoard);
	ManualBoard->AdvanceHandCardHoverMotionForTest(1.0f);
	TestEqual(TEXT("hover motion cannot overwrite active commit translation"),
		FCommitApi::Translation(ManualBoard), MidCommitTranslation);
	TestEqual(TEXT("hover motion cannot overwrite active commit scale"),
		FCommitApi::Scale(ManualBoard), MidCommitScale);
	TestEqual(TEXT("commit opacity stays intact through its first half"),
		FCommitApi::Opacity(ManualBoard), InitialOpacity);
	TestEqual(TEXT("damage presentation still has not started at half commit"),
		ManualBoard->GetActiveBattlePresentationEventIdForTest(), static_cast<uint64>(0));
	ManualBoard->AdvanceVisualsAtRealTime(0.179);
	TestTrue(TEXT("the second half fades the committed source card"), FCommitApi::Opacity(ManualBoard) < 0.05f);
	TestTrue(TEXT("commit remains active strictly before 0.18 seconds"), FCommitApi::IsActive(ManualBoard));
	TestEqual(TEXT("damage presentation remains gated strictly before 0.18 seconds"),
		ManualBoard->GetActiveBattlePresentationEventIdForTest(), static_cast<uint64>(0));
	ManualBoard->AdvanceVisualsAtRealTime(0.18);
	TestFalse(TEXT("commit completes exactly at 0.18 seconds"), FCommitApi::IsActive(ManualBoard));
	TestEqual(TEXT("one active hand play completes exactly one commit"), FCommitApi::CompletionCount(ManualBoard), 1);
	TestTrue(TEXT("the first damage packet starts only after commit completion"),
		ManualBoard->GetActiveBattlePresentationEventIdForTest() != 0);
	TestEqual(TEXT("normal completion returns the reusable hand slot to identity scale"),
		ManualSourceButton ? ManualSourceButton->GetRenderTransform().Scale : FVector2D::ZeroVector,
		FVector2D(1.0f, 1.0f));
	TestEqual(TEXT("normal completion returns the reusable hand slot to identity translation"),
		ManualSourceButton ? ManualSourceButton->GetRenderTransform().Translation : FVector2D(1.0f, 1.0f),
		FVector2D::ZeroVector);
	TestEqual(TEXT("the spent source card stays hidden while its damage queue is still playing"),
		ManualSourceButton ? ManualSourceButton->GetRenderOpacity() : 1.0f,
		0.0f);
	ManualBoard->AdvanceVisualsAtRealTime(0.48);
	TestEqual(TEXT("later frames cannot flash the spent source card back during damage presentation"),
		ManualSourceButton ? ManualSourceButton->GetRenderOpacity() : 1.0f,
		0.0f);
	ManualBoard->AdvanceVisualsAtRealTime(100.0);
	TestEqual(TEXT("automatic follow-up packets never create extra hand commits"), FCommitApi::CompletionCount(ManualBoard), 1);
	TestEqual(TEXT("the active-card continuation still executes exactly once"), FGateApi::Continuations(ManualBoard), 1);
	TestTrue(TEXT("the deferred hand refresh makes the next real card in the reused slot visible"),
		ManualSourceButton && ManualSourceButton->GetRenderOpacity() >= 0.58f);

	UGameInstance* const AutomaticGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const AutomaticSubsystem = NewObject<UGameXXKMVPSubsystem>(AutomaticGameInstance);
	FName AutomaticCardInstanceId;
	if (!TestTrue(TEXT("automatic no-damage fixture builds a status-and-resource card battle"),
		BuildBoardPresentationGateFixture(
			AutomaticSubsystem,
			AutomaticCardInstanceId,
			Error,
			TEXT("Npc.JinGui.HouXiangTuoShen"),
			false)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const AutomaticBoard = NewObject<UGameXXKBattleBoardWidget>();
	AutomaticBoard->SetMVPSubsystem(AutomaticSubsystem);
	TestTrue(TEXT("automatic commit Board initializes"), AutomaticBoard->Initialize());
	AutomaticBoard->NativeConstruct();
	TestTrue(TEXT("automatic commit Board begins its visual session"), AutomaticBoard->BeginBattleVisualSession(8302));
	TestTrue(TEXT("automatic target card resolves from one hand click"), AutomaticBoard->ClickCardInHand(AutomaticCardInstanceId));
	TestTrue(TEXT("automatic target card starts the same commit feedback"), FCommitApi::IsActive(AutomaticBoard));
	TestEqual(TEXT("automatic commit retains the resolved source instance"), FCommitApi::InstanceId(AutomaticBoard), AutomaticCardInstanceId);
	TestEqual(TEXT("status-only automatic card owns no damage queue"), AutomaticBoard->GetBattlePresentationQueueCountForTest(), 0);
	TestEqual(TEXT("status-only continuation cannot execute before commit"), FGateApi::Continuations(AutomaticBoard), 0);
	AutomaticBoard->AdvanceVisualsAtRealTime(4.0);
	AutomaticBoard->AdvanceVisualsAtRealTime(4.179);
	TestTrue(TEXT("status-only commit remains pending before its boundary"), FCommitApi::IsActive(AutomaticBoard));
	TestEqual(TEXT("status-only continuation remains deferred before its boundary"), FGateApi::Continuations(AutomaticBoard), 0);
	AutomaticBoard->AdvanceVisualsAtRealTime(4.18);
	TestFalse(TEXT("status-only commit drains at its boundary"), FCommitApi::IsActive(AutomaticBoard));
	TestEqual(TEXT("status-only continuation executes exactly once after commit"), FGateApi::Continuations(AutomaticBoard), 1);

	UGameInstance* const FailedGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const FailedSubsystem = NewObject<UGameXXKMVPSubsystem>(FailedGameInstance);
	FName FailedCardInstanceId;
	if (!TestTrue(TEXT("failed-play fixture builds an automatic card battle"),
		BuildBoardPresentationGateFixture(
			FailedSubsystem,
			FailedCardInstanceId,
			Error,
			TEXT("Npc.JinGui.HouXiangTuoShen"),
			false)))
	{
		AddError(Error);
		return false;
	}
	FailedSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.SharedEnergy = 0;
	UGameXXKBattleBoardWidget* const FailedBoard = NewObject<UGameXXKBattleBoardWidget>();
	FailedBoard->SetMVPSubsystem(FailedSubsystem);
	TestTrue(TEXT("failed-play Board initializes"), FailedBoard->Initialize());
	FailedBoard->NativeConstruct();
	TestTrue(TEXT("failed-play Board begins its visual session"), FailedBoard->BeginBattleVisualSession(8303));
	TestFalse(TEXT("unaffordable automatic card is rejected"), FailedBoard->ClickCardInHand(FailedCardInstanceId));
	TestFalse(TEXT("rejected card never starts commit"), FCommitApi::IsActive(FailedBoard));
	TestEqual(TEXT("rejected card never increments commit completion"), FCommitApi::CompletionCount(FailedBoard), 0);

	UGameInstance* const CancelGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const CancelSubsystem = NewObject<UGameXXKMVPSubsystem>(CancelGameInstance);
	FName CancelCardInstanceId;
	if (!TestTrue(TEXT("commit-cancel fixture builds a manual card battle"),
		BuildBoardPresentationGateFixture(CancelSubsystem, CancelCardInstanceId, Error)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const CancelBoard = NewObject<UGameXXKBattleBoardWidget>();
	CancelBoard->SetMVPSubsystem(CancelSubsystem);
	TestTrue(TEXT("commit-cancel Board initializes"), CancelBoard->Initialize());
	CancelBoard->NativeConstruct();
	TestTrue(TEXT("commit-cancel Board begins its visual session"), CancelBoard->BeginBattleVisualSession(8304));
	UButton* const CancelSourceButton = CancelBoard->WidgetTree
		? Cast<UButton>(CancelBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00")))
		: nullptr;
	const FWidgetTransform CancelInitialTransform = CancelSourceButton
		? CancelSourceButton->GetRenderTransform()
		: FWidgetTransform();
	const float CancelInitialOpacity = CancelSourceButton ? CancelSourceButton->GetRenderOpacity() : 1.0f;
	TestTrue(TEXT("commit-cancel card enters targeting"), CancelBoard->ClickCardInHand(CancelCardInstanceId));
	TestTrue(TEXT("commit-cancel card resolves"), CancelBoard->ConfirmTargetingUnit(TEXT("Enemy")));
	CancelBoard->AdvanceVisualsAtRealTime(8.0);
	CancelBoard->AdvanceVisualsAtRealTime(8.09);
	TestTrue(TEXT("commit-cancel fixture reaches a transformed active frame"), FCommitApi::IsActive(CancelBoard));
	CancelBoard->CancelBattleVisualSession(8304);
	TestFalse(TEXT("visual-session cancellation clears commit"), FCommitApi::IsActive(CancelBoard));
	TestEqual(TEXT("visual-session cancellation restores the exact initial transform"),
		CancelSourceButton ? CancelSourceButton->GetRenderTransform() : FWidgetTransform(),
		CancelInitialTransform);
	TestEqual(TEXT("visual-session cancellation restores the exact initial opacity"),
		CancelSourceButton ? CancelSourceButton->GetRenderOpacity() : 0.0f,
		CancelInitialOpacity);
	CancelBoard->AdvanceVisualsAtRealTime(100.0);
	TestEqual(TEXT("visual-session cancellation discards continuation without executing it"), FGateApi::Continuations(CancelBoard), 0);

	UGameInstance* const TeardownGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const TeardownSubsystem = NewObject<UGameXXKMVPSubsystem>(TeardownGameInstance);
	FName TeardownCardInstanceId;
	if (!TestTrue(TEXT("commit-teardown fixture builds an automatic card battle"),
		BuildBoardPresentationGateFixture(
			TeardownSubsystem,
			TeardownCardInstanceId,
			Error,
			TEXT("Npc.JinGui.HouXiangTuoShen"),
			false)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const TeardownBoard = NewObject<UGameXXKBattleBoardWidget>();
	TeardownBoard->SetMVPSubsystem(TeardownSubsystem);
	TestTrue(TEXT("commit-teardown Board initializes"), TeardownBoard->Initialize());
	TeardownBoard->NativeConstruct();
	TestTrue(TEXT("commit-teardown Board begins its visual session"), TeardownBoard->BeginBattleVisualSession(8305));
	UButton* const TeardownSourceButton = TeardownBoard->WidgetTree
		? Cast<UButton>(TeardownBoard->WidgetTree->FindWidget(TEXT("BattleHandCard_00")))
		: nullptr;
	const FWidgetTransform TeardownInitialTransform = TeardownSourceButton
		? TeardownSourceButton->GetRenderTransform()
		: FWidgetTransform();
	const float TeardownInitialOpacity = TeardownSourceButton ? TeardownSourceButton->GetRenderOpacity() : 1.0f;
	TestTrue(TEXT("commit-teardown automatic card resolves"), TeardownBoard->ClickCardInHand(TeardownCardInstanceId));
	TeardownBoard->AdvanceVisualsAtRealTime(12.0);
	TeardownBoard->AdvanceVisualsAtRealTime(12.09);
	TestTrue(TEXT("commit-teardown fixture reaches a transformed active frame"), FCommitApi::IsActive(TeardownBoard));
	TeardownBoard->NativeDestruct();
	TestFalse(TEXT("widget teardown clears commit"), FCommitApi::IsActive(TeardownBoard));
	TestEqual(TEXT("widget teardown restores the exact initial transform"),
		TeardownSourceButton ? TeardownSourceButton->GetRenderTransform() : FWidgetTransform(),
		TeardownInitialTransform);
	TestEqual(TEXT("widget teardown restores the exact initial opacity"),
		TeardownSourceButton ? TeardownSourceButton->GetRenderOpacity() : 0.0f,
		TeardownInitialOpacity);
	TestEqual(TEXT("widget teardown never executes the discarded continuation"), FGateApi::Continuations(TeardownBoard), 0);

	UGameInstance* const LargeDeltaGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const LargeDeltaSubsystem = NewObject<UGameXXKMVPSubsystem>(LargeDeltaGameInstance);
	FName LargeDeltaCardInstanceId;
	FName LargeDeltaTargetUnitId;
	FName LargeDeltaOwnerUnitId;
	if (!TestTrue(TEXT("large-delta fixture enters a one-health route battle"),
		BuildRouteRewardFixture(
			LargeDeltaSubsystem,
			LargeDeltaCardInstanceId,
			LargeDeltaTargetUnitId,
			LargeDeltaOwnerUnitId,
			Error,
			true)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const LargeDeltaBoard = NewObject<UGameXXKBattleBoardWidget>();
	LargeDeltaBoard->SetMVPSubsystem(LargeDeltaSubsystem);
	TestTrue(TEXT("large-delta Board initializes"), LargeDeltaBoard->Initialize());
	LargeDeltaBoard->NativeConstruct();
	TestTrue(TEXT("large-delta Board begins its visual session"), LargeDeltaBoard->BeginBattleVisualSession(8306));
	TestTrue(TEXT("large-delta card enters targeting"), LargeDeltaBoard->ClickCardInHand(LargeDeltaCardInstanceId));
	TestTrue(TEXT("large-delta card commits"), LargeDeltaBoard->ConfirmTargetingUnit(LargeDeltaTargetUnitId));
	LargeDeltaBoard->AdvanceVisualsAtRealTime(20.0);
	LargeDeltaBoard->AdvanceVisualsAtRealTime(100.0);
	TestFalse(TEXT("one large frame drains commit, hit, and death without leaving commit active"),
		FCommitApi::IsActive(LargeDeltaBoard));
	TestEqual(TEXT("one large frame completes the source card exactly once"),
		FCommitApi::CompletionCount(LargeDeltaBoard),
		1);
	TestEqual(TEXT("one large frame drains every queued damage and death entry"),
		LargeDeltaBoard->GetBattlePresentationQueueCountForTest(),
		0);
	TestTrue(TEXT("one large frame reaches the terminal route reward"), LargeDeltaBoard->HasPendingRouteReward());
	TestEqual(TEXT("one large frame executes the terminal continuation exactly once"),
		FGateApi::Continuations(LargeDeltaBoard),
		1);

	UGameInstance* const NoSessionGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const NoSessionSubsystem = NewObject<UGameXXKMVPSubsystem>(NoSessionGameInstance);
	FName NoSessionCardInstanceId;
	if (!TestTrue(TEXT("no-session fixture builds a reflected damage card battle"),
		BuildBoardPresentationGateFixture(NoSessionSubsystem, NoSessionCardInstanceId, Error)))
	{
		AddError(Error);
		return false;
	}
	UGameXXKBattleBoardWidget* const NoSessionBoard = NewObject<UGameXXKBattleBoardWidget>();
	NoSessionBoard->SetMVPSubsystem(NoSessionSubsystem);
	TestTrue(TEXT("no-session Board initializes"), NoSessionBoard->Initialize());
	NoSessionBoard->NativeConstruct();
	TestTrue(TEXT("no-session card enters targeting"), NoSessionBoard->ClickCardInHand(NoSessionCardInstanceId));
	TestTrue(TEXT("no-session card still commits authoritative gameplay"),
		NoSessionBoard->ConfirmTargetingUnit(TEXT("Enemy")));
	TestFalse(TEXT("a missing visual session never starts played-card commit"), FCommitApi::IsActive(NoSessionBoard));
	TestTrue(TEXT("a missing visual session initially leaves the queued presentation locked"),
		FGateApi::IsLocked(NoSessionBoard));
	NoSessionBoard->RefreshFromState();
	TestFalse(TEXT("refresh safely downgrades a presentation that has no visual session"),
		FGateApi::IsLocked(NoSessionBoard));
	TestEqual(TEXT("no-session downgrade executes the committed continuation exactly once"),
		FGateApi::Continuations(NoSessionBoard),
		1);
	NoSessionBoard->RefreshFromState();
	TestEqual(TEXT("later refreshes never repeat the no-session continuation"),
		FGateApi::Continuations(NoSessionBoard),
		1);
	return true;
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
	BlockingEvent.AttackerUnitId = TEXT("Npc.JinGui");
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
	GateBoard->AdvanceVisualsAtRealTime(0.301);
	TestEqual(TEXT("the marker exposes the packet-local intermediate health"), GateBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 90);
	GateBoard->AdvanceVisualsAtRealTime(0.821);
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
	TestEqual(TEXT("the Board enqueues two Heavy Arrow packets and the reflected packet before refresh"),
		OrderedBoard->GetBattlePresentationQueueCountForTest(),
		3);
	OrderedBoard->AdvanceVisualsAtRealTime(0.0);
	OrderedBoard->AdvanceVisualsAtRealTime(0.18);
	TestEqual(TEXT("packet one retains the Heavy Arrow attacker"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Npc.JinGui")));
	TestEqual(TEXT("packet one retains the primary target"), FGateApi::Target(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("the first target baseline is seeded from packet one"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 100);
	OrderedBoard->AdvanceVisualsAtRealTime(0.481);
	TestEqual(TEXT("packet one marker applies only packet one's health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 89);
	OrderedBoard->AdvanceVisualsAtRealTime(1.001);
	TestEqual(TEXT("packet two reverses the reflected source"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("packet two reverses the reflected target"), FGateApi::Target(OrderedBoard), FName(TEXT("Npc.JinGui")));
	TestEqual(TEXT("packet one's target override survives the reflected intermediate entry"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 89);
	OrderedBoard->AdvanceVisualsAtRealTime(1.101);
	TestEqual(TEXT("the reflection marker applies its own target health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Npc.JinGui")), 95);
	OrderedBoard->AdvanceVisualsAtRealTime(1.301);
	TestEqual(TEXT("packet three returns to the Heavy Arrow attacker"), FGateApi::Attacker(OrderedBoard), FName(TEXT("Npc.JinGui")));
	TestEqual(TEXT("packet three returns to the primary target"), FGateApi::Target(OrderedBoard), FName(TEXT("Enemy")));
	TestEqual(TEXT("packet three begins at packet one's committed target health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 89);
	OrderedBoard->AdvanceVisualsAtRealTime(1.401);
	TestEqual(TEXT("packet three marker reaches the final target health without early reconciliation"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 78);
	OrderedBoard->AdvanceVisualsAtRealTime(1.601);
	TestFalse(TEXT("the ordered batch unlocks after all three packets"), FGateApi::IsLocked(OrderedBoard));
	TestEqual(TEXT("ordered target HUD reconciles to authoritative final health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Enemy")), 78);
	TestEqual(TEXT("reflected target HUD reconciles to authoritative final health"), OrderedBoard->GetDisplayedHealthForTest(TEXT("Npc.JinGui")), 95);
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
	const FGameXXKCardCombatUnit* const LethalTargetAfterCommit =
		LethalSubsystem->GetRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate(
			[LethalTargetUnitId](const FGameXXKCardCombatUnit& Unit)
			{
				return Unit.UnitId == LethalTargetUnitId;
			});
	TestTrue(TEXT("lethal direct damage consumes the target vulnerability in authoritative state"),
		LethalTargetAfterCommit
		&& GameXXKCardRules::GetCombatStatusStacks(
			*LethalTargetAfterCommit,
			EGameXXKCardStatus::Vulnerability) == 0);
	TestFalse(TEXT("terminal reward handling is deferred until after Death"), LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(0.0);
	LethalBoard->AdvanceVisualsAtRealTime(1.001);
	TestTrue(TEXT("lethal Hit transitions to Death before removal"), LethalBoard->IsBattleDeathPresentationActiveForTest());
	TestFalse(TEXT("reward remains deferred throughout Death"), LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(1.901);
	TestFalse(TEXT("fixed-HUD status reconciliation does not open a separate full-screen status presentation"),
		LethalBoard->IsBattleStatusPresentationActiveForTest());
	TestEqual(TEXT("the presentation gate releases the defeated target after Death"),
		FGateApi::Target(LethalBoard), NAME_None);
	TestNull(TEXT("the defeated target visual is removed after Death when no later full-screen event needs it"),
		LethalBoard->GetUnitVisualForTest(LethalTargetUnitId));
	TestEqual(TEXT("fixed-HUD status reconciliation exposes no full-screen signed status readout"),
		LethalBoard->GetActiveBattleStatusDeltaForTest(), 0);
	TestTrue(TEXT("the reward gate opens after Hit and Death while status state stays on the fixed HUD"),
		LethalBoard->HasPendingRouteReward());
	LethalBoard->AdvanceVisualsAtRealTime(100.0);
	TestTrue(TEXT("the reward gate remains open after later visual time advances"),
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
	TestEqual(TEXT("player hand cards preserve the approved current PSD frame at runtime"), Board->GetCardFrameRuntimeSizeForTest(), FVector2D(206.0f, 285.0f));
	TestEqual(TEXT("approved PSD card frame remains un-tinted"), Board->GetCardFrameTintForTest(), FLinearColor::White);
	TestEqual(TEXT("hero cards resolve the original hero card portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Hero.Generic.QingFengYiShi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Hero.T_CardPortrait_Hero")));
	TestEqual(TEXT("task NPC cards resolve their locked named-NPC portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief")));
	TestEqual(TEXT("profession cards resolve the shared role portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Profession.Blade.LieFengZhan")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Role_Blade.T_CardPortrait_Role_Blade")));
	TestEqual(TEXT("general route cards resolve their shared ink-command crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.General.PoJiaTuCi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_General.T_CardPortrait_Route_General")));
	TestEqual(TEXT("terrain route cards resolve their shared landscape crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Terrain.DuanYaLuoShi")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Terrain.T_CardPortrait_Route_Terrain")));
	TestEqual(TEXT("rare route cards resolve their shared relic crest"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Rare.TieYiYiJue")), FString(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_Rare.T_CardPortrait_Route_Rare")));
	TestEqual(TEXT("tiger boss route cards resolve the real final-idle tiger boss portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Boss.FuHuDuanJiang")), FString(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch3_TigerBoss.T_CardPortrait_Enemy_Ch3_TigerBoss")));
	TestEqual(TEXT("black-bear boss route cards resolve the real final-idle black-bear portrait"), Board->GetCardPortraitResourcePathForTest(TEXT("Route.Boss.XiongPiPiJia")), FString(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_Ch2_BlackBearBoss.T_CardPortrait_Enemy_Ch2_BlackBearBoss")));
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
		const bool bBossRoute = Definition.AcquisitionKey == TEXT("Route.Boss.BlackBear")
			|| Definition.AcquisitionKey == TEXT("Route.Boss.Tiger");
		TestTrue(
			FString::Printf(TEXT("route card %s uses its approved category or real boss portrait root"), *Definition.Id.ToString()),
			bBossRoute
				? RoutePortraitPath.StartsWith(TEXT("/Game/GameXXK/UI/Battle/EnemyCardArt/T_CardPortrait_Enemy_"))
				: RoutePortraitPath.StartsWith(TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Route_")));
	}
	TestEqual(TEXT("all thirty route-card definitions resolve a non-empty category portrait"), RouteDefinitionCount, 30);
	TestEqual(TEXT("hero lower information strip uses pale parchment"), Board->GetCardInfoStripTintForTest(TEXT("Hero.Generic.QingFengYiShi")), FLinearColor(0.945f, 0.894f, 0.800f, 1.0f));
	TestEqual(TEXT("blade lower information strip uses cinnabar only"), Board->GetCardInfoStripTintForTest(TEXT("Profession.Blade.LieFengZhan")), FLinearColor(0.714f, 0.282f, 0.247f, 1.0f));
	TestEqual(TEXT("task NPC lower information strip uses near-black"), Board->GetCardInfoStripTintForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FLinearColor(0.145f, 0.137f, 0.129f, 1.0f));
	TestEqual(TEXT("hero card name band uses black ink"), Board->GetCardInfoInkTintForTest(TEXT("Hero.Generic.QingFengYiShi")), FLinearColor(0.137f, 0.118f, 0.098f, 1.0f));
	TestEqual(TEXT("companion card name band shares the hero black ink"), Board->GetCardInfoInkTintForTest(TEXT("Profession.Blade.LieFengZhan")), FLinearColor(0.137f, 0.118f, 0.098f, 1.0f));
	TestEqual(TEXT("task NPC name band keeps the pale stone ink"), Board->GetCardInfoInkTintForTest(TEXT("Npc.TusiChief.ZhaiZhuHaoLing")), FLinearColor(0.722f, 0.706f, 0.671f, 1.0f));
	TestTrue(TEXT("card battle board exposes the active five-card hand"), Board->GetVisibleHandCardCountForTest() > 0);
	USizeBox* FirstHandCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleHandCardSize_00"))) : nullptr;
	USizeBox* FirstIntentCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattleEnemyIntentCardSize_00"))) : nullptr;
	TestNotNull(TEXT("player hand keeps a named PSD card-size box"), FirstHandCardSize);
	TestNotNull(TEXT("enemy intent keeps its independent compact card-size box"), FirstIntentCardSize);
	TestEqual(TEXT("player hand width preserves the approved current layout"), FirstHandCardSize ? FirstHandCardSize->GetWidthOverride() : 0.0f, 206.0f);
	TestEqual(TEXT("player hand height preserves the approved current layout"), FirstHandCardSize ? FirstHandCardSize->GetHeightOverride() : 0.0f, 285.0f);
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
	TestEqual(TEXT("shared-energy fixture uses the stable one-Qi manual route card"), CardPreview.CardId, FName(TEXT("Route.General.PoJiaTuCi")));
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
	const UGameXXKBattleUnitVisualWidget* const LegalTargetVisual = Board->GetUnitVisualForTest(TargetUnitId);
	TestTrue(TEXT("card targeting keeps every legal target at full color"),
		LegalTargetVisual && !LegalTargetVisual->IsDimmedForCardTargetingForTest());
	TestTrue(TEXT("card targeting greys every living unit that cannot receive the selected card"),
		OwnerVisual && OwnerVisual->IsDimmedForCardTargetingForTest());
	TestEqual(TEXT("previewing a card does not deal damage"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHealthBeforePreview);

	Board->UpdateTargetingPointer(FVector2D(520.0f, 360.0f));
	TestEqual(TEXT("the card targeting arrow endpoint follows the cursor"), Board->GetTargetingPointerPositionForTest(), FVector2D(520.0f, 360.0f));
	TestFalse(TEXT("a stable but non-highlighted unit cannot commit the card"), Board->ConfirmTargetingUnit(OwnerUnitId));
	TestTrue(TEXT("an invalid target keeps the card selection active"), Board->IsCardTargetingForTest());
	TestTrue(TEXT("right-click or Escape cancellation clears card-targeting state"), Board->CancelBattleTargeting());
	TestFalse(TEXT("cancel removes the card target highlights"), Board->IsTargetUnitHighlighted(TargetUnitId));
	TestTrue(TEXT("cancel restores the formerly invalid unit to full color"),
		OwnerVisual && !OwnerVisual->IsDimmedForCardTargetingForTest());
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
	FGameXXKTargetOutcomePreviewManualHoverTest,
	"GameXXK.Integration.CardBattle.TargetOutcomePreview.ManualHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTargetOutcomePreviewManualHoverTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("cannot enter the 268435456-byte resident budget; using fallback."), EAutomationExpectedErrorFlags::Contains, 0);
	UGameInstance* const GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FName CardInstanceId;
	FName FirstTargetUnitId;
	FName OwnerUnitId;
	FGameXXKCardPlayPreview Playability;
	FString Error;
	TestTrue(FString::Printf(TEXT("two-target manual fixture builds through the real catalog/adapter: %s"), *Error),
		BuildManualTargetCardFixture(
			Subsystem, CardInstanceId, FirstTargetUnitId, OwnerUnitId, Playability, Error, 2));
	TArray<FName> LegalEnemyTargets;
	for (const FGameXXKCardTargetCandidateView& Candidate : Playability.TargetRequest.CandidateViews)
	{
		if (Candidate.bCanSelect && Candidate.Side == EGameXXKCardTargetSide::Enemy)
		{
			LegalEnemyTargets.Add(Candidate.UnitId);
		}
	}
	TestEqual(TEXT("manual fixture exposes two legal stable enemy targets"), LegalEnemyTargets.Num(), 2);
	if (LegalEnemyTargets.Num() != 2)
	{
		return false;
	}

	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("manual outcome Board initializes"), Board->Initialize());
	Board->NativeConstruct();
	TestTrue(TEXT("manual outcome Board begins its real visual session"), Board->BeginBattleVisualSession(8701));
	FlushAsyncLoading();
	UButton* FirstProxy = Board->GetUnitTargetProxyForTest(LegalEnemyTargets[0]);
	UButton* const SecondProxy = Board->GetUnitTargetProxyForTest(LegalEnemyTargets[1]);
	UButton* const OwnerProxy = Board->GetUnitTargetProxyForTest(OwnerUnitId);
	TestNotNull(TEXT("first legal enemy has a real transparent proxy"), FirstProxy);
	TestNotNull(TEXT("second legal enemy has a real transparent proxy"), SecondProxy);
	TestNotNull(TEXT("illegal card owner still has a real transparent proxy"), OwnerProxy);
	if (!FirstProxy || !SecondProxy || !OwnerProxy)
	{
		return false;
	}

	FirstProxy->OnHovered.Broadcast();
	TestFalse(TEXT("unit hover outside TargetingCard never displays an outcome"), Board->IsCardOutcomePreviewVisibleForTest());
	TestEqual(TEXT("unit hover outside TargetingCard never runs outcome simulation"), Board->GetCardOutcomePreviewBuildCountForTest(), 0);
	TestTrue(TEXT("manual card enters the existing TargetingCard click path"), Board->ClickCardInHand(CardInstanceId));
	OwnerProxy->OnHovered.Broadcast();
	TestFalse(TEXT("illegal target hover does not display an outcome"), Board->IsCardOutcomePreviewVisibleForTest());
	TestEqual(TEXT("illegal target hover does not run outcome simulation"), Board->GetCardOutcomePreviewBuildCountForTest(), 0);

	FirstProxy->OnHovered.Broadcast();
	TestTrue(TEXT("legal manual target hover displays an outcome"), Board->IsCardOutcomePreviewVisibleForTest());
	TestEqual(TEXT("manual hover exposes the stable card instance"), Board->GetCardOutcomePreviewCardInstanceIdForTest(), CardInstanceId);
	TestEqual(TEXT("manual hover exposes the first stable target"), Board->GetCardOutcomePreviewTargetUnitIdForTest(), LegalEnemyTargets[0]);
	TestEqual(TEXT("manual hover is classified without exposing the private enum"), Board->GetCardOutcomePreviewClassForTest(), FString(TEXT("ManualUnit")));
	TestTrue(TEXT("manual hover displays at least one concise line"), Board->GetCardOutcomePreviewLinesForTest().Num() >= 1);
	TestTrue(TEXT("manual hover never displays more than two lines"), Board->GetCardOutcomePreviewLinesForTest().Num() <= 2);
	const UGameXXKBattleUnitVisualWidget* const FirstTargetVisual = Board->GetUnitVisualForTest(LegalEnemyTargets[0]);
	TestNotNull(TEXT("first legal target has a real formation visual"), FirstTargetVisual);
	const FVector2D FirstTargetCenter = FirstTargetVisual ? FirstTargetVisual->GetStageCenter() : FVector2D::ZeroVector;
	const FVector2D FirstTargetAnchor(
		FirstTargetCenter.X / 1920.0f,
		FirstTargetCenter.Y / 1080.0f);
	TestTrue(TEXT("legal hover snaps the targeting arrow head to the first target center"),
		Board->GetTargetingPointerPositionForTest().Equals(FirstTargetCenter, 0.01f));
	Board->UpdateTargetingPointer(FVector2D(1730.0f, 900.0f));
	TestTrue(TEXT("controller mouse updates cannot pull the arrow away while the target remains hovered"),
		Board->GetTargetingPointerPositionForTest().Equals(FirstTargetCenter, 0.01f));
	TestTrue(TEXT("manual preview shares the first arrow-target anchor instead of the HUD anchor"),
		Board->GetSingleOutcomePreviewAnchorForTest().Equals(FirstTargetAnchor, 0.001f));
	TestEqual(TEXT("manual preview bottom stays twelve pixels above the 410px target visual"),
		Board->GetSingleOutcomePreviewOffsetsForTest(), FMargin(0.0f, -217.0f, 272.0f, 56.0f));
	TestEqual(TEXT("first manual hover performs one outcome build"), Board->GetCardOutcomePreviewBuildCountForTest(), 1);
	FirstProxy->OnHovered.Broadcast();
	TestEqual(TEXT("identical consecutive hover reuses the complete-state cache"), Board->GetCardOutcomePreviewBuildCountForTest(), 1);

	SecondProxy->OnHovered.Broadcast();
	TestEqual(TEXT("changing legal target performs exactly one additional build"), Board->GetCardOutcomePreviewBuildCountForTest(), 2);
	TestEqual(TEXT("changing legal target updates the stable target id"), Board->GetCardOutcomePreviewTargetUnitIdForTest(), LegalEnemyTargets[1]);
	const UGameXXKBattleUnitVisualWidget* const SecondTargetVisual = Board->GetUnitVisualForTest(LegalEnemyTargets[1]);
	TestNotNull(TEXT("second legal target has a real formation visual"), SecondTargetVisual);
	const FVector2D SecondTargetCenter = SecondTargetVisual ? SecondTargetVisual->GetStageCenter() : FVector2D::ZeroVector;
	const FVector2D SecondTargetAnchor(
		SecondTargetCenter.X / 1920.0f,
		SecondTargetCenter.Y / 1080.0f);
	TestTrue(TEXT("changing target snaps the arrow head to the second target center"),
		Board->GetTargetingPointerPositionForTest().Equals(SecondTargetCenter, 0.01f));
	TestTrue(TEXT("manual preview follows the second arrow-target anchor"),
		Board->GetSingleOutcomePreviewAnchorForTest().Equals(SecondTargetAnchor, 0.001f));
	FirstProxy->OnUnhovered.Broadcast();
	TestTrue(TEXT("late unhover from the old target cannot clear the newer hover"), Board->IsCardOutcomePreviewVisibleForTest());
	SecondProxy->OnUnhovered.Broadcast();
	AssertOutcomeCleared(*this, Board, TEXT("current manual target unhover"));

	FGameXXKCardInstance* const RuntimeCard = Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand.FindByPredicate(
		[CardInstanceId](const FGameXXKCardInstance& Card) { return Card.InstanceId == CardInstanceId; });
	TestNotNull(TEXT("manual failure fixture retains the real hand card"), RuntimeCard);
	if (!RuntimeCard)
	{
		return false;
	}
	RuntimeCard->CardId = TEXT("Missing.TargetOutcome.Card");
	FirstProxy->OnHovered.Broadcast();
	TestTrue(TEXT("failed manual preview replaces old content with one visible fallback"), Board->IsCardOutcomePreviewVisibleForTest());
	TestEqual(TEXT("failed manual preview keeps the requested target"), Board->GetCardOutcomePreviewTargetUnitIdForTest(), LegalEnemyTargets[0]);
	TestEqual(TEXT("failed manual preview exposes exactly one line"), Board->GetCardOutcomePreviewLinesForTest().Num(), 1);
	TestEqual(TEXT("failed manual preview uses the neutral fallback text"), Board->GetCardOutcomePreviewLinesForTest()[0], FString(TEXT("无法预演")));
	TestTrue(TEXT("failed manual preview remains above the first arrow-target position"),
		Board->GetSingleOutcomePreviewAnchorForTest().Equals(FirstTargetAnchor, 0.001f));
	const int32 ManualFailureBuildCount = Board->GetCardOutcomePreviewBuildCountForTest();
	FirstProxy->OnHovered.Broadcast();
	TestEqual(TEXT("identical failed manual hover reuses the complete-state cache"),
		Board->GetCardOutcomePreviewBuildCountForTest(), ManualFailureBuildCount);
	TestEqual(TEXT("cached manual failure retains exactly the neutral fallback line"),
		Board->GetCardOutcomePreviewLinesForTest(), TArray<FString>{TEXT("无法预演")});
	TestEqual(TEXT("cached manual failure retains the requested target"),
		Board->GetCardOutcomePreviewTargetUnitIdForTest(), LegalEnemyTargets[0]);
	TestFalse(TEXT("manual submit failure is reported"), Board->ConfirmTargetingUnit(LegalEnemyTargets[0]));
	AssertOutcomeCleared(*this, Board, TEXT("manual submit failure"));

	RuntimeCard->CardId = TEXT("Route.General.PoJiaTuCi");
	Board->RefreshFromState();
	FirstProxy = Board->GetUnitTargetProxyForTest(LegalEnemyTargets[0]);
	TestNotNull(TEXT("manual target proxy survives authoritative recovery"), FirstProxy);
	if (!FirstProxy)
	{
		return false;
	}
	FirstProxy->OnHovered.Broadcast();
	TestTrue(TEXT("recovered manual preview is visible before cancellation"), Board->IsCardOutcomePreviewVisibleForTest());
	TestTrue(TEXT("manual targeting cancellation succeeds"), Board->CancelBattleTargeting());
	AssertOutcomeCleared(*this, Board, TEXT("manual targeting cancellation"));

	TestTrue(TEXT("manual card can re-enter the unchanged click path"), Board->ClickCardInHand(CardInstanceId));
	FirstProxy->OnHovered.Broadcast();
	TestTrue(TEXT("manual preview is visible immediately before successful submit"), Board->IsCardOutcomePreviewVisibleForTest());
	TestTrue(TEXT("manual submit succeeds through the existing resolver"), Board->ConfirmTargetingUnit(LegalEnemyTargets[0]));
	AssertOutcomeCleared(*this, Board, TEXT("manual submit success"));
	Board->CancelBattleVisualSession(8701);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTargetOutcomePreviewGroupHandHoverTest,
	"GameXXK.Integration.CardBattle.TargetOutcomePreview.GroupHandHover",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTargetOutcomePreviewGroupHandHoverTest::RunTest(const FString& Parameters)
{
	UGameInstance* const GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FName CardInstanceId;
	FGameXXKCardPlayPreview Playability;
	FGameXXKCardOutcomePreview ExpectedOutcome;
	FString Error;
	TestTrue(FString::Printf(TEXT("three-enemy pure group fixture builds through catalog/adapter/rules: %s"), *Error),
		BuildPureEnemyGroupCardFixture(Subsystem, CardInstanceId, Playability, ExpectedOutcome, Error));
	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("group outcome Board initializes"), Board->Initialize());
	Board->NativeConstruct();
	UButton* const GroupCardButton = Board->GetHandCardButtonForTest(0);
	TestNotNull(TEXT("group fixture exposes its real hand button"), GroupCardButton);
	if (!GroupCardButton)
	{
		return false;
	}
	TMap<FName, int32> EnemyHpBefore;
	for (const FGameXXKCardCombatUnit& Unit : Subsystem->GetRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			EnemyHpBefore.Add(Unit.UnitId, Unit.HP);
		}
	}
	GroupCardButton->OnHovered.Broadcast();
	TestTrue(TEXT("pure group hand hover displays the shared group widget"), Board->IsCardOutcomePreviewVisibleForTest());
	TestEqual(TEXT("pure group hover exposes its stable card"), Board->GetCardOutcomePreviewCardInstanceIdForTest(), CardInstanceId);
	TestEqual(TEXT("pure group hover requests NAME_None"), Board->GetCardOutcomePreviewTargetUnitIdForTest(), NAME_None);
	TestEqual(TEXT("pure group hover exposes the safe class string"), Board->GetCardOutcomePreviewClassForTest(), FString(TEXT("PureEnemyGroup")));
	const TArray<FString> GroupLines = Board->GetCardOutcomePreviewLinesForTest();
	const TArray<FString> ExpectedGroupLines = FlattenOutcomeLines(ExpectedOutcome.EnemyPositionLines);
	TestEqual(TEXT("group widget text exactly matches the real rules output"), GroupLines, ExpectedGroupLines);
	TestEqual(TEXT("three living enemies produce exactly three ordered lines"), GroupLines.Num(), 3);
	for (int32 PositionIndex = 0; PositionIndex < GroupLines.Num(); ++PositionIndex)
	{
		TestTrue(*FString::Printf(TEXT("group line %d starts with its 1P/2P/3P position"), PositionIndex + 1),
			GroupLines[PositionIndex].StartsWith(FString::Printf(TEXT("%dP"), PositionIndex + 1)));
		TestTrue(*FString::Printf(TEXT("group line %d includes real resolved damage"), PositionIndex + 1),
			GroupLines[PositionIndex].Contains(TEXT("伤害")));
	}
	TestEqual(TEXT("first pure-group hover builds once"), Board->GetCardOutcomePreviewBuildCountForTest(), 1);
	GroupCardButton->OnHovered.Broadcast();
	TestEqual(TEXT("same group hover and full state reuse one build"), Board->GetCardOutcomePreviewBuildCountForTest(), 1);
	GroupCardButton->OnUnhovered.Broadcast();
	AssertOutcomeCleared(*this, Board, TEXT("group hand unhover"));

	GroupCardButton->OnHovered.Broadcast();
	TestTrue(TEXT("automatic group click resolves once"), Board->ClickCardInHand(CardInstanceId));
	TestFalse(TEXT("automatic group click never enters TargetingCard"), Board->IsCardTargetingForTest());
	AssertOutcomeCleared(*this, Board, TEXT("automatic group submit success"));
	TestFalse(TEXT("automatic group click removes the one played instance from hand"),
		Subsystem->GetRuntimeState().CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate(
			[CardInstanceId](const FGameXXKCardInstance& Card) { return Card.InstanceId == CardInstanceId; }));
	for (const FGameXXKCardOutcomeTarget& ExpectedTarget : ExpectedOutcome.EnemyPositionTargets)
	{
		const FGameXXKCardCombatUnit* const CommittedUnit = Subsystem->GetRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate(
			[&ExpectedTarget](const FGameXXKCardCombatUnit& Unit) { return Unit.UnitId == ExpectedTarget.UnitId; });
		const int32* const BeforeHp = EnemyHpBefore.Find(ExpectedTarget.UnitId);
		TestNotNull(*FString::Printf(TEXT("preview target %s remains in authoritative runtime"), *ExpectedTarget.UnitId.ToString()), CommittedUnit);
		TestNotNull(*FString::Printf(TEXT("preview target %s has captured pre-submit HP"), *ExpectedTarget.UnitId.ToString()), BeforeHp);
		if (CommittedUnit && BeforeHp)
		{
			TestEqual(
				*FString::Printf(TEXT("position %d committed HP damage equals its exact preview total"), ExpectedTarget.SlotNumber),
				*BeforeHp - CommittedUnit->HP,
				GetPreviewHealthDamage(ExpectedTarget));
		}
		TestEqual(*FString::Printf(TEXT("position %d pure enemy group preview has no healing side effect"), ExpectedTarget.SlotNumber), ExpectedTarget.EffectiveHealing, 0);
		TestEqual(*FString::Printf(TEXT("position %d pure enemy group preview has no armor side effect"), ExpectedTarget.SlotNumber), ExpectedTarget.EffectiveArmor, 0);
	}

	UGameInstance* const TwoEnemyGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const TwoEnemySubsystem = NewObject<UGameXXKMVPSubsystem>(TwoEnemyGameInstance);
	FName TwoEnemyCardId;
	FGameXXKCardPlayPreview TwoEnemyPlayability;
	FGameXXKCardOutcomePreview TwoEnemyOutcome;
	TestTrue(TEXT("two-enemy pure group fixture builds"), BuildPureEnemyGroupCardFixture(
		TwoEnemySubsystem, TwoEnemyCardId, TwoEnemyPlayability, TwoEnemyOutcome, Error, 2));
	FGameXXKRuntimeState& SparseEnemyState = TwoEnemySubsystem->GetMutableRuntimeState();
	FGameXXKCardCombatUnit* FirstLivingEnemy = nullptr;
	FGameXXKCardCombatUnit* SecondLivingEnemy = nullptr;
	for (FGameXXKCardCombatUnit& Unit : SparseEnemyState.CardRun.ActiveBattle.Units)
	{
		if (Unit.Side != EGameXXKCardTargetSide::Enemy || Unit.HP <= 0)
		{
			continue;
		}
		if (!FirstLivingEnemy)
		{
			FirstLivingEnemy = &Unit;
		}
		else if (!SecondLivingEnemy)
		{
			SecondLivingEnemy = &Unit;
		}
	}
	TestNotNull(TEXT("sparse fixture retains its first living enemy"), FirstLivingEnemy);
	TestNotNull(TEXT("sparse fixture retains its second living enemy"), SecondLivingEnemy);
	if (!FirstLivingEnemy || !SecondLivingEnemy)
	{
		return false;
	}
	if (SecondLivingEnemy->StableSortOrder < FirstLivingEnemy->StableSortOrder)
	{
		Swap(FirstLivingEnemy, SecondLivingEnemy);
	}
	const int32 EnemyStableSortBase = FirstLivingEnemy->StableSortOrder;
	FirstLivingEnemy->BattleSlotNumber = 1;
	FirstLivingEnemy->StableSortOrder = EnemyStableSortBase;
	FirstLivingEnemy->EnemyDefinitionId = TEXT("Enemy.Ch1.Rooster");
	FirstLivingEnemy->CombatLevel = 1;
	SecondLivingEnemy->BattleSlotNumber = 3;
	SecondLivingEnemy->StableSortOrder = EnemyStableSortBase + 2;
	SecondLivingEnemy->EnemyDefinitionId = TEXT("Enemy.Ch1.Weasel");
	SecondLivingEnemy->CombatLevel = 1;
	for (FGameXXKBattleRuntimeUnit& LegacyEnemy : SparseEnemyState.ActiveBattleEnemies)
	{
		const FGameXXKCardCombatUnit* const CardEnemy = LegacyEnemy.Id == FirstLivingEnemy->UnitId
			? FirstLivingEnemy
			: (LegacyEnemy.Id == SecondLivingEnemy->UnitId ? SecondLivingEnemy : nullptr);
		if (CardEnemy)
		{
			LegacyEnemy.BattleSlotNumber = CardEnemy->BattleSlotNumber;
			LegacyEnemy.EnemyDefinitionId = CardEnemy->EnemyDefinitionId;
			LegacyEnemy.CombatLevel = CardEnemy->CombatLevel;
		}
	}
	Error.Reset();
	TestTrue(FString::Printf(TEXT("sparse 1P/3P card runtime validates before projection sync: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(SparseEnemyState.CardRun.ActiveBattle, &Error));
	Error.Reset();
	TestTrue(FString::Printf(TEXT("sparse 1P/3P fixture syncs to the legacy projection: %s"), *Error),
		FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(SparseEnemyState, &Error));
	TArray<int32> LegacyLivingEnemySlots;
	for (const FGameXXKBattleRuntimeUnit& Unit : SparseEnemyState.ActiveBattleEnemies)
	{
		if (Unit.HP > 0)
		{
			LegacyLivingEnemySlots.Add(Unit.BattleSlotNumber);
		}
	}
	LegacyLivingEnemySlots.Sort();
	TestEqual(TEXT("legacy projection preserves the sparse 1P/3P positions"),
		LegacyLivingEnemySlots, TArray<int32>{1, 3});
	TwoEnemyPlayability = FGameXXKCardPlayPreview();
	TwoEnemyOutcome = FGameXXKCardOutcomePreview();
	Error.Reset();
	TestTrue(FString::Printf(TEXT("sparse fixture rebuilds real adapter playability: %s"), *Error),
		FGameXXKCardBattleAdapter::BuildCardPlayPreview(
			SparseEnemyState, TwoEnemyCardId, TwoEnemyPlayability, &Error));
	TestTrue(TEXT("sparse fixture remains a playable automatic AllEnemies card"),
		TwoEnemyPlayability.bCanPlay
			&& !TwoEnemyPlayability.TargetRequest.bRequiresManualSelection
			&& TwoEnemyPlayability.TargetRequest.EffectiveMode == EGameXXKCardTargetMode::AllEnemies);
	Error.Reset();
	TestTrue(FString::Printf(TEXT("sparse fixture rebuilds real outcome rules: %s"), *Error),
		FGameXXKCardOutcomePreviewRules::Build(
			SparseEnemyState, TwoEnemyCardId, NAME_None, TwoEnemyOutcome, &Error));
	UGameXXKBattleBoardWidget* const TwoEnemyBoard = NewObject<UGameXXKBattleBoardWidget>();
	TwoEnemyBoard->SetMVPSubsystem(TwoEnemySubsystem);
	TestTrue(TEXT("two-enemy group Board initializes"), TwoEnemyBoard->Initialize());
	TwoEnemyBoard->NativeConstruct();
	UButton* const TwoEnemyButton = TwoEnemyBoard->GetHandCardButtonForTest(0);
	TestNotNull(TEXT("two-enemy group has a real hand button"), TwoEnemyButton);
	if (TwoEnemyButton)
	{
		TwoEnemyButton->OnHovered.Broadcast();
		const TArray<FString> TwoEnemyLines = TwoEnemyBoard->GetCardOutcomePreviewLinesForTest();
		TestEqual(TEXT("two-enemy widget text exactly matches the real rules output"),
			TwoEnemyLines, FlattenOutcomeLines(TwoEnemyOutcome.EnemyPositionLines));
		TArray<FString> SparseLinePrefixes;
		for (const FString& Line : TwoEnemyLines)
		{
			SparseLinePrefixes.Add(Line.Left(2));
		}
		TestEqual(TEXT("sparse group lines have the strict ordered 1P/3P prefixes"),
			SparseLinePrefixes, TArray<FString>{TEXT("1P"), TEXT("3P")});
		const FString JoinedSparseLines = FString::Join(TwoEnemyLines, TEXT("|"));
		TestTrue(TEXT("sparse group retains the 1P line"), JoinedSparseLines.Contains(TEXT("1P")));
		TestFalse(TEXT("missing middle enemy omits every 2P line"), JoinedSparseLines.Contains(TEXT("2P")));
		TestTrue(TEXT("sparse group retains the 3P line"), JoinedSparseLines.Contains(TEXT("3P")));
	}

	UGameInstance* const FailureGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const FailureSubsystem = NewObject<UGameXXKMVPSubsystem>(FailureGameInstance);
	FName FailureCardId;
	FGameXXKCardPlayPreview FailurePlayability;
	FGameXXKCardOutcomePreview FailureOutcome;
	TestTrue(TEXT("group failure base fixture builds"), BuildPureEnemyGroupCardFixture(
		FailureSubsystem, FailureCardId, FailurePlayability, FailureOutcome, Error));
	UGameXXKBattleBoardWidget* const FailureBoard = NewObject<UGameXXKBattleBoardWidget>();
	FailureBoard->SetMVPSubsystem(FailureSubsystem);
	TestTrue(TEXT("group failure Board initializes"), FailureBoard->Initialize());
	FailureBoard->NativeConstruct();
	for (FGameXXKCardCombatUnit& Unit : FailureSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units)
	{
		if (Unit.Side == EGameXXKCardTargetSide::Enemy)
		{
			Unit.BattleSlotNumber = INDEX_NONE;
			Unit.StableSortOrder = 99;
			break;
		}
	}
	UButton* const FailureButton = FailureBoard->GetHandCardButtonForTest(0);
	TestNotNull(TEXT("group failure uses a real hand button"), FailureButton);
	if (FailureButton)
	{
		FailureButton->OnHovered.Broadcast();
		TestTrue(TEXT("NAME_None build failure is visible in the group widget"), FailureBoard->IsCardOutcomePreviewVisibleForTest());
		TestEqual(TEXT("NAME_None build failure retains the group target id"), FailureBoard->GetCardOutcomePreviewTargetUnitIdForTest(), NAME_None);
		TestEqual(TEXT("NAME_None build failure has one neutral fallback line"), FailureBoard->GetCardOutcomePreviewLinesForTest(), TArray<FString>{TEXT("无法预演")});
		TestEqual(TEXT("NAME_None build failure does not masquerade as a successful class"), FailureBoard->GetCardOutcomePreviewClassForTest(), FString(TEXT("None")));
		const int32 GroupFailureBuildCount = FailureBoard->GetCardOutcomePreviewBuildCountForTest();
		FailureButton->OnHovered.Broadcast();
		TestEqual(TEXT("identical failed NAME_None hover reuses the complete-state cache"),
			FailureBoard->GetCardOutcomePreviewBuildCountForTest(), GroupFailureBuildCount);
		TestEqual(TEXT("cached NAME_None failure retains exactly the neutral fallback line"),
			FailureBoard->GetCardOutcomePreviewLinesForTest(), TArray<FString>{TEXT("无法预演")});
		TestEqual(TEXT("cached NAME_None failure remains in the group target context"),
			FailureBoard->GetCardOutcomePreviewTargetUnitIdForTest(), NAME_None);
		if (!FailureSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand.IsEmpty())
		{
			FailureSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand[0].CardId = TEXT("Missing.GroupSubmit.Card");
		}
		TestFalse(TEXT("automatic submit failure is reported after the malformed authoritative state"), FailureBoard->ClickCardInHand(FailureCardId));
		AssertOutcomeCleared(*this, FailureBoard, TEXT("automatic group submit failure"));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTargetOutcomePreviewCacheAndClearTest,
	"GameXXK.Integration.CardBattle.TargetOutcomePreview.CacheAndClear",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTargetOutcomePreviewCacheAndClearTest::RunTest(const FString& Parameters)
{
	UGameInstance* const GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FGameXXKCardPlayPreview Playability;
	FString Error;
	TestTrue(TEXT("cache fixture builds"), BuildManualTargetCardFixture(
		Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Playability, Error));
	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("cache Board initializes"), Board->Initialize());
	Board->NativeConstruct();
	TestTrue(TEXT("cache Board starts its visual session"), Board->BeginBattleVisualSession(8702));
	TestTrue(TEXT("cache card enters manual targeting"), Board->ClickCardInHand(CardInstanceId));
	UButton* const TargetProxy = Board->GetUnitTargetProxyForTest(TargetUnitId);
	TestNotNull(TEXT("cache target has a real proxy"), TargetProxy);
	if (!TargetProxy)
	{
		return false;
	}
	TargetProxy->OnHovered.Broadcast();
	const int32 BuildCountBeforeRefresh = Board->GetCardOutcomePreviewBuildCountForTest();
	Subsystem->GetMutableRuntimeState().PlayerGold += 1;
	Board->RefreshFromState();
	AssertOutcomeCleared(*this, Board, TEXT("Refresh authoritative full-state change"));
	TestEqual(TEXT("Refresh clear never rewinds the lifetime build count"), Board->GetCardOutcomePreviewBuildCountForTest(), BuildCountBeforeRefresh);
	TargetProxy->OnHovered.Broadcast();
	TestEqual(TEXT("new authoritative full state forces exactly one new build"), Board->GetCardOutcomePreviewBuildCountForTest(), BuildCountBeforeRefresh + 1);
	FGameXXKBattlePresentationEvent Event;
	Event.TargetUnitId = TargetUnitId;
	Board->QueuePresentation(Event);
	AssertOutcomeCleared(*this, Board, TEXT("QueuePresentation first event"));
	TestEqual(TEXT("QueuePresentation clear preserves cumulative build count"), Board->GetCardOutcomePreviewBuildCountForTest(), BuildCountBeforeRefresh + 1);
	Board->CancelBattleVisualSession(8702);

	UGameInstance* const CancelGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const CancelSubsystem = NewObject<UGameXXKMVPSubsystem>(CancelGameInstance);
	FName CancelCardId;
	FName CancelTargetId;
	FName CancelOwnerId;
	FGameXXKCardPlayPreview CancelPlayability;
	TestTrue(TEXT("session-cancel fixture builds"), BuildManualTargetCardFixture(
		CancelSubsystem, CancelCardId, CancelTargetId, CancelOwnerId, CancelPlayability, Error));
	UGameXXKBattleBoardWidget* const CancelBoard = NewObject<UGameXXKBattleBoardWidget>();
	CancelBoard->SetMVPSubsystem(CancelSubsystem);
	TestTrue(TEXT("session-cancel Board initializes"), CancelBoard->Initialize());
	CancelBoard->NativeConstruct();
	TestTrue(TEXT("session-cancel Board begins visual session"), CancelBoard->BeginBattleVisualSession(8703));
	TestTrue(TEXT("session-cancel card targets"), CancelBoard->ClickCardInHand(CancelCardId));
	UButton* const CancelProxy = CancelBoard->GetUnitTargetProxyForTest(CancelTargetId);
	if (CancelProxy)
	{
		CancelProxy->OnHovered.Broadcast();
	}
	TestTrue(TEXT("session-cancel preview is visible before cancellation"), CancelBoard->IsCardOutcomePreviewVisibleForTest());
	const int32 CancelBuildCount = CancelBoard->GetCardOutcomePreviewBuildCountForTest();
	CancelBoard->CancelBattleVisualSession(8703);
	AssertOutcomeCleared(*this, CancelBoard, TEXT("CancelBattleVisualSession"));
	TestEqual(TEXT("visual-session clear preserves cumulative build count"), CancelBoard->GetCardOutcomePreviewBuildCountForTest(), CancelBuildCount);

	UGameInstance* const DestructGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const DestructSubsystem = NewObject<UGameXXKMVPSubsystem>(DestructGameInstance);
	FName DestructCardId;
	FName DestructTargetId;
	FName DestructOwnerId;
	FGameXXKCardPlayPreview DestructPlayability;
	TestTrue(TEXT("destruct fixture builds"), BuildManualTargetCardFixture(
		DestructSubsystem, DestructCardId, DestructTargetId, DestructOwnerId, DestructPlayability, Error));
	UGameXXKBattleBoardWidget* const DestructBoard = NewObject<UGameXXKBattleBoardWidget>();
	DestructBoard->SetMVPSubsystem(DestructSubsystem);
	TestTrue(TEXT("destruct Board initializes"), DestructBoard->Initialize());
	DestructBoard->NativeConstruct();
	TestTrue(TEXT("destruct Board begins visual session"), DestructBoard->BeginBattleVisualSession(8704));
	TestTrue(TEXT("destruct card targets"), DestructBoard->ClickCardInHand(DestructCardId));
	UButton* const DestructProxy = DestructBoard->GetUnitTargetProxyForTest(DestructTargetId);
	if (DestructProxy)
	{
		DestructProxy->OnHovered.Broadcast();
	}
	TestTrue(TEXT("destruct preview is visible before NativeDestruct"), DestructBoard->IsCardOutcomePreviewVisibleForTest());
	const int32 DestructBuildCount = DestructBoard->GetCardOutcomePreviewBuildCountForTest();
	DestructBoard->NativeDestruct();
	AssertOutcomeCleared(*this, DestructBoard, TEXT("NativeDestruct"));
	TestEqual(TEXT("NativeDestruct clear preserves cumulative build count"), DestructBoard->GetCardOutcomePreviewBuildCountForTest(), DestructBuildCount);

	UGameInstance* const SwitchGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const SwitchSubsystem = NewObject<UGameXXKMVPSubsystem>(SwitchGameInstance);
	FName GroupCardId;
	FGameXXKCardPlayPreview GroupPlayability;
	FGameXXKCardOutcomePreview GroupOutcome;
	TestTrue(TEXT("card-switch group fixture builds"), BuildPureEnemyGroupCardFixture(
		SwitchSubsystem, GroupCardId, GroupPlayability, GroupOutcome, Error));
	TestTrue(TEXT("card-switch fixture has a second real hand instance"), SwitchSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand.Num() >= 2);
	if (SwitchSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand.Num() >= 2)
	{
		SwitchSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Deck.Hand[1].CardId = TEXT("Route.General.PoJiaTuCi");
	}
	UGameXXKBattleBoardWidget* const SwitchBoard = NewObject<UGameXXKBattleBoardWidget>();
	SwitchBoard->SetMVPSubsystem(SwitchSubsystem);
	TestTrue(TEXT("card-switch Board initializes"), SwitchBoard->Initialize());
	SwitchBoard->NativeConstruct();
	UButton* const GroupButton = SwitchBoard->GetHandCardButtonForTest(0);
	UButton* const OtherButton = SwitchBoard->GetHandCardButtonForTest(1);
	if (GroupButton)
	{
		GroupButton->OnHovered.Broadcast();
	}
	TestTrue(TEXT("group preview is visible before switching hand cards"), SwitchBoard->IsCardOutcomePreviewVisibleForTest());
	if (OtherButton)
	{
		OtherButton->OnHovered.Broadcast();
	}
	AssertOutcomeCleared(*this, SwitchBoard, TEXT("hovering a different non-group hand card"));

	UGameInstance* const RemoveGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const RemoveSubsystem = NewObject<UGameXXKMVPSubsystem>(RemoveGameInstance);
	FName RemoveCardId;
	FName RemoveTargetId;
	FName RemoveOwnerId;
	FGameXXKCardPlayPreview RemovePlayability;
	TestTrue(TEXT("RemoveUnitVisual clear fixture builds"), BuildManualTargetCardFixture(
		RemoveSubsystem, RemoveCardId, RemoveTargetId, RemoveOwnerId, RemovePlayability, Error));
	UGameXXKBattleBoardWidget* const RemoveBoard = NewObject<UGameXXKBattleBoardWidget>();
	RemoveBoard->SetMVPSubsystem(RemoveSubsystem);
	TestTrue(TEXT("RemoveUnitVisual clear Board initializes"), RemoveBoard->Initialize());
	RemoveBoard->NativeConstruct();
	TestTrue(TEXT("RemoveUnitVisual clear card targets"), RemoveBoard->ClickCardInHand(RemoveCardId));
	RemoveBoard->HandleUnitTargetProxyHoverChanged(RemoveTargetId, true);
	TestTrue(TEXT("RemoveUnitVisual preview is visible before removal"), RemoveBoard->IsCardOutcomePreviewVisibleForTest());
	const int32 RemoveBuildCount = RemoveBoard->GetCardOutcomePreviewBuildCountForTest();
	const FGameXXKRuntimeState RemoveStateBefore = RemoveSubsystem->GetRuntimeState();
	RemoveBoard->RemoveUnitVisualForTest(RemoveTargetId);
	AssertOutcomeCleared(*this, RemoveBoard, TEXT("death RemoveUnitVisual path"));
	TestEqual(TEXT("RemoveUnitVisual clear preserves cumulative build count"), RemoveBoard->GetCardOutcomePreviewBuildCountForTest(), RemoveBuildCount);
	TestTrue(TEXT("RemoveUnitVisual seam does not mutate authoritative runtime state"),
		RuntimeStatesEqual(RemoveStateBefore, RemoveSubsystem->GetRuntimeState()));

	const auto AssertTerminalClear = [this, &Error](const EGameXXKCardBattlePhase Phase, const TCHAR* Context)
	{
		UGameInstance* const TerminalGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* const TerminalSubsystem = NewObject<UGameXXKMVPSubsystem>(TerminalGameInstance);
		FName TerminalCardId;
		FName TerminalTargetId;
		FName TerminalOwnerId;
		if (!TestTrue(*FString::Printf(TEXT("%s fixture builds"), Context), BuildRouteRewardFixture(
			TerminalSubsystem, TerminalCardId, TerminalTargetId, TerminalOwnerId, Error)))
		{
			return;
		}
		UGameXXKBattleBoardWidget* const TerminalBoard = NewObject<UGameXXKBattleBoardWidget>();
		TerminalBoard->SetMVPSubsystem(TerminalSubsystem);
		TestTrue(*FString::Printf(TEXT("%s Board initializes"), Context), TerminalBoard->Initialize());
		TerminalBoard->NativeConstruct();
		TestTrue(*FString::Printf(TEXT("%s card targets"), Context), TerminalBoard->ClickCardInHand(TerminalCardId));
		TerminalBoard->HandleUnitTargetProxyHoverChanged(TerminalTargetId, true);
		TestTrue(*FString::Printf(TEXT("%s preview is visible before terminal handling"), Context), TerminalBoard->IsCardOutcomePreviewVisibleForTest());
		const int32 TerminalBuildCount = TerminalBoard->GetCardOutcomePreviewBuildCountForTest();
		TerminalSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Phase = Phase;
		TestTrue(*FString::Printf(TEXT("%s invokes the real terminal handler"), Context), TerminalBoard->ResolveCardBattleTerminalStateForTest());
		AssertOutcomeCleared(*this, TerminalBoard, Context);
		TestEqual(*FString::Printf(TEXT("%s clear preserves cumulative build count"), Context),
			TerminalBoard->GetCardOutcomePreviewBuildCountForTest(), TerminalBuildCount);
	};
	AssertTerminalClear(EGameXXKCardBattlePhase::Victory, TEXT("terminal Victory branch"));
	AssertTerminalClear(EGameXXKCardBattlePhase::Defeat, TEXT("terminal Defeat branch"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTargetOutcomePreviewLayoutInvariantTest,
	"GameXXK.Integration.CardBattle.TargetOutcomePreview.LayoutInvariant",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTargetOutcomePreviewLayoutInvariantTest::RunTest(const FString& Parameters)
{
	AddExpectedError(TEXT("cannot enter the 268435456-byte resident budget; using fallback."), EAutomationExpectedErrorFlags::Contains, 0);
	UGameInstance* const GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FGameXXKCardPlayPreview Playability;
	FString Error;
	TestTrue(TEXT("layout manual fixture builds"), BuildManualTargetCardFixture(
		Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Playability, Error));
	UGameXXKBattleBoardWidget* const Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("layout Board initializes"), Board->Initialize());
	Board->NativeConstruct();
	TestTrue(TEXT("layout Board begins visual session"), Board->BeginBattleVisualSession(8705));
	FlushAsyncLoading();
	TestTrue(TEXT("layout card enters targeting"), Board->ClickCardInHand(CardInstanceId));
	const TMap<FName, FCanvasLayoutSnapshot> BeforeManualHover = CaptureNonOutcomeCanvasLayout(Board);
	UButton* const TargetProxy = Board->GetUnitTargetProxyForTest(TargetUnitId);
	TestNotNull(TEXT("layout target has a real proxy"), TargetProxy);
	if (TargetProxy)
	{
		TargetProxy->OnHovered.Broadcast();
	}
	const TMap<FName, FCanvasLayoutSnapshot> AfterManualHover = CaptureNonOutcomeCanvasLayout(Board);
	AssertCanvasLayoutUnchanged(*this, BeforeManualHover, AfterManualHover);
	UCanvasPanel* const OutcomeLayer = Board->GetBattleOutcomePreviewLayerForTest();
	UCanvasPanel* const RootCanvas = Board->GetBattleControlsLayerForTest();
	TestNotNull(TEXT("outcome overlay is one RootCanvas sibling"), OutcomeLayer);
	TestTrue(TEXT("outcome overlay is directly parented by RootCanvas"),
		OutcomeLayer && OutcomeLayer->GetParent() == RootCanvas);
	TestEqual(TEXT("outcome overlay is input transparent"),
		OutcomeLayer ? OutcomeLayer->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	TArray<UWidget*> ManualBoardWidgets;
	Board->WidgetTree->GetAllWidgets(ManualBoardWidgets);
	int32 SingleOutcomeWidgetCount = 0;
	int32 GroupOutcomeWidgetCount = 0;
	UGameXXKCardOutcomePreviewWidget* SingleOutcomeWidget = nullptr;
	UGameXXKCardOutcomePreviewWidget* GroupOutcomeWidget = nullptr;
	for (UWidget* const Widget : ManualBoardWidgets)
	{
		if (Widget && Widget->GetFName() == TEXT("BattleSingleOutcomePreview"))
		{
			++SingleOutcomeWidgetCount;
			SingleOutcomeWidget = Cast<UGameXXKCardOutcomePreviewWidget>(Widget);
		}
		else if (Widget && Widget->GetFName() == TEXT("BattleGroupOutcomePreview"))
		{
			++GroupOutcomeWidgetCount;
			GroupOutcomeWidget = Cast<UGameXXKCardOutcomePreviewWidget>(Widget);
		}
	}
	TestEqual(TEXT("WidgetTree contains exactly one named single outcome widget"), SingleOutcomeWidgetCount, 1);
	TestEqual(TEXT("WidgetTree contains exactly one named group outcome widget"), GroupOutcomeWidgetCount, 1);
	TestNotNull(TEXT("named single outcome widget has the concrete preview type"), SingleOutcomeWidget);
	TestNotNull(TEXT("named group outcome widget has the concrete preview type"), GroupOutcomeWidget);
	TestTrue(TEXT("single outcome widget is directly parented by the outcome overlay"),
		SingleOutcomeWidget && SingleOutcomeWidget->GetParent() == OutcomeLayer);
	TestTrue(TEXT("group outcome widget is directly parented by the outcome overlay"),
		GroupOutcomeWidget && GroupOutcomeWidget->GetParent() == OutcomeLayer);
	TestEqual(TEXT("visible single outcome widget is input transparent"),
		SingleOutcomeWidget ? SingleOutcomeWidget->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	if (SingleOutcomeWidget)
	{
		SingleOutcomeWidget->TakeWidget();
		TArray<UWidget*> SingleOutcomeChildren;
		SingleOutcomeWidget->WidgetTree->GetAllWidgets(SingleOutcomeChildren);
		TestTrue(TEXT("visible single outcome widget has real generated children"), !SingleOutcomeChildren.IsEmpty());
		for (UWidget* const Child : SingleOutcomeChildren)
		{
			TestEqual(*FString::Printf(TEXT("single outcome child %s is input transparent"), *GetNameSafe(Child)),
				Child ? Child->GetVisibility() : ESlateVisibility::Collapsed,
				ESlateVisibility::HitTestInvisible);
		}
	}
	TestEqual(TEXT("outcome overlay uses only z=1"), Board->GetBattleOutcomePreviewLayerZForTest(), 1);
	const UGameXXKBattleUnitVisualWidget* const OutcomeTargetVisual = Board->GetUnitVisualForTest(TargetUnitId);
	TestNotNull(TEXT("layout outcome target has a real formation visual"), OutcomeTargetVisual);
	const FVector2D OutcomeTargetCenter = OutcomeTargetVisual ? OutcomeTargetVisual->GetStageCenter() : FVector2D::ZeroVector;
	const FVector2D OutcomeTargetAnchor(OutcomeTargetCenter.X / 1920.0f, OutcomeTargetCenter.Y / 1080.0f);
	TestTrue(TEXT("single outcome uses the arrow target anchor"),
		Board->GetSingleOutcomePreviewAnchorForTest().Equals(OutcomeTargetAnchor, 0.001f));
	TestTrue(TEXT("layout hover snaps the targeting arrow head to the same target anchor"),
		Board->GetTargetingPointerPositionForTest().Equals(OutcomeTargetCenter, 0.01f));
	TestEqual(TEXT("single outcome uses exact alignment"), Board->GetSingleOutcomePreviewAlignmentForTest(), FVector2D(0.5f, 1.0f));
	TestEqual(TEXT("single outcome sits above the full target visual without changing its compact size"),
		Board->GetSingleOutcomePreviewOffsetsForTest(), FMargin(0.0f, -217.0f, 272.0f, 56.0f));
	const UCanvasPanelSlot* const ProjectedLayerSlot = Cast<UCanvasPanelSlot>(Board->GetBattleProjectedUnitHudLayerForTest()->Slot);
	const UCanvasPanelSlot* const RootCanvasSlot = Cast<UCanvasPanelSlot>(Board->GetBattleControlsLayerForTest()->Slot);
	TestNotNull(TEXT("projected HUD layer remains attached by Canvas slot"), ProjectedLayerSlot);
	TestNotNull(TEXT("RootCanvas remains attached by Canvas slot"), RootCanvasSlot);
	TestEqual(TEXT("projected HUD layer remains z=0"), ProjectedLayerSlot ? ProjectedLayerSlot->GetZOrder() : INDEX_NONE, 0);
	TestEqual(TEXT("RootCanvas remains z=20 on BattleDesignStage"), RootCanvasSlot ? RootCanvasSlot->GetZOrder() : INDEX_NONE, 20);

	UGameInstance* const GroupGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* const GroupSubsystem = NewObject<UGameXXKMVPSubsystem>(GroupGameInstance);
	FName GroupCardId;
	FGameXXKCardPlayPreview GroupPlayability;
	FGameXXKCardOutcomePreview GroupOutcome;
	TestTrue(TEXT("layout group fixture builds"), BuildPureEnemyGroupCardFixture(
		GroupSubsystem, GroupCardId, GroupPlayability, GroupOutcome, Error));
	UGameXXKBattleBoardWidget* const GroupBoard = NewObject<UGameXXKBattleBoardWidget>();
	GroupBoard->SetMVPSubsystem(GroupSubsystem);
	TestTrue(TEXT("layout group Board initializes"), GroupBoard->Initialize());
	GroupBoard->NativeConstruct();
	const TMap<FName, FCanvasLayoutSnapshot> BeforeGroupHover = CaptureNonOutcomeCanvasLayout(GroupBoard);
	UButton* const GroupButton = GroupBoard->GetHandCardButtonForTest(0);
	TestNotNull(TEXT("layout group uses a real hand button"), GroupButton);
	if (GroupButton)
	{
		GroupButton->OnHovered.Broadcast();
	}
	const TMap<FName, FCanvasLayoutSnapshot> AfterGroupHover = CaptureNonOutcomeCanvasLayout(GroupBoard);
	AssertCanvasLayoutUnchanged(*this, BeforeGroupHover, AfterGroupHover);
	UBorder* const GroupDetailPanel = GroupBoard->WidgetTree
		? Cast<UBorder>(GroupBoard->WidgetTree->FindWidget(TEXT("BattleHandCardDetailPanel")))
		: nullptr;
	TestEqual(TEXT("group hand hover shows the hover-following card tooltip"),
		GroupDetailPanel ? GroupDetailPanel->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	UGameXXKCardOutcomePreviewWidget* const VisibleGroupOutcomeWidget = GroupBoard->WidgetTree
		? Cast<UGameXXKCardOutcomePreviewWidget>(GroupBoard->WidgetTree->FindWidget(TEXT("BattleGroupOutcomePreview")))
		: nullptr;
	TestNotNull(TEXT("group Board retains the concrete named group outcome widget"), VisibleGroupOutcomeWidget);
	TestTrue(TEXT("visible group outcome widget is directly parented by its outcome overlay"),
		VisibleGroupOutcomeWidget
			&& VisibleGroupOutcomeWidget->GetParent() == GroupBoard->GetBattleOutcomePreviewLayerForTest());
	TestEqual(TEXT("visible group outcome widget is input transparent"),
		VisibleGroupOutcomeWidget ? VisibleGroupOutcomeWidget->GetVisibility() : ESlateVisibility::Collapsed,
		ESlateVisibility::HitTestInvisible);
	if (VisibleGroupOutcomeWidget)
	{
		VisibleGroupOutcomeWidget->TakeWidget();
		TArray<UWidget*> GroupOutcomeChildren;
		VisibleGroupOutcomeWidget->WidgetTree->GetAllWidgets(GroupOutcomeChildren);
		TestTrue(TEXT("visible group outcome widget has real generated children"), !GroupOutcomeChildren.IsEmpty());
		for (UWidget* const Child : GroupOutcomeChildren)
		{
			TestEqual(*FString::Printf(TEXT("group outcome child %s is input transparent"), *GetNameSafe(Child)),
				Child ? Child->GetVisibility() : ESlateVisibility::Collapsed,
				ESlateVisibility::HitTestInvisible);
		}
	}
	TestEqual(TEXT("group outcome keeps the fixed 0.245/0.34 anchor"), GroupBoard->GetGroupOutcomePreviewAnchorForTest(), FVector2D(0.245f, 0.34f));
	TestEqual(TEXT("group outcome uses exact alignment"), GroupBoard->GetGroupOutcomePreviewAlignmentForTest(), FVector2D(0.5f, 1.0f));
	TestEqual(TEXT("group outcome uses exact offsets/size"), GroupBoard->GetGroupOutcomePreviewOffsetsForTest(), FMargin(0.0f, 0.0f, 620.0f, 108.0f));
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
	TestFalse(TEXT("the insight tooltip keeps a concise non-empty description"), Board->GetCardTooltipTextForTest().IsEmpty());
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
	FGameXXKCardBattleBoardPendingHeroTaskSearchTest,
	"GameXXK.Integration.CardBattle.BoardPendingHeroTaskSearch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardPendingHeroTaskSearchTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Battle;
	State.bHasActiveBattle = true;
	State.ActiveBattleNodeId = 28;
	State.ActiveBattleEnemies = {MakeEnemy(TEXT("MoneyRat"), TEXT("钱鼠"))};

	FString Error;
	TestTrue(FString::Printf(TEXT("Mage-search fixture initializes card run: %s"), *Error),
		FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &Error));
	TestTrue(FString::Printf(TEXT("Mage-search fixture begins card battle: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(State, EGameXXKNodeKind::Battle, EGameXXKCardTerrain::Plain, 329, &Error));
	FGameXXKCardBattleRuntime& Runtime = State.CardRun.ActiveBattle;
	FGameXXKBattleDeckState& Deck = Runtime.Deck;
	if (Deck.Hand.Num() >= Deck.HandLimit)
	{
		TestTrue(TEXT("Mage-search fixture frees one hand slot"),
			GameXXKCardRules::MoveHandCardToDiscard(Deck, Deck.Hand.Last().InstanceId, &Error));
	}
	TestTrue(FString::Printf(TEXT("Mage-search fixture first opens the canonical insight panel: %s"), *Error),
		GameXXKCardRules::BeginInsight(Deck, 2, &Error));

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("Mage-search board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();

	UWidget* InsightPanel = Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePanel")) : nullptr;
	UCanvasPanelSlot* InsightPanelSlot = InsightPanel ? Cast<UCanvasPanelSlot>(InsightPanel->Slot) : nullptr;
	USizeBox* InsightCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCardSize_00"))) : nullptr;
	TestNotNull(TEXT("canonical insight panel exists"), InsightPanel);
	TestNotNull(TEXT("canonical insight panel keeps its canvas slot"), InsightPanelSlot);
	TestNotNull(TEXT("canonical insight candidate keeps its size box"), InsightCardSize);
	if (!InsightPanel || !InsightPanelSlot || !InsightCardSize)
	{
		return false;
	}
	const FAnchors InsightAnchors = InsightPanelSlot->GetAnchors();
	const FMargin InsightOffsets = InsightPanelSlot->GetOffsets();
	const float InsightCardWidth = InsightCardSize->GetWidthOverride();
	const float InsightCardHeight = InsightCardSize->GetHeightOverride();

	TestTrue(FString::Printf(TEXT("canonical insight cancels before installing the Mage search fixture: %s"), *Error),
		GameXXKCardRules::CancelInsight(Deck, &Error));
	const FGameXXKCardInstance* SearchCandidate = Runtime.Deck.DrawPile.FindByPredicate([&Runtime](const FGameXXKCardInstance& Card)
	{
		return Runtime.EquippedHeroCardIds.Contains(Card.CardId);
	});
	if (!SearchCandidate)
	{
		SearchCandidate = Runtime.Deck.DiscardPile.FindByPredicate([&Runtime](const FGameXXKCardInstance& Card)
		{
			return Runtime.EquippedHeroCardIds.Contains(Card.CardId);
		});
	}
	TestNotNull(TEXT("Mage-search fixture finds a real equipped protagonist card in draw or discard"), SearchCandidate);
	if (!SearchCandidate)
	{
		return false;
	}
	const FGameXXKCardInstance OfferedCandidate = *SearchCandidate;
	Runtime.HeroSpellTask = FGameXXKHeroSpellTaskRuntime();
	Runtime.HeroSpellTask.bActive = true;
	Runtime.HeroSpellTask.LockedHeroCardIds = Runtime.EquippedHeroCardIds;
	Runtime.HeroSpellTask.StarterReward = EGameXXKHeroSpellTaskReward::Universal;
	Runtime.HeroSpellTask.StarterOwnerUnitId = OfferedCandidate.OwnerUnitId;
	Deck.PendingChoice = FGameXXKPendingCardChoice();
	Deck.PendingChoice.Kind = EGameXXKCardPendingChoiceKind::HeroTaskSearchChooseToHand;
	Deck.PendingChoice.Candidates = {OfferedCandidate};
	Deck.PendingChoice.RequiredCount = 1;
	Deck.PendingChoice.RequiredHandPickCount = 1;
	Deck.PendingChoice.bCanCancel = false;
	TestTrue(FString::Printf(TEXT("Mage-search fixture is a valid saved runtime: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(Runtime, &Error));

	Board->RefreshFromState();
	UWidget* SearchPanel = Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePanel")) : nullptr;
	UCanvasPanelSlot* SearchPanelSlot = SearchPanel ? Cast<UCanvasPanelSlot>(SearchPanel->Slot) : nullptr;
	USizeBox* SearchCardSize = Board->WidgetTree ? Cast<USizeBox>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCardSize_00"))) : nullptr;
	UTextBlock* SearchPrompt = Board->WidgetTree ? Cast<UTextBlock>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoicePrompt"))) : nullptr;
	TestTrue(TEXT("Mage search reuses the exact existing pending-choice panel instance"), SearchPanel == InsightPanel);
	TestTrue(TEXT("Mage search makes the shared panel visible"), SearchPanel && SearchPanel->GetVisibility() == ESlateVisibility::Visible);
	TestNotNull(TEXT("Mage search retains the existing panel canvas slot"), SearchPanelSlot);
	TestNotNull(TEXT("Mage search retains the existing candidate size box"), SearchCardSize);
	TestNotNull(TEXT("Mage search exposes the existing prompt text block"), SearchPrompt);
	if (SearchPanelSlot)
	{
		TestEqual(TEXT("Mage search keeps the insight panel minimum anchor"), SearchPanelSlot->GetAnchors().Minimum, InsightAnchors.Minimum);
		TestEqual(TEXT("Mage search keeps the insight panel maximum anchor"), SearchPanelSlot->GetAnchors().Maximum, InsightAnchors.Maximum);
		const FMargin SearchOffsets = SearchPanelSlot->GetOffsets();
		TestEqual(TEXT("Mage search keeps the insight panel left offset"), SearchOffsets.Left, InsightOffsets.Left);
		TestEqual(TEXT("Mage search keeps the insight panel top offset"), SearchOffsets.Top, InsightOffsets.Top);
		TestEqual(TEXT("Mage search keeps the insight panel width"), SearchOffsets.Right, InsightOffsets.Right);
		TestEqual(TEXT("Mage search keeps the insight panel height"), SearchOffsets.Bottom, InsightOffsets.Bottom);
	}
	if (SearchCardSize)
	{
		TestEqual(TEXT("Mage search keeps the insight candidate width"), SearchCardSize->GetWidthOverride(), InsightCardWidth);
		TestEqual(TEXT("Mage search keeps the insight candidate height"), SearchCardSize->GetHeightOverride(), InsightCardHeight);
	}
	TestEqual(TEXT("Mage search uses the approved concise prompt"), SearchPrompt ? SearchPrompt->GetText().ToString() : FString(), FString(TEXT("选择一张尚未完成任务的主角牌")));

	UButton* FirstChoiceButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattlePendingChoiceCard_00"))) : nullptr;
	TestNotNull(TEXT("Mage search presents its real candidate through the existing card button"), FirstChoiceButton);
	if (!FirstChoiceButton)
	{
		return false;
	}
	FirstChoiceButton->OnHovered.Broadcast();
	TestFalse(TEXT("Mage-search hover keeps a concise non-empty description"), Board->GetCardTooltipTextForTest().IsEmpty());
	FirstChoiceButton->OnUnhovered.Broadcast();
	FirstChoiceButton->OnClicked.Broadcast();
	TestEqual(TEXT("clicking the Mage candidate clears the blocking search"),
		State.CardRun.ActiveBattle.Deck.PendingChoice.Kind,
		EGameXXKCardPendingChoiceKind::None);
	TestTrue(TEXT("clicking the Mage candidate moves the same real instance into hand"),
		State.CardRun.ActiveBattle.Deck.Hand.ContainsByPredicate([OfferedCandidate](const FGameXXKCardInstance& Card)
		{
			return Card.InstanceId == OfferedCandidate.InstanceId;
		}));
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
	TestFalse(TEXT("the forced-discard tooltip keeps a concise non-empty description"), Board->GetCardTooltipTextForTest().IsEmpty());
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
	TestFalse(TEXT("concise hand tooltip omits the legacy interaction instruction"), DetailText.Contains(TEXT("点击后选择高亮合法目标。")));
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
	TestTrue(TEXT("battle board exposes the pending three-choice reward state"), Board->HasPendingRouteReward());
	TestEqual(TEXT("battle board exposes exactly three saved reward slots"), Board->GetPendingRouteRewardCardIds().Num(), 3);
	const TArray<FGameXXKBattleRewardOption>& RewardOptions = Subsystem->GetRuntimeState().CardRun.PendingReward.Options;
	TestEqual(TEXT("the normal battle reward opens with a relic option"), RewardOptions[0].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle reward follows with a second relic option"), RewardOptions[1].Kind, EGameXXKBattleRewardKind::Relic);
	TestEqual(TEXT("the normal battle reward ends with a deck-card upgrade option"), RewardOptions[2].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
	TestEqual(TEXT("reward overlay hides the spent battle hand"), Board->GetVisibleHandCardCountForTest(), 0);
	UButton* RelicRewardButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleRewardCard_00"))) : nullptr;
	UButton* UpgradeRewardButton = Board->WidgetTree ? Cast<UButton>(Board->WidgetTree->FindWidget(TEXT("BattleRewardCard_02"))) : nullptr;
	TestNotNull(TEXT("the first relic option remains a hoverable reward slot"), RelicRewardButton);
	TestNotNull(TEXT("the deck-card upgrade option remains a hoverable reward card"), UpgradeRewardButton);
	if (!RelicRewardButton || !UpgradeRewardButton)
	{
		return false;
	}
	TestTrue(TEXT("the relic reward slot keeps its loaded centered relic icon visible"),
		Board->IsRewardPortraitVisibleForTest(0));
	// Slot 2 (DeckCardUpgrade) portrait visibility follows the existing card-art
	// presentation logic (visible only when the card binds a portrait); no strong assertion.
	const TArray<FName> RewardIdsBeforeHover = Board->GetPendingRouteRewardCardIds();
	const FGameXXKRuntimeState StateBeforeRewardHover = Subsystem->GetRuntimeState();
	RelicRewardButton->OnHovered.Broadcast();
	const FGameXXKRelicDefinition* RewardRelic = FGameXXKRelicCatalog::FindDefinition(RewardOptions[0].RelicId);
	TestTrue(TEXT("hovering a relic reward option reveals the shared tooltip panel"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("the relic reward tooltip states the relic name"),
		RewardRelic && Board->GetCardTooltipTextForTest().Contains(RewardRelic->DisplayName.ToString()));
	TestTrue(TEXT("the relic reward tooltip states the relic description"),
		RewardRelic && Board->GetCardTooltipTextForTest().Contains(RewardRelic->Description.ToString()));
	const FMargin TooltipOffsetsSlot0 = Board->GetHandCardDetailPanelOffsetsForTest();
	RelicRewardButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a relic reward hides the shared tooltip panel"), Board->IsCardTooltipVisibleForTest());
	UpgradeRewardButton->OnHovered.Broadcast();
	TestTrue(TEXT("hovering the deck-card upgrade option reveals the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	const FGameXXKCardDefinition* UpgradeTooltipDefinition = FGameXXKCardCatalog::FindCardDefinition(RewardOptions[2].CardId);
	TestTrue(TEXT("the reward tooltip titles the actual upgrade card"),
		UpgradeTooltipDefinition && Board->GetCardTooltipTextForTest().Contains(UpgradeTooltipDefinition->DisplayName.ToString()));
	TestEqual(TEXT("reward hover preserves every saved reward slot id"), Board->GetPendingRouteRewardCardIds(), RewardIdsBeforeHover);
	TestTrue(TEXT("reward hover preserves the complete runtime state"),
		RuntimeStatesEqual(Subsystem->GetRuntimeState(), StateBeforeRewardHover));
	const FMargin TooltipOffsetsSlot2 = Board->GetHandCardDetailPanelOffsetsForTest();
	TestTrue(TEXT("the tooltip follows the hovered reward slot instead of a fixed anchor"),
		FMath::Abs((TooltipOffsetsSlot2.Left - TooltipOffsetsSlot0.Left) - 2.0f * (206.0f + 10.0f)) < 2.0f);
	UpgradeRewardButton->OnUnhovered.Broadcast();
	TestFalse(TEXT("leaving a reward immediately hides the shared card tooltip"), Board->IsCardTooltipVisibleForTest());
	TestTrue(TEXT("skip reward resolves through adapter then Rules victory gate"), Board->SkipPendingRouteReward());
	TestEqual(TEXT("skipping a reward advances the route map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleBoardBossCardRewardTest,
	"GameXXK.Integration.CardBattle.BoardBossCardReward",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleBoardBossCardRewardTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FString Error;
	TestTrue(FString::Printf(TEXT("boss fixture enters a playable boss card battle: %s"), *Error),
		BuildBossRewardFixture(Subsystem, Error));

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	TestTrue(TEXT("the boss fixture starts with an empty boss-card slot"),
		State.CardRun.BossCardSlots.IsEmpty());

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("boss-card board initializes its widget tree"), Board->Initialize());
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
	if (!TestTrue(TEXT("the saved victory gate creates a pending boss reward"), Subsystem->ResolveBattleVictory(true)))
	{
		return false;
	}
	Board->RefreshFromState();

	const TArray<FGameXXKBattleRewardOption>& RewardOptions = State.CardRun.PendingReward.Options;
	if (!TestEqual(TEXT("the boss victory offers exactly three tiered options"), RewardOptions.Num(), 3))
	{
		return false;
	}
	TestEqual(TEXT("the boss reward opens with a boss-card option"), RewardOptions[0].Kind, EGameXXKBattleRewardKind::BossCard);
	TestEqual(TEXT("the boss reward follows with a deck-card upgrade option"), RewardOptions[1].Kind, EGameXXKBattleRewardKind::DeckCardUpgrade);
	TestEqual(TEXT("the boss reward ends with a relic option"), RewardOptions[2].Kind, EGameXXKBattleRewardKind::Relic);
	const FName BossRewardCardId = RewardOptions[0].CardId;
	TestTrue(TEXT("the boss-card option names a route boss card"), BossRewardCardId.ToString().StartsWith(TEXT("Route.Boss.")));
	constexpr int32 BossOptionIndex = 0;

	FGameXXKRouteCardAcquisitionPreview BossPreview;
	TestTrue(FString::Printf(TEXT("a free boss slot previews the boss option as directly committable: %s"), *Error),
		FGameXXKCardBattleAdapter::PreviewPendingRouteReward(
			State,
			BossRewardCardId,
			NAME_None,
			BossPreview,
			&Error));
	TestEqual(TEXT("the free-slot preview decision is CanCommit"),
		BossPreview.Decision,
		EGameXXKRouteCardAcquisitionDecision::CanCommit);
	TestEqual(TEXT("the free-slot preview names the boss card"), BossPreview.CardId, BossRewardCardId);

	TestTrue(TEXT("one Board click commits the boss card and resolves the reward"),
		Board->ChoosePendingBattleRewardOption(BossOptionIndex, NAME_None));
	TestTrue(TEXT("the committed boss card occupies one of the three boss slots"),
		State.CardRun.BossCardSlots.Contains(BossRewardCardId));
	TestEqual(TEXT("the boss slot stays within the three-card cap"), State.CardRun.BossCardSlots.Num(), 1);
	TestTrue(TEXT("reward resolution advances the boss battle to the next chapter map"),
		State.Screen == EGameXXKScreen::DungeonMap);
	TestEqual(TEXT("the resolved reward clears the saved pending offer"),
		State.CardRun.PendingReward.Options.Num(), 0);

	TestEqual(TEXT("the removed replacement state machine leaves no awaiting reward card"),
		Board->GetRouteRewardCardIdAwaitingReplacementForTest(),
		NAME_None);
	TestEqual(TEXT("the removed replacement state machine leaves no selected entry"),
		Board->GetSelectedRouteRewardReplacementEntryIdForTest(),
		NAME_None);
	TestTrue(TEXT("the removed replacement state machine exposes no eligible replacement entries"),
		Board->GetRouteRewardReplacementEntryIds().IsEmpty());

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
	TestTrue(TEXT("victory gate creates a pending tiered route reward"), Subsystem->ResolveBattleVictory(false));

	FString ValidationError;
	TestTrue(TEXT("validator accepts the pending tiered offer"),
		FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError));

	FGameXXKRuntimeState RollbackState = State;
	RollbackState.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState RollbackBefore = RollbackState;
	TestFalse(TEXT("choose-and-finish reports a deliberately failed victory settlement"),
		UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
			RollbackState,
			2,
			NAME_None,
			&Error));
	TestTrue(TEXT("failed post-choice victory settlement rolls back the complete runtime"),
		RuntimeStatesEqual(RollbackState, RollbackBefore));

	FGameXXKRuntimeState SkipRollbackState = State;
	SkipRollbackState.CardRun.RouteTravelMoney = MAX_int32;
	const FGameXXKRuntimeState SkipRollbackBefore = SkipRollbackState;
	TestFalse(TEXT("skip-and-finish reports a deliberately failed victory settlement"),
		UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(SkipRollbackState, &Error));
	TestTrue(TEXT("failed post-skip victory settlement rolls back the complete runtime"),
		RuntimeStatesEqual(SkipRollbackState, SkipRollbackBefore));

	FGameXXKRuntimeState DirectState = State;
	TestTrue(TEXT("one rules facade call commits a tiered option and finishes victory"),
		UGameXXKMVPRules::ResolvePendingBattleRewardChoiceAndFinish(
			DirectState,
			2,
			NAME_None,
			&Error));
	TestEqual(TEXT("direct facade returns to the route map"), DirectState.Screen, EGameXXKScreen::DungeonMap);

	FGameXXKRuntimeState SkipState = State;
	TestTrue(TEXT("one rules facade call skips and finishes victory"),
		UGameXXKMVPRules::SkipPendingRouteRewardAndFinish(SkipState, &Error));
	TestEqual(TEXT("skip facade returns to the route map"), SkipState.Screen, EGameXXKScreen::DungeonMap);
	return true;
}

#endif
