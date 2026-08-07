param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Backup = 'outputs/UI_PSD/Candidates/Backups/GameXXK_UI_Master_V1.before-page18-close-control-repair.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page18-close-control-repair.report.json',
    [string]$Validation = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.page18-close-control-repair.validation.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ManualEditing/Page18CharacterDeckWorkspaceFinal'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputFile([string]$Value,[string]$Label){if(-not(Test-Path -LiteralPath $Value -PathType Leaf)){throw "Missing ${Label}: $Value"};(Resolve-Path -LiteralPath $Value).Path}
function Resolve-Destination([string]$Value){if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))}}
function Assert-New([string]$Value,[string]$Label){if(Test-Path -LiteralPath $Value){throw "Destination exists for ${Label}: $Value"};[IO.Directory]::CreateDirectory((Split-Path -Parent $Value))|Out-Null}
function Get-PngSize([string]$Value){$image=[Drawing.Image]::FromFile($Value);try{@($image.Width,$image.Height)}finally{$image.Dispose()}}

$projectRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath=Resolve-InputFile $Psd 'master PSD'
$builderPath=Resolve-InputFile (Join-Path $PSScriptRoot 'build-page18-close-control-repair-jsx.js') 'page-18 close repair builder'
$backupPath=Resolve-Destination $Backup
$reportPath=Resolve-Destination $Report
$validationPath=Resolve-Destination $Validation
$exportDirectoryPath=Resolve-Destination $ExportDirectory
Assert-New $backupPath 'page-18 close repair backup'
Assert-New $reportPath 'page-18 close repair report'
Assert-New $validationPath 'page-18 close repair validation'
if(Test-Path -LiteralPath $exportDirectoryPath){if(@(Get-ChildItem -LiteralPath $exportDirectoryPath -File).Count -gt 0){throw "Page-18 close repair export directory is not empty: $exportDirectoryPath"}}else{[IO.Directory]::CreateDirectory($exportDirectoryPath)|Out-Null}

$beforeHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
Copy-Item -LiteralPath $psdPath -Destination $backupPath
$backupHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if($beforeHash-ne$backupHash){throw 'Page-18 close repair backup hash mismatch'}

$jsxDirectory=Join-Path $projectRoot 'tmp/ui_psd_pipeline';[IO.Directory]::CreateDirectory($jsxDirectory)|Out-Null
$jsxPath=Join-Path $jsxDirectory 'apply-page18-close-control-repair.jsx'
if(Test-Path -LiteralPath $jsxPath){Remove-Item -LiteralPath $jsxPath}
$node=Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if($LASTEXITCODE-ne 0){throw "Page-18 close repair JSX generation failed: $LASTEXITCODE"}
$app=New-Object -ComObject Photoshop.Application
$targetComparison=[IO.Path]::GetFullPath($psdPath)
for($index=1;$index-le$app.Documents.Count;$index++){$document=$app.Documents.Item($index);try{$documentPath=[IO.Path]::GetFullPath([string]$document.FullName)}catch{continue};if($documentPath.Equals($targetComparison,[StringComparison]::OrdinalIgnoreCase)-and -not$document.Saved){throw "Target PSD has unsaved Photoshop changes: $psdPath"}}
$app.Visible=$true
$app.DoJavaScriptFile($jsxPath)

if(-not(Test-Path -LiteralPath $reportPath -PathType Leaf)){$errorReceipt="$reportPath.error.txt";if(Test-Path -LiteralPath $errorReceipt){throw "Photoshop page-18 close repair failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)"};throw "Photoshop did not write page-18 close repair report: $reportPath"}
$reportData=[IO.File]::ReadAllText($reportPath,[Text.Encoding]::UTF8)|ConvertFrom-Json
if($reportData.status-ne'PASS'){throw 'Unexpected page-18 close repair report'}

Add-Type -AssemblyName System.Drawing
$exports=@()
foreach($record in @($reportData.exports)){$exportPath=[string]$record.path;if(-not(Test-Path -LiteralPath $exportPath -PathType Leaf)){throw "Missing page-18 close repair export: $exportPath"};$size=Get-PngSize $exportPath;if($size[0]-ne 1920-or$size[1]-ne 1080){throw "Unexpected page-18 close repair dimensions: $exportPath"};$exports+=[ordered]@{page=[string]$record.name;path=$exportPath;width=1920;height=1080;sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()}}
$pixelScript=@'
import json, sys
from PIL import Image
a=Image.open(sys.argv[1]).convert('RGBA')
b=Image.open(sys.argv[2]).convert('RGBA')
print(json.dumps({'pixel_identical': list(a.getdata()) == list(b.getdata())}))
'@
$pixelResult=($pixelScript|python - $exports[0].path $exports[1].path)|ConvertFrom-Json
if(-not[bool]$pixelResult.pixel_identical){throw 'Repaired page 18 is not pixel-identical to page 03'}
$afterHash=(Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if($afterHash-eq$beforeHash){throw 'PSD hash did not change after page-18 close repair'}
[ordered]@{status='PASS';beforeSha256=$beforeHash;backupSha256=$backupHash;afterSha256=$afterHash;backupMatchesBefore=($beforeHash-eq$backupHash);closeButtonBounds=@($reportData.closeButtonBounds);pixelIdenticalToPage03=[bool]$pixelResult.pixel_identical;exports=$exports}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath $validationPath
Write-Output 'Page-18 close-control repair PASS'
Write-Output "Before SHA256: $beforeHash"
Write-Output "After SHA256:  $afterHash"
Write-Output "Review folder: $exportDirectoryPath"
