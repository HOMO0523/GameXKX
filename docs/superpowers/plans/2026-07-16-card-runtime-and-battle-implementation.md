# Card Runtime and Battle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace the fixed single-action battle loop with a deterministic shared-hand card runtime: five-card hand, three shared energy, owner-specific inner power, statuses, terrain, enemy intents, explicit target-selection/ink-arrow interaction data, and one enemy phase after the player ends their phase.

**Architecture:** Immutable card data lives in a focused catalogue; serializable card/deck/status types live in a separate public header; a pure rules module performs draw, validation, effect application and phase transitions. `UGameXXKMVPRules` remains the compatibility facade that owns `FGameXXKRuntimeState`, creates battle snapshots and exposes only game-state transitions to `UGameXXKMVPSubsystem`.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT/UENUM serialization, `FRandomStream`, UMG-facing Blueprint APIs, Unreal Automation Tests, cold UBT builds.

---

## File structure

| File | Responsibility |
| --- | --- |
| Create `Source/GameXXK/Public/GameXXKCardTypes.h` | Serializable public card, effect, target-selection, terrain, status, deck, intent and card-view types. |
| Create `Source/GameXXK/Public/GameXXKCardCatalog.h` / `Private/GameXXKCardCatalog.cpp` | Read-only 174-card catalogue and lookup helpers. |
| Create `Source/GameXXK/Public/GameXXKCardRules.h` / `Private/GameXXKCardRules.cpp` | Deck draw/reshuffle, status helpers, card play validation/effects, phase and intent execution. |
| Modify `Source/GameXXK/Public/GameXXKMVPRules.h` | Include card types; extend runtime battle unit/state; expose card-play/end-phase/read-only view APIs. |
| Modify `Source/GameXXK/Private/GameXXKMVPRules.cpp` | Replace legacy action-loop helpers with card battle integration while retaining public flow behavior. |
| Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` / `Private/MVP/GameXXKMVPSubsystem.cpp` | Thin `PlayBattleCard`, `EndBattlePlayerPhase`, hand/intent view wrappers. |
| Create `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp` | 174-definition and immutable lookup tests. |
| Create `Source/GameXXK/Private/Tests/GameXXKCardRulesTest.cpp` | Draw/reshuffle, costs, statuses, phase and intent tests. |
| Modify `Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp` | Replace old one-action/instant-enemy assumptions with the approved shared-hand contract. |

## Constants and stable IDs

Use `FName` IDs, never localized display text, for every persisted card reference. The catalog must use the exact Chinese display names, costs and effects in specification §§9–11, with these namespaces:

```text
Card.Hero.*                 12 definitions
Card.Partner.Blade.*        18 definitions
Card.Partner.Guard.*        18 definitions
Card.Partner.Medic.*        18 definitions
Card.Partner.Hunter.*       18 definitions
Card.Partner.Sorcerer.*     18 definitions
Card.Partner.Formation.*    18 definitions
Card.Npc.Tusi.*             4 definitions
Card.Npc.SongJinbao.*       4 definitions
Card.Npc.Yuebai.*           4 definitions
Card.Npc.ZhouGuangzu.*      4 definitions
Card.Npc.Jingui.*           4 definitions
Card.Npc.Qionger.*          4 definitions
Card.Route.Common.*         10 definitions
Card.Route.Terrain.*        10 definitions
Card.Route.Rare.*           5 definitions
Card.Route.BlackBear.*      2 definitions
Card.Route.Tiger.*          3 definitions
```

This is exactly `12 + 108 + 24 + 30 = 174`; the two starting travel cards are existing route IDs `Card.Route.Common.PierceArmor` and `Card.Route.Common.GuardAndRecover`, not extra definitions.

### Task 1: Add serializable card types and the immutable 174-card catalogue

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Create: `Source/GameXXK/Public/GameXXKCardCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKCardCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`

- [ ] **Step 1: Write the red catalogue test.**

Create `GameXXKCardCatalogTest.cpp` with direct lookup/count assertions. The header does not exist yet, so this must fail at compilation first.

```cpp
#include "GameXXKCardCatalog.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGameXXKCardCatalogTest,
    "GameXXK.PartyDeck.CardCatalog",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardCatalogTest::RunTest(const FString& Parameters)
{
    const TArray<FGameXXKCardDef>& Cards = GameXXKCardCatalog::GetAll();
    TestEqual(TEXT("catalogue contains exactly 174 cards"), Cards.Num(), 174);
    TestEqual(TEXT("hero catalogue count"), GameXXKCardCatalog::CountBySource(EGameXXKCardSource::Hero), 12);
    TestEqual(TEXT("all partner candidate cards"), GameXXKCardCatalog::CountBySource(EGameXXKCardSource::Partner), 108);
    TestEqual(TEXT("task NPC cards"), GameXXKCardCatalog::CountBySource(EGameXXKCardSource::TaskNpc), 24);
    TestEqual(TEXT("route cards"), GameXXKCardCatalog::CountBySource(EGameXXKCardSource::Route), 30);

    const FGameXXKCardDef* Crane = GameXXKCardCatalog::Find(TEXT("Card.Hero.CraneWingSlash"));
    TestNotNull(TEXT("Crane Wing Slash has a stable definition"), Crane);
    TestEqual(TEXT("Crane costs two energy"), Crane ? Crane->EnergyCost : -1, 2);
    TestEqual(TEXT("Crane costs eight inner power"), Crane ? Crane->InnerPowerCost : -1, 8);

    const FGameXXKCardDef* Tiger = GameXXKCardCatalog::Find(TEXT("Card.Route.Tiger.FerryHuntWind"));
    TestNotNull(TEXT("Tiger reward is a route definition"), Tiger);
    TestEqual(TEXT("Tiger reward source is route"), Tiger ? Tiger->Source : EGameXXKCardSource::Hero, EGameXXKCardSource::Route);
    TestEqual(TEXT("Crane has a manual single-enemy target contract"), Crane ? Crane->TargetSpec.Mode : EGameXXKTargetSelectionMode::None, EGameXXKTargetSelectionMode::SingleEnemy);
    TestEqual(TEXT("Guiyuan allows its owner as a single-ally target"),
        GameXXKCardCatalog::Find(TEXT("Card.Hero.GuiyuanArt"))->TargetSpec.Mode,
        EGameXXKTargetSelectionMode::SingleAlly);
    TestNull(TEXT("unknown cards are rejected"), GameXXKCardCatalog::Find(TEXT("Card.Unknown")));
    return true;
}

