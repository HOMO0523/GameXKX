# Meta Shop, Partner Recruitment, and Unified Character UI Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implement the permanent-currency meta shop, six equipment packs, six profession-specific companion packs, the deterministic 72-name pool, and one unified hero/companion character interface with a 200-slot shared equipment warehouse.

**Architecture:** This plan begins only after the equipment-instance foundation plan is green. Permanent pack progress and companion identity live in save-authoritative structs; `FGameXXKMetaShopRules` performs every purchase on a complete runtime-state copy and commits only after gold, pity, generated instances, names, pending replacement, and warehouse capacity all validate. UMG renders rule-built snapshots through one character-sheet shell, one warehouse panel, one equipment tooltip, and one shop widget; widgets never calculate equipment stats, prices, pity, ownership, or comparison deltas.

**Tech Stack:** Unreal Engine 5.8, C++20/UE reflection, UMG/Slate built in C++, UE Automation Tests, project UE MCP scripts, Python acceptance probes, UBT cold builds without Live Coding or Hot Reload.

---

## Dependencies and authority

**Authoritative specification:** [`docs/superpowers/specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md`](../specs/2026-07-22-meta-equipment-partner-three-chapter-route-design.md), especially sections 2, 4, 5, 11, 12, and 13.1.

**Required predecessor:** [`docs/superpowers/plans/2026-07-22-meta-equipment-foundation.md`](2026-07-22-meta-equipment-foundation.md) must be implemented and passing before Task 1 starts.

## Implementation guardrails

- Work in `D:\UE5 demo\GameXXK` on `main`. Do not create a worktree and do not use UnrealBridge.
- The repository already contains user-owned edits. Before every task run:

```powershell
git status --short
git diff --check
git diff -- Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/UI/GameXXKInventoryWindowWidget.cpp Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp
```

- Treat the existing modifications to companion rules/types, subsystem, player controller, inventory, Town HUD, related tests, and the untracked `GameXXKCompanionRosterWidget.*` as user-owned. Never reset, replace, regenerate, or stage unrelated hunks.
- Prefer the new focused files in this plan. When a dirty existing file must change, make the smallest localized edit, inspect `git diff -- <file>`, stage with `git add -p <file>`, then inspect `git diff --cached` before committing.
- Do not modify character sprites, PaperZD assets, placed levels, camera transforms, HD2D planes, PSD-derived texture tuning, or any user-adjusted binary asset.
- Do not use Live Coding or Hot Reload as verification. `--check-only` is diagnostic only.
- If the editor is running, first save dirty packages through UE MCP. Use the project pipeline for the cold-build boundary:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

  Expected: UE MCP saves dirty packages, the editor closes safely when required, UBT reports `Result: Succeeded`, and the editor relaunches. If UE MCP cannot save, stop without force-closing the editor.
- Use fixed seeds in every name, pack, pity, migration, and save test. Reopening a widget or loading a save must never reroll a result.
- `PlayerGold` is the only currency accepted here. Route merchant code continues using `RouteTravelMoney`; no new meta-shop function may accept or mutate route currency.

## Predecessor API contract

This plan consumes, and does not reimplement, the following plan-one API:

```cpp
// Source/GameXXK/Public/GameXXKMVPRules.h
FGameXXKRuntimeState::EquipmentCollection;
// type: FGameXXKEquipmentCollectionState
// authoritative fields:
// TArray<FGameXXKEquipmentInstance> EquipmentInstances
// TArray<FName> WarehouseInstanceIds
// TMap<FName, FGameXXKEquipmentLoadout> CharacterLoadouts

// Source/GameXXK/Public/GameXXKEquipmentRules.h
static FName HeroCharacterId(); // exactly "Player"
static bool HasWarehouseCapacity(
    const FGameXXKEquipmentCollectionState& Collection,
    int32 RequiredSlots = 1);
static const FGameXXKEquipmentInstance* FindInstance(
    const FGameXXKEquipmentCollectionState& Collection,
    FName InstanceId);
static int32 CountWarehouseItems(
    const FGameXXKEquipmentCollectionState& Collection);
static bool CreateRolledInstance(
    FGameXXKEquipmentCollectionState& InOutCollection,
    const FGameXXKEquipmentCreateRequest& Request,
    FName& OutInstanceId,
    FString* OutError = nullptr);
static FGameXXKEquipmentTransactionResult EquipInstance(
    FGameXXKEquipmentCollectionState& InOutCollection,
    const FGameXXKCompanionRosterState& Roster,
    FName CharacterId,
    EGameXXKEquipmentSlot Slot,
    FName InstanceId);
static FGameXXKEquipmentTransactionResult UnequipInstance(
    FGameXXKEquipmentCollectionState& InOutCollection,
    FName CharacterId,
    EGameXXKEquipmentSlot Slot);
static bool BuildLoadoutSnapshot(
    const FGameXXKEquipmentCollectionState& Collection,
    FName CharacterId,
    const FGameXXKCharacterStats& BareStats,
    FGameXXKEquipmentLoadoutSnapshot& OutSnapshot,
    FString* OutError = nullptr);
static bool BuildTooltipSnapshot(
    const FGameXXKEquipmentCollectionState& Collection,
    FName InstanceId,
    FName CompareCharacterId,
    const FGameXXKCharacterStats& CompareBareStats,
    FGameXXKEquipmentTooltipSnapshot& OutSnapshot,
    FString* OutError = nullptr);

// Source/GameXXK/Public/GameXXKEquipmentEconomyRules.h
static bool Equip(
    FGameXXKRuntimeState& InOutState,
    FName CharacterId,
    EGameXXKEquipmentSlot Slot,
    FName InstanceId,
    FGameXXKEquipmentTransactionResult& OutResult);
static bool Unequip(
    FGameXXKRuntimeState& InOutState,
    FName CharacterId,
    EGameXXKEquipmentSlot Slot,
    FGameXXKEquipmentTransactionResult& OutResult);
static bool EnhanceInstance(
    FGameXXKRuntimeState& InOutState,
    FName InstanceId,
    FGameXXKEquipmentTransactionResult& OutResult);
static bool BeginReforge(
    FGameXXKRuntimeState& InOutState,
    FName InstanceId,
    int32 AffixIndex,
    FGameXXKEquipmentTransactionResult& OutResult);
static bool ResolvePendingReforge(
    FGameXXKRuntimeState& InOutState,
    bool bAccept,
    FGameXXKEquipmentTransactionResult& OutResult);
static bool DismantleBatch(
    FGameXXKRuntimeState& InOutState,
    const TArray<FName>& InstanceIds,
    bool bConfirmedProtected,
    FGameXXKEquipmentTransactionResult& OutResult);

// Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h
bool GetEquipmentWarehouseSnapshot(TArray<FName>& OutOrderedInstanceIds) const;
bool GetEquipmentLoadoutSnapshot(FName CharacterId, FGameXXKEquipmentLoadoutSnapshot& OutSnapshot) const;
bool GetEquipmentTooltipSnapshot(FName InstanceId, FName CompareCharacterId, FGameXXKEquipmentTooltipSnapshot& OutSnapshot) const;
bool EquipEquipmentInstance(FName CharacterId, EGameXXKEquipmentSlot Slot, FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
bool UnequipEquipmentSlot(FName CharacterId, EGameXXKEquipmentSlot Slot, FGameXXKEquipmentTransactionResult& OutResult);
bool EnhanceEquipmentInstance(FName InstanceId, FGameXXKEquipmentTransactionResult& OutResult);
bool BeginEquipmentReforge(FName InstanceId, int32 AffixIndex, FGameXXKEquipmentTransactionResult& OutResult);
bool ResolveEquipmentReforge(bool bAccept, FGameXXKEquipmentTransactionResult& OutResult);
bool DismantleEquipmentInstances(const TArray<FName>& InstanceIds, bool bConfirmedProtected, FGameXXKEquipmentTransactionResult& OutResult);
bool ResolvePendingPermanentCompanionReplacement(
    FName DismissedInstanceId,
    FName ActivePermanentCompanionInstanceIdAfterReplacement,
    FGameXXKEquipmentTransactionResult& OutResult);
bool DiscardPendingPermanentCompanionRecruitment();
bool IsCompanionLoadoutMutationLocked() const;
```

Plan one atomically synchronizes `EquipmentInstances`, `WarehouseInstanceIds`, `CharacterLoadouts`, and each instance's redundant owner fields. Hero loadout key is `FGameXXKEquipmentRules::HeroCharacterId()`; a companion loadout key is `FGameXXKPermanentCompanion::InstanceId`. The subsystem facade resolves `CompareBareStats` internally for the hero/current permanent companion before calling the frozen rule, so plan-two UI must call `GetEquipmentTooltipSnapshot` instead of calling `BuildTooltipSnapshot` itself. Tooltip ViewModels consume plan-one snapshots and never infer ownership by scanning owner fields. The final replacement overload above is the same plan-one equipment-safe companion transaction, not a second plan-two path; it returns `WarehouseFull` and its message while preserving the pending candidate. Replacement, discard, hero/companion loadout commits, and all equipment facade mutations evaluate the route lock before `BeginRuntimeStateMutation` or any write, so rejected calls are byte-identical. All equipment query and mutation UI in this plan consumes the frozen subsystem facade above; it must not reimplement warehouse/loadout/tooltip calculation or wrap economy rules in a second transaction facade. If these declarations or pre-mutation guards are absent after plan one, reconcile plan one first; do not create compatibility equipment state in this plan.

## Shared UE MCP automation command

The pipeline leaves the editor running, so never start `UnrealEditor-Cmd.exe` beside it. For every green/red automation step, set the first line to the exact filter string named by that step and run this complete block against the already-running editor:

```powershell
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.Companion.NameCatalog"
@'
import os
from scripts.ue_mcp_client import UnrealMCPClient

toolset = "AutomationTestToolset.AutomationTestToolset"
filters = [value for value in os.environ["GAMEXXK_AUTOMATION_FILTERS"].split("|") if value]
client = UnrealMCPClient(timeout=60.0)
assert client.connect(), "UE MCP is unavailable"
print(client.call_tool("DiscoverTests", {"bForceRediscover": True}, toolset_name=toolset, timeout=180.0))
for filter_expression in filters:
    print(client.call_tool(
        "RunTestsByFilter",
        {"filterExpression": filter_expression},
        toolset_name=toolset,
        timeout=900.0))
'@ | python -
```

Expected for a green step: every requested filter reports zero failed tests. Expected for a red step after the test compiles: the named new assertions fail for the reason stated in that task. Every compile-red and compile-green boundary uses `ue_tdd_pipeline.py`; it saves through MCP, closes safely, performs UBT, and relaunches only after a successful build.

## File map

### New rule and catalog files

- `Source/GameXXK/Public/GameXXKCompanionNameCatalog.h` — exact 72-name definitions and deterministic unused-name resolver.
- `Source/GameXXK/Private/GameXXKCompanionNameCatalog.cpp` — six profession pools and global validation.
- `Source/GameXXK/Public/GameXXKMetaShopTypes.h` — pack definitions, saved pity/sequence state, preview/result/error types.
- `Source/GameXXK/Public/GameXXKMetaShopCatalog.h` — immutable six equipment-pack and six companion-pack catalog API.
- `Source/GameXXK/Private/GameXXKMetaShopCatalog.cpp` — exact IDs, prices, supported draw counts, set/role mapping.
- `Source/GameXXK/Public/GameXXKMetaShopRules.h` — deterministic preview and validate-copy-commit purchase API.
- `Source/GameXXK/Private/GameXXKMetaShopRules.cpp` — equipment and companion pack transactions.
- `Source/GameXXK/Public/GameXXKCharacterSheetTypes.h` — sheet context/tab, warehouse filter/sort, item/character/shop ViewModels.
- `Source/GameXXK/Public/GameXXKCharacterSheetPresenter.h` — pure construction of sheet, warehouse, comparison, and shop views.
- `Source/GameXXK/Private/GameXXKCharacterSheetPresenter.cpp` — filtering, sorting, portrait/name resolution, and disabled-reason mapping.

### New UI files

- `Source/GameXXK/Public/UI/GameXXKEquipmentTooltipWidget.h`
- `Source/GameXXK/Private/UI/GameXXKEquipmentTooltipWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKEquipmentWarehousePanelWidget.h`
- `Source/GameXXK/Private/UI/GameXXKEquipmentWarehousePanelWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKCharacterSkillPanelWidget.h`
- `Source/GameXXK/Private/UI/GameXXKCharacterSkillPanelWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKCharacterSheetWidget.h`
- `Source/GameXXK/Private/UI/GameXXKCharacterSheetWidget.cpp`
- `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h`
- `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`

### Existing gameplay files to modify

- `Source/GameXXK/Public/GameXXKCompanionTypes.h` — persisted `DisplayNameId`, role-pack progress, and pending-name identity.
- `Source/GameXXK/Public/GameXXKCompanionCatalog.h`
- `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp` — one portrait key per profession.
- `Source/GameXXK/Public/GameXXKCompanionRules.h`
- `Source/GameXXK/Private/GameXXKCompanionRules.cpp` — name assignment/reservation/release and role-specific recruit entry.
- `Source/GameXXK/Public/GameXXKMVPRules.h`
- `Source/GameXXK/Private/GameXXKMVPRules.cpp` — persisted meta-shop state/current-version wiring; no second migration chain.
- `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp` — extend plan one's dispatcher with the deterministic 7→8 stage.
- `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- `Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp` — version-8 serialized object and checksum-safe backup/write helpers.
- `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp` — meta-shop preview/purchase, existing carry/replacement routing, safe slot load, and removal of player-facing free recruitment; consume rather than duplicate plan-one equipment facades.
- `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp` — own/focus sheet and shop; `I/K/T/P`, `C` compatibility alias, Escape cleanup.
- `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp` — route legacy open calls into the unified sheet and remove the free-recruit button path without discarding existing card logic.
- `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- `Source/GameXXK/Private/GameXXKMVPCommandRouter.cpp` — make all town character/backpack/partner/shop buttons use the new controller entry points.

### New and extended tests

- `Source/GameXXK/Private/Tests/GameXXKCompanionNameCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMetaShopCatalogTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMetaShopEquipmentPackTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMetaShopCompanionPackTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMetaShopAtomicityTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCharacterSheetPresenterTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentWarehouseWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKEquipmentTooltipWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCharacterSheetWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`
- `Source/GameXXK/Private/Tests/GameXXKCharacterSheetPlayerFlowTest.cpp`
- Extend plan one's `GameXXKEquipmentSaveMigrationTest.cpp` plus existing companion recruitment, starter companion, inventory, player-flow, and save-game tests.

