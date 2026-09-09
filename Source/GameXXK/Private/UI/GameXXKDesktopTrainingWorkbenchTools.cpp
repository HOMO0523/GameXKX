#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "GameXXKEquipmentCatalog.h"
#include "GameXXKGemRules.h"
#include "GameXXKToolCombineProbability.h"
#include "GameXXKToolSelectionRules.h"
#include "GameXXKTravelMoneyRules.h"
#include "GameXXKTalentRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKEquipmentTooltipPresentation.h"

int32 UGameXXKDesktopTrainingWorkbenchWidget::GetDesktopAvailableQuantity(
    const EGameXXKDesktopItemContainer Container, const FGameXXKDesktopInventoryEntryKey& Entry) const
{
    const auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem || !Entry.IsValid()) return 0;
    const auto& State = Subsystem->GetRuntimeState();
    int32 Available = Entry.bEquipmentInstance ? 1 : (Container == EGameXXKDesktopItemContainer::Backpack
        ? State.Inventory.FindRef(Entry.EntryId) : State.DesktopInventory.WarehouseItems.FindRef(Entry.EntryId));
    for (const auto& Reserved : ToolSlots)
        if (Reserved.Entry == Entry && Reserved.AuthoritativeContainer == Container) Available -= Reserved.Quantity;
    if (CarriedEntry.IsValid() && CarriedEntry.Payload.Entry == Entry && CarriedEntry.Payload.AuthoritativeContainer == Container)
        Available -= CarriedEntry.Payload.Quantity;
    return FMath::Max(0, Available);
}

const FGameXXKEquipmentInstance* UGameXXKDesktopTrainingWorkbenchWidget::GetToolEquipment() const
{
    const auto* Subsystem = ResolveMVPSubsystem();
    return Subsystem && ToolSlots.IsValidIndex(0) && ToolSlots[0].Entry.bEquipmentInstance
        ? FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection, ToolSlots[0].Entry.EntryId) : nullptr;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::IsToolCellActive(const int32 Index) const
{
    if (Index < 0 || Index >= 9) return false;
    if (ActiveToolMode == EGameXXKDesktopToolMode::Dismantle || ActiveToolMode == EGameXXKDesktopToolMode::Combine) return true;
    if (Index == 0) return true;
    const auto* Equipment = GetToolEquipment();
    return ActiveToolMode == EGameXXKDesktopToolMode::Socket && Equipment && Equipment->SocketedGems.IsValidIndex(Index - 1);
}

bool UGameXXKDesktopTrainingWorkbenchWidget::CanPlaceEntryInToolCell(
    const FGameXXKDesktopInventoryEntryKey& Entry, const int32 Index) const
{
    if (!Entry.IsValid() || !IsToolCellActive(Index)) return false;
    EGameXXKGemType Type;
    EGameXXKGemQuality Quality;
    const bool Gem = !Entry.bEquipmentInstance && FGameXXKGemRules::TryParseItemId(Entry.EntryId, Type, Quality);
    bool KindMatches = false;
    switch (ActiveToolMode)
    {
    case EGameXXKDesktopToolMode::Dismantle: KindMatches = Entry.bEquipmentInstance || Entry.EntryId == FGameXXKTravelMoneyRules::ItemId(); break;
    case EGameXXKDesktopToolMode::Combine: KindMatches = ActiveToolCombineKind == EGameXXKToolCombineKind::Equipment ? Entry.bEquipmentInstance : Gem; break;
    case EGameXXKDesktopToolMode::Socket: return Index == 0 ? Entry.bEquipmentInstance : Gem;
    default: return Index == 0 && Entry.bEquipmentInstance;
    }
    if (!KindMatches) return false;
    const auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem) return false;
    const auto& State = Subsystem->GetRuntimeState();
    const int32 Rank = FGameXXKToolSelectionRules::GetEntryQualityRank(State, Entry);
    if (Rank > 0)
    {
        for (int32 Other = 0; Other < ToolSlots.Num(); ++Other)
        {
            if (Other == Index || !ToolSlots[Other].IsValid()) continue;
            const int32 ExistingRank = FGameXXKToolSelectionRules::GetEntryQualityRank(State, ToolSlots[Other].Entry);
            if (ExistingRank > 0 && ExistingRank != Rank) return false;
        }
    }
    return true;
}

TArray<FGameXXKToolInputRef> UGameXXKDesktopTrainingWorkbenchWidget::BuildToolInputs() const
{
    TArray<FGameXXKToolInputRef> Inputs;
    for (const auto& Entry : ToolSlots)
        if (Entry.IsValid()) Inputs.Add({Entry.AuthoritativeContainer, Entry.AuthoritativeSlotIndex, Entry.Entry, Entry.Quantity});
    return Inputs;
}

