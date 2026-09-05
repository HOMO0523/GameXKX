#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Components/SceneComponent.h"
#include "Components/TextRenderComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleStatusIconStyle.h"
#include "UI/GameXXKBattleStatusIconWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState BuildSceneBattleState()
	{
		FGameXXKRuntimeState State =
			GameXXKPermanentPartyTestFixtures::MakeStartedState();
		State.Screen = EGameXXKScreen::DungeonMap;
		State.CurrentMapId = TEXT("HuangshanRoute");
		State.QuestState = EGameXXKQuestState::Accepted;
		State.bFollowerJoined = true;
		State.bDungeonActive = true;
		State.bHasGeneratedRouteMap = true;
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{0, 0, 0, EGameXXKNodeKind::Start, FVector2D(0.5f, 0.0f), TArray<int32>{1}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{1, 1, 0, EGameXXKNodeKind::Battle, FVector2D(0.5f, 0.5f), TArray<int32>{2}});
		State.RouteMapNodes.Add(FGameXXKRouteMapNode{2, 2, 0, EGameXXKNodeKind::Boss, FVector2D(0.5f, 1.0f), TArray<int32>{}});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{0, 1});
		State.RouteMapEdges.Add(FGameXXKRouteMapEdge{1, 2});
		State.VisitedRouteNodeIds.Add(0);
		State.ReachableRouteNodeIds.Add(1);
		UGameXXKMVPRules::SelectRouteNodeById(State, 1);
		return State;
	}

	FGameXXKBattleRuntimeUnit MakeSceneUnit(const FName UnitId, const TCHAR* DisplayName, const bool bEnemy)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = FText::FromString(DisplayName);
		Unit.HP = 30;
		Unit.MaxHP = 40;
		Unit.bEnemy = bEnemy;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeCardSceneUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 30;
		Unit.MaxHP = 40;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKRuntimeState BuildFixedSlotSceneBattleState()
	{
		FGameXXKRuntimeState State = BuildSceneBattleState();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleParty = {
			MakeSceneUnit(TEXT("Player"), TEXT("Hero"), false),
			MakeSceneUnit(TEXT("CompanionInstance.Companion_Blade_Test"), TEXT("Blade"), false),
			MakeSceneUnit(TEXT("Npc.YueBai"), TEXT("Yue Bai"), false)};
		State.ActiveBattleEnemies = {
			MakeSceneUnit(TEXT("Enemy.Outer"), TEXT("Outer"), true),
			MakeSceneUnit(TEXT("Enemy.Middle"), TEXT("Middle"), true),
			MakeSceneUnit(TEXT("Enemy.Inner"), TEXT("Inner"), true)};
		State.CardRun.bHasActiveCardBattle = true;
		State.CardRun.ActiveBattle.Units = {
			MakeCardSceneUnit(TEXT("Player"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 0),
			MakeCardSceneUnit(TEXT("CompanionInstance.Companion_Blade_Test"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Blade, 1),
			MakeCardSceneUnit(TEXT("Npc.YueBai"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 2),
			MakeCardSceneUnit(TEXT("Enemy.Outer"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 0),
			MakeCardSceneUnit(TEXT("Enemy.Middle"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 1),
			MakeCardSceneUnit(TEXT("Enemy.Inner"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 2)};

		// Legacy projections deliberately disagree with the card runtime below.
		// The actor's visible HUD must always prefer the active card battle values.
		State.ActiveBattleParty[0].HP = 11;
		State.ActiveBattleParty[0].MaxHP = 17;
		State.ActiveBattleParty[0].MP = 2;
		State.ActiveBattleParty[0].MaxMP = 5;
		State.ActiveBattleParty[0].Shield = 1;
		State.ActiveBattleParty[1].HP = 13;
		State.ActiveBattleParty[1].MaxHP = 21;
		State.ActiveBattleParty[1].MP = 3;
		State.ActiveBattleParty[1].MaxMP = 6;
		State.ActiveBattleParty[2].HP = 14;
		State.ActiveBattleParty[2].MaxHP = 22;
		State.ActiveBattleParty[2].MP = 4;
		State.ActiveBattleParty[2].MaxMP = 7;
		State.ActiveBattleEnemies[0].HP = 18;
		State.ActiveBattleEnemies[0].MaxHP = 26;
		State.ActiveBattleEnemies[0].MP = 5;
		State.ActiveBattleEnemies[0].MaxMP = 8;

		FGameXXKCardCombatUnit& Hero = State.CardRun.ActiveBattle.Units[0];
		Hero.HP = 72;
		Hero.MaxHP = 100;
		Hero.Mana = 18;
		Hero.MaxMana = 30;
		Hero.Armor = 7;
		Hero.Statuses = {FGameXXKCardStatusStack{EGameXXKCardStatus::Poison, 2}};

	FGameXXKCardCombatUnit& Companion = State.CardRun.ActiveBattle.Units[1];
	Companion.HP = 55;
	Companion.MaxHP = 80;
	Companion.Mana = 0;
	Companion.MaxMana = 0;

	FGameXXKCardCombatUnit& QuestNpc = State.CardRun.ActiveBattle.Units[2];
	QuestNpc.HP = 31;
	QuestNpc.MaxHP = 60;
	QuestNpc.Mana = 0;
	QuestNpc.MaxMana = 0;

		FGameXXKCardCombatUnit& Enemy = State.CardRun.ActiveBattle.Units[3];
		Enemy.HP = 240;
		Enemy.MaxHP = 240;
		Enemy.Mana = 99;
		Enemy.MaxMana = 100;
		return State;
	}

	FGameXXKBattleRuntimeUnit MakeRuntimeEnemy()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("MoneyRat");
		Unit.DisplayName = FText::FromString(TEXT("钱鼠"));
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildRuntimeBattleState(UGameXXKMVPSubsystem& Subsystem, FString& OutError)
	{
		FGameXXKRuntimeState& State = Subsystem.GetMutableRuntimeState();
		State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
		State.Screen = EGameXXKScreen::Battle;
		State.bHasActiveBattle = true;
		State.ActiveBattleNodeId = 17;
		State.ActiveBattleEnemies = {MakeRuntimeEnemy()};
		return FGameXXKCardBattleAdapter::EnsureCardRunInitialized(State, &OutError)
			&& FGameXXKCardBattleAdapter::BeginCardBattle(
				State,
				EGameXXKNodeKind::Battle,
				EGameXXKCardTerrain::Plain,
				2,
				&OutError);
	}

	template <typename TActorType>
	int32 CountActors(UWorld* World)
	{
		int32 Count = 0;
		if (World)
		{
			for (TActorIterator<TActorType> It(World); It; ++It)
			{
				++Count;
			}
		}
		return Count;
	}

	bool MatchesCanonicalOrLegacyIdle(
		const UPaperFlipbook* Flipbook,
		const FName UnitId,
		const TCHAR* LegacyAssetFragment)
	{
		if (!Flipbook)
		{
			return false;
		}

		const FString Path = Flipbook->GetPathName();
		return Path == FGameXXKBattleAnimationPresentation::ResolveIdleFlipbookPath(UnitId, true).ToString()
			|| Path.Contains(LegacyAssetFragment);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleSceneActorTest,
	"GameXXK.MVP.Battle.SceneActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleSceneActorTest::RunTest(const FString& Parameters)
{
	// The normal controller path leaves the legacy scene presentation dormant.
	// The original assertions below remain isolated compatibility coverage only.
	{
		const FName TestWorldName = MakeUniqueObjectName(
			nullptr,
			UWorld::StaticClass(),
			TEXT("GameXXKFullscreenHudBattleWorld"),
			EUniqueObjectNameOptions::GloballyUnique);
		FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
		UWorld* const TestWorld = UWorld::CreateWorld(EWorldType::Game, false, TestWorldName, GetTransientPackage());
		TestNotNull(TEXT("scene-retirement test creates an isolated game world"), TestWorld);
		if (!TestWorld)
		{
			return false;
		}
		TestWorld->AddToRoot();
		WorldContext.SetCurrentWorld(TestWorld);
		TestWorld->InitializeActorsForPlay(FURL());

		UGameInstance* const TestGameInstance = NewObject<UGameInstance>();
		UGameXXKMVPSubsystem* const Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
		FString FixtureError;
		TestTrue(FString::Printf(TEXT("scene-retirement fixture starts a real card battle: %s"), *FixtureError),
			BuildRuntimeBattleState(*Subsystem, FixtureError));
		if (!Subsystem->GetRuntimeState().CardRun.bHasActiveCardBattle)
		{
			TestWorld->DestroyWorld(false);
			GEngine->DestroyWorldContext(TestWorld);
			TestWorld->RemoveFromRoot();
			return false;
		}
		Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::DungeonMap;

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGameXXKMVPPlayerController* const Controller = TestWorld->SpawnActor<AGameXXKMVPPlayerController>(
			AGameXXKMVPPlayerController::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		ACameraActor* const ExistingCamera = TestWorld->SpawnActor<ACameraActor>(
			ACameraActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		TestNotNull(TEXT("scene-retirement test spawns the real MVP controller"), Controller);
		TestNotNull(TEXT("scene-retirement test supplies one existing view camera"), ExistingCamera);
		if (!Controller || !ExistingCamera || !ExistingCamera->GetCameraComponent())
		{
			TestWorld->DestroyWorld(false);
			GEngine->DestroyWorldContext(TestWorld);
			TestWorld->RemoveFromRoot();
			return false;
		}

		ExistingCamera->SetActorLocation(FVector(401.0f, -222.0f, 733.0f));
		ExistingCamera->SetActorRotation(FRotator(-19.0f, 71.0f, 2.0f));
		ExistingCamera->GetCameraComponent()->FieldOfView = 53.0f;
		ExistingCamera->GetCameraComponent()->AspectRatio = 1.42f;
		ExistingCamera->GetCameraComponent()->bConstrainAspectRatio = false;
		Controller->SetViewTarget(ExistingCamera);
		Controller->SetMVPSubsystemForTest(Subsystem);
		TestTrue(TEXT("scene-retirement controller creates its HUD widgets"), Controller->EnsurePlayerFlowWidgetsForTest());

		TestEqual(TEXT("fixture starts with no battle presenter"), CountActors<AGameXXKBattleScenePresenter>(TestWorld), 0);
		TestEqual(TEXT("fixture starts with no battle scene unit actor"), CountActors<AGameXXKBattleSceneUnitActor>(TestWorld), 0);
		const int32 CameraActorCountBeforeBattle = CountActors<ACameraActor>(TestWorld);
		AActor* const ViewTargetBeforeBattle = Controller->GetViewTarget();
		const FVector CameraLocationBeforeBattle = ExistingCamera->GetActorLocation();
		const FRotator CameraRotationBeforeBattle = ExistingCamera->GetActorRotation();
		const float CameraFovBeforeBattle = ExistingCamera->GetCameraComponent()->FieldOfView;
		const float CameraAspectBeforeBattle = ExistingCamera->GetCameraComponent()->AspectRatio;

		Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Battle;
		Controller->RefreshPlayerFlowWidgetsForTest();
		TestTrue(TEXT("runtime battle uses the fullscreen HUD overlay"), Controller->IsBattleOverlayActive());
		TestEqual(TEXT("player flow spawns no battle presenter"), CountActors<AGameXXKBattleScenePresenter>(TestWorld), 0);
		TestEqual(TEXT("player flow spawns no battle scene unit actor"), CountActors<AGameXXKBattleSceneUnitActor>(TestWorld), 0);
		TestEqual(TEXT("player flow spawns no battle camera"), CountActors<ACameraActor>(TestWorld), CameraActorCountBeforeBattle);
		TestTrue(TEXT("player flow never replaces the active view target"), Controller->GetViewTarget() == ViewTargetBeforeBattle);
		TestEqual(TEXT("player flow never moves the active camera"), ExistingCamera->GetActorLocation(), CameraLocationBeforeBattle);
		TestTrue(TEXT("player flow never rotates the active camera"), ExistingCamera->GetActorRotation().Equals(CameraRotationBeforeBattle, 0.001f));
		TestTrue(TEXT("player flow never changes active camera FOV"),
			FMath::IsNearlyEqual(ExistingCamera->GetCameraComponent()->FieldOfView, CameraFovBeforeBattle, 0.001f));
		TestTrue(TEXT("player flow never changes active camera aspect"),
			FMath::IsNearlyEqual(ExistingCamera->GetCameraComponent()->AspectRatio, CameraAspectBeforeBattle, 0.001f));

		UGameXXKBattleBoardWidget* const RuntimeBoard = Controller->GetBattleBoardWidgetForTest();
		TestNotNull(TEXT("runtime battle keeps presentation in the board-owned HUD stage"), RuntimeBoard);
		if (RuntimeBoard)
		{
			TestNull(TEXT("runtime board does not construct the retired fullscreen animation layer"),
				RuntimeBoard->GetWidgetFromName(TEXT("BattleAnimationLayer")));
			TestEqual(TEXT("runtime board owns no duplicate participant images"),
				RuntimeBoard->GetDuplicateParticipantImageCountForTest(),
				0);
			TestEqual(TEXT("runtime board presentation performs no synchronous animation texture loads"),
				RuntimeBoard->GetAtlasCacheStatsForTest().SyncLoadCount,
				0);
		}
		TestEqual(TEXT("board-owned HUD presentation never creates a presenter"), CountActors<AGameXXKBattleScenePresenter>(TestWorld), 0);
		TestEqual(TEXT("board-owned HUD presentation never creates scene units"), CountActors<AGameXXKBattleSceneUnitActor>(TestWorld), 0);
		TestEqual(TEXT("board-owned HUD presentation never creates a camera"), CountActors<ACameraActor>(TestWorld), CameraActorCountBeforeBattle);

		Controller->EndPlay(EEndPlayReason::Destroyed);
		TestWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(TestWorld);
		TestWorld->RemoveFromRoot();
	}

	// Status icon language is intentionally a pure projection so all runtime
	// status values remain visible even before the generated texture assets are
	// imported into the project.
	const FGameXXKBattleStatusIconStyle ArmorStyle = FGameXXKBattleStatusIconStyle::ResolveArmorIconStyle();
	TestEqual(TEXT("armor uses the explicit shield status icon"), ArmorStyle.IconId, FName(TEXT("ArmorShield")));
	TestFalse(TEXT("armor icon has a future imported texture path"), ArmorStyle.TexturePath.IsNull());
	TestEqual(
		TEXT("armor tooltip uses the one approved concise rule"),
		ArmorStyle.Tooltip,
		FString(TEXT("优先抵挡直接攻击；所属阵营回合开始时通常清空。")));
	TestTrue(TEXT("armor keeps a native paper-ink fallback when its texture is absent"), ArmorStyle.bUsesPaperInkFallback);

	const TArray<EGameXXKCardStatus> RequiredStatusIcons = {
		EGameXXKCardStatus::Momentum,
		EGameXXKCardStatus::Agility,
		EGameXXKCardStatus::Vulnerability,
		EGameXXKCardStatus::Bleed,
		EGameXXKCardStatus::Poison,
		EGameXXKCardStatus::Burn,
		EGameXXKCardStatus::Mark,
		EGameXXKCardStatus::Guard,
		EGameXXKCardStatus::DamageOverTime,
		EGameXXKCardStatus::CannotReceiveVulnerability,
		EGameXXKCardStatus::NextAttackBonus,
		EGameXXKCardStatus::NextAttackAppliesVulnerability,
		EGameXXKCardStatus::NextHealingBonus,
		EGameXXKCardStatus::TerrainBonusDouble,
		EGameXXKCardStatus::NextTerrainCardFree,
		EGameXXKCardStatus::NextTerrainCardEnergyReduction,
		EGameXXKCardStatus::RedirectSingleTargetEnemyAttack,
		EGameXXKCardStatus::TerrainBonusDoubleThisRound,
		EGameXXKCardStatus::Medicine,
		EGameXXKCardStatus::Weak,
		EGameXXKCardStatus::Wealth,
		EGameXXKCardStatus::Rage,
		EGameXXKCardStatus::Prey,
		EGameXXKCardStatus::Charge,
		EGameXXKCardStatus::Counter,
		EGameXXKCardStatus::Block};
	for (const EGameXXKCardStatus Status : RequiredStatusIcons)
	{
		const FGameXXKBattleStatusIconStyle Style = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(Status);
		TestFalse(*FString::Printf(TEXT("status value %d resolves a visible icon id"), static_cast<int32>(Status)), Style.IconId.IsNone());
		TestFalse(*FString::Printf(TEXT("status value %d has a real mechanics tooltip"), static_cast<int32>(Status)), Style.Tooltip.IsEmpty());
		TestTrue(*FString::Printf(TEXT("status value %d has deterministic status priority"), static_cast<int32>(Status)), Style.Priority > 0);
		TestTrue(*FString::Printf(TEXT("status value %d has a paper-ink fallback"), static_cast<int32>(Status)), Style.bUsesPaperInkFallback);
		TestFalse(*FString::Printf(TEXT("status value %d fallback is never brushless"), static_cast<int32>(Status)), Style.FallbackGlyph.IsEmpty());
		TestNotEqual(*FString::Printf(TEXT("status value %d has an explicit style instead of the unknown fallback"), static_cast<int32>(Status)), Style.IconId, FName(TEXT("UnknownStatus")));
	}
	const FGameXXKBattleStatusIconStyle WeakStyle = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(EGameXXKCardStatus::Weak);
	TestEqual(TEXT("serialized status 21 is localized as weak"), WeakStyle.DisplayName, FString(TEXT("虚弱")));
	const FGameXXKBattleStatusIconStyle FallbackStyle = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(static_cast<EGameXXKCardStatus>(255));
	TestEqual(TEXT("unknown valid future statuses remain visibly represented"), FallbackStyle.IconId, FName(TEXT("UnknownStatus")));
	TestTrue(TEXT("unknown status tooltip retains its enum name"), FallbackStyle.Tooltip.Contains(TEXT("255")));
	TestTrue(TEXT("unknown status has a paper-ink fallback treatment"), FallbackStyle.bUsesPaperInkFallback);
	TestEqual(TEXT("unknown status fallback is a visible centered question glyph"), FallbackStyle.FallbackGlyph, FString(TEXT("?")));
	TestFalse(TEXT("armor fallback glyph is not a brushless empty icon"), ArmorStyle.FallbackGlyph.IsEmpty());
	const FGameXXKBattleStatusIconStyle AgilityStyle = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(EGameXXKCardStatus::Agility);
	TestEqual(TEXT("agility uses the approved player-facing name"), AgilityStyle.DisplayName, FString(TEXT("灵动")));
	TestEqual(
		TEXT("agility uses the approved perfect-dodge and fallback-dodge rule"),
		AgilityStyle.Tooltip,
		FString(TEXT("25%概率消耗1层完美闪避；失败时可消耗2层闪避。")));
	const FGameXXKBattleStatusIconStyle BlockStyle = FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(EGameXXKCardStatus::Block);
	TestEqual(TEXT("block has its own explicit shield icon"), BlockStyle.IconId, FName(TEXT("BlockShield")));
	TestEqual(TEXT("block is not mislabeled as counter"), BlockStyle.DisplayName, FString(TEXT("格挡")));
	TestEqual(TEXT("badge stacks above ninety-nine display a capped seal"), UGameXXKBattleStatusIconWidget::FormatStackForTest(128), FString(TEXT("99+")));
	TestEqual(TEXT("status tooltip factory remains hover-only"), UGameXXKBattleStatusIconWidget::GetTooltipVisibilityForTest(), ESlateVisibility::HitTestInvisible);

	const TArray<FGameXXKBattleStatusBadgeModel> InitialBadgeModels = UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(
		7,
		{FGameXXKCardStatusStack{EGameXXKCardStatus::Poison, 2}, FGameXXKCardStatusStack{EGameXXKCardStatus::Momentum, 1}});
	TestEqual(TEXT("armor plus each nonzero status creates a separate numeric badge"), InitialBadgeModels.Num(), 3);
	if (InitialBadgeModels.Num() == 3)
	{
		TestEqual(TEXT("armor badge shows its current armor stack"), InitialBadgeModels[0].Stacks, 7);
		TestEqual(TEXT("armor badge comes before lower priority combat statuses"), InitialBadgeModels[0].Style.IconId, FName(TEXT("ArmorShield")));
		TestTrue(TEXT("badge tooltip contains the real current stack"), InitialBadgeModels[1].Tooltip.Contains(TEXT("2")) || InitialBadgeModels[2].Tooltip.Contains(TEXT("2")));
		TestTrue(TEXT("every rendered badge model owns hover tooltip data"), InitialBadgeModels[0].Tooltip.Len() > 0 && InitialBadgeModels[1].Tooltip.Len() > 0 && InitialBadgeModels[2].Tooltip.Len() > 0);
	}
	const TArray<FGameXXKBattleStatusBadgeModel> ResetBadgeModels = UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(0, {});
	TestEqual(TEXT("state reset discards old badge models and their hover tooltip data"), ResetBadgeModels.Num(), 0);

	// The scene owns fixed presentation slots, not the mutable legacy-party array order.
	const FGameXXKRuntimeState FixedSlotState = BuildFixedSlotSceneBattleState();
	const TArray<FGameXXKBattleSceneUnitPlacement> FixedPlacements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(FixedSlotState);
	const FGameXXKBattleSceneUnitPlacement* HeroPlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("Player");
	});
	const FGameXXKBattleSceneUnitPlacement* BladePlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("CompanionInstance.Companion_Blade_Test");
	});
	const FGameXXKBattleSceneUnitPlacement* QuestNpcPlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("Npc.YueBai");
	});
	TestNotNull(TEXT("fixed formation includes Hero at a role-derived placement"), HeroPlacement);
	TestNotNull(TEXT("fixed formation includes Blade at a role-derived placement"), BladePlacement);
	TestNotNull(TEXT("fixed formation includes the task NPC at a role-derived placement"), QuestNpcPlacement);
	if (HeroPlacement && BladePlacement && QuestNpcPlacement)
	{
		TestEqual(TEXT("Hero remains the central 我 2P"), HeroPlacement->SlotNumber, 2);
		TestEqual(TEXT("permanent Blade remains 我 1P"), BladePlacement->SlotNumber, 1);
		TestEqual(TEXT("selected NPC remains 我 3P"), QuestNpcPlacement->SlotNumber, 3);
		TestEqual(TEXT("party 2P uses the approved outward middle coordinate"), HeroPlacement->Location, FVector(-20.0f, 225.0f, 90.0f));
		TestEqual(TEXT("party 1P uses the approved outward outer coordinate"), BladePlacement->Location, FVector(-80.0f, 295.0f, 90.0f));
		TestEqual(TEXT("party 3P uses the approved outward inner coordinate"), QuestNpcPlacement->Location, FVector(40.0f, 155.0f, 90.0f));
		TestTrue(TEXT("party slots open toward the central hand space"), BladePlacement->Location.Y > HeroPlacement->Location.Y && HeroPlacement->Location.Y > QuestNpcPlacement->Location.Y);
	}
	const FVector TownBattleAnchor(20400.0f, 4580.0f, 1490.6024691f);
	const TArray<FGameXXKBattleSceneUnitPlacement> AnchoredPlacements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForStateAtAnchor(
		FixedSlotState,
		TownBattleAnchor);
	const FGameXXKBattleSceneUnitPlacement* AnchoredHeroPlacement = AnchoredPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("town-grounded battle formation includes the hero"), AnchoredHeroPlacement);
	if (AnchoredHeroPlacement)
	{
		TestEqual(
			TEXT("town-grounded hero keeps its fixed local central P2 lane above the retained Landscape"),
			AnchoredHeroPlacement->Location,
			TownBattleAnchor + FVector(-20.0f, 225.0f, 90.0f));
	}
	const FGameXXKBattleSceneUnitPlacement* EnemyOuterPlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return Placement.bEnemy && Placement.UnitId == TEXT("Enemy.Outer");
	});
	const FGameXXKBattleSceneUnitPlacement* EnemyMiddlePlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return Placement.bEnemy && Placement.UnitId == TEXT("Enemy.Middle");
	});
	const FGameXXKBattleSceneUnitPlacement* EnemyInnerPlacement = FixedPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return Placement.bEnemy && Placement.UnitId == TEXT("Enemy.Inner");
	});
	TestNotNull(TEXT("fixed formation includes enemy 1P"), EnemyOuterPlacement);
	TestNotNull(TEXT("fixed formation includes enemy 2P"), EnemyMiddlePlacement);
	TestNotNull(TEXT("fixed formation includes enemy 3P"), EnemyInnerPlacement);
	if (EnemyOuterPlacement && EnemyMiddlePlacement && EnemyInnerPlacement)
	{
		TestEqual(TEXT("enemy outer unit remains 敌 1P"), EnemyOuterPlacement->SlotNumber, 1);
		TestEqual(TEXT("enemy middle unit remains 敌 2P"), EnemyMiddlePlacement->SlotNumber, 2);
		TestEqual(TEXT("enemy inner unit remains 敌 3P"), EnemyInnerPlacement->SlotNumber, 3);
		TestEqual(TEXT("enemy 1P uses the approved outward outer coordinate"), EnemyOuterPlacement->Location, FVector(-80.0f, -295.0f, 90.0f));
		TestEqual(TEXT("enemy 2P uses the approved outward middle coordinate"), EnemyMiddlePlacement->Location, FVector(-20.0f, -225.0f, 90.0f));
		TestEqual(TEXT("enemy 3P uses the approved outward inner coordinate"), EnemyInnerPlacement->Location, FVector(40.0f, -155.0f, 90.0f));
		TestTrue(TEXT("enemy slots open toward the central hand space"), EnemyOuterPlacement->Location.Y < EnemyInnerPlacement->Location.Y);
	}
	if (HeroPlacement && QuestNpcPlacement && EnemyOuterPlacement)
	{
		const TArray<FGameXXKBattleSceneUnitRefreshDecision> RefreshDecisions = AGameXXKBattleScenePresenter::BuildUnitRefreshDecisions(
			{TEXT("Player"), TEXT("CompanionInstance.Companion_Blade_Test"), TEXT("Enemy.Outer")},
			{*HeroPlacement, *QuestNpcPlacement, *EnemyOuterPlacement});
		auto HasRefreshAction = [&RefreshDecisions](const FName UnitId, const EGameXXKBattleSceneRefreshAction Action)
		{
			return RefreshDecisions.ContainsByPredicate([UnitId, Action](const FGameXXKBattleSceneUnitRefreshDecision& Decision)
			{
				return Decision.UnitId == UnitId && Decision.Action == Action;
			});
		};
		TestTrue(TEXT("unchanged hero UnitId is retained through a membership change"), HasRefreshAction(TEXT("Player"), EGameXXKBattleSceneRefreshAction::Retain));
		TestTrue(TEXT("unchanged enemy UnitId is retained through a membership change"), HasRefreshAction(TEXT("Enemy.Outer"), EGameXXKBattleSceneRefreshAction::Retain));
		TestTrue(TEXT("departed companion UnitId is removed without rebuilding retained actors"), HasRefreshAction(TEXT("CompanionInstance.Companion_Blade_Test"), EGameXXKBattleSceneRefreshAction::Remove));
		TestTrue(TEXT("new quest NPC UnitId is spawned without replacing retained actors"), HasRefreshAction(TEXT("Npc.YueBai"), EGameXXKBattleSceneRefreshAction::Spawn));
	}

	AGameXXKBattleSceneUnitActor* HudHeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
	UPaperFlipbookComponent* HudHeroVisual = HudHeroActor->GetBattleVisualComponent();
	TestNull(TEXT("battle scene actor retires every WidgetComponent"), HudHeroActor->FindComponentByClass<UWidgetComponent>());
	TArray<UWidgetComponent*> HudWidgetComponents;
	HudHeroActor->GetComponents<UWidgetComponent>(HudWidgetComponents);
	TestEqual(TEXT("battle scene actor has no hidden WidgetComponent fallback"), HudWidgetComponents.Num(), 0);
	TestNotNull(TEXT("battle scene actor keeps a battle visual for pure HUD projection"), HudHeroVisual);
	if (!HudHeroVisual)
	{
		return false;
	}
	const FVector EmptyActorHudProjection = HudHeroActor->GetBattleHudProjectionWorldLocation();
	TestFalse(TEXT("new-object pure HUD-foot projection remains finite"), EmptyActorHudProjection.ContainsNaN());
	const FVector EmptyExpectedHudProjection = HudHeroVisual->Bounds.Origin - FVector(0.0f, 0.0f, HudHeroVisual->Bounds.BoxExtent.Z);
	TestTrue(TEXT("new-object pure HUD-foot projection exactly follows visual bounds"), EmptyActorHudProjection.Equals(EmptyExpectedHudProjection, KINDA_SMALL_NUMBER));

	UGameInstance* SnapshotGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* SnapshotSubsystem = NewObject<UGameXXKMVPSubsystem>(SnapshotGameInstance);
	SnapshotSubsystem->GetMutableRuntimeState() = BuildFixedSlotSceneBattleState();
	const FRotator PreservedVisualRotation = HudHeroVisual->GetRelativeRotation();
	const FVector PreservedVisualLocation = HudHeroVisual->GetRelativeLocation();
	const FVector PreservedVisualScale = HudHeroVisual->GetRelativeScale3D();
	HudHeroActor->SetMVPSubsystemForTest(SnapshotSubsystem);
	HudHeroActor->ConfigureFromRuntimeUnit(false, 0, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[0], 2);
	TestEqual(TEXT("hero actor takes Card runtime HP instead of legacy projection"), HudHeroActor->GetCurrentHealthForTest(), 72);
	TestEqual(TEXT("hero actor takes Card runtime maximum HP instead of legacy projection"), HudHeroActor->GetMaxHealthForTest(), 100);
	TestEqual(TEXT("runtime refresh never changes the tuned battle visual rotation"), HudHeroVisual->GetRelativeRotation(), PreservedVisualRotation);
	TestEqual(TEXT("runtime refresh never changes the tuned battle visual location"), HudHeroVisual->GetRelativeLocation(), PreservedVisualLocation);
	TestEqual(TEXT("runtime refresh never changes the tuned battle visual scale"), HudHeroVisual->GetRelativeScale3D(), PreservedVisualScale);
	const FVector ConfiguredHudProjection = HudHeroActor->GetBattleHudProjectionWorldLocation();
	TestFalse(TEXT("configured pure HUD-foot projection remains finite"), ConfiguredHudProjection.ContainsNaN());
	const FVector ConfiguredExpectedHudProjection = HudHeroVisual->Bounds.Origin - FVector(0.0f, 0.0f, HudHeroVisual->Bounds.BoxExtent.Z);
	TestTrue(TEXT("configured pure HUD-foot projection exactly follows visual bounds"), ConfiguredHudProjection.Equals(ConfiguredExpectedHudProjection, KINDA_SMALL_NUMBER));

	AGameXXKBattleSceneUnitActor* CompanionHudActor = NewObject<AGameXXKBattleSceneUnitActor>();
	CompanionHudActor->SetMVPSubsystemForTest(SnapshotSubsystem);
	CompanionHudActor->ConfigureFromRuntimeUnit(false, 1, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[1], 1);
	TestEqual(TEXT("permanent companion retains the configured 1P slot"), CompanionHudActor->GetSlotNumberForTest(), 1);

	AGameXXKBattleSceneUnitActor* QuestNpcHudActor = NewObject<AGameXXKBattleSceneUnitActor>();
	QuestNpcHudActor->SetMVPSubsystemForTest(SnapshotSubsystem);
	QuestNpcHudActor->ConfigureFromRuntimeUnit(false, 2, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[2], 3);
	TestEqual(TEXT("temporary quest NPC retains the configured 3P slot"), QuestNpcHudActor->GetSlotNumberForTest(), 3);

	AGameXXKBattleSceneUnitActor* EnemyHudActor = NewObject<AGameXXKBattleSceneUnitActor>();
	EnemyHudActor->SetMVPSubsystemForTest(SnapshotSubsystem);
	EnemyHudActor->ConfigureFromRuntimeUnit(true, 0, SnapshotSubsystem->GetRuntimeState().ActiveBattleEnemies[0], 1);
	TestEqual(TEXT("enemy takes the Card runtime HP"), EnemyHudActor->GetCurrentHealthForTest(), 240);

	FGameXXKCardCombatUnit* SnapshotHeroCardUnit = SnapshotSubsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("card-authoritative HUD fixture retains the hero combat unit"), SnapshotHeroCardUnit);
	if (SnapshotHeroCardUnit)
	{
		SnapshotHeroCardUnit->HP = 63;
		HudHeroActor->ConfigureFromRuntimeUnit(false, 0, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[0], 2);
		TestEqual(TEXT("retained actor refresh takes Card runtime HP without a legacy sync"), HudHeroActor->GetCurrentHealthForTest(), 63);

		SnapshotHeroCardUnit->HP = 55;
		HudHeroActor->ConfigureFromRuntimeUnit(false, 0, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[0], 2);
		TestEqual(TEXT("retained hero refresh keeps changed Card runtime HP"), HudHeroActor->GetCurrentHealthForTest(), 55);

		SnapshotHeroCardUnit->bLiving = false;
		HudHeroActor->ConfigureFromRuntimeUnit(false, 0, SnapshotSubsystem->GetRuntimeState().ActiveBattleParty[0], 2);
		TestFalse(TEXT("defeated card hero cannot retain a target outline"), HudHeroActor->IsCardTargetOutlineEnabled());
	}

	FGameXXKRuntimeState HeroAndNpcOnlyState = FixedSlotState;
	HeroAndNpcOnlyState.ActiveBattleParty.RemoveAll([](const FGameXXKBattleRuntimeUnit& Unit)
	{
		return Unit.Id == TEXT("CompanionInstance.Companion_Blade_Test");
	});
	HeroAndNpcOnlyState.CardRun.ActiveBattle.Units.RemoveAll([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("CompanionInstance.Companion_Blade_Test");
	});
	const TArray<FGameXXKBattleSceneUnitPlacement> HeroAndNpcOnlyPlacements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(HeroAndNpcOnlyState);
	const FGameXXKBattleSceneUnitPlacement* LoneQuestNpcPlacement = HeroAndNpcOnlyPlacements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("Npc.YueBai");
	});
	TestNotNull(TEXT("quest NPC still has a scene placement without a permanent partner"), LoneQuestNpcPlacement);
	if (LoneQuestNpcPlacement)
	{
		TestEqual(TEXT("missing companion never shifts the task NPC into 我 2P"), LoneQuestNpcPlacement->SlotNumber, 3);
		TestEqual(TEXT("missing companion never shifts the task NPC coordinate"), LoneQuestNpcPlacement->Location, FVector(40.0f, 155.0f, 90.0f));
	}

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildSceneBattleState();
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].HP = 50;
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].MaxHP = 50;
	FGameXXKCardCombatUnit* CardHero = Subsystem->GetMutableRuntimeState().CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestNotNull(TEXT("scene status projection has the authoritative card hero"), CardHero);
	if (CardHero)
	{
		CardHero->Armor = 7;
		CardHero->Statuses = {FGameXXKCardStatusStack{EGameXXKCardStatus::Poison, 2}};
		FString ProjectionError;
		TestTrue(TEXT("card armor projection updates the legacy shield"), FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(Subsystem->GetMutableRuntimeState(), &ProjectionError));
		TestEqual(TEXT("armor seven projects to legacy Shield seven"), Subsystem->GetRuntimeState().ActiveBattleParty[0].Shield, 7);
	}

	const TArray<FGameXXKBattleSceneUnitPlacement> Placements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(Subsystem->GetRuntimeState());
	TestTrue(TEXT("battle scene exposes at least one enemy placement"), Placements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement) { return Placement.bEnemy; }));
	TestTrue(TEXT("battle scene exposes at least one party placement"), Placements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement) { return !Placement.bEnemy; }));

	const FGameXXKBattleSceneUnitPlacement* FirstEnemyPlacement = Placements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return Placement.bEnemy && Placement.UnitIndex == 0;
	});
	const FGameXXKBattleSceneUnitPlacement* HeroPartyPlacement = Placements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitId == TEXT("Player");
	});
	TestTrue(TEXT("battle scene exposes the first enemy placement"), FirstEnemyPlacement != nullptr);
	TestTrue(TEXT("battle scene exposes the central hero placement"), HeroPartyPlacement != nullptr);
	if (FirstEnemyPlacement && HeroPartyPlacement)
	{
		TestTrue(TEXT("battle scene enemy 1P stays in the fixed outer lane under the fixed camera"), FirstEnemyPlacement->Location.Y <= -190.0f);
		TestEqual(TEXT("battle scene hero remains in the central fixed P2 lane"), HeroPartyPlacement->Location, FVector(-20.0f, 225.0f, 90.0f));
		TestTrue(TEXT("battle scene enemy lane stays above the board"), FMath::Abs(FirstEnemyPlacement->Location.Z - 90.0) <= KINDA_SMALL_NUMBER);
		TestTrue(TEXT("battle scene hero lane stays above the board"), FMath::Abs(HeroPartyPlacement->Location.Z - 90.0) <= KINDA_SMALL_NUMBER);
	}

	for (const FGameXXKBattleSceneUnitPlacement& Placement : Placements)
	{
		if (Placement.bEnemy)
		{
			TestTrue(TEXT("enemy placements retain one of the fixed negative-Y P lanes"), Placement.Location.Y <= -150.0f && Placement.Location.Y >= -330.0f);
		}
		else
		{
			TestTrue(TEXT("party placements retain one of the fixed positive-Y P lanes"), Placement.Location.Y >= 90.0f && Placement.Location.Y <= 330.0f);
		}
		TestTrue(TEXT("battle scene unit rows stay inside the fixed P-slot depth band"), Placement.Location.X >= -190.0f && Placement.Location.X <= 45.0f);
		TestTrue(TEXT("every placement keeps a valid fixed P-slot number"), Placement.SlotNumber >= 1 && Placement.SlotNumber <= 3);
	}

	AGameXXKBattleSceneUnitActor* EnemyVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	UPaperFlipbookComponent* EnemyBattleVisual = EnemyVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
	TestNotNull(TEXT("battle scene enemy actor has a Paper2D scene visual like town characters"), EnemyBattleVisual);
	if (EnemyBattleVisual)
	{
		TestEqual(TEXT("battle scene visual keeps HD2D town sprite rotation"), EnemyBattleVisual->GetRelativeRotation(), FRotator(0.0f, 90.0f, -30.0f));
		TestEqual(TEXT("battle scene visual is lifted above the board for the fixed camera"), EnemyBattleVisual->GetRelativeLocation(), FVector::ZeroVector);
		TestEqual(TEXT("battle scene visual keeps the town character plane scale"), EnemyBattleVisual->GetRelativeScale3D(), FVector(0.55f, 0.55f, 0.55f));
		TestNull(TEXT("battle scene actor owns no hidden health/status text renderer"), EnemyVisualActor->FindComponentByClass<UTextRenderComponent>());
		EnemyVisualActor->ConfigureFromRuntimeUnit(true, 0, Subsystem->GetRuntimeState().ActiveBattleEnemies[0]);
		TestNotNull(TEXT("battle scene enemy actor assigns a visible flipbook"), EnemyBattleVisual->GetFlipbook());
		if (EnemyBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene actor resolves the canonical idle or its legacy money-mouse fallback"),
				MatchesCanonicalOrLegacyIdle(
					EnemyBattleVisual->GetFlipbook(),
					Subsystem->GetRuntimeState().ActiveBattleEnemies[0].Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse")));
		}

		AGameXXKBattleSceneUnitActor* WolfVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* WolfBattleVisual = WolfVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		// The canonical normal encounter now contains one MoneyRat.  Keep this
		// compatibility assertion self-contained instead of relying on a second
		// encounter member that is no longer part of the rules fixture.
		FGameXXKBattleRuntimeUnit LegacyWolfUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		LegacyWolfUnit.Id = TEXT("Wolf");
		WolfVisualActor->ConfigureFromRuntimeUnit(true, 1, LegacyWolfUnit);
		TestNotNull(TEXT("battle scene Wolf actor assigns a visible flipbook"), WolfBattleVisual ? WolfBattleVisual->GetFlipbook() : nullptr);
		if (WolfBattleVisual && WolfBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("legacy Wolf resolves the canonical idle or generic legacy enemy fallback"),
				MatchesCanonicalOrLegacyIdle(
					WolfBattleVisual->GetFlipbook(),
					LegacyWolfUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_Default")));
			TestFalse(
				TEXT("legacy Wolf fallback never maps to the Niu Huan event NPC visual"),
				WolfBattleVisual->GetFlipbook()->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_NiuHuan")));
		}

		FGameXXKBattleRuntimeUnit MoneyRatUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		MoneyRatUnit.Id = TEXT("MoneyRat");
		AGameXXKBattleSceneUnitActor* MoneyRatVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* MoneyRatBattleVisual = MoneyRatVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		MoneyRatVisualActor->ConfigureFromRuntimeUnit(true, 0, MoneyRatUnit);
		TestNotNull(TEXT("battle scene MoneyRat actor assigns a visible flipbook"), MoneyRatBattleVisual ? MoneyRatBattleVisual->GetFlipbook() : nullptr);
		if (MoneyRatBattleVisual && MoneyRatBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene MoneyRat actor resolves the canonical idle or money-mouse fallback"),
				MatchesCanonicalOrLegacyIdle(
					MoneyRatBattleVisual->GetFlipbook(),
					MoneyRatUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse")));
		}

		FGameXXKBattleRuntimeUnit BlackBearUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		BlackBearUnit.Id = TEXT("BlackBear");
		AGameXXKBattleSceneUnitActor* BlackBearVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* BlackBearBattleVisual = BlackBearVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		BlackBearVisualActor->ConfigureFromRuntimeUnit(true, 0, BlackBearUnit);
		TestNotNull(TEXT("battle scene BlackBear actor assigns a visible flipbook"), BlackBearBattleVisual ? BlackBearBattleVisual->GetFlipbook() : nullptr);
		if (BlackBearBattleVisual && BlackBearBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene BlackBear actor resolves the canonical idle or black-bear fallback"),
				MatchesCanonicalOrLegacyIdle(
					BlackBearBattleVisual->GetFlipbook(),
					BlackBearUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear")));
		}

		FGameXXKBattleRuntimeUnit TigerUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		TigerUnit.Id = TEXT("Tiger");
		AGameXXKBattleSceneUnitActor* TigerVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* TigerBattleVisual = TigerVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		TigerVisualActor->ConfigureFromRuntimeUnit(true, 0, TigerUnit);
		TestNotNull(TEXT("battle scene Tiger actor assigns a visible flipbook"), TigerBattleVisual ? TigerBattleVisual->GetFlipbook() : nullptr);
		if (TigerBattleVisual && TigerBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene Tiger actor resolves the canonical idle or tiger fallback"),
				MatchesCanonicalOrLegacyIdle(
					TigerBattleVisual->GetFlipbook(),
					TigerUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger")));
		}

		FGameXXKBattleRuntimeUnit EliteUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		EliteUnit.Id = TEXT("EliteBandit");
		AGameXXKBattleSceneUnitActor* EliteVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* EliteBattleVisual = EliteVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		EliteVisualActor->ConfigureFromRuntimeUnit(true, 0, EliteUnit);
		TestNotNull(TEXT("battle scene EliteBandit actor assigns a visible flipbook"), EliteBattleVisual ? EliteBattleVisual->GetFlipbook() : nullptr);
		if (EliteBattleVisual && EliteBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene EliteBandit actor resolves the canonical idle or black-bear fallback"),
				MatchesCanonicalOrLegacyIdle(
					EliteBattleVisual->GetFlipbook(),
					EliteUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear")));
		}

		FGameXXKBattleRuntimeUnit BossUnit = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
		BossUnit.Id = TEXT("Boss");
		AGameXXKBattleSceneUnitActor* BossVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* BossBattleVisual = BossVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		BossVisualActor->ConfigureFromRuntimeUnit(true, 0, BossUnit);
		TestNotNull(TEXT("battle scene Boss actor assigns a visible flipbook"), BossBattleVisual ? BossBattleVisual->GetFlipbook() : nullptr);
		if (BossBattleVisual && BossBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene Boss actor resolves the canonical idle or tiger fallback"),
				MatchesCanonicalOrLegacyIdle(
					BossBattleVisual->GetFlipbook(),
					BossUnit.Id,
					TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger")));
		}
	}

	AGameXXKBattleSceneUnitActor* PartyVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	UPaperFlipbookComponent* PartyBattleVisual = PartyVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
	TestNotNull(TEXT("battle scene party actor has a Paper2D scene visual like town characters"), PartyBattleVisual);
	if (PartyBattleVisual)
	{
		PartyVisualActor->SetMVPSubsystemForTest(Subsystem);
		PartyVisualActor->ConfigureFromRuntimeUnit(false, 0, Subsystem->GetRuntimeState().ActiveBattleParty[0], 1);
		TestNotNull(TEXT("battle scene hero actor assigns the hero battle flipbook"), PartyBattleVisual->GetFlipbook());
		TestEqual(TEXT("status actor exposes the fixed role slot for test inspection"), PartyVisualActor->GetSlotNumberForTest(), 1);

		if (CardHero)
		{
			CardHero->Armor = 0;
			CardHero->Statuses.Reset();
			FString ClearedProjectionError;
			TestTrue(TEXT("cleared card status projects before retained actor refresh"), FGameXXKCardBattleAdapter::SyncCardBattleToLegacyProjection(Subsystem->GetMutableRuntimeState(), &ClearedProjectionError));
			const FGameXXKBattleRuntimeUnit ClearedStatusParty = Subsystem->GetRuntimeState().ActiveBattleParty[0];
			PartyVisualActor->ConfigureFromRuntimeUnit(false, 0, ClearedStatusParty, 1);
		}

		PartyVisualActor->PlayHitFeedback();
		TestTrue(TEXT("hit feedback enables the actor tick for temporary visual motion"), PartyVisualActor->IsActorTickEnabled());
		PartyVisualActor->Tick(1.0f);
		TestFalse(TEXT("completed hit feedback restores the actor to a non-ticking idle state"), PartyVisualActor->IsActorTickEnabled());
		TestEqual(TEXT("completed hit feedback restores the base scene scale"), PartyBattleVisual->GetRelativeScale3D(), FVector(0.55f, 0.55f, 0.55f));
	}

	AGameXXKBattleSceneUnitActor* EnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	EnemyActor->SetMVPSubsystemForTest(Subsystem);
	EnemyActor->ConfigureFromRuntimeUnit(true, 0, Subsystem->GetRuntimeState().ActiveBattleEnemies[0]);
	const int32 EnemyHPBefore = Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP;
	TestFalse(TEXT("enemy actor cannot open party command menu"), EnemyActor->CanOpenPartyCommandMenu());
	TestTrue(TEXT("living enemy actor can receive targeted battle action"), EnemyActor->CanReceiveTargetedBattleAction());
	TestFalse(TEXT("enemy actor direct primary attack shortcut is disabled for player-facing input"), EnemyActor->ApplyPrimaryPartyAttack(nullptr));
	TestEqual(TEXT("enemy actor shortcut does not damage enemy"), Subsystem->GetRuntimeState().ActiveBattleEnemies[0].HP, EnemyHPBefore);

	AGameXXKBattleSceneUnitActor* PartyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	PartyActor->SetMVPSubsystemForTest(Subsystem);
	PartyActor->ConfigureFromRuntimeUnit(false, 0, Subsystem->GetRuntimeState().ActiveBattleParty[0]);
	TestTrue(TEXT("living party actor can open command menu"), PartyActor->CanOpenPartyCommandMenu());
	TestFalse(TEXT("party actor cannot receive targeted enemy action"), PartyActor->CanReceiveTargetedBattleAction());

	FGameXXKBattleRuntimeUnit DefeatedParty = Subsystem->GetRuntimeState().ActiveBattleParty[0];
	DefeatedParty.HP = 0;
	DefeatedParty.bDefeated = true;
	AGameXXKBattleSceneUnitActor* DefeatedPartyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	DefeatedPartyActor->ConfigureFromRuntimeUnit(false, 0, DefeatedParty);
	TestFalse(TEXT("defeated party actor cannot open command menu"), DefeatedPartyActor->CanOpenPartyCommandMenu());

	FGameXXKBattleRuntimeUnit DefeatedEnemy = Subsystem->GetRuntimeState().ActiveBattleEnemies[0];
	DefeatedEnemy.HP = 0;
	DefeatedEnemy.bDefeated = true;
	AGameXXKBattleSceneUnitActor* DefeatedEnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	DefeatedEnemyActor->ConfigureFromRuntimeUnit(true, 0, DefeatedEnemy);
	TestFalse(TEXT("defeated enemy actor cannot receive targeted battle action"), DefeatedEnemyActor->CanReceiveTargetedBattleAction());

	return true;
}

#endif
