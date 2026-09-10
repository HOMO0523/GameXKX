#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKHuntRules.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKTravelMoneyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKChestStoredStackTest,
    "GameXXK.Training.Chests.StoredStackAndRollback",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKChestStoredStackTest::RunTest(const FString&)
{
    auto* MVP = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
    if (!TestTrue(TEXT("fixture starts"), MVP->StartGame())) return false;
    auto Base = MVP->GetRuntimeStateCopy();
    FString Error;
    const auto Tier = EGameXXKTrainingRewardTier::NormalChest;
    if (!TestTrue(TEXT("append chest"), FGameXXKTrainingRules::AppendChestToken(Base.Training, Tier,
        FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), 5, &Error))) return false;
    bool Gem = false, Material = false, Order = false, Money = false;
    for (int32 Seed = 1; Seed <= 1024 && !(Gem && Material && Order && Money); ++Seed)
    {
        auto Initial = Base;
        Initial.Training.ChallengeRewardSeed = Seed;
        auto Preview = Initial;
        FGameXXKTrainingChestOpenResult Loot;
        if (!FGameXXKTrainingChestRules::OpenOne(Preview, Tier, Loot) || Loot.ItemDeltas.IsEmpty()) continue;
        const auto Pair = *Loot.ItemDeltas.CreateConstIterator();
        const FName Id = Pair.Key;
        if (Id == FGameXXKTravelMoneyRules::ItemId())
        {
            if (Money) continue;
            Money = true;
            Initial.DesktopInventory.WarehouseItems.Add(Id, 7);
            TestTrue(TEXT("money may occupy both containers"), FGameXXKDesktopInventoryRules::Normalize(Initial, &Error));
            const int32 Before = Initial.Inventory.FindRef(Id);
            TestTrue(TEXT("money keeps backpack drop policy"), FGameXXKTrainingChestRules::OpenOne(Initial, Tier, Loot));
            TestEqual(TEXT("stored money unchanged"), Initial.DesktopInventory.WarehouseItems.FindRef(Id), 7);
            TestEqual(TEXT("money drop enters backpack"), Initial.Inventory.FindRef(Id), Before + Pair.Value);
            continue;
        }
        EGameXXKGemType Type; EGameXXKGemQuality Quality;
        const bool IsGem = FGameXXKGemRules::TryParseItemId(Id, Type, Quality);
        const bool IsOrder = Id == FGameXXKHuntRules::OrderId(EGameXXKTrainingDifficulty::Normal);
        bool& Seen = IsGem ? Gem : (IsOrder ? Order : Material);
        if (Seen) continue;
        Seen = true;
        Initial.Inventory.Remove(Id);
        Initial.DesktopInventory.WarehouseItems.Add(Id, 5);
        Initial.DesktopInventory.LockedItemIds.Add(Id);
        if (!TestTrue(TEXT("stored fixture valid"), FGameXXKDesktopInventoryRules::Normalize(Initial, &Error))) return false;
        const auto Key = FGameXXKDesktopInventoryRules::MakeItemEntry(Id);
        const int32 StoredSlot = FGameXXKDesktopInventoryRules::FindEntrySlot(Initial, EGameXXKDesktopItemContainer::Warehouse, Key);
        auto Single = Initial;
        TestTrue(TEXT("stored loot does not block open-one"), FGameXXKTrainingChestRules::OpenOne(Single, Tier, Loot));
        TestEqual(TEXT("loot merged exactly once"), Single.DesktopInventory.WarehouseItems.FindRef(Id), 5 + Pair.Value);
        TestEqual(TEXT("no duplicate backpack stack"), Single.Inventory.FindRef(Id), 0);
        TestTrue(TEXT("lock retained"), Single.DesktopInventory.LockedItemIds.Contains(Id));
        TestEqual(TEXT("warehouse slot retained"), FGameXXKDesktopInventoryRules::FindEntrySlot(Single, EGameXXKDesktopItemContainer::Warehouse, Key), StoredSlot);
        TestTrue(TEXT("partition valid"), FGameXXKDesktopInventoryRules::Validate(Single, &Error));
        auto Batch = Initial;
        TestTrue(TEXT("right-click batch also succeeds"), FGameXXKTrainingChestRules::OpenAll(Batch, Tier, Loot));
        TestEqual(TEXT("batch same quantity"), Batch.DesktopInventory.WarehouseItems.FindRef(Id), 5 + Pair.Value);
        TestEqual(TEXT("one token consumed"), Loot.OpenedCount, 1);

        auto Full = Initial;
        for (const FName FillId : FGameXXKGemRules::GetAllItemIds())
        {
            if (FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Full, EGameXXKDesktopItemContainer::Backpack) == INDEX_NONE) break;
            if (Full.DesktopInventory.WarehouseItems.Contains(FillId)) continue;
            Full.Inventory.FindOrAdd(FillId) = 1;
            if (!FGameXXKDesktopInventoryRules::Normalize(Full, &Error)) { AddError(Error); return false; }
        }
        TestEqual(TEXT("backpack truly full"), FGameXXKDesktopInventoryRules::FindFirstEmptySlot(Full, EGameXXKDesktopItemContainer::Backpack), INDEX_NONE);
        TestTrue(TEXT("existing stored stack needs no new backpack slot"), FGameXXKTrainingChestRules::OpenAll(Full, Tier, Loot));
        TestEqual(TEXT("full-backpack drop merged"), Full.DesktopInventory.WarehouseItems.FindRef(Id), 5 + Pair.Value);

        auto Overflow = Initial;
        Overflow.DesktopInventory.WarehouseItems[Id] = MAX_int32;
        const int32 Ordinal = Overflow.Training.NextChestOpenOrdinal;
        TestFalse(TEXT("stack overflow rejects"), FGameXXKTrainingChestRules::OpenAll(Overflow, Tier, Loot));
        TestEqual(TEXT("overflow is not capacity error"), Loot.Error, EGameXXKTrainingChestOpenError::Overflow);
        TestEqual(TEXT("failed open keeps token"), Overflow.Training.OwnedChestTokens.Num(), Initial.Training.OwnedChestTokens.Num());
        TestEqual(TEXT("failed open keeps random ordinal"), Overflow.Training.NextChestOpenOrdinal, Ordinal);
        TestEqual(TEXT("failed open keeps stack"), Overflow.DesktopInventory.WarehouseItems.FindRef(Id), MAX_int32);
    }
    TestTrue(TEXT("gem branch covered"), Gem); TestTrue(TEXT("material branch covered"), Material);
    TestTrue(TEXT("order branch covered"), Order); TestTrue(TEXT("travel money branch covered"), Money);
    return true;
}
#endif
