param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Paper = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/main_shop_panel.png',
    [string]$Slot = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Components/detail_item_slot.png',
    [string]$ReferenceBase = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/AfterUser03BackpackSync/03_主角背包.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-page13-selected-tooltip.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page13-selected-tooltip.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page13-selected-tooltip.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/AfterPage13SelectedTooltip'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputFile([string]$Value, [string]$Label) { if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) { throw "Missing ${Label}: $Value" }; (Resolve-Path -LiteralPath $Value).Path }
function Resolve-Destination([string]$Value) { if ([IO.Path]::IsPathRooted($Value)) { [IO.Path]::GetFullPath($Value) } else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Value)) } }
function Assert-New([string]$Value, [string]$Label) { if (Test-Path -LiteralPath $Value) { throw "Destination exists for ${Label}: $Value" }; [IO.Directory]::CreateDirectory((Split-Path -Parent $Value)) | Out-Null }
function Get-PngSize([string]$Value) { $image = [Drawing.Image]::FromFile($Value); try { @($image.Width, $image.Height) } finally { $image.Dispose() } }

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$paperPath = Resolve-InputFile $Paper 'approved current paper'
$slotPath = Resolve-InputFile $Slot 'approved detail item slot'
$referenceBasePath = Resolve-InputFile $ReferenceBase 'page-03 reference export'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-page13-selected-tooltip-jsx.js') 'page-13 tooltip builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory
Assert-New $reportPath 'page-13 tooltip report'
Assert-New $validationPath 'page-13 tooltip validation'
if (Test-Path -LiteralPath $exportDirectoryPath) { if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0) { throw "Tooltip export directory is not empty: $exportDirectoryPath" } }
else { [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null }

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) { [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null; Copy-Item -LiteralPath $psdPath -Destination $backupPath }
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) { throw 'Page-13 tooltip backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-page13-selected-tooltip.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }
$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --paper $paperPath --slot $slotPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "Page-13 tooltip JSX generation failed: $LASTEXITCODE" }

$app = New-Object -ComObject Photoshop.Application
$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) { $document = $app.Documents.Item($index); try { $documentPath = [IO.Path]::GetFullPath([string]$document.FullName) } catch { continue }; if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) { throw "Target PSD has unsaved Photoshop changes: $psdPath" } }
$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { $errorReceipt = "$reportPath.error.txt"; if (Test-Path -LiteralPath $errorReceipt) { throw "Photoshop page-13 tooltip pass failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)" }; throw "Photoshop did not write page-13 tooltip report: $reportPath" }
$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS' -or -not [bool]$reportData.selectedStateVisibleOnlyOnPage13 -or @($reportData.textLayers).Count -ne 6) { throw 'Unexpected page-13 tooltip report' }

Add-Type -AssemblyName System.Drawing
$exports = @()
foreach ($record in @($reportData.exports)) { $exportPath = [string]$record.path; if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing page-13 tooltip export: $exportPath" }; $size = Get-PngSize $exportPath; if ($size[0] -ne 1920 -or $size[1] -ne 1080) { throw "Unexpected tooltip export dimensions: $exportPath" }; $exports += [ordered]@{ page=[string]$record.name; path=$exportPath; width=1920; height=1080; sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant() } }
$referenceBaseHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $referenceBasePath).Hash.ToLowerInvariant()
if ($exports[0].sha256 -ne $referenceBaseHash) { throw 'Page 03 changed during page-13 tooltip composition' }
if ($exports[0].sha256 -eq $exports[1].sha256) { throw 'Page 13 tooltip export does not differ from page 03' }

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after page-13 tooltip composition' }
[ordered]@{ status='PASS'; beforeSha256=$beforeHash; backupSha256=$backupHash; afterSha256=$afterHash; backupMatchesBefore=($beforeHash-eq$backupHash); topLevelPageCount=[int]$reportData.topLevelPageCount; targetPage=[string]$reportData.targetPage; tooltipGroup=[string]$reportData.tooltipGroup; composition=@($reportData.composition); paperBounds=@($reportData.paperBounds); slotBounds=@($reportData.slotBounds); iconBounds=@($reportData.iconBounds); textLayers=@($reportData.textLayers); page03Unchanged=$true; selectedStateVisibleOnlyOnPage13=$true; exports=$exports } | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $validationPath
Write-Output 'Page-13 selected tooltip composition PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
