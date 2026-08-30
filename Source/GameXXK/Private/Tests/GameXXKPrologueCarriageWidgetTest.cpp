#include "UI/GameXXKPrologueCarriageWidget.h"

#include "Components/Image.h"
#include "Engine/Texture2D.h"
#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FGameXXKPrologueCarriageWidgetTest,
	"GameXXK.Prologue.Carriage.Widget",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FGameXXKPrologueCarriageWidgetTest::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("frame zero occupies the first atlas cell"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(0),
		FBox2f(FVector2f(0.0f, 0.0f), FVector2f(0.125f, 0.125f)));
	TestEqual(TEXT("frame seven completes the first atlas row"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(7),
		FBox2f(FVector2f(0.875f, 0.0f), FVector2f(1.0f, 0.125f)));
	TestEqual(TEXT("frame eight begins the second atlas row"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(8),
		FBox2f(FVector2f(0.0f, 0.125f), FVector2f(0.125f, 0.25f)));
	TestEqual(TEXT("frame fifty-nine ignores the four empty atlas cells"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(59),
		FBox2f(FVector2f(0.375f, 0.875f), FVector2f(0.5f, 1.0f)));
	TestEqual(TEXT("negative frame clamps to zero"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(-10),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(0));
	TestEqual(TEXT("out-of-range frame clamps to fifty-nine"),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(99),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(59));

	UGameXXKPrologueCarriageWidget* Widget =
		NewObject<UGameXXKPrologueCarriageWidget>();
	if (!TestNotNull(TEXT("carriage widget fixture exists"), Widget))
	{
		return false;
	}
	Widget->TakeWidget();
	UImage* Image = Widget->GetCarriageImageForTest();
	if (!TestNotNull(TEXT("carriage widget builds one real image"), Image))
	{
		return false;
	}
	TestEqual(TEXT("world image never captures input"),
		Image->GetVisibility(),
		ESlateVisibility::HitTestInvisible);
	TestEqual(TEXT("left-facing source is mirrored for rightward travel"),
		Image->GetRenderTransform().Scale,
		FVector2D(-1.0f, 1.0f));
	TestEqual(TEXT("horizontal mirror uses the image center pivot"),
		Image->GetRenderTransformPivot(),
		FVector2D(0.5f, 0.5f));

	UTexture2D* Texture = NewObject<UTexture2D>();
	TestTrue(TEXT("real image accepts an atlas frame"),
		Widget->SetAtlasFrame(Texture, 35));
	TestEqual(TEXT("presented frame is observable"),
		Widget->GetPresentedFrameForTest(), 35);
	TestEqual(TEXT("presented texture is observable"),
		Widget->GetPresentedTextureForTest(), Texture);
	TestEqual(TEXT("presented UV matches the real frame helper"),
		Widget->GetPresentedUvForTest(),
		UGameXXKPrologueCarriageWidget::FrameUvForTest(35));
	TestEqual(TEXT("image brush owns the requested texture"),
		Image->GetBrush().GetResourceObject(),
		static_cast<UObject*>(Texture));

	TestFalse(TEXT("empty texture fails safely"),
		Widget->SetAtlasFrame(nullptr, 0));
	TestEqual(TEXT("failed update preserves the last good frame"),
		Widget->GetPresentedFrameForTest(), 35);
	TestEqual(TEXT("failed update preserves the last good texture"),
		Widget->GetPresentedTextureForTest(), Texture);

	return true;
}

#endif
