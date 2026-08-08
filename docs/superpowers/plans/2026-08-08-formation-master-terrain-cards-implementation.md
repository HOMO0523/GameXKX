# Formation Master Terrain Cards Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Give every Formation Master terrain card a reliable base effect on every terrain, make the matching-terrain bonus materially stronger, eliminate the four target-mode deadlocks, and make Formation Master terrain cards participate in the existing terrain-card cost and doubling rules.

**Architecture:** Keep card identity and values declarative in `FGameXXKCardCatalog`, while the shared resolver owns two generic contracts: a false target-independent condition is skipped before its effect target is resolved, and any card with a `TerrainIsAny` effect is a terrain card (with the legacy `Route.Terrain` acquisition key retained for conditionless terrain cards). Formation cards with a chosen ally never change target mode; their unconditional selected-target effect resolves once, then their matching-terrain group effects resolve in addition. `DoubleTerrainBonus` continues to copy only conditioned effects.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, UBT cold builds, commandlet reports, the existing card-balance observation harness, and the project UE MCP lifecycle scripts.

**Prerequisite:** Complete `docs/superpowers/plans/2026-08-08-global-mark-rule-implementation.md` first. This plan's 回声震杀, 山门封锁, 地脉借力, and post-change balance checks assume the global Mark rule and card-start Mark snapshot already exist.

---

## File map

- Modify `Source/GameXXK/Private/GameXXKCardRules.cpp`: skip false target-independent conditions before effect-target resolution; classify both legacy route terrain cards and terrain-conditioned cards as terrain cards.
- Modify `Source/GameXXK/Private/GameXXKCardCatalog.cpp`: replace the eleven confirmed Formation Master definitions with the approved base-plus-bonus values and remove three Formation target overrides.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp`: move the target-override validator fixture to 藤桥飞渡 and remove obsolete Formation override expectations.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp`: prove the three chosen-ally Formation cards remain manual targets and no longer advertise “目标改为”.
- Modify `Source/GameXXK/Private/Tests/GameXXKFormationMasterTargetingDiagnosticTest.cpp`: turn the 122/126 diagnostic into a strict 126/126 gate and update the Water-shore board interaction to manual ally selection.
- Create `Source/GameXXK/Private/Tests/GameXXKTerrainConditionalResolutionTest.cpp`: reproduce and lock the generic 藤桥飞渡 condition-before-target fix.
- Create `Source/GameXXK/Private/Tests/GameXXKFormationMasterCardDesignTest.cpp`: assert every approved current-quality value and effect order.
- Create `Source/GameXXK/Private/Tests/GameXXKFormationMasterEffectsTest.cpp`: resolve representative mismatched/matched terrain pairs and check actual state deltas.
- Create `Source/GameXXK/Private/Tests/GameXXKFormationTerrainSynergyTest.cpp`: prove Formation terrain cards consume cost/doubling windows while 定阵 does not.

## Shared verification commands

Before any cold build, save and close a running editor through the project helper:

```powershell
python -B -c "from scripts.ue_tdd_pipeline import save_running_editor_before_close, kill_editor; import sys; ok=save_running_editor_before_close(); kill_editor() if ok else None; sys.exit(0 if ok else 1)"
```

If MCP is unavailable while an editor is running, this command must fail without killing the editor. Do not bypass it.

Cold build:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload
```

Define this helper once in the implementation PowerShell session, then call it with each task's exact test path and a unique report name:

```powershell
function Invoke-GameXXKAutomation {
    param(
        [Parameter(Mandatory = $true)][string]$TestPath,
        [Parameter(Mandatory = $true)][string]$ReportName
    )
    & 'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' `
        'D:\UE5 demo\GameXXK\GameXXK.uproject' `
        -unattended -nopause -nosplash -nullrhi `
        "-ExecCmds=Automation RunTests $TestPath; Quit" `
        "-ReportOutputPath=D:\UE5 demo\GameXXK\Saved\Automation\$ReportName"
    if ($LASTEXITCODE -ne 0) { throw "Automation commandlet exited $LASTEXITCODE for $TestPath" }
}
```

Success means the generated `index.json` records zero failed tests. Localized log text by itself is not sufficient evidence.

### Task 0: Establish the prerequisite and unchanged baseline

**Files:**
- Read: `AGENTS.md`
- Read: `docs/design/2026-08-08-formation-master-mark-rules-design.md`
- Read: `docs/superpowers/plans/2026-08-08-global-mark-rule-implementation.md`
- Inspect: all files in the file map above

- [ ] **Step 1: Confirm the Mark prerequisite is complete**

Run:

```powershell
git log -8 --oneline
git status --short
```

Then run these tests with fresh report directories:

```text
GameXXK.Data.MarkRules
GameXXK.Integration.MarkCardCompatibility
```

Expected: both pass. Stop if the global Mark plan is not implemented or its focused tests are not green.

- [ ] **Step 2: Protect user-owned work**

Run:

```powershell
git diff -- Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests
git status --short -- Source/GameXXK docs/superpowers/plans docs/design
```

Expected: unrelated untracked assets/build outputs may remain, but no overlapping tracked edit is silently overwritten. If an overlapping user edit exists, stop and reconcile it explicitly.

- [ ] **Step 3: Save/close the editor and cold-build**

Run the lifecycle command and shared UBT command.

Expected: exit code 0, with no Live Coding or Hot Reload.

- [ ] **Step 4: Record the pre-change Formation baseline**

Run:

```text
GameXXK.Diagnostics.FormationMasterTargeting
GameXXK.Diagnostics.FormationMasterBoardTargeting
GameXXK.Data.CardCatalog
GameXXK.Integration.CardText
```

Expected before this plan: the diagnostics pass with the documented four warning pairs, 126 previews and 122 resolutions. Preserve those reports as the RED baseline; do not normalize the warnings away.

### Task 1: Skip inactive terrain effects before resolving their targets

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKTerrainConditionalResolutionTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2858-2909,4378-4591`

