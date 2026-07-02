#include "Interaction/GameXXKInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameXXKMVPRules.h"
#include "GameFramework/InputSettings.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKTownExitActor.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKTownNpcActor.h"
#include "Town/GameXXKTownPlayerPawn.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	struct FTownDirectionSpec
	{
		EGameXXKTownFacingDirection Direction;
		const TCHAR* Name;
		const TCHAR* Path;
	};

	constexpr EGameXXKTownFacingDirection TownFacingSouth = EGameXXKTownFacingDirection::South;
	constexpr EGameXXKTownFacingDirection TownFacingSouthWest = EGameXXKTownFacingDirection::SouthWest;
	constexpr EGameXXKTownFacingDirection TownFacingWest = EGameXXKTownFacingDirection::West;
	constexpr EGameXXKTownFacingDirection TownFacingNorthWest = EGameXXKTownFacingDirection::NorthWest;
	constexpr EGameXXKTownFacingDirection TownFacingNorth = EGameXXKTownFacingDirection::North;
	constexpr EGameXXKTownFacingDirection TownFacingNorthEast = EGameXXKTownFacingDirection::NorthEast;
	constexpr EGameXXKTownFacingDirection TownFacingEast = EGameXXKTownFacingDirection::East;
	constexpr EGameXXKTownFacingDirection TownFacingSouthEast = EGameXXKTownFacingDirection::SouthEast;

	const FTownDirectionSpec TownDirectionSpecs[] = {
		{ TownFacingSouth, TEXT("South"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_South.FB_Hero_Walk_South") },
		{ TownFacingSouthWest, TEXT("SouthWest"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_SouthWest.FB_Hero_Walk_SouthWest") },
		{ TownFacingWest, TEXT("West"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_West.FB_Hero_Walk_West") },
		{ TownFacingNorthWest, TEXT("NorthWest"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_NorthWest.FB_Hero_Walk_NorthWest") },
		{ TownFacingNorth, TEXT("North"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_North.FB_Hero_Walk_North") },
		{ TownFacingNorthEast, TEXT("NorthEast"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_NorthEast.FB_Hero_Walk_NorthEast") },
		{ TownFacingEast, TEXT("East"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_East.FB_Hero_Walk_East") },
		{ TownFacingSouthEast, TEXT("SouthEast"), TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_SouthEast.FB_Hero_Walk_SouthEast") },
	};

	int32 TownDirectionIndex(EGameXXKTownFacingDirection Direction)
	{
		return static_cast<int32>(Direction);
	}

	bool ExpectTownFacing(FAutomationTestBase& Test, AGameXXKTownPlayerPawn* Player, EGameXXKTownFacingDirection ExpectedDirection, const TCHAR* Context)
	{
		const int64 ActualDirection = Player ? static_cast<int64>(Player->GetTownFacingDirection()) : INDEX_NONE;
		return Test.TestEqual(FString::Printf(TEXT("%s facing direction"), Context), ActualDirection, static_cast<int64>(ExpectedDirection));
	}

	bool HasAxisMapping(const TArray<FInputAxisKeyMapping>& Mappings, FKey Key, float Scale)
	{
		return Mappings.ContainsByPredicate([Key, Scale](const FInputAxisKeyMapping& Mapping)
		{
			return Mapping.Key == Key && FMath::IsNearlyEqual(Mapping.Scale, Scale);
		});
	}
}

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
	TArray<FInputAxisKeyMapping> HorizontalAxisMappings;
	TArray<FInputAxisKeyMapping> VerticalAxisMappings;
	UInputSettings::GetInputSettings()->GetAxisMappingByName(TEXT("TownMoveHorizontal"), HorizontalAxisMappings);
	UInputSettings::GetInputSettings()->GetAxisMappingByName(TEXT("TownMoveVertical"), VerticalAxisMappings);
	TestTrue(TEXT("TownMoveHorizontal axis maps A left"), HasAxisMapping(HorizontalAxisMappings, EKeys::A, -1.0f));
	TestTrue(TEXT("TownMoveHorizontal axis maps D right"), HasAxisMapping(HorizontalAxisMappings, EKeys::D, 1.0f));
	TestTrue(TEXT("TownMoveHorizontal axis maps Left arrow"), HasAxisMapping(HorizontalAxisMappings, EKeys::Left, -1.0f));
	TestTrue(TEXT("TownMoveHorizontal axis maps Right arrow"), HasAxisMapping(HorizontalAxisMappings, EKeys::Right, 1.0f));
	TestTrue(TEXT("TownMoveHorizontal axis maps gamepad left stick X"), HasAxisMapping(HorizontalAxisMappings, EKeys::Gamepad_LeftX, 1.0f));
	TestTrue(TEXT("TownMoveVertical axis maps S backward"), HasAxisMapping(VerticalAxisMappings, EKeys::S, -1.0f));
	TestTrue(TEXT("TownMoveVertical axis maps W forward"), HasAxisMapping(VerticalAxisMappings, EKeys::W, 1.0f));
	TestTrue(TEXT("TownMoveVertical axis maps Down arrow"), HasAxisMapping(VerticalAxisMappings, EKeys::Down, -1.0f));
	TestTrue(TEXT("TownMoveVertical axis maps Up arrow"), HasAxisMapping(VerticalAxisMappings, EKeys::Up, 1.0f));
	TestTrue(TEXT("TownMoveVertical axis maps gamepad left stick Y"), HasAxisMapping(VerticalAxisMappings, EKeys::Gamepad_LeftY, 1.0f));

	AGameXXKHeroCharacter* HeroCharacter = NewObject<AGameXXKHeroCharacter>();
	TestNotNull(TEXT("hero character owns interaction component"), HeroCharacter->GetInteractionComponent());
	TestNotNull(TEXT("hero character owns Character movement component"), HeroCharacter->GetMovementComponent());
	TestNotNull(TEXT("hero character has explicit Pawn overlap collision"), HeroCharacter->GetTownCollisionComponent());
	TestTrue(TEXT("hero W key is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::W));
	TestTrue(TEXT("hero A key is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::A));
	TestTrue(TEXT("hero S key is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::S));
	TestTrue(TEXT("hero D key is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::D));
	TestTrue(TEXT("hero Up arrow is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::Up));
	TestTrue(TEXT("hero Left arrow is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::Left));
	TestTrue(TEXT("hero Down arrow is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::Down));
	TestTrue(TEXT("hero Right arrow is accepted movement input"), HeroCharacter->IsSupportedMovementKey(EKeys::Right));
	TestFalse(TEXT("hero E key is not town interaction"), HeroCharacter->IsInteractionKey(EKeys::E));
	TestTrue(TEXT("hero F key is town interaction"), HeroCharacter->IsInteractionKey(EKeys::F));
	TestTrue(TEXT("hero has Paper2D visual shell"), HeroCharacter->HasTownVisual());
	TestEqual(TEXT("hero default town flipbook path points at hero South walk"), HeroCharacter->GetDefaultTownFlipbookPath().ToString(), FString(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_South.FB_Hero_Walk_South")));
	TestEqual(TEXT("hero input binds movement press/release keys, movement axes, plus F"), HeroCharacter->CountTownInputBindingsForTest(), 19);

	TestTrue(TEXT("player has Paper2D visual shell"), Player->HasTownVisual());
	TestEqual(TEXT("player default town flipbook path points at hero South walk"), Player->GetDefaultTownFlipbookPath().ToString(), FString(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_South.FB_Hero_Walk_South")));
	TestEqual(TEXT("player exposes default town flipbook path as string for automation"), Player->GetDefaultTownFlipbookPathString(), FString(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_South.FB_Hero_Walk_South")));
	TestTrue(TEXT("player loads project hero South walk flipbook by default"), Player->HasAssignedTownFlipbook());
	UPaperFlipbook* TestFlipbook = NewObject<UPaperFlipbook>(Player);
	Player->SetDefaultTownFlipbookForTest(TestFlipbook);
	TestEqual(TEXT("player stores default town flipbook"), Player->GetDefaultTownFlipbook(), TestFlipbook);
	TestEqual(TEXT("player applies default town flipbook to visual component"), Player->GetCurrentTownFlipbook(), TestFlipbook);
	TestTrue(TEXT("player reports assigned town flipbook"), Player->HasAssignedTownFlipbook());
	TestEqual(TEXT("town input binds movement press/release keys, movement axes, plus F"), Player->CountTownInputBindingsForTest(), 19);

	AGameXXKTownPlayerPawn* FacingPlayer = NewObject<AGameXXKTownPlayerPawn>();
	UPaperFlipbook* DirectionFlipbooks[UE_ARRAY_COUNT(TownDirectionSpecs)] = {};
	for (const FTownDirectionSpec& Spec : TownDirectionSpecs)
	{
		const FSoftObjectPath DirectionPath = FacingPlayer->GetTownFlipbookPathForDirection(Spec.Direction);
		TestEqual(FString::Printf(TEXT("player %s walk flipbook path"), Spec.Name), DirectionPath.ToString(), FString(Spec.Path));

		DirectionFlipbooks[TownDirectionIndex(Spec.Direction)] = NewObject<UPaperFlipbook>(FacingPlayer);
		FacingPlayer->SetTownDirectionFlipbookForTest(Spec.Direction, DirectionFlipbooks[TownDirectionIndex(Spec.Direction)]);
	}

	ExpectTownFacing(*this, FacingPlayer, TownFacingSouth, TEXT("initial player"));
	TestEqual(TEXT("initial player uses South direction flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingSouth)]);
	FacingPlayer->MoveForwardPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorth, TEXT("W pressed"));
	TestEqual(TEXT("W pressed applies North flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	FacingPlayer->MoveRightPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorthEast, TEXT("W+D pressed"));
	TestEqual(TEXT("W+D pressed applies NorthEast flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorthEast)]);
	FacingPlayer->MoveForwardReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingEast, TEXT("D remains pressed"));
	TestEqual(TEXT("D remains pressed applies East flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingEast)]);
	FacingPlayer->MoveRightReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingEast, TEXT("all movement released after East"));
	TestEqual(TEXT("all movement released preserves East flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingEast)]);
	FacingPlayer->MoveBackwardPressed();
	FacingPlayer->MoveLeftPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingSouthWest, TEXT("S+A pressed"));
	TestEqual(TEXT("S+A pressed applies SouthWest flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingSouthWest)]);
	FacingPlayer->MoveBackwardReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingWest, TEXT("A remains pressed"));
	TestEqual(TEXT("A remains pressed applies West flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingWest)]);
	FacingPlayer->MoveForwardPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorthWest, TEXT("W+A pressed"));
	TestEqual(TEXT("W+A pressed applies NorthWest flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorthWest)]);
	FacingPlayer->MoveLeftReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorth, TEXT("W remains pressed"));
	TestEqual(TEXT("W remains pressed applies North flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	FacingPlayer->MoveForwardReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorth, TEXT("all movement released after North"));
	TestEqual(TEXT("all movement released preserves North flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	FacingPlayer->MoveRightPressed();
	FacingPlayer->MoveRightPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingEast, TEXT("D+Right pressed together"));
	FacingPlayer->MoveRightReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingEast, TEXT("Right remains after D released"));
	TestEqual(TEXT("Right remains after D released preserves East flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingEast)]);
	FacingPlayer->MoveRightReleased();
	ExpectTownFacing(*this, FacingPlayer, TownFacingEast, TEXT("D+Right both released"));
	FacingPlayer->MoveRightPressed();
	FacingPlayer->MoveRightPressed();
	FacingPlayer->ResetTownMovementInput();
	FacingPlayer->MoveForwardPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingNorth, TEXT("reset clears stale horizontal key state"));
	TestEqual(TEXT("reset clears stale horizontal key state flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	FacingPlayer->MoveForwardReleased();
	FacingPlayer->MoveRightPressed();
	FacingPlayer->MoveBackwardPressed();
	ExpectTownFacing(*this, FacingPlayer, TownFacingSouthEast, TEXT("S+D pressed"));
	TestEqual(TEXT("S+D pressed applies SouthEast flipbook"), FacingPlayer->GetCurrentTownFlipbook(), DirectionFlipbooks[TownDirectionIndex(TownFacingSouthEast)]);

	AGameXXKTownPlayerPawn* AxisPlayer = NewObject<AGameXXKTownPlayerPawn>();
	UPaperFlipbook* AxisFlipbooks[UE_ARRAY_COUNT(TownDirectionSpecs)] = {};
	for (const FTownDirectionSpec& Spec : TownDirectionSpecs)
	{
		AxisFlipbooks[TownDirectionIndex(Spec.Direction)] = NewObject<UPaperFlipbook>(AxisPlayer);
		AxisPlayer->SetTownDirectionFlipbookForTest(Spec.Direction, AxisFlipbooks[TownDirectionIndex(Spec.Direction)]);
	}

	AxisPlayer->MoveHorizontal(1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingEast, TEXT("horizontal movement input"));
	TestEqual(TEXT("horizontal movement input applies East flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingEast)]);
	AxisPlayer->MoveHorizontal(0.05f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingEast, TEXT("small horizontal axis drift preserves facing"));
	TestEqual(TEXT("small horizontal axis drift preserves East flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingEast)]);
	AxisPlayer->MoveVertical(1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingNorth, TEXT("vertical movement input after deadzone horizontal drift"));
	TestEqual(TEXT("vertical movement input after deadzone horizontal drift applies North flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	AxisPlayer->MoveHorizontal(1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingNorthEast, TEXT("horizontal plus vertical movement input"));
	TestEqual(TEXT("horizontal plus vertical movement input applies NorthEast flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingNorthEast)]);
	AxisPlayer->MoveHorizontal(0.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingNorth, TEXT("vertical movement input remains"));
	TestEqual(TEXT("vertical movement input applies North flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	AxisPlayer->MoveVertical(0.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingNorth, TEXT("zero movement input"));
	TestEqual(TEXT("zero movement input preserves North flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingNorth)]);
	AxisPlayer->MoveHorizontal(-1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingWest, TEXT("negative horizontal movement input"));
	TestEqual(TEXT("negative horizontal movement input applies West flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingWest)]);
	AxisPlayer->MoveVertical(-1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingSouthWest, TEXT("negative horizontal plus negative vertical movement input"));
	TestEqual(TEXT("negative horizontal plus negative vertical movement input applies SouthWest flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingSouthWest)]);
	AxisPlayer->MoveHorizontal(1.0f);
	ExpectTownFacing(*this, AxisPlayer, TownFacingSouthEast, TEXT("positive horizontal plus negative vertical movement input"));
	TestEqual(TEXT("positive horizontal plus negative vertical movement input applies SouthEast flipbook"), AxisPlayer->GetCurrentTownFlipbook(), AxisFlipbooks[TownDirectionIndex(TownFacingSouthEast)]);

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
	const FString NpcAutosaveSlot = UGameXXKMVPSubsystem::GetDefaultSaveSlotName();
	UGameplayStatics::DeleteGameInSlot(NpcAutosaveSlot, 0);
	Subsystem->OpenWorldMap();
	Subsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());

	AGameXXKTownExitActor* TownExit = NewObject<AGameXXKTownExitActor>();
	TownExit->SetMVPSubsystemForTest(Subsystem);
	TestNotNull(TEXT("town exit has interaction area"), TownExit->GetInteractionArea());
	TestNotNull(TEXT("town exit has visible feedback text"), TownExit->GetFeedbackTextComponent());
	TestEqual(TEXT("town exit area is query-only overlap"), TownExit->GetInteractionArea()->GetCollisionEnabled(), ECollisionEnabled::QueryOnly);
	TestEqual(TEXT("town exit area overlaps Pawn channel"), TownExit->GetInteractionArea()->GetCollisionResponseToChannel(ECC_Pawn), ECollisionResponse::ECR_Overlap);
	TestTrue(TEXT("town exit area generates overlap events"), TownExit->GetInteractionArea()->GetGenerateOverlapEvents());
	TownExit->NotifyActorBeginOverlap(Player);
	TestTrue(TEXT("town exit overlap focuses player interaction"), Player->GetInteractionComponent()->GetFocusedActor() == TownExit);
	Player->Interact();
	TestEqual(TEXT("exit before quest keeps player in town"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::Town);
	TestFalse(TEXT("exit before quest records failed interaction"), TownExit->WasLastInteractionSuccessful());
	TestFalse(TEXT("exit before quest does not start dungeon"), Subsystem->GetRuntimeState().bDungeonActive);
	TestEqual(TEXT("exit before quest displays quest requirement"), TownExit->GetFeedbackTextComponent()->Text.ToString(), FString(TEXT("请先接任务")));
	TownExit->NotifyActorEndOverlap(Player);
	TestNull(TEXT("town exit end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

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
	UGameXXKMVPSubsystem* ReloadedAfterQuestF = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("F quest interaction autosaves default slot"), ReloadedAfterQuestF->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestEqual(TEXT("F quest autosave persists accepted quest"), ReloadedAfterQuestF->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("F quest autosave restores follower join state"), ReloadedAfterQuestF->GetRuntimeState().bFollowerJoined);
	QuestNpc->NotifyActorEndOverlap(Player);
	TestNull(TEXT("quest NPC end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

	TownExit->NotifyActorBeginOverlap(Player);
	TestTrue(TEXT("accepted quest exit overlap focuses player interaction"), Player->GetInteractionComponent()->GetFocusedActor() == TownExit);
	Player->Interact();
	TestTrue(TEXT("accepted quest exit records successful interaction"), TownExit->WasLastInteractionSuccessful());
	TestEqual(TEXT("accepted quest exit opens dungeon map"), Subsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("accepted quest exit starts dungeon"), Subsystem->GetRuntimeState().bDungeonActive);
	TestEqual(TEXT("accepted quest exit clears failure feedback"), TownExit->GetFeedbackTextComponent()->Text.ToString(), FString());
	Subsystem->FailDungeonToTown();
	TownExit->NotifyActorEndOverlap(Player);
	TestNull(TEXT("accepted quest exit end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

	MerchantNpc->SetMVPSubsystemForTest(Subsystem);
	MerchantNpc->NotifyActorBeginOverlap(Player);
	const int32 GoldBeforeMerchantF = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeMerchantF = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	Player->Interact();
	TestEqual(TEXT("F on merchant spends one powder price"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeMerchantF - 10);
	TestEqual(TEXT("F on merchant adds healing powder"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeMerchantF + 1);
	TestTrue(TEXT("merchant NPC records successful buy"), MerchantNpc->WasLastInteractionSuccessful());
	UGameXXKMVPSubsystem* ReloadedAfterMerchantF = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("F merchant interaction autosaves default slot"), ReloadedAfterMerchantF->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestEqual(TEXT("F merchant autosave persists spent gold"), ReloadedAfterMerchantF->GetRuntimeState().PlayerGold, GoldBeforeMerchantF - 10);
	TestEqual(TEXT("F merchant autosave keeps inventory out of save scope"), UGameXXKMVPRules::GetItemCount(ReloadedAfterMerchantF->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), 0);
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

	UGameplayStatics::DeleteGameInSlot(NpcAutosaveSlot, 0);
	return true;
}

#endif
