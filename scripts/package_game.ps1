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
$uatArguments = @(
    'BuildCookRun', "-project=$projectFile", '-noP4', '-unattended', '-utf8output',
    '-platform=Win64', "-clientconfig=$configuration", "-target=$gameTarget", '-cook', '-stage',
    '-pak', '-iostore', '-compressed', '-nodebuginfo', '-prereqs', '-archive',
    "-archivedirectory=$OutputDirectory", '-CookPartialGC',
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
    arguments = $uatArguments
    note = 'Canonical pure-2D desktop, story, route and BattleBoard flow; complete F10; unused images and legacy 3D environments excluded; no bundled symbols.'
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
try {
    & $uat @uatArguments 2>&1 | Tee-Object -FilePath (Join-Path $ReportDirectory 'build-cook-stage.log')
    $buildResult = $LASTEXITCODE
    $metadata['finished_at'] = (Get-Date).ToString('o')
    $metadata['exit_code'] = $buildResult
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
    if ($buildResult -ne 0) { throw "BuildCookRun failed with exit code $buildResult. See $ReportDirectory" }
    # This target is x64. Its x64 prerequisite installer is retained.
    $unusedArmInstaller = Join-Path $OutputDirectory 'Windows\Engine\Extras\Redist\en-us\vc_redist.arm64.exe'
    if (Test-Path -LiteralPath $unusedArmInstaller -PathType Leaf) {
        $resolvedInstaller = (Resolve-Path -LiteralPath $unusedArmInstaller).Path
        if (-not $resolvedInstaller.StartsWith($OutputDirectory.TrimEnd('\') + '\', [StringComparison]::OrdinalIgnoreCase)) {
            throw 'Unexpected prerequisite path outside the package'
        }
        Remove-Item -LiteralPath $resolvedInstaller
    }
    $devTools = Join-Path $OutputDirectory 'DevTools'
    [IO.Directory]::CreateDirectory($devTools) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gamexxk_dev_client.py') -Destination $devTools
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\design\dev-workbench.md') -Destination (Join-Path $devTools 'README.md')
    @"
GameXXK - Pure 2D / $Flavor

启动：Windows/$gameTarget.exe
F10：打开或收起试炼手札；Esc：关闭面板。
配装、解锁、道具、试武、快照与批测功能均保留。
临时试验开始前会保存正常进度；点击“返回原进度”结束试验。
需要保留试验成果时请使用 Dev 快照。
退出请使用游戏内退出按钮并确认；保存失败时不会退出。

本包包含桌面挂机、剧情、教程、路线和 BattleBoard 的纯 2D 流程。
首次运行若提示缺少 VC 运行库，请安装 Windows/Engine/Extras/Redist/en-us/vc_redist.x64.exe。
本机脚本接口说明见 DevTools/README.md；游戏本身不需要安装 Python。
"@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'README.txt') -Encoding utf8
    Write-Output "$Flavor package: $OutputDirectory"
}
finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