#endif
```

Extend this test with a full-catalog semantic pass: every one of the 174 definitions has `TargetSpec.Mode != Invalid`; any effect with `SelectedTarget` has a compatible manual single-target/any-unit spec; `None`/self/all/automatic specs do not accidentally contain selectable-target effects; every hard status/health/terrain filter has a valid range; every soft conditional/consumption/modifier uses a supported data-driven field; and no definition falls through to a card-name branch. Add focused fixtures for `不动如山` (all other allies), `援护步` (lowest-health other ally), `合击令` (each living party member attacks the selected enemy), `摄灵火`/`伏虎断江` (bounded status consumption), and `熊罴皮甲`/`一诺千金` (persistent trigger modifiers).

- [ ] **Step 2: Cold-build the red test.**

Run:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
```

Expected result: build fails because `GameXXKCardCatalog.h` and the card types are absent; do not change unrelated compiler errors.

- [ ] **Step 3: Define the exact public type contract.**

Create `GameXXKCardTypes.h` with the following serialized vocabulary. Keep numeric values explicit and append new enum members only; all display text stays in the catalogue.

```cpp
#pragma once

#include "CoreMinimal.h"
#include "GameXXKCardTypes.generated.h"

UENUM(BlueprintType)
enum class EGameXXKCardSource : uint8 { Hero, Partner, TaskNpc, Route };
UENUM(BlueprintType)
enum class EGameXXKTargetSelectionMode : uint8
{
    Invalid, None, Self, SingleEnemy, SingleAlly, OtherAlly, AllEnemies, AllAllies,
    RandomEnemy, LowestHealthAlly, AnyLivingUnit
};
UENUM(BlueprintType)
enum class EGameXXKTargetPresentation : uint8
{
    None, SelfSeal, EnemyArrow, AllyArrow, AnyUnitArrow, EnemyGroupRing,
    AllyGroupRing, AutoLock
};
UENUM(BlueprintType)
enum class EGameXXKCardEffectTarget : uint8
{
    SelectedTarget, Owner, AllEnemies, AllAllies, AllOtherAllies, RandomEnemy,
    LowestHealthAlly, LowestHealthOtherAlly, None
};
UENUM(BlueprintType)
enum class EGameXXKTargetSide : uint8 { Party, Enemy, None };
UENUM(BlueprintType)
enum class EGameXXKTerrain : uint8 { Plains, Mountain, Forest, Waterbank, Village, Cave };
UENUM(BlueprintType)
enum class EGameXXKStatusType : uint8 { Armor, Agility, Momentum, Guard, Break, Mark, Bleed, Poison, Burn };
UENUM(BlueprintType)
enum class EGameXXKBattlePhase : uint8 { None, Player, Enemy, Reward };
UENUM(BlueprintType)
enum class EGameXXKCardEffectKind : uint8
{
    DamagePercent, Heal, GainInnerPower, GainArmor, AddStatus, RemoveDot,
    Draw, DrawThenDiscard, Insight, ConsumeStatus, SetNextAttackBonus, SetNextCardEnergyDiscount,
    RevealIntent, ModifyTerrainMultiplier, AttackAllEnemies, HealAllAllies,
    ApplyGuard, EachLivingAllyAttackSelectedTarget, AddCombatModifier
};

UENUM(BlueprintType)
enum class EGameXXKPendingCardChoiceKind : uint8 { None, DiscardFromHand, InsightSelectAndReorder };
UENUM(BlueprintType)
enum class EGameXXKCombatModifierTrigger : uint8
{
    NextAttack, NextCardsEnergyDiscount, FirstDirectHitRetaliate,
    NextTerrainCardFree, NextTerrainEffectMultiplier
};

USTRUCT(BlueprintType)
struct FGameXXKStatusRequirement
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKStatusType Type = EGameXXKStatusType::Armor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinimumStacks = 1;
};

USTRUCT(BlueprintType)
struct FGameXXKTargetSpec
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTargetSelectionMode Mode = EGameXXKTargetSelectionMode::Invalid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTargetMustBeLiving = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bAllowOwner = true;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKStatusRequirement> RequiredStatuses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EGameXXKStatusType> ForbiddenStatuses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MinHealthPercent = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaxHealthPercent = 100;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresTerrain = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTerrain RequiredTerrain = EGameXXKTerrain::Plains;
};

USTRUCT(BlueprintType)
struct FGameXXKTargetCandidateView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName UnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTargetSide Side = EGameXXKTargetSide::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanSelect = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisabledReason;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsAutoLocked = false;
};

USTRUCT(BlueprintType)
struct FGameXXKCardTargetRequest
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CardInstanceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName SourceUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTargetSelectionMode Mode = EGameXXKTargetSelectionMode::Invalid;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTargetPresentation Presentation = EGameXXKTargetPresentation::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresManualSelection = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKTargetCandidateView> CandidateViews;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText Hint;
};

USTRUCT(BlueprintType)
struct FGameXXKCardPlayPreview
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKCardTargetRequest TargetRequest;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> AutomaticTargetUnitIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bCanPlay = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText FailureReason;
};

USTRUCT(BlueprintType)
struct FGameXXKCardPileSourceCount
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCardSource Source = EGameXXKCardSource::Hero;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKBattlePileView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Count = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardPileSourceCount> SourceCounts;
};

USTRUCT(BlueprintType)
struct FGameXXKDiscardCardView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName InstanceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCardSource Source = EGameXXKCardSource::Hero;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
};

USTRUCT(BlueprintType)
struct FGameXXKEnemyIntentView
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EnemyUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName IntentId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayText;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PreviewDamage = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName TargetUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsNextIntentRevealed = false;
};

USTRUCT(BlueprintType)
struct FGameXXKCombatModifier
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCombatModifierTrigger Trigger = EGameXXKCombatModifierTrigger::NextAttack;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName BoundTargetUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Value = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RemainingTriggers = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresTerrain = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTerrain RequiredTerrain = EGameXXKTerrain::Plains;
};

USTRUCT(BlueprintType)
struct FGameXXKCardEffectCondition
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKStatusRequirement> RequiredSelectedTargetStatuses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKStatusRequirement> RequiredOwnerStatuses;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKStatusType ConsumedSelectedTargetStatus = EGameXXKStatusType::Armor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaximumConsumedSelectedTargetStacks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BonusPercentPerSelectedTargetStack = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaximumBonusStacks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 MaximumSelectedTargetHealthPercent = 100;
};

USTRUCT(BlueprintType)
struct FGameXXKCardEffect
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCardEffectKind Kind = EGameXXKCardEffectKind::DamagePercent;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCardEffectTarget Target = EGameXXKCardEffectTarget::SelectedTarget;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKStatusType Status = EGameXXKStatusType::Armor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 FlatValue = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 PercentValue = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTerrain RequiredTerrain = EGameXXKTerrain::Plains;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresTerrain = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKCardEffectCondition Condition;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKCombatModifier Modifier;
};

USTRUCT(BlueprintType)
struct FGameXXKCardDef
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKCardSource Source = EGameXXKCardSource::Hero;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RoleOrNpcId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 EnergyCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InnerPowerCost = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKTargetSpec TargetSpec;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardEffect> Effects;
};

USTRUCT(BlueprintType)
struct FGameXXKCardInstance
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName InstanceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTemporaryRouteCard = false;
};

USTRUCT(BlueprintType)
struct FGameXXKPendingCardChoice
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKPendingCardChoiceKind Kind = EGameXXKPendingCardChoiceKind::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequiredDiscardCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RequiredHandPickCount = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> EligibleInstanceIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardInstance> InsightTopCards;
};

USTRUCT(BlueprintType)
struct FGameXXKStatusStack
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKStatusType Type = EGameXXKStatusType::Armor;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Stacks = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName GuardedUnitId = NAME_None;
};

USTRUCT(BlueprintType)
struct FGameXXKBattleDeckState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKBattlePhase Phase = EGameXXKBattlePhase::None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 SharedEnergy = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKTerrain Terrain = EGameXXKTerrain::Plains;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 InitialShuffleSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CurrentRandomSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardInstance> DrawPile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardInstance> DiscardPile;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCardInstance> Hand;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKPendingCardChoice PendingChoice;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKCombatModifier> CombatModifiers;
};
```

