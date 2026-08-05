param(
    [Parameter(Mandatory = $true)]
    [string]$Psd,

    [Parameter(Mandatory = $true)]
    [string]$ComponentsManifest,

    [Parameter(Mandatory = $true)]
    [string]$MasterManifest,

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
$componentsPath = Resolve-ExistingFile $ComponentsManifest 'component manifest'
$masterPath = Resolve-ExistingFile $MasterManifest 'master manifest'
$receiptPath = if ([IO.Path]::IsPathRooted($Receipt)) {
    [IO.Path]::GetFullPath($Receipt)
} else {
    [IO.Path]::GetFullPath((Join-Path (Get-Location) $Receipt))
}
$psdDirectory = Split-Path -Parent $psdPath
$psdBaseName = [IO.Path]::GetFileNameWithoutExtension($psdPath)
$backups = @(Get-ChildItem -LiteralPath $psdDirectory -File -Filter "$psdBaseName.before-shell-split.*.psd")
if ($backups.Count -lt 1) {
    throw "No before-shell-split backup exists beside the target PSD: $psdPath"
}

$node = Get-Command node -ErrorAction Stop
$builder = Join-Path $PSScriptRoot 'build-shell-split-jsx.js'
$builderPath = Resolve-ExistingFile $builder 'JSX builder'
$jsxDirectory = Join-Path $projectRoot 'tmp\meta-shop-shell-split'
[IO.Directory]::CreateDirectory($jsxDirectory) | Out-Null
$jsxPath = Join-Path $jsxDirectory 'apply-shell-split.jsx'
[IO.Directory]::CreateDirectory((Split-Path -Parent $receiptPath)) | Out-Null

& $node.Source $builderPath `
    --psd $psdPath `
    --components $componentsPath `
    --master-manifest $masterPath `
    --output $jsxPath `
    --receipt $receiptPath
if ($LASTEXITCODE -ne 0) {
    throw "Shell-split JSX generation failed with exit code $LASTEXITCODE"
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
        throw "Target PSD has unsaved Photoshop changes. Save it before shell injection: $psdPath"
    }
}

if (Test-Path -LiteralPath $receiptPath) {
    throw "Shell-split receipt already exists; refusing a duplicate run: $receiptPath"
}
$app.Visible = $true
$app.DoJavaScriptFile($jsxPath)

if (-not (Test-Path -LiteralPath $receiptPath -PathType Leaf)) {
    throw "Photoshop did not write the shell-split receipt: $receiptPath"
}
$receiptData = Get-Content -Raw -LiteralPath $receiptPath | ConvertFrom-Json
if ($receiptData.page -ne '07_商店交易') {
    throw "Unexpected receipt page: $($receiptData.page)"
}
if ($receiptData.group -ne '00_ShellComponents') {
    throw "Unexpected receipt group: $($receiptData.group)"
}
if ([int]$receiptData.importedLayerCount -ne 9) {
    throw "Expected nine imported layers, got $($receiptData.importedLayerCount)"
}
if ($receiptData.originalLayerHidden -ne $true) {
    throw 'Original composite shell was not hidden.'
}
if (@($receiptData.componentLayers).Count -ne 8) {
    throw "Expected eight component layer names, got $(@($receiptData.componentLayers).Count)"
}

Write-Output 'PSD shell split finished.'
