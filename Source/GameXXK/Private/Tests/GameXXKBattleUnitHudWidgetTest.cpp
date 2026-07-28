#include "Misc/AutomationTest.h"

#include "Components/VerticalBox.h"
#include "GameXXKBattlePresentation.h"
#include "UI/GameXXKBattleUnitHudWidget.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"
#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardStatusStack MakeHudStatus(const EGameXXKCardStatus Status, const int32 Stacks)
	{
		FGameXXKCardStatusStack Result;
		Result.Status = Status;
		Result.Stacks = Stacks;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitHudWidgetTest,
	"GameXXK.UI.Battle.UnitHudWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitHudWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleUnitHudWidget* const UnitWidget = NewObject<UGameXXKBattleUnitHudWidget>();
	TestNotNull(TEXT("composite unit HUD widget is created"), UnitWidget);
	if (!UnitWidget)
	{
		return false;
	}

	TestTrue(TEXT("composite unit HUD prepares a native tree for board embedding"), UnitWidget->PrepareForBoardEmbedding());
	TestEqual(TEXT("composite root is self-hit-test-invisible"), UnitWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("composite root exposes its input transparency contract"),
		UGameXXKBattleUnitHudWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
	UVerticalBox* const RootBox = Cast<UVerticalBox>(UnitWidget->GetWidgetFromName(TEXT("BattleUnitHudRoot")));
	TestNotNull(TEXT("composite unit HUD owns a vertical native root"), RootBox);
	if (RootBox)
	{
		TestEqual(TEXT("composite root owns resource and status child widgets"), RootBox->GetChildrenCount(), 2);
	}

	FGameXXKBattleUnitHudView HeroView;
	HeroView.UnitId = TEXT("Player");
	HeroView.Side = EGameXXKCardTargetSide::Party;
	HeroView.Role = EGameXXKCharacterRole::Hero;
	HeroView.DisplayName = FText::FromString(TEXT("主角"));
	HeroView.SlotNumber = 1;
	HeroView.bLiving = true;
	HeroView.bShowMana = true;
	HeroView.CurrentHP = 72;
	HeroView.MaxHP = 100;
	HeroView.CurrentMana = 18;
	HeroView.MaxMana = 30;
	HeroView.Armor = 7;
	HeroView.Statuses = {MakeHudStatus(EGameXXKCardStatus::Poison, 2)};
	UnitWidget->SetUnitView(HeroView);

	TestEqual(TEXT("composite HUD retains stable unit identity"), UnitWidget->GetUnitIdForTest(), FName(TEXT("Player")));
	TestEqual(TEXT("composite HUD retains authoritative side"), UnitWidget->GetSideForTest(), EGameXXKCardTargetSide::Party);
	TestEqual(TEXT("composite HUD retains fixed slot number"), UnitWidget->GetSlotNumberForTest(), 1);
	UGameXXKBattleUnitResourceWidget* const ResourceWidget = UnitWidget->GetResourceWidgetForTest();
	UGameXXKBattleUnitStatusEffectsWidget* const StatusWidget = UnitWidget->GetStatusEffectsWidgetForTest();
	TestNotNull(TEXT("composite HUD embeds the ordinary resource widget"), ResourceWidget);
	TestNotNull(TEXT("composite HUD embeds the ordinary status widget"), StatusWidget);
	if (ResourceWidget)
	{
		TestEqual(TEXT("hero health text is sourced through the composite HUD"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
		TestEqual(TEXT("hero mana text is sourced through the composite HUD"), ResourceWidget->GetManaDisplayTextForTest(), FString(TEXT("内力 18 / 30")));
	}
	if (StatusWidget)
	{
		TestEqual(TEXT("armor badge is present through the composite HUD"), StatusWidget->GetIconIdForTest(0), FName(TEXT("ArmorShield")));
		TestTrue(TEXT("poison badge is present through the composite HUD"),
			StatusWidget->GetIconCountForTest() > 1 && StatusWidget->GetIconIdForTest(1) == FName(TEXT("PoisonVial")));
	}

	FGameXXKBattleUnitHudView EnemyView = HeroView;
	EnemyView.UnitId = TEXT("Enemy.One");
	EnemyView.Side = EGameXXKCardTargetSide::Enemy;
	EnemyView.Role = EGameXXKCharacterRole::Invalid;
	EnemyView.DisplayName = FText::FromString(TEXT("黑熊"));
	EnemyView.SlotNumber = 1;
	EnemyView.bLiving = true;
	EnemyView.bShowMana = false;
	EnemyView.CurrentMana = 99;
	EnemyView.MaxMana = 100;
	UnitWidget->SetUnitView(EnemyView);
	TestEqual(TEXT("enemy composite HUD always collapses its mana row"),
		ResourceWidget ? ResourceWidget->GetManaRowVisibilityForTest() : ESlateVisibility::Visible,
		ESlateVisibility::Collapsed);
	TestTrue(TEXT("enemy fixture remains living for its mana visibility policy"), EnemyView.bLiving);

	FGameXXKBattleUnitHudView DefeatedView = EnemyView;
	DefeatedView.bLiving = false;
	UnitWidget->SetUnitView(DefeatedView);
	TestEqual(TEXT("defeated unit collapses the composite HUD"), UnitWidget->GetVisibility(), ESlateVisibility::Collapsed);

	UnitWidget->SetUnitView(EnemyView);
	TestEqual(TEXT("restored living unit makes the composite HUD visible but input-transparent"), UnitWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestEqual(TEXT("restored living unit keeps its root input-transparent"), RootBox->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);

	return true;
}

#endif
