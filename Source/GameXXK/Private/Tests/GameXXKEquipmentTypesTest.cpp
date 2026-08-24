#include "Misc/AutomationTest.h"

#include "GameXXKCardQualityRules.h"
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
	TestEqual(TEXT("equipment quality Common keeps saved value one"), static_cast<uint8>(EGameXXKEquipmentQuality::Common), static_cast<uint8>(1));
	TestEqual(TEXT("equipment quality Rare keeps saved value two"), static_cast<uint8>(EGameXXKEquipmentQuality::Rare), static_cast<uint8>(2));
	TestEqual(TEXT("equipment quality Epic keeps saved value three"), static_cast<uint8>(EGameXXKEquipmentQuality::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("equipment quality Legendary appends at saved value four"), static_cast<uint8>(EGameXXKEquipmentQuality::Legendary), static_cast<uint8>(4));
	TestEqual(TEXT("equipment quality Cosmic appends at saved value ten"), static_cast<uint8>(EGameXXKEquipmentQuality::Cosmic), static_cast<uint8>(10));
	TestEqual(TEXT("affix tier Common keeps saved value one"), static_cast<uint8>(EGameXXKAffixTier::Common), static_cast<uint8>(1));
	TestEqual(TEXT("affix tier Rare keeps saved value two"), static_cast<uint8>(EGameXXKAffixTier::Rare), static_cast<uint8>(2));
	TestEqual(TEXT("affix tier Epic keeps saved value three"), static_cast<uint8>(EGameXXKAffixTier::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("affix tier Legendary appends at saved value four"), static_cast<uint8>(EGameXXKAffixTier::Legendary), static_cast<uint8>(4));
	TestEqual(TEXT("affix tier Cosmic appends at saved value ten"), static_cast<uint8>(EGameXXKAffixTier::Cosmic), static_cast<uint8>(10));
	TestEqual(TEXT("independent card quality Common stays at one"), static_cast<uint8>(EGameXXKCardQuality::Common), static_cast<uint8>(1));
	TestEqual(TEXT("independent card quality Rare stays at two"), static_cast<uint8>(EGameXXKCardQuality::Rare), static_cast<uint8>(2));
	TestEqual(TEXT("independent card quality Epic stays capped at three"), static_cast<uint8>(EGameXXKCardQuality::Epic), static_cast<uint8>(3));
	TestEqual(TEXT("independent Common card display stays Chinese"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Common).ToString(), FString(TEXT("普通")));
	TestEqual(TEXT("independent Rare card display stays Chinese"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Rare).ToString(), FString(TEXT("稀有")));
	TestEqual(TEXT("independent Epic card display stays Chinese"), FGameXXKCardQualityRules::GetDisplayName(EGameXXKCardQuality::Epic).ToString(), FString(TEXT("珍稀")));
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
