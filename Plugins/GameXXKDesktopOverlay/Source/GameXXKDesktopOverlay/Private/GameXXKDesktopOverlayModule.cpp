#include "IGameXXKDesktopOverlayModule.h"

#include "Features/IModularFeatures.h"
#include "GameXXKDesktopSwapchainProvider.h"
#include "RenderGraphUtils.h"

class FGameXXKDesktopOverlayModule final : public IGameXXKDesktopOverlayModule
{
public:
	virtual void StartupModule() override
	{
		Provider = MakeUnique<FGameXXKDesktopSwapchainProvider>();
		IModularFeatures::Get().RegisterModularFeature(
			IDXGISwapchainProvider::GetModularFeatureName(),
			Provider.Get());
	}

	virtual void ShutdownModule() override
	{
		if (Provider)
		{
			IModularFeatures::Get().UnregisterModularFeature(
				IDXGISwapchainProvider::GetModularFeatureName(),
				Provider.Get());
			Provider.Reset();
		}
	}

	virtual bool IsRuntimeSupported() const override
	{
		return Provider && Provider->IsRuntimeSupported();
	}

	virtual void BeginOverlayWindowCreation() override
	{
		if (Provider)
		{
			Provider->BeginOverlayWindowCreation();
		}
	}

	virtual bool EndOverlayWindowCreation(void* NativeWindowHandle) override
	{
		return Provider && Provider->EndOverlayWindowCreation(NativeWindowHandle);
	}

	virtual bool IsOverlayAttached(void* NativeWindowHandle) const override
	{
		return Provider && Provider->IsOverlayAttached(NativeWindowHandle);
	}

	virtual void ReleaseOverlayWindow(void* NativeWindowHandle) override
	{
		if (Provider)
		{
			Provider->ReleaseOverlayWindow(NativeWindowHandle);
		}
	}

	virtual void AddTransparentClearPass_RenderThread(
		FRDGBuilder& GraphBuilder,
		FRDGTexture* OutputTexture) const override
	{
		if (OutputTexture)
		{
			AddClearRenderTargetPass(
				GraphBuilder,
				OutputTexture,
				FLinearColor::Transparent);
		}
	}

	virtual FIntRect GetCompositionGlassMarginsForTest() const override
	{
		return Provider
			? Provider->GetCompositionGlassMarginsForTest()
			: FIntRect(0, 0, 0, 0);
	}

	virtual bool UsesRenderTargetTransparentClearForTest() const override
	{
		return true;
	}

	virtual FLinearColor GetRenderTargetTransparentClearColorForTest() const override
	{
		return FLinearColor::Transparent;
	}

	virtual bool ShouldInterceptForTest(
		const FString& WindowTitle,
		const bool bCreationScopeActive,
		const ERHIInterfaceType RHIType) const override
	{
		return Provider && Provider->ShouldInterceptForTest(
			WindowTitle,
			bCreationScopeActive,
			RHIType);
	}

private:
	TUniquePtr<FGameXXKDesktopSwapchainProvider> Provider;
};

IMPLEMENT_MODULE(FGameXXKDesktopOverlayModule, GameXXKDesktopOverlay)
