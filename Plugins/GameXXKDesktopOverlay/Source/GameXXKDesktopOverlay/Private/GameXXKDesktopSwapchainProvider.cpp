#include "GameXXKDesktopSwapchainProvider.h"

#include "DynamicRHI.h"
#include "IGameXXKDesktopOverlayModule.h"
#include "Microsoft/AllowMicrosoftPlatformTypes.h"
THIRD_PARTY_INCLUDES_START
#include <dcomp.h>
#include <dwmapi.h>
#include <windows.h>
THIRD_PARTY_INCLUDES_END
#include "Microsoft/HideMicrosoftPlatformTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogGameXXKDesktopOverlay, Log, All);

namespace
{
	FString WindowTitleForHandle(const HWND WindowHandle)
	{
		WCHAR Buffer[128] = {};
		const int32 Length = WindowHandle
			? ::GetWindowTextW(WindowHandle, Buffer, UE_ARRAY_COUNT(Buffer))
			: 0;
		return Length > 0 ? FString(Length, Buffer) : FString();
	}

	void RefreshWindowFrame(const HWND WindowHandle)
	{
		if (WindowHandle && ::IsWindow(WindowHandle))
		{
			::SetWindowPos(
				WindowHandle,
				nullptr,
				0,
				0,
				0,
				0,
				SWP_FRAMECHANGED
					| SWP_NOACTIVATE
					| SWP_NOMOVE
					| SWP_NOOWNERZORDER
					| SWP_NOREDRAW
					| SWP_NOSIZE
					| SWP_NOZORDER);
		}
	}
}

struct FGameXXKDesktopSwapchainProvider::FPrivate
{
	struct FWindowAssociation
	{
		LONG_PTR OriginalExtendedStyle = 0;
		TRefCountPtr<IDXGISwapChain1> SwapChain;
		TRefCountPtr<IDCompositionTarget> Target;
		TRefCountPtr<IDCompositionVisual> Visual;
	};

	bool bCreationScopeActive = false;
	TRefCountPtr<IDCompositionDevice> CompositionDevice;
	TMap<HWND, FWindowAssociation> WindowAssociations;

	bool EnsureCompositionDevice()
	{
		if (CompositionDevice.IsValid())
		{
			return true;
		}
		return SUCCEEDED(::DCompositionCreateDevice(
			nullptr,
			__uuidof(IDCompositionDevice),
			reinterpret_cast<void**>(CompositionDevice.GetInitReference())));
	}

	void RestoreWindowStyle(const HWND WindowHandle, const LONG_PTR OriginalExtendedStyle)
	{
		if (!WindowHandle || !::IsWindow(WindowHandle))
		{
			return;
		}
		::SetWindowLongPtrW(WindowHandle, GWL_EXSTYLE, OriginalExtendedStyle);
		MARGINS Margins = {};
		::DwmExtendFrameIntoClientArea(WindowHandle, &Margins);
		RefreshWindowFrame(WindowHandle);
	}

	void ReleaseWindow(const HWND WindowHandle)
	{
		FWindowAssociation* Association = WindowAssociations.Find(WindowHandle);
		if (!Association)
		{
			return;
		}
		if (Association->Target.IsValid())
		{
			Association->Target->SetRoot(nullptr);
		}
		if (CompositionDevice.IsValid())
		{
			CompositionDevice->Commit();
		}
		const LONG_PTR OriginalExtendedStyle = Association->OriginalExtendedStyle;
		WindowAssociations.Remove(WindowHandle);
		RestoreWindowStyle(WindowHandle, OriginalExtendedStyle);
	}

	void ReleaseAllWindows()
	{
		TArray<HWND> Handles;
		WindowAssociations.GetKeys(Handles);
		for (const HWND WindowHandle : Handles)
		{
			ReleaseWindow(WindowHandle);
		}
		CompositionDevice.SafeRelease();
	}
};

FGameXXKDesktopSwapchainProvider::FGameXXKDesktopSwapchainProvider()
	: Private(MakeUnique<FPrivate>())
{
}

FGameXXKDesktopSwapchainProvider::~FGameXXKDesktopSwapchainProvider()
{
	if (Private)
	{
		Private->ReleaseAllWindows();
	}
}

