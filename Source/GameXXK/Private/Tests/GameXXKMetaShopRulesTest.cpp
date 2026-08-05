#include "Misc/AutomationTest.h"

#include "GameXXKMetaShopRules.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMetaShopCatalogTest,
	"GameXXK.MetaShop.Catalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMetaShopCatalogTest::RunTest(const FString& Parameters)
{
	const TArray<FGameXXKMetaShopProductDefinition>& Products = FGameXXKMetaShopRules::GetProducts();
	TestEqual(TEXT("seven fixed products"), Products.Num(), 7);
	if (Products.Num() != 7)
	{
		return false;
	}

	const EGameXXKMetaShopProductId ExpectedIds[] = {
		EGameXXKMetaShopProductId::PoJunPack,
		EGameXXKMetaShopProductId::XuanJiaPack,
		EGameXXKMetaShopProductId::QingNangPack,
		EGameXXKMetaShopProductId::ZhuiFengPack,
		EGameXXKMetaShopProductId::ShiGuPack,
		EGameXXKMetaShopProductId::ShanHePack,
		EGameXXKMetaShopProductId::CompanionPack,
	};
	const EGameXXKEquipmentSet ExpectedSets[] = {
		EGameXXKEquipmentSet::PoJun,
		EGameXXKEquipmentSet::XuanJia,
		EGameXXKEquipmentSet::QingNang,
		EGameXXKEquipmentSet::ZhuiFeng,
		EGameXXKEquipmentSet::ShiGu,
		EGameXXKEquipmentSet::ShanHe,
	};

	TSet<EGameXXKMetaShopProductId> UniqueIds;
	for (int32 Index = 0; Index < 6; ++Index)
	{
		const FGameXXKMetaShopProductDefinition& Product = Products[Index];
		TestEqual(FString::Printf(TEXT("equipment product %d keeps stable id"), Index), Product.ProductId, ExpectedIds[Index]);
		TestEqual(FString::Printf(TEXT("equipment product %d keeps set order"), Index), Product.EquipmentSet, ExpectedSets[Index]);
		TestEqual(FString::Printf(TEXT("equipment product %d is an equipment pack"), Index), Product.Kind, EGameXXKMetaShopProductKind::EquipmentPack);
		TestEqual(FString::Printf(TEXT("equipment product %d costs 100"), Index), Product.Price, FGameXXKMetaShopRules::EquipmentPackPrice);
		TestFalse(FString::Printf(TEXT("equipment product %d has a display name"), Index), Product.DisplayName.IsEmpty());
		TestFalse(FString::Printf(TEXT("equipment product %d has an icon path"), Index), Product.IconSoftPath.IsNull());
		UniqueIds.Add(Product.ProductId);
		TestTrue(FString::Printf(TEXT("equipment product %d can be found by id"), Index), FGameXXKMetaShopRules::FindProduct(Product.ProductId) == &Product);
	}

	const FGameXXKMetaShopProductDefinition& Companion = Products[6];
	TestEqual(TEXT("companion product keeps stable id"), Companion.ProductId, EGameXXKMetaShopProductId::CompanionPack);
	TestEqual(TEXT("companion is last"), Companion.Kind, EGameXXKMetaShopProductKind::CompanionPack);
	TestEqual(TEXT("companion has no equipment set"), Companion.EquipmentSet, EGameXXKEquipmentSet::Invalid);
	TestEqual(TEXT("companion costs 500"), Companion.Price, FGameXXKMetaShopRules::CompanionPackPrice);
	TestFalse(TEXT("companion has a display name"), Companion.DisplayName.IsEmpty());
	TestFalse(TEXT("companion has an icon path"), Companion.IconSoftPath.IsNull());
	UniqueIds.Add(Companion.ProductId);
	TestEqual(TEXT("all product ids are unique"), UniqueIds.Num(), 7);
	TestTrue(TEXT("companion can be found by id"), FGameXXKMetaShopRules::FindProduct(EGameXXKMetaShopProductId::CompanionPack) == &Companion);
	TestTrue(TEXT("invalid product id is not found"), FGameXXKMetaShopRules::FindProduct(EGameXXKMetaShopProductId::Invalid) == nullptr);
	return true;
}

#endif
