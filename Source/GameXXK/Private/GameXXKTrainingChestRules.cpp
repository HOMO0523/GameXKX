#include "GameXXKTrainingChestRules.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKTravelMoneyRules.h"
#include "GameXXKHuntRules.h"
#include "UI/GameXXKLocalization.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

namespace
{
	void SetFailure(FGameXXKTrainingChestOpenResult& Out, const EGameXXKTrainingChestOpenError Error, const TCHAR* Message)
	{
		Out = FGameXXKTrainingChestOpenResult();
		Out.Error = Error;
		Out.Message = GameXXKLocalization::Source(Message);
	}

	uint32 BuildSeed(const FGameXXKRuntimeState& State, const FGameXXKTrainingChestToken& Token)
	{
		const FString SeedText = FString::Printf(
			TEXT("%d|%d|%d|%d|%s"),
			State.Training.ChallengeRewardSeed,
			Token.AcquisitionOrdinal,
			State.Training.NextChestOpenOrdinal,
			static_cast<int32>(Token.Tier),
			*Token.SourceStageId.ToString());
		return FCrc::StrCrc32(*SeedText);
	}

	int32 FindOldestToken(const FGameXXKTrainingProgress& Progress, const EGameXXKTrainingRewardTier Tier)
	{
		for (int32 Index = 0; Index < Progress.OwnedChestTokens.Num(); ++Index)
			if (Progress.OwnedChestTokens[Index].Tier == Tier) return Index;
		return INDEX_NONE;
	}

