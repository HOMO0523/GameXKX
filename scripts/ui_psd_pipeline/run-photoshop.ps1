param(
    [string]$Root,
    [switch]$CheckOnly
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($Root)) {
    $projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
    $Root = Join-Path $projectRoot 'SourceArt\UI\PSD\town-v2'
}

$root = (Resolve-Path -LiteralPath $Root).Path
$jsx = Join-Path $root 'compose.jsx'
if (-not (Test-Path -LiteralPath $jsx)) {
    throw "Missing Photoshop composition script: $jsx"
}

if ($CheckOnly) {
    Write-Output "PSD package composition is ready: $jsx"
    return
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

$app.Visible = $true
$app.DoJavaScriptFile($jsx)
Write-Output 'Photoshop composition finished.'
