#include "Misc/AutomationTest.h"

#include "GameXXKCardBattleAdapter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	int32 ExpectedBattleSeedMix(const int32 BaseSeed, const int32 NodeId)
	{
		constexpr int64 Multiplier = 486187739;
		return BaseSeed ^ static_cast<int32>(static_cast<int64>(NodeId) * Multiplier);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCardBattleSeedMixTest,
	"GameXXK.MVP.BattleSeedMix",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCardBattleSeedMixTest::RunTest(const FString& Parameters)
{
	struct FCase
	{
		int32 BaseSeed;
		int32 NodeId;
	};

	const FCase Cases[] = {
		{0, 0},
		{0x13579BDF, 1},
		{0x13579BDF, 2},
		{0x13579BDF, 317},
		{0x13579BDF, 4410},
		{0x13579BDF, 1000000},
		{0x13579BDF, 100000000},
		{0x13579BDF, MAX_int32},
		{0x13579BDF, MIN_int32},
		{0x13579BDF, -1},
		{MAX_int32, 1},
		{MIN_int32, 2},
	};

	for (const FCase& Case : Cases)
	{
		const int32 Expected = ExpectedBattleSeedMix(Case.BaseSeed, Case.NodeId);
		const int32 Actual = FGameXXKCardBattleAdapter::MixBattleSeed(Case.BaseSeed, Case.NodeId);
		if (Actual != Expected)
		{
			AddError(FString::Printf(
				TEXT("battle seed mix changed for BaseSeed=%d NodeId=%d: expected %d but found %d"),
				Case.BaseSeed, Case.NodeId, Expected, Actual));
		}
	}

	// The previous behavior for large positive node ids was a wrapping int32 multiply.
	// Pin the same wrapped result so the fix cannot silently change battle seeds.
	TestEqual(
		TEXT("large node ids keep the historical wrapped int32 result"),
		FGameXXKCardBattleAdapter::MixBattleSeed(0x13579BDF, 100000000),
		ExpectedBattleSeedMix(0x13579BDF, 100000000));

	return true;
}

#endif
