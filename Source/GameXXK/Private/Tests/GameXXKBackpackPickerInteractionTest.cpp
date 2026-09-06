#include "Misc/AutomationTest.h"
#include "Engine/GameInstance.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/CanvasPanelSlot.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UI/GameXXKInventoryWindowWidget.h"

#if WITH_DEV_AUTOMATION_TESTS
namespace
{
	UGameXXKDesktopTrainingWorkbenchWidget* MakePickerFixture(UGameXXKMVPSubsystem*& Sub)
	{
		Sub=NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>());
		if (!Sub->StartGame()) return nullptr;
		auto* Widget=NewObject<UGameXXKDesktopTrainingWorkbenchWidget>();
		Widget->SetMVPSubsystem(Sub); Widget->ConstructForTest(); Widget->OpenBackpack();
		return Widget;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBackpackPickerDeferredSelectionTest,
	"GameXXK.DesktopTraining.BackpackPicker.SelectionAndCancel", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKBackpackPickerDeferredSelectionTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Sub=nullptr; auto* Widget=MakePickerFixture(Sub);
	if (!TestNotNull(TEXT("the real backpack opens"),Widget)) return false;
	const auto PartyBefore=Sub->GetRuntimeState().CardRun.PartySelection;
	const FName Hero=Widget->GetActiveBackpackCharacterIdForTest();
	Widget->HandleActionClicked(81);
	TestEqual(TEXT("opening partner candidates does not prematurely switch the viewed owner"),Widget->GetActiveBackpackCharacterIdForTest(),Hero);
	int32 Cards=0;
	double RowY=0.0;
	for (int32 I=0; I<6; ++I)
	{
		const auto* Card=Widget->WidgetTree->FindWidget(*FString::Printf(TEXT("CharacterRosterPortraitButton_1_%d"),I));
		if (!Card) continue;
		++Cards;
		const auto* CardSlot=Cast<UCanvasPanelSlot>(Card->Slot);
		TestTrue(TEXT("candidate cards keep a readable portrait-card aspect ratio"),CardSlot && CardSlot->GetSize().X>=130 && CardSlot->GetSize().Y>CardSlot->GetSize().X*1.3f);
		if(CardSlot)
		{
			if(I==0)RowY=CardSlot->GetPosition().Y;
			TestEqual(TEXT("all six cards stay in one horizontal row"),CardSlot->GetPosition().Y,RowY);
		}
	}
	TestEqual(TEXT("one partner page exposes six selectable characters"),Cards,6);
	Widget->HandleActionClicked(657);
	TestEqual(TEXT("cancel keeps the prior viewed owner"),Widget->GetActiveBackpackCharacterIdForTest(),Hero);
	TestTrue(TEXT("cancel keeps the backpack open"),Widget->IsBackpackExpandedForTest());
	TestNull(TEXT("the text selector has no representative portrait"),Widget->WidgetTree->FindWidget(TEXT("CharacterRosterRepresentativePortrait_1")));
	const auto Partners=Widget->GetCompanionCharacterIdsForTest();
	if (!TestTrue(TEXT("the fixture has a second partner"),Partners.Num()>=2)) return false;
	Widget->HandleActionClicked(81); Widget->HandleActionClicked(401);
	TestEqual(TEXT("choosing a card opens that character's backpack"),Widget->GetActiveBackpackCharacterIdForTest(),Partners[1]);
	TestEqual(TEXT("view switching preserves the deployed partner"),Sub->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,PartyBefore.ActivePermanentCompanionInstanceId);
	TestEqual(TEXT("view switching preserves the deployed NPC"),Sub->GetRuntimeState().CardRun.PartySelection.QuestNpc.NpcId,PartyBefore.QuestNpc.NpcId);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBackpackPickerOwnerDraftTest,
	"GameXXK.DesktopTraining.BackpackPicker.OwnerDraftIsolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKBackpackPickerOwnerDraftTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Sub=nullptr; auto* Widget=MakePickerFixture(Sub);
	if (!Widget) return false;
	auto* Inventory=Cast<UGameXXKInventoryWindowWidget>(Widget->WidgetTree->FindWidget(TEXT("EmbeddedApprovedBackpack")));
	if (!TestNotNull(TEXT("the embedded owner editor exists"),Inventory)) return false;
	Inventory->OpenCharacterBackpackTabForTest(EGameXXKCharacterBackpackTab::Deck);
	const auto Committed=Sub->GetRuntimeState().CardRun.HeroSelectedCardIds;
	if (!TestTrue(TEXT("hero begins with eight committed cards"),Committed.Num()==8)) return false;
	TestTrue(TEXT("one click edits the pending deck only"),Inventory->ToggleHeroDeckCardForTest(Committed[0]));
	const auto Draft=Inventory->GetPendingHeroDeckIdsForTest();
	TestEqual(TEXT("the draft now has seven cards"),Draft.Num(),7);
	const auto Partners=Widget->GetCompanionCharacterIdsForTest();
	if (Partners.IsEmpty()) return false;
	Widget->SelectBackpackCharacterForTest(Partners[0]);
	TestEqual(TEXT("the partner editor keeps its own five-card selection"),Widget->GetEmbeddedPendingDeckIdsForTest().Num(),5);
	Widget->SelectBackpackCharacterForTest(TEXT("Player"));
	TestTrue(TEXT("returning to the hero restores the same uncommitted draft"),Widget->GetEmbeddedPendingDeckIdsForTest()==Draft);
	TestTrue(TEXT("view switching never commits the incomplete hero draft"),Sub->GetRuntimeState().CardRun.HeroSelectedCardIds==Committed);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKBackpackPickerPagingTest,
	"GameXXK.DesktopTraining.BackpackPicker.PageSelectionIdentity", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FGameXXKBackpackPickerPagingTest::RunTest(const FString& Parameters)
{
	UGameXXKMVPSubsystem* Sub=nullptr; auto* Widget=MakePickerFixture(Sub);
	if (!Widget) return false;
	auto& Roster=Sub->GetMutableRuntimeState().CardRun.CompanionRoster.PermanentCompanions;
	if (Roster.IsEmpty()) return false;
	FGameXXKPermanentCompanion Extra=Roster[0]; Extra.InstanceId=TEXT("CompanionInstance.PickerExtra.00000001"); Extra.bIsActive=false;
	Roster.Add(Extra);
	const auto Ids=Widget->GetCompanionCharacterIdsForTest();
	if (!TestEqual(TEXT("the fixture owns seven distinct candidates"),Ids.Num(),7)) return false;
	const FName PartnerBefore=Sub->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId;
	Widget->HandleActionClicked(81); Widget->HandleActionClicked(659);
	TestNotNull(TEXT("the last page renders its remaining candidate"),Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_0")));
	TestNull(TEXT("the last page does not duplicate a missing second candidate"),Widget->WidgetTree->FindWidget(TEXT("CharacterRosterPortraitButton_1_1")));
	Widget->HandleActionClicked(400);
	TestEqual(TEXT("a page-two click resolves the global candidate identity"),Widget->GetActiveBackpackCharacterIdForTest(),Ids[6]);
	TestEqual(TEXT("paging and selection never deploy another partner"),Sub->GetRuntimeState().CardRun.PartySelection.ActivePermanentCompanionInstanceId,PartnerBefore);
	return true;
}
#endif
