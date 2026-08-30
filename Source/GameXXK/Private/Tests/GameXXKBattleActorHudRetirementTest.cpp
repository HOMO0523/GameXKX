#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "GameXXKPermanentPartyTestFixtures.h"
#include "Blueprint/WidgetTree.h"
#include "Camera/CameraActor.h"
#include "Components/CanvasPanel.h"
#include "Components/WidgetInteractionComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPPlayerController.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleBoardWidget.h"
#include "UI/GameXXKBattleUnitHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	template<typename TActorType>
	int32 CountWorldActors(UWorld* const World)
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

	int32 CountWorldFlipbookComponents(UWorld* const World)
	{
		int32 Count = 0;
		for (TObjectIterator<UPaperFlipbookComponent> It; It; ++It)
		{
			if (It->GetWorld() == World)
			{
				++Count;
			}
		}
		return Count;
	}

	FGameXXKBattleRuntimeUnit MakeActorHudRetirementEnemy()
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = TEXT("ActorHudRetirement.Enemy");
		Unit.DisplayName = FText::FromString(TEXT("界面归属敌人"));
		Unit.HP = 240;
		Unit.MaxHP = 240;
		Unit.Attack = 8;
		Unit.Defense = 0;
		Unit.Speed = 8;
		Unit.bEnemy = true;
		return Unit;
	}

	bool BuildActorHudRetirementFixture(
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
			State = GameXXKPermanentPartyTestFixtures::MakeStartedState();
			State.Screen = EGameXXKScreen::Battle;
			State.bHasActiveBattle = true;
			State.ActiveBattleNodeId = 47;
			State.ActiveBattleEnemies = {MakeActorHudRetirementEnemy()};

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

			const FGameXXKCardCombatUnit* const Hero = State.CardRun.ActiveBattle.Units.FindByPredicate(
				[](const FGameXXKCardCombatUnit& Unit)
				{
					return Unit.Side == EGameXXKCardTargetSide::Party
						&& Unit.Role == EGameXXKCharacterRole::Hero
						&& Unit.bLiving;
				});
			if (!Hero || State.CardRun.ActiveBattle.Deck.Hand.IsEmpty())
			{
				continue;
			}
			// Pin one known, affordable manual attack instead of depending on the
			// shuffled opening hand chosen by the fixture seed.
			FGameXXKCardInstance& FixtureCard = State.CardRun.ActiveBattle.Deck.Hand[0];
			FixtureCard.CardId = TEXT("Hero.Generic.QingFengYiShi");
			FixtureCard.OwnerUnitId = Hero->UnitId;
			State.CardRun.ActiveBattle.Deck.SharedEnergy = FMath::Max(
				State.CardRun.ActiveBattle.Deck.SharedEnergy,
				1);

			for (const FGameXXKCardInstance& CardInstance : State.CardRun.ActiveBattle.Deck.Hand)
			{
				if (CardInstance.InstanceId != FixtureCard.InstanceId)
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
	FGameXXKBattleActorHudRetirementTest,
	"GameXXK.UI.Battle.ActorHudRetirement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleActorHudRetirementTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FName CardInstanceId;
	FName TargetUnitId;
	FName OwnerUnitId;
	FString Error;
	TestTrue(
		FString::Printf(TEXT("retirement fixture enters a deterministic card battle: %s"), *Error),
		BuildActorHudRetirementFixture(Subsystem, CardInstanceId, TargetUnitId, OwnerUnitId, Error));
	if (CardInstanceId.IsNone() || TargetUnitId.IsNone() || OwnerUnitId.IsNone())
	{
		return false;
	}

	UGameXXKBattleBoardWidget* Board = NewObject<UGameXXKBattleBoardWidget>();
	Board->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("retirement board initializes its widget tree"), Board->Initialize());
	Board->NativeConstruct();
	Board->RefreshFromState();
	TestNull(TEXT("retired Board constructs no duplicate legacy animation layer"),
		Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleAnimationLayer")) : nullptr);
	TestEqual(TEXT("duplicate central participant images"), Board->GetDuplicateParticipantImageCountForTest(), 0);
	TestEqual(TEXT("synchronous animation loads"), Board->GetAtlasCacheStatsForTest().SyncLoadCount, 0);
	TestNull(TEXT("legacy footer widget is absent from the board"),
		Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleUnitFooter_00")) : nullptr);
	TestNull(TEXT("legacy board resource and status paper panel is absent"),
		Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleStatusPaperPanel")) : nullptr);
	TestNull(TEXT("legacy board resource and status text is absent"),
		Board->WidgetTree ? Board->WidgetTree->FindWidget(TEXT("BattleStatusText")) : nullptr);
	UCanvasPanel* const ProjectedHudLayer = Board->GetBattleProjectedUnitHudLayerForTest();
	TestNotNull(TEXT("retirement board owns a projected unit HUD layer"), ProjectedHudLayer);
	TestEqual(TEXT("projected HUD layer remains input transparent"),
		ProjectedHudLayer ? ProjectedHudLayer->GetVisibility() : ESlateVisibility::Visible,
		ESlateVisibility::SelfHitTestInvisible);
	UGameXXKBattleUnitHudWidget* const HeroHud = Board->GetProjectedUnitHudForTest(TEXT("Player"));
	TestNotNull(TEXT("retirement board finds the hero HUD by stable UnitId"), HeroHud);
	TestEqual(TEXT("projected hero HUD retains the input-transparent root contract"),
		UGameXXKBattleUnitHudWidget::GetRootHitTestVisibilityForTest(),
		ESlateVisibility::SelfHitTestInvisible);

	const FVector2D PointerProjection(442.0f, 283.0f);
	TestTrue(TEXT("manual card still enters arrow targeting after actor HUD retirement"), Board->ClickCardInHand(CardInstanceId));
	TestTrue(TEXT("manual card remains in targeting state after actor HUD retirement"), Board->IsCardTargetingActive());
	TestFalse(TEXT("card-arrow source is resolved by the fixed HUD stage without an actor projection"),
		Board->GetTargetingSourcePositionForTest().IsNearlyZero());
	TestTrue(TEXT("legal enemy remains highlighted after actor HUD retirement"), Board->IsTargetUnitHighlighted(TargetUnitId));
	Board->UpdateTargetingPointer(PointerProjection);
	TestEqual(TEXT("mouse pointer still drives the targeting arrow endpoint"), Board->GetTargetingPointerPositionForTest(), PointerProjection);

	AGameXXKMVPPlayerController* Controller = NewObject<AGameXXKMVPPlayerController>();
	TestNotNull(TEXT("retirement controller can be constructed"), Controller);
	if (Controller)
	{
		TArray<UWidgetInteractionComponent*> HoverBridgeComponents;
		Controller->GetComponents(HoverBridgeComponents);
		TestEqual(TEXT("board-owned projected screen HUD requires no world-widget interaction bridge"), HoverBridgeComponents.Num(), 0);
	}

	FString ControllerHeader;
	FString ControllerSource;
	FString BoardSource;
	FString UnitVisualSource;
	FString AtlasCacheSource;
	FString LegacyLayerSource;
	TestTrue(TEXT("retirement test reads the runtime controller header"), FFileHelper::LoadFileToString(
		ControllerHeader,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h"))));
	TestTrue(TEXT("retirement test reads the runtime controller source"), FFileHelper::LoadFileToString(
		ControllerSource,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp"))));
	TestTrue(TEXT("retirement test reads the runtime Board source"), FFileHelper::LoadFileToString(
		BoardSource,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Private/UI/GameXXKBattleBoardWidget.cpp"))));
	TestTrue(TEXT("retirement test reads the runtime unit-visual source"), FFileHelper::LoadFileToString(
		UnitVisualSource,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Private/UI/GameXXKBattleUnitVisualWidget.cpp"))));
	TestTrue(TEXT("retirement test reads the runtime atlas-cache source"), FFileHelper::LoadFileToString(
		AtlasCacheSource,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Private/UI/GameXXKBattleAtlasCache.cpp"))));
	TestTrue(TEXT("retirement test reads the dormant compatibility-layer source"), FFileHelper::LoadFileToString(
		LegacyLayerSource,
		*FPaths::Combine(FPaths::ProjectDir(), TEXT("Source/GameXXK/Private/UI/GameXXKBattleAnimationLayerWidget.cpp"))));
	for (const TCHAR* const RetiredControllerToken : {
		TEXT("GameXXKBattleScenePresenter"),
		TEXT("GameXXKBattleSceneUnitActor"),
		TEXT("PaperFlipbookComponent"),
		TEXT("ApplyBattleSceneCamera"),
		TEXT("RefreshBattleSceneAfterCardMutation")})
	{
		TestFalse(
			FString::Printf(TEXT("controller header has no retired runtime token %s"), RetiredControllerToken),
			ControllerHeader.Contains(RetiredControllerToken));
		TestFalse(
			FString::Printf(TEXT("controller source has no retired runtime token %s"), RetiredControllerToken),
			ControllerSource.Contains(RetiredControllerToken));
	}
	TestFalse(TEXT("active Board source has no legacy animation-layer reference"),
		BoardSource.Contains(TEXT("GameXXKBattleAnimationLayerWidget")));
	for (const TPair<const TCHAR*, const FString*> ActiveAnimationSource : {
		TPair<const TCHAR*, const FString*>(TEXT("Board"), &BoardSource),
		TPair<const TCHAR*, const FString*>(TEXT("unit visual"), &UnitVisualSource),
		TPair<const TCHAR*, const FString*>(TEXT("atlas cache"), &AtlasCacheSource)})
	{
		TestFalse(
			FString::Printf(TEXT("active %s action path performs no TryLoad"), ActiveAnimationSource.Key),
			ActiveAnimationSource.Value->Contains(TEXT("TryLoad")));
		TestFalse(
			FString::Printf(TEXT("active %s action path performs no LoadSynchronous"), ActiveAnimationSource.Key),
			ActiveAnimationSource.Value->Contains(TEXT("LoadSynchronous")));
	}
	TestFalse(TEXT("compatibility animation layer owns no duplicate attacker image"),
		LegacyLayerSource.Contains(TEXT("AttackerImage")));
	TestFalse(TEXT("compatibility animation layer owns no duplicate target image"),
		LegacyLayerSource.Contains(TEXT("TargetImage")));
	TestFalse(TEXT("compatibility animation layer performs no synchronous action texture load"),
		LegacyLayerSource.Contains(TEXT("TryLoad")));

	const FName TestWorldName = MakeUniqueObjectName(
		nullptr,
		UWorld::StaticClass(),
		TEXT("GameXXKBattleRetirementWorld"),
		EUniqueObjectNameOptions::GloballyUnique);
	FWorldContext& WorldContext = GEngine->CreateNewWorldContext(EWorldType::Game);
	UWorld* const RuntimeWorld = UWorld::CreateWorld(EWorldType::Game, false, TestWorldName, GetTransientPackage());
	TestNotNull(TEXT("retirement fixture creates an isolated runtime world"), RuntimeWorld);
	if (RuntimeWorld)
	{
		RuntimeWorld->AddToRoot();
		WorldContext.SetCurrentWorld(RuntimeWorld);
		RuntimeWorld->InitializeActorsForPlay(FURL());
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGameXXKMVPPlayerController* const RuntimeController = RuntimeWorld->SpawnActor<AGameXXKMVPPlayerController>(
			AGameXXKMVPPlayerController::StaticClass(),
			FVector::ZeroVector,
			FRotator::ZeroRotator,
			SpawnParameters);
		TestNotNull(TEXT("retirement fixture spawns the real runtime controller"), RuntimeController);
		if (RuntimeController)
		{
			RuntimeController->SetMVPSubsystemForTest(Subsystem);
			RuntimeController->EnsurePlayerFlowWidgetsForTest();
			RuntimeController->RefreshPlayerFlowWidgetsForTest();
		}
		TestEqual(TEXT("runtime scene presenter count"), CountWorldActors<AGameXXKBattleScenePresenter>(RuntimeWorld), 0);
		TestEqual(TEXT("runtime unit actor count"), CountWorldActors<AGameXXKBattleSceneUnitActor>(RuntimeWorld), 0);
		TestEqual(TEXT("runtime battle camera count"), CountWorldActors<ACameraActor>(RuntimeWorld), 0);
		TestEqual(TEXT("runtime flipbook component count"), CountWorldFlipbookComponents(RuntimeWorld), 0);
		if (RuntimeController)
		{
			RuntimeController->EndPlay(EEndPlayReason::Destroyed);
		}
		RuntimeWorld->DestroyWorld(false);
		GEngine->DestroyWorldContext(RuntimeWorld);
		RuntimeWorld->RemoveFromRoot();
	}

	TestTrue(TEXT("retirement preserves the canonical hero idle flipbook package"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_character_00_hero_idle")));
	TestTrue(TEXT("retirement preserves the canonical enemy idle flipbook package"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/BattleAnimations/IdleFlipbooks/FB_enemy_01_rooster_idle")));
	TestTrue(TEXT("retirement preserves the legacy hero directional flipbook package"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West")));

	return true;
}

#endif
