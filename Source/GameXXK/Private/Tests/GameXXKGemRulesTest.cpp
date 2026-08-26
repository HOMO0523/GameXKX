#include "Misc/AutomationTest.h"

#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"
#include "Serialization/MemoryReader.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
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
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGemCatalogTest,
	"GameXXK.Equipment.Gems.CatalogAndIcons",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGemCatalogTest::RunTest(const FString& Parameters)
{
	const TCHAR* QualityNames[] = {
		TEXT("普通"), TEXT("稀有"), TEXT("珍稀"), TEXT("传奇"), TEXT("不朽"),
		TEXT("至宝"), TEXT("超凡"), TEXT("天界"), TEXT("登神"), TEXT("宇宙"),
	};
	const int32 ExpectedAttackDefense[] = {1, 2, 4, 8, 16, 32, 64, 128, 256, 512};
	const int32 ExpectedHealth[] = {10, 20, 40, 80, 160, 320, 640, 1280, 2560, 5120};
	const TArray<FName> GemIds = FGameXXKGemRules::GetAllItemIds();
	TestEqual(TEXT("three types times ten qualities"), GemIds.Num(), 30);
	TSet<FName> UniqueIds;
	for (const FName GemId : GemIds)
	{
		UniqueIds.Add(GemId);
	}
	TestEqual(TEXT("all gem item ids are unique"), UniqueIds.Num(), 30);

	const TArray<FName> KnownIds = UGameXXKMVPRules::GetKnownItemIds();
	for (int32 TypeRank = 1; TypeRank <= 3; ++TypeRank)
	{
		const EGameXXKGemType Type = static_cast<EGameXXKGemType>(TypeRank);
		for (int32 Rank = 1; Rank <= 10; ++Rank)
		{
			const EGameXXKGemQuality Quality = FGameXXKGemRules::QualityFromRank(Rank);
			const FName ItemId = FGameXXKGemRules::MakeItemId(Type, Quality);
			EGameXXKGemType ParsedType;
			EGameXXKGemQuality ParsedQuality;
			TestTrue(FString::Printf(TEXT("gem item %d/%d parses"), TypeRank, Rank),
				FGameXXKGemRules::TryParseItemId(ItemId, ParsedType, ParsedQuality));
			TestEqual(TEXT("parsed gem type is exact"), ParsedType, Type);
			TestEqual(TEXT("parsed gem quality is exact"), ParsedQuality, Quality);
			TestEqual(TEXT("gem quality display is exact"),
				FGameXXKGemRules::GetQualityDisplayName(Quality).ToString(), FString(QualityNames[Rank - 1]));
			const int32 ExpectedBonus = Type == EGameXXKGemType::MaxHealth
				? ExpectedHealth[Rank - 1]
				: ExpectedAttackDefense[Rank - 1];
			TestEqual(TEXT("gem stat bonus is exact"), FGameXXKGemRules::GetStatBonus(Type, Quality), ExpectedBonus);
			TestEqual(TEXT("next gem quality is exact"),
				FGameXXKGemRules::GetNextQuality(Quality),
				Rank == 10 ? EGameXXKGemQuality::Invalid : FGameXXKGemRules::QualityFromRank(Rank + 1));
			TestTrue(TEXT("gem item is registered in known inventory ids"), KnownIds.Contains(ItemId));
			bool bFound = false;
			const FGameXXKItemDef ItemDef = UGameXXKMVPRules::GetItemDef(ItemId, bFound);
			TestTrue(TEXT("gem item definition resolves"), bFound);
			TestEqual(TEXT("gem item is a stackable material"), ItemDef.Kind, EGameXXKItemKind::Material);
			const FSoftObjectPath IconPath = FGameXXKGemRules::GetIconTexturePath(Type, Quality);
			TestTrue(TEXT("gem icon path is valid"), IconPath.IsValid());
			TestEqual(TEXT("item-id icon lookup is identical"),
				FGameXXKGemRules::GetIconTexturePathForItemId(ItemId).ToString(), IconPath.ToString());
			TestNotNull(TEXT("imported gem icon loads"), IconPath.TryLoad());
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGemSocketProjectionTest,
	"GameXXK.Equipment.Gems.SocketCapacityProjectionAndSave",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGemSocketProjectionTest::RunTest(const FString& Parameters)
{
	const int32 ExpectedCapacities[] = {1, 1, 1, 1, 1, 2, 3, 4, 5, 6};
	FGameXXKEquipmentCollectionState Collection;
	Collection.EquipmentSchemaVersion = 1;
	Collection.CollectionSeed = 0x47454D53;
	TArray<FName> InstanceIds;
	for (int32 Rank = 1; Rank <= 10; ++Rank)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::ShanHe;
		Request.Quality = FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(Rank);
		Request.ItemLevel = Rank;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		FString Error;
		TestTrue(FString::Printf(TEXT("quality %d equipment creates: %s"), Rank, *Error),
			FGameXXKEquipmentRules::CreateRolledInstance(Collection, Request, InstanceId, &Error));
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(Collection, InstanceId);
		if (!TestNotNull(TEXT("created socket equipment resolves"), Instance))
		{
			return false;
		}
		TestEqual(TEXT("quality-derived socket capacity is exact"), Instance->SocketedGems.Num(), ExpectedCapacities[Rank - 1]);
		InstanceIds.Add(InstanceId);
	}

	const FName CosmicId = InstanceIds.Last();
	FGameXXKEquipmentInstance* Cosmic = Collection.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	if (!TestNotNull(TEXT("cosmic instance resolves"), Cosmic))
	{
		return false;
	}
	Cosmic->SocketedGems = {
		{EGameXXKGemType::Attack, EGameXXKGemQuality::Common},
		{EGameXXKGemType::Attack, EGameXXKGemQuality::Cosmic},
		{EGameXXKGemType::Defense, EGameXXKGemQuality::Rare},
		{EGameXXKGemType::MaxHealth, EGameXXKGemQuality::Common},
		{EGameXXKGemType::MaxHealth, EGameXXKGemQuality::Cosmic},
		{},
	};
	Collection.WarehouseInstanceIds.Remove(CosmicId);
	Cosmic->OwnerKind = EGameXXKEquipmentOwnerKind::Hero;
	Cosmic->OwnerCharacterId = FGameXXKEquipmentRules::HeroCharacterId();
	Collection.CharacterLoadouts.FindOrAdd(FGameXXKEquipmentRules::HeroCharacterId()).WeaponInstanceId = CosmicId;
	FString Error;
	TestTrue(FString::Printf(TEXT("socketed collection validates: %s"), *Error),
		FGameXXKEquipmentRules::ValidateCollectionState(Collection, &Error));

	FGameXXKEquipmentCollectionState EmptySocketCollection = Collection;
	FGameXXKEquipmentInstance* EmptyCosmic = EmptySocketCollection.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	for (FGameXXKSocketedGem& Gem : EmptyCosmic->SocketedGems)
	{
		Gem = FGameXXKSocketedGem();
	}
	FGameXXKCharacterStats Bare;
	Bare.MaxHealth = 100;
	Bare.MaxMana = 50;
	Bare.Attack = 10;
	Bare.Defense = 10;
	Bare.Speed = 5;
	FGameXXKEquipmentLoadoutSnapshot WithGems;
	FGameXXKEquipmentLoadoutSnapshot WithoutGems;
	TestTrue(TEXT("socketed loadout projects"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
		Collection, FGameXXKEquipmentRules::HeroCharacterId(), Bare, WithGems, &Error));
	TestTrue(TEXT("empty-socket loadout projects"), FGameXXKEquipmentRules::BuildLoadoutSnapshot(
		EmptySocketCollection, FGameXXKEquipmentRules::HeroCharacterId(), Bare, WithoutGems, &Error));
	TestEqual(TEXT("socket attack total"), WithGems.SocketGemFlatStats.Attack, 513);
	TestEqual(TEXT("socket defense total"), WithGems.SocketGemFlatStats.Defense, 2);
	TestEqual(TEXT("socket max-health total"), WithGems.SocketGemFlatStats.MaxHealth, 5130);
	TestEqual(TEXT("socket attack is a final flat delta"), WithGems.AttributesBeforeRoute.Attack - WithoutGems.AttributesBeforeRoute.Attack, 513);
	TestEqual(TEXT("socket defense is a final flat delta"), WithGems.AttributesBeforeRoute.Defense - WithoutGems.AttributesBeforeRoute.Defense, 2);
	TestEqual(TEXT("socket health is a final flat delta"), WithGems.AttributesBeforeRoute.MaxHealth - WithoutGems.AttributesBeforeRoute.MaxHealth, 5130);

	const TArray<uint8> Bytes = SerializeCollection(Collection);
	FGameXXKEquipmentCollectionState Reloaded;
	FMemoryReader Reader(Bytes, true);
	FObjectAndNameAsStringProxyArchive Archive(Reader, false);
	Archive.ArIsSaveGame = true;
	FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Archive, &Reloaded, nullptr);
	const FGameXXKEquipmentInstance* ReloadedCosmic = FGameXXKEquipmentRules::FindInstance(Reloaded, CosmicId);
	TestNotNull(TEXT("socketed instance survives save serialization"), ReloadedCosmic);
	if (ReloadedCosmic)
	{
		TestEqual(TEXT("all six sockets survive save serialization"), ReloadedCosmic->SocketedGems.Num(), 6);
		TestEqual(TEXT("cosmic health gem survives save serialization"), ReloadedCosmic->SocketedGems[4].Quality, EGameXXKGemQuality::Cosmic);
	}

	FGameXXKEquipmentCollectionState MissingSockets = Collection;
	FGameXXKEquipmentInstance* MissingCosmic = MissingSockets.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	MissingCosmic->SocketedGems.SetNum(1);
	const FGameXXKSocketedGem PreservedFirst = MissingCosmic->SocketedGems[0];
	TestTrue(TEXT("missing quality sockets normalize"), FGameXXKEquipmentRules::NormalizeSocketArrays(MissingSockets, &Error));
	MissingCosmic = MissingSockets.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	TestEqual(TEXT("normalization appends to cosmic capacity"), MissingCosmic->SocketedGems.Num(), 6);
	TestEqual(TEXT("normalization preserves existing first socket type"), MissingCosmic->SocketedGems[0].Type, PreservedFirst.Type);

	FGameXXKEquipmentCollectionState TooManySockets = Collection;
	FGameXXKEquipmentInstance* OverCapacity = TooManySockets.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	OverCapacity->SocketedGems.AddDefaulted();
	const TArray<uint8> BeforeRejectedNormalize = SerializeCollection(TooManySockets);
	TestFalse(TEXT("over-capacity sockets reject normalization"), FGameXXKEquipmentRules::NormalizeSocketArrays(TooManySockets, &Error));
	TestEqual(TEXT("failed socket normalization is atomic"), SerializeCollection(TooManySockets), BeforeRejectedNormalize);

	FGameXXKEquipmentCollectionState HalfGem = Collection;
	FGameXXKEquipmentInstance* HalfGemInstance = HalfGem.EquipmentInstances.FindByPredicate(
		[CosmicId](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == CosmicId; });
	HalfGemInstance->SocketedGems[0].Quality = EGameXXKGemQuality::Invalid;
	TestFalse(TEXT("half-populated gem rejects validation"), FGameXXKEquipmentRules::ValidateCollectionState(HalfGem, &Error));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKGemV25MigrationTest,
	"GameXXK.Equipment.Gems.V25AbsentSocketNormalization",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKGemV25MigrationTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* FixtureSubsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("gem v25 fixture starts with a saveable ordered party"),
		FixtureSubsystem && FixtureSubsystem->StartGame()))
	{
		return false;
	}
	FGameXXKRuntimeState Runtime = FixtureSubsystem->GetRuntimeStateCopy();
	FGameXXKEquipmentCreateRequest Request;
	Request.Set = EGameXXKEquipmentSet::PoJun;
	Request.Quality = EGameXXKEquipmentQuality::Treasure;
	Request.ItemLevel = 10;
	Request.bForceSlot = true;
	Request.ForcedSlot = EGameXXKEquipmentSlot::Accessory;
	FName InstanceId;
	FString Error;
	if (!TestTrue(TEXT("v25 migration fixture creates Treasure equipment"),
		FGameXXKEquipmentRules::CreateRolledInstance(Runtime.EquipmentCollection, Request, InstanceId, &Error)))
	{
		return false;
	}
	FGameXXKEquipmentInstance* Instance = Runtime.EquipmentCollection.EquipmentInstances.FindByPredicate(
		[InstanceId](const FGameXXKEquipmentInstance& Candidate) { return Candidate.InstanceId == InstanceId; });
	Instance->SocketedGems.SetNum(1);
	Instance->SocketedGems[0] = {EGameXXKGemType::Attack, EGameXXKGemQuality::Rare};
	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(Runtime);
	Source.SaveVersion = 25;
	Source.RuntimeState.Talents = FGameXXKTalentProgress();
	TestEqual(TEXT("socket migration source stays v25"), Source.SaveVersion, 25);
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(FString::Printf(TEXT("v25 absent sockets migrate: %s"), *Report.Error),
		FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	const FGameXXKEquipmentInstance* MigratedInstance = FGameXXKEquipmentRules::FindInstance(
		Migrated.RuntimeState.EquipmentCollection, InstanceId);
	if (!TestNotNull(TEXT("migrated Treasure instance resolves"), MigratedInstance))
	{
		return false;
	}
	TestEqual(TEXT("Treasure migration appends second socket"), MigratedInstance->SocketedGems.Num(), 2);
	TestEqual(TEXT("existing socket survives v25 normalization"), MigratedInstance->SocketedGems[0].Type, EGameXXKGemType::Attack);
	TestTrue(TEXT("appended socket is empty"), MigratedInstance->SocketedGems[1].IsEmpty());
	return true;
}

#endif
