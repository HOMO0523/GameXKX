#include "GameXXKEditorCaptureAutomationLibrary.h"

#include "Framework/Application/SlateApplication.h"
#include "LevelEditor.h"
#include "Modules/ModuleManager.h"
#include "SLevelViewport.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UnrealClient.h"
#include "Widgets/Docking/SDockTab.h"
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
