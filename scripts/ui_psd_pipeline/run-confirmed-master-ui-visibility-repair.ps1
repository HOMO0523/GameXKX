param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-confirmed-ui-visibility-repair.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-visibility-repair.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-visibility-repair.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ConfirmedIntegration/AfterVisibilityRepair'
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
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-confirmed-master-ui-visibility-repair-jsx.js') 'visibility-repair JSX builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory

Assert-NewDestination $reportPath 'repair report'
Assert-NewDestination $validationPath 'repair validation'
if (Test-Path -LiteralPath $exportDirectoryPath) {
    if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File -ErrorAction SilentlyContinue).Count -gt 0) {
        throw "Repair export directory is not empty: $exportDirectoryPath"
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
if ($beforeHash -ne $backupHash) { throw 'Visibility-repair backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-confirmed-master-ui-visibility-repair.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "Visibility-repair JSX generation failed with exit code $LASTEXITCODE" }

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
    if (Test-Path -LiteralPath $errorReceipt) { throw "Photoshop repair failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)" }
    throw "Photoshop did not write repair report: $reportPath"
}
$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected repair report status: $($reportData.status)" }
if ([int]$reportData.pageCount -ne 18) { throw 'Repair changed top-level page count' }
if ([int]$reportData.hiddenLegacyCurrencyLayerCount -ne 24) { throw 'Repair did not hide all legacy currency layers' }
if ([int]$reportData.hiddenLegacyShopPriceIconCount -ne 10) { throw 'Repair did not hide all legacy shop price icons' }
if ([int]$reportData.visibleShopIngotPriceCount -ne 8 -or [int]$reportData.hiddenShopStateIngotPriceCount -ne 2) {
    throw 'Repair changed shop ingot state visibility'
}

Add-Type -AssemblyName System.Drawing
$exports = @()
foreach ($record in @($reportData.exports)) {
    $exportPath = [string]$record.path
    if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing repaired export: $exportPath" }
    $size = Get-PngSize $exportPath
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) { throw "Unexpected repaired export dimensions: $exportPath" }
    $exports += [ordered]@{ page = [string]$record.name; path = $exportPath; width = 1920; height = 1080; sha256 = (Get-FileHash $exportPath -Algorithm SHA256).Hash.ToLowerInvariant() }
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after visibility repair' }
$validationData = [ordered]@{
    status = 'PASS'
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.pageCount
    hiddenLegacyCurrencyLayerCount = [int]$reportData.hiddenLegacyCurrencyLayerCount
    hiddenLegacyShopPriceIconCount = [int]$reportData.hiddenLegacyShopPriceIconCount
    visibleShopIngotPriceCount = [int]$reportData.visibleShopIngotPriceCount
    hiddenShopStateIngotPriceCount = [int]$reportData.hiddenShopStateIngotPriceCount
    exports = $exports
}
$validationData | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'Confirmed Master UI visibility repair PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
