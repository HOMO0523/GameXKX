# 三箱品质概率表与宇宙获取路径（2026-09-11）

## 状态

用户已批准最终方案，代码 / 测试 / 设计总表 / 运行时契约 / 游戏内文本四处已同步。冷 UBT 通过，定向与横向回归完成。

> 本轮经过三次方案迭代，历史顺序为：①三箱重列（方案IV，宇宙20bp）→ ②九合一去空转（A4）→ ③**最终版：九合一提案B + 讨伐箱乙方案**。本文只描述最终生效的数值。

## 一、九合一品质表（提案B）

每档**只前进一档或保持原品质**，**没有跨档跳变**；成功率随档位单一平滑递减，失败保持原品质且同样消耗九件。

| 输入品质 | 结果分布 | 期望前进档数 |
|---|---|---:|
| 普通 | 100% 稀有 | 1.00 |
| 稀有 | 100% 珍稀 | 1.00 |
| 珍稀 | 100% 传奇 | 1.00 |
| 传奇 | 10% 保持传奇 / 90% 不朽 | 0.90 |
| 不朽 | 15% 保持不朽 / 85% 至宝 | 0.85 |
| 至宝 | 20% 保持至宝 / 80% 超凡 | 0.80 |
| 超凡 | 25% 保持超凡 / 75% 天界 | 0.75 |
| 天界 | 30% 保持天界 / 70% 登神 | 0.70 |
| 登神 | 40% 保持登神 / 60% 宇宙 | 0.60 |
| 宇宙 | 品质上限，不允许继续合成 | — |

修复的问题：旧表期望前进档数为 **1.00 → 1.503（+50%）→ 1.100（-27%）→ 0.500（-52%）**，三个断点且方向不一致。

## 二、三箱品质表（基点，每列精确合计 10000）

| 品质 | 普通箱 | 高级箱 | 讨伐箱（普通/困难） | 讨伐箱（地狱） |
|---|---:|---:|---:|---:|
| 普通 | 7000 | 5500 | 280 | 264 |
| 稀有 | 2500 | 2500 | 1500 | 1500 |
| 珍稀 | 400 | 1200 | 4800 | 4800 |
| 传奇 | 90 | 600 | 2400 | 2400 |
| 不朽 | 9 | 150 | 600 | 600 |
| 至宝 | 1 | 40 | 200 | 200 |
| 超凡 | 0 | 10 | 100 | 100 |
| 天界 | 0 | 0 | 80 | 80 |
| 登神 | 0 | 0 | 40 | 40 |
| 宇宙 | 0 | 0 | 0 | 16 |

- 珍稀及以上按严格 ×4 阶梯：500 → 2000 → 8000（讨伐箱 = 高级箱 ×4 = 普通箱 ×16）
- **讨伐箱是唯一直出天界/登神/宇宙的来源**：天界 80bp、登神 40bp 不分难度，宇宙 16bp 仅地狱
- 讨伐箱顶端逐级递减：至宝200 > 超凡100 > 天界80 > 登神40 > 宇宙16，**无倒挂**
- 普通箱因基点精度（设计值 0.001% 低于 1 基点）止于至宝；高级箱止于超凡
- 出令率与类别分布不变（普通 2% / 高级 8%；50% 装备 / 30% 宝石 / 20% 材料）

修复的问题：旧讨伐箱在 超凡(40) 与 宇宙(20) 之间**天界/登神两行为 0**，阶梯断档。

## 三、节奏实测（挂机口径，40,000 小时 ×7 seed）

| 路径 | 单件宇宙 | 全队 18 件 |
|---|---:|---:|
| **地狱讨伐箱** | **37.3 天** | **1.84 年** ✅ 落在批准的 1–2 年 |
| 非地狱讨伐箱（无宇宙直出） | 228.8 天 | 11.28 年 |

其中地狱路径的直出贡献约 84%（40,000 小时内平均直出 37.4 件），合成分担其余部分。**因此地狱讨伐是宇宙的核心来源**，普通/困难路线基本只能靠合成，慢 6 倍。

