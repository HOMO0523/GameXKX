param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$ReferenceBase = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/AfterUser03BackpackSync/03_主角背包.png',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-page13-selection-visibility-repair.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page13-selection-visibility-repair.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page13-selection-visibility-repair.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ApprovedEngineIntegration/AfterPage13TooltipSelectionRepair'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputFile([string]$Value,[string]$Label){if(-not(Test-Path -LiteralPath $Value -PathType Leaf)){throw "Missing ${Label}: $Value"};(Resolve-Path -LiteralPath $Value).Path}
function Resolve-Destination([string]$Value){if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))}}
function Assert-New([string]$Value,[string]$Label){if(Test-Path -LiteralPath $Value){throw "Destination exists for ${Label}: $Value"};[IO.Directory]::CreateDirectory((Split-Path -Parent $Value))|Out-Null}
function Get-PngSize([string]$Value){$image=[Drawing.Image]::FromFile($Value);try{@($image.Width,$image.Height)}finally{$image.Dispose()}}

$projectRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath=Resolve-InputFile $Psd 'master PSD'
$referenceBasePath=Resolve-InputFile $ReferenceBase 'page-03 reference export'
$builderPath=Resolve-InputFile (Join-Path $PSScriptRoot 'build-page13-selection-visibility-repair-jsx.js') 'selection repair builder'
$backupPath=Resolve-Destination $Backup
$reportPath=Resolve-Destination $Report
$validationPath=Resolve-Destination $Validation
$exportDirectoryPath=Resolve-Destination $ExportDirectory
Assert-New $reportPath 'selection repair report'
Assert-New $validationPath 'selection repair validation'
if(Test-Path -LiteralPath $exportDirectoryPath){if(@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0){throw "Selection repair export directory is not empty: $exportDirectoryPath"}}else{[IO.Directory]::CreateDirectory($exportDirectoryPath)|Out-Null}

$beforeHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if(-not(Test-Path -LiteralPath $backupPath -PathType Leaf)){[IO.Directory]::CreateDirectory((Split-Path -Parent $backupPath))|Out-Null;Copy-Item -LiteralPath $psdPath -Destination $backupPath}
$backupHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if($beforeHash -ne $backupHash){throw 'Selection repair backup hash mismatch'}

$jsxDirectory=Join-Path $projectRoot 'tmp/ui_psd_pipeline';[IO.Directory]::CreateDirectory($jsxDirectory)|Out-Null
$jsxPath=Join-Path $jsxDirectory 'apply-page13-selection-visibility-repair.jsx'
if(Test-Path -LiteralPath $jsxPath){Remove-Item -LiteralPath $jsxPath}
$node=Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if($LASTEXITCODE -ne 0){throw "Selection repair JSX generation failed: $LASTEXITCODE"}
$app=New-Object -ComObject Photoshop.Application
$targetComparison=[IO.Path]::GetFullPath($psdPath)
for($index=1;$index -le $app.Documents.Count;$index++){$document=$app.Documents.Item($index);try{$documentPath=[IO.Path]::GetFullPath([string]$document.FullName)}catch{continue};if($documentPath.Equals($targetComparison,[StringComparison]::OrdinalIgnoreCase)-and -not $document.Saved){throw "Target PSD has unsaved Photoshop changes: $psdPath"}}
$app.Visible=$true
$app.DoJavaScriptFile($jsxPath)

if(-not(Test-Path -LiteralPath $reportPath -PathType Leaf)){$errorReceipt="$reportPath.error.txt";if(Test-Path -LiteralPath $errorReceipt){throw "Photoshop selection repair failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)"};throw "Photoshop did not write selection repair report: $reportPath"}
$reportData=[IO.File]::ReadAllText($reportPath,[Text.Encoding]::UTF8)|ConvertFrom-Json
if($reportData.status -ne 'PASS' -or [bool]$reportData.page03SelectionGroupVisible -or [bool]$reportData.page03SelectionInkVisible -or -not[bool]$reportData.page13SelectionGroupVisible -or -not[bool]$reportData.page13SelectionInkVisible -or -not[bool]$reportData.tooltipVisible){throw 'Unexpected selection repair report'}

Add-Type -AssemblyName System.Drawing
$exports=@()
foreach($record in @($reportData.exports)){$exportPath=[string]$record.path;if(-not(Test-Path -LiteralPath $exportPath -PathType Leaf)){throw "Missing selection repair export: $exportPath"};$size=Get-PngSize $exportPath;if($size[0]-ne 1920-or$size[1]-ne 1080){throw "Unexpected selection repair export dimensions: $exportPath"};$exports+=[ordered]@{page=[string]$record.name;path=$exportPath;width=1920;height=1080;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()}}
$pixelScript=@'
import json, sys
from PIL import Image
ref=Image.open(sys.argv[1]).convert('RGBA')
base=Image.open(sys.argv[2]).convert('RGBA')
selected=Image.open(sys.argv[3]).convert('RGBA')
def changed(a,b,box):
    aa=a.crop(box); bb=b.crop(box)
    return sum(1 for x,y in zip(aa.getdata(),bb.getdata()) if x != y)
result={
  'base_pixel_identical': list(ref.getdata()) == list(base.getdata()),
  'selection_changed_pixels': changed(base,selected,(1100,260,1270,340)),
  'tooltip_changed_pixels': changed(base,selected,(1090,400,1550,700)),
  'outside_changed_pixels': changed(base,selected,(300,170,1090,1000)),
}
print(json.dumps(result))
'@
$pixelResult=($pixelScript | python - $referenceBasePath $exports[0].path $exports[1].path)|ConvertFrom-Json
if(-not[bool]$pixelResult.base_pixel_identical){throw 'Page 03 pixels changed during selection repair'}
if([int]$pixelResult.selection_changed_pixels -le 0){throw 'Page 13 selected-state ink is not visible in the selection region'}
if([int]$pixelResult.tooltip_changed_pixels -le 0){throw 'Page 13 tooltip is not visible in the tooltip region'}
if([int]$pixelResult.outside_changed_pixels -ne 0){throw 'Unexpected pixels changed outside the selected-state and tooltip regions'}

$afterHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if($afterHash -eq $beforeHash){throw 'PSD hash did not change after selection visibility repair'}
[ordered]@{status='PASS';beforeSha256=$beforeHash;backupSha256=$backupHash;afterSha256=$afterHash;backupMatchesBefore=($beforeHash-eq$backupHash);topLevelPageCount=[int]$reportData.topLevelPageCount;page03SelectionHidden=$true;page13SelectionVisible=$true;tooltipVisible=$true;selectionInkBounds=@($reportData.selectionInkBounds);pixelChecks=[ordered]@{page03PixelIdentical=[bool]$pixelResult.base_pixel_identical;selectionChangedPixels=[int]$pixelResult.selection_changed_pixels;tooltipChangedPixels=[int]$pixelResult.tooltip_changed_pixels;outsideChangedPixels=[int]$pixelResult.outside_changed_pixels};exports=$exports}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath $validationPath
Write-Output 'Page-13 tooltip and selection visibility PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
