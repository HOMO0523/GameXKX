#include "GameXXKMVPRules.h"
#include "GameXXKCharacterStatRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "UI/GameXXKInventoryWindowWidget.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	static int32 InventoryCategoryRank(EGameXXKItemKind Kind)
	{
		switch (Kind)
		{
		case EGameXXKItemKind::Weapon:
		case EGameXXKItemKind::Armor:
		case EGameXXKItemKind::Accessory:
			return 0;
		case EGameXXKItemKind::Consumable:
			return 1;
		case EGameXXKItemKind::Material:
			return 2;
		case EGameXXKItemKind::Task:
			return 3;
		default:
			return 4;
		}
	}

	static FName FindWarehouseInstanceForSlot(
		const FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot)
	{
		for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
		{
			const FGameXXKEquipmentInstance* Instance =
				FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, InstanceId);
			const FGameXXKEquipmentDefinition* Definition = Instance
				? FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId)
				: nullptr;
			if (Definition && Definition->Slot == Slot)
			{
				return InstanceId;
			}
		}
		return NAME_None;
	}

	static void TestHeroMatchesProjection(
		FAutomationTestBase& Test,
		const TCHAR* Context,
		const FGameXXKRuntimeState& State)
	{
		FGameXXKEquipmentLoadoutSnapshot Snapshot;
		FString Error;
		Test.TestTrue(
			FString::Printf(TEXT("%s builds the authoritative loadout projection: %s"), Context, *Error),
			FGameXXKEquipmentRules::BuildLoadoutSnapshot(
				State.EquipmentCollection,
				FGameXXKEquipmentRules::HeroCharacterId(),
				FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel),
				Snapshot,
				&Error));
		Test.TestEqual(FString::Printf(TEXT("%s mirrors attack"), Context), State.PlayerAttack, Snapshot.AttributesBeforeRoute.Attack);
		Test.TestEqual(FString::Printf(TEXT("%s mirrors defense"), Context), State.PlayerDefense, Snapshot.AttributesBeforeRoute.Defense);
		Test.TestEqual(FString::Printf(TEXT("%s mirrors speed"), Context), State.PlayerSpeed, Snapshot.AttributesBeforeRoute.Speed);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKInventoryEnhancementTest,
	"GameXXK.MVP.Inventory.EnhancementAndStorage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInventoryEnhancementTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	const FName LegacyWeapon = UGameXXKMVPRules::ItemWoodenSword();
	const FName LegacyArmor = UGameXXKMVPRules::ItemStarterClothArmor();
	const FName LegacyAccessory = UGameXXKMVPRules::ItemClothTalisman();
	const FName EnhancementStone = UGameXXKMVPRules::ItemEnhancementStone();
	const FName QingshanRouteSeal = UGameXXKMVPRules::ItemQingshanRouteSeal();
	const FName WeaponInstanceId = FindWarehouseInstanceForSlot(State, EGameXXKEquipmentSlot::Weapon);
	const FName ArmorInstanceId = FindWarehouseInstanceForSlot(State, EGameXXKEquipmentSlot::Armor);
	const FName ShoesInstanceId = FindWarehouseInstanceForSlot(State, EGameXXKEquipmentSlot::Shoes);
	TestFalse(TEXT("new game no longer grants the retired legacy wooden sword"),
		UGameXXKMVPRules::GetItemCount(State, LegacyWeapon) > 0);
	TestFalse(TEXT("new game no longer grants the retired legacy cloth armor"),
		UGameXXKMVPRules::GetItemCount(State, LegacyArmor) > 0);
	TestFalse(TEXT("new game no longer grants the retired legacy cloth talisman"),
		UGameXXKMVPRules::GetItemCount(State, LegacyAccessory) > 0);
	TestFalse(TEXT("starter weapon instance resolves"), WeaponInstanceId.IsNone());
	TestFalse(TEXT("starter armor instance resolves"), ArmorInstanceId.IsNone());
	TestFalse(TEXT("starter shoes instance resolves"), ShoesInstanceId.IsNone());
	if (WeaponInstanceId.IsNone() || ArmorInstanceId.IsNone() || ShoesInstanceId.IsNone())
	{
		return false;
	}

	TestEqual(TEXT("new game grants ten enhancement materials"), State.EnhancementMaterial, 10);
	TestEqual(TEXT("new game exposes enhancement material in the backpack"), UGameXXKMVPRules::GetItemCount(State, EnhancementStone), 10);
	TestFalse(TEXT("legacy item-id enhancement does not invent retired starter gear"), UGameXXKMVPRules::CanEnhanceItem(State, LegacyWeapon));
	TestFalse(TEXT("consumables cannot be enhanced"), UGameXXKMVPRules::CanEnhanceItem(State, UGameXXKMVPRules::ItemHealingPowder()));
	TestFalse(TEXT("legacy item-id decomposition does not invent retired starter gear"), UGameXXKMVPRules::CanDecomposeItem(State, LegacyWeapon));
	TestFalse(TEXT("consumables cannot be decomposed"), UGameXXKMVPRules::CanDecomposeItem(State, UGameXXKMVPRules::ItemHealingPowder()));
	TestFalse(TEXT("task items cannot be bought directly"), UGameXXKMVPRules::BuyItem(State, QingshanRouteSeal, 1));
	TestEqual(TEXT("new game equipment schema is one"), State.EquipmentCollection.EquipmentSchemaVersion, 1);
	TestTrue(TEXT("new game collection seed is nonzero"), State.EquipmentCollection.CollectionSeed != 0);
	TestEqual(TEXT("new game starter ordinal advances exactly six times"), State.EquipmentCollection.NextInstanceOrdinal, 6);
	TestEqual(TEXT("new game has exactly six starter warehouse entries"), State.EquipmentCollection.WarehouseInstanceIds.Num(), 6);
	const TArray<FName> ExpectedStarterOrder{
		TEXT("Equipment.Starter.Weapon"),
		TEXT("Equipment.Starter.Head"),
		TEXT("Equipment.Starter.Armor"),
		TEXT("Equipment.Starter.Belt"),
		TEXT("Equipment.Starter.Shoes"),
		TEXT("Equipment.Starter.Accessory")};
	for (int32 Index = 0; Index < ExpectedStarterOrder.Num() && State.EquipmentCollection.WarehouseInstanceIds.IsValidIndex(Index); ++Index)
	{
		const FGameXXKEquipmentInstance* Starter = FGameXXKEquipmentRules::FindInstance(
			State.EquipmentCollection,
			State.EquipmentCollection.WarehouseInstanceIds[Index]);
		TestEqual(TEXT("starter warehouse order is exact"), Starter ? Starter->BaseEquipmentId : NAME_None, ExpectedStarterOrder[Index]);
		TestEqual(TEXT("starter equipment remains unequipped"), Starter ? Starter->OwnerKind : EGameXXKEquipmentOwnerKind::Invalid, EGameXXKEquipmentOwnerKind::Warehouse);
	}

	const int32 BaseAttack = State.PlayerAttack;
	const int32 BaseDefense = State.PlayerDefense;
	const int32 BaseSpeed = State.PlayerSpeed;
	FGameXXKEquipmentTransactionResult EquipmentResult;
	TestTrue(TEXT("starter weapon enhancement spends one material"),
		FGameXXKEquipmentEconomyRules::EnhanceInstance(State, WeaponInstanceId, EquipmentResult));
	const FGameXXKEquipmentInstance* EnhancedWeapon =
		FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, WeaponInstanceId);
	TestEqual(TEXT("starter weapon enhancement level starts at one"), EnhancedWeapon ? EnhancedWeapon->EnhancementLevel : INDEX_NONE, 1);
	TestEqual(TEXT("weapon enhancement consumes one material"), State.EnhancementMaterial, 9);
	TestEqual(TEXT("weapon enhancement consumes one backpack material"), UGameXXKMVPRules::GetItemCount(State, EnhancementStone), 9);
	TestEqual(TEXT("un-equipped weapon enhancement does not change attack"), State.PlayerAttack, BaseAttack);
	TestTrue(TEXT("enhanced starter weapon equips"), FGameXXKEquipmentEconomyRules::Equip(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Weapon,
		WeaponInstanceId,
		EquipmentResult));
	TestTrue(TEXT("enhanced starter weapon increases attack"), State.PlayerAttack > BaseAttack);
	TestHeroMatchesProjection(*this, TEXT("enhanced starter weapon"), State);

	TestTrue(TEXT("starter armor enhancement succeeds"),
		FGameXXKEquipmentEconomyRules::EnhanceInstance(State, ArmorInstanceId, EquipmentResult));
	TestTrue(TEXT("enhanced starter armor equips"), FGameXXKEquipmentEconomyRules::Equip(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Armor,
		ArmorInstanceId,
		EquipmentResult));
	TestTrue(TEXT("enhanced starter armor increases defense"), State.PlayerDefense > BaseDefense);
	TestHeroMatchesProjection(*this, TEXT("enhanced starter armor"), State);

	TestTrue(TEXT("starter shoes enhancement succeeds"),
		FGameXXKEquipmentEconomyRules::EnhanceInstance(State, ShoesInstanceId, EquipmentResult));
	TestTrue(TEXT("enhanced starter shoes equip"), FGameXXKEquipmentEconomyRules::Equip(
		State,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Shoes,
		ShoesInstanceId,
		EquipmentResult));
	TestTrue(TEXT("enhanced starter shoes increase speed"), State.PlayerSpeed > BaseSpeed);
	TestHeroMatchesProjection(*this, TEXT("enhanced starter shoes"), State);

	State.EnhancementMaterial = 20;
	State.Inventory.FindOrAdd(EnhancementStone) = 20;
	const int32 AttackBeforeWeaponCap = State.PlayerAttack;
	while (const FGameXXKEquipmentInstance* WeaponInstance =
		FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, WeaponInstanceId))
	{
		if (WeaponInstance->EnhancementLevel >= FGameXXKEquipmentRules::MaxEnhancementLevel)
		{
			break;
		}
		TestTrue(TEXT("starter weapon can advance toward the cap"),
			FGameXXKEquipmentEconomyRules::EnhanceInstance(State, WeaponInstanceId, EquipmentResult));
	}
	EnhancedWeapon = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, WeaponInstanceId);
	TestEqual(TEXT("enhancement caps at plus ten"), EnhancedWeapon ? EnhancedWeapon->EnhancementLevel : INDEX_NONE, 10);
	TestFalse(TEXT("plus ten weapon cannot consume another material"),
		FGameXXKEquipmentEconomyRules::EnhanceInstance(State, WeaponInstanceId, EquipmentResult));
	TestTrue(TEXT("equipped plus ten starter weapon exceeds its plus-one attack projection"),
		State.PlayerAttack > AttackBeforeWeaponCap);
	TestHeroMatchesProjection(*this, TEXT("plus-ten starter weapon"), State);

	const FGameXXKSaveState SaveState = UGameXXKMVPRules::MakeSaveState(State);
	TestEqual(TEXT("enhancement save uses the current version seven format"), SaveState.SaveVersion, FGameXXKSaveMigration::CurrentSaveVersion);
	FGameXXKRuntimeState Restored;
	FGameXXKSaveMigrationReport RestoreReport;
	TestTrue(TEXT("current enhancement save restores through typed dispatcher"), FGameXXKSaveMigration::TryRestoreRuntimeState(SaveState, Restored, RestoreReport));
	TestEqual(TEXT("save restores enhancement material"), Restored.EnhancementMaterial, State.EnhancementMaterial);
	TestEqual(TEXT("save restores enhancement material backpack item"), UGameXXKMVPRules::GetItemCount(Restored, EnhancementStone), State.EnhancementMaterial);
	const FGameXXKEquipmentInstance* RestoredWeapon =
		FGameXXKEquipmentRules::FindInstance(Restored.EquipmentCollection, WeaponInstanceId);
	TestEqual(TEXT("save restores modern instance enhancement levels"), RestoredWeapon ? RestoredWeapon->EnhancementLevel : INDEX_NONE, 10);

	FGameXXKSaveState VersionTwoSave;
	VersionTwoSave.SaveVersion = 2;
	VersionTwoSave.RuntimeState = FGameXXKRuntimeState();
	VersionTwoSave.RuntimeState.Inventory.Reset();
	VersionTwoSave.RuntimeState.Inventory.Add(LegacyWeapon, 1);
	VersionTwoSave.RuntimeState.EnhancementMaterial = 0;
	VersionTwoSave.RuntimeState.ItemEnhancementLevels.Add(LegacyWeapon, 4);
	FGameXXKRuntimeState MigratedVersionTwoState;
	FGameXXKSaveMigrationReport VersionTwoReport;
	TestTrue(TEXT("version two migration succeeds through typed dispatcher"), FGameXXKSaveMigration::TryRestoreRuntimeState(VersionTwoSave, MigratedVersionTwoState, VersionTwoReport));
	TestEqual(TEXT("version two migration grants default enhancement materials"), MigratedVersionTwoState.EnhancementMaterial, 10);
	TestEqual(TEXT("version two migration adds enhancement material backpack item"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, EnhancementStone), 10);
	TestEqual(TEXT("version two migration clears unsupported enhancement levels"), UGameXXKMVPRules::GetItemEnhancementLevel(MigratedVersionTwoState, LegacyWeapon), 0);
	TestEqual(TEXT("version two migration keeps only explicit legacy weapon"), MigratedVersionTwoState.EquipmentCollection.EquipmentInstances.Num(), 1);
	TestEqual(TEXT("version two migration inherits no starter armor"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, LegacyArmor), 0);
	TestEqual(TEXT("version two migration inherits no starter accessory"), UGameXXKMVPRules::GetItemCount(MigratedVersionTwoState, LegacyAccessory), 0);

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestNotNull(TEXT("inventory test subsystem exists"), Subsystem);
	Subsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	TestTrue(TEXT("inventory task test starts a new game"), Subsystem->StartGame());
	TestTrue(TEXT("inventory task test enters Qingshan"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("inventory task test accepts the Qingshan route quest"), Subsystem->AcceptQuest());
	TestEqual(TEXT("accepting the route quest adds its task item"), Subsystem->GetItemCount(QingshanRouteSeal), 1);
	TestFalse(TEXT("active task item cannot be sold"), Subsystem->CanSellItem(QingshanRouteSeal));
	TestTrue(TEXT("test state can buy a second equipment item"), Subsystem->BuyItem(UGameXXKMVPRules::ItemIronSword(), 1));
	const FName UiStarterWeaponInstanceId = FindWarehouseInstanceForSlot(
		Subsystem->GetRuntimeState(),
		EGameXXKEquipmentSlot::Weapon);
	TestFalse(TEXT("inventory UI fixture resolves its starter weapon instance"), UiStarterWeaponInstanceId.IsNone());
	if (UiStarterWeaponInstanceId.IsNone())
	{
		return false;
	}

	UGameXXKInventoryWindowWidget* InventoryWindow = NewObject<UGameXXKInventoryWindowWidget>();
	InventoryWindow->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("inventory window opens for filter and action tests"), InventoryWindow->OpenFreeInventoryForTest());
	TestEqual(TEXT("inventory starts on all filter"), InventoryWindow->GetActiveInventoryFilterForTest(), EGameXXKInventoryFilter::All);
	const int32 InitialStarterWeaponSlot =
		InventoryWindow->FindBackpackEquipmentInstanceSlotForTest(UiStarterWeaponInstanceId);
	TestTrue(TEXT("backpack exposes the starter weapon instance"), InitialStarterWeaponSlot != INDEX_NONE);
	InventoryWindow->HandleConfiguredSlotClicked(
		EGameXXKInventorySlotSource::PlayerBackpack,
		InitialStarterWeaponSlot,
		NAME_None);
	TestEqual(TEXT("backpack selection starts on the starter weapon instance"),
		InventoryWindow->GetSelectedBackpackSlotIndexForTest(), InitialStarterWeaponSlot);
	TestTrue(TEXT("equipment filter preserves the selected starter weapon"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Equipment));
	const int32 EquipmentFilterStarterWeaponIndex =
		InventoryWindow->FindBackpackEquipmentInstanceSlotForTest(UiStarterWeaponInstanceId);
	TestTrue(TEXT("equipment filter keeps the starter weapon visible"), EquipmentFilterStarterWeaponIndex != INDEX_NONE);
	TestEqual(TEXT("equipment filter rebinds the selected highlight to the starter weapon"),
		InventoryWindow->GetSelectedBackpackSlotIndexForTest(), EquipmentFilterStarterWeaponIndex);
	TestTrue(TEXT("sort preserves the selected starter weapon"), InventoryWindow->SortInventoryForTest());
	const int32 SortedStarterWeaponIndex =
		InventoryWindow->FindBackpackEquipmentInstanceSlotForTest(UiStarterWeaponInstanceId);
	TestTrue(TEXT("sorted backpack keeps the starter weapon visible"), SortedStarterWeaponIndex != INDEX_NONE);
	TestEqual(TEXT("sort rebinds the selected highlight to the starter weapon"),
		InventoryWindow->GetSelectedBackpackSlotIndexForTest(), SortedStarterWeaponIndex);
	const int32 ModernEnhancementMaterialBefore = Subsystem->GetItemCount(EnhancementStone);
	const FGameXXKEquipmentInstance* ModernWeaponBeforeEnhancement =
		FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, UiStarterWeaponInstanceId);
	TestEqual(TEXT("selected modern starter weapon begins unenhanced"),
		ModernWeaponBeforeEnhancement ? ModernWeaponBeforeEnhancement->EnhancementLevel : INDEX_NONE, 0);
	TestTrue(TEXT("main inventory enhancement action upgrades the selected modern equipment instance"),
		InventoryWindow->EnhanceSelectedEquipmentInstanceForTest());
	const FGameXXKEquipmentInstance* ModernWeaponAfterEnhancement =
		FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, UiStarterWeaponInstanceId);
	TestEqual(TEXT("modern inventory enhancement writes the authoritative instance level"),
		ModernWeaponAfterEnhancement ? ModernWeaponAfterEnhancement->EnhancementLevel : INDEX_NONE, 1);
	TestEqual(TEXT("modern inventory enhancement spends one mirrored enhancement stone"),
		Subsystem->GetItemCount(EnhancementStone), ModernEnhancementMaterialBefore - 1);
	TestTrue(TEXT("modern inventory detail refreshes to the new enhancement level"),
		InventoryWindow->GetSelectedDetailTextForTest().ToString().Contains(TEXT("强化 +1")));
	TestTrue(TEXT("selected starter weapon equips before equipped-slot decomposition"), InventoryWindow->ExecuteSelectedPrimaryActionForTest());
	TestEqual(TEXT("starter weapon instance is equipped before equipped-slot decomposition"),
		InventoryWindow->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon),
		UiStarterWeaponInstanceId);
	InventoryWindow->HandleConfiguredSlotClicked(EGameXXKInventorySlotSource::Equipment, 0, FName(TEXT("Weapon")));
	const int32 MaterialBeforeEquippedDecompose = Subsystem->GetItemCount(EnhancementStone);
	const int32 SandBeforeEquippedDecompose = Subsystem->GetRuntimeState().EquipmentCollection.RefinementSand;
	const int32 GoldBeforeEquippedDecompose = Subsystem->GetRuntimeState().PlayerGold;
	TestTrue(TEXT("equipped item can open global decomposition confirmation"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("equipped item decomposition confirmation executes"), InventoryWindow->ConfirmDialogForTest());
	TestNull(TEXT("equipped item decomposition removes the authoritative instance"),
		FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, UiStarterWeaponInstanceId));
	TestTrue(TEXT("equipped item decomposition clears the equipment slot"),
		InventoryWindow->GetEquippedInstanceForSlotForTest(EGameXXKEquipmentSlot::Weapon).IsNone());
	TestEqual(TEXT("each decomposed item grants one enhancement stone"),
		Subsystem->GetItemCount(EnhancementStone), MaterialBeforeEquippedDecompose + 1);
	TestEqual(TEXT("each decomposed item grants one refinement sand"),
		Subsystem->GetRuntimeState().EquipmentCollection.RefinementSand, SandBeforeEquippedDecompose + 1);
	TestEqual(TEXT("each decomposed item grants ten gold"),
		Subsystem->GetRuntimeState().PlayerGold, GoldBeforeEquippedDecompose + 10);
	TestTrue(TEXT("all filter is restored after equipped-slot decomposition"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::All));
	TestTrue(TEXT("equipment filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Equipment));
	for (const FName InstanceId : InventoryWindow->GetVisibleBackpackEquipmentInstanceIdsForTest())
	{
		const FGameXXKEquipmentInstance* Instance =
			FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, InstanceId);
		TestNotNull(TEXT("equipment filter contains only valid equipment instances"), Instance);
	}
	for (const FName ItemId : InventoryWindow->GetVisibleBackpackItemIdsForTest())
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		TestTrue(TEXT("equipment filter contains only equipment"), bFound && InventoryCategoryRank(Def.Kind) == 0);
	}
	TestTrue(TEXT("material filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Materials));
	TestTrue(TEXT("material filter shows the enhancement material"), InventoryWindow->GetVisibleBackpackItemIdsForTest().Contains(EnhancementStone));
	for (const FName ItemId : InventoryWindow->GetVisibleBackpackItemIdsForTest())
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		TestTrue(TEXT("material filter contains only material"), bFound && Def.Kind == EGameXXKItemKind::Material);
	}
	TestTrue(TEXT("material filter can select enhancement material"), InventoryWindow->SelectPlayerBackpackItemForTest(EnhancementStone));
	TestTrue(TEXT("material selection does not offer an invalid primary action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().IsEmpty());
	TestFalse(TEXT("material selection cannot open decomposition"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("task filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::Tasks));
	TestTrue(TEXT("task filter shows active Qingshan task item"), InventoryWindow->GetVisibleBackpackItemIdsForTest().Contains(QingshanRouteSeal));
	TestTrue(TEXT("task filter can select active Qingshan task item"), InventoryWindow->SelectPlayerBackpackItemForTest(QingshanRouteSeal));
	TestTrue(TEXT("task selection does not offer an invalid primary action"), InventoryWindow->GetSelectedPrimaryActionTextForTest().IsEmpty());
	TestFalse(TEXT("task selection cannot open decomposition"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestTrue(TEXT("all filter is selectable"), InventoryWindow->SelectInventoryFilterForTest(EGameXXKInventoryFilter::All));
	TestTrue(TEXT("sort control succeeds"), InventoryWindow->SortInventoryForTest());
	const TArray<FName> SortedItems = InventoryWindow->GetVisibleBackpackItemIdsForTest();
	int32 PreviousRank = INDEX_NONE;
	FString PreviousName;
	for (const FName ItemId : SortedItems)
	{
		bool bFound = false;
		const FGameXXKItemDef Def = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
		const int32 Rank = InventoryCategoryRank(Def.Kind);
		TestTrue(TEXT("sort keeps category order"), PreviousRank == INDEX_NONE || PreviousRank <= Rank);
		TestTrue(TEXT("sort keeps display name order inside a category"), PreviousRank != Rank || PreviousName <= Def.DisplayName.ToString());
		PreviousRank = Rank;
		PreviousName = Def.DisplayName.ToString();
	}

	const FName Powder = UGameXXKMVPRules::ItemHealingPowder();
	TestTrue(TEXT("inventory test buys a consumable before validating decomposition rejection"),
		Subsystem->BuyItem(Powder, 1));
	const int32 GoldBeforeDecompose = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeDecompose = Subsystem->GetItemCount(Powder);
	TestTrue(TEXT("decompose selects a player backpack item"), InventoryWindow->SelectPlayerBackpackItemForTest(Powder));
	TestFalse(TEXT("decompose rejects consumables instead of selling them"), InventoryWindow->RequestSelectedDecomposeForTest());
	TestFalse(TEXT("consumable decompose does not open confirmation"), InventoryWindow->IsConfirmationDialogVisibleForTest());
	TestEqual(TEXT("consumable decompose keeps the item"), Subsystem->GetItemCount(Powder), PowderBeforeDecompose);
	TestEqual(TEXT("consumable decompose keeps gold unchanged"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeDecompose);

	const FName IronSword = UGameXXKMVPRules::ItemIronSword();
	TestTrue(TEXT("inventory can select equipment for enhancement"), InventoryWindow->SelectPlayerBackpackItemForTest(IronSword));
	TestTrue(TEXT("inventory enhancement opens confirmation"), InventoryWindow->RequestSelectedEnhanceForTest());
	TestTrue(TEXT("inventory enhancement confirmation executes"), InventoryWindow->ConfirmDialogForTest());
	TestTrue(TEXT("inventory detail reports enhancement level"), InventoryWindow->GetSelectedDetailTextForTest().ToString().Contains(TEXT("+1 / +10")));

	const FString EnhancementSaveSlot = TEXT("GameXXK_MVP_Automation_InventoryEnhancement");
	const FString EnhancementBackupSlot = EnhancementSaveSlot + TEXT(".PreV7Backup");
	const int32 EnhancementSaveUserIndex = 0;
	UGameplayStatics::DeleteGameInSlot(EnhancementSaveSlot, EnhancementSaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(EnhancementBackupSlot, EnhancementSaveUserIndex);
	UGameInstance* EnhancementSaveSourceGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EnhancementSaveSourceSubsystem = NewObject<UGameXXKMVPSubsystem>(EnhancementSaveSourceGameInstance);
	EnhancementSaveSourceSubsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
	EnhancementSaveSourceSubsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
	const FName SaveWeaponInstanceId = FindWarehouseInstanceForSlot(
		EnhancementSaveSourceSubsystem->GetRuntimeState(),
		EGameXXKEquipmentSlot::Weapon);
	FGameXXKEquipmentTransactionResult SaveEnhancementResult;
	TestFalse(TEXT("save path fixture resolves its modern starter weapon"), SaveWeaponInstanceId.IsNone());
	TestTrue(TEXT("save path enhancement succeeds"),
		EnhancementSaveSourceSubsystem->EnhanceEquipmentInstance(SaveWeaponInstanceId, SaveEnhancementResult));
	TestTrue(TEXT("save path writes enhancement state"), EnhancementSaveSourceSubsystem->SaveCurrentGame(EnhancementSaveSlot, EnhancementSaveUserIndex));
	TestFalse(TEXT("current v7 save creates no migration backup"), UGameplayStatics::DoesSaveGameExist(EnhancementBackupSlot, EnhancementSaveUserIndex));
	UGameInstance* EnhancementSaveLoadGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* EnhancementSaveLoadSubsystem = NewObject<UGameXXKMVPSubsystem>(EnhancementSaveLoadGameInstance);
	TestTrue(TEXT("save path reloads enhancement state"), EnhancementSaveLoadSubsystem->LoadGameFromSlot(EnhancementSaveSlot, EnhancementSaveUserIndex));
	TestEqual(TEXT("real SaveGame reload preserves enhancement material"), EnhancementSaveLoadSubsystem->GetRuntimeState().EnhancementMaterial, 9);
	TestEqual(TEXT("real SaveGame reload preserves enhancement material backpack item"), EnhancementSaveLoadSubsystem->GetItemCount(EnhancementStone), 9);
	const FGameXXKEquipmentInstance* ReloadedSaveWeapon = FGameXXKEquipmentRules::FindInstance(
		EnhancementSaveLoadSubsystem->GetRuntimeState().EquipmentCollection,
		SaveWeaponInstanceId);
	TestEqual(TEXT("real SaveGame reload preserves modern instance enhancement level"),
		ReloadedSaveWeapon ? ReloadedSaveWeapon->EnhancementLevel : INDEX_NONE,
		1);
	UGameplayStatics::DeleteGameInSlot(EnhancementSaveSlot, EnhancementSaveUserIndex);
	UGameplayStatics::DeleteGameInSlot(EnhancementBackupSlot, EnhancementSaveUserIndex);
	TestTrue(TEXT("selling the final enhanced copy succeeds"), Subsystem->SellItem(IronSword, 1));
	TestEqual(TEXT("selling the final enhanced copy clears its enhancement state"), Subsystem->GetItemEnhancementLevel(IronSword), 0);

	return true;
}

#endif
