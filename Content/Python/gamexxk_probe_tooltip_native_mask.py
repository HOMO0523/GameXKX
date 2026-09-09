"""Read-only audit of this editor process's visible tooltip HWND regions."""
import ctypes
from ctypes import wintypes
import json
import os

user = ctypes.WinDLL('user32')
gdi = ctypes.WinDLL('gdi32')
user.GetWindowThreadProcessId.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.DWORD)]
user.GetWindowRgn.argtypes = [wintypes.HWND, wintypes.HANDLE]
user.IsWindowVisible.argtypes = [wintypes.HWND]
user.GetClientRect.argtypes = [wintypes.HWND, ctypes.POINTER(wintypes.RECT)]
user.GetWindowTextW.argtypes = [wintypes.HWND, wintypes.LPWSTR, ctypes.c_int]
gdi.CreateRectRgn.argtypes = [ctypes.c_int] * 4
gdi.CreateRectRgn.restype = wintypes.HANDLE
gdi.GetRgnBox.argtypes = [wintypes.HANDLE, ctypes.POINTER(wintypes.RECT)]
gdi.PtInRegion.argtypes = [wintypes.HANDLE, ctypes.c_int, ctypes.c_int]
gdi.DeleteObject.argtypes = [wintypes.HANDLE]
rows = []

@ctypes.WINFUNCTYPE(wintypes.BOOL, wintypes.HWND, wintypes.LPARAM)
def visit(handle, _):
    pid = wintypes.DWORD()
    user.GetWindowThreadProcessId(handle, ctypes.byref(pid))
    if pid.value != os.getpid() or not user.IsWindowVisible(handle):
        return True
    title = ctypes.create_unicode_buffer(256)
    user.GetWindowTextW(handle, title, len(title))
    if title.value:
        return True
    rect, bounds = wintypes.RECT(), wintypes.RECT()
    user.GetClientRect(handle, ctypes.byref(rect))
    region = gdi.CreateRectRgn(0, 0, 0, 0)
    kind = user.GetWindowRgn(handle, region)
    gdi.GetRgnBox(region, ctypes.byref(bounds))
    rows.append({'hwnd': int(handle), 'native_allocation': [rect.right, rect.bottom],
                 'region_type': kind, 'visible_bounds': [bounds.left, bounds.top, bounds.right, bounds.bottom],
                 'top_left_visible': bool(gdi.PtInRegion(region, bounds.left, bounds.top)),
                 'center_visible': bool(gdi.PtInRegion(region, (bounds.left+bounds.right)//2, (bounds.top+bounds.bottom)//2))})
    gdi.DeleteObject(region)
    return True

user.EnumWindows.argtypes = [type(visit), wintypes.LPARAM]
user.EnumWindows(visit, 0)
print(json.dumps(rows))
