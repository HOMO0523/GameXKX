#include "Misc/AutomationTest.h"
// Re-evaluate the optional-include gate whenever the relic implementation is introduced.

#if __has_include("GameXXKRelicCatalog.h") && __has_include("GameXXKRelicRules.h") && __has_include("UI/GameXXKRelicBarWidget.h")
#define GAMEXXK_HAS_RELIC_SYSTEM 1
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKRelicBarWidget.h"
#else
#define GAMEXXK_HAS_RELIC_SYSTEM 0
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRelicCatalogTest,
	"GameXXK.Route.Relics.CatalogAndRunLifetime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRelicCatalogTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required 30-relic route system has not been implemented."));
	return false;
#else
	const TArray<FGameXXKRelicDefinition>& Definitions = FGameXXKRelicCatalog::GetAllDefinitions();
	TestEqual(TEXT("the route run exposes exactly thirty designed relics"), Definitions.Num(), 30);
	TSet<FName> UniqueIds;
	for (const FGameXXKRelicDefinition& Definition : Definitions)
	{
		TestFalse(TEXT("every relic has a stable id"), Definition.Id.IsNone());
		TestFalse(TEXT("every relic has a Chinese display name"), Definition.DisplayName.IsEmpty());
		TestFalse(TEXT("every relic explains its live effect"), Definition.Description.IsEmpty());
		TestTrue(TEXT("every relic binds a project texture"), Definition.IconTexturePath.ToString().Contains(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_")));
		UniqueIds.Add(Definition.Id);
	}
	TestEqual(TEXT("all thirty relic ids are distinct"), UniqueIds.Num(), 30);

	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("a fresh run can acquire its first relic"), FGameXXKRelicRules::AcquireRelic(State, Definitions[0].Id));
	TestTrue(TEXT("a second acquisition is retained in the same run"), FGameXXKRelicRules::AcquireRelic(State, Definitions[1].Id));
	TestEqual(TEXT("the latest relic is stored first for right-top HUD priority"), State.CardRun.Relics[0].RelicId, Definitions[1].Id);
	TestEqual(TEXT("both relics persist in the active route state"), State.CardRun.Relics.Num(), 2);
	FGameXXKRelicRules::ClearRouteRelics(State);
	TestTrue(TEXT("leaving the route clears all run-only relics"), State.CardRun.Relics.IsEmpty());
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRelicBarWidgetTest,
	"GameXXK.UI.Relics.SixColumnWrapAndTooltip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRelicBarWidgetTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required right-top relic bar has not been implemented."));
	return false;
#else
	UGameXXKRelicBarWidget* Bar = NewObject<UGameXXKRelicBarWidget>();
	TestTrue(TEXT("the relic bar builds its native runtime layout"), Bar->PrepareForEmbedding());
	TestEqual(TEXT("the bar has a fixed six-column contract"), Bar->GetColumnCountForTest(), 6);
	TestEqual(TEXT("seven relics wrap to two rows"), Bar->CalculateRowCountForTest(7), 2);
	TestTrue(TEXT("every generated icon owns a hover tooltip"), Bar->UsesTooltipsForTest());
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRelicBarHitTestTest,
	"GameXXK.UI.Relics.FullscreenRootDoesNotBlockBattleClicks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRelicBarHitTestTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required right-top relic bar has not been implemented."));
	return false;
#else
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.bDungeonActive = true;
	State.Screen = EGameXXKScreen::Battle;
	const TArray<FGameXXKRelicDefinition>& Definitions = FGameXXKRelicCatalog::GetAllDefinitions();
	if (Definitions.IsEmpty() || !FGameXXKRelicRules::AcquireRelic(State, Definitions[0].Id))
	{
		AddError(TEXT("Could not create the acquired-relic fixture."));
		return false;
	}

	UGameXXKRelicBarWidget* Bar = NewObject<UGameXXKRelicBarWidget>(TestGameInstance);
	Bar->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("the relic bar builds its native runtime layout"), Bar->PrepareForEmbedding());
	Bar->RefreshFromState();

	TestEqual(TEXT("the acquired relic is rendered"), Bar->GetRenderedRelicCountForTest(), 1);
	TestEqual(
		TEXT("the full-screen relic layer ignores its own hit test so battle buttons underneath remain clickable"),
		Bar->GetVisibility(),
		ESlateVisibility::SelfHitTestInvisible);
	UWidget* FullscreenRoot = Bar->GetWidgetFromName(TEXT("RelicBarRootCanvas"));
	TestNotNull(TEXT("the relic bar exposes its full-screen root"), FullscreenRoot);
	if (FullscreenRoot)
	{
		TestEqual(
			TEXT("the full-screen canvas also ignores its own hit test while leaving relic icon children interactive"),
			FullscreenRoot->GetVisibility(),
			ESlateVisibility::SelfHitTestInvisible);
	}
	return true;
