[CmdletBinding()]
param(
    [string]$EditorExe = '',
    [int[]]$SampleSeconds = @(20, 50),
    [switch]$DescribeOnly,
    [switch]$AllowConcurrentEditor
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$mapPath = '/Game/GameXXK/Maps/L_DesktopTrainingHUD'
$resolutionWidth = 1672
$resolutionHeight = 941
$reportDirectory = Join-Path $projectRoot 'Saved\HarnessReports'
$resolvedSampleSeconds = @($SampleSeconds | Where-Object { $_ -ge 0 } | Sort-Object -Unique)
if ($resolvedSampleSeconds.Count -eq 0) {
    throw 'At least one non-negative sample time is required.'
}

if ([string]::IsNullOrWhiteSpace($EditorExe)) {
    $editorCandidates = @(
        $env:UE_EDITOR_EXE,
        'D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe'
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $EditorExe = $editorCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}

$contract = [ordered]@{
    editor_exe = $EditorExe
    project_file = $projectFile
    map = $mapPath
    resolution = [ordered]@{
        width = $resolutionWidth
        height = $resolutionHeight
    }
    sample_seconds = @($resolvedSampleSeconds)
    report_directory = $reportDirectory
}

if ($DescribeOnly) {
    $contract | ConvertTo-Json -Depth 5
    return
}

if ([string]::IsNullOrWhiteSpace($EditorExe) -or -not (Test-Path -LiteralPath $EditorExe)) {
    throw 'UnrealEditor.exe was not found. Pass -EditorExe or set UE_EDITOR_EXE.'
}
if (-not (Test-Path -LiteralPath $projectFile)) {
    throw "Project file is missing: $projectFile"
}

if (-not $AllowConcurrentEditor) {
    $existingProjectEditors = @(Get-CimInstance Win32_Process -Filter "Name = 'UnrealEditor.exe'" -ErrorAction SilentlyContinue |
        Where-Object { $_.CommandLine -and $_.CommandLine.Contains($projectFile, [System.StringComparison]::OrdinalIgnoreCase) })
    if ($existingProjectEditors.Count -gt 0) {
        $ids = ($existingProjectEditors.ProcessId -join ', ')
        throw "A GameXXK UnrealEditor process is already running (PID $ids). Save/close it or pass -AllowConcurrentEditor intentionally."
    }
}

New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$reportPath = Join-Path $reportDirectory "desktop-training-hud-memory-$timestamp.json"
$arguments = @(
    $projectFile,
    $mapPath,
    '-game',
    '-windowed',
    "-ResX=$resolutionWidth",
    "-ResY=$resolutionHeight",
    '-NoSplash'
)

$startInfo = [System.Diagnostics.ProcessStartInfo]::new()
$startInfo.FileName = $EditorExe
$startInfo.UseShellExecute = $false
$startInfo.CreateNoWindow = $false
$startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Normal
foreach ($argument in $arguments) {
    $startInfo.ArgumentList.Add([string]$argument)
}
$startInfo.Environment['UE_SKIP_UBT_SDK_SETUP'] = '1'

$process = [System.Diagnostics.Process]::new()
$process.StartInfo = $startInfo
$stopwatch = [System.Diagnostics.Stopwatch]::new()
$samples = [System.Collections.Generic.List[object]]::new()
$runError = $null
$cleanupMode = 'not-started'
$startedAt = Get-Date
$gitHead = (& git -C $projectRoot rev-parse HEAD 2>$null | Select-Object -First 1)
$report = [ordered]@{
    schema_version = 1
    captured_at = $startedAt.ToString('o')
    git_head = $gitHead
    editor_exe = $EditorExe
    project_file = $projectFile
    map = $mapPath
    resolution = [ordered]@{
        width = $resolutionWidth
        height = $resolutionHeight
    }
    arguments = @($arguments)
    process_id = $null
    status = 'starting'
    cleanup = $cleanupMode
    samples = $samples
    error = $null
}

try {
    if (-not $process.Start()) {
        throw 'The editor process did not start.'
    }
    $report.process_id = $process.Id
    $report.status = 'sampling'
    $stopwatch.Start()

    foreach ($targetSecond in $resolvedSampleSeconds) {
        while ($stopwatch.Elapsed.TotalSeconds -lt $targetSecond) {
            if ($process.HasExited) {
                throw "UnrealEditor exited before the ${targetSecond}s sample with code $($process.ExitCode)."
            }
            $remainingMilliseconds = [math]::Ceiling(($targetSecond - $stopwatch.Elapsed.TotalSeconds) * 1000.0)
            Start-Sleep -Milliseconds ([math]::Max(1, [math]::Min(250, $remainingMilliseconds)))
        }
        $process.Refresh()
        if ($process.HasExited) {
            throw "UnrealEditor exited at the ${targetSecond}s sample with code $($process.ExitCode)."
        }
        $sample = [ordered]@{
            seconds = [int]$targetSecond
            elapsed_seconds = [math]::Round($stopwatch.Elapsed.TotalSeconds, 3)
            working_set_mib = [math]::Round($process.WorkingSet64 / 1MB, 1)
            private_memory_mib = [math]::Round($process.PrivateMemorySize64 / 1MB, 1)
            responding = $process.Responding
        }
        $samples.Add($sample)
        Write-Host ("{0}s  Working {1:N1} MiB  Private {2:N1} MiB" -f
            $sample.seconds, $sample.working_set_mib, $sample.private_memory_mib)
    }
    $report.status = 'completed'
}
catch {
    $runError = $_
    $report.status = 'failed'
    $report.error = $_.Exception.Message
}
finally {
    $stopwatch.Stop()
    if ($process.Id -gt 0 -and -not $process.HasExited) {
        $closedNormally = $process.CloseMainWindow()
        if ($closedNormally) {
            $null = $process.WaitForExit(5000)
        }
        if (-not $process.HasExited) {
            $process.Kill($true)
            $process.WaitForExit()
            $cleanupMode = 'killed-launched-process-tree'
        }
        else {
            $cleanupMode = 'closed-launched-process'
        }
    }
    elseif ($process.Id -gt 0) {
        $cleanupMode = 'launched-process-already-exited'
    }
    $report.cleanup = $cleanupMode
    $report.samples = @($samples)
    $report | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $reportPath -Encoding utf8
    $process.Dispose()
}

Write-Host "Report: $reportPath"
if ($null -ne $runError) {
    throw $runError
}
