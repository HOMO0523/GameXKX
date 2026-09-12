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
		const EGameXXKTrainingDifficulty SourceDifficulty = FGameXXKTrainingRules::DifficultyFromStageId(Token.SourceStageId);
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
		const bool bOrder=FGameXXKTrainingChestRules::ResolveOrderDrop(Tier,Stream.RandRange(0,FGameXXKTrainingChestRules::LootRollDomain-1));
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
			Request.Quality = FGameXXKTrainingChestRules::ResolveLootQuality(Tier,SourceDifficulty,Stream.RandRange(0,FGameXXKTrainingChestRules::LootRollDomain-1));
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
				const auto Quality=FGameXXKTrainingChestRules::ResolveLootQuality(Tier,SourceDifficulty,Stream.RandRange(0,FGameXXKTrainingChestRules::LootRollDomain-1));
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
	if(Roll<0||Roll>=LootRollDomain)return false;
	return Tier==EGameXXKTrainingRewardTier::NormalChest?Roll<200:
		Tier==EGameXXKTrainingRewardTier::AdvancedChest&&Roll<800;
}

EGameXXKEquipmentQuality FGameXXKTrainingChestRules::ResolveLootQuality(EGameXXKTrainingRewardTier Tier,EGameXXKTrainingDifficulty SourceDifficulty,int32 Roll)
{
	if(Roll<0||Roll>=LootRollDomain)return EGameXXKEquipmentQuality::Invalid;
	// Approved 2026-09-11 three-chest table, in basis points. Every column sums to exactly
	// LootRollDomain. The 珍稀-and-above mass is a strict x4 ladder: normal 500 -> advanced 2000 ->
	// hunt 8000 (1:4:16). The hunt chest is the only source that reaches the top three ranks:
	// 天界 80bp and 登神 40bp are droppable on every difficulty, and 宇宙 16bp only from 地狱.
	// The top of the column decays monotonically (至宝200 > 超凡100 > 天界80 > 登神40 > 宇宙16),
	// so no tier is more common than the one below it. 普通箱 stops at 至宝 because the design's
	// 0.001% 超凡 tail is below one basis point; 高级箱 stops at 超凡.
	struct FQualityRow { int32 NormalChest; int32 AdvancedChest; int32 HuntChest; int32 HellHuntChest; };
	static constexpr FQualityRow Rows[]={
		{7000,5500, 280, 264}, // 普通
		{2500,2500,1500,1500}, // 稀有
		{ 400,1200,4800,4800}, // 珍稀
		{  90, 600,2400,2400}, // 传奇
		{   9, 150, 600, 600}, // 不朽
		{   1,  40, 200, 200}, // 至宝
		{   0,  10, 100, 100}, // 超凡
		{   0,   0,  80,  80}, // 天界
		{   0,   0,  40,  40}, // 登神
		{   0,   0,   0,  16}, // 宇宙（仅地狱讨伐箱）
	};
	static constexpr EGameXXKEquipmentQuality Qualities[]={
		EGameXXKEquipmentQuality::Common,
		EGameXXKEquipmentQuality::Rare,
		EGameXXKEquipmentQuality::Epic,
		EGameXXKEquipmentQuality::Legendary,
		EGameXXKEquipmentQuality::Immortal,
		EGameXXKEquipmentQuality::Treasure,
		EGameXXKEquipmentQuality::Transcendent,
		EGameXXKEquipmentQuality::Celestial,
		EGameXXKEquipmentQuality::Ascendant,
		EGameXXKEquipmentQuality::Cosmic,
	};
	if(Tier!=EGameXXKTrainingRewardTier::NormalChest&&Tier!=EGameXXKTrainingRewardTier::AdvancedChest&&Tier!=EGameXXKTrainingRewardTier::HuntChest)
		return EGameXXKEquipmentQuality::Invalid;
	int32 Remaining=Roll;
	for(int32 Index=0;Index<UE_ARRAY_COUNT(Rows);++Index)
	{
		const int32 Weight=Tier==EGameXXKTrainingRewardTier::NormalChest?Rows[Index].NormalChest:
			Tier==EGameXXKTrainingRewardTier::AdvancedChest?Rows[Index].AdvancedChest:
			SourceDifficulty==EGameXXKTrainingDifficulty::Hell?Rows[Index].HellHuntChest:Rows[Index].HuntChest;
		if(Remaining<Weight)return Qualities[Index];
		Remaining-=Weight;
	}
	return EGameXXKEquipmentQuality::Invalid;
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
