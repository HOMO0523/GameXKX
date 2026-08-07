param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-user03-backpack-state-sync.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.user03-backpack-state-sync.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.user03-backpack-state-sync.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/AfterUser03BackpackSync'
)

$ErrorActionPreference = 'Stop'

function Resolve-InputFile([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) { throw "Missing ${Label}: $Value" }
    (Resolve-Path -LiteralPath $Value).Path
}

function Resolve-Destination([string]$Value) {
    if ([IO.Path]::IsPathRooted($Value)) { [IO.Path]::GetFullPath($Value) }
    else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Value)) }
}

function Assert-New([string]$Value, [string]$Label) {
    if (Test-Path -LiteralPath $Value) { throw "Destination exists for ${Label}: $Value" }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Value)) | Out-Null
}

function Get-PngSize([string]$Value) {
    $image = [Drawing.Image]::FromFile($Value)
    try { @($image.Width, $image.Height) }
    finally { $image.Dispose() }
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-user03-backpack-state-sync-jsx.js') 'user-03 sync builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory
Assert-New $reportPath 'user-03 sync report'
Assert-New $validationPath 'user-03 sync validation'
if (Test-Path -LiteralPath $exportDirectoryPath) {
    if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0) {
        throw "User-03 sync export directory is not empty: $exportDirectoryPath"
    }
}
else {
    [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
}
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) { throw 'User-03 sync backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-user03-backpack-state-sync.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }
$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "User-03 sync JSX generation failed: $LASTEXITCODE" }

$app = New-Object -ComObject Photoshop.Application
$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) {
    $document = $app.Documents.Item($index)
    try { $documentPath = [IO.Path]::GetFullPath([string]$document.FullName) }
    catch { continue }
    if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) {
        throw "Target PSD has unsaved Photoshop changes: $psdPath"
    }
}
$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
    $errorReceipt = "$reportPath.error.txt"
    if (Test-Path -LiteralPath $errorReceipt) {
        throw "Photoshop user-03 sync failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)"
    }
    throw "Photoshop did not write user-03 sync report: $reportPath"
}

$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS' -or -not [bool]$reportData.baseVisibleSignatureMatch -or -not [bool]$reportData.selectionVisibleOnlyOnTarget) {
    throw 'Unexpected user-03 sync report'
}
$expectedTargetArtCount = [int]$reportData.sourceVisibleArtCount + [int]$reportData.selectionArtCount
if ([int]$reportData.targetVisibleArtCount -ne $expectedTargetArtCount) {
    throw 'Target visible art-layer count does not equal base plus selected-state ink'
}

Add-Type -AssemblyName System.Drawing
$exports = @()
foreach ($record in @($reportData.exports)) {
    $exportPath = [string]$record.path
    if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing user-03 sync export: $exportPath" }
    $size = Get-PngSize $exportPath
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) { throw "Unexpected user-03 sync export dimensions: $exportPath" }
    $exports += [ordered]@{
        page = [string]$record.name
        path = $exportPath
        width = 1920
        height = 1080
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()
    }
}
if ($exports.Count -ne 2 -or $exports[0].sha256 -eq $exports[1].sha256) {
    throw 'Page 03 and page 13 review exports should differ by selected-state ink'
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after user-03 sync' }

[ordered]@{
    status = 'PASS'
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.topLevelPageCount
    sourcePage = [string]$reportData.sourcePage
    targetPage = [string]$reportData.targetPage
    hiddenTargetGroups = @($reportData.hiddenTargetGroups)
    clonedGroupsBottomToTop = @($reportData.clonedGroupsBottomToTop)
    visibleArtCount = [int]$reportData.sourceVisibleArtCount
    decomposeButtonBounds = @($reportData.sourceButtonBounds)
    decomposeLabelBounds = @($reportData.sourceLabelBounds)
    scrollbarButtonBounds = @($reportData.sourceScrollbarButtonBounds)
    selectionVisibleOnlyOnPage13 = $true
    baseVisibleSignatureMatch = $true
    page03And13PixelIdentical = $false
    expectedDifference = '39_SelectedSlotInk visible only on page 13'
    exports = $exports
} | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'User-03 backpack state sync PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Page 03 review hash: $($exports[0].sha256)"
Write-Output "Page 13 review hash: $($exports[1].sha256)"
Write-Output "Review folder: $exportDirectoryPath"
