#include "Misc/AutomationTest.h"
#include "Misc/PackageName.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FPartyDeckVisualExpectation
	{
		const TCHAR* RuntimeId;
		const TCHAR* ExpectedFlipbookPath;
	};

}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPartyDeckBattleSceneUnitActorTest,
	"GameXXK.MVP.Battle.PartyDeckVisualMapping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPartyDeckBattleSceneUnitActorTest::RunTest(const FString& Parameters)
{
	const FPartyDeckVisualExpectation NamedNpcExpectations[] = {
		{TEXT("Npc.TusiChief"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/TusiChief/Flipbooks/FB_PartyDeckNPC_TusiChief_Idle_South")},
		{TEXT("Npc.SongJinBao"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/SongJinBao/Flipbooks/FB_PartyDeckNPC_SongJinBao_Idle_South")},
		{TEXT("Npc.YueBai"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/YueBai/Flipbooks/FB_PartyDeckNPC_YueBai_Idle_South")},
		{TEXT("Npc.ZhouGuangZu"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/ZhouGuangZu/Flipbooks/FB_PartyDeckNPC_ZhouGuangZu_Idle_South")},
		{TEXT("Npc.JinGui"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/JinGui/Flipbooks/FB_PartyDeckNPC_JinGui_Idle_South")},
		{TEXT("Npc.QiongMeiEr"), TEXT("/Game/GameXXK/Characters/PartyDeckNPC/QiongMeiEr/Flipbooks/FB_PartyDeckNPC_QiongMeiEr_Idle_South")},
	};
	const FPartyDeckVisualExpectation PersistentCompanionExpectations[] = {
		{TEXT("CompanionInstance.Companion_Blade_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Blade/Flipbooks/FB_PartyDeckPartner_Blade_Idle_South")},
		{TEXT("CompanionInstance.Companion_Guard_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Guard/Flipbooks/FB_PartyDeckPartner_Guard_Idle_South")},
		{TEXT("CompanionInstance.Companion_Healer_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Healer/Flipbooks/FB_PartyDeckPartner_Healer_Idle_South")},
		{TEXT("CompanionInstance.Companion_Hunter_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Hunter/Flipbooks/FB_PartyDeckPartner_Hunter_Idle_South")},
		{TEXT("CompanionInstance.Companion_Sorcerer_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/Sorcerer/Flipbooks/FB_PartyDeckPartner_Sorcerer_Idle_South")},
		{TEXT("CompanionInstance.Companion_FormationMaster_01.00001CA3"), TEXT("/Game/GameXXK/Characters/PartyDeckPartners/FormationMaster/Flipbooks/FB_PartyDeckPartner_FormationMaster_Idle_South")},
	};

	const auto VerifyPreservedPartyDeckPackage = [this](const FPartyDeckVisualExpectation& Expectation)
	{
		TestTrue(
			FString::Printf(TEXT("retirement preserves PartyDeck package for %s"), Expectation.RuntimeId),
			FPackageName::DoesPackageExist(Expectation.ExpectedFlipbookPath));
	};

	for (const FPartyDeckVisualExpectation& Expectation : NamedNpcExpectations)
	{
		VerifyPreservedPartyDeckPackage(Expectation);
	}
	for (const FPartyDeckVisualExpectation& Expectation : PersistentCompanionExpectations)
	{
		VerifyPreservedPartyDeckPackage(Expectation);
	}

	TestTrue(TEXT("retirement leaves the hero compatibility flipbook package available"),
		FPackageName::DoesPackageExist(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West")));

	return true;
}

#endif