bool FGameXXKDesktopSwapchainProvider::IsRuntimeSupported() const
{
	BOOL bCompositionEnabled = 0;
	return GDynamicRHI
		&& GDynamicRHI->GetInterfaceType() == ERHIInterfaceType::D3D12
		&& SUCCEEDED(::DwmIsCompositionEnabled(&bCompositionEnabled))
		&& bCompositionEnabled;
}

void FGameXXKDesktopSwapchainProvider::BeginOverlayWindowCreation()
{
	if (Private)
	{
		Private->bCreationScopeActive = true;
	}
}

bool FGameXXKDesktopSwapchainProvider::EndOverlayWindowCreation(void* NativeWindowHandle)
{
	if (!Private)
	{
		return false;
	}
	Private->bCreationScopeActive = false;
	const bool bAttached = NativeWindowHandle
		&& Private->WindowAssociations.Contains(static_cast<HWND>(NativeWindowHandle));
	UE_LOG(
		LogGameXXKDesktopOverlay,
		Log,
		TEXT("Overlay creation ended: handle=%p attached=%s associations=%d"),
		NativeWindowHandle,
		bAttached ? TEXT("true") : TEXT("false"),
		Private->WindowAssociations.Num());
	return bAttached;
}

bool FGameXXKDesktopSwapchainProvider::IsOverlayAttached(void* NativeWindowHandle) const
{
	return Private
		&& NativeWindowHandle
		&& Private->WindowAssociations.Contains(static_cast<HWND>(NativeWindowHandle));
}

void FGameXXKDesktopSwapchainProvider::ReleaseOverlayWindow(void* NativeWindowHandle)
{
	if (Private && NativeWindowHandle)
	{
		Private->ReleaseWindow(static_cast<HWND>(NativeWindowHandle));
	}
}

FIntRect FGameXXKDesktopSwapchainProvider::GetCompositionGlassMarginsForTest() const
{
	return FIntRect(-1, -1, -1, -1);
}

bool FGameXXKDesktopSwapchainProvider::ShouldInterceptForTest(
	const FString& WindowTitle,
	const bool bInCreationScopeActive,
	const ERHIInterfaceType RHIType) const
{
	return bInCreationScopeActive
		&& RHIType == ERHIInterfaceType::D3D12
		&& WindowTitle == IGameXXKDesktopOverlayModule::OverlayWindowTitle;
}

bool FGameXXKDesktopSwapchainProvider::SupportsRHI(const ERHIInterfaceType RHIType) const
{
	return RHIType == ERHIInterfaceType::D3D12;
}

const TCHAR* FGameXXKDesktopSwapchainProvider::GetProviderName() const
{
	return TEXT("GameXXKDesktopOverlay");
}