- [ ] **Step 1: Write the failing 藤桥飞渡 regression**

Create the test with this complete fixture and both affected terrains:

```cpp
#include "GameXXKCardQualityRules.h"
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKCardCombatUnit MakeTerrainConditionUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const EGameXXKCharacterRole Role,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.Role = Role;
		Unit.bLiving = true;
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.Attack = 20;
		Unit.Mana = 0;
		Unit.MaxMana = 100;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeTerrainConditionCards()
	{
		TArray<FGameXXKCardInstance> Cards;
		for (int32 Index = 0; Index < 10; ++Index)
		{
			FGameXXKCardInstance& Card = Cards.AddDefaulted_GetRef();
			Card.InstanceId = FName(*FString::Printf(TEXT("TerrainCondition.%d"), Index));
			Card.CardId = TEXT("Npc.QiongMeiEr.TengQiaoFeiDu");
			Card.OwnerUnitId = TEXT("Qiong");
			Card.CurrentQuality = FGameXXKCardQualityRules::GetCardBaseQuality(Card.CardId);
			Card.SourceEntryId = FName(*FString::Printf(TEXT("TerrainCondition.Source.%d"), Index));
			Card.AcquisitionOrdinal = Index;
		}
		return Cards;
	}

	FGameXXKCardCombatUnit* FindTerrainConditionUnit(
		FGameXXKCardBattleRuntime& Runtime,
		const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTerrainConditionalResolutionTest,
	"GameXXK.Integration.TerrainConditionalResolution",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTerrainConditionalResolutionTest::RunTest(const FString& Parameters)
{
	for (const EGameXXKCardTerrain Terrain : {
		EGameXXKCardTerrain::Cliff,
		EGameXXKCardTerrain::Forest})
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeTerrainConditionUnit(TEXT("Qiong"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::QuestNpc, 1),
			MakeTerrainConditionUnit(TEXT("Hero"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hero, 2),
			MakeTerrainConditionUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		FGameXXKCardBattleRuntime Runtime;
		FString Error;
		TestTrue(TEXT("藤桥飞渡 correct-terrain fixture initializes"),
			GameXXKCardRules::InitializeCardBattleRuntime(Runtime, MakeTerrainConditionCards(), Units, Terrain, 18101, &Error));
		if (Runtime.Deck.Hand.IsEmpty())
		{
			return false;
		}

		const FName InstanceId = Runtime.Deck.Hand[0].InstanceId;
		FGameXXKCardPlayPreview Preview;
		TestTrue(TEXT("藤桥飞渡 correct-terrain preview builds"),
			GameXXKCardRules::BuildCardPlayPreview(Runtime, InstanceId, Preview, &Error));
		TestEqual(TEXT("藤桥飞渡 retains its declared all-allies override"),
			Preview.TargetRequest.EffectiveMode, EGameXXKCardTargetMode::AllAllies);
		TestFalse(TEXT("藤桥飞渡 correct-terrain override needs no selected unit"),
			Preview.TargetRequest.bRequiresManualSelection);

		FGameXXKCardPlayResult Result;
		TestTrue(TEXT("inactive selected-target effects are skipped before target resolution"),
			GameXXKCardRules::ResolveCardPlay(Runtime, InstanceId, NAME_None, Result, &Error));
		for (const FName AllyId : {FName(TEXT("Qiong")), FName(TEXT("Hero"))})
		{
			FGameXXKCardCombatUnit* Ally = FindTerrainConditionUnit(Runtime, AllyId);
			TestNotNull(TEXT("藤桥飞渡 keeps each ally addressable"), Ally);
			if (Ally)
			{
				TestEqual(TEXT("Rare 藤桥飞渡 grants two Agility to every ally"),
					GameXXKCardRules::GetCombatStatusStacks(*Ally, EGameXXKCardStatus::Agility), 2);
				TestEqual(TEXT("Rare 藤桥飞渡 grants five mana to every ally"), Ally->Mana, 5);
			}
		}
		TestFalse(TEXT("the resolved card leaves the hand"),
			Runtime.Deck.Hand.ContainsByPredicate([InstanceId](const FGameXXKCardInstance& Card)
			{
				return Card.InstanceId == InstanceId;
			}));
	}
	return true;
}

#endif
```

- [ ] **Step 2: Cold-build and verify RED**

Run UBT, then `GameXXK.Integration.TerrainConditionalResolution` with report `TerrainConditionalResolution_Red`.

Expected: both terrain cases reject atomically with `Selected-target effect has no current living stable target`, because the resolver still resolves the inactive selected-target effect before testing its negated terrain condition.

- [ ] **Step 3: Add the target-independence classifier**

Place this beside `IsConditionSatisfied`:

