param(
    [Parameter(Mandatory = $true)]
    [string]$Psd,

    [Parameter(Mandatory = $true)]
    [string]$Manifest,

    [Parameter(Mandatory = $true)]
    [string]$MasterManifest,

    [Parameter(Mandatory = $true)]
    [string]$SourceReceipt,

    [Parameter(Mandatory = $true)]
    [string]$Receipt
)

$ErrorActionPreference = 'Stop'

function Resolve-ExistingFile([string]$Value, [string]$Label) {
    if (-not (Test-Path -LiteralPath $Value -PathType Leaf)) {
        throw "Missing ${Label}: $Value"
    }
    return (Resolve-Path -LiteralPath $Value).Path
}

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$psdPath = Resolve-ExistingFile $Psd 'PSD'
$manifestPath = Resolve-ExistingFile $Manifest 'family correction manifest'
$masterPath = Resolve-ExistingFile $MasterManifest 'master manifest'
$sourceReceiptPath = Resolve-ExistingFile $SourceReceipt 'source receipt'
$receiptPath = if ([IO.Path]::IsPathRooted($Receipt)) {
    [IO.Path]::GetFullPath($Receipt)
} else {
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $Receipt))
}
if (Test-Path -LiteralPath $receiptPath) {
    throw "Family correction receipt already exists: $receiptPath"
}

$sourceData = Get-Content -Raw -Encoding UTF8 -LiteralPath $sourceReceiptPath | ConvertFrom-Json
$currentHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $psdPath).Hash.ToLowerInvariant()
if ($currentHash -ne ([string]$sourceData.sourceSha256).ToLowerInvariant()) {
    throw "PSD hash differs from the family correction source lock: $currentHash"
}
$backupPath = Resolve-ExistingFile ([string]$sourceData.backupPsd) 'family correction backup'
$backupHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $backupPath).Hash.ToLowerInvariant()
if ($backupHash -ne ([string]$sourceData.backupSha256).ToLowerInvariant()) {
    throw "Family correction backup hash mismatch: $backupPath"
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

$node = Get-Command node -ErrorAction Stop
$builderPath = Resolve-ExistingFile (Join-Path $PSScriptRoot 'build-family-corrections-jsx.js') 'family correction JSX builder'
$jsxDirectory = Join-Path $projectRoot 'tmp\family-corrections'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-family-corrections.jsx'
[IO.Directory]::CreateDirectory((Split-Path -Parent $receiptPath)) | Out-Null

& $node.Source $builderPath `
    --psd $psdPath `
    --manifest $manifestPath `
    --master-manifest $masterPath `
    --source-receipt $sourceReceiptPath `
    --output $jsxPath `
    --receipt $receiptPath
if ($LASTEXITCODE -ne 0) {
    throw "Family correction JSX generation failed with exit code $LASTEXITCODE"
}

$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
    throw "Photoshop did not write the family correction receipt: $receiptPath"
}
$receiptData = Get-Content -Raw -Encoding UTF8 -LiteralPath $receiptPath | ConvertFrom-Json
if ([int]$receiptData.pageCount -ne 16) {
    throw "Expected sixteen corrected pages, got $($receiptData.pageCount)"
}
if ([int]$receiptData.familyCount -ne 4) {
    throw "Expected four corrected families, got $($receiptData.familyCount)"
}
if (@($receiptData.pages).Count -ne 16) {
    throw "Expected sixteen page receipts, got $(@($receiptData.pages).Count)"
}
foreach ($page in @($receiptData.pages)) {
    if ($page.group -ne '00_FamilyCorrection') {
        throw "Unexpected correction group for $($page.page): $($page.group)"
    }
    if ([int]$page.importedLayerCount -lt 1) {
        throw "No imported layers reported for $($page.page)"
    }
}

Write-Output 'PSD family corrections finished.'
