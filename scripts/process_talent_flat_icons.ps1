param(
    [Parameter(Mandatory = $true)]
    [string]$InputPath,
    [string]$OutputDirectory = ""
)

$ErrorActionPreference = "Stop"
Add-Type -AssemblyName System.Drawing

$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
if ([string]::IsNullOrWhiteSpace($OutputDirectory)) {
    $OutputDirectory = Join-Path $projectRoot "SourceArt\UI\Talents\flat"
}
$resolvedInput = (Resolve-Path -LiteralPath $InputPath).Path
[System.IO.Directory]::CreateDirectory($OutputDirectory) | Out-Null

$names = @(
    "Attack", "Health", "Defense", "Critical",
    "Movement", "Backpack", "Gold", "Experience",
    "Offline", "Time", "Chest"
)
$source = [System.Drawing.Bitmap]::FromFile($resolvedInput)
$manifestIcons = @()
try {
    for ($index = 0; $index -lt $names.Count; $index++) {
        $column = $index % 4
        $row = [Math]::Floor($index / 4)
        $cellLeft = [Math]::Floor($column * $source.Width / 4.0)
        $cellRight = [Math]::Floor(($column + 1) * $source.Width / 4.0)
        $cellTop = [Math]::Floor($row * $source.Height / 3.0)
        $cellBottom = [Math]::Floor(($row + 1) * $source.Height / 3.0)

        $minX = $cellRight
        $minY = $cellBottom
        $maxX = -1
        $maxY = -1
        for ($y = $cellTop; $y -lt $cellBottom; $y++) {
            for ($x = $cellLeft; $x -lt $cellRight; $x++) {
                if ($source.GetPixel($x, $y).A -gt 8) {
                    if ($x -lt $minX) { $minX = $x }
                    if ($x -gt $maxX) { $maxX = $x }
                    if ($y -lt $minY) { $minY = $y }
                    if ($y -gt $maxY) { $maxY = $y }
                }
            }
        }
        if ($maxX -lt $minX -or $maxY -lt $minY) {
            throw "No non-transparent pixels found for $($names[$index])."
        }

        $sourceWidth = $maxX - $minX + 1
        $sourceHeight = $maxY - $minY + 1
        $targetExtent = 369.0
        $scale = [Math]::Min($targetExtent / $sourceWidth, $targetExtent / $sourceHeight)
        $targetWidth = [Math]::Max(1, [Math]::Round($sourceWidth * $scale))
        $targetHeight = [Math]::Max(1, [Math]::Round($sourceHeight * $scale))
        $targetX = [Math]::Floor((512 - $targetWidth) / 2.0)
        $targetY = [Math]::Floor((512 - $targetHeight) / 2.0)

        $output = New-Object System.Drawing.Bitmap 512, 512, ([System.Drawing.Imaging.PixelFormat]::Format32bppArgb)
        try {
            $graphics = [System.Drawing.Graphics]::FromImage($output)
            try {
                $graphics.Clear([System.Drawing.Color]::Transparent)
                $graphics.CompositingMode = [System.Drawing.Drawing2D.CompositingMode]::SourceCopy
                $graphics.CompositingQuality = [System.Drawing.Drawing2D.CompositingQuality]::HighQuality
                $graphics.InterpolationMode = [System.Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode = [System.Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $sourceRect = New-Object System.Drawing.Rectangle $minX, $minY, $sourceWidth, $sourceHeight
                $targetRect = New-Object System.Drawing.Rectangle $targetX, $targetY, $targetWidth, $targetHeight
                $graphics.DrawImage($source, $targetRect, $sourceRect, [System.Drawing.GraphicsUnit]::Pixel)
            }
            finally {
                $graphics.Dispose()
            }
            $fileName = "T_TalentFlat_$($names[$index]).png"
            $outputPath = Join-Path $OutputDirectory $fileName
            $output.Save($outputPath, [System.Drawing.Imaging.ImageFormat]::Png)
            $manifestIcons += [ordered]@{
                name = $names[$index]
                file = $fileName
                source_cell = @($column, $row)
                source_bounds = @($minX, $minY, $maxX, $maxY)
                output_bounds = @(
                    [int]$targetX,
                    [int]$targetY,
                    [int]($targetX + $targetWidth - 1),
                    [int]($targetY + $targetHeight - 1)
                )
            }
        }
        finally {
            $output.Dispose()
        }
    }
}
finally {
    $source.Dispose()
}

$sheetDestination = Join-Path $OutputDirectory "T_TalentFlat_SourceSheet_v1.png"
Copy-Item -LiteralPath $resolvedInput -Destination $sheetDestination -Force

$manifest = [ordered]@{
    version = 1
    canvas = @(512, 512)
    source_sheet = "T_TalentFlat_SourceSheet_v1.png"
    icons = $manifestIcons
}
$manifest | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory "manifest.json") -Encoding UTF8
Write-Output ($manifest | ConvertTo-Json -Depth 8 -Compress)