HRESULT FGameXXKDesktopSwapchainProvider::CreateSwapChainForHwnd(
	IDXGIFactory2* Factory,
	IUnknown* Device,
	HWND WindowHandle,
	const DXGI_SWAP_CHAIN_DESC1* Description,
	const DXGI_SWAP_CHAIN_FULLSCREEN_DESC* FullscreenDescription,
	IDXGIOutput* RestrictToOutput,
	IDXGISwapChain1** OutSwapChain)
{
	if (!Factory || !Device || !WindowHandle || !Description || !OutSwapChain)
	{
		return E_INVALIDARG;
	}
	const FString WindowTitle = WindowTitleForHandle(WindowHandle);
	UE_LOG(
		LogGameXXKDesktopOverlay,
		Log,
		TEXT("Swap-chain request: title='%s' scope=%s hwnd=%p"),
		*WindowTitle,
		Private && Private->bCreationScopeActive ? TEXT("true") : TEXT("false"),
		WindowHandle);
	const bool bIntercept = Private
		&& ShouldInterceptForTest(
			WindowTitle,
			Private->bCreationScopeActive,
			ERHIInterfaceType::D3D12);
	if (!bIntercept)
	{
		return Factory->CreateSwapChainForHwnd(
			Device,
			WindowHandle,
			Description,
			FullscreenDescription,
			RestrictToOutput,
			OutSwapChain);
	}
	if (!IsRuntimeSupported() || !Private->EnsureCompositionDevice())
	{
		UE_LOG(
			LogGameXXKDesktopOverlay,
			Warning,
			TEXT("Overlay composition preflight failed for '%s'"),
			*WindowTitle);
		return Factory->CreateSwapChainForHwnd(
			Device,
			WindowHandle,
			Description,
			FullscreenDescription,
			RestrictToOutput,
			OutSwapChain);
	}
	UE_LOG(
		LogGameXXKDesktopOverlay,
		Log,
		TEXT("Creating DirectComposition swap chain for '%s' size=%ux%u format=%d"),
		*WindowTitle,
		Description->Width,
		Description->Height,
		static_cast<int32>(Description->Format));

	Private->ReleaseWindow(WindowHandle);
	FPrivate::FWindowAssociation Association;
	Association.OriginalExtendedStyle = ::GetWindowLongPtrW(WindowHandle, GWL_EXSTYLE);
	const LONG_PTR CompositionStyle =
		(Association.OriginalExtendedStyle & ~static_cast<LONG_PTR>(WS_EX_COMPOSITED))
		| static_cast<LONG_PTR>(WS_EX_NOREDIRECTIONBITMAP);
	::SetWindowLongPtrW(WindowHandle, GWL_EXSTYLE, CompositionStyle);
	const FIntRect GlassMargins = GetCompositionGlassMarginsForTest();
	MARGINS Margins = {
		GlassMargins.Min.X,
		GlassMargins.Min.Y,
		GlassMargins.Max.X,
		GlassMargins.Max.Y};
	::DwmExtendFrameIntoClientArea(WindowHandle, &Margins);
	RefreshWindowFrame(WindowHandle);

	DXGI_SWAP_CHAIN_DESC1 CompositionDescription = *Description;
	CompositionDescription.Scaling = DXGI_SCALING_STRETCH;
	CompositionDescription.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	CompositionDescription.AlphaMode = DXGI_ALPHA_MODE_PREMULTIPLIED;

	HRESULT Result = Factory->CreateSwapChainForComposition(
		Device,
		&CompositionDescription,
		RestrictToOutput,
		Association.SwapChain.GetInitReference());
	if (SUCCEEDED(Result))
	{
		Result = Private->CompositionDevice->CreateTargetForHwnd(
			WindowHandle,
			1,
			Association.Target.GetInitReference());
	}
	if (SUCCEEDED(Result))
	{
		Result = Private->CompositionDevice->CreateVisual(
			Association.Visual.GetInitReference());
	}
	if (SUCCEEDED(Result))
	{
		Result = Association.Visual->SetContent(Association.SwapChain.GetReference());
	}
	if (SUCCEEDED(Result))
	{
		Result = Association.Target->SetRoot(Association.Visual.GetReference());
	}
	if (SUCCEEDED(Result))
	{
		Result = Private->CompositionDevice->Commit();
	}
	if (SUCCEEDED(Result))
	{
		Private->WindowAssociations.Add(WindowHandle, Association);
		*OutSwapChain = Association.SwapChain.GetReference();
		(*OutSwapChain)->AddRef();
		UE_LOG(LogGameXXKDesktopOverlay, Log, TEXT("DirectComposition overlay attached successfully"));
		return S_OK;
	}

	UE_LOG(
		LogGameXXKDesktopOverlay,
		Error,
		TEXT("DirectComposition overlay attachment failed: HRESULT=0x%08x"),
		static_cast<uint32>(Result));
	Private->RestoreWindowStyle(WindowHandle, Association.OriginalExtendedStyle);
	return Factory->CreateSwapChainForHwnd(
		Device,
		WindowHandle,
		Description,
		FullscreenDescription,
		RestrictToOutput,
		OutSwapChain);
}

HRESULT FGameXXKDesktopSwapchainProvider::CreateSwapChain(
	IDXGIFactory* Factory,
	IUnknown* Device,
	DXGI_SWAP_CHAIN_DESC* Description,
	IDXGISwapChain** OutSwapChain)
{
	if (!Factory || !Device || !Description || !OutSwapChain)
	{
		return E_INVALIDARG;
	}
	return Factory->CreateSwapChain(Device, Description, OutSwapChain);
}
