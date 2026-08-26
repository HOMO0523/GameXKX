#include "GameXXKEditorCaptureAutomationLibrary.h"

#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/Widget.h"
#include "Engine/GameInstance.h"
#include "Engine/Texture.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/FileManager.h"
#include "ImageUtils.h"
#include "LevelEditor.h"
#include "Misc/DateTime.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "MVP/GameXXKMVPSubsystem.h"
#include "RenderingThread.h"
#include "SLevelViewport.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Slate/WidgetRenderer.h"
#include "Styling/CoreStyle.h"
#include "UI/GameXXKDesktopTrainingWorkbenchWidget.h"
#include "UnrealClient.h"
#include "UObject/StrongObjectPtr.h"
#include "Widgets/Docking/SDockTab.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/SWindow.h"

namespace
{
	struct FLevelViewportCaptureAudit
	{
		bool bLevelEditorTabForeground = false;
		bool bLevelEditorTabVisible = false;
		bool bSlateWindowActive = false;
		bool bViewportVisible = false;
		bool bViewportFocused = false;
		bool bInvalidated = false;
		FIntPoint ViewportSize = FIntPoint::ZeroValue;
	};

	FString CaptureAuditJson(
		const bool bSuccess,
		const FString& Error,
		const FLevelViewportCaptureAudit& Audit)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetBoolField(TEXT("success"), bSuccess);
		Result->SetStringField(TEXT("error"), Error);
		Result->SetStringField(TEXT("operation"), TEXT("PrepareLevelViewportForCapture"));
		Result->SetBoolField(TEXT("level_editor_tab_foreground"), Audit.bLevelEditorTabForeground);
		Result->SetBoolField(TEXT("level_editor_tab_visible"), Audit.bLevelEditorTabVisible);
		Result->SetBoolField(TEXT("slate_window_active"), Audit.bSlateWindowActive);
		Result->SetBoolField(TEXT("viewport_visible"), Audit.bViewportVisible);
		Result->SetBoolField(TEXT("viewport_focused"), Audit.bViewportFocused);
		Result->SetBoolField(TEXT("invalidated"), Audit.bInvalidated);
		Result->SetNumberField(TEXT("viewport_width"), Audit.ViewportSize.X);
		Result->SetNumberField(TEXT("viewport_height"), Audit.ViewportSize.Y);

