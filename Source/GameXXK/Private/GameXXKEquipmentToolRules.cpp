#include "GameXXKEquipmentToolRules.h"

#include "GameXXKEquipmentEconomyRules.h"
#include "GameXXKToolCombineProbability.h"
#include "GameXXKToolSelectionRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKTalentRules.h"
#include "GameXXKTravelMoneyRules.h"
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
			if (State.EquipmentCollection.PendingReforge.bActive
				&& State.EquipmentCollection.PendingReforge.InstanceId == Ref.ExpectedEntry.EntryId)
			{
				Fail(Out, EGameXXKEquipmentTransactionError::PendingReforgeExists, TEXT("请先采用或保留该装备的洗炼结果"));
				return false;
			}
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

	bool ValidateToolTalents(
		const FGameXXKRuntimeState& State,
		FGameXXKEquipmentTransactionResult& Out)
	{
		FGameXXKTalentProjection Projection;
		if (!FGameXXKTalentRules::BuildProjection(State.Talents, Projection))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("天赋数据无效"));
			return false;
		}
		return true;
	}

	bool AddAward(FGameXXKRuntimeState& Candidate, const int64 Award, FGameXXKEquipmentTransactionResult& Out)
	{
		FGameXXKTalentProjection Projection;
		if (!FGameXXKTalentRules::BuildProjection(Candidate.Talents, Projection))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("天赋数据无效"));
			return false;
		}
		const int64 BoostedAward = FMath::RoundToInt64(
			static_cast<double>(Award) * Projection.GetToolExperienceMultiplier());
		if (BoostedAward < 0
			|| !FGameXXKEquipmentToolRules::AddRawExperience(Candidate.ToolProgress, BoostedAward))
		{
			Fail(Out, EGameXXKEquipmentTransactionError::InvalidRequest, TEXT("工具经验溢出"));
			return false;
		}
		Out.ToolExperienceDelta = BoostedAward;
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
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateToolTalents(InOutState, OutResult))
	{
		return false;
	}
	if (Inputs.Num() < 1 || Inputs.Num() > 9)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("分解需要 1 到 9 格装备或行旅钱"));
		return false;
	}
	TArray<FName> Ids;
	TArray<FGameXXKToolInputRef> MoneyInputs;
	TSet<uint64> SourceSlots;
	int64 MoneyQuantity = 0;
	int64 Award = 0;
	for (const FGameXXKToolInputRef& Ref : Inputs)
	{
		if (!ResolveExact(InOutState, Ref, true, OutResult)) return false;
		const uint64 SourceSlot = (static_cast<uint64>(Ref.Container) << 32) | static_cast<uint32>(Ref.SlotIndex);
		if (SourceSlots.Contains(SourceSlot))
		{
			Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("不能重复分解同一个来源格"));
			return false;
		}
		SourceSlots.Add(SourceSlot);
		if (!Ref.ExpectedEntry.bEquipmentInstance)
		{
			const int32 Held = ItemsFor(InOutState, Ref.Container).FindRef(Ref.ExpectedEntry.EntryId);
			const int32 Quantity = Ref.Quantity == 0 ? Held : Ref.Quantity;
			if (Ref.ExpectedEntry.EntryId != FGameXXKTravelMoneyRules::ItemId() || Quantity <= 0 || Quantity > Held)
			{
				Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("分解输入或行旅钱数量无效"));
				return false;
			}
			FGameXXKToolInputRef& MoneyRef = MoneyInputs.Add_GetRef(Ref);
			MoneyRef.Quantity = Quantity;
			MoneyQuantity += Quantity;
			continue;
		}
		if (Ids.Contains(Ref.ExpectedEntry.EntryId))
		{
			Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, TEXT("分解输入必须是不同的未装备装备"));
			return false;
		}
		const FGameXXKEquipmentInstance* Instance = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Ref.ExpectedEntry.EntryId);
		Award += GetQualityExperienceMultiplier(FGameXXKEquipmentQualityRules::GetRank(Instance->Quality));
		Ids.Add(Ref.ExpectedEntry.EntryId);
	}
	if (MoneyQuantity > 0 && !bConfirmed)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::ConfirmationRequired, TEXT("确认将行旅钱按每个 2500 金币分解"));
		OutResult.bConfirmationRequired = true;
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	if (!Ids.IsEmpty() && !FGameXXKEquipmentEconomyRules::DismantleBatch(Candidate, Ids, bConfirmed, OutResult))
	{
		return false;
	}
	FGameXXKTalentProjection TalentProjection;
	if (!FGameXXKTalentRules::BuildProjection(Candidate.Talents, TalentProjection))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::CollectionInvalid, TEXT("天赋数据无效"));
		return false;
	}
	const int64 BaseGoldAward = static_cast<int64>(Candidate.PlayerGold) - InOutState.PlayerGold;
	const int64 BoostedGoldAward = FMath::RoundToInt64(
		static_cast<double>(BaseGoldAward) * TalentProjection.GetToolGoldMultiplier());
	// Currency redemption is fixed; talent bonuses apply only to equipment rewards.
	const int64 BoostedGoldTotal = static_cast<int64>(InOutState.PlayerGold) + BoostedGoldAward
		+ MoneyQuantity * FGameXXKTravelMoneyRules::DismantleGoldPerUnit;
	if (BoostedGoldAward < BaseGoldAward || BoostedGoldTotal > MAX_int32)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest, TEXT("工具金币奖励溢出"));
		return false;
	}
	Candidate.PlayerGold = static_cast<int32>(BoostedGoldTotal);
	for (const FGameXXKToolInputRef& Ref : MoneyInputs)
	{
		TMap<FName, int32>& Items = ItemsFor(Candidate, Ref.Container);
		const int32 Remaining = Items.FindRef(Ref.ExpectedEntry.EntryId) - Ref.Quantity;
		if (Remaining > 0) Items.Add(Ref.ExpectedEntry.EntryId, Remaining);
		else Items.Remove(Ref.ExpectedEntry.EntryId);
	}
	if (!AddAward(Candidate, Award, OutResult)
		|| !Finish(Candidate, OutResult)) return false;
	OutResult.bSucceeded = true;
	if (MoneyQuantity > 0) OutResult.Message = FText::FromString(FString::Printf(
		TEXT("已分解 %lld 个行旅钱，兑换 %lld 金币"), MoneyQuantity,
		MoneyQuantity * FGameXXKTravelMoneyRules::DismantleGoldPerUnit));
	InOutState = MoveTemp(Candidate);
	return true;
}