```cpp
	bool CanEvaluateConditionBeforeEffectTargets(
		const EGameXXKCardEffectConditionType ConditionType)
	{
		switch (ConditionType)
		{
		case EGameXXKCardEffectConditionType::None:
		case EGameXXKCardEffectConditionType::OwnerHasStatus:
		case EGameXXKCardEffectConditionType::OwnerArmorAtLeast:
		case EGameXXKCardEffectConditionType::OwnerHealthBelowPercent:
		case EGameXXKCardEffectConditionType::TerrainIsAny:
		case EGameXXKCardEffectConditionType::OwnerHasDamageOverTime:
			return true;
		case EGameXXKCardEffectConditionType::TargetHasStatus:
		case EGameXXKCardEffectConditionType::TargetHasAnyDamageOverTime:
		case EGameXXKCardEffectConditionType::TargetHealthBelowPercent:
			return false;
		}
		return false;
	}
```

- [ ] **Step 4: Pre-check without consuming, then resolve targets**

In `ResolveCurrentCardEffects`, replace the ordinary-effect block that currently calls `ResolveEffectTargetIds` before locating `Owner` and `ConditionTarget` with this ordering. `ConditionSnapshot` is the card-start snapshot introduced by the prerequisite Mark plan:

```cpp
			FGameXXKCardCombatUnit* Owner = FindCombatUnitById(InOutRuntime.Units, Instance.OwnerUnitId);
			if (!Owner || !Owner->bLiving)
			{
				return true;
			}
			FGameXXKCardCombatUnit* ConditionTarget = CardTargetIds.Num() == 1
				? FindCombatUnitById(InOutRuntime.Units, CardTargetIds[0])
				: nullptr;

			if (CanEvaluateConditionBeforeEffectTargets(Effect.Condition.Type))
			{
				bool bConditionSatisfiedBeforeTargets = false;
				if (!IsConditionSatisfied(
					Effect.Condition,
					InOutRuntime,
					*Owner,
					ConditionTarget,
					&ConditionSnapshot,
					bConditionSatisfiedBeforeTargets,
					OutError))
				{
					return false;
				}
				if (!bConditionSatisfiedBeforeTargets)
				{
					continue;
				}
			}

			TArray<FName> EffectTargetIds;
			if (!ResolveEffectTargetIds(
				InOutRuntime,
				Instance.OwnerUnitId,
				CardTargetIds,
				Effect.Target,
				EffectTargetIds,
				OutError))
			{
				return false;
			}
```

Keep the existing later `TryApplyEffectConditionAndConsumption` call. The pre-check is read-only; consumption still occurs exactly once at the existing commit point, and target-dependent conditions still evaluate per resolved target.

- [ ] **Step 5: Cold-build and verify GREEN plus atomicity regressions**

Run UBT, then:

```text
GameXXK.Integration.TerrainConditionalResolution
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardCombatRules
```

Expected: all pass. 藤桥飞渡 grants its Rare all-allies values on both Cliff and Forest, while malformed/illegal targets elsewhere still roll the card play back atomically.

- [ ] **Step 6: Commit the generic resolver repair**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKTerrainConditionalResolutionTest.cpp'
git commit -m "fix: skip inactive terrain effects before targeting"
```

### Task 2: Lock and implement the approved Formation catalog

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKFormationMasterCardDesignTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp:278-283,599-637`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp:23-48`
- Modify: `Source/GameXXK/Private/Tests/GameXXKFormationMasterTargetingDiagnosticTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardCatalog.cpp:938-981`

- [ ] **Step 1: Add a failing exact-value contract**

Create `GameXXKFormationMasterCardDesignTest.cpp`. Build each definition at `GetCardBaseQuality(CardId)`, then assert the following exact ordered effective definitions:

```cpp
struct FExpectedFormationEffect
{
	EGameXXKCardEffectType Type;
	EGameXXKCardEffectTarget Target;
	int32 Magnitude;
	EGameXXKCardStatus Status = EGameXXKCardStatus::None;
	EGameXXKCardEffectConditionType Condition = EGameXXKCardEffectConditionType::None;
	EGameXXKCardTerrain Terrain = EGameXXKCardTerrain::Invalid;
	EGameXXKCardTerrain AlternateTerrain = EGameXXKCardTerrain::Invalid;
};

struct FExpectedFormationCard
{
	FName CardId;
	EGameXXKCardQuality Quality;
	int32 EnergyCost;
	int32 ManaCost;
	EGameXXKCardTargetMode TargetMode;
	bool bRequiresNoTargetOverrides;
	TArray<FExpectedFormationEffect> Effects;
};

