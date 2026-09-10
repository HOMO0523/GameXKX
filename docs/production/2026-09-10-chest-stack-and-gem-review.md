# 宝箱堆叠与宝石文案修正

用户反馈右键开宝箱被阻止，但背包尚有空位；同时要求宝石标题去品质、品质独立一行，中英文描述均精简。追加反馈：仓库行旅钱数量缺失，仓库 `x20` 样式需统一为背包的数字描边样式。

## 已取证

- 只读现场：`Saved/StorySystem/ChestBlock/live-snapshot.json`。18/20 背包格占用、22 个仓库格占用、2 只普通宝箱，存档错误为空。
- 原始快照中的两只剩余宝箱在旧版本临时会话内可以开启，产物为装备及行旅钱。因此不能将当前两只宝箱直接归因于仓库分区缺陷。
- 以相同布局、仅在临时副本切换掉落种子为 3：洗练砂已在仓库时整批开启失败；去掉测试副本的仓库物品后，两只宝箱均开启成功，得到装备与 1 份洗练砂。失败前后运行时状态完全相等。
- 原因：开箱直接新增 `State.Inventory`，与已有 `WarehouseItems` 同 ID 构成禁止的双分区；`Normalize` 的分区失败又被统一误报为 `BackpackFull`。
- 证据：`Saved/StorySystem/ChestBlock/reproduction-before-fix.json`。临时会话恢复后真实状态精确相等，所有 `.sav` 文件哈希一致。
- 最新编译前正常存档：522770 金币、等级 12、HP 265；备份 `Saved/StorySystem/GateSaveFix/PlayerSaveBackup-20260910-190149`。PIE 已停止，脏包数为 0。

## 实现范围

- `TrainingChestRules`：非行旅钱的已有仓库堆叠原位增量，保留物理格和锁定；新条目仍进入背包。行旅钱维持可分置两容器的现行规则。容量与溢出失败保留宝箱、随机开启序号、原库存。
- 库存整理失败使用独立错误，不再冒充背包满。未放宽全局库存分区校验，也未改变掉落概率或玩家种子。
- 宝石标题按 17 种类型使用短名称；10 档品质在描述首行显示。保留数值、作用范围、递减与 75% 上限，删除重复计算解释。Tooltip 缓存增加语言版本条件。
- 独立本地化片段：`gem-compact.entries.json`（31 条）和 `chest-stack.entries.json`（1 条），由共享本地化任务合入总字典。
- 仓库计数：共享 UI 任务持有 Workbench / InventoryWindow，协调其统一数字和描边，避免并行覆盖。

## 验证状态

旧版本受保护会话的确定性失败已复现。新增定向测试：

- `GameXXK.Training.Chests.StoredStackAndRollback`
- `GameXXK.Localization.Surfaces.CompactGemNamesAndDescriptions`

冷 UBT 已通过：`Saved/Codex/UIGuidanceLocalization-20260910/deck-quality-and-tools-fixed-build.log`，5 个增量动作、133.26 秒。首次共享构建的两处 UI 变量重名已修正。

独立 18866 自动化结果：本批 38 项均有通过证据。宝箱 4/4（含原有容量、掉落、迁移及新增仓库堆叠/回滚）和宝石两项在 `deck-quality-and-tools-tests.json` 通过，覆盖 17 类 × 10 品质 × 2 语言的短标题及正文。两处测试前置修正后分别在 `deck-quality-fixtures-tests.json`、`warehouse-live-final-tests.json` 通过；未放宽运行时隐藏面板早退或卡牌品质规则。

## 实机验收完成

- 18765 / 默认纯 2D 地图，用户确认暂停操作后，用 Sky 原生右键一次开启两只普通箱，数量 2 → 0；仓库已有洗练砂 3 → 4，背包未产生同 ID 第二堆，存档错误为空。测试副本暂停自动游历，避免其奖励种子更新干扰精确掉落比对。`ChestBlock/ui-expected.json` 与 `right-click-after.json` 保存断言依据。
- 仓库与背包同时放置 20 个行旅钱，实际画面均为白字黑边的 `20`，位于格子右下角，没有 `x` 前缀。图：`Saved/StorySystem/chest-money-counts-final.png`。同格数量 20 → 30、不重建面板的检查通过。
- `Saved/StorySystem/ChestBlock/gem-compact-zh.png`、`gem-compact-en.png`：火焰宝石 / Fire Gem 标题短且不带品质，品质另起一行，正文全部在框内，悬停状态下切换语言可刷新。
- `Saved/StorySystem/ChestBlock/shatter-plain-english-pills.png`：Shatter 短标题及 Vulnerability / Mark 标签清晰，标签没有描边，正文无中文残留。
- 20:06 恢复测试前完整真实 RuntimeState，退出 Dev 会话，并确认测试期间所有 `.sav` 哈希未变；随后正常保存。真实玩家 522770 金币、等级 12、HP 265、普通宝箱 6 个。备份：`Saved/StorySystem/GateSaveFix/PlayerSaveBackup-20260910-200602`。PIE 已停止、脏包 0。测试奖励未写入真实存档。
