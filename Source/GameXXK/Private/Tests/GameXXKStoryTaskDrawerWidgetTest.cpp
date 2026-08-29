#include "Misc/AutomationTest.h"

#include "UI/GameXXKStoryTaskDrawerWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/ScrollBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Engine/Texture2D.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace GameXXKStoryTaskDrawerWidgetTestPrivate
{
	FGameXXKStoryTaskDrawerEntryView MakeEntry(
		const TCHAR* Id,
		const EGameXXKTaskState State,
		const TCHAR* ActionLabel,
		const EGameXXKStoryTaskContinuation Continuation)
	{
		FGameXXKStoryTaskDrawerEntryView Entry;
		Entry.TaskId = FName(Id);
		Entry.State = State;
		Entry.Title = FText::FromString(FString::Printf(TEXT("标题 %s"), Id));
		Entry.Summary = FText::FromString(FString::Printf(TEXT("摘要 %s"), Id));
		Entry.Description = FText::FromString(FString::Printf(TEXT("描述 %s\n第二行\n第三行"), Id));
		Entry.ActionLabel = FText::FromString(ActionLabel);
		Entry.Continuation = Continuation;
		Entry.MaterialReward.Gold = 25;
		return Entry;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKStoryTaskDrawerWidgetTest,
	"GameXXK.Narrative.StoryTask.DrawerWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKStoryTaskDrawerWidgetTest::RunTest(const FString& Parameters)
{
	using namespace GameXXKStoryTaskDrawerWidgetTestPrivate;

	UGameXXKStoryTaskDrawerWidget* const Widget = NewObject<UGameXXKStoryTaskDrawerWidget>();
	if (!TestNotNull(TEXT("drawer widget is constructed"), Widget))
	{
		return false;
	}

	FGameXXKStoryTaskDrawerSnapshot Snapshot;
	Snapshot.Actionable = {
		MakeEntry(TEXT("Task.Drawer.Actionable.One"), EGameXXKTaskState::Available, TEXT("接取任务"), EGameXXKStoryTaskContinuation::NarrativeReplay),
		MakeEntry(TEXT("Task.Drawer.Actionable.Two"), EGameXXKTaskState::Active, TEXT("继续剧情"), EGameXXKStoryTaskContinuation::RouteResume)};
	Snapshot.Claimable = {
		MakeEntry(TEXT("Task.Drawer.Claimable.One"), EGameXXKTaskState::Completed, TEXT("领取奖励"), EGameXXKStoryTaskContinuation::ObjectiveResume)};
	for (int32 Index = 0; Index < 6; ++Index)
	{
		Snapshot.Actionable.Add(MakeEntry(*FString::Printf(TEXT("Task.Drawer.Actionable.Scroll.%d"), Index), EGameXXKTaskState::Available, TEXT("接取任务"), EGameXXKStoryTaskContinuation::NarrativeReplay));
		Snapshot.Claimable.Add(MakeEntry(*FString::Printf(TEXT("Task.Drawer.Claimable.Scroll.%d"), Index), EGameXXKTaskState::Completed, TEXT("领取奖励"), EGameXXKStoryTaskContinuation::NarrativeReplay));
	}
	Snapshot.bHasClaimableRedDot = true;
	FGameXXKStoryTaskDrawerUiState UiState;
	UiState.SelectedActionableTaskId = Snapshot.Actionable[0].TaskId;
	UiState.SelectedClaimableTaskId = Snapshot.Claimable[0].TaskId;
	UiState.ActionableScrollOffset = 31.0f;
	UiState.ClaimableScrollOffset = 57.0f;

	Widget->ApplySnapshot(Snapshot, UiState);
	Widget->TakeWidget();

	UBorder* const PanelBorder = Cast<UBorder>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerPanel")));
	UButton* const CloseButton = Cast<UButton>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerClose")));
	TestNotNull(TEXT("panel border exists"), PanelBorder);
	TestEqual(TEXT("panel keeps Warehouse-local child coordinates"), PanelBorder ? PanelBorder->GetPadding() : FMargin(1.0f), FMargin(0.0f));
	TestTrue(TEXT("panel uses approved tall paper"), PanelBorder && PanelBorder->Background.GetResourceObject()
		&& PanelBorder->Background.GetResourceObject()->GetPathName().Contains(TEXT("T_MasterV2_PanelTall")));
	TestNotNull(TEXT("named close control exists"), CloseButton);
	if (!CloseButton)
	{
		return false;
	}
	UCanvasPanelSlot* const CloseSlot = CloseButton ? Cast<UCanvasPanelSlot>(CloseButton->Slot) : nullptr;
	TestNotNull(TEXT("close is positioned in local canvas"), CloseSlot);
	TestEqual(TEXT("actual close local position matches Warehouse"), CloseSlot ? CloseSlot->GetPosition() : FVector2D::ZeroVector, FVector2D(315.0f, 31.0f));
	TestEqual(TEXT("actual close local size matches Warehouse"), CloseSlot ? CloseSlot->GetSize() : FVector2D::ZeroVector, FVector2D(44.0f, 44.0f));
	TestEqual(TEXT("panel fallback is a visible box brush"), PanelBorder ? static_cast<ESlateBrushDrawType::Type>(PanelBorder->Background.DrawAs) : ESlateBrushDrawType::NoDrawType, ESlateBrushDrawType::Box);
	UTextBlock* const CloseLabel = Cast<UTextBlock>(CloseButton->GetContent());
	TestNotNull(TEXT("close retains a visible textual fallback"), CloseLabel);
	TestTrue(TEXT("close textual fallback is the multiply sign"), CloseLabel && CloseLabel->GetText().ToString().Contains(TEXT("×")));
	TestNotNull(TEXT("actionable tab exists"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerActionableTab")));
	TestNotNull(TEXT("claimable tab exists"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerClaimableTab")));
	TestNotNull(TEXT("claimable red dot exists"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerClaimableRedDot")));
	TestNotNull(TEXT("scroll list exists"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerList")));
	TestNotNull(TEXT("fixed primary action exists"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerPrimaryAction")));
	UImage* const RedDot = Cast<UImage>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerClaimableRedDot")));
	TestTrue(TEXT("claimable red dot follows snapshot"), RedDot && RedDot->GetVisibility() != ESlateVisibility::Collapsed);
	TestEqual(TEXT("actionable tab has all compact rows"), Widget->GetRowCountForTest(), 8);
	TestEqual(TEXT("compact rows have no action buttons"), Widget->CountRowActionButtonsForTest(), 0);
	UButton* const FirstRow = Cast<UButton>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerRow_0")));
	TestNotNull(TEXT("first actual selection row exists"), FirstRow);
	if (!FirstRow)
	{
		return false;
	}
	UVerticalBox* const FirstRowContents = FirstRow ? Cast<UVerticalBox>(FirstRow->GetContent()) : nullptr;
	TestNotNull(TEXT("row has a two-line vertical container"), FirstRowContents);
	if (!FirstRowContents)
	{
		return false;
	}
	UHorizontalBox* const FirstLine = FirstRowContents ? Cast<UHorizontalBox>(FirstRowContents->GetChildAt(0)) : nullptr;
	UTextBlock* const Summary = FirstRowContents ? Cast<UTextBlock>(FirstRowContents->GetChildAt(1)) : nullptr;
	TestNotNull(TEXT("row first line puts marker and title together"), FirstLine);
	if (!FirstLine)
	{
		return false;
	}
	TestEqual(TEXT("row first line has marker and title"), FirstLine ? FirstLine->GetChildrenCount() : 0, 2);
	if (FirstLine->GetChildrenCount() < 2)
	{
		return false;
	}
	UWidget* const MarkerWidget = FirstLine->GetChildAt(0);
	TestNotNull(TEXT("row marker child exists"), MarkerWidget);
	if (!MarkerWidget)
	{
		return false;
	}
	UTextBlock* const RowTitle = Cast<UTextBlock>(FirstLine->GetChildAt(1));
	UHorizontalBoxSlot* const MarkerSlot = Cast<UHorizontalBoxSlot>(MarkerWidget->Slot);
	TestNotNull(TEXT("row second line is summary"), Summary);
	TestFalse(TEXT("title is single-line"), RowTitle ? RowTitle->GetAutoWrapText() : true);
	TestFalse(TEXT("summary is single-line"), Summary ? Summary->GetAutoWrapText() : true);
	TestEqual(TEXT("title uses ellipsis overflow"), RowTitle ? RowTitle->GetTextOverflowPolicy() : ETextOverflowPolicy::Clip, ETextOverflowPolicy::Ellipsis);
	TestEqual(TEXT("summary uses ellipsis overflow"), Summary ? Summary->GetTextOverflowPolicy() : ETextOverflowPolicy::Clip, ETextOverflowPolicy::Ellipsis);
	TestTrue(TEXT("marker has an explicit title separator"), MarkerSlot && MarkerSlot->GetPadding().Right >= 8.0f);
	UButton* const InjectedRowAction = Widget->WidgetTree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("StoryTaskDrawerInjectedRowAction"));
	FirstRowContents->AddChildToVerticalBox(InjectedRowAction);
	TestEqual(TEXT("actual nested row action is counted"), Widget->CountRowActionButtonsForTest(), 1);
	InjectedRowAction->RemoveFromParent();
	TestEqual(TEXT("normal compact rows still expose zero actions"), Widget->CountRowActionButtonsForTest(), 0);
	TestEqual(TEXT("initial selected task is actionable first"), Widget->GetSelectedTaskIdForTest(), Snapshot.Actionable[0].TaskId);
	TestEqual(TEXT("primary uses selected row label"), Widget->GetPrimaryActionLabelForTest().ToString(), FString(TEXT("接取任务")));
	TestTrue(TEXT("primary is enabled for valid selection"), Widget->IsPrimaryActionEnabledForTest());

	int32 CloseCount = 0;
	int32 PrimaryCount = 0;
	FName PrimaryTaskId = NAME_None;
	EGameXXKTaskState PrimaryState = EGameXXKTaskState::Locked;
	EGameXXKStoryTaskContinuation PrimaryContinuation = EGameXXKStoryTaskContinuation::NarrativeReplay;
	Widget->SetCloseRequested(FGameXXKStoryTaskDrawerCloseDelegate::CreateLambda([&CloseCount]() { ++CloseCount; }));
	Widget->SetPrimaryActionRequested(FGameXXKStoryTaskDrawerPrimaryActionDelegate::CreateLambda([&PrimaryCount, &PrimaryTaskId, &PrimaryState, &PrimaryContinuation](
		const FName TaskId,
		const EGameXXKTaskState State,
		const EGameXXKStoryTaskContinuation Continuation)
	{
		++PrimaryCount;
		PrimaryTaskId = TaskId;
		PrimaryState = State;
		PrimaryContinuation = Continuation;
	}));

	const FString FirstRowPath = FirstRow ? FirstRow->GetPathName() : FString();
	FirstRow->OnClicked.Broadcast();
	TestTrue(TEXT("actual row pointer remains valid after its callback"), IsValid(FirstRow));
	TestEqual(TEXT("actual row remains in drawer tree after callback"), Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerRow_0")), static_cast<UWidget*>(FirstRow));
	UVerticalBox* const CurrentRows = Cast<UVerticalBox>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerRows")));
	TestEqual(TEXT("actual row remains parented by the live list after callback"), FirstRow ? FirstRow->GetParent() : nullptr, static_cast<UPanelWidget*>(CurrentRows));
	TestEqual(TEXT("actual row path remains stable after callback"), FirstRow ? FirstRow->GetPathName() : FString(), FirstRowPath);
	Widget->SimulateSelectRowForTest(1);
	TestEqual(TEXT("row selection changes fixed detail task only"), Widget->GetSelectedTaskIdForTest(), Snapshot.Actionable[1].TaskId);
	TestEqual(TEXT("row selection changes fixed action label only"), Widget->GetPrimaryActionLabelForTest().ToString(), FString(TEXT("继续剧情")));
	TestEqual(TEXT("selection does not emit action"), PrimaryCount, 0);
	UGameXXKStoryTaskDrawerScrollBox* const List = Cast<UGameXXKStoryTaskDrawerScrollBox>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerList")));
	TestNotNull(TEXT("actual scroll list is available"), List);
	if (!List)
	{
		return false;
	}
	List->OnUserScrolled.Broadcast(73.0f);
	Widget->SimulateSelectTabForTest(EGameXXKStoryTaskDrawerTab::Claimable);
	TestEqual(TEXT("claimable tab changes row count"), Widget->GetRowCountForTest(), 7);
	TestEqual(TEXT("claimable keeps its independent selection"), Widget->GetSelectedTaskIdForTest(), Snapshot.Claimable[0].TaskId);
	List->OnUserScrolled.Broadcast(91.0f);
	Widget->SimulateSelectTabForTest(EGameXXKStoryTaskDrawerTab::Actionable);
	TestEqual(TEXT("actionable selection survives tab switch"), Widget->GetSelectedTaskIdForTest(), Snapshot.Actionable[1].TaskId);
	const FGameXXKStoryTaskDrawerUiState RestoredUiState = Widget->GetUiStateForTest();
	TestEqual(TEXT("actionable user scroll state remains local"), RestoredUiState.ActionableScrollOffset, 73.0f);
	TestEqual(TEXT("actual ScrollBox desired actionable offset is restored"), List ? List->GetDesiredScrollOffsetForTest() : 0.0f, 73.0f);
	Widget->SimulateSelectTabForTest(EGameXXKStoryTaskDrawerTab::Claimable);
	TestEqual(TEXT("claimable user scroll state remains local"), Widget->GetUiStateForTest().ClaimableScrollOffset, 91.0f);
	TestEqual(TEXT("actual ScrollBox desired claimable offset is restored"), List ? List->GetDesiredScrollOffsetForTest() : 0.0f, 91.0f);
	Widget->SimulateSelectTabForTest(EGameXXKStoryTaskDrawerTab::Actionable);

	Widget->SimulatePrimaryActionForTest();
	TestEqual(TEXT("primary emits exactly once"), PrimaryCount, 1);
	TestEqual(TEXT("primary emits selected task"), PrimaryTaskId, Snapshot.Actionable[1].TaskId);
	TestEqual(TEXT("primary emits selected state"), PrimaryState, EGameXXKTaskState::Active);
	TestEqual(TEXT("primary emits selected continuation"), PrimaryContinuation, EGameXXKStoryTaskContinuation::RouteResume);
	Widget->OpenDrawer();
	CloseButton->OnClicked.Broadcast();
	TestEqual(TEXT("actual close emits once"), CloseCount, 1);
	Widget->CloseDrawer();
	TestEqual(TEXT("external close is idempotent while closed"), CloseCount, 1);
	Widget->OpenDrawer();
	Widget->CloseDrawer();
	TestEqual(TEXT("external close emits once after reopening"), CloseCount, 2);
	Widget->CloseDrawer();
	TestEqual(TEXT("repeated external close does not duplicate"), CloseCount, 2);

	FGameXXKStoryTaskDrawerSnapshot EmptySnapshot;
	FGameXXKStoryTaskDrawerUiState EmptyUiState;
	EmptyUiState.ActiveTab = EGameXXKStoryTaskDrawerTab::Claimable;
	Widget->ApplySnapshot(EmptySnapshot, EmptyUiState);
	TestEqual(TEXT("empty tab has no rows"), Widget->GetRowCountForTest(), 0);
	TestFalse(TEXT("empty selection disables primary"), Widget->IsPrimaryActionEnabledForTest());
	UButton* const EmptyPrimaryButton = Cast<UButton>(Widget->FindNamedControlForTest(TEXT("StoryTaskDrawerPrimaryAction")));
	TestNotNull(TEXT("actual primary remains available for empty-state inspection"), EmptyPrimaryButton);
	if (!EmptyPrimaryButton)
	{
		return false;
	}
	TestTrue(TEXT("actual primary is disabled for empty selection"), !EmptyPrimaryButton->GetIsEnabled());
	TestTrue(TEXT("updated empty snapshot hides actual red dot"), RedDot && RedDot->GetVisibility() == ESlateVisibility::Collapsed);
	return true;
}

#endif