const TArray<FExpectedFormationCard> Expectations = {
	{TEXT("Profession.FormationMaster.YinShuiHuiYuan"), EGameXXKCardQuality::Common, 1, 0, EGameXXKCardTargetMode::SingleAlly, true, {
		{EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6},
		{EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry},
		{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry}}},
	{TEXT("Profession.FormationMaster.LinYingMiZong"), EGameXXKCardQuality::Common, 1, 0, EGameXXKCardTargetMode::SingleAlly, true, {
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Forest},
		{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Forest}}},
	{TEXT("Profession.FormationMaster.JieShanWeiZhang"), EGameXXKCardQuality::Common, 1, 0, EGameXXKCardTargetMode::AllAllies, false, {
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cliff},
		{EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Vulnerability, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cliff}}},
	{TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), EGameXXKCardQuality::Rare, 2, 0, EGameXXKCardTargetMode::AllAllies, false, {
		{EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 12},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8},
		{EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 8, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village},
		{EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::AllAllies, 2, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village}}},
	{TEXT("Profession.FormationMaster.HuiShengZhenSha"), EGameXXKCardQuality::Rare, 2, 6, EGameXXKCardTargetMode::SingleEnemy, false, {
		{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 240},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cave},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Mark, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cave}}},
	{TEXT("Profession.FormationMaster.ZhenShaZhen"), EGameXXKCardQuality::Epic, 3, 10, EGameXXKCardTargetMode::AllEnemies, false, {
		{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::AllEnemies, 320},
		{EGameXXKCardEffectType::IgnoreDefense, EGameXXKCardEffectTarget::AllEnemies, 8, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cave},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 3, EGameXXKCardStatus::Vulnerability},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 4, EGameXXKCardStatus::Poison, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cave}}},
	{TEXT("Profession.FormationMaster.ShanMenFengSuo"), EGameXXKCardQuality::Common, 1, 0, EGameXXKCardTargetMode::SingleEnemy, false, {
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cliff},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cliff}}},
	{TEXT("Profession.FormationMaster.ShuiJingZheGuang"), EGameXXKCardQuality::Rare, 1, 0, EGameXXKCardTargetMode::SingleAlly, false, {
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 16},
		{EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 8, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry},
		{EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry}}},
	{TEXT("Profession.FormationMaster.LinFengFuZhen"), EGameXXKCardQuality::Common, 0, 0, EGameXXKCardTargetMode::SingleAlly, true, {
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Forest}}},
	{TEXT("Profession.FormationMaster.ZhenQiGuWu"), EGameXXKCardQuality::Rare, 1, 0, EGameXXKCardTargetMode::AllAllies, false, {
		{EGameXXKCardEffectType::ApplyBattleModifier, EGameXXKCardEffectTarget::AllAllies, 0},
		{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 6, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village}}},
	{TEXT("Profession.FormationMaster.DiMaiJieLi"), EGameXXKCardQuality::Rare, 2, 0, EGameXXKCardTargetMode::SingleEnemy, false, {
		{EGameXXKCardEffectType::DamagePercentAttack, EGameXXKCardEffectTarget::SelectedTarget, 200},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cliff},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Mark, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Forest},
		{EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::CardOwner, 6, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry},
		{EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 8, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Village},
		{EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Poison, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Cave},
		{EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 2, EGameXXKCardStatus::None, EGameXXKCardEffectConditionType::TerrainIsAny, EGameXXKCardTerrain::Plain}}}
};
```

For every entry, assert card existence, base quality, costs, target mode, exact effect count and exact fields at every index. Also assert every listed condition has `bNegate == false`. For 阵旗鼓舞 effect 0, additionally assert:

```cpp
TestEqual(TEXT("阵旗鼓舞 waits for the next attack"), Effect.Modifier.Trigger, EGameXXKCardBattleModifierTrigger::OnNextAttack);
TestEqual(TEXT("阵旗鼓舞 adds twenty percent"), Effect.Modifier.Magnitude, 20);
TestEqual(TEXT("阵旗鼓舞 applies to all allies"), Effect.Modifier.RecipientScope, EGameXXKCardModifierRecipientScope::AllAllies);
TestEqual(TEXT("阵旗鼓舞 modifies the played attack"), Effect.Modifier.Target, EGameXXKCardEffectTarget::PlayedCard);
```

- [ ] **Step 2: Make existing tests express the new target contract**

In `GameXXKCardCatalogTest.cpp`:

1. Change the malformed target-override fixture at current lines 278-283 from 引水回元 to `Npc.QiongMeiEr.TengQiaoFeiDu`, which intentionally retains an override.
2. Retain the 藤桥飞渡 `TestTerrainTargetOverride` call.
3. Delete only the three Formation `TestTerrainTargetOverride` calls.
4. After the existing `TestSingleAllyTarget` calls for 引水回元, 林影迷踪 and 林风拂阵, assert `TargetSpec.ModeOverrides.Num() == 0` for each.

In `GameXXKCardTextTest.cpp`, load those three definitions and assert their current-quality detail text:

```cpp
TestTrue(TEXT("引水回元 keeps a chosen-ally target"),
	YinShuiText.Contains(TEXT("单体友方")) && YinShuiText.Contains(TEXT("选择目标")));
TestTrue(TEXT("引水回元 names its base and water bonus"),
	YinShuiText.Contains(TEXT("所选目标获得6点内力"))
	&& YinShuiText.Contains(TEXT("地形为水岸或渡口"))
	&& YinShuiText.Contains(TEXT("全体友方获得4点内力"))
	&& YinShuiText.Contains(TEXT("出牌者抽1张牌")));
TestTrue(TEXT("林影迷踪 names base, forest spread and draw"),
	LinYingText.Contains(TEXT("所选目标获得2层灵动"))
	&& LinYingText.Contains(TEXT("全体友方获得1层灵动"))
	&& LinYingText.Contains(TEXT("出牌者抽1张牌")));
TestTrue(TEXT("林风拂阵 names base Agility and forest armor"),
	LinFengText.Contains(TEXT("所选目标获得1层灵动"))
	&& LinFengText.Contains(TEXT("全体友方获得4点护甲")));
