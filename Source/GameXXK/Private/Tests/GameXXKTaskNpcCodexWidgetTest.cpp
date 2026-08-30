#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/GameInstance.h"
#include "GameXXKCardCatalog.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "UI/GameXXKTownHudWidget.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FExpectedTaskNpcCodexEntry
	{
		FName NpcId;
		const TCHAR* PortraitResourcePath;
		const TCHAR* DisplayName;
	};

	const FExpectedTaskNpcCodexEntry ExpectedTaskNpcs[] = {
		{TEXT("Npc.TusiChief"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_TusiChief.T_CardPortrait_Npc_TusiChief"), TEXT("土司首领")},
		{TEXT("Npc.SongJinBao"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_SongJinBao.T_CardPortrait_Npc_SongJinBao"), TEXT("宋金宝")},
		{TEXT("Npc.YueBai"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_YueBai.T_CardPortrait_Npc_YueBai"), TEXT("月白")},
		{TEXT("Npc.ZhouGuangZu"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_ZhouGuangZu.T_CardPortrait_Npc_ZhouGuangZu"), TEXT("周光祖")},
		{TEXT("Npc.JinGui"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_JinGui.T_CardPortrait_Npc_JinGui"), TEXT("金贵")},
		{TEXT("Npc.QiongMeiEr"), TEXT("/Game/GameXXK/UI/PartyDeck/CardArt/T_CardPortrait_Npc_QiongMeiEr.T_CardPortrait_Npc_QiongMeiEr"), TEXT("琼么儿")}
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTaskNpcCodexWidgetTest,
	"GameXXK.MVP.UI.TaskNpcCodex",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTaskNpcCodexWidgetTest::RunTest(const FString& Parameters)
{
	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	UGameXXKTownHudWidget* TownHud = NewObject<UGameXXKTownHudWidget>();

	TestTrue(TEXT("new game starts through the real subsystem"), Subsystem->StartGame());
	TestTrue(TEXT("Qingshan enters the real town state"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TownHud->SetMVPSubsystem(Subsystem);
	TownHud->TakeWidget();
	TownHud->RefreshFromState();
	TestTrue(TEXT("the existing codex opens in Town"), TownHud->OpenCompanionCodexForTest());

	const TArray<FName> TaskNpcIds = TownHud->GetTaskNpcCodexEntryIdsForTest();
	TestEqual(TEXT("task NPC codex has exactly the approved six entries"), TaskNpcIds.Num(), static_cast<int32>(UE_ARRAY_COUNT(ExpectedTaskNpcs)));
	TestFalse(TEXT("retired event NPC NiuHuan is never listed as a fixed-NPC entry"), TaskNpcIds.Contains(TEXT("Npc.Event.NiuHuan")));
	TestFalse(TEXT("retired event NPC NiuHuan is not an approved fixed-NPC definition"), FGameXXKCompanionCatalog::FindQuestNpcDefinition(TEXT("Npc.Event.NiuHuan")) != nullptr);

	for (const FExpectedTaskNpcCodexEntry& Expected : ExpectedTaskNpcs)
	{
		TestTrue(FString::Printf(TEXT("task NPC codex lists %s"), *Expected.NpcId.ToString()), TaskNpcIds.Contains(Expected.NpcId));
		TestTrue(FString::Printf(TEXT("%s is selectable from the task NPC codex"), *Expected.NpcId.ToString()), TownHud->SelectTaskNpcCodexEntryForTest(Expected.NpcId));
		TestTrue(
			FString::Printf(TEXT("%s keeps its named original identity in the selected detail"), *Expected.NpcId.ToString()),
			TownHud->GetTaskNpcCodexDetailForTest().ToString().Contains(Expected.DisplayName));
		TestEqual(
			FString::Printf(TEXT("%s uses its locked original-art portrait"), *Expected.NpcId.ToString()),
			TownHud->GetTaskNpcPortraitResourcePathForTest(Expected.NpcId),
			FString(Expected.PortraitResourcePath));

		const TArray<FName> Loadout = TownHud->GetTaskNpcFixedRouteLoadoutForTest(Expected.NpcId);
		TestEqual(FString::Printf(TEXT("%s exposes exactly three fixed route cards"), *Expected.NpcId.ToString()), Loadout.Num(), 3);
		for (const FName CardId : Loadout)
		{
			const FGameXXKCardDefinition* Card = FGameXXKCardCatalog::FindCardDefinition(CardId);
			TestNotNull(FString::Printf(TEXT("%s loadout card resolves"), *CardId.ToString()), Card);
			if (Card)
			{
				TestEqual(FString::Printf(TEXT("%s is a task-NPC card"), *CardId.ToString()), Card->Owner, EGameXXKCardOwner::QuestNpc);
				TestEqual(FString::Printf(TEXT("%s belongs to its displayed NPC"), *CardId.ToString()), Card->NpcId, Expected.NpcId);
			}
		}
	}

	TestTrue(TEXT("selecting a listed task NPC succeeds"), TownHud->SelectTaskNpcCodexEntryForTest(TEXT("Npc.TusiChief")));
	const FString TusiDetail = TownHud->GetTaskNpcCodexDetailForTest().ToString();
	TestTrue(TEXT("selected NPC detail names the original identity"), TusiDetail.Contains(TEXT("土司首领")));
	TestTrue(TEXT("selected NPC detail identifies permanent ownership"), TusiDetail.Contains(TEXT("固定 NPC")));
	TestTrue(TEXT("selected NPC detail says it can join the formation"), TusiDetail.Contains(TEXT("可编入队伍")));
	TestFalse(TEXT("selected NPC detail has no temporary-route wording"), TusiDetail.Contains(TEXT("临时")));
	TestFalse(TEXT("selected NPC detail has no non-recruitable wording"), TusiDetail.Contains(TEXT("不可招募")));
	TestTrue(TEXT("selected NPC detail includes its combat role"), TusiDetail.Contains(TEXT("战斗定位")));
	TestTrue(TEXT("selected NPC detail includes its base attributes"), TusiDetail.Contains(TEXT("生命 115")));
	const UTextBlock* Caption = TownHud->WidgetTree
		? Cast<UTextBlock>(TownHud->WidgetTree->FindWidget(TEXT("TownHudTaskNpcCodexCaption")))
		: nullptr;
	TestNotNull(TEXT("fixed NPC codex owns a caption"), Caption);
	TestTrue(TEXT("fixed NPC caption describes all six as owned"),
		Caption && Caption->GetText().ToString().Contains(TEXT("固定拥有")));
	TestFalse(TEXT("fixed NPC caption does not advertise route-only support"),
		Caption && Caption->GetText().ToString().Contains(TEXT("临时")));
	TestNotNull(
		TEXT("selected task NPC has a real named original-art portrait widget"),
		TownHud->WidgetTree ? Cast<UImage>(TownHud->WidgetTree->FindWidget(TEXT("TownHudTaskNpcCodexDetailPortrait"))) : nullptr);
	for (int32 LoadoutIndex = 0; LoadoutIndex < 3; ++LoadoutIndex)
	{
		TestNotNull(
			FString::Printf(TEXT("selected task NPC renders fixed route card %d with the PSD card face"), LoadoutIndex + 1),
			TownHud->WidgetTree ? Cast<UCanvasPanel>(TownHud->WidgetTree->FindWidget(FName(*FString::Printf(TEXT("TownHudTaskNpcCodexLoadoutCard_%d"), LoadoutIndex)))) : nullptr);
	}

	return true;
}

#endif
