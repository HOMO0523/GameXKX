// GameXXKLauncher.cpp
//
// Root-level entry point for the packaged GameXXK playtest build.
//
// Why this exists instead of a .lnk or the UE launcher stub:
//
//  * The UE stub that UBT generates (Windows\<Target>.exe) decides whether to show
//    the "Microsoft Visual C++ 2015-2022 Redistributable is required" dialog by
//    reading HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64. Nothing
//    inside the package can satisfy that check, and the stub has no fallback for a
//    missing installer, so it is removed from the package entirely.
//
//  * A .lnk cannot do this job either: WScript.Shell bakes in an absolute target,
//    and Windows will not resolve a hand-written RELATIVE_PATH-only shortcut once
//    the archive is extracted somewhere else. Verified experimentally.
//
// So the entry point is a real executable that resolves everything from its own
// location. It touches no registry key and installs nothing: the VC++ runtime is
// shipped app-local next to the game executable, where the Win32 loader finds it
// before System32.
//
// Build: scripts/build_launcher.ps1 (cl.exe + rc.exe, see that script for flags).

#include <windows.h>
// WIN32_LEAN_AND_MEAN (set on the command line) pulls shellapi.h out of windows.h,
// but CommandLineToArgvW and ShellExecuteExW both live there.
#include <shellapi.h>
#include <shlwapi.h>
#include <strsafe.h>

namespace
{
    // Folder holding the real game executable, relative to this launcher.
    const wchar_t* const kGameBinRelative = L"Windows\\GameXXK\\Binaries\\Win64";

    // Preferred executable names, in order. The staged name depends on the build
    // flavour (ShippingF10 -> GameXXKDev-Win64-Shipping.exe), so a pattern search
    // is used as a fallback to keep this binary flavour-agnostic.
    const wchar_t* const kPreferredExes[] = {
        L"GameXXKDev-Win64-Shipping.exe",
        L"GameXXK-Win64-Shipping.exe",
        L"GameXXK-Win64-Development.exe",
    };

    void Fail(const wchar_t* detail)
    {
        wchar_t message[1024];
        if (FAILED(StringCchPrintfW(message, ARRAYSIZE(message),
            L"找不到游戏程序，无法启动。\n\n"
            L"请确认压缩包已完整解压，并且没有把 GameXXK.exe 单独移出来。\n"
            L"完整解压后，GameXXK.exe 应该和 Windows 文件夹在同一层。\n\n"
            L"%s", detail)))
        {
            StringCchCopyW(message, ARRAYSIZE(message), L"GameXXK launcher failed.");
        }
        MessageBoxW(nullptr, message, L"GameXXK", MB_OK | MB_ICONERROR);
    }

    // Directory containing this launcher, with a trailing separator.
    bool GetLauncherDir(wchar_t* out, size_t cch)
    {
        const DWORD len = GetModuleFileNameW(nullptr, out, static_cast<DWORD>(cch));
        if (len == 0 || len >= cch)
        {
            return false;
        }
        return PathRemoveFileSpecW(out) != FALSE;
    }

    bool JoinPath(wchar_t* out, size_t cch, const wchar_t* dir, const wchar_t* leaf)
    {
        return SUCCEEDED(StringCchPrintfW(out, cch, L"%s\\%s", dir, leaf));
    }

    // Looks for the game executable in the expected folder.
    bool FindGameExe(const wchar_t* binDir, wchar_t* out, size_t cch)
    {
        for (const wchar_t* candidate : kPreferredExes)
        {
            if (!JoinPath(out, cch, binDir, candidate))
            {
                return false;
            }
            if (PathFileExistsW(out))
            {
                return true;
            }
        }

        // Fallback: any *Win64*.exe in the folder, then any .exe at all.
        const wchar_t* patterns[] = { L"*Win64*.exe", L"*.exe" };
        for (const wchar_t* pattern : patterns)
        {
            wchar_t search[MAX_PATH];
            if (!JoinPath(search, ARRAYSIZE(search), binDir, pattern))
            {
                continue;
            }
            WIN32_FIND_DATAW found;
            HANDLE handle = FindFirstFileW(search, &found);
            if (handle == INVALID_HANDLE_VALUE)
            {
                continue;
            }
            const bool ok = JoinPath(out, cch, binDir, found.cFileName);
            FindClose(handle);
            if (ok)
            {
                return true;
            }
        }
        return false;
    }

    // Rebuilds the command line minus argv[0], so -UserDir and friends still work.
    bool BuildArguments(wchar_t* out, size_t cch)
    {
        int argc = 0;
        wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);
        if (argv == nullptr)
        {
            return SUCCEEDED(StringCchCopyW(out, cch, L""));
        }

        out[0] = L'\0';
        bool ok = true;
        for (int i = 1; i < argc && ok; ++i)
        {
            if (i > 1)
            {
                ok = SUCCEEDED(StringCchCatW(out, cch, L" "));
            }
            if (ok)
            {
                ok = SUCCEEDED(StringCchCatW(out, cch, L"\"")) &&
                     SUCCEEDED(StringCchCatW(out, cch, argv[i])) &&
                     SUCCEEDED(StringCchCatW(out, cch, L"\""));
            }
        }
        LocalFree(argv);
        return ok;
    }
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    wchar_t launcherDir[MAX_PATH];
    if (!GetLauncherDir(launcherDir, ARRAYSIZE(launcherDir)))
    {
        Fail(L"无法确定程序所在目录。");
        return 1;
    }

    wchar_t binDir[MAX_PATH];
    if (!JoinPath(binDir, ARRAYSIZE(binDir), launcherDir, kGameBinRelative))
    {
        Fail(L"路径过长。");
        return 1;
    }

    wchar_t gameExe[MAX_PATH];
    if (!FindGameExe(binDir, gameExe, ARRAYSIZE(gameExe)))
    {
        Fail(binDir);
        return 1;
    }

    wchar_t arguments[2048];
    BuildArguments(arguments, ARRAYSIZE(arguments));

    // Run the game from its own folder; the engine resolves its content paths
    // relative to the executable, not the caller's working directory.
    wchar_t workDir[MAX_PATH];
    if (FAILED(StringCchCopyW(workDir, ARRAYSIZE(workDir), gameExe)))
    {
        Fail(L"路径过长。");
        return 1;
    }
    PathRemoveFileSpecW(workDir);

    SHELLEXECUTEINFOW exec = {};
    exec.cbSize = sizeof(exec);
    exec.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_FLAG_NO_UI;
    exec.lpFile = gameExe;
    exec.lpParameters = arguments[0] != L'\0' ? arguments : nullptr;
    exec.lpDirectory = workDir;
    exec.nShow = SW_SHOWNORMAL;

    if (!ShellExecuteExW(&exec))
    {
        wchar_t detail[MAX_PATH + 128];
        StringCchPrintfW(detail, ARRAYSIZE(detail), L"%s\n(错误码 %lu)", gameExe, GetLastError());
        Fail(detail);
        return 1;
    }

    if (exec.hProcess != nullptr)
    {
        CloseHandle(exec.hProcess);
    }
    return 0;
}