#endif
}

namespace
{
	FGameXXKRuntimeState BuildSingleEncounterRoute(const EGameXXKNodeKind EncounterKind)
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteSeed = 20260722;
		State.CurrentRouteNodeId = 0;
		State.PendingRouteNodeId = INDEX_NONE;
		State.RouteMapNodes = {
			FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}},
			FGameXXKRouteMapNode{1, 1, 0, EncounterKind, FVector2D(0.5f, 0.5f), TArray<int32>{2}},
			FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}}
		};
		State.RouteMapEdges = { FGameXXKRouteMapEdge{0, 1}, FGameXXKRouteMapEdge{1, 2} };
		State.VisitedRouteNodeIds = {0};
		State.ReachableRouteNodeIds = {1};
		State.CardRun.RouteProgress.CurrentChapter = 1;
		FGameXXKRouteEconomyRules::InitializeRoute(State.CardRun);
		return State;
	}

	int32 SumRouteAttributes(const FGameXXKRouteAttributeBonuses& Bonuses)
	{
		return Bonuses.MaxHealth + Bonuses.MaxMana + Bonuses.Attack + Bonuses.Defense + Bonuses.Speed;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteRelicInteractionTest,
	"GameXXK.Route.Relics.EventAttributeAndChestChoice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteRelicInteractionTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required route encounter and relic interaction has not been implemented."));
	return false;
