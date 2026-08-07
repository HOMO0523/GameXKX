param(
    [string]$Destination = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeApproved',
    [string]$Manifest = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/final-approved-runtime-assets-manifest.json'
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$destinationPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $Destination))
$manifestPath = [IO.Path]::GetFullPath((Join-Path $projectRoot $Manifest))
if (Test-Path -LiteralPath $manifestPath) { throw "Approved runtime manifest already exists: $manifestPath" }
if (Test-Path -LiteralPath $destinationPath) {
    if (@(Get-ChildItem -LiteralPath $destinationPath -File -ErrorAction SilentlyContinue).Count -gt 0) {
        throw "Approved runtime destination is not empty: $destinationPath"
    }
} else {
    [IO.Directory]::CreateDirectory($destinationPath) | Out-Null
}

$assetMap = [ordered]@{
    T_MasterV2_MainMenuBackground = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/main_menu_tiger_hero_v9_loose_inkwash.png'
    T_MasterV2_MenuBrushNormal = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/components/menu_brush_normal_from_kit.png'
    T_MasterV2_MenuBrushPrimary = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/main-menu/components/menu_brush_primary_from_kit.png'
    T_MasterV2_IdentityPanel = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/identity_panel.png'
    T_MasterV2_CurrencyStripShort = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Generated/town-hud/components/currency_strip_320.png'
    T_MasterV2_HeroPortrait = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/hero_portrait.png'
    T_MasterV2_Ingot = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/resource_gold.png'
    T_MasterV2_NavDiscBackpack = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/nav_disc_backpack.png'
    T_MasterV2_NavDiscCompanion = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/nav_disc_companion.png'
    T_MasterV2_NavDiscCodex = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/nav_disc_codex.png'
    T_MasterV2_NavDiscTask = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/nav_disc_task.png'
    T_MasterV2_NavDiscRoute = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/nav_disc_route.png'
    T_MasterV2_NavBackpack = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/nav_backpack.png'
    T_MasterV2_NavCompanion = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/nav_companion.png'
    T_MasterV2_NavCodex = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/nav_codex.png'
    T_MasterV2_NavTask = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/nav_scroll.png'
    T_MasterV2_NavRoute = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/nav_route.png'
    T_MasterV2_PanelLarge = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ShellComponents/main_shop_panel.png'
    T_MasterV2_ItemSlot = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Components/inventory_slot_r1_c1.png'
    T_MasterV2_DetailSlot = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Components/detail_item_slot.png'
    T_MasterV2_EquipmentSlot = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Components/equipment_slot_left_01.png'
    T_MasterV2_SelectionInk = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Content/category_selected_ink.png'
    T_MasterV2_ButtonPurchase = 'SourceArt/UI/PSD/gamexxk-v4/calibration-v2/Components/tab_02_equipment_selected.png'
    T_MasterV2_TabNormal = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_tab_normal.png'
    T_MasterV2_TabSelected = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_tab_selected.png'
    T_MasterV2_ButtonNeutral = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_button_normal.png'
    T_MasterV2_ButtonPrimary = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_button_primary.png'
    T_MasterV2_ButtonDanger = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_button_danger.png'
    T_MasterV2_HeroFullBody = 'SourceArt/UI/PSD/gamexxk-v3/hero-backpack/Assets/hero_runtime_idle_frame_0000.png'
    T_MasterV2_BackpackScrollbarRight = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/LayoutAssets/03_主角背包_inventory_scrollbar_right.png'
    T_MasterV2_CloseInk = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/Assets/Controls/close_button_ink_v2.png'
    T_MasterV2_TooltipPaper = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/RuntimeAssets/UIV4_tooltip_panel.png'
    T_MasterV2_CardFrame = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/CardDeckKit/card_frame_base_PSD057.png'
    T_MasterV2_CardLockedIcon = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/CardDeckKit/card_state_locked_icon_full.png'
    T_MasterV2_CompanionPageLeft = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_left_Button.png'
    T_MasterV2_CompanionPageRight = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchControls/companion_page_right_Button.png'
    T_MasterV2_CompanionBlade = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_blade.png'
    T_MasterV2_CompanionBladeInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_blade_inactive.png'
    T_MasterV2_CompanionGuard = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_guard.png'
    T_MasterV2_CompanionGuardInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_guard_inactive.png'
    T_MasterV2_CompanionHealer = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_healer.png'
    T_MasterV2_CompanionHealerInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_healer_inactive.png'
    T_MasterV2_CompanionHunter = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_hunter.png'
    T_MasterV2_CompanionHunterInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_hunter_inactive.png'
    T_MasterV2_CompanionSorcerer = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_sorcerer.png'
    T_MasterV2_CompanionSorcererInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_sorcerer_inactive.png'
    T_MasterV2_CompanionFormationMaster = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_formation_master.png'
    T_MasterV2_CompanionFormationMasterInactive = 'SourceArt/UI/PSD/gamexxk-v4/ui-master/ManualEditing/PartnerSwitchPortraits/partner_portrait_formation_master_inactive.png'
}

Add-Type -AssemblyName System.Drawing
$records = @()
foreach ($entry in $assetMap.GetEnumerator()) {
    $sourcePath = [IO.Path]::GetFullPath((Join-Path $projectRoot $entry.Value))
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) { throw "Missing approved source: $sourcePath" }
    $fileName = "$($entry.Key).png"
    $targetPath = Join-Path $destinationPath $fileName
    if (Test-Path -LiteralPath $targetPath) { throw "Approved target already exists: $targetPath" }
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath
    $image = [Drawing.Bitmap]::FromFile($targetPath)
    try {
        $width = $image.Width
        $height = $image.Height
        $hasAlpha = [Drawing.Image]::IsAlphaPixelFormat($image.PixelFormat)
    } finally {
        $image.Dispose()
    }
    $records += [ordered]@{
        name = [string]$entry.Key
        file = "RuntimeApproved/$fileName"
        source = [string]$entry.Value
        width = $width
        height = $height
        hasAlpha = $hasAlpha
        sha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath $targetPath).Hash.ToLowerInvariant()
        ueDestination = "/Game/GameXXK/UI/MasterV2/Approved/$($entry.Key)"
        textBaked = $false
        status = 'approved_pending_import'
    }
}

$manifestData = [ordered]@{
    version = 1
    sourcePsd = 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd'
    sourcePsdSha256 = (Get-FileHash -Algorithm SHA256 -LiteralPath (Join-Path $projectRoot 'outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.psd')).Hash.ToLowerInvariant()
    approvedPages = @(
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
    ueDestinationRoot = '/Game/GameXXK/UI/MasterV2/Approved'
    textBaked = $false
    assetCount = $records.Count
    assets = $records
}
[IO.Directory]::CreateDirectory((Split-Path -Parent $manifestPath)) | Out-Null
$manifestData | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath $manifestPath -Encoding UTF8

Write-Output "Approved runtime assets prepared: $($records.Count)"
Write-Output "Destination: $destinationPath"
Write-Output "Manifest: $manifestPath"
