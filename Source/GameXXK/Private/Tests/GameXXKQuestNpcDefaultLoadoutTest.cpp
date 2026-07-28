#include "GameXXKCardBattleAdapter.h"
#include "GameXXKCompanionCatalog.h"
#include "GameXXKMVPRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FExpectedQuestNpcLoadout
	{
		FName NpcId;
		TArray<FName> CardIds;
	};
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKQuestNpcDefaultLoadoutTest,
	"GameXXK.Data.Companion.QuestNpcDefaultLoadouts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKQuestNpcDefaultLoadoutTest::RunTest(const FString& Parameters)
{
	const TArray<FExpectedQuestNpcLoadout> ExpectedLoadouts = {
		{TEXT("Npc.TusiChief"), {TEXT("Npc.TusiChief.ZhaiZhuHaoLing"), TEXT("Npc.TusiChief.ShiMenShouShi"), TEXT("Npc.TusiChief.TuSiJunLing")}},
		{TEXT("Npc.SongJinBao"), {TEXT("Npc.SongJinBao.ErMuMiBao"), TEXT("Npc.SongJinBao.ShangQianGuWu"), TEXT("Npc.SongJinBao.YiNuoQianJin")}},
		{TEXT("Npc.YueBai"), {TEXT("Npc.YueBai.QingYanDianDeng"), TEXT("Npc.YueBai.CanJuanPiZhu"), TEXT("Npc.YueBai.YueBaiZhaoYe")}},
		{TEXT("Npc.ZhouGuangZu"), {TEXT("Npc.ZhouGuangZu.YiCaoBianShi"), TEXT("Npc.ZhouGuangZu.HuangShanFuZhi"), TEXT("Npc.ZhouGuangZu.YanFenFengMai")}},
		{TEXT("Npc.JinGui"), {TEXT("Npc.JinGui.ShiJingErMu"), TEXT("Npc.JinGui.QiaoYanZhouXuan"), TEXT("Npc.JinGui.ZaYiChouBei")}},
		{TEXT("Npc.QiongMeiEr"), {TEXT("Npc.QiongMeiEr.TengQiaoFeiDu"), TEXT("Npc.QiongMeiEr.GuWuMiZong"), TEXT("Npc.QiongMeiEr.YinLingZhenXin")}}};

	for (const FExpectedQuestNpcLoadout& ExpectedLoadout : ExpectedLoadouts)
	{
		const FGameXXKQuestNpcDefinition* Definition = FGameXXKCompanionCatalog::FindQuestNpcDefinition(ExpectedLoadout.NpcId);
		TestNotNull(FString::Printf(TEXT("the named task NPC exists (%s)"), *ExpectedLoadout.NpcId.ToString()), Definition);
		if (!Definition)
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("the named task NPC retains four fixed cards (%s)"), *ExpectedLoadout.NpcId.ToString()), Definition->FixedCardIds.Num(), 4);

		FGameXXKRuntimeState State = UGameXXKMVPRules::CreateNewGame();
		FString Error;
		if (!TestTrue(FString::Printf(TEXT("the route attaches %s with its authored default loadout: %s"), *ExpectedLoadout.NpcId.ToString(), *Error),
			FGameXXKCardBattleAdapter::SetQuestNpcForCurrentRun(State, ExpectedLoadout.NpcId, {}, &Error)))
		{
			return false;
		}
		TestEqual(FString::Printf(TEXT("the route stores the authored three-card NPC loadout in order (%s)"), *ExpectedLoadout.NpcId.ToString()),
			State.CardRun.PartySelection.QuestNpc.SelectedCardIds,
			ExpectedLoadout.CardIds);
		TestEqual(FString::Printf(TEXT("the route stores the task NPC identity with its authored loadout (%s)"), *ExpectedLoadout.NpcId.ToString()),
			State.CardRun.ActiveTemporaryQuestNpcId,
			ExpectedLoadout.NpcId);
	}

	return true;
}

#endif
