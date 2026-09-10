"""Read-only Win32 sizes in the editor thread's actual DPI awareness context."""
import ctypes
from ctypes import wintypes
import json
import sys

u=ctypes.WinDLL('user32');hwnd=int(sys.argv[1])
class RECT(ctypes.Structure):_fields_=[('left',ctypes.c_long),('top',ctypes.c_long),('right',ctypes.c_long),('bottom',ctypes.c_long)]
class MONITORINFO(ctypes.Structure):_fields_=[('cbSize',ctypes.c_ulong),('monitor',RECT),('work',RECT),('flags',ctypes.c_ulong)]
u.GetWindowRect.argtypes=[ctypes.c_void_p,ctypes.POINTER(RECT)];u.GetClientRect.argtypes=[ctypes.c_void_p,ctypes.POINTER(RECT)]
u.MonitorFromWindow.argtypes=[ctypes.c_void_p,ctypes.c_ulong];u.MonitorFromWindow.restype=ctypes.c_void_p
u.GetMonitorInfoW.argtypes=[ctypes.c_void_p,ctypes.POINTER(MONITORINFO)]
u.GetWindowDpiAwarenessContext.argtypes=[ctypes.c_void_p];u.GetWindowDpiAwarenessContext.restype=ctypes.c_void_p
u.GetAwarenessFromDpiAwarenessContext.argtypes=[ctypes.c_void_p];u.GetAwarenessFromDpiAwarenessContext.restype=ctypes.c_int
u.GetDpiForWindow.argtypes=[ctypes.c_void_p];u.GetDpiForWindow.restype=ctypes.c_uint
window=RECT();client=RECT();monitor=MONITORINFO();monitor.cbSize=ctypes.sizeof(MONITORINFO)
assert u.GetWindowRect(hwnd,ctypes.byref(window)) and u.GetClientRect(hwnd,ctypes.byref(client))
assert u.GetMonitorInfoW(u.MonitorFromWindow(hwnd,2),ctypes.byref(monitor))
as_list=lambda r:[r.left,r.top,r.right,r.bottom]
print(json.dumps({'hwnd':hwnd,'window':as_list(window),'client':as_list(client),'workArea':as_list(monitor.work),
    'dpi':u.GetDpiForWindow(hwnd),'awareness':u.GetAwarenessFromDpiAwarenessContext(u.GetWindowDpiAwarenessContext(hwnd))}))
