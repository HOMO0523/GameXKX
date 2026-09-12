param(
    [string]$EngineRoot = 'D:\UE_5.8',
    [ValidateSet('Development', 'ShippingF10')]
    [string]$Flavor = 'ShippingF10',
    [switch]$SkipBuild,
    [string]$OutputDirectory = '',
    [string]$ReportDirectory = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot "Packaged\$Flavor-$stamp" }
if (-not $ReportDirectory) { $ReportDirectory = Join-Path $projectRoot "Saved\Packaging\$stamp" }
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
$ReportDirectory = [IO.Path]::GetFullPath($ReportDirectory)
$uat = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
if (-not (Test-Path -LiteralPath $uat -PathType Leaf)) { throw "RunUAT not found: $uat" }
$editors = Get-CimInstance Win32_Process -Filter "Name='UnrealEditor.exe' OR Name='UnrealEditor-Cmd.exe'" |
    Where-Object { $_.CommandLine -and $_.CommandLine.Contains($projectFile) }
if ($editors) { throw 'Save player progress and dirty packages through MCP, then close this project editor before packaging.' }
foreach ($directory in @($OutputDirectory, $ReportDirectory, (Join-Path $ReportDirectory 'Temp'))) {
    [IO.Directory]::CreateDirectory($directory) | Out-Null
}

$overrides = @{
    TEMP = (Join-Path $ReportDirectory 'Temp')
    TMP = (Join-Path $ReportDirectory 'Temp')
    UE_SKIP_UBT_SDK_SETUP = '1'
    'UE-LocalDataCachePath' = (Join-Path $projectRoot 'Saved\ImageOptimization\DDC')
    # AutomationTool clears this directory at startup. Keep reports and backups outside it.
    uebp_LogFolder = (Join-Path $ReportDirectory 'UAT')
    uebp_FinalLogFolder = (Join-Path $ReportDirectory 'UAT')
}
$previous = @{}
foreach ($key in $overrides.Keys) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, $overrides[$key], 'Process')
}
$configuration = if ($Flavor -eq 'ShippingF10') { 'Shipping' } else { 'Development' }
$gameTarget = if ($Flavor -eq 'ShippingF10') { 'GameXXKDev' } else { 'GameXXK' }

# --- App-local VC++ runtime (2026-09-11) -----------------------------------
# The UE launcher stub refuses to launch until the VC++ 2015-2022 redistributable
# is registered under HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64,
# so players on a clean machine get the "required component" dialog and cannot
# open the game. UE ships an app-local copy of that runtime; staging it through
# the supported -applocaldirectory switch drops the DLLs beside the game
# executable, where the Win32 loader searches before System32. Launching
# GameXXK\Binaries\Win64\GameXXKDev-Win64-Shipping.exe directly then bypasses
# the stub's registry check entirely: no install, no admin rights, no dialog.
# Only Microsoft.VC.CRT is staged. The UCRT is an operating-system component on
# Windows 10 and later, and XAudio2 2.9 already ships inside the package.
$appLocalSource = Join-Path $EngineRoot 'Engine\Binaries\ThirdParty\AppLocalDependencies\Win64\x64\Microsoft.VC.CRT'
if (-not (Test-Path -LiteralPath $appLocalSource -PathType Container)) {
    throw "App-local VC++ runtime not found: $appLocalSource"
}
$appLocalRoot = Join-Path $projectRoot 'Saved\AppLocalDependencies'
$appLocalTarget = Join-Path $appLocalRoot 'Win64\x64\Microsoft.VC.CRT'
[IO.Directory]::CreateDirectory($appLocalTarget) | Out-Null
$appLocalCount = 0
foreach ($runtimeDll in Get-ChildItem -LiteralPath $appLocalSource -Filter '*.dll' -File) {
    Copy-Item -LiteralPath $runtimeDll.FullName -Destination $appLocalTarget -Force
    $appLocalCount++
}
if ($appLocalCount -eq 0) { throw "No app-local runtime DLLs found in $appLocalSource" }
Write-Output "App-local VC++ runtime: $appLocalCount DLL(s) prepared in $appLocalTarget"