bool FGameXXKEquipmentToolRules::CombineEquipment(
	FGameXXKRuntimeState& InOutState,
	const TArray<FGameXXKToolInputRef>& Inputs,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	FString RecipeError;
	if (GetCombineInputQualityRank(InOutState, EGameXXKToolCombineKind::Equipment, Inputs, &RecipeError) == 0)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, *RecipeError);
		return false;
	}
	if (!ValidateToolTalents(InOutState, OutResult))
	{
		return false;
	}
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
	FGameXXKTalentProjection Capacity;
	if (!FGameXXKTalentRules::BuildProjection(InOutState.Talents, Capacity)) return false;
	const bool bOutputToBackpack = BackpackAfter < Capacity.BackpackCapacity;
	if (!bOutputToBackpack && WarehouseAfter >= Capacity.WarehousePageCount * 20)
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
	Request.Quality = FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(
		FGameXXKToolCombineProbability::Resolve(FGameXXKEquipmentQualityRules::GetRank(InputQuality), Stream.RandRange(0, 999)));
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
	TArray<FGameXXKToolInputRef> Inputs;
	FGameXXKToolInputRef Unit = Input;
	Unit.Quantity = 1;
	Inputs.Init(Unit, 9);
	return CombineGems(InOutState, Inputs, OutResult);
}

