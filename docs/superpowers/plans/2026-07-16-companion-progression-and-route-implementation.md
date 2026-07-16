# Companion Progression and Route Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add the approved persistent 12-slot permanent-partner system, temporary task NPC selection, deterministic personal decks, save migration, terrain and post-battle route rewards while preserving the existing quest/follower and town-to-route flow.

**Architecture:** Immutable partner templates, task-NPC definitions and deterministic personal-card selection live outside the UI. `FGameXXKRuntimeState` owns only serialized player/run state; `UGameXXKMVPRules` is the state-transition boundary; `UGameXXKMVPSubsystem` is the only UI-facing mutator. The card-runtime plan supplies card definitions, deck state and battle APIs; this plan supplies the party that seeds them and the route lifecycle that awards temporary cards.

**Tech Stack:** Unreal Engine 5.8 C++, USTRUCT/UENUM persistence, `FRandomStream`, Unreal Automation Tests, existing save system, cold UBT builds, project MCP/PIE scripts.

---

## File structure

| File | Responsibility |
| --- | --- |
| Create `Source/GameXXK/Public/GameXXKCompanionTypes.h` | Serializable partner, task-NPC, recruitment, run-deck, terrain and pending-reward types. |
| Create `Source/GameXXK/Public/GameXXKCompanionCatalog.h` / `Private/GameXXKCompanionCatalog.cpp` | Immutable 24 recruitment templates, six named task NPCs, role growth and deterministic personal-card helpers. |
| Create `Source/GameXXK/Public/GameXXKCompanionRules.h` / `Private/GameXXKCompanionRules.cpp` | Pure roster, unlock, XP/star, party/deck build and reward-pool validation logic. |
| Modify `Source/GameXXK/Public/GameXXKMVPRules.h` / `Private/GameXXKMVPRules.cpp` | Attach companion/run state to save, transition route/battle/reward lifecycle, and migrate old saves safely. |
| Modify `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h` / `Private/MVP/GameXXKMVPSubsystem.cpp` | Expose recruit, configure, task-NPC, reward and read-only view operations to UI. |
| Modify `Source/GameXXK/Public/MVP/GameXXKBattleSceneUnitActor.h` / `Private/MVP/GameXXKBattleSceneUnitActor.cpp` | Use Money Rat/Black Bear/Tiger mappings and never show 牛欢 as a combat actor. |
| Create/modify focused tests under `Source/GameXXK/Private/Tests/` | Deterministic catalog, roster, persistence, party/deck, reward, route and scene regression coverage. |

## Task 1: Add immutable companion and task-NPC catalogues

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Create: `Source/GameXXK/Public/GameXXKCompanionCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionCatalogTest.cpp`

- [ ] **Step 1: Write the red catalogue test.**

Create `GameXXKCompanionCatalogTest.cpp` before the headers exist. It must assert the fixed content, rather than inspecting any UI text:

```cpp
const TArray<FGameXXKCompanionTemplateDef>& Templates = GameXXKCompanionCatalog::GetRecruitTemplates();
TestEqual(TEXT("there are exactly 24 recruit templates"), Templates.Num(), 24);
for (EGameXXKPartnerRole Role : { EGameXXKPartnerRole::Blade, EGameXXKPartnerRole::Guard,
    EGameXXKPartnerRole::Medic, EGameXXKPartnerRole::Hunter,
    EGameXXKPartnerRole::Sorcerer, EGameXXKPartnerRole::Formation })
{
    TestEqual(TEXT("each role has four templates"), GameXXKCompanionCatalog::CountTemplates(Role), 4);
}

const TArray<FGameXXKTaskNpcDef>& Npcs = GameXXKCompanionCatalog::GetTaskNpcs();
TestEqual(TEXT("there are exactly six task NPCs"), Npcs.Num(), 6);
TestTrue(TEXT("Tusi leader exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.TusiLeader")) != nullptr);
TestTrue(TEXT("Song Jinbao exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.SongJinbao")) != nullptr);
TestTrue(TEXT("Moon White exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.Yuebai")) != nullptr);
TestTrue(TEXT("Zhou Guangzu exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.ZhouGuangzu")) != nullptr);
TestTrue(TEXT("Jingui exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.Jingui")) != nullptr);
TestTrue(TEXT("Qiongmeier exists"), GameXXKCompanionCatalog::FindTaskNpc(TEXT("Npc.Qionger")) != nullptr);

const TArray<FName> First = GameXXKCompanionCatalog::BuildPersonalCardIds(EGameXXKPartnerRole::Blade, 1207);
const TArray<FName> Second = GameXXKCompanionCatalog::BuildPersonalCardIds(EGameXXKPartnerRole::Blade, 1207);
TestEqual(TEXT("personal deck always has twelve cards"), First.Num(), 12);
TestEqual(TEXT("same seed is stable"), First, Second);
TestEqual(TEXT("first four are the role core cards"), GameXXKCompanionCatalog::CountCoreCards(First), 4);
TestEqual(TEXT("personal deck has no duplicate definition"), Algo::Unique(First), 12);
```

