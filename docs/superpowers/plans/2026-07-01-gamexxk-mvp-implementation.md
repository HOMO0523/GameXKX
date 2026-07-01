# GameXXK MVP Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build the first playable 《霞客行》 MVP loop from main menu to world map, town, quest, follower, node dungeon, battle, trading, retry, Boss clear, and region unlock.

**Architecture:** C++ owns deterministic gameplay rules and SaveGame state. UMG, Blueprint, Paper2D, and PaperZD own presentation, level assembly, animation assignment, and UI layout. TestMap UE 5.4 content is migrated into a prototype folder and used as visual/UI reference, while GameXXK runtime state remains owned by new GameXXK systems.

**Tech Stack:** UE 5.8, C++ runtime module, Unreal Automation Tests, UMG, Paper2D, PaperZD, UE MCP automation, GPT image generation for missing sprites.

---

## Scope Split

The confirmed MVP spec covers several subsystems. Implement it as these sequential slices so each slice can compile, test, and commit independently:

1. Pure C++ MVP rules: progression, quest, inventory, trade, dungeon, battle, SaveGame shape.
2. TestMap prototype content migration.
3. Menu, world map, town movement, interaction, quest, follower, and merchant shell.
4. Dungeon map UI binding and battle UI binding.
5. Character sprites, PaperZD setup, simple town blockout, and acceptance demo wiring.
6. Defense page updates and final verification.

## File Structure

Create or modify these files:

- `D:/UE5 demo/GameXXK/Source/GameXXK/GameXXK.Build.cs`
  Add UI, Paper2D, and PaperZD dependencies when C++ classes need them.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpTypes.h`
  Shared enums and structs for regions, quests, items, dungeon nodes, battle actors, actions, and rewards.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKProgressionRules.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKProgressionRules.cpp`
  Pure state transitions for world map unlocks and quest state.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKInventoryRules.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKInventoryRules.cpp`
  Inventory, equipment, and trading rules.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKDungeonRules.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKDungeonRules.cpp`
  Fixed five-node dungeon map and node completion rules.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKBattleRules.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKBattleRules.cpp`
  Turn order, BP, weakness, break, actions, victory, failure, XP and gold rewards.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKSaveGame.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKSaveGame.cpp`
  Minimal persisted state.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpSubsystem.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKMvpSubsystem.cpp`
  GameInstance subsystem that wraps pure rules for UI and level code.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Interaction/GameXXKInteractable.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Interaction/GameXXKInteractionComponent.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Interaction/GameXXKInteractionComponent.cpp`
  `F` interaction detection and reusable interactable interface.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Town/GameXXKTownPlayerPawn.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Town/GameXXKTownPlayerPawn.cpp`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Town/GameXXKTownNpcActor.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
  Town movement, right-click target hooks, NPC roles, follower behavior.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/*.h`
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/*.cpp`
  Thin C++ widget bases for main menu, world map, dialogue, inventory, trade, character panel, dungeon map, and battle.
- `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/*.cpp`
  Automation tests for each rule slice.
- `D:/UE5 demo/GameXXK/Content/Prototype/TestMap/1Game/...`
  Migrated TestMap prototype content.
- `D:/UE5 demo/GameXXK/Content/GameXXK/...`
  GameXXK-owned Blueprints, widgets, maps, sprites, and data assets.
- `D:/UE5 demo/GameXXK/docs/defense/index.html`
  Add dated implementation progress entries.

## Shared Commands

Use these commands from `D:/UE5 demo/GameXXK`.

Build:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
```

Run MVP automation tests:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -ExecCmds='Automation RunTests GameXXK.MVP;Quit' -TestExit='Automation Test Queue Empty' -unattended -nop4 -nosplash -log
```

Run bridge-driven PIE smoke test after editor is available:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 5
```

If build reports Live Coding is active, close the running editor or disable Live Coding before retrying.

## Task 1: Core Progression And Quest Rules

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpTypes.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKProgressionRules.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKProgressionRules.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKProgressionRulesTests.cpp`

- [ ] **Step 1: Write the failing progression test**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Mvp/GameXXKMvpTypes.h"
#include "Mvp/GameXXKProgressionRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKProgressionRulesTest,
    "GameXXK.MVP.Progression.QuestGateAndUnlock",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKProgressionRulesTest::RunTest(const FString& Parameters)
{
    FGameXXKProgressionState State = FGameXXKProgressionRules::MakeNewGame();

    TestTrue(TEXT("青山驿站 starts unlocked"), FGameXXKProgressionRules::IsRegionUnlocked(State, EGameXXKRegionId::QingshanInn));
    TestFalse(TEXT("潭江渡口 starts locked"), FGameXXKProgressionRules::IsRegionUnlocked(State, EGameXXKRegionId::TanjiangFerry));
    TestFalse(TEXT("Exit blocks dungeon before quest"), FGameXXKProgressionRules::CanEnterDungeon(State));

    FGameXXKProgressionRules::AcceptQuest(State);
    TestEqual(TEXT("Quest is accepted"), State.QuestState, EGameXXKQuestState::Accepted);
    TestTrue(TEXT("Accepted quest enables dungeon entry"), FGameXXKProgressionRules::CanEnterDungeon(State));
    TestTrue(TEXT("Follower becomes active"), State.bFollowerActive);

    FGameXXKProgressionRules::FailDungeon(State);
    TestEqual(TEXT("Failure keeps quest accepted"), State.QuestState, EGameXXKQuestState::Accepted);
    TestTrue(TEXT("Failure keeps follower active"), State.bFollowerActive);

    FGameXXKProgressionRules::CompleteBoss(State);
    TestEqual(TEXT("Boss completes quest"), State.QuestState, EGameXXKQuestState::Completed);
    TestFalse(TEXT("Completion dismisses follower"), State.bFollowerActive);
    TestTrue(TEXT("Boss unlocks 潭江渡口"), FGameXXKProgressionRules::IsRegionUnlocked(State, EGameXXKRegionId::TanjiangFerry));
    return true;
}

#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run the build command.

Expected: FAIL because `Mvp/GameXXKMvpTypes.h` and `Mvp/GameXXKProgressionRules.h` do not exist.

- [ ] **Step 3: Add minimal progression types**

Create `GameXXKMvpTypes.h` with these definitions:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameXXKMvpTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKRegionId : uint8
{
    QingshanInn,
    HuangshanPass,
    TanjiangFerry
};

UENUM(BlueprintType)
enum class EGameXXKQuestState : uint8
{
    NotAccepted,
    Accepted,
    Completed
};

USTRUCT(BlueprintType)
struct FGameXXKProgressionState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TSet<EGameXXKRegionId> UnlockedRegions;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKQuestState QuestState = EGameXXKQuestState::NotAccepted;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bFollowerActive = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 PlayerLevel = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentXP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Gold = 0;
};
```

- [ ] **Step 4: Add minimal progression rules**

Create `GameXXKProgressionRules.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Mvp/GameXXKMvpTypes.h"

class GAMEXXK_API FGameXXKProgressionRules
{
public:
    static FGameXXKProgressionState MakeNewGame();
    static bool IsRegionUnlocked(const FGameXXKProgressionState& State, EGameXXKRegionId RegionId);
    static bool CanEnterDungeon(const FGameXXKProgressionState& State);
    static void AcceptQuest(FGameXXKProgressionState& State);
    static void FailDungeon(FGameXXKProgressionState& State);
    static void CompleteBoss(FGameXXKProgressionState& State);
};
```

Create `GameXXKProgressionRules.cpp`:

```cpp
#include "Mvp/GameXXKProgressionRules.h"

FGameXXKProgressionState FGameXXKProgressionRules::MakeNewGame()
{
    FGameXXKProgressionState State;
    State.UnlockedRegions.Add(EGameXXKRegionId::QingshanInn);
    State.QuestState = EGameXXKQuestState::NotAccepted;
    State.bFollowerActive = false;
    State.PlayerLevel = 1;
    State.CurrentXP = 0;
    State.Gold = 0;
    return State;
}

bool FGameXXKProgressionRules::IsRegionUnlocked(const FGameXXKProgressionState& State, EGameXXKRegionId RegionId)
{
    return State.UnlockedRegions.Contains(RegionId);
}

bool FGameXXKProgressionRules::CanEnterDungeon(const FGameXXKProgressionState& State)
{
    return State.QuestState == EGameXXKQuestState::Accepted;
}

void FGameXXKProgressionRules::AcceptQuest(FGameXXKProgressionState& State)
{
    if (State.QuestState == EGameXXKQuestState::NotAccepted)
    {
        State.QuestState = EGameXXKQuestState::Accepted;
        State.bFollowerActive = true;
    }
}

void FGameXXKProgressionRules::FailDungeon(FGameXXKProgressionState& State)
{
    if (State.QuestState == EGameXXKQuestState::Accepted)
    {
        State.bFollowerActive = true;
    }
}

void FGameXXKProgressionRules::CompleteBoss(FGameXXKProgressionState& State)
{
    State.QuestState = EGameXXKQuestState::Completed;
    State.bFollowerActive = false;
    State.UnlockedRegions.Add(EGameXXKRegionId::TanjiangFerry);
}
```

- [ ] **Step 5: Run test to verify it passes**

Run the build command, then run the automation command.

Expected: build succeeds and `GameXXK.MVP.Progression.QuestGateAndUnlock` passes.

- [ ] **Step 6: Commit**

```powershell
git add Source\GameXXK\Public\Mvp Source\GameXXK\Private\Mvp Source\GameXXK\Private\Tests
git commit -m "feat: add MVP progression rules"
```

## Task 2: Inventory, Equipment, And Trading Rules

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpTypes.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKInventoryRules.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKInventoryRules.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKInventoryRulesTests.cpp`

- [ ] **Step 1: Write failing inventory and trading tests**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Mvp/GameXXKInventoryRules.h"
#include "Mvp/GameXXKMvpTypes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKInventoryRulesTest,
    "GameXXK.MVP.Inventory.TradeAndDungeonConsumables",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInventoryRulesTest::RunTest(const FString& Parameters)
{
    FGameXXKInventory Player;
    Player.Gold = 100;
    FGameXXKItemDefinition Potion;
    Potion.ItemId = TEXT("healing_pill");
    Potion.DisplayName = FText::FromString(TEXT("回血药"));
    Potion.ItemType = EGameXXKItemType::Consumable;
    Potion.BuyPrice = 25;
    Potion.SellPrice = 10;

    TestTrue(TEXT("Buying potion succeeds"), FGameXXKInventoryRules::BuyItem(Player, Potion, 2));
    TestEqual(TEXT("Gold reduced"), Player.Gold, 50);
    TestEqual(TEXT("Potion stack added"), FGameXXKInventoryRules::GetItemCount(Player, TEXT("healing_pill")), 2);

    FGameXXKInventory DungeonSnapshot = FGameXXKInventoryRules::MakeDungeonSnapshot(Player);
    TestTrue(TEXT("Can consume one potion in dungeon"), FGameXXKInventoryRules::ConsumeItem(DungeonSnapshot, TEXT("healing_pill"), 1));
    FGameXXKInventoryRules::CommitDungeonConsumables(Player, DungeonSnapshot);
    TestEqual(TEXT("Used consumable is deducted from main inventory"), FGameXXKInventoryRules::GetItemCount(Player, TEXT("healing_pill")), 1);

    TestTrue(TEXT("Selling one potion succeeds"), FGameXXKInventoryRules::SellItem(Player, Potion, 1));
    TestEqual(TEXT("Sell price awarded"), Player.Gold, 60);
    TestEqual(TEXT("Potion removed"), FGameXXKInventoryRules::GetItemCount(Player, TEXT("healing_pill")), 0);
    return true;
}

#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run the build command.

Expected: FAIL because inventory types and rules do not exist.

- [ ] **Step 3: Add inventory types**

Append these types to `GameXXKMvpTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKItemType : uint8
{
    Consumable,
    Weapon,
    Armor
};

USTRUCT(BlueprintType)
struct FGameXXKItemDefinition
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKItemType ItemType = EGameXXKItemType::Consumable;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BuyPrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SellPrice = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 AttackBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DefenseBonus = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 HpBonus = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKItemStack
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ItemId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Quantity = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKInventory
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Gold = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGameXXKItemStack> Items;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EquippedWeaponId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName EquippedArmorId;
};
```

- [ ] **Step 4: Add inventory rules**

Create `GameXXKInventoryRules.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Mvp/GameXXKMvpTypes.h"

class GAMEXXK_API FGameXXKInventoryRules
{
public:
    static int32 GetItemCount(const FGameXXKInventory& Inventory, FName ItemId);
    static void AddItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity);
    static bool RemoveItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity);
    static bool BuyItem(FGameXXKInventory& PlayerInventory, const FGameXXKItemDefinition& Definition, int32 Quantity);
    static bool SellItem(FGameXXKInventory& PlayerInventory, const FGameXXKItemDefinition& Definition, int32 Quantity);
    static bool ConsumeItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity);
    static FGameXXKInventory MakeDungeonSnapshot(const FGameXXKInventory& Inventory);
    static void CommitDungeonConsumables(FGameXXKInventory& MainInventory, const FGameXXKInventory& DungeonSnapshot);
};
```

Create `GameXXKInventoryRules.cpp`:

```cpp
#include "Mvp/GameXXKInventoryRules.h"

namespace
{
FGameXXKItemStack* FindStack(FGameXXKInventory& Inventory, FName ItemId)
{
    return Inventory.Items.FindByPredicate([ItemId](const FGameXXKItemStack& Stack)
    {
        return Stack.ItemId == ItemId;
    });
}

const FGameXXKItemStack* FindStack(const FGameXXKInventory& Inventory, FName ItemId)
{
    return Inventory.Items.FindByPredicate([ItemId](const FGameXXKItemStack& Stack)
    {
        return Stack.ItemId == ItemId;
    });
}
}

int32 FGameXXKInventoryRules::GetItemCount(const FGameXXKInventory& Inventory, FName ItemId)
{
    const FGameXXKItemStack* Stack = FindStack(Inventory, ItemId);
    return Stack ? Stack->Quantity : 0;
}

void FGameXXKInventoryRules::AddItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity)
{
    if (ItemId.IsNone() || Quantity <= 0)
    {
        return;
    }

    if (FGameXXKItemStack* Stack = FindStack(Inventory, ItemId))
    {
        Stack->Quantity += Quantity;
        return;
    }

    FGameXXKItemStack NewStack;
    NewStack.ItemId = ItemId;
    NewStack.Quantity = Quantity;
    Inventory.Items.Add(NewStack);
}

bool FGameXXKInventoryRules::RemoveItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity)
{
    if (ItemId.IsNone() || Quantity <= 0)
    {
        return false;
    }

    FGameXXKItemStack* Stack = FindStack(Inventory, ItemId);
    if (!Stack || Stack->Quantity < Quantity)
    {
        return false;
    }

    Stack->Quantity -= Quantity;
    Inventory.Items.RemoveAll([](const FGameXXKItemStack& Candidate)
    {
        return Candidate.Quantity <= 0;
    });
    return true;
}

bool FGameXXKInventoryRules::BuyItem(FGameXXKInventory& PlayerInventory, const FGameXXKItemDefinition& Definition, int32 Quantity)
{
    if (Quantity <= 0)
    {
        return false;
    }

    const int32 Cost = Definition.BuyPrice * Quantity;
    if (PlayerInventory.Gold < Cost)
    {
        return false;
    }

    PlayerInventory.Gold -= Cost;
    AddItem(PlayerInventory, Definition.ItemId, Quantity);
    return true;
}

bool FGameXXKInventoryRules::SellItem(FGameXXKInventory& PlayerInventory, const FGameXXKItemDefinition& Definition, int32 Quantity)
{
    if (!RemoveItem(PlayerInventory, Definition.ItemId, Quantity))
    {
        return false;
    }

    PlayerInventory.Gold += Definition.SellPrice * Quantity;
    return true;
}

bool FGameXXKInventoryRules::ConsumeItem(FGameXXKInventory& Inventory, FName ItemId, int32 Quantity)
{
    return RemoveItem(Inventory, ItemId, Quantity);
}

FGameXXKInventory FGameXXKInventoryRules::MakeDungeonSnapshot(const FGameXXKInventory& Inventory)
{
    return Inventory;
}

void FGameXXKInventoryRules::CommitDungeonConsumables(FGameXXKInventory& MainInventory, const FGameXXKInventory& DungeonSnapshot)
{
    for (const FGameXXKItemStack& SnapshotStack : DungeonSnapshot.Items)
    {
        if (FGameXXKItemStack* MainStack = FindStack(MainInventory, SnapshotStack.ItemId))
        {
            MainStack->Quantity = SnapshotStack.Quantity;
        }
    }

    MainInventory.Items.RemoveAll([](const FGameXXKItemStack& Candidate)
    {
        return Candidate.Quantity <= 0;
    });
}
```

- [ ] **Step 5: Run tests**

Run build and automation commands.

Expected: progression test and inventory test pass.

- [ ] **Step 6: Commit**

```powershell
git add Source\GameXXK\Public\Mvp Source\GameXXK\Private\Mvp Source\GameXXK\Private\Tests
git commit -m "feat: add MVP inventory and trade rules"
```

## Task 3: Fixed Node Dungeon Rules

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpTypes.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKDungeonRules.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKDungeonRules.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKDungeonRulesTests.cpp`

- [ ] **Step 1: Write failing dungeon tests**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Mvp/GameXXKDungeonRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKDungeonRulesTest,
    "GameXXK.MVP.Dungeon.FixedFiveNodeRoute",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDungeonRulesTest::RunTest(const FString& Parameters)
{
    FGameXXKDungeonState Dungeon = FGameXXKDungeonRules::CreateFixedMvpDungeon(1234);
    TestEqual(TEXT("Fixed dungeon has five nodes"), Dungeon.Nodes.Num(), 5);

    TArray<int32> Reachable = FGameXXKDungeonRules::GetReachableNodeIndices(Dungeon);
    TestEqual(TEXT("Start exposes two route choices"), Reachable.Num(), 2);
    TestTrue(TEXT("Battle route is reachable"), Reachable.Contains(1));
    TestTrue(TEXT("Event route is reachable"), Reachable.Contains(2));

    TestTrue(TEXT("Complete battle node"), FGameXXKDungeonRules::CompleteNode(Dungeon, 1));
    Reachable = FGameXXKDungeonRules::GetReachableNodeIndices(Dungeon);
    TestEqual(TEXT("Camp is reachable after route branch"), Reachable.Num(), 1);
    TestTrue(TEXT("Camp index reachable"), Reachable.Contains(3));

    TestTrue(TEXT("Complete camp"), FGameXXKDungeonRules::CompleteNode(Dungeon, 3));
    Reachable = FGameXXKDungeonRules::GetReachableNodeIndices(Dungeon);
    TestEqual(TEXT("Boss is reachable after camp"), Reachable.Num(), 1);
    TestTrue(TEXT("Boss index reachable"), Reachable.Contains(4));
    return true;
}

#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run the build command.

Expected: FAIL because dungeon types and rules do not exist.

- [ ] **Step 3: Add dungeon types**

Append these types to `GameXXKMvpTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKDungeonNodeType : uint8
{
    Start,
    Battle,
    Event,
    Camp,
    Boss
};

USTRUCT(BlueprintType)
struct FGameXXKDungeonNode
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 NodeIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKDungeonNodeType NodeType = EGameXXKDungeonNodeType::Battle;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<int32> NextNodeIndices;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bCompleted = false;
};

USTRUCT(BlueprintType)
struct FGameXXKDungeonState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Seed = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentNodeIndex = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGameXXKDungeonNode> Nodes;
};
```

- [ ] **Step 4: Implement fixed dungeon rules**

Create `GameXXKDungeonRules.h` with:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Mvp/GameXXKMvpTypes.h"

class GAMEXXK_API FGameXXKDungeonRules
{
public:
    static FGameXXKDungeonState CreateFixedMvpDungeon(int32 Seed);
    static TArray<int32> GetReachableNodeIndices(const FGameXXKDungeonState& Dungeon);
    static bool CompleteNode(FGameXXKDungeonState& Dungeon, int32 NodeIndex);
};
```

Create `GameXXKDungeonRules.cpp`:

```cpp
#include "Mvp/GameXXKDungeonRules.h"

namespace
{
FGameXXKDungeonNode MakeNode(int32 Index, EGameXXKDungeonNodeType Type, TArray<int32> Next)
{
    FGameXXKDungeonNode Node;
    Node.NodeIndex = Index;
    Node.NodeType = Type;
    Node.NextNodeIndices = MoveTemp(Next);
    Node.bCompleted = Type == EGameXXKDungeonNodeType::Start;
    return Node;
}
}

FGameXXKDungeonState FGameXXKDungeonRules::CreateFixedMvpDungeon(int32 Seed)
{
    FGameXXKDungeonState Dungeon;
    Dungeon.Seed = Seed;
    Dungeon.CurrentNodeIndex = 0;
    Dungeon.Nodes.Add(MakeNode(0, EGameXXKDungeonNodeType::Start, {1, 2}));
    Dungeon.Nodes.Add(MakeNode(1, EGameXXKDungeonNodeType::Battle, {3}));
    Dungeon.Nodes.Add(MakeNode(2, EGameXXKDungeonNodeType::Event, {3}));
    Dungeon.Nodes.Add(MakeNode(3, EGameXXKDungeonNodeType::Camp, {4}));
    Dungeon.Nodes.Add(MakeNode(4, EGameXXKDungeonNodeType::Boss, {}));
    return Dungeon;
}

TArray<int32> FGameXXKDungeonRules::GetReachableNodeIndices(const FGameXXKDungeonState& Dungeon)
{
    if (!Dungeon.Nodes.IsValidIndex(Dungeon.CurrentNodeIndex))
    {
        return {};
    }

    const FGameXXKDungeonNode& Current = Dungeon.Nodes[Dungeon.CurrentNodeIndex];
    TArray<int32> Reachable;
    for (int32 NextIndex : Current.NextNodeIndices)
    {
        if (Dungeon.Nodes.IsValidIndex(NextIndex) && !Dungeon.Nodes[NextIndex].bCompleted)
        {
            Reachable.Add(NextIndex);
        }
    }
    return Reachable;
}

bool FGameXXKDungeonRules::CompleteNode(FGameXXKDungeonState& Dungeon, int32 NodeIndex)
{
    if (!Dungeon.Nodes.IsValidIndex(NodeIndex))
    {
        return false;
    }

    TArray<int32> Reachable = GetReachableNodeIndices(Dungeon);
    if (!Reachable.Contains(NodeIndex))
    {
        return false;
    }

    Dungeon.Nodes[NodeIndex].bCompleted = true;
    Dungeon.CurrentNodeIndex = NodeIndex;
    return true;
}
```

- [ ] **Step 5: Run tests**

Run build and automation commands.

Expected: progression, inventory, and dungeon tests pass.

- [ ] **Step 6: Commit**

```powershell
git add Source\GameXXK\Public\Mvp Source\GameXXK\Private\Mvp Source\GameXXK\Private\Tests
git commit -m "feat: add MVP dungeon route rules"
```

## Task 4: Minimal Octopath-Like Battle Rules

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpTypes.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKBattleRules.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKBattleRules.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKBattleRulesTests.cpp`

- [ ] **Step 1: Write failing battle tests**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Mvp/GameXXKBattleRules.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKBattleRulesTest,
    "GameXXK.MVP.Battle.SpeedBpWeaknessAndRewards",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKBattleRulesTest::RunTest(const FString& Parameters)
{
    FGameXXKBattleState Battle = FGameXXKBattleRules::CreateTestBattle();
    TArray<int32> Order = FGameXXKBattleRules::BuildTurnOrder(Battle);
    TestEqual(TEXT("Fastest actor acts first"), Battle.Actors[Order[0]].ActorId, FName(TEXT("hero")));

    FGameXXKBattleRules::BeginTurn(Battle, 0);
    TestEqual(TEXT("Hero gains one BP"), Battle.Actors[0].BP, 1);

    FGameXXKBattleAction WeakAttack;
    WeakAttack.SourceActorIndex = 0;
    WeakAttack.TargetActorIndex = 2;
    WeakAttack.ActionType = EGameXXKBattleActionType::BasicAttack;
    WeakAttack.DamageType = EGameXXKDamageType::Sword;
    FGameXXKBattleActionResult Result = FGameXXKBattleRules::ApplyAction(Battle, WeakAttack);

    TestTrue(TEXT("Weakness is reported"), Result.bHitWeakness);
    TestEqual(TEXT("Shield reduced"), Battle.Actors[2].Shield, 0);
    TestTrue(TEXT("Enemy is broken"), Battle.Actors[2].bBroken);

    Battle.Actors[2].CurrentHP = 0;
    FGameXXKBattleOutcome Outcome = FGameXXKBattleRules::EvaluateOutcome(Battle);
    TestEqual(TEXT("Battle continues while another enemy lives"), Outcome.OutcomeType, EGameXXKBattleOutcomeType::InProgress);

    Battle.Actors[3].CurrentHP = 0;
    Outcome = FGameXXKBattleRules::EvaluateOutcome(Battle);
    TestEqual(TEXT("All enemies defeated"), Outcome.OutcomeType, EGameXXKBattleOutcomeType::Victory);
    TestTrue(TEXT("Victory grants XP"), Outcome.XpReward > 0);
    TestTrue(TEXT("Victory grants gold"), Outcome.GoldReward > 0);
    return true;
}

#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run the build command.

Expected: FAIL because battle types and rules do not exist.

- [ ] **Step 3: Add battle types**

Append battle enums and structs to `GameXXKMvpTypes.h`:

```cpp
UENUM(BlueprintType)
enum class EGameXXKTeam : uint8
{
    Player,
    Enemy
};

UENUM(BlueprintType)
enum class EGameXXKDamageType : uint8
{
    Sword,
    Staff,
    Spirit
};

UENUM(BlueprintType)
enum class EGameXXKBattleActionType : uint8
{
    BasicAttack,
    Skill,
    Defend,
    Item
};

UENUM(BlueprintType)
enum class EGameXXKBattleOutcomeType : uint8
{
    InProgress,
    Victory,
    Defeat
};

USTRUCT(BlueprintType)
struct FGameXXKBattleActor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FName ActorId;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKTeam Team = EGameXXKTeam::Player;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 MaxHP = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 CurrentHP = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Attack = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Defense = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Speed = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BP = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 Shield = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKDamageType Weakness = EGameXXKDamageType::Sword;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bBroken = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 BreakTurnsRemaining = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleAction
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 SourceActorIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 TargetActorIndex = INDEX_NONE;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKBattleActionType ActionType = EGameXXKBattleActionType::BasicAttack;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKDamageType DamageType = EGameXXKDamageType::Sword;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleActionResult
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 DamageApplied = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bHitWeakness = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool bTargetBroken = false;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleOutcome
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGameXXKBattleOutcomeType OutcomeType = EGameXXKBattleOutcomeType::InProgress;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 XpReward = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 GoldReward = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleState
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FGameXXKBattleActor> Actors;
};
```

- [ ] **Step 4: Implement battle rules**

Create `GameXXKBattleRules.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Mvp/GameXXKMvpTypes.h"

class GAMEXXK_API FGameXXKBattleRules
{
public:
    static FGameXXKBattleState CreateTestBattle();
    static TArray<int32> BuildTurnOrder(const FGameXXKBattleState& Battle);
    static void BeginTurn(FGameXXKBattleState& Battle, int32 ActorIndex);
    static FGameXXKBattleActionResult ApplyAction(FGameXXKBattleState& Battle, const FGameXXKBattleAction& Action);
    static FGameXXKBattleOutcome EvaluateOutcome(const FGameXXKBattleState& Battle);
};
```

Create `GameXXKBattleRules.cpp`:

```cpp
#include "Mvp/GameXXKBattleRules.h"
#include "Algo/Sort.h"

FGameXXKBattleState FGameXXKBattleRules::CreateTestBattle()
{
    FGameXXKBattleState Battle;

    auto AddActor = [&Battle](FName Id, EGameXXKTeam Team, int32 HP, int32 Attack, int32 Defense, int32 Speed, int32 Shield, EGameXXKDamageType Weakness)
    {
        FGameXXKBattleActor Actor;
        Actor.ActorId = Id;
        Actor.Team = Team;
        Actor.MaxHP = HP;
        Actor.CurrentHP = HP;
        Actor.Attack = Attack;
        Actor.Defense = Defense;
        Actor.Speed = Speed;
        Actor.Shield = Shield;
        Actor.Weakness = Weakness;
        Battle.Actors.Add(Actor);
    };

    AddActor(TEXT("hero"), EGameXXKTeam::Player, 100, 18, 4, 12, 1, EGameXXKDamageType::Spirit);
    AddActor(TEXT("follower"), EGameXXKTeam::Player, 80, 14, 3, 9, 1, EGameXXKDamageType::Staff);
    AddActor(TEXT("bandit"), EGameXXKTeam::Enemy, 30, 9, 2, 6, 1, EGameXXKDamageType::Sword);
    AddActor(TEXT("wolf"), EGameXXKTeam::Enemy, 28, 8, 1, 8, 1, EGameXXKDamageType::Staff);
    return Battle;
}

TArray<int32> FGameXXKBattleRules::BuildTurnOrder(const FGameXXKBattleState& Battle)
{
    TArray<int32> Order;
    for (int32 Index = 0; Index < Battle.Actors.Num(); ++Index)
    {
        if (Battle.Actors[Index].CurrentHP > 0)
        {
            Order.Add(Index);
        }
    }

    Algo::Sort(Order, [&Battle](int32 A, int32 B)
    {
        return Battle.Actors[A].Speed > Battle.Actors[B].Speed;
    });
    return Order;
}

void FGameXXKBattleRules::BeginTurn(FGameXXKBattleState& Battle, int32 ActorIndex)
{
    if (!Battle.Actors.IsValidIndex(ActorIndex))
    {
        return;
    }

    FGameXXKBattleActor& Actor = Battle.Actors[ActorIndex];
    Actor.BP = FMath::Clamp(Actor.BP + 1, 0, 3);
    if (Actor.BreakTurnsRemaining > 0)
    {
        Actor.BreakTurnsRemaining -= 1;
        Actor.bBroken = Actor.BreakTurnsRemaining > 0;
    }
}

FGameXXKBattleActionResult FGameXXKBattleRules::ApplyAction(FGameXXKBattleState& Battle, const FGameXXKBattleAction& Action)
{
    FGameXXKBattleActionResult Result;
    if (!Battle.Actors.IsValidIndex(Action.SourceActorIndex) || !Battle.Actors.IsValidIndex(Action.TargetActorIndex))
    {
        return Result;
    }

    FGameXXKBattleActor& Source = Battle.Actors[Action.SourceActorIndex];
    FGameXXKBattleActor& Target = Battle.Actors[Action.TargetActorIndex];
    if (Source.CurrentHP <= 0 || Target.CurrentHP <= 0)
    {
        return Result;
    }

    const int32 Damage = FMath::Max(1, Source.Attack - Target.Defense);
    Target.CurrentHP = FMath::Max(0, Target.CurrentHP - Damage);
    Result.DamageApplied = Damage;

    if (Action.DamageType == Target.Weakness && Target.Shield > 0)
    {
        Target.Shield -= 1;
        Result.bHitWeakness = true;
        if (Target.Shield <= 0)
        {
            Target.bBroken = true;
            Target.BreakTurnsRemaining = 1;
            Result.bTargetBroken = true;
        }
    }

    return Result;
}

FGameXXKBattleOutcome FGameXXKBattleRules::EvaluateOutcome(const FGameXXKBattleState& Battle)
{
    bool bAnyPlayerAlive = false;
    bool bAnyEnemyAlive = false;

    for (const FGameXXKBattleActor& Actor : Battle.Actors)
    {
        if (Actor.CurrentHP <= 0)
        {
            continue;
        }

        bAnyPlayerAlive |= Actor.Team == EGameXXKTeam::Player;
        bAnyEnemyAlive |= Actor.Team == EGameXXKTeam::Enemy;
    }

    FGameXXKBattleOutcome Outcome;
    if (!bAnyEnemyAlive)
    {
        Outcome.OutcomeType = EGameXXKBattleOutcomeType::Victory;
        Outcome.XpReward = 20;
        Outcome.GoldReward = 30;
    }
    else if (!bAnyPlayerAlive)
    {
        Outcome.OutcomeType = EGameXXKBattleOutcomeType::Defeat;
    }
    return Outcome;
}
```

- [ ] **Step 5: Run tests**

Run build and automation commands.

Expected: all `GameXXK.MVP` rule tests pass.

- [ ] **Step 6: Commit**

```powershell
git add Source\GameXXK\Public\Mvp Source\GameXXK\Private\Mvp Source\GameXXK\Private\Tests
git commit -m "feat: add MVP battle rules"
```

## Task 5: SaveGame And MVP Subsystem

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKSaveGame.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKSaveGame.cpp`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpSubsystem.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKMvpSubsystem.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKSaveGameTests.cpp`

- [ ] **Step 1: Write failing SaveGame test**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Mvp/GameXXKSaveGame.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKSaveGameShapeTest,
    "GameXXK.MVP.SaveGame.MinimalStateShape",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSaveGameShapeTest::RunTest(const FString& Parameters)
{
    UGameXXKSaveGame* Save = NewObject<UGameXXKSaveGame>();
    TestNotNull(TEXT("Save object creates"), Save);
    TestTrue(TEXT("Default progression unlocks first region"), Save->Progression.UnlockedRegions.Contains(EGameXXKRegionId::QingshanInn));
    TestEqual(TEXT("Default level is one"), Save->Progression.PlayerLevel, 1);
    TestEqual(TEXT("Default XP is zero"), Save->Progression.CurrentXP, 0);
    TestEqual(TEXT("Default gold is zero"), Save->Progression.Gold, 0);
    return true;
}

#endif
```

- [ ] **Step 2: Run test to verify it fails**

Run the build command.

Expected: FAIL because `UGameXXKSaveGame` does not exist.

- [ ] **Step 3: Implement SaveGame**

Create `GameXXKSaveGame.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SaveGame.h"
#include "Mvp/GameXXKMvpTypes.h"
#include "GameXXKSaveGame.generated.h"

UCLASS()
class GAMEXXK_API UGameXXKSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UGameXXKSaveGame();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="MVP")
    FGameXXKProgressionState Progression;
};
```

Create `GameXXKSaveGame.cpp`:

```cpp
#include "Mvp/GameXXKSaveGame.h"
#include "Mvp/GameXXKProgressionRules.h"

UGameXXKSaveGame::UGameXXKSaveGame()
{
    Progression = FGameXXKProgressionRules::MakeNewGame();
}
```

- [ ] **Step 4: Implement MVP subsystem wrapper**

Create `GameXXKMvpSubsystem.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Mvp/GameXXKMvpTypes.h"
#include "GameXXKMvpSubsystem.generated.h"

UCLASS()
class GAMEXXK_API UGameXXKMvpSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
    void StartNewGame();

    UFUNCTION(BlueprintPure, Category="GameXXK|MVP")
    const FGameXXKProgressionState& GetProgression() const;

    UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
    void AcceptQuest();

    UFUNCTION(BlueprintPure, Category="GameXXK|MVP")
    bool CanEnterDungeon() const;

    UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
    void FailDungeon();

    UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
    void CompleteBoss();

private:
    UPROPERTY()
    FGameXXKProgressionState Progression;
};
```

Create `GameXXKMvpSubsystem.cpp`:

```cpp
#include "Mvp/GameXXKMvpSubsystem.h"
#include "Mvp/GameXXKProgressionRules.h"

void UGameXXKMvpSubsystem::StartNewGame()
{
    Progression = FGameXXKProgressionRules::MakeNewGame();
}

const FGameXXKProgressionState& UGameXXKMvpSubsystem::GetProgression() const
{
    return Progression;
}

void UGameXXKMvpSubsystem::AcceptQuest()
{
    FGameXXKProgressionRules::AcceptQuest(Progression);
}

bool UGameXXKMvpSubsystem::CanEnterDungeon() const
{
    return FGameXXKProgressionRules::CanEnterDungeon(Progression);
}

void UGameXXKMvpSubsystem::FailDungeon()
{
    FGameXXKProgressionRules::FailDungeon(Progression);
}

void UGameXXKMvpSubsystem::CompleteBoss()
{
    FGameXXKProgressionRules::CompleteBoss(Progression);
}
```

- [ ] **Step 5: Run tests**

Run build and automation commands.

Expected: all rule and SaveGame tests pass.

- [ ] **Step 6: Commit**

```powershell
git add Source\GameXXK\Public\Mvp Source\GameXXK\Private\Mvp Source\GameXXK\Private\Tests
git commit -m "feat: add MVP save state subsystem"
```

## Task 6: Migrate TestMap Prototype Content

**Files:**
- Create: `D:/UE5 demo/GameXXK/Content/Prototype/TestMap/1Game/...`
- Modify: `D:/UE5 demo/GameXXK/docs/defense/index.html`

- [ ] **Step 1: Copy TestMap content into an isolated prototype folder**

Run:

```powershell
$src = 'D:\UE5 demo\TestMap\Content\1Game'
$dst = 'D:\UE5 demo\GameXXK\Content\Prototype\TestMap\1Game'
New-Item -ItemType Directory -Force -Path $dst | Out-Null
Copy-Item -LiteralPath (Join-Path $src '*') -Destination $dst -Recurse -Force
```

Expected: `Content\Prototype\TestMap\1Game\UI`, `Data`, `Texture`, `Map`, `Control`, and `Material` exist.

- [ ] **Step 2: Verify copied asset count**

Run:

```powershell
(Get-ChildItem -Recurse 'D:\UE5 demo\GameXXK\Content\Prototype\TestMap\1Game' -Include *.uasset,*.umap).Count
```

Expected: `42`.

- [ ] **Step 3: Build after migration**

Run the build command.

Expected: build succeeds. Asset upgrade happens when opened in UE 5.8, not during C++ build.

- [ ] **Step 4: Update defense page**

Add a `2026-07-01` entry to `docs/defense/index.html` stating that TestMap UE 5.4 node-map prototype content was copied into `Content/Prototype/TestMap/1Game` as reference content and is not the runtime authority.

- [ ] **Step 5: Commit**

```powershell
git add Content\Prototype\TestMap docs\defense\index.html
git commit -m "chore: migrate TestMap prototype content"
```

## Task 7: Town Interaction, Movement, Quest NPC, Merchant NPC, And Follower Shell

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/GameXXK.Build.cs`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Interaction/GameXXKInteractable.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Interaction/GameXXKInteractionComponent.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Interaction/GameXXKInteractionComponent.cpp`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Town/GameXXKTownPlayerPawn.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Town/GameXXKTownPlayerPawn.cpp`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Town/GameXXKTownNpcActor.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Town/GameXXKTownNpcActor.cpp`
- Test: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Tests/GameXXKInteractionTests.cpp`

- [ ] **Step 1: Add module dependencies**

In `GameXXK.Build.cs`, extend dependencies:

```csharp
PublicDependencyModuleNames.AddRange(new string[] {
    "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput",
    "UMG", "Slate", "SlateCore", "Paper2D"
});
```

- [ ] **Step 2: Write failing interaction test**

```cpp
#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Interaction/GameXXKInteractionComponent.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKInteractionComponentTest,
    "GameXXK.MVP.Interaction.ComponentTracksFocusedActor",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKInteractionComponentTest::RunTest(const FString& Parameters)
{
    UGameXXKInteractionComponent* Component = NewObject<UGameXXKInteractionComponent>();
    AActor* Actor = NewObject<AActor>();
    Component->SetFocusedActorForTest(Actor);
    TestEqual(TEXT("Focused actor stored"), Component->GetFocusedActor(), Actor);
    Component->SetFocusedActorForTest(nullptr);
    TestNull(TEXT("Focused actor clears"), Component->GetFocusedActor());
    return true;
}

#endif
```

- [ ] **Step 3: Run test to verify it fails**

Run the build command.

Expected: FAIL because interaction component does not exist.

- [ ] **Step 4: Add interactable interface**

Create `GameXXKInteractable.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "GameXXKInteractable.generated.h"

UINTERFACE(Blueprintable)
class GAMEXXK_API UGameXXKInteractable : public UInterface
{
    GENERATED_BODY()
};

class GAMEXXK_API IGameXXKInteractable
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GameXXK|Interaction")
    FText GetInteractionPrompt() const;

    UFUNCTION(BlueprintNativeEvent, BlueprintCallable, Category="GameXXK|Interaction")
    void Interact(APawn* InstigatorPawn);
};
```

- [ ] **Step 5: Add interaction component**

Create `GameXXKInteractionComponent.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameXXKInteractionComponent.generated.h"

UCLASS(ClassGroup=(GameXXK), meta=(BlueprintSpawnableComponent))
class GAMEXXK_API UGameXXKInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintPure, Category="GameXXK|Interaction")
    AActor* GetFocusedActor() const;

    UFUNCTION(BlueprintCallable, Category="GameXXK|Interaction")
    void Interact();

    void SetFocusedActorForTest(AActor* Actor);

private:
    UPROPERTY()
    TObjectPtr<AActor> FocusedActor;
};
```

Create `GameXXKInteractionComponent.cpp`:

```cpp
#include "Interaction/GameXXKInteractionComponent.h"
#include "Interaction/GameXXKInteractable.h"

AActor* UGameXXKInteractionComponent::GetFocusedActor() const
{
    return FocusedActor.Get();
}

void UGameXXKInteractionComponent::Interact()
{
    AActor* Actor = FocusedActor.Get();
    APawn* OwnerPawn = Cast<APawn>(GetOwner());
    if (Actor && Actor->GetClass()->ImplementsInterface(UGameXXKInteractable::StaticClass()))
    {
        IGameXXKInteractable::Execute_Interact(Actor, OwnerPawn);
    }
}

void UGameXXKInteractionComponent::SetFocusedActorForTest(AActor* Actor)
{
    FocusedActor = Actor;
}
```

- [ ] **Step 6: Add town pawn and NPC shell**

Create `AGameXXKTownPlayerPawn` with:

- `UPaperFlipbookComponent* Visual`
- `UFloatingPawnMovement* Movement`
- `UGameXXKInteractionComponent* Interaction`
- movement input functions for `MoveX` and `MoveY`
- `Interact` function bound to `F`

Create `AGameXXKTownNpcActor` with:

- enum role: quest, merchant
- `bFollowerActive`
- `FollowTarget`
- `FollowDistance`
- `Tick` movement toward target when follower is active
- `Interact_Implementation` that routes to Blueprint events `OnQuestInteract` or `OnMerchantInteract`

- [ ] **Step 7: Run tests and build**

Run build and automation commands.

Expected: all rule tests and interaction test pass.

- [ ] **Step 8: Commit**

```powershell
git add Source\GameXXK\GameXXK.Build.cs Source\GameXXK\Public\Interaction Source\GameXXK\Private\Interaction Source\GameXXK\Public\Town Source\GameXXK\Private\Town Source\GameXXK\Private\Tests
git commit -m "feat: add MVP town interaction shell"
```

## Task 8: UI Widget Bases For Menu, World Map, Dialogue, Inventory, Trade, Dungeon, Battle, And Character Panel

**Files:**
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKMainMenuWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKWorldMapWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKQuestDialogWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKInventoryWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKTradeWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKCharacterPanelWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKDungeonMapWidget.h`
- Create: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/UI/GameXXKBattleWidget.h`
- Create matching `.cpp` files under `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/`

- [ ] **Step 1: Create a common UI event pattern**

Each widget base is a `UUserWidget` with Blueprint-callable methods that call into `UGameXXKMvpSubsystem` or expose `BlueprintImplementableEvent` hooks.

Example `GameXXKWorldMapWidget.h`:

```cpp
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Mvp/GameXXKMvpTypes.h"
#include "GameXXKWorldMapWidget.generated.h"

UCLASS(Abstract, Blueprintable)
class GAMEXXK_API UGameXXKWorldMapWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category="GameXXK|WorldMap")
    bool TrySelectRegion(EGameXXKRegionId RegionId);

    UFUNCTION(BlueprintImplementableEvent, Category="GameXXK|WorldMap")
    void OnLockedRegionSelected(EGameXXKRegionId RegionId);

    UFUNCTION(BlueprintImplementableEvent, Category="GameXXK|WorldMap")
    void OnUnlockedRegionSelected(EGameXXKRegionId RegionId);
};
```

`TrySelectRegion` gets `UGameXXKMvpSubsystem` from the game instance and checks region unlock state.

- [ ] **Step 2: Add widget bases**

Add minimal widget bases:

- `UGameXXKMainMenuWidget::StartGame`
- `UGameXXKQuestDialogWidget::AcceptQuest`
- `UGameXXKInventoryWidget::RefreshInventory`
- `UGameXXKTradeWidget::BuyItemById`, `SellItemById`
- `UGameXXKCharacterPanelWidget::SetCharacterSummary`
- `UGameXXKDungeonMapWidget::SelectNode`
- `UGameXXKBattleWidget::SubmitAction`

Each function is thin and delegates to subsystem/rules or broadcasts Blueprint events.

- [ ] **Step 3: Build**

Run build command.

Expected: build succeeds.

- [ ] **Step 4: Commit**

```powershell
git add Source\GameXXK\Public\UI Source\GameXXK\Private\UI
git commit -m "feat: add MVP UI widget bases"
```

## Task 9: Blueprint And Asset Assembly Via UE MCP

**Files:**
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/UI/`
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Maps/`
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Characters/`
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Data/`
- Modify: `D:/UE5 demo/GameXXK/Config/DefaultEngine.ini`

- [ ] **Step 1: Open or launch UE 5.8 editor with UE MCP**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 1
```

Expected: editor launches or UE MCP reports readiness. If Live Coding blocks build, close the editor and rerun build first.

- [ ] **Step 2: Create GameXXK content folders**

Use UE MCP or editor content browser to create:

```text
/Game/GameXXK/UI
/Game/GameXXK/Maps
/Game/GameXXK/Characters/Hero
/Game/GameXXK/Characters/Follower
/Game/GameXXK/Characters/Merchant
/Game/GameXXK/Data
/Game/GameXXK/Sprites
```

- [ ] **Step 3: Create Blueprint widgets**

Create widget blueprints derived from the C++ widget bases:

```text
/Game/GameXXK/UI/WBP_MainMenu
/Game/GameXXK/UI/WBP_WorldMap
/Game/GameXXK/UI/WBP_QuestDialog
/Game/GameXXK/UI/WBP_Inventory
/Game/GameXXK/UI/WBP_Trade
/Game/GameXXK/UI/WBP_CharacterPanel
/Game/GameXXK/UI/WBP_DungeonMap
/Game/GameXXK/UI/WBP_Battle
```

Each widget can use simple text/buttons first. No decorative layout is needed before the acceptance loop works.

- [ ] **Step 4: Create maps**

Create:

```text
/Game/GameXXK/Maps/L_Main
/Game/GameXXK/Maps/L_QingshanInn
```

`L_Main` starts with main menu UI. `L_QingshanInn` contains a straight road blockout, quest NPC, merchant NPC, player spawn, and exit trigger.

- [ ] **Step 5: Configure startup map**

Set `GameDefaultMap` and `EditorStartupMap` to `/Game/GameXXK/Maps/L_Main`.

- [ ] **Step 6: PIE smoke**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 5
```

Expected: PIE starts without fatal errors.

- [ ] **Step 7: Commit**

```powershell
git add Content\GameXXK Config\DefaultEngine.ini
git commit -m "feat: assemble MVP menu and town shell"
```

## Task 10: Sprite Generation And PaperZD Character Setup

**Files:**
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Sprites/Generated/`
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Characters/Hero/`
- Create under `D:/UE5 demo/GameXXK/Content/GameXXK/Characters/Follower/`
- Use source images:
  - hero: `C:/Users/shxuw/Documents/Tencent Files/978420603/nt_qq/nt_data/Pic/2026-07/Ori/9799dd77196a1b110c31300906c6f233.png`
  - follower: `C:/Users/shxuw/Documents/Tencent Files/978420603/nt_qq/nt_data/Pic/2026-07/Ori/7b55047faaee29a3fc1ef33a37375ed0.png`
  - merchant: `C:/Users/shxuw/Documents/Tencent Files/978420603/nt_qq/nt_data/Pic/2026-07/Ori/17d068c1ece3b52cae953312e8b3232e.png`

- [ ] **Step 1: Generate missing idle/action sheets**

Use built-in `image_gen` for project-bound bitmap assets. Generate:

```text
hero_idle_4dir.png
hero_attack_4dir.png
hero_hit_4dir.png
hero_skill_4dir.png
hero_die_4dir.png
follower_idle_4dir.png
follower_attack_4dir.png
follower_hit_4dir.png
follower_skill_4dir.png
follower_die_4dir.png
```

Prompt constraints:

```text
Use case: stylized-concept
Asset type: 2D RPG pixel sprite sheet for UE PaperZD
Primary request: create a tiny 1x RPG pixel sprite sheet matching the supplied character identity.
Style/medium: crisp pixel art, transparent-ready flat chroma-key background.
Composition/framing: grid sprite sheet, consistent cell size, centered tiny character, generous padding.
Constraints: keep costume colors and silhouette from the reference; no text; no watermark; no shadows; no background texture.
Avoid: large portrait sprites, inconsistent row direction, cropped limbs, painterly rendering.
```

- [ ] **Step 2: Move final images into workspace**

Save selected outputs under:

```text
D:/UE5 demo/GameXXK/Content/GameXXK/Sprites/Generated/
```

Never leave project-referenced assets only under the default generated-image directory.

- [ ] **Step 3: Import sprites and set up PaperZD**

In UE 5.8:

- import generated sheets
- create Paper2D sprites/flipbooks
- create PaperZD animation sources/animation blueprints for hero and follower
- set hero town movement to use existing 8-direction sheet from image 3
- fill missing diagonal movement directions by horizontal mirroring
- set follower idle/action flipbooks for battle and town follow

- [ ] **Step 4: PIE smoke**

Run:

```powershell
python scripts\ue_tdd_pipeline.py --pie-duration 5
```

Expected: town opens, hero visual displays, follower visual displays after quest acceptance.

- [ ] **Step 5: Commit**

```powershell
git add Content\GameXXK\Sprites Content\GameXXK\Characters
git commit -m "feat: add MVP character sprite assets"
```

## Task 11: Wire Dungeon UI, Battle UI, Trading UI, SaveGame, And Acceptance Path

**Files:**
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Public/Mvp/GameXXKMvpSubsystem.h`
- Modify: `D:/UE5 demo/GameXXK/Source/GameXXK/Private/Mvp/GameXXKMvpSubsystem.cpp`
- Modify: UI widget C++ files under `D:/UE5 demo/GameXXK/Source/GameXXK/Private/UI/`
- Modify: Blueprints under `D:/UE5 demo/GameXXK/Content/GameXXK/UI/`
- Modify: town map under `D:/UE5 demo/GameXXK/Content/GameXXK/Maps/L_QingshanInn`

- [ ] **Step 1: Add subsystem orchestration API**

Add Blueprint-callable methods:

```cpp
UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
void OpenDungeonFromTownExit();

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
bool SelectDungeonNode(int32 NodeIndex);

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
void ApplyEventChoice(int32 ChoiceIndex);

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
void ApplyCampChoice(int32 ChoiceIndex);

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
FGameXXKBattleState StartBattleForNode(int32 NodeIndex);

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
FGameXXKBattleActionResult SubmitBattleAction(const FGameXXKBattleAction& Action);

UFUNCTION(BlueprintCallable, Category="GameXXK|MVP")
void CommitBattleOutcome(const FGameXXKBattleOutcome& Outcome);
```

- [ ] **Step 2: Wire UI buttons**

In Blueprint widgets:

- main menu `开始游戏` calls `StartNewGame` and opens world map
- world map unlocked `青山驿站` opens town map
- quest dialog `接受` calls `AcceptQuest`
- town exit calls `OpenDungeonFromTownExit`
- dungeon node click calls `SelectDungeonNode`
- event choices call `ApplyEventChoice`
- camp choices call `ApplyCampChoice`
- battle buttons call `SubmitBattleAction`
- merchant buy/sell calls trade APIs

- [ ] **Step 3: Add SaveGame read/write**

Subsystem uses slot name:

```cpp
static const TCHAR* MvpSaveSlot = TEXT("GameXXK_MVP_Save");
static constexpr int32 MvpSaveUserIndex = 0;
```

Load on `StartGame`, save on Boss victory and after failure when XP or gold changed.

- [ ] **Step 4: Acceptance smoke test**

Manual PIE script or checklist:

```text
Main menu -> Start
World map -> QingshanInn
Town move -> F quest -> accept
Follower follows
Exit -> dungeon UI
Battle node -> win
Event node -> choose reward
Camp node -> choose heal or potion
Force failure in one run -> return town with XP/gold kept and used potion gone
Trade with merchant
Retry -> Boss victory
World map shows TanjiangFerry unlocked
Restart PIE -> minimal SaveGame state restored
```

- [ ] **Step 5: Commit**

```powershell
git add Source\GameXXK Content\GameXXK Config docs\defense\index.html
git commit -m "feat: wire MVP acceptance loop"
```

## Task 12: Final Verification, Defense Page, And Push

**Files:**
- Modify: `D:/UE5 demo/GameXXK/docs/defense/index.html`

- [ ] **Step 1: Update defense page**

Add entries for:

- MVP design locked
- C++ rules implemented and tested
- TestMap prototype migrated
- town and UI shell assembled
- battle/trade/save acceptance path verified
- sprite generation/import status

- [ ] **Step 2: Run verification**

Run:

```powershell
git diff --check
python -m py_compile scripts\ue_mcp_client.py scripts\ue_mcp_smoke.py scripts\ue_tdd_pipeline.py
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development -Project='D:\UE5 demo\GameXXK\GameXXK.uproject' -NoHotReload
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -ExecCmds='Automation RunTests GameXXK.MVP;Quit' -TestExit='Automation Test Queue Empty' -unattended -nop4 -nosplash -log
python scripts\ue_tdd_pipeline.py --pie-duration 5
```

Expected:

- `git diff --check` exits 0
- Python scripts compile
- UE build exits 0
- `GameXXK.MVP` automation tests pass
- PIE smoke has no fatal errors

- [ ] **Step 3: Commit docs update**

```powershell
git add docs\defense\index.html
git commit -m "docs: update MVP implementation record"
```

- [ ] **Step 4: Push**

```powershell
git push
```

Expected: `main -> main`.

## Self-Review

Spec coverage:

- Main menu, world map UI, town, quest, follower, merchant, node dungeon, battle, failure loop, trading, SaveGame, TestMap migration, sprites, and defense record are each mapped to at least one task.
- Core rules are front-loaded and covered by automation tests before UI assembly.
- TestMap migration is isolated under `Content/Prototype/TestMap/1Game` and does not take runtime authority.
- The acceptance demo path is explicitly listed in Task 11 and final verification is listed in Task 12.

Type consistency:

- Shared state is rooted in `FGameXXKProgressionState`.
- Inventory uses `FGameXXKInventory` and `FGameXXKItemDefinition`.
- Dungeon uses `FGameXXKDungeonState` and `FGameXXKDungeonNode`.
- Battle uses `FGameXXKBattleState`, `FGameXXKBattleActor`, `FGameXXKBattleAction`, `FGameXXKBattleActionResult`, and `FGameXXKBattleOutcome`.
- UI and Blueprint APIs call into `UGameXXKMvpSubsystem`.

Execution note:

- The current repository is on `main`. Before implementing this plan, create or enter an isolated workspace with `superpowers:using-git-worktrees`, then execute task-by-task using subagent-driven development or inline plan execution.
