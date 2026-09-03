#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerUniversalRewardMatrixTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;

	int32 StatusStacks(FGameXXKCardBattleRuntime& Runtime, const FName UnitId, const EGameXXKCardStatus Status)
	{
		return GameXXKCardRules::GetCombatStatusStacks(*FindUnit(Runtime, UnitId), Status);
	}

	int32 HealthLost(FGameXXKCardBattleRuntime& Runtime, const FName UnitId)
	{
		return 10000 - FindUnit(Runtime, UnitId)->HP;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererYanMuRewardMatrixTest,
	"GameXXK.Data.PartnerCards.Sorcerer.UniversalRewards.YanMu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererYanMuRewardMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRewardMatrixTest;
	const FName Starter(TEXT("Profession.Sorcerer.YanMuHuTi"));
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Normal;
	if (!BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Normal, 59801, Normal))
	{
		return false;
	}
	ResolveCompletedReward(*this, Normal, Result);
	TestEqual(TEXT("YanMu normal reward deals group three-hundred-percent"), HealthLost(Normal, EnemyAId), 60);

	FGameXXKCardBattleRuntime Fire;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Fire, 59802, Fire);
	ResolveCompletedReward(*this, Fire, Result);
	TestEqual(TEXT("level-one YanMu Fire coefficient three generates four Burn"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Burn), 4);
	TestEqual(TEXT("YanMu triggers the newly generated Burn once without consumption"), HealthLost(Fire, EnemyAId), 4);

	FGameXXKCardBattleRuntime Ice;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Ice, 59803, Ice);
	FindUnit(Ice, SorcererId)->Armor = 20;
	ResolveCompletedReward(*this, Ice, Result);
	TestEqual(TEXT("YanMu Ice reward has a distinct 220-percent base plus twenty Armor"), HealthLost(Ice, EnemyAId), 48);
	TestEqual(TEXT("YanMu Ice reward consumes all armor"), FindUnit(Ice, SorcererId)->Armor, 0);

	FGameXXKCardBattleRuntime Lightning;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Lightning, 59804, Lightning);
	ResolveCompletedReward(*this, Lightning, Result);
	TestEqual(TEXT("YanMu Lightning reward deals three marked sixty-percent hits"), HealthLost(Lightning, EnemyAId), 39);
	TestEqual(TEXT("YanMu Lightning reward consumes Mark"), StatusStacks(Lightning, EnemyAId, EGameXXKCardStatus::Mark), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererLieFuRewardMatrixTest,
	"GameXXK.Data.PartnerCards.Sorcerer.UniversalRewards.LieFu",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererLieFuRewardMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRewardMatrixTest;
	const FName Starter(TEXT("Profession.Sorcerer.LieFu"));
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Normal;
	if (!BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Normal, 59811, Normal))
	{
		return false;
	}
	Normal.Deck.SharedEnergy = 5;
	FindUnit(Normal, SorcererId)->Mana = 0;
	FindUnit(Normal, SorcererId)->MaxMana = 100;
	FindUnit(Normal, AllyId)->Mana = 0;
	FindUnit(Normal, AllyId)->MaxMana = 100;
	ResolveCompletedReward(*this, Normal, Result);
	TestEqual(TEXT("LieFu normal reward restores two Energy"), Normal.Deck.SharedEnergy, 7);
	TestEqual(TEXT("LieFu normal reward draws three"), Normal.Deck.Hand.Num(), 3);
	TestEqual(TEXT("LieFu normal reward restores owner six Mana"), FindUnit(Normal, SorcererId)->Mana, 6);
	TestEqual(TEXT("LieFu normal reward restores ally six Mana"), FindUnit(Normal, AllyId)->Mana, 6);

	FGameXXKCardBattleRuntime Fire;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Fire, 59812, Fire);
	Fire.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, Fire, Result);
	TestEqual(TEXT("level-one LieFu coefficient four generates five Burn"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Burn), 5);
	TestEqual(TEXT("LieFu Fire reward restores one Energy"), Fire.Deck.SharedEnergy, 6);
	TestEqual(TEXT("LieFu Fire reward draws three"), Fire.Deck.Hand.Num(), 3);

	FGameXXKCardBattleRuntime Ice;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Ice, 59813, Ice);
	Ice.Deck.SharedEnergy = 5;
	FindUnit(Ice, SorcererId)->Armor = 8;
	ResolveCompletedReward(*this, Ice, Result);
	TestEqual(TEXT("LieFu consumes eight Armor into a 108-percent Ice attack"), HealthLost(Ice, EnemyAId), 21);
	TestEqual(TEXT("LieFu Ice reward refunds twenty-five percent armor"), FindUnit(Ice, SorcererId)->Armor, 2);
	TestEqual(TEXT("LieFu Ice reward restores one Energy"), Ice.Deck.SharedEnergy, 6);
	TestEqual(TEXT("LieFu Ice reward draws two"), Ice.Deck.Hand.Num(), 2);

	FGameXXKCardBattleRuntime Lightning;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Lightning, 59814, Lightning);
	Lightning.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, Lightning, Result);
	TestEqual(TEXT("LieFu Lightning reward deals two marked forty-percent hits"), HealthLost(Lightning, EnemyAId), 18);
	TestEqual(TEXT("LieFu Lightning reward consumes Mark"), StatusStacks(Lightning, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("LieFu Lightning reward restores one Energy"), Lightning.Deck.SharedEnergy, 6);
	TestEqual(TEXT("LieFu Lightning reward draws two"), Lightning.Deck.Hand.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererXingHuoRewardMatrixTest,
	"GameXXK.Data.PartnerCards.Sorcerer.UniversalRewards.XingHuo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererXingHuoRewardMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRewardMatrixTest;
	const FName Starter(TEXT("Profession.Sorcerer.XingHuoHuiShou"));
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Normal;
	if (!BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Normal, 59821, Normal))
	{
		return false;
	}
	FindUnit(Normal, SorcererId)->Defense = 100;
	ResolveCompletedReward(*this, Normal, Result);
	TestEqual(TEXT("XingHuo normal reward grants eighty percent of caster Defense to owner"), FindUnit(Normal, SorcererId)->Armor, 80);
	TestEqual(TEXT("XingHuo normal reward grants the same eighty Armor to ally"), FindUnit(Normal, AllyId)->Armor, 80);
	TestEqual(TEXT("XingHuo normal reward applies Weak two"), StatusStacks(Normal, EnemyAId, EGameXXKCardStatus::Weak), 2);

	FGameXXKCardBattleRuntime Fire;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Fire, 59822, Fire);
	FindUnit(Fire, SorcererId)->Defense = 100;
	ResolveCompletedReward(*this, Fire, Result);
	TestEqual(TEXT("XingHuo Fire reward grants sixty percent of caster Defense"), FindUnit(Fire, AllyId)->Armor, 60);
	TestEqual(TEXT("level-one XingHuo Fire coefficient four generates five Burn"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Burn), 5);
	TestEqual(TEXT("XingHuo Fire reward applies Weak one"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Weak), 1);

	FGameXXKCardBattleRuntime Ice;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Ice, 59823, Ice);
	FindUnit(Ice, SorcererId)->Armor = 8;
	ResolveCompletedReward(*this, Ice, Result);
	TestEqual(TEXT("XingHuo consumes eight Armor into a 108-percent Ice attack"), HealthLost(Ice, EnemyAId), 21);
	TestEqual(TEXT("XingHuo refunds only a quarter of consumed Armor to owner"), FindUnit(Ice, SorcererId)->Armor, 2);
	TestEqual(TEXT("XingHuo refunds the same quarter to ally"), FindUnit(Ice, AllyId)->Armor, 2);

	FGameXXKCardBattleRuntime Lightning;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Lightning, 59824, Lightning);
	FindUnit(Lightning, SorcererId)->Defense = 100;
	ResolveCompletedReward(*this, Lightning, Result);
	TestEqual(TEXT("XingHuo Lightning reward deals two marked thirty-percent hits"), HealthLost(Lightning, EnemyAId), 12);
	TestEqual(TEXT("XingHuo Lightning reward consumes Mark"), StatusStacks(Lightning, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("XingHuo Lightning reward grants forty percent of caster Defense"), FindUnit(Lightning, AllyId)->Armor, 40);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererChiYanRewardMatrixTest,
	"GameXXK.Data.PartnerCards.Sorcerer.UniversalRewards.ChiYan",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererChiYanRewardMatrixTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerUniversalRewardMatrixTest;
	const FName Starter(TEXT("Profession.Sorcerer.ChiYanFengJie"));
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Normal;
	if (!BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Normal, 59831, Normal))
	{
		return false;
	}
	ResolveCompletedReward(*this, Normal, Result);
	TestEqual(TEXT("ChiYan normal reward replays fifth card attack"), HealthLost(Normal, EnemyAId), 10);
	TestEqual(TEXT("ChiYan normal reward replays fifth card Mark"), StatusStacks(Normal, EnemyAId, EGameXXKCardStatus::Mark), 2);
	TestEqual(TEXT("ChiYan normal reward draws one"), Normal.Deck.Hand.Num(), 1);

	FGameXXKCardBattleRuntime Fire;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Fire, 59832, Fire);
	TestEqual(TEXT("ChiYan Fire reward fixture begins without Burn"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Burn), 0);
	ResolveCompletedReward(*this, Fire, Result);
	TestEqual(TEXT("ChiYan Fire reward replays last Fire attack"), HealthLost(Fire, EnemyAId), 12);
	TestEqual(TEXT("ChiYan doubles five generated replay Burn then adds coefficient two as three"), StatusStacks(Fire, EnemyAId, EGameXXKCardStatus::Burn), 13);
	TArray<FName> FireBurnAuditTargets;
	for (const FGameXXKCardStatusChangeResult& Change : Result.StatusChanges)
	{
		if (Change.Status == EGameXXKCardStatus::Burn)
		{
			FireBurnAuditTargets.Add(Change.TargetUnitId);
		}
	}
	TArray<FString> FireBurnAuditTargetNames;
	for (const FGameXXKCardStatusChangeResult& Change : Result.StatusChanges)
	{
		if (Change.Status == EGameXXKCardStatus::Burn)
		{
			FireBurnAuditTargetNames.Add(FString::Printf(TEXT("%s:%d"), *Change.TargetUnitId.ToString(), Change.AppliedStacks));
		}
	}
	TestEqual(
		FString::Printf(
			TEXT("ChiYan Fire reward keeps every Burn audit phase in stable enemy order (actual: %s)"),
			*FString::Join(FireBurnAuditTargetNames, TEXT(","))),
		FireBurnAuditTargets,
		TArray<FName>{EnemyAId, EnemyBId, EnemyAId, EnemyBId});

	FGameXXKCardBattleRuntime Ice;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Ice, 59833, Ice);
	FindUnit(Ice, SorcererId)->Armor = 4;
	const int32 MaxManaBefore = FindUnit(Ice, SorcererId)->MaxMana;
	ResolveCompletedReward(*this, Ice, Result);
	TestEqual(TEXT("ChiYan Ice reward replays last Ice max-Mana effect"), FindUnit(Ice, SorcererId)->MaxMana, MaxManaBefore + 4);
	TestEqual(TEXT("ChiYan includes the replay recovery overflow in standard Ice"), HealthLost(Ice, EnemyAId), 22);
	TestEqual(TEXT("ChiYan Ice reward consumes armor after replay"), FindUnit(Ice, SorcererId)->Armor, 0);
	TestEqual(TEXT("ChiYan Ice reward draws one"), Ice.Deck.Hand.Num(), 1);

	FGameXXKCardBattleRuntime Lightning;
	BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Lightning, 59834, Lightning);
	ResolveCompletedReward(*this, Lightning, Result);
	TestEqual(TEXT("ChiYan Lightning reward combines replay and remaining-Mark lightning"), HealthLost(Lightning, EnemyAId), 47);
	TestEqual(TEXT("ChiYan Lightning reward consumes remaining Mark"), StatusStacks(Lightning, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("ChiYan Lightning reward draws one"), Lightning.Deck.Hand.Num(), 1);
	return true;
}

#endif
