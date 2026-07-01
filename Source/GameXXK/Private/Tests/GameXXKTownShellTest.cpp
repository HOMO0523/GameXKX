#include "Interaction/GameXXKInteractionComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "GameXXKMVPRules.h"
#include "MVP/GameXXKMVPSubsystem.h"
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
	UPrimitiveComponent* PlayerCollision = Player->GetTownCollisionComponent();
	TestNotNull(TEXT("player has explicit Pawn overlap collision"), PlayerCollision);
	TestEqual(TEXT("player collision is query-only overlap"), PlayerCollision->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("player collision is Pawn object channel"), PlayerCollision->GetCollisionObjectType(), ECollisionChannel::ECC_Pawn);
	TestEqual(TEXT("player collision overlaps world dynamic NPC areas"), PlayerCollision->GetCollisionResponseToChannel(ECC_WorldDynamic), ECollisionResponse::ECR_Overlap);
	TestTrue(TEXT("player collision generates overlap events"), PlayerCollision->GetGenerateOverlapEvents());
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
	TestNotNull(TEXT("quest NPC has interaction area"), QuestNpc->GetInteractionArea());
	TestEqual(TEXT("quest NPC area is query-only overlap"), QuestNpc->GetInteractionArea()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("quest NPC area overlaps Pawn channel"), QuestNpc->GetInteractionArea()->GetCollisionResponseToChannel(ECC_Pawn), ECollisionResponse::ECR_Overlap);
	TestTrue(TEXT("quest NPC area generates overlap events"), QuestNpc->GetInteractionArea()->GetGenerateOverlapEvents());

	AGameXXKTownNpcActor* MerchantNpc = NewObject<AGameXXKTownNpcActor>();
	MerchantNpc->SetNpcRole(EGameXXKTownNpcRole::Merchant);
	TestTrue(TEXT("merchant NPC can trade"), MerchantNpc->CanTrade());
	TestFalse(TEXT("merchant NPC does not offer quest"), MerchantNpc->CanOfferQuest());

	UGameInstance* TestGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	Subsystem->OpenWorldMap();
	Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());

	QuestNpc->SetMVPSubsystemForTest(Subsystem);
	QuestNpc->NotifyActorBeginOverlap(Player);
	TestTrue(TEXT("quest NPC overlap focuses player interaction"), Player->GetInteractionComponent()->GetFocusedActor() == QuestNpc);
	MerchantNpc->NotifyActorBeginOverlap(Player);
	TestTrue(TEXT("later merchant overlap becomes focused"), Player->GetInteractionComponent()->GetFocusedActor() == MerchantNpc);
	MerchantNpc->NotifyActorEndOverlap(Player);
	TestTrue(TEXT("ending merchant overlap restores quest focus"), Player->GetInteractionComponent()->GetFocusedActor() == QuestNpc);
	Player->Interact();
	TestEqual(TEXT("F on quest NPC accepts quest"), Subsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("F on quest NPC starts follower"), QuestNpc->IsFollowerActive());
	TestTrue(TEXT("quest follower targets player"), QuestNpc->GetFollowTarget() == Player);
	TestTrue(TEXT("quest NPC records successful interaction"), QuestNpc->WasLastInteractionSuccessful());
	QuestNpc->NotifyActorEndOverlap(Player);
	TestNull(TEXT("quest NPC end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

	MerchantNpc->SetMVPSubsystemForTest(Subsystem);
	MerchantNpc->NotifyActorBeginOverlap(Player);
	const int32 GoldBeforeMerchantF = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeMerchantF = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	Player->Interact();
	TestEqual(TEXT("F on merchant spends one powder price"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeMerchantF - 10);
	TestEqual(TEXT("F on merchant adds healing powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeMerchantF + 1);
	TestTrue(TEXT("merchant NPC records successful buy"), MerchantNpc->WasLastInteractionSuccessful());
	Subsystem->GetMutableRuntimeState().PlayerGold = 0;
	const int32 PowderBeforeFailedMerchantF = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	Player->Interact();
	TestEqual(TEXT("merchant no-gold path keeps gold"), Subsystem->GetRuntimeState().PlayerGold, 0);
	TestEqual(TEXT("merchant no-gold path keeps inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeFailedMerchantF);
	TestFalse(TEXT("merchant NPC records failed buy"), MerchantNpc->WasLastInteractionSuccessful());
	MerchantNpc->NotifyActorEndOverlap(Player);
	TestNull(TEXT("merchant NPC end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

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