`GameXXKCardCatalog.h` must expose only read-only lookup operations:

```cpp
namespace GameXXKCardCatalog
{
    const TArray<FGameXXKCardDef>& GetAll();
    const FGameXXKCardDef* Find(FName CardId);
    int32 CountBySource(EGameXXKCardSource Source);
}
```

- [ ] **Step 4: Implement the immutable catalogue from the approved specification.**

In `GameXXKCardCatalog.cpp`, build a single `static const TArray<FGameXXKCardDef>` through a local `MakeCard` helper. Encode every row of design specification §§9–11 exactly once, including all fees, target type, effects, NPC identity and terrain conditions. Do not make display name or numerical balance a UI concern.

Use this helper pattern for every entry; the examples below lock the old formulas into the new data representation.

```cpp
static FGameXXKCardDef MakeCard(
    FName Id, const TCHAR* Name, EGameXXKCardSource Source, FName Owner,
    int32 Energy, int32 InnerPower, const FGameXXKTargetSpec& TargetSpec,
    TArray<FGameXXKCardEffect> Effects)
{
    FGameXXKCardDef Card;
    Card.Id = Id;
    Card.DisplayName = FText::FromString(Name);
    Card.Source = Source;
    Card.RoleOrNpcId = Owner;
    Card.EnergyCost = Energy;
    Card.InnerPowerCost = InnerPower;
    Card.TargetSpec = TargetSpec;
    Card.Effects = MoveTemp(Effects);
    return Card;
}

static FGameXXKCardEffect Damage(int32 Percent, int32 Flat = 0)
{
    FGameXXKCardEffect Effect;
    Effect.Kind = EGameXXKCardEffectKind::DamagePercent;
    Effect.Target = EGameXXKCardEffectTarget::SelectedTarget;
    Effect.PercentValue = Percent;
    Effect.FlatValue = Flat;
    return Effect;
}

static FGameXXKTargetSpec SingleEnemyTarget()
{
    FGameXXKTargetSpec Target;
    Target.Mode = EGameXXKTargetSelectionMode::SingleEnemy;
    return Target;
}

static FGameXXKTargetSpec SingleAllyTarget()
{
    FGameXXKTargetSpec Target;
    Target.Mode = EGameXXKTargetSelectionMode::SingleAlly;
    Target.bAllowOwner = true;
    return Target;
}

static const TArray<FGameXXKCardDef> Cards = {
    MakeCard(TEXT("Card.Hero.CraneWingSlash"), TEXT("鹤羽斩"), EGameXXKCardSource::Hero,
        TEXT("Hero"), 2, 8, SingleEnemyTarget(), { Damage(160, 6) }),
    MakeCard(TEXT("Card.Hero.GuiyuanArt"), TEXT("归元术"), EGameXXKCardSource::Hero,
        TEXT("Hero"), 2, 10, SingleAllyTarget(),
        { { EGameXXKCardEffectKind::Heal, EGameXXKCardEffectTarget::SelectedTarget, EGameXXKStatusType::Armor, 36 } }),
};
```