---

### Task 1: Add the exact deterministic 72-name catalog

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCompanionNameCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKCompanionNameCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCompanionNameCatalogTest.cpp`

- [ ] **Step 1: Write the failing catalog and resolver test**

Create `GameXXKCompanionNameCatalogTest.cpp` with automation name `GameXXK.Data.Companion.NameCatalog`. Assert six supported roles, exactly 12 names per role, exactly 72 globally unique IDs, exact order per role, rejection of `Hero`, and deterministic selection that skips occupied IDs.

```cpp
TestEqual(TEXT("Blade count"), FGameXXKCompanionNameCatalog::GetNamesForRole(EGameXXKCharacterRole::Blade).Num(), 12);
TestEqual(TEXT("all names"), FGameXXKCompanionNameCatalog::GetAllNames().Num(), 72);
TestTrue(TEXT("catalog valid"), FGameXXKCompanionNameCatalog::ValidateCatalog(&Error));

TSet<FName> Occupied{TEXT("沈砺"), TEXT("霍川")};
FName First;
FName Reopened;
TestTrue(TEXT("resolve"), FGameXXKCompanionNameCatalog::ResolveUnusedName(
    EGameXXKCharacterRole::Blade, 9137, Occupied, First, &Error));
TestTrue(TEXT("reopen"), FGameXXKCompanionNameCatalog::ResolveUnusedName(
    EGameXXKCharacterRole::Blade, 9137, Occupied, Reopened, &Error));
TestEqual(TEXT("same seed same name"), Reopened, First);
TestFalse(TEXT("not occupied"), Occupied.Contains(First));
```

- [ ] **Step 2: Run the red build**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: compile fails because `FGameXXKCompanionNameCatalog` does not exist.

- [ ] **Step 3: Declare the complete catalog API**

```cpp
struct GAMEXXK_API FGameXXKCompanionNameDefinition
{
    FName NameId = NAME_None;
    EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
};

class GAMEXXK_API FGameXXKCompanionNameCatalog final
{
public:
    static const TArray<FGameXXKCompanionNameDefinition>& GetAllNames();
    static TArray<FName> GetNamesForRole(EGameXXKCharacterRole Role);
    static bool IsKnownName(FName NameId);
    static bool ResolveUnusedName(
        EGameXXKCharacterRole Role,
        int32 SelectionSeed,
        const TSet<FName>& OccupiedNameIds,
        FName& OutNameId,
        FString* OutError = nullptr);
    static bool ValidateCatalog(FString* OutError = nullptr);
};
```

- [ ] **Step 4: Implement the six exact ordered pools**

Use these arrays verbatim; `ResolveUnusedName` builds the role list minus `OccupiedNameIds`, seeds `FRandomStream` with `SelectionSeed`, and indexes `RandRange(0, Available.Num() - 1)`.

```cpp
auto AddRole = [&Definitions](EGameXXKCharacterRole Role, std::initializer_list<const TCHAR*> Names)
{
    for (const TCHAR* Name : Names)
        Definitions.Add({FName(Name), Role});
};
AddRole(EGameXXKCharacterRole::Blade,
    {TEXT("沈砺"), TEXT("霍川"), TEXT("裴烈"), TEXT("柳锋"), TEXT("陆骁"), TEXT("唐刃"), TEXT("江断岳"), TEXT("魏长风"), TEXT("叶青崖"), TEXT("罗惊鸿"), TEXT("韩照野"), TEXT("楚横刀")});
AddRole(EGameXXKCharacterRole::Guard,
    {TEXT("石岳"), TEXT("程垣"), TEXT("方镇"), TEXT("杜岩"), TEXT("秦垒"), TEXT("郭磐"), TEXT("许定山"), TEXT("卢守义"), TEXT("邵安城"), TEXT("梁厚川"), TEXT("贺重门"), TEXT("赵不移")});
AddRole(EGameXXKCharacterRole::Healer,
    {TEXT("苏岚"), TEXT("白芷"), TEXT("云苓"), TEXT("宁葵"), TEXT("沈慈"), TEXT("温棠"), TEXT("林药"), TEXT("顾清荷"), TEXT("唐知草"), TEXT("叶含香"), TEXT("陆回春"), TEXT("江素问")});
AddRole(EGameXXKCharacterRole::Hunter,
    {TEXT("林隼"), TEXT("燕逐"), TEXT("叶弦"), TEXT("霍翎"), TEXT("杜苍"), TEXT("卫野"), TEXT("罗追月"), TEXT("江听风"), TEXT("秦落羽"), TEXT("楚寻踪"), TEXT("韩鸣镝"), TEXT("许穿林")});
AddRole(EGameXXKCharacterRole::Sorcerer,
    {TEXT("墨玄"), TEXT("洛烛"), TEXT("星澜"), TEXT("祁夜"), TEXT("温符"), TEXT("楚灯"), TEXT("闻幽"), TEXT("段箓"), TEXT("谢灵"), TEXT("顾晦"), TEXT("唐观星"), TEXT("江蜃")});
AddRole(EGameXXKCharacterRole::FormationMaster,
    {TEXT("顾衡"), TEXT("陆枢"), TEXT("方纬"), TEXT("宋策"), TEXT("贺图"), TEXT("罗经"), TEXT("岳阵"), TEXT("齐筹"), TEXT("梁定盘"), TEXT("卫司南"), TEXT("秦布势"), TEXT("许观局")});
```

Store each Chinese display string directly as its stable `FName NameId`; UI later uses `FText::FromName(NameId)`. `ValidateCatalog` rejects invalid roles, empty names, non-12 role counts, and any duplicate across the 72 entries.

- [ ] **Step 5: Run the green test and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.Companion.NameCatalog"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/GameXXKCompanionNameCatalog.h Source/GameXXK/Private/GameXXKCompanionNameCatalog.cpp Source/GameXXK/Private/Tests/GameXXKCompanionNameCatalogTest.cpp
git diff --cached --check
git commit -m "feat: add deterministic companion name catalog"
```

Expected: one automation test passes with 72 unique names.

### Task 2: Persist companion names and unify profession portraits

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCompanionCatalog.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionCatalog.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`

- [ ] **Step 1: Add failing identity, reservation, and release tests**

Add assertions for: new profiles persist `DisplayNameId`; two same-role new templates never share a name; a full-roster pending candidate reserves its name across reopen; duplicate-template sigil outcomes do not reserve a name; discarding/replacing releases the candidate/dismissed name; and every template in one profession resolves the same profession portrait key.

```cpp
TestFalse(TEXT("new recruit named"), Result.Companion.DisplayNameId.IsNone());
TestEqual(TEXT("role portrait"), Result.Companion.PortraitVariantId, FName(TEXT("Portrait.Companion.Blade")));
TestNotEqual(TEXT("same-role names differ"), BladeA.DisplayNameId, BladeB.DisplayNameId);
TestEqual(TEXT("pending name stable"), Reopened.PendingRecruitment.Candidate.DisplayNameId, PendingName);
TestTrue(TEXT("released name can resolve again"), AvailableAfterDiscard.Contains(PendingName));
```

- [ ] **Step 2: Run the targeted test and confirm red**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: UBT compile failure shows absent `DisplayNameId`; this is the intended red result before implementation.

- [ ] **Step 3: Add save-owned display identity**

Add to `FGameXXKPermanentCompanion` immediately after `NameSeed`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FName DisplayNameId = NAME_None;
```

Do not add a parallel occupied-name array. The occupied set is derived from `PermanentCompanions[*].DisplayNameId` plus `PendingRecruitment.Candidate.DisplayNameId`; therefore a discard or replacement releases a name by removing that persisted owner.

- [ ] **Step 4: Add deterministic assignment and profession portrait resolution**

Declare and implement:

```cpp
static FName GetProfessionPortraitKey(EGameXXKCharacterRole Role);
static bool AssignUnusedDisplayName(
    const FGameXXKCompanionRosterState& Roster,
    EGameXXKCharacterRole Role,
    int32 NameSeed,
    FName& OutNameId,
    FString* OutError = nullptr);
```

`GetProfessionPortraitKey` returns exactly `Portrait.Companion.Blade`, `Guard`, `Healer`, `Hunter`, `Sorcerer`, or `FormationMaster`. In `RecruitPermanentCompanion`, first detect duplicate template; duplicate returns one existing `SigilCount` increment and no name. For a new template, derive the name seed from stable text rather than process-local `FName` indexes, resolve against roster plus pending, persist the name on the candidate, and retain it unchanged for `PendingReplacement`.

```cpp
const int32 NameSeed = HashCombine(
    RecruitSeed,
    static_cast<int32>(FCrc::StrCrc32(*RecruitTemplateId.ToString())));
```

- [ ] **Step 5: Run green companion tests and commit isolated hunks**

Expected: all old deterministic card-pool and replacement tests remain green; the new identity tests pass.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.Companion"
# Run the complete shared UE MCP automation block above.
git add -p Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Public/GameXXKCompanionCatalog.h Source/GameXXK/Private/GameXXKCompanionCatalog.cpp Source/GameXXK/Public/GameXXKCompanionRules.h Source/GameXXK/Private/GameXXKCompanionRules.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRulesTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp
git diff --cached --check
git commit -m "feat: persist unique companion names"
```

### Task 3: Define saved meta-shop progress and the exact 12-pack catalog

**Files:**

- Create: `Source/GameXXK/Public/GameXXKMetaShopTypes.h`
- Create: `Source/GameXXK/Public/GameXXKMetaShopCatalog.h`
- Create: `Source/GameXXK/Private/GameXXKMetaShopCatalog.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopCatalogTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`

- [ ] **Step 1: Write the failing catalog/state test**

Create automation test `GameXXK.Data.MetaShop.Catalog`. Assert 12 unique pack IDs, six equipment and six companion packs, exact prices, equipment ten-pull support, companion single-only support, and six distinct role-progress entries.

```cpp
TestEqual(TEXT("pack count"), FGameXXKMetaShopCatalog::GetPacks().Num(), 12);
TestEqual(TEXT("equipment single"), Pojun->SinglePrice, 50);
TestEqual(TEXT("equipment ten"), Pojun->TenPrice, 450);
TestTrue(TEXT("equipment ten allowed"), Pojun->bSupportsTenPull);
TestEqual(TEXT("companion single"), Blade->SinglePrice, 150);
TestFalse(TEXT("companion ten forbidden"), Blade->bSupportsTenPull);
```

- [ ] **Step 2: Run red build**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: compile fails because meta-shop types and catalog do not exist.

- [ ] **Step 3: Add the exact saved and transient types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKMetaShopPackKind : uint8 { Equipment, Companion };

UENUM(BlueprintType)
enum class EGameXXKMetaShopPurchaseError : uint8
{
    None,
    InvalidPack,
    InvalidDrawCount,
    InsufficientPermanentGold,
    EquipmentWarehouseFull,
    PendingCompanionReplacement,
    NoUnusedCompanionName,
    RouteLocked,
    InvalidRuntimeState
};

USTRUCT(BlueprintType)
struct FGameXXKCompanionPackProgress
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) EGameXXKCharacterRole Role = EGameXXKCharacterRole::Invalid;
    UPROPERTY(SaveGame) int32 DrawOrdinal = 0;
    UPROPERTY(SaveGame) int32 ConsecutiveNonNewTemplates = 0;
};

USTRUCT(BlueprintType)
struct FGameXXKMetaShopState
{
    GENERATED_BODY()
    UPROPERTY(SaveGame) int32 SequenceSeed = 0;
    UPROPERTY(SaveGame) int32 EquipmentDrawOrdinal = 0;
    UPROPERTY(SaveGame) int32 ConsecutiveNonEpicEquipment = 0;
    UPROPERTY(SaveGame) TArray<FGameXXKCompanionPackProgress> CompanionPackProgress;
};

USTRUCT(BlueprintType)
struct FGameXXKMetaShopPackDefinition
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName PackId = NAME_None;
    UPROPERTY(BlueprintReadOnly) EGameXXKMetaShopPackKind Kind = EGameXXKMetaShopPackKind::Equipment;
    UPROPERTY(BlueprintReadOnly) EGameXXKEquipmentSet EquipmentSet = EGameXXKEquipmentSet::Legacy;
    UPROPERTY(BlueprintReadOnly) EGameXXKCharacterRole CompanionRole = EGameXXKCharacterRole::Invalid;
    UPROPERTY(BlueprintReadOnly) int32 SinglePrice = 0;
    UPROPERTY(BlueprintReadOnly) int32 TenPrice = 0;
    UPROPERTY(BlueprintReadOnly) bool bSupportsTenPull = false;
};

USTRUCT(BlueprintType)
struct FGameXXKMetaShopPurchasePreview
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName PackId = NAME_None;
    UPROPERTY(BlueprintReadOnly) int32 DrawCount = 0;
    UPROPERTY(BlueprintReadOnly) int32 Price = 0;
    UPROPERTY(BlueprintReadOnly) int32 BalanceBefore = 0;
    UPROPERTY(BlueprintReadOnly) int32 BalanceAfter = 0;
    UPROPERTY(BlueprintReadOnly) int32 MissingGold = 0;
    UPROPERTY(BlueprintReadOnly) bool bCanPurchase = false;
    UPROPERTY(BlueprintReadOnly) EGameXXKMetaShopPurchaseError Error = EGameXXKMetaShopPurchaseError::None;
};

USTRUCT(BlueprintType)
struct FGameXXKMetaShopPurchaseResult
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) bool bSucceeded = false;
    UPROPERTY(BlueprintReadOnly) EGameXXKMetaShopPurchaseError Error = EGameXXKMetaShopPurchaseError::None;
    UPROPERTY(BlueprintReadOnly) int32 BalanceBefore = 0;
    UPROPERTY(BlueprintReadOnly) int32 BalanceAfter = 0;
    UPROPERTY(BlueprintReadOnly) TArray<FName> GeneratedEquipmentInstanceIds;
    UPROPERTY(BlueprintReadOnly) EGameXXKCompanionRecruitOutcome CompanionOutcome = EGameXXKCompanionRecruitOutcome::Invalid;
    UPROPERTY(BlueprintReadOnly) FGameXXKPermanentCompanion Companion;
};

class GAMEXXK_API FGameXXKMetaShopCatalog final
{
public:
    static const TArray<FGameXXKMetaShopPackDefinition>& GetPacks();
    static const FGameXXKMetaShopPackDefinition* Find(FName PackId);
    static bool ValidateCatalog(FString* OutError = nullptr);
};
```

