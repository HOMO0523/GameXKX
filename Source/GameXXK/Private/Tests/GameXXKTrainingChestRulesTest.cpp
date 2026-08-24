#include "Misc/AutomationTest.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKGemRules.h"
#include "GameXXKTrainingChestRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveMigration.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState MakeChestRuntime(FAutomationTestBase& Test)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		Test.TestTrue(TEXT("chest fixture starts"), Subsystem && Subsystem->StartGame());
		return Subsystem ? Subsystem->GetRuntimeStateCopy() : FGameXXKRuntimeState();
	}

	bool AppendToken(FAutomationTestBase& Test, FGameXXKRuntimeState& State, EGameXXKTrainingRewardTier Tier, int32 Level = 17)
	{
		FString Error;
		return Test.TestTrue(TEXT("chest token appends"), FGameXXKTrainingRules::AppendChestToken(
			State.Training,
			Tier,
			FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1),
			Level,
			&Error));
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChestWalletAndOpenTest,
	"GameXXK.Training.Chests.WalletOpenAndCapacity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChestWalletAndOpenTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState State = MakeChestRuntime(*this);
	AppendToken(*this, State, EGameXXKTrainingRewardTier::NormalChest);
	AppendToken(*this, State, EGameXXKTrainingRewardTier::NormalChest);
	AppendToken(*this, State, EGameXXKTrainingRewardTier::AdvancedChest);
	TestEqual(TEXT("normal token count"), FGameXXKTrainingRules::CountChestTokens(State.Training, EGameXXKTrainingRewardTier::NormalChest), 2);
	TestEqual(TEXT("advanced token count"), FGameXXKTrainingRules::CountChestTokens(State.Training, EGameXXKTrainingRewardTier::AdvancedChest), 1);
	FGameXXKTrainingChestOpenResult Result;
	TestTrue(TEXT("open all normal succeeds"), FGameXXKTrainingChestRules::OpenAll(State, EGameXXKTrainingRewardTier::NormalChest, Result));
	TestEqual(TEXT("open all opens only requested tier"), Result.OpenedCount, 2);
	TestEqual(TEXT("normal tokens consumed"), FGameXXKTrainingRules::CountChestTokens(State.Training, EGameXXKTrainingRewardTier::NormalChest), 0);
	TestEqual(TEXT("advanced token retained"), FGameXXKTrainingRules::CountChestTokens(State.Training, EGameXXKTrainingRewardTier::AdvancedChest), 1);
	TestEqual(TEXT("legacy normal chest never enters inventory"), State.Inventory.FindRef(UGameXXKMVPRules::ItemTrainingNormalChest()), 0);

	FGameXXKRuntimeState Full = MakeChestRuntime(*this);
	AppendToken(*this, Full, EGameXXKTrainingRewardTier::NormalChest);
	Full.Inventory.Reset();
	Full.DesktopInventory.WarehouseItems.Reset();
	Full.DesktopInventory.BackpackSlots.Init(FGameXXKDesktopInventoryRules::MakeItemEntry(UGameXXKMVPRules::ItemEnhancementStone()), FGameXXKDesktopInventoryRules::BackpackCapacity);
	const int32 OpenOrdinalBefore = Full.Training.NextChestOpenOrdinal;
	TestFalse(TEXT("full backpack rejects before consuming"), FGameXXKTrainingChestRules::OpenOne(Full, EGameXXKTrainingRewardTier::NormalChest, Result));
	TestEqual(TEXT("full backpack error"), Result.Error, EGameXXKTrainingChestOpenError::BackpackFull);
	TestEqual(TEXT("blocked token remains"), FGameXXKTrainingRules::CountChestTokens(Full.Training, EGameXXKTrainingRewardTier::NormalChest), 1);
	TestEqual(TEXT("blocked outcome keeps open ordinal"), Full.Training.NextChestOpenOrdinal, OpenOrdinalBefore);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChestLootMatrixTest,
	"GameXXK.Training.Chests.DeterministicLootBranches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChestLootMatrixTest::RunTest(const FString& Parameters)
{
	bool bSawEquipment = false;
	bool bSawItem = false;
	for (int32 Seed = 1; Seed <= 128 && (!bSawEquipment || !bSawItem); ++Seed)
	{
		FGameXXKRuntimeState State = MakeChestRuntime(*this);
		State.Training.ChallengeRewardSeed = Seed;
		AppendToken(*this, State, EGameXXKTrainingRewardTier::AdvancedChest, 37);
		FGameXXKTrainingChestOpenResult Result;
		if (!FGameXXKTrainingChestRules::OpenOne(State, EGameXXKTrainingRewardTier::AdvancedChest, Result)) continue;
		if (!Result.EquipmentInstanceIds.IsEmpty())
		{
			bSawEquipment = true;
			const FGameXXKEquipmentInstance* Equipment = FGameXXKEquipmentRules::FindInstance(
				State.EquipmentCollection, Result.EquipmentInstanceIds[0]);
			TestNotNull(TEXT("advanced chest equipment exists"), Equipment);
			if (Equipment)
			{
				TestEqual(TEXT("advanced chest equipment is Rare"), Equipment->Quality, EGameXXKEquipmentQuality::Rare);
				TestEqual(TEXT("chest preserves source item level"), Equipment->ItemLevel, 37);
			}
		}
		if (!Result.ItemDeltas.IsEmpty())
		{
			bSawItem = true;
			for (const TPair<FName, int32>& Pair : Result.ItemDeltas)
			{
				EGameXXKGemType Type; EGameXXKGemQuality Quality;
				if (FGameXXKGemRules::TryParseItemId(Pair.Key, Type, Quality))
					TestEqual(TEXT("advanced chest gem is Rare"), Quality, EGameXXKGemQuality::Rare);
				else
					TestEqual(TEXT("advanced chest material quantity is three"), Pair.Value, 3);
			}
		}
	}
	TestTrue(TEXT("deterministic seed sweep reaches equipment branch"), bSawEquipment);
	TestTrue(TEXT("deterministic seed sweep reaches item branch"), bSawItem);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTrainingChestLegacyMigrationTest,
	"GameXXK.Training.Chests.LegacyStackMigration",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTrainingChestLegacyMigrationTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState Runtime = MakeChestRuntime(*this);
	Runtime.Inventory.Add(UGameXXKMVPRules::ItemTrainingNormalChest(), 2);
	Runtime.DesktopInventory.WarehouseItems.Add(UGameXXKMVPRules::ItemTrainingAdvancedChest(), 1);
	FString Error;
	TestTrue(TEXT("legacy chest fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(Runtime, &Error));
	FGameXXKSaveState Source = UGameXXKMVPRules::MakeSaveState(Runtime);
	FGameXXKSaveState Migrated;
	FGameXXKSaveMigrationReport Report;
	TestTrue(TEXT("legacy chest stacks migrate at v25"), FGameXXKSaveMigration::MigrateToCurrent(Source, Migrated, Report));
	TestEqual(TEXT("two normal tokens created"), FGameXXKTrainingRules::CountChestTokens(Migrated.RuntimeState.Training, EGameXXKTrainingRewardTier::NormalChest), 2);
	TestEqual(TEXT("one advanced token created"), FGameXXKTrainingRules::CountChestTokens(Migrated.RuntimeState.Training, EGameXXKTrainingRewardTier::AdvancedChest), 1);
	TestEqual(TEXT("normal legacy stack removed"), Migrated.RuntimeState.Inventory.FindRef(UGameXXKMVPRules::ItemTrainingNormalChest()), 0);
	TestEqual(TEXT("advanced warehouse stack removed"), Migrated.RuntimeState.DesktopInventory.WarehouseItems.FindRef(UGameXXKMVPRules::ItemTrainingAdvancedChest()), 0);
	return true;
}

#endif
