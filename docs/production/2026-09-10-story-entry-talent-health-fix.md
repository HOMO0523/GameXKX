# 主线入口被生命天赋阻断

用户实测点击主线章节后仍停留在背包。真实运行状态反馈为 `任务进度暂未保存：Persistent player resources are invalid.`，并非任务未解锁。

## 原因与处理

`OpenChapter` 会先提交“已查看章节”存档；挂机把包含永久天赋的真实气血写回 `PlayerHP`，而 `PlayerMaxHP` 一直表示装备后的基础上限。已学习“强身砺刃”时真实上限多5，先后观察到150/145、180/175。旧资源校验只加路线属性，没有加永久天赋，因此拒绝正常存档，并阻断章节打开。

保留基础上限字段的含义，引入统一的实际气血上限计算。存档校验、装备镜像同步、升级镜像、战斗投影与写回、回血物品和路线清理均按实际上限处理；战斗投影保留已有伤势，避免再次叠天赋时额外回血。另补百草小篓的路线回血上限，避免满天赋气血被一次治疗反向夹低。不增加存档版本，不删除天赋，不把当前气血硬切回基础值。

## 真实进度与验证

- 重启前额外创建恢复槽 `GameXXK_StoryEntry_Recovery_20260910_034723`，未覆盖原槽。44000金币、6级、540经验、180当前气血/175基础上限；记录 `Saved/StorySystem/TaskEntryBug/recovery-slot.json`。
- 修复后通过正常 `LoadGameFromSlot` 加载恢复槽，再 `SaveCurrentGame` 保存默认槽成功。金币、经验、装备集合、库存、天赋、剧情进度与重启前导出逐项一致；证据 `recovery-verified.json` 和 `restored-live-snapshot.json`。
- 默认2D地图真实鼠标执行“任务→天台山→右上角关闭→任务→天台山”成功。再次打开时 `MAIN_STORY`、反馈空、195当前气血/190基础上限，仍是玩家实际挂机进度，未导入开发夹具、未代玩家领取剧情奖励。证据 `native-entry-after-reopen.json`、`Saved/StorySystem/task-entry-native-fixed.png`。
- 实机输入校准：此次Slate截图为1536×816，真实桌面为1920×1020。测试脚本显式使用1.25坐标校准；未经校准的模拟点击不作为UI失效证据。修复仅在检查脚本中，不改游戏坐标换算。
- 首轮冷UBT通过：`Saved/Diagnostics/TalentUpgradeUsability/build-fixed.log`。本任务49项扩展检查45通过，全部16项MainStory通过；相邻任务25项Talents/属性检查通过。
- 最终百草小篓补充冷UBT通过：`Saved/StorySystem/TaskEntryBug/build-final.log`，5动作、33.62秒。`Saved/Automation/TaskEntryTalentHealthFinal20260910/index.json` 18/18、0警告/错误，包含全部17项MainStory和非卡牌遗物触发回归；新覆盖回血不会降低满天赋气血、路线清理保留永久气血、已有伤势跨投影不凭空恢复。
- 随后的隔离美术实拍完成5章树、8位半身像、逐次回看与原生关闭，临时开发状态已恢复，HUD恢复50%，PIE停止、脏包0。正常项目17份存档/设置哈希全保持；没有以测试进度覆盖玩家存档。相邻任务正常保存后的最新玩家进度51140金币/195气血仍保留。

## 扩展检查边界

`Saved/Automation/TaskEntryTalentHealth20260910/index.json` 的4项失败保留，不宣称整份检查全绿：

1. `WorkbenchHealthRegionAndSaveValidity`：旧装备夹具要求40最大内力，现有饰品给出的值是30；随后45当前内力导致资源校验失败。气血断言通过。
2. `CatalogAndRunLifetime`：旧断言要求30种普通遗物，退休 `WineCup` 后目录为29种。
3. `EventAttributeAndChestChoice`：手工旧路线夹具退出失败。
4. `EventAttributesProjectOncePerBattle`：裸 `CreateNewGame` 的旧3D路线夹具进入失败，未到达投影检查。

这四个夹具均未学习天赋；本次有效气血公式在零天赋时与原基础加路线数值相同。后两项旧路线夹具仍是独立待整理检查，不为通过它们改变当前2D玩法或跳过持久化校验。
