#include "GameXXKCardBattleAdapter.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownNpcCharacter.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTownNpcInteractionRulesTest,
	"GameXXK.MVP.Town.NpcInteractionRules",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTownNpcInteractionRulesTest::RunTest(const FString& Parameters)
{
	AGameXXKTownNpcActor* TusiChief = NewObject<AGameXXKTownNpcActor>();
	TusiChief->SetNpcId(TEXT("Npc.TusiChief"));
	TestEqual(TEXT("Tusi Chief identity is stable"), TusiChief->GetNpcId(), FName(TEXT("Npc.TusiChief")));
	TestEqual(TEXT("Tusi Chief is always the story NPC"), TusiChief->GetNpcRole(), EGameXXKTownNpcRole::Quest);
	TestTrue(TEXT("Tusi Chief has a story action"), TusiChief->HasPrimaryInteractionAction());
	TestTrue(TEXT("Tusi Chief can join the route party"), TusiChief->CanJoinParty());

	AGameXXKTownNpcActor* SongJinBao = NewObject<AGameXXKTownNpcActor>();
	SongJinBao->SetNpcId(TEXT("Npc.SongJinBao"));
	TestEqual(TEXT("Song Jinbao is always the merchant NPC"), SongJinBao->GetNpcRole(), EGameXXKTownNpcRole::Merchant);
	TestTrue(TEXT("Song Jinbao has a shop action"), SongJinBao->HasPrimaryInteractionAction());
	TestTrue(TEXT("Song Jinbao can join the route party"), SongJinBao->CanJoinParty());

	AGameXXKTownNpcActor* YueBai = NewObject<AGameXXKTownNpcActor>();
	YueBai->SetNpcId(TEXT("Npc.YueBai"));
	TestEqual(TEXT("ordinary named NPC remains generic"), YueBai->GetNpcRole(), EGameXXKTownNpcRole::Generic);
	TestFalse(TEXT("ordinary named NPC has no story or shop action"), YueBai->HasPrimaryInteractionAction());
	TestTrue(TEXT("ordinary named NPC still has the join option"), YueBai->CanJoinParty());

	AGameXXKTownNpcCharacter* YueBaiCharacter = NewObject<AGameXXKTownNpcCharacter>();
	YueBaiCharacter->SetNpcId(TEXT("Npc.YueBai"));
	TestEqual(
		TEXT("named town NPC uses its final static idle artwork"),
		YueBaiCharacter->GetDefaultTownFlipbookPathString(),
		FString(TEXT("/Game/GameXXK/Characters/PartyDeckNPC/YueBai/Flipbooks/FB_PartyDeckNPC_YueBai_Idle_South.FB_PartyDeckNPC_YueBai_Idle_South")));

	YueBai->ActivateFollower(NewObject<AActor>(), 96.0f);
	TestFalse(TEXT("retired follower activation is a no-op"), YueBai->IsFollowerActive());
	TestNull(TEXT("retired follower activation stores no target"), YueBai->GetFollowTarget());

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestTrue(TEXT("town-NPC fixture opens the world map"), Subsystem->OpenWorldMap());
	TestTrue(TEXT("town-NPC fixture enters Qingshan town"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("Yue Bai can be selected directly from the town interaction"), Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	TestEqual(TEXT("town interaction stores the selected named NPC"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("town interaction applies the fixed three-card NPC loadout"), Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestFalse(TEXT("town recruitment never reactivates legacy following"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestFalse(TEXT("town recruitment never stores a moving NPC location"), Subsystem->GetRuntimeState().bHasQuestNpcLocation);
	TestTrue(TEXT("selecting another NPC replaces the current task NPC"), Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.SongJinBao")));
	TestEqual(TEXT("replacement NPC becomes the active route support"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.SongJinBao")));

	return true;
}

#endif
