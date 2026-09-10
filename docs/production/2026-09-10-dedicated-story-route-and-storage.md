# 独立短任务地图与真实战斗存档

用户新档在任务问号无法开战。读到的状态为S00-04已读8句、Gate尚未发布、310840金币、2级115气血；反馈来自PersistTrainingCheckpoint写入失败，而第一层语义校验已经通过。

## 已取证的保存原因

写入前后FGameXXKSaveState.export_text差异为战斗单位DisplayName从未带key的FText变成带随机NSLOCTEXT标识的FText，公鸡、山羊、狸猫、主角、伙伴等名称显示内容不变。原始引擎SaveGame往返成功，旧封装在第一次序列化之前计算校验，因此误拒绝自己的输出。现场导出在Saved/StorySystem/GateSaveFix/player-before.json、battle-candidate.json、state-before.txt与state-after.txt；Dev探针结束后原玩家状态逐字段恢复，正式手动槽与恢复点hash保持。

SaveStorage.Write现先在副本上执行引擎序列化/读回，再计算原有校验值。Verify算法不变，保留旧文件兼容和篡改检测。新增AutomaticGateBattleRealStorageRoundTrip将真实主线checkpoint对象写到唯一磁盘测试槽并读回；此前返回true或SaveGameToMemory的委托不足以覆盖该问题。

## 用户批准的地图分离

用户明确“任务1-1和1-1解耦分开，进的图不一样”，并选择“短任务图，只保留本段剧情和任务战斗”。

- 独立任务地图使用StoryJourney.<节点ID>的持久化地图身份及专门构造器，只有已走过的出发点和可点击的任务目标，单屏显示；不再调用普通地图生成器再覆盖第三层。
- 地图标题为任务1-1等，隐藏普通地图的行旅钱/卡槽和混合节点图例。普通1-1等保留原始完整随机路线。
- 只复用对应阶段的战斗敌人配置与现有战斗界面；任务成功不清除普通关卡、不修改普通关卡选择、不生成普通地图随机奖励，报酬只从主线节点领取。
- 调查章节在任务节点完成调查；战斗章节仍按局外对白→明确入图→任务节点直接战斗。结束后可在同次行程继续该章线性剧情，退出保留任务进度。
- 旧未完成任务路线会以原任务/随机种子/对白/奖励进度转为短任务图，不重置玩家存档。已批准61图不变。

上述代码已齐套，正在本轮冷UBT和定向检查。当前玩家正常存档额外完整备份为Saved/StorySystem/GateSaveFix/PlayerSaveBackup-20260910-112759；不得以此前408470或其他测试基线覆盖这份新档。

后续完成：独立短图/存储规范化的33动作冷UBT258.09秒通过（build-storage-and-routes.log），23项MainStory/Storage检查全过。用户实际已经从短图进入任务战斗，actual-player-battle.json记录Gate=1、AwaitingBattle、第4回合及真实剩余敌人HP。短图又按用户要求改为居中短直线，单cpp冷UBT4动作43.86秒通过。之后横移缓存偏移修复及61节点真实战斗全通跑见2026-09-10-main-story-full-playthrough-review.md，最新玩家进度以实时存档及latest-player-backup.json为准。