TestFalse(TEXT("Formation chosen-ally text no longer advertises target replacement"),
	YinShuiText.Contains(TEXT("改为")) || LinYingText.Contains(TEXT("改为")) || LinFengText.Contains(TEXT("改为")));
```

- [ ] **Step 3: Tighten the 126-pair and board diagnostics before changing the catalog**

In `GameXXKFormationMasterTargetingDiagnosticTest.cpp`:

- Delete `IsKnownFormationTerrainTargetMismatch` and all warning branches.
- Require `ResolveCount == 18 * 7` directly.
- For 引水回元, 林影迷踪 and 林风拂阵 on every terrain, assert `EffectiveMode == SingleAlly`, `bRequiresManualSelection == true`, owner/Hero legal, enemy illegal.
- Replace the Water-shore board tail with the same interaction as Plain: clicking the hand card enters targeting, both `Hero` and `FormationOwner` highlight, `Enemy` does not, clicking the Hero proxy removes the card and exits targeting.

These changes must be made before the catalog edit so the test is genuinely RED against the old target overrides.

- [ ] **Step 4: Cold-build and verify RED**

Run UBT, then:

```text
GameXXK.Integration.FormationMaster.CardDesign
GameXXK.Data.CardCatalog
GameXXK.Integration.CardText
GameXXK.Diagnostics.FormationMasterTargeting
GameXXK.Diagnostics.FormationMasterBoardTargeting
```

Expected: assertions fail on the old values and the old Water/Forest target overrides. Compilation itself succeeds.

- [ ] **Step 5: Replace the eleven catalog definitions with the approved raw values**

Use these exact effects. These are raw catalog magnitudes; the existing quality rules produce the effective values tested above:

```cpp
		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.YinShuiHuiYuan"), TEXT("引水回元"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)) }, Frame, Pool, true);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinYingMiZong"), TEXT("林影迷踪"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Agility, 1, TerrainIs(EGameXXKCardTerrain::Forest)), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Forest)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.JieShanWeiZhang"), TEXT("借山为障"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 5, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cliff)), Effect(EGameXXKCardEffectType::RemoveStatus, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.CunZhaiYuanZhen"), TEXT("村寨援阵"), 2, 0, EGameXXKCardTargetMode::AllAllies,
			{ Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 6), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4), Effect(EGameXXKCardEffectType::Heal, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::AllAllies, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.HuiShengZhenSha"), TEXT("回声震杀"), 2, 6, EGameXXKCardTargetMode::SingleEnemy,
			{ Attack(120, EGameXXKCardEffectTarget::SelectedTarget), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 2, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cave)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark, 1, TerrainIs(EGameXXKCardTerrain::Cave)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenShaZhen"), TEXT("镇煞阵"), 3, 10, EGameXXKCardTargetMode::AllEnemies,
			{ Attack(80, EGameXXKCardEffectTarget::AllEnemies), Effect(EGameXXKCardEffectType::IgnoreDefense, EGameXXKCardEffectTarget::AllEnemies, 8, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Cave)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::AllEnemies, 2, EGameXXKCardStatus::Poison, 1, TerrainIs(EGameXXKCardTerrain::Cave)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShanMenFengSuo"), TEXT("山门封锁"), 1, 0, EGameXXKCardTargetMode::SingleEnemy,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Vulnerability), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 3, EGameXXKCardStatus::Vulnerability, 1, TerrainIs(EGameXXKCardTerrain::Cliff)), Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Mark, 1, TerrainIs(EGameXXKCardTerrain::Cliff)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ShuiJingZheGuang"), TEXT("水镜折光"), 1, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::SelectedTarget, 8), Effect(EGameXXKCardEffectType::GainMana, EGameXXKCardEffectTarget::SelectedTarget, 6, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)), Effect(EGameXXKCardEffectType::RemoveAnyDamageOverTime, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::WaterShore, EGameXXKCardTerrain::Ferry)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.LinFengFuZhen"), TEXT("林风拂阵"), 0, 0, EGameXXKCardTargetMode::SingleAlly,
			{ Effect(EGameXXKCardEffectType::ApplyStatus, EGameXXKCardEffectTarget::SelectedTarget, 1, EGameXXKCardStatus::Agility), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 4, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Forest)) }, Frame, Pool);

		AddCard(Cards, EGameXXKCardOwner::Profession, EGameXXKCardRarity::Permanent, EGameXXKCharacterRole::FormationMaster, OwnerId, nullptr,
			TEXT("Profession.FormationMaster.ZhenQiGuWu"), TEXT("阵旗鼓舞"), 1, 0, EGameXXKCardTargetMode::AllAllies,
			{ Modifier(EGameXXKCardBattleModifierTrigger::OnNextAttack, EGameXXKCardEffectType::BonusDamagePercent, EGameXXKCardEffectTarget::PlayedCard, 20, 1, 0, FGameXXKCardEffectCondition(), EGameXXKCardStatus::None, EGameXXKCardModifierRecipientScope::AllAllies, EGameXXKCardEffectTarget::AllAllies), Effect(EGameXXKCardEffectType::DrawCards, EGameXXKCardEffectTarget::CardOwner, 1, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)), Effect(EGameXXKCardEffectType::AddArmor, EGameXXKCardEffectTarget::AllAllies, 3, EGameXXKCardStatus::None, 1, TerrainIs(EGameXXKCardTerrain::Village)) }, Frame, Pool);
