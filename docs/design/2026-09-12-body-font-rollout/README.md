# 正文字体切换：芝士奶盖乌龙宋（标题仍用江湖古风体）

2026-09-12 用户决定：**除标题以外的文本**换成芝士奶盖乌龙宋，标题继续用现有江湖古风体。

## 口径（已确认）

| 角色 | 字体 | 覆盖 |
|---|---|---|
| **Title** | 江湖古风体 `FF_Trial_ZhHans_JiangHuGuFeng_Font` | 页面/面板主标题、卡名、装备名、意图标题、结算与弹窗标题、路线关名、对话角色名、单个印章展示字；**以及主界面的大按钮文字：底部五个导航盘（仓库/编队/天赋/工具/历练）、教程与任务两个导航盘、挑战与游历主操作按钮**（2026-09-12 用户追加要求） |
| **Body** | 荆南圆体 `FF_Body_KeinannMaruPOP_Font` | 按钮、页签、标签、正文、数字、提示、状态名、pill、副标题、费用、战报、结算正文、输入框 |

> 2026-09-12 字体更替：正文首次试用芝士奶盖乌龙宋 Lite Bold，用户判定**过细**，当日换成**荆南圆体（けいなんポップ体 / KeinannMaruPOP）**。源包、授权与覆盖实测见 `Saved/FontPreview/20260912-keinann-maru-pop/`（OFL 1.1；项目 1799 个唯一字符**零缺字**；日文子集 `けいなん丸ポップ体JP.ttf` 缺 542 个中文字，未采用）。被替换的芝士奶盖乌龙宋资产已从 `/Game/GameXXK/UI/Fonts/Body` 删除。

## 为什么需要这张表

本地化总表（`docs/design/2026-09-10-ui-localization/`）此前**没有标题/正文的排版角色列**：`01_全部文本` 的"用途"只有 `full`/`compact`（本地化长短），`10_全角色卡牌短名` 只有卡名的字体宽度预检。角色清单因此在本目录 `font-roles.tsv` 建立，并已导出为总表第 14 页 `14_文本排版角色`（118 行 = 表头 + **114 个调用点**：Title 25、Body 73、Manual 16）。

`Manual` 表示该调用点在共享 helper 内，由 helper 的角色参数分派（例如工作台 `MakeText`（正文）/`MakeTitleText`（标题）、`GameXXKRouteMerchantWidget::MakeText`/`MakeTitleText`、`GameXXKEquipmentTooltipPresentation::Text`/`TextWidth`、`SGameXXKDevWorkbench::Text` 复用既有 `bDisplay` 语义）。

## 实现

- `FGameXXKInRunUiStyle` 用 `EGameXXKFontRole{Title,Body}` 取代被忽略的 `bDisplay` 参数：
  `Font(Role, Size, bBold)`、`TitleFont`/`BodyFont`、`OutlinedFont(Role, …)`、`OutlinedTitleFont`/`OutlinedBodyFont`、`FontPath(Role)`。
  旧签名被删除，编译器强制每个调用点表态，避免漏改。字体资产路径常量 `TitleFontPath`/`BodyFontPath` 同时供测试与审计使用。
- 桌面工作台的 `ApplySelectedRuntimeFont` 会把整棵树的字体无条件刷成江湖体，已改为 `ApplyMissingRuntimeFonts`：只补齐**没有**项目字体的 TextBlock（否则会退回 Roboto），不再覆盖已判定的角色字体。
- `RefreshGameTextMaterialAspects` 按 `/Game/GameXXK/UI/Fonts/` 前缀刷新 `TextAspect`，新字体放在同一目录下，切语言后的描边/流光宽度继续生效。
- 两处原先用引擎默认字体的正文（伙伴名册 `MakeText`、工作台堆叠数量）并入统一入口。
- `GameXXK.DesktopTraining.Workbench.SelectedRuntimeFont` 断言从"全部等于江湖体"改为"全部属于两种项目字体，且两种都出现"；`BattleUnitResource`、`CardOutcomePreview`、`EnemyPhaseBadge`、`CardBattleBoardEnemyIntent` 四个测试的字体断言改为 Body，卡名/装备名/卡组名测试改为 Title。

## 工具

| 用途 | 入口 |
|---|---|
| 源包与许可归档 | `Saved/FontPreview/20260912-keinann-maru-pop/`（`font-manifest.json` 记录 sha256、覆盖与授权依据）；被替换的 `Saved/FontPreview/20260912-cheese-foam-oolong-song/` 仅作历史留存 |
| 导入 + 校验（UE MCP） | `python scripts/gamexxk_body_font_apply.py [--no-launch]` → `Content/Python/gamexxk_import_body_font.py`、`gamexxk_validate_body_font.py` |
| 静态角色守卫（无 UE） | `python scripts/gamexxk_font_roles_check.py [--json]`：核对角色表、禁止旧签名调用复活、核对字体路径常量与源文件哈希 |
| 运行期排版审计 | `python -X utf8 -c` 驱动 `Content/Python/gamexxk_probe_font_rollout.py --phase audit`（逐控件字体直方图、外源字体、溢出）；`--phase measure` 对比两字体字宽 |
| 逐调用点迁移脚本 | `Saved/Diagnostics/refactor_font_roles.py`（一次性；按角色表重写旧调用，`Saved/Diagnostics/font-role-refactor-report.txt` 为逐行证据） |

## 验证

见本目录 `acceptance.md`。
