#include "Components/BoxComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKBattleRuntimeUnit MakeLegacyUnit(
		const FName UnitId,
		const FText& DisplayName,
		const int32 Health,
		const int32 MaximumHealth,
		const int32 Mana,
		const int32 MaximumMana,
		const int32 Shield = 0)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = UnitId;
		Unit.DisplayName = DisplayName;
		Unit.HP = Health;
		Unit.MaxHP = MaximumHealth;
		Unit.MP = Mana;
		Unit.MaxMP = MaximumMana;
		Unit.Shield = Shield;
		return Unit;
	}

	FGameXXKCardCombatUnit MakeCardUnit(
		const FName UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 Health,
		const int32 MaximumHealth,
		const int32 Mana,
		const int32 MaximumMana,
		const int32 Armor)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = UnitId;
		Unit.Side = Side;
		Unit.bLiving = true;
		Unit.HP = Health;
		Unit.MaxHP = MaximumHealth;
		Unit.Mana = Mana;
		Unit.MaxMana = MaximumMana;
		Unit.Armor = Armor;
		return Unit;
	}

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleSceneActorHudTest,
	"GameXXK.MVP.Battle.SceneActorHud",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleSceneActorHudTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.CardRun.bHasActiveCardBattle = true;

	FGameXXKCardCombatUnit HeroCard = MakeCardUnit(
		TEXT("Hero.Hud"),
		EGameXXKCardTargetSide::Party,
		72,
		100,
		18,
		30,
		7);
	State.CardRun.ActiveBattle.Units.Add(HeroCard);

	FGameXXKCardCombatUnit EnemyCard = MakeCardUnit(
		TEXT("Enemy.Hud"),
		EGameXXKCardTargetSide::Enemy,
		240,
		240,
		99,
		100,
		0);
	EnemyCard.StableSortOrder = 2;
	State.CardRun.ActiveBattle.Units.Add(EnemyCard);

	FGameXXKCardCombatUnit CompanionCard = MakeCardUnit(
		TEXT("Companion.Hud"),
		EGameXXKCardTargetSide::Party,
		55,
		80,
		0,
		0,
		2);
	CompanionCard.Role = EGameXXKCharacterRole::Blade;
	State.CardRun.ActiveBattle.Units.Add(CompanionCard);

	FGameXXKCardCombatUnit NpcCard = MakeCardUnit(
		TEXT("Npc.Hud"),
		EGameXXKCardTargetSide::Party,
		31,
		60,
		0,
		0,
		1);
	NpcCard.Role = EGameXXKCharacterRole::QuestNpc;
	State.CardRun.ActiveBattle.Units.Add(NpcCard);

	AGameXXKBattleSceneUnitActor* HeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
	HeroActor->SetMVPSubsystemForTest(Subsystem);
	const FGameXXKBattleRuntimeUnit LegacyHero = MakeLegacyUnit(
		TEXT("Hero.Hud"),
		FText::FromString(TEXT("Hero HUD")),
		1,
		1,
		0,
		0,
		0);
	HeroActor->ConfigureFromRuntimeUnit(false, 0, LegacyHero);

	TestNull(TEXT("actor owns no resource/status WidgetComponent"), HeroActor->FindComponentByClass<UWidgetComponent>());
	TArray<UWidgetComponent*> WidgetComponents;
	HeroActor->GetComponents<UWidgetComponent>(WidgetComponents);
	TestEqual(TEXT("actor enumerates zero WidgetComponents"), WidgetComponents.Num(), 0);
	const FVector HudProjection = HeroActor->GetBattleHudProjectionWorldLocation();
	TestFalse(TEXT("pure HUD-foot projection stays finite"), HudProjection.ContainsNaN());
	UPaperFlipbookComponent* ProjectionVisual = HeroActor->GetBattleVisualComponent();
	TestNotNull(TEXT("actor keeps its battle visual for pure projection"), ProjectionVisual);
	if (ProjectionVisual)
	{
		const FVector ExpectedHudProjection = ProjectionVisual->Bounds.Origin - FVector(0.0f, 0.0f, ProjectionVisual->Bounds.BoxExtent.Z);
		TestTrue(TEXT("HUD-foot projection exactly follows the visual bounds foot formula"), HudProjection.Equals(ExpectedHudProjection, KINDA_SMALL_NUMBER));
	}
	UPaperFlipbookComponent* HighlightVisual = HeroActor->GetBattleVisualComponent();
	TestNotNull(TEXT("target highlight regression actor has a flipbook visual"), HighlightVisual);
	if (!HighlightVisual || !HighlightVisual->GetFlipbook() || HighlightVisual->GetFlipbookLength() <= KINDA_SMALL_NUMBER)
	{
		AddError(TEXT("target highlight playback regression requires a loaded non-empty flipbook"));
		return false;
	}

	HeroActor->SetCardTargetHighlight(true);
	TestTrue(TEXT("first target highlight transition enables the outline"), HeroActor->IsCardTargetOutlineEnabled());
	const float PlaybackProbe = FMath::Min(0.05f, HighlightVisual->GetFlipbookLength() * 0.5f);
	HighlightVisual->SetPlaybackPosition(PlaybackProbe, false);
	HeroActor->SetCardTargetHighlight(true);
	TestEqual(TEXT("repeating unchanged target highlight preserves flipbook playback"), HighlightVisual->GetPlaybackPosition(), PlaybackProbe);
	HeroActor->SetCardTargetHighlight(false);
	TestFalse(TEXT("clearing target highlight still updates the outline"), HeroActor->IsCardTargetOutlineEnabled());

	TestEqual(TEXT("card runtime overrides legacy health"), HeroActor->GetCurrentHealthForTest(), 72);
	TestEqual(TEXT("card runtime overrides legacy maximum health"), HeroActor->GetMaxHealthForTest(), 100);

	AGameXXKBattleSceneUnitActor* EnemyActor = NewObject<AGameXXKBattleSceneUnitActor>();
	EnemyActor->SetMVPSubsystemForTest(Subsystem);
	const FGameXXKBattleRuntimeUnit LegacyEnemy = MakeLegacyUnit(
		TEXT("Enemy.Hud"),
		FText::FromString(TEXT("Enemy HUD")),
		1,
		1,
		0,
		0,
		0);
	EnemyActor->ConfigureFromRuntimeUnit(true, 0, LegacyEnemy);
	TestEqual(TEXT("stable enemy sort order drives enemy display slot"), EnemyActor->GetSlotNumberForTest(), 3);

	AGameXXKBattleSceneUnitActor* CompanionActor = NewObject<AGameXXKBattleSceneUnitActor>();
	CompanionActor->SetMVPSubsystemForTest(Subsystem);
	CompanionActor->ConfigureFromRuntimeUnit(false, 1, MakeLegacyUnit(TEXT("Companion.Hud"), FText::FromString(TEXT("Companion HUD")), 1, 1, 0, 0));
	TestEqual(TEXT("permanent companion uses 1P slot"), CompanionActor->GetSlotNumberForTest(), 1);

	AGameXXKBattleSceneUnitActor* NpcActor = NewObject<AGameXXKBattleSceneUnitActor>();
	NpcActor->SetMVPSubsystemForTest(Subsystem);
	NpcActor->ConfigureFromRuntimeUnit(false, 2, MakeLegacyUnit(TEXT("Npc.Hud"), FText::FromString(TEXT("NPC HUD")), 1, 1, 0, 0));
	TestEqual(TEXT("temporary quest NPC uses 3P slot"), NpcActor->GetSlotNumberForTest(), 3);

	// The BattleVisual may correct asymmetric sprite content inside a fixed P-slot,
	// but the actor root and HitArea remain the authoritative target/collision anchor.
	AGameXXKBattleSceneUnitActor* PartyP1VisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	AGameXXKBattleSceneUnitActor* PartyP2VisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	AGameXXKBattleSceneUnitActor* PartyP3VisualActor = NewObject<AGameXXKBattleSceneUnitActor>();
	TestNotNull(TEXT("party P1 visual-centering actor exists"), PartyP1VisualActor);
	TestNotNull(TEXT("party P2 visual-centering actor exists"), PartyP2VisualActor);
	TestNotNull(TEXT("party P3 visual-centering actor exists"), PartyP3VisualActor);
	if (!PartyP1VisualActor || !PartyP2VisualActor || !PartyP3VisualActor)
	{
		return false;
	}

	const FVector P1RootBefore = PartyP1VisualActor->GetActorLocation();
	const FVector P2RootBefore = PartyP2VisualActor->GetActorLocation();
	const FVector P3RootBefore = PartyP3VisualActor->GetActorLocation();
	const FVector P1HitAreaBefore = PartyP1VisualActor->GetHitArea()->GetRelativeLocation();
	const FVector P2HitAreaBefore = PartyP2VisualActor->GetHitArea()->GetRelativeLocation();
	const FVector P3HitAreaBefore = PartyP3VisualActor->GetHitArea()->GetRelativeLocation();
	const FVector P1AuthoredVisualBase(5.0f, -3.0f, 11.0f);
	PartyP1VisualActor->GetBattleVisualComponent()->SetRelativeLocation(P1AuthoredVisualBase);

	PartyP1VisualActor->ConfigureFromRuntimeUnit(false, 1, MakeLegacyUnit(TEXT("Companion.VisualCenter"), FText::FromString(TEXT("P1")), 1, 1, 0, 0), 1);
	PartyP2VisualActor->ConfigureFromRuntimeUnit(false, 0, MakeLegacyUnit(TEXT("Hero.VisualCenter"), FText::FromString(TEXT("P2")), 1, 1, 0, 0), 2);
	PartyP3VisualActor->ConfigureFromRuntimeUnit(false, 2, MakeLegacyUnit(TEXT("Npc.VisualCenter"), FText::FromString(TEXT("P3")), 1, 1, 0, 0), 3);

	TestEqual(TEXT("party P1 asymmetric companion visual is shifted right inside its fixed slot without overwriting authored plane tuning"), PartyP1VisualActor->GetBattleVisualComponent()->GetRelativeLocation(), P1AuthoredVisualBase + FVector(0.0f, 24.0f, 0.0f));
	TestEqual(TEXT("party P2 hero visual stays centered inside its fixed slot"), PartyP2VisualActor->GetBattleVisualComponent()->GetRelativeLocation(), FVector::ZeroVector);
	TestEqual(TEXT("party P3 task NPC visual is shifted left inside its fixed slot"), PartyP3VisualActor->GetBattleVisualComponent()->GetRelativeLocation(), FVector(0.0f, -8.0f, 0.0f));
	TestEqual(TEXT("party P1 visual correction never moves the actor root"), PartyP1VisualActor->GetActorLocation(), P1RootBefore);
	TestEqual(TEXT("party P2 visual correction never moves the actor root"), PartyP2VisualActor->GetActorLocation(), P2RootBefore);
	TestEqual(TEXT("party P3 visual correction never moves the actor root"), PartyP3VisualActor->GetActorLocation(), P3RootBefore);
	TestEqual(TEXT("party P1 visual correction never moves the hit area"), PartyP1VisualActor->GetHitArea()->GetRelativeLocation(), P1HitAreaBefore);
	TestEqual(TEXT("party P2 visual correction never moves the hit area"), PartyP2VisualActor->GetHitArea()->GetRelativeLocation(), P2HitAreaBefore);
	TestEqual(TEXT("party P3 visual correction never moves the hit area"), PartyP3VisualActor->GetHitArea()->GetRelativeLocation(), P3HitAreaBefore);

	return true;
}

#endif