		FString Output;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output);
		FJsonSerializer::Serialize(Result, Writer);
		return Output;
	}

	struct FHudLayerFrame
	{
		FIntPoint Size = FIntPoint::ZeroValue;
		TArray<FColor> Pixels;
	};

	struct FHudLayerDifference
	{
		double MeanAbsoluteError = 0.0;
		double RootMeanSquareError = 0.0;
		double PeakSignalToNoiseRatio = 0.0;
		int32 MaximumChannelError = 0;
	};

	enum class EHudLayerBlendMode : uint8
	{
		StraightAlpha,
		PremultipliedAlpha,
		RepeatedAlpha
	};

	bool SavePng(
		const FString& Filename,
		const FHudLayerFrame& Frame)
	{
		if (Frame.Size.X <= 0
			|| Frame.Size.Y <= 0
			|| Frame.Pixels.Num() != Frame.Size.X * Frame.Size.Y)
		{
			return false;
		}
		TArray64<uint8> Compressed;
		FImageUtils::PNGCompressImageArray(
			Frame.Size.X,
			Frame.Size.Y,
			TArrayView64<const FColor>(Frame.Pixels.GetData(), Frame.Pixels.Num()),
			Compressed);
		return !Compressed.IsEmpty()
			&& FFileHelper::SaveArrayToFile(Compressed, *Filename);
	}

	bool CaptureWidgetFrame(
		const TSharedRef<SWidget>& SlateWidget,
		const FIntPoint Size,
		FHudLayerFrame& OutFrame,
		FString& OutError)
	{
		if (Size.X <= 0 || Size.Y <= 0)
		{
			OutError = TEXT("HUD draw size is invalid");
			return false;
		}
		FWidgetRenderer Renderer(true, true);
		TStrongObjectPtr<UTextureRenderTarget2D> RenderTarget(
			FWidgetRenderer::CreateTargetFor(
				FVector2D(static_cast<float>(Size.X), static_cast<float>(Size.Y)),
				TF_Bilinear,
				true));
		if (!RenderTarget.IsValid())
		{
			OutError = TEXT("FWidgetRenderer did not create a render target");
			return false;
		}

		const FVector2D DrawSize(static_cast<float>(Size.X), static_cast<float>(Size.Y));
		Renderer.DrawWidget(RenderTarget.Get(), SlateWidget, DrawSize, 0.0f, false);
		FlushRenderingCommands();
		Renderer.DrawWidget(RenderTarget.Get(), SlateWidget, DrawSize, 0.0f, false);
		FlushRenderingCommands();
		FTextureRenderTargetResource* Resource =
			RenderTarget->GameThread_GetRenderTargetResource();
		if (!Resource)
		{
			OutError = TEXT("HUD render target has no resource");
			return false;
		}
		FReadSurfaceDataFlags ReadFlags(RCM_UNorm);
		ReadFlags.SetLinearToGamma(false);
		OutFrame.Size = Size;
		OutFrame.Pixels.Reset();
		if (!Resource->ReadPixels(OutFrame.Pixels, ReadFlags)
			|| OutFrame.Pixels.Num() != Size.X * Size.Y)
		{
			OutError = TEXT("HUD render target readback failed");
			return false;
		}
		return true;
	}

	uint8 BlendChannel(
		const uint8 Foreground,
		const uint8 Background,
		const uint8 Alpha,
		const EHudLayerBlendMode Mode)
	{
		const int32 A = static_cast<int32>(Alpha);
		const int32 InverseA = 255 - A;
		int32 Value = 0;
		switch (Mode)
		{
		case EHudLayerBlendMode::StraightAlpha:
			Value = (static_cast<int32>(Foreground) * A
				+ static_cast<int32>(Background) * InverseA
				+ 127) / 255;
			break;
		case EHudLayerBlendMode::PremultipliedAlpha:
			Value = static_cast<int32>(Foreground)
				+ (static_cast<int32>(Background) * InverseA + 127) / 255;
			break;
		case EHudLayerBlendMode::RepeatedAlpha:
			Value = (static_cast<int32>(Foreground) * A * A
				+ static_cast<int32>(Background) * InverseA * 255
				+ 32512) / 65025;
			break;
		default:
			break;
		}
		return static_cast<uint8>(FMath::Clamp(Value, 0, 255));
	}

	FHudLayerFrame CompositeFrame(
		const FHudLayerFrame& Foreground,
		const FHudLayerFrame& Background,
		const EHudLayerBlendMode Mode)
	{
		FHudLayerFrame Result;
		if (Foreground.Size != Background.Size
			|| Foreground.Pixels.Num() != Background.Pixels.Num())
		{
			return Result;
		}
		Result.Size = Foreground.Size;
		Result.Pixels.SetNumUninitialized(Foreground.Pixels.Num());
		for (int32 Index = 0; Index < Foreground.Pixels.Num(); ++Index)
		{
			const FColor& Fg = Foreground.Pixels[Index];
			const FColor& Bg = Background.Pixels[Index];
			Result.Pixels[Index] = FColor(
				BlendChannel(Fg.R, Bg.R, Fg.A, Mode),
				BlendChannel(Fg.G, Bg.G, Fg.A, Mode),
				BlendChannel(Fg.B, Bg.B, Fg.A, Mode),
				255);
		}
		return Result;
	}

	FHudLayerFrame SolidFrame(
		const FIntPoint Size,
		const FColor Color)
	{
		FHudLayerFrame Result;
		Result.Size = Size;
		Result.Pixels.Init(Color, Size.X * Size.Y);
		return Result;
	}

	FHudLayerFrame AlphaFrame(const FHudLayerFrame& Source)
	{
		FHudLayerFrame Result;
		Result.Size = Source.Size;
		Result.Pixels.SetNumUninitialized(Source.Pixels.Num());
		for (int32 Index = 0; Index < Source.Pixels.Num(); ++Index)
		{
			const uint8 Alpha = Source.Pixels[Index].A;
			Result.Pixels[Index] = FColor(Alpha, Alpha, Alpha, 255);
		}
		return Result;
	}

	FHudLayerDifference CompareRgb(
		const FHudLayerFrame& Reference,
		const FHudLayerFrame& Candidate)
	{
		FHudLayerDifference Result;
		if (Reference.Size != Candidate.Size
			|| Reference.Pixels.Num() != Candidate.Pixels.Num()
			|| Reference.Pixels.IsEmpty())
		{
			return Result;
		}
		double AbsoluteErrorSum = 0.0;
		double SquaredErrorSum = 0.0;
		int32 ChannelCount = 0;
		for (int32 Index = 0; Index < Reference.Pixels.Num(); ++Index)
		{
			const int32 Errors[3] = {
				FMath::Abs(static_cast<int32>(Reference.Pixels[Index].R)
					- static_cast<int32>(Candidate.Pixels[Index].R)),
				FMath::Abs(static_cast<int32>(Reference.Pixels[Index].G)
					- static_cast<int32>(Candidate.Pixels[Index].G)),
				FMath::Abs(static_cast<int32>(Reference.Pixels[Index].B)
					- static_cast<int32>(Candidate.Pixels[Index].B))
			};
			for (const int32 Error : Errors)
			{
				AbsoluteErrorSum += static_cast<double>(Error);
				SquaredErrorSum += static_cast<double>(Error * Error);
				Result.MaximumChannelError = FMath::Max(
					Result.MaximumChannelError,
					Error);
				++ChannelCount;
			}
		}
		Result.MeanAbsoluteError = AbsoluteErrorSum / static_cast<double>(ChannelCount);
		Result.RootMeanSquareError = FMath::Sqrt(
			SquaredErrorSum / static_cast<double>(ChannelCount));
		Result.PeakSignalToNoiseRatio = Result.RootMeanSquareError <= SMALL_NUMBER
			? 100.0
			: 20.0 * FMath::LogX(
				10.0,
				255.0 / Result.RootMeanSquareError);
		return Result;
	}

	TSharedRef<FJsonObject> DifferenceJson(
		const FHudLayerDifference& Difference)
	{
		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		Result->SetNumberField(TEXT("mean_absolute_error"), Difference.MeanAbsoluteError);
		Result->SetNumberField(TEXT("root_mean_square_error"), Difference.RootMeanSquareError);
		Result->SetNumberField(TEXT("peak_signal_to_noise_ratio_db"), Difference.PeakSignalToNoiseRatio);
		Result->SetNumberField(TEXT("maximum_channel_error"), Difference.MaximumChannelError);
		return Result;
	}

	TSharedRef<FJsonObject> AlphaStatisticsJson(const FHudLayerFrame& Frame)
	{
		TArray<int64> Histogram;
		Histogram.Init(0, 256);
		int64 Transparent = 0;
		int64 Soft = 0;
		int64 Opaque = 0;
		double AlphaSum = 0.0;
		for (const FColor& Pixel : Frame.Pixels)
		{
			++Histogram[Pixel.A];
			AlphaSum += static_cast<double>(Pixel.A);
			if (Pixel.A <= 1)
			{
				++Transparent;
			}
			else if (Pixel.A >= 254)
			{
				++Opaque;
			}
			else
			{
				++Soft;
			}
		}

		TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
		const double PixelCount = static_cast<double>(FMath::Max(1, Frame.Pixels.Num()));
		Result->SetNumberField(TEXT("pixel_count"), Frame.Pixels.Num());
		Result->SetNumberField(TEXT("transparent_pixel_count"), Transparent);
		Result->SetNumberField(TEXT("soft_alpha_pixel_count"), Soft);
		Result->SetNumberField(TEXT("opaque_pixel_count"), Opaque);
		Result->SetNumberField(TEXT("transparent_ratio"), static_cast<double>(Transparent) / PixelCount);
		Result->SetNumberField(TEXT("soft_alpha_ratio"), static_cast<double>(Soft) / PixelCount);
		Result->SetNumberField(TEXT("opaque_ratio"), static_cast<double>(Opaque) / PixelCount);
		Result->SetNumberField(TEXT("mean_alpha"), AlphaSum / PixelCount);
		TArray<TSharedPtr<FJsonValue>> HistogramValues;
		HistogramValues.Reserve(Histogram.Num());
		for (const int64 Count : Histogram)
		{
			HistogramValues.Add(MakeShared<FJsonValueNumber>(static_cast<double>(Count)));
		}
		Result->SetArrayField(TEXT("histogram_0_255"), HistogramValues);
		return Result;
	}

	void PrimeWidgetTextureResources(UWidgetTree* WidgetTree)
	{
		if (!WidgetTree)
		{
			return;
		}
		TSet<UTexture*> Textures;
		WidgetTree->ForEachWidget([&Textures](UWidget* Widget)
		{
			UObject* ResourceObject = nullptr;
			if (const UBorder* Border = Cast<UBorder>(Widget))
			{
				ResourceObject = Border->Background.GetResourceObject();
			}
			else if (const UImage* Image = Cast<UImage>(Widget))
			{
				ResourceObject = Image->GetBrush().GetResourceObject();
			}
			if (UTexture* Texture = Cast<UTexture>(ResourceObject))
			{
				Textures.Add(Texture);
			}
		});
		for (UTexture* Texture : Textures)
		{
			if (!Texture)
			{
				continue;
			}
			Texture->SetForceMipLevelsToBeResident(30.0f);
			Texture->UpdateResource();
		}
		FlushRenderingCommands();
	}

	bool CaptureHudStateLayers(
		UGameXXKDesktopTrainingWorkbenchWidget* Widget,
		const FString& Label,
		const FString& OutputDirectory,
		TSharedRef<FJsonObject>& OutReport,
		FString& OutError)
	{
		if (!Widget)
		{
			OutError = TEXT("Workbench widget is unavailable");
			return false;
		}
		const FVector2D DrawSizeVector = Widget->GetDesktopWindowSizeForHost();
		const FIntPoint DrawSize(
			FMath::Max(1, FMath::RoundToInt(DrawSizeVector.X)),
			FMath::Max(1, FMath::RoundToInt(DrawSizeVector.Y)));
		Widget->TakeWidget();
		if (!Widget->WidgetTree)
		{
			OutError = TEXT("Workbench widget tree is unavailable");
			return false;
		}
		UWidget* RootWidget = Widget->WidgetTree->RootWidget;
		if (!RootWidget)
		{
			OutError = TEXT("Workbench root widget is unavailable");
			return false;
		}
		const TSharedRef<SWidget> SlateWidget = RootWidget->TakeWidget();
		PrimeWidgetTextureResources(Widget->WidgetTree);
		UWidget* Background = Widget->WidgetTree->FindWidget(TEXT("WorkbenchBackground"));
		UPanelWidget* Parent = Background ? Background->GetParent() : nullptr;
		if (!Background || !Parent)
		{
			OutError = TEXT("WorkbenchBackground or its parent panel is unavailable");
			return false;
		}

		struct FVisibilitySnapshot
		{
			TWeakObjectPtr<UWidget> Widget;
			ESlateVisibility Visibility = ESlateVisibility::Visible;
		};
		TArray<FVisibilitySnapshot> VisibilitySnapshots;
		VisibilitySnapshots.Reserve(Parent->GetChildrenCount());
		for (int32 ChildIndex = 0; ChildIndex < Parent->GetChildrenCount(); ++ChildIndex)
		{
			if (UWidget* Child = Parent->GetChildAt(ChildIndex))
			{
				VisibilitySnapshots.Add({Child, Child->GetVisibility()});
			}
		}
		const auto RestoreVisibility = [&VisibilitySnapshots]()
		{
			for (const FVisibilitySnapshot& Snapshot : VisibilitySnapshots)
			{
				if (Snapshot.Widget.IsValid())
				{
					Snapshot.Widget->SetVisibility(Snapshot.Visibility);
				}
			}
		};

		FHudLayerFrame FullFrame;
		if (!CaptureWidgetFrame(SlateWidget, DrawSize, FullFrame, OutError))
		{
			RestoreVisibility();
			return false;
		}
		const bool bFullFrameHasVisiblePixels = FullFrame.Pixels.ContainsByPredicate(
			[](const FColor& Pixel)
			{
				return Pixel.A > 0 || Pixel.R > 0 || Pixel.G > 0 || Pixel.B > 0;
			});
		if (!bFullFrameHasVisiblePixels)
		{
			RestoreVisibility();
			OutError = FString::Printf(
				TEXT("%s full reference frame is empty"),
				*Label);
			return false;
		}

		for (const FVisibilitySnapshot& Snapshot : VisibilitySnapshots)
		{
			if (Snapshot.Widget.IsValid())
			{
				Snapshot.Widget->SetVisibility(
					Snapshot.Widget.Get() == Background
						? ESlateVisibility::Visible
						: ESlateVisibility::Collapsed);
			}
		}
		FHudLayerFrame BackgroundFrame;
		if (!CaptureWidgetFrame(SlateWidget, DrawSize, BackgroundFrame, OutError))
		{
			RestoreVisibility();
			return false;
		}

		RestoreVisibility();
		Background->SetVisibility(ESlateVisibility::Collapsed);
		FHudLayerFrame ForegroundFrame;
		const bool bForegroundCaptured = CaptureWidgetFrame(
			SlateWidget,
			DrawSize,
			ForegroundFrame,
			OutError);
		RestoreVisibility();
		if (!bForegroundCaptured)
		{
			return false;
		}

		const FHudLayerFrame StraightRecomposition = CompositeFrame(
			ForegroundFrame,
			BackgroundFrame,
			EHudLayerBlendMode::StraightAlpha);
		const FHudLayerFrame PremultipliedRecomposition = CompositeFrame(
			ForegroundFrame,
			BackgroundFrame,
			EHudLayerBlendMode::PremultipliedAlpha);
		const FHudLayerFrame RepeatedAlphaRecomposition = CompositeFrame(
			ForegroundFrame,
			BackgroundFrame,
			EHudLayerBlendMode::RepeatedAlpha);

		const FHudLayerFrame DesktopBlack = SolidFrame(DrawSize, FColor::Black);
		const FHudLayerFrame DesktopGray = SolidFrame(DrawSize, FColor(96, 96, 96, 255));
		const FHudLayerFrame DesktopWhite = SolidFrame(DrawSize, FColor::White);
		const FHudLayerFrame Alpha = AlphaFrame(ForegroundFrame);

		struct FNamedFrame
		{
			FString Suffix;
			const FHudLayerFrame* Frame = nullptr;
		};
		const TArray<FNamedFrame> Frames = {
			{TEXT("full_reference"), &FullFrame},
			{TEXT("root_background_only"), &BackgroundFrame},
			{TEXT("foreground_rgba"), &ForegroundFrame},
			{TEXT("foreground_alpha"), &Alpha},
			{TEXT("recompose_straight"), &StraightRecomposition},
			{TEXT("recompose_premultiplied"), &PremultipliedRecomposition},
			{TEXT("recompose_repeated_alpha"), &RepeatedAlphaRecomposition}
		};
		for (const FNamedFrame& NamedFrame : Frames)
		{
			const FString Filename = OutputDirectory / FString::Printf(
				TEXT("%s_%s.png"),
				*Label,
				*NamedFrame.Suffix);
			if (!NamedFrame.Frame || !SavePng(Filename, *NamedFrame.Frame))
			{
				OutError = FString::Printf(TEXT("Failed to save %s"), *Filename);
				return false;
			}
		}

		const TArray<TPair<FString, FHudLayerFrame>> DesktopFrames = {
			{TEXT("black_straight"), CompositeFrame(ForegroundFrame, DesktopBlack, EHudLayerBlendMode::StraightAlpha)},
			{TEXT("black_premultiplied"), CompositeFrame(ForegroundFrame, DesktopBlack, EHudLayerBlendMode::PremultipliedAlpha)},
			{TEXT("gray_straight"), CompositeFrame(ForegroundFrame, DesktopGray, EHudLayerBlendMode::StraightAlpha)},
			{TEXT("gray_premultiplied"), CompositeFrame(ForegroundFrame, DesktopGray, EHudLayerBlendMode::PremultipliedAlpha)},
			{TEXT("white_straight"), CompositeFrame(ForegroundFrame, DesktopWhite, EHudLayerBlendMode::StraightAlpha)},
			{TEXT("white_premultiplied"), CompositeFrame(ForegroundFrame, DesktopWhite, EHudLayerBlendMode::PremultipliedAlpha)}
		};
		for (const TPair<FString, FHudLayerFrame>& DesktopFrame : DesktopFrames)
		{
			const FString Filename = OutputDirectory / FString::Printf(
				TEXT("%s_desktop_%s.png"),
				*Label,
				*DesktopFrame.Key);
			if (!SavePng(Filename, DesktopFrame.Value))
			{
				OutError = FString::Printf(TEXT("Failed to save %s"), *Filename);
				return false;
			}
		}

		OutReport->SetStringField(TEXT("label"), Label);
		OutReport->SetNumberField(TEXT("width"), DrawSize.X);
		OutReport->SetNumberField(TEXT("height"), DrawSize.Y);
		OutReport->SetObjectField(
			TEXT("foreground_alpha"),
			AlphaStatisticsJson(ForegroundFrame));
		OutReport->SetObjectField(
			TEXT("straight_recomposition_error"),
			DifferenceJson(CompareRgb(FullFrame, StraightRecomposition)));
		OutReport->SetObjectField(
			TEXT("premultiplied_recomposition_error"),
			DifferenceJson(CompareRgb(FullFrame, PremultipliedRecomposition)));
		OutReport->SetObjectField(
			TEXT("repeated_alpha_recomposition_error"),
			DifferenceJson(CompareRgb(FullFrame, RepeatedAlphaRecomposition)));
		return true;
	}
}

