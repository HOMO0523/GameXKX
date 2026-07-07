#include "GameXXKMVPRules.h"
#include "Components/TextRenderComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleScenePresenter.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState BuildSceneBattleState()
	{
		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleSceneActorTest,
	"GameXXK.MVP.Battle.SceneActors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleSceneActorTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->GetMutableRuntimeState() = BuildSceneBattleState();
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].HP = 50;
	Subsystem->GetMutableRuntimeState().ActiveBattleEnemies[0].MaxHP = 50;

	const TArray<FGameXXKBattleSceneUnitPlacement> Placements = AGameXXKBattleScenePresenter::BuildUnitPlacementsForState(Subsystem->GetRuntimeState());
	TestTrue(TEXT("battle scene exposes at least one enemy placement"), Placements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement) { return Placement.bEnemy; }));
	TestTrue(TEXT("battle scene exposes at least one party placement"), Placements.ContainsByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement) { return !Placement.bEnemy; }));

	const FGameXXKBattleSceneUnitPlacement* FirstEnemyPlacement = Placements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return Placement.bEnemy && Placement.UnitIndex == 0;
	});
	const FGameXXKBattleSceneUnitPlacement* FirstPartyPlacement = Placements.FindByPredicate([](const FGameXXKBattleSceneUnitPlacement& Placement)
	{
		return !Placement.bEnemy && Placement.UnitIndex == 0;
	});
	TestTrue(TEXT("battle scene exposes the first enemy placement"), FirstEnemyPlacement != nullptr);
	TestTrue(TEXT("battle scene exposes the first party placement"), FirstPartyPlacement != nullptr);
	if (FirstEnemyPlacement && FirstPartyPlacement)
	{
		TestTrue(TEXT("battle scene uses visible Y lanes for screen-space left/right under the fixed +X camera"), FirstEnemyPlacement->Location.Y >= -240.0f && FirstEnemyPlacement->Location.Y <= -200.0f && FirstPartyPlacement->Location.Y >= 240.0f && FirstPartyPlacement->Location.Y <= 300.0f);
		TestTrue(TEXT("battle scene opposing lanes share comparable camera depth"), FMath::Abs(FirstEnemyPlacement->Location.X - FirstPartyPlacement->Location.X) <= 20.0f);
		TestTrue(TEXT("battle scene enemy lane stays above the board"), FMath::Abs(FirstEnemyPlacement->Location.Z - 90.0) <= KINDA_SMALL_NUMBER);
		TestTrue(TEXT("battle scene party lane stays above the board"), FMath::Abs(FirstPartyPlacement->Location.Z - 90.0) <= KINDA_SMALL_NUMBER);
	}

	for (const FGameXXKBattleSceneUnitPlacement& Placement : Placements)
	{
		if (Placement.bEnemy)
		{
			TestTrue(TEXT("enemy placements use the visible screen-left negative Y lane"), Placement.Location.Y >= -240.0f && Placement.Location.Y <= -200.0f);
		}
		else
		{
			TestTrue(TEXT("party placements use the visible screen-right positive Y lane"), Placement.Location.Y >= 240.0f && Placement.Location.Y <= 300.0f);
		}
		TestTrue(TEXT("battle scene unit rows stay inside the lower fixed-camera depth band"), Placement.Location.X >= -220.0f && Placement.Location.X <= 60.0f);
	}

	AGameXXKBattleSceneUnitActor* EnemyVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	UPaperFlipbookComponent* EnemyBattleVisual = EnemyVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
	TestNotNull(TEXT("battle scene enemy actor has a Paper2D scene visual like town characters"), EnemyBattleVisual);
	if (EnemyBattleVisual)
	{
		TestEqual(TEXT("battle scene visual keeps HD2D town sprite rotation"), EnemyBattleVisual->GetRelativeRotation(), FRotator(0.0f, 90.0f, -30.0f));
		TestEqual(TEXT("battle scene visual is lifted above the board for the fixed camera"), EnemyBattleVisual->GetRelativeLocation(), FVector::ZeroVector);
		TestEqual(TEXT("battle scene visual keeps the town character plane scale"), EnemyBattleVisual->GetRelativeScale3D(), FVector(0.55f, 0.55f, 0.55f));
		TestFalse(TEXT("battle scene debug label is hidden so it does not cover the sprite"), EnemyVisualActor->GetLabelTextComponent()->IsVisible());
		EnemyVisualActor->ConfigureFromRuntimeUnit(true, 0, Subsystem->GetRuntimeState().ActiveBattleEnemies[0]);
		TestNotNull(TEXT("battle scene enemy actor assigns a visible flipbook"), EnemyBattleVisual->GetFlipbook());
		if (EnemyBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene Bandit actor uses the GameXXK money mouse visual"),
				EnemyBattleVisual->GetFlipbook()->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_MoneyMouse")));
		}

		AGameXXKBattleSceneUnitActor* WolfVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
		UPaperFlipbookComponent* WolfBattleVisual = WolfVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
		WolfVisualActor->ConfigureFromRuntimeUnit(true, 1, Subsystem->GetRuntimeState().ActiveBattleEnemies[1]);
		TestNotNull(TEXT("battle scene Wolf actor assigns a visible flipbook"), WolfBattleVisual ? WolfBattleVisual->GetFlipbook() : nullptr);
		if (WolfBattleVisual && WolfBattleVisual->GetFlipbook())
		{
			TestTrue(
				TEXT("battle scene Wolf actor uses the GameXXK niu huan visual"),
				WolfBattleVisual->GetFlipbook()->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_NiuHuan")));
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
				TEXT("battle scene EliteBandit actor uses the GameXXK black bear visual"),
				EliteBattleVisual->GetFlipbook()->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Enemy_BlackBear")));
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
				TEXT("battle scene Boss actor uses the GameXXK tiger boss visual"),
				BossBattleVisual->GetFlipbook()->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Enemies/Flipbooks/FB_Boss_Tiger")));
		}
	}

	AGameXXKBattleSceneUnitActor* PartyVisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	UPaperFlipbookComponent* PartyBattleVisual = PartyVisualActor->FindComponentByClass<UPaperFlipbookComponent>();
	TestNotNull(TEXT("battle scene party actor has a Paper2D scene visual like town characters"), PartyBattleVisual);
	if (PartyBattleVisual)
	{
		PartyVisualActor->ConfigureFromRuntimeUnit(false, 0, Subsystem->GetRuntimeState().ActiveBattleParty[0]);
		TestNotNull(TEXT("battle scene hero actor assigns the hero battle flipbook"), PartyBattleVisual->GetFlipbook());
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
