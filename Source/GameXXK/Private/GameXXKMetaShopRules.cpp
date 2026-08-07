#include "GameXXKMetaShopRules.h"

#include "GameXXKCompanionRules.h"
#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKSaveMigration.h"

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
		Product.Description = NSLOCTEXT("GameXXKMetaShop", "CompanionPackDescription", "获得一名永久伙伴；名册满员（12 人）时不可购买");
		Product.IconSoftPath = FSoftObjectPath(TEXT("/Game/GameXXK/UI/MetaShop/V2/T_MetaShop_CompanionPack.T_MetaShop_CompanionPack"));
		return Product;
	}

	TArray<FGameXXKMetaShopProductDefinition> BuildProducts()
	{
		return {
			MakeEquipmentPack(EGameXXKMetaShopProductId::PoJunPack, EGameXXKEquipmentSet::PoJun, NSLOCTEXT("GameXXKMetaShop", "PoJunPackName", "破军装备包"), TEXT("/Game/GameXXK/UI/Equipment/pojun_weapon.pojun_weapon")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::XuanJiaPack, EGameXXKEquipmentSet::XuanJia, NSLOCTEXT("GameXXKMetaShop", "XuanJiaPackName", "玄甲装备包"), TEXT("/Game/GameXXK/UI/Equipment/xuanjia_weapon.xuanjia_weapon")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::QingNangPack, EGameXXKEquipmentSet::QingNang, NSLOCTEXT("GameXXKMetaShop", "QingNangPackName", "青囊装备包"), TEXT("/Game/GameXXK/UI/Equipment/qingnang_weapon.qingnang_weapon")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ZhuiFengPack, EGameXXKEquipmentSet::ZhuiFeng, NSLOCTEXT("GameXXKMetaShop", "ZhuiFengPackName", "追风装备包"), TEXT("/Game/GameXXK/UI/Equipment/zhuifeng_weapon.zhuifeng_weapon")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ShiGuPack, EGameXXKEquipmentSet::ShiGu, NSLOCTEXT("GameXXKMetaShop", "ShiGuPackName", "蚀骨装备包"), TEXT("/Game/GameXXK/UI/Equipment/shigu_weapon.shigu_weapon")),
			MakeEquipmentPack(EGameXXKMetaShopProductId::ShanHePack, EGameXXKEquipmentSet::ShanHe, NSLOCTEXT("GameXXKMetaShop", "ShanHePackName", "山河装备包"), TEXT("/Game/GameXXK/UI/Equipment/shanhe_weapon.shanhe_weapon")),
			MakeCompanionPack(),
		};
	}

	FText ErrorMessage(const EGameXXKMetaShopError Error)
	{
		switch (Error)
		{
		case EGameXXKMetaShopError::InvalidProduct:
			return NSLOCTEXT("GameXXKMetaShop", "InvalidProduct", "商品无效。");
		case EGameXXKMetaShopError::NotInTown:
			return NSLOCTEXT("GameXXKMetaShop", "NotInTown", "只能在城镇商店购买。");
		case EGameXXKMetaShopError::InsufficientGold:
			return NSLOCTEXT("GameXXKMetaShop", "InsufficientGold", "元宝不足。");
		case EGameXXKMetaShopError::WarehouseFull:
			return NSLOCTEXT("GameXXKMetaShop", "WarehouseFull", "装备仓库已满。");
		case EGameXXKMetaShopError::RosterFull:
			return NSLOCTEXT("GameXXKMetaShop", "RosterFull", "名册已满（12 人），无法招募更多伙伴。");
		case EGameXXKMetaShopError::PendingCompanionExists:
			return NSLOCTEXT("GameXXKMetaShop", "PendingCompanionExists", "请先处理待替换的伙伴。");
		case EGameXXKMetaShopError::PurchaseOrdinalExhausted:
			return NSLOCTEXT("GameXXKMetaShop", "PurchaseOrdinalExhausted", "商店购买序列已耗尽。");
		case EGameXXKMetaShopError::EquipmentCreationFailed:
			return NSLOCTEXT("GameXXKMetaShop", "EquipmentCreationFailed", "装备生成失败。");
		case EGameXXKMetaShopError::CompanionCreationFailed:
			return NSLOCTEXT("GameXXKMetaShop", "CompanionCreationFailed", "伙伴生成失败。");
		case EGameXXKMetaShopError::InvalidRuntimeState:
			return NSLOCTEXT("GameXXKMetaShop", "InvalidRuntimeState", "当前存档状态无效。");
		default:
			return FText::GetEmpty();
		}
	}

	void FailPreview(FGameXXKMetaShopPurchasePreview& OutPreview, const EGameXXKMetaShopError Error)
	{
		OutPreview.bAvailable = false;
		OutPreview.Error = Error;
		OutPreview.Message = ErrorMessage(Error);
	}

	void FailResult(FGameXXKMetaShopPurchaseResult& OutResult, const EGameXXKMetaShopError Error)
	{
		OutResult.bPurchased = false;
		OutResult.Error = Error;
		OutResult.Message = ErrorMessage(Error);
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

EGameXXKEquipmentQuality FGameXXKMetaShopRules::QualityFromRoll(const int32 RollOneToHundred)
{
	if (RollOneToHundred < 1 || RollOneToHundred > 100)
	{
		return EGameXXKEquipmentQuality::Invalid;
	}
	if (RollOneToHundred <= 70)
	{
		return EGameXXKEquipmentQuality::Common;
	}
	if (RollOneToHundred <= 95)
	{
		return EGameXXKEquipmentQuality::Rare;
	}
	return EGameXXKEquipmentQuality::Epic;
}

bool FGameXXKMetaShopRules::PreviewPurchase(
	const FGameXXKRuntimeState& State,
	const EGameXXKMetaShopProductId ProductId,
	FGameXXKMetaShopPurchasePreview& OutPreview)
{
	OutPreview = FGameXXKMetaShopPurchasePreview();
	const FGameXXKMetaShopProductDefinition* Product = FindProduct(ProductId);
	if (!Product)
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::InvalidProduct);
		return false;
	}

	OutPreview.ProductId = Product->ProductId;
	OutPreview.Kind = Product->Kind;
	OutPreview.EquipmentSet = Product->EquipmentSet;
	OutPreview.Price = Product->Price;
	OutPreview.GoldBefore = State.PlayerGold;
	OutPreview.GoldAfter = State.PlayerGold;

	if (State.Screen != EGameXXKScreen::Town)
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::NotInTown);
		return false;
	}
	if (State.MetaShop.NextPurchaseOrdinal == MAX_int32)
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::PurchaseOrdinalExhausted);
		return false;
	}

	FString ValidationError;
	if (!FGameXXKSaveMigration::ValidateRuntimeState(State, ValidationError))
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::InvalidRuntimeState);
		return false;
	}
	if (State.PlayerGold < Product->Price)
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::InsufficientGold);
		return false;
	}
	if (Product->Kind == EGameXXKMetaShopProductKind::EquipmentPack
		&& !FGameXXKEquipmentRules::HasWarehouseCapacity(State.EquipmentCollection))
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::WarehouseFull);
		return false;
	}
	if (Product->Kind == EGameXXKMetaShopProductKind::CompanionPack
		&& State.CardRun.CompanionRoster.PermanentCompanions.Num() >= FGameXXKCompanionRules::MaxPermanentCompanions)
	{
		// A full roster cannot buy the companion pack at all; the player must dismiss first.
		FailPreview(OutPreview, EGameXXKMetaShopError::RosterFull);
		return false;
	}
	if (Product->Kind == EGameXXKMetaShopProductKind::CompanionPack
		&& (State.CardRun.CompanionRoster.PendingRecruitment.bHasPendingRecruitment
			|| State.CardRun.CompanionRoster.PendingRecruitOrder.bHasPendingOrder))
	{
		FailPreview(OutPreview, EGameXXKMetaShopError::PendingCompanionExists);
		return false;
	}

	OutPreview.GoldAfter = State.PlayerGold - Product->Price;
	OutPreview.bAvailable = true;
	return true;
}

