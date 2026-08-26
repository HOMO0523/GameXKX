param(
    [string]$Psd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$Report = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.backpack-tab-scrollbar-export.report.json',
    [string]$ExportDirectory = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/ManualEditing/BackpackTabScrollbarExport'
)

$ErrorActionPreference = 'Stop'
function Resolve-InputFile([string]$Value,[string]$Label){if(-not(Test-Path -LiteralPath $Value -PathType Leaf)){throw "Missing ${Label}: $Value"};(Resolve-Path -LiteralPath $Value).Path}
function Resolve-Destination([string]$Value){if([IO.Path]::IsPathRooted($Value)){[IO.Path]::GetFullPath($Value)}else{[IO.Path]::GetFullPath((Join-Path (Get-Location) $Value))}}
function Get-PngSize([string]$Value){$image=[Drawing.Image]::FromFile($Value);try{@($image.Width,$image.Height)}finally{$image.Dispose()}}

$projectRoot=Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath=Resolve-InputFile $Psd 'master PSD'
$builderPath=Resolve-InputFile (Join-Path $PSScriptRoot 'build-backpack-tab-scrollbar-export-jsx.js') 'backpack tab/scrollbar export builder'
$reportPath=Resolve-Destination $Report
$exportDirectoryPath=Resolve-Destination $ExportDirectory
[IO.Directory]::CreateDirectory((Split-Path -Parent $reportPath))|Out-Null
if(-not(Test-Path -LiteralPath $exportDirectoryPath)){[IO.Directory]::CreateDirectory($exportDirectoryPath)|Out-Null}

$jsxDirectory=Join-Path $projectRoot 'tmp/ui_psd_pipeline';[IO.Directory]::CreateDirectory($jsxDirectory)|Out-Null
$jsxPath=Join-Path $jsxDirectory 'apply-backpack-tab-scrollbar-export.jsx'
if(Test-Path -LiteralPath $jsxPath){Remove-Item -LiteralPath $jsxPath}
$node=Get-Command node -ErrorAction Stop
& $node.Source $builderPath --psd $psdPath --output $jsxPath --report $reportPath --export-dir $exportDirectoryPath
if($LASTEXITCODE-ne 0){throw "Backpack export JSX generation failed: $LASTEXITCODE"}

$app=New-Object -ComObject Photoshop.Application
$targetComparison=[IO.Path]::GetFullPath($psdPath)
for($index=1;$index-le$app.Documents.Count;$index++){$document=$app.Documents.Item($index);try{$documentPath=[IO.Path]::GetFullPath([string]$document.FullName)}catch{continue};if($documentPath.Equals($targetComparison,[StringComparison]::OrdinalIgnoreCase)-and -not$document.Saved){throw "Target PSD has unsaved Photoshop changes: $psdPath"}}
$app.Visible=$true
$app.DoJavaScriptFile($jsxPath)

if(-not(Test-Path -LiteralPath $reportPath -PathType Leaf)){$errorReceipt="$reportPath.error.txt";if(Test-Path -LiteralPath $errorReceipt){throw "Photoshop backpack export failed: $(Get-Content -Raw -Encoding UTF8 $errorReceipt)"};throw "Photoshop did not write backpack export report: $reportPath"}
$reportData=[IO.File]::ReadAllText($reportPath,[Text.Encoding]::UTF8)|ConvertFrom-Json
if($reportData.status-ne'PASS'){throw 'Unexpected backpack export report'}

Add-Type -AssemblyName System.Drawing
$expected=@{'T_MasterV2_BackpackScrollbarThumb'=@(30,126)}
$exports=@()
foreach($record in @($reportData.exports)){$exportPath=[string]$record.path;if(-not(Test-Path -LiteralPath $exportPath -PathType Leaf)){throw "Missing backpack export: $exportPath"};$size=Get-PngSize $exportPath;$want=$expected[[string]$record.name];if($want-and($size[0]-ne$want[0]-or$size[1]-ne$want[1])){throw "Unexpected backpack export dimensions for $($record.name): $size (want $want)"};$exports+=[ordered]@{name=[string]$record.name;path=$exportPath;width=$size[0];height=$size[1];sha256=(Get-FileHash -Algorithm SHA256 -LiteralPath $exportPath).Hash.ToLowerInvariant()}}

[ordered]@{status='PASS';exports=$exports}|ConvertTo-Json -Depth 8|Set-Content -Encoding UTF8 -LiteralPath $reportPath
Write-Output 'Backpack tab/scrollbar export PASS'
foreach($export in $exports){Write-Output "  $($export.name): $($export.width)x$($export.height) -> $($export.path)"}
