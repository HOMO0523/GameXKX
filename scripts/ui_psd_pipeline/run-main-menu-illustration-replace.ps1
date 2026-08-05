param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Illustration = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/main_menu_tiger_hero_v9_loose_inkwash.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-main-menu-illustration-v9.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-illustration-v9.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-illustration-v9.validation.json',
    [string]$Export
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingFile([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) {
        throw "Missing ${Label}: $Value"
    }
    return (Resolve-Path -LiteralPath $Value).Path
}

function Resolve-Destination([string]$Value) {
    if ([IO.Path]::IsPathRooted($Value)) {
        return [IO.Path]::GetFullPath($Value)
    }
    return [IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))
}

$mainMenuName = -join @([char]0x4E3B, [char]0x83DC, [char]0x5355)
if ([string]::IsNullOrWhiteSpace($Export)) {
    $Export = "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/After/01_${mainMenuName}.png"
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-ExistingFile $Psd 'master PSD'
$illustrationPath = Resolve-ExistingFile $Illustration 'replacement illustration'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportPath = Resolve-Destination $Export
$builderPath = Resolve-ExistingFile (Join-Path $PSScriptRoot 'build-main-menu-illustration-replace-jsx.js') 'replacement JSX builder'
$expectedBeforeHash = '8b3947c39db81b5223b55577bbfd7bba4ab7abd7e611ca635cfb7dcd11a72338'
$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $expectedBeforeHash) {
    throw "Current PSD hash differs from the assembled v5 lock: $beforeHash"
}

[IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
if (Test-Path -LiteralPath $backupPath -PathType Leaf) {
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
    if ($backupHash -ne $beforeHash) { throw "Existing v9 backup hash mismatch: $backupPath" }
} else {
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
    $backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
    if ($backupHash -ne $beforeHash) { throw 'Failed to lock the v9 replacement backup' }
}

foreach ($destination in @($reportPath, $validationPath)) {
    if (Test-Path -LiteralPath $destination) { throw "Destination already exists: $destination" }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
}
$errorReceiptPath = "$reportPath.error.txt"
if (Test-Path -LiteralPath $errorReceiptPath -PathType Leaf) { Remove-Item -LiteralPath $errorReceiptPath }

$archiveDirectory = Join-Path $projectRoot 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/Intermediate'
[IO.Directory]::CreateDirectory($archiveDirectory) | Out-Null
$archivedExportPath = Join-Path $archiveDirectory '01_main_menu_v5.png'
if (Test-Path -LiteralPath $exportPath -PathType Leaf) {
    $currentExportHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash
    if (Test-Path -LiteralPath $archivedExportPath -PathType Leaf) {
        if ((Get-FileHash -Algorithm SHA256 -LiteralPath $archivedExportPath).Hash -ne $currentExportHash) {
            throw "Existing v5 export archive differs: $archivedExportPath"
        }
    } else {
        Copy-Item -LiteralPath $exportPath -Destination $archivedExportPath
    }
    Remove-Item -LiteralPath $exportPath
}
[IO.Directory]::CreateDirectory((Split-Path -Parent $exportPath)) | Out-Null

$app = $null
try {
    $app = [Runtime.InteropServices.Marshal]::GetActiveObject('Photoshop.Application')
} catch {
    for ($attempt = 1; $attempt -le 5 -and $null -eq $app; $attempt++) {
        try { $app = New-Object -ComObject Photoshop.Application }
        catch {
            if ($attempt -eq 5) { throw }
            Start-Sleep -Seconds 2
        }
    }
}

$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) {
    $document = $app.Documents.Item($index)
    try { $documentPath = [IO.Path]::GetFullPath([string]$document.FullName) }
    catch { continue }
    if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) {
        throw "Target PSD has unsaved Photoshop changes: $psdPath"
    }
}

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'replace-main-menu-illustration-v9.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath `
    --psd $psdPath `
    --image $illustrationPath `
    --output $jsxPath `
    --report $reportPath `
    --export $exportPath
if ($LASTEXITCODE -ne 0) { throw "Replacement JSX generation failed with exit code $LASTEXITCODE" }

try {
    $app.Visible = $true
    $app.DoJavaScriptFile($jsxPath)
} catch {
    if (-not (Test-Path -LiteralPath $exportPath) -and (Test-Path -LiteralPath $archivedExportPath)) {
        Copy-Item -LiteralPath $archivedExportPath -Destination $exportPath
    }
    throw
}

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { throw "Missing replacement report: $reportPath" }
if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing replacement export: $exportPath" }
$reportData = Get-Content -Raw -Encoding UTF8 -LiteralPath $reportPath | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected report status: $($reportData.status)" }
if (-not [bool]$reportData.nonTargetSignatureMatch) { throw 'Non-target page signature changed' }
if (-not [bool]$reportData.pagePeerSignatureMatch) { throw 'A peer group in page 01 changed' }

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Image]::FromFile($exportPath)
try { $exportWidth = $image.Width; $exportHeight = $image.Height }
finally { $image.Dispose() }
if ($exportWidth -ne 1920 -or $exportHeight -ne 1080) {
    throw "Unexpected export dimensions: ${exportWidth}x${exportHeight}"
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after illustration replacement' }

$validationData = [ordered]@{
    status = 'PASS'
    page = "01_$mainMenuName"
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    topLevelPageCount = [int]$reportData.pageCount
    nonTargetSignatureMatch = [bool]$reportData.nonTargetSignatureMatch
    pagePeerSignatureMatch = [bool]$reportData.pagePeerSignatureMatch
    illustrationGroup = [string]$reportData.illustrationGroup
    illustrationLayer = [string]$reportData.illustrationLayer
    replacement = $illustrationPath
    export = $exportPath
    exportWidth = $exportWidth
    exportHeight = $exportHeight
}
$validationData | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'Main-menu illustration replacement PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Export: $exportPath"
Write-Output "Validation: $validationPath"
