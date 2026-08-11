#include "Misc/AutomationTest.h"

#include "GameXXKAffixCatalog.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKEquipmentRules.h"
#include "Misc/Crc.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool IsValidTier(const EGameXXKAffixTier Tier)
	{
		return Tier >= EGameXXKAffixTier::Common && Tier <= EGameXXKAffixTier::Epic;
	}

	TArray<uint8> SerializeCollection(const FGameXXKEquipmentCollectionState& Collection)
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Writer, false);
		Archive.ArIsSaveGame = true;
		FGameXXKEquipmentCollectionState Copy = Collection;
		FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Archive, &Copy, nullptr);
		return Bytes;
	}

	bool DeserializeCollection(const TArray<uint8>& Bytes, FGameXXKEquipmentCollectionState& OutCollection)
	{
		FMemoryReader Reader(Bytes, true);
		FObjectAndNameAsStringProxyArchive Archive(Reader, false);
		Archive.ArIsSaveGame = true;
		FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Archive, &OutCollection, nullptr);
		return !Archive.IsError();
	}

	void TestBytesEqual(
		FAutomationTestBase& Test,
		const FString& What,
		const TArray<uint8>& Before,
		const FGameXXKEquipmentCollectionState& After)
	{
		Test.TestEqual(What, SerializeCollection(After), Before);
	}

	FGameXXKEquipmentCreateRequest MakeRequest(
		const EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::PoJun,
		const EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Epic,
		const int32 ItemLevel = 10,
		const EGameXXKEquipmentSlot ForcedSlot = EGameXXKEquipmentSlot::Invalid)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = Set;
		Request.Quality = Quality;
		Request.ItemLevel = ItemLevel;
		Request.bForceSlot = ForcedSlot != EGameXXKEquipmentSlot::Invalid;
		Request.ForcedSlot = ForcedSlot;
		return Request;
	}

	FName CreateChecked(
		FAutomationTestBase& Test,
		FGameXXKEquipmentCollectionState& Collection,
		const FGameXXKEquipmentCreateRequest& Request)
	{
		FName InstanceId;
		FString Error;
		FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, InstanceId, &Error);
		return InstanceId;
	}

	FName& SlotRef(FGameXXKEquipmentLoadout& Loadout, const EGameXXKEquipmentSlot Slot)
	{
		switch (Slot)
		{
		case EGameXXKEquipmentSlot::Weapon: return Loadout.WeaponInstanceId;
		case EGameXXKEquipmentSlot::Head: return Loadout.HeadInstanceId;
		case EGameXXKEquipmentSlot::Armor: return Loadout.ArmorInstanceId;
		case EGameXXKEquipmentSlot::Belt: return Loadout.BeltInstanceId;
		case EGameXXKEquipmentSlot::Shoes: return Loadout.ShoesInstanceId;
		default: return Loadout.AccessoryInstanceId;
		}
	}

	FGameXXKPermanentCompanion MakeCompanion(const int32 Index)
	{
		FGameXXKPermanentCompanion Companion;
		Companion.InstanceId = FName(*FString::Printf(TEXT("Companion.Instance.%02d"), Index));
		return Companion;
	}

	FGameXXKEquipmentInstance MakeLegacyWarehouseInstance(const int32 Index)
	{
		const FGameXXKEquipmentDefinition* Definition = FGameXXKEquipmentCatalog::FindDefinition(TEXT("Item.WoodenSword"));
		FGameXXKEquipmentInstance Instance;
		Instance.InstanceId = FName(*FString::Printf(TEXT("EquipmentInstance.Legacy.%03d"), Index));
		Instance.BaseEquipmentId = Definition->Id;
		Instance.ItemLevel = 1;
		Instance.Quality = EGameXXKEquipmentQuality::Common;
		Instance.ScalingRule = Definition->ScalingRule;
		Instance.LegacyBaseStatSnapshot = Definition->LegacyBaseStatSnapshot;
		Instance.OwnerKind = EGameXXKEquipmentOwnerKind::Warehouse;
		return Instance;
	}

	FGameXXKEquipmentCollectionState MakeLegacyOverflow(const int32 Count)
	{
		FGameXXKEquipmentCollectionState Collection;
		Collection.bLegacyWarehouseOverflow = Count > FGameXXKEquipmentRules::WarehouseCapacity;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKEquipmentInstance Instance = MakeLegacyWarehouseInstance(Index);
			Collection.WarehouseInstanceIds.Add(Instance.InstanceId);
			Collection.EquipmentInstances.Add(MoveTemp(Instance));
		}
		return Collection;
	}

	bool IsLegalRoll(const FGameXXKEquipmentInstance& Instance)
	{
		const FGameXXKEquipmentDefinition* Equipment = FGameXXKEquipmentCatalog::FindDefinition(Instance.BaseEquipmentId);
		if (!Equipment)
		{
			return false;
		}
		TSet<EGameXXKEquipmentModifierKind> ModifierKinds;
		for (const FGameXXKEquipmentAffixRoll& Roll : Instance.RolledAffixes)
		{
			const FGameXXKAffixDefinition* Affix = FGameXXKAffixCatalog::FindDefinition(Roll.AffixId);
			if (!Affix || (Affix->Set != EGameXXKEquipmentSet::Invalid && Affix->Set != Equipment->Set)
				|| Affix->Unit != Roll.Unit || ModifierKinds.Contains(Affix->ModifierKind))
			{
				return false;
			}
			ModifierKinds.Add(Affix->ModifierKind);
			const FGameXXKAffixMagnitudeRange Range = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.Unit, Roll.Tier);
			if (Roll.Magnitude < Range.Minimum || Roll.Magnitude > Range.Maximum)
			{
				return false;
			}
		}
		return true;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentRulesDeterministicRollTest,
	"GameXXK.Equipment.Rules.DeterministicRolls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentRulesDeterministicRollTest::RunTest(const FString& Parameters)
{
	const double StartSeconds = FPlatformTime::Seconds();
	int32 SlotCounts[6] = {};
	int32 TierCounts[3] = {};
	int32 RetiredSpeedRollCount = 0;
	for (int32 FixedSeed = 1; FixedSeed <= 10000; ++FixedSeed)
	{
		FGameXXKEquipmentCollectionState Collection;
		Collection.CollectionSeed = FixedSeed;
		const EGameXXKEquipmentSet Set = static_cast<EGameXXKEquipmentSet>(
			static_cast<uint8>(EGameXXKEquipmentSet::PoJun) + (FixedSeed - 1) % 6);
		const FName InstanceId = CreateChecked(*this, Collection, MakeRequest(Set));
		if (InstanceId.IsNone())
		{
			AddError(TEXT("CreateRolledInstance must create the first fixed-seed roll"));
			return false;
		}
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(Collection, InstanceId);
		if (!Instance)
		{
			AddError(FString::Printf(TEXT("fixed seed %d did not resolve its created instance"), FixedSeed));
			continue;
		}
		const FGameXXKEquipmentDefinition* Equipment = FGameXXKEquipmentCatalog::FindDefinition(Instance->BaseEquipmentId);
		if (!Equipment)
		{
			AddError(FString::Printf(TEXT("fixed seed %d rolled an unknown equipment row"), FixedSeed));
			continue;
		}
		++SlotCounts[static_cast<uint8>(Equipment->Slot) - 1];
		if (!IsLegalRoll(*Instance))
		{
			AddError(FString::Printf(TEXT("fixed seed %d rolled an illegal affix"), FixedSeed));
		}
		if (Instance->RolledAffixes.Num() != 3)
		{
			AddError(FString::Printf(TEXT("fixed seed %d did not roll three epic affixes"), FixedSeed));
		}
		for (const FGameXXKEquipmentAffixRoll& Roll : Instance->RolledAffixes)
		{
			RetiredSpeedRollCount += Roll.AffixId == FName(TEXT("Affix.Universal.Speed")) ? 1 : 0;
			if (IsValidTier(Roll.Tier))
			{
				++TierCounts[static_cast<uint8>(Roll.Tier) - 1];
			}
		}
	}
	for (int32 SlotIndex = 0; SlotIndex < 6; ++SlotIndex)
	{
		TestTrue(FString::Printf(TEXT("slot %d is approximately uniform across 10,000 rolls"), SlotIndex + 1), SlotCounts[SlotIndex] >= 1500 && SlotCounts[SlotIndex] <= 1834);
	}
	TestTrue(TEXT("epic-quality common tiers follow the catalog's 50% weight"), TierCounts[0] >= 14000 && TierCounts[0] <= 16000);
	TestTrue(TEXT("epic-quality rare tiers follow the catalog's 35% weight"), TierCounts[1] >= 9500 && TierCounts[1] <= 11500);
	TestTrue(TEXT("epic-quality epic tiers follow the catalog's 15% weight"), TierCounts[2] >= 3500 && TierCounts[2] <= 5500);
	TestEqual(TEXT("new equipment never rolls the retired Speed affix"), RetiredSpeedRollCount, 0);
	AddInfo(FString::Printf(TEXT("[EquipmentRules] 10000 deterministic rolls completed in %.3f seconds"), FPlatformTime::Seconds() - StartSeconds));

	FGameXXKEquipmentCollectionState Uninterrupted;
	Uninterrupted.CollectionSeed = 0x13572468;
	for (int32 Index = 0; Index < 40; ++Index)
	{
		CreateChecked(*this, Uninterrupted, MakeRequest(EGameXXKEquipmentSet::ShanHe, static_cast<EGameXXKEquipmentQuality>(1 + Index % 3)));
	}
	const TArray<uint8> SaveBytes = SerializeCollection(Uninterrupted);
	FGameXXKEquipmentCollectionState Reloaded;
	TestTrue(TEXT("serialized collection reloads"), DeserializeCollection(SaveBytes, Reloaded));
	for (int32 Index = 40; Index < 80; ++Index)
	{
		const FGameXXKEquipmentCreateRequest Request = MakeRequest(EGameXXKEquipmentSet::ShanHe, static_cast<EGameXXKEquipmentQuality>(1 + Index % 3));
		CreateChecked(*this, Uninterrupted, Request);
		CreateChecked(*this, Reloaded, Request);
	}
	TestEqual(TEXT("save/reload continuation remains byte-for-byte deterministic"), SerializeCollection(Reloaded), SerializeCollection(Uninterrupted));

	FGameXXKEquipmentCollectionState KnownSeed;
	KnownSeed.CollectionSeed = 42;
	KnownSeed.NextInstanceOrdinal = 7;
	const FName KnownId = CreateChecked(*this, KnownSeed, MakeRequest(EGameXXKEquipmentSet::PoJun, EGameXXKEquipmentQuality::Rare, 9, EGameXXKEquipmentSlot::Weapon));
	TestEqual(TEXT("stable instance ID uses seed hex and ordinal"), KnownId, FName(TEXT("EquipmentInstance.0000002A.7")));
	const FGameXXKEquipmentInstance* KnownInstance = FGameXXKEquipmentRules::FindInstance(KnownSeed, KnownId);
	const uint32 ExpectedCrc = FCrc::StrCrc32(TEXT("42|7|3|1|2|9"));
	TestNotNull(TEXT("known-seed instance resolves"), KnownInstance);
	if (KnownInstance)
	{
		TestEqual(TEXT("roll stream seed uses the frozen stable ASCII CRC input"), static_cast<uint32>(KnownInstance->AcquisitionSeed), ExpectedCrc);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentRulesOwnershipTest,
	"GameXXK.Equipment.Rules.OwnershipAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentRulesOwnershipTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("hero uses the stable Player owner ID"), FGameXXKEquipmentRules::HeroCharacterId(), FName(TEXT("Player")));
	FGameXXKEquipmentCollectionState CapacityCollection;
	const FName FirstCapacityId = CreateChecked(*this, CapacityCollection, MakeRequest(EGameXXKEquipmentSet::PoJun, EGameXXKEquipmentQuality::Common, 1, EGameXXKEquipmentSlot::Weapon));
	if (FirstCapacityId.IsNone())
	{
		AddError(TEXT("CreateRolledInstance must create the first capacity fixture"));
		return false;
	}
	for (int32 Index = 1; Index < 200; ++Index)
	{
		CreateChecked(*this, CapacityCollection, MakeRequest(EGameXXKEquipmentSet::PoJun, EGameXXKEquipmentQuality::Common, 1, EGameXXKEquipmentSlot::Weapon));
	}
	TestEqual(TEXT("warehouse accepts exactly 200 items"), FGameXXKEquipmentRules::CountWarehouseItems(CapacityCollection), 200);
	TestFalse(TEXT("warehouse has no 201st acquisition slot"), FGameXXKEquipmentRules::HasWarehouseCapacity(CapacityCollection));
	const TArray<uint8> FullBefore = SerializeCollection(CapacityCollection);
	FName FailedId(TEXT("MustBeCleared"));
	TestFalse(TEXT("201st acquisition is rejected"), FGameXXKEquipmentRules::CreateRolledInstance(CapacityCollection, MakeRequest(), FailedId));
	TestTrue(TEXT("failed acquisition clears output instance ID"), FailedId.IsNone());
	TestBytesEqual(*this, TEXT("201st acquisition failure preserves every serialized field"), FullBefore, CapacityCollection);

	FGameXXKCompanionRosterState Roster;
	for (int32 Index = 0; Index < 12; ++Index)
	{
		Roster.PermanentCompanions.Add(MakeCompanion(Index));
	}
	FGameXXKEquipmentCollectionState PartyCollection;
	TArray<FName> Owners = {FGameXXKEquipmentRules::HeroCharacterId()};
	for (const FGameXXKPermanentCompanion& Companion : Roster.PermanentCompanions)
	{
		Owners.Add(Companion.InstanceId);
	}
	for (const FName OwnerId : Owners)
	{
		for (uint8 SlotValue = 1; SlotValue <= 6; ++SlotValue)
		{
			const EGameXXKEquipmentSlot Slot = static_cast<EGameXXKEquipmentSlot>(SlotValue);
			const FName InstanceId = CreateChecked(*this, PartyCollection, MakeRequest(EGameXXKEquipmentSet::XuanJia, EGameXXKEquipmentQuality::Rare, 4, Slot));
			const FGameXXKEquipmentTransactionResult Result = FGameXXKEquipmentRules::EquipInstance(PartyCollection, Roster, OwnerId, Slot, InstanceId);
			TestTrue(TEXT("hero and twelve companions each accept an independent slot"), Result.bSucceeded);
		}
	}
	TestEqual(TEXT("hero plus twelve companions own thirteen loadouts"), PartyCollection.CharacterLoadouts.Num(), 13);
	TestEqual(TEXT("all 78 equipped items leave the warehouse"), PartyCollection.WarehouseInstanceIds.Num(), 0);
	TestTrue(TEXT("full supported roster collection validates"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(PartyCollection, Roster));

	FGameXXKEquipmentCollectionState TaskNpcCollection;
	const FName TaskNpcItem = CreateChecked(*this, TaskNpcCollection, MakeRequest(EGameXXKEquipmentSet::QingNang, EGameXXKEquipmentQuality::Common, 2, EGameXXKEquipmentSlot::Accessory));
	const TArray<uint8> TaskNpcBefore = SerializeCollection(TaskNpcCollection);
	const FGameXXKEquipmentTransactionResult TaskNpcResult = FGameXXKEquipmentRules::EquipInstance(TaskNpcCollection, Roster, TEXT("Npc.QingshanQuestGiver"), EGameXXKEquipmentSlot::Accessory, TaskNpcItem);
	TestFalse(TEXT("task NPC cannot own equipment"), TaskNpcResult.bSucceeded);
	TestEqual(TEXT("task NPC rejection is typed InvalidOwner"), TaskNpcResult.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestBytesEqual(*this, TEXT("task NPC rejection preserves the complete collection"), TaskNpcBefore, TaskNpcCollection);

	FGameXXKEquipmentCollectionState SwapCollection;
	const FName FirstWeapon = CreateChecked(*this, SwapCollection, MakeRequest(EGameXXKEquipmentSet::PoJun, EGameXXKEquipmentQuality::Common, 1, EGameXXKEquipmentSlot::Weapon));
	const FName SecondWeapon = CreateChecked(*this, SwapCollection, MakeRequest(EGameXXKEquipmentSet::ShiGu, EGameXXKEquipmentQuality::Rare, 2, EGameXXKEquipmentSlot::Weapon));
	const FName TailWeapon = CreateChecked(*this, SwapCollection, MakeRequest(EGameXXKEquipmentSet::ShanHe, EGameXXKEquipmentQuality::Epic, 3, EGameXXKEquipmentSlot::Weapon));
	TestTrue(TEXT("first weapon equips"), FGameXXKEquipmentRules::EquipInstance(SwapCollection, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, FirstWeapon).bSucceeded);
	TestTrue(TEXT("second weapon swaps into occupied slot"), FGameXXKEquipmentRules::EquipInstance(SwapCollection, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, SecondWeapon).bSucceeded);
	TestEqual(TEXT("displaced item returns at incoming item's stable warehouse index"), SwapCollection.WarehouseInstanceIds[0], FirstWeapon);
	TestEqual(TEXT("unrelated tail order is stable during swap"), SwapCollection.WarehouseInstanceIds[1], TailWeapon);
	TestTrue(TEXT("unequip returns swapped item"), FGameXXKEquipmentRules::UnequipInstance(SwapCollection, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon).bSucceeded);
	TestEqual(TEXT("unequipped item appends after stable existing order"), SwapCollection.WarehouseInstanceIds.Last(), SecondWeapon);

	FGameXXKEquipmentCollectionState FullUnequip = MakeLegacyOverflow(200);
	const FName EquippedId(TEXT("EquipmentInstance.Legacy.Equipped"));
	FGameXXKEquipmentInstance Equipped = MakeLegacyWarehouseInstance(999);
	Equipped.InstanceId = EquippedId;
	Equipped.OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	Equipped.OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	FullUnequip.EquipmentInstances.Add(Equipped);
	FullUnequip.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).WeaponInstanceId = EquippedId;
	TestTrue(TEXT("full-warehouse equipped fixture is valid"), FGameXXKEquipmentRules::ValidateCollectionState(FullUnequip));
	const TArray<uint8> UnequipBefore = SerializeCollection(FullUnequip);
	const FGameXXKEquipmentTransactionResult FullUnequipResult = FGameXXKEquipmentRules::UnequipInstance(FullUnequip, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon);
	TestFalse(TEXT("unequip fails at normal warehouse capacity"), FullUnequipResult.bSucceeded);
	TestEqual(TEXT("full unequip reports WarehouseFull"), FullUnequipResult.Error, EGameXXKEquipmentTransactionError::WarehouseFull);
	TestBytesEqual(*this, TEXT("full-warehouse unequip rolls back byte-for-byte"), UnequipBefore, FullUnequip);
	const FGameXXKEquipmentTransactionResult FullReturnResult = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(FullUnequip, FGameXXKEquipmentRules::HeroCharacterId());
	TestFalse(TEXT("return-all fails at normal warehouse capacity"), FullReturnResult.bSucceeded);
	TestEqual(TEXT("full return-all reports WarehouseFull"), FullReturnResult.Error, EGameXXKEquipmentTransactionError::WarehouseFull);
	TestBytesEqual(*this, TEXT("full-warehouse return-all rolls back byte-for-byte"), UnequipBefore, FullUnequip);

	FGameXXKEquipmentCollectionState ModernSource;
	const FName EquippedModernId = CreateChecked(*this, ModernSource, MakeRequest(
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentQuality::Common,
		1,
		EGameXXKEquipmentSlot::Weapon));
	if (EquippedModernId.IsNone())
	{
		AddError(TEXT("legacy-overflow modern swap fixture must be created"));
		return false;
	}
	FGameXXKEquipmentCollectionState OverflowWithEquippedModern = MakeLegacyOverflow(201);
	FGameXXKEquipmentInstance EquippedModern = ModernSource.EquipmentInstances[0];
	EquippedModern.OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	EquippedModern.OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	OverflowWithEquippedModern.EquipmentInstances.Add(EquippedModern);
	OverflowWithEquippedModern.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).WeaponInstanceId = EquippedModernId;
	TestTrue(TEXT("legacy overflow may coexist with equipped modern equipment"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(OverflowWithEquippedModern, Roster));
	const FName OverflowIncomingLegacyId = OverflowWithEquippedModern.WarehouseInstanceIds[37];
	const TArray<uint8> OverflowSwapBefore = SerializeCollection(OverflowWithEquippedModern);
	const FGameXXKEquipmentTransactionResult OverflowSwap = FGameXXKEquipmentRules::EquipInstance(
		OverflowWithEquippedModern,
		Roster,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Weapon,
		OverflowIncomingLegacyId);
	TestFalse(TEXT("overflow swap rejects moving a displaced modern item into the over-cap legacy warehouse"), OverflowSwap.bSucceeded);
	TestEqual(TEXT("overflow swap terminal validation reports CollectionInvalid"), OverflowSwap.Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestBytesEqual(*this, TEXT("overflow swap terminal validation failure rolls back byte-for-byte"), OverflowSwapBefore, OverflowWithEquippedModern);
	const FGameXXKEquipmentTransactionResult OverflowUnequip = FGameXXKEquipmentRules::UnequipInstance(
		OverflowWithEquippedModern,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Weapon);
	TestEqual(TEXT("legacy overflow explicitly blocks unequip"), OverflowUnequip.Error, EGameXXKEquipmentTransactionError::WarehouseFull);
	TestBytesEqual(*this, TEXT("legacy-overflow unequip failure rolls back byte-for-byte"), OverflowSwapBefore, OverflowWithEquippedModern);
	const FGameXXKEquipmentTransactionResult OverflowReturn = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(
		OverflowWithEquippedModern,
		FGameXXKEquipmentRules::HeroCharacterId());
	TestEqual(TEXT("legacy overflow explicitly blocks return-all"), OverflowReturn.Error, EGameXXKEquipmentTransactionError::WarehouseFull);
	TestBytesEqual(*this, TEXT("legacy-overflow return-all failure rolls back byte-for-byte"), OverflowSwapBefore, OverflowWithEquippedModern);

	FGameXXKEquipmentCollectionState RecoverableOverflow = MakeLegacyOverflow(201);
	const FName RecoveryEquipId = RecoverableOverflow.WarehouseInstanceIds[0];
	const FGameXXKEquipmentTransactionResult RecoveryEquip = FGameXXKEquipmentRules::EquipInstance(
		RecoverableOverflow,
		Roster,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Weapon,
		RecoveryEquipId);
	TestTrue(TEXT("equipping out of 201-item legacy overflow succeeds"), RecoveryEquip.bSucceeded);
	TestEqual(TEXT("equipping out of overflow leaves exactly 200 warehouse items"), RecoverableOverflow.WarehouseInstanceIds.Num(), 200);
	TestFalse(TEXT("equipping down to 200 automatically clears legacy overflow"), RecoverableOverflow.bLegacyWarehouseOverflow);
	TestTrue(TEXT("recovered normal-capacity collection validates"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(RecoverableOverflow, Roster));

	FGameXXKEquipmentCollectionState PersistedLoadouts;
	const FName PersistedHeroWeapon = CreateChecked(*this, PersistedLoadouts, MakeRequest(
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentQuality::Common,
		2,
		EGameXXKEquipmentSlot::Weapon));
	const FName PersistedPartnerWeapon = CreateChecked(*this, PersistedLoadouts, MakeRequest(
		EGameXXKEquipmentSet::XuanJia,
		EGameXXKEquipmentQuality::Rare,
		3,
		EGameXXKEquipmentSlot::Weapon));
	const FName PersistedSwapWeapon = CreateChecked(*this, PersistedLoadouts, MakeRequest(
		EGameXXKEquipmentSet::ShiGu,
		EGameXXKEquipmentQuality::Epic,
		4,
		EGameXXKEquipmentSlot::Weapon));
	const FName PersistedPartnerId = Roster.PermanentCompanions[0].InstanceId;
	TestTrue(TEXT("persisted hero fixture equips"), FGameXXKEquipmentRules::EquipInstance(
		PersistedLoadouts,
		Roster,
		FGameXXKEquipmentRules::HeroCharacterId(),
		EGameXXKEquipmentSlot::Weapon,
		PersistedHeroWeapon).bSucceeded);
	TestTrue(TEXT("persisted partner fixture equips"), FGameXXKEquipmentRules::EquipInstance(
		PersistedLoadouts,
		Roster,
		PersistedPartnerId,
		EGameXXKEquipmentSlot::Weapon,
		PersistedPartnerWeapon).bSucceeded);
	FGameXXKEquipmentCollectionState ReloadedLoadouts;
	TestTrue(TEXT("hero and partner loadouts deserialize"), DeserializeCollection(SerializeCollection(PersistedLoadouts), ReloadedLoadouts));
	FGameXXKEquipmentCollectionState UninterruptedLoadouts = PersistedLoadouts;
	auto ContinueLoadoutMutations = [&Roster, PersistedPartnerId, PersistedSwapWeapon](FGameXXKEquipmentCollectionState& Collection)
	{
		TArray<FGameXXKEquipmentTransactionResult> Results;
		Results.Add(FGameXXKEquipmentRules::EquipInstance(
			Collection,
			Roster,
			FGameXXKEquipmentRules::HeroCharacterId(),
			EGameXXKEquipmentSlot::Weapon,
			PersistedSwapWeapon));
		Results.Add(FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(Collection, PersistedPartnerId));
		return Results;
	};
	const TArray<FGameXXKEquipmentTransactionResult> UninterruptedResults = ContinueLoadoutMutations(UninterruptedLoadouts);
	const TArray<FGameXXKEquipmentTransactionResult> ReloadedResults = ContinueLoadoutMutations(ReloadedLoadouts);
	TestEqual(TEXT("uninterrupted and reloaded swap result success matches"), ReloadedResults[0].bSucceeded, UninterruptedResults[0].bSucceeded);
	TestEqual(TEXT("uninterrupted and reloaded swap affected IDs match"), ReloadedResults[0].AffectedInstanceIds, UninterruptedResults[0].AffectedInstanceIds);
	TestEqual(TEXT("uninterrupted and reloaded return result success matches"), ReloadedResults[1].bSucceeded, UninterruptedResults[1].bSucceeded);
	TestEqual(TEXT("uninterrupted and reloaded return affected IDs match"), ReloadedResults[1].AffectedInstanceIds, UninterruptedResults[1].AffectedInstanceIds);
	TestEqual(TEXT("save/reload continuation produces a byte-identical collection"), SerializeCollection(ReloadedLoadouts), SerializeCollection(UninterruptedLoadouts));
	TestTrue(TEXT("reloaded swap/return preserves owner redundancy"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(ReloadedLoadouts, Roster));
	const FGameXXKEquipmentInstance* ReloadedHeroSwap = FGameXXKEquipmentRules::FindInstance(ReloadedLoadouts, PersistedSwapWeapon);
	const FGameXXKEquipmentInstance* ReloadedPartnerReturn = FGameXXKEquipmentRules::FindInstance(ReloadedLoadouts, PersistedPartnerWeapon);
	TestNotNull(TEXT("reloaded hero swap instance resolves"), ReloadedHeroSwap);
	TestNotNull(TEXT("reloaded partner return instance resolves"), ReloadedPartnerReturn);
	if (ReloadedHeroSwap && ReloadedPartnerReturn)
	{
		TestEqual(TEXT("reloaded swap owner remains Hero"), ReloadedHeroSwap->OwnerKind, EGameXXKEquipmentOwnerKind::Hero);
		TestEqual(TEXT("reloaded swap owner character remains Player"), ReloadedHeroSwap->OwnerCharacterId, FGameXXKEquipmentRules::HeroCharacterId());
		TestEqual(TEXT("reloaded returned partner item owner becomes Warehouse"), ReloadedPartnerReturn->OwnerKind, EGameXXKEquipmentOwnerKind::Warehouse);
		TestTrue(TEXT("reloaded returned partner item clears redundant character owner"), ReloadedPartnerReturn->OwnerCharacterId.IsNone());
	}

	FGameXXKEquipmentCollectionState ReturnAllCollection;
	for (uint8 SlotValue = 1; SlotValue <= 6; ++SlotValue)
	{
		const EGameXXKEquipmentSlot Slot = static_cast<EGameXXKEquipmentSlot>(SlotValue);
		const FName Id = CreateChecked(*this, ReturnAllCollection, MakeRequest(EGameXXKEquipmentSet::ZhuiFeng, EGameXXKEquipmentQuality::Common, 1, Slot));
		TestTrue(TEXT("return-all fixture equips"), FGameXXKEquipmentRules::EquipInstance(ReturnAllCollection, Roster, FGameXXKEquipmentRules::HeroCharacterId(), Slot, Id).bSucceeded);
	}
	const FGameXXKEquipmentTransactionResult ReturnResult = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(ReturnAllCollection, FGameXXKEquipmentRules::HeroCharacterId());
	TestTrue(TEXT("return-all succeeds with six free slots"), ReturnResult.bSucceeded);
	TestEqual(TEXT("return-all reports all six affected IDs"), ReturnResult.AffectedInstanceIds.Num(), 6);
	TestEqual(TEXT("return-all appends in six-slot order"), ReturnAllCollection.WarehouseInstanceIds, ReturnResult.AffectedInstanceIds);
	TestFalse(TEXT("return-all removes empty loadout index"), ReturnAllCollection.CharacterLoadouts.Contains(FGameXXKEquipmentRules::HeroCharacterId()));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentRulesValidationRollbackTest,
	"GameXXK.Equipment.Rules.ValidationAndRollback",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentRulesValidationRollbackTest::RunTest(const FString& Parameters)
{
	FGameXXKCompanionRosterState Roster;
	Roster.PermanentCompanions.Add(MakeCompanion(0));
	FGameXXKEquipmentCollectionState Base;
	const FName WeaponId = CreateChecked(*this, Base, MakeRequest(EGameXXKEquipmentSet::PoJun, EGameXXKEquipmentQuality::Epic, 10, EGameXXKEquipmentSlot::Weapon));
	if (WeaponId.IsNone())
	{
		AddError(TEXT("CreateRolledInstance must create the validation baseline"));
		return false;
	}
	TestTrue(TEXT("baseline validates"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(Base, Roster));

	auto ExpectInvalid = [this](const FString& Label, FGameXXKEquipmentCollectionState Corrupt)
	{
		FString Error;
		TestFalse(Label, FGameXXKEquipmentRules::ValidateCollectionState(Corrupt, &Error));
		TestFalse(Label + TEXT(" produces an error"), Error.IsEmpty());
	};

	FGameXXKEquipmentCollectionState DuplicateId = Base;
	const FGameXXKEquipmentInstance DuplicateInstance = DuplicateId.EquipmentInstances[0];
	DuplicateId.EquipmentInstances.Add(DuplicateInstance);
	ExpectInvalid(TEXT("duplicate instance IDs are rejected"), DuplicateId);

	FGameXXKEquipmentCollectionState DuplicateWarehouse = Base;
	DuplicateWarehouse.WarehouseInstanceIds.Add(WeaponId);
	ExpectInvalid(TEXT("duplicate warehouse indexes are rejected"), DuplicateWarehouse);

	FGameXXKEquipmentCollectionState DoubleOwner = Base;
	DoubleOwner.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).WeaponInstanceId = WeaponId;
	ExpectInvalid(TEXT("same instance cannot be warehouse and loadout owned"), DoubleOwner);

	FGameXXKEquipmentCollectionState MissingLocation = Base;
	MissingLocation.WarehouseInstanceIds.Reset();
	ExpectInvalid(TEXT("every instance needs exactly one authoritative location"), MissingLocation);

	FGameXXKEquipmentCollectionState BadOwner = Base;
	BadOwner.EquipmentInstances[0].OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	BadOwner.EquipmentInstances[0].OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	ExpectInvalid(TEXT("redundant warehouse owner corruption is rejected"), BadOwner);

	FGameXXKEquipmentCollectionState BadSlot = Base;
	BadSlot.WarehouseInstanceIds.Reset();
	BadSlot.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).HeadInstanceId = WeaponId;
	BadSlot.EquipmentInstances[0].OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	BadSlot.EquipmentInstances[0].OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	ExpectInvalid(TEXT("catalog slot and loadout slot must match"), BadSlot);

	FGameXXKEquipmentCollectionState BadAffixCount = Base;
	BadAffixCount.EquipmentInstances[0].RolledAffixes.Pop();
	ExpectInvalid(TEXT("epic equipment requires exactly three affixes"), BadAffixCount);

	FGameXXKEquipmentCollectionState BadTier = Base;
	BadTier.EquipmentInstances[0].RolledAffixes[0].Tier = EGameXXKAffixTier::Invalid;
	ExpectInvalid(TEXT("invalid affix tier is rejected"), BadTier);

	auto SetLegalMagnitudeForTier = [](FGameXXKEquipmentAffixRoll& Roll, const EGameXXKAffixTier Tier)
	{
		Roll.Tier = Tier;
		Roll.Magnitude = FGameXXKAffixCatalog::GetMagnitudeRange(Roll.Unit, Tier).Minimum;
	};
	FGameXXKEquipmentCollectionState CommonQuality;
	const FName CommonQualityId = CreateChecked(*this, CommonQuality, MakeRequest(
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentQuality::Common,
		1,
		EGameXXKEquipmentSlot::Weapon));
	if (CommonQualityId.IsNone())
	{
		AddError(TEXT("common-quality tier-boundary fixture must be created"));
		return false;
	}
	FGameXXKEquipmentCollectionState CommonToRare = CommonQuality;
	SetLegalMagnitudeForTier(CommonToRare.EquipmentInstances[0].RolledAffixes[0], EGameXXKAffixTier::Rare);
	ExpectInvalid(TEXT("Common quality rejects an otherwise legal Rare-tier affix"), CommonToRare);
	FGameXXKEquipmentCollectionState CommonToEpic = CommonQuality;
	SetLegalMagnitudeForTier(CommonToEpic.EquipmentInstances[0].RolledAffixes[0], EGameXXKAffixTier::Epic);
	ExpectInvalid(TEXT("Common quality rejects an otherwise legal Epic-tier affix"), CommonToEpic);

	FGameXXKEquipmentCollectionState RareQuality;
	const FName RareQualityId = CreateChecked(*this, RareQuality, MakeRequest(
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentQuality::Rare,
		1,
		EGameXXKEquipmentSlot::Weapon));
	if (RareQualityId.IsNone())
	{
		AddError(TEXT("rare-quality tier-boundary fixture must be created"));
		return false;
	}
	FGameXXKEquipmentCollectionState RareToEpic = RareQuality;
	SetLegalMagnitudeForTier(RareToEpic.EquipmentInstances[0].RolledAffixes[0], EGameXXKAffixTier::Epic);
	ExpectInvalid(TEXT("Rare quality rejects an otherwise legal Epic-tier affix"), RareToEpic);

	FGameXXKEquipmentCollectionState RarePendingToEpic = RareQuality;
	RarePendingToEpic.NextReforgeOrdinal = 1;
	RarePendingToEpic.PendingReforge.bActive = true;
	RarePendingToEpic.PendingReforge.InstanceId = RareQualityId;
	RarePendingToEpic.PendingReforge.AffixIndex = 0;
	RarePendingToEpic.PendingReforge.OriginalAffix = RarePendingToEpic.EquipmentInstances[0].RolledAffixes[0];
	RarePendingToEpic.PendingReforge.CandidateAffix = RarePendingToEpic.PendingReforge.OriginalAffix;
	SetLegalMagnitudeForTier(RarePendingToEpic.PendingReforge.CandidateAffix, EGameXXKAffixTier::Epic);
	RarePendingToEpic.PendingReforge.PaidRefinementSand = 1;
	RarePendingToEpic.PendingReforge.ConsumedReforgeOrdinal = 0;
	ExpectInvalid(TEXT("Rare quality rejects an Epic-tier pending reforge candidate"), RarePendingToEpic);

	FGameXXKEquipmentCollectionState BadMagnitude = Base;
	BadMagnitude.EquipmentInstances[0].RolledAffixes[0].Magnitude = MAX_int32;
	ExpectInvalid(TEXT("out-of-catalog magnitude is rejected"), BadMagnitude);

	FGameXXKEquipmentCollectionState BadUnit = Base;
	BadUnit.EquipmentInstances[0].RolledAffixes[0].Unit = EGameXXKEquipmentMagnitudeUnit::Invalid;
	ExpectInvalid(TEXT("affix unit corruption is rejected"), BadUnit);

	FGameXXKEquipmentCollectionState BadSetAffix = Base;
	BadSetAffix.EquipmentInstances[0].RolledAffixes[0].AffixId = TEXT("Affix.XuanJia.ArmorGain");
	BadSetAffix.EquipmentInstances[0].RolledAffixes[0].Unit = EGameXXKEquipmentMagnitudeUnit::BasisPoints;
	ExpectInvalid(TEXT("another set's affix is rejected"), BadSetAffix);

	FGameXXKEquipmentCollectionState DuplicateModifier = Base;
	DuplicateModifier.EquipmentInstances[0].RolledAffixes[1] = DuplicateModifier.EquipmentInstances[0].RolledAffixes[0];
	ExpectInvalid(TEXT("duplicate ModifierKind is rejected"), DuplicateModifier);

	FGameXXKEquipmentCollectionState BadEnhancement = Base;
	BadEnhancement.EquipmentInstances[0].EnhancementLevel = 11;
	ExpectInvalid(TEXT("enhancement above ten is rejected"), BadEnhancement);

	FGameXXKEquipmentCollectionState BadLevel = Base;
	BadLevel.EquipmentInstances[0].ItemLevel = 21;
	ExpectInvalid(TEXT("item level above twenty is rejected"), BadLevel);

	FGameXXKEquipmentCollectionState BadPending = Base;
	BadPending.PendingReforge.bActive = true;
	BadPending.PendingReforge.InstanceId = WeaponId;
	BadPending.PendingReforge.AffixIndex = 99;
	ExpectInvalid(TEXT("stale pending reforge index is rejected"), BadPending);

	FGameXXKEquipmentCollectionState Overflow = MakeLegacyOverflow(201);
	TestTrue(TEXT("flagged legacy 201-item overflow remains loadable"), FGameXXKEquipmentRules::ValidateCollectionState(Overflow));
	TestFalse(TEXT("legacy overflow blocks new acquisition capacity"), FGameXXKEquipmentRules::HasWarehouseCapacity(Overflow));
	FGameXXKEquipmentCollectionState UnflaggedOverflow = Overflow;
	UnflaggedOverflow.bLegacyWarehouseOverflow = false;
	ExpectInvalid(TEXT("201 items without migration flag are rejected"), UnflaggedOverflow);
	FGameXXKEquipmentCollectionState ModernOverflow = Overflow;
	ModernOverflow.EquipmentInstances.Last() = Base.EquipmentInstances[0];
	ModernOverflow.EquipmentInstances.Last().InstanceId = ModernOverflow.WarehouseInstanceIds.Last();
	ExpectInvalid(TEXT("migration overflow cannot conceal a modern over-cap item"), ModernOverflow);

	FGameXXKEquipmentCollectionState Owned = Base;
	TestTrue(TEXT("known permanent companion can equip"), FGameXXKEquipmentRules::EquipInstance(Owned, Roster, Roster.PermanentCompanions[0].InstanceId, EGameXXKEquipmentSlot::Weapon, WeaponId).bSucceeded);
	FGameXXKCompanionRosterState EmptyRoster;
	FString RosterError;
	TestFalse(TEXT("expired companion owner is rejected against current roster"), FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(Owned, EmptyRoster, &RosterError));

	FGameXXKEquipmentCollectionState InvalidRequestCollection;
	const TArray<uint8> InvalidRequestBefore = SerializeCollection(InvalidRequestCollection);
	FName InvalidRequestId(TEXT("MustClear"));
	FGameXXKEquipmentCreateRequest InvalidRequest = MakeRequest();
	InvalidRequest.ItemLevel = 0;
	TestFalse(TEXT("invalid creation request fails"), FGameXXKEquipmentRules::CreateRolledInstance(InvalidRequestCollection, InvalidRequest, InvalidRequestId));
	TestBytesEqual(*this, TEXT("invalid request failure is byte-for-byte atomic"), InvalidRequestBefore, InvalidRequestCollection);

	FGameXXKEquipmentCollectionState ExhaustedOrdinalCollection;
	ExhaustedOrdinalCollection.NextInstanceOrdinal = MAX_int32;
	const TArray<uint8> ExhaustedOrdinalBefore = SerializeCollection(ExhaustedOrdinalCollection);
	FName ExhaustedOrdinalId(TEXT("MustClear"));
	FString ExhaustedOrdinalError;
	TestFalse(TEXT("creation rejects an instance ordinal that cannot be safely incremented"), FGameXXKEquipmentRules::CreateRolledInstance(
		ExhaustedOrdinalCollection,
		MakeRequest(),
		ExhaustedOrdinalId,
		&ExhaustedOrdinalError));
	TestTrue(TEXT("exhausted ordinal failure clears output instance ID"), ExhaustedOrdinalId.IsNone());
	TestFalse(TEXT("exhausted ordinal failure reports an error"), ExhaustedOrdinalError.IsEmpty());
	TestBytesEqual(*this, TEXT("exhausted ordinal failure is byte-for-byte atomic"), ExhaustedOrdinalBefore, ExhaustedOrdinalCollection);

	const TArray<uint8> MissingEquipBefore = SerializeCollection(Base);
	const FGameXXKEquipmentTransactionResult MissingEquip = FGameXXKEquipmentRules::EquipInstance(Base, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, TEXT("EquipmentInstance.Missing"));
	TestFalse(TEXT("missing equip instance fails"), MissingEquip.bSucceeded);
	TestBytesEqual(*this, TEXT("missing equip instance preserves complete snapshot"), MissingEquipBefore, Base);

	const TArray<uint8> SlotMismatchBefore = SerializeCollection(Base);
	const FGameXXKEquipmentTransactionResult SlotMismatch = FGameXXKEquipmentRules::EquipInstance(Base, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Head, WeaponId);
	TestEqual(TEXT("wrong-slot equip reports SlotMismatch"), SlotMismatch.Error, EGameXXKEquipmentTransactionError::SlotMismatch);
	TestBytesEqual(*this, TEXT("slot mismatch preserves complete snapshot"), SlotMismatchBefore, Base);

	FGameXXKEquipmentCollectionState AlreadyEquipped = Base;
	TestTrue(TEXT("already-equipped failure fixture equips once"), FGameXXKEquipmentRules::EquipInstance(AlreadyEquipped, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, WeaponId).bSucceeded);
	const TArray<uint8> AlreadyEquippedBefore = SerializeCollection(AlreadyEquipped);
	const FGameXXKEquipmentTransactionResult AlreadyEquippedResult = FGameXXKEquipmentRules::EquipInstance(AlreadyEquipped, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, WeaponId);
	TestEqual(TEXT("an equipped instance cannot be equipped a second time"), AlreadyEquippedResult.Error, EGameXXKEquipmentTransactionError::ItemNotInWarehouse);
	TestBytesEqual(*this, TEXT("already-equipped rejection preserves complete snapshot"), AlreadyEquippedBefore, AlreadyEquipped);

	const FGameXXKEquipmentTransactionResult EmptySlotUnequip = FGameXXKEquipmentRules::UnequipInstance(Base, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon);
	TestEqual(TEXT("unequipping an empty slot reports InstanceMissing"), EmptySlotUnequip.Error, EGameXXKEquipmentTransactionError::InstanceMissing);
	TestBytesEqual(*this, TEXT("empty-slot unequip preserves complete snapshot"), SlotMismatchBefore, Base);

	const FGameXXKEquipmentTransactionResult InvalidOwnerUnequip = FGameXXKEquipmentRules::UnequipInstance(Base, NAME_None, EGameXXKEquipmentSlot::Weapon);
	TestEqual(TEXT("empty unequip owner reports InvalidOwner"), InvalidOwnerUnequip.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestBytesEqual(*this, TEXT("invalid-owner unequip preserves complete snapshot"), SlotMismatchBefore, Base);

	const FGameXXKEquipmentTransactionResult InvalidOwnerReturn = FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(Base, NAME_None);
	TestEqual(TEXT("empty return-all owner reports InvalidOwner"), InvalidOwnerReturn.Error, EGameXXKEquipmentTransactionError::InvalidOwner);
	TestBytesEqual(*this, TEXT("invalid-owner return-all preserves complete snapshot"), SlotMismatchBefore, Base);

	FGameXXKEquipmentCollectionState CorruptMutation = Base;
	CorruptMutation.EquipmentInstances[0].OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	const TArray<uint8> CorruptBefore = SerializeCollection(CorruptMutation);
	FName CorruptCreateId(TEXT("MustClear"));
	TestFalse(TEXT("creation rejects a corrupt input collection"), FGameXXKEquipmentRules::CreateRolledInstance(CorruptMutation, MakeRequest(), CorruptCreateId));
	TestBytesEqual(*this, TEXT("corrupt-input creation preserves complete snapshot"), CorruptBefore, CorruptMutation);
	TestEqual(TEXT("equip rejects a corrupt input collection"), FGameXXKEquipmentRules::EquipInstance(CorruptMutation, Roster, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon, WeaponId).Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestBytesEqual(*this, TEXT("corrupt-input equip preserves complete snapshot"), CorruptBefore, CorruptMutation);
	TestEqual(TEXT("unequip rejects a corrupt input collection"), FGameXXKEquipmentRules::UnequipInstance(CorruptMutation, FGameXXKEquipmentRules::HeroCharacterId(), EGameXXKEquipmentSlot::Weapon).Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestBytesEqual(*this, TEXT("corrupt-input unequip preserves complete snapshot"), CorruptBefore, CorruptMutation);
	TestEqual(TEXT("return-all rejects a corrupt input collection"), FGameXXKEquipmentRules::ReturnAllEquipmentToWarehouse(CorruptMutation, FGameXXKEquipmentRules::HeroCharacterId()).Error, EGameXXKEquipmentTransactionError::CollectionInvalid);
	TestBytesEqual(*this, TEXT("corrupt-input return-all preserves complete snapshot"), CorruptBefore, CorruptMutation);
	return true;
}

#endif
