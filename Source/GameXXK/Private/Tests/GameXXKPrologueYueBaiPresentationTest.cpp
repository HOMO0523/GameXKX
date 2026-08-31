#include "UI/GameXXKPrologueYueBaiWidget.h"

#include "Components/Image.h"
#include "Components/CapsuleComponent.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"
#include "Town/GameXXKTownNpcCharacter.h"

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

	YueBai->Destroy();
	Hero->Destroy();
	return true;
}

#endif
