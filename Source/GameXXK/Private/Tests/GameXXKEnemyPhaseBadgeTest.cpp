#include "UI/GameXXKBattleUnitStatusEffectsWidget.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyPhaseBadgeTest,
	"GameXXK.UI.Battle.EnemyIntent.PhaseBadges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyPhaseBadgeTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKBattleStatusBadgeModel> HellPhaseOne =
		UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(0, {}, 1, 3);
	if (!TestEqual(TEXT("Hell phase one shows two remaining phase marks"), HellPhaseOne.Num(), 2))
	{
		return false;
	}
	TestEqual(TEXT("Hell renders phase three first"), HellPhaseOne[0].Style.IconId, FName(TEXT("EnemyPhase.3")));
	TestEqual(TEXT("Hell renders phase two second"), HellPhaseOne[1].Style.IconId, FName(TEXT("EnemyPhase.2")));
	TestEqual(TEXT("phase three fallback glyph"), HellPhaseOne[0].Style.FallbackGlyph, FString(TEXT("三")));
	TestEqual(TEXT("phase two fallback glyph"), HellPhaseOne[1].Style.FallbackGlyph, FString(TEXT("二")));

	const TArray<FGameXXKBattleStatusBadgeModel> HellPhaseTwo =
		UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(0, {}, 2, 3);
	TestEqual(TEXT("after one transition only phase three remains"), HellPhaseTwo.Num(), 1);
	TestEqual(TEXT("remaining mark is phase three"), HellPhaseTwo[0].Style.IconId, FName(TEXT("EnemyPhase.3")));
	TestTrue(TEXT("phase marks outrank armor and ordinary status"), HellPhaseTwo[0].Style.Priority > 1000);

	const TArray<FGameXXKBattleStatusBadgeModel> Normal =
		UGameXXKBattleUnitStatusEffectsWidget::BuildBadgeModels(0, {}, 1, 1);
	TestTrue(TEXT("single-phase enemies show no phase mark"), Normal.IsEmpty());
	return true;
}

#endif