- [ ] **Step 4: Build the exact catalog and add runtime ownership**

Add the save-authoritative field to `FGameXXKRuntimeState`:

```cpp
UPROPERTY(BlueprintReadWrite, EditAnywhere, SaveGame)
FGameXXKMetaShopState MetaShop;
```

Catalog entries are:

```text
MetaShop.Equipment.PoJun    -> EGameXXKEquipmentSet::PoJun    50 / 450
MetaShop.Equipment.XuanJia  -> EGameXXKEquipmentSet::XuanJia  50 / 450
MetaShop.Equipment.QingNang -> EGameXXKEquipmentSet::QingNang 50 / 450
MetaShop.Equipment.ZhuiFeng -> EGameXXKEquipmentSet::ZhuiFeng 50 / 450
MetaShop.Equipment.ShiGu    -> EGameXXKEquipmentSet::ShiGu    50 / 450
MetaShop.Equipment.ShanHe   -> EGameXXKEquipmentSet::ShanHe   50 / 450
MetaShop.Companion.Blade           -> Blade           150
MetaShop.Companion.Guard           -> Guard           150
MetaShop.Companion.Healer          -> Healer          150
MetaShop.Companion.Hunter          -> Hunter          150
MetaShop.Companion.Sorcerer        -> Sorcerer        150
MetaShop.Companion.FormationMaster -> FormationMaster 150
```

Initialize the six `CompanionPackProgress` entries in the same role order as Task 1. Reject duplicate IDs, unknown set IDs, unsupported roles, non-positive prices, or companion packs with ten-pull enabled.

- [ ] **Step 5: Run green catalog test and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.MetaShop.Catalog"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/GameXXKMetaShopTypes.h Source/GameXXK/Public/GameXXKMetaShopCatalog.h Source/GameXXK/Private/GameXXKMetaShopCatalog.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopCatalogTest.cpp
git add -p Source/GameXXK/Public/GameXXKMVPRules.h
git diff --cached --check
git commit -m "feat: define permanent meta shop catalog"
```

Expected: `GameXXK.Data.MetaShop.Catalog` passes with 12 valid offers and six independent role progress records.

### Task 4: Implement deterministic equipment-pack pity and atomic single/ten purchases

**Files:**

- Create: `Source/GameXXK/Public/GameXXKMetaShopRules.h`
- Create: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopEquipmentPackTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopAtomicityTest.cpp`

- [ ] **Step 1: Write failing equipment draw and rollback tests**

Use automation names `GameXXK.Data.MetaShop.EquipmentPack` and `GameXXK.Data.MetaShop.Atomicity`. Cover roll boundaries `0–6999 Common`, `7000–9499 Rare`, `9500–9999 Epic`; 29 consecutive misses force draw 30 Epic; ten Common-quality rolls force draw ten to at least Rare; natural Epic takes priority; six set packs share one miss counter; instance item level equals `PlayerLevel`; insufficient gold, 199/200 ten-pull capacity, generator failure, corrupt collection/roster ownership, and route-locked purchase leave the entire runtime state byte-equivalent. A route-locked preview returns `RouteLocked`, zeroes `bCanPurchase`, and exposes `路线进行中无法使用局外商店`.

```cpp
TestEqual(TEXT("6999 common"), FGameXXKMetaShopRules::ResolveEquipmentQuality(6999, 0, false), EGameXXKEquipmentQuality::Common);
TestEqual(TEXT("7000 rare"), FGameXXKMetaShopRules::ResolveEquipmentQuality(7000, 0, false), EGameXXKEquipmentQuality::Rare);
TestEqual(TEXT("9500 epic"), FGameXXKMetaShopRules::ResolveEquipmentQuality(9500, 0, false), EGameXXKEquipmentQuality::Epic);
TestEqual(TEXT("thirtieth epic"), FGameXXKMetaShopRules::ResolveEquipmentQuality(0, 29, false), EGameXXKEquipmentQuality::Epic);
TestFalse(TEXT("ten pull rejected at 199/200"), FGameXXKMetaShopRules::Purchase(State, PackId, 10, Result, &Error));
TestTrue(TEXT("failed state unchanged"), SerializeRuntimeState(State) == SerializeRuntimeState(Before));
State.CardRun.bLoadoutLockedForRoute = true;
const TArray<uint8> BeforeRouteLockedPurchase = SerializeRuntimeState(State);
TestFalse(TEXT("route purchase rejected"), FGameXXKMetaShopRules::Purchase(State, PackId, 1, Result, &Error));
TestEqual(TEXT("route typed error"), Result.Error, EGameXXKMetaShopPurchaseError::RouteLocked);
TestTrue(TEXT("route failure byte identical"), SerializeRuntimeState(State) == BeforeRouteLockedPurchase);
```

Define the test helper in `GameXXKMetaShopAtomicityTest.cpp` so every reflected field participates in rollback comparison:

```cpp
static TArray<uint8> SerializeRuntimeState(const FGameXXKRuntimeState& State)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    FGameXXKRuntimeState Copy = State;
    FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
    return Bytes;
}
```

- [ ] **Step 2: Run red tests**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: compile fails because `FGameXXKMetaShopRules` is absent.

- [ ] **Step 3: Declare one public preview/purchase surface**

```cpp
class GAMEXXK_API FGameXXKMetaShopRules final
{
public:
    static void InitializeMissingRoleProgress(
        FGameXXKMetaShopState& InOutState);
    static bool ValidateState(
        const FGameXXKMetaShopState& State,
        FString* OutError = nullptr);
    static EGameXXKEquipmentQuality ResolveEquipmentQuality(
        int32 Roll0To9999,
        int32 ConsecutiveNonEpic,
        bool bForceAtLeastRare);
    static FGameXXKMetaShopPurchasePreview PreviewPurchase(
        const FGameXXKRuntimeState& State,
        FName PackId,
        int32 DrawCount);
    static bool Purchase(
        FGameXXKRuntimeState& InOutState,
        FName PackId,
        int32 DrawCount,
        FGameXXKMetaShopPurchaseResult& OutResult,
        FString* OutError = nullptr);
    static FText GetPurchaseErrorText(
        EGameXXKMetaShopPurchaseError Error,
        int32 MissingGold = 0);
};
```

`InitializeMissingRoleProgress` preserves existing counters, inserts only missing supported roles, rejects/removes no saved rows, and emits the canonical `Blade/Guard/Healer/Hunter/Sorcerer/FormationMaster` order. `ValidateState` accepts only a non-zero `SequenceSeed`, non-negative equipment and role draw ordinals, an equipment miss counter in `0..29`, exactly one progress row for each of the six supported roles, and each role miss counter in `0..3`. It rejects duplicates and invalid roles without mutation. `PreviewPurchase` and `Purchase` both call it before exposing or committing a result.

- [ ] **Step 4: Implement equipment purchase as validate-copy-commit**

The implementation order is exact:

```cpp
OutResult = FGameXXKMetaShopPurchaseResult();
OutResult.BalanceBefore = InOutState.PlayerGold;
const bool bRouteLocked = InOutState.Screen != EGameXXKScreen::Town
    || InOutState.CardRun.bLoadoutLockedForRoute
    || InOutState.CardRun.bHasActiveCardBattle;
if (bRouteLocked)
    return ReturnFailure(EGameXXKMetaShopPurchaseError::RouteLocked, 0, OutResult, OutError, TEXT("路线进行中无法使用局外商店"));
const FGameXXKMetaShopPackDefinition* Pack = FGameXXKMetaShopCatalog::Find(PackId);
if (Pack == nullptr)
    return ReturnFailure(EGameXXKMetaShopPurchaseError::InvalidPack, 0, OutResult, OutError, TEXT("无效商品包"));
if (Pack->Kind != EGameXXKMetaShopPackKind::Equipment || (DrawCount != 1 && DrawCount != 10))
    return ReturnFailure(EGameXXKMetaShopPurchaseError::InvalidDrawCount, 0, OutResult, OutError, TEXT("无效抽取数量"));
const int32 Price = DrawCount == 10 ? Pack->TenPrice : Pack->SinglePrice;
if (InOutState.PlayerGold < Price)
    return ReturnFailure(EGameXXKMetaShopPurchaseError::InsufficientPermanentGold, Price - InOutState.PlayerGold, OutResult, OutError, TEXT("永久金币不足"));
if (!FGameXXKEquipmentRules::HasWarehouseCapacity(InOutState.EquipmentCollection, DrawCount))
    return ReturnFailure(EGameXXKMetaShopPurchaseError::EquipmentWarehouseFull, 0, OutResult, OutError, TEXT("装备背包已满"));

FGameXXKRuntimeState Candidate = InOutState;
TArray<FName> GeneratedIds;
bool bAllPreviousDrawsCommon = true;
for (int32 DrawIndex = 0; DrawIndex < DrawCount; ++DrawIndex)
{
    const int32 DrawSeed = HashCombine(Candidate.MetaShop.SequenceSeed, Candidate.MetaShop.EquipmentDrawOrdinal);
    FRandomStream Stream(DrawSeed);
    const bool bTenthMinimumRare = DrawCount == 10 && DrawIndex == 9 && bAllPreviousDrawsCommon;
    const EGameXXKEquipmentQuality Quality = ResolveEquipmentQuality(
        Stream.RandRange(0, 9999), Candidate.MetaShop.ConsecutiveNonEpicEquipment, bTenthMinimumRare);
    FName NewInstanceId;
    FGameXXKEquipmentCreateRequest CreateRequest;
    CreateRequest.Set = Pack->EquipmentSet;
    CreateRequest.Quality = Quality;
    CreateRequest.ItemLevel = Candidate.PlayerLevel;
    CreateRequest.bForceSlot = false;
    CreateRequest.ForcedSlot = EGameXXKEquipmentSlot::Invalid;
    FString RollError;
    if (!FGameXXKEquipmentRules::CreateRolledInstance(
            Candidate.EquipmentCollection, CreateRequest, NewInstanceId, &RollError))
        return ReturnFailure(EGameXXKMetaShopPurchaseError::InvalidRuntimeState, 0, OutResult, OutError, *RollError);
    Candidate.MetaShop.ConsecutiveNonEpicEquipment = Quality == EGameXXKEquipmentQuality::Epic
        ? 0 : Candidate.MetaShop.ConsecutiveNonEpicEquipment + 1;
    ++Candidate.MetaShop.EquipmentDrawOrdinal;
    GeneratedIds.Add(NewInstanceId);
    bAllPreviousDrawsCommon &= Quality == EGameXXKEquipmentQuality::Common;
}
FString ValidationError;
if (!ValidateState(Candidate.MetaShop, &ValidationError)
    || !FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
        Candidate.EquipmentCollection, Candidate.CardRun.CompanionRoster, &ValidationError))
    return ReturnFailure(EGameXXKMetaShopPurchaseError::InvalidRuntimeState, 0, OutResult, OutError, *ValidationError);

Candidate.PlayerGold -= Price;
OutResult.bSucceeded = true;
OutResult.Error = EGameXXKMetaShopPurchaseError::None;
OutResult.BalanceAfter = Candidate.PlayerGold;
OutResult.GeneratedEquipmentInstanceIds = GeneratedIds;
InOutState = MoveTemp(Candidate);
return true;
```

Add a private-file helper with the exact signature below. It populates typed result/error and returns without assigning `Candidate`. Do not debit before every generated instance succeeds.

```cpp
static bool ReturnFailure(
    EGameXXKMetaShopPurchaseError Error,
    int32 MissingGold,
    FGameXXKMetaShopPurchaseResult& OutResult,
    FString* OutError,
    const TCHAR* Message);
```

`PreviewPurchase` performs the identical route-lock predicate before price/capacity checks, returns `RouteLocked`, `bCanPurchase=false`, and the same message. This is the rule-layer authority; subsystem and widgets must still enforce their own boundary checks in Tasks 6 and 10.

- [ ] **Step 5: Run green tests and commit**

Run both `GameXXK.Data.MetaShop.EquipmentPack` and `GameXXK.Data.MetaShop.Atomicity`. Expected: all deterministic boundary, shared-pity, capacity, debit-once, and rollback assertions pass.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.MetaShop.EquipmentPack|StartsWith:GameXXK.Data.MetaShop.Atomicity"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/GameXXKMetaShopRules.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopEquipmentPackTest.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopAtomicityTest.cpp
git diff --cached --check
git commit -m "feat: add atomic equipment pack purchases"
```

### Task 5: Implement profession-specific companion packs and independent protection

**Files:**

- Modify: `Source/GameXXK/Public/GameXXKMetaShopRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMetaShopRules.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopCompanionPackTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCompanionTypes.h`
- Modify: `Source/GameXXK/Public/GameXXKCompanionRules.h`
- Modify: `Source/GameXXK/Private/GameXXKCompanionRules.cpp`

- [ ] **Step 1: Write failing role-pool tests**

Create `GameXXK.Data.MetaShop.CompanionPack`. Cover all six professions, only four matching templates per pool, one 150-gold debit, no ten-pull, independent role miss counters, the fourth pull forced new after three non-new outcomes when an unowned template remains, protection disabled when all four are owned, duplicate gives exactly one existing sigil, each new companion has a saved 12-card personal pool and unique name, and a 12-member roster produces one stable pending candidate. Exhaust all 72 names while leaving an unowned role template, then assert the companion rule reports typed `NoUnusedName`, the shop maps it to `NoUnusedCompanionName`, and no gold/progress/roster/pending field changes. Also corrupt one equipment owner/loadout relation before a companion purchase and assert `InvalidRuntimeState`, unchanged gold/progress/roster/sigil/pending state, and byte-identical complete runtime state.

```cpp
TestFalse(TEXT("companion ten rejected"), FGameXXKMetaShopRules::Purchase(State, BladePack, 10, Result, &Error));
TestEqual(TEXT("single price"), BeforeGold - State.PlayerGold, 150);
TestTrue(TEXT("typed success"), Result.bSucceeded);
TestEqual(TEXT("typed error clear"), Result.Error, EGameXXKMetaShopPurchaseError::None);
TestEqual(TEXT("balance before"), Result.BalanceBefore, BeforeGold);
TestEqual(TEXT("balance after"), Result.BalanceAfter, BeforeGold - 150);
TestEqual(TEXT("role fixed"), Result.Companion.Role, EGameXXKCharacterRole::Blade);
TestEqual(TEXT("personal pool"), Result.Companion.PersonalCardIds.Num(), 12);
TestEqual(TEXT("outcome copied"), Result.CompanionOutcome, EGameXXKCompanionRecruitOutcome::Recruited);
TestTrue(TEXT("recruited payload is exact"),
    SerializeCompanion(Result.Companion) == SerializeCompanion(FindPersistedCompanion(State, Result.Companion.InstanceId)));
