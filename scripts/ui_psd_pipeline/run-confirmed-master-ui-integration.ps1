param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$CompactCurrencyStrip = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png',
    [string]$IngotIcon = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/resource_gold.png',
    [string]$SelectedSlot = '',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-confirmed-ui-integration.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-integration.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.confirmed-ui-integration.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ConfirmedIntegration/After'
)

$ErrorActionPreference = 'Stop'

function Resolve-InputFile([string]$Value, [string]$Label) {
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

function Assert-NewDestination([string]$Value, [string]$Label) {
    if (Test-Path -LiteralPath $Value) {
        throw "Destination already exists for ${Label}: $Value"
    }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $Value)) | Out-Null
}

function Get-PngSize([string]$Value) {
    $image = [Drawing.Image]::FromFile($Value)
    try {
        return @($image.Width, $image.Height)
    } finally {
        $image.Dispose()
    }
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$stripPath = Resolve-InputFile $CompactCurrencyStrip 'compact currency strip'
$ingotPath = Resolve-InputFile $IngotIcon 'ingot icon'
if ([string]::IsNullOrWhiteSpace($SelectedSlot)) {
    throw 'The rejected selected-slot asset was removed. Supply a newly user-approved square asset explicitly.'
}
$selectedSlotPath = Resolve-InputFile $SelectedSlot 'selected slot asset'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-confirmed-master-ui-integration-jsx.js') 'integration JSX builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory

$targetPages = @(
    '03_主角背包',
    '04_伙伴编队',
    '05_图鉴',
    '06_任务日志',
    '07_商店交易',
    '13_主角背包_物品选中',
    '14_伙伴编队_角色选中',
    '15_图鉴_怪物选中'
)

Assert-NewDestination $reportPath 'report'
Assert-NewDestination $validationPath 'validation'
if (Test-Path -LiteralPath $exportDirectoryPath) {
    $existingExports = @(Get-ChildItem -LiteralPath $exportDirectoryPath -File -ErrorAction SilentlyContinue)
    if ($existingExports.Count -gt 0) {
        throw "Export directory is not empty: $exportDirectoryPath"
    }
} else {
    [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null
}

$errorReceiptPath = "$reportPath.error.txt"
if (Test-Path -LiteralPath $errorReceiptPath) {
    Remove-Item -LiteralPath $errorReceiptPath
}

Add-Type -AssemblyName System.Drawing
$stripSize = Get-PngSize $stripPath
if ($stripSize[0] -ne 320 -or $stripSize[1] -ne 86) {
    throw "Unexpected compact currency strip dimensions: $($stripSize[0])x$($stripSize[1])"
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
}
$backupPath = Resolve-InputFile $backupPath 'integration backup PSD'
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($backupHash -ne $beforeHash) {
    throw "Integration backup does not match the current PSD: $backupHash"
}

$app = New-Object -ComObject Photoshop.Application
$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) {
    $document = $app.Documents.Item($index)
    $documentPath = $null
    try {
        $documentPath = [IO.Path]::GetFullPath([string]$document.FullName)
    } catch {
        continue
    }
    if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) {
        throw "Target PSD has unsaved Photoshop changes: $psdPath"
    }
}

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-confirmed-master-ui-integration.jsx'
if (Test-Path -LiteralPath $jsxPath) {
    Remove-Item -LiteralPath $jsxPath
}

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath `
    --psd $psdPath `
    --currency-strip $stripPath `
    --ingot $ingotPath `
    --selected-slot $selectedSlotPath `
    --output $jsxPath `
    --report $reportPath `
    --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) {
    throw "Integration JSX generation failed with exit code $LASTEXITCODE"
}

$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
    if (Test-Path -LiteralPath $errorReceiptPath -PathType Leaf) {
        throw "Photoshop failed: $(Get-Content -Raw -Encoding UTF8 -LiteralPath $errorReceiptPath)"
    }
    throw "Photoshop did not write the report: $reportPath"
}

$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected report status: $($reportData.status)" }
if ([int]$reportData.pageCount -ne 18) { throw "Expected eighteen top-level pages, got $($reportData.pageCount)" }
if (-not [bool]$reportData.nonTargetSignatureMatch) { throw 'Non-target page signature changed' }
if (-not [bool]$reportData.statePairBaseSignatureMatch) { throw 'Page 03 and page 13 base signatures differ' }
if ([bool]$reportData.standaloneBackpackImported) { throw 'Standalone backpack PSD was unexpectedly imported' }
if ([int]$reportData.shopPriceIconCount -ne 10) { throw 'Expected ten converted shop price icons' }
if ((@($reportData.currencyStripBox) -join ',') -ne '1570,28,320,86') {
    throw "Unexpected currency strip box: $(@($reportData.currencyStripBox) -join ',')"
}
if ((@($reportData.targetPages) -join '|') -ne ($targetPages -join '|')) {
    throw 'Target-page list changed'
}

$exportValidation = @()
foreach ($pageName in $targetPages) {
    $exportPath = Join-Path $exportDirectoryPath "$pageName.png"
    if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) {
        throw "Missing page export: $exportPath"
    }
    $size = Get-PngSize $exportPath
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) {
        throw "Unexpected export dimensions for ${pageName}: $($size[0])x$($size[1])"
    }
    $exportValidation += [ordered]@{
        page = $pageName
        path = $exportPath
        width = [int]$size[0]
        height = [int]$size[1]
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()
    }
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) {
    throw 'PSD hash did not change after confirmed UI integration'
}

$validationData = [ordered]@{
    status = 'PASS'
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.pageCount
    targetPages = @($reportData.targetPages)
    nonTargetSignatureMatch = [bool]$reportData.nonTargetSignatureMatch
    statePairBaseSignatureMatch = [bool]$reportData.statePairBaseSignatureMatch
    standaloneBackpackImported = [bool]$reportData.standaloneBackpackImported
    shopPriceIconCount = [int]$reportData.shopPriceIconCount
    shopInsufficientMessage = [string]$reportData.shopInsufficientMessage
    currencyStripBox = @($reportData.currencyStripBox)
    compactStripWidth = [int]$stripSize[0]
    compactStripHeight = [int]$stripSize[1]
    sourceHashes = [ordered]@{
        compactCurrencyStrip = (Get-FileHash -Algorithm SHA256 -LiteralPath $stripPath).Hash.ToLowerInvariant()
        ingotIcon = (Get-FileHash -Algorithm SHA256 -LiteralPath $ingotPath).Hash.ToLowerInvariant()
        selectedSlot = (Get-FileHash -Algorithm SHA256 -LiteralPath $selectedSlotPath).Hash.ToLowerInvariant()
    }
    exports = $exportValidation
}
$validationData | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'Confirmed Master UI integration PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
Write-Output "Validation: $validationPath"
