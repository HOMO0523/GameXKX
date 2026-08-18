#include "GameXXKTrainingRules.h"
#include "GameXXKEquipmentRules.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingLayout.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"

#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Misc/AutomationTest.h"
#include "Widgets/SNullWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest,
	"GameXXK.DesktopTraining.Workbench.SlateBuildContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchSlateBuildContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the Slate build contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	const TSharedRef<SWidget> SlateWidget = Widget->TakeWidget();
	TestNotNull(TEXT("workbench creates a WidgetTree root before Slate paints"), Widget->WidgetTree ? Widget->WidgetTree->RootWidget.Get() : nullptr);
	TestTrue(TEXT("workbench TakeWidget is not the null Slate placeholder"), SlateWidget != SNullWidget::NullWidget);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest,
	"GameXXK.DesktopTraining.Workbench.NativeConstructDoesNotRebuildSlateTree",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchNativeConstructDoesNotRebuildSlateTreeTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("workbench widget exists for the native construct lifecycle contract"), Widget);
	if (!Widget)
	{
		return false;
	}

	Widget->TakeWidget();
	Widget->ConstructForTest();
	TestEqual(TEXT("NativeConstruct leaves the Slate tree built by RebuildWidget intact"),
		Widget->GetProgrammaticLayoutBuildCountForTest(),
		1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest,
	"GameXXK.DesktopTraining.Workbench.MasterV2ResourceContract",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchMasterV2ResourceContractTest::RunTest(const FString& Parameters)
{
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("resource contract widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	const TArray<FString> ResourcePaths = Widget->GetMasterV2ResourcePathsForTest();

	int32 ApprovedResourceCount = 0;
	bool bHasPanelLarge = false;
	bool bHasItemSlot = false;
	bool bHasEquipmentSlot = false;
	int32 NavDiscCount = 0;
	for (const FString& Path : ResourcePaths)
	{
		if (Path.Contains(TEXT("/Game/GameXXK/UI/MasterV2/Approved/")))
		{
			++ApprovedResourceCount;
			bHasPanelLarge |= Path.Contains(TEXT("T_MasterV2_PanelLarge"));
			bHasItemSlot |= Path.Contains(TEXT("T_MasterV2_ItemSlot"));
			bHasEquipmentSlot |= Path.Contains(TEXT("T_MasterV2_EquipmentSlot"));
			NavDiscCount += Path.Contains(TEXT("T_MasterV2_NavDisc")) ? 1 : 0;
		}
	}
	TestTrue(TEXT("workbench uses approved MasterV2 brush resources"), ApprovedResourceCount >= 3);
	TestTrue(TEXT("workbench uses the approved large panel texture"), bHasPanelLarge);
	TestTrue(TEXT("workbench uses the approved item slot texture"), bHasItemSlot);
	TestTrue(TEXT("workbench uses the approved equipment slot texture"), bHasEquipmentSlot);
	TestEqual(TEXT("workbench exposes all five approved circular navigation icons"), NavDiscCount, 5);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingReferenceGeometryTest,
	"GameXXK.DesktopTraining.Workbench.ReferenceGeometry",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingReferenceGeometryTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKDesktopTrainingLayout;
	TestEqual(TEXT("reference canvas is the approved UI Master size"), GetReferenceCanvasSize(), FVector2D(1672.0f, 941.0f));
	TestEqual(TEXT("warehouse matches the selected layout"), GetWarehouseRect(), FVector4(10.0f, 17.0f, 363.0f, 908.0f));
	TestEqual(TEXT("center shell matches the selected layout"), GetCenterShellRect(), FVector4(386.0f, 17.0f, 970.0f, 908.0f));
	TestEqual(TEXT("right shell matches the selected layout"), GetRightShellRect(), FVector4(1369.0f, 17.0f, 291.0f, 908.0f));
	TestEqual(TEXT("idle strip matches the selected layout"), GetIdleStripRect(), FVector4(394.0f, 21.0f, 953.0f, 202.0f));
	TestEqual(TEXT("backpack surface matches the selected layout"), GetContentRect(), FVector4(397.0f, 244.0f, 945.0f, 533.0f));
	TestEqual(TEXT("navigation matches the selected layout"), GetNavigationRect(), FVector4(397.0f, 788.0f, 945.0f, 137.0f));

	const FFitTransform FullHD = MakeFitTransform(FVector2D(1920.0f, 1080.0f));
	const FFitTransform QHD = MakeFitTransform(FVector2D(2560.0f, 1440.0f));
	TestTrue(TEXT("Full HD uses one uniform scale"), FMath::IsNearlyEqual(FullHD.Scale, 1080.0f / 941.0f, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD uses one uniform scale"), FMath::IsNearlyEqual(QHD.Scale, 1440.0f / 941.0f, KINDA_SMALL_NUMBER));
	const FVector4 FullHDNode = FullHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	const FVector4 QHDNode = QHD.ApplyRect(FVector4(0.0f, 0.0f, 58.0f, 58.0f));
	TestTrue(TEXT("Full HD nodes remain circular"), FMath::IsNearlyEqual(FullHDNode.Z, FullHDNode.W, KINDA_SMALL_NUMBER));
	TestTrue(TEXT("QHD nodes remain circular"), FMath::IsNearlyEqual(QHDNode.Z, QHDNode.W, KINDA_SMALL_NUMBER));
	return true;
}

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
	TestTrue(TEXT("challenge refresh keeps the workbench shell visible"), Widget->IsWorkbenchVisibleForTest());
	TestTrue(TEXT("challenge keeps warehouse and map shells read-only"), Widget->AreChallengeSidePanelsReadOnlyForTest());
	TestTrue(TEXT("challenge viewport exposes active auto-battle control"), Widget->IsAutoBattleVisibleForTest());
	TestFalse(TEXT("challenge viewport does not expose travel retry control"), Widget->IsRetryVisibleForTest());
	const FVector4 ChallengeViewportRect = Widget->GetChallengeViewportRectForTest();
	const FVector4 ChallengeCombatStripRect = Widget->GetChallengeCombatStripRectForTest();
	const FVector4 ChallengeBattleBoardRect = Widget->GetChallengeBattleBoardRectForTest();
	TestTrue(TEXT("challenge uses the full continuous center canvas"),
		FMath::IsNearlyEqual(ChallengeViewportRect.Z, 960.0f)
		&& FMath::IsNearlyEqual(ChallengeViewportRect.W, 968.0f));
	TestEqual(TEXT("challenge combat strip reserves three enemy and three party slots"),
		Widget->GetChallengeCombatSlotCountForTest(), 6);
	TestTrue(TEXT("challenge combat strip precedes the battle board inside the center canvas"),
		ChallengeCombatStripRect.Y + ChallengeCombatStripRect.W <= ChallengeBattleBoardRect.Y
		&& ChallengeBattleBoardRect.Z >= 710.0f
		&& ChallengeBattleBoardRect.W >= 535.0f);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelTickDefersSlateRebuildTest,
	"GameXXK.DesktopTraining.Workbench.TravelTickDefersSlateRebuild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelTickDefersSlateRebuildTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel tick fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel tick fixture starts a cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel tick fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel tick fixture opens the workbench"), Widget->OpenWorkbench());

	// 1-1 uses the one-health travel exception, so two logical seconds are
	// enough to walk into and clear the first encounter.  The Slate widget tree
	// must not be rebuilt synchronously from NativeTick while Slate is iterating.
	for (int32 TickIndex = 0; TickIndex < 4; ++TickIndex)
	{
		Widget->TickForTest(1.0f);
	}
	TestTrue(TEXT("travel NativeTick defers a layout rebuild until after Slate tick"), Widget->HasPendingLayoutRefreshForTest());
	TestTrue(TEXT("deferred travel refresh keeps the workbench visible"), Widget->IsWorkbenchVisibleForTest());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualStrip",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualStripTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));

	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual fixture opens the workbench"), Widget->OpenWorkbench());
	TestTrue(TEXT("top strip creates a live travel visual surface"), Widget->HasTravelVisualStripForTest());
	TestTrue(TEXT("travel visual surface declares the generated walkloop atlas"),
		Widget->GetTravelVisualAtlasResourcePathForTest().Contains(TEXT("walkloop_pilot_v1")));
	TestTrue(TEXT("travel visual surface declares the seamless background"),
		Widget->GetTravelVisualBackgroundResourcePathForTest().Contains(TEXT("TrainingIdleStrip_Background")));

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip moves while the runner is walking"), Widget->GetTravelVisualScrollOffsetForTest() > 0.0f);
	TestEqual(TEXT("travel strip displays the generated 12 fps walkloop frame"), Widget->GetTravelVisualWalkFrameForTest(), 6);

	Widget->TickForTest(0.5f);
	TestTrue(TEXT("travel strip keeps the same visual runtime across deferred layout refresh"),
		Widget->GetTravelVisualScrollOffsetForTest() >= 0.0f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest,
	"GameXXK.DesktopTraining.Workbench.TravelVisualLoop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKDesktopTrainingWorkbenchTravelVisualLoopTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestNotNull(TEXT("travel visual loop fixture subsystem exists"), Subsystem);
	if (!Subsystem || !Subsystem->StartGame())
	{
		return false;
	}

	const FName StageId = FGameXXKTrainingRules::MakeStageId(EGameXXKTrainingDifficulty::Normal, 1);
	TestTrue(TEXT("travel visual loop fixture starts the cleared stage"), Subsystem->StartTrainingTravel(StageId));
	UGameXXKDesktopTrainingWorkbenchWidget* Widget = NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
	TestNotNull(TEXT("travel visual loop fixture widget exists"), Widget);
	if (!Widget)
	{
		return false;
	}
	Widget->SetMVPSubsystem(Subsystem);
	TestTrue(TEXT("travel visual loop fixture opens the workbench"), Widget->OpenWorkbench());

	// Normal 1-1 has nine travel encounters.  Each one uses two walking ticks
	// followed by one combat tick in the deterministic MVP runner.
	for (int32 TickIndex = 0; TickIndex < 27; ++TickIndex)
	{
		Widget->TickForTest(1.0f);
	}
	TestEqual(TEXT("one completed travel route increments the visual loop count"),
		Widget->GetTravelVisualCompletedLoopCountForTest(), 1);
	TestEqual(TEXT("the travel runner returns to the same 1-1 stage after its loop"),
		Subsystem->GetTrainingProgressCopy().CurrentTravelStageId, StageId);
	TestTrue(TEXT("the next encounter is walking after the visual loop reset"),
		Subsystem->GetTrainingTravelRuntimeCopy().Phase == EGameXXKTrainingTravelPhase::Walking);
	return true;
}

#endif
