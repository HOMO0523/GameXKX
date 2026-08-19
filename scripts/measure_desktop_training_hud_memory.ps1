[CmdletBinding()]
param(
    [string]$EditorExe = '',
    [int[]]$SampleSeconds = @(20, 50),
    [ValidateSet('empty', 'travel', 'challenge', 'town3d', 'all')]
    [string]$Profile = 'all',
    [switch]$DescribeOnly,
    [switch]$AllowConcurrentEditor
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$projectFile = Join-Path $projectRoot 'GameXXK.uproject'
$hudMap = '/Game/GameXXK/Maps/L_DesktopTrainingHUD'
$townMap = '/Game/GameXXK/Maps/Prototype/L_Qingshan_AsianVillage_Demo'
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

$profileDefinitions = @(
    [ordered]@{ name = 'empty'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=empty') },
    [ordered]@{ name = 'travel'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=travel') },
    [ordered]@{ name = 'challenge'; map = $hudMap; extra_arguments = @('-GameXXKPerfProfile=challenge') },
    [ordered]@{ name = 'town3d'; map = $townMap; extra_arguments = @() }
)
$selectedProfiles = if ($Profile -eq 'all') {
    @($profileDefinitions)
}
else {
    @($profileDefinitions | Where-Object { $_.name -eq $Profile })
}

function New-ProfileArguments([object]$Definition) {
    $commonArguments = @(
        $projectFile,
        $Definition.map,
        '-game',
        '-windowed',
        "-ResX=$resolutionWidth",
        "-ResY=$resolutionHeight",
        '-NoSplash'
    )
    return @($commonArguments + @($Definition.extra_arguments))
}

$contractProfiles = @($selectedProfiles | ForEach-Object {
    [ordered]@{
        name = $_.name
        map = $_.map
        arguments = @(New-ProfileArguments $_)
    }
})
$contract = [ordered]@{
    schema_version = 2
    editor_exe = $EditorExe
    project_file = $projectFile
    resolution = [ordered]@{
        width = $resolutionWidth
        height = $resolutionHeight
    }
    sample_seconds = @($resolvedSampleSeconds)
    report_directory = $reportDirectory
    profiles = @($contractProfiles)
}

if ($DescribeOnly) {
    $contract | ConvertTo-Json -Depth 7
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

function Get-GpuProcessMetrics([int]$ProcessId) {
    $prefix = "pid_${ProcessId}_"
    try {
        $engineRows = @(Get-CimInstance -ClassName 'Win32_PerfFormattedData_GPUPerformanceCounters_GPUEngine' -ErrorAction Stop |
            Where-Object { $_.Name.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) -and $_.Name -match 'engtype_(3D|Compute)' })
        $memoryRows = @(Get-CimInstance -ClassName 'Win32_PerfFormattedData_GPUPerformanceCounters_GPUProcessMemory' -ErrorAction Stop |
            Where-Object { $_.Name.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase) })
        $engineSum = ($engineRows | Measure-Object -Property UtilizationPercentage -Sum).Sum
        $dedicatedSum = ($memoryRows | Measure-Object -Property DedicatedUsage -Sum).Sum
        $sharedSum = ($memoryRows | Measure-Object -Property SharedUsage -Sum).Sum
        if ($null -eq $engineSum) { $engineSum = 0 }
        if ($null -eq $dedicatedSum) { $dedicatedSum = 0 }
        if ($null -eq $sharedSum) { $sharedSum = 0 }
        return [ordered]@{
            gpu_engine_percent = [math]::Round([double]$engineSum, 2)
            gpu_dedicated_mib = [math]::Round([double]$dedicatedSum / 1MB, 1)
            gpu_shared_mib = [math]::Round([double]$sharedSum / 1MB, 1)
            gpu_metric_error = $null
        }
    }
    catch {
        return [ordered]@{
            gpu_engine_percent = 0.0
            gpu_dedicated_mib = 0.0
            gpu_shared_mib = 0.0
            gpu_metric_error = $_.Exception.Message
        }
    }
}

function New-EditorStartInfo([string[]]$Arguments) {
    $startInfo = [System.Diagnostics.ProcessStartInfo]::new()
    $startInfo.FileName = $EditorExe
    $startInfo.UseShellExecute = $false
    $startInfo.CreateNoWindow = $false
    $startInfo.WindowStyle = [System.Diagnostics.ProcessWindowStyle]::Normal
    foreach ($argument in $Arguments) {
        $startInfo.ArgumentList.Add([string]$argument)
    }
    $startInfo.Environment['UE_SKIP_UBT_SDK_SETUP'] = '1'
    return $startInfo
}

New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$timestamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$reportName = if ($Profile -eq 'all') {
    "desktop-training-performance-matrix-$timestamp.json"
}
else {
    "desktop-training-$Profile-performance-$timestamp.json"
}
$reportPath = Join-Path $reportDirectory $reportName
$startedAt = Get-Date
$gitHead = (& git -C $projectRoot rev-parse HEAD 2>$null | Select-Object -First 1)
$profileResults = [System.Collections.Generic.List[object]]::new()
$hadFailure = $false

foreach ($profileDefinition in $selectedProfiles) {
    $profileName = [string]$profileDefinition.name
    $arguments = @(New-ProfileArguments $profileDefinition)
    $process = [System.Diagnostics.Process]::new()
    $process.StartInfo = New-EditorStartInfo $arguments
    $stopwatch = [System.Diagnostics.Stopwatch]::new()
    $samples = [System.Collections.Generic.List[object]]::new()
    $cleanupMode = 'not-started'
    $profileError = $null
    $processStarted = $false
    $profileResult = [ordered]@{
        profile_name = $profileName
        map = [string]$profileDefinition.map
        arguments = @($arguments)
        process_id = $null
        status = 'starting'
        cleanup = $cleanupMode
        samples = @()
        error = $null
    }

    try {
        if (-not $process.Start()) {
            throw 'The editor process did not start.'
        }
        $processStarted = $true
        $profileResult.process_id = $process.Id
        $profileResult.status = 'sampling'
        $stopwatch.Start()
        $process.Refresh()
        $previousCpuTime = $process.TotalProcessorTime
        $previousElapsedSeconds = 0.0

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

            $currentElapsedSeconds = $stopwatch.Elapsed.TotalSeconds
            $currentCpuTime = $process.TotalProcessorTime
            $cpuDeltaSeconds = ($currentCpuTime - $previousCpuTime).TotalSeconds
            $wallDeltaSeconds = [math]::Max(0.001, $currentElapsedSeconds - $previousElapsedSeconds)
            $cpuPercent = [math]::Round(
                100.0 * $cpuDeltaSeconds / ($wallDeltaSeconds * [Environment]::ProcessorCount),
                2)
            $gpu = Get-GpuProcessMetrics $process.Id
            $sample = [ordered]@{
                seconds = [int]$targetSecond
                elapsed_seconds = [math]::Round($currentElapsedSeconds, 3)
                working_set_mib = [math]::Round($process.WorkingSet64 / 1MB, 1)
                private_memory_mib = [math]::Round($process.PrivateMemorySize64 / 1MB, 1)
                cpu_percent = $cpuPercent
                gpu_engine_percent = $gpu.gpu_engine_percent
                gpu_dedicated_mib = $gpu.gpu_dedicated_mib
                gpu_shared_mib = $gpu.gpu_shared_mib
                gpu_metric_error = $gpu.gpu_metric_error
                responding = $process.Responding
            }
            $samples.Add($sample)
            Write-Host ("[{0}] {1}s  CPU {2:N2}%  GPU {3:N2}%  Working {4:N1} MiB  Private {5:N1} MiB  Dedicated {6:N1} MiB" -f
                $profileName,
                $sample.seconds,
                $sample.cpu_percent,
                $sample.gpu_engine_percent,
                $sample.working_set_mib,
                $sample.private_memory_mib,
                $sample.gpu_dedicated_mib)
            $previousCpuTime = $currentCpuTime
            $previousElapsedSeconds = $currentElapsedSeconds
        }
        $profileResult.status = 'completed'
    }
    catch {
        $hadFailure = $true
        $profileError = $_
        $profileResult.status = 'failed'
        $profileResult.error = $_.Exception.Message
    }
    finally {
        $stopwatch.Stop()
        if ($processStarted -and -not $process.HasExited) {
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
        elseif ($processStarted) {
            $cleanupMode = 'launched-process-already-exited'
        }
        $profileResult.cleanup = $cleanupMode
        $profileResult.samples = @($samples)
        $profileResults.Add($profileResult)
        $process.Dispose()
    }

    if ($null -ne $profileError) {
        Write-Warning "Profile $profileName failed: $($profileError.Exception.Message)"
    }
}

$report = [ordered]@{
    schema_version = 2
    captured_at = $startedAt.ToString('o')
    git_head = $gitHead
    editor_exe = $EditorExe
    project_file = $projectFile
    resolution = [ordered]@{
        width = $resolutionWidth
        height = $resolutionHeight
    }
    sample_seconds = @($resolvedSampleSeconds)
    requested_profile = $Profile
    status = if ($hadFailure) { 'failed' } else { 'completed' }
    profile_results = @($profileResults)
}
$report | ConvertTo-Json -Depth 9 | Set-Content -LiteralPath $reportPath -Encoding utf8

Write-Host "Report: $reportPath"
if ($hadFailure) {
    throw "One or more performance profiles failed. Report: $reportPath"
}