TestEqual(TEXT("no equipment payload"), Result.GeneratedEquipmentInstanceIds.Num(), 0);
TestEqual(TEXT("duplicate sigil"), State.CardRun.CompanionRoster.SigilCount, BeforeSigils + 1);
TestEqual(TEXT("fourth protected"), Fourth.Outcome, EGameXXKCompanionRecruitOutcome::Recruited);
```

Repeat the complete result assertions for `DuplicateSigil` and `PendingReplacement`: `bSucceeded=true`, `Error=None`, exact before/after balance, empty equipment IDs, exact copied outcome, and byte-equal copied `Companion` payload from the recruitment result. Compare the recruited payload against the persisted roster entry, the pending payload against the saved pending candidate, and the duplicate payload against the direct role-recruit fixture; do not rely on names/roles alone. For every failed purchase assert `bSucceeded=false`, the exact typed error, `BalanceBefore == BalanceAfter == original gold`, empty generated IDs, `CompanionOutcome=Invalid`, a default companion payload, and byte-identical runtime state. Do not assert only the roster side effect.

- [ ] **Step 2: Run red test**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: equipment purchases may pass, but companion purchases fail because role selection and independent protection are absent.

- [ ] **Step 3: Add a role-specific deterministic recruit entry**

Extend the transient recruitment result with a typed failure; it is not saved and does not alter successful outcome semantics:

```cpp
UENUM(BlueprintType)
enum class EGameXXKCompanionRecruitFailure : uint8
{
    None,
    NoUnusedName,
    InvalidState
};

// Added to FGameXXKCompanionRecruitResult:
UPROPERTY(BlueprintReadOnly, VisibleAnywhere)
EGameXXKCompanionRecruitFailure Failure = EGameXXKCompanionRecruitFailure::None;
```

```cpp
static bool RecruitPermanentCompanionForRolePack(
    FGameXXKCompanionRosterState& InOutRoster,
    EGameXXKCharacterRole Role,
    int32 RecruitSeed,
    bool bForceUnownedTemplate,
    FGameXXKCompanionRecruitResult& OutResult,
    FString* OutError = nullptr);
```

Filter `FGameXXKCompanionCatalog::GetRecruitTemplates()` to the requested role and sort by `TemplateId`. When protected, select only unowned templates; otherwise select all four. Call the existing profile/card/name creation path exactly once. A duplicate returns `DuplicateSigil` without creating or reserving a name. A full roster with a new template returns `PendingReplacement` and persists the candidate, including its name and cards. Exhausted names set `Failure=NoUnusedName`; all other validation failures set `InvalidState`; every success keeps `Failure=None`.

- [ ] **Step 4: Add companion purchase to the same atomic rule**

Before copying, reject an existing `PendingRecruitment`; this prevents purchasing over a saved candidate. On the candidate copy, derive the role draw seed from `SequenceSeed`, the frozen role enum value, and that role's `DrawOrdinal` using stable text CRC rather than `GetTypeHash(FName)`. Set `bForceUnownedTemplate` when `ConsecutiveNonNewTemplates >= 3` and an unowned template exists. After result:

```cpp
const FString StableRoleDrawKey = FString::Printf(
    TEXT("%d|%d|%d"), Candidate.MetaShop.SequenceSeed,
    static_cast<int32>(Pack->CompanionRole), Progress.DrawOrdinal);
const int32 RecruitSeed = static_cast<int32>(FCrc::StrCrc32(*StableRoleDrawKey));

FGameXXKCompanionRecruitResult RecruitResult;
FString RecruitError;
if (!FGameXXKCompanionRules::RecruitPermanentCompanionForRolePack(
        Candidate.CardRun.CompanionRoster, Pack->CompanionRole, RecruitSeed,
        bForceUnownedTemplate, RecruitResult, &RecruitError))
{
    const EGameXXKMetaShopPurchaseError PurchaseError =
        RecruitResult.Failure == EGameXXKCompanionRecruitFailure::NoUnusedName
        ? EGameXXKMetaShopPurchaseError::NoUnusedCompanionName
        : EGameXXKMetaShopPurchaseError::InvalidRuntimeState;
    return ReturnFailure(PurchaseError, 0, OutResult, OutError, *RecruitError);
}

const bool bNewTemplate = RecruitResult.Outcome == EGameXXKCompanionRecruitOutcome::Recruited
    || RecruitResult.Outcome == EGameXXKCompanionRecruitOutcome::PendingReplacement;
Progress.ConsecutiveNonNewTemplates = bNewTemplate
    ? 0 : FMath::Min(3, Progress.ConsecutiveNonNewTemplates + 1);
++Progress.DrawOrdinal;

FString ValidationError;
if (!ValidateState(Candidate.MetaShop, &ValidationError)
    || !FGameXXKEquipmentRules::ValidateCollectionAgainstRoster(
        Candidate.EquipmentCollection, Candidate.CardRun.CompanionRoster, &ValidationError))
    return ReturnFailure(EGameXXKMetaShopPurchaseError::InvalidRuntimeState, 0, OutResult, OutError, *ValidationError);

Candidate.PlayerGold -= 150;
OutResult.bSucceeded = true;
OutResult.Error = EGameXXKMetaShopPurchaseError::None;
OutResult.BalanceBefore = InOutState.PlayerGold;
OutResult.BalanceAfter = Candidate.PlayerGold;
OutResult.GeneratedEquipmentInstanceIds.Reset();
OutResult.CompanionOutcome = RecruitResult.Outcome;
OutResult.Companion = RecruitResult.Companion;
InOutState = MoveTemp(Candidate);
return true;
```

Populate every success field before committing. `Recruited`, `DuplicateSigil`, and `PendingReplacement` all copy the exact `RecruitResult.Outcome` and `RecruitResult.Companion`; only equipment purchases populate `GeneratedEquipmentInstanceIds`. Every equipment and companion purchase must pass `ValidateCollectionAgainstRoster` immediately before debit/commit, including no-op collection branches, so corrupted ownership cannot be hidden by a shop transaction. If name resolution, profile validation, meta-shop validation, or collection/roster validation fails, reset `OutResult` through `ReturnFailure` and do not charge, advance protection, add a sigil, or alter pending state.

- [ ] **Step 5: Run green test and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.MetaShop.CompanionPack"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Private/Tests/GameXXKMetaShopCompanionPackTest.cpp
git add -p Source/GameXXK/Public/GameXXKMetaShopRules.h Source/GameXXK/Private/GameXXKMetaShopRules.cpp Source/GameXXK/Public/GameXXKCompanionTypes.h Source/GameXXK/Public/GameXXKCompanionRules.h Source/GameXXK/Private/GameXXKCompanionRules.cpp
git diff --cached --check
git commit -m "feat: add profession companion packs"
```

Expected: all six role packs, protection sequences, duplicate sigils, pending candidates, names, and rollback checks pass.

### Task 6: Expose shop transactions through the subsystem and close the free-recruit path

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKStarterCompanionTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopSubsystemTest.cpp`

- [ ] **Step 1: Write failing facade and starter-boundary tests**

Test that `StartNewGame` still grants exactly one free random companion on a fresh save; a save already containing any companion receives no extra free companion; player-facing free recruitment rejects without mutation; preview is read-only; purchase mutates once; subsystem errors match rule errors; and Town-only purchase is independently rejected by the subsystem while a route is active. Serialize the complete runtime state before the locked call and require byte-identical failure.

```cpp
const FGameXXKRuntimeState Before = Subsystem->GetRuntimeStateCopy();
TestFalse(TEXT("free click disabled"), Subsystem->StartRandomPermanentCompanionRecruitment(FreeResult));
const FGameXXKRuntimeState AfterFreeClick = Subsystem->GetRuntimeStateCopy();
TestEqual(TEXT("free click gold unchanged"), AfterFreeClick.PlayerGold, Before.PlayerGold);
TestEqual(TEXT("free click roster unchanged"), AfterFreeClick.CardRun.CompanionRoster.PermanentCompanions.Num(), Before.CardRun.CompanionRoster.PermanentCompanions.Num());
TestEqual(TEXT("free click ordinal unchanged"), AfterFreeClick.CardRun.CompanionRoster.RecruitSequenceOrdinal, Before.CardRun.CompanionRoster.RecruitSequenceOrdinal);
TestTrue(TEXT("paid purchase"), Subsystem->PurchaseMetaShopPack(BladePack, 1, PaidResult));
FGameXXKRuntimeState& LockedState = Subsystem->GetMutableRuntimeState();
LockedState.Screen = EGameXXKScreen::DungeonMap;
LockedState.CardRun.bLoadoutLockedForRoute = true;
LockedState.CardRun.bHasActiveCardBattle = false;
const TArray<uint8> BeforeLockedPurchase = SerializeRuntimeState(Subsystem->GetRuntimeStateCopy());
TestFalse(TEXT("subsystem route lock"), Subsystem->PurchaseMetaShopPack(BladePack, 1, PaidResult));
TestEqual(TEXT("subsystem typed route lock"), PaidResult.Error, EGameXXKMetaShopPurchaseError::RouteLocked);
TestTrue(TEXT("subsystem route rollback"), SerializeRuntimeState(Subsystem->GetRuntimeStateCopy()) == BeforeLockedPurchase);
```

- [ ] **Step 2: Run red tests**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: free click still succeeds and no meta-shop facade exists.

- [ ] **Step 3: Add copy-safe preview and mutation facade**

```cpp
UFUNCTION(BlueprintPure, Category="GameXXK|MetaShop")
TArray<FGameXXKMetaShopPackDefinition> GetMetaShopPacks() const;

UFUNCTION(BlueprintPure, Category="GameXXK|MetaShop")
FGameXXKMetaShopPurchasePreview PreviewMetaShopPurchase(FName PackId, int32 DrawCount) const;

UFUNCTION(BlueprintCallable, Category="GameXXK|MetaShop")
bool PurchaseMetaShopPack(FName PackId, int32 DrawCount, FGameXXKMetaShopPurchaseResult& OutResult);
```

`PreviewMetaShopPurchase` delegates to the rule preview, so route state produces a disabled `RouteLocked` preview. `PurchaseMetaShopPack` repeats the Town/route guard before calling `BeginRuntimeStateMutation()`, fills the typed result on rejection, and returns without touching RuntimeState. Only an unlocked call invokes `FGameXXKMetaShopRules::Purchase(RuntimeState, PackId, DrawCount, OutResult, &Error)` and broadcasts/refreshed state on success. It never reads or changes `RouteTravelMoney`.

```cpp
if (RuntimeState.Screen != EGameXXKScreen::Town
    || RuntimeState.CardRun.bLoadoutLockedForRoute
    || RuntimeState.CardRun.bHasActiveCardBattle)
{
    OutResult = FGameXXKMetaShopPurchaseResult();
    OutResult.Error = EGameXXKMetaShopPurchaseError::RouteLocked;
    OutResult.BalanceBefore = RuntimeState.PlayerGold;
    OutResult.BalanceAfter = RuntimeState.PlayerGold;
    return false;
}
BeginRuntimeStateMutation(BattleHudFixtureView);
```

- [ ] **Step 4: Preserve only the new-game starter path**

Keep the existing direct pure-rule call inside `StartNewGame`. After its saved recruit sequence seed exists, initialize the saved meta-shop seed and canonical six role rows exactly once. Mark `StartRandomPermanentCompanionRecruitment` deprecated for Blueprint compatibility and make it return `false` without mutation. Keep `RecruitPermanentCompanionFromSeed` only under its existing test/development purpose; no player-facing widget may call either function.

```cpp
RuntimeState.MetaShop.SequenceSeed = HashCombine(
    RuntimeState.CardRun.CompanionRoster.RecruitSequenceSeed,
    0x4D455441); // "META"
FGameXXKMetaShopRules::InitializeMissingRoleProgress(RuntimeState.MetaShop);

UFUNCTION(BlueprintCallable, Category="GameXXK|Companion",
    meta=(DeprecatedFunction, DeprecationMessage="Permanent companions are obtained from profession packs."))
bool StartRandomPermanentCompanionRecruitment(FGameXXKCompanionRecruitResult& OutResult);

bool UGameXXKMVPSubsystem::StartRandomPermanentCompanionRecruitment(FGameXXKCompanionRecruitResult& OutResult)
{
    OutResult = FGameXXKCompanionRecruitResult();
    return false;
}
```

- [ ] **Step 5: Run green tests and commit isolated hunks**

Expected: fresh starter, old-save no-grant, paid mutation, preview purity, and currency separation pass.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.MetaShop.Subsystem|StartsWith:GameXXK.MVP.StarterCompanion|StartsWith:GameXXK.MVP.Companion.RecruitmentFlow"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Private/Tests/GameXXKMetaShopSubsystemTest.cpp
git add -p Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKStarterCompanionTest.cpp
git diff --cached --check
git commit -m "feat: expose paid meta shop facade"
```

### Task 7: Build one character-sheet and warehouse presentation model

**Files:**

- Create: `Source/GameXXK/Public/GameXXKCharacterSheetTypes.h`
- Create: `Source/GameXXK/Public/GameXXKCharacterSheetPresenter.h`
- Create: `Source/GameXXK/Private/GameXXKCharacterSheetPresenter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCharacterSheetPresenterTest.cpp`

- [ ] **Step 1: Write failing view construction tests**

Cover hero and companion contexts, exact tab ordering, six equipment slots, hero fixed portrait, profession-shared companion portrait, selected character name, equipped-state exclusion from warehouse, set/slot/quality/level filters, four stable sort modes, comparison against selected character's same slot, 200/200 full text, and route-lock disabled reasons.

