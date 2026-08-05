param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Illustration = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/main_menu_tiger_hero_v9_loose_inkwash.png',
    [string]$Brush = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/category_selected_ink.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-main-menu-tiger-hero.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-tiger-hero.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.main-menu-tiger-hero.validation.json',
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

function New-RecoloredBrush(
    [string]$SourcePath,
    [string]$DestinationPath,
    [int]$Red,
    [int]$Green,
    [int]$Blue
) {
    if (Test-Path -LiteralPath $DestinationPath -PathType Leaf) {
        return
    }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $DestinationPath)) | Out-Null
    Add-Type -AssemblyName System.Drawing
    $sourceBitmap = [System.Drawing.Bitmap]::new($SourcePath)
    $targetBitmap = [System.Drawing.Bitmap]::new(
        $sourceBitmap.Width,
        $sourceBitmap.Height,
        [System.Drawing.Imaging.PixelFormat]::Format32bppArgb
    )
    try {
        for ($y = 0; $y -lt $sourceBitmap.Height; $y++) {
            for ($x = 0; $x -lt $sourceBitmap.Width; $x++) {
                $sourcePixel = $sourceBitmap.GetPixel($x, $y)
                $targetPixel = [System.Drawing.Color]::FromArgb($sourcePixel.A, $Red, $Green, $Blue)
                $targetBitmap.SetPixel($x, $y, $targetPixel)
            }
        }
        $targetBitmap.Save($DestinationPath, [System.Drawing.Imaging.ImageFormat]::Png)
    } finally {
        $targetBitmap.Dispose()
        $sourceBitmap.Dispose()
    }
}

$mainMenuName = -join @([char]0x4E3B, [char]0x83DC, [char]0x5355)
$mainMenuTitle = -join @([char]0x971E, [char]0x5BA2, [char]0x884C)
$startGameLabel = -join @([char]0x5F00, [char]0x59CB, [char]0x6E38, [char]0x620F)
$loadGameLabel = -join @([char]0x52A0, [char]0x8F7D, [char]0x5B58, [char]0x6863)
$settingsLabel = -join @([char]0x8BBE, [char]0x7F6E, [char]0x6E38, [char]0x620F)
$exitLabel = -join @([char]0x9000, [char]0x51FA)
if ([string]::IsNullOrWhiteSpace($Export)) {
    $Export = "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/MainMenuTigerHero/After/01_${mainMenuName}.png"
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-ExistingFile $Psd 'master PSD'
$illustrationPath = Resolve-ExistingFile $Illustration 'main-menu illustration'
$brushPath = Resolve-ExistingFile $Brush 'brush component'
$backupPath = Resolve-ExistingFile $Backup 'master PSD backup'
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportPath = Resolve-Destination $Export
$builderPath = Resolve-ExistingFile (Join-Path $PSScriptRoot 'build-main-menu-tiger-hero-jsx.js') 'JSX builder'
$derivedBrushDirectory = Join-Path $projectRoot 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/components'
$primaryBrushPath = Join-Path $derivedBrushDirectory 'menu_brush_primary_from_kit.png'
$normalBrushPath = Join-Path $derivedBrushDirectory 'menu_brush_normal_from_kit.png'

New-RecoloredBrush $brushPath $primaryBrushPath 18 90 85
New-RecoloredBrush $brushPath $normalBrushPath 36 75 72
$primaryBrushPath = Resolve-ExistingFile $primaryBrushPath 'derived primary brush component'
$normalBrushPath = Resolve-ExistingFile $normalBrushPath 'derived normal brush component'

foreach ($destination in @($reportPath, $validationPath, $exportPath)) {
    if (Test-Path -LiteralPath $destination) {
        throw "Destination already exists: $destination"
    }
    [IO.Directory]::CreateDirectory((Split-Path -Parent $destination)) | Out-Null
}
$errorReceiptPath = "$reportPath.error.txt"
if (Test-Path -LiteralPath $errorReceiptPath -PathType Leaf) {
    Remove-Item -LiteralPath $errorReceiptPath
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) {
    throw "Current PSD does not match the locked backup: $beforeHash"
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
$jsxPath = Join-Path $jsxDirectory 'apply-main-menu-tiger-hero.jsx'
if (Test-Path -LiteralPath $jsxPath) {
    Remove-Item -LiteralPath $jsxPath
}

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath `
    --psd $psdPath `
    --image $illustrationPath `
    --brush-primary $primaryBrushPath `
    --brush-normal $normalBrushPath `
    --output $jsxPath `
    --report $reportPath `
    --export $exportPath
if ($LASTEXITCODE -ne 0) {
    throw "Main-menu JSX generation failed with exit code $LASTEXITCODE"
}

$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) {
    $errorReceipt = "$reportPath.error.txt"
    if (Test-Path -LiteralPath $errorReceipt) {
        throw "Photoshop failed: $(Get-Content -Raw -LiteralPath $errorReceipt)"
    }
    throw "Photoshop did not write the report: $reportPath"
}
if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) {
    throw "Photoshop did not write the page export: $exportPath"
}

$reportData = Get-Content -Raw -Encoding UTF8 -LiteralPath $reportPath | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Unexpected report status: $($reportData.status)" }
if ($reportData.page -ne "01_$mainMenuName") { throw "Unexpected page: $($reportData.page)" }
if ([int]$reportData.pageCount -ne 18) { throw "Expected eighteen top-level pages, got $($reportData.pageCount)" }
if (-not [bool]$reportData.nonTargetSignatureMatch) { throw 'Non-target page signature changed' }
if ($reportData.title -ne $mainMenuTitle) { throw "Unexpected title: $($reportData.title)" }
$expectedLabels = @($startGameLabel, $loadGameLabel, $settingsLabel, $exitLabel)
if ((@($reportData.buttonLabels) -join '|') -ne ($expectedLabels -join '|')) {
    throw "Unexpected button labels: $(@($reportData.buttonLabels) -join ', ')"
}

Add-Type -AssemblyName System.Drawing
$image = [System.Drawing.Image]::FromFile($exportPath)
try {
    $exportWidth = $image.Width
    $exportHeight = $image.Height
} finally {
    $image.Dispose()
}
if ($exportWidth -ne 1920 -or $exportHeight -ne 1080) {
    throw "Unexpected export dimensions: ${exportWidth}x${exportHeight}"
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) {
    throw 'PSD hash did not change after main-menu assembly'
}

$validationData = [ordered]@{
    status = 'PASS'
    page = "01_$mainMenuName"
    beforeSha256 = $beforeHash
    backupSha256 = $backupHash
    afterSha256 = $afterHash
    backupMatchesBefore = ($beforeHash -eq $backupHash)
    topLevelPageCount = [int]$reportData.pageCount
    nonTargetSignatureMatch = [bool]$reportData.nonTargetSignatureMatch
    title = [string]$reportData.title
    buttonLabels = @($reportData.buttonLabels)
    createdGroups = @($reportData.createdGroups)
    legacyGroups = @($reportData.legacyGroups)
    illustration = $illustrationPath
    brushSource = $brushPath
    brushComponents = @($reportData.brushComponents)
    export = $exportPath
    exportWidth = $exportWidth
    exportHeight = $exportHeight
}
$validationData | ConvertTo-Json -Depth 5 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

Write-Output "Main-menu PSD update PASS"
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Export: $exportPath"
Write-Output "Validation: $validationPath"