int32 FGameXXKEquipmentToolRules::GetCombineInputQualityRank(
	const FGameXXKRuntimeState& State, const EGameXXKToolCombineKind Kind,
	const TArray<FGameXXKToolInputRef>& Inputs, FString* OutError)
{
	SetError(OutError, FString());
	if (Inputs.Num() != 9)
	{
		SetError(OutError, FString::Printf(TEXT("已放入 %d/9，需同品质九件"), Inputs.Num()));
		return 0;
	}
	int32 Rank = 0;
	TSet<FName> EquipmentIds;
	TMap<FName, int32> GemCounts;
	for (const auto& Ref : Inputs)
	{
		FGameXXKEquipmentTransactionResult Check;
		if (!ResolveExact(State, Ref, true, Check))
		{
			SetError(OutError, Check.Message.ToString());
			return 0;
		}
		int32 CurrentRank = 0;
		if (Kind == EGameXXKToolCombineKind::Equipment && Ref.ExpectedEntry.bEquipmentInstance)
		{
			if (EquipmentIds.Contains(Ref.ExpectedEntry.EntryId) || Ref.Quantity < 0 || Ref.Quantity > 1)
			{
				SetError(OutError, TEXT("不能重复放入同一件装备"));
				return 0;
			}
			EquipmentIds.Add(Ref.ExpectedEntry.EntryId);
			CurrentRank = FGameXXKEquipmentQualityRules::GetRank(FGameXXKEquipmentRules::FindInstance(
				State.EquipmentCollection, Ref.ExpectedEntry.EntryId)->Quality);
		}
		else if (Kind == EGameXXKToolCombineKind::Gem && !Ref.ExpectedEntry.bEquipmentInstance)
		{
			EGameXXKGemType Type;
			EGameXXKGemQuality Quality;
			if (Ref.Quantity < 0 || Ref.Quantity > 1 || !FGameXXKGemRules::TryParseItemId(Ref.ExpectedEntry.EntryId, Type, Quality)
				|| ++GemCounts.FindOrAdd(Ref.ExpectedEntry.EntryId) > ItemsFor(State, Ref.Container).FindRef(Ref.ExpectedEntry.EntryId))
			{
				SetError(OutError, TEXT("每格需一颗宝石，来源数量不足或已变化"));
				return 0;
			}
			CurrentRank = FGameXXKGemRules::GetQualityRank(Quality);
		}
		if (CurrentRank < 1 || CurrentRank >= 10 || (Rank && Rank != CurrentRank))
		{
			SetError(OutError, CurrentRank == 10 ? TEXT("宇宙品质已达上限") : TEXT("九格必须同品质，装备和宝石不能混合"));
			return 0;
		}
		Rank = CurrentRank;
	}
	return Rank;
}

bool FGameXXKEquipmentToolRules::CombineGems(
	FGameXXKRuntimeState& InOutState, const TArray<FGameXXKToolInputRef>& Inputs,
	FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	FString RecipeError;
	const int32 Rank = GetCombineInputQualityRank(InOutState, EGameXXKToolCombineKind::Gem, Inputs, &RecipeError);
	if (!Rank)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRecipe, *RecipeError);
		return false;
	}
	if (!ValidateToolTalents(InOutState, OutResult)) return false;
	if (InOutState.EquipmentCollection.NextInstanceOrdinal == MAX_int32)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidRequest, TEXT("合成序号已达上限"));
		return false;
	}
	TArray<FString> SeedParts;
	for (const auto& Ref : Inputs) SeedParts.Add(Ref.ExpectedEntry.EntryId.ToString());
	SeedParts.Sort();
	const FString SeedText = FString::Printf(TEXT("Gem|%d|%d|%s"), InOutState.EquipmentCollection.CollectionSeed,
		InOutState.EquipmentCollection.NextInstanceOrdinal, *FString::Join(SeedParts, TEXT("|")));
	FRandomStream Stream(static_cast<int32>(FCrc::StrCrc32(*SeedText)));
	const EGameXXKGemQuality OutputQuality = FGameXXKGemRules::QualityFromRank(
		FGameXXKToolCombineProbability::Resolve(Rank, Stream.RandRange(0, 999)));
	TArray<FName> OutputIds;
	for (const FName Id : FGameXXKGemRules::GetAllItemIds())
	{
		EGameXXKGemType Type;
		EGameXXKGemQuality Quality;
		if (FGameXXKGemRules::TryParseItemId(Id, Type, Quality) && Quality == OutputQuality) OutputIds.Add(Id);
	}
	if (OutputIds.IsEmpty()) return false;
	OutputIds.Sort(FNameLexicalLess());
	const FName OutputId = OutputIds[Stream.RandRange(0, OutputIds.Num() - 1)];
	FGameXXKRuntimeState Candidate = InOutState;
	for (const auto& Ref : Inputs)
	{
		auto& SourceItems = ItemsFor(Candidate, Ref.Container);
		const int32 Remaining = SourceItems.FindRef(Ref.ExpectedEntry.EntryId) - 1;
		if (Remaining > 0) SourceItems.Add(Ref.ExpectedEntry.EntryId, Remaining); else SourceItems.Remove(Ref.ExpectedEntry.EntryId);
	}
	FString Error;
	if (!FGameXXKDesktopInventoryRules::Normalize(Candidate, &Error))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("宝石合成输入整理失败"));
		return false;
	}
	if (Candidate.Inventory.FindRef(OutputId) == MAX_int32 || Candidate.DesktopInventory.WarehouseItems.FindRef(OutputId) == MAX_int32)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("宝石数量已达上限"));
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
	++Candidate.EquipmentCollection.NextInstanceOrdinal;
	const int64 Award = 9 * GetQualityExperienceMultiplier(Rank);
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
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateToolTalents(InOutState, OutResult))
	{
		return false;
	}
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
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateToolTalents(InOutState, OutResult))
	{
		return false;
	}
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
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ValidateToolTalents(InOutState, OutResult))
	{
		return false;
	}
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
		const int64 ReturnedTotal = static_cast<int64>(Candidate.Inventory.FindRef(OldId))
			+ Candidate.DesktopInventory.WarehouseItems.FindRef(OldId) + 1;
		if (ReturnedTotal > MAX_int32)
		{
			Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("返还宝石数量超出上限"));
			return false;
		}
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

