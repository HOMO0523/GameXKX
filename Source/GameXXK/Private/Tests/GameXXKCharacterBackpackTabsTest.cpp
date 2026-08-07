#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterBackpackTabsTest,
	"GameXXK.MVP.UI.FinalInventory.CharacterTabsAndDeckBackpack",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterBackpackTabsTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("character-tab fixture enters town"), Subsystem && Subsystem->EnsureQingshanTownRuntimeForDirectMap());
	if (!Subsystem)
	{
		return false;
	}
	TestTrue(TEXT("character-tab fixture initializes the canonical hero card pool"), Subsystem->PrepareCompanionRosterForTown());

	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->TakeWidget();
	TestTrue(TEXT("final character backpack opens"), Inventory->OpenFreeInventoryForTest());
	TestEqual(TEXT("the final character backpack exposes five real tabs"), Inventory->GetCharacterTabButtonCountForTest(), 5);
	TestEqual(TEXT("equipment is the default character tab"), Inventory->GetActiveCharacterBackpackTabForTest(), EGameXXKCharacterBackpackTab::Equipment);

	TestTrue(TEXT("the attribute tab can be opened"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes));
	TestTrue(TEXT("the attribute tab renders the live hero snapshot"), Inventory->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("攻击")));

	TestTrue(TEXT("the card tab replaces the equipment backpack"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));
	TestEqual(TEXT("the card backpack exposes all twelve hero cards"), Inventory->GetHeroCardBackpackIdsForTest().Num(), 12);
	TestEqual(TEXT("the character deck starts with eight cards"), Inventory->GetPendingHeroDeckIdsForTest().Num(), 8);
	TestTrue(TEXT("the card backpack uses the approved final frame"), Inventory->GetHeroCardFrameResourcePathForTest().Contains(TEXT("T_MasterV2_CardFrame")));
	TestTrue(TEXT("locked cards use the approved simplified ink lock"), Inventory->GetHeroLockedCardIconResourcePathForTest().Contains(TEXT("T_MasterV2_CardLockedIcon")));

	const TArray<FName> OriginalDeck = Inventory->GetPendingHeroDeckIdsForTest();
	const FName RemovedCard = OriginalDeck[0];
	const TArray<FName> HeroCardBackpack = Inventory->GetHeroCardBackpackIdsForTest();
	const FName* ReplacementCard = HeroCardBackpack.FindByPredicate([&OriginalDeck](const FName CardId)
	{
		return !OriginalDeck.Contains(CardId);
	});
	TestNotNull(TEXT("the twelve-card backpack has a replacement candidate"), ReplacementCard);
	if (!ReplacementCard)
	{
		return false;
	}
	const FName ReplacementCardId = *ReplacementCard;
	TestTrue(TEXT("an equipped hero card can be staged out"), Inventory->ToggleHeroDeckCardForTest(RemovedCard));
	TestTrue(TEXT("an unlocked backpack card can be staged in"), Inventory->ToggleHeroDeckCardForTest(ReplacementCardId));
	TestTrue(TEXT("the staged eight-card deck can be committed"), Inventory->ApplyHeroDeckForTest());
	TestEqual(TEXT("the card tab persists the changed deck through the facade"), Subsystem->GetHeroCardLoadout(), Inventory->GetPendingHeroDeckIdsForTest());

	TestTrue(TEXT("the talent tab is clickable"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Talents));
	TestTrue(TEXT("the talent tab explicitly reports its unavailable state"), Inventory->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("尚未开放")));
	TestTrue(TEXT("the title tab is clickable"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Titles));
	TestTrue(TEXT("the title tab explicitly reports its unavailable state"), Inventory->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("尚未开放")));
	TestTrue(TEXT("returning to equipment restores the real equipment backpack"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Equipment));
	return true;
}

#endif
