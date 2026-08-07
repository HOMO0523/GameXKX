param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Portrait = 'SourceAssets/PartyDeck/card-portraits/generated/hero.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-page18-card-portraits.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page18-card-portraits.report.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/Page18CardPortraits'
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

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-InputFile $Psd 'master PSD'
$portraitPath = Resolve-InputFile $Portrait 'hero card portrait'
$builderPath = Resolve-InputFile (Join-Path $PSScriptRoot 'build-page18-card-portraits-jsx.js') 'page-18 card portrait builder'
$backupPath = Resolve-Destination $Backup
$reportPath = Resolve-Destination $Report
$exportDirectoryPath = Resolve-Destination $ExportDirectory

Assert-New $reportPath 'page-18 card portrait report'
if (Test-Path -LiteralPath $exportDirectoryPath) {
    if (@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0) { throw "Page-18 card portrait export directory is not empty: $exportDirectoryPath" }
} else {
    [IO.Directory]::CreateDirectory($exportDirectoryPath) | Out-Null
}

$beforeHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
Assert-New $backupPath 'page-18 card portrait backup'
Copy-Item -LiteralPath $psdPath -Destination $backupPath
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($beforeHash -ne $backupHash) { throw 'Page-18 card portrait backup hash mismatch' }

$jsxDirectory = Join-Path $projectRoot 'tmp/ui_psd_pipeline'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-page18-card-portraits.jsx'
if (Test-Path -LiteralPath $jsxPath) { Remove-Item -LiteralPath $jsxPath }

$node = Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --portrait $portraitPath --export-dir $exportDirectoryPath
if ($LASTEXITCODE -ne 0) { throw "Page-18 card portrait JSX generation failed: $LASTEXITCODE" }

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
    if (Test-Path -LiteralPath $errorReceipt) { throw "Photoshop page-18 card portrait placement failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)" }
    throw "Photoshop did not write page-18 card portrait report: $reportPath"
}

$reportData = [IO.File]::ReadAllText($reportPath, [Text.Encoding]::UTF8) | ConvertFrom-Json
if ($reportData.status -ne 'PASS') { throw "Page-18 card portrait report status is not PASS" }
if ($reportData.frameCount -ne 9) { throw "Page-18 card portrait frame count is not 9: $($reportData.frameCount)" }
foreach ($entry in $reportData.placed) {
    if (-not $entry.portrait -or $entry.portrait.Count -ne 4) { throw "Page-18 card portrait entry is missing placement bounds: $($entry.cardName)" }
    $w = $entry.portrait[2] - $entry.portrait[0]
    $h = $entry.portrait[3] - $entry.portrait[1]
    if ($w -le 0 -or $h -le 0) { throw "Page-18 card portrait has invalid size for $($entry.cardName): $w x $h" }
}

$afterHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($afterHash -eq $beforeHash) { throw "Page-18 card portrait placement did not change the PSD: $psdPath" }

$exportFile = Join-Path $exportDirectoryPath '18_主角背包_卡组页.png'
if (-not (Test-Path -LiteralPath $exportFile -PathType Leaf)) { throw "Page-18 card portrait export is missing: $exportFile" }
$exportHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $exportFile).Hash.ToLowerInvariant()

$summary = [pscustomobject]@{
    status = $reportData.status
    frameCount = $reportData.frameCount
    placed = @($reportData.placed | ForEach-Object { "$($_.cardName) -> layer=$($_.layerName) at ($($_.portrait[0]),$($_.portrait[1])) size=$($_.size[0])x$($_.size[1])" })
    psdBefore = $beforeHash
    psdAfter = $afterHash
    backup = $backupPath
    export = $exportFile
    exportSha256 = $exportHash
}
$summaryPath = Join-Path $exportDirectoryPath 'placement-summary.json'
$summary | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $summaryPath -Encoding UTF8
Write-Output 'Page-18 card portrait placement: PASS'
Write-Output "PSD before: $beforeHash"
Write-Output "PSD after:  $afterHash"
Write-Output "Backup: $backupPath"
Write-Output "Export:  $exportFile"
Write-Output "Summary: $summaryPath"
