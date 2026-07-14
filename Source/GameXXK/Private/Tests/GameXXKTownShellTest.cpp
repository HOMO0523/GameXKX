#include "Interaction/GameXXKInteractionComponent.h"
#include "Components/BoxComponent.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SphereComponent.h"
#include "Components/TextRenderComponent.h"
#include "GameXXKMVPRules.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/Material.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "MVP/GameXXKMVPGameMode.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "Town/GameXXKHeroCharacter.h"
#include "Town/GameXXKPlayerOcclusionRevealComponent.h"
#include "Town/GameXXKOcclusionMaterialMap.h"
#include "Town/GameXXKTownExitActor.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "HAL/PlatformProcess.h"
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

	FString HeroIdleFlipbookPath(const TCHAR* DirectionName)
	{
		return FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Idle_%s.FB_Hero_Idle_%s"), DirectionName, DirectionName);
	}

	FString HeroWalkFlipbookPath(const TCHAR* DirectionName)
	{
		return FString::Printf(TEXT("/Game/GameXXK/Characters/Hero/Flipbooks/FB_Hero_Walk_%s.FB_Hero_Walk_%s"), DirectionName, DirectionName);
	}

	int32 TownDirectionIndex(EGameXXKTownFacingDirection Direction)
	{
		return static_cast<int32>(Direction);
	}

	template <typename TTownPlayer>
	bool ExpectTownFacing(FAutomationTestBase& Test, TTownPlayer* Player, EGameXXKTownFacingDirection ExpectedDirection, const TCHAR* Context)
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
	TestEqual(TEXT("hero capsule supports physical walking collision"), HeroCharacter->GetTownCollisionComponent()->GetCollisionEnabled(), ECollisionEnabled::QueryAndPhysics);
	TestEqual(TEXT("hero capsule blocks static gameplay proxies"), HeroCharacter->GetTownCollisionComponent()->GetCollisionResponseToChannel(ECC_WorldStatic), ECollisionResponse::ECR_Block);
	TestTrue(TEXT("hero movement uses gravity for 3D town terrain"), HeroCharacter->GetCharacterMovement() && HeroCharacter->GetCharacterMovement()->GravityScale > 0.0f);
	TestEqual(TEXT("hero defaults to walking movement"), HeroCharacter->GetCharacterMovement()->DefaultLandMovementMode, MOVE_Walking);
	TestEqual(
		TEXT("hero is not constrained to a fixed world-Z plane"),
		HeroCharacter->GetCharacterMovement()->ConstrainDirectionToPlane(FVector::UpVector),
		FVector::UpVector);
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
	const UPaperFlipbookComponent* HeroVisual = HeroCharacter->GetTownVisualComponent();
	TestNotNull(TEXT("hero exposes Paper2D visual component"), HeroVisual);
	TestTrue(TEXT("hero visual writes Custom Depth for occlusion silhouette gating"),
		HeroVisual && HeroVisual->bRenderCustomDepth);
	TestEqual(TEXT("hero visual uses the dedicated occlusion stencil"),
		HeroVisual ? HeroVisual->CustomDepthStencilValue : INDEX_NONE, 13);
	UPaperFlipbookComponent* RevealVisual = HeroCharacter->GetOcclusionRevealVisualComponent();
	UGameXXKPlayerOcclusionRevealComponent* RevealController = HeroCharacter->GetOcclusionRevealComponent();
	TestNotNull(TEXT("hero owns a Paper2D occlusion reveal visual"), RevealVisual);
	TestNotNull(TEXT("hero owns an occlusion reveal controller"), RevealController);
	TestEqual(TEXT("cutout starts with no modified components"), RevealController ? RevealController->GetModifiedComponentCount() : INDEX_NONE, 0);
	TestEqual(TEXT("scene reveal samples center plus eight circle points"), RevealController ? RevealController->GetOcclusionSampleCount() : INDEX_NONE, 9);
	TestTrue(TEXT("scene reveal can peel multiple blocking layers per ray"), RevealController && RevealController->GetMaxOcclusionLayers() >= 4);
	TestEqual(TEXT("scene reveal keeps the player-facing visual radius"), RevealController ? RevealController->GetRevealRadiusNormalized() : -1.0f, 0.18f);
	TestTrue(TEXT("scene reveal traces a tighter player-body footprint than its visual halo"),
		RevealController && RevealController->GetOcclusionDetectionRadiusNormalized() < RevealController->GetRevealRadiusNormalized());
	TestTrue(TEXT("cutout restores original material slots"), RevealController && RevealController->RestoresOriginalMaterials());
	if (RevealVisual)
	{
		TestEqual(TEXT("reveal visual has no collision"), RevealVisual->GetCollisionEnabled(), ECollisionEnabled::NoCollision);
		TestFalse(TEXT("reveal visual starts hidden"), RevealVisual->IsVisible());
		TestFalse(TEXT("reveal visual does not cast shadows"), RevealVisual->CastShadow);
	}
	if (RevealController)
	{
		TestEqual(TEXT("reveal samples at 20 Hz"), RevealController->GetTraceInterval(), 0.05f);
		TestEqual(TEXT("reveal trace uses a stable sphere"), RevealController->GetTraceRadius(), 36.0f);
		TestEqual(TEXT("reveal waits before activation"), RevealController->GetActivationDelay(), 0.08f);
		TestEqual(TEXT("reveal fades after visibility returns"), RevealController->GetReleaseDuration(), 0.22f);
		RevealController->UpdateMaterialParametersForTest(
			FVector::ZeroVector, FVector::ForwardVector, FVector(500.0f, 0.0f, 0.0f));
		TestEqual(TEXT("occlusion stores hero camera-forward depth"),
			RevealController->GetLastHeroViewDepthForTest(), 500.0f);
		TestTrue(TEXT("occlusion has positive front-depth bias"),
			RevealController->GetOcclusionDepthBias() > 0.0f);
		UPaperFlipbook* RevealSyncFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
		HeroCharacter->GetTownVisualComponent()->SetFlipbook(RevealSyncFlipbook);
		RevealController->SetOcclusionOverrideForTest(true);
		RevealController->UpdateRevealForTest(0.10f);
		HeroCharacter->SynchronizeOcclusionRevealVisualForTest();
		TestTrue(TEXT("continuous obstruction enables material cutout state"), RevealController->IsRevealActive());
		TestFalse(TEXT("duplicate Paper2D renderer stays disabled in final path"), RevealVisual && RevealVisual->IsVisible());

		RevealController->SetOcclusionOverrideForTest(false);
		RevealController->UpdateRevealForTest(0.23f);
		TestFalse(TEXT("clear view disables material cutout after release"), RevealController->IsRevealActive());

		UMaterialInterface* OriginalRoof = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asian_Village/materials/building_materials/M_thatched_roof.M_thatched_roof"));
		UMaterialInterface* ExpectedCutout = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/M_thatched_roof_Cutout.M_thatched_roof_Cutout"));
		UMaterial* UnmappedMaterial = NewObject<UMaterial>(HeroCharacter);
		UMaterialInterface* UnmappedInterface = UnmappedMaterial;
		TestNotNull(TEXT("pilot original roof material loads"), OriginalRoof);
		TestNotNull(TEXT("pilot cutout roof material loads"), ExpectedCutout);
		FGameXXKOcclusionMaterialMap MaterialMap;
		TestEqual(TEXT("pilot roof resolves to project cutout"), MaterialMap.Resolve(OriginalRoof), ExpectedCutout);
		UMaterialInterface* OriginalRoofBeam = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asian_Village/materials/building_materials/base_materials/MI_wooden_board_03_Inst.MI_wooden_board_03_Inst"));
		UMaterialInterface* ExpectedBeamCutout = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_wooden_board_03_Inst_Cutout.MI_wooden_board_03_Inst_Cutout"));
		TestNotNull(TEXT("roof beam original material loads"), OriginalRoofBeam);
		TestNotNull(TEXT("roof beam cutout material loads"), ExpectedBeamCutout);
		TestEqual(TEXT("roof beam resolves with every component slot"), MaterialMap.Resolve(OriginalRoofBeam), ExpectedBeamCutout);
		UMaterialInterface* OriginalBambooWall = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asian_Village/materials/building_materials/base_materials/MI_bamboo_wall_01_Inst.MI_bamboo_wall_01_Inst"));
		UMaterialInterface* ExpectedBambooCutout = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_bamboo_wall_01_Inst_Cutout.MI_bamboo_wall_01_Inst_Cutout"));
		UMaterialInterface* OriginalBuildingWood = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Asian_Village/materials/building_materials/MI_building_wood_01_Inst.MI_building_wood_01_Inst"));
		UMaterialInterface* ExpectedBuildingWoodCutout = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/GameXXK/Materials/Occlusion/AsianVillage/MI_building_wood_01_Inst_Cutout.MI_building_wood_01_Inst_Cutout"));
		TestNotNull(TEXT("bamboo wall cutout material loads"), ExpectedBambooCutout);
		TestNotNull(TEXT("building wood cutout material loads"), ExpectedBuildingWoodCutout);
		TestEqual(TEXT("bamboo wall resolves with every component slot"), MaterialMap.Resolve(OriginalBambooWall), ExpectedBambooCutout);
		TestEqual(TEXT("building wood resolves with every component slot"), MaterialMap.Resolve(OriginalBuildingWood), ExpectedBuildingWoodCutout);
		TestNull(TEXT("unmapped material is never replaced"), MaterialMap.Resolve(UnmappedInterface));

		UStaticMesh* MaterialTestMesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
		TestNotNull(TEXT("material test mesh loads"), MaterialTestMesh);
		UStaticMeshComponent* MaterialTestComponent = NewObject<UStaticMeshComponent>(HeroCharacter);
		MaterialTestComponent->SetStaticMesh(MaterialTestMesh);
		MaterialTestComponent->SetMaterial(0, OriginalRoof);
		RevealController->ApplyCutoutMaterialsForTest(MaterialTestComponent);
		TestEqual(TEXT("mapped slot receives cutout"), MaterialTestComponent->GetMaterial(0), ExpectedCutout);
		RevealController->RestoreAllModifiedComponentsForTest();
		TestEqual(TEXT("restore returns mapped slot pointer-identically"), MaterialTestComponent->GetMaterial(0), OriginalRoof);

		UStaticMeshComponent* UnmappedTestComponent = NewObject<UStaticMeshComponent>(HeroCharacter);
		UnmappedTestComponent->SetStaticMesh(MaterialTestMesh);
		UnmappedTestComponent->SetMaterial(0, UnmappedInterface);
		RevealController->ApplyCutoutMaterialsForTest(UnmappedTestComponent);
		TestEqual(TEXT("unmapped slot remains original"), UnmappedTestComponent->GetMaterial(0), UnmappedInterface);
		RevealController->RestoreAllModifiedComponentsForTest();
		TestEqual(TEXT("restore preserves unmapped slot pointer-identically"), UnmappedTestComponent->GetMaterial(0), UnmappedInterface);
	}
	if (HeroVisual)
	{
		TestEqual(TEXT("hero Paper2D visual uses tuned HD2D land offset"), static_cast<float>(HeroVisual->GetRelativeLocation().Z), -80.0f);
		TestEqual(TEXT("hero Paper2D visual faces Ocean-style top-down camera"), static_cast<float>(HeroVisual->GetRelativeRotation().Yaw), 90.0f);
		TestEqual(TEXT("hero Paper2D visual uses tuned HD2D roll"), static_cast<float>(HeroVisual->GetRelativeRotation().Roll), -30.0f);
		TestEqual(TEXT("hero Paper2D visual uses tuned HD2D scale"), HeroVisual->GetRelativeScale3D(), FVector(1.0f, 1.0f, 1.0f));
	}
	TestEqual(TEXT("hero default town flipbook path points at hero South idle"), HeroCharacter->GetDefaultTownFlipbookPath().ToString(), HeroIdleFlipbookPath(TEXT("South")));
	TestEqual(TEXT("hero South idle path is explicit"), HeroCharacter->GetTownIdleFlipbookPathForDirection(TownFacingSouth).ToString(), HeroIdleFlipbookPath(TEXT("South")));
	TestEqual(TEXT("hero South walk path is explicit"), HeroCharacter->GetTownWalkFlipbookPathForDirection(TownFacingSouth).ToString(), HeroWalkFlipbookPath(TEXT("South")));
	TestFalse(TEXT("hero starts in idle visual state"), HeroCharacter->IsTownMoving());
	TestEqual(TEXT("hero input binds movement press/release keys, movement axes, plus F"), HeroCharacter->CountTownInputBindingsForTest(), 19);
	const USpringArmComponent* HeroCameraBoom = HeroCharacter->GetCameraBoom();
	const UCameraComponent* HeroCamera = HeroCharacter->GetTopDownCameraComponent();
	TestNotNull(TEXT("hero owns Ocean-style camera boom"), HeroCameraBoom);
	TestNotNull(TEXT("hero owns Ocean-style top-down camera"), HeroCamera);
	if (HeroCameraBoom)
	{
		TestTrue(TEXT("hero camera boom uses absolute rotation like Ocean"), HeroCameraBoom->IsUsingAbsoluteRotation());
		TestEqual(TEXT("hero camera boom has Ocean-style arm length"), HeroCameraBoom->TargetArmLength, 800.0f);
		TestEqual(TEXT("hero camera boom has Ocean-style pitch"), static_cast<float>(HeroCameraBoom->GetRelativeRotation().Pitch), -60.0f);
		TestFalse(TEXT("hero camera boom ignores collision like Ocean"), HeroCameraBoom->bDoCollisionTest);
	}
	if (HeroCamera)
	{
		TestEqual(TEXT("hero camera remains perspective like Ocean"), HeroCamera->ProjectionMode, ECameraProjectionMode::Perspective);
		TestFalse(TEXT("hero top-down camera ignores pawn control rotation"), HeroCamera->bUsePawnControlRotation);
	}

	UClass* HeroBlueprintClass = LoadClass<AGameXXKHeroCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Hero/BP_HeroCharacter.BP_HeroCharacter_C"), nullptr, LOAD_NoWarn);
	TestNotNull(TEXT("editable BP_HeroCharacter class loads for camera verification"), HeroBlueprintClass);
	if (HeroBlueprintClass)
	{
		AGameXXKHeroCharacter* HeroBlueprintDefault = Cast<AGameXXKHeroCharacter>(HeroBlueprintClass->GetDefaultObject());
		TestNotNull(TEXT("editable BP_HeroCharacter has a hero CDO"), HeroBlueprintDefault);
		if (HeroBlueprintDefault)
		{
			const USpringArmComponent* HeroBlueprintCameraBoom = HeroBlueprintDefault->GetCameraBoom();
			const UCameraComponent* HeroBlueprintCamera = HeroBlueprintDefault->GetTopDownCameraComponent();
			TestNotNull(TEXT("BP_HeroCharacter inherits editable Ocean-style camera boom"), HeroBlueprintCameraBoom);
			TestNotNull(TEXT("BP_HeroCharacter inherits editable Ocean-style top-down camera"), HeroBlueprintCamera);
			if (HeroBlueprintCameraBoom)
			{
				TestTrue(TEXT("BP_HeroCharacter camera boom keeps Ocean-style absolute rotation"), HeroBlueprintCameraBoom->IsUsingAbsoluteRotation());
				TestEqual(TEXT("BP_HeroCharacter preserves the user-tuned camera arm length"), HeroBlueprintCameraBoom->TargetArmLength, 1000.0f);
				TestEqual(TEXT("BP_HeroCharacter preserves the user-tuned camera pitch"), static_cast<float>(HeroBlueprintCameraBoom->GetRelativeRotation().Pitch), -30.0f);
			}
			if (HeroBlueprintCamera)
			{
				TestEqual(TEXT("BP_HeroCharacter camera remains perspective like Ocean"), HeroBlueprintCamera->ProjectionMode, ECameraProjectionMode::Perspective);
			}
		}
	}

	UPaperFlipbook* HeroEastFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
	UPaperFlipbook* HeroNorthFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
	UPaperFlipbook* HeroEastIdleFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
	UPaperFlipbook* HeroNorthEastFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
	UPaperFlipbook* HeroNorthEastIdleFlipbook = NewObject<UPaperFlipbook>(HeroCharacter);
	HeroCharacter->SetTownWalkDirectionFlipbookForTest(TownFacingEast, HeroEastFlipbook);
	HeroCharacter->SetTownWalkDirectionFlipbookForTest(TownFacingNorth, HeroNorthFlipbook);
	HeroCharacter->SetTownIdleDirectionFlipbookForTest(TownFacingEast, HeroEastIdleFlipbook);
	HeroCharacter->SetTownWalkDirectionFlipbookForTest(TownFacingNorthEast, HeroNorthEastFlipbook);
	HeroCharacter->SetTownIdleDirectionFlipbookForTest(TownFacingNorthEast, HeroNorthEastIdleFlipbook);
	HeroCharacter->MoveRightPressed();
	ExpectTownFacing(*this, HeroCharacter, TownFacingEast, TEXT("hero D pressed"));
	TestTrue(TEXT("hero D pressed enters walk visual state"), HeroCharacter->IsTownMoving());
	TestEqual(TEXT("hero D pressed applies East flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroEastFlipbook);
	HeroCharacter->MoveRightReleased();
	TestFalse(TEXT("hero D released returns to idle visual state"), HeroCharacter->IsTownMoving());
	TestEqual(TEXT("hero D released preserves facing but applies East idle flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroEastIdleFlipbook);
	HeroCharacter->MoveVertical(1.0f);
	ExpectTownFacing(*this, HeroCharacter, TownFacingNorth, TEXT("hero positive vertical axis"));
	TestTrue(TEXT("hero positive vertical axis enters walk visual state"), HeroCharacter->IsTownMoving());
	TestEqual(TEXT("hero positive vertical axis applies North flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroNorthFlipbook);
	HeroCharacter->ResetTownMovementInput();
	HeroCharacter->MoveForwardPressed();
	HeroCharacter->MoveRightPressed();
	ExpectTownFacing(*this, HeroCharacter, TownFacingNorthEast, TEXT("hero W+D pressed"));
	TestEqual(TEXT("hero W+D pressed applies NorthEast walk flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroNorthEastFlipbook);
	HeroCharacter->MoveForwardReleased();
	HeroCharacter->MoveRightReleased();
	TestFalse(TEXT("hero W+D released together returns to idle visual state"), HeroCharacter->IsTownMoving());
	ExpectTownFacing(*this, HeroCharacter, TownFacingNorthEast, TEXT("hero W+D released together preserves diagonal facing"));
	TestEqual(TEXT("hero W+D released together applies NorthEast idle flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroNorthEastIdleFlipbook);
	HeroCharacter->ResetTownMovementInput();
	HeroCharacter->MoveForwardPressed();
	HeroCharacter->MoveRightPressed();
	ExpectTownFacing(*this, HeroCharacter, TownFacingNorthEast, TEXT("hero W+D pressed before staggered release"));
	HeroCharacter->MoveForwardReleased();
	FPlatformProcess::Sleep(0.08f);
	HeroCharacter->MoveRightReleased();
	TestFalse(TEXT("hero W then delayed D release returns to idle visual state"), HeroCharacter->IsTownMoving());
	ExpectTownFacing(*this, HeroCharacter, TownFacingEast, TEXT("hero W then delayed D release uses last cardinal facing"));
	TestEqual(TEXT("hero W then delayed D release applies East idle flipbook"), HeroCharacter->GetCurrentTownFlipbook(), HeroEastIdleFlipbook);
	HeroCharacter->ResetTownMovementInput();

	const FString HeroInteractionAutosaveSlot = UGameXXKMVPSubsystem::GetDefaultSaveSlotName();
	UGameplayStatics::DeleteGameInSlot(HeroInteractionAutosaveSlot, 0);
	UGameInstance* HeroInteractionGameInstance = NewObject<UGameInstance>();
	UGameXXKMVPSubsystem* HeroInteractionSubsystem = NewObject<UGameXXKMVPSubsystem>(HeroInteractionGameInstance);
	HeroInteractionSubsystem->OpenWorldMap();
	HeroInteractionSubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());
	AGameXXKTownNpcActor* HeroQuestNpc = NewObject<AGameXXKTownNpcActor>();
	HeroQuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
	HeroQuestNpc->SetMVPSubsystemForTest(HeroInteractionSubsystem);
	HeroCharacter->GetInteractionComponent()->SetFocusedActorForTest(HeroQuestNpc);
	HeroCharacter->Interact();
	TestEqual(TEXT("hero F on quest NPC accepts quest"), HeroInteractionSubsystem->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("hero F on quest NPC starts follower"), HeroQuestNpc->IsFollowerActive());
	TestTrue(TEXT("hero F quest follower targets hero character"), HeroQuestNpc->GetFollowTarget() == HeroCharacter);
	TestTrue(TEXT("hero F quest NPC records successful interaction"), HeroQuestNpc->WasLastInteractionSuccessful());
	UGameXXKMVPSubsystem* ReloadedHeroQuestF = NewObject<UGameXXKMVPSubsystem>(HeroInteractionGameInstance);
	TestFalse(TEXT("hero F quest interaction does not autosave default slot"), ReloadedHeroQuestF->LoadGameFromSlot(HeroInteractionAutosaveSlot, 0));
	AGameXXKTownExitActor* HeroTownExit = NewObject<AGameXXKTownExitActor>();
	HeroTownExit->SetMVPSubsystemForTest(HeroInteractionSubsystem);
	HeroCharacter->GetInteractionComponent()->SetFocusedActorForTest(HeroTownExit);
	HeroCharacter->Interact();
	TestEqual(TEXT("hero F accepted quest exit opens dungeon map"), HeroInteractionSubsystem->GetRuntimeState().Screen, EGameXXKScreen::DungeonMap);
	TestTrue(TEXT("hero F accepted quest exit starts dungeon"), HeroInteractionSubsystem->GetRuntimeState().bDungeonActive);
	TestTrue(TEXT("hero F town exit records successful interaction"), HeroTownExit->WasLastInteractionSuccessful());
	UGameplayStatics::DeleteGameInSlot(HeroInteractionAutosaveSlot, 0);

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
	UClass* MerchantVisualClass = LoadClass<AGameXXKHeroCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Merchant/BP_MerchantCharacter.BP_MerchantCharacter_C"), nullptr, LOAD_NoWarn);
	UClass* PersonVisualClass = LoadClass<AGameXXKHeroCharacter>(nullptr, TEXT("/Game/GameXXK/Characters/Follower/BP_NpcCharacter.BP_NpcCharacter_C"), nullptr, LOAD_NoWarn);
	TestNotNull(TEXT("merchant NPC visual uses copied hero blueprint"), MerchantVisualClass);
	TestNotNull(TEXT("person NPC visual uses copied hero blueprint"), PersonVisualClass);
	MerchantNpc->SetVisualCharacterClass(MerchantVisualClass);
	TestEqual(TEXT("merchant NPC stores merchant visual class"), MerchantNpc->GetVisualCharacterClass().Get(), MerchantVisualClass);
	QuestNpc->SetVisualCharacterClass(PersonVisualClass);
	TestEqual(TEXT("quest NPC stores person visual class"), QuestNpc->GetVisualCharacterClass().Get(), PersonVisualClass);
	AGameXXKMVPGameMode* MVPGameMode = NewObject<AGameXXKMVPGameMode>();
	TestEqual(TEXT("MVP GameMode configures merchant visual class"), MVPGameMode->GetMerchantTownNpcVisualClass().Get(), MerchantVisualClass);
	TestEqual(TEXT("MVP GameMode configures person visual class"), MVPGameMode->GetPersonTownNpcVisualClass().Get(), PersonVisualClass);

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
	TestTrue(TEXT("town exit interaction area is on the player-facing side of the north gate"), TownExit->GetInteractionArea()->GetRelativeLocation().Y < -1.0f);
	TestTrue(TEXT("town exit interaction area is wide enough to trigger before the gate mesh"), TownExit->GetInteractionArea()->GetUnscaledBoxExtent().Y >= 160.0f);
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
	TestFalse(TEXT("F quest interaction waits for manual save"), ReloadedAfterQuestF->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestTrue(TEXT("manual save after F quest writes default slot"), Subsystem->SaveCurrentGame(TEXT(""), 0));
	UGameXXKMVPSubsystem* ReloadedAfterQuestManualSave = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("manual quest save loads default slot"), ReloadedAfterQuestManualSave->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestEqual(TEXT("manual quest save persists accepted quest"), ReloadedAfterQuestManualSave->GetRuntimeState().QuestState, EGameXXKQuestState::Accepted);
	TestTrue(TEXT("manual quest save restores follower join state"), ReloadedAfterQuestManualSave->GetRuntimeState().bFollowerJoined);
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
	UGameplayStatics::DeleteGameInSlot(NpcAutosaveSlot, 0);

	MerchantNpc->SetMVPSubsystemForTest(Subsystem);
	MerchantNpc->NotifyActorBeginOverlap(Player);
	const int32 GoldBeforeMerchantF = Subsystem->GetRuntimeState().PlayerGold;
	const int32 PowderBeforeMerchantF = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	Player->Interact();
	TestEqual(TEXT("F on merchant opens shop without spending gold"), Subsystem->GetRuntimeState().PlayerGold, GoldBeforeMerchantF);
	TestEqual(TEXT("F on merchant opens shop without changing inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeMerchantF);
	TestEqual(TEXT("F on merchant opens trade panel"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::Trade);
	TestTrue(TEXT("merchant NPC records successful shop open"), MerchantNpc->WasLastInteractionSuccessful());
	UGameXXKMVPSubsystem* ReloadedAfterMerchantF = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestFalse(TEXT("F merchant interaction waits for manual save"), ReloadedAfterMerchantF->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestTrue(TEXT("manual save after merchant writes default slot"), Subsystem->SaveCurrentGame(TEXT(""), 0));
	UGameXXKMVPSubsystem* ReloadedAfterMerchantManualSave = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
	TestTrue(TEXT("manual merchant save loads default slot"), ReloadedAfterMerchantManualSave->LoadGameFromSlot(NpcAutosaveSlot, 0));
	TestEqual(TEXT("manual merchant save keeps gold until explicit trade"), ReloadedAfterMerchantManualSave->GetRuntimeState().PlayerGold, GoldBeforeMerchantF);
	TestEqual(TEXT("manual merchant save keeps inventory until explicit trade"), UGameXXKMVPRules::GetItemCount(ReloadedAfterMerchantManualSave->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeMerchantF);
	Subsystem->GetMutableRuntimeState().PlayerGold = 0;
	const int32 PowderBeforeFailedMerchantF = UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder());
	Player->Interact();
	TestEqual(TEXT("merchant no-gold path keeps gold"), Subsystem->GetRuntimeState().PlayerGold, 0);
	TestEqual(TEXT("merchant no-gold path keeps inventory"), UGameXXKMVPRules::GetItemCount(Subsystem->GetRuntimeState(), UGameXXKMVPRules::ItemHealingPowder()), PowderBeforeFailedMerchantF);
	TestTrue(TEXT("merchant no-gold path still opens shop"), MerchantNpc->WasLastInteractionSuccessful());
	MerchantNpc->NotifyActorEndOverlap(Player);
	TestNull(TEXT("merchant NPC end overlap clears focus"), Player->GetInteractionComponent()->GetFocusedActor());

	if (UWorld* ProximityWorld = GWorld)
	{
		AGameXXKTownPlayerPawn* NearbyPlayer = ProximityWorld->SpawnActor<AGameXXKTownPlayerPawn>(FVector::ZeroVector, FRotator::ZeroRotator);
		AGameXXKTownNpcActor* NearbyMerchantNpc = ProximityWorld->SpawnActor<AGameXXKTownNpcActor>(FVector(300.0f, 0.0f, 0.0f), FRotator::ZeroRotator);
		TestNotNull(TEXT("proximity fallback test spawns player"), NearbyPlayer);
		TestNotNull(TEXT("proximity fallback test spawns merchant"), NearbyMerchantNpc);
		if (NearbyPlayer && NearbyMerchantNpc)
		{
			NearbyMerchantNpc->SetNpcRole(EGameXXKTownNpcRole::Merchant);
			NearbyMerchantNpc->SetMVPSubsystemForTest(Subsystem);
			Subsystem->CloseTownPanel();
			TestNull(TEXT("proximity fallback starts without focused actor"), NearbyPlayer->GetInteractionComponent()->GetFocusedActor());
			NearbyPlayer->Interact();
			TestEqual(TEXT("F near merchant opens trade without exact overlap focus"), Subsystem->GetRuntimeState().TownPanelMode, EGameXXKTownPanelMode::Trade);
			TestTrue(TEXT("nearby merchant records proximity interaction"), NearbyMerchantNpc->WasLastInteractionSuccessful());
		}
		if (NearbyMerchantNpc)
		{
			NearbyMerchantNpc->Destroy();
		}
		if (NearbyPlayer)
		{
			NearbyPlayer->Destroy();
		}
	}

	if (UWorld* GatePriorityWorld = GWorld)
	{
		const FVector GatePriorityOrigin(750000.0f, 750000.0f, 100000.0f);
		AGameXXKTownPlayerPawn* GatePriorityPlayer = GatePriorityWorld->SpawnActor<AGameXXKTownPlayerPawn>(
			GatePriorityOrigin,
			FRotator::ZeroRotator);
		AGameXXKTownNpcActor* GatePriorityFollower = GatePriorityWorld->SpawnActor<AGameXXKTownNpcActor>(
			GatePriorityOrigin + FVector(96.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator);
		AGameXXKTownExitActor* GatePriorityExit = GatePriorityWorld->SpawnActor<AGameXXKTownExitActor>(
			GatePriorityOrigin + FVector(256.0f, 0.0f, 0.0f),
			FRotator::ZeroRotator);
		TestNotNull(TEXT("gate priority test spawns player"), GatePriorityPlayer);
		TestNotNull(TEXT("gate priority test spawns following quest NPC"), GatePriorityFollower);
		TestNotNull(TEXT("gate priority test spawns town exit"), GatePriorityExit);
		if (GatePriorityPlayer && GatePriorityFollower && GatePriorityExit)
		{
			GatePriorityFollower->SetNpcRole(EGameXXKTownNpcRole::Quest);
			GatePriorityFollower->SetMVPSubsystemForTest(Subsystem);
			GatePriorityFollower->ActivateFollower(GatePriorityPlayer, 96.0f);
			GatePriorityExit->SetMVPSubsystemForTest(Subsystem);
			GatePriorityPlayer->GetInteractionComponent()->SetFocusedActorForTest(GatePriorityFollower);
			TestTrue(TEXT("following quest NPC initially owns F focus"),
				GatePriorityPlayer->GetInteractionComponent()->GetFocusedActor() == GatePriorityFollower);
			GatePriorityPlayer->Interact();
			TestTrue(TEXT("F at town exit bypasses following quest NPC"), GatePriorityExit->WasLastInteractionSuccessful());
			TestEqual(TEXT("F at town exit opens route when follower is nearby"),
				Subsystem->GetRuntimeState().Screen,
				EGameXXKScreen::DungeonMap);
			Subsystem->FailDungeonToTown();
		}
		if (GatePriorityExit)
		{
			GatePriorityExit->Destroy();
		}
		if (GatePriorityFollower)
		{
			GatePriorityFollower->Destroy();
		}
		if (GatePriorityPlayer)
		{
			GatePriorityPlayer->Destroy();
		}
	}

	AGameXXKTownNpcActor* FollowerNpc = NewObject<AGameXXKTownNpcActor>();
	FollowerNpc->SetNpcRole(EGameXXKTownNpcRole::Follower);
	FollowerNpc->ActivateFollower(Player, 96.0f);
	TestTrue(TEXT("follower becomes active"), FollowerNpc->IsFollowerActive());
	TestTrue(TEXT("follower target stored"), FollowerNpc->GetFollowTarget() == Player);
	TestEqual(TEXT("follower distance stored"), FollowerNpc->GetFollowDistance(), 96.0f);
	FollowerNpc->DismissFollower();
	TestFalse(TEXT("follower dismisses after route clear"), FollowerNpc->IsFollowerActive());

	UWorld* TestWorld = GWorld;
	TestNotNull(TEXT("automation world exists for follower movement input test"), TestWorld);
	if (TestWorld)
	{
		FActorSpawnParameters SpawnParameters;
		SpawnParameters.ObjectFlags |= RF_Transient;
		SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		AGameXXKHeroCharacter* MovingHero = TestWorld->SpawnActor<AGameXXKHeroCharacter>(AGameXXKHeroCharacter::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		AGameXXKTownNpcActor* InputFollowerNpc = TestWorld->SpawnActor<AGameXXKTownNpcActor>(AGameXXKTownNpcActor::StaticClass(), FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters);
		TestNotNull(TEXT("spawned moving hero for follower movement input test"), MovingHero);
		TestNotNull(TEXT("spawned quest follower for movement input test"), InputFollowerNpc);
		if (MovingHero && InputFollowerNpc)
		{
			MovingHero->SetActorLocation(FVector::ZeroVector);
			InputFollowerNpc->SetActorLocation(FVector::ZeroVector);
			MovingHero->MoveHorizontal(1.0f);
			InputFollowerNpc->ActivateFollower(MovingHero, 96.0f);
			TestEqual(TEXT("hero exposes right movement intent for quest follower"), MovingHero->GetTownMovementIntentVector(), FVector::RightVector);
			TestTrue(TEXT("moving hero exposes positive town movement speed"), MovingHero->GetCharacterMovement() && MovingHero->GetCharacterMovement()->MaxWalkSpeed > 1.0f);
			TestTrue(TEXT("input follower is active before movement input tick"), InputFollowerNpc->IsFollowerActive());
			TestTrue(TEXT("input follower targets moving hero before movement input tick"), InputFollowerNpc->GetFollowTarget() == MovingHero);
			const FVector FollowerBeforeInputMove = InputFollowerNpc->GetActorLocation();
			InputFollowerNpc->Tick(0.25f);
			TestTrue(TEXT("quest follower stays idle while hero remains inside follow distance"), InputFollowerNpc->GetActorLocation().Equals(FollowerBeforeInputMove, 1.0f));
			InputFollowerNpc->SetActorLocation(FVector::ZeroVector);
			MovingHero->SetActorLocation(FVector(0.0f, 240.0f, 0.0f));
			const FVector FollowerBeforeRangeChase = InputFollowerNpc->GetActorLocation();
			InputFollowerNpc->Tick(0.25f);
			TestTrue(TEXT("quest follower starts chasing only after hero leaves follow distance"), InputFollowerNpc->GetActorLocation().Y > FollowerBeforeRangeChase.Y + 1.0f);
		}
		AGameXXKMVPGameMode* DirectNpcGameMode = NewObject<AGameXXKMVPGameMode>();
		UClass* DirectNpcClass = DirectNpcGameMode ? DirectNpcGameMode->GetPersonTownNpcCharacterClass().Get() : nullptr;
		TestNotNull(TEXT("direct follower NPC blueprint class is configured"), DirectNpcClass);
		TestTrue(TEXT("direct follower NPC blueprint is a hero-style Character class"),
			DirectNpcClass && DirectNpcClass->IsChildOf(AGameXXKHeroCharacter::StaticClass()));
		TestTrue(TEXT("direct follower NPC blueprint uses the NPC Character runtime class"),
			DirectNpcClass && DirectNpcClass->IsChildOf(AGameXXKTownNpcCharacter::StaticClass()));
		AGameXXKTownNpcCharacter* DirectFollowerNpc = DirectNpcClass
			? TestWorld->SpawnActor<AGameXXKTownNpcCharacter>(DirectNpcClass, FVector::ZeroVector, FRotator::ZeroRotator, SpawnParameters)
			: nullptr;
		TestNotNull(TEXT("direct follower NPC blueprint spawns as a Character body"), DirectFollowerNpc);
		if (MovingHero && DirectFollowerNpc)
		{
			UPaperFlipbook* NpcEastWalkFlipbook = NewObject<UPaperFlipbook>(DirectFollowerNpc);
			UPaperFlipbook* NpcEastIdleFlipbook = NewObject<UPaperFlipbook>(DirectFollowerNpc);
			DirectFollowerNpc->SetTownWalkDirectionFlipbookForTest(TownFacingEast, NpcEastWalkFlipbook);
			DirectFollowerNpc->SetTownIdleDirectionFlipbookForTest(TownFacingEast, NpcEastIdleFlipbook);
			const UCapsuleComponent* DirectFollowerCapsule = DirectFollowerNpc->GetCapsuleComponent();
			const float ExpectedGroundedRootZ = DirectFollowerCapsule ? DirectFollowerCapsule->GetScaledCapsuleHalfHeight() : 0.0f;
			DirectFollowerNpc->Tick(0.0f);
			TestTrue(FString::Printf(TEXT("direct follower NPC Character root stays at capsule half-height instead of sinking to ground plane actual=%.2f expected=%.2f"),
				DirectFollowerNpc->GetActorLocation().Z,
				ExpectedGroundedRootZ),
				DirectFollowerNpc->GetActorLocation().Z >= ExpectedGroundedRootZ - 1.0f);
			const FVector DirectFollowerGroundedStart = DirectFollowerNpc->GetActorLocation();
			MovingHero->SetActorLocation(DirectFollowerGroundedStart);
			MovingHero->ResetTownMovementInput();
			MovingHero->MoveHorizontal(1.0f);
			const FVector DirectFollowerBeforeInputMove = DirectFollowerNpc->GetActorLocation();
			DirectFollowerNpc->ActivateFollower(MovingHero, 96.0f);
			TestTrue(TEXT("direct follower NPC Character activates follower behavior"), DirectFollowerNpc->IsFollowerActive());
			TestTrue(TEXT("direct follower NPC Character stores the hero follow target"), DirectFollowerNpc->GetFollowTarget() == MovingHero);
			DirectFollowerNpc->Tick(0.25f);
			TestTrue(TEXT("direct follower NPC Character stays idle while hero remains inside follow distance"), DirectFollowerNpc->GetActorLocation().Equals(DirectFollowerBeforeInputMove, 1.0f));
			TestFalse(TEXT("direct follower NPC Character keeps idle visual state while hero remains inside follow distance"), DirectFollowerNpc->IsTownMoving());
			MovingHero->SetActorLocation(DirectFollowerGroundedStart + FVector(0.0f, 240.0f, 0.0f));
			const FVector DirectFollowerBeforeRangeChase = DirectFollowerNpc->GetActorLocation();
			DirectFollowerNpc->Tick(0.25f);
			TestTrue(TEXT("direct follower NPC Character moves after hero leaves follow distance"), DirectFollowerNpc->GetActorLocation().Y > DirectFollowerBeforeRangeChase.Y + 1.0f);
			TestTrue(TEXT("direct follower NPC Character enters walk visual state while chasing"), DirectFollowerNpc->IsTownMoving());
			TestEqual(TEXT("direct follower NPC Character uses East walk flipbook while chasing"), DirectFollowerNpc->GetCurrentTownFlipbook(), NpcEastWalkFlipbook);
			MovingHero->ResetTownMovementInput();
			MovingHero->SetActorLocation(DirectFollowerNpc->GetActorLocation() + FVector(0.0f, 48.0f, 0.0f));
			DirectFollowerNpc->Tick(0.25f);
			TestFalse(TEXT("direct follower NPC Character returns to idle visual state when back inside follow distance"), DirectFollowerNpc->IsTownMoving());
			TestEqual(TEXT("direct follower NPC Character preserves East facing idle after follow input stops"), DirectFollowerNpc->GetCurrentTownFlipbook(), NpcEastIdleFlipbook);
		}
		AGameXXKTownNpcCharacter* RecordingQuestNpc = TestWorld->SpawnActor<AGameXXKTownNpcCharacter>(
			AGameXXKTownNpcCharacter::StaticClass(),
			FVector(30.0f, -80.0f, 72.0f),
			FRotator::ZeroRotator,
			SpawnParameters);
		TestNotNull(TEXT("spawned direct quest NPC for save-state recording"), RecordingQuestNpc);
		if (MovingHero && RecordingQuestNpc)
		{
			UGameplayStatics::DeleteGameInSlot(NpcAutosaveSlot, 0);
			UGameXXKMVPSubsystem* RecordingSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			RecordingSubsystem->OpenWorldMap();
			RecordingSubsystem->SelectWorldRegion(UGameXXKMVPRules::RegionQingshan());
			RecordingQuestNpc->SetNpcRole(EGameXXKTownNpcRole::Quest);
			RecordingQuestNpc->SetMVPSubsystemForTest(RecordingSubsystem);
			MovingHero->SetActorLocation(RecordingQuestNpc->GetActorLocation());
			TestTrue(TEXT("direct quest NPC accepts quest for save-state recording"), RecordingQuestNpc->ApplyDefaultInteraction(MovingHero));
			const FVector RecordedQuestNpcAcceptLocation = RecordingQuestNpc->GetActorLocation();
			TestTrue(TEXT("quest NPC accept records a task NPC location flag"), RecordingSubsystem->GetRuntimeState().bHasQuestNpcLocation);
			TestTrue(TEXT("quest NPC accept records the task NPC location"),
				RecordingSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordedQuestNpcAcceptLocation, 0.1f));
			UGameXXKMVPSubsystem* ReloadedRecordingSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			TestFalse(TEXT("quest NPC accept waits for manual save"), ReloadedRecordingSubsystem->LoadGameFromSlot(NpcAutosaveSlot, 0));
			TestTrue(TEXT("manual save after quest NPC accept writes task NPC state"), RecordingSubsystem->SaveCurrentGame(TEXT(""), 0));
			UGameXXKMVPSubsystem* ReloadedRecordingManualSaveSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			TestTrue(TEXT("quest NPC manual save can be loaded for task NPC state"), ReloadedRecordingManualSaveSubsystem->LoadGameFromSlot(NpcAutosaveSlot, 0));
			TestTrue(TEXT("quest NPC manual save persists task NPC location flag"), ReloadedRecordingManualSaveSubsystem->GetRuntimeState().bHasQuestNpcLocation);
			TestTrue(TEXT("quest NPC manual save persists task NPC location"),
				ReloadedRecordingManualSaveSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordedQuestNpcAcceptLocation, 0.1f));
			ReloadedRecordingSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			TestTrue(TEXT("quest NPC saved location still loads before follower moves"), ReloadedRecordingSubsystem->LoadGameFromSlot(NpcAutosaveSlot, 0));
			TestTrue(TEXT("quest NPC saved location matches accept location before follower moves"),
				ReloadedRecordingSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordedQuestNpcAcceptLocation, 0.1f));
			MovingHero->SetActorLocation(RecordedQuestNpcAcceptLocation + FVector(0.0f, 260.0f, 0.0f));
			RecordingQuestNpc->Tick(0.25f);
			TestTrue(TEXT("quest NPC follower movement updates runtime task NPC location"),
				RecordingSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordingQuestNpc->GetActorLocation(), 0.1f));
			TestFalse(TEXT("quest NPC follower movement records a changed task NPC location"),
				RecordingSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordedQuestNpcAcceptLocation, 0.1f));
			UGameXXKMVPSubsystem* ReloadedAfterMoveSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			TestTrue(TEXT("quest NPC follower movement keeps prior manual save loadable"), ReloadedAfterMoveSubsystem->LoadGameFromSlot(NpcAutosaveSlot, 0));
			TestTrue(TEXT("quest NPC follower movement does not overwrite saved accept location before manual save"),
				ReloadedAfterMoveSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordedQuestNpcAcceptLocation, 0.1f));
			TestTrue(TEXT("manual save after quest NPC movement writes moved task NPC state"), RecordingSubsystem->SaveCurrentGame(TEXT(""), 0));
			UGameXXKMVPSubsystem* ReloadedAfterMoveManualSaveSubsystem = NewObject<UGameXXKMVPSubsystem>(TestGameInstance);
			TestTrue(TEXT("quest NPC movement manual save loads task NPC state"), ReloadedAfterMoveManualSaveSubsystem->LoadGameFromSlot(NpcAutosaveSlot, 0));
			TestTrue(TEXT("quest NPC movement manual save persists the moved task NPC location"),
				ReloadedAfterMoveManualSaveSubsystem->GetRuntimeState().QuestNpcLocation.Equals(RecordingQuestNpc->GetActorLocation(), 0.1f));
		}
		if (RecordingQuestNpc)
		{
			RecordingQuestNpc->Destroy();
		}
		if (DirectFollowerNpc)
		{
			DirectFollowerNpc->Destroy();
		}
		if (InputFollowerNpc)
		{
			InputFollowerNpc->Destroy();
		}
		if (MovingHero)
		{
			MovingHero->Destroy();
		}
	}

	UGameplayStatics::DeleteGameInSlot(NpcAutosaveSlot, 0);
	return true;
}

#endif
