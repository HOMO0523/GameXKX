#include "Components/SceneComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbookComponent.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

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
	FGameXXKCardStatusStack Poison;
	Poison.Status = EGameXXKCardStatus::Poison;
	Poison.Stacks = 2;
	HeroCard.Statuses.Add(Poison);
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
		9,
		16,
		2);
	CompanionCard.Role = EGameXXKCharacterRole::Blade;
	State.CardRun.ActiveBattle.Units.Add(CompanionCard);

	FGameXXKCardCombatUnit NpcCard = MakeCardUnit(
		TEXT("Npc.Hud"),
		EGameXXKCardTargetSide::Party,
		31,
		60,
		6,
		12,
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

	TestNotNull(TEXT("HUD anchor exists"), HeroActor->GetHudAnchorComponentForTest());
	TestNotNull(TEXT("resource HUD anchor exists"), HeroActor->GetResourceHudAnchorComponentForTest());
	TestNotNull(TEXT("status HUD anchor exists"), HeroActor->GetStatusEffectsAnchorComponentForTest());
	TestNotNull(TEXT("resource HUD component exists"), HeroActor->GetResourceHudWidgetComponentForTest());
	TestNotNull(TEXT("status HUD component exists"), HeroActor->GetStatusEffectsWidgetComponentForTest());
	if (!HeroActor->GetHudAnchorComponentForTest() || !HeroActor->GetResourceHudAnchorComponentForTest()
		|| !HeroActor->GetStatusEffectsAnchorComponentForTest() || !HeroActor->GetResourceHudWidgetComponentForTest()
		|| !HeroActor->GetStatusEffectsWidgetComponentForTest())
	{
		return false;
	}

	TestTrue(TEXT("HUD anchor attaches to battle visual"), HeroActor->GetHudAnchorComponentForTest()->GetAttachParent() == HeroActor->GetBattleVisualComponent());
	TestTrue(TEXT("resource anchor attaches to HUD anchor"), HeroActor->GetResourceHudAnchorComponentForTest()->GetAttachParent() == HeroActor->GetHudAnchorComponentForTest());
	TestTrue(TEXT("status anchor attaches to HUD anchor"), HeroActor->GetStatusEffectsAnchorComponentForTest()->GetAttachParent() == HeroActor->GetHudAnchorComponentForTest());
	TestTrue(TEXT("resource component attaches to resource anchor"), HeroActor->GetResourceHudWidgetComponentForTest()->GetAttachParent() == HeroActor->GetResourceHudAnchorComponentForTest());
	TestTrue(TEXT("status component attaches to status anchor"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetAttachParent() == HeroActor->GetStatusEffectsAnchorComponentForTest());
	TestEqual(TEXT("resource HUD is screen space"), HeroActor->GetResourceHudWidgetComponentForTest()->GetWidgetSpace(), EWidgetSpace::Screen);
	TestEqual(TEXT("status HUD is screen space"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetWidgetSpace(), EWidgetSpace::Screen);
	TestTrue(TEXT("resource HUD draw size"), HeroActor->GetResourceHudWidgetComponentForTest()->GetDrawSize().Equals(FVector2D(300.0f, 96.0f)));
	TestTrue(TEXT("status HUD draw size"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetDrawSize().Equals(FVector2D(300.0f, 46.0f)));
	TestTrue(TEXT("resource HUD bottom pivot"), HeroActor->GetResourceHudWidgetComponentForTest()->GetPivot().Equals(FVector2D(0.5f, 1.0f)));
	TestTrue(TEXT("status HUD top pivot"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetPivot().Equals(FVector2D(0.5f, 0.0f)));
	TestFalse(TEXT("resource HUD is not two-sided"), HeroActor->GetResourceHudWidgetComponentForTest()->GetTwoSided());
	TestFalse(TEXT("status HUD is not two-sided"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetTwoSided());
	TestTrue(TEXT("resource HUD class"), HeroActor->GetResourceHudWidgetComponentForTest()->GetWidgetClass() == UGameXXKBattleUnitResourceWidget::StaticClass());
	TestTrue(TEXT("status HUD class"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetWidgetClass() == UGameXXKBattleUnitStatusEffectsWidget::StaticClass());
	TestEqual(TEXT("resource HUD has no collision"), HeroActor->GetResourceHudWidgetComponentForTest()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestEqual(TEXT("status HUD has no collision"), HeroActor->GetStatusEffectsWidgetComponentForTest()->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
	TestTrue(TEXT("no-world HUD anchor stays finite"), !HeroActor->GetHudAnchorComponentForTest()->GetComponentLocation().ContainsNaN());

	TestEqual(TEXT("card runtime overrides legacy health"), HeroActor->GetCurrentHealthForTest(), 72);
	TestEqual(TEXT("card runtime overrides legacy maximum health"), HeroActor->GetMaxHealthForTest(), 100);
	TestEqual(TEXT("card runtime overrides legacy mana"), HeroActor->GetCurrentManaForTest(), 18);
	TestEqual(TEXT("card runtime overrides legacy maximum mana"), HeroActor->GetMaxManaForTest(), 30);
	TestEqual(TEXT("card runtime overrides legacy armor"), HeroActor->GetArmorForTest(), 7);
	TestTrue(TEXT("party actor shows Qi when card max mana is positive"), HeroActor->ShouldShowQiForTest());
	TestTrue(TEXT("status presentation projects card poison"), HeroActor->GetStatusTextForTest().Contains(TEXT("毒 2")));

	const int32 ResourceGenerationBeforeManaChange = HeroActor->GetResourcePresentationGenerationForTest();
	const int32 StatusGenerationBeforeManaChange = HeroActor->GetStatusEffectsPresentationGenerationForTest();
	State.CardRun.ActiveBattle.Units[0].Mana = 19;
	HeroActor->ConfigureFromRuntimeUnit(false, 0, LegacyHero);
	TestTrue(TEXT("mana change refreshes resource presentation"), HeroActor->GetResourcePresentationGenerationForTest() > ResourceGenerationBeforeManaChange);
	TestEqual(TEXT("mana-only change does not refresh status presentation"), HeroActor->GetStatusEffectsPresentationGenerationForTest(), StatusGenerationBeforeManaChange);

	const int32 StatusGenerationBeforeStatusChange = HeroActor->GetStatusEffectsPresentationGenerationForTest();
	State.CardRun.ActiveBattle.Units[0].Armor = 3;
	State.CardRun.ActiveBattle.Units[0].Statuses.Reset();
	FGameXXKCardStatusStack Bleed;
	Bleed.Status = EGameXXKCardStatus::Bleed;
	Bleed.Stacks = 2;
	State.CardRun.ActiveBattle.Units[0].Statuses.Add(Bleed);
	HeroActor->ConfigureFromRuntimeUnit(false, 0, LegacyHero);
	TestEqual(TEXT("card runtime status mutation updates armor"), HeroActor->GetArmorForTest(), 3);
	TestTrue(TEXT("card runtime status mutation updates status text"), HeroActor->GetStatusTextForTest().Contains(TEXT("流 2")));
	TestTrue(TEXT("status mutation refreshes status presentation"), HeroActor->GetStatusEffectsPresentationGenerationForTest() > StatusGenerationBeforeStatusChange);

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
	TestFalse(TEXT("enemy actor hides Qi even with mana"), EnemyActor->ShouldShowQiForTest());
	TestEqual(TEXT("stable enemy sort order drives enemy display slot"), EnemyActor->GetSlotNumberForTest(), 3);

	AGameXXKBattleSceneUnitActor* CompanionActor = NewObject<AGameXXKBattleSceneUnitActor>();
	CompanionActor->SetMVPSubsystemForTest(Subsystem);
	CompanionActor->ConfigureFromRuntimeUnit(false, 1, MakeLegacyUnit(TEXT("Companion.Hud"), FText::FromString(TEXT("Companion HUD")), 1, 1, 0, 0));
	TestTrue(TEXT("permanent companion shows Qi"), CompanionActor->ShouldShowQiForTest());
	TestEqual(TEXT("permanent companion uses 2P slot"), CompanionActor->GetSlotNumberForTest(), 2);

	AGameXXKBattleSceneUnitActor* NpcActor = NewObject<AGameXXKBattleSceneUnitActor>();
	NpcActor->SetMVPSubsystemForTest(Subsystem);
	NpcActor->ConfigureFromRuntimeUnit(false, 2, MakeLegacyUnit(TEXT("Npc.Hud"), FText::FromString(TEXT("NPC HUD")), 1, 1, 0, 0));
	TestTrue(TEXT("temporary quest NPC shows Qi"), NpcActor->ShouldShowQiForTest());
	TestEqual(TEXT("temporary quest NPC uses 3P slot"), NpcActor->GetSlotNumberForTest(), 3);

	State.CardRun.ActiveBattle.Units[0].bLiving = false;
	HeroActor->ConfigureFromRuntimeUnit(false, 0, LegacyHero);
	TestFalse(TEXT("defeated actor hides resource HUD component"), HeroActor->GetResourceHudWidgetComponentForTest()->IsVisible());
	TestFalse(TEXT("defeated actor hides status HUD component"), HeroActor->GetStatusEffectsWidgetComponentForTest()->IsVisible());

	return true;
}

#endif