Also test that 牛欢 is absent from task NPC and recruitment catalogues, while `Enemy.MoneyRat`, `Enemy.BlackBear` and `Enemy.Tiger` are not recruitable IDs.

- [ ] **Step 2: Cold-build the red test.**

Run the shared cold build command from the implementation index. Expected result: missing companion headers/catalogue symbols; do not paper over any unrelated error.

- [ ] **Step 3: Define the serialized partner/run contract.**

Create `GameXXKCompanionTypes.h` and include `GameXXKCardTypes.h`. Use stable IDs and raw numeric state only; UI labels remain in the immutable catalogues.

```cpp
UENUM(BlueprintType)
enum class EGameXXKPartnerRole : uint8 { Blade, Guard, Medic, Hunter, Sorcerer, Formation };

UENUM(BlueprintType)
enum class EGameXXKRouteRewardKind : uint8 { Normal, Elite, Boss };

USTRUCT(BlueprintType)
struct FGameXXKCompanionRecord
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName CompanionInstanceId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RecruitTemplateId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PortraitVariantId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NameSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKPartnerRole Role = EGameXXKPartnerRole::Blade;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Level = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Experience = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 Star = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 CardSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> PersonalCardIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SelectedCardIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> EquippedItemIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsNew = true;
};

USTRUCT(BlueprintType)
struct FGameXXKTaskNpcDef
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName Id = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName PassiveId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> FixedCardIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<EGameXXKTerrain> PreferredTerrains;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseHP = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseAttack = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseDefense = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 BaseInnerPower = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKRecruitmentOffer
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OfferId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 OfferSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName RecruitTemplateId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKCompanionRecord PreviewRecord;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bIsDuplicate = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresDismissal = false;
};

USTRUCT(BlueprintType)
struct FGameXXKPendingRouteReward
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RouteNodeId = INDEX_NONE;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) EGameXXKRouteRewardKind Kind = EGameXXKRouteRewardKind::Normal;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 RewardSeed = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> OfferedCardIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bRequiresTemporaryReplacement = false;
};

USTRUCT(BlueprintType)
struct FGameXXKRunDeckEntry
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName EntryId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName DefinitionId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName OwnerUnitId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bTemporaryRouteCard = false;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 AcquisitionOrdinal = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKAdventureRunState
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FName ActiveTaskNpcId = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> SelectedTaskNpcCardIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FGameXXKRunDeckEntry> RunDeck;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) TArray<FName> TemporaryRouteEntryIds;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NextRunDeckEntryOrdinal = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) int32 NextBattleInstanceSerial = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) FGameXXKPendingRouteReward PendingReward;
    UPROPERTY(EditAnywhere, BlueprintReadWrite) bool bHasPendingReward = false;
};
```

`PersonalCardIds` must contain the immutable 12-card result; `UnlockedPersonalCardIds` is derived from level/star on read, not saved as independently mutable history. `SelectedCardIds` is an ordered five-card configuration and must be a subset of currently unlocked personal cards. `RunDeck` is a persistent canonical **recipe** of `FGameXXKRunDeckEntry`, not a duplicate of in-battle draw/hand/discard card instances. Each entry gets a stable `EntryId` from route seed plus `NextRunDeckEntryOrdinal`; each battle materializes it into a separate stable instance ID containing `EntryId` plus `NextBattleInstanceSerial`.

- [ ] **Step 4: Implement immutable definitions and seed rules.**

Implement exactly 24 template IDs (`Partner.Blade.01`…`Partner.Formation.04`), four per role, and exactly six task-NPC IDs named in the test. Each task NPC must reference its precise four `Card.Npc.*` definitions and passive from the approved specification. Do not put 牛欢, Black Bear, Tiger or Money Rat into either table.

