#include "GameXXKEquipmentToolRules.h"

#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKGemRules.h"
#include "Math/RandomStream.h"
#include "Misc/Crc.h"

namespace
{
	void SetError(FString* OutError, const FString& Error)
	{
		if (OutError) *OutError = Error;
	}

	void Fail(FGameXXKEquipmentTransactionResult& Out, const EGameXXKEquipmentTransactionError Error, const TCHAR* Message)
	{
		Out = FGameXXKEquipmentTransactionResult();
		Out.Error = Error;
		Out.Message = FText::FromString(Message);
	}

	bool IsContainerValid(const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Backpack
			|| Container == EGameXXKDesktopItemContainer::Warehouse;
	}

	const TMap<FName, int32>& ItemsFor(const FGameXXKRuntimeState& State, const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems : State.Inventory;
	}

	TMap<FName, int32>& ItemsFor(FGameXXKRuntimeState& State, const EGameXXKDesktopItemContainer Container)
	{
		return Container == EGameXXKDesktopItemContainer::Warehouse
			? State.DesktopInventory.WarehouseItems : State.Inventory;
	}

	bool ResolveExact(
		const FGameXXKRuntimeState& State,
		const FGameXXKToolInputRef& Ref,
		const bool bRejectLocked,
		FGameXXKEquipmentTransactionResult& Out)
	{
		if (!IsContainerValid(Ref.Container)
			|| Ref.SlotIndex < 0
			|| Ref.SlotIndex >= FGameXXKDesktopInventoryRules::BackpackCapacity
			|| !Ref.ExpectedEntry.IsValid()
			|| FGameXXKDesktopInventoryRules::GetEntryAt(State, Ref.Container, Ref.SlotIndex) != Ref.ExpectedEntry)
		{
			Fail(Out, EGameXXKEquipmentTransactionError::InputStale, TEXT("工具输入来源已变化"));
			return false;
		}
		if (bRejectLocked && FGameXXKDesktopInventoryRules::IsEntryLocked(State, Ref.ExpectedEntry))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::InputLocked, TEXT("锁定物品不能被分解或合成"));
			return false;
		}
		if (Ref.ExpectedEntry.bEquipmentInstance)
		{
			const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(
				State.EquipmentCollection, Ref.ExpectedEntry.EntryId);
			const bool bWarehousePartition = State.DesktopInventory.WarehouseEquipmentInstanceIds.Contains(Ref.ExpectedEntry.EntryId);
			if (!Instance
				|| Instance->OwnerKind != EGameXXKEquipmentOwnerKind::Warehouse
				|| !State.EquipmentCollection.WarehouseInstanceIds.Contains(Ref.ExpectedEntry.EntryId)
				|| bWarehousePartition != (Ref.Container == EGameXXKDesktopItemContainer::Warehouse))
			{
				Fail(Out, EGameXXKEquipmentTransactionError::InputStale, TEXT("工具装备已不在原格"));
				return false;
			}
		}
		else if (ItemsFor(State, Ref.Container).FindRef(Ref.ExpectedEntry.EntryId) <= 0)
		{
			Fail(Out, EGameXXKEquipmentTransactionError::InputStale, TEXT("工具道具数量已变化"));
			return false;
		}
		return true;
	}

	bool Finish(FGameXXKRuntimeState& Candidate, FGameXXKEquipmentTransactionResult& Out)
	{
		FString Error;
		if (!FGameXXKEquipmentRules::NormalizeSocketArrays(Candidate.EquipmentCollection, &Error)
			|| !FGameXXKEquipmentEconomyRules::SynchronizeRuntimeMirrors(Candidate)
			|| !FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error)
			|| !FGameXXKEquipmentToolRules::ValidateProgress(Candidate.ToolProgress, &Error))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("工具结果未通过完整校验"));
			return false;
		}
		return true;
	}

	bool AddAward(FGameXXKRuntimeState& Candidate, const int64 Award, FGameXXKEquipmentTransactionResult& Out)
	{
		if (!FGameXXKEquipmentToolRules::AddRawExperience(Candidate.ToolProgress, Award))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::InvalidRequest, TEXT("工具经验溢出"));
			return false;
		}
		Out.ToolExperienceDelta = Award;
		return true;
	}
}

