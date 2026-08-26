#pragma once

#include "CoreMinimal.h"
#include "Windows/IDXGISwapchainProvider.h"

class FGameXXKDesktopSwapchainProvider final : public IDXGISwapchainProvider
{
public:
	FGameXXKDesktopSwapchainProvider();
	~FGameXXKDesktopSwapchainProvider();

	bool IsRuntimeSupported() const;
	void BeginOverlayWindowCreation();
	bool EndOverlayWindowCreation(void* NativeWindowHandle);
	bool IsOverlayAttached(void* NativeWindowHandle) const;
	void ReleaseOverlayWindow(void* NativeWindowHandle);
	FIntRect GetCompositionGlassMarginsForTest() const;

	bool ShouldInterceptForTest(
		const FString& WindowTitle,
		bool bCreationScopeActive,
		ERHIInterfaceType RHIType) const;

	virtual bool SupportsRHI(ERHIInterfaceType RHIType) const override;
	virtual const TCHAR* GetProviderName() const override;
	virtual HRESULT CreateSwapChainForHwnd(
		IDXGIFactory2* Factory,
		IUnknown* Device,
		HWND WindowHandle,
		const DXGI_SWAP_CHAIN_DESC1* Description,
		const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* FullscreenDescription,
		IDXGIOutput* RestrictToOutput,
		IDXGISwapChain1** OutSwapChain) override;
	virtual HRESULT CreateSwapChain(
		IDXGIFactory* Factory,
		IUnknown* Device,
		DXGI_SWAP_CHAIN_DESC* Description,
		IDXGISwapChain** OutSwapChain) override;

private:
	struct FPrivate;
	TUniquePtr<FPrivate> Private;
};