For multi-step cards, add multiple `FGameXXKCardEffect` entries in exact textual order; do not branch on localized names. Build an internal `TMap<FName, int32>` once and reject duplicate IDs with `checkf` in non-shipping builds.

- [ ] **Step 5: Build and run the catalogue test green.**

Run the cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CardCatalog;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: one `GameXXK.PartyDeck.CardCatalog` success, with four source counts `12/108/24/30`.

- [ ] **Step 6: Commit only the new catalogue files and test if no pre-existing file is staged.**

```powershell
git add -- Source/GameXXK/Public/GameXXKCardTypes.h Source/GameXXK/Public/GameXXKCardCatalog.h Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp
git diff --cached --check
git commit -m "feat: add party deck card catalogue"
```

### Task 2: Implement deterministic deck handling and global keyword rules

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCardRules.h`
- Create: `Source/GameXXK/Private/GameXXKCardRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCardRulesTest.cpp`

- [ ] **Step 1: Write red tests for draw, reshuffle, caps and keyword timing.**

Create a test that constructs 18 unique temporary instances, initializes a deck with saved seed `1207`, and asserts the following contract:

```cpp
TestEqual(TEXT("18 instances start as 5 hand / 13 draw / 0 discard"), Deck.Hand.Num(), 5);
TestEqual(TEXT("18 instances leave thirteen in draw pile"), Deck.DrawPile.Num(), 13);
TestEqual(TEXT("battle starts with no discarded cards"), Deck.DiscardPile.Num(), 0);
TestEqual(TEXT("battle starts at three shared energy"), Deck.SharedEnergy, 3);
TestEqual(TEXT("normal round refill target stays five"), Deck.HandLimit, 5);
FGameXXKBattleDeckState CapacityDeck;
TestTrue(TEXT("25 instances initialize for hard-cap coverage"), GameXXKCardRules::InitializeBattleDeck(CapacityDeck, MakeInstances(25), 1208));
TestTrue(TEXT("effect draws fill the twenty-card hard capacity"), GameXXKCardRules::DrawCards(CapacityDeck, 15, 0));
TestEqual(TEXT("battle hand hard capacity is twenty"), CapacityDeck.Hand.Num(), 20);
TestTrue(TEXT("overflow resolves without consuming cards"), GameXXKCardRules::DrawCards(CapacityDeck, 3, 0));
TestEqual(TEXT("five undrawn cards remain in the shuffled draw pile"), CapacityDeck.DrawPile.Num(), 5);
FGameXXKBattleDeckState DrawDiscardDeck;
TestTrue(TEXT("draw-discard deck initializes"), GameXXKCardRules::InitializeBattleDeck(DrawDiscardDeck, Instances, 1209));
TestTrue(TEXT("played card frees a hand slot"), GameXXKCardRules::MoveHandCardToDiscard(DrawDiscardDeck, DrawDiscardDeck.Hand.Last().InstanceId));
TestTrue(TEXT("draw-two-discard-one declares exactly one discard"), GameXXKCardRules::DrawCards(DrawDiscardDeck, 2, 1));
TestEqual(TEXT("draw-two-discard-one exposes six cards from a four-card post-play hand"), DrawDiscardDeck.Hand.Num(), 6);
TestEqual(TEXT("one chosen discard is required"), DrawDiscardDeck.PendingChoice.RequiredDiscardCount, 1);
TestTrue(TEXT("submit forced discard returns hand to five"), GameXXKCardRules::SubmitForcedDiscard(DrawDiscardDeck, { DrawDiscardDeck.Hand.Last().InstanceId }));
TestEqual(TEXT("this draw-two/discard-one fixture returns to the round-refill target"), DrawDiscardDeck.Hand.Num(), 5);
FGameXXKBattleDeckState ShortDrawDeck;
TArray<FGameXXKCardInstance> ShortInstances = Instances;
ShortInstances.SetNum(5);
TestTrue(TEXT("short draw deck initializes"), GameXXKCardRules::InitializeBattleDeck(ShortDrawDeck, ShortInstances, 1210));
TestTrue(TEXT("short draw fixture frees one slot"), GameXXKCardRules::MoveHandCardToDiscard(ShortDrawDeck, ShortDrawDeck.Hand.Last().InstanceId));
TestTrue(TEXT("short draw still resolves its declared discard"), GameXXKCardRules::DrawCards(ShortDrawDeck, 2, 1));
TestEqual(TEXT("short draw retains the explicitly declared discard count"), ShortDrawDeck.PendingChoice.RequiredDiscardCount, 1);
FGameXXKBattleDeckState InsightDeck;
TestTrue(TEXT("insight deck initializes"), GameXXKCardRules::InitializeBattleDeck(InsightDeck, Instances, 1208));
TestTrue(TEXT("played insight card frees one hand slot"), GameXXKCardRules::MoveHandCardToDiscard(InsightDeck, InsightDeck.Hand.Last().InstanceId));
TestTrue(TEXT("insight opens a three-card choose-and-reorder preview"), GameXXKCardRules::BeginInsight(InsightDeck, 3));
TestEqual(TEXT("insight exposes only three top cards"), InsightDeck.PendingChoice.Candidates.Num(), 3);
TestEqual(TEXT("insight requires one card to hand"), InsightDeck.PendingChoice.RequiredHandPickCount, 1);
TestTrue(TEXT("insight selection moves one top card into hand and reorders the rest"),
    GameXXKCardRules::SubmitInsightChoice(InsightDeck, InsightDeck.PendingChoice.Candidates[1].InstanceId,
        { InsightDeck.PendingChoice.Candidates[2].InstanceId, InsightDeck.PendingChoice.Candidates[0].InstanceId }));