# NOTE: '-prereqs' is deliberately NOT passed.
#
# The vc_redist installer is not shipped because shipping it is worse than not
# shipping it: the launcher stub decides whether to show the "required component"
# dialog by reading
#   HKLM\SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\x64
# and it has no "installer is missing" fallback for the VC++ path (unlike its
# GameInput path). With no installer staged, a player who sees that dialog and
# clicks yes only gets "Couldn't start: ...vc_redist.x64.exe" -- a dead end.
#
# Testers who see any error simply leave, so the requirement is that the game
# opens with no dialog at all. That is achieved by the app-local runtime staged
# above plus removing the stub (done after staging), so no code path ever checks
# the machine-wide runtime. Verified: the running game loads VCRUNTIME140,
# VCRUNTIME140_1 and MSVCP140 from its own directory even when System32 has an
# identical version, because the Win32 loader searches the application directory
# first.
$uatArguments = @(
    'BuildCookRun', "-project=$projectFile", '-noP4', '-unattended', '-utf8output',
    '-platform=Win64', "-clientconfig=$configuration", "-target=$gameTarget", '-cook', '-stage',
    '-pak', '-iostore', '-compressed', '-nodebuginfo', '-archive',
    "-archivedirectory=$OutputDirectory", '-CookPartialGC',
    "-applocaldirectory=$appLocalRoot",
    '-ubtargs=-NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=4'
)
if (-not $SkipBuild) { $uatArguments += '-build' }
$metadata = @{
    started_at = (Get-Date).ToString('o')
    source_commit = (git -C $projectRoot rev-parse HEAD)
    configuration = $configuration
    target = $gameTarget
    flavor = $Flavor
    output = $OutputDirectory
    applocal_runtime_dlls = $appLocalCount
    prerequisites_installer = $false
    launcher_stub = 'removed'
    arguments = $uatArguments
    note = 'Canonical pure-2D desktop, story, route and BattleBoard flow; complete F10; unused images and legacy 3D environments excluded; no bundled symbols; DX11/SM5 retained so DX11-only machines can still launch; VC++ runtime staged app-local and the launcher stub removed so the game opens with no prerequisite dialog on a machine without the redistributable.'
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
try {
    & $uat @uatArguments 2>&1 | Tee-Object -FilePath (Join-Path $ReportDirectory 'build-cook-stage.log')
    $buildResult = $LASTEXITCODE
    $metadata['finished_at'] = (Get-Date).ToString('o')
    $metadata['exit_code'] = $buildResult
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
    if ($buildResult -ne 0) { throw "BuildCookRun failed with exit code $buildResult. See $ReportDirectory" }
    # Any leftover redistributable installers are removed. With '-prereqs' gone
    # they should not be staged at all; this is a belt-and-braces guard so a stale
    # staged build can never leak one back into the package.
    foreach ($staleRedist in @(
            'Windows\Engine\Extras\Redist\en-us\vc_redist.arm64.exe',
            'Windows\Engine\Extras\Redist\en-us\vc_redist.x64.exe')) {
        $stalePath = Join-Path $OutputDirectory $staleRedist
        if (Test-Path -LiteralPath $stalePath -PathType Leaf) {
            $resolvedStale = (Resolve-Path -LiteralPath $stalePath).Path
            if (-not $resolvedStale.StartsWith($OutputDirectory.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
                throw 'Unexpected prerequisite path outside the package'
            }
            Remove-Item -LiteralPath $resolvedStale
            Write-Output "Removed stale prerequisite installer: $staleRedist"
        }
    }

    # Remove the launcher stub. UBT bakes a VC++ runtime check into it that reads
    # the machine-wide redistributable registry key, and it exposes no way to
    # satisfy that check from inside the package. Leaving the stub in place would
    # mean players who double-click it get the "required component" dialog and,
    # with no installer staged, no way forward. The game executable beside the
    # app-local runtime DLLs starts correctly on its own, so the stub is pure risk.
    $launcherStub = Join-Path $OutputDirectory "Windows\$gameTarget.exe"
    if (Test-Path -LiteralPath $launcherStub -PathType Leaf) {
        $resolvedStub = (Resolve-Path -LiteralPath $launcherStub).Path
        if (-not $resolvedStub.StartsWith($OutputDirectory.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected launcher stub path outside the package'
        }
        Remove-Item -LiteralPath $resolvedStub
        Write-Output "Removed launcher stub: Windows\$gameTarget.exe"
    }

    $devTools = Join-Path $OutputDirectory 'DevTools'
    [IO.Directory]::CreateDirectory($devTools) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gamexxk_dev_client.py') -Destination $devTools
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\design\dev-workbench.md') -Destination (Join-Path $devTools 'README.md')
    @"
GameXXK - Pure 2D / $Flavor

启动：双击 GameXXK.exe        ← 就是这个，图标是书法字

F10：打开或收起试炼手札；Esc：关闭面板。
配装、解锁、道具、试武、快照与批测功能均保留。
临时试验开始前会保存正常进度；点击“返回原进度”结束试验。
需要保留试验成果时请使用 Dev 快照。
退出请使用游戏内退出按钮并确认；保存失败时不会退出。

本包包含桌面挂机、剧情、教程、路线和 BattleBoard 的纯 2D 流程。

【不需要安装任何东西】
- 本包已自带 VC++ 运行库，放在游戏程序旁边，不会出现
  "Microsoft Visual C++ Redistributable" 之类的提示框。
- GameXXK.exe 可以放在任何目录；整个文件夹随便改名、换盘符都能用。
- 如果系统提示 "Windows 已保护你的电脑"（SmartScreen），
  请点「更多信息」→「仍要运行」。这是所有未签名程序的通用提示，与本作品无关。

【关于显卡】
桌面浮窗形态需要 DX12（Windows 10 1909+ 或更新，且显卡支持 Shader Model 6）。
只有 DX11 的机器仍可启动，但会是普通窗口，不是桌面浮窗形态。

本机脚本接口说明见 DevTools/README.md；游戏本身不需要安装 Python。
"@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'README.txt') -Encoding utf8

    # Root-level entry point. The UE launcher stub is deleted above, so this is the
    # intended way in; the game executable it starts needs no machine-wide runtime
    # because the DLLs were staged beside it.
    # Note the staged folder is named after the PROJECT, not after the target.
    $projectName = [IO.Path]::GetFileNameWithoutExtension($projectFile)
    $shippingExeName = if ($Flavor -eq 'ShippingF10') {
        "$gameTarget-Win64-Shipping.exe"
    } else {
        "$gameTarget-Win64-Development.exe"
    }
    $gameExeRelative = "Windows\$projectName\Binaries\Win64\$shippingExeName"
    $gameExeFull = Join-Path $OutputDirectory $gameExeRelative
    if (-not (Test-Path -LiteralPath $gameExeFull -PathType Leaf)) {
        throw "Packaged game executable not found: $gameExeRelative"
    }

    # Small unsigned exe that resolves the game relative to its own location and
    # launches it. Built with /MT so it needs no VC++ runtime of its own, which
    # also means it survives being extracted anywhere.
    & (Join-Path $PSScriptRoot 'build_launcher.ps1') | Out-Host
    $launcherExe = Join-Path $projectRoot 'Saved\Launcher\GameXXK.exe'
    if (-not (Test-Path -LiteralPath $launcherExe -PathType Leaf)) {
        throw "Launcher executable not found: $launcherExe"
    }
    Copy-Item -LiteralPath $launcherExe -Destination (Join-Path $OutputDirectory 'GameXXK.exe') -Force

    # GameXXK.exe is the only entry point. No .bat fallback and no launcher stub:
    # one double-clickable file at the root, with the icon, that needs nothing
    # installed. Assert it landed, so a silent copy failure cannot ship.
    $rootLauncher = Join-Path $OutputDirectory 'GameXXK.exe'
    if (-not (Test-Path -LiteralPath $rootLauncher -PathType Leaf)) {
        $listing = [System.IO.Directory]::GetFileSystemEntries($OutputDirectory) -join '; '
        throw "Root launcher was not written: '$rootLauncher'. Directory contains: $listing"
    }

    Write-Output "$Flavor package: $OutputDirectory"
    Write-Output "Entry point: $rootLauncher -> $gameExeRelative"
}
finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
