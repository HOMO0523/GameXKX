[CmdletBinding()]
param(
    [ValidateSet('workbench', 'travel', 'challenge')]
    [string]$Profile = 'workbench',
    [ValidateSet('1280x720', '1672x941', '1920x1080')]
    [string]$Resolution = '1672x941',
    [switch]$CloseRunningEditor,
    [switch]$DescribeOnly,
    [string]$EditorExe = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$hudMap = '/Game/GameXXK/Maps/L_DesktopTrainingHUD'
$parts = $Resolution.Split('x')
$width = [int]$parts[0]
$height = [int]$parts[1]

if ([string]::IsNullOrWhiteSpace($EditorExe)) {
    $editorCandidates = @(
        $env:UE_EDITOR_EXE,
        'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $EditorExe = $editorCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

if ([string]::IsNullOrWhiteSpace($EditorExe) -or -not (Test-Path -LiteralPath $EditorExe)) {
    throw "UnrealEditor.exe was not found. Pass -EditorExe or set UE_EDITOR_EXE."
}
if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file is missing: $projectFile"
}

$arguments = @(
    $projectFile,
    $hudMap,
    '-game',
    '-windowed',
    "-ResX=$width",
    "-ResY=$height",
    '-NoSplash',
    '-NoZenAutoLaunch',
    '-DDC-ForceMemoryCache'
)
if ($Profile -ne 'workbench') {
    $arguments += "-GameXXKPerfProfile=$Profile"
}

$contract = [ordered]@{
    script = 'launch_desktop_mvp_demo.ps1'
    profile = $Profile
    resolution = [ordered]@{ width = $width; height = $height }
    editor_exe = $EditorExe
    project_file = $projectFile
    map = $hudMap
    arguments = @($arguments)
    close_running_editor = [bool]$CloseRunningEditor
}

if ($DescribeOnly) {
    $contract | ConvertTo-Json -Depth 5
    return
}

if ($CloseRunningEditor) {
    Write-Host '[DEMO] Saving running editor before close...'
    $scriptsDir = Join-Path $projectRoot 'scripts'
    $env:PYTHONPATH = $scriptsDir
    $code = @"
import sys
sys.path.insert(0, r'$scriptsDir')
from ue_tdd_pipeline import save_running_editor_before_close, kill_editor
if not save_running_editor_before_close():
    print('CLOSE_FAIL')
    sys.exit(1)
if not kill_editor():
    print('CLOSE_FAIL')
    sys.exit(1)
print('CLOSE_OK')
"@
    $closeOutput = ($code | & python -) 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Could not safely close the running editor: $closeOutput"
    }
    Write-Host '[DEMO] Running editor closed safely.'
}

$env:UE_SKIP_UBT_SDK_SETUP = '1'
$quotedArguments = $arguments | ForEach-Object {
    if ($_ -match '[\s"]') {
        '"' + ($_ -replace '"', '\"') + '"'
    }
    else {
        $_
    }
}
$argumentString = $quotedArguments -join ' '
$process = Start-Process -FilePath $EditorExe -ArgumentList $argumentString -WorkingDirectory $projectRoot -PassThru -WindowStyle Normal
if ($null -eq $process -or $process.HasExited) {
    throw 'Failed to start the GameXXK MVP demo window.'
}

Write-Host "[DEMO] Started GameXXK MVP demo PID=$($process.Id)"
Write-Host "[DEMO] Map: $hudMap"
Write-Host "[DEMO] Profile: $Profile"
Write-Host "[DEMO] Resolution: ${width}x${height}"
if ($Profile -eq 'workbench') {
    Write-Host '[DEMO] 演示路径：'
    Write-Host '  1. 在右侧历练地图选择一个关卡；'
    Write-Host '  2. 点击“游历”观看顶部挂机条；'
    Write-Host '  3. 点击“挑战”会关闭工作台并进入现有路线图；'
    Write-Host '  4. 玩家自己点路线节点，进入全屏 BattleBoard；'
    Write-Host '  5. 可用自动战斗或手动出牌。'
}
elseif ($Profile -eq 'travel') {
    Write-Host '[DEMO] 已自动进入 1-1 游历；观察顶部挂机条走动/遇敌/攻击/受击/死亡。'
}
elseif ($Profile -eq 'challenge') {
    Write-Host '[DEMO] 已自动进入现有全屏 BattleBoard 的 CardBattle（快捷演示，不模拟玩家选路线）。'
}

$contract | ConvertTo-Json -Depth 5 | Write-Host