TestEqual(TEXT("insight returns hand to five after played card freed a slot"), InsightDeck.Hand.Num(), 5);
```

Add a separate `FGameXXKCardRules` test that snapshots every `InstanceId` across draw/hand/discard before and after each action, asserting each appears exactly once and the total stays 18. It must also assert that a UI-only hand display reorder does not call any rules mutation and therefore cannot alter logical hand/discard order. Finally assert the approved caps/timing: armor ≤99 and clears at its owner’s next phase start; agility ≤2 and absorbs one direct hit; momentum ≤3; break ≤5 and is consumed by next direct hit; mark ≤5 but has no automatic damage; bleed/poison/burn cap at 8 and deal 3/2/3 unblockable damage at the affected side’s phase end.

- [ ] **Step 2: Cold-build the red test.**

Run the Task 1 cold-build command. Expected result: missing `GameXXKCardRules` symbols only.

- [ ] **Step 3: Add pure deck and status APIs.**

Create this public API in `GameXXKCardRules.h`:

```cpp
namespace GameXXKCardRules
{
    bool InitializeBattleDeck(FGameXXKBattleDeckState& Deck, const TArray<FGameXXKCardInstance>& Cards, int32 Seed);
    bool DrawCards(FGameXXKBattleDeckState& Deck, int32 Count, int32 RequiredDiscardCount);
    bool MoveHandCardToDiscard(FGameXXKBattleDeckState& Deck, FName InstanceId);
    bool SubmitForcedDiscard(FGameXXKBattleDeckState& Deck, const TArray<FName>& DiscardedInstanceIds);
    bool BeginInsight(FGameXXKBattleDeckState& Deck, int32 LookCount);
    bool SubmitInsightChoice(FGameXXKBattleDeckState& Deck, FName SelectedTopInstanceId, const TArray<FName>& OrderedRemainingInstanceIds);
    bool CancelInsight(FGameXXKBattleDeckState& Deck);
    bool ValidateDeckState(const FGameXXKBattleDeckState& Deck);
    bool ValidateCardCopies(const TArray<FGameXXKCardInstance>& Cards);
    int32 GetStatusStacks(const FGameXXKBattleRuntimeUnit& Unit, EGameXXKStatusType Type);
    void AddStatus(FGameXXKBattleRuntimeUnit& Unit, EGameXXKStatusType Type, int32 Amount, FName GuardedUnitId = NAME_None);
    int32 ConsumeStatus(FGameXXKBattleRuntimeUnit& Unit, EGameXXKStatusType Type, int32 Maximum);
    int32 ApplyEndOfSideDot(FGameXXKBattleRuntimeUnit& Unit);
    void ClearStatusesAtOwnerPhaseStart(FGameXXKBattleRuntimeUnit& Unit);
}
```

The round transition discards the old hand and refills only to the normal `HandLimit` target of five, reshuffling discard only when the draw pile is exhausted. Effect-driven `DrawCards` is separate: it may grow the hand up to the hard battle capacity of twenty. Persist the initial and current random states in `FGameXXKBattleDeckState`; never use `FMath::Rand`, so save/load and tests remain deterministic.

`DrawCards` draws until the request is satisfied, no card remains available, or the hand reaches twenty. Once the hand is full, every unfulfilled draw consumes, destroys, and discards nothing: all undrawn instances remain represented and the complete remaining draw pile is deterministically shuffled. `RequiredDiscardCount` is explicit—capacity overflow never invents a discard—and a positive value opens the exact `ForcedDiscard` choice. Neither a draw nor its overflow path may shrink or rewrite `ActiveInstanceIds`; the source `RouteCardEntries`/`RouteCardIds` authority lives outside this battle-only API and remains unchanged. `BeginInsight` copies (but does not remove) the first N `DrawPile` cards into `PendingChoice` and opens only below the twenty-card capacity. `SubmitInsightChoice` accepts exactly one selected offered instance and a complete permutation of the remaining offered IDs, moves the selected card to hand, rewrites only the remaining top order, and clears the choice; cancel changes neither top cards nor hand. All transitions call `ValidateDeckState` before commit.

- [ ] **Step 4: Add status fields to battle units and implement their exact effects.**

`FGameXXKBattleRuntimeUnit::Id` is the sole stable unit identity. Do **not** add a second `OwnerId`; `FGameXXKCardInstance::OwnerUnitId` must always reference this existing `Id` (except `RouteLeader`, mapped to `Player` only inside rules). Add:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 BattleSlot = INDEX_NONE;
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 Armor = 0;
UPROPERTY(BlueprintReadWrite, EditAnywhere) TArray<FGameXXKStatusStack> Statuses;
UPROPERTY(BlueprintReadWrite, EditAnywhere) int32 NextAttackPercentBonus = 0;
```

