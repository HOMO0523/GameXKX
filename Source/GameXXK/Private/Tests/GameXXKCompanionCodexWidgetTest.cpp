#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/ScrollBox.h"
#include "Components/SizeBox.h"
#include "Components/UniformGridPanel.h"
#include "Engine/GameInstance.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKTownHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString GetButtonNormalResourcePath(const UButton* Button)
	{
		const UObject* Resource = Button ? Button->GetStyle().Normal.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}

	FString GetBorderResourcePath(const UBorder* Border)
	{
		const UObject* Resource = Border ? Border->Background.GetResourceObject() : nullptr;
		return Resource ? Resource->GetPathName() : FString();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKCompanionCodexWidgetTest,
	"GameXXK.MVP.UI.CompanionCodex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKCompanionCodexWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKTownHudWidget* TownHud = NewObject<UGameXXKTownHudWidget>();

	TestTrue(TEXT("new game starts through the real subsystem"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan enters the real town state"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("accepting the town quest discovers the Guide entry"), Subsystem->AcceptQuest());

	TownHud->SetMVPSubsystem(Subsystem);
	TownHud->TakeWidget();
	TownHud->RefreshFromState();

	UButton* CompanionButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCompanion"))) : nullptr;
	TestNotNull(TEXT("town HUD preserves its companion navigation button"), CompanionButton);
	if (CompanionButton)
	{
		CompanionButton->OnClicked.Broadcast();
	}

	TestFalse(TEXT("page-02 companion navigation keeps the codex overlay closed"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("test helper opens the local codex overlay"), TownHud->OpenCompanionCodexForTest());
	TownHud->RefreshFromState();
	TestTrue(TEXT("refreshing while the runtime state remains in Town preserves the open codex overlay"), TownHud->IsCompanionCodexOpenForTest());
	TestNotNull(TEXT("codex uses a real named scroll box"), TownHud->WidgetTree ? Cast<UScrollBox>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexScroll"))) : nullptr);
	UUniformGridPanel* CodexGrid = TownHud->WidgetTree ? Cast<UUniformGridPanel>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexGrid"))) : nullptr;
	TestNotNull(TEXT("codex uses a real named six-column PSD card grid"), CodexGrid);
	USizeBox* CodexCardSize = CodexGrid ? Cast<USizeBox>(CodexGrid->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("codex uses a real fixed first card frame"), CodexCardSize);
	if (CodexCardSize)
	{
		TestFalse(TEXT("dynamically rebuilt codex card widgets do not use a reused explicit TownHudCodexCardSize_ name"), CodexCardSize->GetFName().ToString().StartsWith(TEXT("TownHudCodexCardSize_")));
		TestEqual(TEXT("first codex card frame uses the approved PSD057 width"), static_cast<double>(CodexCardSize->GetWidthOverride()), 113.0);
		TestEqual(TEXT("first codex card frame uses the approved PSD057 height"), static_cast<double>(CodexCardSize->GetHeightOverride()), 129.0);
	}
	TestEqual(TEXT("codex opens on the aggregate category"), TownHud->GetActiveCodexCategoryForTest(), EGameXXKCodexCategory::All);
	TestEqual(TEXT("codex uses six fixed card columns"), TownHud->GetCodexColumnCountForTest(), 6);
	TestEqual(TEXT("codex card width follows the approved PSD057 draw size"), TownHud->GetCodexCardSizeForTest().X, 113.0);
	TestEqual(TEXT("codex card height follows the approved PSD057 draw size"), TownHud->GetCodexCardSizeForTest().Y, 129.0);
	UButton* TaskNpcCard = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudTaskNpcCodexCard_Npc_TusiChief"))) : nullptr;
	UButton* GenericCard = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudGenericCodexCard_Codex_Guide"))) : nullptr;
	TestNotNull(TEXT("task NPC list card is a real PSD057 button"), TaskNpcCard);
	TestNotNull(TEXT("generic codex list card is a real PSD057 button"), GenericCard);
	TestTrue(TEXT("task NPC list cards use the approved untouched PSD057 frame"),
		GetButtonNormalResourcePath(TaskNpcCard).Contains(TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057")));
	TestTrue(TEXT("generic codex list cards use the approved untouched PSD057 frame"),
		GetButtonNormalResourcePath(GenericCard).Contains(TEXT("/Game/GameXXK/UI/Cards/Textures/T_CardFrame_PSD057")));
	UBorder* CodexFrame = TownHud->WidgetTree ? Cast<UBorder>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexFrame"))) : nullptr;
	UBorder* CodexDetail = TownHud->WidgetTree ? Cast<UBorder>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexDetail"))) : nullptr;
	UBorder* TaskNpcDetailSlot = TownHud->WidgetTree ? Cast<UBorder>(TownHud->WidgetTree->FindWidget(TEXT("TownHudTaskNpcCodexDetailPortraitSlot"))) : nullptr;
	TestTrue(TEXT("codex outer frame uses the approved backpack window paper"),
		GetBorderResourcePath(CodexFrame).Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame")));
	TestTrue(TEXT("codex detail frame uses the approved backpack window paper"),
		GetBorderResourcePath(CodexDetail).Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_WindowFrame")));
	TestTrue(TEXT("task NPC detail portrait uses the approved backpack slot"),
		GetBorderResourcePath(TaskNpcDetailSlot).Contains(TEXT("/Game/GameXXK/UI/Town/Textures/Backpack/T_TownBackpack_Slot")));
	TestEqual(TEXT("aggregate collection summary reports the discovered Guide"), TownHud->GetCodexCollectionSummaryForTest().ToString(), FString(TEXT("已收录 1 / 22")));
	TestTrue(TEXT("aggregate list exposes the discovered Guide"), TownHud->GetVisibleCodexEntryIdsForTest().Contains(FName(TEXT("Codex.Guide"))));
	TestTrue(TEXT("unread discovered Guide raises the companion badge"), TownHud->HasCompanionUnreadNoticeForTest());

	TestTrue(TEXT("selecting the Guide uses the codex interaction path"), TownHud->SelectCodexEntryForTest(FName(TEXT("Codex.Guide"))));
	TestFalse(TEXT("reading the Guide clears the companion unread badge"), TownHud->HasCompanionUnreadNoticeForTest());
	TestTrue(TEXT("runtime codex discovery adds the Money Rat while the overlay is open"), UGameXXKMVPRules::DiscoverCodexEntry(Subsystem->GetMutableRuntimeState(), FName(TEXT("Codex.Enemy.Ch1.MoneyRat"))));
	TownHud->RefreshFromState();
	TestTrue(TEXT("refresh after runtime codex discovery keeps the overlay open"), TownHud->IsCompanionCodexOpenForTest());
	TestEqual(TEXT("refresh rebuilds the aggregate collection summary from runtime codex data"), TownHud->GetCodexCollectionSummaryForTest().ToString(), FString(TEXT("已收录 2 / 22")));
	TestTrue(TEXT("refresh rebuilds the aggregate list with the newly discovered Money Rat"), TownHud->GetVisibleCodexEntryIdsForTest().Contains(FName(TEXT("Codex.Enemy.Ch1.MoneyRat"))));
	USizeBox* RefreshedCodexCardSize = CodexGrid ? Cast<USizeBox>(CodexGrid->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("refresh rebuilds the first codex card frame in the grid"), RefreshedCodexCardSize);
	if (RefreshedCodexCardSize)
	{
		TestFalse(TEXT("refresh does not reuse an explicit codex card size widget name"), RefreshedCodexCardSize->GetFName().ToString().StartsWith(TEXT("TownHudCodexCardSize_")));
		TestEqual(TEXT("refresh retains the approved PSD057 card width"), static_cast<double>(RefreshedCodexCardSize->GetWidthOverride()), 113.0);
		TestEqual(TEXT("refresh retains the approved PSD057 card height"), static_cast<double>(RefreshedCodexCardSize->GetHeightOverride()), 129.0);
	}
	TestTrue(TEXT("Spirit is a valid empty category"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::Spirit));
	TestTrue(TEXT("Spirit category exposes its empty state"), TownHud->IsCodexEmptyStateVisibleForTest());
	TestEqual(TEXT("Spirit category explains that nothing is collected"), TownHud->GetCodexCollectionSummaryForTest().ToString(), FString(TEXT("尚未收录")));
	TestTrue(TEXT("aggregate category can be restored"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::All));
	TestTrue(TEXT("codex scroll offset can be updated"), TownHud->SetCodexScrollOffsetForTest(64.0f));
	TestEqual(TEXT("codex stores its requested scroll offset"), TownHud->GetCodexScrollOffsetForTest(), 64.0f);
	TestTrue(TEXT("Hero category can be selected"), TownHud->SelectCodexCategoryForTest(EGameXXKCodexCategory::Hero));
	TestEqual(TEXT("changing category resets the codex scroll offset"), TownHud->GetCodexScrollOffsetForTest(), 0.0f);
	USizeBox* CategoryCodexCardSize = CodexGrid ? Cast<USizeBox>(CodexGrid->GetChildAt(0)) : nullptr;
	TestNotNull(TEXT("category change rebuilds the first codex card frame in the grid"), CategoryCodexCardSize);
	if (CategoryCodexCardSize)
	{
		TestFalse(TEXT("category change does not reuse an explicit codex card size widget name"), CategoryCodexCardSize->GetFName().ToString().StartsWith(TEXT("TownHudCodexCardSize_")));
		TestEqual(TEXT("category change retains the approved PSD057 card width"), static_cast<double>(CategoryCodexCardSize->GetWidthOverride()), 113.0);
		TestEqual(TEXT("category change retains the approved PSD057 card height"), static_cast<double>(CategoryCodexCardSize->GetHeightOverride()), 129.0);
	}

	UButton* CloseButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodexClose"))) : nullptr;
	TestNotNull(TEXT("codex exposes a dedicated close button"), CloseButton);
	TestTrue(TEXT("codex close uses the PSD primary button instead of ActionBlank"),
		GetButtonNormalResourcePath(CloseButton).Contains(TEXT("/Game/GameXXK/UI/Town/Textures/PSD/Controls/T_TownPsd_ButtonPrimary")));
	if (CloseButton)
	{
		CloseButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("close button closes the local codex overlay"), TownHud->IsCompanionCodexOpenForTest());
	if (CompanionButton)
	{
		CompanionButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("companion navigation does not reopen the codex on the page-02 shell"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("test helper reopens the codex"), TownHud->OpenCompanionCodexForTest());
	UButton* CodexDiscButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudCodex"))) : nullptr;
	TestNotNull(TEXT("town HUD exposes the codex navigation disc"), CodexDiscButton);
	if (CodexDiscButton)
	{
		CodexDiscButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("the codex disc shows the unavailable notice and keeps the overlay closed"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("test helper reopens the codex before task navigation"), TownHud->OpenCompanionCodexForTest());
	UButton* TaskButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudTask"))) : nullptr;
	TestNotNull(TEXT("town HUD exposes the task navigation button"), TaskButton);
	if (TaskButton)
	{
		TaskButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("Task closes the codex even without a player controller"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("test helper reopens the codex before inventory navigation"), TownHud->OpenCompanionCodexForTest());
	UButton* InventoryButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudInventory"))) : nullptr;
	TestNotNull(TEXT("town HUD exposes the inventory navigation button"), InventoryButton);
	if (InventoryButton)
	{
		InventoryButton->OnClicked.Broadcast();
	}
	TestFalse(TEXT("Inventory closes the codex even without a player controller"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("test helper opens the codex before a direct runtime WorldMap transition"), TownHud->OpenCompanionCodexForTest());
	TestTrue(TEXT("the runtime subsystem can directly return from Town to WorldMap"), Subsystem->OpenWorldMap());
	TownHud->RefreshFromState();
	TestFalse(TEXT("refreshing after a direct WorldMap transition closes the codex independently of the map button"), TownHud->IsCompanionCodexOpenForTest());
	TestTrue(TEXT("Qingshan can be reselected after the direct WorldMap transition"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TownHud->RefreshFromState();
	TestTrue(TEXT("test helper reopens the codex before its subsystem is detached"), TownHud->OpenCompanionCodexForTest());
	TownHud->SetMVPSubsystem(nullptr);
	TownHud->RefreshFromState();
	TestFalse(TEXT("refreshing without a subsystem closes the codex"), TownHud->IsCompanionCodexOpenForTest());

	TownHud->SetMVPSubsystem(Subsystem);
	TownHud->RefreshFromState();
	TestTrue(TEXT("the codex can reopen after its valid Town subsystem is restored"), TownHud->OpenCompanionCodexForTest());
	UButton* MapButton = TownHud->WidgetTree ? Cast<UButton>(TownHud->WidgetTree->FindWidget(TEXT("TownHudMap"))) : nullptr;
	TestNotNull(TEXT("town HUD exposes the actual world map navigation button"), MapButton);
	if (MapButton)
	{
		MapButton->OnClicked.Broadcast();
	}
	TestEqual(TEXT("the actual map button moves the runtime state to WorldMap"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::WorldMap);
	TestFalse(TEXT("the actual map button closes the codex while leaving Town"), TownHud->IsCompanionCodexOpenForTest());

	return true;
}

#endif