bool FGameXXKEquipmentToolRules::RemoveSocketGem(
	FGameXXKRuntimeState& InOutState, const FGameXXKToolInputRef& Input,
	const int32 SocketIndex, FGameXXKEquipmentTransactionResult& OutResult)
{
	OutResult = FGameXXKEquipmentTransactionResult();
	if (!ResolveExact(InOutState, Input, false, OutResult) || !Input.ExpectedEntry.bEquipmentInstance) return false;
	const auto* Before = FGameXXKEquipmentRules::FindInstance(InOutState.EquipmentCollection, Input.ExpectedEntry.EntryId);
	if (!Before || !Before->SocketedGems.IsValidIndex(SocketIndex) || Before->SocketedGems[SocketIndex].IsEmpty())
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InvalidSocket, TEXT("请选择已镶嵌的宝石"));
		return false;
	}
	const auto Gem = Before->SocketedGems[SocketIndex];
	const FName GemId = FGameXXKGemRules::MakeItemId(Gem.Type, Gem.Quality);
	const int64 Count = static_cast<int64>(InOutState.Inventory.FindRef(GemId))
		+ InOutState.DesktopInventory.WarehouseItems.FindRef(GemId) + 1;
	if (Count > MAX_int32)
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("返还宝石数量超出上限"));
		return false;
	}
	FGameXXKRuntimeState Candidate = InOutState;
	auto* Item = Candidate.EquipmentCollection.EquipmentInstances.FindByPredicate(
		[&](const auto& Instance) { return Instance.InstanceId == Input.ExpectedEntry.EntryId; });
	Item->SocketedGems[SocketIndex] = FGameXXKSocketedGem();
	// Inventory currently owns one stack per item ID across the two containers.
	Candidate.DesktopInventory.WarehouseItems.Remove(GemId);
	Candidate.Inventory.Add(GemId, static_cast<int32>(Count));
	if (!Finish(Candidate, OutResult))
	{
		Fail(OutResult, EGameXXKEquipmentTransactionError::InventoryFull, TEXT("背包没有空位返还宝石"));
		return false;
	}
	OutResult.bSucceeded = true;
	OutResult.OutputEntryId = GemId;
	OutResult.Message = FText::FromString(TEXT("宝石已拆卸并返回背包"));
	InOutState = MoveTemp(Candidate);
	return true;
}

int32 FGameXXKToolSelectionRules::GetEntryQualityRank(const FGameXXKRuntimeState& State, const FGameXXKDesktopInventoryEntryKey& Entry)
{
	if (Entry.bEquipmentInstance)
	{
		const auto* Item = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Entry.EntryId);
		return Item ? FGameXXKEquipmentQualityRules::GetRank(Item->Quality) : 0;
	}
	EGameXXKGemType Type;
	EGameXXKGemQuality Quality;
	return FGameXXKGemRules::TryParseItemId(Entry.EntryId, Type, Quality) ? FGameXXKGemRules::GetQualityRank(Quality) : 0;
}

