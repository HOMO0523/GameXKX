#pragma once

#include "CoreMinimal.h"
#include "GameXXKCompanionTypes.h"
#include "GameXXKEquipmentTypes.h"
#include "UObject/SoftObjectPath.h"

#include "GameXXKMetaShopTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKMetaShopProductId : uint8
{
	Invalid = 0 UMETA(Hidden),
	PoJunPack,
	XuanJiaPack,
	QingNangPack,
	ZhuiFengPack,
	ShiGuPack,
	ShanHePack,
	CompanionPack
};

UENUM(BlueprintType)
enum class EGameXXKMetaShopProductKind : uint8
{
	EquipmentPack = 0,
	CompanionPack
};

UENUM(BlueprintType)
enum class EGameXXKMetaShopError : uint8
{
	None = 0,
	InvalidProduct,
	NotInTown,
	InsufficientGold,
	WarehouseFull,
	RosterFull,
	PendingCompanionExists,
	PurchaseOrdinalExhausted,
	EquipmentCreationFailed,
	CompanionCreationFailed,
	InvalidRuntimeState
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMetaShopState
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 Seed = 0;

	UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
	int32 NextPurchaseOrdinal = 0;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMetaShopProductDefinition
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductId ProductId = EGameXXKMetaShopProductId::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductKind Kind = EGameXXKMetaShopProductKind::EquipmentPack;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentSet EquipmentSet = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText DisplayName;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Description;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FSoftObjectPath IconSoftPath;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMetaShopPurchasePreview
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductId ProductId = EGameXXKMetaShopProductId::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductKind Kind = EGameXXKMetaShopProductKind::EquipmentPack;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentSet EquipmentSet = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 GoldBefore = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 GoldAfter = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bAvailable = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopError Error = EGameXXKMetaShopError::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Message;
};

USTRUCT(BlueprintType)
struct GAMEXXK_API FGameXXKMetaShopPurchaseResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	bool bPurchased = false;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductId ProductId = EGameXXKMetaShopProductId::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopProductKind Kind = EGameXXKMetaShopProductKind::EquipmentPack;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKEquipmentSet EquipmentSet = EGameXXKEquipmentSet::Invalid;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 Price = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	int32 GoldDelta = 0;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FName GeneratedEquipmentId = NAME_None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FGameXXKCompanionRecruitResult CompanionResult;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	EGameXXKMetaShopError Error = EGameXXKMetaShopError::None;

	UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
	FText Message;
};
