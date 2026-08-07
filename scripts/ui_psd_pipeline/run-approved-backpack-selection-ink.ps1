param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-approved-backpack-selection-ink.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.approved-backpack-selection-ink.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.approved-backpack-selection-ink.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/PSD'
)

$ErrorActionPreference = 'Stop'

function Resolve-InputFile([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) { throw "Missing ${Label}: $Value" }
    return (Resolve-Path -LiteralPath $Value).Path
}

function Resolve-Destination([string]$Value) {
    if ([IO.Path]::IsPathRooted($Value)) { return [IO.Path]::GetFullPath($Value) }
    return [IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))
}

function Assert-NewDestination([string]$Value, [string]$Label) {
    if (Test-Path -LiteralPath $Value) { throw "Destination already exists for ${Label}: $Value" }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Value)) | Out-Null
}

function Get-PngSize([string]$Value) {
    $image = [Drawing.Image]::FromFile($Value)
    try { return @($image.Width, $image.Height) } finally { $image.Dispose() }
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-approved-backpack-selection-ink-jsx.js') 'selection-ink JSX builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory

Assert-NewDestination $reportPath 'selection-ink report'
Assert-NewDestination $validationPath 'selection-ink validation'
if (Test-Path -LiteralPath $exportDirectoryPath) {
    if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File -ErrorAction SilentlyContinue).Count -gt 0) {
        throw "Selection-ink export directory is not empty: $exportDirectoryPath"
    }
} else {
    [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
}
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) { throw 'Selection-ink backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-approved-backpack-selection-ink.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "Selection-ink JSX generation failed with exit code $LASTEXITCODE" }

$app = New-Object -ComObject Photoshop.Application
$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) {
    $document = $app.Documents.Item($index)
    try { $documentPath = [IO.Path]::GetFullPath([string]$document.FullName) } catch { continue }
    if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) {
        throw "Target PSD has unsaved Photoshop changes: $psdPath"
    }
}

$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
    $errorReceipt = "$reportPath.error.txt"
    if (Test-Path -LiteralPath $errorReceipt) { throw "Photoshop selection-ink repair failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)" }
    throw "Photoshop did not write selection-ink report: $reportPath"
}
$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected selection-ink report status: $($reportData.status)" }
if ([int]$reportData.topLevelPageCount -ne 18) { throw 'Selection-ink repair changed top-level page count' }
if (-not [bool]$reportData.selectionImmediatelyBehindSlots) { throw 'Selection ink is not immediately behind inventory slots' }
if ([bool]$reportData.legacySelectedGroupVisible) { throw 'Legacy selected group remains visible' }
if ([bool]$reportData.duplicateItemAdded) { throw 'Selection-ink repair duplicated an item' }
$targetBounds = @($reportData.targetBounds | ForEach-Object { [int]$_ })
if ($targetBounds.Count -ne 4 -or [Math]::Abs(($targetBounds[2] - $targetBounds[0]) - 126) -gt 1 -or [Math]::Abs(($targetBounds[3] - $targetBounds[1]) - 43) -gt 1) {
    throw "Unexpected approved selection-ink bounds: $($targetBounds -join ',')"
}

Add-Type -AssemblyName System.Drawing
$exports = @()
foreach ($record in @($reportData.exports)) {
    $exportPath = [string]$record.path
    if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing selection-ink export: $exportPath" }
    $size = Get-PngSize $exportPath
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) { throw "Unexpected selection-ink export dimensions: $exportPath" }
    $exports += [ordered]@{
        page = [string]$record.name
        path = $exportPath
        width = 1920
        height = 1080
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()
    }
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after selection-ink repair' }
$validationData = [ordered]@{
    status = 'PASS'
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.topLevelPageCount
    sourceLayer = [string]$reportData.sourceLayer
    sourceBounds = @($reportData.sourceBounds)
    targetGroup = [string]$reportData.targetGroup
    targetBounds = $targetBounds
    selectionImmediatelyBehindSlots = [bool]$reportData.selectionImmediatelyBehindSlots
    legacySelectedGroupVisible = [bool]$reportData.legacySelectedGroupVisible
    duplicateItemAdded = [bool]$reportData.duplicateItemAdded
    exports = $exports
}
$validationData | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'Approved backpack selection ink PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"

