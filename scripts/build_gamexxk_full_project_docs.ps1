param()

$ErrorActionPreference = 'Stop'

$projectRoot = [System.IO.Path]::GetFullPath((Join-Path $PSScriptRoot '..'))
$designRoot = Join-Path $projectRoot 'docs\design'
$topicRoot = Join-Path $designRoot '2026-08-11-gamexxk-project-plan'
$utf8NoBom = [System.Text.UTF8Encoding]::new($false)

function Read-ProjectText([string]$Path) {
    return [System.IO.File]::ReadAllText($Path).Replace("`r`n", "`n").Trim()
}

function Convert-EmbeddedLinks([string]$Text) {
    return $Text.Replace('](../', '](')
}

$header = @'
# GameXXK 全项目规划全集（A：单文件全集阅读版）

日期：2026-08-11

本文件由 B 版总纲、十个专题附录和“当前已验证代码快照”确定性汇编。离线只读本文件即可区分：批准目标终稿、当前代码实际内容、实现/测试状态与尚未裁决项。

阅读裁决：用户最新明确确认 > 2026-08-08/10/11 专项规格 > 当前代码与自动化（只用于标实现状态）> 更早文档。目标终稿、当前快照和状态证据不得混写；被取代规则只在变更记录出现。
'@

$sections = @(
    @{ Label = 'B 版总纲'; Path = (Join-Path $designRoot '2026-08-11-gamexxk-project-master-plan.md') },
    @{ Label = '专题一'; Path = (Join-Path $topicRoot '01-core-loop-and-meta-flow.md') },
    @{ Label = '专题二'; Path = (Join-Path $topicRoot '02-combat-rules-and-status-matrix.md') },
    @{ Label = '专题三'; Path = (Join-Path $topicRoot '03-rosters-deckbuilding-and-archetypes.md') },
    @{ Label = '专题四'; Path = (Join-Path $topicRoot '04-card-catalog-and-numeric-index.md') },
    @{ Label = '专题五'; Path = (Join-Path $topicRoot '05-equipment-sets-and-economy.md') },
    @{ Label = '专题六'; Path = (Join-Path $topicRoot '06-enemies-route-and-balance.md') },
    @{ Label = '专题七'; Path = (Join-Path $topicRoot '07-ui-and-new-page-boundaries.md') },
    @{ Label = '专题八'; Path = (Join-Path $topicRoot '08-art-and-terrain-backgrounds.md') },
    @{ Label = '专题九'; Path = (Join-Path $topicRoot '09-save-and-compatibility.md') },
    @{ Label = '专题十'; Path = (Join-Path $topicRoot '10-implementation-testing-and-change-log.md') },
    @{ Label = '当前已验证代码快照（198 张）'; Path = (Join-Path $designRoot '2026-08-11-full-card-catalog.md') }
)

$parts = [System.Collections.Generic.List[string]]::new()
$parts.Add($header.Trim())
foreach ($section in $sections) {
    $content = Convert-EmbeddedLinks (Read-ProjectText $section.Path)
    $parts.Add("# 汇编部分：$($section.Label)`n`n$content")
}

$output = ($parts -join "`n`n---`n`n") + "`n"
$markdownPath = Join-Path $designRoot '2026-08-11-gamexxk-full-project-plan-all-in-one.md'
$textPath = Join-Path $designRoot '2026-08-11-gamexxk-full-project-plan-all-in-one.txt'
[System.IO.File]::WriteAllText($markdownPath, $output, $utf8NoBom)
[System.IO.File]::WriteAllText($textPath, $output, $utf8NoBom)

Write-Output "Generated: $markdownPath"
Write-Output "Generated: $textPath"
