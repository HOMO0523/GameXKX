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

#endif
