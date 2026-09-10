# 主线局外前置与入图衔接

用户要求：局外先完成前置对白，明确点击进入1-1，再在任务问号遇到任务怪并直接进入战斗。追加关闭问题后用户确认“现在可以关掉了，但是任务线还是不对”；本轮未扩改全局窗口策略。

共用规则已修改，覆盖四个JourneyBattle（S00-04、S01-07、S03-10、S05-06）：StartTask先在局外播放对白，完成后ReadyToTravel显示对应关卡的进入按钮；BeginTaskJourney才生成路线；选任务问号原子地准备并提交真实战斗。两处JourneyInvestigation（S02-05、S04-04）也需明确点击入图，实地调查对白和选项留在路线内。任务前置、61节点奖励、可补分支和每条线一次旅程规则保留。旧的门口对白进度继续有效，读完直接入战；没有改已批准的插图或清空玩家进度。

旧行为已用可恢复Dev会话复现：开始S00-04立即challenge_active=true；恢复玩家状态逐字段一致、存档hash未变。报告Saved/StorySystem/JourneyFlowFix/pre-fix-reproduction.json。

联合冷UBT通过：Saved/StorySystem/JourneyFlowFix/build-final.log，38动作455.01秒，包含另一任务已齐套的本地化/存储改动。Saved/Automation/StoryJourneyFlowFinal20260910/index.json 21/21、0错误/警告，含六章61节点/两分支/六次旅程和新增局外前置、明确入图、门口自动开战、失败原子回滚及旧存档对白衔接。不能将该结果扩张为另一任务全部逻辑验证。

真实UI全链尚未完成：自动UI的第一次节点点击未进入详情，已恢复Dev并核对存档不变；第二次取证期间外部Cmd quit关闭试玩，出现新编辑器窗口。用户随后明确“先这样”，本轮暂停；没有宣称第一章或其余章节逐章实机验收完毕。最新正常保存保护备份为JourneyFlowFix/latest-player-backup.json（当时408470金币、11级、HP319），旧的403220/第一轮存档基线不可覆盖之后用户的新进度。

最新交接：用户确认曾自行关闭/重开窗口，随后要求“开个新存档然后停住，我自己验证”。已停止自动实机：原Slot1和DesktopCheckpoint准备为通过正常MVP StartGame/Save/Load往返验证的新档（1级、10000金币、0主线进度）；原最新进度509940金币/11级已保留在此前空闲的Slot2，其他手动槽不变，并保留26份sav备份。见 `Saved/StorySystem/JourneyFlowFix/user-fresh-save.json`。新档由用户自行启动验证，未推进剧情或战斗。自动实机后段另遇Windows DWM窗口创建断言及新编辑器显存分配失败，相关日志保留，未改动全局窗口策略来掩盖环境问题。