```cpp
TestEqual(TEXT("tab count"), HeroView.Tabs.Num(), 5);
TestEqual(TEXT("equipment slots"), HeroView.LoadoutSlots.Num(), 6);
TestEqual(TEXT("hero key"), HeroView.CharacterId, FGameXXKEquipmentRules::HeroCharacterId());
TestEqual(TEXT("companion name"), CompanionView.DisplayName, FText::FromName(Companion.DisplayNameId));
TestEqual(TEXT("four sort modes"), StaticEnum<EGameXXKWarehouseSort>()->NumEnums() - 1, 4);
```

- [ ] **Step 2: Run red build**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: compile fails because character-sheet presentation types are absent.

- [ ] **Step 3: Define UI-only types**

```cpp
UENUM(BlueprintType)
enum class EGameXXKCharacterSheetContext : uint8 { Hero, Companion };
UENUM(BlueprintType)
enum class EGameXXKCharacterSheetTab : uint8 { Attributes, Equipment, Skills, Talents, Titles };
UENUM(BlueprintType)
enum class EGameXXKWarehouseSort : uint8 { Newest, LevelDescending, QualityDescending, SetThenSlot };

USTRUCT(BlueprintType)
struct FGameXXKEquipmentWarehouseFilter
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadWrite) FName CharacterId = NAME_None;
    UPROPERTY(BlueprintReadWrite) bool bFilterSet = false;
    UPROPERTY(BlueprintReadWrite) EGameXXKEquipmentSet Set = EGameXXKEquipmentSet::Legacy;
    UPROPERTY(BlueprintReadWrite) bool bFilterSlot = false;
    UPROPERTY(BlueprintReadWrite) EGameXXKEquipmentSlot Slot = EGameXXKEquipmentSlot::Weapon;
    UPROPERTY(BlueprintReadWrite) bool bFilterQuality = false;
    UPROPERTY(BlueprintReadWrite) EGameXXKEquipmentQuality Quality = EGameXXKEquipmentQuality::Common;
    UPROPERTY(BlueprintReadWrite) int32 MinimumItemLevel = 1;
    UPROPERTY(BlueprintReadWrite) int32 MaximumItemLevel = 20;
    UPROPERTY(BlueprintReadWrite) EGameXXKWarehouseSort Sort = EGameXXKWarehouseSort::Newest;
};

UENUM(BlueprintType)
enum class EGameXXKComparisonTone : uint8 { Neutral, Positive, Negative };

USTRUCT(BlueprintType)
struct FGameXXKEquipmentTooltipLineView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FText Label;
    UPROPERTY(BlueprintReadOnly) FText Value;
    UPROPERTY(BlueprintReadOnly) FText Delta;
    UPROPERTY(BlueprintReadOnly) EGameXXKComparisonTone Tone = EGameXXKComparisonTone::Neutral;
};

USTRUCT(BlueprintType)
struct FGameXXKEquipmentTooltipView
{
    GENERATED_BODY()
    UPROPERTY(BlueprintReadOnly) FName InstanceId = NAME_None;
    UPROPERTY(BlueprintReadOnly) FText DisplayName;
    UPROPERTY(BlueprintReadOnly) FText QualityText;
    UPROPERTY(BlueprintReadOnly) FText SlotText;
    UPROPERTY(BlueprintReadOnly) FText SetProgressText;
    UPROPERTY(BlueprintReadOnly) FText ItemLevelText;
    UPROPERTY(BlueprintReadOnly) FText EnhancementText;
    UPROPERTY(BlueprintReadOnly) TArray<FGameXXKEquipmentTooltipLineView> BaseStatLines;
    UPROPERTY(BlueprintReadOnly) TArray<FGameXXKEquipmentTooltipLineView> AffixLines;
    UPROPERTY(BlueprintReadOnly) FText DisabledReason;
};
```

Define `FGameXXKEquipmentWarehouseEntryView`, `FGameXXKCharacterSheetView`, and `FGameXXKMetaShopPackView`. Every equipment entry carries `FGameXXKEquipmentTooltipView`; no widget type contains formulas.

- [ ] **Step 4: Implement presenter functions over the frozen subsystem facade**

```cpp
static bool BuildCharacterSheetView(
    const UGameXXKMVPSubsystem& Subsystem,
    EGameXXKCharacterSheetContext Context,
    FName CompanionInstanceId,
    EGameXXKCharacterSheetTab ActiveTab,
    FGameXXKCharacterSheetView& OutView,
    FString* OutError = nullptr);
static TArray<FGameXXKEquipmentWarehouseEntryView> BuildWarehouseEntries(
    const UGameXXKMVPSubsystem& Subsystem,
    const FGameXXKEquipmentWarehouseFilter& Filter);
static TArray<FGameXXKMetaShopPackView> BuildMetaShopPackViews(
    const UGameXXKMVPSubsystem& Subsystem,
    EGameXXKMetaShopPackKind Kind);
```

The presenter may read roster/name/meta-shop fields from `Subsystem.GetRuntimeState()`, but every equipment read goes through plan one's frozen facade:

```cpp
const FName CharacterId = Context == EGameXXKCharacterSheetContext::Hero
    ? FGameXXKEquipmentRules::HeroCharacterId()
    : Companion->InstanceId;

FGameXXKEquipmentLoadoutSnapshot LoadoutSnapshot;
if (!Subsystem.GetEquipmentLoadoutSnapshot(CharacterId, LoadoutSnapshot))
    return false;

TArray<FName> OrderedWarehouseIds;
if (!Subsystem.GetEquipmentWarehouseSnapshot(OrderedWarehouseIds))
    return false;

FGameXXKEquipmentTooltipSnapshot TooltipSnapshot;
if (!Subsystem.GetEquipmentTooltipSnapshot(InstanceId, CharacterId, TooltipSnapshot))
    return false;
```

The facade internally resolves plan one's new `CompareBareStats` argument and returns deltas for the selected candidate's complete post-replacement character attributes, not raw item-stat differences. Format the immutable facade snapshots into `FGameXXKEquipmentTooltipView`; do not rescan collection arrays or redundant owner fields. `CharacterId` is the comparison target. `Newest` reverses `OrderedWarehouseIds`; quality/level/set sorts use snapshot fields and stable `InstanceId` as the final tie-break. Equipment mutations display `FGameXXKEquipmentTransactionResult::Message` verbatim. The presenter maps only `EGameXXKMetaShopPurchaseError` through `FGameXXKMetaShopRules::GetPurchaseErrorText`; it does not translate equipment error codes or invent alternate equipment messages.

- [ ] **Step 5: Run green presenter tests and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.CharacterSheet.Presenter"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/GameXXKCharacterSheetTypes.h Source/GameXXK/Public/GameXXKCharacterSheetPresenter.h Source/GameXXK/Private/GameXXKCharacterSheetPresenter.cpp Source/GameXXK/Private/Tests/GameXXKCharacterSheetPresenterTest.cpp
git diff --cached --check
git commit -m "feat: add character sheet presentation model"
```

### Task 8: Implement the shared 200-slot warehouse and rich equipment tooltip

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKEquipmentTooltipWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKEquipmentTooltipWidget.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKEquipmentWarehousePanelWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKEquipmentWarehousePanelWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEquipmentTooltipWidgetTest.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKEquipmentWarehouseWidgetTest.cpp`

- [ ] **Step 1: Write failing tooltip and layout tests**

Assert a normal 200-slot logical grid, 4 columns, 50 rows, exactly 10 rows in the clipped viewport, vertical mouse-wheel scroll, visible right scrollbar, no page buttons, full warning text, filter/sort refresh without state mutation, and one tooltip at every filled slot. Add migrated-overflow fixtures with 201 and 237 ordered warehouse IDs: logical slots equal `max(200, actual count)`, row count rounds up by four, the capacity label remains `actual/200`, every overflow ID is reachable by scroll/selection, and confirmed batch dismantle can reduce overflow without hiding an item. Tooltip assertions cover item level, quality, slot, set and 2/4/6 progress, base stats, enhancement level/contribution, every affix type/tier/value, complete post-replacement character-attribute deltas, and disabled reason.

Also assert the shared action strip calls only plan one's frozen subsystem facade. Common `+0` unequipped dismantle succeeds in one call. Any Rare/Epic, enhanced, or equipped selection first returns `ConfirmationRequired` without mutation; confirmation calls `DismantleEquipmentInstances` exactly once more with the complete stable-ID array and `bConfirmedProtected=true`. The confirmed facade transaction may atomically clear equipped slots and dismantle them. A mixed batch failure is byte-identical and never loops per ID. Enhancement, begin-reforge, accept-reforge, and cancel-reforge buttons call `EnhanceEquipmentInstance`, `BeginEquipmentReforge`, and `ResolveEquipmentReforge(true/false)` respectively.

```cpp
TestEqual(TEXT("capacity"), Widget->GetLogicalSlotCountForTest(), 200);
TestEqual(TEXT("columns"), Widget->GetColumnCountForTest(), 4);
TestEqual(TEXT("rows"), Widget->GetTotalRowCountForTest(), 50);
TestEqual(TEXT("visible rows"), Widget->GetVisibleRowCountForTest(), 10);
TestTrue(TEXT("scroll box"), Widget->HasVerticalScrollBoxForTest());
TestFalse(TEXT("no pagination"), Widget->HasPaginationControlsForTest());
TestEqual(TEXT("overflow logical slots"), OverflowWidget->GetLogicalSlotCountForTest(), 237);
TestEqual(TEXT("overflow rows"), OverflowWidget->GetTotalRowCountForTest(), 60);
TestEqual(TEXT("capacity text stays 200"), OverflowWidget->GetCapacityTextForTest().ToString(), TEXT("237/200"));
TestEqual(TEXT("one confirmed batch call"), FacadeSpy->GetDismantleCallCount(), 2);
TestEqual(TEXT("confirmed batch contains every id"), FacadeSpy->GetLastDismantleIds(), SelectedIds);
TestEqual(TEXT("one committed economy batch"), FacadeSpy->GetCommittedDismantleBatchCount(), 1);
```

- [ ] **Step 2: Run red UI tests**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: widget classes are missing.

- [ ] **Step 3: Build the tooltip only from snapshots**

`UGameXXKEquipmentTooltipWidget::SetView(const FGameXXKEquipmentTooltipView&)` creates text rows and positive/negative comparison colors. It must not call catalogs or calculate stats. Use quality colors: Common warm white, Rare muted blue, Epic muted purple. Keep the PSD paper background and low-saturation ink text.

```cpp
void UGameXXKEquipmentTooltipWidget::SetView(const FGameXXKEquipmentTooltipView& InView)
{
    View = InView;
    RebuildHeader(View.DisplayName, View.QualityText, View.ItemLevelText, View.SlotText);
    RebuildSetAndEnhancement(View.SetProgressText, View.EnhancementText);
    RebuildLines(View.BaseStatLines, View.AffixLines);
    DisabledReasonText->SetText(View.DisabledReason);
    DisabledReasonText->SetVisibility(View.DisabledReason.IsEmpty()
        ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}
```

- [ ] **Step 4: Build the normal-capacity and migrated-overflow structure**

```cpp
static constexpr int32 NormalCapacity = 200;
static constexpr int32 ColumnCount = 4;
static constexpr int32 VisibleRows = 10;

TArray<FName> OrderedInstanceIds;
if (!ResolveSubsystem()->GetEquipmentWarehouseSnapshot(OrderedInstanceIds))
    return;
const int32 LogicalSlotCount = FMath::Max(NormalCapacity, OrderedInstanceIds.Num());
const int32 TotalRows = FMath::DivideAndRoundUp(LogicalSlotCount, ColumnCount);
CapacityText->SetText(FText::FromString(FString::Printf(
    TEXT("%d/%d"), OrderedInstanceIds.Num(), NormalCapacity)));

WarehouseScroll = WidgetTree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass());
WarehouseScroll->SetOrientation(EOrientation::Orient_Vertical);
WarehouseScroll->SetAlwaysShowScrollbar(true);
WarehouseGrid = WidgetTree->ConstructWidget<UUniformGridPanel>(UUniformGridPanel::StaticClass());
WarehouseScroll->AddChild(WarehouseGrid);
for (int32 SlotIndex = 0; SlotIndex < LogicalSlotCount; ++SlotIndex)
{
    UButton* SlotButton = BuildEquipmentSlot(SlotIndex);
    WarehouseGrid->AddChildToUniformGrid(SlotButton, SlotIndex / ColumnCount, SlotIndex % ColumnCount);
}
```

Clip the scroll box to ten row extents; do not implement virtual paging. Apply the project ink scrollbar style. Filled slots use high-fill equipment icons and attach `UGameXXKEquipmentTooltipWidget`; empty slots remain hit-test visible only for selection feedback. `bLegacyWarehouseOverflow` never truncates the display: normal acquisition remains blocked by plan-one rules, while all migrated IDs remain selectable for equipment or dismantling until the saved overflow flag clears.

- [ ] **Step 5: Bind every equipment action directly to plan one's frozen facade**

Do not add a plan-two equipment subsystem API or call `FGameXXKEquipmentEconomyRules` from UI. The warehouse and the central six loadout slots share one action controller that invokes only the predecessor facade. Show `OutResult.Message` verbatim on every failure/confirmation; the presenter never remaps equipment errors.

```cpp
void UGameXXKEquipmentWarehousePanelWidget::HandleDismantleSelected()
{
    FGameXXKEquipmentTransactionResult Result;
    if (ResolveSubsystem()->DismantleEquipmentInstances(SelectedInstanceIds, false, Result))
        return RefreshFromFacade();
    if (Result.Error == EGameXXKEquipmentTransactionError::ConfirmationRequired)
        return ShowDismantleConfirmation(SelectedInstanceIds, Result.Message);
    ShowEquipmentMessage(Result.Message);
}

void UGameXXKEquipmentWarehousePanelWidget::ConfirmDismantle(const TArray<FName>& StableIds)
{
    FGameXXKEquipmentTransactionResult Result;
    ResolveSubsystem()->DismantleEquipmentInstances(StableIds, true, Result);
    Result.bSucceeded ? RefreshFromFacade() : ShowEquipmentMessage(Result.Message);
}

void HandleEnhance(FName InstanceId) { CallAndRefresh(&UGameXXKMVPSubsystem::EnhanceEquipmentInstance, InstanceId); }
void HandleBeginReforge(FName InstanceId, int32 AffixIndex) { CallAndRefreshBeginReforge(InstanceId, AffixIndex); }
void HandleAcceptReforge() { CallAndRefreshResolveReforge(true); }
void HandleCancelReforge() { CallAndRefreshResolveReforge(false); }
```