`Armor` becomes authoritative. At battle initialization/old active-battle restore, if `Armor == 0 && Shield > 0`, migrate `Shield` into `Armor`, then synchronize legacy `Shield = Armor` only while old widgets/tests still consume it; card resolution may not independently consume both. `bDefending` becomes a compatibility presentation mirror of its corresponding card/status and cannot reduce damage a second time. Implement direct damage through one helper in `GameXXKCardRules.cpp`. It must consume agility before armor, redirect a single-target hit through guard, apply break’s `+10% per stack` then remove break, consume armor before HP, and call the existing defeat marker after HP changes. DoT must bypass armor/agility/guard. Use persisted `BattleSlot`, then `Id`, for lowest-health ties; never use UI position or an array index as identity.

- [ ] **Step 5: Run the keyword/deck test green.**

Run the cold build and:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CardRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: deck/keyword test succeeds with no zero-cost or hand-cap regression.

- [ ] **Step 6: Commit only new rules/types/test files plus the isolated battle-unit header hunk.**

Review `git diff -- Source/GameXXK/Public/GameXXKMVPRules.h` before staging. If it contains unrelated current user work, stage no mixed file and record the task as uncommitted until that overlap is reconciled; do not stage another agent’s changes.

### Task 3: Add card play, player/end phase, and enemy intent execution

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardRulesTest.cpp`

- [ ] **Step 1: Add red phase tests before changing the old action loop.**

Extend `GameXXKCardRulesTest.cpp` with a route battle state and assert target preparation, play and phase timing:

```cpp
FGameXXKCardTargetRequest TargetRequest;
TestTrue(TEXT("Crane produces a manual target request"), UGameXXKMVPRules::GetBattleCardTargetRequest(State, HandInstanceId, TargetRequest));
TestTrue(TEXT("Crane requires one manual target"), TargetRequest.bRequiresManualSelection);
const FGameXXKTargetCandidateView* EnemyCandidate = TargetRequest.CandidateViews.FindByPredicate([](const FGameXXKTargetCandidateView& View) { return View.bCanSelect; });
TestNotNull(TEXT("Crane exposes a legal enemy candidate"), EnemyCandidate);
const FName EnemyUnitId = EnemyCandidate ? EnemyCandidate->UnitId : NAME_None;
TestEqual(TEXT("Crane exposes the living enemy by stable UnitId"), EnemyUnitId, State.ActiveBattleEnemies[0].Id);
TestTrue(TEXT("requesting a target has no battle mutation"), UGameXXKMVPRules::GetBattleHand(State).ContainsByPredicate([HandInstanceId](const FGameXXKCardInstance& Card) { return Card.InstanceId == HandInstanceId; }));
TestTrue(TEXT("playing a valid card does not run enemy phase"), UGameXXKMVPRules::PlayBattleCard(State, HandInstanceId, EnemyUnitId));
TestEqual(TEXT("enemy HP changes before ending the phase"), State.ActiveBattleEnemies[0].HP, EnemyHPBefore - ExpectedDamage);
TestEqual(TEXT("enemy has not attacked after one card"), State.PlayerHP, HeroHPBefore);
TestTrue(TEXT("player may play a second card in same phase"), UGameXXKMVPRules::PlayBattleCard(State, SecondHandInstanceId, NAME_None));
TestTrue(TEXT("only EndBattlePlayerPhase executes enemy intent"), UGameXXKMVPRules::EndBattlePlayerPhase(State));
TestTrue(TEXT("enemy phase changes hero HP once"), State.PlayerHP < HeroHPBefore);
```

Add target-contract cases for `Self`, `SingleAlly`, `OtherAlly`, `AllEnemies`, `AllAllies`, `RandomEnemy`, `LowestHealthAlly` and `AnyLivingUnit`: only manual modes return a non-empty candidate list; all candidate IDs are living and relation-valid; invalid side/index/UnitId returns false with the hand, energy, MP and zones unchanged; automatic targets are deterministic after save/load; and a marked-target damage bonus does not remove unmarked enemies from the candidate list unless the definition's `TargetSpec.RequiredStatuses` explicitly requires mark.

- [ ] **Step 2: Add the facade/subsystem declarations.**

Add these methods to `UGameXXKMVPRules` and matching one-line `UGameXXKMVPSubsystem` wrappers:

```cpp
static bool GetBattleCardTargetRequest(const FGameXXKRuntimeState& State, FName CardInstanceId, FGameXXKCardTargetRequest& OutRequest);
static bool BuildBattleCardPlayPreview(const FGameXXKRuntimeState& State, FName CardInstanceId, FGameXXKCardPlayPreview& OutPreview);
static bool PlayBattleCard(FGameXXKRuntimeState& State, FName CardInstanceId, FName TargetUnitId = NAME_None);
static bool SubmitBattlePendingDiscards(FGameXXKRuntimeState& State, const TArray<FName>& DiscardedInstanceIds);
static bool CommitBattleInsightChoice(FGameXXKRuntimeState& State, FName SelectedTopInstanceId, const TArray<FName>& OrderedRemainingInstanceIds);
static bool CancelBattlePendingChoice(FGameXXKRuntimeState& State);
static bool EndBattlePlayerPhase(FGameXXKRuntimeState& State);
static TArray<FGameXXKCardInstance> GetBattleHand(const FGameXXKRuntimeState& State);
static FGameXXKBattlePileView GetBattleDrawPileView(const FGameXXKRuntimeState& State);
static TArray<FGameXXKDiscardCardView> GetBattleDiscardViews(const FGameXXKRuntimeState& State);
static TArray<FGameXXKEnemyIntentView> GetVisibleEnemyIntents(const FGameXXKRuntimeState& State);
```

Add `FGameXXKBattleDeckState ActiveBattleDeck` and a battle random seed/current-stream state to `FGameXXKRuntimeState`. Add `FGameXXKBattlePileView` (draw count + source/owner aggregate counts, no draw-order IDs) and `FGameXXKDiscardCardView` (discarded card view ordered newest first) for safe read-only inspection. `FGameXXKEnemyIntentView` contains enemy unit ID, intent ID, display text, preview damage, target unit ID and whether it is the next hidden/revealed intent. `UGameXXKMVPSubsystem` exposes the same methods and never accepts UI array positions as authoritative targets.

- [ ] **Step 3: Implement play validation and data-driven effects.**

`GetBattleCardTargetRequest` must evaluate `TargetSpec` entirely in rules code and return source UnitId, target presentation, every relevant candidate's stable UnitId/side/selectability/disabled reason, whether input is required and an explicit reason if no legal candidate exists. It cannot mutate hand, resources, draw/discard piles or random state. `BuildBattleCardPlayPreview` repeats that evaluation using a **copy** of the saved random stream and exposes the auto-locked target(s) for `RandomEnemy`/`LowestHealthAlly` without consuming RNG; actual `PlayBattleCard` rebuilds/consumes the real stream exactly once. `PlayBattleCard` must perform these checks in this fixed order: active battle/player phase; no unresolved draw/discard or insight choice; hand instance exists; definition exists; owner exists and is living; enough shared energy after a pending discount; enough owner inner power; requested UnitId is present in a freshly rebuilt target request (or `NAME_None` is correct for an automatic/no-target mode). On failure return `false` with no mutation.

On success resolve any automatic target with the saved random stream/tie-break contract, deduct costs, then move the validated instance from `Hand` to `DiscardPile` **before** resolving its effects. That frees a hand slot for Draw/Insight and makes the just-played card eligible if an empty draw pile immediately reshuffles the discard; no hidden exclusion is allowed. Resolve the `Effects` array in order, open draw/insight pending choices if requested, and leave phase equal to `Player`. Group effects use stable UnitId order. The interaction coordinator must reject End Turn while `ActiveBattleDeck.PendingChoice` is unresolved, but it must never require the UI's arrow coordinate or display-order array.

For a route card whose owner ID is `RouteLeader`, resolve owner stats/inner power against `Player`. For a card whose owner is defeated, reject it; at the next player phase start its cards go to discard. Use `FGameXXKCardEffectKind` dispatch rather than card-name comparisons. `FGameXXKCardEffectTarget::SelectedTarget` resolves against the target chosen under `TargetSpec`; `Owner`, `AllEnemies`, `AllAllies`, `AllOtherAllies`, `RandomEnemy`, `LowestHealthAlly` and `LowestHealthOtherAlly` are explicit data-driven alternatives, never inferred from localized text. `ApplyGuard`, `EachLivingAllyAttackSelectedTarget` and `CombatModifiers` are required for guard linkage, 合击令 and next-card/first-hit effects; per-effect `Condition` carries soft status/stack/health/terrain gates and consumption, while `TargetSpec` alone contains hard target filtering.

- [ ] **Step 4: Replace the legacy immediate-reply loop.**

In `GameXXKMVPRules.cpp`, remove `FinishPlayerBattleAction` as a public behavior. Keep a private compatibility helper only long enough to redirect the old `ExecuteBattleBasicAttack`, `ExecuteBattleCraneWingSlash`, `ExecuteBattleGuiyuanArt`, `ExecuteBattleDefend`, and `ExecuteBattleHealingPowder` functions to their matching card IDs or return false; no old function may call `RunEnemyAI` directly.

`EndBattlePlayerPhase` must:

1. discard remaining hand;
2. apply party-side end-of-phase DoT;
3. stop immediately if the hero is defeated;
4. set phase to `Enemy`;
5. execute each living enemy’s current intent exactly once;
6. apply enemy-side end-of-phase DoT;
7. generate non-repeating next intents, including the tiger’s one half-health counter insertion;
8. detect victory/failure; otherwise clear phase-local armor and start the next player phase with five cards and energy three.

Create intents for Money Rat, Black Bear and Tiger exactly as defined in specification §8. Replace hard-coded Bandit/Wolf/EliteBandit/Boss encounter rows with runtime IDs `MoneyRat`, `BlackBear`, `Tiger`; preserve their public display mapping in a later scene task.

- [ ] **Step 5: Run red/green phase and existing battle tests.**

After the failing assertion is observed, run:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CardRules+GameXXK.MVP.Battle.EncounterRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: both test names succeed; old encounter tests have been rewritten to assert an explicit end-phase rather than instant enemy retaliation.

- [ ] **Step 6: Commit the battle-phase change only after reviewing all modified legacy files.**

Use `git diff --check` and inspect every staged hunk. Do not stage dirty Town HUD, inventory, world-map or unrelated test changes.

### Task 4: Verify core battle compatibility before companion integration

**Files:**

- Modify only if a focused test exposes a defect: files from Tasks 1–3.

- [ ] **Step 1: Run the focused full core suite after a cold build.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.+GameXXK.MVP.Battle.;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: the card catalog/rules and existing battle suite all report success. A failure must create a targeted red regression test before changing implementation.

- [ ] **Step 2: Run a safe PIE smoke cycle.**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
```

Expected result: the pipeline saves dirty packages through MCP if necessary, cold-builds, launches a fresh editor, starts/stops PIE, and reports no failed `[TDD]` assertion.

- [ ] **Step 3: Record that UI work is blocked until this gate is green.**

Do not replace battle buttons or create card art until this plan’s core suite is green; the next plan may use `PlayBattleCard`, `EndBattlePlayerPhase`, `GetBattleHand`, and `GetVisibleEnemyIntents` as stable APIs.
