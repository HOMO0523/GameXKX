#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FName CreateInventoryWidgetEquipment(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::XuanJia;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 4;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(
			TEXT("final inventory fixture creates an instance-based equipment item"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalInventoryWidgetTest,
	"GameXXK.MVP.UI.FinalInventory.SixSlotsAndRightClickActions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	const FName FirstWeapon = CreateInventoryWidgetEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);
	const FName ReplacementWeapon = CreateInventoryWidgetEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);

	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->TakeWidget();
	TestTrue(TEXT("final hero inventory opens"), Inventory->OpenFreeInventoryForTest());
	TestEqual(TEXT("final hero inventory exposes six square equipment slots"), Inventory->GetEquipmentSlotCountForTest(), 6);
	TestEqual(TEXT("final hero inventory exposes twenty backpack cells"), Inventory->GetBackpackSlotCountForTest(), 20);
	TestEqual(
		TEXT("final hero inventory scrolls through the full two-hundred-slot storage"),
		Inventory->GetBackpackStorageCapacityForTest(),
		FGameXXKEquipmentRules::WarehouseCapacity);
	TestTrue(TEXT("final hero inventory owns a real scroll box"), Inventory->HasBackpackScrollBoxForTest());
	TestEqual(TEXT("final hero inventory uses four backpack columns"), Inventory->GetBackpackColumnCountForTest(), 4);
	TestTrue(
		TEXT("final hero inventory uses the approved ink close button"),
		Inventory->GetCloseButtonResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CloseInk")));
	TestTrue(
		TEXT("final hero inventory uses the approved right scrollbar"),
		Inventory->GetScrollbarResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_BackpackScrollbarRight")));
	TestTrue(
		TEXT("final hero inventory reuses the approved shared selection ink"),
		Inventory->GetSelectionInkResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_SelectionInk")));
	TestTrue(
		TEXT("final hero inventory composes tooltips from the approved item-slot paper"),
		Inventory->GetTooltipResourcePathForTest().Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_ItemSlot")));

	const TArray<FName> WarehouseEntries = Inventory->GetVisibleBackpackEquipmentInstanceIdsForTest();
	TestTrue(TEXT("warehouse equipment appears in the scrollable backpack"), WarehouseEntries.Contains(FirstWeapon));
	TestTrue(TEXT("replacement equipment also appears in the scrollable backpack"), WarehouseEntries.Contains(ReplacementWeapon));
	const int32 FirstWeaponSlot = Inventory->FindBackpackEquipmentInstanceSlotForTest(FirstWeapon);
	TestTrue(TEXT("warehouse equipment resolves to a clickable backpack cell"), FirstWeaponSlot >= 0);
	Inventory->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::PlayerBackpack, FirstWeaponSlot, NAME_None);
	TestTrue(
		TEXT("clicking an instance equipment cell exposes its composed tooltip detail"),
		Inventory->GetSelectedDetailTextForTest().ToString().Contains(TEXT("装备等级 4")));

	TestTrue(
		TEXT("right-clicking a warehouse equipment cell equips it"),
		Inventory->HandleConfiguredSlotRightClicked(
			EGameXXKInventorySlotSource::PlayerBackpack,
			FirstWeaponSlot,
			NAME_None));
	TestEqual(
		TEXT("first quick-equip fills the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon),
		FirstWeapon);
	TestTrue(TEXT("right-click action replaces occupied equipment"), Inventory->QuickEquipBackpackInstanceForTest(ReplacementWeapon));
	TestEqual(
		TEXT("replacement instance becomes visible in the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon),
		ReplacementWeapon);
	TestTrue(TEXT("replaced instance returns to the shared warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(FirstWeapon));
	TestTrue(TEXT("right-click equipment-slot action unequips"), Inventory->QuickUnequipSlotForTest(EGameXXKEquipmentSlot::Weapon));
	TestTrue(
		TEXT("right-click unequip clears the hero weapon slot"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon).IsNone());
	TestTrue(TEXT("unequipped replacement returns to the shared warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(ReplacementWeapon));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEmbeddedNpcInventoryWidgetTest,
	"GameXXK.MVP.UI.FinalInventory.EmbeddedNpcEquipment",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEmbeddedNpcInventoryWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("embedded NPC inventory fixture starts the owned roster"), Subsystem->StartGame());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	const FName WeaponId = CreateInventoryWidgetEquipment(*this, State, EGameXXKEquipmentSlot::Weapon);
	const FName TusiChiefId(TEXT("Npc.TusiChief"));

	UGameXXKInventoryWindowWidget* Inventory = NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(TusiChiefId);
	Inventory->TakeWidget();
	TestTrue(TEXT("embedded NPC inventory opens"), Inventory->OpenFreeInventoryForTest());
	TestEqual(TEXT("embedded inventory keeps the configured named NPC owner"),
		Inventory->GetConfiguredCharacterIdForTest(), TusiChiefId);
	TestTrue(TEXT("right-click equips the NPC instead of the hero"), Inventory->QuickEquipBackpackInstanceForTest(WeaponId));
	TestEqual(TEXT("NPC equipment slot reads the NPC loadout"),
		Inventory->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon), WeaponId);
	TestTrue(TEXT("hero loadout remains untouched by NPC equipment"),
		!State.EquipmentCollection.CharacterLoadouts.Contains(FGameXXKEquipmentRules::HeroCharacterId()));
	const FGameXXKEquipmentInstance* Equipped = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, WeaponId);
	TestNotNull(TEXT("NPC equipped instance remains authoritative"), Equipped);
	if (Equipped)
	{
		TestEqual(TEXT("embedded NPC equip writes the QuestNpc owner kind"),
			Equipped->OwnerKind, EGameXXKEquipmentOwnerKind::QuestNpc);
	}
	TestTrue(TEXT("embedded NPC opens the shared card-deck tab"),
		Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));
	const TArray<FName> NpcCardPool = Inventory->GetHeroCardBackpackIdsForTest();
	TArray<FName> PendingNpcCards = Inventory->GetPendingHeroDeckIdsForTest();
	TestEqual(TEXT("NPC card editor exposes the NPC's four fixed cards"), NpcCardPool.Num(), 4);
	TestEqual(TEXT("NPC card editor selects exactly three cards"), PendingNpcCards.Num(), 3);
	if (NpcCardPool.Num() == 4 && PendingNpcCards.Num() == 3)
	{
		const FName RemovedCard = PendingNpcCards[0];
		const FName* AddedCardPtr = NpcCardPool.FindByPredicate([&PendingNpcCards](const FName CardId)
		{
			return !PendingNpcCards.Contains(CardId);
		});
		const FName AddedCard = AddedCardPtr ? *AddedCardPtr : NAME_None;
		TestTrue(TEXT("NPC card editor can remove one selected card"), Inventory->ToggleHeroDeckCardForTest(RemovedCard));
		TestTrue(TEXT("NPC card editor can select the omitted fourth card"), Inventory->ToggleHeroDeckCardForTest(AddedCard));
		TestTrue(TEXT("NPC card editor applies the edited three-card deck"), Inventory->ApplyHeroDeckForTest());
		const FGameXXKQuestNpcOwnedCardLoadout* SavedLoadout =
			State.CardRun.PartySelection.QuestNpcCardLoadouts.Find(TusiChiefId);
		TestNotNull(TEXT("NPC card editor writes the owned loadout map"), SavedLoadout);
		if (SavedLoadout)
		{
			TestTrue(TEXT("edited NPC deck contains the newly selected card"), SavedLoadout->SelectedCardIds.Contains(AddedCard));
			TestFalse(TEXT("edited NPC deck removes the deselected card"), SavedLoadout->SelectedCardIds.Contains(RemovedCard));
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEmbeddedInventorySessionStateTest,
	"GameXXK.MVP.UI.FinalInventory.EmbeddedSessionState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEmbeddedInventorySessionStateTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	TestTrue(TEXT("embedded session fixture starts the owned roster"), Subsystem->StartGame());
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FName TusiChiefId(TEXT("Npc.TusiChief"));

	UGameXXKInventoryWindowWidget* Original = NewObject<UGameXXKInventoryWindowWidget>();
	Original->SetMVPSubsystem(Subsystem);
	Original->ConfigureDesktopTrainingEmbeddedMode(true);
	Original->ConfigureDesktopTrainingCharacter(TusiChiefId);
	Original->TakeWidget();
	TestTrue(TEXT("original embedded inventory opens"), Original->OpenFreeInventoryForTest());
	TestTrue(TEXT("inventory filter changes before hibernation"),
		Original->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Materials));
	TestTrue(TEXT("inventory sort changes before hibernation"), Original->SortInventoryForTest());
	TestTrue(TEXT("deck tab opens before hibernation"),
		Original->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck));

	const TArray<FName> CardPool = Original->GetHeroCardBackpackIdsForTest();
	TArray<FName> PendingCards = Original->GetPendingHeroDeckIdsForTest();
	TestEqual(TEXT("NPC fixture exposes four cards"), CardPool.Num(), 4);
	TestEqual(TEXT("NPC fixture starts with three selected cards"), PendingCards.Num(), 3);
	if (CardPool.Num() != 4 || PendingCards.Num() != 3)
	{
		return false;
	}
	const FName RemovedCard = PendingCards[0];
	const FName* AddedCard = CardPool.FindByPredicate([&PendingCards](const FName CardId)
	{
		return !PendingCards.Contains(CardId);
	});
	TestNotNull(TEXT("NPC fixture has one omitted card"), AddedCard);
	if (!AddedCard)
	{
		return false;
	}
	TestTrue(TEXT("pending edit removes one card"), Original->ToggleHeroDeckCardForTest(RemovedCard));
	TestTrue(TEXT("pending edit adds the omitted card"), Original->ToggleHeroDeckCardForTest(*AddedCard));
	Original->SetBackpackScrollOffsetForTest(137.0f);

	const FGameXXKEmbeddedInventorySessionState Snapshot = Original->CaptureEmbeddedSessionState();
	UGameXXKInventoryWindowWidget* Restored = NewObject<UGameXXKInventoryWindowWidget>();
	Restored->SetMVPSubsystem(Subsystem);
	Restored->ConfigureDesktopTrainingEmbeddedMode(true);
	Restored->TakeWidget();
	TestTrue(TEXT("fresh embedded inventory opens"), Restored->OpenFreeInventoryForTest());
	Restored->RestoreEmbeddedSessionState(Snapshot);

	TestEqual(TEXT("owner restores"), Restored->GetConfiguredCharacterIdForTest(), TusiChiefId);
	TestEqual(TEXT("subpage restores"), Restored->GetActiveCharacterBackpackTabForTest(), EGameXXKCharacterBackpackTab::Deck);
	TestEqual(TEXT("filter restores"), Restored->GetActiveInventoryFilterForTest(), EGameXXKInventoryFilter::Materials);
	TestTrue(TEXT("sort restores"), Restored->IsBackpackSortedForTest());
	TestEqual(TEXT("pending deck edit restores"), Restored->GetPendingHeroDeckIdsForTest(), Snapshot.PendingDeckIds);
	TestTrue(TEXT("scroll offset restores"), FMath::IsNearlyEqual(Restored->GetBackpackScrollOffsetForTest(), 137.0f));
	return true;
}

#endif
