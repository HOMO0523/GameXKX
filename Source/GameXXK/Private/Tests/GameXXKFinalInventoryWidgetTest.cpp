#include "Misc/AutomationTest.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKBattleAnimationPresentation.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Image.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/TextBlock.h"
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

	void TestInventoryLockedOverlay(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		UImage* LockedIcon)
	{
		if (!Test.TestNotNull(Context, LockedIcon))
		{
			return;
		}
		const UObject* Resource = LockedIcon->GetBrush().GetResourceObject();
		Test.TestTrue(
			*FString::Printf(TEXT("%s uses the approved locked-card texture"), Context),
			Resource && Resource->GetPathName().Contains(
				TEXT("/Game/GameXXK/UI/MasterV2/Approved/T_MasterV2_CardLockedIcon")));
		Test.TestEqual(
			*FString::Printf(TEXT("%s is hit-test-invisible"), Context),
			LockedIcon->GetVisibility(),
			ESlateVisibility::HitTestInvisible);
		const UOverlaySlot* OverlaySlot = Cast<UOverlaySlot>(LockedIcon->Slot);
		if (Test.TestNotNull(
			*FString::Printf(TEXT("%s owns overlay geometry"), Context),
			OverlaySlot))
		{
			Test.TestEqual(
				*FString::Printf(TEXT("%s is right aligned"), Context),
				OverlaySlot->GetHorizontalAlignment(),
				HAlign_Right);
			Test.TestEqual(
				*FString::Printf(TEXT("%s is top aligned"), Context),
				OverlaySlot->GetVerticalAlignment(),
				VAlign_Top);
		}
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
	FGameXXKFinalInventoryLockOverlayTest,
	"GameXXK.MVP.UI.FinalInventory.PersistentLockOverlays",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryLockOverlayTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	const FName EquippedWeapon = CreateInventoryWidgetEquipment(
		*this, State, EGameXXKEquipmentSlot::Weapon);
	FGameXXKEquipmentTransactionResult EquipResult;
	if (!TestTrue(TEXT("lock-overlay fixture equips a real weapon"),
		Subsystem->EquipEquipmentInstance(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EquippedWeapon,
			EquipResult))
		|| !TestTrue(TEXT("lock-overlay fixture normalizes"),
			Subsystem->NormalizeDesktopInventoryState()))
	{
		return false;
	}

	const FName StoneId = UGameXXKMVPRules::ItemEnhancementStone();
	const FGameXXKDesktopInventoryEntryKey StoneEntry =
		FGameXXKDesktopInventoryRules::MakeItemEntry(StoneId);
	const FGameXXKDesktopInventoryEntryKey WeaponEntry =
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquippedWeapon);
	FString Error;
	TestTrue(TEXT("fixture locks the whole item stack"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State, StoneEntry, true, &Error));
	TestTrue(TEXT("fixture locks the equipped instance"),
		FGameXXKDesktopInventoryRules::SetEntryLocked(
			State, WeaponEntry, true, &Error));

	UGameXXKInventoryWindowWidget* Inventory =
		NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("lock-overlay final inventory opens"),
		Inventory->OpenFreeInventoryForTest()))
	{
		return false;
	}
	const int32 StoneDisplaySlot = Inventory->FindBackpackItemSlotForTest(StoneId);
	if (!TestTrue(TEXT("locked stack has a visible Backpack cell"),
		StoneDisplaySlot != INDEX_NONE))
	{
		return false;
	}
	UImage* BackpackLockedIcon = Inventory->WidgetTree
		? Cast<UImage>(Inventory->WidgetTree->FindWidget(
			*FString::Printf(
				TEXT("InventoryBackpackLockedIcon_%03d"),
				StoneDisplaySlot)))
		: nullptr;
	UImage* EquipmentLockedIcon = Inventory->WidgetTree
		? Cast<UImage>(Inventory->WidgetTree->FindWidget(
			TEXT("InventoryEquipmentLockedIcon_Weapon")))
		: nullptr;
	TestInventoryLockedOverlay(
		*this, TEXT("Backpack lock overlay"), BackpackLockedIcon);
	TestInventoryLockedOverlay(
		*this, TEXT("Equipment lock overlay"), EquipmentLockedIcon);

	UGameXXKInventorySlotButton* BackpackButton = Inventory->WidgetTree
		? Cast<UGameXXKInventorySlotButton>(Inventory->WidgetTree->FindWidget(
			*FString::Printf(TEXT("InventoryBackpackSlot_%02d"), StoneDisplaySlot)))
		: nullptr;
	if (TestNotNull(TEXT("locked Backpack cell owns a real button"), BackpackButton))
	{
		BackpackButton->OnClicked.Broadcast();
		TestTrue(TEXT("ordinary locked-cell OnClicked keeps select/detail behavior"),
			Inventory->GetSelectedBackpackSlotIndexForTest() != INDEX_NONE);
	}
	UGameXXKInventorySlotButton* EquipmentButton = Inventory->WidgetTree
		? Cast<UGameXXKInventorySlotButton>(Inventory->WidgetTree->FindWidget(
			TEXT("InventoryEquipmentSlot_Weapon")))
		: nullptr;
	if (TestNotNull(TEXT("locked Equipment cell owns a real button"), EquipmentButton))
	{
		EquipmentButton->OnClicked.Broadcast();
		TestFalse(TEXT("ordinary equipped-cell OnClicked exposes detail"),
			Inventory->GetSelectedDetailTextForTest().IsEmpty());
	}

	TestTrue(TEXT("explicit Backpack Alt seam unlocks the whole item stack"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::PlayerBackpack,
			StoneDisplaySlot,
			NAME_None));
	TestFalse(TEXT("whole item stack is unlocked by item ID"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, StoneEntry));
	TestTrue(TEXT("explicit Backpack Alt seam relocks the whole item stack"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::PlayerBackpack,
			StoneDisplaySlot,
			NAME_None));
	TestTrue(TEXT("explicit Equipment Alt seam unlocks the equipped instance"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::Equipment,
			0,
			TEXT("Weapon")));
	TestFalse(TEXT("equipped instance is unlocked by stable ID"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, WeaponEntry));
	TestTrue(TEXT("explicit Equipment Alt seam relocks the equipped instance"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::Equipment,
			0,
			TEXT("Weapon")));
	TestFalse(TEXT("empty cell lock is rejected"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::PlayerBackpack,
			FGameXXKDesktopInventoryRules::BackpackCapacity - 1,
			NAME_None));
	TestFalse(TEXT("unknown cell lock is rejected"),
		Inventory->HandleConfiguredSlotAltClicked(
			EGameXXKInventorySlotSource::None,
			INDEX_NONE,
			NAME_None));
	TestTrue(TEXT("rejected locks preserve the valid item lock"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, StoneEntry));
	TestTrue(TEXT("rejected locks preserve the valid equipment lock"),
		FGameXXKDesktopInventoryRules::IsEntryLocked(State, WeaponEntry));
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalInventoryOwnerCentralArtTest,
	"GameXXK.MVP.UI.FinalInventory.OwnerCentralArt",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryOwnerCentralArtTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("owner-art fixture starts the thirteen-owner roster"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FGameXXKPermanentCompanion* Guard =
		Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId.ToString().Contains(TEXT("Companion_Guard_"));
			});
	if (!TestNotNull(TEXT("owner-art fixture owns the Guard companion"), Guard))
	{
		return false;
	}
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const FName GuardId = Guard->InstanceId;
	const FName YueBaiId(TEXT("Npc.YueBai"));

	UGameXXKInventoryWindowWidget* Inventory =
		NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(HeroId);
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("owner-art embedded inventory opens"),
		Inventory->OpenFreeInventoryForTest()))
	{
		return false;
	}
	UImage* CentralImage = Inventory->WidgetTree
		? Cast<UImage>(Inventory->WidgetTree->FindWidget(TEXT("InventoryCentralHeroIdle")))
		: nullptr;
	if (!TestNotNull(TEXT("owner-art inventory owns the central character image"),
		CentralImage))
	{
		return false;
	}

	const FString HeroResourcePath = CentralImage->GetBrush().GetResourceObject()
		? CentralImage->GetBrush().GetResourceObject()->GetPathName()
		: FString();
	const FBox2f HeroUv = CentralImage->GetBrush().GetUVRegion();
	TestTrue(TEXT("Hero keeps the approved full-body texture"),
		HeroResourcePath.Contains(TEXT("T_MasterV2_HeroFullBody")));

	Inventory->ConfigureDesktopTrainingCharacter(GuardId);
	const FString GuardExpectedTexturePath(
		TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_02_guard_2k_idle_atlas.T_character_02_guard_2k_idle_atlas"));
	const FBox2f ExpectedFrameZeroUv(
		FVector2f(0.0f, 0.0f),
		FVector2f(0.125f, 0.125f));
	const FString GuardResourcePath = CentralImage->GetBrush().GetResourceObject()
		? CentralImage->GetBrush().GetResourceObject()->GetPathName()
		: FString();
	const FBox2f GuardUv = CentralImage->GetBrush().GetUVRegion();
	TestEqual(TEXT("Guard central art uses the authored 2K Idle atlas"),
		GuardResourcePath, GuardExpectedTexturePath);
	TestTrue(TEXT("Guard central art uses frame-zero UV"),
		GuardUv.Min.Equals(ExpectedFrameZeroUv.Min, 0.0001f)
		&& GuardUv.Max.Equals(ExpectedFrameZeroUv.Max, 0.0001f));
	TestTrue(TEXT("Guard central art replaces the Hero resource"),
		GuardResourcePath != HeroResourcePath);
	TestTrue(TEXT("Guard atlas UV replaces the full-body UV"),
		!GuardUv.Min.Equals(HeroUv.Min, 0.0001f)
		|| !GuardUv.Max.Equals(HeroUv.Max, 0.0001f));
	TestEqual(TEXT("central owner art keeps a bottom-center pivot"),
		CentralImage->GetRenderTransformPivot(), FVector2D(0.5f, 1.0f));
	TestTrue(TEXT("valid Guard art is opaque"),
		FMath::IsNearlyEqual(CentralImage->GetRenderOpacity(), 1.0f));

	Inventory->ConfigureDesktopTrainingCharacter(YueBaiId);
	const FString YueBaiExpectedTexturePath(
		TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_character_09_yue_bai_2k_idle_atlas.T_character_09_yue_bai_2k_idle_atlas"));
	const FString YueBaiResourcePath = CentralImage->GetBrush().GetResourceObject()
		? CentralImage->GetBrush().GetResourceObject()->GetPathName()
		: FString();
	const FBox2f YueBaiUv = CentralImage->GetBrush().GetUVRegion();
	TestEqual(TEXT("Yue Bai central art uses the authored 2K Idle atlas"),
		YueBaiResourcePath, YueBaiExpectedTexturePath);
	TestTrue(TEXT("Yue Bai central art uses frame-zero UV"),
		YueBaiUv.Min.Equals(ExpectedFrameZeroUv.Min, 0.0001f)
		&& YueBaiUv.Max.Equals(ExpectedFrameZeroUv.Max, 0.0001f));
	TestTrue(TEXT("Yue Bai replaces the Guard atlas resource"),
		YueBaiResourcePath != GuardResourcePath);

	Inventory->ConfigureDesktopTrainingCharacter(TEXT("Character.Unknown"));
	TestNull(TEXT("invalid owner clears the previous atlas resource"),
		CentralImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("invalid owner clears the previous atlas opacity"),
		FMath::IsNearlyZero(CentralImage->GetRenderOpacity()));

	Inventory->ConfigureDesktopTrainingCharacter(HeroId);
	const FString RestoredHeroResourcePath = CentralImage->GetBrush().GetResourceObject()
		? CentralImage->GetBrush().GetResourceObject()->GetPathName()
		: FString();
	const FBox2f RestoredHeroUv = CentralImage->GetBrush().GetUVRegion();
	TestEqual(TEXT("switching back restores the same Hero resource"),
		RestoredHeroResourcePath, HeroResourcePath);
	TestTrue(TEXT("switching back restores the same Hero UV"),
		RestoredHeroUv.Min.Equals(HeroUv.Min, 0.0001f)
		&& RestoredHeroUv.Max.Equals(HeroUv.Max, 0.0001f));
	TestTrue(TEXT("switching back restores Hero opacity"),
		FMath::IsNearlyEqual(CentralImage->GetRenderOpacity(), 1.0f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalInventoryMissingCentralAtlasTest,
	"GameXXK.MVP.UI.FinalInventory.MissingCentralAtlasClearsPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryMissingCentralAtlasTest::RunTest(
	const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("missing-atlas fixture starts the owned roster"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FGameXXKPermanentCompanion* Guard =
		Subsystem->GetRuntimeState().CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId.ToString().Contains(TEXT("Companion_Guard_"));
			});
	if (!TestNotNull(TEXT("missing-atlas fixture owns Guard"), Guard))
	{
		return false;
	}

	UGameXXKInventoryWindowWidget* Inventory =
		NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(Guard->InstanceId);
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("missing-atlas embedded inventory opens"),
		Inventory->OpenFreeInventoryForTest()))
	{
		return false;
	}
	UImage* CentralImage = Inventory->WidgetTree
		? Cast<UImage>(Inventory->WidgetTree->FindWidget(
			TEXT("InventoryCentralHeroIdle")))
		: nullptr;
	if (!TestNotNull(TEXT("missing-atlas fixture owns central art"), CentralImage))
	{
		return false;
	}
	const UObject* PreviousResource = CentralImage->GetBrush().GetResourceObject();
	const FBox2f PreviousUv = CentralImage->GetBrush().GetUVRegion();
	if (!TestNotNull(TEXT("missing-atlas fixture starts from loaded Guard art"),
		PreviousResource))
	{
		return false;
	}

	FGameXXKBattleAnimationClipDescriptor MissingClip;
	MissingClip.AssetId = TEXT("character_missing_owner_2k_idle");
	MissingClip.TexturePath = FSoftObjectPath(
		TEXT("/Game/GameXXK/BattleAnimations/Atlases/T_MissingCentralOwnerAtlasForTest.T_MissingCentralOwnerAtlasForTest"));
	MissingClip.FrameCount = 60;
	TestTrue(TEXT("missing-atlas seam receives a structurally valid descriptor"),
		MissingClip.IsValid());
	AddExpectedError(
		TEXT("T_MissingCentralOwnerAtlasForTest"),
		EAutomationExpectedErrorFlags::Contains,
		1);
	Inventory->RefreshCentralCharacterPresentationFromClipForTest(MissingClip);

	TestNull(TEXT("missing descriptor path clears the previous resource"),
		CentralImage->GetBrush().GetResourceObject());
	TestTrue(TEXT("missing descriptor path clears presentation opacity"),
		FMath::IsNearlyZero(CentralImage->GetRenderOpacity()));
	const FBox2f ClearedUv = CentralImage->GetBrush().GetUVRegion();
	TestTrue(TEXT("missing descriptor path clears the previous frame UV"),
		!ClearedUv.Min.Equals(PreviousUv.Min, 0.0001f)
		|| !ClearedUv.Max.Equals(PreviousUv.Max, 0.0001f));
	TestTrue(TEXT("missing descriptor path never retains the old resource"),
		CentralImage->GetBrush().GetResourceObject() != PreviousResource);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKFinalInventoryLegacyOwnerIsolationTest,
	"GameXXK.MVP.UI.FinalInventory.LegacyEquipmentOwnerIsolation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKFinalInventoryLegacyOwnerIsolationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem =
		NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("legacy-owner fixture starts the thirteen-owner roster"),
		Subsystem && Subsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.Screen = EGameXXKScreen::Town;
	State.EquippedWeapon = UGameXXKMVPRules::ItemIronSword();
	const FGameXXKPermanentCompanion* Guard =
		State.CardRun.CompanionRoster.PermanentCompanions.FindByPredicate(
			[](const FGameXXKPermanentCompanion& Candidate)
			{
				return Candidate.InstanceId.ToString().Contains(TEXT("Companion_Guard_"));
			});
	if (!TestNotNull(TEXT("legacy-owner fixture owns the Guard companion"), Guard))
	{
		return false;
	}

	UGameXXKInventoryWindowWidget* Inventory =
		NewObject<UGameXXKInventoryWindowWidget>();
	Inventory->SetMVPSubsystem(Subsystem);
	Inventory->ConfigureDesktopTrainingEmbeddedMode(true);
	Inventory->ConfigureDesktopTrainingCharacter(
		FGameXXKEquipmentRules::HeroCharacterId());
	Inventory->TakeWidget();
	if (!TestTrue(TEXT("legacy-owner embedded inventory opens"),
		Inventory->OpenFreeInventoryForTest()))
	{
		return false;
	}
	UGameXXKInventorySlotButton* WeaponButton = Inventory->WidgetTree
		? Cast<UGameXXKInventorySlotButton>(Inventory->WidgetTree->FindWidget(
			TEXT("InventoryEquipmentSlot_Weapon")))
		: nullptr;
	UOverlay* WeaponOverlay = WeaponButton
		? Cast<UOverlay>(WeaponButton->GetContent())
		: nullptr;
	UImage* WeaponIcon = WeaponOverlay && WeaponOverlay->GetChildrenCount() > 0
		? Cast<UImage>(WeaponOverlay->GetChildAt(0))
		: nullptr;
	UTextBlock* WeaponLabel = WeaponOverlay && WeaponOverlay->GetChildrenCount() > 1
		? Cast<UTextBlock>(WeaponOverlay->GetChildAt(1))
		: nullptr;
	if (!TestNotNull(TEXT("legacy-owner fixture owns the weapon icon"), WeaponIcon)
		|| !TestNotNull(TEXT("legacy-owner fixture owns the weapon label"), WeaponLabel))
	{
		return false;
	}
	TestNotNull(TEXT("Hero may render the top-level legacy weapon mirror"),
		WeaponIcon->GetBrush().GetResourceObject());
	TestEqual(TEXT("Hero legacy weapon icon is visible"),
		WeaponIcon->GetVisibility(), ESlateVisibility::HitTestInvisible);

	Inventory->ConfigureDesktopTrainingCharacter(Guard->InstanceId);
	TestEqual(TEXT("Guard keeps an empty weapon-slot label"),
		WeaponLabel->GetText().ToString(), FString(TEXT("武器")));
	TestEqual(TEXT("Guard empty weapon slot hides its icon"),
		WeaponIcon->GetVisibility(), ESlateVisibility::Collapsed);
	TestNull(TEXT("Guard empty weapon slot clears the Hero icon resource"),
		WeaponIcon->GetBrush().GetResourceObject());

	Inventory->ConfigureDesktopTrainingCharacter(TEXT("Npc.YueBai"));
	TestEqual(TEXT("Yue Bai keeps an empty weapon-slot label"),
		WeaponLabel->GetText().ToString(), FString(TEXT("武器")));
	TestEqual(TEXT("Yue Bai empty weapon slot hides its icon"),
		WeaponIcon->GetVisibility(), ESlateVisibility::Collapsed);
	TestNull(TEXT("Yue Bai empty weapon slot keeps the stale Hero icon cleared"),
		WeaponIcon->GetBrush().GetResourceObject());
	return true;
}

#endif