`BuildPersonalCardIds(Role, Seed)` must take the four catalogued core cards in fixed catalog order, use a local `FRandomStream(Seed)` to sample eight distinct entries from the remaining 14, and append those sampled entries in generated order. It may never call `FMath::Rand` or query display text. Add helpers for role base/growth values and task-NPC level-scaled stats from specification §5.

- [ ] **Step 5: Build and run green.**

Run a cold build, then:

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CompanionCatalog;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: 24 templates/6 NPCs/seed contract pass with no load-order dependence.

## Task 2: Implement recruitment, growth, unlocks and shared partner backpack rules

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Create: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`

- [ ] **Step 1: Write red roster/progression tests.**

Cover these exact user-visible rules in `GameXXKCompanionRulesTest.cpp`:

```cpp
TestTrue(TEXT("new offer is deterministic and remains pending"), UGameXXKMVPRules::CreateRecruitmentOffer(State, 8001));
const FGameXXKRecruitmentOffer OfferBeforeCancel = State.PendingRecruitmentOffer;
TestTrue(TEXT("cancelling full-roster replacement keeps exact offer"), UGameXXKMVPRules::CancelRecruitmentReplacement(State));
TestEqual(TEXT("cancel cannot reroll offer seed"), State.PendingRecruitmentOffer.OfferSeed, OfferBeforeCancel.OfferSeed);

TestTrue(TEXT("duplicate template becomes one contract seal"), UGameXXKMVPRules::ConfirmRecruitmentOffer(DuplicateState, NAME_None));
TestEqual(TEXT("duplicate adds one seal"), DuplicateState.ContractSeals, SealsBefore + 1);
TestEqual(TEXT("duplicate does not add a second partner"), DuplicateState.CompanionRoster.Num(), 1);

TestFalse(TEXT("thirteenth unique recruit cannot auto-enter roster"), UGameXXKMVPRules::ConfirmRecruitmentOffer(FullState, NAME_None));
TestTrue(TEXT("explicit dismissal replaces exactly one old record"), UGameXXKMVPRules::ConfirmRecruitmentOffer(FullState, DismissedCompanionId));
TestEqual(TEXT("roster remains capped at twelve"), FullState.CompanionRoster.Num(), 12);

