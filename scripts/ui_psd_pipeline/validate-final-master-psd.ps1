param(
    [string]$PsdPath = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd',
    [string]$ReportPath = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.final-ui-baseline.validation.json'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$resolvedPsdPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $PsdPath))
$resolvedReportPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $ReportPath))

if (-not (Test-Path -LiteralPath $resolvedPsdPath -PathType Leaf)) {
    throw "Master PSD does not exist: $resolvedPsdPath"
}

$requiredPages = @(
    '00_公共组件',
    '01_主菜单',
    '02_城镇HUD',
    '03_主角背包',
    '06_任务日志',
    '07_商店交易',
    '10_战斗HUD',
    '11_战斗奖励结算',
    '12_系统菜单',
    '13_主角背包_物品选中',
    '17_战斗HUD_卡牌选中目标',
    '18_主角背包_卡组页'
)

$requiredExternalAssets = @(
    'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/CardDeckKit/card_frame_base_PSD057.png',
    'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/CardDeckKit/card_state_locked_icon_full.png',
    'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_left_Button.png',
    'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_right_Button.png',
    'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_switch_portraits_manifest.json'
)

function Get-LayerRecords {
    param(
        [Parameter(Mandatory = $true)]$LayerSet,
        [Parameter(Mandatory = $true)][string]$Prefix
    )

    $records = @()
    foreach ($layer in @($LayerSet.ArtLayers)) {
        $records += [ordered]@{
            path = "$Prefix/$($layer.Name)"
            name = [string]$layer.Name
            kind = 'art'
            visible = [bool]$layer.Visible
        }
    }
    foreach ($child in @($LayerSet.LayerSets)) {
        $childPath = "$Prefix/$($child.Name)"
        $records += [ordered]@{
            path = $childPath
            name = [string]$child.Name
            kind = 'group'
            visible = [bool]$child.Visible
        }
        $records += Get-LayerRecords -LayerSet $child -Prefix $childPath
    }
    return $records
}

$photoshop = New-Object -ComObject Photoshop.Application
$document = $null
$openedForValidation = $false
foreach ($candidate in @($photoshop.Documents)) {
    try {
        if ([IO.Path]::GetFullPath([string]$candidate.FullName) -eq $resolvedPsdPath) {
            $document = $candidate
            break
        }
    } catch {
        # A new unsaved Photoshop document has no FullName; it is unrelated to this validation.
    }
}
if (-not $document) {
    $document = $photoshop.Open($resolvedPsdPath)
    $openedForValidation = $true
}

try {
    $topLevelNames = @($document.LayerSets | ForEach-Object { [string]$_.Name })
    $missingPages = @($requiredPages | Where-Object { $_ -notin $topLevelNames })
    $pageRecords = @()
    foreach ($pageName in $requiredPages) {
        $page = @($document.LayerSets) | Where-Object { $_.Name -eq $pageName } | Select-Object -First 1
        if ($page) {
            $pageRecords += Get-LayerRecords -LayerSet $page -Prefix $pageName
        }
    }

    $page03 = @($pageRecords | Where-Object { $_.path -like '03_主角背包/*' })
    $page13 = @($pageRecords | Where-Object { $_.path -like '13_主角背包_物品选中/*' })
    $page18 = @($pageRecords | Where-Object { $_.path -like '18_主角背包_卡组页/*' })

    $checks = @(
        [ordered]@{ name = 'page03_has_20_inventory_slots'; passed = @($page03 | Where-Object { $_.name -match '^\d+_inventory_slot_[1-5]_[1-4]$' }).Count -eq 20 },
        [ordered]@{ name = 'page03_has_6_equipment_frames'; passed = @($page03 | Where-Object { $_.name -match '^\d+_equipment_frame_[1-6]$' }).Count -eq 6 },
        [ordered]@{ name = 'page03_has_new_scrollbar_button'; passed = @($page03 | Where-Object { $_.name -eq 'inventory_scrollbar_Button' }).Count -eq 1 },
        [ordered]@{ name = 'page03_has_decompose_action'; passed = @($page03 | Where-Object { $_.path -eq '03_主角背包/43_InventoryActions/01_DecomposeButton' }).Count -eq 1 },
        [ordered]@{ name = 'page13_has_tooltip_paper'; passed = @($page13 | Where-Object { $_.name -eq '01_TooltipPaper_CurrentParchment' }).Count -eq 1 },
        [ordered]@{ name = 'page13_has_shared_selection_ink'; passed = @($page13 | Where-Object { $_.name -eq '01_SharedShopSelectionInk' -and $_.visible }).Count -eq 1 },
        [ordered]@{ name = 'page18_has_close_button'; passed = @($page18 | Where-Object { $_.path -eq '18_主角背包_卡组页/02_WindowControls/01_CloseButton' }).Count -eq 1 },
        [ordered]@{ name = 'page18_has_9_card_frames'; passed = @($page18 | Where-Object { $_.name -like 'card_frame_base_PSD057*' }).Count -eq 9 },
        [ordered]@{ name = 'page18_has_lock_art'; passed = @($page18 | Where-Object { $_.name -eq 'card_state_locked_icon_full' }).Count -eq 1 },
        [ordered]@{ name = 'page18_has_partner_page_arrows'; passed = @($page18 | Where-Object { $_.name -in @('companion_page_left_Button', 'companion_page_right_Button') }).Count -eq 2 }
    )

    $externalRecords = @()
    foreach ($relativePath in $requiredExternalAssets) {
        $absolutePath = [IO.Path]::GetFullPath((Join-Path $projectRoot $relativePath))
        $exists = Test-Path -LiteralPath $absolutePath -PathType Leaf
        $externalRecords += [ordered]@{
            path = $relativePath
            exists = $exists
            sha256 = if ($exists) { (Get-FileHash -Algorithm SHA256 -LiteralPath $absolutePath).Hash.ToLowerInvariant() } else { $null }
        }
    }

    $passed = $missingPages.Count -eq 0
    $passed = $passed -and @($checks | Where-Object { -not $_.passed }).Count -eq 0
    $passed = $passed -and @($externalRecords | Where-Object { -not $_.exists }).Count -eq 0
    $report = [ordered]@{
        status = if ($passed) { 'PASS' } else { 'FAIL' }
        generatedAt = [DateTimeOffset]::Now.ToString('o')
        psd = [ordered]@{
            path = $PsdPath
            sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $resolvedPsdPath).Hash.ToLowerInvariant()
            width = [int]$document.Width
            height = [int]$document.Height
            topLevelPages = $topLevelNames
            missingRequiredPages = $missingPages
        }
        checks = $checks
        externalAssets = $externalRecords
    }

    [IO.Directory]::CreateDirectory((Split-Path -Parent $resolvedReportPath)) | Out-Null
    $report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $resolvedReportPath -Encoding UTF8
    if (-not $passed) {
        throw "Final Master PSD validation failed. Report: $resolvedReportPath"
    }
    Write-Output "Final Master PSD validation: PASS"
    Write-Output "PSD SHA256: $($report.psd.sha256)"
    Write-Output "Report: $resolvedReportPath"
} finally {
    if ($openedForValidation -and $document) {
        $document.Close(2) # psDoNotSaveChanges
    }
}
