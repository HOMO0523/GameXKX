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

	AActor* FollowTarget = NewObject<AActor>();
	YueBai->ActivateFollower(FollowTarget, 96.0f);
	TestTrue(TEXT("town NPC follower activation stores an active state"), YueBai->IsFollowerActive());
	TestTrue(TEXT("town NPC follower activation stores its target"), YueBai->GetFollowTarget() == FollowTarget);
	TestEqual(TEXT("town NPC follower activation stores its distance"), YueBai->GetFollowDistance(), 96.0f);

	UGameInstance* RecoveryGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* RecoverySubsystem = NewObject<UGameXXKMVPSubsystem>(RecoveryGameInstance);
	FGameXXKRuntimeState& RecoveryState = RecoverySubsystem->GetMutableRuntimeState();
	RecoveryState = UGameXXKMVPRules::CreateNewGame();
	RecoveryState.QuestState = EGameXXKQuestState::Accepted;
	RecoveryState.bFollowerJoined = true;
	RecoveryState.bHasQuestNpcLocation = false;
	AGameXXKTownNpcActor* RestoredActorFollower = NewObject<AGameXXKTownNpcActor>();
	RestoredActorFollower->SetNpcId(TEXT("Npc.TusiChief"));
	RestoredActorFollower->SetMVPSubsystemForTest(RecoverySubsystem);
	RestoredActorFollower->ActivateFollower(FollowTarget, 96.0f);
	TestTrue(TEXT("restoring an actor follower backfills a missing legacy NPC location flag"),
		RecoveryState.bHasQuestNpcLocation);
	TestEqual(TEXT("restoring an actor follower records its actual current location"),
		RecoveryState.QuestNpcLocation, RestoredActorFollower->GetActorLocation());

	RecoveryState.bHasQuestNpcLocation = false;
	RecoveryState.QuestNpcLocation = FVector::ZeroVector;
	AGameXXKTownNpcCharacter* RestoredCharacterFollower = NewObject<AGameXXKTownNpcCharacter>();
	RestoredCharacterFollower->SetNpcId(TEXT("Npc.TusiChief"));
	RestoredCharacterFollower->SetMVPSubsystemForTest(RecoverySubsystem);
	RestoredCharacterFollower->ActivateFollower(FollowTarget, 96.0f);
	TestTrue(TEXT("restoring a character follower backfills a missing legacy NPC location flag"),
		RecoveryState.bHasQuestNpcLocation);
	TestEqual(TEXT("restoring a character follower records its actual current location"),
		RecoveryState.QuestNpcLocation, RestoredCharacterFollower->GetActorLocation());

	UGameInstance* GameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance);
	TestTrue(TEXT("town-NPC fixture opens the world map"), Subsystem->OpenWorldMap());
	TestTrue(TEXT("town-NPC fixture enters Qingshan town"), Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan()));
	TestTrue(TEXT("town-NPC fixture accepts the narrative quest"), Subsystem->AcceptQuest());
	// New semantics: accepting the quest keeps the guide NPC in town. Simulate the dialog
	// 入队 recruit (controller RecruitPendingTownNpc) so the route-support selection path
	// below still verifies it preserves an already-recruited narrative follower.
	Subsystem->GetMutableRuntimeState().bFollowerJoined = true;
	const FVector NarrativeFollowerLocation(320.0f, -96.0f, 72.0f);
	Subsystem->RecordQuestNpcLocation(NarrativeFollowerLocation);
	TestTrue(TEXT("Yue Bai can be selected directly from the town interaction"), Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.YueBai")));
	TestEqual(TEXT("town interaction stores the selected named NPC"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.YueBai")));
	TestEqual(TEXT("town interaction applies the fixed three-card NPC loadout"), Subsystem->GetRuntimeState().CardRun.PartySelection.QuestNpc.SelectedCardIds.Num(), 3);
	TestTrue(TEXT("route-support selection preserves the narrative follower"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestTrue(TEXT("route-support selection preserves the narrative follower location flag"), Subsystem->GetRuntimeState().bHasQuestNpcLocation);
	TestEqual(TEXT("route-support selection preserves the narrative follower location"), Subsystem->GetRuntimeState().QuestNpcLocation, NarrativeFollowerLocation);
	TestTrue(TEXT("selecting another NPC replaces the current task NPC"), Subsystem->SelectTownQuestNpcForParty(TEXT("Npc.SongJinBao")));
	TestEqual(TEXT("replacement NPC becomes the active route support"), Subsystem->GetRuntimeState().CardRun.ActiveTemporaryQuestNpcId, FName(TEXT("Npc.SongJinBao")));
	TestTrue(TEXT("replacing route support still preserves the narrative follower"), Subsystem->GetRuntimeState().bFollowerJoined);
	TestEqual(TEXT("replacing route support still preserves the narrative follower location"), Subsystem->GetRuntimeState().QuestNpcLocation, NarrativeFollowerLocation);

	return true;
}

#endif