TestEqual(TEXT("new partner starts level one star one"), NewRecord.Level, 1);
TestEqual(TEXT("new partner starts with six unlocked cards"), GameXXKCompanionRules::GetUnlockedCardIds(NewRecord).Num(), 6);
TestTrue(TEXT("level four unlocks seventh card"), GameXXKCompanionRules::GetUnlockedCardIds(LevelFourRecord).Contains(LevelFourRecord.PersonalCardIds[6]));
TestTrue(TEXT("four-star unlocks twelfth card"), GameXXKCompanionRules::GetUnlockedCardIds(FourStarLevelTwelveRecord).Contains(FourStarLevelTwelveRecord.PersonalCardIds[11]));
```

Also test the XP formula `40 + 20 × (current level - 1)`, star costs `1/2/3/4`, level 1–20 / star 1–5 bounds, correct refunds (all equipment + invested experience materials, no spent seals), single active permanent partner, partner cards exactly five, and shared partner-backpack items are not copied per companion.

- [ ] **Step 2: Cold-build the red test.**

Run the shared cold build. Expected failure: companion rules and rule APIs do not exist.

- [ ] **Step 3: Implement pure roster state transitions.**

`GameXXKCompanionRules` must expose side-effect-free operations for:

```cpp
bool ValidateCompanionCardSelection(const FGameXXKCompanionRecord& Companion, const TArray<FName>& OrderedCards);
TArray<FName> GetUnlockedCardIds(const FGameXXKCompanionRecord& Companion);
bool GrantExperience(FGameXXKCompanionRecord& Companion, int32 Amount, int32& OutConsumedMaterial);
bool UpgradeStar(FGameXXKCompanionRecord& Companion, int32& InOutContractSeals);
bool BuildRecruitPreview(const FGameXXKRuntimeState& State, int32 OfferSeed, FGameXXKRecruitmentOffer& OutOffer);
bool ConfirmRecruitment(FGameXXKRuntimeState& State, FName DismissedCompanionId);
bool SetActiveCompanion(FGameXXKRuntimeState& State, FName CompanionInstanceId);
```

Generate `CompanionInstanceId`, `NameSeed`, `PortraitVariantId` and `CardSeed` only from the persisted offer seed. If the chosen `RecruitTemplateId` already appears in `CompanionRoster`, set `bIsDuplicate`; confirmation consumes the offer and grants exactly one `ContractSeals` item/state increment. If a unique result would exceed 12, leave the offer pending until a valid non-empty dismissal ID is supplied. Dismissing an active companion sets no active permanent partner until a replacement is explicitly activated; it must return old equipment to `PartnerInventory` and only the tracked XP-material investment, not seals.

- [ ] **Step 4: Add facade/subsystem operations with atomic validation.**

Add state fields only after the tests name the behavior: `CompanionRoster`, `ActiveCompanionInstanceId`, `PartnerInventory`, `ContractSeals`, `PendingRecruitmentOffer`, `bHasPendingRecruitmentOffer`, `HeroUnlockedCardIds`, `HeroSelectedCardIds`, and `AdventureRun`. Add matching `UGameXXKMVPRules`/subsystem methods:

```cpp
bool CreateRecruitmentOffer(FGameXXKRuntimeState& State, int32 OfferSeed);
bool ConfirmRecruitmentOffer(FGameXXKRuntimeState& State, FName DismissedCompanionId);
bool CancelRecruitmentReplacement(FGameXXKRuntimeState& State);
bool SetActiveCompanion(FGameXXKRuntimeState& State, FName CompanionInstanceId);
bool SetCompanionSelectedCards(FGameXXKRuntimeState& State, FName CompanionInstanceId, const TArray<FName>& OrderedCardIds);
bool SetHeroSelectedCards(FGameXXKRuntimeState& State, const TArray<FName>& OrderedCardIds);
bool SetTaskNpcSelectedCards(FGameXXKRuntimeState& State, FName NpcId, const TArray<FName>& OrderedCardIds);
```

All reject invalid IDs/duplicates/locked cards before modifying persistent state. The caller never supplies a random card list or a template-derived duplicate decision.

- [ ] **Step 5: Build and run the roster suite green.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.CompanionRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: roster ownership, deterministic offers, card unlocks and refund boundaries pass.

## Task 3: Save migration and old-mainline compatibility

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionCodexPersistenceTest.cpp`

- [ ] **Step 1: Write red migration tests.**

Construct a version-5 save with quest state, `bFollowerJoined`, quest NPC location, inventory, route fields and existing codex state. Restore it and assert:

```cpp
TestEqual(TEXT("new save version is six"), Saved.SaveVersion, 6);
TestTrue(TEXT("old accepted follower remains accepted"), Restored.bFollowerJoined);
TestEqual(TEXT("old quest NPC location survives"), Restored.QuestNpcLocation, VersionFive.RuntimeState.QuestNpcLocation);
TestEqual(TEXT("old save starts with no fabricated permanent partner"), Restored.CompanionRoster.Num(), 0);
TestEqual(TEXT("old save receives default eight selected hero cards"), Restored.HeroSelectedCardIds.Num(), 8);
TestFalse(TEXT("old follower is not silently converted into permanent partner"), Restored.ActiveCompanionInstanceId != NAME_None);
```

Save a state with 12 companions, a pending full-roster recruit offer, a selected task NPC, temporary route entries and an active battle deck state; reload and assert every stable ID, run-entry ID, battle-instance ID, initial/current random state, draw/hand/discard distribution, pending discard/Insight choice, automatic-target preview, card-zone count and pending reward remains unchanged. The test must continue the same post-load draw/auto-target action and compare it with an un-saved control state, proving no reroll or reshuffle drift.

- [ ] **Step 2: Implement version-six migration.**

Raise `CurrentSaveVersion` from 5 to 6 only after the red test exists. Append the new UPROPERTY fields to `FGameXXKRuntimeState`; `FGameXXKSaveState` continues to own its nested runtime state and legacy mirror fields. In `MakeSaveState` / `RestoreFromSaveState`, make copy direction explicit and run a `MigratePartyDeckState(State, SavedVersion)` after current codex migration.

