#include "GameXXKCombatScalingRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatScalingArithmeticTest,
	"GameXXK.Data.CombatScaling.Arithmetic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatScalingArithmeticTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Common 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Common), 101);
	TestEqual(TEXT("Rare 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Rare), 122);
	TestEqual(TEXT("Epic 101"), FGameXXKCombatScalingRules::ScaleContinuousCeil(101, EGameXXKCardQuality::Epic), 142);

	TestEqual(TEXT("level 10 DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Common, 10), 9);
	TestEqual(TEXT("level 100 common DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Common, 100), 30);
	TestEqual(TEXT("level 100 rare DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Rare, 100), 36);
	TestEqual(TEXT("level 100 epic DOT"), FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Epic, 100), 42);

	TestEqual(TEXT("DOT cap 25"), FGameXXKCombatScalingRules::ResolveDotCap(25), 25);
	TestEqual(TEXT("DOT cap 26"), FGameXXKCombatScalingRules::ResolveDotCap(26), 50);
	TestEqual(TEXT("DOT cap 100"), FGameXXKCombatScalingRules::ResolveDotCap(100), 100);
	TestEqual(TEXT("DOT cap 135"), FGameXXKCombatScalingRules::ResolveDotCap(135), 150);

	TestEqual(TEXT("cost zero armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 0, EGameXXKCardQuality::Common), 144);
	TestEqual(TEXT("cost one armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 1, EGameXXKCardQuality::Common), 287);
	TestEqual(TEXT("cost two armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 2, EGameXXKCardQuality::Common), 502);
	TestEqual(TEXT("cost three armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 3, EGameXXKCardQuality::Common), 716);
	TestEqual(TEXT("rare cost two armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 2, EGameXXKCardQuality::Rare), 602);
	TestEqual(TEXT("epic cost three armor"), FGameXXKCombatScalingRules::ResolvePrintedCostArmor(358, 3, EGameXXKCardQuality::Epic), 1003);

	TestEqual(TEXT("plus thirty five levels"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 135, 100), 135);
	TestEqual(TEXT("minus thirty five levels"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 100, 135), 65);
	TestEqual(TEXT("upper clamp"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 200, 1), 150);
	TestEqual(TEXT("lower clamp"), FGameXXKCombatScalingRules::ApplyLevelDifferenceCeil(100, 1, 200), 50);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCombatScalingUnifiedHealingTest,
	"GameXXK.Data.CombatScaling.UnifiedHealing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCombatScalingUnifiedHealingTest::RunTest(const FString& Parameters)
{
	struct FCase { EGameXXKCardQuality Quality; int32 Level; int32 Medicine; int32 Expected; };
	for (const FCase& Case : {FCase{EGameXXKCardQuality::Common, 100, 0, 125},
		{EGameXXKCardQuality::Rare, 1, 0, 32}, {EGameXXKCardQuality::Rare, 24, 0, 59},
		{EGameXXKCardQuality::Rare, 25, 0, 60}, {EGameXXKCardQuality::Rare, 100, 0, 150},
		{EGameXXKCardQuality::Rare, 100, 5, 180}, {EGameXXKCardQuality::Rare, 135, 0, 192},
		{EGameXXKCardQuality::Epic, 100, 0, 175}, {EGameXXKCardQuality::Epic, 100, 5, 210}})
	{
		for (const EGameXXKCardQuality LegacyReference : {EGameXXKCardQuality::Common,
			EGameXXKCardQuality::Rare, EGameXXKCardQuality::Epic})
		{
			TestEqual(TEXT("all healing coefficients use the same quality and level multiplier, including legacy metadata"),
				FGameXXKCombatScalingRules::ResolveMedicineHealing(25, Case.Medicine, Case.Quality, Case.Level, LegacyReference), Case.Expected);
		}
	}
	TestEqual(TEXT("heal and DOT share generation at zero Medicine"),
		FGameXXKCombatScalingRules::ResolveMedicineHealing(6, 0, EGameXXKCardQuality::Rare, 100),
		FGameXXKCombatScalingRules::ResolveDotAddition(6, EGameXXKCardQuality::Rare, 100));
	TestEqual(TEXT("combined coefficient addition and generation saturate safely"),
		FGameXXKCombatScalingRules::ResolveMedicineHealing(MAX_int32, MAX_int32, EGameXXKCardQuality::Epic, 135), MAX_int32);
	return true;
}

#endif