Never iterate selected IDs into repeated dismantle calls. The initial unconfirmed call and, only when required, one confirmed call each receive the full stable selection. Rare/Epic quality, any enhancement level above zero, and any currently equipped ID require confirmation; after confirmation, plan one's one `DismantleBatch` transaction clears affected loadout slots, grants exact materials, synchronizes mirrors, validates roster/collection, and commits or rolls back as a unit. The selected-item panel exposes `强化`, `洗炼`, and the saved pending-reforge preview exposes `接受新词缀` / `保留原词缀`; route locks and resource errors come directly from `Result.Message`.

- [ ] **Step 6: Run green tests and commit**

Expected: normal/overflow structure, scrolling, final-attribute Tooltip deltas, filtering, equip swap, enhancement/reforge flows, protected/equipped confirmation, one-call batch atomicity, and rollback tests pass.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.UI.EquipmentWarehouse|StartsWith:GameXXK.UI.EquipmentTooltip|StartsWith:GameXXK.Equipment.Facade"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/UI/GameXXKEquipmentTooltipWidget.h Source/GameXXK/Private/UI/GameXXKEquipmentTooltipWidget.cpp Source/GameXXK/Public/UI/GameXXKEquipmentWarehousePanelWidget.h Source/GameXXK/Private/UI/GameXXKEquipmentWarehousePanelWidget.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentTooltipWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentWarehouseWidgetTest.cpp
git diff --cached --check
git commit -m "feat: add shared equipment warehouse widget"
```

### Task 9: Build the unified hero/companion character sheet

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKCharacterSkillPanelWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKCharacterSkillPanelWidget.cpp`
- Create: `Source/GameXXK/Public/UI/GameXXKCharacterSheetWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKCharacterSheetWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCharacterSheetWidgetTest.cpp`
- Modify: `Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h`
- Modify: `Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp`

- [ ] **Step 1: Write failing shared-layout tests**

Assert left tabs in order `属性/装备/技能/天赋/称号`; central fixed portrait plus six equipment slots; right content swaps by tab; hero portrait never changes; companion portrait changes only by profession; companion mode has a horizontal selector with at most 12 entries and no duplicate names; Equipment tab embeds the shared warehouse; Skills tab edits hero eight-card or selected companion five-card loadout; Titles displays `尚未获得称号` when no title data exists. Hovering any occupied central loadout slot must open the same rich comparison Tooltip class used by warehouse entries, proving that every equipment appearance shares one presentation path. For hero and selected-companion fixtures, assert the displayed deltas equal the complete final character-stat delta obtained from `GetEquipmentTooltipSnapshot(InstanceId, CharacterId, ...)`; the widget never supplies or reconstructs `CompareBareStats` and never calls `BuildTooltipSnapshot` directly.

Add an active-route fixture for both hero and companion Skills tabs. Every card toggle and the commit button is disabled with exact Tooltip `路线进行中无法编辑技能`. Bypass the widget and call both `SetHeroCardLoadout` and `SetPermanentCompanionCardLoadout`; each backend call must reject, and serialization of the complete runtime state before/after each attempt must be byte-identical. Read-only inspection, scrolling, and Tooltip hover remain usable.

Also assert the carry button states:

```text
selected active companion: 已携带, disabled
selected inactive companion in town: 携带当前伙伴, enabled, confirmation required
route active: 携带当前伙伴, disabled, tooltip 路线进行中无法更换伙伴
```

- [ ] **Step 2: Run red test**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: the unified sheet is absent and current companion roster uses a separate grid layout.

- [ ] **Step 3: Extract one reusable skill panel**

```cpp
void UGameXXKCharacterSkillPanelWidget::SetCharacter(
    EGameXXKCharacterSheetContext Context,
    FName CompanionInstanceId);
bool UGameXXKCharacterSkillPanelWidget::CommitSelection();
```

Hero mode reads unlocked hero cards and commits exactly eight through `SetHeroCardLoadout`. Companion mode reads the selected profile and commits exactly five unlocked cards through `SetPermanentCompanionCardLoadout`. Reuse existing card faces and hover tooltip behavior; do not alter card rules.

Refresh the mutation lock from the subsystem every time the panel opens, the route state changes, or the selected companion changes. The UI guard is presentation only; the existing subsystem methods remain the authoritative second guard.

```cpp
void UGameXXKCharacterSkillPanelWidget::RefreshMutationState()
{
    bReadOnly = ResolveSubsystem()->IsCompanionLoadoutMutationLocked();
    SetEveryCardToggleEnabled(!bReadOnly);
    CommitButton->SetIsEnabled(!bReadOnly);
    CommitButton->SetToolTipText(bReadOnly
        ? FText::FromString(TEXT("路线进行中无法编辑技能"))
        : FText::GetEmpty());
}

bool UGameXXKCharacterSkillPanelWidget::CommitSelection()
{
    RefreshMutationState();
    if (bReadOnly)
        return false;
    return Context == EGameXXKCharacterSheetContext::Hero
        ? ResolveSubsystem()->SetHeroCardLoadout(PendingCardIds)
        : ResolveSubsystem()->SetPermanentCompanionCardLoadout(CompanionInstanceId, PendingCardIds);
}
```

- [ ] **Step 4: Build one full-screen sheet shell**

Use a 1920×1080 safe-area composition with a 240-unit left tab rail, a 700-unit central portrait/loadout region, and the remaining right content region. Place three equipment slots to each side of the portrait. `SetOpenContext(Context, Tab, CompanionInstanceId)` rebuilds the presenter snapshot and does not mutate runtime state.

```cpp
void UGameXXKCharacterSheetWidget::SetOpenContext(
    EGameXXKCharacterSheetContext InContext,
    EGameXXKCharacterSheetTab InTab,
    FName InCompanionInstanceId)
{
    Context = InContext;
    ActiveTab = InTab;
    SelectedCompanionInstanceId = InCompanionInstanceId;
    RefreshFromState();
    SetVisibility(ESlateVisibility::Visible);
}

static constexpr float ReferenceWidth = 1920.0f;
static constexpr float ReferenceHeight = 1080.0f;
static constexpr float TabRailWidth = 240.0f;
static constexpr float PortraitRegionWidth = 700.0f;
```

In companion context, build the top selector as a horizontal `UScrollBox`, ordered by roster order, maximum 12. The bottom carry button calls the existing subsystem confirmation path only after the player confirms. A replacement returns the old companion to the roster; no companion or equipment instance is deleted.

The six central equipment slots and the warehouse reuse the single Task 8 action controller and only plan one's frozen subsystem facade. Equipping, unequipping, enhancing, reforging, and protected batch dismantling never call equipment rules directly from this sheet. Every failure and confirmation renders `FGameXXKEquipmentTransactionResult::Message` verbatim; no sheet/presenter error table translates equipment error codes.

- [ ] **Step 5: Convert the old roster to a compatibility entry, not a second feature implementation**

Retain its test seams and existing card data until all new tests pass. Remove its free-recruit button action and make its player-facing open action delegate to `OpenCompanionCharacterSheet(Equipment)`. Do not wholesale replace the current untracked source file.

```cpp
void UGameXXKCompanionRosterWidget::HandleRecruitClicked()
{
    if (AGameXXKMVPPlayerController* Controller = GetOwningPlayer<AGameXXKMVPPlayerController>())
    {
        Controller->OpenCompanionCharacterSheet(EGameXXKCharacterSheetTab::Equipment);
    }
}
```

- [ ] **Step 6: Run green sheet and legacy roster tests, then commit**

Run `GameXXK.UI.CharacterSheet`, `GameXXK.UI.CompanionRoster`, and `GameXXK.MVP.Companion.RecruitmentFlow`. Expected: new shared layout passes and old companion rule/card tests remain green.

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.UI.CharacterSheet|StartsWith:GameXXK.UI.CompanionRoster|StartsWith:GameXXK.MVP.Companion.RecruitmentFlow"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/UI/GameXXKCharacterSkillPanelWidget.h Source/GameXXK/Private/UI/GameXXKCharacterSkillPanelWidget.cpp Source/GameXXK/Public/UI/GameXXKCharacterSheetWidget.h Source/GameXXK/Private/UI/GameXXKCharacterSheetWidget.cpp Source/GameXXK/Private/Tests/GameXXKCharacterSheetWidgetTest.cpp
git add -p Source/GameXXK/Public/UI/GameXXKCompanionRosterWidget.h Source/GameXXK/Private/UI/GameXXKCompanionRosterWidget.cpp
git diff --cached --check
git commit -m "feat: unify hero and companion character sheets"
```

### Task 10: Build the player-facing meta-shop widget

**Files:**

- Create: `Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h`
- Create: `Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp`

- [ ] **Step 1: Write failing shop widget tests**

Assert Equipment/Companion tabs, six pack cards per tab, permanent-gold total, direct price under every product, direct purchase buttons, one/ten buttons only for equipment, disabled gray buttons with exact tooltip reason, and no charge on disabled click. Generated equipment hover uses the same `UGameXXKEquipmentTooltipWidget` and frozen `GetEquipmentTooltipSnapshot(InstanceId, CharacterId, ...)` path as the warehouse, comparing against the currently selected hero/companion; no shop widget calls `BuildTooltipSnapshot` directly. Assert purchase result views: new companion shows its saved name, profession, and all 12 personal cards; duplicate shows `获得信物 ×1`; full roster opens the persisted replacement flow; closing/reopening never rerolls.

For a full 12-person roster, assert the overlay contains the exact pending-candidate details plus all 12 current companions in roster order as selectable replacement targets. No target selected means confirm is disabled. Selecting a row does not mutate state. Confirm invokes plan one's equipment-safe `ResolvePendingPermanentCompanionReplacement`; discard invokes `DiscardPendingPermanentCompanionRecruitment`; neither path reimplements roster or equipment mutation. Closing the overlay or the entire shop never implicitly discards. Reopening calls `TryGetPendingPermanentCompanionRecruitment` and restores the byte-identical candidate. If equipment return would overflow the warehouse, the resolve attempt fails atomically, displays `装备背包已满`, and preserves the candidate, selected target, roster, loadouts, and collection byte-for-byte. If the dismissed companion was active, the candidate becomes active; otherwise active selection is unchanged.

Add a route-active fixture. Every offer, replacement confirm, and discard button is disabled, and each shows exact Tooltip `路线进行中无法使用局外商店`. Invoke the click handlers anyway and require complete runtime-state serialization to remain byte-identical. The Task 6 direct subsystem rejection and predecessor replacement/discard guards remain the authoritative backend tests.

```cpp
TestEqual(TEXT("equipment offers"), Widget->GetVisibleOfferCountForTest(), 6);
TestEqual(TEXT("single price"), Widget->GetPriceForTest(PojunPack, 1), 50);
TestEqual(TEXT("ten price"), Widget->GetPriceForTest(PojunPack, 10), 450);
TestFalse(TEXT("poor button disabled"), Widget->IsPurchaseEnabledForTest(PojunPack, 1));
TestTrue(TEXT("shortfall tooltip"), Widget->GetDisabledReasonForTest(PojunPack, 1).ToString().Contains(TEXT("还差")));
```

- [ ] **Step 2: Run red widget test**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: `UGameXXKMetaShopWidget` is absent.

- [ ] **Step 3: Build the shop from presenter snapshots**

Use existing PSD paper, rectangular tab, primary/disabled button, role-card art, and plan-one set-icon assets. Preserve source aspect ratios and nine-slice margins; do not stretch tabs/buttons vertically. `RefreshFromState` requests six `FGameXXKMetaShopPackView` values and binds price/enabled/reason without recomputing rules.

```cpp
void UGameXXKMetaShopWidget::RefreshFromState()
{
    const UGameXXKMVPSubsystem* Subsystem = ResolveSubsystem();
    PermanentGoldText->SetText(FText::AsNumber(Subsystem->GetRuntimeState().PlayerGold));
    const TArray<FGameXXKMetaShopPackView> Views = FGameXXKCharacterSheetPresenter::BuildMetaShopPackViews(
        *Subsystem, ActiveKind);
    RebuildOfferCards(Views);
    FGameXXKPermanentCompanion PendingCandidate;
    if (Subsystem->TryGetPendingPermanentCompanionRecruitment(PendingCandidate))
        ShowPendingReplacement(PendingCandidate);
}
```

- [ ] **Step 4: Make purchase input single-shot and result-driven**

Disable all purchase buttons from click until the synchronous subsystem result returns. On failure, refresh and show the typed error. On success, refresh balance and pack buttons, then show the result overlay. Do not call `SetInputMode` from offer/result buttons; only the player controller owns input mode.

```cpp
void UGameXXKMetaShopWidget::HandlePurchase(FName PackId, int32 DrawCount)
{
    if (bPurchaseInFlight)
        return;
    TGuardValue<bool> PurchaseGuard(bPurchaseInFlight, true);
    SetAllPurchaseButtonsEnabled(false);
    FGameXXKMetaShopPurchaseResult Result;
    ResolveSubsystem()->PurchaseMetaShopPack(PackId, DrawCount, Result);
    RefreshFromState();
    Result.bSucceeded ? ShowPurchaseResult(Result) : ShowPurchaseError(Result.Error);
}
```

`ShowPurchaseError` maps only `EGameXXKMetaShopPurchaseError`. Equipment result/confirmation text is always the predecessor transaction's `Result.Message` verbatim. Route-lock state is refreshed from `PreviewMetaShopPurchase` immediately before a click, so a stale enabled button still cannot reach mutation.

- [ ] **Step 5: Implement the persisted 12-roster replacement result layer**

Keep this inside the shop result overlay; do not open or fork the legacy roster widget. The candidate card renders saved identity/profession/12-card data, the roster list is single-select, and confirm/discard/close are separate actions. `RefreshFromState` is the only reopen/rehydration source.

```cpp
void UGameXXKMetaShopWidget::HandleConfirmReplacement()
{
    if (SelectedReplacementId.IsNone() || IsMetaShopMutationLocked())
        return;

    const FName ExistingActiveCompanionId = ResolveSubsystem()->GetRuntimeState()
        .CardRun.CompanionRoster.ActivePermanentCompanionInstanceId;
    const FName ActiveAfterReplacement = IsSelectedReplacementActive()
        ? PendingReplacementCandidate.InstanceId
        : ExistingActiveCompanionId;
    FGameXXKEquipmentTransactionResult Result;
    const bool bResolved = ResolveSubsystem()->ResolvePendingPermanentCompanionReplacement(
        SelectedReplacementId, ActiveAfterReplacement, Result);
    if (!bResolved)
    {
        ShowReplacementError(Result.Message);
        RefreshPendingReplacementWithoutClearingSelection();
        return;
    }
    ClearPendingReplacementSelection();
    RefreshFromState();
}

