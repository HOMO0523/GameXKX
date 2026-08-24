#include "GameXXKTrainingChestRules.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKMVPRules.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

namespace
{
	void SetFailure(FGameXXKTrainingChestOpenResult& Out, const EGameXXKTrainingChestOpenError Error, const TCHAR* Message)
	{
		Out = FGameXXKTrainingChestOpenResult();
		Out.Error = Error;
		Out.Message = FText::FromString(Message);
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

	bool OpenOneInternal(
		FGameXXKRuntimeState& InOutState,
		const EGameXXKTrainingRewardTier Tier,
		FGameXXKTrainingChestOpenResult& Out)
	{
		if (Tier != EGameXXKTrainingRewardTier::NormalChest && Tier != EGameXXKTrainingRewardTier::AdvancedChest)
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
		const bool bEquipment = Stream.RandRange(0, 1) == 0;
		if (bEquipment)
		{
			if (FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Candidate, EGameXXKDesktopItemContainer::Backpack) == INDEX_NONE)
			{
				SetFailure(Out, EGameXXKTrainingChestOpenError::BackpackFull, TEXT("背包已满，宝箱未消耗"));
				return false;
			}
			FGameXXKEquipmentCreateRequest Request;
			Request.Set = static_cast<EGameXXKEquipmentSet>(Stream.RandRange(static_cast<int32>(EGameXXKEquipmentSet::PoJun), static_cast<int32>(EGameXXKEquipmentSet::ShanHe)));
			Request.Quality = Tier == EGameXXKTrainingRewardTier::AdvancedChest ? EGameXXKEquipmentQuality::Rare : EGameXXKEquipmentQuality::Common;
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
			const int32 Outcome = Stream.RandRange(0, 4);
			FName ItemId;
			int32 Quantity = 1;
			if (Outcome <= 2)
			{
				ItemId = FGameXXKGemRules::MakeItemId(
					static_cast<EGameXXKGemType>(Outcome + 1),
					Tier == EGameXXKTrainingRewardTier::AdvancedChest ? EGameXXKGemQuality::Rare : EGameXXKGemQuality::Common);
			}
			else if (Outcome == 3)
			{
				ItemId = UGameXXKMVPRules::ItemEnhancementStone();
				Quantity = Tier == EGameXXKTrainingRewardTier::AdvancedChest ? 3 : 1;
			}
			else
			{
				ItemId = UGameXXKMVPRules::ItemRefinementSand();
				Quantity = Tier == EGameXXKTrainingRewardTier::AdvancedChest ? 3 : 1;
			}
			if (!Candidate.Inventory.Contains(ItemId)
				&& FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Candidate, EGameXXKDesktopItemContainer::Backpack) == INDEX_NONE)
			{
				SetFailure(Out, EGameXXKTrainingChestOpenError::BackpackFull, TEXT("背包已满，宝箱未消耗"));
				return false;
			}
			const int64 After = static_cast<int64>(Candidate.Inventory.FindRef(ItemId)) + Quantity;
			if (After > MAX_int32)
			{
				SetFailure(Out, EGameXXKTrainingChestOpenError::Overflow, TEXT("宝箱道具数量溢出"));
				return false;
			}
			Candidate.Inventory.Add(ItemId, static_cast<int32>(After));
			Step.ItemDeltas.Add(ItemId, Quantity);
		}
		if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
		{
			SetFailure(Out, EGameXXKTrainingChestOpenError::BackpackFull, TEXT("背包无法容纳宝箱产物"));
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
		Step.bSucceeded = true;
		Step.OpenedCount = 1;
		InOutState = MoveTemp(Candidate);
		Out = MoveTemp(Step);
		return true;
	}
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
		for (const TPair<FName, int32>& Pair : Step.ItemDeltas) OutResult.ItemDeltas.FindOrAdd(Pair.Key) += Pair.Value;
	}
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}
