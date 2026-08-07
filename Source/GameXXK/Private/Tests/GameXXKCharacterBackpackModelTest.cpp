#include "Misc/AutomationTest.h"

#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKCharacterBackpackModel.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const TArray<EGameXXKEquipmentSlot> ExpectedSlotOrder = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};

	FName CreateWarehouseItem(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::ShanHe;
		Request.Quality = EGameXXKEquipmentQuality::Rare;
		Request.ItemLevel = 6;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(
			TEXT("character-backpack fixture creates a warehouse equipment instance"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}

	UGameXXKMVPSubsystem* CreateTownSubsystem()
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		Subsystem->GetMutableRuntimeState() = UGameXXKMVPRules::CreateNewGame();
		Subsystem->GetMutableRuntimeState().Screen = EGameXXKScreen::Town;
		return Subsystem;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterBackpackModelSixSlotTest,
	"GameXXK.MVP.UI.CharacterBackpackModel.SixSlotSnapshot",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterBackpackModelSixSlotTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = CreateTownSubsystem();
	FGameXXKCharacterBackpackModel Model;
	Model.Bind(Subsystem, FGameXXKEquipmentRules::HeroCharacterId());

	const TArray<FGameXXKCharacterBackpackSlotView> EmptySnapshot = Model.GetSixSlotSnapshot();
	TestEqual(TEXT("hero backpack always exposes six equipment slots"), EmptySnapshot.Num(), 6);
	for (int32 Index = 0; Index < ExpectedSlotOrder.Num() && Index < EmptySnapshot.Num(); ++Index)
	{
		TestEqual(TEXT("equipment slot order is stable"), EmptySnapshot[Index].Slot, ExpectedSlotOrder[Index]);
		TestTrue(TEXT("new hero loadout starts empty"), EmptySnapshot[Index].EquippedInstanceId.IsNone());
	}

	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	for (const EGameXXKEquipmentSlot Slot : ExpectedSlotOrder)
	{
		const FName InstanceId = CreateWarehouseItem(*this, State, Slot);
		FGameXXKEquipmentTransactionResult Result;
		TestTrue(TEXT("model equips every authoritative slot"), Model.QuickEquip(InstanceId, Result));
		TestTrue(TEXT("quick-equip returns a typed success"), Result.bSucceeded);
	}

	const TArray<FGameXXKCharacterBackpackSlotView> FullSnapshot = Model.GetSixSlotSnapshot();
	TestEqual(TEXT("full hero snapshot still has exactly six slots"), FullSnapshot.Num(), 6);
	for (const FGameXXKCharacterBackpackSlotView& SlotView : FullSnapshot)
	{
		TestFalse(TEXT("each full-loadout slot exposes its real instance id"), SlotView.EquippedInstanceId.IsNone());
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterBackpackModelReplacementAndOwnerTest,
	"GameXXK.MVP.UI.CharacterBackpackModel.ReplacementAndOwner",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterBackpackModelReplacementAndOwnerTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = CreateTownSubsystem();
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	FGameXXKCharacterBackpackModel HeroModel;
	HeroModel.Bind(Subsystem, FGameXXKEquipmentRules::HeroCharacterId());

	const FName FirstWeapon = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Weapon);
	const FName ReplacementWeapon = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Weapon);
	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("hero equips its first weapon"), HeroModel.QuickEquip(FirstWeapon, Result));
	TestTrue(TEXT("hero quick-equip replaces an occupied slot"), HeroModel.QuickEquip(ReplacementWeapon, Result));
	TestTrue(TEXT("replaced weapon returns to the warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(FirstWeapon));
	TestFalse(TEXT("replacement weapon leaves the warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(ReplacementWeapon));
	TestEqual(
		TEXT("replacement weapon is the visible hero slot"),
		HeroModel.GetSixSlotSnapshot()[0].EquippedInstanceId,
		ReplacementWeapon);

	FGameXXKCompanionRecruitResult RecruitedCompanion;
	FString RecruitError;
	TestTrue(
		TEXT("fixture recruits a permanent companion owner"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(
			State.CardRun.CompanionRoster,
			RecruitedCompanion,
			&RecruitError));
	FGameXXKCharacterBackpackModel CompanionModel;
	CompanionModel.Bind(Subsystem, RecruitedCompanion.Companion.InstanceId);
	const FName CompanionHead = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Head);
	TestTrue(TEXT("companion equips through the same model"), CompanionModel.QuickEquip(CompanionHead, Result));
	TestEqual(
		TEXT("companion snapshot owns the equipped head instance"),
		CompanionModel.GetSixSlotSnapshot()[1].EquippedInstanceId,
		CompanionHead);
	TestTrue(
		TEXT("hero head slot remains untouched by companion equipment"),
		HeroModel.GetSixSlotSnapshot()[1].EquippedInstanceId.IsNone());

	TestTrue(TEXT("companion can right-click unequip its head slot"), CompanionModel.QuickUnequip(EGameXXKEquipmentSlot::Head, Result));
	TestTrue(TEXT("unequipped companion head returns to the warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(CompanionHead));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCharacterBackpackModelFailureTest,
	"GameXXK.MVP.UI.CharacterBackpackModel.FailuresDoNotMutate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCharacterBackpackModelFailureTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = CreateTownSubsystem();
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	FGameXXKCharacterBackpackModel Model;
	Model.Bind(Subsystem, FGameXXKEquipmentRules::HeroCharacterId());
	const FName Weapon = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Weapon);

	State.CardRun.bLoadoutLockedForRoute = true;
	FGameXXKEquipmentTransactionResult Result;
	TestFalse(TEXT("route lock blocks quick-equip"), Model.QuickEquip(Weapon, Result));
	TestEqual(TEXT("route lock preserves the typed error"), Result.Error, EGameXXKEquipmentTransactionError::RouteLocked);
	TestTrue(TEXT("blocked item remains in the warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Contains(Weapon));
	TestTrue(TEXT("blocked hero loadout remains empty"), Model.GetSixSlotSnapshot()[0].EquippedInstanceId.IsNone());
	return true;
}

#endif
