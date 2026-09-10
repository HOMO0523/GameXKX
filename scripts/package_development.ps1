param(
    [string]$EngineRoot = 'D:\UE_5.8',
    [string]$OutputDirectory = '',
    [string]$ReportDirectory = ''
)

$ErrorActionPreference = 'Stop'
$projectRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $projectRoot "Packaged\Development-$stamp" }
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
    uebp_LogFolder = $ReportDirectory
    uebp_FinalLogFolder = $ReportDirectory
}
$previous = @{}
foreach ($key in $overrides.Keys) {
    $previous[$key] = [Environment]::GetEnvironmentVariable($key, 'Process')
    [Environment]::SetEnvironmentVariable($key, $overrides[$key], 'Process')
}
$uatArguments = @(
    'BuildCookRun', "-project=$projectFile", '-noP4', '-unattended', '-utf8output',
    '-platform=Win64', '-clientconfig=Development', '-build', '-cook', '-stage',
    '-pak', '-iostore', '-compressed', '-nodebuginfo', '-prereqs', '-archive',
    "-archivedirectory=$OutputDirectory", '-CookPartialGC',
    '-ubtargs=-NoHotReload -NoHotReloadFromIDE -NoUBA -MaxParallelActions=2'
)
$metadata = @{
    started_at = (Get-Date).ToString('o')
    source_commit = (git -C $projectRoot rev-parse HEAD)
    configuration = 'Development'
    output = $OutputDirectory
    arguments = $uatArguments
    note = 'Full configured maps and runtime cook directories; F10 enabled; compressed containers; symbols stay outside the package.'
}
$metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
try {
    & $uat @uatArguments 2>&1 | Tee-Object -FilePath (Join-Path $ReportDirectory 'build-cook-stage.log')
    $buildResult = $LASTEXITCODE
    $metadata['finished_at'] = (Get-Date).ToString('o')
    $metadata['exit_code'] = $buildResult
    $metadata | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $ReportDirectory 'package-command.json') -Encoding utf8
    if ($buildResult -ne 0) { throw "BuildCookRun failed with exit code $buildResult. See $ReportDirectory" }
    $devTools = Join-Path $OutputDirectory 'DevTools'
    [IO.Directory]::CreateDirectory($devTools) | Out-Null
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'gamexxk_dev_client.py') -Destination $devTools
    Copy-Item -LiteralPath (Join-Path $projectRoot 'docs\design\dev-workbench.md') -Destination (Join-Path $devTools 'README.md')
    Write-Output "Development package: $OutputDirectory"
}
finally {
    foreach ($key in $previous.Keys) { [Environment]::SetEnvironmentVariable($key, $previous[$key], 'Process') }
}
