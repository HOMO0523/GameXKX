#include "GameXXKTrainingRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Engine/GameInstance.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchLayoutContractTest,
	"GameXXK.DesktopTraining.Workbench.LayoutContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchLayoutContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget can be constructed without a live viewport"), Widget);
	if (!Widget)
	{
		return false;
	}
	TestEqual(TEXT("warehouse uses four columns"), Widget->GetWarehouseColumnCountForTest(), 4);
	const FVector2D BackpackRatio = Widget->GetBackpackAspectRatioForTest();
	TestTrue(TEXT("backpack aspect ratio keeps the real wide proportion"), FMath::IsNearlyEqual(BackpackRatio.X / BackpackRatio.Y, 1.76f, 0.001f));

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("workbench read model fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}
	FGameXXKRuntimeState& State = Subsystem->GetMutableRuntimeState();
	State.PlayerGold = 4242;
	State.Inventory.Empty();
	State.Inventory.Add(UGameXXKMVPRules::ItemHealingPowder(), 3);
	State.Inventory.Add(UGameXXKMVPRules::ItemTrainingNormalChest(), 2);
	for (int32 ExtraIndex = 0; ExtraIndex < 15; ++ExtraIndex)
	{
		FGameXXKEquipmentCreateRequest Request;
		Request.Set = EGameXXKEquipmentSet::Starter;
		Request.Quality = EGameXXKEquipmentQuality::Common;
		Request.ItemLevel = 1 + ExtraIndex;
		Request.bForceSlot = true;
		Request.ForcedSlot = EGameXXKEquipmentSlot::Weapon;
		FName InstanceId;
		FString Error;
		TestTrue(TEXT("warehouse pagination fixture creates an equipment instance"),
			FGameXXKEquipmentRules::CreateRolledInstance(State.EquipmentCollection, Request, InstanceId, &Error));
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("Tab/backpack entry opens the formation-backed backpack view"), Widget->OpenBackpack());
	TestEqual(TEXT("backpack defaults to the hero character"),
		Widget->GetActiveBackpackCharacterIdForTest(),
		FGameXXKEquipmentRules::HeroCharacterId());
	const TArray<FName> BackpackCharacterIds = Widget->GetBackpackCharacterIdsForTest();
	TestEqual(TEXT("backpack exposes the hero and both starter companions"), BackpackCharacterIds.Num(), 3);
	TestTrue(TEXT("backpack character list keeps the hero first"),
		BackpackCharacterIds.Num() > 0
		&& BackpackCharacterIds[0] == FGameXXKEquipmentRules::HeroCharacterId());
	if (BackpackCharacterIds.Num() > 1)
	{
		TestTrue(TEXT("backpack can switch to a permanent companion"), Widget->SelectBackpackCharacterForTest(BackpackCharacterIds[1]));
		TestEqual(TEXT("selected companion becomes the backpack read-model owner"),
			Widget->GetActiveBackpackCharacterIdForTest(),
			BackpackCharacterIds[1]);
	}
	TestFalse(TEXT("backpack rejects an unknown character"), Widget->SelectBackpackCharacterForTest(FName(TEXT("Character.Unknown"))));
	Widget->HandleActionClicked(3);
	TestEqual(TEXT("tools navigation replaces the right-side map"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Tools);
	TestTrue(TEXT("tools panel is active outside challenge viewport"), Widget->IsToolsPanelActiveForTest());
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("training navigation returns to the map shell"), Widget->GetActiveNavForTest(), EGameXXKDesktopTrainingNav::Training);
	TestFalse(TEXT("training navigation is not the tools panel"), Widget->IsToolsPanelActiveForTest());
	TestEqual(TEXT("warehouse exposes two pages at twenty slots per page"), Widget->GetWarehousePageCountForTest(), 2);
	TestEqual(TEXT("warehouse starts on its first page"), Widget->GetWarehousePageIndexForTest(), 0);
	TestEqual(TEXT("warehouse first page exposes twenty visible instances"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 20);
	TestTrue(TEXT("warehouse advances to the second page"), Widget->NextWarehousePageForTest());
	TestEqual(TEXT("warehouse page index advances without mutating the save"), Widget->GetWarehousePageIndexForTest(), 1);
	TestEqual(TEXT("warehouse second page exposes the remaining instance"), Widget->GetVisibleWarehouseInstanceIdsForTest().Num(), 1);
	TestTrue(TEXT("warehouse returns to the first page"), Widget->PreviousWarehousePageForTest());
	TestEqual(TEXT("warehouse page index clamps at zero"), Widget->GetWarehousePageIndexForTest(), 0);
	const TArray<FName> WarehouseBeforeSort = Widget->GetVisibleWarehouseInstanceIdsForTest();
	TestTrue(TEXT("warehouse sort action succeeds"), Widget->SortWarehouseForTest());
	const TArray<FName> WarehouseAfterSort = Widget->GetVisibleWarehouseInstanceIdsForTest();
	TestEqual(TEXT("warehouse sort preserves the visible item count"), WarehouseAfterSort.Num(), WarehouseBeforeSort.Num());
	TestFalse(TEXT("warehouse sort changes the ascending fixture order"), WarehouseAfterSort == WarehouseBeforeSort);
	TestTrue(TEXT("warehouse sort is idempotent"), Widget->SortWarehouseForTest());
	TestEqual(TEXT("warehouse sort keeps a stable deterministic order"), Widget->GetVisibleWarehouseInstanceIdsForTest(), WarehouseAfterSort);
	const int32 WarehouseBeforeEquip = State.EquipmentCollection.WarehouseInstanceIds.Num();
	TestTrue(TEXT("warehouse slot can quick-equip into the selected backpack character"), Widget->QuickEquipVisibleWarehouseSlotForTest(0));
	TestEqual(TEXT("quick-equip removes the moved instance from warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Num(), WarehouseBeforeEquip - 1);
	const int32 WarehouseBeforeUnequip = State.EquipmentCollection.WarehouseInstanceIds.Num();
	TestTrue(TEXT("backpack can quick-unequip the active weapon slot"), Widget->QuickUnequipActiveBackpackSlotForTest(0));
	TestEqual(TEXT("quick-unequip returns the item to the warehouse"), State.EquipmentCollection.WarehouseInstanceIds.Num(), WarehouseBeforeUnequip + 1);
	TestEqual(TEXT("workbench reads the authoritative runtime gold"), Widget->GetRuntimeGoldForTest(), 4242);
	const FName TravelStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel fixture starts the default cleared stage"), Subsystem->StartTrainingTravel(TravelStage));
	FGameXXKTrainingOfflineReward SimulatedTravelReward;
	TestTrue(TEXT("travel fixture creates a pending offline reward"),
		Subsystem->SimulateTrainingTravelOffline(64, SimulatedTravelReward));
	TestTrue(TEXT("workbench exposes pending travel gold for collection"), Widget->GetPendingTravelGoldForTest() > 0);
	TestEqual(TEXT("workbench exposes pending normal travel chests"),
		Widget->GetPendingTravelNormalChestCountForTest(), SimulatedTravelReward.NormalChestCount);
	TestEqual(TEXT("workbench exposes pending advanced travel chests"),
		Widget->GetPendingTravelAdvancedChestCountForTest(), SimulatedTravelReward.AdvancedChestCount);
	const int32 GoldBeforeCollect = Widget->GetRuntimeGoldForTest();
	TestTrue(TEXT("workbench collect action deposits pending travel rewards"), Widget->CollectTravelRewardsForTest());
	TestEqual(TEXT("collect action deposits pending travel gold"),
		Widget->GetRuntimeGoldForTest(), GoldBeforeCollect + SimulatedTravelReward.Gold);
	TestEqual(TEXT("collect action clears pending travel gold"), Widget->GetPendingTravelGoldForTest(), 0);
	TestEqual(TEXT("workbench warehouse occupancy comes from the equipment collection"),
		Widget->GetWarehouseOccupancyForTest(),
		State.EquipmentCollection.WarehouseInstanceIds.Num());
	const TArray<FName> VisibleItems = Widget->GetVisibleBackpackItemIdsForTest();
	TestTrue(TEXT("workbench backpack read model includes healing powder"), VisibleItems.Contains(UGameXXKMVPRules::ItemHealingPowder()));
	TestTrue(TEXT("workbench backpack read model includes a travel chest"), VisibleItems.Contains(UGameXXKMVPRules::ItemTrainingNormalChest()));
	TestEqual(TEXT("three difficulty bands each expose nine stage definitions"), FGameXXKTrainingRules::GetStageDefinitions().Num(), 27);
	TestEqual(TEXT("normal 1-1 id remains stable"), FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1), FName(TEXT("Training.Normal.1-1")));
	const FName ChallengeStage = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 2);
	TestTrue(TEXT("challenge fixture selects the first uncompleted stage"), Widget->SelectStageForTest(ChallengeStage));
	TestTrue(TEXT("challenge fixture enters the enlarged challenge viewport"), Widget->ClickChallengeForTest());
	TestTrue(TEXT("challenge keeps warehouse and map shells read-only"), Widget->AreChallengeSidePanelsReadOnlyForTest());
	TestTrue(TEXT("challenge viewport exposes active auto-battle control"), Widget->IsAutoBattleVisibleForTest());
	TestFalse(TEXT("challenge viewport does not expose travel retry control"), Widget->IsRetryVisibleForTest());
	const EGameXXKDesktopTrainingNav NavDuringChallenge = Widget->GetActiveNavForTest();
	Widget->HandleActionClicked(4);
	TestEqual(TEXT("challenge locks bottom navigation while side shells remain visible"), Widget->GetActiveNavForTest(), NavDuringChallenge);
	TestTrue(TEXT("backpack entry returns to the workbench after challenge shell assertion"), Widget->OpenBackpack());
	Widget->HandleActionClicked(14);
	TestTrue(TEXT("backpack settings action opens an independent settings surface"), Widget->IsSettingsPanelOpenForTest());
	TestTrue(TEXT("opening settings keeps the workbench visible"), Widget->IsWorkbenchVisibleForTest());
	Widget->HandleActionClicked(15);
	TestFalse(TEXT("close action closes the workbench independently of settings"), Widget->IsWorkbenchVisibleForTest());
	TestFalse(TEXT("closing the workbench clears the settings surface"), Widget->IsSettingsPanelOpenForTest());
	return true;
}

#endif
