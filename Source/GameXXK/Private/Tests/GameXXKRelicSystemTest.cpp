#include "Misc/AutomationTest.h"
// Re-evaluate the optional-include gate whenever the relic implementation is introduced.

#if __has_include("GameXXKRelicCatalog.h") && __has_include("GameXXKRelicRules.h") && __has_include("UI/GameXXKRelicBarWidget.h")
#define GAMEXXK_HAS_RELIC_SYSTEM 1
#include "GameXXKRelicCatalog.h"
#include "GameXXKRelicRules.h"
#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCardRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKRouteEconomyRules.h"
#include "GameXXKRouteEncounterCatalog.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKRelicBarWidget.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#else
#define GAMEXXK_HAS_RELIC_SYSTEM 0
#endif

#if WITH_DEV_AUTOMATION_TESTS

#if GAMEXXK_HAS_RELIC_SYSTEM
namespace
{
	const FName LifeSavingTalismanRelicId(TEXT("Relic.LifeSavingTalisman"));

	FGameXXKBattleRuntimeUnit MakeLifeSavingLegacyUnit(
		const FName UnitId,
		const bool bEnemy,
		const FName EnemyDefinitionId = NAME_None)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = FText::FromName(UnitId);
		Unit.HP = bEnemy ? 100 : 100;
		Unit.MaxHP = Unit.HP;
		Unit.MP = bEnemy ? 0 : 20;
		Unit.MaxMP = Unit.MP;
		Unit.Attack = bEnemy ? 20 : 20;
		Unit.Defense = 0;
		Unit.Speed = bEnemy ? 8 : 10;
		Unit.bEnemy = bEnemy;
		Unit.EnemyDefinitionId = EnemyDefinitionId;
		Unit.BattleSlotNumber = bEnemy ? 1 : INDEX_NONE;
		Unit.CombatLevel = 1;
		return Unit;
	}

	FGameXXKCardCombatUnit* FindLifeSavingUnit(FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	const FGameXXKCardCombatUnit* FindLifeSavingUnit(const FGameXXKRuntimeState& State, const FName UnitId)
	{
		return State.CardRun.ActiveBattle.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool OwnsLifeSavingTalisman(const FGameXXKRuntimeState& State)
	{
		return State.CardRun.Relics.ContainsByPredicate([](const FGameXXKRelicInstance& Instance)
		{
			return Instance.RelicId == LifeSavingTalismanRelicId;
		});
	}

	bool RoundTripLifeSavingRuntime(
		const FGameXXKCardBattleRuntime& Source,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive WriteArchive(Writer, false);
		WriteArchive.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime SourceCopy = Source;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(WriteArchive, &SourceCopy, nullptr);
		if (Writer.IsError())
		{
			return false;
		}

		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive ReadArchive(Reader, false);
		ReadArchive.ArIsSaveGame = true;
		FGameXXKCardBattleRuntime::StaticStruct()->SerializeItem(ReadArchive, &OutRuntime, nullptr);
		return !Reader.IsError();
	}

	bool BeginLifeSavingBattle(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& OutState,
		const bool bOwnTalisman,
		FString& OutError)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		if (!FGameXXKCardBattleAdapter::EnsureCardRunInitialized(OutState, &OutError))
		{
			Test.AddError(FString::Printf(TEXT("life-saving fixture failed to initialize its card run: %s"), *OutError));
			return false;
		}
		if (bOwnTalisman && !FGameXXKRelicRules::AcquireRelic(OutState, LifeSavingTalismanRelicId, &OutError))
		{
			Test.AddError(FString::Printf(TEXT("life-saving fixture failed to acquire its talisman: %s"), *OutError));
			return false;
		}
		OutState.ActiveBattleParty = {MakeLifeSavingLegacyUnit(TEXT("Player"), false)};
		OutState.ActiveBattleEnemies = {MakeLifeSavingLegacyUnit(
			TEXT("Enemy.Rooster.P1"),
			true,
			TEXT("Enemy.Ch1.Rooster"))};
		OutState.bHasActiveBattle = true;
		OutState.ActiveBattleNodeId = 48001;
		if (!FGameXXKCardBattleAdapter::BeginCardBattle(
			OutState,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			48001,
			&OutError))
		{
			Test.AddError(FString::Printf(TEXT("life-saving fixture failed to begin its card battle: %s"), *OutError));
			return false;
		}
		return true;
	}

	FGameXXKCardEnemyIntent MakeLifeSavingIntent(
		const FName EnemyUnitId,
		const FName PartyUnitId,
		const TArray<int32>& PacketDamages)
	{
		FGameXXKCardEnemyIntent Intent;
		Intent.CardId = TEXT("Test.Intent.LifeSavingTalisman");
		Intent.CardDisplayName = TEXT("Life-saving test intent");
		Intent.SourceUnitId = EnemyUnitId;
		Intent.SuggestedTargetUnitId = PartyUnitId;
		Intent.Damage = PacketDamages.IsEmpty() ? 0 : PacketDamages[0];
		Intent.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		Intent.ResolutionOrder = 0;
		for (const int32 Damage : PacketDamages)
		{
			FGameXXKResolvedEnemyIntentEffect& Effect = Intent.Effects.AddDefaulted_GetRef();
			Effect.Type = EGameXXKEnemyIntentEffectType::DirectDamage;
			Effect.TargetUnitIds = {PartyUnitId};
			Effect.Magnitude = Damage;
			Effect.BaseMagnitude = Damage;
			Effect.HitCount = 1;
			Effect.TargetRule = EGameXXKEnemyIntentTargetRule::LowestHealthParty;
		}
		return Intent;
	}

	bool PrepareLifeSavingEnemyMutation(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const int32 PlayerHealth,
		const TArray<int32>& PacketDamages,
		FString& OutError)
	{
		FGameXXKCardCombatUnit* Player = FindLifeSavingUnit(State, TEXT("Player"));
		const FGameXXKCardCombatUnit* Enemy = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.bLiving;
		});
		if (!Player || !Enemy)
		{
			Test.AddError(TEXT("life-saving enemy fixture lost its player or enemy."));
			return false;
		}
		Player->MaxHP = 100;
		Player->HP = PlayerHealth;
		Player->bLiving = PlayerHealth > 0;
		Player->Defense = 0;
		Player->Armor = 0;
		State.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Enemy;
		State.CardRun.EnemyIntents = {MakeLifeSavingIntent(Enemy->UnitId, Player->UnitId, PacketDamages)};
		State.CardRun.NextEnemyIntentIndex = 0;
		return FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(State, &OutError);
	}

	bool ResolveLifeSavingIntent(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		TArray<FGameXXKCardDamageResult>& OutDamageResults,
		FString& OutError)
	{
		FGameXXKCardEnemyIntent ResolvedIntent;
		bool bFinished = false;
		const bool bResolved = FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			State,
			ResolvedIntent,
			OutDamageResults,
			bFinished,
			&OutError);
		Test.TestTrue(FString::Printf(TEXT("life-saving enemy intent resolves: %s"), *OutError), bResolved);
		return bResolved;
	}
}
#endif

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
		TestEqual(TEXT("the life-saving talisman documents its complete live effect"), LifeSavingTalisman->Description.ToString(), FString(TEXT("战斗中任一角色气血将降至50%以下时，令其至少保留1点气血，消耗此遗物并使全队恢复30%最大气血。")));
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
	FGameXXKLifeSavingTalismanThresholdClampTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.ThresholdClampAndLivingPartyHeal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanThresholdClampTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman threshold behavior has not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState ExactHalfState;
	if (!BeginLifeSavingBattle(*this, ExactHalfState, true, Error)
		|| !PrepareLifeSavingEnemyMutation(*this, ExactHalfState, 51, {1}, Error))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> ExactHalfResults;
	if (!ResolveLifeSavingIntent(*this, ExactHalfState, ExactHalfResults, Error))
	{
		return false;
	}
	TestEqual(TEXT("exactly fifty percent audits one ordinary packet"), ExactHalfResults.Num(), 1);
	if (ExactHalfResults.Num() == 1)
	{
		TestEqual(TEXT("exactly fifty percent keeps the real packet result"), ExactHalfResults[0].TargetHealthAfter, 50);
		TestEqual(TEXT("exactly fifty percent keeps the real health damage"), ExactHalfResults[0].HealthDamage, 1);
	}
	TestEqual(TEXT("exactly fifty percent receives no emergency healing"),
		FindLifeSavingUnit(ExactHalfState, TEXT("Player"))->HP, 50);
	TestTrue(TEXT("exactly fifty percent keeps the talisman armed and owned"), OwnsLifeSavingTalisman(ExactHalfState));
	TestTrue(TEXT("exactly fifty percent keeps the runtime talisman armed"),
		ExactHalfState.CardRun.ActiveBattle.bLifeSavingTalismanArmed);
	TestFalse(TEXT("exactly fifty percent raises no pending consumption"),
		ExactHalfState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);

	FGameXXKRuntimeState BelowHalfState;
	if (!BeginLifeSavingBattle(*this, BelowHalfState, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit CeilAlly;
	CeilAlly.UnitId = TEXT("Ally.Ceil");
	CeilAlly.Side = EGameXXKCardTargetSide::Party;
	CeilAlly.Role = EGameXXKCharacterRole::Guard;
	CeilAlly.HP = 1;
	CeilAlly.MaxHP = 3;
	CeilAlly.Attack = 1;
	CeilAlly.Speed = 1;
	CeilAlly.StableSortOrder = 97;
	CeilAlly.bLiving = true;
	BelowHalfState.CardRun.ActiveBattle.Units.Add(CeilAlly);
	FGameXXKBattleRuntimeUnit CeilLegacy = MakeLifeSavingLegacyUnit(CeilAlly.UnitId, false);
	CeilLegacy.HP = CeilAlly.HP;
	CeilLegacy.MaxHP = CeilAlly.MaxHP;
	BelowHalfState.ActiveBattleParty.Add(CeilLegacy);

	FGameXXKCardCombatUnit CappedAlly = CeilAlly;
	CappedAlly.UnitId = TEXT("Ally.Capped");
	CappedAlly.HP = 9;
	CappedAlly.MaxHP = 10;
	CappedAlly.StableSortOrder = 98;
	BelowHalfState.CardRun.ActiveBattle.Units.Add(CappedAlly);
	FGameXXKBattleRuntimeUnit CappedLegacy = MakeLifeSavingLegacyUnit(CappedAlly.UnitId, false);
	CappedLegacy.HP = CappedAlly.HP;
	CappedLegacy.MaxHP = CappedAlly.MaxHP;
	BelowHalfState.ActiveBattleParty.Add(CappedLegacy);
	if (!PrepareLifeSavingEnemyMutation(*this, BelowHalfState, 51, {2}, Error))
	{
		return false;
	}
	TMap<FName, int32> LivingPartyHealthBefore;
	for (const FGameXXKCardCombatUnit& Unit : BelowHalfState.CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party)
		{
			LivingPartyHealthBefore.Add(Unit.UnitId, Unit.HP);
		}
	}
	TestTrue(TEXT("the living-party heal fixture covers at least three party members"), LivingPartyHealthBefore.Num() >= 3);
	TArray<FGameXXKCardDamageResult> BelowHalfResults;
	if (!ResolveLifeSavingIntent(*this, BelowHalfState, BelowHalfResults, Error))
	{
		return false;
	}
	TestEqual(TEXT("the first below-half packet remains one audited packet"), BelowHalfResults.Num(), 1);
	if (BelowHalfResults.Num() == 1)
	{
		TestEqual(TEXT("the damage audit retains the real pre-heal forty-nine health"), BelowHalfResults[0].TargetHealthAfter, 49);
		TestEqual(TEXT("the nonlethal crossing packet keeps its full two damage"), BelowHalfResults[0].HealthDamage, 2);
	}
	for (const TPair<FName, int32>& Pair : LivingPartyHealthBefore)
	{
		const FGameXXKCardCombatUnit* Unit = FindLifeSavingUnit(BelowHalfState, Pair.Key);
		if (!TestNotNull(FString::Printf(TEXT("the living-party heal retains %s"), *Pair.Key.ToString()), Unit))
		{
			continue;
		}
		const int32 PacketHealth = Pair.Key == TEXT("Player") ? 49 : Pair.Value;
		const int32 RequestedHealing = (Unit->MaxHP * 30 + 99) / 100;
		TestEqual(
			FString::Printf(TEXT("the living-party heal uses ceil thirty percent for %s"), *Pair.Key.ToString()),
			Unit->HP,
			FMath::Min(Unit->MaxHP, PacketHealth + RequestedHealing));
	}
	TestEqual(TEXT("the below-half protected target finishes at seventy-nine health"),
		FindLifeSavingUnit(BelowHalfState, TEXT("Player"))->HP, 79);
	TestEqual(TEXT("ceil healing grants one point for a three-health ally"),
		FindLifeSavingUnit(BelowHalfState, CeilAlly.UnitId)->HP, 2);
	TestEqual(TEXT("party healing caps at maximum health"),
		FindLifeSavingUnit(BelowHalfState, CappedAlly.UnitId)->HP, 10);
	TestFalse(TEXT("the below-half crossing consumes the catalog talisman"), OwnsLifeSavingTalisman(BelowHalfState));
	TestFalse(TEXT("successful adapter finalization clears the pending consumption"),
		BelowHalfState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanLethalClampTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.LethalClampNeverRevivesDefeatedAlly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanLethalClampTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman lethal clamp has not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState State;
	if (!BeginLifeSavingBattle(*this, State, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit DefeatedAlly;
	DefeatedAlly.UnitId = TEXT("Ally.PreDefeated");
	DefeatedAlly.Side = EGameXXKCardTargetSide::Party;
	DefeatedAlly.Role = EGameXXKCharacterRole::Guard;
	DefeatedAlly.HP = 0;
	DefeatedAlly.MaxHP = 100;
	DefeatedAlly.Speed = 1;
	DefeatedAlly.StableSortOrder = 99;
	DefeatedAlly.bLiving = false;
	State.CardRun.ActiveBattle.Units.Add(DefeatedAlly);
	FGameXXKBattleRuntimeUnit DefeatedLegacy = MakeLifeSavingLegacyUnit(DefeatedAlly.UnitId, false);
	DefeatedLegacy.HP = 0;
	DefeatedLegacy.MaxHP = 100;
	DefeatedLegacy.bDefeated = true;
	State.ActiveBattleParty.Add(DefeatedLegacy);
	if (!PrepareLifeSavingEnemyMutation(*this, State, 5, {100}, Error))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveLifeSavingIntent(*this, State, Results, Error))
	{
		return false;
	}
	TestEqual(TEXT("the lethal protected boundary produces one packet"), Results.Num(), 1);
	if (Results.Num() == 1)
	{
		TestEqual(TEXT("the lethal packet audit reports the actual one-health clamp"), Results[0].TargetHealthAfter, 1);
		TestEqual(TEXT("the lethal packet audit reduces effective health damage to four"), Results[0].HealthDamage, 4);
	}
	const FGameXXKCardCombatUnit* ProtectedPlayer = FindLifeSavingUnit(State, TEXT("Player"));
	if (TestNotNull(TEXT("the lethal clamp retains the protected player"), ProtectedPlayer))
	{
		TestEqual(TEXT("the protected player heals from one to thirty-one"), ProtectedPlayer->HP, 31);
		TestTrue(TEXT("the protected player never becomes nonliving"), ProtectedPlayer->bLiving);
	}
	const FGameXXKCardCombatUnit* AllyAfter = FindLifeSavingUnit(State, DefeatedAlly.UnitId);
	if (TestNotNull(TEXT("the pre-defeated ally remains in the battle runtime"), AllyAfter))
	{
		TestEqual(TEXT("the pre-defeated ally receives no talisman healing"), AllyAfter->HP, 0);
		TestFalse(TEXT("the talisman never revives a pre-defeated ally"), AllyAfter->bLiving);
	}
	TestEqual(TEXT("pre-death protection leaves the enemy phase nonterminal"),
		State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestFalse(TEXT("the lethal clamp consumes the catalog talisman"), OwnsLifeSavingTalisman(State));
	TestFalse(TEXT("the consumed lethal clamp remains disarmed"),
		State.CardRun.ActiveBattle.bLifeSavingTalismanArmed);
	TestFalse(TEXT("the lethal clamp publishes no pending state"),
		State.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanMultiHitTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.LaterMultiHitCanKillAfterConsumption",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanMultiHitTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman multi-hit behavior has not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState State;
	if (!BeginLifeSavingBattle(*this, State, true, Error)
		|| !PrepareLifeSavingEnemyMutation(*this, State, 51, {2, 100}, Error))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> Results;
	if (!ResolveLifeSavingIntent(*this, State, Results, Error))
	{
		return false;
	}
	TestEqual(TEXT("the two-packet intent keeps both immutable damage audits"), Results.Num(), 2);
	if (Results.Num() == 2)
	{
		TestEqual(TEXT("the crossing hit records forty-nine before emergency healing"), Results[0].TargetHealthAfter, 49);
		TestEqual(TEXT("the later hit starts after the one-use thirty-percent heal"), Results[1].TargetHealthBefore, 79);
		TestEqual(TEXT("the later hit follows unchanged lethal damage logic"), Results[1].TargetHealthAfter, 0);
		TestEqual(TEXT("the later hit deals the healed target's remaining seventy-nine health"), Results[1].HealthDamage, 79);
	}
	const FGameXXKCardCombatUnit* Player = FindLifeSavingUnit(State, TEXT("Player"));
	if (TestNotNull(TEXT("the multi-hit fixture retains its player record"), Player))
	{
		TestEqual(TEXT("the later hit may kill after talisman consumption"), Player->HP, 0);
		TestFalse(TEXT("the later hit uses ordinary defeated state"), Player->bLiving);
	}
	TestEqual(TEXT("the later lethal hit commits the ordinary defeat phase"),
		State.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Defeat);
	TestFalse(TEXT("the first crossing hit consumes the talisman exactly once"), OwnsLifeSavingTalisman(State));
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanSelfAndDotTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.SelfLossAndPlayerDot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanSelfAndDotTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman party-loss sources have not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState SelfState;
	if (!BeginLifeSavingBattle(*this, SelfState, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit* SelfPlayer = FindLifeSavingUnit(SelfState, TEXT("Player"));
	if (!SelfPlayer || SelfState.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
	{
		AddError(TEXT("the self-loss fixture lost its player or opening hand."));
		return false;
	}
	SelfPlayer->MaxHP = 100;
	SelfPlayer->HP = 50;
	SelfPlayer->bLiving = true;
	FGameXXKCardInstance& SelfCard = SelfState.CardRun.ActiveBattle.Deck.Hand[0];
	SelfCard.CardId = TEXT("Hero.Healer.YiXueCuiFang");
	const FName SelfCardInstanceId = SelfCard.InstanceId;
	FGameXXKCardPlayResult SelfResult;
	if (!TestTrue(FString::Printf(TEXT("the real self-loss card resolves through the adapter: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveCardPlay(SelfState, SelfCardInstanceId, NAME_None, SelfResult, &Error)))
	{
		return false;
	}
	TestTrue(TEXT("the real self-loss card publishes an emergency living-party healing audit"),
		!SelfResult.HealingResults.IsEmpty());
	TestTrue(TEXT("the real self-loss card retains a forty-nine-health packet audit"),
		SelfResult.DamageResults.ContainsByPredicate([](const FGameXXKCardDamageResult& Result)
		{
			return Result.ResolvedTargetUnitId == TEXT("Player")
				&& Result.Cause == EGameXXKCardDamageCause::SelfLoss
				&& Result.TargetHealthAfter == 49;
		}));
	TestEqual(TEXT("self-loss protection heals the player from forty-nine to seventy-nine"),
		FindLifeSavingUnit(SelfState, TEXT("Player"))->HP, 79);
	TestFalse(TEXT("self-loss protection consumes the talisman"), OwnsLifeSavingTalisman(SelfState));

	FGameXXKRuntimeState ReflectionState;
	if (!BeginLifeSavingBattle(*this, ReflectionState, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit* ReflectionPlayer = FindLifeSavingUnit(ReflectionState, TEXT("Player"));
	FGameXXKCardCombatUnit* ReflectionEnemy = ReflectionState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.bLiving;
	});
	if (!ReflectionPlayer || !ReflectionEnemy || ReflectionState.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
	{
		AddError(TEXT("the reflection fixture lost its player, enemy, or opening hand."));
		return false;
	}
	ReflectionPlayer->MaxHP = 100;
	ReflectionPlayer->HP = 50;
	ReflectionPlayer->bLiving = true;
	ReflectionPlayer->Defense = 0;
	ReflectionPlayer->Armor = 0;
	ReflectionEnemy->Attack = 10;
	TestEqual(TEXT("the reflection fixture applies four Bleed to the attacked enemy"),
		GameXXKCardRules::AddCombatStatus(*ReflectionEnemy, EGameXXKCardStatus::Bleed, 4), 4);
	FGameXXKCardBattleModifierRuntime& Reflection = ReflectionState.CardRun.ActiveBattle.Modifiers.AddDefaulted_GetRef();
	Reflection.ModifierId = TEXT("Test.LifeSaving.Reflection");
	Reflection.SourceCardInstanceId = ReflectionState.CardRun.ActiveBattle.Deck.ActiveInstanceIds[0];
	Reflection.SourceUnitId = ReflectionEnemy->UnitId;
	Reflection.RecipientUnitIds = {ReflectionEnemy->UnitId};
	Reflection.Definition.Trigger = EGameXXKCardBattleModifierTrigger::FirstDirectDamageReceivedThisRound;
	Reflection.Definition.EffectType = EGameXXKCardEffectType::DamagePercentAttack;
	Reflection.Definition.Target = EGameXXKCardEffectTarget::Attacker;
	Reflection.Definition.Magnitude = 50;
	Reflection.Definition.RemainingTriggers = 1;
	Reflection.Definition.Expiry = EGameXXKCardModifierExpiry::AfterTriggerCount;
	Reflection.Definition.RecipientScope = EGameXXKCardModifierRecipientScope::CardOwner;
	Reflection.Definition.RecipientTarget = EGameXXKCardEffectTarget::CardOwner;
	Reflection.Definition.bPersistent = true;
	FGameXXKCardInstance& ReflectionCard = ReflectionState.CardRun.ActiveBattle.Deck.Hand[0];
	ReflectionCard.CardId = TEXT("Profession.Blade.YinXueDao");
	const FName ReflectionCardInstanceId = ReflectionCard.InstanceId;
	const FName ReflectionEnemyUnitId = ReflectionEnemy->UnitId;
	TArray<FGameXXKCardCombatUnit> ReflectionPartyBefore;
	for (const FGameXXKCardCombatUnit& Unit : ReflectionState.CardRun.ActiveBattle.Units)
	{
		if (Unit.bLiving && Unit.Side == EGameXXKCardTargetSide::Party)
		{
			ReflectionPartyBefore.Add(Unit);
		}
	}
	FGameXXKCardPlayResult ReflectionResult;
	if (!TestTrue(FString::Printf(TEXT("the enemy reflection resolves through the adapter: %s"), *Error),
		FGameXXKCardBattleAdapter::ResolveCardPlay(
			ReflectionState,
			ReflectionCardInstanceId,
			ReflectionEnemyUnitId,
			ReflectionResult,
			&Error)))
	{
		return false;
	}
	TestTrue(TEXT("the reflected party packet keeps its actual pre-heal audit"),
		ReflectionResult.DamageResults.ContainsByPredicate([](const FGameXXKCardDamageResult& Result)
		{
			return Result.Cause == EGameXXKCardDamageCause::Counter
				&& Result.ResolvedTargetUnitId == TEXT("Player")
				&& Result.HealthDamage == 5
				&& Result.TargetHealthAfter == 49;
		}));
	TestEqual(TEXT("reflection audit contains one ordinary lifesteal plus exactly one talisman heal per living party member"),
		ReflectionResult.HealingResults.Num(), 1 + ReflectionPartyBefore.Num());
	if (!ReflectionResult.HealingResults.IsEmpty())
	{
		const FGameXXKCardHealingResult& Lifesteal = ReflectionResult.HealingResults[0];
		TestEqual(TEXT("ordinary YinXueDao lifesteal keeps the player as source"), Lifesteal.SourceUnitId, FName(TEXT("Player")));
		TestEqual(TEXT("ordinary YinXueDao lifesteal keeps the player as target"), Lifesteal.TargetUnitId, FName(TEXT("Player")));
		TestEqual(TEXT("ordinary YinXueDao lifesteal requests four"), Lifesteal.RequestedHealing, 4);
		TestEqual(TEXT("ordinary YinXueDao lifesteal restores four"), Lifesteal.EffectiveHealing, 4);
	}
	for (int32 PartyIndex = 0; PartyIndex < ReflectionPartyBefore.Num(); ++PartyIndex)
	{
		const int32 AuditIndex = PartyIndex + 1;
		if (!ReflectionResult.HealingResults.IsValidIndex(AuditIndex))
		{
			continue;
		}
		const FGameXXKCardCombatUnit& Before = ReflectionPartyBefore[PartyIndex];
		const FGameXXKCardHealingResult& TalismanHeal = ReflectionResult.HealingResults[AuditIndex];
		const int32 RequestedHealing = (Before.MaxHP * 30 + 99) / 100;
		const int32 HealthBeforeTalisman = Before.UnitId == TEXT("Player") ? 49 : Before.HP;
		TestTrue(FString::Printf(TEXT("talisman healing for %s has no enemy or unit source"), *Before.UnitId.ToString()),
			TalismanHeal.SourceUnitId.IsNone());
		TestEqual(FString::Printf(TEXT("talisman healing retains target order for %s"), *Before.UnitId.ToString()),
			TalismanHeal.TargetUnitId, Before.UnitId);
		TestEqual(FString::Printf(TEXT("talisman healing requests catalog thirty percent for %s"), *Before.UnitId.ToString()),
			TalismanHeal.RequestedHealing, RequestedHealing);
		TestEqual(FString::Printf(TEXT("talisman healing records exact effective healing for %s"), *Before.UnitId.ToString()),
			TalismanHeal.EffectiveHealing, FMath::Min(RequestedHealing, Before.MaxHP - HealthBeforeTalisman));
	}
	TestEqual(TEXT("reflection protection heals the player from forty-nine to seventy-nine"),
		FindLifeSavingUnit(ReflectionState, TEXT("Player"))->HP, 79);
	TestFalse(TEXT("reflection protection consumes the talisman"), OwnsLifeSavingTalisman(ReflectionState));

	FGameXXKRuntimeState DotState;
	if (!BeginLifeSavingBattle(*this, DotState, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit* DotPlayer = FindLifeSavingUnit(DotState, TEXT("Player"));
	if (!DotPlayer)
	{
		return false;
	}
	DotPlayer->MaxHP = 100;
	DotPlayer->HP = 51;
	DotPlayer->bLiving = true;
	TestEqual(TEXT("the DOT fixture applies two Poison"),
		GameXXKCardRules::AddCombatStatus(*DotPlayer, EGameXXKCardStatus::Poison, 2), 2);
	TestTrue(TEXT("the DOT fixture synchronizes its player projection"),
		FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(DotState, &Error));
	TArray<FGameXXKCardDamageResult> DotResults;
	if (!TestTrue(FString::Printf(TEXT("the player-end DOT resolves through the adapter: %s"), *Error),
		FGameXXKCardBattleAdapter::EndPlayerCardPhase(DotState, DotResults, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the player-end DOT retains its forty-nine-health packet audit"), DotResults.Num(), 1);
	if (DotResults.Num() == 1)
	{
		TestEqual(TEXT("the DOT packet audit precedes talisman healing"), DotResults[0].TargetHealthAfter, 49);
	}
	TestEqual(TEXT("player DOT protection heals the player to seventy-nine"),
		FindLifeSavingUnit(DotState, TEXT("Player"))->HP, 79);
	TestEqual(TEXT("player DOT protection continues into the enemy phase"),
		DotState.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestFalse(TEXT("player DOT protection consumes the talisman"), OwnsLifeSavingTalisman(DotState));
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanAutomaticReplayTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.AutomaticReplayFinalizesAtomically",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanAutomaticReplayTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman replay boundary has not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState State;
	if (!BeginLifeSavingBattle(*this, State, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit* Player = FindLifeSavingUnit(State, TEXT("Player"));
	if (!Player)
	{
		return false;
	}
	Player->MaxHP = 100;
	Player->HP = 50;
	Player->bLiving = true;
	FGameXXKResolvedCardSnapshot Replay;
	Replay.CardId = TEXT("Hero.Healer.YiXueCuiFang");
	Replay.Quality = EGameXXKCardQuality::Common;
	Replay.OwnerUnitId = TEXT("Player");
	State.CardRun.ActiveBattle.AutomaticResolutionQueue = FGameXXKAutomaticResolutionQueue();
	State.CardRun.ActiveBattle.AutomaticResolutionQueue.bActive = true;
	State.CardRun.ActiveBattle.AutomaticResolutionQueue.Origin = EGameXXKCardResolutionOrigin::AutomaticReplay;
	State.CardRun.ActiveBattle.AutomaticResolutionQueue.PendingCards = {Replay};
	TArray<FGameXXKCardPlayResult> Results;
	if (!TestTrue(FString::Printf(TEXT("the saved automatic replay resolves through the adapter: %s"), *Error),
		FGameXXKCardBattleAdapter::ResumeAutomaticResolutionQueue(State, Results, &Error)))
	{
		return false;
	}
	TestEqual(TEXT("the replay publishes one resumed card result"), Results.Num(), 1);
	if (Results.Num() == 1)
	{
		TestTrue(TEXT("the resumed replay publishes emergency healing"), !Results[0].HealingResults.IsEmpty());
		TestTrue(TEXT("the resumed replay retains the forty-nine-health packet audit"),
			Results[0].DamageResults.ContainsByPredicate([](const FGameXXKCardDamageResult& Result)
			{
				return Result.Cause == EGameXXKCardDamageCause::SelfLoss
					&& Result.TargetHealthAfter == 49;
			}));
	}
	TestEqual(TEXT("automatic replay protection heals the player to seventy-nine"),
		FindLifeSavingUnit(State, TEXT("Player"))->HP, 79);
	TestFalse(TEXT("automatic replay protection consumes the talisman"), OwnsLifeSavingTalisman(State));
	TestFalse(TEXT("automatic replay finalization clears pending consumption"),
		State.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanRuntimeLifecycleTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.RuntimeLifecycleAndPersistence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanRuntimeLifecycleTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman runtime projection has not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState ArmedState;
	if (!BeginLifeSavingBattle(*this, ArmedState, true, Error))
	{
		return false;
	}
	const FGameXXKCardBattleRuntime& ArmedRuntime = ArmedState.CardRun.ActiveBattle;
	TestTrue(TEXT("battle start arms an owned life-saving talisman"), ArmedRuntime.bLifeSavingTalismanArmed);
	TestFalse(TEXT("battle start has no pending talisman consumption"), ArmedRuntime.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("battle start projects the catalog healing magnitude"), ArmedRuntime.LifeSavingTalismanHealingPercent, 30);
	TestTrue(FString::Printf(TEXT("the armed catalog projection validates: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(ArmedRuntime, &Error));
	FGameXXKCardBattleRuntime InvalidMagnitudeRuntime = ArmedRuntime;
	InvalidMagnitudeRuntime.LifeSavingTalismanHealingPercent = 0;
	TestFalse(TEXT("an armed projection rejects a zero healing percentage"),
		GameXXKCardRules::ValidateCardBattleRuntime(InvalidMagnitudeRuntime));
	InvalidMagnitudeRuntime.LifeSavingTalismanHealingPercent = 101;
	TestFalse(TEXT("an armed projection rejects a healing percentage above one hundred"),
		GameXXKCardRules::ValidateCardBattleRuntime(InvalidMagnitudeRuntime));
	InvalidMagnitudeRuntime = ArmedRuntime;
	InvalidMagnitudeRuntime.bLifeSavingTalismanArmed = false;
	TestFalse(TEXT("an inactive projection rejects a stale catalog magnitude"),
		GameXXKCardRules::ValidateCardBattleRuntime(InvalidMagnitudeRuntime));

	const FGameXXKCardBattleRuntime CopiedRuntime = ArmedRuntime;
	TestTrue(TEXT("copying an active runtime preserves the armed flag"), CopiedRuntime.bLifeSavingTalismanArmed);
	TestFalse(TEXT("copying an active runtime preserves the pending flag"), CopiedRuntime.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("copying an active runtime preserves the catalog magnitude"), CopiedRuntime.LifeSavingTalismanHealingPercent, 30);
	FGameXXKCardBattleRuntime RoundTrippedRuntime;
	TestTrue(TEXT("the armed active runtime round-trips through SaveGame serialization"),
		RoundTripLifeSavingRuntime(CopiedRuntime, RoundTrippedRuntime));
	TestTrue(TEXT("SaveGame round-trip preserves the armed flag"), RoundTrippedRuntime.bLifeSavingTalismanArmed);
	TestFalse(TEXT("SaveGame round-trip preserves the pending flag"), RoundTrippedRuntime.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("SaveGame round-trip preserves the catalog magnitude"), RoundTrippedRuntime.LifeSavingTalismanHealingPercent, 30);

	FGameXXKCardBattleRuntime PendingRuntime = ArmedRuntime;
	PendingRuntime.bLifeSavingTalismanArmed = false;
	PendingRuntime.bLifeSavingTalismanConsumptionPending = true;
	FGameXXKCardBattleRuntime PendingRoundTrip;
	TestTrue(TEXT("a candidate pending projection round-trips through SaveGame serialization"),
		RoundTripLifeSavingRuntime(PendingRuntime, PendingRoundTrip));
	TestFalse(TEXT("pending round-trip remains disarmed"), PendingRoundTrip.bLifeSavingTalismanArmed);
	TestTrue(TEXT("pending round-trip preserves consumption intent"), PendingRoundTrip.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("pending round-trip preserves the catalog magnitude"), PendingRoundTrip.LifeSavingTalismanHealingPercent, 30);
	TestTrue(FString::Printf(TEXT("the pending rules-layer candidate remains structurally valid: %s"), *Error),
		GameXXKCardRules::ValidateCardBattleRuntime(PendingRoundTrip, &Error));

	FGameXXKRuntimeState ClearedState = ArmedState;
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(ClearedState);
	TestFalse(TEXT("clearing a battle removes the active card-battle marker"), ClearedState.CardRun.bHasActiveCardBattle);
	TestFalse(TEXT("clearing a battle resets the armed flag"), ClearedState.CardRun.ActiveBattle.bLifeSavingTalismanArmed);
	TestFalse(TEXT("clearing a battle resets the pending flag"), ClearedState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("clearing a battle resets the projected magnitude"), ClearedState.CardRun.ActiveBattle.LifeSavingTalismanHealingPercent, 0);

	FGameXXKRuntimeState ConsumedState;
	if (!BeginLifeSavingBattle(*this, ConsumedState, true, Error)
		|| !PrepareLifeSavingEnemyMutation(*this, ConsumedState, 51, {2}, Error))
	{
		return false;
	}
	TArray<FGameXXKCardDamageResult> DamageResults;
	if (!ResolveLifeSavingIntent(*this, ConsumedState, DamageResults, Error))
	{
		return false;
	}
	TestFalse(TEXT("successful consumption removes the catalog relic"), OwnsLifeSavingTalisman(ConsumedState));
	TestFalse(TEXT("successful consumption leaves the runtime disarmed"), ConsumedState.CardRun.ActiveBattle.bLifeSavingTalismanArmed);
	TestFalse(TEXT("successful consumption clears pending intent"), ConsumedState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("successful consumption clears the projected magnitude"), ConsumedState.CardRun.ActiveBattle.LifeSavingTalismanHealingPercent, 0);
	FGameXXKCardBattleAdapter::ClearActiveCardBattle(ConsumedState);
	if (!TestTrue(FString::Printf(TEXT("a later battle starts after talisman consumption: %s"), *Error),
		FGameXXKCardBattleAdapter::BeginCardBattle(
			ConsumedState,
			EGameXXKNodeKind::Battle,
			EGameXXKCardTerrain::Plain,
			48002,
			&Error)))
	{
		return false;
	}
	TestFalse(TEXT("a later battle does not rearm a consumed talisman"),
		ConsumedState.CardRun.ActiveBattle.bLifeSavingTalismanArmed);
	TestFalse(TEXT("a later battle starts with no pending consumption"),
		ConsumedState.CardRun.ActiveBattle.bLifeSavingTalismanConsumptionPending);
	TestEqual(TEXT("a later battle has no stale talisman magnitude"),
		ConsumedState.CardRun.ActiveBattle.LifeSavingTalismanHealingPercent, 0);
	return true;
#endif
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKLifeSavingTalismanControlsAndRollbackTest,
	"GameXXK.Integration.CardBattleAdapter.LifeSavingTalisman.ControlsRollbackAndNoCharmCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKLifeSavingTalismanControlsAndRollbackTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_RELIC_SYSTEM
	AddError(TEXT("The required life-saving talisman controls have not been implemented."));
	return false;
#else
	FString Error;
	FGameXXKRuntimeState EnemyOnlyState;
	if (!BeginLifeSavingBattle(*this, EnemyOnlyState, true, Error))
	{
		return false;
	}
	FGameXXKCardCombatUnit* LowPlayer = FindLifeSavingUnit(EnemyOnlyState, TEXT("Player"));
	FGameXXKCardCombatUnit* DotEnemy = EnemyOnlyState.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.Side == EGameXXKCardTargetSide::Enemy && Unit.bLiving;
	});
	if (!LowPlayer || !DotEnemy)
	{
		return false;
	}
	LowPlayer->MaxHP = 100;
	LowPlayer->HP = 49;
	LowPlayer->bLiving = true;
	TestEqual(TEXT("the enemy-only fixture applies two Poison to the enemy"),
		GameXXKCardRules::AddCombatStatus(*DotEnemy, EGameXXKCardStatus::Poison, 2), 2);
	EnemyOnlyState.CardRun.ActiveBattle.Phase = EGameXXKCardBattlePhase::Enemy;
	EnemyOnlyState.CardRun.EnemyIntents.Reset();
	EnemyOnlyState.CardRun.NextEnemyIntentIndex = 0;
	TArray<FGameXXKCardDamageResult> EnemyDotResults;
	TestTrue(FString::Printf(TEXT("the enemy-only DOT boundary resolves: %s"), *Error),
		FGameXXKCardBattleAdapter::CompleteEnemyCardPhase(EnemyOnlyState, EnemyDotResults, &Error));
	TestEqual(TEXT("an enemy-only packet never heals the already-low party"),
		FindLifeSavingUnit(EnemyOnlyState, TEXT("Player"))->HP, 49);
	TestTrue(TEXT("an enemy-only packet keeps the talisman owned"), OwnsLifeSavingTalisman(EnemyOnlyState));

	FGameXXKRuntimeState OutsideBattleState = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("the outside-battle fixture acquires the talisman"),
		FGameXXKRelicRules::AcquireRelic(OutsideBattleState, LifeSavingTalismanRelicId));
	FGameXXKCardDamageResult OutsidePacket;
	OutsidePacket.ResolvedTargetUnitId = TEXT("Player");
	OutsidePacket.HealthDamage = 1;
	OutsidePacket.TargetHealthBefore = 50;
	OutsidePacket.TargetHealthAfter = 49;
	FGameXXKRelicRules::ApplyDamageTaken(OutsideBattleState, {OutsidePacket});
	TestTrue(TEXT("no active card battle means no talisman consumption"), OwnsLifeSavingTalisman(OutsideBattleState));

	FGameXXKRuntimeState RollbackState;
	if (!BeginLifeSavingBattle(*this, RollbackState, true, Error)
		|| !PrepareLifeSavingEnemyMutation(*this, RollbackState, 51, {2}, Error))
	{
		return false;
	}
	RollbackState.ActiveBattleParty[0].Id = TEXT("Missing.Legacy.Player");
	FGameXXKCardEnemyIntent ResolvedIntent;
	TArray<FGameXXKCardDamageResult> RollbackResults;
	bool bFinished = false;
	Error.Reset();
	TestFalse(TEXT("a failed legacy projection rejects the protected packet atomically"),
		FGameXXKCardBattleAdapter::ResolveNextEnemyIntent(
			RollbackState,
			ResolvedIntent,
			RollbackResults,
			bFinished,
			&Error));
	TestEqual(TEXT("rollback preserves pre-packet player health"),
		FindLifeSavingUnit(RollbackState, TEXT("Player"))->HP, 51);
	TestEqual(TEXT("rollback preserves the pre-packet enemy phase"),
		RollbackState.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Enemy);
	TestTrue(TEXT("rollback preserves catalog talisman ownership"), OwnsLifeSavingTalisman(RollbackState));

	FGameXXKRuntimeState NoCharmState;
	if (!BeginLifeSavingBattle(*this, NoCharmState, false, Error)
		|| !PrepareLifeSavingEnemyMutation(*this, NoCharmState, 51, {2, 100}, Error))
	{
		return false;
	}
	const TArray<FName> DeckLedgerBefore = NoCharmState.CardRun.ActiveBattle.Deck.ActiveInstanceIds;
	TArray<FName> HandBefore;
	for (const FGameXXKCardInstance& Card : NoCharmState.CardRun.ActiveBattle.Deck.Hand)
	{
		HandBefore.Add(Card.InstanceId);
	}
	TArray<FGameXXKCardDamageResult> NoCharmResults;
	if (!ResolveLifeSavingIntent(*this, NoCharmState, NoCharmResults, Error))
	{
		return false;
	}
	TestEqual(TEXT("no-charm multi-hit keeps both original packets"), NoCharmResults.Num(), 2);
	if (NoCharmResults.Num() == 2)
	{
		TestEqual(TEXT("no-charm first hit keeps forty-nine health"), NoCharmResults[0].TargetHealthAfter, 49);
		TestEqual(TEXT("no-charm later hit starts from unhealed forty-nine"), NoCharmResults[1].TargetHealthBefore, 49);
		TestEqual(TEXT("no-charm later hit deals the original forty-nine lethal damage"), NoCharmResults[1].HealthDamage, 49);
		TestEqual(TEXT("no-charm later hit reaches zero"), NoCharmResults[1].TargetHealthAfter, 0);
	}
	const FGameXXKCardCombatUnit* NoCharmPlayer = FindLifeSavingUnit(NoCharmState, TEXT("Player"));
	TestTrue(TEXT("no-charm player reaches ordinary zero-health defeat"), NoCharmPlayer && NoCharmPlayer->HP == 0 && !NoCharmPlayer->bLiving);
	TestEqual(TEXT("no-charm terminal phase remains ordinary defeat"),
		NoCharmState.CardRun.ActiveBattle.Phase, EGameXXKCardBattlePhase::Defeat);
	TestEqual(TEXT("no-charm lethal intent preserves the existing deck ledger"),
		NoCharmState.CardRun.ActiveBattle.Deck.ActiveInstanceIds, DeckLedgerBefore);
	TArray<FName> HandAfter;
	for (const FGameXXKCardInstance& Card : NoCharmState.CardRun.ActiveBattle.Deck.Hand)
	{
		HandAfter.Add(Card.InstanceId);
	}
	TestEqual(TEXT("no-charm lethal intent preserves the existing hand ordering"),
		HandAfter, HandBefore);
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
