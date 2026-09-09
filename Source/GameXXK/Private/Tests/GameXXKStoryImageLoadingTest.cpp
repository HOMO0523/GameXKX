#include "Misc/AutomationTest.h"
#include "UI/GameXXKAsyncStoryImage.h"
#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKStoryImageReleaseTest,"GameXXK.MainStory.ImageRequestRelease",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKStoryImageReleaseTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameXXKAsyncStoryImage> Image(NewObject<UGameXXKAsyncStoryImage>());
	TStrongObjectPtr<UTexture2D> First(NewObject<UTexture2D>()),Second(NewObject<UTexture2D>());
	Image->TakeWidget();
	Image->SetStoryTexture(FSoftObjectPath(First.Get()));
	TestEqual(TEXT("already loaded source presents without loading another image"),Image->GetLoadedTexture(),First.Get());
	TestEqual(TEXT("layout aspect is independent of texture size/compilation"),FVector2D(Image->GetBrush().ImageSize),FVector2D(1536,512));
	Image->SetStoryTexture(FSoftObjectPath(Second.Get()));
	TestEqual(TEXT("replacing an image drops the first source"),Image->GetLoadedTexture(),Second.Get());
	Image->ClearStoryTexture();
	TestNull(TEXT("close releases the texture"),Image->GetLoadedTexture());
	TestNull(TEXT("close releases the brush material and its source"),Image->GetBrush().GetResourceObject());
	TestTrue(TEXT("close clears requested path"),Image->GetRequestedPath().IsNull());
	Image->SetStoryTexture(FSoftObjectPath(First.Get()));
	Image->ReleaseSlateResources(true);
	TestNull(TEXT("Slate release also clears image residency"),Image->GetLoadedTexture());
	Image->TakeWidget();
	TestEqual(TEXT("routine Slate reattachment restores the same desired source"),Image->GetLoadedTexture(),First.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKStoryImageCancellationTest,"GameXXK.MainStory.ImageRequestCancellation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKStoryImageCancellationTest::RunTest(const FString&)
{
	TStrongObjectPtr<UGameXXKAsyncStoryImage> Image(NewObject<UGameXXKAsyncStoryImage>());
	Image->SetStoryTexture(FSoftObjectPath(TEXT("/Game/GameXXK/UI/StoryNodes/T_Story_S00_01.T_Story_S00_01")));
	Image->ClearStoryTexture();
	FlushAsyncLoading();
	TestNull(TEXT("a completion after close cannot restore the old image"),Image->GetLoadedTexture());
	TestTrue(TEXT("cancelled target stays empty"),Image->GetRequestedPath().IsNull());
	return true;
}
#endif
