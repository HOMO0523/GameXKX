#include "GameXXKSorcererPartnerRuntimeTestUtils.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKSorcererPartnerIceLightningRuntimeTest
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;

	bool BuildSingleCardRuntime(
		FAutomationTestBase& Test,
		const FName CardId,
		const int32 Seed,
		FGameXXKCardBattleRuntime& OutRuntime)
	{
		return BuildRuntime(
			Test,
			{MakeCard(CardId, 0)},
			{
				MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
				MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10),
				MakeUnit(EnemyBId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 11)
			},
			Seed,
			OutRuntime);
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerIceRuntimeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.IceLightning.IceBaseAndBranchBoundaries",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerIceRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerIceLightningRuntimeTest;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime NormalRestore;
	if (!BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.SheLingHuo"), 59501, NormalRestore))
	{
		return false;
	}
	FGameXXKCardCombatUnit* NormalOwner = FindUnit(NormalRestore, SorcererId);
	NormalOwner->Mana = 9;
	NormalOwner->MaxMana = 10;
	ResolveAutomaticSnapshot(*this, NormalRestore, TEXT("Profession.Sorcerer.SheLingHuo"), 1, EGameXXKSorcererCardFamily::None, EGameXXKSorcererTaskBranch::Normal, Result);
	TestEqual(TEXT("normal branch caps current Mana after floor current-Mana twenty-five percent"), FindUnit(NormalRestore, SorcererId)->Mana, 10);
	TestEqual(TEXT("current-Mana restore always converts its own overflow to armor"), FindUnit(NormalRestore, SorcererId)->Armor, 1);

	FGameXXKCardBattleRuntime IceRestore;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.SheLingHuo"), 59502, IceRestore);
	FGameXXKCardCombatUnit* IceOwner = FindUnit(IceRestore, SorcererId);
	IceOwner->Mana = 9;
	IceOwner->MaxMana = 10;
	ResolveAutomaticSnapshot(*this, IceRestore, TEXT("Profession.Sorcerer.SheLingHuo"), 2, EGameXXKSorcererCardFamily::Universal, EGameXXKSorcererTaskBranch::Ice, Result);
	TestEqual(TEXT("Ice branch caps current Mana"), FindUnit(IceRestore, SorcererId)->Mana, 10);
	TestEqual(TEXT("Ice branch converts one overflow to one armor"), FindUnit(IceRestore, SorcererId)->Armor, 1);

	FGameXXKCardBattleRuntime MaxManaRuntime;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.FenMaiFu"), 59503, MaxManaRuntime);
	FGameXXKCardCombatUnit* MaxManaOwner = FindUnit(MaxManaRuntime, SorcererId);
	MaxManaOwner->Mana = 5;
	MaxManaOwner->MaxMana = 10;
	ResolveAutomaticSnapshot(*this, MaxManaRuntime, TEXT("Profession.Sorcerer.FenMaiFu"), 2, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Ice, Result);
	TestEqual(TEXT("Ice max-Mana card increases maximum by four"), FindUnit(MaxManaRuntime, SorcererId)->MaxMana, 14);
	TestEqual(TEXT("Ice max-Mana card leaves current Mana unchanged"), FindUnit(MaxManaRuntime, SorcererId)->Mana, 5);
	TestEqual(TEXT("Ice max-Mana card also grants four armor"), FindUnit(MaxManaRuntime, SorcererId)->Armor, 4);

	const TArray<FName> IceBranchIds = {
		TEXT("Profession.Sorcerer.FenMaiFu"),
		TEXT("Profession.Sorcerer.BaoYanShu"),
		TEXT("Profession.Sorcerer.JuLing"),
		TEXT("Profession.Sorcerer.LiHuoYin"),
		TEXT("Profession.Sorcerer.YanQiang")};
	TArray<FGameXXKCardInstance> IceBranchCards;
	for (int32 Index = 0; Index < IceBranchIds.Num(); ++Index)
	{
		IceBranchCards.Add(MakeCard(IceBranchIds[Index], Index));
	}
	FGameXXKCardBattleRuntime IceBranchRuntime;
	if (!BuildRuntime(*this, IceBranchCards, {
		MakeUnit(SorcererId, EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Sorcerer, 1),
		MakeUnit(EnemyAId, EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)}, 595031, IceBranchRuntime)
		|| !InstallAllCardsInHand(*this, IceBranchRuntime, IceBranchCards))
	{
		return false;
	}
	FindUnit(IceBranchRuntime, SorcererId)->Mana = 10;
	FindUnit(IceBranchRuntime, SorcererId)->MaxMana = 10;
	FGameXXKCardPlayResult BranchResult;
	ResolveActive(*this, IceBranchRuntime, IceBranchCards[0].InstanceId, BranchResult, TEXT("Ice starter"));
	ResolveActive(*this, IceBranchRuntime, IceBranchCards[1].InstanceId, BranchResult, TEXT("four-Mana predecessor in Ice task"));
	FindUnit(IceBranchRuntime, SorcererId)->Mana = 13;
	FindUnit(IceBranchRuntime, SorcererId)->Armor = 0;
	ResolveActive(*this, IceBranchRuntime, IceBranchCards[2].InstanceId, BranchResult, TEXT("Core Mana echo in Ice task"));
	TestEqual(TEXT("Ice branch caps another Sorcerer card's Mana gain"), FindUnit(IceBranchRuntime, SorcererId)->Mana, 14);
	TestEqual(TEXT("Ice branch converts another Sorcerer card's four overflow Mana to armor"), FindUnit(IceBranchRuntime, SorcererId)->Armor, 4);

	FGameXXKCardBattleRuntime ZeroArmorRuntime;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.LingYanLianDan"), 59504, ZeroArmorRuntime);
	ResolveAutomaticSnapshot(*this, ZeroArmorRuntime, TEXT("Profession.Sorcerer.LingYanLianDan"), 2, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Ice, Result);
	TestEqual(TEXT("zero armor becomes four"), FindUnit(ZeroArmorRuntime, SorcererId)->Armor, 4);

	FGameXXKCardBattleRuntime DoubleArmorRuntime;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.LingYanLianDan"), 59505, DoubleArmorRuntime);
	FindUnit(DoubleArmorRuntime, SorcererId)->Armor = 60;
	ResolveAutomaticSnapshot(*this, DoubleArmorRuntime, TEXT("Profession.Sorcerer.LingYanLianDan"), 3, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Ice, Result);
	TestEqual(TEXT("nonzero armor doubles without a gameplay cap"), FindUnit(DoubleArmorRuntime, SorcererId)->Armor, 120);

	FGameXXKCardBattleRuntime IceSearchRuntime;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.HuLingMu"), 59506, IceSearchRuntime);
	FGameXXKCardCombatUnit* IceSearchOwner = FindUnit(IceSearchRuntime, SorcererId);
	IceSearchOwner->Mana = 12;
	IceSearchOwner->MaxMana = 20;
	ResolveAutomaticSnapshot(*this, IceSearchRuntime, TEXT("Profession.Sorcerer.HuLingMu"), 3, EGameXXKSorcererCardFamily::Ice, EGameXXKSorcererTaskBranch::Ice, Result);
	TestEqual(TEXT("candidate-free Ice search grants its same three armor twice"), FindUnit(IceSearchRuntime, SorcererId)->Armor, 6);
	TestTrue(TEXT("Ice base suite deals no direct damage"), Result.DamageResults.IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKSorcererPartnerLightningRuntimeTest,
	"GameXXK.Data.PartnerCards.Sorcerer.Runtime.IceLightning.LightningMarkHitsAndDeathStop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKSorcererPartnerLightningRuntimeTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKSorcererPartnerRuntimeTestUtils;
	using namespace GameXXKSorcererPartnerIceLightningRuntimeTest;
	FGameXXKCardPlayResult Result;

	FGameXXKCardBattleRuntime MarkEarly;
	if (!BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.ChiXiaoFenXing"), 59511, MarkEarly)
		|| !ResolveAutomaticSnapshot(*this, MarkEarly, TEXT("Profession.Sorcerer.ChiXiaoFenXing"), 2, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result))
	{
		return false;
	}
	TestEqual(TEXT("early Mark card applies three"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(MarkEarly, EnemyAId), EGameXXKCardStatus::Mark), 3);

	FGameXXKCardBattleRuntime MarkLate;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.ChiXiaoFenXing"), 59512, MarkLate);
	ResolveAutomaticSnapshot(*this, MarkLate, TEXT("Profession.Sorcerer.ChiXiaoFenXing"), 3, EGameXXKSorcererCardFamily::Fire, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("late Mark card applies base two"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(MarkLate, EnemyAId), EGameXXKCardStatus::Mark), 2);

	FGameXXKCardBattleRuntime SearchEarly;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.FenTianJue"), 59513, SearchEarly);
	const int32 SearchHp = FindUnit(SearchEarly, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, SearchEarly, TEXT("Profession.Sorcerer.FenTianJue"), 1, EGameXXKSorcererCardFamily::None, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("candidate-free lightning search resolves two seventy-percent hits"), SearchHp - FindUnit(SearchEarly, EnemyAId)->HP, 28);
	TestEqual(TEXT("early lightning search applies Mark three after both hits"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(SearchEarly, EnemyAId), EGameXXKCardStatus::Mark), 3);

	FGameXXKCardBattleRuntime ChainEarly;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.NingYanChengRen"), 59514, ChainEarly);
	GameXXKCardRules::AddCombatStatus(*FindUnit(ChainEarly, EnemyAId), EGameXXKCardStatus::Mark, 3);
	const int32 ChainEarlyHp = FindUnit(ChainEarly, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, ChainEarly, TEXT("Profession.Sorcerer.NingYanChengRen"), 3, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("position-three chain uses three fifty-percent marked hits"), ChainEarlyHp - FindUnit(ChainEarly, EnemyAId)->HP, 33);
	TestEqual(TEXT("position-three chain emits three packets"), Result.DamageResults.Num(), 3);
	TestEqual(TEXT("each chain hit consumes one Mark"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(ChainEarly, EnemyAId), EGameXXKCardStatus::Mark), 0);

	FGameXXKCardBattleRuntime ChainLate;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.NingYanChengRen"), 59515, ChainLate);
	GameXXKCardRules::AddCombatStatus(*FindUnit(ChainLate, EnemyAId), EGameXXKCardStatus::Mark, 3);
	const int32 ChainLateHp = FindUnit(ChainLate, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, ChainLate, TEXT("Profession.Sorcerer.NingYanChengRen"), 4, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("position-four chain uses three sixty-five-percent marked hits"), ChainLateHp - FindUnit(ChainLate, EnemyAId)->HP, 42);

	FGameXXKCardBattleRuntime StormEarly;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.RanLingHuanYuan"), 59516, StormEarly);
	GameXXKCardRules::AddCombatStatus(*FindUnit(StormEarly, EnemyAId), EGameXXKCardStatus::Mark, 2);
	const int32 StormEarlyHp = FindUnit(StormEarly, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, StormEarly, TEXT("Profession.Sorcerer.RanLingHuanYuan"), 3, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("position-three storm uses two thirty-percent hits"), StormEarlyHp - FindUnit(StormEarly, EnemyAId)->HP, 12);

	FGameXXKCardBattleRuntime StormLate;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.RanLingHuanYuan"), 59517, StormLate);
	GameXXKCardRules::AddCombatStatus(*FindUnit(StormLate, EnemyAId), EGameXXKCardStatus::Mark, 2);
	const int32 StormLateHp = FindUnit(StormLate, EnemyAId)->HP;
	ResolveAutomaticSnapshot(*this, StormLate, TEXT("Profession.Sorcerer.RanLingHuanYuan"), 4, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("position-four storm uses two forty-five-percent hits"), StormLateHp - FindUnit(StormLate, EnemyAId)->HP, 20);

	FGameXXKCardBattleRuntime DeathStop;
	BuildSingleCardRuntime(*this, TEXT("Profession.Sorcerer.NingYanChengRen"), 59518, DeathStop);
	FindUnit(DeathStop, EnemyAId)->HP = 10;
	FindUnit(DeathStop, EnemyAId)->MaxHP = 10;
	GameXXKCardRules::AddCombatStatus(*FindUnit(DeathStop, EnemyAId), EGameXXKCardStatus::Mark, 3);
	ResolveAutomaticSnapshot(*this, DeathStop, TEXT("Profession.Sorcerer.NingYanChengRen"), 4, EGameXXKSorcererCardFamily::Lightning, EGameXXKSorcererTaskBranch::Lightning, Result);
	TestEqual(TEXT("lethal lightning stops remaining scheduled strikes"), Result.DamageResults.Num(), 1);
	TestEqual(TEXT("only the lethal strike consumes one Mark"), GameXXKCardRules::GetCombatStatusStacks(*FindUnit(DeathStop, EnemyAId), EGameXXKCardStatus::Mark), 2);
	return true;
}

#endif
