#include "Misc/AutomationTest.h"
#include "UI/GameXXKTalentTotals.h"
#include "UI/GameXXKTalentTreeWidget.h"
#include "GameXXKTalentCatalog.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Blueprint/WidgetTree.h"
#include "Engine/GameInstance.h"
#include "UObject/StrongObjectPtr.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTalentTotalsEmptyTest,"GameXXK.Talents.Totals.ExcludesBaseAndCompatibilityFloors",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKTalentTotalsEmptyTest::RunTest(const FString&)
{
	FGameXXKTalentProgress Progress;Progress.MinimumBackpackCapacity=200;Progress.MinimumWarehousePages=6;
	TArray<FGameXXKTalentTotalGroup> Groups;
	TestTrue(TEXT("legacy inventory floors remain valid"),GameXXKTalentTotals::Build(Progress,Groups));
	TestEqual(TEXT("base stats and compatibility capacity are not reported as learned talents"),Groups.Num(),0);
	Progress.NodeRanks.Add(TEXT("Talent.Root"),1);
	TestTrue(TEXT("the first learned talent is summarized"),GameXXKTalentTotals::Build(Progress,Groups));
	TestEqual(TEXT("only capacity has a learned bonus"),Groups.Num(),1);
	if(Groups.Num()==1 && Groups[0].Entries.Num()==1)
	{
		TestEqual(TEXT("warehouse pages are incremental, not legacy total pages"),Groups[0].Entries[0].Amount,1);
		TestEqual(TEXT("the entry keeps its page unit"),Groups[0].Entries[0].Value.ToString(),FString(TEXT("+1页")));
	}
	else AddError(TEXT("expected one merged warehouse entry"));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTalentTotalsFullTest,"GameXXK.Talents.Totals.MergesEffectsAndPreservesCapsAndUnits",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKTalentTotalsFullTest::RunTest(const FString&)
{
	FGameXXKTalentProgress Progress;
	for(const auto& Node:FGameXXKTalentCatalog::GetDefinitions())Progress.NodeRanks.Add(Node.Id,Node.MaxRank);
	TArray<FGameXXKTalentTotalGroup> Groups;FString Error;
	if(!TestTrue(TEXT("the complete valid talent tree can be summarized"),GameXXKTalentTotals::Build(Progress,Groups,&Error))){AddError(Error);return false;}
	TMap<FName,FGameXXKTalentTotalEntry> Entries;int32 Count=0;
	for(const auto& Group:Groups)for(const auto& Entry:Group.Entries){++Count;Entries.Add(Entry.Id,Entry);}
	TestEqual(TEXT("every effective attribute is included once"),Count,25);
	TestEqual(TEXT("same-type contributions cannot duplicate rows"),Entries.Num(),Count);
	TestEqual(TEXT("category separation stays explicit"),Groups.Num(),7);
	TestEqual(TEXT("foundation and attack tracks merge at the effective cap"),Entries.FindRef(TEXT("FlatAttack")).Amount,200);
	TestEqual(TEXT("percentage attack is kept separate from fixed attack"),Entries.FindRef(TEXT("RouteAttackPercent")).Value.ToString(),FString(TEXT("+100%")));
	TestEqual(TEXT("base backpack capacity is excluded"),Entries.FindRef(TEXT("BackpackSlots")).Amount,180);
	TestEqual(TEXT("base warehouse page is excluded"),Entries.FindRef(TEXT("WarehousePages")).Amount,5);
	TestEqual(TEXT("movement displays the real reduction in travel time"),Entries.FindRef(TEXT("TravelMovement")).Value.ToString(),FString(TEXT("-2.5秒")));
	TestEqual(TEXT("critical chance respects its cap"),Entries.FindRef(TEXT("CriticalChancePercent")).Amount,20);
	TestEqual(TEXT("offline chest time retains its distinct minute unit"),Entries.FindRef(TEXT("OfflineChestMinutes")).Value.ToString(),FString(TEXT("+525分")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKTalentTotalsPanelTest,"GameXXK.Talents.Totals.ReadOnlyPanelToggle",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKTalentTotalsPanelTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameXXKMVPSubsystem> Sub(NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>()));
	if(!Sub->StartGame())return false;
	TStrongObjectPtr<UGameXXKTalentTreeWidget> Widget(NewObject<UGameXXKTalentTreeWidget>());
	Widget->SetMVPSubsystem(Sub.Get());Widget->RebuildForTest();
	const int32 Gold=Sub->GetRuntimeState().PlayerGold;const FName Selected=Widget->GetSelectedNodeIdForTest();
	UWidget* Root=Widget->WidgetTree->RootWidget;
	TestTrue(TEXT("the actual totals button opens its read-only list"),Widget->ToggleTotalsForTest());
	TestEqual(TEXT("opening totals keeps the graph root"),Widget->WidgetTree->RootWidget.Get(),Root);
	TestEqual(TEXT("opening totals does not buy or change a talent"),Sub->GetRuntimeState().PlayerGold,Gold);
	TestEqual(TEXT("selection is preserved behind totals"),Widget->GetSelectedNodeIdForTest(),Selected);
	TestFalse(TEXT("the same button returns to talent details"),Widget->ToggleTotalsForTest());
	TestFalse(TEXT("the totals list is closed"),Widget->IsTotalsVisibleForTest());
	TestEqual(TEXT("root rank remains unchanged"),Sub->GetRuntimeState().Talents.NodeRanks.FindRef(TEXT("Talent.Root")),0);
	FGameXXKTalentPurchaseResult Purchase;
	TestTrue(TEXT("external root purchase uses the same authority"),Sub->PurchaseTalentNode(TEXT("Talent.Root"),Purchase));
	TestTrue(TEXT("external branch purchase succeeds"),Sub->PurchaseTalentNode(TEXT("Talent.Entry.Combat"),Purchase));
	Widget->SetMVPSubsystem(Sub.Get());Widget->TickForTest(0);
	TestEqual(TEXT("reopening the cached panel refreshes external rank changes"),Widget->GetNodeRankForTest(TEXT("Talent.Entry.Combat")).ToString(),FString(TEXT("1/1")));
	return true;
}
#endif