For saves `< 6`: preserve all existing story/quest/location/inventory fields verbatim; retain `bFollowerJoined` as the existing quest-route gate; initialize an empty permanent roster, no active permanent partner, no active task NPC, the first eight hero cards unlocked/selected, an empty temporary route deck and no pending offer/reward. Do not infer a named task NPC or a companion from the old Guide codex entry. For saves `>= 6`, validate/recover only structurally impossible state (missing catalog CardId, duplicated instance in a card zone) with a logged repair; never reroll companions, offers, rewards, draw piles or task-NPC choices.

- [ ] **Step 3: Run persistence tests green.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.MVP.SaveGame+GameXXK.MVP.Codex.SaveMigration+GameXXK.PartyDeck.CompanionPersistence;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

Expected result: the legacy mainline stays save-compatible and new deterministic run state survives a real save/load.

## Task 4: Build the three-person party, deterministic route deck, terrain and card rewards

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKRouteMapSeedRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKPartyDeckRouteTest.cpp`

- [ ] **Step 1: Write red party/deck and reward tests.**

Test the exact route contract:

```cpp
TestEqual(TEXT("full party has exactly hero partner npc"), UGameXXKMVPRules::BuildPartySnapshot(State).Num(), 3);
TestEqual(TEXT("full entry deck is eighteen"), UGameXXKMVPRules::BuildAdventureRunDeck(State).Num(), 18);
TestEqual(TEXT("canonical run deck starts with eight hero slots"), RunDeck[0].OwnerUnitId, TEXT("Hero"));
TestEqual(TEXT("missing partner and NPC consume unique fallback sequence"), MissingMembersDeck.Num(), 18);
TestEqual(TEXT("route deck has no more than two copies of any definition"), CountMaxCopies(RunDeck), 2);

TestEqual(TEXT("normal reward has three distinct offers"), PendingNormal.OfferedCardIds.Num(), 3);
TestTrue(TEXT("elite reward contains rare offer"), ContainsRare(PendingElite));
TestTrue(TEXT("black bear reward contains black-bear unique when legal"), ContainsId(PendingBear, TEXT("Card.Route.BlackBear.")));
TestTrue(TEXT("victory waits for reward choose or skip"), State.bHasPendingReward);
TestFalse(TEXT("route node not complete before reward resolution"), IsNodeVisited(State, NodeId));
TestTrue(TEXT("skip resolves reward and completes node"), UGameXXKMVPRules::SkipPendingRouteReward(State));
```

Also cover 30-card cap replacement only of temporary route cards, same CardId maximum two, normal/elite/boss fallback when legal candidates are exhausted, random rewards stable across save/load, six terrains assigned to nodes, task NPC's selected 3 of fixed 4, and cleanup after boss/failure leaves permanent partner records untouched while clearing task NPC and temporary route cards.

- [ ] **Step 2: Extend route node and party/run builders.**

Add `Terrain`, `EncounterId` and `RewardSeed` (all serialized) to `FGameXXKRouteMapNode`; route generation assigns only `Plains`, `Mountain`, `Forest`, `Waterbank`, `Village`, or `Cave` deterministically from the route seed. Add the following rules APIs, then wrap them in the subsystem:

```cpp
static TArray<FGameXXKBattleRuntimeUnit> BuildPartySnapshot(const FGameXXKRuntimeState& State);
static TArray<FGameXXKRunDeckEntry> BuildAdventureRunDeck(FGameXXKRuntimeState& State);
static TArray<FGameXXKCardInstance> MaterializeBattleDeck(FGameXXKRuntimeState& State, int32 RouteNodeId);
static bool SetActiveTaskNpc(FGameXXKRuntimeState& State, FName NpcId);
static bool ChoosePendingRouteReward(FGameXXKRuntimeState& State, FName CardId, FName ReplaceTemporaryEntryId = NAME_None);
static bool SkipPendingRouteReward(FGameXXKRuntimeState& State);
```

`BuildPartySnapshot` must create Hero, then at most one active permanent partner, then at most one active task NPC; never append `bFollowerJoined` as a fourth actor. The old follower field remains the narrative/route gate only. NPC level-scaled state uses current hero level but NPCs never gain XP, star or equipment.

`BuildAdventureRunDeck` must write canonical `AdventureRun.RunDeck` entry order exactly as specification §6.5.1: hero selected slots, active partner selected slots, active NPC selected slots, two travel basics, then temporary route cards in acquisition order. It must fill absent 5/3 member slots with the eight non-cycling fallback cards in their specified order, allocate stable `EntryId`s, validate 18 start entries and max two definitions, and never make configuration-slot order affect shuffle probability. `MaterializeBattleDeck` is the only place that creates `FGameXXKCardInstance`s from entries at battle start; it uses the next persisted battle serial and supplies those instances to the card-runtime plan's shuffle/draw system.

- [ ] **Step 3: Gate victory on deterministic pending rewards.**

Refactor `ResolveBattleVictory` so normal/elite/boss victory creates `AdventureRun.PendingReward` and switches to the reward state without marking the route node complete. Construct offers with `FRandomStream(RewardSeed)`:

- Normal: three distinct legal cards, with two generic/terrain candidates plus one candidate matching current terrain.
- Elite: three distinct legal cards with at least one rare candidate.
- Boss: three distinct legal cards including one corresponding Black Bear/Tiger unique if legal; otherwise rare, then generic/terrain fallback.

`ChoosePendingRouteReward` validates the offered CardId, duplicate cap and 30-card rule. At 30 it accepts only a stable `ReplaceTemporaryEntryId` currently in `AdventureRun.TemporaryRouteEntryIds`; hero/partner/NPC entries must be non-interactive/ineligible. Skip clears the pending reward without adding a card. Only either successful choice or skip calls the existing node-completion and continuation path.

- [ ] **Step 4: Apply route lifecycle cleanup and progression awards.**

On normal/elite/Boss victory grant active permanent partner XP `30/60/120`; on task completion grant 30. Unlock the four hero cards on the exact existing milestones in specification §5.5. On `ResolveBossClear`, `FailDungeonToTown`, abandon route or save repair, clear active task NPC, selected NPC cards, `AdventureRun.RunDeck`, temporary route cards, pending reward and active battle card zones; retain permanent roster, card unlocks, active partner, contract seals and partner inventory. Do not clear `bFollowerJoined` or quest NPC location.

- [ ] **Step 5: Run route suite green.**

```powershell
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.Route+GameXXK.MVP.RouteMap.SeedRules+GameXXK.MVP.Battle.EncounterRules;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

