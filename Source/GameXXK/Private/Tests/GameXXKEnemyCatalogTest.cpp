#include "Misc/AutomationTest.h"

#include "GameXXKEnemyCatalog.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FExpectedEnemy
	{
		FName Id;
		FString DisplayName;
		int32 Chapter = 0;
		EGameXXKEnemyTier Tier = EGameXXKEnemyTier::Normal;
		int32 BaseHP = 0;
		float HPPerLevel = 0.0f;
		int32 BaseAttack = 0;
		float AttackPerLevel = 0.0f;
		int32 BaseDefense = 0;
		float DefensePerLevel = 0.0f;
		int32 Speed = 0;
		int32 IntentCount = 0;
		EGameXXKEnemyPassiveId PassiveId = EGameXXKEnemyPassiveId::None;
		EGameXXKEnemyPhaseId PhaseId = EGameXXKEnemyPhaseId::None;
	};

	const TArray<FExpectedEnemy>& GetExpectedEnemies()
	{
		static const TArray<FExpectedEnemy> Enemies{
			{TEXT("Enemy.Ch1.Rooster"), TEXT("公鸡"), 1, EGameXXKEnemyTier::Normal, 46, 7.0f, 8, 1.1f, 1, 0.25f, 10, 3},
			{TEXT("Enemy.Ch1.Goat"), TEXT("山羊"), 1, EGameXXKEnemyTier::Normal, 58, 8.0f, 7, 1.0f, 3, 0.35f, 6, 3},
			{TEXT("Enemy.Ch1.Weasel"), TEXT("黄鼬"), 1, EGameXXKEnemyTier::Normal, 42, 6.0f, 9, 1.2f, 1, 0.20f, 11, 3},
			{TEXT("Enemy.Ch1.Civet"), TEXT("狸猫"), 1, EGameXXKEnemyTier::Normal, 48, 7.0f, 8, 1.1f, 2, 0.25f, 9, 3},
			{TEXT("Enemy.Ch1.IronfeatherRooster"), TEXT("铁羽斗鸡"), 1, EGameXXKEnemyTier::Elite, 118, 15.0f, 14, 1.7f, 5, 0.50f, 11, 4, EGameXXKEnemyPassiveId::IronfeatherFirstHit},
			{TEXT("Enemy.Ch1.BluehornGoatKing"), TEXT("青角羊王"), 1, EGameXXKEnemyTier::Elite, 138, 17.0f, 13, 1.6f, 7, 0.65f, 7, 4, EGameXXKEnemyPassiveId::BluehornArmorRetention},
			{TEXT("Enemy.Ch1.MoneyRat"), TEXT("金钱鼠"), 1, EGameXXKEnemyTier::Boss, 240, 24.0f, 17, 2.0f, 8, 0.75f, 10, 6, EGameXXKEnemyPassiveId::MoneyRatWealth, EGameXXKEnemyPhaseId::MoneyRatMadHoard},
			{TEXT("Enemy.Ch2.GrayWolf"), TEXT("灰狼"), 2, EGameXXKEnemyTier::Normal, 62, 9.0f, 11, 1.3f, 2, 0.30f, 12, 3},
			{TEXT("Enemy.Ch2.Boar"), TEXT("野猪"), 2, EGameXXKEnemyTier::Normal, 76, 10.0f, 10, 1.2f, 5, 0.45f, 7, 3},
			{TEXT("Enemy.Ch2.Macaque"), TEXT("猕猴"), 2, EGameXXKEnemyTier::Normal, 58, 8.0f, 10, 1.3f, 2, 0.25f, 13, 3},
			{TEXT("Enemy.Ch2.Porcupine"), TEXT("豪猪"), 2, EGameXXKEnemyTier::Normal, 70, 9.0f, 9, 1.1f, 5, 0.50f, 8, 3, EGameXXKEnemyPassiveId::PorcupineCounter},
			{TEXT("Enemy.Ch2.GraymaneWolfKing"), TEXT("苍鬃狼王"), 2, EGameXXKEnemyTier::Elite, 158, 18.0f, 18, 2.0f, 6, 0.55f, 13, 4, EGameXXKEnemyPassiveId::GraymaneMarkedHunt},
			{TEXT("Enemy.Ch2.RedtuskBoarKing"), TEXT("赤獠猪王"), 2, EGameXXKEnemyTier::Elite, 188, 20.0f, 17, 1.9f, 9, 0.75f, 8, 4, EGameXXKEnemyPassiveId::RedtuskRage},
			{TEXT("Enemy.Ch2.BlackBear"), TEXT("黑熊"), 2, EGameXXKEnemyTier::Boss, 320, 30.0f, 23, 2.4f, 11, 0.90f, 7, 6, EGameXXKEnemyPassiveId::BlackBearThickHide, EGameXXKEnemyPhaseId::BlackBearEnraged},
			{TEXT("Enemy.Ch3.VenomSnake"), TEXT("毒蛇"), 3, EGameXXKEnemyTier::Normal, 72, 9.0f, 12, 1.35f, 2, 0.25f, 14, 3},
			{TEXT("Enemy.Ch3.Wildcat"), TEXT("山猫"), 3, EGameXXKEnemyTier::Normal, 70, 9.0f, 14, 1.50f, 3, 0.30f, 14, 3},
			{TEXT("Enemy.Ch3.Vulture"), TEXT("秃鹫"), 3, EGameXXKEnemyTier::Normal, 74, 9.0f, 13, 1.45f, 3, 0.30f, 15, 3},
			{TEXT("Enemy.Ch3.GiantToad"), TEXT("巨蟾"), 3, EGameXXKEnemyTier::Normal, 94, 12.0f, 11, 1.25f, 7, 0.60f, 6, 3},
			{TEXT("Enemy.Ch3.WhiteApe"), TEXT("白猿"), 3, EGameXXKEnemyTier::Elite, 198, 21.0f, 21, 2.20f, 8, 0.65f, 12, 4, EGameXXKEnemyPassiveId::WhiteApeStatusGuard},
			{TEXT("Enemy.Ch3.SpiralHornDeer"), TEXT("盘角鹿"), 3, EGameXXKEnemyTier::Elite, 210, 22.0f, 20, 2.10f, 9, 0.75f, 11, 4, EGameXXKEnemyPassiveId::DeerHealCooldown},
			{TEXT("Enemy.Ch3.Tiger"), TEXT("老虎"), 3, EGameXXKEnemyTier::Boss, 380, 34.0f, 28, 2.70f, 12, 1.00f, 14, 6, EGameXXKEnemyPassiveId::TigerPredator, EGameXXKEnemyPhaseId::TigerDread}};
		return Enemies;
	}

	const TMap<FName, TArray<FName>>& GetExpectedIntentIds()
	{
		static const TMap<FName, TArray<FName>> IntentIds{
			{TEXT("Enemy.Ch1.Rooster"), {TEXT("Peck"), TEXT("DoublePeck"), TEXT("Crow")}},
			{TEXT("Enemy.Ch1.Goat"), {TEXT("Horn"), TEXT("Stomp"), TEXT("Charge")}},
			{TEXT("Enemy.Ch1.Weasel"), {TEXT("Harass"), TEXT("StinkFog"), TEXT("Escape")}},
			{TEXT("Enemy.Ch1.Civet"), {TEXT("Claw"), TEXT("Feint"), TEXT("Pickpocket")}},
			{TEXT("Enemy.Ch1.IronfeatherRooster"), {TEXT("RapidPeck"), TEXT("IronGuard"), TEXT("BattleCry"), TEXT("BloodFight")}},
			{TEXT("Enemy.Ch1.BluehornGoatKing"), {TEXT("Pierce"), TEXT("HerdStomp"), TEXT("GuardHerd"), TEXT("RageCharge")}},
			{TEXT("Enemy.Ch1.MoneyRat"), {TEXT("CoinVolley"), TEXT("Hoard"), TEXT("GreedyMark"), TEXT("Pickpocket"), TEXT("BreakWealth"), TEXT("CoinCrash")}},
			{TEXT("Enemy.Ch2.GrayWolf"), {TEXT("Bite"), TEXT("Pursuit"), TEXT("CallPack")}},
			{TEXT("Enemy.Ch2.Boar"), {TEXT("Tusk"), TEXT("Bristle"), TEXT("ArmorBreakCharge")}},
			{TEXT("Enemy.Ch2.Macaque"), {TEXT("ThrowStone"), TEXT("Snatch"), TEXT("Hasten")}},
			{TEXT("Enemy.Ch2.Porcupine"), {TEXT("Quill"), TEXT("BristleGuard"), TEXT("QuillVolley")}},
			{TEXT("Enemy.Ch2.GraymaneWolfKing"), {TEXT("HuntMark"), TEXT("ContinuousHunt"), TEXT("PackOrder"), TEXT("Sidestep")}},
			{TEXT("Enemy.Ch2.RedtuskBoarKing"), {TEXT("HeavyArmor"), TEXT("Earthquake"), TEXT("RageStrike"), TEXT("RedCharge")}},
			{TEXT("Enemy.Ch2.BlackBear"), {TEXT("Sweep"), TEXT("Pounce"), TEXT("WeakRoar"), TEXT("Rend"), TEXT("CounterPosture"), TEXT("Quake")}},
			{TEXT("Enemy.Ch3.VenomSnake"), {TEXT("VenomBite"), TEXT("Coil"), TEXT("ToxicPursuit")}},
			{TEXT("Enemy.Ch3.Wildcat"), {TEXT("Rake"), TEXT("Stalk"), TEXT("BloodPursuit")}},
			{TEXT("Enemy.Ch3.Vulture"), {TEXT("Gaze"), TEXT("Dive"), TEXT("WingCut")}},
			{TEXT("Enemy.Ch3.GiantToad"), {TEXT("Tongue"), TEXT("PoisonFog"), TEXT("Inflate")}},
			{TEXT("Enemy.Ch3.WhiteApe"), {TEXT("ThrowRock"), TEXT("Disturb"), TEXT("BoulderCharge"), TEXT("WideSweep")}},
			{TEXT("Enemy.Ch3.SpiralHornDeer"), {TEXT("Horn"), TEXT("TerrainBless"), TEXT("HerdArmor"), TEXT("SpringHeal")}},
			{TEXT("Enemy.Ch3.Tiger"), {TEXT("MarkPrey"), TEXT("TigerPounce"), TEXT("TailSweep"), TEXT("BleedingRend"), TEXT("DreadRoar"), TEXT("Ambush")}}};
		return IntentIds;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKEnemyCatalogTest,
	"GameXXK.Data.EnemyCatalog",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKEnemyCatalogTest::RunTest(const FString& Parameters)
{
	FString ValidationError;
	TestTrue(TEXT("catalog validates"), FGameXXKEnemyCatalog::Validate(&ValidationError));
	const TArray<FGameXXKEnemyDefinition>& Definitions = FGameXXKEnemyCatalog::GetAllDefinitions();
	TestEqual(TEXT("exact enemy count"), Definitions.Num(), 21);
	TestEqual(TEXT("expected identity matrix count"), GetExpectedEnemies().Num(), 21);
	TestEqual(TEXT("expected intent matrix count"), GetExpectedIntentIds().Num(), 21);

	TSet<FName> SeenIds;
	for (const FExpectedEnemy& Expected : GetExpectedEnemies())
	{
		const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Expected.Id);
		TestNotNull(FString::Printf(TEXT("catalog has %s"), *Expected.Id.ToString()), Definition);
		if (!Definition)
		{
			continue;
		}
		TestTrue(FString::Printf(TEXT("ID %s is globally unique"), *Expected.Id.ToString()), !SeenIds.Contains(Definition->Id));
		SeenIds.Add(Definition->Id);
		TestEqual(FString::Printf(TEXT("display %s"), *Expected.Id.ToString()), Definition->DisplayName.ToString(), Expected.DisplayName);
		TestEqual(FString::Printf(TEXT("chapter %s"), *Expected.Id.ToString()), Definition->Chapter, Expected.Chapter);
		TestEqual(FString::Printf(TEXT("tier %s"), *Expected.Id.ToString()), Definition->Tier, Expected.Tier);
		TestEqual(FString::Printf(TEXT("base HP %s"), *Expected.Id.ToString()), Definition->BaseHP, Expected.BaseHP);
		TestTrue(FString::Printf(TEXT("HP growth %s"), *Expected.Id.ToString()), FMath::IsNearlyEqual(Definition->HPPerLevel, Expected.HPPerLevel));
		TestEqual(FString::Printf(TEXT("base attack %s"), *Expected.Id.ToString()), Definition->BaseAttack, Expected.BaseAttack);
		TestTrue(FString::Printf(TEXT("attack growth %s"), *Expected.Id.ToString()), FMath::IsNearlyEqual(Definition->AttackPerLevel, Expected.AttackPerLevel));
		TestEqual(FString::Printf(TEXT("base defense %s"), *Expected.Id.ToString()), Definition->BaseDefense, Expected.BaseDefense);
		TestTrue(FString::Printf(TEXT("defense growth %s"), *Expected.Id.ToString()), FMath::IsNearlyEqual(Definition->DefensePerLevel, Expected.DefensePerLevel));
		TestEqual(FString::Printf(TEXT("speed %s"), *Expected.Id.ToString()), Definition->Speed, Expected.Speed);
		TestEqual(FString::Printf(TEXT("intent count %s"), *Expected.Id.ToString()), Definition->Intents.Num(), Expected.IntentCount);
		TestEqual(FString::Printf(TEXT("passive %s"), *Expected.Id.ToString()), Definition->PassiveId, Expected.PassiveId);
		TestEqual(FString::Printf(TEXT("phase %s"), *Expected.Id.ToString()), Definition->PhaseId, Expected.PhaseId);
		TestEqual(FString::Printf(TEXT("phase threshold %s"), *Expected.Id.ToString()), Definition->PhaseThresholdPercent, Expected.PhaseId == EGameXXKEnemyPhaseId::None ? 0 : 50);
		TestTrue(FString::Printf(TEXT("codex ID %s"), *Expected.Id.ToString()), !Definition->CodexId.IsNone());
		TestTrue(FString::Printf(TEXT("portrait path %s"), *Expected.Id.ToString()), Definition->PortraitSoftPath.IsValid());
		TestTrue(FString::Printf(TEXT("battle visual path %s"), *Expected.Id.ToString()), Definition->BattleVisualSoftPath.IsValid());
		const TArray<FName>* ExpectedIntentIds = GetExpectedIntentIds().Find(Expected.Id);
		TestNotNull(FString::Printf(TEXT("intent matrix for %s"), *Expected.Id.ToString()), ExpectedIntentIds);
		if (ExpectedIntentIds)
		{
			TestEqual(FString::Printf(TEXT("intent matrix size %s"), *Expected.Id.ToString()), Definition->Intents.Num(), ExpectedIntentIds->Num());
			for (int32 IntentIndex = 0; IntentIndex < FMath::Min(Definition->Intents.Num(), ExpectedIntentIds->Num()); ++IntentIndex)
			{
				TestEqual(
					FString::Printf(TEXT("intent %d ID %s"), IntentIndex, *Expected.Id.ToString()),
					Definition->Intents[IntentIndex].Id,
					(*ExpectedIntentIds)[IntentIndex]);
			}
		}
		for (const FGameXXKEnemyIntentDefinition& Intent : Definition->Intents)
		{
			TestTrue(FString::Printf(TEXT("intent ID %s"), *Expected.Id.ToString()), !Intent.Id.IsNone());
			TestTrue(FString::Printf(TEXT("intent effects %s"), *Intent.Id.ToString()), !Intent.Effects.IsEmpty());
		}

		const FGameXXKEnemyComputedStats LevelFive = FGameXXKEnemyCatalog::ComputeStats(Expected.Id, 5);
		TestTrue(FString::Printf(TEXT("computed HP stays positive %s"), *Expected.Id.ToString()), LevelFive.MaxHP >= 1);
		TestTrue(FString::Printf(TEXT("computed attack stays positive %s"), *Expected.Id.ToString()), LevelFive.Attack >= 1);
		TestTrue(FString::Printf(TEXT("computed defense stays non-negative %s"), *Expected.Id.ToString()), LevelFive.Defense >= 0);
		TestTrue(FString::Printf(TEXT("computed speed stays positive %s"), *Expected.Id.ToString()), LevelFive.Speed >= 1);
	}

	for (int32 Chapter = 1; Chapter <= 3; ++Chapter)
	{
		TestEqual(FString::Printf(TEXT("chapter %d normal pool"), Chapter), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Normal).Num(), 4);
		TestEqual(FString::Printf(TEXT("chapter %d elite pool"), Chapter), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Elite).Num(), 2);
		TestEqual(FString::Printf(TEXT("chapter %d boss pool"), Chapter), FGameXXKEnemyCatalog::GetPool(Chapter, EGameXXKEnemyTier::Boss).Num(), 1);
		for (const EGameXXKEnemyTier Tier : {EGameXXKEnemyTier::Normal, EGameXXKEnemyTier::Elite, EGameXXKEnemyTier::Boss})
		{
			for (const FName Id : FGameXXKEnemyCatalog::GetPool(Chapter, Tier))
			{
				const FGameXXKEnemyDefinition* Definition = FGameXXKEnemyCatalog::Find(Id);
				TestTrue(FString::Printf(TEXT("pool member %s has matching chapter"), *Id.ToString()), Definition && Definition->Chapter == Chapter);
				TestTrue(FString::Printf(TEXT("pool member %s has matching tier"), *Id.ToString()), Definition && Definition->Tier == Tier);
			}
		}
	}
	return true;
}

#endif
