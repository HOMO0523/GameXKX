#include "GameXXKCardRules.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

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
			Instance.InstanceId = FName(*FString::Printf(TEXT("Compatibility.Instance.%s.%d"), *CardId.ToString(), Index));
			Instance.CardId = CardId;
			Instance.CurrentQuality = EGameXXKCardQuality::Common;
			Instance.OwnerUnitId = TEXT("Hunter");
			Instance.SourceEntryId = FName(*FString::Printf(TEXT("Compatibility.Source.%s.%d"), *CardId.ToString(), Index));
			Instance.AcquisitionOrdinal = Index;
		}
		return Instances;
	}

	bool InitializeCompatibilityRuntime(FGameXXKCardBattleRuntime& OutRuntime, const FName CardId)
	{
		TArray<FGameXXKCardCombatUnit> Units = {
			MakeCompatibilityUnit(TEXT("Hunter"), EGameXXKCardTargetSide::Party, EGameXXKCharacterRole::Hunter, 1),
			MakeCompatibilityUnit(TEXT("Enemy"), EGameXXKCardTargetSide::Enemy, EGameXXKCharacterRole::Invalid, 10)};
		return GameXXKCardRules::InitializeCardBattleRuntime(
			OutRuntime,
			MakeCompatibilityCards(CardId, 10),
			Units,
			EGameXXKCardTerrain::Plain,
			18801);
	}

	FGameXXKCardCombatUnit* FindCompatibilityUnit(
		TArray<FGameXXKCardCombatUnit>& Units,
		const FName UnitId)
	{
		return Units.FindByPredicate([UnitId](const FGameXXKCardCombatUnit& Unit)
		{
			return Unit.UnitId == UnitId;
		});
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKMarkCardCompatibilityTest,
	"GameXXK.Integration.MarkCardCompatibility",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKMarkCardCompatibilityTest::RunTest(const FString& Parameters)
{
	FGameXXKCardBattleRuntime ZhuiLieRuntime;
	const bool bZhuiLieInitialized = InitializeCompatibilityRuntime(ZhuiLieRuntime, TEXT("Profession.Hunter.ZhuiLie"));
	TestTrue(TEXT("Zhui Lie compatibility runtime initializes"), bZhuiLieInitialized);
	if (!bZhuiLieInitialized)
	{
		return false;
	}
	FGameXXKCardCombatUnit* ZhuiLieEnemy = FindCompatibilityUnit(ZhuiLieRuntime.Units, TEXT("Enemy"));
	TestNotNull(TEXT("Zhui Lie keeps the stable enemy target"), ZhuiLieEnemy);
	if (!ZhuiLieEnemy)
	{
		return false;
	}
	TestEqual(TEXT("Zhui Lie target starts with one Mark"),
		GameXXKCardRules::AddCombatStatus(*ZhuiLieEnemy, EGameXXKCardStatus::Mark, 1), 1);
	FGameXXKCardPlayResult ZhuiLieResult;
	TestTrue(TEXT("Zhui Lie resolves against the marked enemy"), GameXXKCardRules::ResolveCardPlay(
		ZhuiLieRuntime, ZhuiLieRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), ZhuiLieResult));
	TestEqual(TEXT("Zhui Lie direct damage consumes the target's only live Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindCompatibilityUnit(ZhuiLieRuntime.Units, TEXT("Enemy")), EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("Zhui Lie keeps its start-of-card Mark draw reward"), ZhuiLieRuntime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("Zhui Lie keeps its start-of-card Mark mana reward"),
		FindCompatibilityUnit(ZhuiLieRuntime.Units, TEXT("Hunter"))->Mana, 18);

	FGameXXKCardBattleRuntime LethalZhuiLieRuntime;
	const bool bLethalZhuiLieInitialized = InitializeCompatibilityRuntime(
		LethalZhuiLieRuntime,
		TEXT("Profession.Hunter.ZhuiLie"));
	TestTrue(TEXT("lethal Zhui Lie compatibility runtime initializes"), bLethalZhuiLieInitialized);
	if (!bLethalZhuiLieInitialized)
	{
		return false;
	}
	FGameXXKCardCombatUnit* LethalZhuiLieEnemy = FindCompatibilityUnit(LethalZhuiLieRuntime.Units, TEXT("Enemy"));
	TestNotNull(TEXT("lethal Zhui Lie keeps the stable enemy target"), LethalZhuiLieEnemy);
	if (!LethalZhuiLieEnemy)
	{
		return false;
	}
	LethalZhuiLieEnemy->HP = 1;
	TestEqual(TEXT("lethal Zhui Lie target starts with one Mark"),
		GameXXKCardRules::AddCombatStatus(*LethalZhuiLieEnemy, EGameXXKCardStatus::Mark, 1), 1);
	LethalZhuiLieRuntime.Deck.SharedEnergy = 3;
	FGameXXKCardPlayResult LethalZhuiLieResult;
	TestTrue(TEXT("lethal Zhui Lie resolves against the marked enemy"), GameXXKCardRules::ResolveCardPlay(
		LethalZhuiLieRuntime,
		LethalZhuiLieRuntime.Deck.Hand[0].InstanceId,
		TEXT("Enemy"),
		LethalZhuiLieResult));
	TestFalse(TEXT("lethal Zhui Lie defeats the marked enemy"),
		FindCompatibilityUnit(LethalZhuiLieRuntime.Units, TEXT("Enemy"))->bLiving);
	TestEqual(TEXT("lethal Zhui Lie still grants its start-of-card Mark draw reward"),
		LethalZhuiLieRuntime.Deck.Hand.Num(), 5);
	TestEqual(TEXT("lethal Zhui Lie still grants its start-of-card Mark Energy reward"),
		LethalZhuiLieRuntime.Deck.SharedEnergy, 3);

	FGameXXKCardBattleRuntime BaiBuRuntime;
	const bool bBaiBuInitialized = InitializeCompatibilityRuntime(BaiBuRuntime, TEXT("Profession.Hunter.BaiBuChuanYang"));
	TestTrue(TEXT("Bai Bu Chuan Yang compatibility runtime initializes"), bBaiBuInitialized);
	if (!bBaiBuInitialized)
	{
		return false;
	}
	FGameXXKCardCombatUnit* BaiBuEnemy = FindCompatibilityUnit(BaiBuRuntime.Units, TEXT("Enemy"));
	TestNotNull(TEXT("Bai Bu Chuan Yang keeps the stable enemy target"), BaiBuEnemy);
	if (!BaiBuEnemy)
	{
		return false;
	}
	TestEqual(TEXT("Bai Bu Chuan Yang target starts with five Mark"),
		GameXXKCardRules::AddCombatStatus(*BaiBuEnemy, EGameXXKCardStatus::Mark, 5), 5);
	FGameXXKCardPlayResult BaiBuResult;
	TestTrue(TEXT("Bai Bu Chuan Yang resolves against the five-Mark enemy"), GameXXKCardRules::ResolveCardPlay(
		BaiBuRuntime, BaiBuRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), BaiBuResult));
	TestEqual(TEXT("Bai Bu Chuan Yang direct damage consumes one live Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*FindCompatibilityUnit(BaiBuRuntime.Units, TEXT("Enemy")), EGameXXKCardStatus::Mark), 4);
	TestEqual(TEXT("Bai Bu Chuan Yang keeps its five-Mark start-of-card draw reward"), BaiBuRuntime.Deck.Hand.Num(), 6);

	FGameXXKCardBattleRuntime LianZhuRuntime;
	const bool bLianZhuInitialized = InitializeCompatibilityRuntime(LianZhuRuntime, TEXT("Profession.Hunter.LianZhuJian"));
	TestTrue(TEXT("Lian Zhu Jian compatibility runtime initializes"), bLianZhuInitialized);
	if (!bLianZhuInitialized)
	{
		return false;
	}
	FGameXXKCardPlayResult LianZhuResult;
	TestTrue(TEXT("Lian Zhu Jian resolves against an initially unmarked enemy"), GameXXKCardRules::ResolveCardPlay(
		LianZhuRuntime, LianZhuRuntime.Deck.Hand[0].InstanceId, TEXT("Enemy"), LianZhuResult));
	TestEqual(TEXT("Lian Zhu Jian produces one attack plus one Bleed trigger"), LianZhuResult.DamageResults.Num(), 2);
	int32 DirectAttackPackets = 0;
	int32 BleedPackets = 0;
	for (const FGameXXKCardDamageResult& DamageResult : LianZhuResult.DamageResults)
	{
		DirectAttackPackets += DamageResult.Cause == EGameXXKCardDamageCause::DirectAttack ? 1 : 0;
		BleedPackets += DamageResult.Cause == EGameXXKCardDamageCause::Bleed ? 1 : 0;
		TestEqual(TEXT("an unmarked Lian Zhu packet receives no Mark bonus"), DamageResult.MarkDamageBonusPercent, 0);
	}
	TestEqual(TEXT("Lian Zhu Jian has exactly one base direct attack without stored Charge"), DirectAttackPackets, 1);
	TestEqual(TEXT("Lian Zhu Jian triggers its newly applied Bleed exactly once"), BleedPackets, 1);
	const FGameXXKCardCombatUnit* LianZhuEnemy = FindCompatibilityUnit(LianZhuRuntime.Units, TEXT("Enemy"));
	const FGameXXKCardCombatUnit* LianZhuHunter = FindCompatibilityUnit(LianZhuRuntime.Units, TEXT("Hunter"));
	TestEqual(TEXT("Lian Zhu Jian never creates an implicit Mark"),
		GameXXKCardRules::GetCombatStatusStacks(*LianZhuEnemy, EGameXXKCardStatus::Mark), 0);
	TestEqual(TEXT("the attack consumes one of Lian Zhu Jian's eight Bleed"),
		GameXXKCardRules::GetCombatStatusStacks(*LianZhuEnemy, EGameXXKCardStatus::Bleed), 7);
	TestEqual(TEXT("Lian Zhu Jian leaves Poison6"),
		GameXXKCardRules::GetCombatStatusStacks(*LianZhuEnemy, EGameXXKCardStatus::Poison), 6);
	TestEqual(TEXT("Lian Zhu Jian grants one new Charge after its attack"),
		GameXXKCardRules::GetCombatStatusStacks(*LianZhuHunter, EGameXXKCardStatus::Charge), 1);

	return true;
}

#endif
