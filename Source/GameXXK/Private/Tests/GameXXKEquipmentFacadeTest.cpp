#include "Misc/AutomationTest.h"

#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& State)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	void TestRuntimeUnchanged(
		FAutomationTestBase& Test,
		const FString& Label,
		const TArray<uint8>& Before,
		const FGameXXKRuntimeState& After)
	{
		Test.TestEqual(Label, SerializeRuntimeState(After), Before);
	}

	FName CreateWarehouseItem(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentSlot Slot,
		const EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::ShanHe,
		const EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Rare,
		const int32 ItemLevel = 6)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = Set;
		Request.Quality = Quality;
		Request.ItemLevel = ItemLevel;
		Request.bForceSlot = true;
		Request.ForcedSlot = Slot;

		FName InstanceId;
		FString Error;
		if (!Test.TestTrue(TEXT("facade fixture creates a warehouse weapon"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error)))
		{
			Test.AddError(Error);
		}
		return InstanceId;
	}

	FName CreateWarehouseWeapon(FAutomationTestBase& Test, FGameXXKRuntimeState& State)
	{
		return CreateWarehouseItem(Test, State, EGameXXKEquipmentSlot::Weapon);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadeReadSnapshotTest,
	"GameXXK.Equipment.Facade.ReadSnapshots",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadeReadSnapshotTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	const FName WeaponId = CreateWarehouseWeapon(*this, State);
	if (WeaponId.IsNone())
	{
		return false;
	}

	TArray<FName> WarehouseIds;
	TestTrue(TEXT("facade exposes the exact ordered warehouse instance IDs"),
		Subsystem->GetEquipmentWarehouseSnapshot(WarehouseIds));
	TestEqual(TEXT("facade preserves authoritative warehouse order"), WarehouseIds, State.EquipmentCollection.WarehouseInstanceIds);

	FGameXXKEquipmentLoadoutSnapshot HeroLoadout;
	TestTrue(TEXT("facade builds the hero loadout from authoritative naked stats"),
		Subsystem->GetEquipmentLoadoutSnapshot(FGameXXKEquipmentRules::HeroCharacterId(), HeroLoadout));
	const FGameXXKCharacterStats ExpectedHeroBare = FGameXXKCharacterStatRules::GetBareHeroStats(State.PlayerLevel);
	TestEqual(TEXT("hero facade snapshot carries the authoritative naked maximum health"), HeroLoadout.BareStats.MaxHealth, ExpectedHeroBare.MaxHealth);
	TestEqual(TEXT("hero facade snapshot carries the authoritative naked speed"), HeroLoadout.BareStats.Speed, ExpectedHeroBare.Speed);

	FGameXXKCompanionRecruitResult RecruitedCompanion;
	FString RecruitError;
	TestTrue(TEXT("facade companion snapshot fixture recruits an authoritative permanent companion"),
		FGameXXKCompanionRules::CreateAndResolveNextRecruitment(State.CardRun.CompanionRoster, RecruitedCompanion, &RecruitError));
	FGameXXKEquipmentLoadoutSnapshot CompanionLoadout;
	TestTrue(TEXT("facade resolves permanent-companion bare stats rather than the hero stats"),
		Subsystem->GetEquipmentLoadoutSnapshot(RecruitedCompanion.Companion.InstanceId, CompanionLoadout));
	FGameXXKCharacterStats ExpectedCompanionBare;
	TestTrue(TEXT("companion fixture exposes valid canonical bare stats"),
		FGameXXKCharacterStatRules::GetBareCompanionStats(
			RecruitedCompanion.Companion.Role,
			RecruitedCompanion.Companion.Level,
			RecruitedCompanion.Companion.Star,
			ExpectedCompanionBare));
	TestEqual(TEXT("companion facade snapshot carries the authoritative naked maximum health"), CompanionLoadout.BareStats.MaxHealth, ExpectedCompanionBare.MaxHealth);
	TestEqual(TEXT("companion facade snapshot carries the authoritative naked speed"), CompanionLoadout.BareStats.Speed, ExpectedCompanionBare.Speed);

	FGameXXKEquipmentTooltipSnapshot Tooltip;
	TestTrue(TEXT("facade builds a same-character equipment comparison tooltip"),
		Subsystem->GetEquipmentTooltipSnapshot(WeaponId, FGameXXKEquipmentRules::HeroCharacterId(), Tooltip));
	TestEqual(TEXT("facade tooltip identifies the requested instance"), Tooltip.InstanceId, WeaponId);
	TestEqual(TEXT("facade tooltip evaluates the candidate weapon slot"), Tooltip.Slot, EGameXXKEquipmentSlot::Weapon);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadeMutationTest,
	"GameXXK.Equipment.Facade.Mutations",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadeMutationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemEnhancementStone()) = 10;
	State.EnhancementMaterial = 10;
	State.EquipmentCollection.RefinementSand = 30;
	const FName WeaponId = CreateWarehouseWeapon(*this, State);
	if (WeaponId.IsNone())
	{
		return false;
	}

	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("facade equips an item through the complete runtime transaction"),
		Subsystem->EquipEquipmentInstance(
			FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, WeaponId, Result));
	TestTrue(TEXT("facade equip reports a typed success result"), Result.bSucceeded && Result.Error == EGameXXKEquipmentTransactionError::None);

	TestTrue(TEXT("facade enhances an equipped item through the complete runtime transaction"),
		Subsystem->EnhanceEquipmentInstance(WeaponId, Result));
	TestEqual(TEXT("facade enhancement returns the authoritative stone delta"), Result.EnhancementStoneDelta, -1);

	TestTrue(TEXT("facade starts a persisted reforge preview"),
		Subsystem->BeginEquipmentReforge(WeaponId, 0, Result));
	TestTrue(TEXT("facade keeps the pending reforge visible in the authoritative save state"),
		State.EquipmentCollection.PendingReforge.bActive);
	TestTrue(TEXT("facade resolves a persisted reforge preview"),
		Subsystem->ResolveEquipmentReforge(false, Result));
	TestFalse(TEXT("facade resolution clears the pending reforge"), State.EquipmentCollection.PendingReforge.bActive);

	TestTrue(TEXT("facade unequips through the complete runtime transaction"),
		Subsystem->UnequipEquipmentSlot(FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, Result));
	TestTrue(TEXT("facade dismantles warehouse instances through the complete runtime transaction"),
		Subsystem->DismantleEquipmentInstances({WeaponId}, true, Result));
	TestNull(TEXT("facade dismantle removes the authoritative equipment instance"),
		FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, WeaponId));

	const FName NpcRejectedWeapon = CreateWarehouseWeapon(*this, State);
	const TArray<uint8> BeforeNpcEquip = SerializeRuntimeState(State);
	TestFalse(TEXT("facade rejects a task NPC as an equipment owner"),
		Subsystem->EquipEquipmentInstance(TEXT("Npc.TusiChief"), EGameXXKEquipmentSlot::Weapon, NpcRejectedWeapon, Result));
	TestEqual(TEXT("task-NPC equipment rejection has the stable typed error"), Result.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestEqual(TEXT("task-NPC equipment rejection uses the stable Chinese message"), Result.Message.ToString(), TEXT("该角色不能持有装备"));
	TestRuntimeUnchanged(*this, TEXT("task-NPC equipment rejection preserves the complete runtime state"), BeforeNpcEquip, State);

	State.CardRun.bLoadoutLockedForRoute = true;
	const TArray<uint8> BeforeRouteLockedMutation = SerializeRuntimeState(State);
	TestFalse(TEXT("facade blocks equipment mutations while the route loadout is locked"),
		Subsystem->EnhanceEquipmentInstance(NpcRejectedWeapon, Result));
	TestEqual(TEXT("route-locked equipment mutation has the stable typed error"), Result.Error, EGameXXKEquipmentTransactionError::RouteLocked);
	TestRuntimeUnchanged(*this, TEXT("route-locked equipment mutation preserves the complete runtime state"), BeforeRouteLockedMutation, State);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadeFullLoadoutTest,
	"GameXXK.Equipment.Facade.FullLoadoutAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadeFullLoadoutTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;

	const TArray<EGameXXKEquipmentSlot> Slots = {
		EGameXXKEquipmentSlot::Weapon,
		EGameXXKEquipmentSlot::Head,
		EGameXXKEquipmentSlot::Armor,
		EGameXXKEquipmentSlot::Belt,
		EGameXXKEquipmentSlot::Shoes,
		EGameXXKEquipmentSlot::Accessory};
	FGameXXKEquipmentTransactionResult Result;
	for (const EGameXXKEquipmentSlot Slot : Slots)
	{
		const FName InstanceId = CreateWarehouseItem(*this, State, Slot);
		if (InstanceId.IsNone())
		{
			return false;
		}
		TestTrue(TEXT("facade equips each slot of a complete six-piece loadout"),
			Subsystem->EquipEquipmentInstance(FGameXXKEquipmentRules::HeroCharacterId(), Slot, InstanceId, Result));
	}

	FGameXXKEquipmentLoadoutSnapshot FullLoadout;
	TestTrue(TEXT("facade reads a complete hero loadout"),
		Subsystem->GetEquipmentLoadoutSnapshot(FGameXXKEquipmentRules::HeroCharacterId(), FullLoadout));
	TestEqual(TEXT("complete loadout contains all six ShanHe pieces"), FullLoadout.SetPieceCounts.FindRef(EGameXXKEquipmentSet::ShanHe), 6);

	const FName CandidateWeapon = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Weapon);
	FGameXXKEquipmentTooltipSnapshot Tooltip;
	TestTrue(TEXT("facade tooltip compares a warehouse candidate against the full equipment loadout"),
		Subsystem->GetEquipmentTooltipSnapshot(CandidateWeapon, FGameXXKEquipmentRules::HeroCharacterId(), Tooltip));
	TestEqual(TEXT("tooltip current stats are the full authoritative loadout projection"), Tooltip.CurrentCharacterStats.Attack, FullLoadout.AttributesBeforeRoute.Attack);
	TestEqual(TEXT("tooltip current set count is the full six-piece loadout"), Tooltip.CurrentSetPieceCounts.FindRef(EGameXXKEquipmentSet::ShanHe), 6);
	TestEqual(TEXT("tooltip candidate keeps a complete six-piece replacement loadout"), Tooltip.CandidateSetPieceCounts.FindRef(EGameXXKEquipmentSet::ShanHe), 6);
	TestEqual(TEXT("tooltip candidate is equipable in the current slot"), Tooltip.EquipError, EGameXXKEquipmentTransactionError::None);

	UGameXXKMVPSubsystem* CapacitySubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& CapacityState = CapacitySubsystem->GetMutableRuntimeState();
	CapacityState = UGameXXKMVPRules::CreateNewGame();
	CapacityState.Screen = EGameXXKScreen::Town;
	const FName EquippedWeapon = CreateWarehouseWeapon(*this, CapacityState);
	TestTrue(TEXT("capacity fixture equips its reserved weapon"),
		CapacitySubsystem->EquipEquipmentInstance(FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, EquippedWeapon, Result));
	const int32 RemainingWarehouseSlots = FGameXXKEquipmentRules::WarehouseCapacity
		- FGameXXKEquipmentRules::CountWarehouseItems(CapacityState.EquipmentCollection);
	for (int32 Index = 0; Index < RemainingWarehouseSlots; ++Index)
	{
		if (CreateWarehouseItem(
			*this,
			CapacityState,
			EGameXXKEquipmentSlot::Head,
			EGameXXKEquipmentSet::PoJun,
			EGameXXKEquipmentQuality::Common,
			1).IsNone())
		{
			return false;
		}
	}
	const TArray<uint8> BeforeFullWarehouseUnequip = SerializeRuntimeState(CapacityState);
	TestFalse(TEXT("facade does not unequip into a full 200-slot warehouse"),
		CapacitySubsystem->UnequipEquipmentSlot(FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, Result));
	TestEqual(TEXT("full warehouse unequip has the stable typed error"), Result.Error, EGameXXKEquipmentTransactionError::WarehouseFull);
	TestRuntimeUnchanged(*this, TEXT("full warehouse unequip preserves the complete runtime state"), BeforeFullWarehouseUnequip, CapacityState);
	return true;
}

#endif
