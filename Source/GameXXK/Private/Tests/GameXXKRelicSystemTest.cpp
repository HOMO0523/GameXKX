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
	AddError(TEXT("The required 31-entry relic catalog has not been implemented."));
	return false;
#else
	TestTrue(TEXT("relic definitions remain eligible for ordinary offers by default"), FGameXXKRelicDefinition().bOfferEligible);
	const TArray<FGameXXKRelicDefinition>& Definitions = FGameXXKRelicCatalog::GetAllDefinitions();
	TestEqual(TEXT("the relic catalog exposes thirty ordinary relics plus the camp-exclusive charm"), Definitions.Num(), 31);
	TSet<FName> UniqueIds;
	int32 OfferEligibleRelicCount = 0;
	for (const FGameXXKRelicDefinition& Definition : Definitions)
	{
		TestFalse(TEXT("every relic has a stable id"), Definition.Id.IsNone());
		TestFalse(TEXT("every relic has a Chinese display name"), Definition.DisplayName.IsEmpty());
		TestFalse(TEXT("every relic explains its live effect"), Definition.Description.IsEmpty());
		TestTrue(TEXT("every relic binds a project texture"), Definition.IconTexturePath.ToString().Contains(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_")));
		UniqueIds.Add(Definition.Id);
		OfferEligibleRelicCount += Definition.bOfferEligible ? 1 : 0;
	}
	TestEqual(TEXT("all thirty-one relic ids are distinct"), UniqueIds.Num(), 31);
	TestEqual(TEXT("exactly thirty relics remain eligible for ordinary offers"), OfferEligibleRelicCount, 30);

	const FName LifeSavingTalismanId(TEXT("Relic.LifeSavingTalisman"));
	const FGameXXKRelicDefinition* LifeSavingTalisman = FGameXXKRelicCatalog::FindDefinition(LifeSavingTalismanId);
	if (TestNotNull(TEXT("the camp-exclusive life-saving talisman has a stable catalog id"), LifeSavingTalisman))
	{
		TestEqual(TEXT("the life-saving talisman preserves its stable id"), LifeSavingTalisman->Id, LifeSavingTalismanId);
		TestEqual(TEXT("the life-saving talisman uses its approved Chinese display name"), LifeSavingTalisman->DisplayName.ToString(), FString(TEXT("保命护符")));
		TestEqual(TEXT("the life-saving talisman documents its complete live effect"), LifeSavingTalisman->Description.ToString(), FString(TEXT("战斗中任一角色气血低于50%时，消耗此遗物，使全队恢复30%最大气血。")));
		TestEqual(TEXT("the life-saving talisman reacts after damage"), LifeSavingTalisman->Trigger, EGameXXKRelicTrigger::DamageTaken);
		TestEqual(TEXT("the life-saving talisman declares the emergency party-heal effect"), LifeSavingTalisman->EffectKind, EGameXXKRelicEffectKind::EmergencyHealPartyPercent);
		TestEqual(TEXT("the life-saving talisman stores a thirty-percent magnitude"), LifeSavingTalisman->Magnitude, 30);
		TestEqual(TEXT("the life-saving talisman is Common quality"), LifeSavingTalisman->BaseQuality, EGameXXKCardQuality::Common);
		TestFalse(TEXT("the life-saving talisman is unique and non-stackable"), LifeSavingTalisman->bStackable);
		TestFalse(TEXT("the life-saving talisman is excluded from ordinary offers"), LifeSavingTalisman->bOfferEligible);
		TestEqual(
			TEXT("the life-saving talisman binds the exact approved icon"),
			LifeSavingTalisman->IconTexturePath.ToString(),
			FString(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_LifeSavingTalisman.T_Relic_LifeSavingTalisman")));
	}

	FGameXXKRuntimeState UniqueCharmState = UGameXXKMVPRules::CreateNewGame();
	FString UniqueCharmError;
	TestTrue(TEXT("a run can acquire the life-saving talisman once"),
		FGameXXKRelicRules::AcquireRelic(UniqueCharmState, LifeSavingTalismanId, &UniqueCharmError));
	UniqueCharmError.Reset();
	TestFalse(TEXT("a run cannot acquire the unique life-saving talisman twice"),
		FGameXXKRelicRules::AcquireRelic(UniqueCharmState, LifeSavingTalismanId, &UniqueCharmError));
	TestFalse(TEXT("duplicate life-saving talisman acquisition reports why it was rejected"), UniqueCharmError.IsEmpty());
	TestEqual(TEXT("duplicate life-saving talisman acquisition keeps one instance"), UniqueCharmState.CardRun.Relics.Num(), 1);
	if (!UniqueCharmState.CardRun.Relics.IsEmpty())
	{
		TestEqual(TEXT("the unique life-saving talisman never gains stacks"), UniqueCharmState.CardRun.Relics[0].Stacks, 1);
	}

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
	FGameXXKCampExclusiveRelicOfferTest,
	"GameXXK.Route.Relics.CampExclusiveRelicIsNotOrdinaryOffer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCampExclusiveRelicOfferTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required camp-exclusive relic catalog has not been implemented."));
	return false;
#else
	const FName LifeSavingTalismanId(TEXT("Relic.LifeSavingTalisman"));
	if (!TestNotNull(TEXT("the ordinary-offer exclusion test finds the life-saving talisman"),
		FGameXXKRelicCatalog::FindDefinition(LifeSavingTalismanId)))
	{
		return false;
	}

	// With all 31 catalog entries in the pool, seed 12 selects the appended
	// life-saving talisman first. This fixed seed makes eligibility regressions
	// fail deterministically without a probabilistic seed sweep.
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	TArray<FName> OfferedRelicIds;
	FString OfferError;
	if (!TestTrue(TEXT("the fixed seed creates an ordinary three-relic offer"),
		FGameXXKRelicRules::CreateRelicOffer(State, 12, 12, OfferedRelicIds, &OfferError)))
	{
		AddError(FString::Printf(TEXT("ordinary relic offer failed: %s"), *OfferError));
		return false;
	}
	TestFalse(TEXT("ordinary relic offers exclude the camp-exclusive life-saving talisman"),
		OfferedRelicIds.Contains(LifeSavingTalismanId));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNonCardRelicTriggerCompatibilityTest,
	"GameXXK.Route.Relics.NonCardCombatTriggersPreserveLegacyEffects",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNonCardRelicTriggerCompatibilityTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required non-card relic trigger behavior has not been implemented."));
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
	FGameXXKCardCombatUnit Ally = Hero;
	Ally.UnitId = TEXT("Relic.Legacy.Ally");
	Ally.HP = 70;
	State.CardRun.ActiveBattle.Units = {Hero, Ally};
	TestTrue(TEXT("the compatibility fixture acquires the battle-start armor relic"),
		FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.AncientCoin")));
	TestTrue(TEXT("the compatibility fixture acquires the round-start owner armor relic"),
		FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.StoneBead")));
	TestTrue(TEXT("the compatibility fixture acquires the round-end healing relic"),
		FGameXXKRelicRules::AcquireRelic(State, TEXT("Relic.RedCord")));

	FGameXXKRelicRules::ApplyBattleStart(State);
	FGameXXKRelicRules::ApplyPlayerRoundStart(State);
	FGameXXKRelicRules::ApplyPlayerRoundEnd(State);
	const FGameXXKCardCombatUnit* HeroAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	const FGameXXKCardCombatUnit* AllyAfter = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Relic.Legacy.Ally");
	});
	if (!TestNotNull(TEXT("the compatibility fixture retains the hero"), HeroAfter)
		|| !TestNotNull(TEXT("the compatibility fixture retains the ally"), AllyAfter))
	{
		return false;
	}
	TestEqual(TEXT("battle-start plus round-start relics preserve hero armor timing"), HeroAfter->Armor, 7);
	TestEqual(TEXT("battle-start relic preserves ally armor timing"), AllyAfter->Armor, 4);
	TestEqual(TEXT("round-end relic preserves hero healing"), HeroAfter->HP, 53);
	TestEqual(TEXT("round-end relic preserves ally healing"), AllyAfter->HP, 73);
	return true;
#endif
}

#endif