void UGameXXKDesktopTrainingWorkbenchWidget::ReconcileToolSlotsForMode()
{
    ToolSlots.SetNum(9);
    if (ActiveToolMode != EGameXXKDesktopToolMode::Combine && ActiveToolMode != EGameXXKDesktopToolMode::Dismantle)
    {
        const int32 Target = ToolSlots.IndexOfByPredicate([](const auto& Entry) { return Entry.IsValid() && Entry.Entry.bEquipmentInstance; });
        const auto Kept = Target == INDEX_NONE ? FDesktopToolEntry() : ToolSlots[Target];
        ReturnAllToolEntries();
        ToolSlots[0] = Kept;
    }
    else
    {
        for (int32 Index = 0; Index < ToolSlots.Num(); ++Index)
            if (ToolSlots[Index].IsValid() && !CanPlaceEntryInToolCell(ToolSlots[Index].Entry, Index)) ToolSlots[Index] = FDesktopToolEntry();
    }
    SelectedToolSocketIndex = INDEX_NONE;
    SelectedToolAffixIndex = 0;
}

void UGameXXKDesktopTrainingWorkbenchWidget::RefreshToolTargetSource()
{
    auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem || !ToolSlots.IsValidIndex(0) || !ToolSlots[0].IsValid()) return;
    auto& Target = ToolSlots[0];
    for (const auto Container : {EGameXXKDesktopItemContainer::Backpack, EGameXXKDesktopItemContainer::Warehouse})
    {
        const int32 SourceSlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(Subsystem->GetRuntimeState(), Container, Target.Entry);
        if (SourceSlotIndex != INDEX_NONE)
        {
            Target.AuthoritativeContainer = Container;
            Target.AuthoritativeSlotIndex = SourceSlotIndex;
            return;
        }
    }
    ReturnAllToolEntries();
}

