#include "UI/GameXXKPrologueYueBaiWidget.h"

#include "Components/Image.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "PaperFlipbook.h"
#include "PaperFlipbookComponent.h"
#include "PaperSprite.h"
#include "SpriteEditorOnlyTypes.h"
#include "Town/GameXXKTownNpcCharacter.h"
#include "UObject/UnrealType.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueYueBaiPresentationTest,
	"GameXXK.Prologue.Aftermath.YueBaiPresentation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueYueBaiPresentationTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("intro duration matches approved atlas"),
		FMath::IsNearlyEqual(
			UGameXXKPrologueYueBaiWidget::GetIntroDurationSecondsForTest(),
			1.07f));
	TestTrue(TEXT("intro rate matches approved atlas"),
		FMath::IsNearlyEqual(
			UGameXXKPrologueYueBaiWidget::GetFramesPerSecondForTest(),
			56.074766f,
			0.001f));
	TestEqual(TEXT("intro has sixty frames"),
		UGameXXKPrologueYueBaiWidget::GetFrameCountForTest(),
		60);
	TestEqual(TEXT("intro 2K source"),
		UGameXXKPrologueYueBaiWidget::GetTexturePathForTest(false),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_2k_atlas.T_character_09_yue_bai_intro_2k_atlas")));
	TestEqual(TEXT("intro 1K fallback"),
		UGameXXKPrologueYueBaiWidget::GetTexturePathForTest(true),
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/Atlases/T_character_09_yue_bai_intro_1k_atlas.T_character_09_yue_bai_intro_1k_atlas")));
	TestEqual(TEXT("frame zero UV"),
		UGameXXKPrologueYueBaiWidget::FrameUvForTest(0),
		FBox2f(FVector2f(0.0f, 0.0f), FVector2f(0.125f, 0.125f)));
	TestEqual(TEXT("frame fifty-nine UV"),
		UGameXXKPrologueYueBaiWidget::FrameUvForTest(59),
		FBox2f(FVector2f(0.375f, 0.875f), FVector2f(0.5f, 1.0f)));

	UGameXXKPrologueYueBaiWidget* Widget = NewObject<UGameXXKPrologueYueBaiWidget>();
	Widget->TakeWidget();
	TestEqual(TEXT("intro owns one image"), Widget->GetImageCountForTest(), 1);
	TestEqual(TEXT("intro never captures input"),
		Widget->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	UTexture2D* Texture = NewObject<UTexture2D>();
	TestTrue(TEXT("intro accepts atlas frame"), Widget->SetAtlasFrame(Texture, 35));
	TestFalse(TEXT("intro rejects missing texture"), Widget->SetAtlasFrame(nullptr, 0));
	const FString NarrativeIdlePath =
		AGameXXKTownNpcCharacter::GetNarrativeFollowerIdleFlipbookPathForTest();
	TestEqual(TEXT("narrative follow selects its isolated blue-flame 2K idle"),
		NarrativeIdlePath,
		FString(TEXT("/Game/GameXXK/Cinematics/Prologue/IdleFlipbooks/FB_character_09_yue_bai_town_2k_idle.FB_character_09_yue_bai_town_2k_idle")));
	TestFalse(TEXT("narrative follow does not reuse the battle idle"),
		NarrativeIdlePath.Contains(TEXT("/BattleAnimations/")));
	TestFalse(TEXT("narrative follow never falls back to 1K"),
		NarrativeIdlePath.Contains(TEXT("_1k_")));
	UPaperFlipbook* NarrativeIdle =
		AGameXXKTownNpcCharacter::LoadNarrativeFollowerIdleFlipbookForTest();
	TestNotNull(TEXT("isolated blue-flame YueBai idle loads"), NarrativeIdle);
	if (NarrativeIdle)
	{
		TestEqual(TEXT("isolated YueBai idle preserves all sixty authored frames"),
			NarrativeIdle->GetNumFrames(),
			60);
		TestTrue(TEXT("blue-flame YueBai idle keeps its approved timing"),
			FMath::IsNearlyEqual(
				NarrativeIdle->GetFramesPerSecond(),
				14.7420147f,
				0.001f));
		const FStructProperty* RenderGeometryProperty =
			FindFProperty<FStructProperty>(
				UPaperSprite::StaticClass(),
				TEXT("RenderGeometry"));
		if (TestNotNull(TEXT("PaperSprite render geometry is inspectable"),
			RenderGeometryProperty))
		{
			for (int32 FrameIndex = 0;
				FrameIndex < NarrativeIdle->GetNumKeyFrames();
				++FrameIndex)
			{
				UPaperSprite* Sprite =
					NarrativeIdle->GetKeyFrameChecked(FrameIndex).Sprite;
				if (!TestNotNull(
					*FString::Printf(TEXT("blue-flame frame %d owns a sprite"), FrameIndex),
					Sprite))
				{
					continue;
				}
				const FSpriteGeometryCollection* RenderGeometry =
					RenderGeometryProperty->ContainerPtrToValuePtr<
						FSpriteGeometryCollection>(Sprite);
				if (TestNotNull(
					*FString::Printf(TEXT("blue-flame frame %d owns render geometry"), FrameIndex),
					RenderGeometry))
				{
					TestEqual(
						*FString::Printf(TEXT("blue-flame frame %d cannot crop its replaced atlas"), FrameIndex),
						RenderGeometry->GeometryType.GetValue(),
						ESpritePolygonMode::SourceBoundingBox);
				}
				float MinU = MAX_flt;
				float MaxU = -MAX_flt;
				float MinV = MAX_flt;
				float MaxV = -MAX_flt;
				for (const FVector4& Vertex : Sprite->BakedRenderData)
				{
					MinU = FMath::Min(MinU, Vertex.Z);
					MaxU = FMath::Max(MaxU, Vertex.Z);
					MinV = FMath::Min(MinV, Vertex.W);
					MaxV = FMath::Max(MaxV, Vertex.W);
				}
				TestTrue(
					*FString::Printf(TEXT("blue-flame frame %d bakes the full cell width"), FrameIndex),
					FMath::IsNearlyEqual(MaxU - MinU, 0.125f, 0.0001f));
				TestTrue(
					*FString::Printf(TEXT("blue-flame frame %d bakes the full cell height"), FrameIndex),
					FMath::IsNearlyEqual(MaxV - MinV, 0.125f, 0.0001f));
			}
		}
	}
	TestEqual(TEXT("ground hit lifts the follower root above the stair"),
		AGameXXKTownNpcCharacter::ResolveNarrativeGroundedRootZForTest(
			1075.0f,
			true,
			300.0f,
			72.0f),
		372.0f);
	TestEqual(TEXT("missing ground preserves the previous root height"),
		AGameXXKTownNpcCharacter::ResolveNarrativeGroundedRootZForTest(
			1075.0f,
			false,
			300.0f,
			72.0f),
		1075.0f);
	TestFalse(TEXT("overhead beams are never accepted as follower ground"),
		AGameXXKTownNpcCharacter::IsNarrativeGroundCandidateForTest(
			1200.0f,
			1.0f,
			1075.0f));
	TestFalse(TEXT("vertical walls are never accepted as follower ground"),
		AGameXXKTownNpcCharacter::IsNarrativeGroundCandidateForTest(
			1000.0f,
			0.0f,
			1075.0f));
	TestTrue(TEXT("walkable stair below the root is accepted as ground"),
		AGameXXKTownNpcCharacter::IsNarrativeGroundCandidateForTest(
			1003.0f,
			1.0f,
			1075.0f));

	UWorld* TestWorld = GWorld;
	if (!TestNotNull(TEXT("automation world exists"), TestWorld))
	{
		return false;
	}
	FActorSpawnParameters SpawnParameters;
	SpawnParameters.ObjectFlags |= RF_Transient;
	SpawnParameters.SpawnCollisionHandlingOverride =
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	AGameXXKTownNpcCharacter* YueBai = TestWorld->SpawnActor<AGameXXKTownNpcCharacter>(
		AGameXXKTownNpcCharacter::StaticClass(),
		FVector(0.0f, -260.0f, 72.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	AActor* Hero = TestWorld->SpawnActor<AActor>(
		AActor::StaticClass(),
		FVector(0.0f, 0.0f, 72.0f),
		FRotator::ZeroRotator,
		SpawnParameters);
	if (!TestNotNull(TEXT("YueBai fixture spawns"), YueBai)
		|| !TestNotNull(TEXT("hero target fixture spawns"), Hero))
	{
		return false;
	}
	YueBai->SetNpcId(TEXT("Npc.YueBai"));
	UPaperFlipbookComponent* YueBaiVisual = YueBai->GetTownVisualComponent();
	if (!TestNotNull(TEXT("YueBai owns its town visual"), YueBaiVisual))
	{
		return false;
	}
	YueBaiVisual->SetLooping(false);
	YueBaiVisual->Stop();
	const FVector VisualScaleBeforeFollow = YueBaiVisual->GetRelativeScale3D();
	TestEqual(TEXT("3D narrative YueBai uses the approved scale multiplier"),
		AGameXXKTownNpcCharacter::GetNarrativeFollowerVisualScaleMultiplierForTest(),
		2.5f);
	const ECollisionEnabled::Type CollisionBefore =
		YueBai->GetCapsuleComponent()->GetCollisionEnabled();
	YueBai->ActivateNarrativeFollower(Hero, 220.0f, 260.0f, 300.0f);
	TestTrue(TEXT("narrative follower active"), YueBai->IsNarrativeFollowerActive());
	TestEqual(TEXT("minimum distance"),
		YueBai->GetNarrativeFollowMinimumForTest(),
		220.0f);
	TestEqual(TEXT("target distance"), YueBai->GetFollowDistance(), 260.0f);
	TestEqual(TEXT("maximum distance"),
		YueBai->GetNarrativeFollowMaximumForTest(),
		300.0f);
	TestEqual(TEXT("narrative follower disables collision"),
		YueBai->GetCapsuleComponent()->GetCollisionEnabled(),
		ECollisionEnabled::NoCollision);
	TestTrue(TEXT("narrative follower restores looping hover idle"),
		YueBaiVisual->IsLooping());
	TestTrue(TEXT("narrative follower resumes stopped hover idle"),
		YueBaiVisual->IsPlaying());
	TestEqual(TEXT("narrative follower enlarges only its visual"),
		YueBaiVisual->GetRelativeScale3D(),
		VisualScaleBeforeFollow * 2.5f);

	const FVector AtTarget = YueBai->GetActorLocation();
	YueBai->Tick(0.25f);
	TestTrue(TEXT("target distance remains still"),
		YueBai->GetActorLocation().Equals(AtTarget, 0.1f));

	YueBai->SetActorLocation(FVector(0.0f, -290.0f, 72.0f));
	const FVector InsideBand = YueBai->GetActorLocation();
	YueBai->Tick(0.25f);
	TestTrue(TEXT("inside hysteresis band remains still"),
		YueBai->GetActorLocation().Equals(InsideBand, 0.1f));

	YueBai->SetActorLocation(FVector(0.0f, -310.0f, 72.0f));
	YueBai->Tick(0.25f);
	TestTrue(TEXT("outside maximum chases toward hero"),
		YueBai->GetActorLocation().Y > -310.0f);
	TestFalse(TEXT("narrative chase keeps hover-idle visual"), YueBai->IsTownMoving());

	YueBai->SetActorLocation(FVector(0.0f, -210.0f, 72.0f));
	YueBai->Tick(0.25f);
	TestTrue(TEXT("inside minimum retreats from hero"),
		YueBai->GetActorLocation().Y < -210.0f);

	YueBai->DismissNarrativeFollower();
	TestFalse(TEXT("narrative follower dismisses"), YueBai->IsNarrativeFollowerActive());
	TestEqual(TEXT("dismiss restores collision"),
		YueBai->GetCapsuleComponent()->GetCollisionEnabled(),
		CollisionBefore);
	TestEqual(TEXT("dismiss restores the authored visual scale"),
		YueBaiVisual->GetRelativeScale3D(),
		VisualScaleBeforeFollow);

	YueBai->Destroy();
	Hero->Destroy();
	return true;
}

#endif
