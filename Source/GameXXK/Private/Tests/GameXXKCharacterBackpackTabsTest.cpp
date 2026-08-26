#include "Misc/AutomationTest.h"

#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/UniformGridPanel.h"

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
	Subsystem->GetMutableRuntimeState().PlayerLevel = 2;
	Subsystem->GetMutableRuntimeState().PlayerXP = 50;

	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->TakeWidget();
	TestTrue(TEXT("final character backpack opens"), Inventory->OpenFreeInventoryForTest());
	TestEqual(TEXT("the final character backpack exposes five real tabs"), Inventory->GetCharacterTabButtonCountForTest(), 5);
	TestEqual(TEXT("equipment is the default character tab"), Inventory->GetActiveCharacterBackpackTabForTest(), EGameXXKCharacterBackpackTab::Equipment);
	UButton* AttributeTab = Inventory->WidgetTree
		? Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_0")))
		: nullptr;
	UButton* EquipmentTab = Inventory->WidgetTree
		? Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterTab_1")))
		: nullptr;
	const UObject* InitialAttributeResource = AttributeTab
		? AttributeTab->GetStyle().Normal.GetResourceObject()
		: nullptr;
	const UObject* InitialEquipmentResource = EquipmentTab
		? EquipmentTab->GetStyle().Normal.GetResourceObject()
		: nullptr;
	TestTrue(TEXT("unselected Attributes uses the approved normal tab"),
		InitialAttributeResource
		&& InitialAttributeResource->GetPathName().Contains(TEXT("003_tab_1")));
	TestTrue(TEXT("selected Equipment uses the approved selected tab"),
		InitialEquipmentResource
		&& InitialEquipmentResource->GetPathName().Contains(TEXT("004_tab_2")));

	TestTrue(TEXT("the attribute tab can be opened"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Attributes));
	const UObject* SelectedAttributeResource = AttributeTab
		? AttributeTab->GetStyle().Normal.GetResourceObject()
		: nullptr;
	const UObject* UnselectedEquipmentResource = EquipmentTab
		? EquipmentTab->GetStyle().Normal.GetResourceObject()
		: nullptr;
	TestTrue(TEXT("selected Attributes switches to the approved selected tab"),
		SelectedAttributeResource
		&& SelectedAttributeResource->GetPathName().Contains(TEXT("004_tab_2")));
	TestTrue(TEXT("unselected Equipment switches to the approved normal tab"),
		UnselectedEquipmentResource
		&& UnselectedEquipmentResource->GetPathName().Contains(TEXT("003_tab_1")));
	TestTrue(TEXT("the attribute tab renders the live hero snapshot"), Inventory->GetCharacterTabBodyTextForTest().ToString().Contains(TEXT("攻击")));
	UProgressBar* ExperienceBar = Inventory->WidgetTree
		? Cast<UProgressBar>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceBar")))
		: nullptr;
	UTextBlock* ExperienceText = Inventory->WidgetTree
		? Cast<UTextBlock>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCharacterExperienceText")))
		: nullptr;
	TestNotNull(TEXT("the attribute tab owns a visible experience bar"), ExperienceBar);
	TestTrue(TEXT("the hero experience bar shows 50 of the level-two 200 threshold"),
		ExperienceBar && FMath::IsNearlyEqual(ExperienceBar->GetPercent(), 0.25f));
	TestTrue(TEXT("the attribute tab labels current and required experience"),
		ExperienceText && ExperienceText->GetText().ToString().Contains(TEXT("50 / 200")));

	TestTrue(TEXT("the card tab replaces the equipment backpack"), Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));
	TestEqual(TEXT("the card backpack exposes all thirty-six hero cards"), Inventory->GetHeroCardBackpackIdsForTest().Num(), 36);
	UUniformGridPanel* HeroDeckGrid = Inventory->WidgetTree
		? Cast<UUniformGridPanel>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckGrid")))
		: nullptr;
	TestNotNull(TEXT("the card backpack keeps the existing scroll-grid layout"), HeroDeckGrid);
	TestEqual(TEXT("the scroll grid materializes a selectable slot for every hero card"),
		HeroDeckGrid ? HeroDeckGrid->GetChildrenCount() : 0, 36);
	TestEqual(TEXT("the character deck starts with eight cards"), Inventory->GetPendingHeroDeckIdsForTest().Num(), 8);
	TestTrue(TEXT("the card backpack uses the approved final frame"), Inventory->GetHeroCardFrameResourcePathForTest().Contains(TEXT("T_MasterV2_CardFrame")));
	UButton* FirstDeckCard = Inventory->WidgetTree
		? Cast<UButton>(Inventory->WidgetTree->FindWidget(TEXT("InventoryHeroDeckCard_00")))
		: nullptr;
	const UObject* FirstDeckFrameResource = FirstDeckCard
		? FirstDeckCard->GetStyle().Normal.GetResourceObject()
		: nullptr;
	TestTrue(TEXT("each rendered deck card actually draws the approved shared frame"),
		FirstDeckFrameResource
			&& FirstDeckFrameResource->GetPathName().Contains(TEXT("T_MasterV2_CardFrame")));
	TestTrue(TEXT("locked cards use the approved simplified ink lock"), Inventory->GetHeroLockedCardIconResourcePathForTest().Contains(TEXT("T_MasterV2_CardLockedIcon")));

	const TArray<FName> OriginalDeck = Inventory->GetPendingHeroDeckIdsForTest();
	const FName RemovedCard = OriginalDeck[0];
	const TArray<FName> HeroCardBackpack = Inventory->GetHeroCardBackpackIdsForTest();
	const FName* ReplacementCard = HeroCardBackpack.FindByPredicate([&OriginalDeck](const FName CardId)
	{
		const FGameXXKCardDefinition* Definition = FGameXXKCardCatalog::FindCardDefinition(CardId);
		return !OriginalDeck.Contains(CardId)
			&& Definition
			&& Definition->LinkedRole != EGameXXKCharacterRole::Invalid;
	});
	TestNotNull(TEXT("an initially unlocked profession-linked card is a selectable replacement"), ReplacementCard);
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
