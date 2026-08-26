# review-visual.ps1 — PS 每次 apply 操作后，用 gpt-5.6-luna 对比改前/改后审核图
#
# 依赖：~/.claude/skills/codex-vision/scripts/codex_vision.ps1（codex-vision 技能 P0）
# 用法（在 PS 操作跑完、Before/After PNG 已导出后）：
#   powershell -File scripts/ui_psd_pipeline/review-visual.ps1 `
#     -Before "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/Before/02_城镇HUD.png" `
#     -After  "SourceArt/UI/PSD/gamexxk-v4/ui-master/Review/TownHUD/After/02_城镇HUD.png" `
#     -Feature town-hud -Intent "货币条改短、金币换元宝图标、删除底部提示框"
#
# 输出：outputs/UI_PSD/Candidates/GameXXK_UI_Master_V1.<feature>.visual-review.md
# 默认弹出一个可见窗口实时显示 codex 交流过程（-NoVisible 关闭）

param(
    [Parameter(Mandatory = $true)][string]$Before,
    [Parameter(Mandatory = $true)][string]$After,
    [string]$Intent = '',
    [string]$Feature = 'review',
    [string]$Out = '',
    [switch]$NoVisible
)

$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$skillScript = Join-Path $HOME '.claude\skills\codex-vision\scripts\codex_vision.ps1'
if (-not (Test-Path -LiteralPath $skillScript)) {
    throw "codex-vision 技能脚本不存在: $skillScript（请先安装 ~/.claude/skills/codex-vision，见 P0）"
}

$beforePath = (Resolve-Path -LiteralPath $Before).Path
$afterPath = (Resolve-Path -LiteralPath $After).Path
foreach ($f in @($beforePath, $afterPath)) {
    if (-not (Test-Path -LiteralPath $f -PathType Leaf)) { throw "审核图不存在: $f" }
}

if (-not $Out) {
    $Out = [IO.Path]::GetFullPath((Join-Path $projectRoot "outputs\UI_PSD\Candidates\GameXXK_UI_Master_V1.${Feature}.visual-review.md"))
}

$prompt = "你是游戏 UI 美术审核员。对比两张图片：图1是本次操作修改前的页面，图2是修改后的页面。"
if ($Intent) {
    $prompt += "本次操作预期只做：$Intent。请特别指出任何预期之外的改动。"
}
$prompt += "请分条列出图2相比图1的所有视觉差异（位置移动、尺寸变化、新增/删除元素、样式或贴图变化、错位/遮挡问题）。最后给一行结论：符合意图（是/否）＋风险提示。"

Write-Output "视觉审核: $Feature"
Write-Output "Before: $beforePath"
Write-Output "After:  $afterPath"
Write-Output "Intent: $(if ($Intent) { $Intent } else { '(未指定，全量对比)' })"
Write-Output ""

$imageList = $beforePath + ',' + $afterPath
$sessionFile = [IO.Path]::GetFullPath((Join-Path $projectRoot "outputs\UI_PSD\Candidates\GameXXK_UI_Master_V1.${Feature}.sessionid"))
if ($NoVisible) {
    & $skillScript -Prompt $prompt -Images $imageList -Effort 'medium' -MaxDim 1280 -Out $Out -Title "$Feature 视觉审核" -Workspace $projectRoot -SessionFile $sessionFile
} else {
    & $skillScript -Prompt $prompt -Images $imageList -Effort 'medium' -MaxDim 1280 -Out $Out -Title "$Feature 视觉审核" -Workspace $projectRoot -SessionFile $sessionFile -Visible
}

Write-Output ""
Write-Output "visual-review saved: $Out"
