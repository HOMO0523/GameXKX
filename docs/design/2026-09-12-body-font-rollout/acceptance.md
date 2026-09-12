# 正文字体切换验收

日期：2026-09-12 · 决策与实现见同目录 `README.md`、逐调用点角色见 `font-roles.tsv`（本地化总表第 14 页同步）。

## 1. 范围与口径

- Title：页面/面板主标题、卡名、装备名、意图标题、结算与弹窗标题、路线关名、对话角色名、印章展示字，以及**主界面大按钮**（底部五个导航盘、教程/任务两个导航盘、挑战/游历）。
- Body：按钮、页签、标签、正文、数字、提示、状态名、pill、副标题、费用、战报、结算正文、输入框。
- 字体：标题 `FF_Trial_ZhHans_JiangHuGuFeng_Font`（不变）；正文 `FF_Body_KeinannMaruPOP_Font`（2026-09-12 先试芝士奶盖乌龙宋，用户判定过细，当日换成荆南圆体）。

## 2. 证据

| 项 | 结果 | 证据 |
|---|---|---|
| 源包与授权 | SIL OFL 1.1；项目 1799 个语料字符**零缺字**；旧江湖体缺失的 `●◆▼▲✓★·×→≤≥＋／～％` 全部由新正文体自带 | `Saved/FontPreview/20260912-keinann-maru-pop/font-manifest.json` |
| 字体导入 | FontFace + Font 双资产、默认字面 `Default`、思源回退保留、旧的芝士奶盖乌龙宋资产已删除 | `Saved/HarnessReports/gamexxk-body-font-apply.json`、同目录 `ue-import-result.json`、`ue-validate-result.json`（10/10 通过） |
| 静态角色守卫 | 角色表 114 行齐全；0 处旧签名 `Font(Size,…)` 复活；路径常量与源文件哈希一致 | `python scripts/gamexxk_font_roles_check.py` → PASS |
| 冷 UBT | 多次 `Result: Succeeded`（含全部字体 API 迁移、字体更替、测试断言修正后各一次） | `python scripts/ai_production_loop.py --run-ubt --json` |
| 运行期字体审计 | 仓库/编队/天赋/工具四面：外源字体 **0**；标题 14/15/14/18 处、正文 35/48/31/44 处；主界面大按钮已按新口径全部归标题（教程36、任务36、挑战22、游历22、仓库/编队/天赋/工具/历练20），面板操作按钮（取出/存入20、升级24）仍为正文 | `Saved/Diagnostics/FontRollout/{hud,warehouse,formation,talents,tools,training-map}.json` |
| 真实 PIE 实拍 | 可见编辑器（`Launch_GameXXK_Editor.cmd`）真实 PIE 下 6 张 HUD 窗口像素截图，1536×816、1.0–1.5MB、7.8万–12万色，六面互不相同 | `Saved/Diagnostics/FontRollout/visible/{workbench,warehouse,formation,talents,tools,training}.png`（另附 `-on-white.png` 白底合成版） |
| 自动化回归 | 8 个桶 682 项：Localization 21/21、DesktopTraining 100/102、Equipment 108/109、Training 50/50、Integration-CardBattle 38/47、ToolsRedesign 14/14、Data 327/331、Development 7/8 | `Saved/Automation/FontRollout20260912/` |

## 3. 自动化结论：新增失败 0

7 个有改动前基线的桶（`Saved/Automation/20260911-proposalB/`）本轮失败集与基线**逐项同名同状态**：Equipment 1、Integration-CardBattle 9、Data 4、Development 1，其余为 0；合计 15 项既有失败，**新增失败 0**。

`GameXXK.DesktopTraining` 无历史桶基线，其 2 项失败（`Workbench.NoticeRailStateMachine`、`Workbench.ParentCloseStack`）已用 `git stash` 回到 HEAD 重新冷编译复跑确认**同样失败**（`Saved/Automation/FontRollout20260912/BASELINE-HEAD-Workbench/`）：前者是 09-10 通知栏改动后的既有失败，后者断言的 Tab 文本箭头在 09-10 已被用户要求改为透明水墨三角图标，属未同步的旧断言。

本次改动直接导致的 3 项字体断言失败已按角色修正：页签→Body、卡 tooltip 行→两种项目字体之一、详细属性 tooltip 逐行→Body；另主动修正 2 个不在本轮桶内的旧断言（卡 tooltip 受击方标签→Body、仓库堆叠数量→Body）。

## 4. 未完成／边界（不得当作已验收）

- **实拍的取景边界**：桌面 HUD 画在自己的顶层窗口 `GameXXKDesktopOverlay`（1536×816）里，`HighResShot`／`unreal.AutomationLibrary.take_high_res_screenshot` 对它只返回纯黑空图（1600×900、单一颜色、27184 字节，已删除）。因此实拍改用 Win32 `PrintWindow(PW_RENDERFULLCONTENT)` 直接抓该窗口：能拿到 HUD 与其半透明遮罩的真实像素，但**抓不到遮罩后面的游戏视口**，所以图偏暗。截图脚本 `Saved/Diagnostics/capture_overlay_window.ps1`。观感确认仍建议用可见编辑器的实时画面（本轮结束时该编辑器已开着并处于 PIE）。
- 中英 × 50%/75%/100% 的逐页排版复检（总表 05 页 168 项）仍停在换字体前的状态，正文换字体后需重跑：重点看 4 字窄按钮、卡面正文、长 tooltip 换行。
- 仅剩 1 处 `potentialOverflow`：编队/背包的 `NPC` 页签（20 号，字宽 52.0 > 可用 50.8，约 2%）。`编辑卡组`/`更换伙伴`/`更换NPC` 三处在旧宋体下的溢出已随新字体消失。若实拍可见，NPC 页签字号 20→19 即可。
