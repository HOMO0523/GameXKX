#include "Misc/AutomationTest.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKToolCombineProbability.h"
#include "GameXXKToolSelectionRules.h"
#include "GameXXKTalentRules.h"
#include "GameXXKTravelMoneyRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Image.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameInstance.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKEquipmentCatalog.h"
#include "UI/GameXXKLocalization.h"
#include "Misc/ScopeExit.h"
#include "../UI/GameXXKChestReceipt.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace ToolInteractionTest
{
    UGameXXKMVPSubsystem* Start(FAutomationTestBase& Test)
    {
        auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
        Test.TestTrue(TEXT("fixture starts"), Subsystem->StartGame());
        auto& State = Subsystem->GetMutableRuntimeState();
        State.Screen = EGameXXKScreen::Town;
        State.Training.bTravelActive = false;
        State.Talents.MinimumBackpackCapacity = 200;
        return Subsystem;
    }
    FGameXXKToolInputRef Ref(const FGameXXKRuntimeState& State, FGameXXKDesktopInventoryEntryKey Entry)
    {
        for (const auto Container : {EGameXXKDesktopItemContainer::Backpack, EGameXXKDesktopItemContainer::Warehouse})
        {
            const int32 Slot = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Container, Entry);
            if (Slot != INDEX_NONE) return {Container, Slot, Entry, 1};
        }
        return {};
    }
    FName Equipment(FAutomationTestBase& Test, FGameXXKRuntimeState& State, EGameXXKEquipmentQuality Quality)
    {
        FGameXXKEquipmentCreateRequest Request;
        Request.Set = EGameXXKEquipmentSet::PoJun;
        Request.Quality = Quality;
        Request.ItemLevel = 10;
        Request.bForceSlot = true;
        Request.ForcedSlot = EGameXXKEquipmentSlot::Accessory;
        FName Id;
        Test.TestTrue(TEXT("equipment creates"), FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, Id));
        Test.TestTrue(TEXT("fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State));
        return Id;
    }
    UGameXXKDesktopTrainingWorkbenchWidget* Open(FAutomationTestBase& Test, UGameXXKMVPSubsystem* Subsystem, EGameXXKDesktopToolMode Mode)
    {
        auto* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
        Widget->SetMVPSubsystem(Subsystem);
        Widget->ConstructForTest();
        Test.TestTrue(TEXT("backpack opens"), Widget->OpenBackpack());
        Widget->HandleActionClicked(3);
        Widget->SetToolModeForTest(Mode);
        Widget->TickForTest(0);
        return Widget;
    }
    UButton* Button(UGameXXKDesktopTrainingWorkbenchWidget* Widget, const TCHAR* Name)
    {
        return Widget->WidgetTree ? Cast<UButton>(Widget->WidgetTree->FindWidget(Name)) : nullptr;
    }
    FString Text(UGameXXKDesktopTrainingWorkbenchWidget* Widget, const TCHAR* Name)
    {
        auto* Label = Widget->WidgetTree ? Cast<UTextBlock>(Widget->WidgetTree->FindWidget(Name)) : nullptr;
        return Label ? Label->GetText().ToString() : FString();
    }
    bool Put(FAutomationTestBase& Test, UGameXXKMVPSubsystem* Subsystem, UGameXXKDesktopTrainingWorkbenchWidget* Widget, FName Id, int32 ToolSlot)
    {
        const auto Input = Ref(Subsystem->GetRuntimeState(), FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id));
        return Test.TestTrue(TEXT("pick target"), Widget->PickUpBackpackSlotForTest(Input.SlotIndex))
            && Test.TestTrue(TEXT("place target"), Widget->DropCarriedOnToolSlotForTest(ToolSlot));
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsDefaultOpenTest,
    "GameXXK.ToolsRedesign.DefaultOpenAndModeSlots", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsDefaultOpenTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    FGameXXKTalentProjection Projection;
    TestTrue(TEXT("base projection resolves"), FGameXXKTalentRules::BuildProjection(Subsystem->GetRuntimeState().Talents, Projection));
    TestTrue(TEXT("tools are available without talent purchases"), Projection.bToolsUnlocked);
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Enhance);
    TestNull(TEXT("no unlock overlay"), Widget->WidgetTree->FindWidget(TEXT("ToolsTalentLockedPanel")));
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const auto* Slot = Button(Widget, *FString::Printf(TEXT("ToolInputSlot_%d"), Index));
        TestTrue(TEXT("all nine cells remain visible"), Slot && Slot->GetVisibility() == ESlateVisibility::Visible);
        TestEqual(TEXT("only enhancement target is interactive"), Slot && Slot->GetIsEnabled(), Index == 0);
    }
    const FName Id = Equipment(*this, Subsystem->GetMutableRuntimeState(), EGameXXKEquipmentQuality::Common);
    FGameXXKEquipmentTransactionResult Result;
    TestTrue(TEXT("dismantle works without talents"), Subsystem->ExecuteToolDismantle(
        {Ref(Subsystem->GetRuntimeState(), FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id))}, true, Result));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsMixedGemsTest,
    "GameXXK.ToolsRedesign.NineMixedGemsAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsMixedGemsTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    // Isolate the nine-cell recipe from the independent default-open regression.
    State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
    State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
    const FName Attack = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Common);
    const FName Defense = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Defense, EGameXXKGemQuality::Common);
    State.Inventory.Add(Attack, 4);
    State.DesktopInventory.WarehouseItems.Add(Defense, 7);
    TestTrue(TEXT("mixed gem storage normalizes"), Subsystem->NormalizeDesktopInventoryState());
    TArray<FGameXXKToolInputRef> Inputs;
    TestTrue(TEXT("four plus seven can auto-fill"), Subsystem->BuildToolCombineAutoFill(EGameXXKToolCombineKind::Gem, true, Inputs));
    TestEqual(TEXT("one gem per visible cell"), Inputs.Num(), 9);
    Inputs.Reset();
    for (int32 Index = 0; Index < 9; ++Index)
        Inputs.Add(Ref(State, FGameXXKDesktopInventoryRules::MakeItemEntry(Index < 4 ? Attack : Defense)));
    FGameXXKEquipmentTransactionResult Result;
    if (!TestTrue(TEXT("mixed-type nine-cell recipe succeeds"), Subsystem->ExecuteToolCombine(EGameXXKToolCombineKind::Gem, Inputs, Result))) return false;
    EGameXXKGemType Type;
    EGameXXKGemQuality Quality;
    TestTrue(TEXT("random result is a catalog gem"), FGameXXKGemRules::TryParseItemId(Result.OutputEntryId, Type, Quality));
    TestEqual(TEXT("common recipe produces rare quality"), Quality, EGameXXKGemQuality::Rare);
    TestEqual(TEXT("four attack gems consumed"), State.Inventory.FindRef(Attack), 0);
    TestEqual(TEXT("only five defense gems consumed"), State.DesktopInventory.WarehouseItems.FindRef(Defense), 2);
    const int64 Experience = State.ToolProgress.Experience;
    TestFalse(TEXT("replaying stale inputs cannot double-spend"), Subsystem->ExecuteToolCombine(EGameXXKToolCombineKind::Gem, Inputs, Result));
    TestEqual(TEXT("failed replay awards nothing"), State.ToolProgress.Experience, Experience);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsAutoFillModesTest,
    "GameXXK.ToolsRedesign.AutoFillAndProbabilityPreview", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsAutoFillModesTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
    State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
    for (int32 Index = 0; Index < 9; ++Index) Equipment(*this, State, EGameXXKEquipmentQuality::Common);
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Dismantle);
    TestNotNull(TEXT("dismantle exposes auto-place"), Button(Widget, TEXT("ToolAutoFill")));
    Widget->HandleActionClicked(311);
    Widget->TickForTest(0);
    TestEqual(TEXT("auto-place reserves nine without consuming"), Widget->GetOccupiedToolSlotCountForTest(), 9);
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Combine);
    Widget->TickForTest(0);
    TestTrue(TEXT("full common recipe shows exact chance"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("100%")));
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Enhance);
    Widget->TickForTest(0);
    TestEqual(TEXT("switch releases excess inputs while keeping target"), Widget->GetOccupiedToolSlotCountForTest(), 1);
    TestNull(TEXT("enhancement hides irrelevant crafting level"), Widget->WidgetTree->FindWidget(TEXT("ToolCraftLevelText")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsReforgeComparisonTest,
    "GameXXK.ToolsRedesign.ReforgeComparisonAndRepeat", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsReforgeComparisonTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
    State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
    State.Inventory.Add(UGameXXKMVPRules::ItemRefinementSand(), 100);
    const FName Id = Equipment(*this, State, EGameXXKEquipmentQuality::Rare);
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Reforge);
    if (!Put(*this, Subsystem, Widget, Id, 0)) return false;
    Widget->HandleActionClicked(321); // Second selectable affix.
    Widget->TickForTest(0);
    if (!TestTrue(TEXT("paid preview starts"), Widget->ConfirmToolForTest())) return false;
    Widget->TickForTest(0);
    TestEqual(TEXT("selected affix is used"), State.EquipmentCollection.PendingReforge.AffixIndex, 1);
    TestFalse(TEXT("pending target cannot be duplicated by picking it up"), Widget->PickUpToolSlotForTest(0));
    TestFalse(TEXT("old affix is visible"), Text(Widget, TEXT("ToolReforgeOriginal")).IsEmpty());
    TestFalse(TEXT("new affix is visible"), Text(Widget, TEXT("ToolReforgeCandidate")).IsEmpty());
    TestEqual(TEXT("original option names the selected version"), Text(Widget, TEXT("ToolReforgeOriginalTitle")), FString(TEXT("原属性")));
    TestEqual(TEXT("new option names the selected version"), Text(Widget, TEXT("ToolReforgeCandidateTitle")), FString(TEXT("新属性")));
    TestTrue(TEXT("original attribute is inside the original-choice button"), Button(Widget, TEXT("ToolReforgeKeep"))->GetContent()->IsA<UVerticalBox>());
    TestTrue(TEXT("new attribute is inside the new-choice button"), Button(Widget, TEXT("ToolReforgeAccept"))->GetContent()->IsA<UVerticalBox>());
    const int32 Sand = State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand());
    TestFalse(TEXT("pending preview cannot be paid twice"), Widget->ConfirmToolForTest());
    TestEqual(TEXT("blocked preview preserves material"), State.Inventory.FindRef(UGameXXKMVPRules::ItemRefinementSand()), Sand);
    Button(Widget, TEXT("ToolReforgeKeep"))->OnClicked.Broadcast();
    Widget->TickForTest(0);
    TestFalse(TEXT("keep resolves pending result"), State.EquipmentCollection.PendingReforge.bActive);
    TestEqual(TEXT("target stays for next reforge"), Widget->GetToolSlotItemIdForTest(0), Id);
    TestTrue(TEXT("same target starts another preview"), Widget->ConfirmToolForTest());
    Widget->TickForTest(0);
    const auto Candidate = State.EquipmentCollection.PendingReforge.CandidateAffix;
    Button(Widget, TEXT("ToolReforgeAccept"))->OnClicked.Broadcast();
    Widget->TickForTest(0);
    const auto* Updated = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Id);
    TestTrue(TEXT("clicking new-property option chooses its exact affix"), Updated && Updated->RolledAffixes[1].AffixId == Candidate.AffixId && Updated->RolledAffixes[1].Magnitude == Candidate.Magnitude);
    TestFalse(TEXT("choosing new properties resolves the preview"), State.EquipmentCollection.PendingReforge.bActive);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsSocketInteractionTest,
    "GameXXK.ToolsRedesign.SocketDropAndRemove", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsSocketInteractionTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
    State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
    const FName Id = Equipment(*this, State, EGameXXKEquipmentQuality::Cosmic);
    const FName Gem = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Defense, EGameXXKGemQuality::Rare);
    State.Inventory.Add(Gem, 1);
    Subsystem->NormalizeDesktopInventoryState();
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Socket);
    if (!Put(*this, Subsystem, Widget, Id, 0)) return false;
    Widget->TickForTest(0);
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const auto* Slot = Button(Widget, *FString::Printf(TEXT("ToolInputSlot_%d"), Index));
        TestEqual(TEXT("cosmic target exposes six actual sockets"), Slot && Slot->GetIsEnabled(), Index <= 6);
    }
    const auto GemRef = Ref(State, FGameXXKDesktopInventoryRules::MakeItemEntry(Gem));
    TestTrue(TEXT("pick gem"), Widget->PickUpBackpackSlotForTest(GemRef.SlotIndex));
    TestTrue(TEXT("drop directly into third actual socket"), Widget->DropCarriedOnToolSlotForTest(3));
    Widget->TickForTest(0);
    const auto* Item = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Id);
    TestTrue(TEXT("drop commits without another confirm"), Item && Item->SocketedGems[2].Type == EGameXXKGemType::Defense);
    TestEqual(TEXT("drop consumes one gem"), State.Inventory.FindRef(Gem), 0);
    Widget->HandleActionClicked(303);
    Widget->TickForTest(0);
    Widget->HandleActionClicked(309);
    Widget->TickForTest(0);
    Item = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Id);
    TestTrue(TEXT("selected gem is removed"), Item && Item->SocketedGems[2].IsEmpty());
    TestEqual(TEXT("removed gem returns to backpack"), State.Inventory.FindRef(Gem), 1);
    TestEqual(TEXT("target remains selected"), Widget->GetToolSlotItemIdForTest(0), Id);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsProbabilityTableTest,
    "GameXXK.ToolsRedesign.ExactProbabilityTable", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsProbabilityTableTest::RunTest(const FString& Parameters)
{
    // Design section 14, indexed by input and output quality rank (1..10).
    const int32 Expected[9][10] = {
        {0,1000,0,0,0,0,0,0,0,0}, {0,0,1000,0,0,0,0,0,0,0}, {0,0,0,1000,0,0,0,0,0,0},
        {0,0,0,0,499,499,2,0,0,0}, {0,0,0,0,0,499,499,2,0,0}, {0,0,0,0,0,0,499,499,2,0},
        {0,0,0,0,0,0,659,339,2,0}, {0,0,0,0,0,0,0,659,339,2}, {0,0,0,0,0,0,0,0,750,250}};
    for (int32 Input = 1; Input <= 9; ++Input)
    {
        int32 Actual[10] = {};
        for (int32 Roll = 0; Roll < 1000; ++Roll)
        {
            const int32 Output = FGameXXKToolCombineProbability::Resolve(Input, Roll);
            if (!TestTrue(TEXT("each draw yields a valid quality"), Output >= 1 && Output <= 10)) return false;
            ++Actual[Output - 1];
        }
        for (int32 Output = 1; Output <= 10; ++Output)
            TestEqual(FString::Printf(TEXT("input %d -> output %d exact per-thousand distribution"), Input, Output), Actual[Output - 1], Expected[Input - 1][Output - 1]);
    }
    TestEqual(TEXT("cosmic input cannot combine"), FGameXXKToolCombineProbability::Resolve(10, 0), 0);
    TestEqual(TEXT("out-of-range draw is rejected"), FGameXXKToolCombineProbability::Resolve(4, 1000), 0);
    TestTrue(TEXT("legendary preview shows rare cross-tier result"), FGameXXKToolCombineProbability::Describe(4).ToString().Contains(TEXT("0.2%")));
    TestTrue(TEXT("high-tier preview explicitly shows unchanged quality"), FGameXXKToolCombineProbability::Describe(7).ToString().Contains(TEXT("原品质")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsPartialGemStackTest,
    "GameXXK.ToolsRedesign.PartialStackAndModeCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsPartialGemStackTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    const FName Gem = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Common);
    const auto Key = FGameXXKDesktopInventoryRules::MakeItemEntry(Gem);
    State.Inventory.Add(Gem, 12);
    Subsystem->NormalizeDesktopInventoryState();
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Combine);
    Widget->HandleActionClicked(310);
    Widget->TickForTest(0);
    TestTrue(TEXT("pick twelve-gem stack"), Widget->PickUpBackpackSlotForTest(Ref(State, Key).SlotIndex));
    for (int32 Cell = 0; Cell < 9; ++Cell)
        if (!TestTrue(TEXT("each drop reserves one gem"), Widget->DropCarriedOnToolSlotForTest(Cell))) return false;
    Widget->TickForTest(0);
    TestFalse(TEXT("full recipe releases unused carry"), Widget->IsCarryingItemForTest());
    TestEqual(TEXT("three unreserved gems remain visible"), Widget->GetDesktopAvailableQuantity(EGameXXKDesktopItemContainer::Backpack, Key), 3);
    TestEqual(TEXT("placing never consumes"), State.Inventory.FindRef(Gem), 12);
    TestTrue(TEXT("manual nine-cell combine commits"), Widget->ConfirmToolForTest());
    TestEqual(TEXT("only nine gems consumed"), State.Inventory.FindRef(Gem), 3);
    TestTrue(TEXT("pick remaining gems"), Widget->PickUpBackpackSlotForTest(Ref(State, Key).SlotIndex));
    TestTrue(TEXT("reserve a partial recipe"), Widget->DropCarriedOnToolSlotForTest(0));
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Enhance);
    Widget->TickForTest(0);
    TestFalse(TEXT("switch cancels the remaining carry"), Widget->IsCarryingItemForTest());
    TestEqual(TEXT("switch returns incompatible gem reservation"), Widget->GetOccupiedToolSlotCountForTest(), 0);
    TestEqual(TEXT("all remaining gems reappear"), Widget->GetDesktopAvailableQuantity(EGameXXKDesktopItemContainer::Backpack, Key), 3);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsSocketFullBackpackTest,
    "GameXXK.ToolsRedesign.RemoveGemFullBackpackAtomic", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsSocketFullBackpackTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    State.Talents.MinimumBackpackCapacity = 20;
    const FName Id = Equipment(*this, State, EGameXXKEquipmentQuality::Common);
    const FName Returned = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Cosmic);
    auto* Item = State.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& I) { return I.InstanceId == Id; });
    Item->SocketedGems[0] = {EGameXXKGemType::Attack, EGameXXKGemQuality::Cosmic};
    for (const FName Gem : FGameXXKGemRules::GetAllItemIds())
    {
        if (Gem == Returned) continue;
        if (FGameXXKDesktopInventoryRules::GetOccupiedSlotCount(State, EGameXXKDesktopItemContainer::Backpack) == 20) break;
        State.Inventory.FindOrAdd(Gem) = 1;
        if (!TestTrue(TEXT("fill legal backpack capacity"), Subsystem->NormalizeDesktopInventoryState())) return false;
    }
    FGameXXKEquipmentTransactionResult Result;
    const auto Input = Ref(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id));
    const int32 Ordinal = State.EquipmentCollection.NextInstanceOrdinal;
    TestFalse(TEXT("full backpack rejects removal"), Subsystem->ExecuteToolRemoveSocketGem(Input, 0, Result));
    TestEqual(TEXT("removal reports space failure"), Result.Error, EGameXXKEquipmentTransactionError::InventoryFull);
    Item = State.EquipmentCollection.EquipmentInstances.FindByPredicate([Id](const auto& I) { return I.InstanceId == Id; });
    TestTrue(TEXT("failed removal retains socket contents"), Item && !Item->SocketedGems[0].IsEmpty());
    TestEqual(TEXT("failed removal creates no loose gem"), State.Inventory.FindRef(Returned), 0);
    TestEqual(TEXT("failed removal preserves random ordinal"), State.EquipmentCollection.NextInstanceOrdinal, Ordinal);
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsAnchoredFillTest,
    "GameXXK.ToolsRedesign.AnchoredQualityAndPartialAutoFill", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsAnchoredFillTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    for (const auto Id : State.EquipmentCollection.WarehouseInstanceIds)
        State.DesktopInventory.LockedEquipmentInstanceIds.Add(Id);
    TArray<FName> Commons;
    TArray<FName> Rares;
    for (int32 Index = 0; Index < 8; ++Index) Commons.Add(Equipment(*this, State, EGameXXKEquipmentQuality::Common));
    for (int32 Index = 0; Index < 9; ++Index) Rares.Add(Equipment(*this, State, EGameXXKEquipmentQuality::Rare));
    TArray<FGameXXKToolInputRef> Inputs;
    TestTrue(TEXT("skip an insufficient lower tier"), Subsystem->BuildToolCombineAutoFill(EGameXXKToolCombineKind::Equipment, true, Inputs));
    TestEqual(TEXT("empty selection finds nine"), Inputs.Num(), 9);
    for (const auto& Input : Inputs)
        TestEqual(TEXT("empty selection chooses the lowest complete tier"), FGameXXKToolSelectionRules::GetEntryQualityRank(State, Input.ExpectedEntry), 2);
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Combine);
    if (!Put(*this, Subsystem, Widget, Commons[0], 4)) return false;
    TestTrue(TEXT("pick another quality"), Widget->PickUpBackpackSlotForTest(Ref(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Rares[0])).SlotIndex));
    TestFalse(TEXT("subsequent cells reject a different quality"), Widget->DropCarriedOnToolSlotForTest(0));
    Widget->CancelCarriedItemForTest();
    Widget->HandleActionClicked(311);
    Widget->TickForTest(0);
    TestEqual(TEXT("anchor stays in its original cell"), Widget->GetToolSlotItemIdForTest(4), Commons[0]);
    TestEqual(TEXT("anchored auto-place fills available eight without escalating tier"), Widget->GetOccupiedToolSlotCountForTest(), 8);
    TestFalse(TEXT("incomplete recipe disables combine"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    TestTrue(TEXT("incomplete recipe shows count"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("8 / 9")));
    for (int32 Cell = 0; Cell < 9; ++Cell)
    {
        const FName Id = Widget->GetToolSlotItemIdForTest(Cell);
        if (!Id.IsNone()) TestTrue(TEXT("only selected tier is filled"), Commons.Contains(Id));
    }
    Commons.Add(Equipment(*this, State, EGameXXKEquipmentQuality::Common));
    Widget->HandleActionClicked(311);
    Widget->TickForTest(0);
    TestEqual(TEXT("later fill completes only the missing cell"), Widget->GetOccupiedToolSlotCountForTest(), 9);
    TestEqual(TEXT("second fill still retains the anchor"), Widget->GetToolSlotItemIdForTest(4), Commons[0]);
    TestTrue(TEXT("complete recipe enables combine"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsZeroMaterialsTest,
    "GameXXK.ToolsRedesign.ZeroMaterialsDisableActions", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsZeroMaterialsTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    State.Inventory.Remove(UGameXXKMVPRules::ItemEnhancementStone());
    State.Inventory.Remove(UGameXXKMVPRules::ItemRefinementSand());
    const FName Id = Equipment(*this, State, EGameXXKEquipmentQuality::Common);
    auto* Widget = Open(*this, Subsystem, EGameXXKDesktopToolMode::Enhance);
    TestTrue(TEXT("empty enhancement panel still shows zero stones"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("强化石 0")));
    TestFalse(TEXT("empty enhancement panel cannot execute"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Reforge);
    Widget->TickForTest(0);
    TestTrue(TEXT("empty reforge panel still shows zero sand"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("洗炼砂 0")));
    TestFalse(TEXT("empty reforge panel cannot execute"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Enhance);
    Widget->TickForTest(0);
    if (!Put(*this, Subsystem, Widget, Id, 0)) return false;
    Widget->TickForTest(0);
    TestTrue(TEXT("zero stone count is visible"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("强化石 0 /")));
    TestFalse(TEXT("zero stones disable enhancement"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    Widget->SetToolModeForTest(EGameXXKDesktopToolMode::Reforge);
    Widget->TickForTest(0);
    TestTrue(TEXT("zero sand count is visible"), Text(Widget, TEXT("ToolRecipePreview")).Contains(TEXT("洗炼砂 0 /")));
    TestFalse(TEXT("zero sand disables reforge"), Button(Widget, TEXT("ToolConfirmButton"))->GetIsEnabled());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolsBestPartialFallbackTest,
    "GameXXK.ToolsRedesign.BestPartialGroupFallback", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKToolsBestPartialFallbackTest::RunTest(const FString& Parameters)
{
    using namespace ToolInteractionTest;
    auto* Subsystem = Start(*this);
    auto& State = Subsystem->GetMutableRuntimeState();
    for (const FName Id : State.EquipmentCollection.WarehouseInstanceIds) State.DesktopInventory.LockedEquipmentInstanceIds.Add(Id);
    for (int32 I = 0; I < 3; ++I) Equipment(*this, State, EGameXXKEquipmentQuality::Common);
    for (int32 I = 0; I < 8; ++I) Equipment(*this, State, EGameXXKEquipmentQuality::Rare);
    for (int32 I = 0; I < 5; ++I) Equipment(*this, State, FGameXXKEquipmentQualityRules::EquipmentQualityFromRank(3));
    TArray<FGameXXKToolInputRef> Inputs;
    auto CheckSelection = [&](int32 Count, int32 Rank)
    {
        TestEqual(TEXT("best available group count"), Inputs.Num(), Count);
        for (const auto& Ref : Inputs) TestEqual(TEXT("every selected input has the chosen quality"), FGameXXKToolSelectionRules::GetEntryQualityRank(State, Ref.ExpectedEntry), Rank);
    };
    TestTrue(TEXT("dismantle fills best partial group"), FGameXXKEquipmentToolRules::BuildDismantleAutoFill(State, true, Inputs));
    CheckSelection(8, 2);
    TestTrue(TEXT("combine also fills best partial group"), FGameXXKEquipmentToolRules::BuildCombineAutoFill(State, EGameXXKToolCombineKind::Equipment, true, Inputs));
    CheckSelection(8, 2);
    for (int32 I = 0; I < 5; ++I) Equipment(*this, State, EGameXXKEquipmentQuality::Common);
    TestTrue(TEXT("equal partial counts choose lower quality"), FGameXXKEquipmentToolRules::BuildDismantleAutoFill(State, true, Inputs));
    CheckSelection(8, 1);
    for (int32 I = 0; I < 9; ++I) Equipment(*this, State, EGameXXKEquipmentQuality::Legendary);
    TestTrue(TEXT("complete higher group takes precedence over partial groups"), FGameXXKEquipmentToolRules::BuildCombineAutoFill(State, EGameXXKToolCombineKind::Equipment, true, Inputs));
    CheckSelection(9, 4);
    State.Inventory.Add(FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Common), 5);
    State.Inventory.Add(FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Rare), 7);
    State.Inventory.Add(FGameXXKGemRules::MakeItemId(EGameXXKGemType::Defense, EGameXXKGemQuality::Rare), 1);
    State.Inventory.Add(FGameXXKGemRules::MakeItemId(EGameXXKGemType::MaxHealth, FGameXXKGemRules::QualityFromRank(3)), 8);
    TestTrue(TEXT("gem fixture normalizes"), Subsystem->NormalizeDesktopInventoryState());
    TestTrue(TEXT("gem counts aggregate across types and break ties toward lower quality"), FGameXXKEquipmentToolRules::BuildCombineAutoFill(State, EGameXXKToolCombineKind::Gem, true, Inputs));
    CheckSelection(8, 2);
    for (const auto& Ref : Inputs) TestEqual(TEXT("each partial gem cell reserves one"), Ref.Quantity, 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKToolSlotQualityContinuityTest,
    "GameXXK.ToolsRedesign.EquipmentSlotQualityContinuity",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKToolSlotQualityContinuityTest::RunTest(const FString&)
{
    using namespace ToolInteractionTest;
    auto* Subsystem=Start(*this);
    const FName Rare=Equipment(*this,Subsystem->GetMutableRuntimeState(),EGameXXKEquipmentQuality::Rare);
    const FName Treasure=Equipment(*this,Subsystem->GetMutableRuntimeState(),EGameXXKEquipmentQuality::Treasure);
    auto* Widget=Open(*this,Subsystem,EGameXXKDesktopToolMode::Dismantle);
    const auto Check=[&](int32 Index)
    {
        const FName Id=Widget->GetToolSlotItemIdForTest(Index);
        const auto* Item=FGameXXKEquipmentRules::FindInstance(Subsystem->GetRuntimeState().EquipmentCollection,Id);
        if(!Item)return;
        for(const TCHAR* Layer:{TEXT("Surface"),TEXT("Frame")})
        {
            auto* Image=Cast<UImage>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("ToolInputSlot_%dEquipment%s"),Index,Layer)));
            if(!TestNotNull(TEXT("Tool equipment has the shared quality layers"),Image))return;
            const auto* Material=Image?Cast<UMaterialInstanceDynamic>(Image->GetBrush().GetResourceObject()):nullptr;
            if(!TestNotNull(TEXT("Quality layer has its material"),Material))return;
            TestEqual(TEXT("Tool quality matches the actual equipment instance"),Material->GetName(),
                FString::Printf(TEXT("EquipmentQuality%d%s"),static_cast<int32>(Item->Quality),Layer));
            TestEqual(TEXT("Placed equipment visibly retains its quality"),Image->GetVisibility(),ESlateVisibility::HitTestInvisible);
        }
    };
    for(const FName Id:{Rare,Treasure})
    {
        if(!Put(*this,Subsystem,Widget,Id,0))return false;
        for(const auto Mode:{EGameXXKDesktopToolMode::Combine,EGameXXKDesktopToolMode::Enhance,
            EGameXXKDesktopToolMode::Reforge,EGameXXKDesktopToolMode::Socket,EGameXXKDesktopToolMode::Dismantle})
        {
            TestTrue(TEXT("The mode can change with the same equipment"),Widget->SetToolModeForTest(Mode));
            Widget->TickForTest(0);Check(0);
        }
        TestTrue(TEXT("Right-click returns the equipment reservation"),Widget->HandleActionRightClicked(300));
        Widget->TickForTest(0);
        TestTrue(TEXT("The cell becomes empty"),Widget->GetToolSlotItemIdForTest(0).IsNone());
        TestNull(TEXT("An empty dismantle cell retains no quality content"),Button(Widget,TEXT("ToolInputSlot_0"))->GetContent());
    }
    Widget->HandleActionClicked(311);Widget->TickForTest(0);
    TestTrue(TEXT("Auto-fill places equipment"),Widget->GetOccupiedToolSlotCountForTest()>0);
    for(int32 Index=0;Index<9;++Index)Check(Index);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKChestMaterialsToToolsTest,
    "GameXXK.ToolsRedesign.ChestMaterialsInStorage",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKChestMaterialsToToolsTest::RunTest(const FString&)
{
    using namespace ToolInteractionTest;
    const FString Language=GameXXKLocalization::GetLanguage();ON_SCOPE_EXIT{GameXXKLocalization::SetLanguage(Language,false);};
    for(const TCHAR* Culture:{TEXT("zh-Hans"),TEXT("en")})for(auto Tier:{EGameXXKTrainingRewardTier::NormalChest,EGameXXKTrainingRewardTier::AdvancedChest,EGameXXKTrainingRewardTier::HuntChest})for(bool Stone:{true,false})
    {
        GameXXKLocalization::SetLanguage(Culture,false);
        auto* MVP=Start(*this);auto& State=MVP->GetMutableRuntimeState();
        State.Training.ActiveTravelEncounterIndex=INDEX_NONE;
        MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
        const FName EquipmentId=Equipment(*this,State,EGameXXKEquipmentQuality::Rare);
        const FName Id=Stone?UGameXXKMVPRules::ItemEnhancementStone():UGameXXKMVPRules::ItemRefinementSand();
        State.Inventory.Remove(Id);State.DesktopInventory.WarehouseItems.Add(Id,20);
        State.Training.OwnedChestTokens.Reset();FString Error;
        TestTrue(TEXT("stored material fixture normalizes"),FGameXXKDesktopInventoryRules::Normalize(State,&Error));
        TestTrue(TEXT("test chest added"),FGameXXKTrainingRules::AppendChestToken(State.Training,Tier,TEXT("Training.Normal.1-1"),5,&Error));
        const auto Base=State;int32 Found=0,DropQuantity=0;
        for(int32 Seed=1;Seed<1024;++Seed)
        {
            auto Preview=Base;Preview.Training.ChallengeRewardSeed=Seed;FGameXXKTrainingChestOpenResult Loot;
            if(FGameXXKTrainingChestRules::OpenOne(Preview,Tier,Loot)&&Loot.ItemDeltas.FindRef(Id)>0){Found=Seed;DropQuantity=Loot.ItemDeltas.FindRef(Id);break;}
        }
        if(!TestTrue(TEXT("both materials are reachable in every chest tier"),Found>0))continue;
        State.Training.ChallengeRewardSeed=Found;FGameXXKTrainingChestOpenResult Loot;
        const bool Opened=MVP->OpenOneTrainingChest(Tier,Loot);
        if(!TestTrue(TEXT("real opening transaction commits: ")+MVP->GetLastSaveLoadError().ToString(),Opened))continue;
        TestEqual(TEXT("drop keeps material in its existing warehouse stack"),State.DesktopInventory.WarehouseItems.FindRef(Id),20+DropQuantity);
        TestEqual(TEXT("no false second backpack stack"),State.Inventory.FindRef(Id),0);
        const FString Receipt=GameXXKChestReceipt::Build(State,Loot).ToString();
        TestTrue(TEXT("receipt identifies the real destination"),Receipt.Contains(GameXXKLocalization::IsEnglish()?TEXT("in Storage"):TEXT("已入仓库")));
        TestTrue(TEXT("receipt shows the actual dropped quantity"),Receipt.Contains(FString::FromInt(DropQuantity)));
        if(GameXXKLocalization::IsEnglish())for(TCHAR Character:Receipt)if(Character>=0x3400&&Character<=0x9fff){AddError(TEXT("Receipt remains Chinese: ")+Receipt);break;}
        auto* Widget=Open(*this,MVP,Stone?EGameXXKDesktopToolMode::Enhance:EGameXXKDesktopToolMode::Reforge);
        Widget->OpenWorkbench();Widget->OpenBackpack();Widget->HandleActionClicked(3);
        Widget->SetToolModeForTest(Stone?EGameXXKDesktopToolMode::Enhance:EGameXXKDesktopToolMode::Reforge);Widget->TickForTest(0);
        if(!Put(*this,MVP,Widget,EquipmentId,0))continue;
        Widget->TickForTest(0);
        TestTrue(TEXT("tool displays warehouse material balance"),Text(Widget,TEXT("ToolRecipePreview")).Contains(FString::Printf(TEXT("%d /"),20+DropQuantity)));
        TestTrue(TEXT("tool enables with stored materials"),Button(Widget,TEXT("ToolConfirmButton"))->GetIsEnabled());
        State.DesktopInventory.WarehouseItems[Id]+=7;Widget->TickForTest(1.1f);
        TestTrue(TEXT("stored quantity refreshes without adding an occupied slot"),Text(Widget,TEXT("ToolRecipePreview")).Contains(FString::Printf(TEXT("%d /"),27+DropQuantity)));
        State.DesktopInventory.WarehouseItems[Id]-=7;Widget->TickForTest(1.1f);
        const auto Before=State;
        MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return false;}));
        TestFalse(TEXT("failed persistence rejects tool use"),Widget->ConfirmToolForTest());
        TestTrue(TEXT("failed persistence preserves material, item and ordinal"),FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Before,&State,PPF_None));
        MVP->SetSaveSlotWriteDelegateForTest(FGameXXKSaveSlotWriteDelegate::CreateLambda([](USaveGame*,const FString&,int32){return true;}));
        TestTrue(TEXT("tool now consumes the material successfully"),Widget->ConfirmToolForTest());
        const int32 Cost=Stone?FGameXXKEquipmentCatalog::GetEnhancementStoneCost(0):FGameXXKEquipmentCatalog::GetReforgeSandCost(EGameXXKEquipmentQuality::Rare);
        TestEqual(TEXT("only the real source stack is charged"),State.DesktopInventory.WarehouseItems.FindRef(Id),20+DropQuantity-Cost);
        TestEqual(TEXT("no backpack material is invented"),State.Inventory.FindRef(Id),0);
        TestTrue(TEXT("post-tool physical inventory remains valid"),FGameXXKDesktopInventoryRules::Validate(State,&Error));
        auto ExactCost=Before;ExactCost.DesktopInventory.WarehouseItems[Id]=Cost;FGameXXKEquipmentTransactionResult ExactResult;
        const auto ExactInput=Ref(ExactCost,FGameXXKDesktopInventoryRules::MakeEquipmentEntry(EquipmentId));
        TestTrue(TEXT("the last stored material can be spent"),Stone?FGameXXKEquipmentToolRules::Enhance(ExactCost,ExactInput,ExactResult):FGameXXKEquipmentToolRules::BeginReforge(ExactCost,ExactInput,0,ExactResult));
        TestEqual(TEXT("empty material stack is not resurrected by legacy mirrors"),ExactCost.Inventory.FindRef(Id)+ExactCost.DesktopInventory.WarehouseItems.FindRef(Id),0);
        if(Stone)TestEqual(TEXT("enhancement actually changes the item"),FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection,EquipmentId)->EnhancementLevel,1);
        else TestTrue(TEXT("reforge really creates a paid preview"),State.EquipmentCollection.PendingReforge.bActive);
        const FName Scrap=Equipment(*this,State,EGameXXKEquipmentQuality::Common);FGameXXKEquipmentTransactionResult Dismantled;
        TestTrue(TEXT("dismantle also works when the output material is stored"),MVP->ExecuteToolDismantle({Ref(State,FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Scrap))},true,Dismantled));
        TestEqual(TEXT("dismantled material retains its home"),State.DesktopInventory.WarehouseItems.FindRef(Id),20+DropQuantity-Cost+1);
        TestEqual(TEXT("dismantle does not create a duplicate partition"),State.Inventory.FindRef(Id),0);
        MVP->ResetSaveSlotWriteDelegateForTest();
    }
    return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKWarehousePlainQuantityTest,
    "GameXXK.ToolsRedesign.WarehousePlainQuantityRefresh",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKWarehousePlainQuantityTest::RunTest(const FString&)
{
    using namespace ToolInteractionTest;
    auto* Subsystem=Start(*this);auto& State=Subsystem->GetMutableRuntimeState();
    const FName Money=FGameXXKTravelMoneyRules::ItemId();State.DesktopInventory.WarehouseItems.Add(Money,20);
    if(!TestTrue(TEXT("warehouse stack fixture is valid"),FGameXXKDesktopInventoryRules::Normalize(State)))return false;
    const int32 Index=FGameXXKDesktopInventoryRules::FindEntrySlot(State,EGameXXKDesktopItemContainer::Warehouse,FGameXXKDesktopInventoryRules::MakeItemEntry(Money));
    auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();Widget->SetMVPSubsystem(Subsystem);Widget->ConstructForTest();
    Widget->OpenWorkbench();Widget->OpenBackpack();Widget->HandleActionClicked(0);Widget->TickForTest(0);
    TestTrue(TEXT("The real warehouse entrance allows live presentation refresh"),Widget->GetVisibility()!=ESlateVisibility::Collapsed&&Widget->GetVisibility()!=ESlateVisibility::Hidden);
    auto* Count=Cast<UTextBlock>(Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("WarehouseStackCount_%d"),Index)));
    if(!TestNotNull(TEXT("warehouse money gets a count label"),Count))return false;
    TestEqual(TEXT("quantity has no x prefix"),Count->GetText().ToString(),FString(TEXT("20")));
    TestTrue(TEXT("quantity uses Jianghu with a readable outline"),Count->GetFont().FontObject&&Count->GetFont().FontObject->GetPathName().Contains(TEXT("JiangHuGuFeng"))&&Count->GetFont().OutlineSettings.OutlineSize>0);
    const int32 Builds=Widget->GetProgrammaticLayoutBuildCountForTest();
    State.DesktopInventory.WarehouseItems[Money]=30;Widget->TickForTest(1.1f);
    TestEqual(TEXT("same-slot quantity refreshes without occupancy changes"),Count->GetText().ToString(),FString(TEXT("30")));
    TestEqual(TEXT("count refresh does not rebuild the whole panel"),Widget->GetProgrammaticLayoutBuildCountForTest(),Builds);
    return true;
}
#endif