void UGameXXKMetaShopWidget::HandleDiscardPending()
{
    if (IsMetaShopMutationLocked())
        return;
    if (ResolveSubsystem()->DiscardPendingPermanentCompanionRecruitment())
        ClearPendingReplacementSelection();
    RefreshFromState();
}
```

This is the same plan-one replacement facade, with its existing equipment-safe transaction result surfaced to the caller; if the predecessor still exposes only the temporary boolean signature, reconcile that predecessor before Task 1 rather than adding a second plan-two transaction. Passing the existing active ID for an inactive dismissal preserves the current party; only dismissing the active companion transfers the slot to the candidate. `WarehouseFull` returns the exact plan-one `Result.Message` (`装备背包已满`) and leaves the saved candidate intact. A close action only hides the overlay. Discard is explicit and releases the reserved name through the existing discard facade. When locked, confirm and discard buttons receive the canonical meta-shop route Tooltip from one helper, while their handlers and subsystem facade still reject direct invocation before mutation.

- [ ] **Step 6: Run green widget tests and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.UI.MetaShop"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Public/UI/GameXXKMetaShopWidget.h Source/GameXXK/Private/UI/GameXXKMetaShopWidget.cpp Source/GameXXK/Private/Tests/GameXXKMetaShopWidgetTest.cpp
git diff --cached --check
git commit -m "feat: add permanent meta shop widget"
```

Expected: layout, direct prices, gray states, shortfalls, shared comparison Tooltip, double-click protection, route-lock rollback, and complete persisted 12-roster resolve/discard/reopen behavior pass.

### Task 11: Wire `I/K/T/P`, town buttons, modal focus, and route locks

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp`
- Modify: `Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp`
- Modify: `Source/GameXXK/Private/GameXXKMVPCommandRouter.cpp`
- Create: `Source/GameXXK/Private/Tests/GameXXKCharacterSheetPlayerFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRosterPlayerFlowTest.cpp`

- [ ] **Step 1: Write failing controller/input tests**

Assert:

```text
I -> hero Equipment tab
K -> hero Skills tab
T -> hero Titles tab
P -> companion Equipment tab
C -> same companion entry for backward compatibility
Escape -> closes whichever new modal is topmost and restores prior Town/RouteMap focus
town backpack button -> same path as I
town character button -> hero Attributes tab
town partner button -> same path as P
town shop button -> meta shop
```

Open every modal, resize/reopen it, and assert mouse cursor/input remain usable. During an active route, allow read-only inspection from RouteMap, but disable every mutation: equip, unequip, batch dismantle, enhance, begin/accept/cancel reforge, carry replacement, hero/companion skill commit, meta-shop purchase, pending-companion replacement confirm, and pending-companion discard. Equipment actions display plan one's `Result.Message`; carry uses `路线进行中无法更换伙伴`; skill editing uses `路线进行中无法编辑技能`; meta-shop actions use `路线进行中无法使用局外商店`. Do not open these meta panels over Battle or an active encounter/reward modal.

For each route-locked UI action, serialize the complete runtime state before dispatch, trigger the public handler even if its button is disabled, and assert byte-identical failure afterward. Repeat backend-bypass checks for `SetHeroCardLoadout`, `SetPermanentCompanionCardLoadout`, `PurchaseMetaShopPack`, every plan-one equipment facade action, replacement resolve, and discard. This proves disabled widgets are not the only guard.

- [ ] **Step 2: Run red player-flow tests**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
```

Expected: no `K/T/P` routes, inventory lacks focus ownership, and town buttons still open legacy panels.

- [ ] **Step 3: Add explicit controller ownership**

```cpp
bool OpenHeroCharacterSheet(EGameXXKCharacterSheetTab Tab);
bool OpenCompanionCharacterSheet(EGameXXKCharacterSheetTab Tab);
bool CloseCharacterSheet();
bool OpenMetaShop();
bool CloseMetaShop();
UGameXXKCharacterSheetWidget* EnsureCharacterSheetWidget();
UGameXXKMetaShopWidget* EnsureMetaShopWidget();
```

`InputKey` handles `EKeys::I/K/T/P/C` on pressed, after rejecting text entry and higher-priority battle targeting. Opening one modal closes task, inventory, old roster, codex, encounter, and the other new modal. `OpenFreeInventoryWindow` redirects to hero Equipment; `OpenCompanionRoster` redirects to companion Equipment; old method names remain for Blueprint compatibility.

`OpenMetaShop` may open over RouteMap only as a read-only catalog. It does not change `Screen` or clear the route lock. The widget derives every offer's enabled state from `PreviewMetaShopPurchase`, while `PurchaseMetaShopPack` repeats the route guard before any mutation. `OpenHeroCharacterSheet` and `OpenCompanionCharacterSheet` similarly retain inspection/Tooltip/scroll behavior while their action controllers consult the subsystem lock immediately before dispatch.

- [ ] **Step 4: Fix the centralized focus branch**

In `ApplyPlayerFlowInputMode`, prioritize active confirmation/result overlays, then meta shop, character sheet, existing quest/task/route panels, then viewport. Use `FInputModeGameAndUI` with the active widget focus, show the cursor, and flush pressed keys once on open. Closing restores RouteMap or Town viewport focus; no child button calls input-mode APIs.

```cpp
if (MetaShopWidget && MetaShopWidget->IsOpenForTest())
{
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(MetaShopWidget->TakeWidget());
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    return;
}
if (CharacterSheetWidget && CharacterSheetWidget->IsOpenForTest())
{
    FInputModeGameAndUI Mode;
    Mode.SetWidgetToFocus(CharacterSheetWidget->TakeWidget());
    Mode.SetHideCursorDuringCapture(false);
    SetInputMode(Mode);
    bShowMouseCursor = true;
    return;
}
```

- [ ] **Step 5: Route every visible town button through the same controller functions**

Remove the small local CharacterPanel and static free-recruit/shop action as player-facing destinations. Keep legacy widget classes compiled for migration tests, but all backpack, character, partner, and permanent shop clicks must call the functions listed in Step 3.

```cpp
void UGameXXKTownHudWidget::HandleBackpackClicked()
{
    if (auto* PC = GetOwningPlayer<AGameXXKMVPPlayerController>()) PC->OpenHeroCharacterSheet(EGameXXKCharacterSheetTab::Equipment);
}
void UGameXXKTownHudWidget::HandleCharacterClicked()
{
    if (auto* PC = GetOwningPlayer<AGameXXKMVPPlayerController>()) PC->OpenHeroCharacterSheet(EGameXXKCharacterSheetTab::Attributes);
}
void UGameXXKTownHudWidget::HandlePartnerClicked()
{
    if (auto* PC = GetOwningPlayer<AGameXXKMVPPlayerController>()) PC->OpenCompanionCharacterSheet(EGameXXKCharacterSheetTab::Equipment);
}
void UGameXXKTownHudWidget::HandleShopClicked()
{
    if (auto* PC = GetOwningPlayer<AGameXXKMVPPlayerController>()) PC->OpenMetaShop();
}
```

- [ ] **Step 6: Run green input and player-flow tests, then commit isolated hunks**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.MVP.UI.CharacterSheetPlayerFlow|StartsWith:GameXXK.MVP.UI.PlayerControllerOwnsFlowWidgets|StartsWith:GameXXK.MVP.UI.CompanionRosterPlayerFlow|StartsWith:GameXXK.Equipment.Facade|StartsWith:GameXXK.Data.MetaShop.Subsystem"
# Run the complete shared UE MCP automation block above.
git add Source/GameXXK/Private/Tests/GameXXKCharacterSheetPlayerFlowTest.cpp
git add -p Source/GameXXK/Public/MVP/GameXXKMVPPlayerController.h Source/GameXXK/Private/MVP/GameXXKMVPPlayerController.cpp Source/GameXXK/Private/UI/GameXXKTownHudWidget.cpp Source/GameXXK/Private/UI/GameXXKTownOverlayWidget.cpp Source/GameXXK/Private/GameXXKMVPCommandRouter.cpp Source/GameXXK/Private/Tests/GameXXKPlayerFlowWidgetTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRosterPlayerFlowTest.cpp
git diff --cached --check
git commit -m "feat: route character and meta shop input"
```

Expected: all keys/buttons share one lifecycle, Escape restores input, route locks explain every disabled mutation, direct handler/backend bypasses reject, and every locked failure is byte-identical.

### Task 12: Migrate plan-one saves to version 8 and round-trip every new state

**Files:**

- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveMigration.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKSaveGame.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp`
- Modify: `Source/GameXXK/Public/GameXXKMVPRules.h`
- Modify: `Source/GameXXK/Private/GameXXKMVPRules.cpp`
- Modify: `Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h`
- Modify: `Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp`

- [ ] **Step 1: Write failing in-memory and real-slot version-7 to version-8 tests**

Create a version-7 fixture produced by plan one with equipment instances/loadouts, existing named and unnamed companions, a full pending candidate, PlayerGold, route/card/codex state, and no meta-shop state. Assert migration preserves all plan-one bytes semantically; assigns unique role names only where missing; preserves any valid existing name; reserves the pending name; normalizes one profession portrait per role; initializes equipment pity to zero and six role progress records; seeds meta shop deterministically; grants no extra starter; and produces version 8. Re-run migration on the v8 result and require byte-identical output.

```cpp
FGameXXKSaveState Version7 = BuildPlanOneVersion7Fixture();
FGameXXKSaveState Migrated;
FGameXXKSaveMigrationReport Report;
TestTrue(TEXT("dispatcher migrates v7"), FGameXXKSaveMigration::MigrateToCurrent(Version7, Migrated, Report));
TestEqual(TEXT("report targets v8"), Report.TargetVersion, 8);
TestEqual(TEXT("version 8"), Migrated.SaveVersion, 8);
TestEqual(TEXT("equipment preserved"), Migrated.RuntimeState.EquipmentCollection.EquipmentInstances.Num(), Version7.RuntimeState.EquipmentCollection.EquipmentInstances.Num());
TestEqual(TEXT("six role progress"), Migrated.RuntimeState.MetaShop.CompanionPackProgress.Num(), 6);
TestEqual(TEXT("no starter added"), Migrated.RuntimeState.CardRun.CompanionRoster.PermanentCompanions.Num(), Version7.RuntimeState.CardRun.CompanionRoster.PermanentCompanions.Num());
```

Extend plan one's real-slot migration fixture, using unique temporary main/backup slot names and its failure-injection seams. Cover:

1. Success writes `<SlotName>.PreV8Backup` as the unmodified v7 object, writes verified v8 to the main slot, then changes live state.
2. An existing `.PreV8Backup` is reusable only when it reloads successfully and its canonical serialized source-slot checksum equals the current v7 main object. A matching backup is never overwritten. A mismatched, unreadable, or corrupt backup forces the next non-overwriting attempt name (`.PreV8Backup.001`, `.002`, and so on), and the report records the actual backup slot used. Prove this with v7 state A backed up first, then a progressed v7 state B in the main slot: inject a post-write failure and assert rollback restores B from the new attempt backup, never stale A. Add a separate corrupt-base-backup fixture and prove its verified numbered attempt is used for rollback.
3. Backup failure, in-memory migration failure, full-state validation failure, upgraded-main write failure, and post-write reload/equality verification failure all return false.
4. Every failure keeps live RuntimeState byte-identical; if a main write was attempted, the original main file is restored from backup and reloaded to prove equality.
5. Every failure surfaces exact text `存档迁移失败，已保留原存档。`.
6. Each case deletes its temporary main, `.PreV8Backup`, and every numbered attempt-backup slot before and after the test.

- [ ] **Step 2: Cold-build the red tests, then run the focused MCP filters**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Equipment.SaveMigration|StartsWith:GameXXK.MVP.SaveGame.MetaShopVersion8Migration"
# Run the complete shared UE MCP automation block above.
```

Expected: the tests compile after the cold boundary, then fail because the dispatcher still targets 7, `.PreV8Backup` is absent, or failure recovery does not preserve both disk and live state. Record the focused failing assertions before implementation.

- [ ] **Step 3: Extend plan one's dispatcher with one explicit 7→8 stage**

Do not bypass `FGameXXKSaveMigration` from `RestoreFromSaveState` or add a second migration chain in meta-shop rules. Change the existing report default to `TargetVersion = 8`, set `CurrentSaveVersion = 8`, and preserve plan one's exact 0→…→7 results before applying the new stage. Reject source versions greater than 8; accept current v8 without mutation. A source in versions 0–6 must first produce the exact frozen plan-one v7 state, then run the same 7→8 function as a direct v7 source.

```cpp
struct FGameXXKSaveMigrationReport
{
    bool bSucceeded = false;
    int32 SourceVersion = 0;
    int32 TargetVersion = 8;
    bool bCreatedLegacyOverflow = false;
    FString BackupSlotName;
    TArray<FString> Warnings;
    FString Error;
};

if (Source.SaveVersion > 8)
    return FailUnsupportedVersion(OutReport);
if (Working.SaveVersion <= 6 && !MigrateThroughFrozenPlanOneVersion7(Working, OutReport))
    return false;
if (Working.SaveVersion == 7 && !MigrateVersion7To8(Working, OutReport))
    return false;
