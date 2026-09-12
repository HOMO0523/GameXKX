<#
.SYNOPSIS
    Builds the root-level GameXXK launcher executable.

.DESCRIPTION
    Compiles SourceArt/UI/AppIcon/GameXXKLauncher.cpp into a small windowed exe with
    the calligraphy icon embedded, and writes it to Saved/Launcher/GameXXK.exe.

    The launcher exists because the package must open on a machine that has no
    machine-wide VC++ redistributable and must show no error dialog:
      * the UE launcher stub is removed (its check is unfixable from inside the
        package and has no fallback when the installer is absent), and
      * a .lnk cannot be shipped pre-made because it would bake in an absolute path.

    It is linked with /MT (static CRT) on purpose. A launcher built against the
    dynamic CRT would itself need VCRUNTIME140.dll, which is precisely the
    dependency it exists to avoid. /MT is the documented "utility program" case for
    the static CRT, and this binary is small enough that it costs almost nothing.

    This is a build step, not a game build step: it needs cl.exe and rc.exe from the
    Visual Studio install, so it is kept separate from the UBT pipeline.
#>
param(
    [string]$VisualStudioRoot = '',
    [string]$WindowsSdkBinRoot = 'C:\Program Files (x86)\Windows Kits\10\bin',
    [string]$WindowsSdkLibRoot = 'C:\Program Files (x86)\Windows Kits\10\Lib'
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$sourceDir = Join-Path $projectRoot 'SourceArt\UI\AppIcon'
$outputDir = Join-Path $projectRoot 'Saved\Launcher'

# --- locate the toolchain -------------------------------------------------
if (-not $VisualStudioRoot) {
    $candidates = @(
        'C:\Program Files\Microsoft Visual Studio\2022\Community',
        'C:\Program Files\Microsoft Visual Studio\2022\Professional',
        'C:\Program Files\Microsoft Visual Studio\2022\Enterprise',
        'C:\Program Files\Microsoft Visual Studio\2022\BuildTools'
    )
    $VisualStudioRoot = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $VisualStudioRoot) { throw 'Visual Studio 2022 not found; pass -VisualStudioRoot.' }
}

$msvcRoot = Get-ChildItem -LiteralPath (Join-Path $VisualStudioRoot 'VC\Tools\MSVC') -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $msvcRoot) { throw "No MSVC toolset under $VisualStudioRoot\VC\Tools\MSVC" }

$clExe = Join-Path $msvcRoot.FullName 'bin\Hostx64\x64\cl.exe'
if (-not (Test-Path -LiteralPath $clExe)) { throw "cl.exe not found: $clExe" }

$sdkVersion = Get-ChildItem -LiteralPath $WindowsSdkLibRoot -Directory |
    Sort-Object Name -Descending | Select-Object -First 1
if (-not $sdkVersion) { throw "No Windows SDK under $WindowsSdkLibRoot" }
$sdkVersionName = $sdkVersion.Name
$rcExe = Join-Path (Join-Path $WindowsSdkBinRoot $sdkVersionName) 'x64\rc.exe'
if (-not (Test-Path -LiteralPath $rcExe)) { throw "rc.exe not found: $rcExe" }

$sdkIncludeRoot = Join-Path (Split-Path $WindowsSdkLibRoot -Parent) "Include\$sdkVersionName"
$includePaths = @(
    (Join-Path $msvcRoot.FullName 'include'),
    (Join-Path $sdkIncludeRoot 'ucrt'),
    (Join-Path $sdkIncludeRoot 'um'),
    (Join-Path $sdkIncludeRoot 'shared')
) | Where-Object { Test-Path -LiteralPath $_ }

$libPaths = @(
    (Join-Path $msvcRoot.FullName 'lib\x64'),
    (Join-Path $WindowsSdkLibRoot "$sdkVersionName\ucrt\x64"),
    (Join-Path $WindowsSdkLibRoot "$sdkVersionName\um\x64")
) | Where-Object { Test-Path -LiteralPath $_ }

if ($includePaths.Count -eq 0) { throw "No include directories resolved under $sdkIncludeRoot" }
if ($libPaths.Count -eq 0) { throw "No library directories resolved under $WindowsSdkLibRoot" }

# --- refresh the .ico when the source art changed -------------------------
$png = Join-Path $sourceDir 'GameXXK-icon-source.png'
$ico = Join-Path $sourceDir 'GameXXK.ico'
$needIco = (-not (Test-Path -LiteralPath $ico)) -or
    ((Test-Path -LiteralPath $png) -and
     ((Get-Item -LiteralPath $png).LastWriteTimeUtc -gt (Get-Item -LiteralPath $ico).LastWriteTimeUtc))
if ($needIco) {
    if (-not (Test-Path -LiteralPath $png)) { throw "Icon missing and no source art to rebuild it: $ico" }
    Write-Output 'Regenerating GameXXK.ico from source art (requires Pillow)'
    $env:PYTHONIOENCODING = 'utf-8'
    python -c @"
from PIL import Image
from pathlib import Path
src = Path(r'$png')
dst = Path(r'$ico')
im = Image.open(src).convert('RGBA')
w, h = im.size
side = min(w, h)
im = im.crop(((w - side) // 2, (h - side) // 2, (w - side) // 2 + side, (h - side) // 2 + side))
im.save(dst, format='ICO', sizes=[(256, 256), (128, 128), (64, 64), (48, 48), (32, 32), (16, 16)])
print(f'wrote {dst}')
"@
    if ($LASTEXITCODE -ne 0) { throw 'Icon regeneration failed' }
}

# --- compile --------------------------------------------------------------
[IO.Directory]::CreateDirectory($outputDir) | Out-Null
$env:INCLUDE = ($includePaths -join ';')
$env:LIB = ($libPaths -join ';')

$resFile = Join-Path $outputDir 'GameXXKLauncher.res'
$exeFile = Join-Path $outputDir 'GameXXK.exe'

Push-Location $outputDir
try {
    Write-Output "Compiling launcher with MSVC $($msvcRoot.Name), SDK $sdkVersionName"
    & $rcExe /nologo /fo$resFile (Join-Path $sourceDir 'GameXXKLauncher.rc')
    if ($LASTEXITCODE -ne 0) { throw "rc.exe failed with exit code $LASTEXITCODE" }

    # /MT matters: see the note at the top of this file.
    & $clExe /nologo /utf-8 /MT /O1 /Os /GS /GL /Gy `
        /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNDEBUG `
        (Join-Path $sourceDir 'GameXXKLauncher.cpp') $resFile `
        /Fe:$exeFile `
        /link /SUBSYSTEM:WINDOWS /LTCG /OPT:REF /OPT:ICF /MACHINE:X64 `
        shlwapi.lib shell32.lib user32.lib
    if ($LASTEXITCODE -ne 0) { throw "cl.exe failed with exit code $LASTEXITCODE" }

    Get-ChildItem -LiteralPath $outputDir -Filter '*.obj' | Remove-Item -Force -ErrorAction SilentlyContinue
}
finally {
    Pop-Location
}

$built = Get-Item -LiteralPath $exeFile
Write-Output ("Launcher built: {0} ({1:N0} bytes)" -f $built.FullName, $built.Length)
