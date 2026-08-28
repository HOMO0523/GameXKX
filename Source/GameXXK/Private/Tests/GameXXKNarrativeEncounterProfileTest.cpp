#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Narrative/GameXXKBattleProfile.h"
#include "Narrative/GameXXKNarrativeEncounterCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeEncounterProfileBoundaryTest,
	"GameXXK.Narrative.Encounter.ProfileBoundary",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeEncounterProfileBoundaryTest::RunTest(const FString& Parameters)
{
	const FGameXXKNarrativeEncounterDefinition* Encounter =
		FGameXXKNarrativeEncounterCatalog::Find(TEXT("Encounter.Main.XuXiake.0-1"));
	if (!TestNotNull(TEXT("tutorial encounter resolves"), Encounter))
	{
		return false;
	}
	TestEqual(TEXT("tutorial encounter uses one rooster"),
		Encounter->EnemyIds, TArray<FName>{TEXT("Enemy.Ch1.Rooster")});
	TestEqual(TEXT("tutorial battle profile id"),
		Encounter->BattleProfileId, FName(TEXT("BattleProfile.Tutorial.0-1")));
	const FGameXXKBattleProfileDefinition* Profile =
		FGameXXKBattleProfileCatalog::Find(Encounter->BattleProfileId);
	if (!TestNotNull(TEXT("battle profile resolves"), Profile))
	{
		return false;
	}
	FString Error;
	TestTrue(TEXT("battle profile validates"),
		FGameXXKBattleProfileCatalog::Validate(*Profile, &Error));
	TestEqual(TEXT("profile has three party anchors"), Profile->PartyAnchors.Num(), 3);
	TestTrue(TEXT("profile supports one to three enemies"), Profile->EnemyAnchors.Num() == 3);
	for (const FGameXXKBattleAnchor& Anchor : Profile->PartyAnchors)
	{
		TestTrue(TEXT("party anchor is normalized"),
			Anchor.NormalizedPosition.X >= 0.0f
			&& Anchor.NormalizedPosition.X <= 1.0f
			&& Anchor.NormalizedPosition.Y >= 0.0f
			&& Anchor.NormalizedPosition.Y <= 1.0f);
	}
	const UScriptStruct* EncounterStruct = FGameXXKNarrativeEncounterDefinition::StaticStruct();
	const UScriptStruct* ProfileStruct = FGameXXKBattleProfileDefinition::StaticStruct();
	for (const FName Forbidden : {
		FName(TEXT("Transform")), FName(TEXT("MapPath")), FName(TEXT("WorldLocation"))})
	{
		TestNull(TEXT("encounter owns no scene transform"), EncounterStruct->FindPropertyByName(Forbidden));
		TestNull(TEXT("battle profile owns no town transform"), ProfileStruct->FindPropertyByName(Forbidden));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKNarrativeEncounterFullScreenBattleTest,
	"GameXXK.Narrative.Encounter.StartsExistingBattleBoardRuntime",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKNarrativeEncounterFullScreenBattleTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("battle fixture starts game"), Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	const FGameXXKRuntimeState Before = Subsystem->GetRuntimeStateCopy();
	FString Error;
	TestFalse(TEXT("unknown encounter rejects atomically"),
		Subsystem->StartNarrativeEncounter(TEXT("Encounter.Missing"), &Error));
	TestEqual(TEXT("unknown encounter leaves screen"),
		Subsystem->GetRuntimeState().Screen, Before.Screen);

	TestTrue(FString::Printf(TEXT("tutorial encounter starts: %s"), *Error),
		Subsystem->StartNarrativeEncounter(TEXT("Encounter.Main.XuXiake.0-1"), &Error));
	const FGameXXKRuntimeState& State = Subsystem->GetRuntimeState();
	TestEqual(TEXT("existing full-screen battle state opens"), State.Screen, EGameXXKScreen::Battle);
	TestTrue(TEXT("legacy battle projection active"), State.bHasActiveBattle);
	TestTrue(TEXT("existing card battle authority active"), State.CardRun.bHasActiveCardBattle);
	TestEqual(TEXT("one enemy projected"), State.ActiveBattleEnemies.Num(), 1);
	TestEqual(TEXT("rooster catalog provenance retained"),
		State.ActiveBattleEnemies[0].EnemyDefinitionId, FName(TEXT("Enemy.Ch1.Rooster")));
	return true;
}

#endif