历史对照：最初方案在 1.14 年内宇宙产出为 0 件；方案IV+20bp 曾达 1.62 年。

## 四、实现清单

| 文件 | 改动 |
|---|---|
| `Source/GameXXK/Public/GameXXKTrainingChestRules.h` | `LootRollDomain = 10000`；`ResolveLootQuality` 增加来源难度参数 |
| `Source/GameXXK/Private/GameXXKTrainingChestRules.cpp` | 十档四列品质表（含天界/登神/宇宙）；普通箱也掷真实随机品质（原固定 `0`）；出令判定用常量 |
| `Source/GameXXK/Private/GameXXKToolCombineProbability.cpp` | 九合一改为提案B（无跳变，成功率单调递减） |
| `Source/GameXXK/Private/Tests/GameXXKHuntExpansionTest.cpp` | `ExactQualityWeights` 改为四列 × 十档精确断言（含难度维度） |
| `Source/GameXXK/Private/Tests/GameXXKToolInteractionRedesignTest.cpp` | 九合一逐千分位期望矩阵改提案B；预览文案断言相应调整 |
| `Source/GameXXK/Private/Tests/GameXXKTrainingChestRulesTest.cpp` | 档域断言上限扩到宇宙 |
| `Content/Localization/GameXXK/strings.json` | `Chest.Hunt.Odds` 中英更新为新概率（含地狱直出宇宙） |
| `docs/design/2026-09-10-hunt-stage-design/…设计总表.xlsx` | `02_宝箱品质概率` 重写为七列十行；`04_九合一品质表` 改为提案B |
| `docs/design/2026-09-10-hunt-stage-design/runtime-contract.md` | 品质段与九合一段更新 |
| `docs/design/2026-09-10-ui-localization/…本地化总表.xlsx` | 随 strings.json 重生成 |
| `docs/production/2026-09-10-ui-guidance-localization-state.json` | 里程碑 `G5P` 更新 |
| `docs/design/2026-09-10-project-progress/…总表.xlsx` | 随 state.json 重生成 |
| `docs/superpowers/specs/2026-08-28-generic-dialogue-tutorial-shop-design.md` | §13/§14 标注"已被取代" |

## 五、验证

- 三箱四列各自合计：10000 × 4；顶端单调不倒挂
- 九合一每档合计 1000‰；期望前进档数为 1.00/1.00/1.00/0.90/0.85/0.80/0.75/0.70/0.60
- 冷 UBT：通过（首次因**交互编辑器占用 `UnrealEditor-GameXXK.dll`** 导致 LNK1104 失败，经 MCP 只读确认 0 脏包、编辑器关闭后通过）
- 定向回归：`GameXXK.Hunt` **10/10**、`GameXXK.Training` **50/50**、`GameXXK.ToolsRedesign` **14/14**、`GameXXK.Localization` **21/21**
- 本地化目录脚本测试 `scripts/test_gamexxk_localization_catalog.py`：**7/7**
- 横向回归（10 桶 609 项）：**591 通过 / 18 失败**，失败集与改动前**完全一致**（18 项既有红项）

## 六、未同步项与既有缺口

- `GameXXK_装备设计总表` **未改动**：其生成器 `scripts/update_equipment_master_workbook.py` 存在与本次无关的既有缺陷（`--check` 报 `KeyError: 'ZhuiFengLateCardDamage'`），无法安全重生成。
- `GameXXKDesktopTrainingWorkbenchTools.cpp` 有 18 处硬编码中文提示，`FGameXXKToolCombineProbability::Describe` 内的"（原品质）"为裸字面量（品质名本身走 `NSLOCTEXT`）。英文环境仍显示中文，属独立本地化任务。
- 本次改动触及玩家可见的**合成预览数值与宝箱 tooltip 文本**，冷 UBT 与自动化均无法覆盖观感，**建议再做一次纯 2D 实机悬停核对**。