```

Leave 地脉借力's raw definition unchanged; the exact-value test locks its already-approved Rare effective values. Do not edit 观势、定阵、困阵、易位阵、八门轮转、万象归阵 or 四象连环.

- [ ] **Step 6: Cold-build and verify the new catalog GREEN**

Run UBT, then all Task 2 paths plus:

```text
GameXXK.Data.CardQuality
```

Expected: exact catalog and quality assertions pass, 126/126 Formation combinations resolve, both Plain and Water Board cases use explicit ally selection, and no obsolete Formation target-switch text remains.

- [ ] **Step 7: Commit the catalog and target contract**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardCatalog.cpp' 'Source/GameXXK/Private/Tests/GameXXKFormationMasterCardDesignTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardCatalogTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardTextTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKFormationMasterTargetingDiagnosticTest.cpp'
git commit -m "feat: strengthen formation terrain cards"
```

### Task 3: Verify real mismatched/matched state deltas

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKFormationMasterEffectsTest.cpp`

- [ ] **Step 1: Add reusable current-quality runtime fixtures**

The new test path is `GameXXK.Integration.FormationMaster.Effects`. Its card-instance helper must create ten copies, set `CurrentQuality = FGameXXKCardQualityRules::GetCardBaseQuality(CardId)`, and use `FormationOwner` as owner. Every runtime contains FormationOwner, Hero, EnemyA and EnemyB, with 500 HP, 20 Attack, 100 max mana and stable orders 1, 2, 10, 11. FormationOwner starts at 20 mana and Hero at 0 unless a row explicitly overrides them. Initialize each scenario separately so no state leaks between cases.

Use these exact access helpers:

```cpp
FGameXXKCardCombatUnit* FindFormationEffectUnit(
	FGameXXKCardBattleRuntime& Runtime,
	const FName UnitId)
{
	return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == UnitId;
	});
}

bool ResolveFormationEffectCard(
	FGameXXKCardBattleRuntime& Runtime,
	const FName SelectedTargetId,
	FGameXXKCardPlayResult& OutResult,
	FString& OutError)
{
	return !Runtime.Deck.Hand.IsEmpty()
		&& GameXXKCardRules::ResolveCardPlay(
			Runtime,
			Runtime.Deck.Hand[0].InstanceId,
			SelectedTargetId,
			OutResult,
			&OutError);
}
```

- [ ] **Step 2: Assert the exact runtime scenarios**

Implement these independent scenarios; values are post-quality and post-cost:

| Card | Terrain | Setup | Required result |
|---|---|---|---|
| 引水回元 | Plain | FormationOwner/Hero mana 0; select Hero | Hero mana 6, owner mana 0, hand 4 |
| 引水回元 | WaterShore and Ferry separately | same | Hero mana 10, owner mana 4, hand 5 |
| 林影迷踪 | Plain | select Hero | Hero Agility 2, owner Agility 0, hand 4 |
| 林影迷踪 | Forest | select Hero | Hero Agility 3, owner Agility 1, hand 5 |
| 林风拂阵 | Plain | select Hero | Hero Agility 1, both allies armor 0 |
| 林风拂阵 | Forest | select Hero | Hero Agility 1, both allies armor 4 |
| 山门封锁 | Plain | select EnemyA | EnemyA Vulnerability 1, Mark 0 |
| 山门封锁 | Cliff | select EnemyA | EnemyA Vulnerability 4, Mark 1 |
| 村寨援阵 | Plain | both allies HP 50, Bleed 1, Poison 2 | both HP 62, armor 8, Bleed 1, Poison 2 |
| 村寨援阵 | Village | same | both HP 70, armor 16, Bleed 0, Poison 1 |
| 镇煞阵 | Plain | both enemies Defense 12 | two results, each RequestedDamage 64 and DamageAfterDefense 52; both enemies Vulnerability 3, Poison 0 |
| 镇煞阵 | Cave | both enemies Defense 12 | two results, each RequestedDamage 64 and DamageAfterDefense 60; both enemies Vulnerability 3, Poison 4 |

Also add one focused scenario per remaining changed/approved card:

- 借山为障: Plain leaves preloaded Vulnerability 2 and gives armor 5; Cliff leaves Vulnerability 1 and gives armor 10 to both allies.
- 回声震杀: Plain produces one 48 requested-damage packet and no statuses; Cave produces the same damage, then Vulnerability 3 and Mark 2 on the surviving target.
- 水镜折光: Plain gives selected Hero armor 16 and leaves Poison 3; WaterShore and Ferry each additionally give mana 8 and leave Poison 1.
- 阵旗鼓舞: Plain registers one +20% `OnNextAttack` modifier whose recipients contain both allies; Village also ends with hand 6 and armor 6 on both allies.
- 地脉借力: every terrain still produces one 40 requested-damage packet; Plain hand 6, Cliff Vulnerability 3, Forest Mark 3, WaterShore/Ferry owner mana +6, Village armor 8 on both allies, Cave Poison 3. Assert only the matching branch changes state in each of the seven independent runtimes.

For full-health, full-mana and no-DoT boundaries, repeat 村寨援阵 and 水镜折光 once with capped resources/no DoT and require successful resolution, capped values, card removal, and no error.

- [ ] **Step 3: Run the effect test and surrounding runtime suite**

Run UBT, then:

```text
GameXXK.Integration.FormationMaster.Effects
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardQuality
GameXXK.Data.MarkRules
```

Expected: all pass; every wrong-terrain case retains its base effect, every correct-terrain case adds the approved bonus, and capped/no-op effects never invalidate the whole card.

- [ ] **Step 4: Commit the state-delta regression**

```powershell
git add -- 'Source/GameXXK/Private/Tests/GameXXKFormationMasterEffectsTest.cpp'
git commit -m "test: lock formation terrain effect deltas"
```

### Task 4: Make Formation cards participate in terrain-card synergies

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKFormationTerrainSynergyTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2967-3013,4287-4375`

