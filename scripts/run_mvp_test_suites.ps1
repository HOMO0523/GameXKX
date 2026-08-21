# Runs the MVP/UI/companion/shop test suites sequentially (one per editor invocation).
param(
    [string]$Engine = "D:\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe",
    [string]$Project = (Join-Path $PSScriptRoot "..\GameXXK.uproject"),
    [string[]]$Suites = @(
        "GameXXK.MVP.StarterCompanion",
        "GameXXK.MVP.Flow",
        "GameXXK.UI.CompanionRoster",
        "GameXXK.MetaShop",
        "GameXXK.Integration.CardBattle"
    ),
    [int]$TimeoutSeconds = 900
)

if (-not (Test-Path -LiteralPath $Engine -PathType Leaf)) {
    throw "UnrealEditor-Cmd was not found: $Engine"
}
if (-not (Test-Path -LiteralPath $Project -PathType Leaf)) {
    throw "GameXXK project was not found: $Project"
}

$PreviousSkipUbtSdkSetup = $env:UE_SKIP_UBT_SDK_SETUP
# UnrealEditor-Cmd otherwise launches a redundant UBT platform probe while
# booting. On this workstation that child can surface a modal dotnet.exe crash
# dialog and indefinitely block the parent commandlet. Formal cold UBT builds
# still perform their normal SDK validation before this script is invoked.
$env:UE_SKIP_UBT_SDK_SETUP = '1'
$AllPass = 0
$All = 0

try {
$Results = @()
foreach ($Suite in $Suites) {
    Write-Host "=== Running $Suite ===" -ForegroundColor Cyan
    $OutFile = Join-Path $env:TEMP "gxxk_test_$($Suite -replace '[^A-Za-z0-9]', '_').log"
    Remove-Item -LiteralPath $OutFile -Force -ErrorAction SilentlyContinue
    $Cmd = "Automation RunTests $Suite; Quit"
    $Start = Get-Date
    $Proc = $null
    $LaunchError = ""
    try {
        $Proc = Start-Process -FilePath $Engine -ArgumentList @(
            "`"$Project`"",
            "-unattended", "-nopause", "-nosplash", "-nullrhi", "-NoLogTimes",
            "-ExecCmds=`"$Cmd`"",
            "-AbsLog=`"$OutFile`""
        ) -PassThru -NoNewWindow -ErrorAction Stop
    }
    catch {
        $LaunchError = $_.Exception.Message
    }
    if ($null -eq $Proc) {
        $Elapsed = ((Get-Date) - $Start).TotalSeconds
        Write-Host "$Suite LAUNCH-FAIL: $LaunchError" -ForegroundColor Red
        $Results += [PSCustomObject]@{
            Suite = $Suite
            State = "LAUNCH-FAIL"
            Passed = 0
            Failed = 0
            Seconds = [math]::Round($Elapsed)
        }
        continue
    }
    $TimedOut = $false
    if (-not $Proc.WaitForExit($TimeoutSeconds * 1000)) {
        $TimedOut = $true
        $Proc.Kill()
        $Proc.WaitForExit()
        Write-Host "$Suite TIMEOUT" -ForegroundColor Yellow
    }
    $Elapsed = ((Get-Date) - $Start).TotalSeconds
    $Text = if (Test-Path $OutFile) { Get-Content $OutFile -Raw } else { "" }
    $Passed = ([regex]::Matches($Text, "Result=\{Success\}")).Count
    # UE 5.8 emits Result={Fail}; older logs used Result={Failed}. Count both so
    # a real failed test can never be misreported as NO-RUN.
    $Failed = ([regex]::Matches($Text, "Result=\{Fail(?:ed)?\}")).Count
    $State = if ($TimedOut) {
        "TIMEOUT"
    }
    elseif ($Proc.ExitCode -ne 0) {
        "PROCESS-FAIL"
    }
    elseif ($Failed -eq 0 -and $Passed -gt 0) {
        "PASS"
    }
    elseif ($Failed -eq 0 -and $Passed -eq 0) {
        "NO-RUN"
    }
    else {
        "FAIL"
    }
    Write-Host ("{0}: {1} passed, {2} failed ({3:N0}s)" -f $Suite, $Passed, $Failed, $Elapsed) -ForegroundColor $(if ($State -eq "PASS") { "Green" } else { "Red" })
    $Results += [PSCustomObject]@{ Suite = $Suite; State = $State; Passed = $Passed; Failed = $Failed; Seconds = [math]::Round($Elapsed) }
}

Write-Host "`n=== SUMMARY ===" -ForegroundColor Cyan
$Results | Format-Table -AutoSize
$AllPass = ($Results | Where-Object { $_.State -eq "PASS" }).Count
$All = $Results.Count
Write-Host "Suites passed: $AllPass / $All"
}
finally {
    if ($null -eq $PreviousSkipUbtSdkSetup) {
        Remove-Item Env:UE_SKIP_UBT_SDK_SETUP -ErrorAction SilentlyContinue
    }
    else {
        $env:UE_SKIP_UBT_SDK_SETUP = $PreviousSkipUbtSdkSetup
    }
}

if ($AllPass -ne $All) {
    exit 1
}
