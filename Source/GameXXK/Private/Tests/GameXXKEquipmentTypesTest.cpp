#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentTypes.h"
#include "GameXXKMVPRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEquipmentTypesContractTest,
	"GameXXK.Equipment.Types.Contract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEquipmentTypesContractTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("six equipment slots use contiguous saved values"), static_cast<uint8>(EGameXXKEquipmentSlot::Accessory), static_cast<uint8>(6));
	TestEqual(TEXT("Starter and six combat sets follow Legacy in the saved enum"), static_cast<uint8>(EGameXXKEquipmentSet::ShanHe), static_cast<uint8>(8));
	TestEqual(TEXT("three equipment qualities use contiguous saved values"), static_cast<uint8>(EGameXXKEquipmentQuality::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("three affix tiers use contiguous saved values"), static_cast<uint8>(EGameXXKAffixTier::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("the approved five universal and thirty set-specific modifier kinds are present"), static_cast<uint8>(EGameXXKEquipmentModifierKind::TeamTerrainPower), static_cast<uint8>(35));

	const FGameXXKEquipmentCollectionState Collection;
	TestEqual(TEXT("a new collection starts at equipment schema one"), Collection.EquipmentSchemaVersion, 1);
	TestNotEqual(TEXT("a new collection has a deterministic non-zero seed"), Collection.CollectionSeed, 0);
	TestEqual(TEXT("a new collection has no instances"), Collection.EquipmentInstances.Num(), 0);
	TestEqual(TEXT("a new collection has no ordered warehouse ids"), Collection.WarehouseInstanceIds.Num(), 0);
	TestEqual(TEXT("a new collection has no character loadouts"), Collection.CharacterLoadouts.Num(), 0);
	TestFalse(TEXT("a new collection has no pending reforge"), Collection.PendingReforge.bActive);
	TestTrue(TEXT("a new collection pending reforge has no instance"), Collection.PendingReforge.InstanceId.IsNone());
	TestEqual(TEXT("a new collection pending reforge has no affix index"), Collection.PendingReforge.AffixIndex, INDEX_NONE);

	const FGameXXKRuntimeState RuntimeState;
	TestEqual(TEXT("runtime equipment collection uses the same schema default"), RuntimeState.EquipmentCollection.EquipmentSchemaVersion, 1);
	TestEqual(TEXT("runtime equipment collection starts empty"), RuntimeState.EquipmentCollection.EquipmentInstances.Num(), 0);
	return true;
}

#endif