bool UGameXXKDesktopTrainingWorkbenchWidget::BuildToolStatus(FString& Text) const
{
    const auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem) { Text = TEXT("工具尚未就绪"); return false; }
    const auto& State = Subsystem->GetRuntimeState();
    const auto Inputs = BuildToolInputs();
    const auto* Item = GetToolEquipment();
    const auto& Pending = State.EquipmentCollection.PendingReforge;
    if (ActiveToolMode == EGameXXKDesktopToolMode::Combine)
    {
        const int32 Rank = FGameXXKEquipmentToolRules::GetCombineInputQualityRank(State, ActiveToolCombineKind, Inputs, &Text);
        if (!Rank)
        {
            if (Inputs.Num() < 9)
            {
                const int32 SelectedRank = Inputs.IsEmpty() ? 0 : FGameXXKToolSelectionRules::GetEntryQualityRank(State, Inputs[0].ExpectedEntry);
                const FString Quality = SelectedRank > 0 ? FGameXXKGemRules::GetQualityDisplayName(FGameXXKGemRules::QualityFromRank(SelectedRank)).ToString() + TEXT(" · ") : FString();
                Text = FString::Printf(TEXT("%s%d / 9"), *Quality, Inputs.Num());
            }
            return false;
        }
        Text = FGameXXKToolCombineProbability::Describe(Rank).ToString();
        return !CarriedEntry.IsValid();
    }
    if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge && Pending.bActive)
    {
        Text = FString::Printf(TEXT("已消耗洗炼砂 %d"), Pending.PaidRefinementSand);
        return false;
    }
    if (Inputs.IsEmpty())
    {
        if (ActiveToolMode == EGameXXKDesktopToolMode::Enhance)
            Text = FString::Printf(TEXT("强化石 %d"), State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone()));
        else if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge)
            Text = FString::Printf(TEXT("洗炼砂 %d"), State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()));
        else Text.Reset();
        return false;
    }
    if (CarriedEntry.IsValid()) { Text = TEXT("请先放下手中物品"); return false; }
    for (const auto& Input : Inputs)
    {
        if (FGameXXKDesktopInventoryRules::GetEntryAt(State, Input.Container, Input.SlotIndex) != Input.ExpectedEntry)
        { Text = TEXT("来源已变化，请重新放入物品"); return false; }
        if (Input.ExpectedEntry.bEquipmentInstance && Pending.bActive && Pending.InstanceId == Input.ExpectedEntry.EntryId)
        { Text = TEXT("请先采用或保留洗炼结果"); return false; }
    }
    if (ActiveToolMode == EGameXXKDesktopToolMode::Dismantle)
    {
        int32 EquipmentCount = 0;
        int64 MoneyCount = 0;
        for (const auto& Input : Inputs)
        {
            if (FGameXXKDesktopInventoryRules::IsEntryLocked(State, Input.ExpectedEntry))
            { Text = TEXT("请先移出锁定物品"); return false; }
            if (Input.ExpectedEntry.bEquipmentInstance) ++EquipmentCount;
            else if (Input.ExpectedEntry.EntryId == FGameXXKTravelMoneyRules::ItemId()) MoneyCount += Input.Quantity;
            else { Text = TEXT("只能分解装备或行旅钱"); return false; }
        }
        FGameXXKTalentProjection Projection;
        FGameXXKTalentRules::BuildProjection(State.Talents, Projection);
        const int64 Gold = FMath::RoundToInt64(EquipmentCount * 10.0 * Projection.GetToolGoldMultiplier())
            + MoneyCount * FGameXXKTravelMoneyRules::DismantleGoldPerUnit;
        Text = FString::Printf(TEXT("金币 +%lld"), Gold);
        if (EquipmentCount) Text += FString::Printf(TEXT("\n强化石 +%d · 洗炼砂 +%d"), EquipmentCount, EquipmentCount);
        return true;
    }
    if (!Item) { Text = TEXT("请在首格放入装备"); return false; }
    if (ActiveToolMode == EGameXXKDesktopToolMode::Enhance)
    {
        if (Item->EnhancementLevel >= FGameXXKEquipmentRules::MaxEnhancementLevel)
        { Text = TEXT("强化已达上限"); return false; }
        const int32 Cost = FGameXXKEquipmentCatalog::GetEnhancementStoneCost(Item->EnhancementLevel);
        const int32 Held = State.Inventory.FindRef(UGameXXKMVPRules::ItemEnhancementStone());
        Text = FString::Printf(TEXT("+%d → +%d\n强化石 %d / %d"), Item->EnhancementLevel, Item->EnhancementLevel + 1, Held, Cost);
        return Held >= Cost;
    }
    if (ActiveToolMode == EGameXXKDesktopToolMode::Reforge)
    {
        const int32 Cost = FGameXXKEquipmentCatalog::GetReforgeSandCost(Item->Quality);
        const int32 Held = State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand());
        Text = FString::Printf(TEXT("洗炼砂 %d / %d"), Held, Cost);
        return Held >= Cost && Item->RolledAffixes.IsValidIndex(SelectedToolAffixIndex);
    }
    if (ActiveToolMode == EGameXXKDesktopToolMode::Socket)
    {
        const bool Selected = Item->SocketedGems.IsValidIndex(SelectedToolSocketIndex) && !Item->SocketedGems[SelectedToolSocketIndex].IsEmpty();
        Text = Selected ? FString::Printf(TEXT("已选第 %d 孔"), SelectedToolSocketIndex + 1) : FString();
        return Selected;
    }
    return false;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::DropCarriedGemInSocket(const int32 Index)
{
    auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem || Index < 1 || !CarriedEntry.IsValid() || !CanPlaceEntryInToolCell(CarriedEntry.Payload.Entry, Index) || !GetToolEquipment()) return false;
    const auto& Target = ToolSlots[0];
    const auto& Gem = CarriedEntry.Payload;
    FGameXXKSocketGemRequest Request;
    Request.EquipmentInput = {Target.AuthoritativeContainer, Target.AuthoritativeSlotIndex, Target.Entry, 1};
    Request.GemInput = {Gem.AuthoritativeContainer, Gem.AuthoritativeSlotIndex, Gem.Entry, 1};
    Request.SocketIndex = Index - 1;
    FGameXXKEquipmentTransactionResult Result;
    if (!Subsystem->ExecuteToolSocket(Request, Result))
    {
        SetNotice(Result.Message, EGameXXKDesktopNoticeCategory::Socket);
        return false;
    }
    // A socket action consumes one unit; remaining source gems become available again.
    CarriedEntry.Reset();
    SelectedToolSocketIndex = Index - 1;
    RefreshToolTargetSource();
    SetNotice(FText::FromString(TEXT("镶嵌完成，可继续放入宝石")), EGameXXKDesktopNoticeCategory::Socket);
    RefreshLayout();
    return true;
}

bool UGameXXKDesktopTrainingWorkbenchWidget::RemoveSelectedToolGem()
{
    auto* Subsystem = ResolveMVPSubsystem();
    if (!Subsystem || !GetToolEquipment() || CarriedEntry.IsValid()) return false;
    const auto& Target = ToolSlots[0];
    FGameXXKEquipmentTransactionResult Result;
    if (!Subsystem->ExecuteToolRemoveSocketGem({Target.AuthoritativeContainer, Target.AuthoritativeSlotIndex, Target.Entry, 1}, SelectedToolSocketIndex, Result))
    {
        SetNotice(Result.Message, EGameXXKDesktopNoticeCategory::Socket);
        return false;
    }
    RefreshToolTargetSource();
    SelectedToolSocketIndex = INDEX_NONE;
    SetNotice(Result.Message, EGameXXKDesktopNoticeCategory::Socket);
    RefreshLayout();
    return true;
}