	bool AddLootStack(FGameXXKRuntimeState& State, FName ItemId, int32 Quantity,
		FGameXXKTrainingChestOpenResult& Out)
	{
		// A stack has one physical home. Retain a stored stack (and its lock/slot)
		// instead of creating a second copy of the same item in the backpack.
		const bool bStored = ItemId != FGameXXKTravelMoneyRules::ItemId()
			&& State.DesktopInventory.WarehouseItems.FindRef(ItemId) > 0;
		auto& Items = bStored ? State.DesktopInventory.WarehouseItems : State.Inventory;
		if (Items.FindRef(ItemId) <= 0
			&& FGameXXKDesktopInventoryRules::FindFirstEmptySlot(State, EGameXXKDesktopItemContainer::Backpack) == INDEX_NONE)
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::BackpackFull, TEXT("背包已满，宝箱未消耗"));
			return false;
		}
		const int64 After = static_cast<int64>(Items.FindRef(ItemId)) + Quantity;
		if (After > MAX_int32)
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::Overflow, TEXT("宝箱道具数量溢出"));
			return false;
		}
		Items.Add(ItemId, static_cast<int32>(After));
		return true;
	}

	bool OpenOneInternal(
		FGameXXKRuntimeState& InOutState,
		const EGameXXKTrainingRewardTier Tier,
		FGameXXKTrainingChestOpenResult& Out)
	{
		if (Tier != EGameXXKTrainingRewardTier::NormalChest && Tier != EGameXXKTrainingRewardTier::AdvancedChest && Tier != EGameXXKTrainingRewardTier::HuntChest)
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::InvalidToken, TEXT("宝箱类型无效"));
			return false;
		}
		const int32 TokenIndex = FindOldestToken(InOutState.Training, Tier);
		if (TokenIndex == INDEX_NONE)
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::NoChest, TEXT("没有该类型宝箱"));
			return false;
		}
		const FGameXXKTrainingChestToken Token = InOutState.Training.OwnedChestTokens[TokenIndex];
		FString Error;
		if (!FGameXXKTrainingRules::ValidateChestTokens(InOutState.Training, &Error))
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::InvalidToken, TEXT("宝箱钱包数据无效"));
			return false;
		}
		FRandomStream Stream(static_cast<int32>(BuildSeed(InOutState, Token)));
		FGameXXKRuntimeState Candidate = InOutState;
		FGameXXKTrainingChestOpenResult Step;
		const bool bHigherTier=Tier!=EGameXXKTrainingRewardTier::NormalChest;
		const bool bOrder=FGameXXKTrainingChestRules::ResolveOrderDrop(Tier,Stream.RandRange(0,9999));
		const bool bEquipment = !bOrder && Stream.RandRange(0, 1) == 0;
		if(bOrder)
		{
			const FName Id=FGameXXKHuntRules::OrderId(FGameXXKTrainingRules::DifficultyFromStageId(Token.SourceStageId));
			if (!AddLootStack(Candidate, Id, 1, Out)) return false;
			Step.ItemDeltas.Add(Id,1);
		}
		else if (bEquipment)
		{
			if (FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Candidate, EGameXXKDesktopItemContainer::Backpack) == INDEX_NONE)
			{
				SetFailure(Out, EGameXXKTrainingChestOpenError::BackpackFull, TEXT("背包已满，宝箱未消耗"));
				return false;
			}
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = static_cast<EGameXXKEquipmentSet>(Stream.RandRange(static_cast<int32>(EGameXXKEquipmentSet::PoJun), static_cast<int32>(EGameXXKEquipmentSet::ShanHe)));
			Request.Quality = FGameXXKTrainingChestRules::ResolveLootQuality(Tier,bHigherTier?Stream.RandRange(0,9999):0);
			Request.ItemLevel = Token.SourceItemLevel;
			Request.bForceSlot = true;
			Request.ForcedSlot = static_cast<EGameXXKEquipmentSlot>(Stream.RandRange(1, 6));
			FName InstanceId;
			if (!FGameXXKEquipmentRules::CreateRolledInstance(Candidate.EquipmentCollection, Request, InstanceId, &Error))
			{
				SetFailure(Out, EGameXXKTrainingChestOpenError::LootInvalid, TEXT("宝箱装备生成失败"));
				return false;
			}
			Step.EquipmentInstanceIds.Add(InstanceId);
		}
		else
		{
			// Total weights: equipment 50%, gems 30%, materials 20%.
			// Ordinary materials share that 20% equally: stone, sand, travel money.
			const int32 Outcome = Stream.RandRange(0, 4);
			const int32 MaterialOutcome = Outcome <= 2 ? INDEX_NONE
				: (Tier == EGameXXKTrainingRewardTier::NormalChest ? Stream.RandRange(0, 2) : Outcome - 3);
			FName ItemId;
			int32 Quantity = 1;
			if (Outcome <= 2)
			{
				const auto Type=static_cast<EGameXXKGemType>(Stream.RandRange(1,FGameXXKGemRules::MaximumTypeRank));
				const auto Quality=FGameXXKTrainingChestRules::ResolveLootQuality(Tier,bHigherTier?Stream.RandRange(0,9999):0);
				ItemId=FGameXXKGemRules::MakeItemId(Type,static_cast<EGameXXKGemQuality>(Quality));
			}
			else if (MaterialOutcome == 0)
			{
				ItemId = UGameXXKMVPRules::ItemEnhancementStone();
				Quantity = bHigherTier ? 3 : 1;
			}
			else if (MaterialOutcome == 1)
			{
				ItemId = UGameXXKMVPRules::ItemRefinementSand();
				Quantity = bHigherTier ? 3 : 1;
			}
			else
			{
				ItemId = FGameXXKTravelMoneyRules::ItemId();
				Quantity = FGameXXKTravelMoneyRules::ChestDropQuantity;
			}
			if (!AddLootStack(Candidate, ItemId, Quantity, Out)) return false;
			Step.ItemDeltas.Add(ItemId, Quantity);
		}
		if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::LootInvalid, TEXT("宝箱物品整理失败，宝箱未消耗"));
			return false;
		}
		Candidate.Training.OwnedChestTokens.RemoveAt(TokenIndex);
		if (Candidate.Training.NextChestOpenOrdinal == MAX_int32)
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::Overflow, TEXT("宝箱开启序号耗尽"));
			return false;
		}
		++Candidate.Training.NextChestOpenOrdinal;
		if (!FGameXXKTrainingRules::ValidateChestTokens(Candidate.Training, &Error))
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::InvalidToken, TEXT("宝箱开启结果无效"));
			return false;
		}
        auto& Receipt=Step.Receipts.AddDefaulted_GetRef();
        Receipt.Tier=Tier;Receipt.OpenOrdinal=Candidate.Training.NextChestOpenOrdinal;
        Receipt.SourceStageId=Token.SourceStageId;Receipt.Quantity=1;
        if(!Step.EquipmentInstanceIds.IsEmpty())
        {
            const auto* Item=FGameXXKEquipmentRules::FindInstance(Candidate.EquipmentCollection,Step.EquipmentInstanceIds[0]);
            check(Item);
            Receipt.EquipmentInstanceId=Item->InstanceId;Receipt.EquipmentBaseId=Item->BaseEquipmentId;
            Receipt.QualityRank=FGameXXKEquipmentQualityRules::GetRank(Item->Quality);Receipt.ItemLevel=Item->ItemLevel;
        }
        else
        {
            const auto Pair=*Step.ItemDeltas.CreateConstIterator();Receipt.ItemId=Pair.Key;Receipt.Quantity=Pair.Value;
            Receipt.bSentToWarehouse=Pair.Key!=FGameXXKTravelMoneyRules::ItemId()&&Candidate.DesktopInventory.WarehouseItems.FindRef(Pair.Key)>0;
            EGameXXKGemType Type;EGameXXKGemQuality Quality;
            Receipt.QualityRank=FGameXXKGemRules::TryParseItemId(Pair.Key,Type,Quality)?FGameXXKGemRules::GetQualityRank(Quality):
                FGameXXKHuntRules::IsOrder(Pair.Key)?FGameXXKEquipmentQualityRules::GetRank(FGameXXKHuntRules::OrderQuality(Pair.Key)):1;
        }
		Step.bSucceeded = true;
		Step.OpenedCount = 1;
		InOutState = MoveTemp(Candidate);
		Out = MoveTemp(Step);
		return true;
	}
}

