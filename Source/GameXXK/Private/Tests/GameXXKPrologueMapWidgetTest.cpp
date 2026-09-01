#include "UI/GameXXKPrologueMapWidget.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueMapWidgetTest,
	"GameXXK.Prologue.Aftermath.MapWidget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueMapWidgetTest::RunTest(const FString& Parameters)
{
	UGameXXKPrologueMapWidget* Widget = NewObject<UGameXXKPrologueMapWidget>();
	Widget->TakeWidget();
	Widget->Configure(EGameXXKPrologueMapMode::StoryCard);

	TestTrue(TEXT("thumbnail starts visible"), Widget->IsThumbnailVisibleForTest());
	TestFalse(TEXT("inspection starts closed"), Widget->IsInspectionOpenForTest());
	TestEqual(TEXT("story card has no title"),
		Widget->GetTitleTextForTest(),
		FText::GetEmpty());
	TestTrue(TEXT("inspect button exists"), Widget->HasInspectButtonForTest());
	TestTrue(TEXT("continue prompt exists"), Widget->HasContinuePromptForTest());
	TestEqual(TEXT("approved task icon path"),
		Widget->GetTaskIconPathForTest(),
		FString(TEXT("/Game/GameXXK/UI/Relics/Icons/T_Relic_OldMap.T_Relic_OldMap")));
	TestEqual(TEXT("approved inspection texture path"),
		Widget->GetInspectionTexturePathForTest(),
		FString(TEXT("/Game/GameXXK/Narrative/Items/T_Tutorial_XuXiakeTravelRouteInspect.T_Tutorial_XuXiakeTravelRouteInspect")));

	int32 InspectRequests = 0;
	int32 CloseRequests = 0;
	int32 ContinueRequests = 0;
	Widget->SetInspectRequestedForTest(FGameXXKPrologueMapInspectRequested::CreateLambda(
		[&InspectRequests]() { ++InspectRequests; }));
	Widget->SetCloseRequestedForTest(FGameXXKPrologueMapCloseRequested::CreateLambda(
		[&CloseRequests]() { ++CloseRequests; }));
	Widget->SetContinueRequestedForTest(FGameXXKPrologueMapContinueRequested::CreateLambda(
		[&ContinueRequests]() { ++ContinueRequests; }));

	TestTrue(TEXT("inspection opens"), Widget->RequestInspectionForTest());
	TestTrue(TEXT("inspect delegate fires once"), InspectRequests == 1);
	TestTrue(TEXT("inspection reports open"), Widget->IsInspectionOpenForTest());
	TestFalse(TEXT("space is blocked while inspecting"), Widget->RequestContinueForTest());
	TestEqual(TEXT("blocked space never fires continue"), ContinueRequests, 0);
	TestTrue(TEXT("close returns to thumbnail"), Widget->RequestCloseInspectionForTest());
	TestEqual(TEXT("close delegate fires once"), CloseRequests, 1);
	TestFalse(TEXT("inspection closes"), Widget->IsInspectionOpenForTest());
	TestTrue(TEXT("space continues from thumbnail"), Widget->RequestContinueForTest());
	TestEqual(TEXT("continue fires once"), ContinueRequests, 1);

	Widget->Configure(EGameXXKPrologueMapMode::InspectOnly);
	TestTrue(TEXT("inspect-only opens inspection immediately"), Widget->IsInspectionOpenForTest());
	TestFalse(TEXT("inspect-only hides continue prompt"), Widget->HasContinuePromptForTest());
	TestFalse(TEXT("inspect-only space remains inert"), Widget->RequestContinueForTest());
	TestTrue(TEXT("inspect-only close is available"), Widget->RequestCloseInspectionForTest());

	const FVector2D Fit = UGameXXKPrologueMapWidget::FitInspectionImageForTest(
		FVector2D(1920.0f, 1080.0f));
	TestTrue(TEXT("inspection fits reference height"), FMath::IsNearlyEqual(Fit.Y, 860.0f));
	TestTrue(TEXT("inspection preserves replacement aspect"),
		FMath::IsNearlyEqual(Fit.X / Fit.Y, 2388.0f / 1668.0f, 0.001f));
	TestTrue(TEXT("replacement inspection is landscape"), Fit.X > Fit.Y);

	return true;
}

#endif