#else
	FGameXXKRuntimeState EventState = BuildSingleEncounterRoute(EGameXXKNodeKind::Event);
	TestTrue(TEXT("selecting a question-mark node opens a saved event"), UGameXXKMVPRules::SelectRouteNodeById(EventState, 1));
	const FGameXXKRouteEncounterDefinition* EventDefinition =
		FGameXXKRouteEncounterCatalog::FindDefinition(EventState.CardRun.PendingEvent.EncounterId);
	TestNotNull(TEXT("the event node resolves to one of the twelve designed entries"), EventDefinition);
	int32 AttributeChoiceIndex = INDEX_NONE;
	if (EventDefinition)
	{
		AttributeChoiceIndex = EventDefinition->Choices.IndexOfByPredicate([](const FGameXXKRouteEncounterChoiceDefinition& Choice)
		{
			return Choice.RewardKind == EGameXXKRouteEncounterRewardKind::RouteAttribute;
		});
	}
	TestTrue(TEXT("the event presents a character-attribute choice"), AttributeChoiceIndex != INDEX_NONE);
	const int32 AttributeTotalBefore = SumRouteAttributes(EventState.CardRun.RouteAttributeBonuses);
	if (AttributeChoiceIndex != INDEX_NONE)
	{
		TestTrue(TEXT("choosing the event attribute resolves the pending node"),
			UGameXXKMVPRules::ResolveRouteEncounterChoice(EventState, AttributeChoiceIndex));
	}
	TestTrue(TEXT("the event increases a route-local character attribute"),
		SumRouteAttributes(EventState.CardRun.RouteAttributeBonuses) > AttributeTotalBefore);
	TestEqual(TEXT("the resolved event returns to the route map"), EventState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the resolved event is marked visited"), EventState.VisitedRouteNodeIds.Contains(1));
	TestTrue(TEXT("events do not directly award relics"), EventState.CardRun.Relics.IsEmpty());

	FGameXXKRuntimeState ChestState = BuildSingleEncounterRoute(EGameXXKNodeKind::Chest);
	TestTrue(TEXT("selecting a treasure node opens its reward panel"), UGameXXKMVPRules::SelectRouteNodeById(ChestState, 1));
	TestEqual(TEXT("a treasure saves exactly three relic candidates"), ChestState.CardRun.PendingRelicOffer.RelicIds.Num(), 3);
	TSet<FName> UniqueOffers;
	for (const FName RelicId : ChestState.CardRun.PendingRelicOffer.RelicIds)
	{
		UniqueOffers.Add(RelicId);
	}
	TestEqual(TEXT("the three relic candidates are distinct"), UniqueOffers.Num(), 3);
	const int32 ChestTravelMoneyBeforeBypass = ChestState.CardRun.RouteTravelMoney;
	const TArray<FName> ChestOfferBeforeBypass = ChestState.CardRun.PendingRelicOffer.RelicIds;
	TestFalse(TEXT("a chest cannot be bypassed through the legacy event gold reward"),
		UGameXXKMVPRules::ResolveEventReward(ChestState, true));
	TestEqual(TEXT("a rejected chest bypass does not add travel money"),
		ChestState.CardRun.RouteTravelMoney, ChestTravelMoneyBeforeBypass);
	TestEqual(TEXT("a rejected chest bypass preserves all three explicit relic choices"),
		ChestState.CardRun.PendingRelicOffer.RelicIds, ChestOfferBeforeBypass);
	TestEqual(TEXT("a rejected chest bypass keeps the player on the relic choice screen"),
		ChestState.Screen, EGameXXKScreen::RouteEvent);
	const FName ChosenRelicId = ChestState.CardRun.PendingRelicOffer.RelicIds.IsValidIndex(1)
		? ChestState.CardRun.PendingRelicOffer.RelicIds[1]
		: NAME_None;
	TestTrue(TEXT("choosing one treasure relic resolves the pending node"),
		UGameXXKMVPRules::ResolveRouteEncounterChoice(ChestState, 1));
	TestEqual(TEXT("only one relic is gained"), ChestState.CardRun.Relics.Num(), 1);
	if (!ChestState.CardRun.Relics.IsEmpty())
	{
		TestEqual(TEXT("the acquired relic matches the clicked middle choice"), ChestState.CardRun.Relics[0].RelicId, ChosenRelicId);
	}
	TestTrue(TEXT("the three-choice offer is cleared after selection"), ChestState.CardRun.PendingRelicOffer.RelicIds.IsEmpty());
	TestEqual(TEXT("the treasure returns to the route map instead of trapping the player"), ChestState.Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("the treasure node is marked visited"), ChestState.VisitedRouteNodeIds.Contains(1));

	ChestState.CardRun.RouteAttributeBonuses.Attack = 7;
	TestTrue(TEXT("leaving the route succeeds"), UGameXXKMVPRules::FailDungeonToTown(ChestState));
	TestTrue(TEXT("route-end cleanup removes all relics"), ChestState.CardRun.Relics.IsEmpty());
	TestEqual(TEXT("route-end cleanup removes all event attribute bonuses"),
		SumRouteAttributes(ChestState.CardRun.RouteAttributeBonuses), 0);
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKRouteAttributeBattleProjectionTest,
	"GameXXK.Route.Relics.EventAttributesProjectOncePerBattle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKRouteAttributeBattleProjectionTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required route attribute battle projection has not been implemented."));
	return false;
