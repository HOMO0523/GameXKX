#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerRewardRuntimeTest
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
	FGameXXKSorcererPartnerCoreRewardsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rewards.Core",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerCoreRewardsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Search;
	if (!BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.LingHuoFu"), EGameXXKSorcererTaskBranch::Normal, 59701, Search))
	{
		return false;
	}
	Search.Deck.SharedEnergy = 5;
	FindUnit(Search, SorcererId)->Mana = 10;
	FindUnit(Search, SorcererId)->MaxMana = 100;
	ResolveCompletedReward(*this, Search, Result);
	TestEqual(TEXT("Core-search reward restores one Energy"), Search.Deck.SharedEnergy, 6);
	TestEqual(TEXT("Core-search reward restores eight Mana"), FindUnit(Search, SorcererId)->Mana, 18);
	TestEqual(TEXT("Core-search reward draws two"), Search.Deck.Hand.Num(), 2);

	FGameXXKCardBattleRuntime Echo;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.JuLing"), EGameXXKSorcererTaskBranch::Normal, 59702, Echo);
	Echo.Deck.SharedEnergy = 5;
	FindUnit(Echo, SorcererId)->Mana = 10;
	FindUnit(Echo, SorcererId)->MaxMana = 100;
	FindUnit(Echo, AllyId)->Mana = 20;
	FindUnit(Echo, AllyId)->MaxMana = 100;
	ResolveCompletedReward(*this, Echo, Result);
	TestEqual(TEXT("Core-echo reward restores owner eight Mana"), FindUnit(Echo, SorcererId)->Mana, 18);
	TestEqual(TEXT("Core-echo reward restores ally eight Mana"), FindUnit(Echo, AllyId)->Mana, 28);
	TestEqual(TEXT("Core-echo reward draws two"), Echo.Deck.Hand.Num(), 2);
	TestEqual(TEXT("Core-echo reward does not restore Energy"), Echo.Deck.SharedEnergy, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerFireRewardsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rewards.Fire",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerFireRewardsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerRewardRuntimeTest;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Lamp;
	if (!BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.LiHuoYin"), EGameXXKSorcererTaskBranch::Fire, 59711, Lamp))
	{
		return false;
	}
	GameXXKCardRules::AddCombatStatus(*FindUnit(Lamp, EnemyAId), EGameXXKCardStatus::Burn, 2);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Lamp, EnemyBId), EGameXXKCardStatus::Burn, 5);
	ResolveCompletedReward(*this, Lamp, Result);
	TestEqual(TEXT("Fire-lamp reward doubles enemy A Burn"), StatusStacks(Lamp, EnemyAId, EGameXXKCardStatus::Burn), 4);
	TestEqual(TEXT("Fire-lamp reward doubles enemy B Burn"), StatusStacks(Lamp, EnemyBId, EGameXXKCardStatus::Burn), 10);

	FGameXXKCardBattleRuntime Spread;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.YanQiang"), EGameXXKSorcererTaskBranch::Fire, 59712, Spread);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Spread, EnemyAId), EGameXXKCardStatus::Burn, 2);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Spread, EnemyBId), EGameXXKCardStatus::Burn, 5);
	ResolveCompletedReward(*this, Spread, Result);
	TestEqual(TEXT("level-one spread equalizes then adds four generated Burn"), StatusStacks(Spread, EnemyAId, EGameXXKCardStatus::Burn), 9);
	TestEqual(TEXT("level-one spread adds the same four generated Burn to the leader"), StatusStacks(Spread, EnemyBId, EGameXXKCardStatus::Burn), 9);

	FGameXXKCardBattleRuntime Burst;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.BaoYanShu"), EGameXXKSorcererTaskBranch::Fire, 59713, Burst);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Burst, EnemyAId), EGameXXKCardStatus::Burn, 3);
	GameXXKCardRules::AddCombatStatus(*FindUnit(Burst, EnemyBId), EGameXXKCardStatus::Burn, 5);
	ResolveCompletedReward(*this, Burst, Result);
	TestEqual(TEXT("Fire-burst reward triggers enemy A Burn twice"), HealthLost(Burst, EnemyAId), 6);
	TestEqual(TEXT("Fire-burst reward triggers enemy B Burn twice"), HealthLost(Burst, EnemyBId), 10);
	TestEqual(TEXT("Fire-burst reward preserves enemy A Burn"), StatusStacks(Burst, EnemyAId, EGameXXKCardStatus::Burn), 3);
	TestEqual(TEXT("Fire-burst reward preserves enemy B Burn"), StatusStacks(Burst, EnemyBId, EGameXXKCardStatus::Burn), 5);

	FGameXXKCardBattleRuntime Search;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.XingHuoLiaoYuan"), EGameXXKSorcererTaskBranch::Fire, 59714, Search);
	Search.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, Search, Result);
	TestEqual(TEXT("level-one Fire-search coefficient six generates seven Burn"), StatusStacks(Search, EnemyAId, EGameXXKCardStatus::Burn), 7);
	TestEqual(TEXT("Fire-search reward restores one Energy"), Search.Deck.SharedEnergy, 6);
	TestEqual(TEXT("Fire-search reward draws two"), Search.Deck.Hand.Num(), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerIceRewardsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rewards.Ice",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerIceRewardsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerRewardRuntimeTest;
	FGameXXKCardPlayResult Result;
	const auto PrepareIce = [this](const FName Starter, const int32 Seed, FGameXXKCardBattleRuntime& Runtime) -> bool
	{
		if (!BuildCompletedRewardRuntime(*this, Starter, EGameXXKSorcererTaskBranch::Ice, Seed, Runtime))
		{
			return false;
		}
		FindUnit(Runtime, SorcererId)->Armor = 4;
		return true;
	};

	FGameXXKCardBattleRuntime CurrentMana;
	if (!PrepareIce(TEXT("Profession.Sorcerer.SheLingHuo"), 59721, CurrentMana))
	{
		return false;
	}
	CurrentMana.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, CurrentMana, Result);
	TestEqual(TEXT("Ice-current reward consumes owner armor"), FindUnit(CurrentMana, SorcererId)->Armor, 0);
	TestEqual(TEXT("standard Ice uses one attack point per consumed Armor"), HealthLost(CurrentMana, EnemyAId), 20);
	TestEqual(TEXT("Ice-current reward restores one Energy"), CurrentMana.Deck.SharedEnergy, 6);
	TestEqual(TEXT("Ice-current reward draws one"), CurrentMana.Deck.Hand.Num(), 1);

	FGameXXKCardBattleRuntime MaxMana;
	PrepareIce(TEXT("Profession.Sorcerer.FenMaiFu"), 59722, MaxMana);
	FindUnit(MaxMana, SorcererId)->Mana = 3;
	FindUnit(MaxMana, SorcererId)->MaxMana = 10;
	ResolveCompletedReward(*this, MaxMana, Result);
	TestEqual(TEXT("capacity starter uses the same standard Ice formula"), HealthLost(MaxMana, EnemyAId), 20);
	TestEqual(TEXT("Ice-max reward raises maximum Mana by eight"), FindUnit(MaxMana, SorcererId)->MaxMana, 18);
	TestEqual(TEXT("Ice-max reward fills current Mana"), FindUnit(MaxMana, SorcererId)->Mana, 18);

	FGameXXKCardBattleRuntime Armor;
	PrepareIce(TEXT("Profession.Sorcerer.LingYanLianDan"), 59723, Armor);
	ResolveCompletedReward(*this, Armor, Result);
	TestEqual(TEXT("mirror starter uses the same standard Ice formula"), HealthLost(Armor, EnemyAId), 20);
	TestEqual(TEXT("mirror refunds one quarter of four consumed Armor to the owner"), FindUnit(Armor, SorcererId)->Armor, 1);
	TestEqual(TEXT("mirror refunds one quarter of four consumed Armor to the ally"), FindUnit(Armor, AllyId)->Armor, 1);

	FGameXXKCardBattleRuntime Search;
	PrepareIce(TEXT("Profession.Sorcerer.HuLingMu"), 59724, Search);
	ResolveCompletedReward(*this, Search, Result);
	TestEqual(TEXT("search starter uses the same standard Ice formula"), HealthLost(Search, EnemyAId), 20);
	TestEqual(TEXT("Ice-search reward applies Weak two"), StatusStacks(Search, EnemyAId, EGameXXKCardStatus::Weak), 2);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerLightningRewardsTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Rewards.Lightning",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerLightningRewardsTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerRewardRuntimeTest;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime Mark;
	if (!BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.ChiXiaoFenXing"), EGameXXKSorcererTaskBranch::Lightning, 59731, Mark))
	{
		return false;
	}
	Mark.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, Mark, Result);
	TestEqual(TEXT("Lightning-mark reward applies Mark five"), StatusStacks(Mark, EnemyAId, EGameXXKCardStatus::Mark), 5);
	TestEqual(TEXT("Lightning-mark reward restores one Energy"), Mark.Deck.SharedEnergy, 6);
	TestEqual(TEXT("Lightning-mark reward draws two"), Mark.Deck.Hand.Num(), 2);

	FGameXXKCardBattleRuntime Search;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.FenTianJue"), EGameXXKSorcererTaskBranch::Lightning, 59732, Search);
	Search.Deck.SharedEnergy = 5;
	ResolveCompletedReward(*this, Search, Result);
	TestEqual(TEXT("Lightning-search reward applies Mark three"), StatusStacks(Search, EnemyAId, EGameXXKCardStatus::Mark), 3);
	TestEqual(TEXT("Lightning-search reward restores one Energy"), Search.Deck.SharedEnergy, 6);
	TestEqual(TEXT("Lightning-search reward draws two"), Search.Deck.Hand.Num(), 2);

	FGameXXKCardBattleRuntime Chain;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.NingYanChengRen"), EGameXXKSorcererTaskBranch::Lightning, 59733, Chain);
	ResolveCompletedReward(*this, Chain, Result);
	TestEqual(TEXT("Lightning-chain reward deals five marked seventy-percent hits"), HealthLost(Chain, EnemyAId), 80);
	TestEqual(TEXT("Lightning-chain reward consumes its five Mark"), StatusStacks(Chain, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("Lightning-chain reward emits ten group packets"), Result.DamageResults.Num(), 10);

	FGameXXKCardBattleRuntime Storm;
	BuildCompletedRewardRuntime(*this, TEXT("Profession.Sorcerer.RanLingHuanYuan"), EGameXXKSorcererTaskBranch::Lightning, 59734, Storm);
	ResolveCompletedReward(*this, Storm, Result);
	TestEqual(TEXT("Lightning-storm reward deals three marked sixty-percent hits"), HealthLost(Storm, EnemyAId), 39);
	TestEqual(TEXT("Lightning-storm reward consumes its three Mark"), StatusStacks(Storm, EnemyAId, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("Lightning-storm reward emits six group packets"), Result.DamageResults.Num(), 6);
	return true;
}

#endif