bool FGameXXKMetaShopRules::Purchase(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKMetaShopProductId ProductId,
	FGameXXKMetaShopPurchaseResult& OutResult)
{
	OutResult = FGameXXKMetaShopPurchaseResult();
	FGameXXKMetaShopPurchasePreview Preview;
	if (!PreviewPurchase(InOutState, ProductId, Preview))
	{
		OutResult.ProductId = Preview.ProductId;
		OutResult.Kind = Preview.Kind;
		OutResult.EquipmentSet = Preview.EquipmentSet;
		OutResult.Price = Preview.Price;
		FailResult(OutResult, Preview.Error);
		return false;
	}

	FGameXXKRuntimeState Candidate = InOutState;
	const uint32 StreamSeed = HashCombine(
		HashCombine(GetTypeHash(Candidate.MetaShop.Seed), GetTypeHash(Candidate.MetaShop.NextPurchaseOrdinal)),
		GetTypeHash(static_cast<uint8>(ProductId)));
	FName GeneratedEquipmentId;
	FGameXXKCompanionRecruitResult CompanionResult;
	if (Preview.Kind == EGameXXKMetaShopProductKind::EquipmentPack)
	{
		FRandomStream Stream(static_cast<int32>(StreamSeed & 0x7fffffffU));
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = Preview.EquipmentSet;
		Request.Quality = QualityFromRoll(Stream.RandRange(1, 100));
		Request.ItemLevel = FMath::Clamp(Candidate.PlayerLevel, 1, FGameXXKEquipmentRules::MaxItemLevel);
		Request.bForceSlot = true;
		Request.ForcedSlot = static_cast<EGameXXKEquipmentSlot>(Stream.RandRange(1, 6));
		if (!FGameXXKEquipmentRules::CreateRolledInstance(
			Candidate.EquipmentCollection,
			Request,
			GeneratedEquipmentId))
		{
			FailResult(OutResult, EGameXXKMetaShopError::EquipmentCreationFailed);
			return false;
		}
	}
	else
	{
		const int32 OrderSeed = FMath::Max(1, static_cast<int32>(StreamSeed & 0x7fffffffU));
		FGameXXKCompanionRecruitOrder Order;
		FString CompanionError;
		if (!FGameXXKCompanionRules::CreateRecruitOrder(
			Candidate.CardRun.CompanionRoster,
			OrderSeed,
			Order,
			&CompanionError)
			|| !FGameXXKCompanionRules::ResolvePendingRecruitOrder(
				Candidate.CardRun.CompanionRoster,
				CompanionResult,
				&CompanionError))
		{
			FailResult(OutResult, EGameXXKMetaShopError::CompanionCreationFailed);
			return false;
		}
	}

	Candidate.PlayerGold -= Preview.Price;
	Candidate.MetaShop.NextPurchaseOrdinal += 1;
	FString ValidationError;
	if (!FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)
		|| !FGameXXKSaveMigration::ValidateRuntimeState(Candidate, ValidationError))
	{
		FailResult(OutResult, EGameXXKMetaShopError::InvalidRuntimeState);
		return false;
	}

	InOutState = MoveTemp(Candidate);
	OutResult.bPurchased = true;
	OutResult.ProductId = Preview.ProductId;
	OutResult.Kind = Preview.Kind;
	OutResult.EquipmentSet = Preview.EquipmentSet;
	OutResult.Price = Preview.Price;
	OutResult.GoldDelta = -Preview.Price;
	OutResult.GeneratedEquipmentId = GeneratedEquipmentId;
	OutResult.CompanionResult = CompanionResult;
	return true;
}

int32 FGameXXKMetaShopRules::DeriveSeed(const FGameXXKRuntimeState& State)
{
	const uint32 Mixed = HashCombine(
		GetTypeHash(State.EquipmentCollection.CollectionSeed),
		GetTypeHash(State.CardRun.CompanionRoster.RecruitSequenceSeed));
	return FMath::Max(1, static_cast<int32>(Mixed & 0x7fffffffU));
}

bool FGameXXKMetaShopRules::ValidateState(const FGameXXKRuntimeState& State, FString* OutError)
{
	if (State.MetaShop.Seed <= 0
		|| State.MetaShop.NextPurchaseOrdinal < 0
		|| State.MetaShop.NextPurchaseOrdinal == MAX_int32)
	{
		if (OutError)
		{
			*OutError = TEXT("Saved meta shop state has an invalid seed or purchase ordinal.");
		}
		return false;
	}
	return true;
}