## Task 5: Correct encounter identity and scene presentation

**Files:**

- Modify: `Source/GameXXK/Private/MVP/GameXXKBattleSceneUnitActor.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleEncounterRulesTest.cpp`

- [ ] **Step 1: Write red visual-identity tests.**

Assert `Enemy.MoneyRat` maps to the normal enemy presentation, `Enemy.BlackBear` maps to the elite bear presentation, and `Enemy.Tiger` maps to the boss tiger presentation. Assert no combat ID maps to 牛欢 and no party snapshot has more than three actors.

- [ ] **Step 2: Replace legacy mappings without touching tuned art assets.**

Replace `Wolf → 牛欢`, `EliteBandit → Bear`, and `Boss → Tiger` compatibility shortcuts with explicit new runtime IDs. Keep existing authored bear/tiger visual assets and positions intact; only adjust data mapping/labels. 牛欢 remains an event NPC with no recruit/combat presentation.

- [ ] **Step 3: Run scene/encounter regression tests.**

Run the focused automation command from Task 4 plus `GameXXK.MVP.Battle.SceneActor`. Expected result: scene mapping agrees with encounter rules and continues to render exactly three friendly slots at most.

## Task 6: Integration gate before UI work

- [ ] **Step 1: Inspect all modified dirty files before staging.**

For every pre-existing file, run `git diff -- <path>` and stage only the party/deck hunks if they are separable. Never stage unrelated town HUD, inventory, map or user-authored content changes.

- [ ] **Step 2: Cold build and focused integration suite.**

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:/UE5 demo/GameXXK/GameXXK.uproject' -WaitMutex -NoHotReloadFromIDE
& 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'D:\UE5 demo\GameXXK\GameXXK.uproject' -Unattended -NoSplash -NoSound -NullRHI '-ExecCmds=Automation RunTests GameXXK.PartyDeck.+GameXXK.MVP.Codex.+GameXXK.MVP.RouteMap.+GameXXK.MVP.Battle.;Quit' '-TestExit=Automation Test Queue Empty' -log -stdout -FullStdOutLogOutput
```

- [ ] **Step 3: Run a safe playable-flow smoke.**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 5
```

Expected result: the original `L_Main → L_QingshanInn → F quest → route → battle` flow remains available, while the state model is ready for the PSD UI plan. Do not start UI replacement if this gate is red.