int64 FGameXXKEquipmentToolRules::GetQualityExperienceMultiplier(const int32 QualityRank)
{
	if (QualityRank < 1 || QualityRank > 10) return 0;
	int64 Result = 1;
	for (int32 Index = 1; Index < QualityRank; ++Index)
	{
		if (Result > MAX_int64 / 9) return 0;
		Result *= 9;
	}
	return Result;
}

int64 FGameXXKEquipmentToolRules::GetExperienceForNextLevel(const int32 CurrentLevel)
{
	return CurrentLevel >= MinimumLevel && CurrentLevel < MaximumLevel ? static_cast<int64>(CurrentLevel) * 100 : 0;
}

FInt32Interval FGameXXKEquipmentToolRules::GetCraftedItemLevelRange(const int32 SelectedCraftingLevel)
{
	const int32 Level = FMath::Clamp(SelectedCraftingLevel, MinimumLevel, MaximumLevel);
	return Level == 1 ? FInt32Interval(1, 10) : FInt32Interval((Level - 1) * 10, Level * 10);
}

bool FGameXXKEquipmentToolRules::NormalizeProgress(FGameXXKToolProgress& InOutProgress)
{
	if (InOutProgress.Level < MinimumLevel || InOutProgress.Level > MaximumLevel || InOutProgress.Experience < 0)
	{
		return false;
	}
	InOutProgress.SelectedCraftingLevel = FMath::Clamp(InOutProgress.SelectedCraftingLevel, MinimumLevel, InOutProgress.Level);
	if (InOutProgress.Level == MaximumLevel) InOutProgress.Experience = 0;
	return true;
}

bool FGameXXKEquipmentToolRules::ValidateProgress(const FGameXXKToolProgress& Progress, FString* OutError)
{
	SetError(OutError, FString());
	if (Progress.Level < MinimumLevel || Progress.Level > MaximumLevel
		|| Progress.Experience < 0
		|| Progress.SelectedCraftingLevel < MinimumLevel
		|| Progress.SelectedCraftingLevel > Progress.Level
		|| (Progress.Level == MaximumLevel && Progress.Experience != 0)
		|| (Progress.Level < MaximumLevel && Progress.Experience >= GetExperienceForNextLevel(Progress.Level)))
	{
		SetError(OutError, TEXT("Tool progression is invalid."));
		return false;
	}
	return true;
}

bool FGameXXKEquipmentToolRules::AddExperience(
	FGameXXKToolProgress& InOutProgress,
	const int64 BaseAward,
	const int32 QualityRank,
	int64* OutAward)
{
	const int64 Multiplier = GetQualityExperienceMultiplier(QualityRank);
	if (BaseAward < 0 || Multiplier <= 0 || (BaseAward > 0 && Multiplier > MAX_int64 / BaseAward)) return false;
	const int64 Award = BaseAward * Multiplier;
	if (OutAward) *OutAward = Award;
	return AddRawExperience(InOutProgress, Award);
}

bool FGameXXKEquipmentToolRules::AddRawExperience(FGameXXKToolProgress& InOutProgress, const int64 Award)
{
	if (!ValidateProgress(InOutProgress) || Award < 0 || InOutProgress.Experience > MAX_int64 - Award) return false;
	InOutProgress.Experience += Award;
	while (InOutProgress.Level < MaximumLevel)
	{
		const int64 Threshold = GetExperienceForNextLevel(InOutProgress.Level);
		if (InOutProgress.Experience < Threshold) break;
		InOutProgress.Experience -= Threshold;
		++InOutProgress.Level;
	}
	return NormalizeProgress(InOutProgress);
}