bool FGameXXKTrainingChestRules::ResolveOrderDrop(EGameXXKTrainingRewardTier Tier,int32 Roll)
{
	if(Roll<0||Roll>=10000)return false;
	return Tier==EGameXXKTrainingRewardTier::NormalChest?Roll<200:
		Tier==EGameXXKTrainingRewardTier::AdvancedChest&&Roll<800;
}

EGameXXKEquipmentQuality FGameXXKTrainingChestRules::ResolveLootQuality(EGameXXKTrainingRewardTier Tier,int32 Roll)
{
	if(Roll<0||Roll>=10000)return EGameXXKEquipmentQuality::Invalid;
	if(Tier==EGameXXKTrainingRewardTier::NormalChest)return EGameXXKEquipmentQuality::Common;
	if(Tier!=EGameXXKTrainingRewardTier::AdvancedChest&&Tier!=EGameXXKTrainingRewardTier::HuntChest)return EGameXXKEquipmentQuality::Invalid;
	const bool bHunt=Tier==EGameXXKTrainingRewardTier::HuntChest;
	if(Roll<(bHunt?6000:9000))return EGameXXKEquipmentQuality::Rare;
	if(Roll<(bHunt?9200:9800))return EGameXXKEquipmentQuality::Epic;
	if(Roll<(bHunt?9800:9950))return EGameXXKEquipmentQuality::Legendary;
	return EGameXXKEquipmentQuality::Immortal;
}

bool FGameXXKTrainingChestRules::OpenOne(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKTrainingRewardTier Tier,
	FGameXXKTrainingChestOpenResult& OutResult)
{
	FGameXXKRuntimeState Candidate = InOutState;
	if (!OpenOneInternal(Candidate, Tier, OutResult)) return false;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKTrainingChestRules::OpenAll(
	FGameXXKRuntimeState& InOutState,
	const EGameXXKTrainingRewardTier Tier,
	FGameXXKTrainingChestOpenResult& OutResult)
{
	OutResult = FGameXXKTrainingChestOpenResult();
	const int32 Bound = FGameXXKTrainingRules::CountChestTokens(InOutState.Training, Tier);
	if (Bound <= 0)
	{
		SetFailure(OutResult, EGameXXKTrainingChestOpenError::NoChest, TEXT("没有该类型宝箱"));
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	for (int32 Index = 0; Index < Bound; ++Index)
	{
		FGameXXKTrainingChestOpenResult Step;
		if (!OpenOneInternal(Candidate, Tier, Step))
		{
			if (Step.Error == EGameXXKTrainingChestOpenError::BackpackFull && OutResult.OpenedCount > 0)
			{
				OutResult.bSucceeded = true;
				OutResult.Error = Step.Error;
				OutResult.Message = Step.Message;
				InOutState = MoveTemp(Candidate);
				return true;
			}
			OutResult = MoveTemp(Step);
			return false;
		}
		++OutResult.OpenedCount;
		OutResult.EquipmentInstanceIds.Append(Step.EquipmentInstanceIds);
        OutResult.Receipts.Append(Step.Receipts);
		for (const TPair<FName, int32>& Pair : Step.ItemDeltas) OutResult.ItemDeltas.FindOrAdd(Pair.Key) += Pair.Value;
	}
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}