- [ ] **Step 1: Write the failing Formation synergy test**

Create `GameXXK.Integration.FormationMaster.TerrainSynergy` with four independent current-quality runtimes:

1. **Double bonus:** WaterShore 引水回元, FormationOwner/Hero start at 0 mana, Hero selected, ten copies in the deck, and FormationOwner has one `TerrainBonusDouble`. Expected Hero mana 14 (`6 + 4 + 4`), owner mana 8, hand 6 (`5 - 1 + 1 + 1`), and the doubling status 0.
2. **Cost reduction:** Plain 引水回元 with one `NextTerrainCardEnergyReduction` on FormationOwner. Expected preview energy cost 0; after selecting Hero and resolving, the status is 0.
3. **Free cost:** Plain 回声震杀 with one `NextTerrainCardFree` on FormationOwner and at least 6 mana. Expected its ordinary two-energy preview cost becomes 0; after selecting EnemyA and resolving, the status is 0.
4. **Non-terrain control:** Plain 定阵 with one stack each of `TerrainBonusDouble`, `NextTerrainCardEnergyReduction`, and `NextTerrainCardFree`. Expected preview energy cost remains 1, both allies gain exactly 5 armor, and all three statuses remain 1.

The first three cases must be RED against the current `IsRouteTerrainCard` predicate; the control should already pass.

- [ ] **Step 2: Verify RED**

Run UBT, then `GameXXK.Integration.FormationMaster.TerrainSynergy` with report `FormationTerrainSynergy_Red`.

Expected: 引水回元 resolves only once per terrain bonus and previews at cost 1; 回声震杀 previews at cost 2; all three terrain statuses remain unused because the cards are not owned by Route.

- [ ] **Step 3: Replace ownership-based classification with semantic classification**

Replace `IsRouteTerrainCard` with:

```cpp
	bool IsTerrainCard(const FGameXXKCardDefinition& Definition)
	{
		const bool bLegacyRouteTerrainCard = Definition.Owner == EGameXXKCardOwner::Route
			&& Definition.AcquisitionKey == TEXT("Route.Terrain");
		return bLegacyRouteTerrainCard
			|| Definition.Effects.ContainsByPredicate([](const FGameXXKCardEffect& Effect)
			{
				return Effect.Condition.Type == EGameXXKCardEffectConditionType::TerrainIsAny;
			});
	}
```

Use `IsTerrainCard` in both `CollectTerrainCardCostStatusOwners` and `BuildTerrainAmplifiedDefinition`. The legacy acquisition-key branch is required for `Route.Terrain.DiMaiHuiXiang`, which has no `TerrainIsAny` effect but is intentionally still a terrain card.

Do not special-case Formation owner IDs or card IDs. The semantic rule also correctly covers NPC/other terrain-conditioned cards, including 藤桥飞渡.

- [ ] **Step 4: Verify GREEN and legacy behavior**

Run UBT, then:

```text
GameXXK.Integration.FormationMaster.TerrainSynergy
GameXXK.Integration.TerrainConditionalResolution
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardBattleRuntime.QualityResolution
GameXXK.Data.CardBattleRuntime.QualityTerrainComposition
GameXXK.Data.CardRules.CardInstanceQualityValidation
```

Expected: Formation double/reduction/free cases pass, 定阵 remains unaffected, Route terrain free/reduction/doubling tests still pass, and `DoubleTerrainBonus` duplicates conditioned attack packets together with their attachments but never duplicates unconditional base effects.

