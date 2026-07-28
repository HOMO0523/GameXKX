#include "Misc/AutomationTest.h"
#include "UI/GameXXKPartyDeckUiStyle.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyDeckUiStyleTest,
	"GameXXK.UI.PartyDeck.Scrollbars",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyDeckUiStyleTest::RunTest(const FString& Parameters)
{
	TestEqual(
		TEXT("PartyDeck scrollbar track is the approved generated paper asset"),
		FGameXXKPartyDeckUiStyle::GetPaperTrackResourcePath(),
		FString(TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollPaperTrack_GeneratedV1.T_PartyDeck_ScrollPaperTrack_GeneratedV1")));
	TestEqual(
		TEXT("PartyDeck scrollbar thumb is the approved generated ink asset"),
		FGameXXKPartyDeckUiStyle::GetInkThumbResourcePath(),
		FString(TEXT("/Game/GameXXK/UI/PartyDeck/Scrollbars/T_PartyDeck_ScrollInkThumb_GeneratedV1.T_PartyDeck_ScrollInkThumb_GeneratedV1")));
	return true;
}

#endif