bool FGameXXKEquipmentToolRules::Dismantle(
	FGameXXKRuntimeState& InOutState,
	const TArray<FGameXXKToolInputRef>& Inputs,
	const bool bConfirmed,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (Inputs.Num() < 1 || Inputs.Num() > 9)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("分解需要 1 到 9 件装备"));
		return false;
	}
	TArray<FName> Ids;
	int64 Award = 0;
	for (const FGameXXKToolInputRef& Ref : Inputs)
	{
		if (!ResolveExact(InOutState, Ref, true, OutResult) || !Ref.ExpectedEntry.bEquipmentInstance || Ids.Contains(Ref.ExpectedEntry.EntryId))
		{
			if (OutResult.Error == EGameXXKEquipmentTransactionError::None)
				Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("分解输入必须是不同的未装备装备"));
			return false;
		}
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Ref.ExpectedEntry.EntryId);
		Award += GetQualityExperienceMultiplier(FGameXXKEquipmentQualityRules::GetRank(Instance->Quality));
		Ids.Add(Ref.ExpectedEntry.EntryId);
	}
	FGameXXKRuntimeState Candidate = InOutState;
	if (!FGameXXKEquipmentEconomyRules::DismantleBatch(Candidate, Ids, bConfirmed, OutResult)
		|| !AddAward(Candidate, Award, OutResult)
		|| !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::CombineEquipment(
	FGameXXKRuntimeState& InOutState,
	const TArray<FGameXXKToolInputRef>& Inputs,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (Inputs.Num() != 9)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("装备合成必须放入 9 件装备"));
		return false;
	}
	TArray<FName> Ids;
	EGameXXKEquipmentQuality InputQuality = EGameXXKEquipmentQuality::Invalid;
	int32 BackpackInputCount = 0;
	for (const FGameXXKToolInputRef& Ref : Inputs)
	{
		if (!ResolveExact(InOutState, Ref, true, OutResult) || !Ref.ExpectedEntry.bEquipmentInstance || Ids.Contains(Ref.ExpectedEntry.EntryId))
		{
			if (OutResult.Error == EGameXXKEquipmentTransactionError::None)
				Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("装备合成输入无效"));
			return false;
		}
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Ref.ExpectedEntry.EntryId);
		if (InputQuality == EGameXXKEquipmentQuality::Invalid) InputQuality = Instance->Quality;
		if (Instance->Quality != InputQuality || Instance->Quality == EGameXXKEquipmentQuality::Cosmic)
		{
			Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("9 件装备必须同品质且不能是宇宙品质"));
			return false;
		}
		if (Ref.Container == EGameXXKDesktopItemContainer::Backpack) ++BackpackInputCount;
		Ids.Add(Ref.ExpectedEntry.EntryId);
	}

	const int32 BackpackAfter = FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(InOutState, EGameXXKDesktopItemContainer::Backpack) - BackpackInputCount;
	const int32 WarehouseAfter = FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(InOutState, EGameXXKDesktopItemContainer::Warehouse) - (9 - BackpackInputCount);
	const bool bOutputToBackpack = BackpackAfter < FGameXXKDesktopInventoryRules::BackpackCapacity;
	if (!bOutputToBackpack && WarehouseAfter >= FGameXXKDesktopInventoryRules::WarehouseCapacity)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("背包和仓库都没有合成产物空位"));
		return false;
	}

	Ids.Sort(FNameLexicalLess());
	FString SeedText = FString::Printf(TEXT("%d|%d|%d"), InOutState.EquipmentCollection.CollectionSeed, InOutState.EquipmentCollection.NextInstanceOrdinal, InOutState.ToolProgress.SelectedCraftingLevel);
	for (const FName Id : Ids) SeedText += TEXT("|") + Id.ToString();
	FRandomStream Stream(static_cast<int32>(FCrc::StrCrc32(*SeedText)));
	FGameXXKRuntimeState Candidate = InOutState;
	for (const FName Id : Ids)
	{
		Candidate.EquipmentCollection.WarehouseInstanceIds.RemoveSingle(Id);
		Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.RemoveSingle(Id);
		Candidate.DesktopInventory.LockedEquipmentInstanceIds.Remove(Id);
		Candidate.EquipmentCollection.EquipmentInstances.RemoveAll([Id](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == Id; });
	}
	FString Error;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("合成输入清理失败"));
		return false;
	}
	const FInt32Interval LevelRange = GetCraftedItemLevelRange(Candidate.ToolProgress.SelectedCraftingLevel);
	FGameXXKEquipmentCreateRequest Request;
	Request.Set = static_cast<EGameXXKEquipmentSet>(Stream.RandRange(static_cast<int32>(EGameXXKEquipmentSet::PoJun), static_cast<int32>(EGameXXKEquipmentSet::ShanHe)));
	Request.Quality = FGameXXKEquipmentQualityRules::GetNext(InputQuality);
	Request.ItemLevel = Stream.RandRange(LevelRange.Min, LevelRange.Max);
	Request.bForceSlot = true;
	Request.ForcedSlot = static_cast<EGameXXKEquipmentSlot>(Stream.RandRange(1, 6));
	FName OutputId;
	if (!FGameXXKEquipmentRules::CreateRolledInstance(Candidate.EquipmentCollection, Request, OutputId, &Error))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("合成装备生成失败"));
		return false;
	}
	if (!bOutputToBackpack) Candidate.DesktopInventory.WarehouseEquipmentInstanceIds.Add(OutputId);
	const int64 Award = 9 * GetQualityExperienceMultiplier(FGameXXKEquipmentQualityRules::GetRank(InputQuality));
	if (!AddAward(Candidate, Award, OutResult) || !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	OutResult.AffectedInstanceIds = Ids;
	OutResult.AffectedInstanceIds.Add(OutputId);
	OutResult.OutputEntryId = OutputId;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::CombineGem(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKToolInputRef& Input,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!ResolveExact(InOutState, Input, true, OutResult) || Input.ExpectedEntry.bEquipmentInstance)
	{
		if (OutResult.Error == EGameXXKEquipmentTransactionError::None)
			Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("道具合成只接受宝石堆叠"));
		return false;
	}
	EGameXXKGemType Type;
	EGameXXKGemQuality Quality;
	if (!FGameXXKGemRules::TryParseItemId(Input.ExpectedEntry.EntryId, Type, Quality)
		|| Quality == EGameXXKGemQuality::Cosmic
		|| ItemsFor(InOutState, Input.Container).FindRef(Input.ExpectedEntry.EntryId) < 9)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("宝石合成需要 9 个同类型同品质宝石"));
		return false;
	}
	const FName OutputId = FGameXXKGemRules::MakeItemId(Type, FGameXXKGemRules::GetNextQuality(Quality));
	FGameXXKRuntimeState Candidate = InOutState;
	TMap<FName, int32>& SourceItems = ItemsFor(Candidate, Input.Container);
	const int32 Remaining = SourceItems.FindRef(Input.ExpectedEntry.EntryId) - 9;
	if (Remaining > 0) SourceItems.Add(Input.ExpectedEntry.EntryId, Remaining); else SourceItems.Remove(Input.ExpectedEntry.EntryId);
	FString Error;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("宝石合成输入整理失败"));
		return false;
	}
	if (Candidate.Inventory.Contains(OutputId)) Candidate.Inventory.FindOrAdd(OutputId) += 1;
	else if (Candidate.DesktopInventory.WarehouseItems.Contains(OutputId)) Candidate.DesktopInventory.WarehouseItems.FindOrAdd(OutputId) += 1;
	else if (FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Candidate, EGameXXKDesktopItemContainer::Backpack) != INDEX_NONE) Candidate.Inventory.Add(OutputId, 1);
	else if (FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Candidate, EGameXXKDesktopItemContainer::Warehouse) != INDEX_NONE) Candidate.DesktopInventory.WarehouseItems.Add(OutputId, 1);
	else
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("没有宝石合成产物空位"));
		return false;
	}
	const int64 Award = 9 * GetQualityExperienceMultiplier(FGameXXKGemRules::GetQualityRank(Quality));
	if (!AddAward(Candidate, Award, OutResult) || !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	OutResult.OutputEntryId = OutputId;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::Enhance(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKToolInputRef& Input,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!ResolveExact(InOutState, Input, false, OutResult) || !Input.ExpectedEntry.bEquipmentInstance) return false;
	const FGameXXKEquipmentInstance* Before = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Input.ExpectedEntry.EntryId);
	const int32 Rank = FGameXXKEquipmentQualityRules::GetRank(Before->Quality);
	FGameXXKRuntimeState Candidate = InOutState;
	if (!FGameXXKEquipmentEconomyRules::EnhanceInstance(Candidate, Input.ExpectedEntry.EntryId, OutResult)) return false;
	const int64 Award = GetQualityExperienceMultiplier(Rank);
	if (!AddAward(Candidate, Award, OutResult) || !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::BeginReforge(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKToolInputRef& Input,
	const int32 AffixIndex,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!ResolveExact(InOutState, Input, false, OutResult) || !Input.ExpectedEntry.bEquipmentInstance) return false;
	const FGameXXKEquipmentInstance* Before = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Input.ExpectedEntry.EntryId);
	const int32 Rank = FGameXXKEquipmentQualityRules::GetRank(Before->Quality);
	FGameXXKRuntimeState Candidate = InOutState;
	if (!FGameXXKEquipmentEconomyRules::BeginReforge(Candidate, Input.ExpectedEntry.EntryId, AffixIndex, OutResult)) return false;
	const int64 Award = GetQualityExperienceMultiplier(Rank);
	if (!AddAward(Candidate, Award, OutResult)) return false;
	Candidate.EquipmentCollection.PendingReforge.bToolExperienceAwarded = true;
	if (!Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::ResolveReforge(
	FGameXXKRuntimeState& InOutState,
	const bool bAccept,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Candidate.EquipmentCollection.PendingReforge.bActive
		|| !Candidate.EquipmentCollection.PendingReforge.bToolExperienceAwarded
		|| !FGameXXKEquipmentEconomyRules::ResolvePendingReforge(Candidate, bAccept, OutResult)
		|| !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::SocketGem(
	FGameXXKRuntimeState& InOutState,
	const FGameXXKSocketGemRequest& Request,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	if (!ResolveExact(InOutState, Request.EquipmentInput, false, OutResult)
		|| !ResolveExact(InOutState, Request.GemInput, false, OutResult)
		|| !Request.EquipmentInput.ExpectedEntry.bEquipmentInstance
		|| Request.GemInput.ExpectedEntry.bEquipmentInstance)
	{
		if (OutResult.Error == EGameXXKEquipmentTransactionError::None)
			Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidSocket, TEXT("镶嵌需要一件装备和一组宝石"));
		return false;
	}
	EGameXXKGemType NewType;
	EGameXXKGemQuality NewQuality;
	if (!FGameXXKGemRules::TryParseItemId(Request.GemInput.ExpectedEntry.EntryId, NewType, NewQuality))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidSocket, TEXT("镶嵌输入不是宝石"));
		return false;
	}
	const FGameXXKEquipmentInstance* Before = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Request.EquipmentInput.ExpectedEntry.EntryId);
	if (!Before || !Before->SocketedGems.IsValidIndex(Request.SocketIndex))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidSocket, TEXT("装备孔位无效"));
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	FGameXXKEquipmentInstance* Equipment = Candidate.EquipmentCollection.EquipmentInstances.FindByPredicate(
		[&Request](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == Request.EquipmentInput.ExpectedEntry.EntryId; });
	TMap<FName, int32>& NewGemItems = ItemsFor(Candidate, Request.GemInput.Container);
	const int32 NewRemaining = NewGemItems.FindRef(Request.GemInput.ExpectedEntry.EntryId) - 1;
	if (NewRemaining > 0) NewGemItems.Add(Request.GemInput.ExpectedEntry.EntryId, NewRemaining); else NewGemItems.Remove(Request.GemInput.ExpectedEntry.EntryId);
	const FGameXXKSocketedGem OldGem = Equipment->SocketedGems[Request.SocketIndex];
	Equipment->SocketedGems[Request.SocketIndex] = {NewType, NewQuality};
	if (!OldGem.IsEmpty())
	{
		const FName OldId = FGameXXKGemRules::MakeItemId(OldGem.Type, OldGem.Quality);
		if (Candidate.DesktopInventory.WarehouseItems.Contains(OldId) && !Candidate.Inventory.Contains(OldId))
		{
			Candidate.Inventory.Add(OldId, Candidate.DesktopInventory.WarehouseItems.FindRef(OldId));
			Candidate.DesktopInventory.WarehouseItems.Remove(OldId);
		}
		Candidate.Inventory.FindOrAdd(OldId) += 1;
	}
	const int64 Award = GetQualityExperienceMultiplier(FGameXXKEquipmentQualityRules::GetRank(Equipment->Quality));
	if (!AddAward(Candidate, Award, OutResult) || !Finish(Candidate, OutResult))
	{
		if (OutResult.Error == EGameXXKEquipmentTransactionError::CollectionInvalid)
			Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("背包没有空间返还原宝石"));
		return false;
	}
	OutResult.bSucceeded = true;
	OutResult.OutputEntryId = Request.EquipmentInput.ExpectedEntry.EntryId;
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::BuildCombineAutoFill(
	const FGameXXKRuntimeState& State,
	const EGameXXKToolCombineKind Kind,
	const bool bIncludeWarehouse,
	TArray<FGameXXKToolInputRef>& OutInputs,
	FString* OutError)
{
	OutInputs.Reset();
	SetError(OutError, FString());
	struct FCandidate
	{
		FGameXXKToolInputRef Ref;
		int32 QualityRank = 0;
		int32 EnhancedOrder = 0;
		int32 ItemLevel = 0;
	};
	TArray<FCandidate> Candidates;
	auto Collect = [&](const EGameXXKDesktopItemContainer Container, const TArray<FGameXXKDesktopInventoryEntryKey>& Slots)
	{
		for (int32 SlotIndex = 0; SlotIndex < Slots.Num(); ++SlotIndex)
		{
			const FGameXXKDesktopInventoryEntryKey& Entry = Slots[SlotIndex];
			if (!Entry.IsValid() || FGameXXKDesktopInventoryRules::IsEntryLocked(State, Entry)) continue;
			FGameXXKToolInputRef Ref{Container, SlotIndex, Entry};
			if (Kind == EGameXXKToolCombineKind::Equipment && Entry.bEquipmentInstance)
			{
				const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Entry.EntryId);
				if (Instance && Instance->OwnerKind == EGameXXKEquipmentOwnerKind::Warehouse && Instance->Quality != EGameXXKEquipmentQuality::Cosmic)
					Candidates.Add({Ref, FGameXXKEquipmentQualityRules::GetRank(Instance->Quality), Instance->EnhancementLevel > 0 ? 1 : 0, Instance->ItemLevel});
			}
			else if (Kind == EGameXXKToolCombineKind::Gem && !Entry.bEquipmentInstance)
			{
				EGameXXKGemType Type; EGameXXKGemQuality Quality;
				if (FGameXXKGemRules::TryParseItemId(Entry.EntryId, Type, Quality)
					&& Quality != EGameXXKGemQuality::Cosmic
					&& ItemsFor(State, Container).FindRef(Entry.EntryId) >= 9)
					Candidates.Add({Ref, FGameXXKGemRules::GetQualityRank(Quality), 0, static_cast<int32>(Type)});
			}
		}
	};
	Collect(EGameXXKDesktopItemContainer::Backpack, State.DesktopInventory.BackpackSlots);
	if (bIncludeWarehouse) Collect(EGameXXKDesktopItemContainer::Warehouse, State.DesktopInventory.WarehouseSlots);
	Candidates.Sort([](const FCandidate& A, const FCandidate& B)
	{
		if (A.QualityRank != B.QualityRank) return A.QualityRank < B.QualityRank;
		if (A.Ref.Container != B.Ref.Container) return A.Ref.Container == EGameXXKDesktopItemContainer::Backpack;
		if (A.EnhancedOrder != B.EnhancedOrder) return A.EnhancedOrder < B.EnhancedOrder;
		if (A.ItemLevel != B.ItemLevel) return A.ItemLevel < B.ItemLevel;
		return A.Ref.ExpectedEntry.EntryId.LexicalLess(B.Ref.ExpectedEntry.EntryId);
	});
	if (Kind == EGameXXKToolCombineKind::Gem)
	{
		if (!Candidates.IsEmpty()) OutInputs.Add(Candidates[0].Ref);
	}
	else
	{
		for (int32 Rank = 1; Rank <= 9; ++Rank)
		{
			for (const FCandidate& Candidate : Candidates)
				if (Candidate.QualityRank == Rank && OutInputs.Num() < 9) OutInputs.Add(Candidate.Ref);
			if (OutInputs.Num() == 9) break;
			OutInputs.Reset();
		}
	}
	if (OutInputs.IsEmpty())
	{
		SetError(OutError, TEXT("没有满足条件的 9 合 1 输入"));
		return false;
	}
	return true;
}