- [ ] **Step 5: Commit semantic terrain classification**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKFormationTerrainSynergyTest.cpp'
git commit -m "fix: recognize formation terrain synergies"
```

### Task 5: Run the complete gameplay-rule gate

**Files:**
- Verify: every source/test file listed in this plan and the prerequisite Mark plan
- Generate only: fresh directories under `Saved/Automation`

- [ ] **Step 1: Save/close any editor and cold-build from current HEAD**

Run the lifecycle helper, then UBT.

Expected: MCP-safe close or no running editor; UBT exit code 0 with `-NoHotReload`.

- [ ] **Step 2: Run every focused group in fresh commandlets**

Run:

```text
GameXXK.Integration.TerrainConditionalResolution
GameXXK.Integration.FormationMaster
GameXXK.Diagnostics.FormationMasterTargeting
GameXXK.Diagnostics.FormationMasterBoardTargeting
GameXXK.Data.CardCatalog
GameXXK.Data.CardQuality
GameXXK.Integration.CardText
GameXXK.Data.CardBattleRuntime
GameXXK.Data.CardCombatRules
GameXXK.Data.MarkRules
GameXXK.Integration.MarkCardCompatibility
GameXXK.Battle.EnemyIntentRules
GameXXK.Battle.EnemyMechanics
GameXXK.MVP.Battle.SceneActors
```

Expected: every `index.json` reports zero failures. Formation diagnostics report 126 previews and 126 resolutions, with no known-mismatch warnings.

- [ ] **Step 3: Check deterministic replay**

Run:

```text
GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay
```

Expected: all 267 cases match their same-process replay fingerprints. The absolute pre-change fingerprint is allowed to change because card rules intentionally changed; same-build replay equality is not.

- [ ] **Step 4: Inspect diff boundaries**

Run:

```powershell
git diff --check
git status --short
git diff --stat
git diff -- Source/GameXXK/Private/GameXXKCardRules.cpp Source/GameXXK/Private/GameXXKCardCatalog.cpp Source/GameXXK/Private/Tests
```

Expected: no whitespace errors, no art/level/PaperZD/camera/HD2D asset changes, and no `Saved` outputs staged.

### Task 6: Run one post-change balance observation and the real MVP flow

**Files:**
- Generate only: the newly created `Saved/BalanceObservation/run_*_001/*` directory
- Generate only: `Saved/HarnessReports/formation-mark-mvp.json`

- [ ] **Step 1: Run one strict 2,400-case observation batch**

With the editor closed, run:

```powershell
python -B scripts/run_card_balance_observation.py --once --timeout-seconds 600
```

Expected: one new timestamped `run_*_001` directory, a 2,400-row `cases.csv`, a passing automation report, and `run_summary.json`. Record the emitted run id.

- [ ] **Step 2: Read the new run, not a mixed-version aggregate**

Resolve the newest completed per-run summary directly:

```powershell
$newRunSummary = Get-ChildItem -LiteralPath 'Saved/BalanceObservation' -Directory -Filter 'run_*_001' |
    Sort-Object LastWriteTimeUtc |
    Select-Object -Last 1 |
    ForEach-Object { Join-Path $_.FullName 'run_summary.json' }
if (-not $newRunSummary -or -not (Test-Path -LiteralPath $newRunSummary)) {
    throw 'The post-change observation did not produce run_summary.json.'
}
Get-Content -LiteralPath $newRunSummary
```

Report:

- victory/defeat/stalemate/error counts and resolved win rate;
- Mark `produced`, `consumed`, and utilization ratio from `status_utilization`;
- top damage/healing/armor sources relevant to FormationMaster/Hunter/marked enemies;
- recurring stalemate identities;
- confirmation that all 2,400 cases were attempted.

Do not claim this existing CSV directly measures per-card play rate or exact incremental Mark damage; those require a separately approved observation-schema extension. Do not auto-adjust card/enemy values from one greedy-policy batch.

- [ ] **Step 3: Run known-stale legacy target tests as diagnostics only**

Run `GameXXK.RouteBalance.FinalCandidateTargets` once and record its result. The accepted baseline already documents this test as stale because it hard-codes the retired high-attack profile and can hit `MaxRounds`. A failure is not hidden and is not a reason to restore old enemy attacks, alter the approved card values, or change the 15% Mark rule in this plan.

- [ ] **Step 4: Launch a stable editor, smoke MCP, then run the real player flow**

Use the project's normal editor launch workflow. Once UE MCP is available, run:

```powershell
python -B scripts/ue_mcp_smoke.py --report Saved/HarnessReports/formation-mark-mcp-smoke.json
python -B scripts/gamexxk_real_play_flow_mcp.py --timeout 180 --report Saved/HarnessReports/formation-mark-mvp.json
```

Expected: MCP smoke is `ok=true`; the real flow reaches player-facing main menu, opens `/Game/GameXXK/Maps/L_QingshanInn`, shows town UI, accepts the quest through `F`, preserves follower/NPC state through manual save, enters the route map, and opens the battle screen. The harness stops PIE and cleans its default test save.

- [ ] **Step 5: Final evidence and commit hygiene**

Run:

```powershell
git diff --check
git status --short
git log -6 --oneline
```

Expected: all intended commits are present; `Saved` evidence remains untracked/ignored and unstaged; user-owned unrelated files are untouched.

## Self-review checklist

- [ ] All eleven approved Formation cards match the confirmed current-quality table exactly.
- [ ] 引水回元、林影迷踪、林风拂阵 remain `SingleAlly` with no terrain target overrides.
- [ ] Wrong terrain loses only the conditional bonus; the base effect always resolves.
- [ ] Correct terrain adds the bonus after the base and never replaces it.
- [ ] WaterShore and Ferry behave identically for 引水回元、水镜折光 and 地脉借力.
- [ ] 镇煞阵's conditional `IgnoreDefense` stays attached to the attack packet.
- [ ] `RemoveAnyDamageOverTime` respects effective-quality magnitude and harmless no-op boundaries.
- [ ] False target-independent conditions are skipped before target resolution without consuming state.
- [ ] Target-dependent conditions still evaluate against each real target and atomic rollback remains intact.
- [ ] 藤桥飞渡 resolves on Cliff and Forest without inventing a selected target.
- [ ] Formation terrain cards use free/reduction/doubling windows; 定阵 does not.
- [ ] `DoubleTerrainBonus` copies only `TerrainIsAny` effects, never unconditional base effects.
- [ ] Formation diagnostics are strict 126/126 with no warning allowlist.
- [ ] Mark prerequisite tests, enemy intent tests, deterministic replay, and real MVP flow all remain covered.
- [ ] No old balance gate failure is concealed or “fixed” by reverting the accepted 250% enemy policy.
