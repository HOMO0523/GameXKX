param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-page06-task-log.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page06-task-log.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page06-task-log.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/RedesignPages/AfterPage06TaskLog'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputFile([string]$Value, [string]$Label) { if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) { throw "Missing ${Label}: $Value" }; (Resolve-Path -LiteralPath $Value).Path }
function Resolve-Destination([string]$Value) { if ([IO.Path]::IsPathRooted($Value)) { [IO.Path]::GetFullPath($Value) } else { [IO.Path]::GetFullPath((Join-Path (Get-Location) $Value)) } }
function Assert-New([string]$Value, [string]$Label) { if (Test-Path -LiteralPath $Value) { throw "Destination exists for ${Label}: $Value" }; [IO.Directory]::CreateDirectory((Split-Path -Parent $Value)) | Out-Null }
function Get-PngSize([string]$Value) { $image = [Drawing.Image]::FromFile($Value); try { @($image.Width, $image.Height) } finally { $image.Dispose() } }

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-page06-task-log-jsx.js') 'page-06 task log builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$validationPath = Resolve-Destination $Validation
$exportDirectoryPath = Resolve-Destination $ExportDirectory
Assert-New $reportPath 'page-06 task log report'
Assert-New $validationPath 'page-06 task log validation'
if (Test-Path -LiteralPath $exportDirectoryPath) { if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0) { throw "Task log export directory is not empty: $exportDirectoryPath" } }
else { [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null }

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) { [IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath)) | Out-Null; Copy-Item -LiteralPath $psdPath -Destination $backupPath }
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) { throw 'Page-06 task log backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-page06-task-log.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }
$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "Page-06 task log JSX generation failed: $LASTEXITCODE" }

$app = $null
for ($comAttempt = 1; $comAttempt -le 12 -and $null -eq $app; $comAttempt++) {
    try { $app = New-Object -ComObject Photoshop.Application } catch { $app = $null; Start-Sleep -Seconds 3 }
}
if ($null -eq $app) { throw 'Could not start Photoshop COM after retries' }
$targetComparison = [IO.Path]::GetFullPath($psdPath)
for ($index = 1; $index -le $app.Documents.Count; $index++) { $document = $app.Documents.Item($index); try { $documentPath = [IO.Path]::GetFullPath([string]$document.FullName) } catch { continue }; if ($documentPath.Equals($targetComparison, [StringComparison]::OrdinalIgnoreCase) -and -not $document.Saved) { throw "Target PSD has unsaved Photoshop changes: $psdPath" } }
$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $reportPath -PathType Leaf)) { $errorReceipt = "$reportPath.error.txt"; if (Test-Path -LiteralPath $errorReceipt) { throw "Photoshop page-06 task log pass failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)" }; throw "Photoshop did not write page-06 task log report: $reportPath" }
$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw 'Unexpected page-06 task log report status' }
if (@($reportData.legacyMoved).Count -ne 18) { throw "Unexpected page-06 legacy move count: $(@($reportData.legacyMoved).Count)" }
if (@($reportData.textLayers).Count -ne 19) { throw "Unexpected page-06 text layer count: $(@($reportData.textLayers).Count)" }
if ([int]$reportData.rowCount -ne 7) { throw 'Unexpected page-06 row count' }
if (-not [bool]$reportData.emptyHintHidden) { throw 'Page-06 empty hint should be hidden' }

Add-Type -AssemblyName System.Drawing
$exports = @()
foreach ($record in @($reportData.exports)) { $exportPath = [string]$record.path; if (-not (Test-Path -LiteralPath $exportPath -PathType Leaf)) { throw "Missing page-06 task log export: $exportPath" }; $size = Get-PngSize $exportPath; if ($size[0] -ne 1920 -or $size[1] -ne 1080) { throw "Unexpected task log export dimensions: $exportPath" }; $exports += [ordered]@{ page=[string]$record.name; path=$exportPath; width=1920; height=1080; sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant() } }

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw 'PSD hash did not change after page-06 task log composition' }
[ordered]@{ status='PASS'; beforeSha256=$beforeHash; backupSha256=$backupHash; afterSha256=$afterHash; backupMatchesBefore=($beforeHash -eq $backupHash); topLevelPageCount=[int]$reportData.topLevelPageCount; targetPage=[string]$reportData.targetPage; legacyMoved=@($reportData.legacyMoved); titleBounds=@($reportData.titleBounds); inkBounds=@($reportData.inkBounds); paperBounds=@($reportData.paperBounds); trackButtonBounds=@($reportData.trackButtonBounds); closeButtonBounds=@($reportData.closeButtonBounds); closeInkBounds=@($reportData.closeInkBounds); textLayers=@($reportData.textLayers); exports=$exports } | ConvertTo-Json -Depth 8 | Set-Content -Encoding UTF8 -LiteralPath $validationPath

try { $app.DoJavaScript('if (app.documents.length > 0) { for (var i = app.documents.length - 1; i >= 0; i--) { try { app.documents[i].close(SaveOptions.DONOTSAVECHANGES); } catch (e) {} } }') } catch {}
try { $app.Quit() } catch {}
Write-Output 'Page-06 task log composition PASS (PS closed)'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
