#include "GameXXKPartyFormationRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDirectQingshanPartyTest,
	"GameXXK.MVP.PlayableShell.DirectQingshanEntryParty",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDirectQingshanPartyTest::RunTest(const FString& Parameters)
{
	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(GameInstance);
	if (!TestNotNull(TEXT("direct-Qingshan subsystem creates"), Subsystem)
		|| !TestTrue(TEXT("direct Qingshan establishes a playable runtime"),
			Subsystem->EnsureQingshanTownRuntimeForDirectMap()))
	{
		return false;
	}

	const FGameXXKCardRunState& CardRun = Subsystem->GetRuntimeState().CardRun;
	TestEqual(TEXT("direct Qingshan owns all six starter partners"),
		CardRun.CompanionRoster.PermanentCompanions.Num(),
		6);
	TestEqual(TEXT("direct Qingshan owns all six named NPC loadouts"),
		CardRun.PartySelection.QuestNpcCardLoadouts.Num(),
		6);
	TestEqual(TEXT("direct Qingshan owns all six named NPC progressions"),
		CardRun.PartySelection.QuestNpcProgressions.Num(),
		6);
	TestEqual(TEXT("direct Qingshan owns an exact three-member formation"),
		CardRun.OrderedFormation.Members.Num(),
		FGameXXKPartyFormationRules::PartySize);
	TestFalse(TEXT("direct Qingshan selects a permanent partner"),
		CardRun.PartySelection.ActivePermanentCompanionInstanceId.IsNone());
	FName ActiveNpcId;
	TestTrue(TEXT("direct Qingshan selects a fixed named NPC"),
		FGameXXKPartyFormationRules::ResolveQuestNpcId(
			Subsystem->GetRuntimeState(),
			ActiveNpcId));

	UGameXXKDesktopTrainingWorkbenchWidget* Workbench =
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	Workbench->SetMVPSubsystem(Subsystem);
	Workbench->ConstructForTest();
	TestEqual(TEXT("formation partner tab exposes six candidates"),
		Workbench->GetCompanionCharacterIdsForTest().Num(),
		6);
	TestEqual(TEXT("formation NPC tab exposes six candidates"),
		Workbench->GetNpcCharacterIdsForTest().Num(),
		6);
	return true;
}

#endif
