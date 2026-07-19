#include "Misc/AutomationTest.h"
#include "UI/GameXXKBattleUnitResourceWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattleUnitResourceWidgetTest,
	"GameXXK.UI.Battle.UnitResourceWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleUnitResourceWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKBattleUnitResourceWidget* ResourceWidget = NewObject<UGameXXKBattleUnitResourceWidget>();
	TestNotNull(TEXT("resource widget is created"), ResourceWidget);
	if (!ResourceWidget)
	{
		return false;
	}

	TestTrue(TEXT("resource widget prepares a native runtime tree for screen-space embedding"), ResourceWidget->PrepareForScreenSpaceEmbedding());
	TestTrue(TEXT("resource widget retains its native runtime tree"), ResourceWidget->HasRuntimeWidgetTreeForTest());

	ResourceWidget->SetUnitResources(TEXT("我 1P"), FText::FromString(TEXT("主角")), 0, 0, 0, 0, true);
	TestEqual(TEXT("zero health snapshot retains its supplied maximum label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 0 / 0")));
	TestEqual(TEXT("zero qi snapshot retains its supplied maximum label"), ResourceWidget->GetQiDisplayTextForTest(), FString(TEXT("气力 0 / 0")));
	TestEqual(TEXT("zero health snapshot uses an empty safe fill"), ResourceWidget->GetHealthPercentForTest(), 0.0f);
	TestEqual(TEXT("zero qi snapshot uses an empty safe fill"), ResourceWidget->GetQiPercentForTest(), 0.0f);
	TestTrue(TEXT("zero qi snapshot remains visible when qi is enabled"), ResourceWidget->IsQiRowVisibleForTest());

	ResourceWidget->SetUnitResources(TEXT("我 1P"), FText::FromString(TEXT("主角")), 72, 100, 18, 30, true);
	TestEqual(TEXT("hero health row uses the required readable label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 72 / 100")));
	TestEqual(TEXT("hero qi row uses the required readable label"), ResourceWidget->GetQiDisplayTextForTest(), FString(TEXT("气力 18 / 30")));
	TestEqual(TEXT("hero health fill follows current and maximum health"), ResourceWidget->GetHealthPercentForTest(), 0.72f);
	TestEqual(TEXT("hero qi fill follows current and maximum qi"), ResourceWidget->GetQiPercentForTest(), 0.60f);
	TestTrue(TEXT("hero qi row is visible when qi is enabled"), ResourceWidget->IsQiRowVisibleForTest());
	TestTrue(TEXT("hero resource content never blocks screen-space targeting"), ResourceWidget->AreContentWidgetsHitTestTransparentForTest());

	ResourceWidget->SetUnitResources(TEXT("敌 1P"), FText::FromString(TEXT("黑熊")), 240, 240, 99, 100, false);
	TestEqual(TEXT("enemy health row uses the required readable label"), ResourceWidget->GetHealthDisplayTextForTest(), FString(TEXT("气血 240 / 240")));
	TestFalse(TEXT("enemy qi row remains hidden despite a mana value"), ResourceWidget->IsQiRowVisibleForTest());
	TestTrue(TEXT("enemy resource content never blocks screen-space targeting"), ResourceWidget->AreContentWidgetsHitTestTransparentForTest());
	TestEqual(TEXT("resource root leaves screen-space hit testing to its owner"), UGameXXKBattleUnitResourceWidget::GetRootHitTestVisibilityForTest(), ESlateVisibility::SelfHitTestInvisible);
	return true;
}

#endif