FString UGameXXKEditorCaptureAutomationLibrary::PrepareLevelViewportForCapture()
{
	FLevelViewportCaptureAudit Audit;
	if (!FSlateApplication::IsInitialized())
	{
		return CaptureAuditJson(false, TEXT("Slate application is not initialized"), Audit);
	}

	FLevelEditorModule* LevelEditorModule =
		FModuleManager::Get().LoadModulePtr<FLevelEditorModule>(TEXT("LevelEditor"));
	if (!LevelEditorModule)
	{
		return CaptureAuditJson(false, TEXT("LevelEditor module is unavailable"), Audit);
	}

	const TSharedPtr<SDockTab> LevelEditorTab = LevelEditorModule->GetLevelEditorTab();
	if (!LevelEditorTab.IsValid())
	{
		return CaptureAuditJson(false, TEXT("LevelEditor main tab is unavailable"), Audit);
	}
	LevelEditorTab->ActivateInParent(ETabActivationCause::SetDirectly);
	LevelEditorModule->FocusViewport();

	const TSharedPtr<SLevelViewport> LevelViewport =
		LevelEditorModule->GetFirstActiveLevelViewport();
	if (!LevelViewport.IsValid())
	{
		return CaptureAuditJson(false, TEXT("active Level Editor viewport is unavailable"), Audit);
	}
	LevelViewport->SetKeyboardFocusToThisViewport();

	Audit.bLevelEditorTabForeground = LevelEditorTab->IsForeground();
	Audit.bLevelEditorTabVisible = LevelEditorTab->GetVisibility().IsVisible();
	Audit.bViewportVisible = LevelViewport->GetVisibility().IsVisible();
	Audit.bViewportFocused =
		LevelViewport->HasKeyboardFocus() || LevelViewport->HasFocusedDescendants();

	FSlateApplication& SlateApplication = FSlateApplication::Get();
	const TSharedPtr<SWindow> ViewportWindow =
		SlateApplication.FindWidgetWindow(LevelViewport.ToSharedRef());
	const TSharedPtr<SWindow> ActiveWindow = SlateApplication.GetActiveTopLevelWindow();
	Audit.bSlateWindowActive =
		ViewportWindow.IsValid() && ActiveWindow.IsValid() && ViewportWindow == ActiveWindow;

	FViewport* NativeViewport = LevelViewport->GetActiveViewport();
	if (!NativeViewport)
	{
		return CaptureAuditJson(false, TEXT("active Level Editor viewport has no render viewport"), Audit);
	}
	Audit.ViewportSize = NativeViewport->GetSizeXY();
	NativeViewport->InvalidateDisplay();
	Audit.bInvalidated = true;

	const bool bSuccess =
		Audit.bLevelEditorTabForeground
		&& Audit.bLevelEditorTabVisible
		&& Audit.bSlateWindowActive
		&& Audit.bViewportVisible
		&& Audit.bViewportFocused
		&& Audit.ViewportSize.X > 0
		&& Audit.ViewportSize.Y > 0
		&& Audit.bInvalidated;
	if (!bSuccess)
	{
		return CaptureAuditJson(
			false,
			TEXT("Level Editor viewport did not satisfy foreground/visibility/focus/size requirements"),
			Audit);
	}
	return CaptureAuditJson(true, TEXT(""), Audit);
}

