#include "GameXXKMetaShopRules.h"

namespace
{
	FGameXXKMetaShopProductDefinition MakeEquipmentPack(
		const EGameXXKMetaShopProductId ProductId,
		const EGameXXKEquipmentSet EquipmentSet,
		const FText& DisplayName,
		const TCHAR* IconObjectPath)
	{
		FGameXXKMetaShopProductDefinition Product;
		Product.ProductId = ProductId;
		Product.Kind = EGameXXKMetaShopProductKind::EquipmentPack;
		Product.EquipmentSet = EquipmentSet;
		Product.Price = FGameXXKMetaShopRules::EquipmentPackPrice;
		Product.DisplayName = DisplayName;
		Product.Description = NSLOCTEXT("GameXXKMetaShop", "EquipmentPackDescription", "随机获得对应套装的一个装备部位");
		Product.IconSoftPath = FSoftObjectPath(IconObjectPath);
		return Product;
	}

	FGameXXKMetaShopProductDefinition MakeCompanionPack()
	{
		FGameXXKMetaShopProductDefinition Product;
		Product.ProductId = EGameXXKMetaShopProductId::CompanionPack;
		Product.Kind = EGameXXKMetaShopProductKind::CompanionPack;
		Product.EquipmentSet = EGameXXKEquipmentSet::Invalid;
		Product.Price = FGameXXKMetaShopRules::CompanionPackPrice;
		Product.DisplayName = NSLOCTEXT("GameXXKMetaShop", "CompanionPackName", "伙伴包");
		Product.Description = NSLOCTEXT("GameXXKMetaShop", "CompanionPackDescription", "获得一名永久伙伴；满员时进入替换流程");
		Product.IconSoftPath = FSoftObjectPath(TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_CompanionPack.T_MetaShop_CompanionPack"));
		return Product;
	}

	TArray<FGameXXKMetaShopProductDefinition> BuildProducts()
	{
		return {
			MakeEquipmentPack(EGameXXKMetaShopProductId::PoJunPack, EGameXXKEquipmentSet::PoJun, NSLOCTEXT("GameXXKMetaShop", "PoJunPackName", "破军装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_PoJunPack.T_MetaShop_PoJunPack")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::XuanJiaPack, EGameXXKEquipmentSet::XuanJia, NSLOCTEXT("GameXXKMetaShop", "XuanJiaPackName", "玄甲装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_XuanJiaPack.T_MetaShop_XuanJiaPack")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::QingNangPack, EGameXXKEquipmentSet::QingNang, NSLOCTEXT("GameXXKMetaShop", "QingNangPackName", "青囊装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_QingNangPack.T_MetaShop_QingNangPack")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ZhuiFengPack, EGameXXKEquipmentSet::ZhuiFeng, NSLOCTEXT("GameXXKMetaShop", "ZhuiFengPackName", "追风装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ZhuiFengPack.T_MetaShop_ZhuiFengPack")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ShiGuPack, EGameXXKEquipmentSet::ShiGu, NSLOCTEXT("GameXXKMetaShop", "ShiGuPackName", "蚀骨装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShiGuPack.T_MetaShop_ShiGuPack")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ShanHePack, EGameXXKEquipmentSet::ShanHe, NSLOCTEXT("GameXXKMetaShop", "ShanHePackName", "山河装备包"), TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_ShanHePack.T_MetaShop_ShanHePack")),
			MakeCompanionPack(),
		};
	}
}

const TArray<FGameXXKMetaShopProductDefinition>& FGameXXKMetaShopRules::GetProducts()
{
	static const TArray<FGameXXKMetaShopProductDefinition> Products = BuildProducts();
	return Products;
}

const FGameXXKMetaShopProductDefinition* FGameXXKMetaShopRules::FindProduct(const EGameXXKMetaShopProductId ProductId)
{
	if (ProductId == EGameXXKMetaShopProductId::Invalid)
	{
		return nullptr;
	}
	return GetProducts().FindByPredicate(
		[ProductId](const FGameXXKMetaShopProductDefinition& Product)
		{
			return Product.ProductId == ProductId;
		});
}
