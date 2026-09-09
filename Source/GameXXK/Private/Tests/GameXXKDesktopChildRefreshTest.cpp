#include "Misc/AutomationTest.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "UObject/StrongObjectPtr.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SWindow.h"
#include "Layout/ArrangedChildren.h"
#include "Input/HittestGrid.h"
#include "Rendering/DrawElements.h"
#include "Types/PaintArgs.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
#include "HAL/FileManager.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "Engine/GameInstance.h"

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopChildRefreshTest,
	"GameXXK.DesktopTraining.Workbench.ChildCallbacksDeferRootRebuild",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopChildRefreshTest::RunTest(const FString&)
{
	TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
	TStrongObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Widget(NewObject<UGameXXKDesktopTrainingWorkbenchWidget>(World.Get()));
	const int32 Before=Widget->GetProgrammaticLayoutBuildCountForTest();
	// A child-owned callback does not set the workbench's private action guard.
	// It must still be unable to tear down the host tree on that call stack.
	Widget->OpenBackpack();
	TestTrue(TEXT("external child request is queued"),Widget->HasPendingLayoutRefreshForTest());
	TestEqual(TEXT("the host remains intact until the callback unwinds"),Widget->GetProgrammaticLayoutBuildCountForTest(),Before);
	World->GetTimerManager().Tick(.016f);
	TestEqual(TEXT("the next game tick applies one structural refresh"),Widget->GetProgrammaticLayoutBuildCountForTest(),Before+1);
	Widget.Reset();
	World->DestroyWorld(false);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopHostSurvivesNavigationTest,
	"GameXXK.DesktopTraining.Workbench.HostSurvivesNavigation",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopHostSurvivesNavigationTest::RunTest(const FString&)
{
	TStrongObjectPtr<UWorld> World(UWorld::CreateWorld(EWorldType::Game,false));
	TStrongObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Widget(NewObject<UGameXXKDesktopTrainingWorkbenchWidget>(World.Get()));
	TSharedPtr<SWidget> HeldInputPath=Widget->TakeWidget();
	const TSharedRef<SBox> Host=SNew(SBox)[HeldInputPath.ToSharedRef()];
	const TSharedRef<SWindow> Window=SNew(SWindow).ClientSize(FVector2D(1280,720))[Host];
	FSlateApplication::Get().AddWindow(Window,false);
	Widget->SetDesktopOverlayContentHost(Host);
	Widget->OpenBackpack();
	World->GetTimerManager().Tick(.016f);
	TestTrue(TEXT("navigation preserves the root held by an input path"),Host->GetChildren()->GetChildAt(0)==HeldInputPath);
	// Slate input/inspection may retain the previous root until after navigation.
	// Releasing it must never run NativeDestruct against a replacement root.
	HeldInputPath.Reset();
	UButton* Navigation=Cast<UButton>(Widget->WidgetTree->FindWidget(TEXT("BottomNavigationButton_1")));
	TestNotNull(TEXT("formation navigation exists"),Navigation);
	if(Navigation) TestTrue(TEXT("old input references cannot release current buttons"),Navigation->GetCachedWidget().IsValid());
	FSlateApplication::Get().RequestDestroyWindow(Window);
	Widget.Reset();
	World->DestroyWorld(false);
	return true;
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FGameXXKDesktopBackpackTransitionPaintTest,
	"GameXXK.DesktopTraining.Workbench.BackpackToggleFirstPaintKeepsStrip",
	EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FGameXXKDesktopBackpackTransitionPaintTest::RunTest(const FString&)
{
	const TCHAR* Section=TEXT("/Script/GameXXK.DesktopHudSettings");
	const TCHAR* Key=TEXT("HudScalePercent");
	const FString SettingsFile=FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(),TEXT("Saved/Config/GameXXKDesktopHudSettings.ini")));
	FString PreviousContents;const bool bHadFile=FFileHelper::LoadFileToString(PreviousContents,*SettingsFile);
	int32 PreviousScale=100;const bool bHadScale=GConfig && GConfig->GetInt(Section,Key,PreviousScale,GGameUserSettingsIni);
	ON_SCOPE_EXIT
	{
		if(GConfig)
		{
			if(bHadScale)GConfig->SetInt(Section,Key,PreviousScale,GGameUserSettingsIni);
			else GConfig->RemoveKey(Section,Key,GGameUserSettingsIni);
			GConfig->Flush(false,GGameUserSettingsIni);
		}
		if(bHadFile)FFileHelper::SaveStringToFile(PreviousContents,*SettingsFile);
		else IFileManager::Get().Delete(*SettingsFile,false,true);
	};
	TStrongObjectPtr<UGameXXKMVPSubsystem> Subsystem(NewObject<UGameXXKMVPSubsystem>(NewObject<UGameInstance>()));
	if(!TestTrue(TEXT("transition fixture starts the normal runtime"),Subsystem->StartGame()))return false;
	TStrongObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Widget(NewObject<UGameXXKDesktopTrainingWorkbenchWidget>());
	Widget->SetMVPSubsystem(Subsystem.Get());
	Widget->InitializeDesktopPresentationHostSize(FVector2D(1920,1020));
	TSharedRef<SWidget> Root=Widget->TakeWidget();
	Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
	const TSharedRef<SWindow> Window=SNew(SWindow).ClientSize(FVector2D(1920,1020)).CreateTitleBar(false);
	const FGeometry RootGeometry=FGeometry::MakeRoot(FVector2D(1920,1020),FSlateLayoutTransform());
	TFunction<bool(const TSharedRef<SWidget>&,const FGeometry&,const TSharedPtr<SWidget>&,FGeometry&)> FindGeometry;
	FindGeometry=[&](const TSharedRef<SWidget>& Current,const FGeometry& Geometry,const TSharedPtr<SWidget>& Target,FGeometry& Out)
	{
		if(Current==Target){Out=Geometry;return true;}
		FArrangedChildren Children(EVisibility::All);Current->ArrangeChildren(Geometry,Children);
		for(int32 I=0;I<Children.Num();++I)
			if(FindGeometry(Children[I].Widget,Children[I].Geometry,Target,Out))return true;
		return false;
	};
	auto PaintStrip=[&]()
	{
		Root->SlatePrepass(1.0f);
		FSlateWindowElementList Elements(Window);FHittestGrid Grid;
		const FPaintArgs Args(&Window.Get(),Grid,FVector2D::ZeroVector,0.0,0.0f);
		Root->Paint(Args,RootGeometry,FSlateRect(0,0,1920,1020),Elements,0,FWidgetStyle(),true);
		UWidget* Strip=Widget->WidgetTree->FindWidget(TEXT("TrainingTravelStrip"));
		FGeometry Geometry;
		TestTrue(TEXT("painted strip remains in the mounted tree"),Strip && FindGeometry(Root,RootGeometry,Strip->GetCachedWidget(),Geometry));
		const FVector2D Position=Geometry.LocalToAbsolute(FVector2D::ZeroVector);
		const FVector2D Size=Geometry.LocalToAbsolute(Geometry.GetLocalSize())-Position;
		return FVector4(Position.X,Position.Y,Size.X,Size.Y);
	};
	for(int32 ScaleAction:{651,656,650})
	{
		Widget->HandleDesktopActionForTest(ScaleAction);
		Widget->TickForTest(0);
		TestEqual(TEXT("the requested player scale is active"),Widget->GetHudScalePercentForTest(),ScaleAction==651?50:ScaleAction==656?75:100);
		for(int32 Warmup=0;Warmup<3;++Warmup)PaintStrip();
		const FVector4 Before=PaintStrip();
		TestTrue(TEXT("fixture paints a full-sized idle strip"),Before.Z>400 && Before.W>50);
		AddInfo(FString::Printf(TEXT("scale action %d initial strip %s"),ScaleAction,*Before.ToString()));
		for(int32 Toggle=0;Toggle<4;++Toggle)
		{
			const bool bWasExpanded=Widget->IsBackpackExpandedForTest();
			Widget->HandleDesktopActionForTest(60);
			Widget->TickForTest(0);
			TestEqual(TEXT("the real backpack toggle changed state"),Widget->IsBackpackExpandedForTest(),!bWasExpanded);
			TestFalse(TEXT("the requested structural refresh has been applied"),Widget->HasPendingLayoutRefreshForTest());
			for(int32 Frame=0;Frame<3;++Frame)
			{
				const FVector4 After=PaintStrip();
				TestTrue(*FString::Printf(TEXT("scale action %d toggle %d frame %d keeps strip position and size: before %s after %s"),ScaleAction,Toggle,Frame,*Before.ToString(),*After.ToString()),After.Equals(Before,.1));
			}
		}
	}
	Widget.Reset();
	return true;
}
#endif
