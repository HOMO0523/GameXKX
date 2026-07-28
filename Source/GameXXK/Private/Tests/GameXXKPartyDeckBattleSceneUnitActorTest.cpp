#include "GameXXKMVPRules.h"
#include "Misc/AutomationTest.h"
#include "MVP/GameXXKBattleSceneUnitActor.h"
#include "PaperFlipbook.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FPartyDeckVisualExpectation
	{
		const TCHAR* RuntimeId;
		const TCHAR* ExpectedFlipbookPath;
	};

	FGameXXKBattleRuntimeUnit MakeLivingPartyUnit(const TCHAR* RuntimeId)
	{
		FGameXXKBattleRuntimeUnit Unit;
		Unit.Id = FName(RuntimeId);
		Unit.HP = 100;
		Unit.MaxHP = 100;
		Unit.bEnemy = false;
		Unit.bDefeated = false;
		return Unit;
	}
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

	const auto VerifyPartyDeckMapping = [this](const FPartyDeckVisualExpectation& Expectation)
	{
		AGameXXKBattleSceneUnitActor* Actor = NewObject<AGameXXKBattleSceneUnitActor>();
		Actor->ConfigureFromRuntimeUnit(false, 1, MakeLivingPartyUnit(Expectation.RuntimeId));
		UPaperFlipbook* Flipbook = Actor->GetCurrentBattleFlipbook();
		TestNotNull(FString::Printf(TEXT("PartyDeck unit %s resolves a visible South Idle flipbook"), Expectation.RuntimeId), Flipbook);
		if (Flipbook)
		{
			TestTrue(
				FString::Printf(TEXT("PartyDeck unit %s resolves its isolated visual"), Expectation.RuntimeId),
				Flipbook->GetPathName().Contains(Expectation.ExpectedFlipbookPath));
		}
	};

	for (const FPartyDeckVisualExpectation& Expectation : NamedNpcExpectations)
	{
		VerifyPartyDeckMapping(Expectation);
	}
	for (const FPartyDeckVisualExpectation& Expectation : PersistentCompanionExpectations)
	{
		VerifyPartyDeckMapping(Expectation);
	}

	AGameXXKBattleSceneUnitActor* HeroActor = NewObject<AGameXXKBattleSceneUnitActor>();
	HeroActor->ConfigureFromRuntimeUnit(false, 0, MakeLivingPartyUnit(TEXT("Player")));
	UPaperFlipbook* HeroFlipbook = HeroActor->GetCurrentBattleFlipbook();
	TestNotNull(TEXT("the existing hero battle flipbook remains available"), HeroFlipbook);
	if (HeroFlipbook)
	{
		TestTrue(
			TEXT("PartyDeck mappings leave the hero visual unchanged"),
			HeroFlipbook->GetPathName().Contains(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_West")));
	}

	return true;
}

#endif
