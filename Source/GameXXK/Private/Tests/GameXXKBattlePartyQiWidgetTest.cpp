#include "Misc/AutomationTest.h"

// Keep the first TDD run executable before the production widget exists.  The test
// source is intentionally rebuilt after the header is added so the complete contract
// below is compiled and exercised.
#if __has_include("UI/GameXXKBattlePartyQiWidget.h")
#include "UI/GameXXKBattlePartyQiWidget.h"
#define GAMEXXK_HAS_BATTLE_PARTY_QI_WIDGET 1
#else
#define GAMEXXK_HAS_BATTLE_PARTY_QI_WIDGET 0
#endif

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKBattlePartyQiWidgetTest,
	"GameXXK.UI.Battle.PartyQiWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattlePartyQiWidgetTest::RunTest(const FString& Parameters)
{
#if !GAMEXXK_HAS_BATTLE_PARTY_QI_WIDGET
	AddError(TEXT("The required Board-owned UGameXXKBattlePartyQiWidget has not been implemented."));
	return false;
#else
	UGameXXKBattlePartyQiWidget* PartyQiWidget = NewObject<UGameXXKBattlePartyQiWidget>();
	TestNotNull(TEXT("party Qi widget is created"), PartyQiWidget);
	if (!PartyQiWidget)
	{
		return false;
	}

	TestTrue(TEXT("party Qi widget prepares a native runtime tree for board embedding"), PartyQiWidget->PrepareForBoardEmbedding());
	TestTrue(TEXT("party Qi widget retains its native runtime tree"), PartyQiWidget->HasRuntimeWidgetTreeForTest());
	TestEqual(TEXT("party Qi wrapper is input-transparent"), PartyQiWidget->GetVisibility(), ESlateVisibility::SelfHitTestInvisible);
	TestTrue(TEXT("party Qi soul icon and numeric overlay remain input-transparent"), PartyQiWidget->AreContentWidgetsHitTestTransparentForTest());
	TestTrue(TEXT("party Qi uses the generated circular soul icon source"), PartyQiWidget->GetPaperFrameResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/Battle/PartyQi/T_BattlePartyQi_SoulOrb")));
	TestEqual(TEXT("party Qi uses a deep-ink numeric overlay on the off-white rice-paper soul icon"), PartyQiWidget->GetQiInkColorForTest(), FLinearColor(0.12f, 0.10f, 0.075f, 1.0f));

	PartyQiWidget->SetSharedQi(7);
	TestEqual(TEXT("party Qi overlays only the exact current shared energy number on the soul icon"), PartyQiWidget->GetDisplayTextForTest(), FString(TEXT("7")));
	TestTrue(TEXT("party Qi keeps no secondary caption outside the circular icon"), PartyQiWidget->GetSubtitleTextForTest().IsEmpty());
	TestFalse(TEXT("party Qi never renders a current/maximum value"), PartyQiWidget->GetDisplayTextForTest().Contains(TEXT("/")));

	PartyQiWidget->SetSharedQi(-3);
	TestEqual(TEXT("party Qi clamps negative shared energy to zero"), PartyQiWidget->GetSharedQiForTest(), 0);
	TestEqual(TEXT("party Qi renders clamped zero exactly"), PartyQiWidget->GetDisplayTextForTest(), FString(TEXT("0")));
	return true;
#endif
}

#endif
