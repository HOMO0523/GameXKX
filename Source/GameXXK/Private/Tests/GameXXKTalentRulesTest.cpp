#include "Misc/AutomationTest.h"

#include "GameXXKTalentCatalog.h"
#include "GameXXKTalentRules.h"
#include "GameXXKMVPRules.h"
#include "Serialization/MemoryWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentPriceCurveTest,
	"GameXXK.Talents.Rules.PriceCurveInt64",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentPriceCurveTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("tier zero costs 2500"), FGameXXKTalentRules::GetPriceForCostTier(0), int64(2500));
	TestEqual(TEXT("tier one rounds to 3400"), FGameXXKTalentRules::GetPriceForCostTier(1), int64(3400));
	TestEqual(TEXT("tier five rounds to 11200"), FGameXXKTalentRules::GetPriceForCostTier(5), int64(11200));
	TestEqual(TEXT("tier thirty-five uses the approved exponential curve"),
		FGameXXKTalentRules::GetPriceForCostTier(35), int64(91121700));
	TestEqual(TEXT("complete capacity path uses checked 64-bit accumulation"),
		FGameXXKTalentRules::GetFullCapacityPathPrice(), int64(1757301500));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentPurchaseTransactionTest,
	"GameXXK.Talents.Rules.PurchaseTransactionRevealAndMaxRank",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentPurchaseTransactionTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
	State.PlayerGold = 2500;
	FGameXXKTalentPurchaseResult Result;
	TestTrue(TEXT("root purchases with exactly 2500 gold"),
		FGameXXKTalentRules::Purchase(State, TEXT("Talent.Root"), Result));
	TestEqual(TEXT("root purchase spends all ordinary gold"), State.PlayerGold, 0);
	TestEqual(TEXT("root purchase records rank one"), State.Talents.NodeRanks.FindRef(TEXT("Talent.Root")), 1);
	TestEqual(TEXT("root purchase reports its fixed price"), Result.Price, int64(2500));

	const FGameXXKTalentNodeDefinition* CombatEntry =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Entry.Combat"));
	TestTrue(TEXT("root reveals the combat entry"),
		CombatEntry && FGameXXKTalentRules::IsRevealed(State.Talents, *CombatEntry));
	const TArray<uint8> BeforeInsufficient = [&State]()
	{
		TArray<uint8> Bytes;
		FMemoryWriter Writer(Bytes, true);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
		return Bytes;
	}();
	TestFalse(TEXT("entry purchase rejects insufficient ordinary gold"),
		FGameXXKTalentRules::Purchase(State, TEXT("Talent.Entry.Combat"), Result));
	TArray<uint8> AfterInsufficient;
	{
		FMemoryWriter Writer(AfterInsufficient, true);
		FGameXXKRuntimeState Copy = State;
		FGameXXKRuntimeState::StaticStruct()->SerializeItem(Writer, &Copy, nullptr);
	}
	TestEqual(TEXT("failed purchase is byte-transactional"), AfterInsufficient, BeforeInsufficient);

	State.PlayerGold = 2500;
	TestTrue(TEXT("combat entry purchases after funding"),
		FGameXXKTalentRules::Purchase(State, TEXT("Talent.Entry.Combat"), Result));
	const FName FirstAttack(TEXT("Talent.Combat.FlatAttack.01"));
	const FName SecondAttack(TEXT("Talent.Combat.FlatAttack.02"));
	const FGameXXKTalentNodeDefinition* FirstAttackNode = FGameXXKTalentCatalog::Find(FirstAttack);
	const FGameXXKTalentNodeDefinition* SecondAttackNode = FGameXXKTalentCatalog::Find(SecondAttack);
	TestTrue(TEXT("entry reveals the first attack node"),
		FirstAttackNode && FGameXXKTalentRules::IsRevealed(State.Talents, *FirstAttackNode));
	TestFalse(TEXT("second attack node stays hidden before its predecessor is purchased"),
		SecondAttackNode && FGameXXKTalentRules::IsRevealed(State.Talents, *SecondAttackNode));

	State.PlayerGold = 3400 * 5;
	for (int32 Rank = 1; Rank <= 5; ++Rank)
	{
		TestTrue(FString::Printf(TEXT("attack rank %d purchases"), Rank),
			FGameXXKTalentRules::Purchase(State, FirstAttack, Result));
		TestEqual(TEXT("every rank of one node keeps the same price"), Result.Price, int64(3400));
	}
	TestEqual(TEXT("attack node reaches rank five"), State.Talents.NodeRanks.FindRef(FirstAttack), 5);
	TestFalse(TEXT("max-rank node rejects another purchase"),
		FGameXXKTalentRules::Purchase(State, FirstAttack, Result));
	TestTrue(TEXT("purchasing predecessor reveals its successor"),
		SecondAttackNode && FGameXXKTalentRules::IsRevealed(State.Talents, *SecondAttackNode));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentProjectionCapsTest,
	"GameXXK.Talents.Rules.ProjectionCapsCapacityOfflineAndTools",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentProjectionCapsTest::RunTest(const FString& Parameters)
{
	FGameXXKTalentProgress Progress;
	for (const FGameXXKTalentNodeDefinition& Node : FGameXXKTalentCatalog::GetDefinitions())
	{
		Progress.NodeRanks.Add(Node.Id, Node.MaxRank);
	}
	FGameXXKTalentProjection Projection;
	FString Error;
	TestTrue(FString::Printf(TEXT("full authored tree projects: %s"), *Error),
		FGameXXKTalentRules::BuildProjection(Progress, Projection, &Error));
	TestEqual(TEXT("flat attack caps at 200"), Projection.FlatAttack, 200);
	TestEqual(TEXT("flat health caps at 200"), Projection.FlatMaxHP, 200);
	TestEqual(TEXT("flat defense caps at 200"), Projection.FlatDefense, 200);
	TestEqual(TEXT("route attack caps at 100 percent"), Projection.RouteAttackPercent, 100);
	TestEqual(TEXT("final damage caps at 100 percent"), Projection.RouteFinalDamagePercent, 100);
	TestEqual(TEXT("route defense caps at 100 percent"), Projection.RouteDefensePercent, 100);
	TestEqual(TEXT("route health caps at 100 percent"), Projection.RouteMaxHPPercent, 100);
	TestEqual(TEXT("critical chance caps at 20 percent"), Projection.CriticalChancePercent, 20);
	TestEqual(TEXT("critical damage caps at 50 percent"), Projection.CriticalDamagePercent, 50);
	TestEqual(TEXT("movement track reaches rank five"), Projection.TravelMovementRank, 5);
	TestTrue(TEXT("movement rank five produces 2.5 seconds"),
		FMath::IsNearlyEqual(Projection.GetTravelWalkSeconds(), 2.5f));
	TestEqual(TEXT("backpack reaches physical capacity 200"), Projection.BackpackCapacity, 200);
	TestEqual(TEXT("warehouse reaches six logical pages"), Projection.WarehousePageCount, 6);
	TestTrue(TEXT("offline rewards are unlocked"), Projection.bOfflineRewardsUnlocked);
	TestTrue(TEXT("all tool modes are unlocked"), Projection.bToolsUnlocked);
	TestEqual(TEXT("online gold reaches +350 percent"), Projection.OnlineGoldPercent, 350);
	TestEqual(TEXT("online experience reaches +350 percent"), Projection.OnlineExperiencePercent, 350);
	TestEqual(TEXT("offline gold reaches +350 percent"), Projection.OfflineGoldPercent, 350);
	TestEqual(TEXT("offline experience reaches +350 percent"), Projection.OfflineExperiencePercent, 350);
	TestEqual(TEXT("offline gold time reaches +350 percent"), Projection.OfflineGoldTimePercent, 350);
	TestEqual(TEXT("offline experience time reaches +350 percent"), Projection.OfflineExperienceTimePercent, 350);
	TestEqual(TEXT("normal chest relative drop reaches +350 percent"), Projection.NormalChestDropPercent, 350);
	TestEqual(TEXT("advanced chest relative drop reaches +350 percent"), Projection.AdvancedChestDropPercent, 350);
	TestEqual(TEXT("offline gold cap reaches 108 hours"), Projection.GetOfflineGoldCapSeconds(), 108 * 60 * 60);
	TestEqual(TEXT("offline experience cap reaches 108 hours"), Projection.GetOfflineExperienceCapSeconds(), 108 * 60 * 60);
	TestEqual(TEXT("offline chest cap reaches 16h45m"), Projection.GetOfflineChestCapSeconds(), 16 * 60 * 60 + 45 * 60);
	TestEqual(TEXT("tool experience reaches +250 percent"), Projection.ToolExperiencePercent, 250);
	TestEqual(TEXT("tool gold reaches +250 percent"), Projection.ToolGoldPercent, 250);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTalentRankOneGridUnlockTest,
	"GameXXK.Talents.Rules.RankOneUnlocksTwoSidesAndNextDiagonal",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTalentRankOneGridUnlockTest::RunTest(const FString& Parameters)
{
	FGameXXKTalentProgress Progress;
	Progress.NodeRanks.Add(TEXT("Talent.Root"), 1);
	Progress.NodeRanks.Add(TEXT("Talent.Entry.Combat"), 1);
	const FGameXXKTalentNodeDefinition* HorizontalRoot =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatAttack.01"));
	const FGameXXKTalentNodeDefinition* VerticalRoot =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatHealth.01"));
	const FGameXXKTalentNodeDefinition* NextMain =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatAttack.08"));
	const FGameXXKTalentNodeDefinition* NextHorizontalRoot =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatDefense.01"));
	const FGameXXKTalentNodeDefinition* NextVerticalRoot =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.Movement.01"));
	if (!TestNotNull(TEXT("horizontal root exists"), HorizontalRoot)
		|| !TestNotNull(TEXT("vertical root exists"), VerticalRoot)
		|| !TestNotNull(TEXT("next main exists"), NextMain)
		|| !TestNotNull(TEXT("next horizontal root exists"), NextHorizontalRoot)
		|| !TestNotNull(TEXT("next vertical root exists"), NextVerticalRoot))
	{
		return false;
	}

	TestTrue(TEXT("entry reveals the first horizontal root"),
		FGameXXKTalentRules::IsRevealed(Progress, *HorizontalRoot));
	TestTrue(TEXT("entry reveals the first vertical root"),
		FGameXXKTalentRules::IsRevealed(Progress, *VerticalRoot));
	TestTrue(TEXT("entry also extends one node along the 45-degree main line"),
		FGameXXKTalentRules::IsRevealed(Progress, *NextMain));
	TestFalse(TEXT("second-cycle horizontal root stays hidden before next main rank one"),
		FGameXXKTalentRules::IsRevealed(Progress, *NextHorizontalRoot));
	TestFalse(TEXT("second-cycle vertical root stays hidden before next main rank one"),
		FGameXXKTalentRules::IsRevealed(Progress, *NextVerticalRoot));
	Progress.NodeRanks.Add(NextMain->Id, 1);
	TestTrue(TEXT("next main rank one reveals second-cycle horizontal root"),
		FGameXXKTalentRules::IsRevealed(Progress, *NextHorizontalRoot));
	TestTrue(TEXT("next main rank one reveals second-cycle vertical root"),
		FGameXXKTalentRules::IsRevealed(Progress, *NextVerticalRoot));
	const FGameXXKTalentNodeDefinition* ThirdMain =
		FGameXXKTalentCatalog::Find(TEXT("Talent.Combat.FlatHealth.08"));
	TestTrue(TEXT("next main rank one continues the 45-degree line again"),
		ThirdMain && FGameXXKTalentRules::IsRevealed(Progress, *ThirdMain));
	return true;
}

#endif
