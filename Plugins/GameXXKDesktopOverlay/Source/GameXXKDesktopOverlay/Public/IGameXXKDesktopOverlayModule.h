#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"
#include "RHIDefinitions.h"

class FRDGBuilder;
class FRDGTexture;

class GAMEXXKDESKTOPOVERLAY_API IGameXXKDesktopOverlayModule : public IModuleInterface
{
public:
	static constexpr const TCHAR* OverlayWindowTitle = TEXT("GameXXKDesktopOverlay");

	static bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded(TEXT("GameXXKDesktopOverlay"));
	}

	static IGameXXKDesktopOverlayModule& Get()
	{
		return FModuleManager::LoadModuleChecked<IGameXXKDesktopOverlayModule>(
			TEXT("GameXXKDesktopOverlay"));
	}

	virtual bool IsRuntimeSupported() const = 0;
	virtual void BeginOverlayWindowCreation() = 0;
	virtual bool EndOverlayWindowCreation(void* NativeWindowHandle) = 0;
	virtual bool IsOverlayAttached(void* NativeWindowHandle) const = 0;
	virtual void ReleaseOverlayWindow(void* NativeWindowHandle) = 0;
	virtual void AddTransparentClearPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FRDGTexture* OutputTexture) const = 0;
	virtual FIntRect GetCompositionGlassMarginsForTest() const = 0;
	virtual bool UsesRenderTargetTransparentClearForTest() const = 0;
	virtual FLinearColor GetRenderTargetTransparentClearColorForTest() const = 0;

	virtual bool ShouldInterceptForTest(
		const FString& WindowTitle,
		bool bCreationScopeActive,
		ERHIInterfaceType RHIType) const = 0;
};
