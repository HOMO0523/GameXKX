param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$TownBackground = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Generated/town_background_clean_no_ui.png',
    [string]$ShellComponents = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents',
    [string]$CurrencyPanel = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/currency_panel.png',
    [string]$IngotIcon = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/resource_gold.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-town-hud-v2.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.town-hud.validation.json',
    [string]$BeforeExport,
    [string]$AfterExport,
    [string]$DerivedStrip = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png'
)

$ErrorActionPreference = 'Stop'

function Resolve-InputFile([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) {
        throw "Missing ${Label}: $Value"
    }
    return (Resolve-Path -LiteralPath $Value).Path
}

function Resolve-InputDirectory([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Container)) {
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

function New-CompactCurrencyStrip([string]$SourcePath, [string]$DestinationPath) {
    $targetWidth = 320
    $targetHeight = 86
    $capWidth = 54
    if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
        $existingSize = Get-PngSize $DestinationPath
        if ($existingSize[0] -ne $targetWidth -or $existingSize[1] -ne $targetHeight) {
            throw "Existing compact currency strip has unexpected dimensions: $($existingSize[0])x$($existingSize[1])"
        }
        return
    }

    [IO.Directory]::CreateDirectory((Split-Path -Parent $DestinationPath)) | Out-Null
    $sourceBitmap = [Drawing.Bitmap]::new($SourcePath)
    $targetBitmap = [Drawing.Bitmap]::new(
        $targetWidth,
        $targetHeight,
        [Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    $graphics = [Drawing.Graphics]::FromImage($targetBitmap)
    try {
        $graphics.CompositingMode = [Drawing.Drawing2D.CompositingMode]::SourceCopy
        $graphics.CompositingQuality = [Drawing.Drawing2D.CompositingQuality]::HighQuality
        $graphics.InterpolationMode = [Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode = [Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage(
            $sourceBitmap,
            [Drawing.Rectangle]::new(0, 0, $capWidth, $targetHeight),
            [Drawing.Rectangle]::new(0, 0, $capWidth, $sourceBitmap.Height),
            [Drawing.GraphicsUnit]::Pixel
        )
        $graphics.DrawImage(
            $sourceBitmap,
            [Drawing.Rectangle]::new($capWidth, 0, $targetWidth - (2 * $capWidth), $targetHeight),
            [Drawing.Rectangle]::new($capWidth, 0, $sourceBitmap.Width - (2 * $capWidth), $sourceBitmap.Height),
            [Drawing.GraphicsUnit]::Pixel
        )
        $graphics.DrawImage(
            $sourceBitmap,
            [Drawing.Rectangle]::new($targetWidth - $capWidth, 0, $capWidth, $targetHeight),
            [Drawing.Rectangle]::new($sourceBitmap.Width - $capWidth, 0, $capWidth, $sourceBitmap.Height),
            [Drawing.GraphicsUnit]::Pixel
        )
        $targetBitmap.Save($DestinationPath, [Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $graphics.Dispose()
        $targetBitmap.Dispose()
        $sourceBitmap.Dispose()
    }
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$townHudName = '02_' + (-join @([char]0x57CE, [char]0x9547)) + 'HUD'
if ([string]::IsNullOrWhiteSpace($BeforeExport)) {
    $BeforeExport = "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/Before/${townHudName}.png"
}
if ([string]::IsNullOrWhiteSpace($AfterExport)) {
    $AfterExport = "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/After/${townHudName}.png"
}

$psdPath = Resolve-InputFile $Psd 'master PSD'
$backgroundPath = Resolve-InputFile $TownBackground 'clean town background'
$shellDirectory = Resolve-InputDirectory $ShellComponents 'shell-component directory'
$currencyPanelPath = Resolve-InputFile $CurrencyPanel 'currency paper source'
$ingotPath = Resolve-InputFile $IngotIcon 'ingot icon'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-town-hud-redesign-jsx.js') 'town-HUD JSX builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$beforeExportPath = Resolve-Destination $BeforeExport
$afterExportPath = Resolve-Destination $AfterExport
$derivedStripPath = Resolve-Destination $DerivedStrip

foreach ($shellName in @(
    'identity_panel.png',
    'nav_disc_backpack.png',
    'nav_disc_companion.png',
    'nav_disc_codex.png',
    'nav_disc_task.png',
    'nav_disc_route.png'
)) {
    Resolve-InputFile (Join-Path $shellDirectory $shellName) "shell component $shellName" | Out-Null
}

foreach ($destinationRecord in @(
    [pscustomobject]@{ Path = $reportPath; Label = 'report' },
    [pscustomobject]@{ Path = $validationPath; Label = 'validation' },
    [pscustomobject]@{ Path = $beforeExportPath; Label = 'before export' },
    [pscustomobject]@{ Path = $afterExportPath; Label = 'after export' }
)) {
    Assert-NewDestination $destinationRecord.Path $destinationRecord.Label
}
$errorReceiptPath = "$reportPath.error.txt"
if (Test-Path -LiteralPath $errorReceiptPath) {
    Remove-Item -LiteralPath $errorReceiptPath
}

Add-Type -AssemblyName System.Drawing
New-CompactCurrencyStrip $currencyPanelPath $derivedStripPath
$derivedStripPath = Resolve-InputFile $derivedStripPath 'derived compact currency strip'
$stripSize = Get-PngSize $derivedStripPath
if ($stripSize[0] -ne 320 -or $stripSize[1] -ne 86) {
    throw "Unexpected compact currency strip dimensions: $($stripSize[0])x$($stripSize[1])"
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
    [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null
    Copy-Item -LiteralPath $psdPath -Destination $backupPath
}
$backupPath = Resolve-InputFile $backupPath 'town-HUD backup PSD'
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($backupHash -ne $beforeHash) {
    throw "Town-HUD backup does not match the current PSD: $backupHash"
}

$app = $null
try {
    $app = [Runtime.InteropServices.Marshal]::GetActiveObject('Photoshop.Application')
} catch {
    for ($attempt = 1; $attempt -le 5 -and $null -eq $app; $attempt++) {
        try {
            $app = New-Object -ComObject Photoshop.Application
        } catch {
            if ($attempt -eq 5) { throw }
            Start-Sleep -Seconds 2
        }
    }
}

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
$jsxPath = Join-Path $jsxDirectory 'apply-town-hud-redesign.jsx'
if (Test-Path -LiteralPath $jsxPath) {
    Remove-Item -LiteralPath $jsxPath
}

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath `
    --psd $psdPath `
    --background $backgroundPath `
    --shell-components $shellDirectory `
    --currency-strip $derivedStripPath `
    --ingot $ingotPath `
    --output $jsxPath `
    --report $reportPath `
    --before-export $beforeExportPath `
    --after-export $afterExportPath
if ($LASTEXITCODE -ne 0) {
    throw "Town-HUD JSX generation failed with exit code $LASTEXITCODE"
}

$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
    if (Test-Path -LiteralPath $errorReceiptPath -PathType Leaf) {
        throw "Photoshop failed: $(Get-Content -Raw -Encoding UTF8 -LiteralPath $errorReceiptPath)"
    }
    throw "Photoshop did not write the report: $reportPath"
}
foreach ($exportPath in @($beforeExportPath, $afterExportPath)) {
    if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) {
        throw "Photoshop did not write the page export: $exportPath"
    }
}

$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected report status: $($reportData.status)" }
if ($reportData.page -ne $townHudName) { throw "Unexpected report page: $($reportData.page)" }
if ([int]$reportData.pageCount -ne 18) { throw "Expected eighteen top-level pages, got $($reportData.pageCount)" }
if (-not [bool]$reportData.nonTargetSignatureMatch) { throw 'Non-target page signature changed' }
if (-not [bool]$reportData.shopPeerSignatureMatch) { throw 'Shop peer signature changed' }
if ([bool]$reportData.persistentPromptVisible) { throw 'Persistent town prompt remains visible' }
if (@($reportData.navigationLayers).Count -ne 5) { throw 'Expected five navigation layers' }
if ((@($reportData.currencyStripBox) -join ',') -ne '1570,28,320,86') {
    throw "Unexpected currency strip box: $(@($reportData.currencyStripBox) -join ',')"
}

$beforeSize = Get-PngSize $beforeExportPath
$afterSize = Get-PngSize $afterExportPath
foreach ($sizeRecord in @(
    [pscustomobject]@{ Size = $beforeSize; Label = 'before' },
    [pscustomobject]@{ Size = $afterSize; Label = 'after' }
)) {
    $size = $sizeRecord.Size
    if ($size[0] -ne 1920 -or $size[1] -ne 1080) {
        throw "Unexpected $($sizeRecord.Label) export dimensions: $($size[0])x$($size[1])"
    }
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) {
    throw 'PSD hash did not change after town-HUD assembly'
}

$validationData = [ordered]@{
    status = 'PASS'
    page = $townHudName
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.pageCount
    nonTargetSignatureMatch = [bool]$reportData.nonTargetSignatureMatch
    shopPeerSignatureMatch = [bool]$reportData.shopPeerSignatureMatch
    persistentPromptVisible = [bool]$reportData.persistentPromptVisible
    currencyStripBox = @($reportData.currencyStripBox)
    navigationLayers = @($reportData.navigationLayers)
    sourceHashes = [ordered]@{
        background = (Get-FileHash -Algorithm SHA256 -LiteralPath $backgroundPath).Hash.ToLowerInvariant()
        compactCurrencyStrip = (Get-FileHash -Algorithm SHA256 -LiteralPath $derivedStripPath).Hash.ToLowerInvariant()
        ingotIcon = (Get-FileHash -Algorithm SHA256 -LiteralPath $ingotPath).Hash.ToLowerInvariant()
    }
    beforeExport = $beforeExportPath
    afterExport = $afterExportPath
    exportWidth = 1920
    exportHeight = 1080
    compactStripWidth = [int]$stripSize[0]
    compactStripHeight = [int]$stripSize[1]
}
$validationData | ConvertTo-Json -Depth 6 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output 'Town-HUD PSD update PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Before export: $beforeExportPath"
Write-Output "After export:  $afterExportPath"
Write-Output "Validation: $validationPath"
