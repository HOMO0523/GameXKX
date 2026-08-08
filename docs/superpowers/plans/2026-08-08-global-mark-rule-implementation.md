# Global Mark Rule Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Turn Mark into a symmetric global direct-hit rule that grants a fixed 15% bonus per marked hit, consumes one stack, preserves existing Mark-gated card rewards, and exposes accurate player-facing text and audit data.

**Architecture:** The common direct-damage resolver remains the single authority for player, enemy, group, redirected, and reactive hits. It reads Mark from the final resolved receiver, combines its fixed bonus additively with Vulnerability, and records the result in `FGameXXKCardDamageResult`; a card-start status snapshot separately preserves existing `TargetHasStatus(Mark)` clauses across the same card play. UI text reads the shared named percentage constant so rules and tooltip cannot drift.

**Tech Stack:** Unreal Engine 5.8 C++, UE Automation Tests, UBT cold builds, commandlet test reports, existing GameXXK card runtime and MCP-safe editor lifecycle scripts.

---

## File map

- Modify `Source/GameXXK/Public/GameXXKCardRules.h`: declare the one authoritative `MarkDirectDamageBonusPercent` constant.
- Modify `Source/GameXXK/Public/GameXXKCardTypes.h`: add Mark audit fields to `FGameXXKCardDamageResult` without changing serialized status enum values.
- Modify `Source/GameXXK/Private/GameXXKCardRules.cpp`: apply and consume Mark in the shared damage path; capture card-start Mark conditions.
- Create `Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp`: focused direct-hit, avoidance, redirection, multi-hit, and non-direct tests.
- Create `Source/GameXXK/Private/Tests/GameXXKMarkCardCompatibilityTest.cpp`: real catalog-card tests for post-hit rewards, five-stack gates, and on-hit Mark timing.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp`: extend the atomic rejected-result sentinel for the new audit fields.
- Modify `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp`: prove a reactive counterattack reads and consumes Mark through the same damage resolver.
- Modify `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp`: lock Graymane multi-hit consumption and enemy-side symmetry.
- Modify `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp`: replace the obsolete passive-tag Mark tooltip.
- Modify `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp`: verify the exact player-facing Mark mechanics.

## Shared verification commands

Use the following cold build throughout this plan:

```powershell
& 'D:\UE_5.8\Engine\Build\BatchFiles\Build.bat' GameXXKEditor Win64 Development '-Project=D:\UE5 demo\GameXXK\GameXXK.uproject' -WaitMutex -NoHotReload
```

Define this helper once in the implementation PowerShell session, then call it with the exact test path and a unique report name listed by each task:

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

Success means the report `index.json` records zero failed tests. Do not infer success from localized log text alone.

### Task 0: Establish a clean, cold baseline

**Files:**
- Read: `AGENTS.md`
- Read: `docs/design/2026-08-08-formation-master-mark-rules-design.md`
- Inspect: `Source/GameXXK/Public/GameXXKCardTypes.h`
- Inspect: `Source/GameXXK/Private/GameXXKCardRules.cpp`

- [ ] **Step 1: Confirm only intended tracked files can be touched**

Run:

```powershell
git status --short
git diff -- Source/GameXXK docs/design docs/superpowers/plans
```

Expected: user-owned untracked art/build outputs may exist; the Mark implementation paths above have no unrelated tracked edits. Stop if a tracked edit overlaps one of those files.

- [ ] **Step 2: Save and close a running editor through the project lifecycle helper**

Run:

```powershell
python -B -c "from scripts.ue_tdd_pipeline import save_running_editor_before_close, kill_editor; import sys; ok=save_running_editor_before_close(); kill_editor() if ok else None; sys.exit(0 if ok else 1)"
```

Expected: either no editor is running, or MCP reports `dirty_after=[]` before the editor closes. If MCP is unavailable while an editor is running, the command exits nonzero and must not be bypassed.

- [ ] **Step 3: Cold-build the unchanged baseline**

Run the shared UBT command.

Expected: exit code 0 and no Live Coding or Hot Reload invocation.

- [ ] **Step 4: Run the existing focused baseline**

Run commandlet reports for:

```text
GameXXK.Data.CardCombatRules
GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntOnlyAmplifiesMarkedCatalogDirectDamage
GameXXK.MVP.Battle.SceneActors
```

Expected: all three current tests pass before the new RED tests are introduced.

### Task 1: Add the core fixed Mark amplification and one-stack consumption

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp`
- Modify: `Source/GameXXK/Public/GameXXKCardRules.h:5-15`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2438-2466`

- [ ] **Step 1: Write the failing direct-damage rule test**

Create `GameXXKMarkRulesTest.cpp` with this initial test:

```cpp
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkDirectDamageRulesTest,
	"GameXXK.Data.MarkRules.DirectDamage",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeMarkUnit(
		const TCHAR* UnitId,
		const EGameXXKCardTargetSide Side,
		const int32 HP,
		const int32 StableSortOrder)
	{
		FGameXXKCardCombatUnit Unit;
		Unit.UnitId = FName(UnitId);
		Unit.Side = Side;
		Unit.bLiving = true;
		Unit.HP = HP;
		Unit.MaxHP = HP;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	FGameXXKCardCombatUnit* FindMarkUnit(TArray<FGameXXKCardCombatUnit>& Units, const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}

	bool ResolveMarkedHit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const int32 RequestedDamage,
		FGameXXKCardDamageResult& OutResult)
	{
		FGameXXKCardDamageContext Context;
		Context.SourceUnitId = TEXT("Attacker");
		Context.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
		TArray<FGameXXKCardGuardLinkRuntime> GuardLinks;
		return GameXXKCardRules::ApplyCombatDirectDamage(
			Units, GuardLinks, Context, TEXT("Target"), RequestedDamage, OutResult);
	}
}

bool FGameXXKMarkDirectDamageRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> FiveMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	TestEqual(TEXT("five Mark stacks can be prepared"),
		GameXXKCardRules::AddCombatStatus(FiveMarkUnits[1], EGameXXKCardStatus::Mark, 5), 5);
	FGameXXKCardDamageResult FiveMarkResult;
	TestTrue(TEXT("a five-Mark direct hit resolves"), ResolveMarkedHit(FiveMarkUnits, 100, FiveMarkResult));
	TestEqual(TEXT("five stacks still grant one fixed fifteen-percent hit"), FiveMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("one hit consumes only one of five Mark stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(FiveMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 4);

	TArray<FGameXXKCardCombatUnit> OneMarkUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(OneMarkUnits[1], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageResult OneMarkResult;
	TestTrue(TEXT("a one-Mark direct hit resolves"), ResolveMarkedHit(OneMarkUnits, 100, OneMarkResult));
	TestEqual(TEXT("one Mark stack grants the same fixed fifteen percent"), OneMarkResult.DamageAfterVulnerability, 115);
	TestEqual(TEXT("the final Mark stack is consumed"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(OneMarkUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 0);

	TArray<FGameXXKCardCombatUnit> CombinedUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Vulnerability, 2);
	GameXXKCardRules::AddCombatStatus(CombinedUnits[1], EGameXXKCardStatus::Mark, 3);
	FGameXXKCardDamageResult CombinedResult;
	TestTrue(TEXT("Vulnerability and Mark resolve together"), ResolveMarkedHit(CombinedUnits, 100, CombinedResult));
	TestEqual(TEXT("two Vulnerability plus Mark use one additive thirty-five-percent multiplier"),
		CombinedResult.DamageAfterVulnerability, 135);
	FGameXXKCardCombatUnit* CombinedTarget = FindMarkUnit(CombinedUnits, TEXT("Target"));
	TestEqual(TEXT("the hit clears all Vulnerability"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Vulnerability), 0);
	TestEqual(TEXT("the hit removes only one Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*CombinedTarget, EGameXXKCardStatus::Mark), 2);
	return true;
}

#endif
```

- [ ] **Step 2: Cold-build and run the new test to verify RED**

Run the shared UBT command, then run `GameXXK.Data.MarkRules.DirectDamage` with report name `MarkRules_Core_Red`.

Expected: UBT succeeds; automation fails because damage remains 100/120 and Mark remains unconsumed.

- [ ] **Step 3: Add the named rule constant**

Inside `namespace GameXXKCardRules` in `GameXXKCardRules.h`, add:

```cpp
	/** Fixed additive direct-hit bonus while the resolved target has at least one Mark stack. */
	inline constexpr int32 MarkDirectDamageBonusPercent = 15;
```

- [ ] **Step 4: Implement the minimal shared-resolver behavior**

Replace the Vulnerability-only amplification block in `ApplyCombatDirectDamageInternal` with:

```cpp
		const bool bDirectAttack = IsDirectAttackDamageKind(Context.Kind);
		const int32 VulnerabilityStacks = bDirectAttack
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Vulnerability)
			: 0;
		const int32 MarkStacks = bDirectAttack
			? GetCombatStatusStacksInternal(*ResolvedTarget, EGameXXKCardStatus::Mark)
			: 0;
		const int32 MarkBonusPercent = MarkStacks > 0
			? GameXXKCardRules::MarkDirectDamageBonusPercent
			: 0;
		const int64 AmplifiedDamage = static_cast<int64>(NewResult.DamageAfterDefense)
			* static_cast<int64>(100 + 10 * VulnerabilityStacks + MarkBonusPercent)
			/ 100;
		NewResult.DamageAfterVulnerability = static_cast<int32>(FMath::Min<int64>(MAX_int32, AmplifiedDamage));
		if (VulnerabilityStacks > 0)
		{
			GameXXKCardRules::ConsumeCombatStatus(*ResolvedTarget, EGameXXKCardStatus::Vulnerability, MAX_int32);
		}
		if (MarkStacks > 0)
		{
			GameXXKCardRules::ConsumeCombatStatus(*ResolvedTarget, EGameXXKCardStatus::Mark, 1);
		}
```

Keep this block after Agility avoidance and defense mitigation, but before armor absorption. Do not change DoT, self-loss, or environmental paths.

- [ ] **Step 5: Cold-build and verify GREEN**

Run UBT, then `GameXXK.Data.MarkRules.DirectDamage` with report name `MarkRules_Core_Green`.

Expected: one successful automation test; 100 damage becomes 115 with any positive Mark count, the combined case becomes 135, and only one Mark is removed per hit.

- [ ] **Step 6: Commit the core rule**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardRules.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp'
git commit -m "feat: add global mark direct-hit amplification"
```

### Task 2: Lock edge cases and expose Mark damage audit fields

**Files:**
- Modify: `Source/GameXXK/Public/GameXXKCardTypes.h:973-1018`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:2438-2470`
- Modify: `Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp:355-380`
- Modify: `Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp:582-597`

- [ ] **Step 1: Add compile-failing audit assertions and behavioral edge tests**

Extend `GameXXKMarkRulesTest.cpp` with a second automation test named `GameXXK.Data.MarkRules.Edges`. It must execute these exact cases:

```cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkDamageEdgeRulesTest,
	"GameXXK.Data.MarkRules.Edges",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMarkDamageEdgeRulesTest::RunTest(const FString& Parameters)
{
	TArray<FGameXXKCardCombatUnit> MultiUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(MultiUnits[1], EGameXXKCardStatus::Mark, 2);
	TArray<FGameXXKCardDamageResult> Hits;
	for (int32 Index = 0; Index < 3; ++Index)
	{
		FGameXXKCardDamageResult& Hit = Hits.AddDefaulted_GetRef();
		TestTrue(FString::Printf(TEXT("sequential hit %d resolves"), Index), ResolveMarkedHit(MultiUnits, 100, Hit));
	}
	TestEqual(TEXT("first marked hit is amplified"), Hits[0].DamageAfterVulnerability, 115);
	TestEqual(TEXT("second marked hit is amplified"), Hits[1].DamageAfterVulnerability, 115);
	TestEqual(TEXT("third unmarked hit is not amplified"), Hits[2].DamageAfterVulnerability, 100);
	TestEqual(TEXT("first hit records two starting Mark stacks"), Hits[0].MarkStacksBeforeHit, 2);
	TestEqual(TEXT("first hit records the named Mark bonus"), Hits[0].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("first hit records one consumed Mark"), Hits[0].MarkStacksConsumed, 1);
	TestEqual(TEXT("third hit records no Mark consumption"), Hits[2].MarkStacksConsumed, 0);

	TArray<FGameXXKCardCombatUnit> AvoidUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(AvoidUnits[1], EGameXXKCardStatus::Mark, 1);
	GameXXKCardRules::AddCombatStatus(AvoidUnits[1], EGameXXKCardStatus::Agility, 1);
	FGameXXKCardDamageResult Avoided;
	TestTrue(TEXT("an agile marked target resolves the avoided packet"), ResolveMarkedHit(AvoidUnits, 100, Avoided));
	TestTrue(TEXT("Agility reports the avoid"), Avoided.bAvoidedByAgility);
	TestEqual(TEXT("an avoided hit records no Mark bonus"), Avoided.MarkDamageBonusPercent, 0);
	TestEqual(TEXT("an avoided hit preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(AvoidUnits, TEXT("Target")), EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> ArmorUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(ArmorUnits[1], EGameXXKCardStatus::Mark, 1);
	GameXXKCardRules::AddCombatArmor(ArmorUnits[1], 99);
	FGameXXKCardDamageResult ArmorHit;
	TestTrue(TEXT("a marked armor-only hit resolves"), ResolveMarkedHit(ArmorUnits, 80, ArmorHit));
	TestEqual(TEXT("Mark amplifies damage before armor"), ArmorHit.ArmorAbsorbed, 92);
	TestEqual(TEXT("armor can absorb the full amplified hit"), ArmorHit.HealthDamage, 0);
	TestEqual(TEXT("an armor-only landed hit consumes Mark"), ArmorHit.MarkStacksConsumed, 1);

	TArray<FGameXXKCardCombatUnit> RedirectUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Enemy, 500, 10),
		MakeMarkUnit(TEXT("Protected"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("Guardian"), EGameXXKCardTargetSide::Party, 500, 2)};
	GameXXKCardRules::AddCombatStatus(RedirectUnits[1], EGameXXKCardStatus::Mark, 1);
	GameXXKCardRules::AddCombatStatus(RedirectUnits[2], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardGuardLinkRuntime Link;
	Link.GuardianUnitId = TEXT("Guardian");
	Link.ProtectedUnitId = TEXT("Protected");
	Link.Stacks = 1;
	Link.RedirectPolicy = EGameXXKCardGuardRedirectPolicy::RedirectNextSingleTargetDirectAttackToGuardian;
	TArray<FGameXXKCardGuardLinkRuntime> Links = {Link};
	FGameXXKCardDamageContext RedirectContext;
	RedirectContext.SourceUnitId = TEXT("Attacker");
	RedirectContext.Kind = EGameXXKCardDamageKind::SingleTargetAttack;
	FGameXXKCardDamageResult Redirected;
	TestTrue(TEXT("the redirected marked hit resolves"), GameXXKCardRules::ApplyCombatDirectDamage(
		RedirectUnits, Links, RedirectContext, TEXT("Protected"), 100, Redirected));
	TestEqual(TEXT("the guardian receives the Mark-amplified damage"), Redirected.HealthDamage, 115);
	TestEqual(TEXT("the guardian Mark is consumed"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(RedirectUnits, TEXT("Guardian")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the original protected target keeps its Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(RedirectUnits, TEXT("Protected")), EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> GroupUnits = {
		MakeMarkUnit(TEXT("Attacker"), EGameXXKCardTargetSide::Party, 500, 1),
		MakeMarkUnit(TEXT("MarkedEnemy"), EGameXXKCardTargetSide::Enemy, 500, 10),
		MakeMarkUnit(TEXT("UnmarkedEnemy"), EGameXXKCardTargetSide::Enemy, 500, 11)};
	GameXXKCardRules::AddCombatStatus(GroupUnits[1], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageContext GroupContext;
	GroupContext.SourceUnitId = TEXT("Attacker");
	GroupContext.Kind = EGameXXKCardDamageKind::GroupAttack;
	TArray<FGameXXKCardGuardLinkRuntime> GroupLinks;
	FGameXXKCardDamageResult MarkedGroupHit;
	FGameXXKCardDamageResult UnmarkedGroupHit;
	TestTrue(TEXT("group attack resolves independently on the marked enemy"),
		GameXXKCardRules::ApplyCombatDirectDamage(GroupUnits, GroupLinks, GroupContext, TEXT("MarkedEnemy"), 100, MarkedGroupHit));
	TestTrue(TEXT("group attack resolves independently on the unmarked enemy"),
		GameXXKCardRules::ApplyCombatDirectDamage(GroupUnits, GroupLinks, GroupContext, TEXT("UnmarkedEnemy"), 100, UnmarkedGroupHit));
	TestEqual(TEXT("the marked group member gets the fixed bonus"), MarkedGroupHit.DamageAfterVulnerability, 115);
	TestEqual(TEXT("the unmarked group member keeps base damage"), UnmarkedGroupHit.DamageAfterVulnerability, 100);
	TestEqual(TEXT("group damage consumes only the marked member's stack"),
		GameXXKCardRules::GetCombatStatusStacks(*FindMarkUnit(GroupUnits, TEXT("MarkedEnemy")), EGameXXKCardStatus::Mark), 0);

	TArray<FGameXXKCardCombatUnit> SelfLossUnits = {
		MakeMarkUnit(TEXT("Self"), EGameXXKCardTargetSide::Party, 500, 1)};
	GameXXKCardRules::AddCombatStatus(SelfLossUnits[0], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageContext SelfLossContext;
	SelfLossContext.SourceUnitId = TEXT("Self");
	SelfLossContext.Kind = EGameXXKCardDamageKind::SelfHealthLoss;
	TArray<FGameXXKCardGuardLinkRuntime> SelfLossLinks;
	FGameXXKCardDamageResult SelfLossResult;
	TestTrue(TEXT("self health loss resolves on a marked unit"), GameXXKCardRules::ApplyCombatDirectDamage(
		SelfLossUnits, SelfLossLinks, SelfLossContext, TEXT("Self"), 100, SelfLossResult));
	TestEqual(TEXT("self health loss is not Mark-amplified"), SelfLossResult.DamageAfterVulnerability, 100);
	TestEqual(TEXT("self health loss preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(SelfLossUnits[0], EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> EnvironmentUnits = {
		MakeMarkUnit(TEXT("EnvironmentTarget"), EGameXXKCardTargetSide::Party, 500, 1)};
	GameXXKCardRules::AddCombatStatus(EnvironmentUnits[0], EGameXXKCardStatus::Mark, 1);
	FGameXXKCardDamageContext EnvironmentContext;
	EnvironmentContext.Kind = EGameXXKCardDamageKind::EnvironmentalHealthLoss;
	TArray<FGameXXKCardGuardLinkRuntime> EnvironmentLinks;
	FGameXXKCardDamageResult EnvironmentResult;
	TestTrue(TEXT("environmental health loss resolves on a marked unit"), GameXXKCardRules::ApplyCombatDirectDamage(
		EnvironmentUnits, EnvironmentLinks, EnvironmentContext, TEXT("EnvironmentTarget"), 100, EnvironmentResult));
	TestEqual(TEXT("environmental health loss is not Mark-amplified"), EnvironmentResult.DamageAfterVulnerability, 100);
	TestEqual(TEXT("environmental health loss preserves Mark"),
		GameXXKCardRules::GetCombatStatusStacks(EnvironmentUnits[0], EGameXXKCardStatus::Mark), 1);

	TArray<FGameXXKCardCombatUnit> DotUnits = {
		MakeMarkUnit(TEXT("Target"), EGameXXKCardTargetSide::Enemy, 500, 10)};
	GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Mark, 1);
	GameXXKCardRules::AddCombatStatus(DotUnits[0], EGameXXKCardStatus::Burn, 1);
	TArray<FGameXXKCardGuardLinkRuntime> DotLinks;
	int32 DotDamage = 0;
	TestTrue(TEXT("DoT resolves on a marked target"), GameXXKCardRules::ApplyCombatEndPhaseDot(
		DotUnits, DotLinks, TEXT("Target"), DotDamage));
	TestEqual(TEXT("DoT leaves Mark untouched"),
		GameXXKCardRules::GetCombatStatusStacks(DotUnits[0], EGameXXKCardStatus::Mark), 1);

	FGameXXKCardCombatUnit PersistentMark = MakeMarkUnit(
		TEXT("PersistentMark"), EGameXXKCardTargetSide::Party, 500, 1);
	TestEqual(TEXT("Mark remains capped at five"),
		GameXXKCardRules::AddCombatStatus(PersistentMark, EGameXXKCardStatus::Mark, 9), 5);
	GameXXKCardRules::BeginCombatUnitPhase(PersistentMark);
	TestEqual(TEXT("Mark has no natural phase decay"),
		GameXXKCardRules::GetCombatStatusStacks(PersistentMark, EGameXXKCardStatus::Mark), 5);
	return true;
}
```

- [ ] **Step 2: Cold-build to verify RED at the type boundary**

Run UBT.

Expected: compilation fails because `FGameXXKCardDamageResult` does not yet define `MarkStacksBeforeHit`, `MarkDamageBonusPercent`, or `MarkStacksConsumed`.

- [ ] **Step 3: Add the three non-save audit fields**

Add immediately after `DamageAfterVulnerability` in `FGameXXKCardDamageResult`:

```cpp
	/** Mark stacks on the final resolved receiver immediately before this hit. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkStacksBeforeHit = 0;

	/** Fixed additive Mark percentage applied to this hit, or zero. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkDamageBonusPercent = 0;

	/** Mark stacks removed by this hit; currently zero or one. */
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	int32 MarkStacksConsumed = 0;
```

Update the comment on `DamageAfterVulnerability` to state that it is the compatibility field containing all direct-hit status amplification, including Mark.

- [ ] **Step 4: Populate the audit fields only on a landed direct hit**

In the core block from Task 1, add:

```cpp
		NewResult.MarkStacksBeforeHit = MarkStacks;
		NewResult.MarkDamageBonusPercent = MarkBonusPercent;
		if (MarkStacks > 0)
		{
			NewResult.MarkStacksConsumed = GameXXKCardRules::ConsumeCombatStatus(
				*ResolvedTarget,
				EGameXXKCardStatus::Mark,
				1);
		}
```

Remove the earlier unrecorded Mark-consumption call. Leave all three fields at zero on Agility avoidance and non-direct damage.

- [ ] **Step 5: Extend the invalid-result sentinel**

In `GameXXKCardCombatRulesTest.cpp`, set these values before copying `InvalidHitSentinel`:

```cpp
	InvalidHit.MarkStacksBeforeHit = 5;
	InvalidHit.MarkDamageBonusPercent = 15;
	InvalidHit.MarkStacksConsumed = 1;
```

The existing `CompareScriptStruct` assertion must still prove rejected damage preserves the caller-owned result atomically.

- [ ] **Step 6: Prove reactive direct damage uses the same Mark rule**

In the existing `LifestealReflectRuntime` fixture in `GameXXKCardBattleRuntimeTest.cpp`, add one Mark to Blade after initialization and before card resolution:

```cpp
	TestEqual(TEXT("the reactive-damage fixture marks the reflected target"),
		GameXXKCardRules::AddCombatStatus(
			*FindRuntimeUnit(LifestealReflectRuntime.Units, TEXT("Blade")),
			EGameXXKCardStatus::Mark,
			1),
		1);
```

Inside the existing two-result audit block, add:

```cpp
		TestEqual(TEXT("the enemy reflection applies the global Mark bonus"),
			LifestealReflectResult.DamageResults[1].MarkDamageBonusPercent, 15);
		TestEqual(TEXT("the enemy reflection consumes exactly one Mark"),
			LifestealReflectResult.DamageResults[1].MarkStacksConsumed, 1);
```

After the block, assert Blade has zero Mark. The reflected packet is only five requested damage, so flooring keeps the existing final HP expectation at 51; the audit fields and consumed stack prove the rule was applied.

- [ ] **Step 7: Cold-build and verify all Mark edge tests GREEN**

Run UBT, then:

```text
GameXXK.Data.MarkRules
GameXXK.Data.CardCombatRules
GameXXK.Data.CardBattleRuntime
```

Expected: all tests pass; the three-hit sequence reports `115, 115, 100`, group members resolve independently, Agility/DoT/self/environmental loss preserve Mark, armor consumes it, redirection uses the guardian, Mark remains capped/persistent, and the reactive counter consumes it.

- [ ] **Step 8: Commit the audit and edge contract**

```powershell
git add -- 'Source/GameXXK/Public/GameXXKCardTypes.h' 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKMarkRulesTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardCombatRulesTest.cpp' 'Source/GameXXK/Private/Tests/GameXXKCardBattleRuntimeTest.cpp'
git commit -m "test: lock mark damage edge semantics"
```

### Task 3: Preserve existing Mark-gated rewards with a card-start snapshot

**Files:**
- Create: `Source/GameXXK/Private/Tests/GameXXKMarkCardCompatibilityTest.cpp`
- Modify: `Source/GameXXK/Private/GameXXKCardRules.cpp:720-750, 2840-2905, 3540-3600, 4062-4280, 4378-4610`

- [ ] **Step 1: Write the failing real-card compatibility test**

Create a test named `GameXXK.Integration.MarkCardCompatibility` with these helpers and cases:

```cpp
#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkCardCompatibilityTest,
	"GameXXK.Integration.MarkCardCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

namespace
{
	FGameXXKCardCombatUnit MakeCompatibilityUnit(
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
		Unit.HP = 500;
		Unit.MaxHP = 500;
		Unit.Attack = 20;
		Unit.Defense = 0;
		Unit.Mana = 20;
		Unit.MaxMana = 20;
		Unit.StableSortOrder = StableSortOrder;
		return Unit;
	}

	TArray<FGameXXKCardInstance> MakeCompatibilityCards(const FName CardId, const int32 Count)
	{
		TArray<FGameXXKCardInstance> Instances;
		for (int32 Index = 0; Index < Count; ++Index)
		{
			FGameXXKCardInstance& Instance = Instances.AddDefaulted_GetRef();
			Instance.InstanceId = FName(*FString::Printf(TEXT("MarkCompatibility.%s.%d"), *CardId.ToString(), Index));
			Instance.CardId = CardId;
			Instance.CurrentQuality = EGameXXKCardQuality::Common;
			Instance.OwnerUnitId = TEXT("Hunter");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Source.%s.%d"), *CardId.ToString(), Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	bool InitializeCompatibilityRuntime(const FName CardId, FGameXXKCardBattleRuntime& OutRuntime)
	{
		const TArray<FGameXXKCardCombatUnit> Units = {
			MakeCompatibilityUnit(TEXT("Hunter"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 1),
			MakeCompatibilityUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		return GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime, MakeCompatibilityCards(CardId, 10), Units, EGameXXKCardTerrain::Plain, 18801);
	}

	FGameXXKCardCombatUnit* FindCompatibilityUnit(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return Runtime.Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}
}

bool FGameXXKMarkCardCompatibilityTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime PursuitRuntime;
	TestTrue(TEXT("追猎 fixture initializes"), InitializeCompatibilityRuntime(TEXT("Profession.Hunter.ZhuiLie"), PursuitRuntime));
	FGameXXKCardCombatUnit* PursuitEnemy = FindCompatibilityUnit(PursuitRuntime, TEXT("Enemy"));
	GameXXKCardRules::AddCombatStatus(*PursuitEnemy, EGameXXKCardStatus::Mark, 1);
	const FName PursuitCardId = PursuitRuntime.Deck.Hand[0].InstanceId;
	FGameXXKCardPlayResult PursuitResult;
	TestTrue(TEXT("追猎 resolves against a marked target"), GameXXKCardRules::ResolveCardPlay(
		PursuitRuntime, PursuitCardId, TEXT("Enemy"), PursuitResult));
	TestEqual(TEXT("追猎 consumes the only Mark through damage"),
		GameXXKCardRules::GetCombatStatusStacks(*FindCompatibilityUnit(PursuitRuntime, TEXT("Enemy")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("追猎 still draws from its card-start Mark condition"), PursuitRuntime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("追猎 still refunds three mana from its card-start Mark condition"),
		FindCompatibilityUnit(PursuitRuntime, TEXT("Hunter"))->Mana, 18);

	FGameXXKCardBattleRuntime FiveStackRuntime;
	TestTrue(TEXT("百步穿杨 fixture initializes"), InitializeCompatibilityRuntime(TEXT("Profession.Hunter.BaiBuChuanYang"), FiveStackRuntime));
	GameXXKCardRules::AddCombatStatus(*FindCompatibilityUnit(FiveStackRuntime, TEXT("Enemy")), EGameXXKCardStatus::Mark, 5);
	FGameXXKCardPlayResult FiveStackResult;
	TestTrue(TEXT("百步穿杨 resolves at five starting Mark stacks"), GameXXKCardRules::ResolveCardPlay(
		FiveStackRuntime, FiveStackRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), FiveStackResult));
	TestEqual(TEXT("百步穿杨 consumes one Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindCompatibilityUnit(FiveStackRuntime, TEXT("Enemy")), EGameXXKCardStatus::Mark), 4);
	TestEqual(TEXT("百步穿杨 still satisfies its five-stack draw gate"), FiveStackRuntime.Deck.Hand.Num(), 6);

	FGameXXKCardBattleRuntime MultiHitRuntime;
	TestTrue(TEXT("连珠箭 fixture initializes"), InitializeCompatibilityRuntime(TEXT("Profession.Hunter.LianZhuJian"), MultiHitRuntime));
	FGameXXKCardPlayResult MultiHitResult;
	TestTrue(TEXT("连珠箭 resolves without starting Mark"), GameXXKCardRules::ResolveCardPlay(
		MultiHitRuntime, MultiHitRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), MultiHitResult));
	TestEqual(TEXT("连珠箭 produces two damage packets"), MultiHitResult.DamageResults.Num(), 2);
	if (MultiHitResult.DamageResults.Num() == 2)
	{
		TestEqual(TEXT("连珠箭 first hit does not use its own on-hit Mark"), MultiHitResult.DamageResults[0].MarkDamageBonusPercent, 0);
		TestEqual(TEXT("连珠箭 second hit uses Mark applied by the first hit"), MultiHitResult.DamageResults[1].MarkDamageBonusPercent, 15);
	}
	TestEqual(TEXT("连珠箭 ends with the second on-hit Mark available"),
		GameXXKCardRules::GetCombatStatusStacks(*FindCompatibilityUnit(MultiHitRuntime, TEXT("Enemy")), EGameXXKCardStatus::Mark), 1);
	return true;
}

#endif
```

- [ ] **Step 2: Cold-build and verify RED**

Run UBT, then `GameXXK.Integration.MarkCardCompatibility` with report name `MarkCompatibility_Red`.

Expected: 追猎 ends with four cards and 15 mana; 百步穿杨 ends with four cards because the live Mark count fell below its gate. The 连珠箭 timing assertions may already pass and remain as a regression.

- [ ] **Step 3: Add a private card-play Mark snapshot**

Near the existing forward declarations in `GameXXKCardRules.cpp`, add:

```cpp
	struct FGameXXKCardPlayConditionSnapshot
	{
		TMap<FName, int32> MarkStacksByUnitId;
	};

	FGameXXKCardPlayConditionSnapshot CaptureCardPlayConditionSnapshot(
		const FGameXXKCardBattleRuntime& Runtime)
	{
		FGameXXKCardPlayConditionSnapshot Snapshot;
		for (const FGameXXKCardCombatUnit& Unit : Runtime.Units)
		{
			Snapshot.MarkStacksByUnitId.Add(
				Unit.UnitId,
				GameXXKCardRules::GetCombatStatusStacks(Unit, EGameXXKCardStatus::Mark));
		}
		return Snapshot;
	}

	int32 GetConditionStatusStacks(
		const FGameXXKCardCombatUnit* Target,
		const EGameXXKCardStatus Status,
		const FGameXXKCardPlayConditionSnapshot* Snapshot)
	{
		if (!Target)
		{
			return 0;
		}
		if (Status == EGameXXKCardStatus::Mark && Snapshot)
		{
			return Snapshot->MarkStacksByUnitId.FindRef(Target->UnitId);
		}
		return GameXXKCardRules::GetCombatStatusStacks(*Target, Status);
	}
```

- [ ] **Step 4: Thread the snapshot through current-card conditions**

Add an optional `const FGameXXKCardPlayConditionSnapshot* Snapshot` parameter to the internal `IsConditionSatisfied` and `TryApplyEffectConditionAndConsumption` functions. For `TargetHasStatus`, use:

```cpp
		case EGameXXKCardEffectConditionType::TargetHasStatus:
			bConditionValue = Target
				&& GetConditionStatusStacks(Target, Condition.Status, Snapshot) >= Condition.MinimumStatusStacks;
			break;
```

Consumptive conditions must remain live-state operations:

```cpp
		const FGameXXKCardPlayConditionSnapshot* EvaluationSnapshot = Condition.bConsumeStatus
			? nullptr
			: Snapshot;
		if (!IsConditionSatisfied(
			Condition,
			InOutRuntime,
			InOutOwner,
			Target,
			EvaluationSnapshot,
			OutSatisfied,
			OutError) || !OutSatisfied)
		{
			return OutError.IsEmpty();
		}
```

At the start of `ResolveCurrentCardEffects`, capture one snapshot:

```cpp
		const FGameXXKCardPlayConditionSnapshot ConditionSnapshot =
			CaptureCardPlayConditionSnapshot(InOutRuntime);
```

Pass `&ConditionSnapshot` to every current-card call of `TryApplyEffectConditionAndConsumption`, and add a snapshot reference parameter to `ResolveAttackPacket`. Modifier triggers outside the current card play continue passing `nullptr` and therefore use live state.

For the per-stack `BonusDamagePercent` attachment calculation, replace the direct status read with:

```cpp
						const int32 StatusStacks = GetConditionStatusStacks(
							ConditionTarget,
							Attachment.Condition.Status,
							&ConditionSnapshot);
```

- [ ] **Step 5: Cold-build and verify GREEN**

Run UBT, then:

```text
GameXXK.Integration.MarkCardCompatibility
GameXXK.Data.MarkRules
GameXXK.Data.CardBattleRuntime
```

Expected: all tests pass; same-card conditional rewards use the starting Mark count while damage uses and consumes live per-hit stacks.

- [ ] **Step 6: Commit snapshot compatibility**

```powershell
git add -- 'Source/GameXXK/Private/GameXXKCardRules.cpp' 'Source/GameXXK/Private/Tests/GameXXKMarkCardCompatibilityTest.cpp'
git commit -m "fix: preserve mark-gated card rewards"
```

### Task 4: Lock enemy multi-hit and intent compatibility

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp:1660-1825`

- [ ] **Step 1: Add exact Graymane assertions**

After the existing three-hit `ContinuousHunt` count assertion, replace the generic loop-only expectations with:

```cpp
	TestEqual(TEXT("continuous hunt first hit uses the first Mark"), IntentResults[0].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("continuous hunt second hit uses the second Mark"), IntentResults[1].MarkDamageBonusPercent, 15);
	TestEqual(TEXT("continuous hunt third hit has no remaining Mark"), IntentResults[2].MarkDamageBonusPercent, 0);
	TestEqual(TEXT("continuous hunt first amplified damage is floored deterministically"),
		IntentResults[0].DamageAfterVulnerability, 18);
	TestEqual(TEXT("continuous hunt second amplified damage is floored deterministically"),
		IntentResults[1].DamageAfterVulnerability, 18);
	TestEqual(TEXT("continuous hunt final unmarked damage remains the requested magnitude"),
		IntentResults[2].DamageAfterVulnerability, 16);
	for (const FGameXXKCardDamageResult& DamageResult : IntentResults)
	{
		TestEqual(TEXT("each continuous-hunt hit keeps the Graymane source"),
			DamageResult.SourceUnitId, FName(TEXT("Enemy.Graymane.P2")));
		TestEqual(TEXT("each continuous-hunt hit keeps the forecasted requested damage"),
			DamageResult.RequestedDamage, 16);
	}
	MarkedHero = State.CardRun.ActiveBattle.Units.FindByPredicate([](const FGameXXKCardCombatUnit& Unit)
	{
		return Unit.UnitId == TEXT("Player");
	});
	TestEqual(TEXT("continuous hunt consumes both starting Mark stacks"),
		GameXXKCardRules::GetCombatStatusStacks(*MarkedHero, EGameXXKCardStatus::Mark), 0);
```

After the later generic 10-damage call, add:

```cpp
	TestEqual(TEXT("the post-hunt generic hit has no Mark bonus"), GenericDamageResult.MarkDamageBonusPercent, 0);
```

- [ ] **Step 2: Run the focused enemy integration**

Run UBT, then `GameXXK.Battle.EnemyIntentRules.GraymaneMarkedHuntOnlyAmplifiesMarkedCatalogDirectDamage`.

Expected: the catalog forecast remains 16 requested damage, actual resolved damage is `18, 18, 16`, and Mark reaches zero without cancelling the already-started three-hit intent.

- [ ] **Step 3: Run the full enemy-intent group**

Run `GameXXK.Battle.EnemyIntentRules` with a fresh report directory.

Expected: all enemy intent tests pass, including Gray Wolf and `MarkedParty` target selection.

- [ ] **Step 4: Commit enemy compatibility**

```powershell
git add -- 'Source/GameXXK/Private/Tests/GameXXKEnemyIntentRulesTest.cpp'
git commit -m "test: lock enemy mark consumption"
```

### Task 5: Update the player-facing Mark tooltip from the shared constant

**Files:**
- Modify: `Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp:345-380`
- Modify: `Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp:1-5, 113-115`

- [ ] **Step 1: Write the failing tooltip assertions**

After the Agility tooltip checks in `GameXXKBattleSceneActorTest.cpp`, add:

```cpp
	const FGameXXKBattleStatusIconStyle MarkStyle =
		FGameXXKBattleStatusIconStyle::ResolveStatusIconStyle(EGameXXKCardStatus::Mark);
	TestTrue(TEXT("Mark tooltip exposes the fifteen-percent direct-hit benefit"),
		MarkStyle.Effect.Contains(TEXT("15%")) && MarkStyle.Effect.Contains(TEXT("直接攻击")));
	TestTrue(TEXT("Mark tooltip explains one-stack landed-hit consumption"),
		MarkStyle.Timing.Contains(TEXT("移除 1 层")));
	TestTrue(TEXT("Mark tooltip distinguishes avoidance and damage over time"),
		MarkStyle.Timing.Contains(TEXT("闪避")) && MarkStyle.Timing.Contains(TEXT("持续伤害")));
	TestFalse(TEXT("Mark tooltip no longer describes a condition-only tag"),
		MarkStyle.Tooltip.Contains(TEXT("不会自动造成伤害")));
```

- [ ] **Step 2: Run the UI test to verify RED**

Run UBT, then `GameXXK.MVP.Battle.SceneActors` with report name `MarkTooltip_Red`.

Expected: the new Mark-specific assertions fail against the old condition-only tooltip.

- [ ] **Step 3: Generate the tooltip from the shared percentage**

Add this include:

```cpp
#include "GameXXKCardRules.h"
```

Replace the Mark case with:

```cpp
	case EGameXXKCardStatus::Mark:
	{
		const FString Effect = FString::Printf(
			TEXT("受到直接攻击时，本次命中伤害提高 %d%%。"),
			GameXXKCardRules::MarkDirectDamageBonusPercent);
		return MakeStyle(
			TEXT("MarkTarget"),
			TEXT("标记"),
			*Effect,
			TEXT("每次有效命中后移除 1 层；闪避、持续伤害和非攻击生命损失不触发。最多 5 层。"),
			FLinearColor(0.55f, 0.33f, 0.34f, 1.0f),
			900);
	}
```

- [ ] **Step 4: Cold-build and verify GREEN**

Run UBT, then:

```text
GameXXK.MVP.Battle.SceneActors
GameXXK.Data.MarkRules
```

Expected: both tests pass and the tooltip contains the same percentage used by combat rules.

- [ ] **Step 5: Commit the tooltip**

```powershell
git add -- 'Source/GameXXK/Private/UI/GameXXKBattleStatusIconStyle.cpp' 'Source/GameXXK/Private/Tests/GameXXKBattleSceneActorTest.cpp'
git commit -m "ui: explain global mark mechanics"
```

### Task 6: Run the complete Mark verification gate

**Files:**
- Verify: all files listed in this plan
- Generate only: `Saved/Automation/MarkRuleFinal/*`

- [ ] **Step 1: Save/close any editor and perform a fresh cold build**

Run the Task 0 lifecycle command, then the shared UBT command.

Expected: save succeeds before any close, and UBT exits 0 with `-NoHotReload`.

- [ ] **Step 2: Run the focused automation set**

Run each path with its own final report directory:

```text
GameXXK.Data.MarkRules
GameXXK.Integration.MarkCardCompatibility
GameXXK.Data.CardCombatRules
GameXXK.Data.CardBattleRuntime
GameXXK.Battle.EnemyIntentRules
GameXXK.Battle.EnemyMechanics
GameXXK.MVP.Battle.SceneActors
```

Expected: every report has zero failed tests and no unexpected warnings.

- [ ] **Step 3: Run a deterministic route replay**

Run:

```text
GameXXK.RouteBalance.Determinism.ChapterTwoNormalReplay
```

Expected: all 267 fixed cases match their same-process replay fingerprints. This checks determinism, not preservation of the pre-change balance hash.

- [ ] **Step 4: Verify the scoped diff**

Run:

```powershell
git diff --check
git status --short
git diff --stat
```

Expected: no whitespace errors; only the planned C++/test files are tracked changes. `Saved/Automation` and existing user-owned untracked outputs are not staged.

- [ ] **Step 5: Record the known-stale legacy balance target test**

Run `GameXXK.RouteBalance.FinalCandidateTargets` once.

Expected: record the exact result without hiding it. The accepted baseline in `docs/design/2026-08-08-card-balance-analysis.md` already documents this test as stale because it hard-codes a retired high-attack profile and can reach `MaxRounds`; it is diagnostic evidence, not this plan's pass/fail gate. Do not alter enemy stats, target bands, or the 15% rule to make this legacy test green.

## Self-review checklist

- [ ] Direct hits use one fixed 15% bonus whenever Mark is positive.
- [ ] Exactly one Mark is consumed per landed hit; five stacks never mean +75% on one hit.
- [ ] Vulnerability and Mark share an additive multiplier.
- [ ] Agility, DoT, self-loss, and environmental damage do not consume Mark.
- [ ] Armor and guard redirection use the final resolved receiver and correct ordering.
- [ ] Multi-hit and group damage read each target's live Mark independently.
- [ ] Reactive counterattacks use the same symmetric Mark rule.
- [ ] On-hit Mark applies after its own hit and can affect a later hit.
- [ ] Existing `TargetHasStatus(Mark, N)` rewards use the card-start snapshot.
- [ ] Enemy intent eligibility stays locked while damage bonus reads live stacks.
- [ ] Tooltip and rules share one named percentage constant.
- [ ] Damage audit fields and existing status-consumed metrics expose real utilization.