bool FGameXXKToolSelectionRules::BuildAutoFillForSelection(
	const FGameXXKRuntimeState& State, const EGameXXKToolCombineKind Kind, const bool bIncludeWarehouse,
	const bool bDismantle, const TArray<FGameXXKToolInputRef>& Existing,
	TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError)
{
	OutInputs.Reset();
	SetError(OutError, FString());
	if (Existing.Num() > 9) return false;
	int32 AnchorRank = 0;
	TMap<FName, int32> ReservedCounts;
	for (const auto& Ref : Existing)
	{
		FGameXXKEquipmentTransactionResult Check;
		const int32 Rank = GetEntryQualityRank(State, Ref.ExpectedEntry);
		if (!ResolveExact(State, Ref, true, Check))
		{
			SetError(OutError, Check.Message.ToString());
			return false;
		}
		if (Rank < 1 || Rank > (bDismantle ? 10 : 9)
			|| Ref.ExpectedEntry.bEquipmentInstance != (Kind == EGameXXKToolCombineKind::Equipment)
			|| (AnchorRank && Rank != AnchorRank) || Ref.Quantity < 0 || Ref.Quantity > 1)
		{
			SetError(OutError, TEXT("自动放置需要同品质同类别的装备或宝石；行旅钱可直接分解"));
			return false;
		}
		AnchorRank = Rank;
		const int32 Used = ++ReservedCounts.FindOrAdd(Ref.ExpectedEntry.EntryId);
		const int32 Held = Ref.ExpectedEntry.bEquipmentInstance ? 1 : ItemsFor(State, Ref.Container).FindRef(Ref.ExpectedEntry.EntryId);
		if (Used > Held)
		{
			SetError(OutError, TEXT("现有工具输入数量已变化"));
			return false;
		}
	}
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
			const auto& Entry = Slots[SlotIndex];
			if (!Entry.IsValid() || FGameXXKDesktopInventoryRules::IsEntryLocked(State, Entry)) continue;
			const int32 Rank = GetEntryQualityRank(State, Entry);
			if (Rank < 1 || Rank > (bDismantle ? 10 : 9) || (AnchorRank && Rank != AnchorRank)) continue;
			const FGameXXKToolInputRef Ref{Container, SlotIndex, Entry, 1};
			if (Kind == EGameXXKToolCombineKind::Equipment && Entry.bEquipmentInstance && !ReservedCounts.Contains(Entry.EntryId))
			{
				const auto* Item = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Entry.EntryId);
				if (Item && Item->OwnerKind == EGameXXKEquipmentOwnerKind::Warehouse
					&& (!State.EquipmentCollection.PendingReforge.bActive || State.EquipmentCollection.PendingReforge.InstanceId != Entry.EntryId))
					Candidates.Add({Ref, Rank, Item->EnhancementLevel > 0 ? 1 : 0, Item->ItemLevel});
			}
			else if (Kind == EGameXXKToolCombineKind::Gem && !Entry.bEquipmentInstance)
			{
				const int32 Available = ItemsFor(State, Container).FindRef(Entry.EntryId) - ReservedCounts.FindRef(Entry.EntryId);
				for (int32 Unit = 0; Unit < FMath::Min(9, Available); ++Unit) Candidates.Add({Ref, Rank, 0, 0});
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
	if (AnchorRank)
	{
		OutInputs = Existing;
		for (auto& Ref : OutInputs) Ref.Quantity = 1;
		for (const auto& Candidate : Candidates)
			if (OutInputs.Num() < 9) OutInputs.Add(Candidate.Ref);
		return true;
	}
	TArray<FGameXXKToolInputRef> BestPartial;
	for (int32 Rank = 1; Rank <= (bDismantle ? 10 : 9); ++Rank)
	{
		for (const auto& Candidate : Candidates)
			if (Candidate.QualityRank == Rank && OutInputs.Num() < 9) OutInputs.Add(Candidate.Ref);
		if (OutInputs.Num() == 9) return true;
		// Iterate low quality first; equal counts retain the lower-quality group.
		if (OutInputs.Num() > BestPartial.Num()) BestPartial = OutInputs;
		OutInputs.Reset();
	}
	if (!BestPartial.IsEmpty())
	{
		OutInputs = MoveTemp(BestPartial);
		return true;
	}
	SetError(OutError, TEXT("没有可自动放入的未锁定物品"));
	return false;
}

bool FGameXXKEquipmentToolRules::BuildCombineAutoFill(const FGameXXKRuntimeState& State,
	const EGameXXKToolCombineKind Kind, const bool bIncludeWarehouse,
	TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError)
{
	return FGameXXKToolSelectionRules::BuildAutoFillForSelection(State, Kind, bIncludeWarehouse, false, {}, OutInputs, OutError);
}

bool FGameXXKEquipmentToolRules::BuildDismantleAutoFill(const FGameXXKRuntimeState& State,
	const bool bIncludeWarehouse, TArray<FGameXXKToolInputRef>& OutInputs, FString* OutError)
{
	return FGameXXKToolSelectionRules::BuildAutoFillForSelection(State, EGameXXKToolCombineKind::Equipment, bIncludeWarehouse, true, {}, OutInputs, OutError);
}
