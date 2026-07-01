#include "Interaction/GameXXKInteractionComponent.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownPlayerPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKTownShellTest,
	"GameXXK.MVP.Town.ShellInputInteractionFollower",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKTownShellTest::RunTest(const FString& Parameters)
{
	UGameXXKInteractionComponent* Interaction = NewObject<UGameXXKInteractionComponent>();
	AActor* FocusedActor = NewObject<AActor>();
	Interaction->SetFocusedActorForTest(FocusedActor);
	TestEqual(TEXT("focused actor stored for F interaction"), Interaction->GetFocusedActor(), FocusedActor);
	TestEqual(TEXT("interaction key is F"), Interaction->GetInteractionKey(), FKey(EKeys::F));
	Interaction->SetFocusedActorForTest(nullptr);
	TestNull(TEXT("focused actor clears"), Interaction->GetFocusedActor());

	AGameXXKTownPlayerPawn* Player = NewObject<AGameXXKTownPlayerPawn>();
	TestNotNull(TEXT("player owns interaction component"), Player->GetInteractionComponent());
	TestNotNull(TEXT("player owns movement component"), Player->GetMovementComponent());
	TestTrue(TEXT("W key is accepted movement input"), Player->IsSupportedMovementKey(EKeys::W));
	TestTrue(TEXT("A key is accepted movement input"), Player->IsSupportedMovementKey(EKeys::A));
	TestTrue(TEXT("S key is accepted movement input"), Player->IsSupportedMovementKey(EKeys::S));
	TestTrue(TEXT("D key is accepted movement input"), Player->IsSupportedMovementKey(EKeys::D));
	TestTrue(TEXT("Up arrow is accepted movement input"), Player->IsSupportedMovementKey(EKeys::Up));
	TestTrue(TEXT("Left arrow is accepted movement input"), Player->IsSupportedMovementKey(EKeys::Left));
	TestTrue(TEXT("Down arrow is accepted movement input"), Player->IsSupportedMovementKey(EKeys::Down));
	TestTrue(TEXT("Right arrow is accepted movement input"), Player->IsSupportedMovementKey(EKeys::Right));
	TestFalse(TEXT("E key is not town interaction"), Player->IsInteractionKey(EKeys::E));
	TestTrue(TEXT("F key is town interaction"), Player->IsInteractionKey(EKeys::F));
	TestTrue(TEXT("player has Paper2D visual shell"), Player->HasTownVisual());
	TestEqual(TEXT("town input binds movement press/release keys plus F"), Player->CountTownInputBindingsForTest(), 17);

	AGameXXKTownNpcActor* QuestNpc = NewObject<AGameXXKTownNpcActor>();
	QuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	TestEqual(TEXT("quest NPC role stored"), QuestNpc->GetNpcRole(), EGameXXKTownNpcRole::Quest);
	TestTrue(TEXT("quest NPC opens quest interaction"), QuestNpc->CanOfferQuest());
	TestFalse(TEXT("quest NPC is not merchant"), QuestNpc->CanTrade());

	AGameXXKTownNpcActor* MerchantNpc = NewObject<AGameXXKTownNpcActor>();
	MerchantNpc->SetNpcRole(EGameXXKTownNpcRole::Merchant);
	TestTrue(TEXT("merchant NPC can trade"), MerchantNpc->CanTrade());
	TestFalse(TEXT("merchant NPC does not offer quest"), MerchantNpc->CanOfferQuest());

	AGameXXKTownNpcActor* FollowerNpc = NewObject<AGameXXKTownNpcActor>();
	FollowerNpc->SetNpcRole(EGameXXKTownNpcRole::Follower);
	FollowerNpc->ActivateFollower(Player, 96.0f);
	TestTrue(TEXT("follower becomes active"), FollowerNpc->IsFollowerActive());
	TestTrue(TEXT("follower target stored"), FollowerNpc->GetFollowTarget() == Player);
	TestEqual(TEXT("follower distance stored"), FollowerNpc->GetFollowDistance(), 96.0f);
	FollowerNpc->DismissFollower();
	TestFalse(TEXT("follower dismisses after route clear"), FollowerNpc->IsFollowerActive());

	return true;
}

#endif