```

The deterministic 7→8 stage performs only the new initialization:

```cpp
if (Working.RuntimeState.MetaShop.SequenceSeed == 0)
{
    Working.RuntimeState.MetaShop.SequenceSeed = HashCombine(
        Working.RuntimeState.CardRun.CompanionRoster.RecruitSequenceSeed,
        0x4D455441); // "META"
}
FGameXXKMetaShopRules::InitializeMissingRoleProgress(Working.RuntimeState.MetaShop);
AssignMissingCompanionNamesInRosterOrder(Working.RuntimeState.CardRun.CompanionRoster);
NormalizeCompanionPortraitKeys(Working.RuntimeState.CardRun.CompanionRoster);
Working.SaveVersion = 8;
```

Name assignment iterates `PermanentCompanions` in saved order, then the pending candidate. It starts with all valid existing names occupied, resolves only missing/invalid/duplicate names, and never changes template, cards, level, star, active selection, equipment loadout key, or instance ID. Validate the 72-name catalog, meta-shop state, full runtime state, and `ValidateCollectionAgainstRoster` before the dispatcher reports success. If no legal name exists, migration fails; it never partially returns a v8 object.

- [ ] **Step 4: Extend `LoadGameFromSlot` with failure-safe real-slot migration**

Preserve plan one's load path and backup design, changing only the target-version boundary and backup suffix. For a pre-v8 slot, execute this exact order:

1. Load the original `UGameXXKSaveGame` into a local object; do not call `BeginRuntimeStateMutation` and do not change live RuntimeState.
2. Canonically serialize the unmodified v7 source and compute its checksum. Reuse `<SlotName>.PreV8Backup` only when the backup reloads successfully and has the identical source checksum. Never overwrite a backup: if it is absent, create it; if it exists but differs, allocate the first free `<SlotName>.PreV8Backup.NNN` attempt slot, save and reload-verify that copy, and record the chosen slot in `FGameXXKSaveMigrationReport::BackupSlotName` plus a mismatch warning.
3. Pass an in-memory copy through `FGameXXKSaveMigration::MigrateToCurrent`.
4. Validate version 8, the entire runtime/card/route/codex state, meta-shop rows/names, plan-one equipment collection, and collection/roster ownership.
5. Write a new `UGameXXKSaveGame` containing the migrated state to the main slot.
6. Reload the main slot and verify it is v8 and serialization-equal to the validated migrated object.
7. Only after Step 6 succeeds call the normal runtime mutation boundary and assign the verified state live.

If Steps 2–4 fail, the main slot was never touched. If Steps 5–6 fail, restore the original main object only from the checksum-matched backup slot recorded for this attempt, reload it, and verify restoration before returning. Never roll back a newly progressed v7 main slot from an older mismatched backup. Every failure keeps live RuntimeState unchanged and surfaces `存档迁移失败，已保留原存档。`. Never delete a successful backup automatically and never overwrite it on a later load. A current v8 slot follows the normal verified load path and does not create a new pre-v8 backup.

- [ ] **Step 5: Round-trip pity, pending candidate, names, gold, and equipment ownership**

Extend save equality assertions to compare `MetaShop`, `DisplayNameId`, `EquipmentCollection`, warehouse indexes, character loadouts, redundant instance owner fields, and pending candidate. Execute a paid equipment pull and companion pull before save; after reload, the next fixed-seed draw must equal the uninterrupted sequence.

```cpp
const FGameXXKSaveState Saved = UGameXXKMVPRules::MakeSaveState(StateAfterPurchases);
const FGameXXKRuntimeState Reloaded = UGameXXKMVPRules::RestoreFromSaveState(Saved);
TestEqual(TEXT("equipment ordinal"), Reloaded.MetaShop.EquipmentDrawOrdinal, StateAfterPurchases.MetaShop.EquipmentDrawOrdinal);
TestEqual(TEXT("equipment misses"), Reloaded.MetaShop.ConsecutiveNonEpicEquipment, StateAfterPurchases.MetaShop.ConsecutiveNonEpicEquipment);
TestEqual(TEXT("pending name"), Reloaded.CardRun.CompanionRoster.PendingRecruitment.Candidate.DisplayNameId, StateAfterPurchases.CardRun.CompanionRoster.PendingRecruitment.Candidate.DisplayNameId);
TestEqual(TEXT("equipment collection"), SerializeEquipmentCollection(Reloaded.EquipmentCollection), SerializeEquipmentCollection(StateAfterPurchases.EquipmentCollection));
```

Define the equality helper in the same test file:

```cpp
static TArray<uint8> SerializeEquipmentCollection(const FGameXXKEquipmentCollectionState& Collection)
{
    TArray<uint8> Bytes;
    FMemoryWriter Writer(Bytes, true);
    FGameXXKEquipmentCollectionState Copy = Collection;
    FGameXXKEquipmentCollectionState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
    return Bytes;
}
```

- [ ] **Step 6: Run green migration/regression tests and commit**

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 300 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Equipment.SaveMigration|StartsWith:GameXXK.MVP.SaveGame.SlotRoundTrip|StartsWith:GameXXK.MVP.SaveGame.MetaShopVersion8Migration|StartsWith:GameXXK.MVP.Inventory.EnhancementAndStorage"
# Run the complete shared UE MCP automation block above.
git add -p Source/GameXXK/Public/MVP/GameXXKSaveMigration.h Source/GameXXK/Private/MVP/GameXXKSaveMigration.cpp Source/GameXXK/Public/MVP/GameXXKSaveGame.h Source/GameXXK/Private/MVP/GameXXKSaveGame.cpp Source/GameXXK/Public/GameXXKMVPRules.h Source/GameXXK/Private/GameXXKMVPRules.cpp Source/GameXXK/Public/MVP/GameXXKMVPSubsystem.h Source/GameXXK/Private/MVP/GameXXKMVPSubsystem.cpp Source/GameXXK/Private/Tests/GameXXKEquipmentSaveMigrationTest.cpp Source/GameXXK/Private/Tests/GameXXKSaveGameTest.cpp Source/GameXXK/Private/Tests/GameXXKCompanionRecruitmentFlowTest.cpp Source/GameXXK/Private/Tests/GameXXKInventoryEnhancementTest.cpp
git diff --cached --check
git commit -m "feat: migrate meta shop and companion identities"
```

Expected: frozen versions 0–6 still reach the same v7 intermediate state, v7 migrates once to v8, v8 is idempotent, real-slot backup/recovery cases pass, version-8 round-trips exactly, and subsequent draws remain deterministic.

### Task 13: Add focused acceptance automation and complete cold/PIE verification

**Files:**

- Create: `scripts/gamexxk_meta_shop_acceptance.py`
- Create: `scripts/test_gamexxk_meta_shop_acceptance.py`
- Create: `Content/Python/gamexxk_probe_meta_shop.py`
- Create: `docs/production/2026-07-22-meta-shop-partner-character-ui-acceptance.md`
- Modify only if required by validator: matching `docs/production/*` state record.

- [ ] **Step 1: Write the failing Python acceptance-plan test**

Assert the runner names every deterministic automation group and refuses to mutate private runtime state. Required groups:

```python
REQUIRED_TESTS = (
    "GameXXK.Data.Companion.NameCatalog",
    "GameXXK.Data.MetaShop.Catalog",
    "GameXXK.Data.MetaShop.EquipmentPack",
    "GameXXK.Data.MetaShop.CompanionPack",
    "GameXXK.Data.MetaShop.Atomicity",
    "GameXXK.Data.MetaShop.Subsystem",
    "GameXXK.Data.CharacterSheet.Presenter",
    "GameXXK.Equipment.Facade",
    "GameXXK.Equipment.CompanionReplacement",
    "GameXXK.Equipment.SaveMigration",
    "GameXXK.UI.CharacterSheet",
    "GameXXK.UI.EquipmentWarehouse",
    "GameXXK.UI.EquipmentTooltip",
    "GameXXK.UI.MetaShop",
    "GameXXK.MVP.UI.CharacterSheetPlayerFlow",
    "GameXXK.MVP.SaveGame.SlotRoundTrip",
    "GameXXK.MVP.SaveGame.MetaShopVersion8Migration",
)
```

- [ ] **Step 2: Run the red Python test**

```powershell
python -m pytest scripts/test_gamexxk_meta_shop_acceptance.py -q
```

Expected: failure because the runner and probe are absent.

- [ ] **Step 3: Implement a read-only-first MCP probe and exact actions**

The probe returns JSON snapshots for PlayerGold, meta-shop pity/ordinals, warehouse count/capacity, active character sheet context/tab, selected companion/name/portrait, visible pack buttons/prices/reasons, pending candidate, and mouse-focus owner. Allowed actions call only public controller/widget/subsystem functions: open by key-equivalent method, purchase one/ten, select companion, confirm carry, resolve/discard pending replacement, equip one instance, close modal. It must not overwrite `RuntimeState` or fabricate pack results.

```python
ALLOWED_ACTIONS = {
    "snapshot",
    "open_hero_equipment",
    "open_hero_skills",
    "open_hero_titles",
    "open_companion_equipment",
    "open_meta_shop",
    "purchase_pack",
    "select_companion",
    "confirm_carry",
    "commit_skill_loadout",
    "unequip_slot",
    "resolve_pending_replacement",
    "discard_pending_replacement",
    "equip_instance",
    "enhance_instance",
    "begin_reforge",
    "resolve_reforge",
    "dismantle_selection",
    "close_modal",
}
if action not in ALLOWED_ACTIONS:
    raise ValueError(f"unsupported public action: {action}")
```

- [ ] **Step 4: Run cold build and all focused automation**

If the editor is running, let the pipeline save through UE MCP and close/restart it safely:

```powershell
python scripts/ue_tdd_pipeline.py --pie-duration 0 --log-lines 500 --filter "[TDD]"
$env:GAMEXXK_AUTOMATION_FILTERS = "StartsWith:GameXXK.Data.Companion.NameCatalog|StartsWith:GameXXK.Data.MetaShop|StartsWith:GameXXK.Data.CharacterSheet.Presenter|StartsWith:GameXXK.Equipment.Facade|StartsWith:GameXXK.Equipment.CompanionReplacement|StartsWith:GameXXK.Equipment.SaveMigration|StartsWith:GameXXK.UI.CharacterSheet|StartsWith:GameXXK.UI.EquipmentWarehouse|StartsWith:GameXXK.UI.EquipmentTooltip|StartsWith:GameXXK.UI.MetaShop|StartsWith:GameXXK.MVP.UI.CharacterSheetPlayerFlow|StartsWith:GameXXK.MVP.SaveGame"
# Run the complete shared UE MCP automation block above.
```

Expected: UBT `Result: Succeeded`; every listed group reports zero failures; no Live Coding/Hot Reload appears in the verification log.

- [ ] **Step 5: Run regression and real player flow**

```powershell
python scripts/gamexxk_mvp_playtest.py --skip-build --test-timeout 600 --report Saved/HarnessReports/meta_shop_mvp_regression.json
python scripts/gamexxk_real_play_flow_mcp.py --timeout 180 --keep-pie --report Saved/HarnessReports/meta_shop_real_flow.json
python scripts/gamexxk_meta_shop_acceptance.py --report Saved/HarnessReports/meta_shop_acceptance.json
```

Expected: main menu → town still works; inventory/companion/save tests remain green; the acceptance report contains direct observed evidence for the following sequence:

1. `I`, `K`, `T`, and `P` open the correct shared sheet tab and Escape restores click input.
2. Hero and selected companion each show six independent loadout slots.
3. Warehouse shows 4 columns, 10 visible rows, scrollbar, and can scroll beyond item 40; automation also proves migrated 201+ overflow items remain visible/selectable/dismantlable with an `actual/200` label.
4. Every equipment hover shows the shared full comparison Tooltip.
5. Equipment-pack single costs 50, ten costs 450, and generated item level equals hero level.
6. Companion-pack single costs 150 and produces the selected profession only.
7. Insufficient gold and warehouse-full buttons are gray, describe the reason, and do not debit.
8. New companion shows a unique saved name and 12-card personal pool; duplicate gives one sigil.
9. A full 12-person roster shows candidate name/profession/12 cards and all 12 replacement rows; close/reopen preserves it, equipment-safe confirm works, WarehouseFull keeps it unchanged, and explicit discard releases its name.
10. Active partner button reads `已携带`; while a route is active, equipment, enhancement/reforge/dismantle, carry, skill-submit, purchase, replace, and discard handlers all reject with the exact route-lock text and byte-identical state.
11. Save/reload preserves names, equipment ownership, gold, pity, ordinals, pending result, filters, and active partner; migration automation proves checksum-matched `.PreV8Backup` reuse, numbered mismatch backups, verified write/reload, and rollback to the current v7 source only.

- [ ] **Step 6: Record evidence and pass the final dirty-tree gate**

Write the acceptance document with exact command lines, automation counts, report paths, PIE screenshots under `Saved/Screenshots`, save-migration fixture result, and non-blocking warnings. Then run:

```powershell
git diff --check
python scripts/harness_state_validator.py
git status --short
```

Review every dirty pre-existing file and confirm user-owned hunks remain. Stage only the new harness/probe/acceptance files and isolated feature hunks. Commit message:

```text
test: verify meta shop and unified character ui
```

---

## Completion checklist

- [ ] Plan-one equipment instance, warehouse, loadout, stat, Tooltip, and migration tests remain green.
- [ ] The catalog contains exactly six equipment packs and six companion packs with approved prices.
- [ ] Equipment rolls are 70/25/5, tenth-pull protected, thirtieth-pull Epic protected, and share one saved counter.
- [ ] Companion packs are profession-specific, single-only, independently protected, and cannot be obtained through a free UI click.
- [ ] New profiles persist one of the exact 72 globally unique names; pending candidates reserve and release names correctly.
- [ ] Same-profession companions share the approved profession portrait identity while names/cards/stats remain distinct.
- [ ] Every purchase validates and atomically commits gold, pity, sequence, generated result, warehouse, sigil, and pending state.
- [ ] The shared warehouse normally holds 200 unequipped instances, renders 4 columns/10 visible rows, scrolls without pagination, excludes equipped items, and expands logical slots to preserve every migrated overflow instance.
- [ ] Hero and up to 12 companions maintain independent six-slot loadouts through the one shared sheet.
- [ ] `I/K/T/P`, compatibility `C`, Town buttons, Escape, mouse focus, and route locks all follow one controller lifecycle; locked UI and direct backend calls are byte-identical failures.
- [ ] Every equipment appearance uses the same rich comparison Tooltip and clear unavailable reason.
- [ ] Version-7 saves migrate once to version 8 without losing equipment, cards, companions, names, active partner, or PlayerGold; checksum-matched backups and numbered attempts prevent stale rollback.
- [ ] Cold build, focused automation, regression automation, real PIE flow, and saved acceptance evidence all pass.
