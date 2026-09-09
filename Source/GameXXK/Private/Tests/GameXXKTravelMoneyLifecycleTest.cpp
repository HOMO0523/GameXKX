#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "GameXXKTravelMoneyRules.h"
#include "GameXXKMVPRules.h"
#include "GameXXKMetaShopRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKGemRules.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelMoneyLifecycleTest,
	"GameXXK.RouteTravelMoney.ShopDismantleAndPersistence", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTravelMoneyLifecycleTest::RunTest(const FString& Parameters)
{
	auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("fixture starts"), Subsystem->StartGame())) return false;
	auto State = Subsystem->GetRuntimeStateCopy();
	State.Screen = EGameXXKScreen::Town;
	State.PlayerGold = 200000;
	const FName Money = FGameXXKTravelMoneyRules::ItemId();
	FGameXXKMetaShopPurchaseResult Purchase;
	TestTrue(TEXT("outside shop sells one ten-coin bundle"), FGameXXKMetaShopRules::Purchase(State, EGameXXKMetaShopProductId::TravelMoneyBundle, Purchase));
	TestEqual(TEXT("bundle has fixed 100000 gold price"), State.PlayerGold, 100000);
	TestEqual(TEXT("bundle grants ten stackable items"), State.Inventory.FindRef(Money), 10);
	FString Error;
	TestTrue(TEXT("physical currency is valid in the save schema"), FGameXXKSaveMigration::ValidateRuntimeState(State, Error));
	State.Talents.NodeRanks.Add(TEXT("Talent.Root"), 1);
	State.Talents.NodeRanks.Add(TEXT("Talent.Entry.Tools"), 1);
	for (int32 Index = 1; Index <= 10; ++Index)
		State.Talents.NodeRanks.Add(FName(*FString::Printf(TEXT("Talent.Tools.Gold.%02d"), Index)), 1);
	FGameXXKToolInputRef Ref;
	Ref.ExpectedEntry = FGameXXKDesktopInventoryRules::MakeItemEntry(Money);
	Ref.SlotIndex = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Ref.Container, Ref.ExpectedEntry);
	Ref.Quantity = 4;
	FGameXXKEquipmentTransactionResult Dismantle;
	const auto BeforeConfirmation = State;
	TestFalse(TEXT("currency redemption requires confirmation"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref}, false, Dismantle));
	TestTrue(TEXT("unconfirmed redemption is atomic"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforeConfirmation, PPF_None));
	TestFalse(TEXT("duplicate source cannot redeem twice"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref, Ref}, true, Dismantle));
	TestTrue(TEXT("four displayed coins redeem"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref}, true, Dismantle));
	TestEqual(TEXT("gold talents cannot amplify fixed currency redemption"), State.PlayerGold, 110000);
	TestEqual(TEXT("partial redemption leaves remaining stack"), State.Inventory.FindRef(Money), 6);
	TestEqual(TEXT("currency cannot farm tool XP"), Dismantle.ToolExperienceDelta, int64(0));
	auto FullLoop = State;
	auto RestRef = Ref; RestRef.Quantity = 6;
	TestTrue(TEXT("remaining six coins can be redeemed"), FGameXXKEquipmentToolRules::Dismantle(FullLoop, {RestRef}, true, Dismantle));
	TestEqual(TEXT("buy then redeem loses exactly 75000 gold even with talents"), FullLoop.PlayerGold, 125000);
	TestEqual(TEXT("complete round trip awards no tool experience"), Dismantle.ToolExperienceDelta, int64(0));
	TestEqual(TEXT("round trip consumes all ten coins"), FullLoop.Inventory.FindRef(Money), 0);
	State.PlayerGold = MAX_int32;
	const auto BeforeOverflow = State;
	TestFalse(TEXT("gold overflow rejects redemption"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref}, true, Dismantle));
	TestTrue(TEXT("overflow does not consume coins"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforeOverflow, PPF_None));
	State.PlayerGold = 99999;
	const auto BeforePoor = State;
	TestFalse(TEXT("99999 gold cannot buy a bundle"), FGameXXKMetaShopRules::Purchase(State, EGameXXKMetaShopProductId::TravelMoneyBundle, Purchase));
	TestTrue(TEXT("failed purchase changes no state"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&State, &BeforePoor, PPF_None));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTravelMoneyChestPoolTest,
	"GameXXK.RouteTravelMoney.OrdinaryChestMaterialPool", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTravelMoneyChestPoolTest::RunTest(const FString& Parameters)
{
	auto* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
	if (!TestTrue(TEXT("chest fixture starts"), Subsystem->StartGame())) return false;
	const auto Base = Subsystem->GetRuntimeStateCopy();
	const FName Stage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	int32 Counts[5] = {};
	int32 MoneySeed = 0;
	for (int32 Seed = 1; Seed <= 3000; ++Seed)
	{
		auto State = Base;
		State.Training.ChallengeRewardSeed = Seed;
		FGameXXKTrainingRules::AppendChestToken(State.Training, EGameXXKTrainingRewardTier::NormalChest, Stage, 17);
		auto Replay = State;
		FGameXXKTrainingChestOpenResult Result, ReplayResult;
		if (!FGameXXKTrainingChestRules::OpenOne(State, EGameXXKTrainingRewardTier::NormalChest, Result))
		{
			AddError(FString::Printf(TEXT("ordinary chest seed %d failed"), Seed)); return false;
		}
		int32 Bucket = 0;
		if (!Result.ItemDeltas.IsEmpty())
		{
			for (const auto& Pair : Result.ItemDeltas)
			{
				EGameXXKGemType Type; EGameXXKGemQuality Quality;
				if (FGameXXKGemRules::TryParseItemId(Pair.Key, Type, Quality)) Bucket = 1;
				else if (Pair.Key == UGameXXKMVPRules::ItemEnhancementStone()) Bucket = 2;
				else if (Pair.Key == UGameXXKMVPRules::ItemRefinementSand()) Bucket = 3;
				else if (Pair.Key == FGameXXKTravelMoneyRules::ItemId())
				{
					Bucket = 4;
					TestEqual(TEXT("travel money always drops in tens"), Pair.Value, 10);
					if (!MoneySeed)
					{
						MoneySeed = Seed;
						TestTrue(TEXT("same saved chest repeats same outcome"), FGameXXKTrainingChestRules::OpenOne(Replay, EGameXXKTrainingRewardTier::NormalChest, ReplayResult));
						TestEqual(TEXT("replay currency count"), ReplayResult.ItemDeltas.FindRef(Pair.Key), 10);
					}
				}
				else { AddError(TEXT("unexpected ordinary chest item")); return false; }
			}
		}
		++Counts[Bucket];
	}
	const double Expected[] = {0.50, 0.30, 1.0/15.0, 1.0/15.0, 1.0/15.0};
	for (int32 Index = 0; Index < 5; ++Index)
	{
		const double Observed = static_cast<double>(Counts[Index]) / 3000.0;
		TestTrue(FString::Printf(TEXT("bucket %d probability %.4f follows approved pool %.4f"), Index, Observed, Expected[Index]),
			FMath::Abs(Observed - Expected[Index]) < 0.014);
	}
	for (int32 Seed = 1; Seed <= 256; ++Seed)
	{
		auto State = Base; State.Training.ChallengeRewardSeed = Seed;
		FGameXXKTrainingRules::AppendChestToken(State.Training, EGameXXKTrainingRewardTier::AdvancedChest, Stage, 17);
		FGameXXKTrainingChestOpenResult Result;
		TestTrue(TEXT("advanced chest still opens"), FGameXXKTrainingChestRules::OpenOne(State, EGameXXKTrainingRewardTier::AdvancedChest, Result));
		TestEqual(TEXT("new money source is ordinary chest only"), Result.ItemDeltas.FindRef(FGameXXKTravelMoneyRules::ItemId()), 0);
	}
	auto Overflow = Base; Overflow.Training.ChallengeRewardSeed = MoneySeed;
	FGameXXKTrainingRules::AppendChestToken(Overflow.Training, EGameXXKTrainingRewardTier::NormalChest, Stage, 17);
	Overflow.Inventory.Add(FGameXXKTravelMoneyRules::ItemId(), MAX_int32);
	FGameXXKDesktopInventoryRules::Normalize(Overflow);
	const auto Before = Overflow;
	FGameXXKTrainingChestOpenResult Result;
	TestFalse(TEXT("currency stack overflow keeps unopened chest"), FGameXXKTrainingChestRules::OpenOne(Overflow, EGameXXKTrainingRewardTier::NormalChest, Result));
	TestTrue(TEXT("overflow does not reroll or consume the token"), FGameXXKRuntimeState::StaticStruct()->CompareScriptStruct(&Overflow, &Before, PPF_None));
	return true;
}
#endif
