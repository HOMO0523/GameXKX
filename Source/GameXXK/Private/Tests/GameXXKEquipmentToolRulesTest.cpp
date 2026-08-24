#include "Misc/AutomationTest.h"

#include "GameXXKDesktopInventoryRules.h"
#include "GameXXKEquipmentToolRules.h"
#include "GameXXKGemRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "MVP/GameXXKSaveGame.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FGameXXKRuntimeState MakeRuntime(FAutomationTestBase& Test)
	{
		UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		Test.TestTrue(TEXT("tool fixture starts"), Subsystem && Subsystem->StartGame());
		return Subsystem ? Subsystem->GetRuntimeStateCopy() : FGameXXKRuntimeState();
	}

	FName CreateEquipment(
		FAutomationTestBase& Test,
		FGameXXKRuntimeState& State,
		const EGameXXKEquipmentQuality Quality,
		const int32 Level = 10)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::PoJun;
		Request.Quality = Quality;
		Request.ItemLevel = Level;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Accessory;
		FName InstanceId;
		FString Error;
		Test.TestTrue(TEXT("tool fixture equipment creates"), FGameXXKEquipmentRules::CreateRolledInstance(
			State.EquipmentCollection, Request, InstanceId, &Error));
		Test.TestTrue(TEXT("tool fixture desktop normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
		return InstanceId;
	}

	FGameXXKToolInputRef RefFor(
		const FGameXXKRuntimeState& State,
		const FGameXXKDesktopInventoryEntryKey& Entry)
	{
		FGameXXKToolInputRef Result;
		for (const EGameXXKDesktopItemContainer Container : {
			EGameXXKDesktopItemContainer::Backpack,
			EGameXXKDesktopItemContainer::Warehouse})
		{
			const int32 Slot = FGameXXKDesktopInventoryRules::FindEntrySlot(State, Container, Entry);
			if (Slot != INDEX_NONE)
			{
				Result = {Container, Slot, Entry};
				break;
			}
		}
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKToolProgressionTest,
	"GameXXK.Equipment.Tools.ProgressionTables",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKToolProgressionTest::RunTest(const FString& Parameters)
{
	const int64 Multipliers[] = {1, 9, 81, 729, 6561, 59049, 531441, 4782969, 43046721, 387420489};
	for (int32 Rank = 1; Rank <= 10; ++Rank)
	{
		TestEqual(TEXT("quality XP multiplier"), FGameXXKEquipmentToolRules::GetQualityExperienceMultiplier(Rank), Multipliers[Rank - 1]);
		const FInt32Interval Range = FGameXXKEquipmentToolRules::GetCraftedItemLevelRange(Rank);
		TestEqual(TEXT("craft range minimum"), Range.Min, Rank == 1 ? 1 : (Rank - 1) * 10);
		TestEqual(TEXT("craft range maximum"), Range.Max, Rank * 10);
	}
	FGameXXKToolProgress Progress;
	TestTrue(TEXT("99 XP stays level one"), FGameXXKEquipmentToolRules::AddRawExperience(Progress, 99));
	TestEqual(TEXT("level one residual"), Progress.Experience, int64(99));
	TestTrue(TEXT("threshold advances with residual"), FGameXXKEquipmentToolRules::AddRawExperience(Progress, 2));
	TestEqual(TEXT("advanced level"), Progress.Level, 2);
	TestEqual(TEXT("advanced residual"), Progress.Experience, int64(1));
	TestTrue(TEXT("large award reaches cap"), FGameXXKEquipmentToolRules::AddRawExperience(Progress, 100000));
	TestEqual(TEXT("tool level caps at ten"), Progress.Level, 10);
	TestEqual(TEXT("capped experience clears"), Progress.Experience, int64(0));
	Progress.SelectedCraftingLevel = 99;
	TestTrue(TEXT("selected level clamps"), FGameXXKEquipmentToolRules::NormalizeProgress(Progress));
	TestEqual(TEXT("selected level clamps to unlocked cap"), Progress.SelectedCraftingLevel, 10);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKToolProgressSaveRoundTripTest,
	"GameXXK.Equipment.Tools.ProgressSaveRoundTrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKToolProgressSaveRoundTripTest::RunTest(const FString& Parameters)
{
	FGameXXKRuntimeState Runtime = MakeRuntime(*this);
	Runtime.ToolProgress.Level = 7;
	Runtime.ToolProgress.Experience = 123;
	Runtime.ToolProgress.SelectedCraftingLevel = 6;
	UGameXXKSaveGame* SaveObject = NewObject<UGameXXKSaveGame>();
	SaveObject->SaveState = UGameXXKMVPRules::MakeSaveState(Runtime);
	TArray<uint8> Bytes;
	TestTrue(TEXT("tool progress serializes through SaveGameToMemory"), UGameplayStatics::SaveGameToMemory(SaveObject, Bytes));
	UGameXXKSaveGame* Reloaded = Cast<UGameXXKSaveGame>(UGameplayStatics::LoadGameFromMemory(Bytes));
	if (!TestNotNull(TEXT("tool progress SaveGame reloads"), Reloaded)) return false;
	TestEqual(TEXT("tool level round-trips"), Reloaded->SaveState.RuntimeState.ToolProgress.Level, 7);
	TestEqual(TEXT("tool residual XP round-trips"), Reloaded->SaveState.RuntimeState.ToolProgress.Experience, int64(123));
	TestEqual(TEXT("selected crafting level round-trips"), Reloaded->SaveState.RuntimeState.ToolProgress.SelectedCraftingLevel, 6);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKToolTransactionsTest,
	"GameXXK.Equipment.Tools.AtomicTransactions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKToolTransactionsTest::RunTest(const FString& Parameters)
{
	{
		FGameXXKRuntimeState State = MakeRuntime(*this);
		const FName Id = CreateEquipment(*this, State, EGameXXKEquipmentQuality::Common);
		const FGameXXKDesktopInventoryEntryKey Entry = FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id);
		const FGameXXKToolInputRef Ref = RefFor(State, Entry);
		FString Error;
		TestTrue(TEXT("dismantle fixture lock sets"), FGameXXKDesktopInventoryRules::SetEntryLocked(State, Entry, true, &Error));
		FGameXXKEquipmentTransactionResult Result;
		TestFalse(TEXT("locked equipment cannot dismantle"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref}, true, Result));
		TestEqual(TEXT("locked dismantle error"), Result.Error, EGameXXKEquipmentTransactionError::InputLocked);
		TestTrue(TEXT("dismantle fixture unlocks"), FGameXXKDesktopInventoryRules::SetEntryLocked(State, Entry, false, &Error));
		const int32 GoldBefore = State.PlayerGold;
		TestTrue(TEXT("one equipment dismantles"), FGameXXKEquipmentToolRules::Dismantle(State, {Ref}, true, Result));
		TestEqual(TEXT("dismantle gold"), State.PlayerGold, GoldBefore + 10);
		TestEqual(TEXT("dismantle quality XP"), Result.ToolExperienceDelta, int64(1));
	}
	{
		FGameXXKRuntimeState State = MakeRuntime(*this);
		TArray<FGameXXKToolInputRef> Inputs;
		for (int32 Index = 0; Index < 9; ++Index)
		{
			const FName Id = CreateEquipment(*this, State, EGameXXKEquipmentQuality::Common, 1 + Index);
			Inputs.Add(RefFor(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id)));
		}
		FGameXXKEquipmentTransactionResult Result;
		TestTrue(TEXT("nine equipment combine"), FGameXXKEquipmentToolRules::CombineEquipment(State, Inputs, Result));
		const FGameXXKEquipmentInstance* Output = FGameXXKEquipmentRules::FindInstance(State.EquipmentCollection, Result.OutputEntryId);
		TestNotNull(TEXT("combined equipment output exists"), Output);
		if (Output)
		{
			TestEqual(TEXT("combined equipment advances quality"), Output->Quality, EGameXXKEquipmentQuality::Rare);
			TestTrue(TEXT("selected-level output is in range"), Output->ItemLevel >= 1 && Output->ItemLevel <= 10);
		}
		TestEqual(TEXT("equipment combine awards 9x quality XP"), Result.ToolExperienceDelta, int64(9));
	}
	{
		FGameXXKRuntimeState State = MakeRuntime(*this);
		const FName CommonAttack = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Common);
		State.Inventory.Add(CommonAttack, 9);
		FString Error;
		TestTrue(TEXT("gem combine fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
		const FGameXXKToolInputRef Input = RefFor(State, FGameXXKDesktopInventoryRules::MakeItemEntry(CommonAttack));
		FGameXXKEquipmentTransactionResult Result;
		TestTrue(TEXT("nine gems combine"), FGameXXKEquipmentToolRules::CombineGem(State, Input, Result));
		TestEqual(TEXT("common gem stack consumed"), State.Inventory.FindRef(CommonAttack), 0);
		TestEqual(TEXT("rare gem produced"), State.Inventory.FindRef(FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Rare)), 1);
		TestEqual(TEXT("gem combine XP"), Result.ToolExperienceDelta, int64(9));
	}
	{
		FGameXXKRuntimeState State = MakeRuntime(*this);
		const FName Id = CreateEquipment(*this, State, EGameXXKEquipmentQuality::Common);
		const FGameXXKToolInputRef EquipmentRef = RefFor(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id));
		State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemEnhancementStone()) = 20;
		State.Inventory.FindOrAdd(UGameXXKMVPRules::ItemRefinementSand()) = 20;
		FString Error;
		TestTrue(TEXT("enhance fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
		FGameXXKEquipmentTransactionResult Result;
		TestTrue(TEXT("manual enhance works"), FGameXXKEquipmentToolRules::Enhance(State, EquipmentRef, Result));
		TestEqual(TEXT("enhance XP once"), Result.ToolExperienceDelta, int64(1));
		const FGameXXKToolInputRef ReforgeRef = RefFor(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id));
		const int64 ExperienceBefore = State.ToolProgress.Experience;
		TestTrue(TEXT("reforge preview works"), FGameXXKEquipmentToolRules::BeginReforge(State, ReforgeRef, 0, Result));
		TestTrue(TEXT("reforge marks XP awarded"), State.EquipmentCollection.PendingReforge.bToolExperienceAwarded);
		TestTrue(TEXT("reforge keep works"), FGameXXKEquipmentToolRules::ResolveReforge(State, false, Result));
		TestEqual(TEXT("reforge resolution does not award twice"), State.ToolProgress.Experience, ExperienceBefore + 1);
	}
	{
		FGameXXKRuntimeState State = MakeRuntime(*this);
		const FName Id = CreateEquipment(*this, State, EGameXXKEquipmentQuality::Common);
		FGameXXKEquipmentInstance* Equipment = State.EquipmentCollection.EquipmentInstances.FindByPredicate(
			[Id](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == Id; });
		Equipment->SocketedGems[0] = {EGameXXKGemType::Defense, EGameXXKGemQuality::Common};
		const FName AttackGem = FGameXXKGemRules::MakeItemId(EGameXXKGemType::Attack, EGameXXKGemQuality::Common);
		State.Inventory.Add(AttackGem, 1);
		FString Error;
		TestTrue(TEXT("socket fixture normalizes"), FGameXXKDesktopInventoryRules::Normalize(State, &Error));
		FGameXXKSocketGemRequest Request;
		Request.EquipmentInput = RefFor(State, FGameXXKDesktopInventoryRules::MakeEquipmentEntry(Id));
		Request.GemInput = RefFor(State, FGameXXKDesktopInventoryRules::MakeItemEntry(AttackGem));
		Request.SocketIndex = 0;
		FGameXXKEquipmentTransactionResult SocketResult;
		TestTrue(TEXT("manual gem replacement works"), FGameXXKEquipmentToolRules::SocketGem(State, Request, SocketResult));
		Equipment = State.EquipmentCollection.EquipmentInstances.FindByPredicate(
			[Id](const FGameXXKEquipmentInstance& Instance) { return Instance.InstanceId == Id; });
		TestEqual(TEXT("new gem enters socket"), Equipment->SocketedGems[0].Type, EGameXXKGemType::Attack);
		TestEqual(TEXT("old gem returns to backpack"), State.Inventory.FindRef(FGameXXKGemRules::MakeItemId(EGameXXKGemType::Defense, EGameXXKGemQuality::Common)), 1);
	}
	return true;
}

#endif