#else
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the fixture reaches an active route"),
		UGameXXKMVPRules::OpenWorldMap(State)
		&& UGameXXKMVPRules::EnterWorldRegion(State, UGameXXKMVPRules::RegionQingshan())
		&& UGameXXKMVPRules::AcceptTownQuest(State)
		&& UGameXXKMVPRules::EnterDungeon(State));
	State.CardRun.RouteAttributeBonuses.MaxHealth = 8;
	State.CardRun.RouteAttributeBonuses.MaxMana = 4;
	State.CardRun.RouteAttributeBonuses.Attack = 3;
	State.bHasGeneratedRouteMap = false;
	State.RouteMapNodes.Reset();
	State.RouteMapEdges.Reset();
	State.ReachableRouteNodeIds.Reset();
	State.DungeonNodeIndex = 1;
	TestTrue(TEXT("the first battle accepts route-local character attributes"),
		UGameXXKMVPRules::AdvanceDungeonNode(State, EGameXXKNodeKind::Battle));
	const FGameXXKBattleRuntimeUnit* FirstHero = State.ActiveBattleParty.FindByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("Player");
	});
	TestNotNull(TEXT("the projected battle contains the hero"), FirstHero);
	if (!FirstHero)
	{
		return false;
	}
	const int32 ExpectedMaxHealth = State.PlayerMaxHP + 8;
	const int32 ExpectedMaxMana = State.PlayerMaxMP + 4;
	TestEqual(TEXT("event max-health bonus reaches the hero"), FirstHero->MaxHP, ExpectedMaxHealth);
	TestEqual(TEXT("event max-mana bonus reaches the hero"), FirstHero->MaxMP, ExpectedMaxMana);
	const int32 FirstAttack = FirstHero->Attack;

	FGameXXKCardBattleAdapter::ClearActiveCardBattle(State);
	FString Error;
	TestTrue(FString::Printf(TEXT("a second battle can be projected from the same route state: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			State,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			20260723,
			&Error));
	const FGameXXKBattleRuntimeUnit* SecondHero = State.ActiveBattleParty.FindByPredicate([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("Player");
	});
	TestNotNull(TEXT("the second battle still contains the hero"), SecondHero);
	if (SecondHero)
	{
		TestEqual(TEXT("route max-health bonus is not applied twice"), SecondHero->MaxHP, ExpectedMaxHealth);
		TestEqual(TEXT("route max-mana bonus is not applied twice"), SecondHero->MaxMP, ExpectedMaxMana);
		TestEqual(TEXT("route attack bonus is not applied twice"), SecondHero->Attack, FirstAttack);
	}
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDamageReactiveRelicOwnershipTest,
	"GameXXK.Route.Relics.DamageReactiveEffectsOnlyProtectParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDamageReactiveRelicOwnershipTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required damage-reactive relic behavior has not been implemented."));
	return false;
#else
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.CardRun.bHasActiveCardBattle = true;
	FGameXXKCardCombatUnit Hero;
	Hero.UnitId = TEXT("Player");
	Hero.Side = EGameXXKCardTargetSide::Party;
	Hero.MaxHP = 100;
	Hero.HP = 50;
	Hero.bLiving = true;
	FGameXXKCardCombatUnit Enemy;
	Enemy.UnitId = TEXT("Enemy.Test");
	Enemy.Side = EGameXXKCardTargetSide::Enemy;
	Enemy.MaxHP = 50;
	Enemy.HP = 40;
	Enemy.bLiving = true;
	State.CardRun.ActiveBattle.Units = {Hero, Enemy};
	TestTrue(TEXT("the route can own the reactive armor relic"), FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.PineCone")));
	TestTrue(TEXT("the route can own the reactive healing relic"), FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.RiverPearl")));

	FGameXXKCardDamageResult EnemyHit;
	EnemyHit.ResolvedTargetUnitId = Enemy.UnitId;
	EnemyHit.HealthDamage = 5;
	FGameXXKRelicRules::ApplyDamageTaken(State, {EnemyHit});
	const FGameXXKCardCombatUnit* EnemyAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Enemy.Test");
	});
	TestNotNull(TEXT("the enemy remains in the runtime"), EnemyAfter);
	if (EnemyAfter)
	{
		TestEqual(TEXT("player relics never heal a damaged enemy"), EnemyAfter->HP, 40);
		TestEqual(TEXT("player relics never armor a damaged enemy"), EnemyAfter->Armor, 0);
	}

	FGameXXKCardDamageResult HeroHit;
	HeroHit.ResolvedTargetUnitId = Hero.UnitId;
	HeroHit.HealthDamage = 5;
	FGameXXKRelicRules::ApplyDamageTaken(State, {HeroHit});
	const FGameXXKCardCombatUnit* HeroAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("the hero remains in the runtime"), HeroAfter);
	if (HeroAfter)
	{
		TestEqual(TEXT("the reactive healing relic restores the damaged party unit"), HeroAfter->HP, 51);
		TestEqual(TEXT("the reactive armor relic protects the damaged party unit"), HeroAfter->Armor, 2);
	}
	return true;
#endif
}

#endif
