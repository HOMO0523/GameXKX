#include "Misc/AutomationTest.h"

#include "GameXXKEncounterRules.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool HasSlot(const TArray<FGameXXKEncounterSlot>& Slots, const int32 SlotNumber)
	{
		return Slots.ContainsByPredicate([SlotNumber](const FGameXXKEncounterSlot& Slot)
		{
			return Slot.BattleSlotNumber == SlotNumber;
		});
	}

	const FGameXXKEncounterSlot* FindSlot(const TArray<FGameXXKEncounterSlot>& Slots, const int32 SlotNumber)
	{
		return Slots.FindByPredicate([SlotNumber](const FGameXXKEncounterSlot& Slot)
		{
			return Slot.BattleSlotNumber == SlotNumber;
		});
	}

	bool IsChapterDefinition(const FGameXXKEncounterSlot* Slot, const int32 Chapter, const EGameXXKEnemyTier Tier)
	{
		if (!Slot)
		{
			return false;
		}
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Slot->EnemyDefinitionId);
		return Definition && Definition->Chapter == Chapter && Definition->Tier == Tier;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEncounterAuthoredStatScaleTest,
	"GameXXK.Route.EncounterFormation.AuthoredStatScales",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEncounterAuthoredStatScaleTest::RunTest(const FString& Parameters)
{
	const int32 ExpectedMaxHP[4][3] = {
		{0, 0, 0},
		{140, 160, 120},
		{140, 160, 100},
		{140, 160, 80}};
	const int32 ExpectedAttack[4][3] = {
		{0, 0, 0},
		{250, 270, 120},
		{250, 170, 100},
		{250, 180, 90}};
	const int32 ExpectedDefense[4][3] = {
		{0, 0, 0},
		{100, 100, 100},
		{100, 105, 100},
		{100, 110, 100}};
	const EGameXXKNodeKind NodeKinds[3] = {
		EGameXXKNodeKind::Battle,
		EGameXXKNodeKind::Elite,
		EGameXXKNodeKind::Boss};

	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		for (int32 NodeIndex = 0; NodeIndex < UE_ARRAY_COUNT(NodeKinds); ++NodeIndex)
		{
			const FGameXXKEncounterStatScale Scale = FGameXXKEncounterRules::GetAuthoredStatScale(Chapter, NodeKinds[NodeIndex]);
			TestEqual(TEXT("the authored HP scale remains locked"), Scale.MaxHPPercent, ExpectedMaxHP[Chapter][NodeIndex]);
			TestEqual(TEXT("the authored attack scale remains locked"), Scale.AttackPercent, ExpectedAttack[Chapter][NodeIndex]);
			TestEqual(TEXT("the authored defense scale remains locked"), Scale.DefensePercent, ExpectedDefense[Chapter][NodeIndex]);
		}
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEncounterFormationTest,
	"GameXXK.Route.EncounterFormation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEncounterFormationTest::RunTest(const FString& Parameters)
{
	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		const int32 RootSeed = 20260723 + Chapter;
		const int32 ChapterSeed = FGameXXKEncounterRules::DeriveChapterSeed(RootSeed, Chapter);
		for (const EGameXXKNodeKind NodeKind : {EGameXXKNodeKind::Battle, EGameXXKNodeKind::Elite, EGameXXKNodeKind::Boss})
		{
			TArray<FGameXXKEncounterSlot> Slots;
			FString Error;
			TestTrue(
				FString::Printf(TEXT("chapter %d %d formation builds: %s"), Chapter, static_cast<int32>(NodeKind), *Error),
				FGameXXKEncounterRules::BuildFormation(Chapter, NodeKind, ChapterSeed, 41, 1, Slots, &Error));
			TestEqual(
				TEXT("encounter formation uses the documented number of occupied presentation slots"),
				Slots.Num(),
				NodeKind == EGameXXKNodeKind::Battle ? 2 : 3);
			TestTrue(TEXT("every formation contains 1P"), HasSlot(Slots, 1));
			TestTrue(TEXT("every formation contains 3P"), HasSlot(Slots, 3));
			const FGameXXKEncounterSlot* OneP = FindSlot(Slots, 1);
			const FGameXXKEncounterSlot* TwoP = FindSlot(Slots, 2);
			const FGameXXKEncounterSlot* ThreeP = FindSlot(Slots, 3);
			if (NodeKind == EGameXXKNodeKind::Battle)
			{
				TestTrue(TEXT("normal 1P is a chapter-normal enemy"), IsChapterDefinition(OneP, Chapter, EGameXXKEnemyTier::Normal));
				TestFalse(TEXT("normal encounters keep the central 2P slot clear"), HasSlot(Slots, 2));
				TestTrue(TEXT("normal 3P is a chapter-normal enemy"), IsChapterDefinition(ThreeP, Chapter, EGameXXKEnemyTier::Normal));
				TestTrue(TEXT("normal sampling is without replacement"), OneP && ThreeP && OneP->EnemyDefinitionId != ThreeP->EnemyDefinitionId);
			}
			else if (NodeKind == EGameXXKNodeKind::Elite)
			{
				TestTrue(TEXT("elite encounters occupy the central 2P slot"), HasSlot(Slots, 2));
				TestTrue(TEXT("elite 1P is a chapter-normal enemy"), IsChapterDefinition(OneP, Chapter, EGameXXKEnemyTier::Normal));
				TestTrue(TEXT("elite 2P is the stronger chapter-elite enemy"), IsChapterDefinition(TwoP, Chapter, EGameXXKEnemyTier::Elite));
				TestTrue(TEXT("elite 3P is a chapter-normal enemy"), IsChapterDefinition(ThreeP, Chapter, EGameXXKEnemyTier::Normal));
				TestTrue(TEXT("elite flanking normal sampling is without replacement"), OneP && ThreeP && OneP->EnemyDefinitionId != ThreeP->EnemyDefinitionId);
			}
			else
			{
				TestTrue(TEXT("boss encounters occupy the central 2P slot"), HasSlot(Slots, 2));
				TestTrue(TEXT("boss 1P is chapter elite A"), IsChapterDefinition(OneP, Chapter, EGameXXKEnemyTier::Elite));
				TestTrue(TEXT("boss 2P is the chapter boss"), IsChapterDefinition(TwoP, Chapter, EGameXXKEnemyTier::Boss));
				TestTrue(TEXT("boss 3P is chapter elite B"), IsChapterDefinition(ThreeP, Chapter, EGameXXKEnemyTier::Elite));
				TestTrue(TEXT("boss flanking elites are distinct"), OneP && ThreeP && OneP->EnemyDefinitionId != ThreeP->EnemyDefinitionId);
			}
		}
	}

	TArray<FGameXXKEncounterSlot> First;
	TArray<FGameXXKEncounterSlot> Second;
	const int32 Seed = FGameXXKEncounterRules::DeriveChapterSeed(9551, 2);
	TestTrue(TEXT("the deterministic formation baseline builds"), FGameXXKEncounterRules::BuildFormation(2, EGameXXKNodeKind::Elite, Seed, 17, 10, First));
	TestTrue(TEXT("the same formation input builds twice"), FGameXXKEncounterRules::BuildFormation(2, EGameXXKNodeKind::Elite, Seed, 17, 10, Second));
	TestEqual(TEXT("same seed chapter node produces same slot count"), First.Num(), Second.Num());
	for (int32 Index = 0; Index < FMath::Min(First.Num(), Second.Num()); ++Index)
	{
		TestEqual(TEXT("same seed chapter node preserves definition identity"), First[Index].EnemyDefinitionId, Second[Index].EnemyDefinitionId);
		TestEqual(TEXT("same seed chapter node preserves slot identity"), First[Index].BattleSlotNumber, Second[Index].BattleSlotNumber);
		TestEqual(TEXT("same seed chapter node preserves combat level"), First[Index].CombatLevel, Second[Index].CombatLevel);
	}

	for (const int32 Snapshot : {1, 5, 10, 15, 20})
	{
		TestEqual(TEXT("normal combat level equals route snapshot"), FGameXXKEncounterRules::GetCombatLevel(EGameXXKEnemyTier::Normal, Snapshot), Snapshot);
		TestEqual(TEXT("elite combat level is route snapshot plus one capped at twenty"), FGameXXKEncounterRules::GetCombatLevel(EGameXXKEnemyTier::Elite, Snapshot), FMath::Min(Snapshot + 1, 20));
		TestEqual(TEXT("boss combat level is route snapshot plus two capped at twenty"), FGameXXKEncounterRules::GetCombatLevel(EGameXXKEnemyTier::Boss, Snapshot), FMath::Min(Snapshot + 2, 20));
	}

	TArray<FGameXXKEncounterSlot> AtomicFailure{{FName(TEXT("Sentinel")), 77, 9}};
	FString InvalidError;
	TestFalse(TEXT("out-of-range chapter is rejected"), FGameXXKEncounterRules::BuildFormation(4, EGameXXKNodeKind::Battle, 1, 1, 1, AtomicFailure, &InvalidError));
	TestEqual(TEXT("invalid formation leaves the caller-owned output untouched"), AtomicFailure.Num(), 1);
	TestEqual(TEXT("invalid formation preserves the existing output entry"), AtomicFailure[0].EnemyDefinitionId, FName(TEXT("Sentinel")));
	return true;
}

#endif
