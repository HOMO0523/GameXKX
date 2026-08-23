#include "Misc/AutomationTest.h"

#include "GameXXKCharacterStatRules.h"
#include "GameXXKCompanionRules.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentCatalog.h"
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

	FName GetEquippedInstanceId(
		const FGameXXKRuntimeState& State,
		const FName CharacterId,
		const EGameXXKEquipmentSlot Slot)
	{
		const FGameXXKEquipmentLoadout* Loadout =
			State.EquipmentCollection.CharacterLoadouts.Find(CharacterId);
		if (!Loadout)
		{
			return NAME_None;
		}
		return FGameXXKEquipmentRules::GetLoadoutSlotInstanceId(*Loadout, Slot);
	}

	FName RecruitGuardOwner(FAutomationTestBase& Test, FGameXXKRuntimeState& State)
	{
		// Template index 4 is the first canonical Guard. A non-zero sequence seed
		// is save-valid and makes the first deterministic ticket select it.
		State.CardRun.CompanionRoster.RecruitSequenceSeed = 4;
		State.CardRun.CompanionRoster.RecruitSequenceOrdinal = 0;
		FString Error;
		FGameXXKCompanionRecruitResult Result;
		if (!FGameXXKCompanionRules::CreateAndResolveNextRecruitment(
			State.CardRun.CompanionRoster,
			Result,
			&Error))
		{
			Test.AddError(Error);
			return NAME_None;
		}
		if (Result.Outcome != EGameXXKCompanionRecruitOutcome::Recruited
			|| Result.Companion.Role != EGameXXKCharacterRole::Guard)
		{
			Test.AddError(TEXT("facade fixture did not resolve the canonical Guard ticket"));
			return NAME_None;
		}
		return Result.Companion.InstanceId;
	}

	int32 NormalizeAndFindEquipmentSlot(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const FName InstanceId,
		const EGameXXKDesktopItemContainer Container)
	{
		FString Error;
		if (!Test.TestTrue(TEXT("physical equipment fixture normalizes"),
			FGameXXKDesktopInventoryRules::Normalize(State, &Error)))
		{
			Test.AddError(Error);
			return INDEX_NONE;
		}
		return FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			Container,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId));
	}

	bool BuildFacadeLegacyOverflowState(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& OutState)
	{
		OutState = UGameXXKMVPRules::CreateNewGame();
		OutState.Screen = EGameXXKScreen::Town;
		OutState.Inventory.Reset();
		OutState.EnhancementMaterial = 0;
		OutState.ItemEnhancementLevels.Reset();
		OutState.EquippedWeapon = NAME_None;
		OutState.EquippedArmor = NAME_None;
		OutState.EquippedAccessory = NAME_None;
		OutState.EquipmentCollection = FGameXXKEquipmentCollectionState();
		OutState.EquipmentCollection.CollectionSeed = 0x4721;
		OutState.DesktopInventory = FGameXXKDesktopInventoryState();
		const FGameXXKEquipmentDefinition* Definition =
			FGameXXKEquipmentCatalog::FindDefinition(TEXT("Item.WoodenSword"));
		if (!Test.TestNotNull(TEXT("facade overflow fixture finds Wooden Sword"), Definition))
		{
			return false;
		}
		for (int32 Index = 0; Index <= FGameXXKEquipmentRules::WarehouseCapacity; ++Index)
		{
			FGameXXKEquipmentInstance Instance;
			Instance.InstanceId = FName(*FString::Printf(
				TEXT("EquipmentInstance.FacadeOverflow.%03d"), Index));
			Instance.BaseEquipmentId = Definition->Id;
			Instance.ItemLevel = 1;
			Instance.Quality = EGameXXKEquipmentQuality::Common;
			Instance.ScalingRule = Definition->ScalingRule;
			Instance.LegacyBaseStatSnapshot = Definition->LegacyBaseStatSnapshot;
			Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
			OutState.EquipmentCollection.WarehouseInstanceIds.Add(Instance.InstanceId);
			OutState.EquipmentCollection.EquipmentInstances.Add(MoveTemp(Instance));
		}
		OutState.EquipmentCollection.NextInstanceOrdinal =
			OutState.EquipmentCollection.EquipmentInstances.Num();
		OutState.EquipmentCollection.bLegacyWarehouseOverflow = true;
		FString Error;
		return Test.TestTrue(
			TEXT("facade overflow collection validates"),
			FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
				OutState.EquipmentCollection,
				OutState.CardRun.CompanionRoster,
				&Error))
			&& Test.TestTrue(
				TEXT("facade overflow fixture projects its first 200 entries"),
				FGameXXKDesktopInventoryRules::Normalize(OutState, &Error));
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
	State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemRefinementSand()) = 30;
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
	TestFalse(TEXT("facade rejects a non-owned quest-giver as an equipment owner"),
		Subsystem->EquipEquipmentInstance(TEXT("Npc.QingshanQuestGiver"), EGameXXKEquipmentSlot::Weapon, NpcRejectedWeapon, Result));
	TestEqual(TEXT("non-owned equipment rejection has the stable typed error"), Result.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestEqual(TEXT("non-owned equipment rejection uses the stable Chinese message"), Result.Message.ToString(), TEXT("该角色不能持有装备"));
	TestRuntimeUnchanged(*this, TEXT("non-owned equipment rejection preserves the complete runtime state"), BeforeNpcEquip, State);

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadePhysicalCellReplacementTest,
	"GameXXK.Equipment.Facade.PhysicalCellReplacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadePhysicalCellReplacementTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State = UGameXXKMVPRules::CreateNewGame();
	State.Screen = EGameXXKScreen::Town;
	const FName HeroId = FGameXXKEquipmentRules::HeroCharacterId();
	const FName GuardId = RecruitGuardOwner(*this, State);
	const FName NpcId(TEXT("Npc.TusiChief"));
	if (!TestFalse(TEXT("physical-cell fixture owns a stable Guard"), GuardId.IsNone()))
	{
		return false;
	}

	const TArray<FName> Owners = {HeroId, GuardId, NpcId};
	TMap<FName, FName> ExistingWeapons;
	TMap<FName, FName> IncomingWeapons;
	FGameXXKEquipmentTransactionResult Result;
	for (const FName OwnerId : Owners)
	{
		const FName ExistingWeapon = CreateWarehouseWeapon(*this, State);
		const FName IncomingWeapon = CreateWarehouseWeapon(*this, State);
		if (ExistingWeapon.IsNone() || IncomingWeapon.IsNone())
		{
			return false;
		}
		TestTrue(TEXT("fixture equips each owner's existing weapon through the established facade"),
			Subsystem->EquipEquipmentInstance(
				OwnerId,
				EGameXXKEquipmentSlot::Weapon,
				ExistingWeapon,
				Result));
		ExistingWeapons.Add(OwnerId, ExistingWeapon);
		IncomingWeapons.Add(OwnerId, IncomingWeapon);
	}

	FString Error;
	TestTrue(TEXT("replacement fixture projects every incoming instance into physical cells"),
		FGameXXKDesktopInventoryRules::Normalize(State, &Error));
	const FName GuardIncoming = IncomingWeapons.FindRef(GuardId);
	const int32 GuardBackpackSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(
		State,
		EGameXXKDesktopItemContainer::Backpack,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(GuardIncoming));
	TestTrue(TEXT("Guard incoming weapon moves to an exact Warehouse source cell"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			GuardBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			71,
			&Error));

	struct FOwnerSource
	{
		FName OwnerId;
		EGameXXKDesktopItemContainer Container;
		int32 SlotIndex = INDEX_NONE;
	};
	TArray<FOwnerSource> Sources;
	for (const FName OwnerId : Owners)
	{
		const EGameXXKDesktopItemContainer Container = OwnerId == GuardId
			? EGameXXKDesktopItemContainer::Warehouse
			: EGameXXKDesktopItemContainer::Backpack;
		const int32 SlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			Container,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(IncomingWeapons.FindRef(OwnerId)));
		if (!TestTrue(TEXT("each owner has an exact incoming physical source cell"), SlotIndex != INDEX_NONE))
		{
			return false;
		}
		Sources.Add({OwnerId, Container, SlotIndex});
	}

	State.DesktopInventory.LockedEquipmentInstanceIds.Add(IncomingWeapons.FindRef(NpcId));
	for (const FOwnerSource& Source : Sources)
	{
		TMap<FName, FGameXXKEquipmentLoadout> UnrelatedLoadoutsBefore;
		for (const FName OtherOwnerId : Owners)
		{
			if (OtherOwnerId != Source.OwnerId)
			{
				if (const FGameXXKEquipmentLoadout* OtherLoadout =
					State.EquipmentCollection.CharacterLoadouts.Find(OtherOwnerId))
				{
					UnrelatedLoadoutsBefore.Add(OtherOwnerId, *OtherLoadout);
				}
			}
		}

		const FName IncomingId = IncomingWeapons.FindRef(Source.OwnerId);
		const FName DisplacedId = ExistingWeapons.FindRef(Source.OwnerId);
		TestTrue(TEXT("Hero, Guard, and named NPC replace from their exact physical source"),
			Subsystem->EquipEquipmentFromDesktopCell(
				Source.OwnerId,
				EGameXXKEquipmentSlot::Weapon,
				Source.Container,
				Source.SlotIndex,
				IncomingId,
				Result));
		TestTrue(TEXT("physical-cell replacement returns a typed success"), Result.bSucceeded);
		TestEqual(TEXT("incoming instance owns only the requested owner's weapon slot"),
			GetEquippedInstanceId(State, Source.OwnerId, EGameXXKEquipmentSlot::Weapon),
			IncomingId);
		TestEqual(TEXT("displaced instance returns to the exact incoming source cell"),
			FGameXXKDesktopInventoryRules::GetEntryAt(State, Source.Container, Source.SlotIndex),
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(DisplacedId));
		TestTrue(TEXT("displaced instance returns to the unequipped equipment collection"),
			State.EquipmentCollection.WarehouseInstanceIds.Contains(DisplacedId));
		TestFalse(TEXT("incoming instance leaves the unequipped equipment collection"),
			State.EquipmentCollection.WarehouseInstanceIds.Contains(IncomingId));
		TestEqual(TEXT("displaced instance returns to the same physical container partition"),
			State.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(DisplacedId),
			Source.Container == EGameXXKDesktopItemContainer::Warehouse);
		for (const TPair<FName, FGameXXKEquipmentLoadout>& Pair : UnrelatedLoadoutsBefore)
		{
			const FGameXXKEquipmentLoadout* UnrelatedAfter =
				State.EquipmentCollection.CharacterLoadouts.Find(Pair.Key);
			TestTrue(TEXT("replacement leaves every unrelated owner loadout byte-identical"),
				UnrelatedAfter
				&& FGameXXKEquipmentLoadout::StaticStruct()->CompareScriptStruct(
					UnrelatedAfter,
					&Pair.Value,
					PPF_None));
		}
	}
	TestTrue(TEXT("persistent equipment locks do not block explicit manual equip"),
		State.DesktopInventory.LockedEquipmentInstanceIds.Contains(IncomingWeapons.FindRef(NpcId)));

	const FName EmptySlotArmor = CreateWarehouseItem(*this, State, EGameXXKEquipmentSlot::Armor);
	const int32 ArmorBackpackSlot = NormalizeAndFindEquipmentSlot(
		*this, State, EmptySlotArmor, EGameXXKDesktopItemContainer::Backpack);
	TestTrue(TEXT("no-displaced fixture moves incoming armor to Warehouse"),
		FGameXXKDesktopInventoryRules::MoveEntry(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			ArmorBackpackSlot,
			EGameXXKDesktopItemContainer::Warehouse,
			88,
			&Error));
	TestTrue(TEXT("equipping into an empty loadout slot succeeds from Warehouse"),
		Subsystem->EquipEquipmentFromDesktopCell(
			HeroId,
			EGameXXKEquipmentSlot::Armor,
			EGameXXKDesktopItemContainer::Warehouse,
			88,
			EmptySlotArmor,
			Result));
	TestEqual(TEXT("empty destination equips the incoming armor"),
		GetEquippedInstanceId(State, HeroId, EGameXXKEquipmentSlot::Armor),
		EmptySlotArmor);
	TestFalse(TEXT("empty destination leaves the exact source cell empty"),
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Warehouse, 88).IsValid());
	TestFalse(TEXT("empty destination removes incoming from the source partition"),
		State.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(EmptySlotArmor));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadeLegacyOverflowBackfillTest,
	"GameXXK.Equipment.Facade.LegacyOverflowBackfill",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadeLegacyOverflowBackfillTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	if (!BuildFacadeLegacyOverflowState(*this, State))
	{
		return false;
	}
	const int32 SourceSlot = 0;
	const FGameXXKDesktopInventoryEntryKey IncomingEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot);
	if (!TestTrue(TEXT("overflow facade source is visible equipment"),
		IncomingEntry.IsValid() && IncomingEntry.bEquipmentInstance))
	{
		return false;
	}
	FName HiddenInstanceId = NAME_None;
	for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
	{
		if (FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId)) == INDEX_NONE)
		{
			HiddenInstanceId = InstanceId;
			break;
		}
	}
	if (!TestFalse(TEXT("overflow facade fixture has one hidden instance"), HiddenInstanceId.IsNone()))
	{
		return false;
	}

	FGameXXKEquipmentTransactionResult Result;
	TestTrue(TEXT("equipping a visible legacy-overflow instance into an empty loadout succeeds"),
		Subsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			IncomingEntry.EntryId,
			Result));
	TestTrue(TEXT("legacy-overflow facade returns typed success"), Result.bSucceeded);
	TestEqual(TEXT("legacy-overflow incoming instance equips into the requested slot"),
		GetEquippedInstanceId(
			State,
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon),
		IncomingEntry.EntryId);
	TestFalse(TEXT("equipping down to 200 clears the legacy overflow flag"),
		State.EquipmentCollection.bLegacyWarehouseOverflow);
	TestEqual(TEXT("legacy overflow recovery leaves exactly 200 unequipped instances"),
		State.EquipmentCollection.WarehouseInstanceIds.Num(),
		FGameXXKEquipmentRules::WarehouseCapacity);
	const FGameXXKDesktopInventoryEntryKey BackfilledEntry =
		FGameXXKDesktopInventoryRules::GetEntryAt(
			State, EGameXXKDesktopItemContainer::Backpack, SourceSlot);
	TestTrue(TEXT("empty loadout source accepts a newly exposed valid backfill"),
		BackfilledEntry.IsValid() && BackfilledEntry.bEquipmentInstance);
	TestEqual(TEXT("the previously hidden instance backfills the vacated exact source"),
		BackfilledEntry,
		FGameXXKDesktopInventoryRules::MakeEquipmentEntry(HiddenInstanceId));
	TestNotEqual(TEXT("incoming instance never remains in its former physical source"),
		BackfilledEntry.EntryId, IncomingEntry.EntryId);
	TestEqual(TEXT("all 200 unequipped instances are physically projected"),
		FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(
			State, EGameXXKDesktopItemContainer::Backpack),
		FGameXXKDesktopInventoryRules::BackpackCapacity);
	TestEqual(TEXT("legacy compatibility count remains available without owning a physical item cell"),
		State.Inventory.FindRef(TEXT("Item.WoodenSword")), 201);
	TestEqual(TEXT("legacy equipment compatibility mirror is not projected as an item stack"),
		FGameXXKDesktopInventoryRules::FindEntrySlot(
			State,
			EGameXXKDesktopItemContainer::Backpack,
			FGameXXKDesktopInventoryRules::MakeItemEntry(TEXT("Item.WoodenSword"))),
		INDEX_NONE);
	for (const FName InstanceId : State.EquipmentCollection.WarehouseInstanceIds)
	{
		TestTrue(TEXT("every recovered unequipped instance has one Backpack cell"),
			FGameXXKDesktopInventoryRules::FindEntrySlot(
				State,
				EGameXXKDesktopItemContainer::Backpack,
				FGameXXKDesktopInventoryRules::MakeEquipmentEntry(InstanceId)) != INDEX_NONE);
	}
	FString Error;
	TestTrue(TEXT("legacy-overflow recovered collection validates"),
		FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
			State.EquipmentCollection,
			State.CardRun.CompanionRoster,
			&Error));
	TestTrue(TEXT("legacy-overflow recovered desktop projection validates"),
		FGameXXKDesktopInventoryRules::Validate(State, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentFacadePhysicalCellFailureTest,
	"GameXXK.Equipment.Facade.PhysicalCellFailures",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentFacadePhysicalCellFailureTest::RunTest(const FString& Parameters)
{
	auto MakeTownFixture = [this](FName& OutWeaponId, int32& OutSourceSlot)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
		State = UGameXXKMVPRules::CreateNewGame();
		State.Screen = EGameXXKScreen::Town;
		OutWeaponId = CreateWarehouseWeapon(*this, State);
		OutSourceSlot = NormalizeAndFindEquipmentSlot(
			*this, State, OutWeaponId, EGameXXKDesktopItemContainer::Backpack);
		return Subsystem;
	};

	FGameXXKEquipmentTransactionResult Result;
	FName WeaponId;
	int32 SourceSlot = INDEX_NONE;
	UGameXXKMVPSubsystem* WrongSlotSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& WrongSlotState = WrongSlotSubsystem->GetMutableRuntimeState();
	const TArray<uint8> BeforeWrongSlot = SerializeRuntimeState(WrongSlotState);
	TestFalse(TEXT("physical-cell equip rejects the wrong equipment slot"),
		WrongSlotSubsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Head,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("wrong-slot rejection keeps the stable typed error"),
		Result.Error, EGameXXKEquipmentTransactionError::SlotMismatch);
	TestRuntimeUnchanged(*this, TEXT("wrong-slot rejection preserves every runtime byte"), BeforeWrongSlot, WrongSlotState);

	UGameXXKMVPSubsystem* StaleSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& StaleState = StaleSubsystem->GetMutableRuntimeState();
	TestTrue(TEXT("stale fixture equips the formerly physical source through another transaction"),
		StaleSubsystem->EquipEquipmentInstance(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			WeaponId,
			Result));
	const TArray<uint8> BeforeStaleSource = SerializeRuntimeState(StaleState);
	TestFalse(TEXT("physical-cell equip rejects a stale source projection"),
		StaleSubsystem->EquipEquipmentFromDesktopCell(
			TEXT("Npc.TusiChief"),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestRuntimeUnchanged(*this, TEXT("stale source rejection preserves every runtime byte"), BeforeStaleSource, StaleState);

	UGameXXKMVPSubsystem* RouteLockedSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& RouteLockedState = RouteLockedSubsystem->GetMutableRuntimeState();
	RouteLockedState.CardRun.bLoadoutLockedForRoute = true;
	const TArray<uint8> BeforeRouteLocked = SerializeRuntimeState(RouteLockedState);
	TestFalse(TEXT("route loadout lock blocks physical-cell equip"),
		RouteLockedSubsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("route-locked physical-cell equip has the stable typed error"),
		Result.Error, EGameXXKEquipmentTransactionError::RouteLocked);
	TestRuntimeUnchanged(*this, TEXT("route-lock rejection preserves every runtime byte"), BeforeRouteLocked, RouteLockedState);

	UGameXXKMVPSubsystem* InvalidOwnerSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& InvalidOwnerState = InvalidOwnerSubsystem->GetMutableRuntimeState();
	const TArray<uint8> BeforeInvalidOwner = SerializeRuntimeState(InvalidOwnerState);
	TestFalse(TEXT("physical-cell equip rejects a non-owned character"),
		InvalidOwnerSubsystem->EquipEquipmentFromDesktopCell(
			TEXT("Npc.QingshanQuestGiver"),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("invalid-owner physical-cell rejection is typed"),
		Result.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestRuntimeUnchanged(*this, TEXT("invalid-owner rejection preserves every runtime byte"), BeforeInvalidOwner, InvalidOwnerState);

	UGameXXKMVPSubsystem* AbaSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& AbaState = AbaSubsystem->GetMutableRuntimeState();
	const FName ReplacementPhysicalWeapon = CreateWarehouseWeapon(*this, AbaState);
	const int32 ReplacementPhysicalSlot = NormalizeAndFindEquipmentSlot(
		*this,
		AbaState,
		ReplacementPhysicalWeapon,
		EGameXXKDesktopItemContainer::Backpack);
	if (!TestTrue(TEXT("equipment ABA fixture finds the replacement physical cell"),
		ReplacementPhysicalSlot != INDEX_NONE))
	{
		return false;
	}
	Swap(
		AbaState.DesktopInventory.BackpackSlots[SourceSlot],
		AbaState.DesktopInventory.BackpackSlots[ReplacementPhysicalSlot]);
	const TArray<uint8> BeforeAba = SerializeRuntimeState(AbaState);
	TestFalse(TEXT("physical-cell equip rejects an ABA-replaced source instance"),
		AbaSubsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("equipment ABA rejection is typed as invalid request"),
		Result.Error, EGameXXKEquipmentTransactionError::InvalidRequest);
	TestRuntimeUnchanged(*this, TEXT("equipment ABA rejection preserves every runtime byte"), BeforeAba, AbaState);

	UGameXXKMVPSubsystem* FullSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& FullState = FullSubsystem->GetMutableRuntimeState();
	for (int32 Index = 0; Index <= FGameXXKDesktopInventoryRules::BackpackCapacity; ++Index)
	{
		FullState.Inventory.Add(FName(*FString::Printf(TEXT("Item.Test.EquipFull.%03d"), Index)), 1);
	}
	const TArray<uint8> BeforeFull = SerializeRuntimeState(FullState);
	TestFalse(TEXT("physical-cell equip rejects an over-capacity normalized candidate"),
		FullSubsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("physical-cell capacity failure is typed as collection invalid"),
		Result.Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestRuntimeUnchanged(*this, TEXT("physical-cell capacity failure preserves every runtime byte"), BeforeFull, FullState);

	UGameXXKMVPSubsystem* ValidationSubsystem = MakeTownFixture(WeaponId, SourceSlot);
	FGameXXKRuntimeState& ValidationState = ValidationSubsystem->GetMutableRuntimeState();
	const FGameXXKEquipmentInstance* DuplicateSource =
		FGameXXKEquipmentRules::FindInstance(ValidationState.EquipmentCollection, WeaponId);
	if (!TestNotNull(TEXT("validation rollback fixture finds its physical equipment instance"), DuplicateSource))
	{
		return false;
	}
	const FGameXXKEquipmentInstance DuplicateInstance = *DuplicateSource;
	ValidationState.EquipmentCollection.EquipmentInstances.Add(DuplicateInstance);
	const TArray<uint8> BeforeValidationFailure = SerializeRuntimeState(ValidationState);
	TestFalse(TEXT("equipment collection validation failure rejects the candidate"),
		ValidationSubsystem->EquipEquipmentFromDesktopCell(
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			EGameXXKDesktopItemContainer::Backpack,
			SourceSlot,
			WeaponId,
			Result));
	TestEqual(TEXT("equipment validation failure is typed as collection invalid"),
		Result.Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestRuntimeUnchanged(*this, TEXT("equipment validation rollback preserves every runtime byte"),
		BeforeValidationFailure, ValidationState);
	return true;
}

#endif