FString UGameXXKEditorCaptureAutomationLibrary::CaptureDesktopHudLayerAudit(
	const FString& OutputDirectory)
{
	TSharedRef<FJsonObject> Result = MakeShared<FJsonObject>();
	Result->SetStringField(TEXT("operation"), TEXT("CaptureDesktopHudLayerAudit"));
	if (!FSlateApplication::IsInitialized() || !FApp::CanEverRender())
	{
		Result->SetBoolField(TEXT("success"), false);
		Result->SetStringField(TEXT("error"), TEXT("Slate rendering is unavailable"));
		FString Json;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> Writer =
			TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Json);
		FJsonSerializer::Serialize(Result, Writer);
		return Json;
	}

	const FString ResolvedOutputDirectory = OutputDirectory.IsEmpty()
		? FPaths::ProjectSavedDir()
			/ TEXT("HudLayerAudit")
			/ FDateTime::Now().ToString(TEXT("%Y%m%d-%H%M%S"))
		: FPaths::ConvertRelativePathToFull(OutputDirectory);
	IFileManager::Get().MakeDirectory(*ResolvedOutputDirectory, true);
	FString Error;
	FHudLayerFrame RendererControlFrame;
	const TSharedRef<SWidget> RendererControlWidget =
		SNew(SBorder)
		.BorderImage(FCoreStyle::Get().GetBrush(TEXT("WhiteBrush")))
		.BorderBackgroundColor(FLinearColor::Red);
	if (!CaptureWidgetFrame(
		RendererControlWidget,
		FIntPoint(64, 64),
		RendererControlFrame,
		Error))
	{
		Error = FString::Printf(TEXT("Renderer control failed: %s"), *Error);
	}
	else
	{
		const bool bRendererControlHasPixels = RendererControlFrame.Pixels.ContainsByPredicate(
			[](const FColor& Pixel)
			{
				return Pixel.A > 0 || Pixel.R > 0 || Pixel.G > 0 || Pixel.B > 0;
			});
		const FString ControlFilename = ResolvedOutputDirectory / TEXT("renderer_control.png");
		if (!bRendererControlHasPixels)
		{
			Error = TEXT("Renderer control frame is empty");
		}
		else if (!SavePng(ControlFilename, RendererControlFrame))
		{
			Error = TEXT("Failed to save renderer control frame");
		}
	}

	TStrongObjectPtr<UGameInstance> GameInstance(NewObject<UGameInstance>());
	UGameXXKMVPSubsystem* Subsystem = NewObject<UGameXXKMVPSubsystem>(GameInstance.Get());
	TStrongObjectPtr<UGameXXKDesktopTrainingWorkbenchWidget> Widget(
		NewObject<UGameXXKDesktopTrainingWorkbenchWidget>());
	if (Error.IsEmpty()
		&& (!GameInstance.IsValid()
		|| !Subsystem
		|| !Widget.IsValid()
		|| !Subsystem->StartGame()))
	{
		Error = TEXT("Failed to create the deterministic desktop HUD fixture");
	}

	TArray<TSharedPtr<FJsonValue>> StateReports;
	if (Error.IsEmpty())
	{
		Widget->SetMVPSubsystem(Subsystem);
		Widget->InitializeDesktopPresentationHostSize(FVector2D(1920.0f, 1020.0f));
		if (!Widget->OpenWorkbench())
		{
			Error = TEXT("Failed to open the deterministic desktop HUD fixture");
		}
	}
	if (Error.IsEmpty())
	{
		Widget->TakeWidget();
		Widget->ConstructForTest();
		Widget->SetVisibility(ESlateVisibility::Visible);
		Widget->SimulateViewportReattachForTest();
	}
	if (Error.IsEmpty())
	{
		TSharedRef<FJsonObject> CollapsedReport = MakeShared<FJsonObject>();
		if (CaptureHudStateLayers(
			Widget.Get(),
			TEXT("collapsed"),
			ResolvedOutputDirectory,
			CollapsedReport,
			Error))
		{
			StateReports.Add(MakeShared<FJsonValueObject>(CollapsedReport));
		}
	}
	if (Error.IsEmpty())
	{
		if (!Widget->OpenBackpack())
		{
			Error = TEXT("Failed to expand the deterministic desktop HUD fixture");
		}
		else
		{
			Widget->SimulateViewportReattachForTest();
			TSharedRef<FJsonObject> ExpandedReport = MakeShared<FJsonObject>();
			if (CaptureHudStateLayers(
				Widget.Get(),
				TEXT("expanded"),
				ResolvedOutputDirectory,
				ExpandedReport,
				Error))
			{
				StateReports.Add(MakeShared<FJsonValueObject>(ExpandedReport));
			}
		}
	}

	Result->SetBoolField(TEXT("success"), Error.IsEmpty());
	Result->SetStringField(TEXT("error"), Error);
	Result->SetStringField(TEXT("output_directory"), ResolvedOutputDirectory);
	Result->SetArrayField(TEXT("states"), StateReports);
	const FString ReportFilename = ResolvedOutputDirectory / TEXT("audit.json");
	Result->SetStringField(TEXT("report_file"), ReportFilename);

	FString Json;
	const TSharedRef<TJsonWriter<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>> PrettyWriter =
		TJsonWriterFactory<TCHAR, TPrettyJsonPrintPolicy<TCHAR>>::Create(&Json);
	FJsonSerializer::Serialize(Result, PrettyWriter);
	FFileHelper::SaveStringToFile(Json, *ReportFilename);

	FString CondensedJson;
	const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> CondensedWriter =
		TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&CondensedJson);
	FJsonSerializer::Serialize(Result, CondensedWriter);
	return CondensedJson;
}
